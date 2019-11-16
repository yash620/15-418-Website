CC = gcc 
CXX = g++ -std=gnu++17
CFLAGS = -g -O3 -Wno-invalid-offsetof -mcx16 -DBWTREE_NODEBUG -mrtm -pthread

test: BTreeTest.cpp BTreeOLC.h BTree_single_threaded.h
	$(CXX) $(CFLAGS) -DDEBUG -o BTreeTest.out BTreeTest.cpp

clean: 
	rm -rf *.out
	rm -rf *.dSYM
