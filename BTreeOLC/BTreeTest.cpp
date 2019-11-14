#include "BTreeOLC.h"
#include "BTree_single_threaded.h"
#include "BTree_rtm.h"
#include <cassert>
#include <vector>
#include <time.h>

#define NUM_ELEMENTS 500

int main() {
    btreertm::BTree<int64_t, int64_t> idx;

    std::vector<int64_t> keys;
    std::vector<int64_t> values;

    for(int i = 0; i < NUM_ELEMENTS; i++) {
        keys.push_back(rand() % 10000);
        values.push_back(rand() % 10000);
    }

    for(auto i = 0; i < NUM_ELEMENTS; i++){
        idx.insert(keys[i], values[i]);
    }

    assert(idx.checkTree());
    for(int i = 0; i < NUM_ELEMENTS; i++){
        int64_t result;
        assert(idx.lookup(keys[i], result));
        assert(result == values[i]);
    }
}
