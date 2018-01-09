/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "MoveOnly.h"

#include <type_traits>
#include <wtf/HashSet.h>
#include <wtf/PriorityQueue.h>

constexpr std::size_t operator "" _z ( unsigned long long n ) { return n; }

template<typename T, bool (*isHigherPriority)(const T&, const T&)>
static void enqueue(PriorityQueue<T, isHigherPriority>& queue, T element)
{
    size_t sizeBefore = queue.size();
    queue.enqueue(WTFMove(element));
    EXPECT_EQ(sizeBefore + 1, queue.size());
    EXPECT_FALSE(queue.isEmpty());
}

template<typename T, bool (*isHigherPriority)(const T&, const T&)>
static T dequeue(PriorityQueue<T, isHigherPriority>& queue)
{
    EXPECT_FALSE(queue.isEmpty());
    size_t sizeBefore = queue.size();
    T result = queue.dequeue();
    EXPECT_EQ(sizeBefore - 1, queue.size());
    return result;
}


TEST(WTF_PriorityQueue, Basic)
{
    const unsigned numElements = 10;
    PriorityQueue<unsigned> queue;

    EXPECT_EQ(0_z, queue.size());
    EXPECT_TRUE(queue.isEmpty());

    for (unsigned i = 0; i < numElements; ++i)
        enqueue(queue, i);

    for (unsigned i = 0; i < numElements; ++i) {
        EXPECT_EQ(i, queue.peek());
        EXPECT_EQ(i, dequeue(queue));
        EXPECT_EQ(numElements - i - 1, queue.size());
    }

    EXPECT_TRUE(queue.isEmpty());
}

TEST(WTF_PriorityQueue, CustomPriorityFunction)
{
    const unsigned numElements = 10;
    PriorityQueue<unsigned, &isGreaterThan<unsigned>> queue;

    EXPECT_EQ(0_z, queue.size());
    EXPECT_TRUE(queue.isEmpty());

    for (unsigned i = 0; i < numElements; ++i) {
        enqueue(queue, i);
        EXPECT_EQ(i + 1, queue.size());
        EXPECT_FALSE(queue.isEmpty());
    }

    for (unsigned i = 0; i < numElements; ++i) {
        EXPECT_EQ(numElements - i - 1, queue.peek());
        EXPECT_EQ(numElements - i - 1, dequeue(queue));
        EXPECT_EQ(numElements - i - 1, queue.size());
    }

    EXPECT_TRUE(queue.isEmpty());
}

template<bool (*isHigherPriority)(const unsigned&, const unsigned&)>
struct CompareMove {
    static bool compare(const MoveOnly& m1, const MoveOnly& m2)
    {
        return isHigherPriority(m1.value(), m2.value());
    }
};

TEST(WTF_PriorityQueue, MoveOnly)
{
    PriorityQueue<MoveOnly, &CompareMove<&isLessThan<unsigned>>::compare> queue;

    Vector<unsigned> values = { 23, 54, 4, 8, 1, 2, 4, 0 };
    Vector<unsigned> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    for (auto value : values)
        queue.enqueue(MoveOnly(value));

    for (auto sortedValue : sorted) {
        auto value = queue.dequeue();
        EXPECT_EQ(sortedValue, value.value());
    }
}

TEST(WTF_PriorityQueue, DecreaseKey)
{
    PriorityQueue<MoveOnly, &CompareMove<&isLessThan<unsigned>>::compare> queue;

    Vector<unsigned> values = { 23, 54, 4, 8, 1, 2, 4, 0 };
    Vector<unsigned> sorted = values;
    sorted[3] = 12;
    std::sort(sorted.begin(), sorted.end());

    for (auto value : values)
        queue.enqueue(MoveOnly(value));

    queue.decreaseKey([] (MoveOnly& m) {
        if (m.value() == 8) {
            m = MoveOnly(12);
            return true;
        }
        return false;
    });

    for (auto sortedValue : sorted) {
        auto value = queue.dequeue();
        EXPECT_EQ(sortedValue, value.value());
    }
}

TEST(WTF_PriorityQueue, IncreaseKey)
{
    PriorityQueue<MoveOnly, &CompareMove<&isGreaterThan<unsigned>>::compare> queue;

    Vector<unsigned> values = { 23, 54, 4, 8, 1, 2, 4, 0 };
    Vector<unsigned> sorted = values;
    sorted[3] = 12;
    std::sort(sorted.begin(), sorted.end(), std::greater<unsigned>());

    for (auto value : values)
        queue.enqueue(MoveOnly(value));

    queue.increaseKey([] (MoveOnly& m) {
        if (m.value() == 8) {
            m = MoveOnly(12);
            return true;
        }
        return false;
    });

    for (auto sortedValue : sorted) {
        auto value = queue.dequeue();
        EXPECT_EQ(sortedValue, value.value());
    }
}

TEST(WTF_PriorityQueue, Iteration)
{
    PriorityQueue<MoveOnly, &CompareMove<&isGreaterThan<unsigned>>::compare> queue;

    Vector<unsigned> values = { 23, 54, 4, 8, 1, 2, 4, 0 };
    Vector<unsigned> sorted = values;
    std::sort(sorted.begin(), sorted.end(), std::greater<unsigned>());

    for (auto value : values)
        queue.enqueue(MoveOnly(value));

    values.clear();
    for (auto& element : queue)
        values.append(element.value());

    std::sort(values.begin(), values.end(), std::greater<unsigned>());
    EXPECT_EQ(values.size(), sorted.size());
    if (values.size() == sorted.size()) {
        for (size_t i = 0; i < values.size(); ++i)
            EXPECT_EQ(sorted[i], values[i]);
    }
}

TEST(WTF_PriorityQueue, RandomActions)
{
    const uint64_t prime1 = 15487237;
    const uint64_t prime2 = 179428283;
    uint64_t randomNumber = 19405709;

    auto nextRandom = [&] () -> uint64_t {
        randomNumber = randomNumber * prime1 + prime2;
        return randomNumber;
    };

    PriorityQueue<uint64_t> queue;
    Vector<uint64_t> values;

    enum Cases {
        Enqueue,
        Dequeue,
        NumberOfCases
    };

    // Seed the queue.
    for (unsigned i = 0; i < 100; ++i) {
        auto number = nextRandom();
        queue.enqueue(number);
        values.append(number);
        EXPECT_TRUE(queue.isValidHeap());
    }

    for (unsigned i = 0; i < 10000; ++i) {
        auto number = nextRandom();
        switch (number % NumberOfCases) {
        case Enqueue: {
            queue.enqueue(number);
            values.append(number);
            EXPECT_TRUE(queue.isValidHeap());
            EXPECT_EQ(values.size(), queue.size());
            continue;
        }

        case Dequeue: {
            EXPECT_EQ(values.size(), queue.size());
            if (values.size() != queue.size())
                break;

            if (!values.size())
                continue;

            // Sort with greater so the last element should be the one we dequeue.
            std::sort(values.begin(), values.end(), std::greater<uint64_t>());
            EXPECT_EQ(values.takeLast(), queue.dequeue());

            continue;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        EXPECT_TRUE(queue.isValidHeap());
    }
}
