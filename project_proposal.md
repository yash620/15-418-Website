# Indexes for In-Memory Databases using Hardware Transactional Memory

## Summary

We plan to build indexes for in-memory databases that focus on improving performance in contentious workloads using hardware transactional memory. We will initially start by exploring improvement in B-Trees then move on to other database indexes, such as ART Indexes and Skip Lists, time permitting. 

## Background

Databases are expected to provide performant access and analytics capabilities on large collections of data. In order to do so they construct indexes on specific attributes of the data to be able to more directly access only the data being requested. Furthermore most databases provide several degrees of parallelism when accessing data. As a result, these indexes have to provide concurrency guarantees that allow multiple threads to interact with them at the same time. 

Traditionally B-Trees were used as database indexes as they were optimized for disk access times. But as in-memory database systems the contention between threads became a more prominent overhead as disk access was no longer a concern. Various research avenues have explored constructing novel data structures to alleviate this problem such as ART Indexes, T-Trees, and Mass Trees. There have also been attempts to create a lock-free approach with the BW-Tree, but that has been shown to be not as performant as other state of the art indexes <sup>[1]</sup>. 

Hardware transactional memory (HTM) leads to a new avenue of optimizations to these indexes as it can be used to improve performance in highly contentious workloads. By providing transactional semantics on memory accesses HTM allows potential for more optimistic concurrency management approaches. HTM comes in two variants, Hardware Lock Elision (HLE) and Restricted Transactional Memory (RTM). Through hardware lock elision the HTM model optimistically tries to execute a transaction without taking a lock, and only when it fails to do so will it take a lock to execute the operation or try another fallback approach. We believe that this optimistic concurrency scheme can speed up the performance of current locking database indexes. 



## Challenge
The initial challenge is understanding the patterns of contention with different database indexes. The second challenge builds off of this in that we would need to come up with effective ways to adapt HTM capabilities to these data structures. Due to the potentially highly contentious nature of certain database workloads we fear that longer running transactional memory operations would fail, so we would have to create a scheme that would allow progress without always falling back to a locked approach. Furthermore we would need to adapt the memory footprint of index operations so that they fit within the HTM constraints.

## Resources
The main resource we would need are HTM capable machines. We believe most modern Intel processors have Intel TSX capabilities and thus can run benchmarks on our own laptops. But in cases our laptops lack TSX support we would reach out to Professor Railing for access to a machine with HTM support. For implementing the B-Tree and Skip List we would like to start from scratch, but for the more complicated indexes we will look for existing implementations that we can adapt to fit the HTM model.  

## Goals and Deliverables

### Plan to Achieve
- Implement B-Tree using HTM
- Implement Skip List using HTM
- Evaluate HTM B-Tree and HTM Skip List performance
- Compare HTM B-tree and HTM Skip List performance to state of the art database indexes

### Hope to Achieve
- Adapt ART Index to use HTM
- Evaluate HTM ART Index performance

For the poster session we would like to show the performance analysis and the speedup of the implementations with HTM. 

# Platform Choice

We plan to implement these data structures in C++ as there are many C++ compilers with support for Intel TSX primitives. Furthermore C++ will provide us with more lower level constructs that will allow a more hands-on memory management approach which will make it more possible for us to fit the memory constraints of HTM operations. 

# Schedule

| Week  | Goal                                                  |
| ------| ----------------------------------------------------- |
| 1     | Implement B-Tree, Implement HTM optimized B-Tree      |
| 2     | Implement Skip List, Implement HTM optimized Skip List|
| 3     | Analyze performance of Skip List and B-Tree           |
| 4     | Adapt ART Index for HTM and evaluate performance      |
| 5     | Finalize implementations and evaluation results       |

# References

[1] Wang, Ziqi., et al. “Building a Bw-Tree Takes More Than Just Buzz Words.” SIGMOD '18 Proceedings of the 2018 International Conference on Management of Data, pp. 473–488 .
[2] Makreshanski, Darko., et al. “To Lock, Swap, or Elide: On the Interplay of Hardware Transactional Memory and Lock-Free Indexing.” VLDB Endowment, 41st International Conference on Very Large Data Bases (VLDB 2015): Proceedings of the VLDB Endowment, Volume 8, Number 1-13, Kohala Coast, Hawaii, USA, 31 August-4 September 2015, 2015, pp. 1298–1309.
[3] Brown, Trevor. “A Template for Implementing Fast Lock-Free Trees Using HTM.”
[4] Leis, Viktor, et al. “The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases.” 2013 IEEE 29th International Conference on Data Engineering (ICDE).

