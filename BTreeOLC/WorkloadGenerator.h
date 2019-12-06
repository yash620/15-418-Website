#include <vector>
#include <time.h>
#include <iostream>
#include <algorithm>
#include <thread>
#include <random>
#include <unordered_map>
#include <float.h>

namespace workload {
enum OpType : int { Insert=0, Lookup=1 };

struct Operation {
    OpType type;
    int64_t key;
    int64_t value;

    Operation(OpType type_, int64_t key_, int64_t value_) :
        type(type_), key(key_), value(value_) {}
};

struct WorkloadGenerator {
    private:
        void generateRandomValues(
            int64_t numValues,
            std::vector<int64_t>& keys,
            std::vector<int64_t>& values,
            int64_t keysStartValue = 0
        ) {
            std::random_device rd;
            std::default_random_engine eng {rd()};
            std::uniform_int_distribution<int64_t> dist(keysStartValue, keysStartValue + numValues * 100); 

            for(int64_t i = 0; i < numValues; i++) {
                keys.push_back(i + keysStartValue);
                values.push_back(dist(eng));
            }

            std::shuffle(std::begin(keys), std::end(keys), eng);
        }

    public:
        /**
         * Generates a vector of index operations
         * @param percentInsert percent chance that each operation is an insert 
         * @return vector of operations that are the workload
         */
        std::vector<Operation> generateWorkload(double percentInsert, int numOperations, int64_t keysStartValue = 0){
            std::vector<Operation> operations; 
            std::vector<int64_t> keys;
            std::vector<int64_t> values;
            generateRandomValues(numOperations, keys, values, keysStartValue);
            srand(time(NULL));

            std::random_device rd;
            std::default_random_engine eng {rd()};
            std::binomial_distribution<int> opTypeDistribution(1, 1-percentInsert); 

            operations.emplace_back(OpType::Insert, keys[0], values[0]);
            int numInserted = 1;
            for(int i = 1; i < numOperations; i++) {
                int opType = opTypeDistribution(eng);
                if(opType == OpType::Insert) {
                    operations.emplace_back(OpType::Insert, keys[numInserted], values[numInserted]);
                    numInserted++;
                } else {
                    int lookUpIndex = rand() % numInserted;
                    operations.emplace_back(OpType::Lookup, keys[lookUpIndex], values[lookUpIndex]);
                }
            }

            return operations;
        }

        /**
         * Generates Parallel workload where each vector of operations returned can be executed by one thread
         */
        std::vector<std::vector<Operation>> generateParallelWorkload(
            double percentInsert, 
            int numOperations, 
            int numThreads
        ) {
            
            int operationsPerThread = numOperations / numThreads; 
            std::vector<std::vector<Operation>> workloads;

            for(int i = 0; i < numThreads-1; i++) {
                workloads.push_back(generateWorkload(percentInsert, operationsPerThread, operationsPerThread * i));
            }

            workloads.push_back(generateWorkload(percentInsert, 
                                                 numOperations - (operationsPerThread * (numThreads-1)), 
                                                 operationsPerThread * (numThreads - 1)));

            return workloads; 
        }

};
}

