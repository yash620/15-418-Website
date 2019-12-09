#include "BTree_locked.h"
#include "BTreeOLC.h"
#include "BTree_single_threaded.h"
#include "BTree_rtm.h"
#include "timing.h"
#include "WorkloadGenerator.h"

#include <cassert>
#include <vector>
#include <time.h>
#include <iostream>
#include <algorithm>
#include <thread>
#include <random>
#include <float.h>

#define NUM_ELEMENTS 1000000
#define NUM_ELEMENTS_TEST 1'000 
#define NUM_ELEMENTS_MULTI_TEST 1'000'000
#define NUM_ELEMENTS_MULTI 10'000'000
#define MULTI_NUM_THREADS 40

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
    std::vector<int64_t>& values,
    std::atomic<int>& insertFallbacks
) {
    for(auto i = startValue; i < endValue; i++){
        //fprintf(stderr,"Thread %d, Inserting Key: %lld, Value %lld \n", threadId, keys[i], values[i]);
        idx.insert(keys[i], values[i]);
    }
    insertFallbacks.fetch_add(idx.getInsertFallbackTimes());
}

template <class Index> 
void indexLookupAssert(
    int threadId,
    Index &idx,
    int startValue,
    int endValue,
    std::vector<int64_t>& keys,
    std::vector<int64_t>& values
) {
   for(auto i = startValue; i < endValue; i++){
        //fprintf(stderr,"Thread %d, Looking Up Key: %lld, Value %lld \n", threadId, keys[i], values[i]);
        int64_t result;
        idx.lookup(keys[i], result);
        if(result != values[i]) {
            fprintf(stderr,"Looking up: %lld \n", keys[i]);
            fprintf(stderr,"Result %lld, value %lld \n", result, values[i]);
        }
        assert(result == values[i]);
   } 
}

template <class Index> 
void indexLookup(
    int threadId,
    Index &idx,
    int startValue,
    int endValue,
    std::vector<int64_t>& keys,
    std::vector<int64_t>& values,
    std::atomic<int>& lookupFallbacks
) {
   for(auto i = startValue; i < endValue; i++){
        int64_t result;
        idx.lookup(keys[i], result);
   } 
   lookupFallbacks.fetch_add(idx.getLookupFallbackTimes());
}

template <class Index>
void executeWorkload(
    Index &idx,
    std::vector<workload::Operation>& ops
) {
    for(const workload::Operation& op : ops) {
        if(op.type == workload::OpType::Insert) {
            idx.insert(op.key, op.value);
        } else {
            int64_t result;
            idx.lookup(op.key, result);
        }
    }
}

template <class Index>
void executeWorkloadAssert(
    Index &idx,
    std::vector<workload::Operation>& ops
) {
    for(workload::Operation& op : ops) {
        if(op.type == workload::OpType::Insert) {
            idx.insert(op.key, op.value);
        } else {
            int64_t result;
            idx.lookup(op.key, result);
            if(result != op.value) {
                fprintf(stderr,"Looking up: %lld \n", op.key);
                fprintf(stderr,"Result %lld, value %lld \n", result, op.value);
            }
            assert(result == op.value);
        }
    }
}

/**
 * Inserts followed by lookups
 */
template <class Index> 
void testTreeSingleThreaded(Index& idx) {
    std::vector<int64_t> keys;
    std::vector<int64_t> values;
    std::atomic<int> insertFallbacks = 0;

    generateRandomValues(NUM_ELEMENTS_TEST, keys, values);
    indexInsert<Index>(0, idx, 0, keys.size(), keys, values, insertFallbacks); 

    assert(idx.checkTree());
    indexLookupAssert<Index>(0, idx, 0, keys.size(), keys, values);
    idx.clear();
}

/**
 * Interleaves inserts and lookups
 */
template <class Index>
void testMixedTreeSingleThreaded(Index& idx) {
    workload::WorkloadGenerator gen;
    std::vector<workload::Operation> ops = gen.generateWorkload(0.5, NUM_ELEMENTS_TEST);
    executeWorkloadAssert(idx, ops);
    idx.clear();
}

/**
 * Inserts followed by lookups
 */
template <class Index>
void testMultiThreaded(Index &idx, int numThreads) {
    std::vector<int64_t> keys;
    std::vector<int64_t> values;
    std::vector<std::thread> threads; 

    generateRandomValues(NUM_ELEMENTS_MULTI_TEST, keys, values); 
    int numValuesPerThreads = NUM_ELEMENTS_MULTI_TEST/numThreads;
    std::atomic<int> insertFallbacks = 0;
     
    int i;
    for(i = 0; i < numThreads-1; i++) {
        threads.push_back(std::thread([&](int threadId){
            indexInsert<Index>(threadId, idx, threadId * numValuesPerThreads, (threadId+1) * numValuesPerThreads, keys, values, insertFallbacks);
        }, i));
    }

    threads.push_back(std::thread([&](int threadId){
        indexInsert<Index>(threadId, idx, threadId * numValuesPerThreads, keys.size(), keys, values, insertFallbacks);
    }, i));

    for(std::thread& t : threads) {
        t.join(); 
    }

    fprintf(stderr, "Done Inserting for Test \n");

    threads.clear();
    assert(idx.checkTree());

    for(i = 0; i < numThreads-1; i++) {
        threads.push_back(std::thread([&](int threadId){
            indexLookupAssert<Index>(threadId, idx, threadId * numValuesPerThreads, (threadId+1) * numValuesPerThreads, keys, values);
        }, i));
    }

    threads.push_back(std::thread([&](int threadId){
        indexLookupAssert<Index>(threadId, idx, threadId * numValuesPerThreads, keys.size(), keys, values);
    }, i));

    for(std::thread& t : threads) {
        t.join(); 
    }

    idx.clear();
}

/**
 * Interleaves inserts and lookups
 */
template <class Index>
void testMixedTreeMultiThreaded(Index& idx, int numThreads) {
    workload::WorkloadGenerator gen;
    std::vector<std::vector<workload::Operation>> ops = gen.generateParallelWorkload(0.5, 
                                                                                    NUM_ELEMENTS_MULTI_TEST,
                                                                                    numThreads);
    std::vector<std::thread> threads;

    for(int i = 0; i < numThreads; i++) {
        threads.push_back(std::thread([&](int threadId){
            executeWorkloadAssert(idx, ops[threadId]);
        }, i));
    }
    for(std::thread& t : threads) {
        t.join(); 
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

    double currElapsed = 0;
    int numValuesPerThreads = numOperations/numThreads; 
    std::atomic<int> insertFallbacks = 0;
    std::atomic<unsigned int> insertRetries[20] = {0};
    
    for(int run = 0; run < numRuns; run++) {
        Timer t;
        int i;
        for(i = 0; i < numThreads-1; i++) {
            threads.push_back(std::thread([&](int threadId){
                indexInsert<Index>(threadId, idx, threadId * numValuesPerThreads, (threadId+1) * numValuesPerThreads, keys, values, insertFallbacks);
                int *temp = idx.getInsertRetries();
                if(temp) {
                    for(int i = 0; i <= 17; i++) {
                        insertRetries[i].fetch_add(temp[i]);
                    }
                }
            }, i));
        }
        t.reset();
        
        int currThreadId = numThreads-1;
        indexInsert<Index>(currThreadId, idx, currThreadId * numValuesPerThreads, keys.size(), keys, values, insertFallbacks);
        for(std::thread& t : threads) {
            t.join(); 
        }

        double elapsed = t.elapsed(); 
        currElapsed += elapsed; 
        idx.clear();
        threads.clear();
    }

    printf("Insert Retries\n");
    for(int i = 0; i <= 17; i++) {   
        printf("%f\n", (float)(insertRetries[i].load()) / numRuns);
    }
    printf("Took Insert Fallback average %f times \n", ((float)insertFallbacks)/numRuns);
    printf("Average Execution Time: %.6fs \n", currElapsed/numRuns);

    return currElapsed/numRuns; 

}

/**
 * Benchmarks inserting multithreaded
 * returns the elapsed time
 */
template <class Index>
double multiLookupThreadedBenchmark(
    Index &idx, 
    int numThreads, 
    int numRuns,
    std::vector<int64_t>& keys,
    std::vector<int64_t>& values 
 ) {
    std::vector<std::thread> threads; 
    int numOperations = keys.size();
    std::atomic<int> insertFallbacks = 0;
    std::atomic<int> lookupFallbacks = 0;
    std::atomic<unsigned int> lookupRetries[20] = {0};

    double currElapsed = 0;
    int numValuesPerThreads = numOperations/numThreads; 
    indexInsert<Index>(0, idx, 0, numOperations, keys, values, insertFallbacks);
    for(int run = 0; run < numRuns; run++) {
        Timer t;
        int i;
        for(i = 0; i < numThreads-1; i++) {
            threads.push_back(std::thread([&](int threadId){
                indexLookup<Index>(threadId, idx, threadId * numValuesPerThreads, (threadId+1) * numValuesPerThreads, keys, values, lookupFallbacks);
                int *tempLookup = idx.getLookupRetries();
                if(tempLookup) {
                    for(int i = 0; i <= 17; i++) {
                       lookupRetries[i].fetch_add(tempLookup[i]);
                    }
                }
            }, i));
        }
        t.reset();
        
        int currThreadId = numThreads-1;
        indexLookup<Index>(currThreadId, idx, currThreadId * numValuesPerThreads, keys.size(), keys, values, lookupFallbacks);
        for(std::thread& t : threads) {
            t.join(); 
        }

        double elapsed = t.elapsed(); 
        currElapsed += elapsed;
        threads.clear();
    }
    
    printf("Lookup Retries\n");
    for(int i = 0; i <= 17; i++) {   
        printf("%f\n", (float)(lookupRetries[i].load()) / numRuns);
    }    
    
    printf("Took Lookup Fallback average %f times \n", ((float)lookupFallbacks)/numRuns);
    printf("Average Execution Time: %.6fs \n", currElapsed/numRuns);

    idx.clear();
    return currElapsed/numRuns; 

}


template <class Index>
double multiThreadedMixedBenchmark(
    Index &idx,
    int numRuns,
    std::vector<std::vector<workload::Operation>> workloads
) {
    std::vector<std::thread> threads;
    double currElapsed = 0;
    std::atomic<int> insertFallbacks = 0;
    std::atomic<int> lookupFallbacks = 0;
    std::atomic<unsigned int> insertRetries[20] = {0};
    std::atomic<unsigned int> lookupRetries[20] = {0};

    for(int run = 0; run < numRuns; run++){
        Timer t;
        for(int i = 0; i < workloads.size(); i++) {
            threads.push_back(std::thread([&](int threadId){
                executeWorkload(idx, workloads[threadId]);
                insertFallbacks.fetch_add(idx.getInsertFallbackTimes());
                lookupFallbacks.fetch_add(idx.getLookupFallbackTimes());
                int *tempInsert = idx.getInsertRetries();
                if(tempInsert) {
                    for(int i = 0; i <= 17; i++) {
                       insertRetries[i].fetch_add(tempInsert[i]);
                    }
                }
                int *tempLookup = idx.getLookupRetries();
                if(tempLookup) {
                    for(int i = 0; i <= 17; i++) {
                       lookupRetries[i].fetch_add(tempLookup[i]);
                    }
                } 
            }, i));
        }

        t.reset();
        for(std::thread& t : threads) {
            t.join(); 
        }
        double elapsed = t.elapsed();
        currElapsed += elapsed;
        threads.clear();
        idx.clear(); 
    }

    printf("Insert Retries\n");
    for(int i = 0; i <= 17; i++) {   
        printf("%f\n", (float)(insertRetries[i].load()) / numRuns);
    }
    
    printf("Lookup Retries\n");
    for(int i = 0; i <= 17; i++) {   
        printf("%f\n", (float)(lookupRetries[i].load()) / numRuns);
    }
    
    printf("Took Insert Fallback average %f times \n", ((float)insertFallbacks/numRuns));
    printf("Took Lookup Fallback average %f times \n", ((float)lookupFallbacks/numRuns));
    printf("Average Execution Time: %.6fs \n", currElapsed/numRuns);
    return currElapsed/numRuns;
}
/**
 * Interleaves inserts and lookups
 */
template <class Index>
void singleThreadedMixedBenchmark(Index& idx, std::vector<workload::Operation> ops) {
    executeWorkload(idx, ops);
    idx.clear();
}


template <class Index>
double singleThreadedInsertBenchmark(
    Index &idx, 
    std::vector<int64_t>& keys,
    std::vector<int64_t>& values,
    int numRuns  
 ) {
    std::vector<std::thread> threads; 
    std::atomic<int> insertFallbacks = 0;
    int numOperations = keys.size();
    double currElapsed = 0;
    Timer t; 
    for(int i = 0; i < numRuns; i++) {
        t.reset();
        indexInsert<Index>(0, idx, 0, numOperations, keys, values, insertFallbacks);

        double elapsed = t.elapsed(); 
        currElapsed += elapsed;
    }
    printf("Average Execution Time: %.6fs \n", currElapsed/numRuns);

    idx.clear();
    return currElapsed/numRuns; 
}

template <class Index>
double singleThreadedLookupBenchmark(
    Index &idx, 
    std::vector<int64_t>& keys,
    std::vector<int64_t>& values,
    int numRuns  
 ) {
    std::vector<std::thread> threads; 
    std::atomic<int> insertFallbacks = 0;
    std::atomic<int> lookupFallbacks = 0;
    
    int numOperations = keys.size();
    double currElapsed = 0;
    indexInsert<Index>(0, idx, 0, numOperations, keys, values, insertFallbacks);
    Timer t; 
    for(int i = 0; i < numRuns; i++) {
        t.reset();
        indexLookup<Index>(0, idx, 0, keys.size(), keys, values, lookupFallbacks);
        double elapsed = t.elapsed(); 
        currElapsed += elapsed;
    }
    printf("Average Execution Time: %.6fms \n", currElapsed/numRuns);

    idx.clear();
    return currElapsed/numRuns; 
}

void runInsertBenchmarks(int numThreads, int numOperations) {
    btreertm::BTree<int64_t, int64_t> idx_rtm(false);
    btreeolc::BTree<int64_t, int64_t> idx_olc;
    btreelocked::BTree<int64_t, int64_t> idx_locked;
    btreesinglethread::BTree<int64_t, int64_t> idx_single;
    std::vector<int64_t> keys;
    std::vector<int64_t> values;
    keys.reserve(numOperations);
    values.reserve(numOperations);

    fprintf(stdout, "Running Insert Benchmarks: threads: %d, operations: %d \n", numThreads, numOperations);


    generateRandomValues(numOperations, keys, values); 
    fprintf(stdout, "Running in %d threads \n", numThreads);

    fprintf(stdout, "Waming up cache: Benchmarking idx_olc \n");
    multiInsertThreadedBenchmark(idx_olc, numThreads, 2, keys, values); 
    fprintf(stdout, "Done Warming up the caches! \n");

    fprintf(stdout, "Benchmarking Multithreaded idx_rtm \n");
    multiInsertThreadedBenchmark(idx_rtm, numThreads, 5, keys, values);

    fprintf(stdout, "Benchmarking idx_olc \n");
    multiInsertThreadedBenchmark(idx_olc, numThreads, 5, keys, values); 

    fprintf(stdout, "Benchmarking idx_locked \n");
    multiInsertThreadedBenchmark(idx_olc, numThreads, 2, keys, values); 

    fprintf(stdout, "Benchmarking idx_single single threaded \n");
    singleThreadedInsertBenchmark(idx_single, keys, values, 5); 
    fprintf(stdout, "------------------------------ \n"); 
}

void runLookupBenchmarks(int numThreads, int numOperations) {
    btreertm::BTree<int64_t, int64_t> idx_rtm(false);
    btreeolc::BTree<int64_t, int64_t> idx_olc;
    btreelocked::BTree<int64_t, int64_t> idx_locked;
    btreesinglethread::BTree<int64_t, int64_t> idx_single;
    std::vector<int64_t> keys;
    std::vector<int64_t> values;
    keys.reserve(numOperations);
    values.reserve(numOperations);

    fprintf(stdout, "Running Lookup Benchmarks: threads: %d, operations: %d \n", numThreads, numOperations);


    generateRandomValues(numOperations, keys, values); 
    fprintf(stdout, "Running in %d threads \n", numThreads);

    fprintf(stdout, "Waming up cache: Benchmarking idx_olc \n");
    multiLookupThreadedBenchmark(idx_olc, numThreads, 2, keys, values); 
    fprintf(stdout, "Done Warming up the caches! \n");

    fprintf(stdout, "Benchmarking Multithreaded idx_rtm \n");
    multiLookupThreadedBenchmark(idx_rtm, numThreads, 5, keys, values);

    fprintf(stdout, "Benchmarking idx_olc \n");
    multiLookupThreadedBenchmark(idx_olc, numThreads, 5, keys, values); 

    fprintf(stdout, "Benchmarking idx_locked \n");
    multiLookupThreadedBenchmark(idx_locked, numThreads, 2, keys, values); 

    fprintf(stdout, "Benchmarking idx_single single threaded \n");
    singleThreadedLookupBenchmark(idx_single, keys, values, 5); 
    fprintf(stdout, "------------------------------ \n"); 
}

void runMixedBenchmarks(int numThreads, int numOperations, double percentInsert) {
    btreertm::BTree<int64_t, int64_t> idx_rtm(false);
    btreeolc::BTree<int64_t, int64_t> idx_olc;
    btreelocked::BTree<int64_t, int64_t> idx_locked;
    btreesinglethread::BTree<int64_t, int64_t> idx_single;

    fprintf(stdout, "Multi threaded mixed benchmark, numThreads: %d, numOperations: %d, percentInsert: %f \n", 
        numThreads, numOperations, percentInsert);

    workload::WorkloadGenerator generator;
    std::vector<std::vector<workload::Operation>> workloads = 
        generator.generateParallelWorkload(percentInsert, numOperations, numThreads);

    fprintf(stdout, "Waming up cache: Benchmarking idx_olc \n");
    multiThreadedMixedBenchmark(idx_olc, 2, workloads);

    fprintf(stdout, "Running multithreaded idx_rtm mixed benchmark \n");
    multiThreadedMixedBenchmark(idx_rtm, 5, workloads);

    fprintf(stdout, "Running multithreaded idx_olc mixed benchmark \n");
    multiThreadedMixedBenchmark(idx_olc, 5, workloads);

    fprintf(stdout, "Running multithreaded idx_locked mixed benchmark \n");
    multiThreadedMixedBenchmark(idx_locked, 2, workloads);

    fprintf(stdout, "Running singlethreaded benchmark \n");
    Timer t;
    for(int i = 0; i < workloads.size(); i++) {
        executeWorkload(idx_single, workloads[i]);
    }
    double elapsed = t.elapsed(); 

    fprintf(stdout, "Execution Time: %.6fs \n", elapsed);
    fprintf(stdout, "------------------------------- \n");
}

void runOLCTests(int numThreads) {
    btreeolc::BTree<int64_t, int64_t> idx_olc;

    fprintf(stderr,"Testing Single Threaded idx_olc");
    testTreeSingleThreaded(idx_olc); 

    fprintf(stderr,"Testing Single Threaded Mixed idx_olc \n");
    testMixedTreeSingleThreaded(idx_olc);

    fprintf(stderr, "Testing Inserts following by Looksups idx_olc \n");
    testMultiThreaded<btreeolc::BTree<int64_t, int64_t>>(idx_olc, numThreads);

    fprintf(stderr,"Testing MultiThreaded Mixed idx_olc \n");
    testMixedTreeMultiThreaded<btreeolc::BTree<int64_t, int64_t>>(idx_olc, numThreads); 

    fprintf(stderr, "---------------------------------\n");
}

void runLockedTests(int numThreads) {
    btreelocked::BTree<int64_t, int64_t> idx_locked;

    fprintf(stderr,"Testing Single Threaded idx_locked \n");
    testTreeSingleThreaded(idx_locked); 

    fprintf(stderr,"Testing Single Threaded Mixed idx_locked \n");
    testMixedTreeSingleThreaded(idx_locked);

    fprintf(stderr, "Testing multi threaded Inserts following by Looksups idx_locked \n");
    testMultiThreaded<btreelocked::BTree<int64_t, int64_t>>(idx_locked, numThreads);

    fprintf(stderr,"Testing MultiThreaded Mixed idx_locked \n");
    testMixedTreeMultiThreaded<btreelocked::BTree<int64_t, int64_t>>(idx_locked, numThreads); 

    fprintf(stderr, "---------------------------------\n");
}

void runRTMTests(int numThreads) {
    btreertm::BTree<int64_t, int64_t> idx_rtm(false);
    fprintf(stderr,"Testing Single Threaded idx_rtm \n");
    testTreeSingleThreaded(idx_rtm);

    fprintf(stderr,"Testing Single Threaded Mixed idx_rtm \n");
    testMixedTreeSingleThreaded(idx_rtm);

    fprintf(stderr, "Testing Inserts following by Looksups idx_rtm \n");
    testMultiThreaded<btreertm::BTree<int64_t, int64_t>>(idx_rtm, numThreads);

    fprintf(stderr,"Testing MultiThreaded Mixed idx_rtm \n");
    testMixedTreeMultiThreaded<btreertm::BTree<int64_t, int64_t>>(idx_rtm, numThreads);

    fprintf(stderr, "---------------------------------\n");
}

void runRTMWeavedTests(int numThreads) {
    btreertm::BTree<int64_t, int64_t> idx_weaved(true);
    fprintf(stderr,"Testing Single Threaded idx_weaved \n");
    testTreeSingleThreaded(idx_weaved);

    fprintf(stderr,"Testing Single Threaded Mixed idx_weaved \n");
    testMixedTreeSingleThreaded(idx_weaved);

    fprintf(stderr, "Testing Inserts following by Looksups idx_olc \n");
    testMultiThreaded<btreertm::BTree<int64_t, int64_t>>(idx_weaved, numThreads);

    fprintf(stderr,"Testing MultiThreaded Mixed idx_rtm \n");
    testMixedTreeMultiThreaded<btreertm::BTree<int64_t, int64_t>>(idx_weaved, numThreads);

    fprintf(stderr, "---------------------------------\n"); 
}

void runFallBackCountsBenchmarks(int numThreads, int numOperations, double percentInsert) {
    btreertm::BTree<int64_t, int64_t> idx_weaved(true);
    btreertm::BTree<int64_t, int64_t> idx_rtm(false);
    btreesinglethread::BTree<int64_t, int64_t> idx_single;


    fprintf(stdout, "Running fallback counts benchmark: numThreads: %d, numOperations: %d, percentInsert: %f \n", 
        numThreads, numOperations, percentInsert);
    
    if(percentInsert >= 1.0 || percentInsert <= 0.0) {
        std::vector<int64_t> keys;
        std::vector<int64_t> values;
        keys.reserve(numOperations);
        values.reserve(numOperations); 

        generateRandomValues(numOperations, keys, values); 

        if(percentInsert >= 1.0) {
            fprintf(stdout, "Waming up cache: Benchmarking idx_rtm \n");
            multiInsertThreadedBenchmark(idx_rtm, numThreads, 2, keys, values); 
            fprintf(stdout, "Done Warming up the caches! \n");

            fprintf(stdout, "Benchmarking Multithreaded insert only idx_weaved \n");
            multiInsertThreadedBenchmark(idx_weaved, numThreads, 5, keys, values);

            fprintf(stdout, "Benchmarking Multithreaded insert only idx_rtm \n");
            multiInsertThreadedBenchmark(idx_rtm, numThreads, 5, keys, values); 

            fprintf(stdout, "Benchmarking single threaded insert only idx_single \n");
            singleThreadedInsertBenchmark(idx_single, keys, values, 5); 
            fprintf(stdout, "------------------------------ \n"); 
        } else {
            fprintf(stdout, "Waming up cache: Benchmarking idx_rtm \n");
            multiLookupThreadedBenchmark(idx_rtm, numThreads, 2, keys, values); 
            fprintf(stdout, "Done Warming up the caches! \n");

            fprintf(stdout, "Benchmarking Multithreaded lookup only idx_weaved \n");
            multiLookupThreadedBenchmark(idx_weaved, numThreads, 5, keys, values);

            fprintf(stdout, "Benchmarking Multithreaded lookup only idx_rtm \n");
            multiLookupThreadedBenchmark(idx_rtm, numThreads, 5, keys, values); 

            fprintf(stdout, "Benchmarking single threaded lookup only idx_single \n");
            singleThreadedLookupBenchmark(idx_single, keys, values, 5); 
            fprintf(stdout, "------------------------------ \n"); 
        }

        return;
    }

    workload::WorkloadGenerator generator;
    std::vector<std::vector<workload::Operation>> workloads = 
        generator.generateParallelWorkload(percentInsert, numOperations, numThreads);

    fprintf(stdout, "Waming up cache: Benchmarking idx_olc \n");
    multiThreadedMixedBenchmark(idx_rtm, 2, workloads);

    fprintf(stdout, "Running fallback counts benchmark index weaved \n");
    multiThreadedMixedBenchmark(idx_weaved, 5, workloads);

    fprintf(stdout, "Running fallback counts benchmark index rtm\n");
    multiThreadedMixedBenchmark(idx_rtm, 5, workloads);

    fprintf(stdout, "Running fallback counts benchmark index single");
    Timer t;
    for(int i = 0; i < workloads.size(); i++) {
        executeWorkload(idx_single, workloads[i]);
    }
    double elapsed = t.elapsed(); 

    fprintf(stdout, "Execution Time: %.6fs \n", elapsed);
    fprintf(stdout, "------------------------------- \n");
}

void runRetryCountsBenchmarks(int numThreads, int numOperations, double percentInsert) {
    btreertm::BTree<int64_t, int64_t> idx_rtm(false);

    fprintf(stdout, "Running fallback counts benchmark: numThreads: %d, numOperations: %d, percentInsert: %f \n", 
        numThreads, numOperations, percentInsert);
    
    if(percentInsert >= 1.0 || percentInsert <= 0.0) {
        std::vector<int64_t> keys;
        std::vector<int64_t> values;
        keys.reserve(numOperations);
        values.reserve(numOperations); 

        generateRandomValues(numOperations, keys, values); 

        if(percentInsert >= 1.0) {
            fprintf(stdout, "Waming up cache: Benchmarking idx_rtm \n");
            multiInsertThreadedBenchmark(idx_rtm, numThreads, 2, keys, values); 
            fprintf(stdout, "Done Warming up the caches! \n");

            fprintf(stdout, "Benchmarking Multithreaded insert only idx_rtm \n");
            multiInsertThreadedBenchmark(idx_rtm, numThreads, 5, keys, values); 

            fprintf(stdout, "------------------------------ \n"); 
        } else {
            fprintf(stdout, "Waming up cache: Benchmarking idx_rtm \n");
            multiLookupThreadedBenchmark(idx_rtm, numThreads, 2, keys, values); 
            fprintf(stdout, "Done Warming up the caches! \n");

            fprintf(stdout, "Benchmarking Multithreaded lookup only idx_rtm \n");
            multiLookupThreadedBenchmark(idx_rtm, numThreads, 5, keys, values); 

            fprintf(stdout, "------------------------------ \n"); 
        }

        return;
    }

    workload::WorkloadGenerator generator;
    std::vector<std::vector<workload::Operation>> workloads = 
        generator.generateParallelWorkload(percentInsert, numOperations, numThreads);

    fprintf(stdout, "Waming up cache: Benchmarking idx_rtm \n");
    multiThreadedMixedBenchmark(idx_rtm, 2, workloads);

    fprintf(stdout, "Running fallback counts benchmark index rtm\n");
    multiThreadedMixedBenchmark(idx_rtm, 5, workloads);

    fprintf(stdout, "------------------------------- \n");
}

int main(int argc, char *argv[]) {
    int numThreads = MULTI_NUM_THREADS; 
    double percentInsert = 0.5;
    char *temp;
    if(argc > 1) {
        numThreads = strtol(argv[1], &temp, 10);
    }

    if(argc > 2) {
        percentInsert = atof(argv[2]);
    }

    //runRTMTests(10);
    //runRTMWeavedTests(10);

    runRetryCountsBenchmarks(numThreads, NUM_ELEMENTS_MULTI, 0.0);
    runRetryCountsBenchmarks(numThreads, NUM_ELEMENTS_MULTI, 0.25);
    runRetryCountsBenchmarks(numThreads, NUM_ELEMENTS_MULTI, 0.5);
    runRetryCountsBenchmarks(numThreads, NUM_ELEMENTS_MULTI, 0.75);
    runRetryCountsBenchmarks(numThreads, NUM_ELEMENTS_MULTI, 1.0);
  
}
