/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TestHarness.h"

#include "pas_condition.h"
#include "pas_lock.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

void testLockBasic()
{
    pas_lock lock = PAS_LOCK_INITIALIZER;

    pas_lock_lock(&lock);
    pas_lock_assert_held(&lock);
    pas_lock_testing_assert_held(&lock);
    CHECK(!pas_lock_try_lock(&lock));
    pas_lock_unlock(&lock);

    CHECK(pas_lock_try_lock(&lock));
    pas_lock_assert_held(&lock);
    CHECK(!pas_lock_try_lock(&lock));
    pas_lock_unlock(&lock);

    pas_lock constructedLock;
    pas_lock_construct(&constructedLock);

    pas_lock* held = nullptr;
    pas_lock_switch(&held, &lock);
    CHECK_EQUAL(held, &lock);
    pas_lock_assert_held(&lock);

    pas_lock_switch(&held, &constructedLock);
    CHECK_EQUAL(held, &constructedLock);
    pas_lock_assert_held(&constructedLock);
    CHECK(pas_lock_try_lock(&lock));
    pas_lock_unlock(&lock);

    pas_lock_switch(&held, nullptr);
    CHECK(!held);
}

void testLockContention()
{
    static constexpr unsigned numThreads = 8;
    static constexpr unsigned iterationsPerThread = 10000;

    pas_lock lock = PAS_LOCK_INITIALIZER;
    unsigned sharedCounter = 0;
    unsigned activeHolders = 0;

    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (unsigned t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t] {
            for (unsigned i = 0; i < iterationsPerThread; ++i) {
                if (((i + t) & 1) || !pas_lock_try_lock(&lock))
                    pas_lock_lock(&lock);
                pas_lock_assert_held(&lock);
                ++activeHolders;
                CHECK_EQUAL(activeHolders, 1u);
                ++sharedCounter;
                --activeHolders;
                pas_lock_unlock(&lock);
            }
        });
    }

    for (std::thread& worker : threads)
        worker.join();

    CHECK_EQUAL(sharedCounter, numThreads * iterationsPerThread);
}

void testConditionSignalOneByOne()
{
    static constexpr unsigned numWaiters = 6;

    pas_lock lock = PAS_LOCK_INITIALIZER;
    pas_condition cond = PAS_CONDITION_INITIALIZER;
    pas_condition readyCond = PAS_CONDITION_INITIALIZER;
    unsigned readyWaiters = 0;
    unsigned permits = 0;
    unsigned awakened = 0;

    std::vector<std::thread> waiters;
    waiters.reserve(numWaiters);
    for (unsigned i = 0; i < numWaiters; ++i) {
        waiters.emplace_back([&] {
            pas_lock_lock(&lock);
            ++readyWaiters;
            pas_condition_signal(&readyCond);
            while (!permits)
                pas_condition_wait(&cond, &lock);
            pas_lock_assert_held(&lock);
            --permits;
            ++awakened;
            pas_lock_unlock(&lock);
        });
    }

    pas_lock_lock(&lock);
    while (readyWaiters < numWaiters)
        pas_condition_wait(&readyCond, &lock);
    pas_lock_unlock(&lock);

    for (unsigned i = 0; i < numWaiters; ++i) {
        pas_lock_lock(&lock);
        ++permits;
        pas_condition_signal(&cond);
        pas_lock_unlock(&lock);
    }

    for (std::thread& waiter : waiters)
        waiter.join();

    CHECK_EQUAL(awakened, numWaiters);
    CHECK_EQUAL(permits, 0u);
}

void testConditionBroadcast()
{
    static constexpr unsigned numWaiters = 8;

    pas_lock lock = PAS_LOCK_INITIALIZER;
    pas_condition cond;
    pas_condition readyCond;
    pas_condition_construct(&cond);
    pas_condition_construct(&readyCond);

    unsigned readyWaiters = 0;
    unsigned finishedWaiters = 0;
    bool shouldProceed = false;

    std::vector<std::thread> waiters;
    waiters.reserve(numWaiters);
    for (unsigned i = 0; i < numWaiters; ++i) {
        waiters.emplace_back([&] {
            pas_lock_lock(&lock);
            ++readyWaiters;
            pas_condition_signal(&readyCond);
            while (!shouldProceed)
                pas_condition_wait(&cond, &lock);
            pas_lock_assert_held(&lock);
            ++finishedWaiters;
            pas_lock_unlock(&lock);
        });
    }

    pas_lock_lock(&lock);
    while (readyWaiters < numWaiters)
        pas_condition_wait(&readyCond, &lock);
    shouldProceed = true;
    pas_condition_broadcast(&cond);
    pas_lock_unlock(&lock);

    for (std::thread& waiter : waiters)
        waiter.join();

    CHECK_EQUAL(finishedWaiters, numWaiters);
}

void testConditionTimedWaitTimeoutAndSignal()
{
    pas_lock lock = PAS_LOCK_INITIALIZER;
    pas_condition cond = PAS_CONDITION_INITIALIZER;

    pas_condition_signal(&cond);
    pas_condition_broadcast(&cond);

    pas_lock_lock(&lock);
    CHECK(!pas_condition_timed_wait_for(&cond, &lock, 0));
    pas_lock_assert_held(&lock);

    uint64_t now = pas_condition_get_time_in_nanoseconds();
    CHECK(!pas_condition_timed_wait_until(&cond, &lock, now));
    pas_lock_assert_held(&lock);

    uint64_t deadline = pas_condition_get_time_in_nanoseconds() + 15ULL * 1000000ULL;
    // A spurious wakeup also returns true, so only the deadline passing reliably reports a timeout.
    while (pas_condition_timed_wait_until(&cond, &lock, deadline)) { }
    pas_lock_assert_held(&lock);
    CHECK_GREATER_EQUAL(pas_condition_get_time_in_nanoseconds(), deadline);

    uint64_t start = pas_condition_get_time_in_nanoseconds();
    while (pas_condition_timed_wait_for(&cond, &lock, 1000000ULL))
        start = pas_condition_get_time_in_nanoseconds();
    CHECK_GREATER_EQUAL(pas_condition_get_time_in_nanoseconds() - start, 1000000ULL);

    CHECK(!pas_condition_timed_wait_until(&cond, &lock, deadline));
    pas_lock_unlock(&lock);

    bool signaled = false;
    bool waiterEntered = false;
    pas_condition enteredCond = PAS_CONDITION_INITIALIZER;

    std::thread notifier([&] {
        pas_lock_lock(&lock);
        while (!waiterEntered)
            pas_condition_wait(&enteredCond, &lock);
        pas_lock_unlock(&lock);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        pas_lock_lock(&lock);
        signaled = true;
        pas_condition_broadcast(&cond);
        pas_lock_unlock(&lock);
    });

    pas_lock_lock(&lock);
    waiterEntered = true;
    pas_condition_signal(&enteredCond);
    uint64_t longDeadline = pas_condition_get_time_in_nanoseconds() + 10ULL * 1000000000ULL;
    while (!signaled) {
        bool didNotTimeOut = pas_condition_timed_wait_until(&cond, &lock, longDeadline);
        CHECK(didNotTimeOut);
    }
    pas_lock_assert_held(&lock);
    pas_lock_unlock(&lock);

    notifier.join();
}

void testConditionProducerConsumerStress()
{
    static constexpr unsigned numProducers = 4;
    static constexpr unsigned numConsumers = 4;
    static constexpr unsigned itemsPerProducer = 2000;
    static constexpr unsigned capacity = 8;

    pas_lock lock = PAS_LOCK_INITIALIZER;
    pas_condition notEmpty = PAS_CONDITION_INITIALIZER;
    pas_condition notFull = PAS_CONDITION_INITIALIZER;

    unsigned buffer[capacity] = { };
    unsigned head = 0;
    unsigned tail = 0;
    unsigned count = 0;
    unsigned remainingItems = numProducers * itemsPerProducer;
    uint64_t producedSum = 0;
    uint64_t consumedSum = 0;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (unsigned p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p] {
            uint64_t localSum = 0;
            for (unsigned i = 1; i <= itemsPerProducer; ++i) {
                unsigned value = p * itemsPerProducer + i;
                localSum += value;

                pas_lock_lock(&lock);
                while (count == capacity)
                    pas_condition_wait(&notFull, &lock);
                buffer[tail] = value;
                tail = (tail + 1) % capacity;
                ++count;
                pas_condition_signal(&notEmpty);
                pas_lock_unlock(&lock);
            }

            pas_lock_lock(&lock);
            producedSum += localSum;
            pas_lock_unlock(&lock);
        });
    }

    for (unsigned c = 0; c < numConsumers; ++c) {
        consumers.emplace_back([&] {
            uint64_t localSum = 0;
            for (;;) {
                pas_lock_lock(&lock);
                while (!count && remainingItems)
                    pas_condition_wait(&notEmpty, &lock);
                if (!count && !remainingItems) {
                    consumedSum += localSum;
                    pas_lock_unlock(&lock);
                    return;
                }
                unsigned value = buffer[head];
                head = (head + 1) % capacity;
                --count;
                --remainingItems;
                if (!remainingItems)
                    pas_condition_broadcast(&notEmpty);
                pas_condition_signal(&notFull);
                pas_lock_unlock(&lock);

                localSum += value;
            }
        });
    }

    for (std::thread& producer : producers)
        producer.join();
    for (std::thread& consumer : consumers)
        consumer.join();

    CHECK_EQUAL(count, 0u);
    CHECK_EQUAL(remainingItems, 0u);
    CHECK_EQUAL(consumedSum, producedSum);
}

} // anonymous namespace

void addLockAndConditionTests()
{
    ADD_TEST(testLockBasic());
    ADD_TEST(testLockContention());
    ADD_TEST(testConditionSignalOneByOne());
    ADD_TEST(testConditionBroadcast());
    ADD_TEST(testConditionTimedWaitTimeoutAndSignal());
    ADD_TEST(testConditionProducerConsumerStress());
}
