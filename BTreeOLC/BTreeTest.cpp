//#include "BTreeOLC.h"
#include "BTreeOLC.h"
#include "BTree_single_threaded.h"
#include "BTree_rtm.h"
#include "timing.h"

#include <cassert>
#include <vector>
#include <time.h>
#include <iostream>
#include <algorithm>
#include <thread>

#define NUM_ELEMENTS 500
#define NUM_ELEMENTS_MULTI 30000

void generateRandomValues(
    int numValues, 
    std::vector<int64_t>& keys, 
    std::vector<int64_t>& values
) {
    srand(time(NULL));
    for(int i = 0; i < numValues; i++) {
        int64_t randKey = rand() % 30000;
        //int64_t randKey = i; 
        while(std::find(keys.begin(), keys.end(), randKey) != keys.end()) {
            randKey = rand() % 30000;
        }

        keys.push_back(randKey);
        values.push_back(rand() % 30000);
        fprintf(stderr,"Generate Key: %lld, Value: %lld \n", keys[i], values[i]);
    }
}

template <class Index> 
void indexInsert(
    int threadId,
    Index &idx, 
    int startValue, 
    int endValue, 
    std::vector<int64_t>& keys, 
    std::vector<int64_t>& values
) {
    for(auto i = startValue; i < endValue; i++){
        //fprintf(stderr,"Thread %d, Inserting Key: %lld, Value %lld \n", threadId, keys[i], values[i]);
        idx.insert(keys[i], values[i]);
    }
}

template <class Index> 
void testTreeSingleThreaded(Index& idx) {
    std::vector<int64_t> keys;
    std::vector<int64_t> values;

    generateRandomValues(NUM_ELEMENTS, keys, values);

    indexInsert<Index>(0, idx, 0, keys.size(), keys, values); 

    assert(idx.checkTree());
    for(int i = 0; i < keys.size(); i++){
        int64_t result;
        assert(idx.lookup(keys[i], result));
        if(result != values[i]) {
            fprintf(stderr,"Looking up: %lld \n", keys[i]);
            fprintf(stderr,"Result %lld, value %lld \n", result, values[i]);
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
        threads.push_back(std::thread([&](int threadId){
            indexInsert<Index>(threadId, idx, threadId * numValuesPerThreads, (threadId+1) * numValuesPerThreads, keys, values);
        }, i));
    }

    threads.push_back(std::thread([&](int threadId){
        indexInsert<Index>(threadId, idx, threadId * numValuesPerThreads, keys.size(), keys, values);
    }, i));

    for(std::thread& t : threads) {
        t.join(); 
    }

    assert(idx.checkTree());
    for(int i = 0; i < keys.size(); i++){
        int64_t result;
        assert(idx.lookup(keys[i], result));
        if(result != values[i]) {
            fprintf(stderr,"Looking up: %lld \n", keys[i]);
            fprintf(stderr,"Result %lld, value %lld \n", result, values[i]);
        }
        assert(result == values[i]);
    } 
}

/**
 * Benchmarks inserting multithreaded
 * returns the elapsed time
 */
template <class Index>
double multiInsertThreadedBenchmark(Index &idx, int numThreads, int numOperations) {
    std::vector<int64_t> keys;
    std::vector<int64_t> values;
    std::vector<std::thread> threads; 

    generateRandomValues(numOperations, keys, values); 
    printf("Done generating values starting benchmark \n");
    int numValuesPerThreads = numOperations/numThreads; 
    Timer t;
    int i;
    for(i = 0; i < numThreads-1; i++) {
        threads.push_back(std::thread([&](int threadId){
            indexInsert<Index>(threadId, idx, threadId * numValuesPerThreads, (threadId+1) * numValuesPerThreads, keys, values);
        }, i));
    }
    t.reset();
    for(std::thread& t : threads) {
        t.join(); 
    }

    double elapsed = t.elapsed(); 
    printf("Execution Time: %.6fms \n", elapsed);

    return elapsed; 

}

template <class Index>
double singleThreadedInsertBenchmark(Index &idx, int numOperations) {
    std::vector<int64_t> keys;
    std::vector<int64_t> values;
    std::vector<std::thread> threads; 

    generateRandomValues(numOperations, keys, values); 
    printf("Done generating values starting benchmark \n");
    Timer t; 
    t.reset();
    indexInsert<Index>(0, idx, 0, numOperations, keys, values);

    double elapsed = t.elapsed(); 
    printf("Execution Time: %.6fms \n", elapsed);

    return elapsed; 
}

int main() {
    btreertm::BTree<int64_t, int64_t> idx_rtm;
    btreeolc::BTree<int64_t, int64_t> idx_olc;
    btreesinglethread::BTree<int64_t, int64_t> idx_single;

    //fprintf\(stderr,"Testing Single Threaded idx_rtm");
    //testTreeSingleThreaded(idx_rtm);

    //fprintf\(stderr,"Testing Single Threaded idx_olc");
    //testTreeSingleThreaded(idx_olc);
    
    //fprintf\(stderr,"Testing Single Threaded idx_single");
    //testTreeSingleThreaded(idx_single);

    //fprintf\(stderr,"Testing MultiThreaded idx_olc");
    //testMultiThreaded<btreeolc::BTree<int64_t, int64_t>>(idx_olc, 10);

    // fprintf(stderr,"Testing MultiThreaded idx_rtm");
    // testMultiThreaded<btreertm::BTree<int64_t, int64_t>>(idx_rtm, 2);

    // fprintf(stdout, "Benchmarking idx_rtm \n");
    // multiInsertThreadedBenchmark(idx_rtm, 40, 30000);

    // fprintf(stdout, "Benchmarking idx_olc \n");
    // multiInsertThreadedBenchmark(idx_olc, 40, 30000); 

    //fprintf(stdout, "Benchmarking idx_olc single threaded \n");
    //singleThreadedInsertBenchmark(idx_olc, 30000); 

    fprintf(stdout, "Benchmarking idx_rtm single threaded \n");
    singleThreadedInsertBenchmark(idx_rtm, 60); 

    //fprintf(stdout, "Benchmarking idx_single single threaded \n");
    //singleThreadedInsertBenchmark(idx_single, 30000); 
}
