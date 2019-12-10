#  Midpoint Report: B+Trees for In-Memory Databases using Hardware Transactional Memory

## Summary
We implemented an optimized B+Tree using restricted transactional memory (RTM) and compared the performance with a more traditional optimistic lock coupling approach. Our deliverables include graphs and performance analysis indicating the benefits and drawbacks of RTM vs optimistic locking along with our HTM B+Tree implementation. We ran our experiments on a 2-socket machine that supports Intel® TSX® with two Intel® Xeon® Silver 4114 processors with 10 cores and 20 hyperthreads each. 

## Background
B+Tree are in memory data structures that are used to index data in databases and file systems. For this project we focused on their uses cases in in-memory databases. B+Tree are a self balancing tree where each node can have at most n children. They ensure balancing by having an invariant that all leaves are the same distance from the root node. B+Trees (Figure 2) are different from B-Trees (Figure 1)  in that in B+Tree inner nodes only store keys used to navigate down the tree. The value associated with the keys is stored within the leaf nodes, and all keys are represented within the leaf nodes. To allow for efficient lookup the keys within each node are sorted in increasing order. This way when traversing the tree at each node the subtree containing a key can be retrieved by doing a binary search on the ranges of the children.

![B-Tree](images/BTree.png)
![B+Tree](images/B+Tree.png)
