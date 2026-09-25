/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/ReadWriteLock.h>

#include <array>
#include <wtf/Atomics.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

namespace {

constexpr Seconds testTimeout = 30_s;

// Every test here can hang rather than fail if the lock is broken, so each one runs its
// threads with a deadline and reports how far each thread got. A hang shows up as a
// failed expectation on the per-thread progress counters instead of a stuck test binary.
class DeadlineGuard {
public:
    bool expired() const { return MonotonicTime::now() > m_deadline; }

private:
    MonotonicTime m_deadline { MonotonicTime::now() + testTimeout };
};

template<typename Body>
Vector<Ref<Thread>> spawn(unsigned count, Body body)
{
    Vector<Ref<Thread>> threads;
    for (unsigned i = 0; i < count; ++i) {
        threads.append(Thread::create("ReadWriteLock test"_s, [i, body] {
            body(i);
        }));
    }
    return threads;
}

void join(Vector<Ref<Thread>>& threads)
{
    for (auto& thread : threads)
        thread->waitForCompletion();
}

} // anonymous namespace

// Three or more writers contending must all make progress. A writer must never spin with the lock
// free, which is what a phase handed off without being claimed would cause.
TEST(WTF_ReadWriteLock, ManyWritersMakeProgress)
{
    constexpr unsigned writerCount = 8;
    constexpr unsigned acquisitionsPerWriter = 1000;

    ReadWriteLock lock;
    DeadlineGuard deadline;
    unsigned counter = 0;
    std::array<Atomic<unsigned>, writerCount> progress { };

    auto threads = spawn(writerCount, [&](unsigned index) {
        for (unsigned i = 0; i < acquisitionsPerWriter && !deadline.expired(); ++i) {
            Locker locker { lock.write() };
            ++counter;
            progress[index].store(i + 1, std::memory_order_relaxed);
        }
    });
    join(threads);

    for (unsigned i = 0; i < writerCount; ++i)
        EXPECT_EQ(acquisitionsPerWriter, progress[i].load(std::memory_order_relaxed));
    EXPECT_EQ(writerCount * acquisitionsPerWriter, counter);
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// A stream of writers must not starve readers: with several writers always queued, the reader wake
// path still has to run on every phase end.
TEST(WTF_ReadWriteLock, WritersDoNotStarveReaders)
{
    constexpr unsigned writerCount = 4;
    constexpr unsigned readerCount = 4;
    constexpr unsigned readsPerReader = 100;

    ReadWriteLock lock;
    DeadlineGuard deadline;
    Atomic<bool> readersDone { false };
    unsigned counter = 0;
    std::array<Atomic<unsigned>, readerCount> readsCompleted { };

    auto writers = spawn(writerCount, [&](unsigned) {
        while (!readersDone.load(std::memory_order_relaxed) && !deadline.expired()) {
            Locker locker { lock.write() };
            ++counter;
        }
    });

    auto readers = spawn(readerCount, [&](unsigned index) {
        for (unsigned i = 0; i < readsPerReader && !deadline.expired(); ++i) {
            Locker locker { lock.read() };
            readsCompleted[index].store(i + 1, std::memory_order_relaxed);
        }
    });

    join(readers);
    readersDone.store(true, std::memory_order_relaxed);
    join(writers);

    for (unsigned i = 0; i < readerCount; ++i)
        EXPECT_EQ(readsPerReader, readsCompleted[i].load(std::memory_order_relaxed));
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// The mirror case: a stream of readers must not starve a writer. A writer registers
// itself before inspecting the lock, which holds new readers out.
TEST(WTF_ReadWriteLock, ReadersDoNotStarveWriter)
{
    constexpr unsigned readerCount = 4;
    constexpr unsigned writesToComplete = 100;

    ReadWriteLock lock;
    DeadlineGuard deadline;
    Atomic<bool> writerDone { false };
    Atomic<unsigned> writesCompleted { 0 };

    auto readers = spawn(readerCount, [&](unsigned) {
        while (!writerDone.load(std::memory_order_relaxed) && !deadline.expired())
            Locker locker { lock.read() };
    });

    auto writer = spawn(1, [&](unsigned) {
        for (unsigned i = 0; i < writesToComplete && !deadline.expired(); ++i) {
            Locker locker { lock.write() };
            writesCompleted.store(i + 1, std::memory_order_relaxed);
        }
    });

    join(writer);
    writerDone.store(true, std::memory_order_relaxed);
    join(readers);

    EXPECT_EQ(writesToComplete, writesCompleted.load(std::memory_order_relaxed));
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// Several writers plus readers, so that WTF::Lock's fair handoff - which writer exclusion delegates
// to - runs with other writers still parked behind it. A woken writer still has to claim a phase of
// its own; if that bookkeeping leaks, a later reader or writer hangs.
TEST(WTF_ReadWriteLock, WriterHandoffWithWritersAndReadersQueued)
{
    constexpr unsigned writerCount = 6;
    constexpr unsigned readerCount = 6;
    constexpr unsigned iterations = 500;

    ReadWriteLock lock;
    DeadlineGuard deadline;
    unsigned counter = 0;
    std::array<Atomic<unsigned>, writerCount + readerCount> progress { };

    auto writers = spawn(writerCount, [&](unsigned index) {
        for (unsigned i = 0; i < iterations && !deadline.expired(); ++i) {
            Locker locker { lock.write() };
            ++counter;
            progress[index].store(i + 1, std::memory_order_relaxed);
        }
    });

    auto readers = spawn(readerCount, [&](unsigned index) {
        for (unsigned i = 0; i < iterations && !deadline.expired(); ++i) {
            Locker locker { lock.read() };
            progress[writerCount + index].store(i + 1, std::memory_order_relaxed);
        }
    });

    join(writers);
    join(readers);

    for (unsigned i = 0; i < writerCount + readerCount; ++i)
        EXPECT_EQ(iterations, progress[i].load(std::memory_order_relaxed));
    EXPECT_EQ(writerCount * iterations, counter);
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// Once a burst of writers has finished and every thread has gone away, a lone reader
// must still be able to acquire. This catches state left behind on behalf of a writer
// that no longer exists, which is invisible while any writer is still running.
TEST(WTF_ReadWriteLock, LoneReaderAfterWriterBurst)
{
    constexpr unsigned writerCount = 4;
    constexpr unsigned iterations = 2000;

    ReadWriteLock lock;
    DeadlineGuard deadline;
    unsigned counter = 0;

    auto writers = spawn(writerCount, [&](unsigned) {
        for (unsigned i = 0; i < iterations && !deadline.expired(); ++i) {
            Locker locker { lock.write() };
            ++counter;
        }
    });
    join(writers);

    EXPECT_TRUE(lock.isQuiescentForTesting());

    Atomic<bool> acquired { false };
    auto reader = spawn(1, [&](unsigned) {
        Locker locker { lock.read() };
        acquired.store(true, std::memory_order_relaxed);
    });
    join(reader);

    EXPECT_TRUE(acquired.load(std::memory_order_relaxed));
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// Mutual exclusion: a writer must never overlap a reader or another writer.
TEST(WTF_ReadWriteLock, MutualExclusion)
{
    constexpr unsigned writerCount = 4;
    constexpr unsigned readerCount = 4;
    constexpr unsigned iterations = 500;

    ReadWriteLock lock;
    DeadlineGuard deadline;
    Atomic<int> readersInside { 0 };
    Atomic<int> writersInside { 0 };
    Atomic<unsigned> violations { 0 };

    auto writers = spawn(writerCount, [&](unsigned) {
        for (unsigned i = 0; i < iterations && !deadline.expired(); ++i) {
            Locker locker { lock.write() };
            writersInside.exchangeAdd(1);
            if (writersInside.load() != 1 || readersInside.load())
                violations.exchangeAdd(1);
            Thread::yield();
            if (writersInside.load() != 1 || readersInside.load())
                violations.exchangeAdd(1);
            writersInside.exchangeAdd(-1);
        }
    });

    auto readers = spawn(readerCount, [&](unsigned) {
        for (unsigned i = 0; i < iterations && !deadline.expired(); ++i) {
            Locker locker { lock.read() };
            readersInside.exchangeAdd(1);
            if (writersInside.load())
                violations.exchangeAdd(1);
            Thread::yield();
            if (writersInside.load())
                violations.exchangeAdd(1);
            readersInside.exchangeAdd(-1);
        }
    });

    join(writers);
    join(readers);

    EXPECT_EQ(0u, violations.load());
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// Readers really are concurrent: with no writers present, several readers must be able
// to hold the lock at the same time.
TEST(WTF_ReadWriteLock, ReadersRunConcurrently)
{
    constexpr unsigned readerCount = 4;

    ReadWriteLock lock;
    DeadlineGuard deadline;
    Atomic<unsigned> arrived { 0 };
    Atomic<unsigned> inside { 0 };
    Atomic<unsigned> maxObserved { 0 };

    auto readers = spawn(readerCount, [&](unsigned) {
        Locker locker { lock.read() };
        unsigned current = inside.exchangeAdd(1) + 1;
        for (;;) {
            unsigned previous = maxObserved.load();
            if (current <= previous || maxObserved.compareExchangeWeak(previous, current))
                break;
        }
        // Wait on a count that only ever grows. Waiting on `inside` would hang until the
        // deadline, because the last reader to arrive decrements it again on its way out
        // before the others have observed it.
        arrived.exchangeAdd(1);
        while (arrived.load() < readerCount && !deadline.expired())
            Thread::yield();
        inside.exchangeAdd(-1);
    });
    join(readers);

    EXPECT_EQ(readerCount, maxObserved.load());
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// A writer must announce itself before it blocks, not only once it holds the lock. That announcement
// is what holds new readers out, and without it a stream of readers could starve writers.
TEST(WTF_ReadWriteLock, WriterAnnouncesItselfBeforeBlocking)
{
    ReadWriteLock lock;
    DeadlineGuard deadline;
    Atomic<bool> writerHasLock { false };

    lock.readLock();

    auto writer = spawn(1, [&](unsigned) {
        Locker locker { lock.write() };
        writerHasLock.store(true, std::memory_order_relaxed);
    });

    while (!lock.hasWriterPresentForTesting() && !deadline.expired())
        Thread::yield();

    // Announced, but it cannot have the lock: we still hold a read lock it has to drain.
    EXPECT_TRUE(lock.hasWriterPresentForTesting());
    EXPECT_FALSE(writerHasLock.load(std::memory_order_relaxed));

    lock.readUnlock();
    join(writer);

    EXPECT_TRUE(writerHasLock.load(std::memory_order_relaxed));
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// A writer parked behind a reader must be woken when that reader leaves.
TEST(WTF_ReadWriteLock, WriterParkedBehindReaderIsWoken)
{
    ReadWriteLock lock;
    DeadlineGuard deadline;
    Atomic<bool> readerLeaving { false };
    Atomic<bool> writerSawReaderLeave { false };

    lock.readLock();

    auto writer = spawn(1, [&](unsigned) {
        Locker locker { lock.write() };
        writerSawReaderLeave.store(readerLeaving.load(std::memory_order_relaxed), std::memory_order_relaxed);
    });

    // The writer sets its parked bit only once it has stopped spinning, and cannot then decline to
    // park while we hold the read lock.
    while (!lock.hasParkedBitsForTesting() && !deadline.expired())
        Thread::yield();
    EXPECT_TRUE(lock.hasParkedBitsForTesting());

    readerLeaving.store(true, std::memory_order_relaxed);
    lock.readUnlock();
    join(writer);

    EXPECT_TRUE(writerSawReaderLeave.load(std::memory_order_relaxed));
    EXPECT_FALSE(lock.hasParkedBitsForTesting());
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// Readers parked behind a writer must all be woken when it leaves.
TEST(WTF_ReadWriteLock, ReadersParkedBehindWriterAreWoken)
{
    constexpr unsigned readerCount = 4;

    ReadWriteLock lock;
    DeadlineGuard deadline;
    Atomic<unsigned> readersArrived { 0 };
    Atomic<bool> writerLeaving { false };
    std::array<Atomic<bool>, readerCount> readerSawWriterLeave { };
    Vector<Ref<Thread>> readers;

    {
        Locker locker { lock.write() };

        readers = spawn(readerCount, [&](unsigned index) {
            readersArrived.exchangeAdd(1);
            Locker locker { lock.read() };
            readerSawWriterLeave[index].store(writerLeaving.load(std::memory_order_relaxed), std::memory_order_relaxed);
        });

        while ((readersArrived.load() < readerCount || !lock.hasParkedBitsForTesting()) && !deadline.expired())
            Thread::yield();
        EXPECT_EQ(readerCount, readersArrived.load());
        EXPECT_TRUE(lock.hasParkedBitsForTesting());

        writerLeaving.store(true, std::memory_order_relaxed);
    }
    join(readers);

    for (unsigned i = 0; i < readerCount; ++i)
        EXPECT_TRUE(readerSawWriterLeave[i].load(std::memory_order_relaxed));

    // A reader that stops spinning just as the writer leaves can set its parked bit too late to be
    // woken and then decline to park, leaving the bit for the next writer to clear.
    {
        Locker locker { lock.write() };
    }
    EXPECT_FALSE(lock.hasParkedBitsForTesting());
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// A reader that is slow to be scheduled after being woken must not lose its turn. With two
// readers parked and a writer looping, the writer's release wakes both, but the first reader
// can complete its whole critical section and the writer can re-acquire before the second
// reader ever runs. If waking only granted permission rather than the lock itself, the second
// reader would find that permission withdrawn, re-park, and repeat forever.
TEST(WTF_ReadWriteLock, WokenReaderKeepsItsTurnWhenDescheduled)
{
    constexpr unsigned readerCount = 2;
    constexpr unsigned readsPerReader = 200;

    ReadWriteLock lock;
    DeadlineGuard deadline;
    Atomic<bool> readersDone { false };
    Atomic<unsigned> writesCompleted { 0 };
    std::array<Atomic<unsigned>, readerCount> readsCompleted { };

    // One writer, looping as tightly as it can, so that it is always either holding the lock
    // or registered as waiting for it.
    auto writer = spawn(1, [&](unsigned) {
        while (!readersDone.load(std::memory_order_relaxed) && !deadline.expired()) {
            Locker locker { lock.write() };
            writesCompleted.exchangeAdd(1);
        }
    });

    auto readers = spawn(readerCount, [&](unsigned index) {
        // Don't start until the writer is genuinely running, otherwise the readers can finish
        // before it ever contends and the test proves nothing.
        while (writesCompleted.load(std::memory_order_relaxed) < 100 && !deadline.expired())
            Thread::yield();

        for (unsigned i = 0; i < readsPerReader && !deadline.expired(); ++i) {
            {
                Locker locker { lock.read() };
                // Hold long enough that the other reader in the cohort is likely still
                // descheduled when this one releases, which is the shape of the race.
                Thread::yield();
            }
            readsCompleted[index].store(i + 1, std::memory_order_relaxed);
        }
    });

    join(readers);
    readersDone.store(true, std::memory_order_relaxed);
    join(writer);

    // If this is 0 the writer never contended and the test was vacuous.
    EXPECT_GT(writesCompleted.load(std::memory_order_relaxed), 100u);
    for (unsigned i = 0; i < readerCount; ++i)
        EXPECT_EQ(readsPerReader, readsCompleted[i].load(std::memory_order_relaxed));
    EXPECT_TRUE(lock.isQuiescentForTesting());
}

// Uncontended acquisition must leave no residue, so that the inline fast paths keep
// working rather than silently degrading to the out-of-line ones.
TEST(WTF_ReadWriteLock, UncontendedQuiescence)
{
    ReadWriteLock lock;

    for (unsigned i = 0; i < 100; ++i) {
        {
            Locker locker { lock.write() };
        }
        EXPECT_TRUE(lock.isQuiescentForTesting());
        EXPECT_FALSE(lock.hasParkedBitsForTesting());
        {
            Locker locker { lock.read() };
        }
        EXPECT_TRUE(lock.isQuiescentForTesting());
        EXPECT_FALSE(lock.hasParkedBitsForTesting());
    }
}

} // namespace TestWebKitAPI
