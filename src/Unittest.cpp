// Assertion-based unit tests for the concurrent memory pool.
// Build with CMake (target `unittest`) or: cl /std:c++17 Unittest.cpp ThreadCache.cpp Centralcache.cpp Pagecache.cpp
#include "ConcurrentAlloc.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <thread>
#include <vector>
#include <unordered_set>

namespace {

int g_failures = 0;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            std::printf("  [FAIL] %s (line %d)\n", msg, __LINE__);  \
            ++g_failures;                                           \
        }                                                           \
    } while (0)

void testBasicAllocFree()
{
    std::printf("[test] basic alloc/free\n");
    const size_t sizes[] = {1, 8, 16, 64, 256, 1024, 4096, 64 * 1024, 256 * 1024};
    for (size_t s : sizes) {
        void* p = ConcurrentAlloc(s);
        CHECK(p != nullptr, "allocation returned null");
        // Returned address must be aligned to at least 8 bytes
        CHECK(reinterpret_cast<std::uintptr_t>(p) % 8 == 0, "misaligned allocation");
        ConcurrentFree(p);
    }
}

void testDistinctObjectsDoNotOverlap()
{
    std::printf("[test] distinct live objects do not share an address\n");
    const int N = 512;
    std::vector<void*> ptrs;
    ptrs.reserve(N);
    for (int i = 0; i < N; ++i) {
        void* p = ConcurrentAlloc(32);
        CHECK(p != nullptr, "allocation returned null");
        ptrs.push_back(p);
    }
    std::unordered_set<void*> seen;
    for (void* p : ptrs) {
        CHECK(seen.insert(p).second, "two live objects share an address");
    }
    for (void* p : ptrs) {
        ConcurrentFree(p);
    }
}

void testReuseAfterFree()
{
    std::printf("[test] freed memory is reused\n");
    void* first = ConcurrentAlloc(64);
    CHECK(first != nullptr, "first allocation returned null");
    ConcurrentFree(first);
    void* second = ConcurrentAlloc(64);
    CHECK(second != nullptr, "second allocation returned null");
    CHECK(first == second, "freed block was not reused by the same size class");
    ConcurrentFree(second);
}

void testLargeObjectPath()
{
    std::printf("[test] large-object path (> 256 KiB)\n");
    const size_t sizes[] = {257 * 1024, 1024 * 1024, 129 * 8 * 1024};
    for (size_t s : sizes) {
        void* p = ConcurrentAlloc(s);
        CHECK(p != nullptr, "large allocation returned null");
        CHECK(reinterpret_cast<std::uintptr_t>(p) % 4096 == 0, "large block is not page-aligned");
        ConcurrentFree(p);
    }
}

void testManyThreads()
{
    std::printf("[test] 8 threads x 20000 alloc/free\n");
    const int THREADS = 8;
    const int ITERS = 20000;
    std::vector<std::thread> ts;
    ts.reserve(THREADS);
    for (int t = 0; t < THREADS; ++t) {
        ts.emplace_back([t, ITERS]() {
            size_t sz = 8 + (t % 5) * 16; // vary size per thread
            std::vector<void*> batch;
            batch.reserve(64);
            for (int i = 0; i < ITERS; ++i) {
                void* p = ConcurrentAlloc(sz);
                if (!p) { ++g_failures; return; }
                batch.push_back(p);
                if (batch.size() == 64) {
                    for (void* q : batch) ConcurrentFree(q);
                    batch.clear();
                }
            }
            for (void* q : batch) ConcurrentFree(q);
        });
    }
    for (auto& th : ts) th.join();
}

void testObjectPoolReuse()
{
    std::printf("[test] ObjectPool construct/destroy/reuse\n");
    struct Item { int a; double b; };
    ObjectPool<Item> pool;
    Item* x = pool.New();
    CHECK(x != nullptr, "ObjectPool.New returned null");
    x->a = 42;
    pool.Delete(x);
    Item* y = pool.New();
    CHECK(y == x, "ObjectPool did not recycle the returned object");
    pool.Delete(y);
}

} // namespace

int main()
{
    std::printf("Running concurrent memory pool tests\n");
    std::printf("=====================================\n");

    testBasicAllocFree();
    testDistinctObjectsDoNotOverlap();
    testReuseAfterFree();
    testLargeObjectPath();
    testManyThreads();
    testObjectPoolReuse();

    std::printf("=====================================\n");
    if (g_failures == 0) {
        std::printf("All tests PASSED\n");
        return 0;
    }
    std::printf("%d check(s) FAILED\n", g_failures);
    return 1;
}
