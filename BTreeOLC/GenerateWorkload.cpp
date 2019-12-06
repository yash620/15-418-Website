#include "WorkloadGenerator.h"
#include <vector>
#include <stdio.h>

int main() {
    workload::WorkloadGenerator gen; 
    std::vector<workload::Operation> workload = gen.generateWorkload(0.5, 100);
    for(workload::Operation op : workload){
        printf("OpType: %s, Key: %lld, Value: %lld \n", ((op.type) ? "Lookup" : "Insert"), op.key, op.value);
    }
}