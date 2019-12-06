#pragma once

#include <cassert>
#include <cstring>
#include <atomic>
#include <immintrin.h>
#include <sched.h>
#include <mutex>
#include <functional>
#include <shared_mutex>

#define MAX_TRANSACTION_RESTART 8
namespace btreertm{

    enum class PageType : uint8_t { BTreeInner=1, BTreeLeaf=2 };

    //static const uint64_t pageSize=4*1024;
    
    // (sizeof Key + sizeof Payload) * 31 + sizeof NodeBase
    static const uint64_t pageSize = 3968 + 72; 

    struct OptLock {
        std::atomic<uint64_t> typeVersionLockObsolete{0b100};
        volatile std::atomic<int> has_lock = 0;

        bool isLocked(uint64_t version) {
            return ((version & 0b10) == 0b10);
        }

        uint64_t readLockOrRestart(bool &needRestart) {
            uint64_t version;
            version = typeVersionLockObsolete.load();
            if (isLocked(version) || isObsolete(version)) {
                _mm_pause();
                needRestart = true;
            }
            return version;
        }

        void writeLockOrRestart(bool &needRestart) {
            uint64_t version;
            version = readLockOrRestart(needRestart);
            if (needRestart) return;

            upgradeToWriteLockOrRestart(version, needRestart);
            if (!needRestart) return;
        }

        void upgradeToWriteLockOrRestart(uint64_t &version, bool &needRestart) {
            if (typeVersionLockObsolete.compare_exchange_strong(version, version + 0b10)) {
                has_lock = 1;
                version = version + 0b10;
            } else {
                has_lock = 0;
                _mm_pause();
                needRestart = true;
            }
        }

        void writeUnlock() {
            typeVersionLockObsolete.fetch_add(0b10);
            has_lock = 0;
        }

        bool isObsolete(uint64_t version) {
            return (version & 1) == 1;
        }

        void checkOrRestart(uint64_t startRead, bool &needRestart) const {
            readUnlockOrRestart(startRead, needRestart);
        }

        void readUnlockOrRestart(uint64_t startRead, bool &needRestart) const {
            needRestart = (startRead != typeVersionLockObsolete.load());
        }

        void writeUnlockObsolete() {
            typeVersionLockObsolete.fetch_add(0b11);
        }
    };

    struct NodeBase : public OptLock{
        PageType type;
        uint16_t count;
    }; 


    struct BTreeLeafBase : public NodeBase {
        static const PageType typeMarker=PageType::BTreeLeaf;
    };

    template<class Key,class Payload>
        struct BTreeLeaf : public BTreeLeafBase {
            struct Entry {
                Key k;
                Payload p;
            };

            //static const uint64_t maxEntries=(pageSize-sizeof(NodeBase))/(sizeof(Key)+sizeof(Payload));
            static const uint64_t maxEntries=31;
            bool isSorted;
            Key keys[maxEntries];
            Payload payloads[maxEntries];

            BTreeLeaf() {
                count=0;
                type=typeMarker;
                isSorted = false;
            }

            bool isFull() { return count==maxEntries; };

            unsigned lowerBound(Key k) {
                unsigned lower=0;
                unsigned upper=count;
                do {
                    unsigned mid=((upper-lower)/2)+lower;
                    if (k<keys[mid]) {
                        upper=mid;
                    } else if (k>keys[mid]) {
                        lower=mid+1;
                    } else {
                        return mid;
                    }
                } while (lower<upper);
                return lower;
            }

            unsigned lowerBoundBF(Key k) { // (Yash) never called I think
                auto base=keys;
                unsigned n=count;
                while (n>1) {
                    const unsigned half=n/2;
                    base=(base[half]<k)?(base+half):base;
                    n-=half;
                }
                return (*base<k)+base-keys;
            }

            static bool compareEntries(Entry a, Entry b) {
                return a.k < b.k;
            }

            bool insert(Key k,Payload p) {
                if(count >= maxEntries) {
                    return false; 
                }
                assert(count<maxEntries);
                // if (count) {
                //    unsigned pos=lowerBound(k);
                //    if ((pos<count) && (keys[pos]==k)) {
                //        // Upsert
                //        payloads[pos] = p;
                //        return;
                //    }
                //    memmove(keys+pos+1,keys+pos,sizeof(Key)*(count-pos));
                //    memmove(payloads+pos+1,payloads+pos,sizeof(Payload)*(count-pos));
                //    keys[pos]=k;
                //    payloads[pos]=p;
                // } else {
                //    keys[0]=k;
                //    payloads[0]=p;
                // }

                keys[count] = k;
                payloads[count] = p;
                isSorted = false;
                count++;
                return true;
            }

            void restructure() {
               if(!isSorted) {
                    Entry temp[count];
                    for(int i = 0; i < count; i++) {
                        temp[i].k = keys[i];
                        temp[i].p = payloads[i];
                    }
                    std::sort(temp, temp + count, compareEntries);
                    for(int i = 0; i < count; i++) {
                        keys[i] = temp[i].k;
                        payloads[i] = temp[i].p;
                    }
                    isSorted = true;
                } 
            }

            BTreeLeaf* split(Key& sep) {
                restructure();   
                BTreeLeaf* newLeaf = new BTreeLeaf();
                newLeaf->count = count-(count/2);
                count = count-newLeaf->count;
                memcpy(newLeaf->keys, keys+count, sizeof(Key)*newLeaf->count);
                memcpy(newLeaf->payloads, payloads+count, sizeof(Payload)*newLeaf->count);
                sep = keys[count-1];
                return newLeaf;
            }
        };

    struct BTreeInnerBase : public NodeBase {
        static const PageType typeMarker=PageType::BTreeInner;
    };

    template<class Key>
        struct BTreeInner : public BTreeInnerBase {
            static const uint64_t maxEntries=31;
            //static const uint64_t maxEntries=(pageSize-sizeof(NodeBase))/(sizeof(Key)+sizeof(NodeBase*));
            NodeBase* children[maxEntries];
            Key keys[maxEntries];

            BTreeInner() {
                count=0;
                type=typeMarker;
            }

            ~BTreeInner() {
                for(int i = 0; i < count; i++) {
                    delete children[i];
                }
            }

            bool isFull() { return count==(maxEntries-1); };

            unsigned lowerBoundBF(Key k) {
                auto base=keys;
                unsigned n=count;
                while (n>1) {
                    const unsigned half=n/2;
                    base=(base[half]<k)?(base+half):base;
                    n-=half;
                }
                return (*base<k)+base-keys;
            }

            unsigned lowerBound(Key k) {
                unsigned lower=0;
                unsigned upper=count;
                do {
                    unsigned mid=((upper-lower)/2)+lower;
                    if (k<keys[mid]) {
                        upper=mid;
                    } else if (k>keys[mid]) {
                        lower=mid+1;
                    } else {
                        return mid;
                    }
                } while (lower<upper);
                return lower;
            }

            BTreeInner* split(Key& sep) {
                BTreeInner* newInner=new BTreeInner();
                newInner->count=count-(count/2);
                count=count-newInner->count-1;
                sep=keys[count];
                memcpy(newInner->keys,keys+count+1,sizeof(Key)*(newInner->count+1));
                memcpy(newInner->children,children+count+1,sizeof(NodeBase*)*(newInner->count+1));
                return newInner;
            }

            void insert(Key k,NodeBase* child) {
                assert(count<maxEntries-1);
                unsigned pos=lowerBound(k);
                memmove(keys+pos+1,keys+pos,sizeof(Key)*(count-pos+1));
                memmove(children+pos+1,children+pos,sizeof(NodeBase*)*(count-pos+1));
                keys[pos]=k;
                children[pos]=child;
                std::swap(children[pos],children[pos+1]);
                count++;
            }

        };


    template<class Key,class Value>
        struct BTree {
           NodeBase* root;

            BTree() {
                root = new BTreeLeaf<Key,Value>();
            }

            void clear() {
                delete root;
                root = new BTreeLeaf<Key, Value>();
            }

            bool checkTree() {
                int height = checkTreeRecursive(root);
                std::cout << height << std::endl;
                return height != -1;
            }

            int checkTreeRecursive(NodeBase *node) {
                if(node->type == PageType::BTreeInner) {
                    auto inner = static_cast<BTreeInner<Key>*>(node);
                    int height1  = 0, height2 = 0;
                    for(int i = 0; i < inner->count; i++) {
                        auto child = inner->children[i];
                        if(height1 == 0) {
                            height1 = checkTreeRecursive(child);
                        } else {
                            height2 = checkTreeRecursive(child);
                        }

                        if((height2 != 0 && height1 != height2) || height1 == -1 || height2 == -1) {
                            std::cout << height1 << " " << height2 << std::endl;
                            return -1;
                        }
                    }
                    return height1;
                } else {
                    return 1;
                }
            }

            void makeRoot(Key k,NodeBase* leftChild,NodeBase* rightChild) {
                auto inner = new BTreeInner<Key>();
                inner->count = 1;
                inner->keys[0] = k;
                inner->children[0] = leftChild;
                inner->children[1] = rightChild;
                root = inner;
            }

            void insert(Key k, Value v) {
                int restartCount = 0;
                int restartReason = 156;
                bool goToLatched = false;
        restart:
                if(goToLatched || restartCount++ > MAX_TRANSACTION_RESTART) { 
                    //fprintf(stderr, "Going to latched version, key: %ld\n", k);
                    //fprintf(stderr, "Due to %d\n", restartReason);
                    insertLatched(k, v);
                    return; 
                }

                if((restartReason = _xbegin()) != _XBEGIN_STARTED) {
                    goto restart;
                }

                // Current node
                NodeBase* node = root;

                // Parent of current node
                BTreeInner<Key>* parent = nullptr;

                while (node->type==PageType::BTreeInner) {
                    auto inner = static_cast<BTreeInner<Key>*>(node);
                    // if(node->has_lock == 1) {
                    //     _xabort(1);
                    // }
                    if(node->isLocked(node->typeVersionLockObsolete.load()) || node->isObsolete(node->typeVersionLockObsolete.load())) {
                        _xabort(1);
                    }

                    //Touch parent lock data to ensure atomicity
                    if(parent) {
                        if(parent->isLocked(parent->typeVersionLockObsolete.load()) || parent->isObsolete(parent->typeVersionLockObsolete.load())) {
                            _xabort(1);
                        }
                        // if(parent->has_lock == 1) {
                        //     _xabort(2);
                        // }
                    }

                    // Split eagerly if full
                    if (inner->isFull()) {
                        // Split
                        // goToLatched = true;
                        // _xabort(11);
                        Key sep; BTreeInner<Key>* newInner = inner->split(sep);
                        if (parent)
                            parent->insert(sep,newInner);
                        else
                            makeRoot(sep,inner,newInner);
                        _xend(); 
                        goto restart;
                    }

                    parent = inner;

                    node = inner->children[inner->lowerBound(k)];
                }

                //Touch parent lock data to ensure atomicity
                 if(parent) {
                        if(parent->isLocked(parent->typeVersionLockObsolete.load()) || parent->isObsolete(parent->typeVersionLockObsolete.load())) {
                            _xabort(4);
                        }
                        // if(parent->has_lock == 1) {
                        //     _xabort(4);
                        // }
                    }

                auto leaf = static_cast<BTreeLeaf<Key,Value>*>(node);
                // if(leaf->has_lock == 1) {
                //     _xabort(3);
                // }
                if(leaf->isLocked(leaf->typeVersionLockObsolete.load()) || leaf->isObsolete(leaf->typeVersionLockObsolete.load())) {
                    _xabort(3);
                }

                // Split leaf if full
                if (leaf->count>=leaf->maxEntries) {
                    // Split
                    // goToLatched = true;
                    // _xabort(12);
                    Key sep; BTreeLeaf<Key,Value>* newLeaf = leaf->split(sep);
                    if (parent)
                        parent->insert(sep, newLeaf);
                    else
                        makeRoot(sep, leaf, newLeaf);
                    _xend();
                    goto restart;
                } else {
                    // only lock leaf node
                    if(!leaf->insert(k, v)) {
                        _xabort(5);
                    }
                    // success
                }
                _xend();
            }

            void insertLatched(Key k, Value v) {
restart:
                bool needRestart = false;

                // Current node
                NodeBase* node = root;
                uint64_t versionNode = node->readLockOrRestart(needRestart);
                if (needRestart || (node!=root)) goto restart;

                // Parent of current node
                BTreeInner<Key>* parent = nullptr;
                uint64_t versionParent;

                while (node->type==PageType::BTreeInner) {
                    auto inner = static_cast<BTreeInner<Key>*>(node);

                    // Split eagerly if full
                    if (inner->isFull()) {
                        // Lock
                        if (parent) {
                            parent->upgradeToWriteLockOrRestart(versionParent, needRestart);
                            if (needRestart) goto restart;
                        }
                        node->upgradeToWriteLockOrRestart(versionNode, needRestart);
                        if (needRestart) {
                            if (parent)
                                parent->writeUnlock();
                            goto restart;
                        }
                        if (!parent && (node != root)) { // there's a new parent
                            node->writeUnlock();
                            goto restart;
                        }
                        // Split
                        Key sep; BTreeInner<Key>* newInner = inner->split(sep);
                        if (parent)
                            parent->insert(sep,newInner);
                        else
                            makeRoot(sep,inner,newInner);
                        // Unlock and restart
                        node->writeUnlock();
                        if (parent)
                            parent->writeUnlock();
                        goto restart;
                    }

                    if (parent) {
                        parent->readUnlockOrRestart(versionParent, needRestart);
                        if (needRestart) goto restart;
                    }

                    parent = inner;
                    versionParent = versionNode;

                    node = inner->children[inner->lowerBound(k)];
                    inner->checkOrRestart(versionNode, needRestart);
                    if (needRestart) goto restart;
                    versionNode = node->readLockOrRestart(needRestart);
                    if (needRestart) goto restart;
                }

                auto leaf = static_cast<BTreeLeaf<Key,Value>*>(node);

                // Split leaf if full
                if (leaf->count>=leaf->maxEntries) {
                    // Lock
                    if (parent) {
                        parent->upgradeToWriteLockOrRestart(versionParent, needRestart);
                        if (needRestart) goto restart;
                    }
                    node->upgradeToWriteLockOrRestart(versionNode, needRestart);
                    if (needRestart) {
                        if (parent) parent->writeUnlock();
                        goto restart;
                    }
                    if (!parent && (node != root)) { // there's a new parent
                        node->writeUnlock();
                        goto restart;
                    }
                    // Split
                    Key sep; BTreeLeaf<Key,Value>* newLeaf = leaf->split(sep);
                    if (parent)
                        parent->insert(sep, newLeaf);
                    else
                        makeRoot(sep, leaf, newLeaf);
                    // Unlock and restart
                    node->writeUnlock();
                    if (parent)
                        parent->writeUnlock();
                    goto restart;
                } else {
                    // only lock leaf node
                    node->upgradeToWriteLockOrRestart(versionNode, needRestart);
                    if (needRestart) goto restart;
                    if (parent) {
                        parent->readUnlockOrRestart(versionParent, needRestart);
                        if (needRestart) {
                            node->writeUnlock();
                            goto restart;
                        }
                    }
                    leaf->insert(k, v);
                    node->writeUnlock();
                    return; // success
                }
            }

            bool lookup(Key k, Value& result) {
                int restartCount = 0;
restart:
                if(restartCount++ > MAX_TRANSACTION_RESTART) {
                    lookupLatched(k, result);
                }

                if(_xbegin() != _XBEGIN_STARTED) {
                    goto restart;
                }
                NodeBase* node = root;

                // Parent of current node
                BTreeInner<Key>* parent = nullptr;

                while (node->type==PageType::BTreeInner) {
                    auto inner = static_cast<BTreeInner<Key>*>(node);
                    if(inner->isLocked(inner->typeVersionLockObsolete.load()) || inner->isObsolete(inner->typeVersionLockObsolete.load())) {
                            _xabort(4);
                    }
                    parent = inner;

                    node = inner->children[inner->lowerBound(k)];
                }

                BTreeLeaf<Key,Value>* leaf = static_cast<BTreeLeaf<Key,Value>*>(node);
               if(leaf->isLocked(leaf->typeVersionLockObsolete.load()) || leaf->isObsolete(leaf->typeVersionLockObsolete.load())) {
                        _xabort(1);
                }
                assert(leaf->count <= leaf->maxEntries);
                leaf->restructure();
                unsigned pos = leaf->lowerBound(k);
                bool success;
                if ((pos<leaf->count) && (leaf->keys[pos]==k)) {
                    success = true;
                    result = leaf->payloads[pos];
                }
                
                _xend();
                return success;
            }


            bool lookupLatched(Key k, Value& result) {
restart:
                NodeBase* node = root;
                bool needRestart = false;
                uint64_t versionNode = node->readLockOrRestart(needRestart);
                if (needRestart || (node!=root)) goto restart;

                // Parent of current node
                BTreeInner<Key>* parent = nullptr;
                uint64_t versionParent;

                while (node->type==PageType::BTreeInner) {
                    auto inner = static_cast<BTreeInner<Key>*>(node);

                    if (parent) {
                        parent->readUnlockOrRestart(versionParent, needRestart);
                        if (needRestart) goto restart;
                    }

                    parent = inner;
                    versionParent = versionNode;

                    node = inner->children[inner->lowerBound(k)];
                    inner->checkOrRestart(versionNode, needRestart);
                    if (needRestart) goto restart;
                    versionNode = node->readLockOrRestart(needRestart);
                    if (needRestart) goto restart;
                }

                BTreeLeaf<Key,Value>* leaf = static_cast<BTreeLeaf<Key,Value>*>(node);
                bool restructured = false;
                if(!leaf->isSorted) {
                    leaf->upgradeToWriteLockOrRestart(versionNode, needRestart);
                    if(needRestart) {
                            goto restart;
                    }
                    leaf->restructure();
                    restructured = true;
                }
                unsigned pos = leaf->lowerBound(k);
                bool success;
                if ((pos<leaf->count) && (leaf->keys[pos]==k)) {
                    success = true;
                    result = leaf->payloads[pos];
                }
                if (parent) {
                    parent->readUnlockOrRestart(versionParent, needRestart);
                    if (needRestart){
                        if(restructured) {
                            leaf->writeUnlock();
                        }
                        goto restart;
                    } 
                }
                if(restructured) {
                    leaf->writeUnlock();
                } else {
                    leaf->readUnlockOrRestart(versionNode, needRestart);
                    if (needRestart) goto restart;
                }
                return success;
            }

        };

}
