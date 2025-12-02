/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"

#include <algorithm>
#include <random>
#include <wtf/DataLog.h>
#include <wtf/IntervalSet.h>
#include <wtf/ListDump.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

struct IntervalSetTest {
    static constexpr bool verbose = false;
};

using Point = uint32_t;
using Value = int;
using Interval = Range<Point>;

TEST(WTF_IntervalSet, Basic)
{
    IntervalSet<Point, Value> intervalSet;

    EXPECT_TRUE(intervalSet.isEmpty());
    EXPECT_FALSE(intervalSet.hasOverlap({ 0, 10 }));
    EXPECT_FALSE(intervalSet.find({ 0, 10 }));

    EXPECT_EQ(intervalSet.begin(), intervalSet.end());
}

TEST(WTF_IntervalSet, SingleInterval)
{
    IntervalSet<Point, Value> intervalSet;

    // Insert a single interval [10, 20) with value 42
    intervalSet.insert({ 10, 20 }, 42);

    EXPECT_FALSE(intervalSet.isEmpty());

    EXPECT_NE(intervalSet.begin(), intervalSet.end());
    auto it = intervalSet.begin();
    EXPECT_EQ(it.interval(), Interval(10, 20));
    EXPECT_EQ(it.value(), 42);
    ++it;
    EXPECT_EQ(it, intervalSet.end());

    // Test overlap detection
    EXPECT_TRUE(intervalSet.hasOverlap({ 15, 25 })); // Overlaps
    EXPECT_TRUE(intervalSet.hasOverlap({ 5, 15 })); // Overlaps
    EXPECT_TRUE(intervalSet.hasOverlap({ 10, 20 })); // Exact match
    EXPECT_FALSE(intervalSet.hasOverlap({ 0, 10 })); // No overlap (adjacent)
    EXPECT_FALSE(intervalSet.hasOverlap({ 20, 30 })); // No overlap (adjacent)
    EXPECT_FALSE(intervalSet.hasOverlap({ 0, 5 })); // No overlap (before)
    EXPECT_FALSE(intervalSet.hasOverlap({ 25, 30 })); // No overlap (after)

    // Test find
    auto result = intervalSet.find({ 15, 16 });
    EXPECT_TRUE(result);
    EXPECT_EQ(result->first, Interval(10, 20));
    EXPECT_EQ(result->second, 42);

    // Test find with non-overlapping interval
    EXPECT_FALSE(intervalSet.find({ 0, 5 }));

    // Test erase functionality
    intervalSet.erase({ 10, 20 });

    EXPECT_EQ(intervalSet.begin(), intervalSet.end());

    // After erase, all overlap checks should return false
    EXPECT_FALSE(intervalSet.hasOverlap({ 15, 25 })); // No longer overlaps
    EXPECT_FALSE(intervalSet.hasOverlap({ 5, 15 })); // No longer overlaps
    EXPECT_FALSE(intervalSet.hasOverlap({ 10, 20 })); // No longer overlaps
    EXPECT_FALSE(intervalSet.hasOverlap({ 0, 10 })); // Still no overlap
    EXPECT_FALSE(intervalSet.hasOverlap({ 20, 30 })); // Still no overlap
    EXPECT_FALSE(intervalSet.hasOverlap({ 0, 5 })); // Still no overlap
    EXPECT_FALSE(intervalSet.hasOverlap({ 25, 30 })); // Still no overlap

    // After erase, all find operations should return nullopt
    EXPECT_FALSE(intervalSet.find({ 15, 16 }));
    EXPECT_FALSE(intervalSet.find({ 10, 20 }));
    EXPECT_FALSE(intervalSet.find({ 0, 5 }));
}

TEST(WTF_IntervalSet, EraseTests)
{
    IntervalSet<Point, Value> intervalSet;

    // Test basic erase functionality
    intervalSet.insert({ 10, 20 }, 100);
    intervalSet.insert({ 30, 40 }, 200);
    intervalSet.insert({ 50, 60 }, 300);

    // Verify iterator traverses all three intervals
    size_t count = 0;
    for (auto it = intervalSet.begin(); it != intervalSet.end(); ++it)
        count++;
    EXPECT_EQ(count, 3u);

    // Verify all intervals are present
    EXPECT_FALSE(intervalSet.isEmpty());
    EXPECT_TRUE(intervalSet.hasOverlap({ 10, 20 }));
    EXPECT_TRUE(intervalSet.hasOverlap({ 30, 40 }));
    EXPECT_TRUE(intervalSet.hasOverlap({ 50, 60 }));

    // Erase middle interval
    intervalSet.erase({ 30, 40 });

    // Verify iterator now traverses only two intervals
    count = 0;
    for (auto it = intervalSet.begin(); it != intervalSet.end(); ++it)
        count++;
    EXPECT_EQ(count, 2u);

    // Verify middle interval is gone, others remain
    EXPECT_FALSE(intervalSet.isEmpty());
    EXPECT_TRUE(intervalSet.hasOverlap({ 10, 20 }));
    EXPECT_FALSE(intervalSet.hasOverlap({ 30, 40 }));
    EXPECT_TRUE(intervalSet.hasOverlap({ 50, 60 }));

    // Verify find operations
    auto result = intervalSet.find({ 15, 16 });
    EXPECT_TRUE(result);
    EXPECT_EQ(result->first, Interval(10, 20));
    EXPECT_EQ(result->second, 100);

    EXPECT_FALSE(intervalSet.find({ 35, 36 }));

    result = intervalSet.find({ 55, 56 });
    EXPECT_TRUE(result);
    EXPECT_EQ(result->first, Interval(50, 60));
    EXPECT_EQ(result->second, 300);

    // Erase first interval
    intervalSet.erase({ 10, 20 });

    // Verify iterator now traverses only one interval
    count = 0;
    for (auto it = intervalSet.begin(); it != intervalSet.end(); ++it)
        count++;
    EXPECT_EQ(count, 1u);

    EXPECT_FALSE(intervalSet.isEmpty());
    EXPECT_FALSE(intervalSet.hasOverlap({ 10, 20 }));
    EXPECT_FALSE(intervalSet.hasOverlap({ 30, 40 }));
    EXPECT_TRUE(intervalSet.hasOverlap({ 50, 60 }));

    // Erase last interval (should make set empty)
    intervalSet.erase({ 50, 60 });

    // Verify iterator shows empty set
    EXPECT_EQ(intervalSet.begin(), intervalSet.end());

    EXPECT_TRUE(intervalSet.isEmpty());
    EXPECT_FALSE(intervalSet.hasOverlap({ 10, 20 }));
    EXPECT_FALSE(intervalSet.hasOverlap({ 30, 40 }));
    EXPECT_FALSE(intervalSet.hasOverlap({ 50, 60 }));

    // Verify all finds return nullopt on empty set
    EXPECT_FALSE(intervalSet.find({ 15, 16 }));
    EXPECT_FALSE(intervalSet.find({ 35, 36 }));
    EXPECT_FALSE(intervalSet.find({ 55, 56 }));
}

TEST(WTF_IntervalSet, EdgeCases)
{
    IntervalSet<Point, Value> intervalSet;

    // Insert interval [0, 1) - single unit interval
    intervalSet.insert({ 0, 1 }, 100);

    EXPECT_TRUE(intervalSet.hasOverlap({ 0, 1 }));
    EXPECT_FALSE(intervalSet.hasOverlap({ 1, 2 }));

    auto result = intervalSet.find({ 0, 1 });
    EXPECT_TRUE(result);
    EXPECT_EQ(result->first, Interval(0, 1));
    EXPECT_EQ(result->second, 100);

    // Test with larger intervals that span the small one
    EXPECT_TRUE(intervalSet.hasOverlap({ 0, 10 }));
    result = intervalSet.find({ 0, 10 });
    EXPECT_TRUE(result);
    EXPECT_EQ(result->first, Interval(0, 1));
    EXPECT_EQ(result->second, 100);
}

enum class IntervalOrdering {
    Ascending,
    Descending,
    Random,
};

template<unsigned numCacheLines>
static void stressTest(IntervalOrdering ordering)
{
    constexpr size_t numberTestIntervals = 10000;
    constexpr size_t maxGap = 1000;
    constexpr size_t maxSize = 1000;
    constexpr size_t maxPoint = numberTestIntervals * (maxGap + maxSize);

    struct TestCase : public std::pair<Interval, Value> {
        TestCase() = default;
        TestCase(const Interval& interval, const Value& value)
            : std::pair<Interval, Value>(interval, value) { }

        void dump(PrintStream& out) const
        {
            out.print("{ ", first, ", ", second, " }");
        }
    };

    using TestIntervalSet = IntervalSet<Point, Value, numCacheLines>;
    TestIntervalSet intervalSet;

    std::mt19937 gen(testing::UnitTest::GetInstance()->random_seed());
    std::uniform_int_distribution<size_t> gapDist(0, maxGap);
    std::uniform_int_distribution<size_t> sizeDist(1, maxSize);
    std::uniform_int_distribution<Value> valueDist(0, 10000);

    // Generate non-overlapping intervals by sorting start points
    Vector<TestCase> testData;
    Point end = 0;
    for (unsigned i = 0; i < numberTestIntervals; ++i) {
        Point start = end + gapDist(gen);
        end = start + sizeDist(gen);
        Value value = valueDist(gen);
        testData.append(TestCase({ start, end }, value));
    }
    dataLogLnIf(IntervalSetTest::verbose, "Test data: ", WTF::listDump(testData));

    auto shuffledTestData = testData;

    switch (ordering) {
    case IntervalOrdering::Ascending:
        break;
    case IntervalOrdering::Descending:
        std::reverse(shuffledTestData.begin(), shuffledTestData.end());
        break;
    case IntervalOrdering::Random:
        // Shuffle the intervals to insert them in random order
        std::shuffle(shuffledTestData.begin(), shuffledTestData.end(), gen);
        break;
    }
    dataLogLnIf(IntervalSetTest::verbose, "After shuffle: ", WTF::listDump(shuffledTestData));

    // Track which intervals are currently in the set for erase operations
    Vector<TestCase> currentlyInserted;
    std::uniform_int_distribution<int> eraseDist(1, 4); // 1 in 4 chance to erase

    auto maybeEraseInterval = [&]() {
        if (currentlyInserted.size() > 1 && eraseDist(gen) == 1) {
            std::uniform_int_distribution<size_t> eraseIndexDist(0, currentlyInserted.size() - 1);
            size_t eraseIndex = eraseIndexDist(gen);
            TestCase toErase = currentlyInserted[eraseIndex];

            intervalSet.erase(toErase.first);
            currentlyInserted.removeAt(eraseIndex);
            dataLogLnIf(IntervalSetTest::verbose, "Erased ", toErase.first, "=", toErase.second, ": ", intervalSet);
        }
    };

    for (const auto& entry : shuffledTestData) {
        intervalSet.insert(entry.first, entry.second);
        currentlyInserted.append(entry);
        dataLogLnIf(IntervalSetTest::verbose, "Added ", entry.first, "=", entry.second, ": ", intervalSet);

        maybeEraseInterval();
    }

    // Validate that nodes are densely populated.
    size_t capacity = TestIntervalSet::leafOrder;
    unsigned expectedHeight = 0;
    while (capacity < currentlyInserted.size()) {
        capacity *= TestIntervalSet::innerOrder;
        expectedHeight++;
    }
    EXPECT_EQ(intervalSet.height(), expectedHeight);

    // Validate iterator traversal: count and ordering
    size_t iteratorCount = 0;
    Point lastEnd = 0;
    for (auto it = intervalSet.begin(); it != intervalSet.end(); ++it) {
        iteratorCount++;
        auto interval = (*it).first;
        // Verify intervals are in sorted order (non-overlapping by construction)
        EXPECT_GE(interval.begin(), lastEnd);
        lastEnd = interval.end();
    }
    EXPECT_EQ(iteratorCount, currentlyInserted.size());

    // Test that all currently inserted intervals can be found with correct values
    std::shuffle(currentlyInserted.begin(), currentlyInserted.end(), gen);
    for (const auto& data : currentlyInserted) {
        dataLogLnIf(IntervalSetTest::verbose, "Testing: interval=", data.first, " value=", data.second);
        EXPECT_TRUE(intervalSet.hasOverlap(data.first));
        auto found = intervalSet.find(data.first);
        EXPECT_TRUE(found);
        EXPECT_EQ(found->first, data.first);
        EXPECT_EQ(found->second, data.second);
    }

    // Sort currentlyInserted by interval start for correct expected value calculation
    std::ranges::sort(currentlyInserted, [](const TestCase& a, const TestCase& b) {
        return a.first.begin() < b.first.begin();
    });

    std::uniform_int_distribution<size_t> pointDist(0, maxPoint);
    // Test random queries with occasional erase operations
    for (unsigned i = 0; i < 500; ++i) {
        Point start = pointDist(gen);
        Point end = start + sizeDist(gen);
        Interval query = { start, end };

        std::optional<std::pair<Interval, Value>> expected;
        for (const auto& data : currentlyInserted) {
            if (query.overlaps(data.first)) {
                expected = std::make_pair(data.first, data.second);
                break;
            }
        }
        dataLogLnIf(IntervalSetTest::verbose, "Testing: random interval=", query);

        EXPECT_EQ(expected.has_value(), intervalSet.hasOverlap(query));
        auto found = intervalSet.find(query);
        if (expected) {
            EXPECT_TRUE(found);
            EXPECT_EQ(found->first, expected->first);
            EXPECT_EQ(found->second, expected->second);
        } else
            EXPECT_FALSE(found);

        // Occasionally erase an interval during query phase (reduced frequency)
        if (i % 2)
            maybeEraseInterval();
    }
}

static constexpr unsigned stressNumCacheLines = 2;

TEST(WTF_IntervalSet, AscendingStressTest)
{
    stressTest<stressNumCacheLines>(IntervalOrdering::Ascending);
}

TEST(WTF_IntervalSet, DescendingStressTest)
{
    stressTest<stressNumCacheLines>(IntervalOrdering::Descending);
}

TEST(WTF_IntervalSet, RandomStressTest)
{
    stressTest<stressNumCacheLines>(IntervalOrdering::Random);
}

TEST(WTF_IntervalSet, Dump)
{
    IntervalSet<int, const char*> intervalSet;

    // Test empty tree
    StringPrintStream emptyOutput;
    intervalSet.dump(emptyOutput);
    String emptyResult = emptyOutput.toString();
    EXPECT_EQ(emptyResult, String("IntervalSet(height=0, leafOrder=4, innerOrder=4) <empty>"_s));

    intervalSet.insert({ 10, 20 }, "first");
    intervalSet.insert({ 30, 40 }, "second");
    intervalSet.insert({ 50, 60 }, "third");

    // Single leaf node
    StringPrintStream basicOutput;
    intervalSet.dump(basicOutput);
    String basicResult = basicOutput.toString();
    String expectedBasic = "IntervalSet(height=0, leafOrder=4, innerOrder=4) coverage=10...60\nLeaf(size=3): 10...20=first, 30...40=second, 50...60=third\n"_s;
    EXPECT_EQ(basicResult, expectedBasic);

    // Add more intervals to cause split
    intervalSet.insert({ 5, 8 }, "before");
    intervalSet.insert({ 25, 28 }, "middle");
    intervalSet.insert({ 65, 70 }, "after");

    StringPrintStream fullOutput;
    intervalSet.dump(fullOutput);
    String fullResult = fullOutput.toString();
    String expectedFull = "IntervalSet(height=1, leafOrder=4, innerOrder=4) coverage=5...70\nInner(size=2, coverage=5...70):\n  [0] 5...28\n    Leaf(size=3): 5...8=before, 10...20=first, 25...28=middle\n  [1] 30...70\n    Leaf(size=3): 30...40=second, 50...60=third, 65...70=after\n"_s;
    EXPECT_EQ(fullResult, expectedFull);
}

TEST(WTF_IntervalSet, DestructorMemoryManagement)
{
    // Test destructor with single leaf node
    {
        IntervalSet<Point, Value> intervalSet;
        intervalSet.insert({ 10, 20 }, 42);
        intervalSet.insert({ 30, 40 }, 84);
    }

    // Test destructor with multi-level tree (force tree growth)
    {
        IntervalSet<Point, Value> intervalSet;

        // Insert enough intervals to force tree growth beyond single leaf
        for (Point i = 0; i < 100; ++i) {
            Point start = i * 10;
            Point end = start + 5;
            intervalSet.insert({ start, end }, static_cast<Value>(i));
        }
    }

    // Test destructor with empty tree
    {
        IntervalSet<Point, Value> intervalSet;
    }
}

TEST(WTF_IntervalSet, EraseLastItemSingleLeaf)
{
    IntervalSet<Point, Value> intervalSet;

    // Test case: Tree with only a single leaf node, erase the last (and only) item
    intervalSet.insert({ 10, 20 }, 42);

    // Verify the interval is present
    EXPECT_TRUE(intervalSet.hasOverlap({ 10, 20 }));
    auto result = intervalSet.find({ 15, 16 });
    EXPECT_TRUE(result);
    EXPECT_EQ(result->first, Interval(10, 20));
    EXPECT_EQ(result->second, 42);

    // Erase the only interval - this should make the tree empty
    intervalSet.erase({ 10, 20 });

    // Verify the tree is now empty
    EXPECT_FALSE(intervalSet.hasOverlap({ 10, 20 }));
    EXPECT_FALSE(intervalSet.find({ 15, 16 }));
    EXPECT_FALSE(intervalSet.find({ 0, 100 })); // Any query should return nullopt

    // Test that we can still insert after emptying the tree
    intervalSet.insert({ 30, 40 }, 100);
    EXPECT_TRUE(intervalSet.hasOverlap({ 30, 40 }));
    result = intervalSet.find({ 35, 36 });
    EXPECT_TRUE(result);
    EXPECT_EQ(result->first, Interval(30, 40));
    EXPECT_EQ(result->second, 100);
}

TEST(WTF_IntervalSet, EraseLastItemWithInnerNodes)
{
    IntervalSet<Point, Value> intervalSet;

    // Build a tree with inner nodes by inserting many intervals
    Vector<Interval> intervals;
    for (Point i = 0; i < 50; ++i) {
        Point start = i * 10;
        Point end = start + 5;
        Interval interval = { start, end };
        intervals.append(interval);
        intervalSet.insert(interval, static_cast<Value>(i));
    }

    // Verify we have a multi-level tree by checking all intervals are present
    for (size_t i = 0; i < intervals.size(); ++i) {
        EXPECT_TRUE(intervalSet.hasOverlap(intervals[i]));
        auto result = intervalSet.find(intervals[i]);
        EXPECT_TRUE(result);
        EXPECT_EQ(result->first, intervals[i]);
        EXPECT_EQ(result->second, static_cast<Value>(i));
    }

    // Erase all intervals one by one until only one remains
    for (size_t i = 0; i < intervals.size() - 1; ++i) {
        intervalSet.erase(intervals[i]);

        // Verify the erased interval is gone
        EXPECT_FALSE(intervalSet.hasOverlap(intervals[i]));
        EXPECT_FALSE(intervalSet.find(intervals[i]));

        // Verify remaining intervals are still present
        for (size_t j = i + 1; j < intervals.size(); ++j)
            EXPECT_TRUE(intervalSet.hasOverlap(intervals[j]));
    }

    // Now erase the very last interval - this should collapse the tree to empty
    Interval lastInterval = intervals.last();
    Value lastValue = static_cast<Value>(intervals.size() - 1);

    // Verify the last interval is still present
    EXPECT_TRUE(intervalSet.hasOverlap(lastInterval));
    auto result = intervalSet.find(lastInterval);
    EXPECT_TRUE(result);
    EXPECT_EQ(result->first, lastInterval);
    EXPECT_EQ(result->second, lastValue);

    intervalSet.erase(lastInterval);

    EXPECT_FALSE(intervalSet.hasOverlap(lastInterval));
    EXPECT_FALSE(intervalSet.find(lastInterval));

    EXPECT_FALSE(intervalSet.hasOverlap({ 0, 1000 }));
    EXPECT_FALSE(intervalSet.find({ 0, 1000 }));

    // Verify we can still insert after completely emptying a complex tree
    intervalSet.insert({ 1000, 2000 }, 999);
    EXPECT_TRUE(intervalSet.hasOverlap({ 1000, 2000 }));
    result = intervalSet.find({ 1500, 1600 });
    EXPECT_TRUE(result);
    EXPECT_EQ(result->first, Interval(1000, 2000));
    EXPECT_EQ(result->second, 999);
}

} // namespace TestWebKitAPI
