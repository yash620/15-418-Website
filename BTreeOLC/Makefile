CC = gcc 
CXX = g++ -std=gnu++17
CFLAGS = -g -O3 -Wno-invalid-offsetof -mcx16 -DBWTREE_NODEBUG -mrtm -pthread
DEBUGFLAGS = -g -O0 -Wno-invalid-offsetof -mcx16 -mrtm -pthread

test: BTreeTest.cpp BTreeOLC.h BTree_single_threaded.h WorkloadGenerator.h
	$(CXX) $(CFLAGS) -DDEBUG -o BTreeTest.out BTreeTest.cpp

debug: BTreeTest.cpp BTreeOLC.h BTree_single_threaded.h WorkloadGenerator.h
	$(CXX) $(DEBUGFLAGS) -DDEBUG -o BTreeTest.out BTreeTest.cpp

workload: GenerateWorkload.cpp WorkloadGenerator.h
	$(CXX) $(CFLAGS) -DDEBUG -o GenerateWorkload.out GenerateWorkload.cpp

benchmark: test 
	for threads in 40 1 2 4 8 16 32 ; do \
		echo "---------------------------NEW RUN-----------------------\n"  ; \
		./BTreeTest.out $$threads 1>> ../transaction_surrounding_call_benchmark.out ; \
	done

clean: 
	rm -rf *.out
	rm -rf *.dSYM
	rm -rf *.data
	rm -rf *.old
