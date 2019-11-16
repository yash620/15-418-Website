//#include "BTreeOLC.h"
#include "BTreeOLC.h"
#include "BTree_single_threaded.h"
#include "BTree_rtm.h"
#include <cassert>
#include <vector>
#include <time.h>
#include <iostream>
#include <algorithm>

#define NUM_ELEMENTS 500

int main() {
    btreertm::BTree<int64_t, int64_t> idx;

    srand(time(NULL));
    std::vector<int64_t> keys;
    std::vector<int64_t> values;

    for(int i = 0; i < NUM_ELEMENTS; i++) {
        int64_t randKey = rand() % 30000;
        while(std::find(keys.begin(), keys.end(), randKey) != keys.end()) {
            randKey = rand() % 30000;
        }

        keys.push_back(randKey);
        values.push_back(rand() % 30000);
        //keys.push_back(i);
        //values.push_back(i * 10);
    }

    fprintf(stderr, "Got here");
    for(auto i = 0; i < NUM_ELEMENTS; i++){
        fprintf(stderr, "Inserted %d \n", i);
        idx.insert(keys[i], values[i]);
    }

    assert(idx.checkTree());
    for(int i = 0; i < NUM_ELEMENTS; i++){
        int64_t result;
        assert(idx.lookup(keys[i], result));
        if(result != values[i]) {
            printf("Looking up: %lld \n", keys[i]);
            printf("Result %lld, value %lld \n", result, values[i]);
        }
        assert(result == values[i]);
    }
}
