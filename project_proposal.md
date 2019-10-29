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
- Implement B-Tree
- Implement B-Tree using HTM
- Implement Skip List
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

# References
