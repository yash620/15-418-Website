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
#include <random>
#include <float.h>

#define NUM_ELEMENTS 1000000
#define NUM_ELEMENTS_TEST 1000 
#define NUM_ELEMENTS_MULTI_TEST 1'000'000
#define NUM_ELEMENTS_MULTI 10'000'000

void generateUniformRandomValues(
    int numValues, 
    std::vector<int64_t>& keys, 
    std::vector<int64_t>& values
) {
    fprintf(stderr, "Generating Random Numbers \n");
    srand(time(NULL));
    std::random_device rd;
    std::default_random_engine eng {rd()};
    std::uniform_int_distribution<int64_t> dist(0, NUM_ELEMENTS_MULTI * 100);

    for(int i = 0; i < numValues; i++) {
        int64_t randKey = dist(eng);
        //int64_t randKey = numValues - i; 
        while(std::find(keys.begin(), keys.end(), randKey) != keys.end()) {
            randKey = dist(eng);
        }

        keys.push_back(randKey);
        values.push_back(dist(eng));
        if(i % 10000 == 0) {
            fprintf(stderr, "Generating %d values \n", i);
        }
        // fprintf(stderr,"Generate Key: %lld, Value: %lld \n", keys[i], values[i]);
    }
    fprintf(stderr, "Done generating random numbers \n");
}

void generateRandomValues(
    int64_t numValues,
    std::vector<int64_t>& keys,
    std::vector<int64_t>& values
) {
    fprintf(stderr, "Generating Random Numbers \n");
    std::random_device rd;
    std::default_random_engine eng {rd()};
    std::uniform_int_distribution<int64_t> dist(0, numValues * 100); 

    for(int64_t i = 0; i < numValues; i++) {
        keys.push_back(i);
        values.push_back(dist(eng));
    }

    std::shuffle(std::begin(keys), std::end(keys), eng);
    fprintf(stderr, "Done generating random numbers \n");
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

    generateRandomValues(NUM_ELEMENTS_TEST, keys, values);

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

    idx.clear();
}

template <class Index>
void testMultiThreaded(Index &idx, int numThreads) {
    std::vector<int64_t> keys;
    std::vector<int64_t> values;
    std::vector<std::thread> threads; 

    generateRandomValues(NUM_ELEMENTS_MULTI_TEST, keys, values); 
    int numValuesPerThreads = NUM_ELEMENTS_MULTI_TEST/numThreads;
    
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

    idx.clear();
}

/**
 * Benchmarks inserting multithreaded
 * returns the elapsed time
 */
template <class Index>
double multiInsertThreadedBenchmark(
    Index &idx, 
    int numThreads, 
    int numRuns,
    std::vector<int64_t>& keys,
    std::vector<int64_t>& values 
 ) {
    std::vector<std::thread> threads; 
    int numOperations = keys.size();

    double currElapsed = DBL_MAX;
    int numValuesPerThreads = numOperations/numThreads; 
    for(int run = 0; run < numRuns; run++) {
        Timer t;
        int i;
        for(i = 0; i < numThreads-1; i++) {
            threads.push_back(std::thread([&](int threadId){
                indexInsert<Index>(threadId, idx, threadId * numValuesPerThreads, (threadId+1) * numValuesPerThreads, keys, values);
            }, i));
        }
        t.reset();
        
        int currThreadId = numThreads-1;
        indexInsert<Index>(currThreadId, idx, currThreadId * numValuesPerThreads, keys.size(), keys, values);
        for(std::thread& t : threads) {
            t.join(); 
        }

        double elapsed = t.elapsed(); 
        currElapsed = std::min(elapsed, currElapsed);
        idx.clear();
        threads.clear();
    }
    printf("Execution Time: %.6fms \n", currElapsed);

    return currElapsed; 

}

template <class Index>
double singleThreadedInsertBenchmark(
    Index &idx, 
    std::vector<int64_t>& keys,
    std::vector<int64_t>& values  
 ) {
    std::vector<std::thread> threads; 

    int numOperations = keys.size();
    printf("Done generating values starting benchmark \n");
    Timer t; 
    t.reset();
    indexInsert<Index>(0, idx, 0, numOperations, keys, values);

    double elapsed = t.elapsed(); 
    printf("Execution Time: %.6fms \n", elapsed);

    idx.clear();
    return elapsed; 
}

int main() {
    btreertm::BTree<int64_t, int64_t> idx_rtm;
    btreeolc::BTree<int64_t, int64_t> idx_olc;
    btreesinglethread::BTree<int64_t, int64_t> idx_single;

    /*
    fprintf(stderr,"Testing Single Threaded idx_rtm \n");
    testTreeSingleThreaded(idx_rtm);

    fprintf(stderr,"Testing MultiThreaded idx_rtm \n");
    testMultiThreaded<btreertm::BTree<int64_t, int64_t>>(idx_rtm, 2);
    */

    //fprintf\(stderr,"Testing Single Threaded idx_olc");
    //testTreeSingleThreaded(idx_olc);
    
    //fprintf\(stderr,"Testing Single Threaded idx_single");
    //testTreeSingleThreaded(idx_single);

    //fprintf\(stderr,"Testing MultiThreaded idx_olc");
    //testMultiThreaded<btreeolc::BTree<int64_t, int64_t>>(idx_olc, 10);


    std::vector<int64_t> keys;
    std::vector<int64_t> values;
    keys.reserve(NUM_ELEMENTS_MULTI);
    values.reserve(NUM_ELEMENTS_MULTI);

    generateRandomValues(NUM_ELEMENTS_MULTI, keys, values); 

    // fprintf(stdout, "Benchmarking idx_olc \n");
    // multiInsertThreadedBenchmark(idx_olc, 40, 2, keys, values); 
    // fprintf(stdout, "Done Warming up the caches! \n");

    // fprintf(stdout, "Benchmarking idx_olc \n");
    // multiInsertThreadedBenchmark(idx_olc, 40, 5, keys, values); 

    fprintf(stdout, "Benchmarking Multithreaded idx_rtm \n");
    multiInsertThreadedBenchmark(idx_rtm, 40, 5, keys, values);

    fprintf(stdout, "Benchmarking idx_olc single threaded \n");
    singleThreadedInsertBenchmark(idx_olc, keys, values); 

    fprintf(stdout, "Benchmarking idx_rtm single threaded \n");
    singleThreadedInsertBenchmark(idx_rtm, keys, values); 

    fprintf(stdout, "Benchmarking idx_single single threaded \n");
    singleThreadedInsertBenchmark(idx_single, keys, values); 
}
