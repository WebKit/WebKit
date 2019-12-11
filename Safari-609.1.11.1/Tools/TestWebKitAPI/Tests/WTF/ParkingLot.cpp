/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <wtf/DataLog.h>
#include <wtf/HashSet.h>
#include <wtf/ListDump.h>
#include <wtf/ParkingLot.h>
#include <wtf/Threading.h>
#include <wtf/ThreadingPrimitives.h>

namespace TestWebKitAPI {

namespace {

struct SingleLatchTest {
    void initialize(unsigned numThreads)
    {
        // This implements a fair (FIFO) semaphore, and it starts out unavailable.
        semaphore.store(0);
        
        for (unsigned i = numThreads; i--;) {
            threads.append(
                Thread::create(
                    "Parking Test Thread",
                    [&] () {
                        down();

                        std::lock_guard<std::mutex> locker(lock);
                        awake.add(Thread::current());
                        lastAwoken = &Thread::current();
                        condition.notify_one();
                    }));
        }
    }

    void unparkOne(unsigned singleUnparkIndex)
    {
        EXPECT_TRUE(nullptr == lastAwoken);
        
        unsigned numWaitingOnAddress = 0;
        Vector<RefPtr<Thread>, 8> queue;
        ParkingLot::forEach(
            [&] (Thread& thread, const void* address) {
                if (address != &semaphore)
                    return;

                queue.append(&thread);

                numWaitingOnAddress++;
            });

        EXPECT_LE(numWaitingOnAddress, threads.size() - singleUnparkIndex);

        up();

        {
            std::unique_lock<std::mutex> locker(lock);
            while (awake.size() < singleUnparkIndex + 1)
                condition.wait(locker);
            EXPECT_TRUE(nullptr != lastAwoken);
            if (!queue.isEmpty() && queue[0] != lastAwoken) {
                dataLog("Woke up wrong thread: queue = ", listDump(queue), ", last awoken = ", lastAwoken, "\n");
                EXPECT_EQ(queue[0], lastAwoken);
            }
            lastAwoken = nullptr;
        }
    }

    void finish(unsigned numSingleUnparks)
    {
        unsigned numWaitingOnAddress = 0;
        ParkingLot::forEach(
            [&] (Thread&, const void* address) {
                if (address != &semaphore)
                    return;
                
                numWaitingOnAddress++;
            });
        
        EXPECT_LE(numWaitingOnAddress, threads.size() - numSingleUnparks);

        semaphore.store(threads.size() - numSingleUnparks);
        ParkingLot::unparkAll(&semaphore);

        numWaitingOnAddress = 0;
        ParkingLot::forEach(
            [&] (Thread&, const void* address) {
                if (address != &semaphore)
                    return;
            
                numWaitingOnAddress++;
            });

        EXPECT_EQ(0u, numWaitingOnAddress);
    
        {
            std::unique_lock<std::mutex> locker(lock);
            while (awake.size() < threads.size())
                condition.wait(locker);
        }

        for (auto& thread : threads)
            thread->waitForCompletion();
    }

    // Semaphore operations.
    void down()
    {
        for (;;) {
            int oldSemaphoreValue = semaphore.load();
            int newSemaphoreValue = oldSemaphoreValue - 1;
            if (!semaphore.compareExchangeWeak(oldSemaphoreValue, newSemaphoreValue))
                continue;
            
            if (oldSemaphoreValue > 0) {
                // We acquired the semaphore. Done.
                return;
            }
            
            // We need to wait.
            if (ParkingLot::compareAndPark(&semaphore, newSemaphoreValue).wasUnparked) {
                // We did wait, and then got woken up. This means that someone who up'd the semaphore
                // passed ownership onto us.
                return;
            }

            // We never parked, because the semaphore value changed. Undo our decrement and try again.
            for (;;) {
                int oldSemaphoreValue = semaphore.load();
                if (semaphore.compareExchangeWeak(oldSemaphoreValue, oldSemaphoreValue + 1))
                    break;
            }
        }
    }
    void up()
    {
        int oldSemaphoreValue;
        for (;;) {
            oldSemaphoreValue = semaphore.load();
            if (semaphore.compareExchangeWeak(oldSemaphoreValue, oldSemaphoreValue + 1))
                break;
        }

        // Check if anyone was waiting on the semaphore. If they were, then pass ownership to them.
        if (oldSemaphoreValue < 0)
            ParkingLot::unparkOne(&semaphore);
    }
    
    Atomic<int> semaphore;
    std::mutex lock;
    std::condition_variable condition;
    HashSet<Ref<Thread>> awake;
    Vector<Ref<Thread>> threads;
    RefPtr<Thread> lastAwoken;
};

void runParkingTest(unsigned numLatches, unsigned delay, unsigned numThreads, unsigned numSingleUnparks)
{
    std::unique_ptr<SingleLatchTest[]> tests = makeUniqueWithoutFastMallocCheck<SingleLatchTest[]>(numLatches);

    for (unsigned latchIndex = numLatches; latchIndex--;)
        tests[latchIndex].initialize(numThreads);

    for (unsigned unparkIndex = 0; unparkIndex < numSingleUnparks; ++unparkIndex) {
        std::this_thread::sleep_for(std::chrono::microseconds(delay));
        for (unsigned latchIndex = numLatches; latchIndex--;)
            tests[latchIndex].unparkOne(unparkIndex);
    }

    for (unsigned latchIndex = numLatches; latchIndex--;)
        tests[latchIndex].finish(numSingleUnparks);
}

void repeatParkingTest(unsigned numRepeats, unsigned numLatches, unsigned delay, unsigned numThreads, unsigned numSingleUnparks)
{
    while (numRepeats--)
        runParkingTest(numLatches, delay, numThreads, numSingleUnparks);
}

} // anonymous namespace

TEST(WTF_ParkingLot, UnparkAllOneFast)
{
    repeatParkingTest(10000, 1, 0, 1, 0);
}

TEST(WTF_ParkingLot, UnparkAllHundredFast)
{
    repeatParkingTest(100, 1, 0, 100, 0);
}

TEST(WTF_ParkingLot, UnparkOneOneFast)
{
    repeatParkingTest(1000, 1, 0, 1, 1);
}

TEST(WTF_ParkingLot, UnparkOneHundredFast)
{
    repeatParkingTest(20, 1, 0, 100, 100);
}

TEST(WTF_ParkingLot, UnparkOneFiftyThenFiftyAllFast)
{
    repeatParkingTest(50, 1, 0, 100, 50);
}

TEST(WTF_ParkingLot, UnparkAllOne)
{
    repeatParkingTest(100, 1, 10000, 1, 0);
}

TEST(WTF_ParkingLot, UnparkAllHundred)
{
    repeatParkingTest(100, 1, 10000, 100, 0);
}

TEST(WTF_ParkingLot, UnparkOneOne)
{
    repeatParkingTest(10, 1, 10000, 1, 1);
}

TEST(WTF_ParkingLot, UnparkOneFifty)
{
    repeatParkingTest(1, 1, 10000, 50, 50);
}

TEST(WTF_ParkingLot, UnparkOneFiftyThenFiftyAll)
{
    repeatParkingTest(2, 1, 10000, 100, 50);
}

TEST(WTF_ParkingLot, HundredUnparkAllOneFast)
{
    repeatParkingTest(100, 100, 0, 1, 0);
}

TEST(WTF_ParkingLot, HundredUnparkAllOne)
{
    repeatParkingTest(1, 100, 10000, 1, 0);
}

} // namespace TestWebKitAPI

