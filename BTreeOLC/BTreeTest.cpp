#include "BTreeOLC.h"
#include "BTree_single_threaded.h"
#include <cassert>
#include <iostream>


int main() {
    btreesinglethread::BTree<int64_t, int64_t> idx;
    int64_t keys[] = {10, 20 , 30, 40, 50, 60};
    int64_t values[] = {100, 200 , 300, 400, 500, 600};

    for(auto i = 0; i < 6; i++){
        idx.insert(keys[i], values[i]);
    } 

    for(int i = 0; i < 6; i++){
        int64_t result; 
        assert(idx.lookup(keys[i], result));
        assert(result == values[i]);
    }
}