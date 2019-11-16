//#include "BTreeOLC.h"
#include "BTreeOLC.h"
#include "BTree_single_threaded.h"
#include "BTree_rtm.h"
#include <cassert>
#include <vector>
#include <time.h>
#include <iostream>
#include <algorithm>
#include <thread>

#define NUM_ELEMENTS 500
#define NUM_ELEMENTS_MULTI 5000

void generateRandomValues(
    int numValues, 
    std::vector<int64_t>& keys, 
    std::vector<int64_t>& values
) {
    srand(time(NULL));
    for(int i = 0; i < numValues; i++) {
        int64_t randKey = rand() % 30000;
        while(std::find(keys.begin(), keys.end(), randKey) != keys.end()) {
            randKey = rand() % 30000;
        }

        keys.push_back(randKey);
        values.push_back(rand() % 30000);
    }
}

template <class Index> 
void indexInsert(
    Index &idx, 
    int startValue, 
    int endValue, 
    std::vector<int64_t>& keys, 
    std::vector<int64_t>& values
) {
    for(auto i = startValue; i < endValue; i++){
        idx.insert(keys[i], values[i]);
    }
}

template <class Index> 
void testTreeSingleThreaded(Index& idx) {
    std::vector<int64_t> keys;
    std::vector<int64_t> values;

    generateRandomValues(NUM_ELEMENTS, keys, values);

    indexInsert<Index>(idx, 0, keys.size(), keys, values); 

    assert(idx.checkTree());
    for(int i = 0; i < keys.size(); i++){
        int64_t result;
        assert(idx.lookup(keys[i], result));
        if(result != values[i]) {
            printf("Looking up: %lld \n", keys[i]);
            printf("Result %lld, value %lld \n", result, values[i]);
        }
        assert(result == values[i]);
    } 
}

template <class Index>
void testMultiThreaded(Index &idx, int numThreads) {
    std::vector<int64_t> keys;
    std::vector<int64_t> values;
    std::vector<std::thread> threads; 

    generateRandomValues(NUM_ELEMENTS_MULTI, keys, values); 
    int numValuesPerThreads = NUM_ELEMENTS_MULTI/numThreads;
    
    int i;
    for(i = 0; i < numThreads-1; i++) {
        threads.push_back(std::thread([&](){
            indexInsert<Index>(idx, i * numValuesPerThreads, (i+1) * numValuesPerThreads, keys, values);
        }));
    }

    threads.push_back(std::thread([&](){
        indexInsert<Index>(idx, i * numValuesPerThreads, keys.size(), keys, values);
    }));

    for(std::thread& t : threads) {
        t.join(); 
    }

    assert(idx.checkTree());
    for(int i = 0; i < keys.size(); i++){
        int64_t result;
        assert(idx.lookup(keys[i], result));
        if(result != values[i]) {
            printf("Looking up: %lld \n", keys[i]);
            printf("Result %lld, value %lld \n", result, values[i]);
        }
        assert(result == values[i]);
    } 
}

int main() {
    btreertm::BTree<int64_t, int64_t> idx_rtm;
    btreeolc::BTree<int64_t, int64_t> idx_olc;
    btreesinglethread::BTree<int64_t, int64_t> idx_single;

    printf("Testing Single Threaded idx_rtm");
    testTreeSingleThreaded(idx_rtm);

    printf("Testing Single Threaded idx_olc");
    testTreeSingleThreaded(idx_olc);
    
    printf("Testing Single Threaded idx_single");
    testTreeSingleThreaded(idx_single);

    printf("Testing MultiThreaded idx_olc");
    testMultiThreaded<btreeolc::BTree<int64_t, int64_t>>(idx_olc, 5);

    printf("Testing MultiThreaded idx_rtm");
    testMultiThreaded<btreertm::BTree<int64_t, int64_t>>(idx_rtm, 5);
}
