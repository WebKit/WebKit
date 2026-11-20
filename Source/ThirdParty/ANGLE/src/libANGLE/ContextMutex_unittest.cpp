//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for ContextMutex.
//

#include "gtest/gtest.h"

#include <chrono>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "libANGLE/ContextMutex.h"

namespace
{

template <class GetContextMutex>
void runBasicContextMutexTest(bool expectToPass, GetContextMutex &&getContextMutex)
{
    constexpr size_t kThreadCount    = 16;
    constexpr size_t kIterationCount = 50'000;

    std::array<std::thread, kThreadCount> threads;
    std::array<egl::ContextMutex *, kThreadCount> contextMutexes = {};

    std::mutex mutex;
    std::condition_variable condVar;
    size_t readyCount = 0;

    std::atomic<size_t> testVar;

    for (size_t i = 0; i < kThreadCount; ++i)
    {
        threads[i] = std::thread([&, i]() {
            {
                std::unique_lock<std::mutex> lock(mutex);
                contextMutexes[i] = getContextMutex();
                contextMutexes[i]->addRef();
                ++readyCount;
                if (readyCount < kThreadCount)
                {
                    condVar.wait(lock, [&]() { return readyCount == kThreadCount; });
                }
                else
                {
                    condVar.notify_all();
                }
            }
            for (size_t j = 0; j < kIterationCount; ++j)
            {
                egl::ScopedContextMutexLock lock(contextMutexes[i]);
                const int local    = testVar.load(std::memory_order_relaxed);
                const int newValue = local + 1;
                testVar.store(newValue, std::memory_order_relaxed);
            }
        });
    }

    for (size_t i = 0; i < kThreadCount; ++i)
    {
        threads[i].join();
        contextMutexes[i]->release();
    }

    if (expectToPass)
    {
        EXPECT_EQ(testVar.load(), kThreadCount * kIterationCount);
    }
    else
    {
        EXPECT_LE(testVar.load(), kThreadCount * kIterationCount);
    }
}

// Tests locking of single ContextMutex mutex.
TEST(ContextMutexTest, SingleMutexLock)
{
    egl::ContextMutex *contextMutex = new egl::ContextMutex();
    contextMutex->addRef();
    runBasicContextMutexTest(true, [&]() { return contextMutex; });
    contextMutex->release();
}

// Tests locking of multiple merged ContextMutex mutexes.
TEST(ContextMutexTest, MultipleMergedMutexLock)
{
    egl::ContextMutex *contextMutex = new egl::ContextMutex();
    contextMutex->addRef();
    runBasicContextMutexTest(true, [&]() {
        egl::ScopedContextMutexLock lock(contextMutex);
        egl::ContextMutex *threadMutex = new egl::ContextMutex();
        egl::ContextMutex::Merge(contextMutex, threadMutex);
        return threadMutex;
    });
    contextMutex->release();
}

// Tests locking of multiple unmerged ContextMutex mutexes.
TEST(ContextMutexTest, MultipleUnmergedMutexLock)
{
    runBasicContextMutexTest(false, [&]() { return new egl::ContextMutex(); });
}

// Creates 2N mutexes and 2 threads, then merges N mutex pairs in each thread. Merging order of
// the first thread is reversed in the second thread.
TEST(ContextMutexTest, TwoThreadsCrossMerge)
{
    constexpr size_t kThreadCount    = 2;
    constexpr size_t kIterationCount = 100;
    static_assert(kThreadCount % 2 == 0);

    std::array<std::thread, kThreadCount> threads;
    std::array<std::array<egl::ContextMutex *, kThreadCount>, kIterationCount> mutexParis = {};

    // Create mutexes.
    for (uint32_t i = 0; i < kIterationCount; ++i)
    {
        for (uint32_t j = 0; j < kThreadCount; ++j)
        {
            mutexParis[i][j] = new egl::ContextMutex();
            // Call without a lock because no concurrent access is possible.
            mutexParis[i][j]->addRef();
        }
    }

    std::mutex mutex;
    std::condition_variable condVar;
    size_t readyCount = 0;
    bool flipFlop     = false;

    auto threadJob = [&](size_t lockMutexIndex) {
        for (size_t i = 0; i < kIterationCount; ++i)
        {
            // Lock the first mutex.
            egl::ScopedContextMutexLock contextMutexLock(mutexParis[i][lockMutexIndex]);
            // Wait until all threads are ready...
            {
                std::unique_lock<std::mutex> lock(mutex);
                ++readyCount;
                if (readyCount == kThreadCount)
                {
                    readyCount = 0;
                    flipFlop   = !flipFlop;
                    condVar.notify_all();
                }
                else
                {
                    const size_t prevFlipFlop = flipFlop;
                    condVar.wait(lock, [&]() { return flipFlop != prevFlipFlop; });
                }
            }
            // Merge mutexes.
            egl::ContextMutex::Merge(mutexParis[i][lockMutexIndex],
                                     mutexParis[i][kThreadCount - lockMutexIndex - 1]);
        }
    };

    // Start threads...
    for (size_t i = 0; i < kThreadCount; ++i)
    {
        threads[i] = std::thread([&, i]() { threadJob(i); });
    }

    // Join with threads...
    for (std::thread &thread : threads)
    {
        thread.join();
    }

    // Destroy mutexes.
    for (size_t i = 0; i < kIterationCount; ++i)
    {
        for (size_t j = 0; j < kThreadCount; ++j)
        {
            // Call without a lock because no concurrent access is possible.
            mutexParis[i][j]->release();
        }
    }
}

// Creates a "leaf" mutex of |root|, like a Context does.
std::unique_ptr<egl::ContextMutex> CreateLeafMutex(egl::ContextMutex *root)
{
    return std::make_unique<egl::ContextMutex>(root);
}

// Destroys a "leaf" mutex, which needs its "root" locked, like a Context does.
void DestroyLeafMutex(std::unique_ptr<egl::ContextMutex> *leaf)
{
    egl::ScopedContextMutexAddRefLock lock(**leaf);
    leaf->reset();
}

// Makes threads lock random pairs of |leaves| with ScopedContextMutexesLock, while |concurrently|
// runs on another thread, and checks that the lock excludes the threads that lock either mutex.
template <class Concurrently>
void runContextMutexPairsTest(const std::vector<egl::ContextMutex *> &leaves,
                              Concurrently &&concurrently)
{
    constexpr size_t kThreadCount    = 8;
    constexpr size_t kIterationCount = 20'000;

    // Incremented without an atomic read-modify-write, by the threads that lock the mutex of the
    // corresponding leaf.
    std::vector<std::atomic<size_t>> counters(leaves.size());
    std::vector<std::vector<size_t>> expectedCounts(kThreadCount,
                                                    std::vector<size_t>(leaves.size(), 0));

    std::vector<std::thread> threads;
    for (size_t threadIndex = 0; threadIndex < kThreadCount; ++threadIndex)
    {
        threads.emplace_back([&, threadIndex]() {
            std::mt19937 random(static_cast<uint32_t>(threadIndex));
            std::uniform_int_distribution<size_t> pick(0, leaves.size() - 1);
            for (size_t iteration = 0; iteration < kIterationCount; ++iteration)
            {
                // Includes pairs of the same mutex, and pairs that share a "root", in both orders.
                const size_t first  = pick(random);
                const size_t second = pick(random);

                egl::ScopedContextMutexesLock lock(leaves[first], leaves[second]);
                for (size_t index : {first, second})
                {
                    counters[index].store(counters[index].load(std::memory_order_relaxed) + 1,
                                          std::memory_order_relaxed);
                    ++expectedCounts[threadIndex][index];
                    if (first == second)
                    {
                        break;
                    }
                }
            }
        });
    }

    std::thread concurrentThread(std::forward<Concurrently>(concurrently));

    for (std::thread &thread : threads)
    {
        thread.join();
    }
    concurrentThread.join();

    for (size_t index = 0; index < leaves.size(); ++index)
    {
        size_t expectedCount = 0;
        for (const std::vector<size_t> &threadCounts : expectedCounts)
        {
            expectedCount += threadCounts[index];
        }
        EXPECT_EQ(expectedCount, counters[index].load()) << "Mutex " << index;
    }
}

// Tests ScopedContextMutexesLock with mutexes that share "root"s.  Leaves A and D share a "root",
// and so do B and C, so locking the pairs in the order of the leaves (i.e. of the Contexts) would
// lock (A, B) and (C, D) with the "root"s in opposite orders, and deadlock. Lock in the order of
// "root" prevents deadlock.
TEST(ContextMutexTest, ScopedContextMutexesLockSharedRoots)
{
    egl::ContextMutex *rootAD = new egl::ContextMutex();
    egl::ContextMutex *rootBC = new egl::ContextMutex();
    egl::ContextMutex *rootE  = new egl::ContextMutex();

    std::array<std::unique_ptr<egl::ContextMutex>, 5> leaves = {
        CreateLeafMutex(rootAD), CreateLeafMutex(rootBC), CreateLeafMutex(rootBC),
        CreateLeafMutex(rootAD), CreateLeafMutex(rootE),
    };

    std::vector<egl::ContextMutex *> leafPointers;
    for (const std::unique_ptr<egl::ContextMutex> &leaf : leaves)
    {
        leafPointers.push_back(leaf.get());
    }
    runContextMutexPairsTest(leafPointers, []() {});

    for (std::unique_ptr<egl::ContextMutex> &leaf : leaves)
    {
        DestroyLeafMutex(&leaf);
    }
}

// Tests ScopedContextMutexesLock while other threads merge the "root"s of the mutexes it locks.
TEST(ContextMutexTest, ScopedContextMutexesLockWhileMerging)
{
    constexpr size_t kLeafCount = 8;
    std::array<std::unique_ptr<egl::ContextMutex>, kLeafCount> leaves;
    std::vector<egl::ContextMutex *> leafPointers;
    for (std::unique_ptr<egl::ContextMutex> &leaf : leaves)
    {
        leaf = CreateLeafMutex(new egl::ContextMutex());
        leafPointers.push_back(leaf.get());
    }

    runContextMutexPairsTest(leafPointers, [&]() {
        // Merge all the "root"s into one, a pair at a time, like when Contexts use EGLImages.
        for (size_t index = 1; index < kLeafCount; ++index)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            egl::ScopedContextMutexLock lock(leafPointers[index - 1]);
            egl::ContextMutex::Merge(leafPointers[index - 1], leafPointers[index]);
        }
    });

    for (std::unique_ptr<egl::ContextMutex> &leaf : leaves)
    {
        DestroyLeafMutex(&leaf);
    }
}

}  // anonymous namespace
