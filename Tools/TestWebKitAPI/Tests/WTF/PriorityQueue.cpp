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

#include <ranges>
#include <type_traits>
#include <wtf/HashSet.h>
#include <wtf/PriorityQueue.h>

constexpr std::size_t operator""_z(unsigned long long n) { return n; }

template<typename T, typename Compare, size_t inlineCapacity>
static void enqueue(PriorityQueue<T, Compare, inlineCapacity>& queue, T element)
{
    size_t sizeBefore = queue.size();
    queue.enqueue(WTF::move(element));
    EXPECT_EQ(sizeBefore + 1, queue.size());
    EXPECT_FALSE(queue.isEmpty());
}

template<typename T, typename Compare, size_t inlineCapacity>
static T dequeue(PriorityQueue<T, Compare, inlineCapacity>& queue)
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

    // The default std::less serves the greatest element first, like std::priority_queue.
    for (unsigned i = 0; i < numElements; ++i) {
        unsigned expected = numElements - i - 1;
        EXPECT_EQ(expected, queue.peek());
        EXPECT_EQ(expected, dequeue(queue));
        EXPECT_EQ(expected, queue.size());
    }

    EXPECT_TRUE(queue.isEmpty());
}

TEST(WTF_PriorityQueue, ReversedComparator)
{
    const unsigned numElements = 10;
    PriorityQueue<unsigned, std::greater<unsigned>> queue;

    EXPECT_EQ(0_z, queue.size());
    EXPECT_TRUE(queue.isEmpty());

    for (unsigned i = 0; i < numElements; ++i) {
        enqueue(queue, i);
        EXPECT_EQ(i + 1, queue.size());
        EXPECT_FALSE(queue.isEmpty());
    }

    for (unsigned i = 0; i < numElements; ++i) {
        EXPECT_EQ(i, queue.peek());
        EXPECT_EQ(i, dequeue(queue));
        EXPECT_EQ(numElements - i - 1, queue.size());
    }

    EXPECT_TRUE(queue.isEmpty());
}

struct MoveOnlyIsLessThan {
    bool operator()(const MoveOnly& a, const MoveOnly& b) const { return a.value() < b.value(); }
};

struct MoveOnlyIsGreaterThan {
    bool operator()(const MoveOnly& a, const MoveOnly& b) const { return a.value() > b.value(); }
};

TEST(WTF_PriorityQueue, MoveOnly)
{
    PriorityQueue<MoveOnly, MoveOnlyIsLessThan> queue;

    Vector<unsigned> values = { 23, 54, 4, 8, 1, 2, 4, 0 };
    Vector<unsigned> sorted = values;
    std::ranges::sort(sorted, std::greater<unsigned>());

    for (auto value : values)
        queue.enqueue(MoveOnly(value));

    for (auto sortedValue : sorted) {
        auto value = queue.dequeue();
        EXPECT_EQ(sortedValue, value.value());
    }
}

TEST(WTF_PriorityQueue, DecreaseKey)
{
    PriorityQueue<MoveOnly, MoveOnlyIsLessThan> queue;

    Vector<unsigned> values = { 23, 54, 4, 8, 1, 2, 4, 0 };
    Vector<unsigned> sorted = values;
    sorted[3] = 3;
    std::ranges::sort(sorted, std::greater<unsigned>());

    for (auto value : values)
        queue.enqueue(MoveOnly(value));

    queue.decreaseKey([] (MoveOnly& m) {
        if (m.value() == 8) {
            m = MoveOnly(3);
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
    PriorityQueue<MoveOnly, MoveOnlyIsLessThan> queue;

    Vector<unsigned> values = { 23, 54, 4, 8, 1, 2, 4, 0 };
    Vector<unsigned> sorted = values;
    sorted[3] = 12;
    std::ranges::sort(sorted, std::greater<unsigned>());

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

// Under a reversed comparator a smaller value compares greater, so shrinking a value increases its key.
TEST(WTF_PriorityQueue, IncreaseKeyWithAReversedComparator)
{
    PriorityQueue<MoveOnly, MoveOnlyIsGreaterThan> queue;

    Vector<unsigned> values = { 23, 54, 4, 8, 1, 2, 4, 0 };
    Vector<unsigned> sorted = values;
    sorted[3] = 3;
    std::ranges::sort(sorted);

    for (auto value : values)
        queue.enqueue(MoveOnly(value));

    queue.increaseKey([] (MoveOnly& m) {
        if (m.value() == 8) {
            m = MoveOnly(3);
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
    PriorityQueue<MoveOnly, MoveOnlyIsGreaterThan> queue;

    Vector<unsigned> values = { 23, 54, 4, 8, 1, 2, 4, 0 };
    Vector<unsigned> sorted = values;
    std::ranges::sort(sorted, std::greater<unsigned>());

    for (auto value : values)
        queue.enqueue(MoveOnly(value));

    values.clear();
    for (auto& element : queue)
        values.append(element.value());

    std::ranges::sort(values, std::greater<unsigned>());
    EXPECT_EQ(values.size(), sorted.size());
    if (values.size() == sorted.size()) {
        for (size_t i = 0; i < values.size(); ++i)
            EXPECT_EQ(sorted[i], values[i]);
    }
}

TEST(WTF_PriorityQueue, EqualElementsAreAValidHeap)
{
    PriorityQueue<unsigned> queue;

    for (unsigned value : { 4u, 1u, 4u, 1u, 4u })
        enqueue(queue, value);

    EXPECT_TRUE(queue.isValidHeap());

    // Finding nothing still validates the heap, so a queue holding equal elements must not trip it.
    auto matchNothing = [] (unsigned&) {
        return false;
    };
    queue.decreaseKey(matchNothing);
    queue.increaseKey(matchNothing);

    EXPECT_EQ(4u, dequeue(queue));
    EXPECT_EQ(4u, dequeue(queue));
    EXPECT_EQ(4u, dequeue(queue));
    EXPECT_EQ(1u, dequeue(queue));
    EXPECT_EQ(1u, dequeue(queue));
    EXPECT_TRUE(queue.isEmpty());
}

static bool comparatorIsReversed { false };

struct ReversibleComparator {
    bool operator()(const unsigned& left, const unsigned& right) const
    {
        return comparatorIsReversed ? left > right : left < right;
    }
};

TEST(WTF_PriorityQueue, IsValidHeapDetectsAGreaterChild)
{
    PriorityQueue<unsigned, ReversibleComparator> queue;

    comparatorIsReversed = false;
    for (unsigned i = 0; i < 8; ++i)
        enqueue(queue, i);
    EXPECT_TRUE(queue.isValidHeap());

    // Reversing the order the elements were sifted with leaves every child greater than its parent.
    comparatorIsReversed = true;
    EXPECT_FALSE(queue.isValidHeap());

    comparatorIsReversed = false;
}

struct PrioritizedTask {
    unsigned priority { 0 };
    unsigned ticket { 0 };
};

// Shaped like the real clients: a greater priority is served first and equal priorities are served FIFO.
struct PrioritizedTaskIsLowerPriority {
    bool operator()(const PrioritizedTask& left, const PrioritizedTask& right) const
    {
        if (left.priority == right.priority)
            return left.ticket > right.ticket;
        return left.priority < right.priority;
    }
};

TEST(WTF_PriorityQueue, IncreaseKeyMovesTowardsTheFront)
{
    PriorityQueue<PrioritizedTask, PrioritizedTaskIsLowerPriority> queue;

    unsigned ticket = 0;
    for (unsigned priority : { 3u, 2u, 3u, 2u, 3u })
        queue.enqueue({ priority, ticket++ });

    queue.increaseKey([] (PrioritizedTask& task) {
        if (task.ticket != 4)
            return false;
        task.priority = 5;
        return true;
    });

    EXPECT_TRUE(queue.isValidHeap());

    Vector<unsigned> served;
    while (!queue.isEmpty())
        served.append(queue.dequeue().ticket);

    Vector<unsigned> expected = { 4, 0, 2, 1, 3 };
    EXPECT_EQ(expected.size(), served.size());
    if (expected.size() == served.size()) {
        for (size_t i = 0; i < expected.size(); ++i)
            EXPECT_EQ(expected[i], served[i]);
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

            // Sort ascending so the last element is the greatest, which is the one we dequeue.
            std::ranges::sort(values);
            EXPECT_EQ(values.takeLast(), queue.dequeue());

            continue;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        EXPECT_TRUE(queue.isValidHeap());
    }
}
