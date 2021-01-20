/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include <mutex>
#include <thread>
#include <wtf/Condition.h>
#include <wtf/DataLog.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

namespace {

static constexpr bool verbose = false;

enum NotifyStyle {
    AlwaysNotifyOne,
    TacticallyNotifyAll
};

template<typename Functor>
void wait(Condition& condition, std::unique_lock<Lock>& locker, const Functor& predicate, Seconds timeout)
{
    if (timeout == Seconds::infinity())
        condition.wait(locker, predicate);
    else {
        // This tests timeouts in the sense that it verifies that we can call wait() again after a
        // timeout happened. That's a non-trivial piece of functionality since upon timeout the
        // ParkingLot has to remove us from the queue.
        while (!predicate())
            condition.waitFor(locker, timeout, predicate);
    }
}

void notify(NotifyStyle notifyStyle, Condition& condition, bool shouldNotify)
{
    switch (notifyStyle) {
    case AlwaysNotifyOne:
        condition.notifyOne();
        break;
    case TacticallyNotifyAll:
        if (shouldNotify)
            condition.notifyAll();
        break;
    }
}

void runTest(
    unsigned numProducers,
    unsigned numConsumers,
    unsigned maxQueueSize,
    unsigned numMessagesPerProducer,
    NotifyStyle notifyStyle,
    Seconds timeout = Seconds::infinity(),
    Seconds delay = 0_s)
{
    Deque<unsigned> queue;
    bool shouldContinue = true;
    Lock lock;
    Condition emptyCondition;
    Condition fullCondition;

    Vector<Ref<Thread>> consumerThreads;
    Vector<Ref<Thread>> producerThreads;

    Vector<unsigned> received;
    Lock receivedLock;
    
    for (unsigned i = numConsumers; i--;) {
        consumerThreads.append(Thread::create(
            "Consumer thread",
            [&] () {
                for (;;) {
                    unsigned result;
                    unsigned shouldNotify = false;
                    {
                        std::unique_lock<Lock> locker(lock);
                        wait(
                            emptyCondition, locker, 
                            [&] () {
                                if (verbose)
                                    dataLog(toString(Thread::current(), ": Checking consumption predicate with shouldContinue = ", shouldContinue, ", queue.size() == ", queue.size(), "\n"));
                                return !shouldContinue || !queue.isEmpty();
                            },
                            timeout);
                        if (!shouldContinue && queue.isEmpty())
                            return;
                        shouldNotify = queue.size() == maxQueueSize;
                        result = queue.takeFirst();
                    }
                    notify(notifyStyle, fullCondition, shouldNotify);

                    {
                        auto locker = holdLock(receivedLock);
                        received.append(result);
                    }
                }
            }));
    }

    sleep(delay);

    for (unsigned i = numProducers; i--;) {
        producerThreads.append(Thread::create(
            "Producer Thread",
            [&] () {
                for (unsigned i = 0; i < numMessagesPerProducer; ++i) {
                    bool shouldNotify = false;
                    {
                        std::unique_lock<Lock> locker(lock);
                        wait(
                            fullCondition, locker,
                            [&] () {
                                if (verbose)
                                    dataLog(toString(Thread::current(), ": Checking production predicate with shouldContinue = ", shouldContinue, ", queue.size() == ", queue.size(), "\n"));
                                return queue.size() < maxQueueSize;
                            },
                            timeout);
                        shouldNotify = queue.isEmpty();
                        queue.append(i);
                    }
                    notify(notifyStyle, emptyCondition, shouldNotify);
                }
            }));
    }

    for (auto& thread : producerThreads)
        thread->waitForCompletion();

    {
        auto locker = holdLock(lock);
        shouldContinue = false;
    }
    emptyCondition.notifyAll();

    for (auto& thread : consumerThreads)
        thread->waitForCompletion();

    EXPECT_EQ(numProducers * numMessagesPerProducer, received.size());
    std::sort(received.begin(), received.end());
    for (unsigned messageIndex = 0; messageIndex < numMessagesPerProducer; ++messageIndex) {
        for (unsigned producerIndex = 0; producerIndex < numProducers; ++producerIndex)
            EXPECT_EQ(messageIndex, received[messageIndex * numProducers + producerIndex]);
    }
}

} // anonymous namespace

TEST(WTF_Condition, OneProducerOneConsumerOneSlot)
{
    runTest(1, 1, 1, 100000, TacticallyNotifyAll);
}

TEST(WTF_Condition, OneProducerOneConsumerOneSlotTimeout)
{
    runTest(
        1, 1, 1, 100000, TacticallyNotifyAll,
        Seconds::fromMilliseconds(10),
        Seconds(1));
}

TEST(WTF_Condition, OneProducerOneConsumerHundredSlots)
{
    runTest(1, 1, 100, 1000000, TacticallyNotifyAll);
}

TEST(WTF_Condition, TenProducersOneConsumerOneSlot)
{
    runTest(10, 1, 1, 10000, TacticallyNotifyAll);
}

TEST(WTF_Condition, TenProducersOneConsumerHundredSlotsNotifyAll)
{
    runTest(10, 1, 100, 10000, TacticallyNotifyAll);
}

TEST(WTF_Condition, TenProducersOneConsumerHundredSlotsNotifyOne)
{
    runTest(10, 1, 100, 10000, AlwaysNotifyOne);
}

TEST(WTF_Condition, OneProducerTenConsumersOneSlot)
{
    runTest(1, 10, 1, 10000, TacticallyNotifyAll);
}

TEST(WTF_Condition, OneProducerTenConsumersHundredSlotsNotifyAll)
{
    runTest(1, 10, 100, 100000, TacticallyNotifyAll);
}

TEST(WTF_Condition, OneProducerTenConsumersHundredSlotsNotifyOne)
{
    runTest(1, 10, 100, 100000, AlwaysNotifyOne);
}

TEST(WTF_Condition, TenProducersTenConsumersOneSlot)
{
    runTest(10, 10, 1, 50000, TacticallyNotifyAll);
}

TEST(WTF_Condition, TenProducersTenConsumersHundredSlotsNotifyAll)
{
    runTest(10, 10, 100, 50000, TacticallyNotifyAll);
}

TEST(WTF_Condition, TenProducersTenConsumersHundredSlotsNotifyOne)
{
    runTest(10, 10, 100, 50000, AlwaysNotifyOne);
}

TEST(WTF_Condition, TimeoutTimesOut)
{
    Lock lock;
    Condition condition;

    lock.lock();
    bool result = condition.waitFor(
        lock, Seconds::fromMilliseconds(10), [] () -> bool { return false; });
    lock.unlock();

    EXPECT_FALSE(result);
}

} // namespace TestWebKitAPI

