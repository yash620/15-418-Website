#  Midpoint Report: Indexes for In-Memory Databases using Hardware Transactional Memory

## Progress
We are currently working on implementing an efficient B-Tree that uses hardware transactional memory (HTM)  to speed up performance. This has proven to be more challenging than we initially imagined. We started off with an optimistic latch coupling (OLC) based B-Tree implementation and are working on transforming it into an HTM based one. We believed the OLC version would be the best starting point and comparison to an HTM implementation as the HTM behavior is similar to the OLC behavior. At the current point we have basic implementations of parallel insert and lookup using HTM. 

We are focused now on optimizing the insert portion.  As part of this optimization we are also building out a testing and benchmarking suite. We believe these suites can speed up our optimization processes for other operations as we would have the benchmarking utilities. Once we are finished with optimizing insert we will look to move on to optimizing lookup operations. 

## Challenges So Far
Using and adapting to TSX has been a challenge. Initially it was difficult to find an environment that could run TSX. This was further challenging given recent discoveries by Intel of TSX security vulnerabilities which has led to many systems turning off TSX capabilities in recent updates. Furthermore we had challenges using the restricted transactional memory (RTM) operations provided by TSX. Often times, even in single threaded tests where there was no contention the TSX transactions would abort and reverting to a fallback locked path. Theoretically this would rarely have to happen in low contention cases, and it took us time to understand the behaviour of TSX and in what situations it could abort. Another challenge was the interaction between the transactional and locked code paths. While some threads were executing the transactional code path, other threads which have had their transactions aborted would take the locked code path. But there are certain interleaving of threads that could lead to race conditions between transactional code paths and locked code paths which has been a challenge for us to address.

## Goals and Deliverables
Based on our current progress we believe we will be able to complete a B-Tree HTM implementation for this project.  As this has proven to be more challenging than anticipated we haven’t had the chance to begin the Skip List portion. We expect that the experience we get from the B-Tree implementation will allow us to more quickly develop the Skip List HTM implementation. Nevertheless, given how we are behind our initial schedule we don’t think the nice to have of optimizing the ART Index would be likely to be completed. The deliverables we hope to achieve by the end of the semester remain the HTM B-Tree implementation, HTM Skip List implementation and an analysis of their performances compared to traditional locking based implementations. For the poster presentation we hope to show graphs comparing the performances of the HTM based implementations vs the traditional locked based approaches. 

## Planned Schedule

| Week  | Goal                                                  |
| ------| ----------------------------------------------------- |
| 11/17 - 11/23     | Finish HTM based B-Tree implementation     |
| 11/24 - 11/30     | Implement Skip List HTM|
| 12/1 - 12/7       | Finish Skip List HTM implementation and analyze performance of B-Tree and Skip List       |
| 12/7 - 12/11      | Create poster     |
