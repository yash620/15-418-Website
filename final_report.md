#  Midpoint Report: B+Trees for In-Memory Databases using Hardware Transactional Memory

## Summary
We implemented an optimized B+Tree using restricted transactional memory (RTM) and compared the performance with a more traditional optimistic lock coupling approach. Our deliverables include graphs and performance analysis indicating the benefits and drawbacks of RTM vs optimistic locking along with our HTM B+Tree implementation. We ran our experiments on a 2-socket machine that supports Intel® TSX® with two Intel® Xeon® Silver 4114 processors with 10 cores and 20 hyperthreads each. 

## Background
B+Tree are in memory data structures that are used to index data in databases and file systems. For this project we focused on their uses cases in in-memory databases. B+Tree are a self balancing tree where each node can have at most n children. They ensure balancing by having an invariant that all leaves are the same distance from the root node. B+Trees (Figure 2) are different from B-Trees (Figure 1)  in that in B+Tree inner nodes only store keys used to navigate down the tree. The value associated with the keys is stored within the leaf nodes, and all keys are represented within the leaf nodes. To allow for efficient lookup the keys within each node are sorted in increasing order. This way when traversing the tree at each node the subtree containing a key can be retrieved by doing a binary search on the ranges of the children.

![B-Tree](images/BTree.png)
*Figure 1: B-Tree*
![B+Tree](images/B+Tree.png)
*Figure 2: B+Tree*

Within in-memory databases B+Trees are used to build indexes on tables. Indexes are intended to allow for efficient data retrieval based on specific attributes of the tuples contained in the table. For this use case the keys are attributes within the tuple and the value is an address pointing to the location of the tuple. Database indexes provide operations such as insert, lookup, delete, and scan but for this project we focused on optimizing insert and lookup. 

Insert takes a key and a value as parameters and traverses the tree to insert key at the bottom of the tree. When inserting though the algorithm has to ensure that no node exceeds its maximum number of children, which it does so by splitting full nodes. A node split involves creating a new node, moving half the keys in the splitting node into the new node, and then adding the new node as a child of the parent. If the root node is split then a new root node is created as well and the old root node and it’s split node become children of the new root node. There are two common approaches of when to split, eager and lazy splitting. In an eager splitting approach the program traversing down the tree will split a node whenever it detects that the node is at capacity. In a lazy approach the key being inserted will be inserted at a leaf and then the leaf is split only if it exceeds capacity and then ancestor nodes are split accordingly if they are full as well. 

Lookups are more straightforward in that they don’t make any modifications. Lookup takes a key as a parameter and returns the value associated with the key. The function traverses the tree, doing a binary search at each node to find the corresponding subtree to traverse down. Once it reaches a leaf node it returns the value associated with the key found there. 

Database workloads often involve querying the index multiple times and launching a query doesn’t have data dependencies on launching other queries. Therefore it is possible to parallelize the workload by running multiple queries on an index concurrently. With inter-query parallelism lookups are dependent on the current node the program is operating on remains unchanged. Inserts, with eager splitting, only require that the current node and it’s parent must not be modified. The parent is a dependency since when splitting a new child is inserted into the parent. With lazy splitting all of the ancestors of the leaf node could be modified, as a split can recursively cause splits all the way up to the root node. Intra query parallelization is difficult since identifying the next node on which to continue the traversal requires finishing a binary search on the current node.  

Within a query there isn’t much cache locality as each step involves a node which is a new data location. Across queries though it is possible to improve cache locality by reordering queries in a way that queries that taking similar paths down the tree would be grouped together. But this would require complex static analysis identifying possibilities for index operation reordering without breaking cross query data dependencies and it would also require knowledge of the operations being run before running them. For this project though we chose to focus more on online operations, where not all the operations are known beforehand, and on parallelizing the data structure rather than on analysis and reordering of the inputs. 

## Approach
### Initial Implementation
We started off with an optimistic lock coupling (OLC) based [approach](https://github.com/wangziqi2016/index-microbench/blob/master/BTreeOLC/BTreeOLC.h) by Dr. Viktor Leis from Technische Universität München. Lock coupling involves holding crabbing down the tree while holding two locks (the locks for the parent and the current node) at a time. Whenever moving on from a node to a child the lock on the parent is released and the lock for the child is grabbed. Optimistic means that readers don’t block or take locks, instead when they would normally release a lock they would verify if the version of the node matches the version they initially read. If the versions changed then they would retry the operation again as the data has changed. Writers do take locks on the node and when they release the lock they update a version counter to indicate that the data was modified. On lookups only read locks would be taken. On writes, write locks only need to be taken when splitting a node or when inserting into the leaf node. For the rest of the traversal only read locks are needed.

We started off with this implementation as OLC is one of the most common parallel implementations for B-Trees and it takes a similar approach to that of restricted transactional memory (RTM). Intel TSX, the RTM instruction set we used, provides three instructions to enable transactionality: 
1. _xbegin: begins a transactional region
2. _xend: ends a transactional region
3. _xabort: aborts the current running transaction.




