CC = gcc 
CXX = g++ -std=gnu++11
CFLAGS = -g -O3 -Wno-invalid-offsetof -mcx16 -DBWTREE_NODEBUG 

test: BTreeTest.cpp BTreeOLC.h BTree_single_threaded.h
	$(CXX) $(CFLAGS) -DDEBUG -o BTreeTest.out BTreeTest.cpp

clean: 
	rm -rf *.out
	rm -rf *.dSYM
