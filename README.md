# Concurrent Memory Pool

A high-performance, thread-safe memory allocator in C++17, inspired by the design of Google's tcmalloc. Built from scratch to explore how a three-tier cache hierarchy can reduce lock contention under high thread counts.

**Timeline:** Feb 2026 – Apr 2026 · **Language:** C++17 · **Platform:** Linux (x86-64)

## Overview

This project implements a three-tier memory pool — `ThreadCache` → `CentralCache` → `PageCache` — that serves allocations up to 256 KiB. Requests are rounded to one of **208 size classes**, and the `PageCache` manages `Span` splitting and coalescing on page boundaries.

The design goal is to keep the common allocation path entirely lock-free per thread, and to touch shared structures only in batches.

## Architecture

```
   Thread 1        Thread 2        Thread N
      |               |               |
 +----------+    +----------+    +----------+
 |ThreadCache|   |ThreadCache|   |ThreadCache|   TLS-isolated, lock-free
 +----------+    +----------+    +----------+
      \              |              /
       \             |             /        batch transfer (slow path)
        +------------+------------+
        |       CentralCache      |          bucketed by size class,
        +------------+------------+          sharded locks
                     |
              +-------------+
              |  PageCache  |                Span split / merge,
              +-------------+                page-level allocator
                     |
              +-------------+
              |  OS (malloc / mmap)
              +-------------+
```

## Key design decisions

**TLS-isolated ThreadCache with intrusive free lists.**
Each thread owns a private `ThreadCache` accessed via thread-local storage, so the fast path performs no locking. Within it, objects are recycled through an intrusive `FreeList` (the free pointer is stored inside the freed object itself), giving **O(1) allocate and deallocate** on the hot path.

**Size-class bucketing plus batched transfer in CentralCache.**
`CentralCache` is partitioned by size class so that threads contending on different sizes do not block each other. It locks per bucket, and moves objects between the thread cache and central cache in **batches** rather than one at a time — this sharply reduces how often the shared structure is touched on the slow path.

**Span lifecycle in PageCache.**
The `PageCache` owns page-aligned `Span`s, splitting a larger span when a size class runs out and coalescing adjacent free spans on release to fight fragmentation.

## Benchmark methodology

To avoid the noise that comes from in-process benchmarking of an allocator, the harness runs each case in an **independent process**:

- **Thread counts:** 1 / 2 / 4 / 8 / 16
- **Object sizes:** 8 – 1024 B
- **Baseline:** glibc `malloc` / `free` under identical workloads
- **Cases:** 80 independent cases
- **Standard mode:** 10 repetitions per case
- **Total volume:** ~4.96 billion allocator operations

## Results and bottleneck analysis

In the 64 B workload, mean throughput scaled down from **41.41 Mops/s (1 thread)** to **15.91 Mops/s (16 threads)**.

Rather than stopping at the numbers, I profiled the scaling collapse with `perf` and `c2c` (cache-to-cache). The bottleneck was traced to a **global mutex in the Linux portable page-mapping layer**, not to the allocator's own size-class buckets. This distinction matters: it means the per-size-class sharding is working as intended, and the remaining contention lives in the OS-facing page-map path.

> This project was an exercise in measuring first and optimizing second — the profiling step turned a vague "it doesn't scale" observation into a specific, actionable finding.

## Skills demonstrated

- Concurrent data structures and lock granularity design
- Thread-local storage and intrusive data structures
- Rigorous, low-noise microbenchmarking methodology
- Performance profiling with `perf` and `c2c` to locate contention
- C++17 systems programming on Linux
