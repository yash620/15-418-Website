#pragma once

#include <cassert>
#include <cstring>
#include <atomic>
#include <immintrin.h>
#include <sched.h>
#include <mutex>
#include <functional>
#include <shared_mutex>

#define MAX_TRANSACTION_RESTART 40
namespace btreertm{

    enum class PageType : uint8_t { BTreeInner=1, BTreeLeaf=2 };

    static const uint64_t pageSize=4*1024;

    struct NodeBase {
        PageType type;
        uint16_t count;
        std::shared_mutex nodeLatch; 
        volatile int version = 0;

        void lockExclusive() {
            nodeLatch.lock();
            version++;
        } 

        void unlockExclusive() {
            if(version != -1) { 
                nodeLatch.unlock();
            }
        }

        void lockShared() {
            if(version != -1) { 
                nodeLatch.lock_shared();
            }
        }

        void unLockShared() {
            if(version != -1) { 
                nodeLatch.unlock_shared();
            }
        }

        // Returns true if fails needs restart, returns false otherwise. 
        bool upgradeToExclusive() {
            int currVersion = version;
            unLockShared();
            lockExclusive();
            if(currVersion + 1 != version) {
                unlockExclusive();
                return true;
            }
            return false;
        }
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

            static const uint64_t maxEntries=(pageSize-sizeof(NodeBase))/(sizeof(Key)+sizeof(Payload));

            Key keys[maxEntries];
            Payload payloads[maxEntries];

            BTreeLeaf() {
                count=0;
                type=typeMarker;
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

            void insert(Key k,Payload p) {
                assert(count<maxEntries);
                if (count) {
                    unsigned pos=lowerBound(k);
                    if ((pos<count) && (keys[pos]==k)) {
                        // Upsert
                        payloads[pos] = p;
                        return;
                    }
                    memmove(keys+pos+1,keys+pos,sizeof(Key)*(count-pos));
                    memmove(payloads+pos+1,payloads+pos,sizeof(Payload)*(count-pos));
                    keys[pos]=k;
                    payloads[pos]=p;
                } else {
                    keys[0]=k;
                    payloads[0]=p;
                }
                count++;
            }

            BTreeLeaf* split(Key& sep) {
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
            static const uint64_t maxEntries=(pageSize-sizeof(NodeBase))/(sizeof(Key)+sizeof(NodeBase*));
            NodeBase* children[maxEntries];
            Key keys[maxEntries];

            BTreeInner() {
                count=0;
                type=typeMarker;
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
        restart:
                if(restartCount++ > MAX_TRANSACTION_RESTART) { 
                    fprintf(stderr, "Going to latched version \n");
                    insertLatched(k, v);
                    return; 
                }

                if(_xbegin() != _XBEGIN_STARTED) 
                    goto restart;

                // Current node
                NodeBase* node = root;

                // Parent of current node
                BTreeInner<Key>* parent = nullptr;

                while (node->type==PageType::BTreeInner) {
                    auto inner = static_cast<BTreeInner<Key>*>(node);

                    //Touch parent lock data to ensure atomicity
                    if(inner->version == -1) {
                        _xend(); 
                        goto restart;
                    }
                    if(parent) {
                        if(parent->version == -1) {
                            _xend();
                            goto restart;
                        }
                    }

                    // Split eagerly if full
                    if (inner->isFull()) {
                        // Split
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

                auto leaf = static_cast<BTreeLeaf<Key,Value>*>(node);

                //Touch parent lock data to ensure atomicity
                if(leaf->version == -1) {
                    _xend();
                    goto restart;
                };
                if(parent) {
                    if(parent->version == -1) {
                        _xend();
                        goto restart;
                    } 
                }

                // Split leaf if full
                if (leaf->count==leaf->maxEntries) {
                    // Split
                    Key sep; BTreeLeaf<Key,Value>* newLeaf = leaf->split(sep);
                    if (parent)
                        parent->insert(sep, newLeaf);
                    else
                        makeRoot(sep, leaf, newLeaf);
                    _xend();
                    goto restart;
                } else {
                    // only lock leaf node
                    leaf->insert(k, v);
                    // success
                }
                _xend();
            }

            void insertLatched(Key k, Value v) {
                int restartCount = 0;
        restart:
                fprintf(stderr,"Key: %lld, Value: %lld, On latched restart count %d \n", k, v, restartCount++);
                // Current node
                NodeBase* node = root;
                node->lockShared();
                if (node != root) {
                    node->unLockShared(); 
                    fprintf(stderr, "Restart at loc %d \n", 1);
                    goto restart;
                }

                // Parent of current node
                BTreeInner<Key>* parent = nullptr;
                bool needRestart; 
                while (node->type==PageType::BTreeInner) {
                    auto inner = static_cast<BTreeInner<Key>*>(node);

                    // Split eagerly if full
                    if (inner->isFull()) {
                        // Lock
                        if (parent) {
                            needRestart = parent->upgradeToExclusive();
                            if (needRestart){ 
                                inner->unLockShared();
                                fprintf(stderr, "Restart at loc %d \n", 2);
                                goto restart;
                            }
                        }
                        needRestart = inner->upgradeToExclusive();
                        if (needRestart) {
                            if (parent)
                                parent->unlockExclusive();
                            fprintf(stderr, "Restart at loc %d \n", 3);
                            goto restart;
                        }
                        if (!parent && (node != root)) { // there's a new parent
                            inner->unlockExclusive();
                            goto restart;
                        }
                        // Split
                        Key sep; BTreeInner<Key>* newInner = inner->split(sep);
                        if (parent)
                            parent->insert(sep,newInner);
                        else
                            makeRoot(sep,inner,newInner);
                        // Unlock and restart
                        inner->unlockExclusive();
                        if (parent)
                            parent->unlockExclusive();
                        fprintf(stderr, "Restart at loc %d \n", 4);
                        goto restart;
                    }

                    if (parent) {
                        parent->lockShared();
                    }

                    parent = inner;

                    node = inner->children[inner->lowerBound(k)];
                    node->lockShared();
                    //inner->unlockShared();
                }

                auto leaf = static_cast<BTreeLeaf<Key,Value>*>(node);

                // Split leaf if full
                if (leaf->count==leaf->maxEntries) {
                    // Lock
                    if (parent) {
                        needRestart = parent->upgradeToExclusive();
                        if (needRestart) {
                            leaf->unLockShared();
                            fprintf(stderr, "Restart at loc %d \n", 5);
                            goto restart;
                        } 
                    }
                    needRestart = leaf->upgradeToExclusive();
                    if (needRestart) {
                        if (parent) parent->unlockExclusive();
                        fprintf(stderr, "Restart at loc %d \n", 5);
                        goto restart;
                    }
                    if (!parent && (leaf != root)) { // there's a new parent
                        leaf->unlockExclusive();
                        fprintf(stderr, "Restart at loc %d \n", 6);
                        goto restart;
                    }
                    // Split
                    Key sep; BTreeLeaf<Key,Value>* newLeaf = leaf->split(sep);
                    if (parent)
                        parent->insert(sep, newLeaf);
                    else
                        makeRoot(sep, leaf, newLeaf);
                    // Unlock and restart
                    leaf->unlockExclusive();
                    if (parent)
                        parent->unlockExclusive();
                    fprintf(stderr, "Restart at loc %d \n", 7);
                    goto restart;
                } else {
                    // only lock leaf node
                    needRestart = leaf->upgradeToExclusive();
                    if (needRestart){
                        if(parent){
                            parent->unLockShared();
                        } 
                        fprintf(stderr, "Restart at loc %d \n", 8);
                        goto restart;
                    } 
                    if (parent) {
                        parent->unLockShared();
                    }
                    leaf->insert(k, v);
                    leaf->unlockExclusive();
                    return; // success
                }
            }

            bool lookup(Key k, Value& result) {
restart:
                if(_xbegin() != _XBEGIN_STARTED)
                    goto restart;
                NodeBase* node = root;

                // Parent of current node
                BTreeInner<Key>* parent = nullptr;

                while (node->type==PageType::BTreeInner) {
                    auto inner = static_cast<BTreeInner<Key>*>(node);

                    parent = inner;

                    node = inner->children[inner->lowerBound(k)];
                }

                BTreeLeaf<Key,Value>* leaf = static_cast<BTreeLeaf<Key,Value>*>(node);
                unsigned pos = leaf->lowerBound(k);
                bool success;
                if ((pos<leaf->count) && (leaf->keys[pos]==k)) {
                    success = true;
                    result = leaf->payloads[pos];
                }
                
                _xend();
                return success;
            }

        };

}
