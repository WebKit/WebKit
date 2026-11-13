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
#include <wtf/OrderedHashSet.h>

#include "Helpers/Test.h"
#include "MoveOnly.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_OrderedHashSet, EmptySet)
{
    OrderedHashSet<int> set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(0u, set.size());
    EXPECT_TRUE(set.begin() == set.end());
}

TEST(WTF_OrderedHashSet, BasicAddAndContains)
{
    OrderedHashSet<int> set;
    auto result = set.add(1);
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_EQ(1u, set.size());

    auto result2 = set.add(1);
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(1u, set.size());

    EXPECT_TRUE(set.contains(1));
    EXPECT_FALSE(set.contains(2));
}

TEST(WTF_OrderedHashSet, InsertionOrderPreserved)
{
    OrderedHashSet<int> set;
    set.add(5);
    set.add(3);
    set.add(1);
    set.add(4);
    set.add(2);

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(5u, values.size());
    EXPECT_EQ(5, values[0]);
    EXPECT_EQ(3, values[1]);
    EXPECT_EQ(1, values[2]);
    EXPECT_EQ(4, values[3]);
    EXPECT_EQ(2, values[4]);
}

TEST(WTF_OrderedHashSet, InsertionOrderPreservedAfterDeletion)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);
    set.add(4);
    set.add(5);

    set.remove(3);

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(4u, values.size());
    EXPECT_EQ(1, values[0]);
    EXPECT_EQ(2, values[1]);
    EXPECT_EQ(4, values[2]);
    EXPECT_EQ(5, values[3]);
}

TEST(WTF_OrderedHashSet, Remove)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    EXPECT_TRUE(set.remove(2));
    EXPECT_FALSE(set.contains(2));
    EXPECT_EQ(2u, set.size());
    EXPECT_FALSE(set.remove(999));
}

TEST(WTF_OrderedHashSet, RemoveByIterator)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    auto it = set.find(2);
    EXPECT_TRUE(set.remove(it));
    EXPECT_FALSE(set.contains(2));
    EXPECT_EQ(2u, set.size());
}

TEST(WTF_OrderedHashSet, Take)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    auto value = set.take(2);
    EXPECT_EQ(2, value);
    EXPECT_FALSE(set.contains(2));
    EXPECT_EQ(2u, set.size());

    auto missing = set.take(999);
    EXPECT_EQ(0, missing); // Default for missing
}

TEST(WTF_OrderedHashSet, TakeAny)
{
    OrderedHashSet<int> set;
    set.add(5);
    set.add(3);
    set.add(1);

    auto value = set.takeAny();
    EXPECT_EQ(5, value); // First in insertion order
    EXPECT_EQ(2u, set.size());
}

TEST(WTF_OrderedHashSet, Clear)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.clear();

    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(0u, set.size());
    EXPECT_TRUE(set.begin() == set.end());
}

TEST(WTF_OrderedHashSet, Swap)
{
    OrderedHashSet<int> set1;
    set1.add(1);
    set1.add(2);

    OrderedHashSet<int> set2;
    set2.add(3);

    set1.swap(set2);

    EXPECT_EQ(1u, set1.size());
    EXPECT_TRUE(set1.contains(3));
    EXPECT_EQ(2u, set2.size());
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set2.contains(2));
}

TEST(WTF_OrderedHashSet, CopyConstruction)
{
    OrderedHashSet<int> set1;
    set1.add(3);
    set1.add(1);
    set1.add(2);

    OrderedHashSet<int> set2(set1);

    EXPECT_EQ(3u, set2.size());

    Vector<int> values;
    for (auto& v : set2)
        values.append(v);

    EXPECT_EQ(3, values[0]);
    EXPECT_EQ(1, values[1]);
    EXPECT_EQ(2, values[2]);
}

TEST(WTF_OrderedHashSet, MoveConstruction)
{
    OrderedHashSet<int> set1;
    set1.add(1);
    set1.add(2);

    OrderedHashSet<int> set2(WTF::move(set1));

    EXPECT_EQ(2u, set2.size());
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set2.contains(2));
    EXPECT_TRUE(set1.isEmpty());
}

TEST(WTF_OrderedHashSet, CopyAssignment)
{
    OrderedHashSet<int> set1;
    set1.add(1);
    set1.add(2);

    OrderedHashSet<int> set2;
    set2.add(9);
    set2 = set1;

    EXPECT_EQ(2u, set2.size());
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set2.contains(2));
    EXPECT_FALSE(set2.contains(9));
}

TEST(WTF_OrderedHashSet, MoveAssignment)
{
    OrderedHashSet<int> set1;
    set1.add(1);

    OrderedHashSet<int> set2;
    set2.add(9);
    set2 = WTF::move(set1);

    EXPECT_EQ(1u, set2.size());
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set1.isEmpty());
}

TEST(WTF_OrderedHashSet, RehashPreservesOrder)
{
    OrderedHashSet<int> set;
    Vector<int> expectedOrder;
    for (int i = 0; i < 100; ++i) {
        set.add(i);
        expectedOrder.append(i);
    }

    Vector<int> actualOrder;
    for (auto& v : set)
        actualOrder.append(v);

    EXPECT_EQ(expectedOrder, actualOrder);
}

TEST(WTF_OrderedHashSet, DeleteAndReinsert)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    set.remove(2);
    set.add(2); // Re-add goes at end

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(3u, values.size());
    EXPECT_EQ(1, values[0]);
    EXPECT_EQ(3, values[1]);
    EXPECT_EQ(2, values[2]); // Re-inserted at end
}

TEST(WTF_OrderedHashSet, ManyDeletesAndInserts)
{
    OrderedHashSet<int> set;
    for (int i = 0; i < 50; ++i)
        set.add(i);

    for (int i = 0; i < 50; i += 2)
        set.remove(i);

    EXPECT_EQ(25u, set.size());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    for (int i = 0; i < 25; ++i)
        EXPECT_EQ(i * 2 + 1, values[i]);
}

TEST(WTF_OrderedHashSet, StringValues)
{
    OrderedHashSet<String> set;
    set.add("banana"_s);
    set.add("apple"_s);
    set.add("cherry"_s);

    Vector<String> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(3u, values.size());
    EXPECT_EQ("banana"_s, values[0]);
    EXPECT_EQ("apple"_s, values[1]);
    EXPECT_EQ("cherry"_s, values[2]);
}

TEST(WTF_OrderedHashSet, InitializerList)
{
    OrderedHashSet<int> set { 5, 3, 1, 4, 2 };

    EXPECT_EQ(5u, set.size());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(5, values[0]);
    EXPECT_EQ(3, values[1]);
    EXPECT_EQ(1, values[2]);
    EXPECT_EQ(4, values[3]);
    EXPECT_EQ(2, values[4]);
}

TEST(WTF_OrderedHashSet, AddAll)
{
    OrderedHashSet<int> set;
    set.add(1);

    Vector<int> toAdd { 2, 3, 4 };
    set.addAll(toAdd);

    EXPECT_EQ(4u, set.size());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(1, values[0]);
    EXPECT_EQ(2, values[1]);
    EXPECT_EQ(3, values[2]);
    EXPECT_EQ(4, values[3]);
}

TEST(WTF_OrderedHashSet, Find)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    auto it = set.find(2);
    EXPECT_TRUE(it != set.end());
    EXPECT_EQ(2, *it);

    auto missing = set.find(999);
    EXPECT_TRUE(missing == set.end());
}

TEST(WTF_OrderedHashSet, ReserveCapacity)
{
    OrderedHashSet<int> set;
    set.reserveInitialCapacity(100);

    for (int i = 0; i < 100; ++i)
        set.add(i);

    EXPECT_EQ(100u, set.size());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(i, values[i]);
}

TEST(WTF_OrderedHashSet, StressTest)
{
    OrderedHashSet<int> set;
    // Insert 1000 elements
    for (int i = 0; i < 1000; ++i)
        set.add(i);

    EXPECT_EQ(1000u, set.size());

    // Remove every 3rd element
    for (int i = 0; i < 1000; i += 3)
        set.remove(i);

    // Verify remaining elements are in insertion order
    Vector<int> remaining;
    for (auto& v : set)
        remaining.append(v);

    int expectedIdx = 0;
    for (int i = 0; i < 1000; ++i) {
        if (i % 3) {
            EXPECT_EQ(i, remaining[expectedIdx]);
            ++expectedIdx;
        }
    }

    EXPECT_EQ(static_cast<unsigned>(expectedIdx), set.size());
}

TEST(WTF_OrderedHashSet, CompactInPlacePreservesOrderAndCapacity)
{
    // With initialBucketCount=8, entriesCapacity starts at 6 and the table
    // does not shrink below initialBucketCount, so compactInPlace is the only
    // rehash path triggered here.
    OrderedHashSet<int> set;
    for (int i = 0; i < 6; ++i)
        set.add(i);
    unsigned initialCapacity = set.capacity();

    set.remove(1);
    set.remove(2);
    set.remove(3);
    set.remove(4);
    EXPECT_EQ(2u, set.size());
    EXPECT_EQ(initialCapacity, set.capacity());

    // Adding now triggers rehashForAdd. liveCount (2) < 3/4 * capacity (6),
    // so compactInPlace runs and reuses both the entries and buckets allocations.
    set.add(100);
    EXPECT_EQ(3u, set.size());
    EXPECT_EQ(initialCapacity, set.capacity());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);
    EXPECT_EQ(3u, values.size());
    EXPECT_EQ(0, values[0]);
    EXPECT_EQ(5, values[1]);
    EXPECT_EQ(100, values[2]);

    EXPECT_TRUE(set.contains(0));
    EXPECT_TRUE(set.contains(5));
    EXPECT_TRUE(set.contains(100));
    EXPECT_FALSE(set.contains(1));
    EXPECT_FALSE(set.contains(4));
}

TEST(WTF_OrderedHashSet, CompactInPlaceOnLargerTable)
{
    // Force a larger table so we can verify compactInPlace works beyond the
    // initial capacity without triggering a grow or shrink rehash.
    OrderedHashSet<int> set;
    set.reserveInitialCapacity(12);
    unsigned reservedCapacity = set.capacity();
    EXPECT_GE(reservedCapacity, 12u);

    for (int i = 0; i < 12; ++i)
        set.add(i);
    EXPECT_EQ(12u, set.size());
    EXPECT_EQ(reservedCapacity, set.capacity());

    // shrinkIfNeeded fires when liveCount < entriesLength / 4 (= 3). Keep liveCount
    // at 3 so the subsequent add takes the compactInPlace branch, not shrink.
    for (int i = 0; i < 9; ++i)
        set.remove(i);
    EXPECT_EQ(3u, set.size());
    EXPECT_EQ(reservedCapacity, set.capacity());

    set.add(100);
    EXPECT_EQ(4u, set.size());
    EXPECT_EQ(reservedCapacity, set.capacity());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);
    EXPECT_EQ(4u, values.size());
    EXPECT_EQ(9, values[0]);
    EXPECT_EQ(10, values[1]);
    EXPECT_EQ(11, values[2]);
    EXPECT_EQ(100, values[3]);
}

TEST(WTF_OrderedHashSet, CompactInPlaceWithStrings)
{
    // Non-trivial value type exercises move/destroy paths inside compactInPlace.
    OrderedHashSet<String> set;
    set.add("zero"_s);
    set.add("one"_s);
    set.add("two"_s);
    set.add("three"_s);
    set.add("four"_s);
    set.add("five"_s);
    unsigned initialCapacity = set.capacity();

    set.remove("one"_s);
    set.remove("two"_s);
    set.remove("three"_s);
    set.remove("four"_s);

    set.add("new"_s);
    EXPECT_EQ(initialCapacity, set.capacity());
    EXPECT_EQ(3u, set.size());

    Vector<String> values;
    for (auto& v : set)
        values.append(v);
    EXPECT_EQ(3u, values.size());
    EXPECT_EQ("zero"_s, values[0]);
    EXPECT_EQ("five"_s, values[1]);
    EXPECT_EQ("new"_s, values[2]);

    EXPECT_TRUE(set.contains("zero"_s));
    EXPECT_TRUE(set.contains("five"_s));
    EXPECT_TRUE(set.contains("new"_s));
    EXPECT_FALSE(set.contains("one"_s));
    EXPECT_FALSE(set.contains("four"_s));
}

TEST(WTF_OrderedHashSet, CompactInPlaceRepeatedCycles)
{
    // Many cycles of "delete most, refill to capacity" keep hitting the compactInPlace
    // branch. Verify the table remains consistent across all of them.
    OrderedHashSet<int> set;
    set.reserveInitialCapacity(12);
    unsigned capacity = set.capacity();

    for (int i = 0; i < 12; ++i)
        set.add(i);

    int nextValue = 100;
    for (int cycle = 0; cycle < 20; ++cycle) {
        Vector<int> snapshot;
        for (auto& v : set)
            snapshot.append(v);
        // Remove down to 3 live entries (just above the shrink threshold).
        for (unsigned i = 0; i < snapshot.size() && set.size() > 3; ++i)
            set.remove(snapshot[i]);
        EXPECT_EQ(3u, set.size());

        // Refilling will trigger compactInPlace on the first add past capacity.
        while (set.size() < 12)
            set.add(nextValue++);
        EXPECT_EQ(12u, set.size());
        EXPECT_EQ(capacity, set.capacity()); // No growth happened.

        // All values must round-trip via contains.
        Vector<int> live;
        for (auto& v : set)
            live.append(v);
        for (int v : live)
            EXPECT_TRUE(set.contains(v));
    }
}

TEST(WTF_OrderedHashSet, RemoveIfAcrossShrinkThreshold)
{
    // Fill past initialBucketCount so shrinkIfNeeded() can actually rehash, then
    // removeIf enough entries to cross the 1/4 shrink threshold. Prior to the fix
    // that deferred shrink, this would rehash mid-iteration and skip entries.
    OrderedHashSet<int> set;
    for (int i = 0; i < 64; ++i)
        set.add(i);
    EXPECT_EQ(64u, set.size());

    bool changed = set.removeIf([](int value) {
        return value >= 8;
    });
    EXPECT_TRUE(changed);
    EXPECT_EQ(8u, set.size());

    Vector<int> remaining;
    for (int v : set)
        remaining.append(v);
    EXPECT_EQ(8u, remaining.size());
    for (unsigned i = 0; i < remaining.size(); ++i)
        EXPECT_EQ(static_cast<int>(i), remaining[i]);

    for (int i = 0; i < 8; ++i)
        EXPECT_TRUE(set.contains(i));
    for (int i = 8; i < 64; ++i)
        EXPECT_FALSE(set.contains(i));
}

TEST(WTF_OrderedHashSet, MoveOnlyValue)
{
    OrderedHashSet<MoveOnly> set;
    auto addResult = set.add(MoveOnly(1));
    EXPECT_TRUE(addResult.isNewEntry);
    EXPECT_EQ(1u, addResult.iterator->value());

    set.add(MoveOnly(2));
    set.add(MoveOnly(3));
    EXPECT_EQ(3u, set.size());
    EXPECT_TRUE(set.contains(MoveOnly(1)));
    EXPECT_TRUE(set.contains(MoveOnly(2)));
    EXPECT_TRUE(set.contains(MoveOnly(3)));

    auto duplicate = set.add(MoveOnly(2));
    EXPECT_FALSE(duplicate.isNewEntry);
    EXPECT_EQ(3u, set.size());

    auto taken = set.take(MoveOnly(2));
    EXPECT_EQ(2u, taken.value());
    EXPECT_FALSE(set.contains(MoveOnly(2)));

    Vector<unsigned> remainingInOrder;
    for (auto& v : set)
        remainingInOrder.append(v.value());
    EXPECT_EQ(2u, remainingInOrder.size());
    EXPECT_EQ(1u, remainingInOrder[0]);
    EXPECT_EQ(3u, remainingInOrder[1]);
}

TEST(WTF_OrderedHashSet, MoveOnlyValueRehash)
{
    OrderedHashSet<MoveOnly> set;
    for (unsigned i = 0; i < 64; ++i)
        set.add(MoveOnly(i));
    EXPECT_EQ(64u, set.size());
    for (unsigned i = 0; i < 64; ++i)
        EXPECT_TRUE(set.contains(MoveOnly(i)));
}

TEST(WTF_OrderedHashSet, HashTranslatorFindContains)
{
    OrderedHashSet<String> set;
    set.add("alpha"_s);
    set.add("beta"_s);
    set.add("gamma"_s);

    auto it = set.find<StringViewHashTranslator>(StringView { "beta"_s });
    EXPECT_TRUE(it != set.end());
    EXPECT_EQ("beta"_s, *it);

    EXPECT_TRUE(set.contains<StringViewHashTranslator>(StringView { "alpha"_s }));
    EXPECT_TRUE(set.contains<StringViewHashTranslator>(StringView { "gamma"_s }));
    EXPECT_FALSE(set.contains<StringViewHashTranslator>(StringView { "delta"_s }));

    auto missing = set.find<StringViewHashTranslator>(StringView { "delta"_s });
    EXPECT_TRUE(missing == set.end());
}

namespace {

struct ConstructingStringViewTranslator {
    static unsigned hash(StringView view) { return StringHash::hash(view.toString()); }
    static bool equal(const String& a, StringView b) { return a == b; }
    static void translate(String* location, StringView view, NOESCAPE const auto&)
    {
        std::construct_at(location, view.toString());
    }
};

using StringOrderedHashTable = WTF::OrderedHashTable<String, String, WTF::IdentityExtractor, DefaultHash<String>, HashTraits<String>, HashTraits<String>, WTF::HashTableMalloc>;

// Counts every construction and destruction, and has neither a default constructor nor a
// deleted-value constructor, so the table cannot be relying on any reserved value of its own.
class CountedValue {
public:
    static int liveCount;
    static int constructionCount;

    explicit CountedValue(int value)
        : m_value(value)
    {
        ++liveCount;
        ++constructionCount;
    }
    CountedValue(const CountedValue& other)
        : m_value(other.m_value)
    {
        ++liveCount;
        ++constructionCount;
    }
    CountedValue(CountedValue&& other)
        : m_value(other.m_value)
    {
        ++liveCount;
        ++constructionCount;
    }
    ~CountedValue()
    {
        EXPECT_NE(m_value, destroyedMarker);
        m_value = destroyedMarker;
        --liveCount;
    }
    CountedValue& operator=(const CountedValue&) = delete;

    int value() const { return m_value; }
    friend bool operator==(const CountedValue& a, const CountedValue& b) { return a.m_value == b.m_value; }

private:
    static constexpr int destroyedMarker = 0x5ead5ead;
    int m_value;
};

int CountedValue::liveCount = 0;
int CountedValue::constructionCount = 0;

struct CountedValueHash {
    static unsigned hash(const CountedValue& value) { return IntHash<int>::hash(value.value()); }
    static bool equal(const CountedValue& a, const CountedValue& b) { return a == b; }
};

struct CountedValueTranslator {
    static unsigned hash(int value) { return IntHash<int>::hash(value); }
    static bool equal(const CountedValue& a, int b) { return a.value() == b; }
    static void translate(CountedValue* location, int value, NOESCAPE const auto&)
    {
        std::construct_at(location, value);
    }
};

// emptyValue() is only what OrderedHashSet::take() returns when nothing is found.
struct CountedValueTraits : WTF::GenericHashTraits<CountedValue> {
    static CountedValue emptyValue() { return CountedValue(INT_MIN); }
};

using CountedOrderedHashSet = OrderedHashSet<CountedValue, CountedValueHash, CountedValueTraits>;
using CountedOrderedHashTable = WTF::OrderedHashTable<CountedValue, CountedValue, WTF::IdentityExtractor, CountedValueHash, CountedValueTraits, CountedValueTraits, WTF::HashTableMalloc>;

// Every value lands in one bucket chain, so removals leave deleted buckets in the middle of the
// probe sequence that later lookups and adds have to step over.
struct CollidingIntHash {
    static unsigned hash(int) { return 7; }
    static bool equal(int a, int b) { return a == b; }
};

template<typename Set>
Vector<int> contentsOf(const Set& set)
{
    Vector<int> result;
    for (auto& value : set)
        result.append(value);
    return result;
}

template<typename Set>
Vector<int> reverseContentsOf(const Set& set)
{
    Vector<int> result;
    for (auto it = set.rbegin(); it != set.rend(); ++it)
        result.append(*it);
    return result;
}

} // anonymous namespace

TEST(WTF_OrderedHashSet, HashTranslatorAddConstructsIntoRawStorage)
{
    CountedValue::liveCount = 0;
    auto noFunctor = [] {
        return CountedValue(-1);
    };
    {
        CountedOrderedHashTable table;
        for (int i = 0; i < 32; ++i)
            EXPECT_TRUE(table.add<CountedValueTranslator>(i, noFunctor).isNewEntry);
        EXPECT_EQ(32u, table.size());
        EXPECT_EQ(32, CountedValue::liveCount);

        // Re-adding through the translator after removals lands in storage whose previous
        // occupant was destroyed, so translate() must construct rather than assign.
        for (int i = 0; i < 32; i += 2)
            table.remove(CountedValue(i));
        EXPECT_EQ(16u, table.size());
        EXPECT_EQ(16, CountedValue::liveCount);
        for (int i = 0; i < 32; i += 2)
            EXPECT_TRUE(table.add<CountedValueTranslator>(i, noFunctor).isNewEntry);
        for (int i = 0; i < 32; ++i)
            EXPECT_FALSE(table.add<CountedValueTranslator>(i, noFunctor).isNewEntry);
        EXPECT_EQ(32u, table.size());
        EXPECT_EQ(32, CountedValue::liveCount);
    }
    EXPECT_EQ(0, CountedValue::liveCount);
}

TEST(WTF_OrderedHashSet, HashTranslatorAddIntoRawStorage)
{
    StringOrderedHashTable table;
    auto noFunctor = []() ALWAYS_INLINE_LAMBDA -> String { return String(); };

    auto a = table.add<ConstructingStringViewTranslator>(StringView { "alpha"_s }, noFunctor);
    EXPECT_TRUE(a.isNewEntry);
    auto b = table.add<ConstructingStringViewTranslator>(StringView { "beta"_s }, noFunctor);
    EXPECT_TRUE(b.isNewEntry);
    auto aAgain = table.add<ConstructingStringViewTranslator>(StringView { "alpha"_s }, noFunctor);
    EXPECT_FALSE(aAgain.isNewEntry);
    EXPECT_TRUE(aAgain.iterator == a.iterator);

    EXPECT_EQ(2u, table.size());
    auto it = table.begin();
    EXPECT_EQ("alpha"_s, *it);
    ++it;
    EXPECT_EQ("beta"_s, *it);
    ++it;
    EXPECT_TRUE(it == table.end());
}

TEST(WTF_OrderedHashSet, HashTranslatorAddTriggersRehashAndCompact)
{
    StringOrderedHashTable table;
    auto noFunctor = []() ALWAYS_INLINE_LAMBDA -> String { return String(); };

    Vector<String> keys;
    for (unsigned i = 0; i < 64; ++i)
        keys.append(String::number(i));

    for (const auto& k : keys)
        table.add<ConstructingStringViewTranslator>(StringView { k }, noFunctor);
    EXPECT_EQ(64u, table.size());

    // Removing half and adding them back fills the entries array, which compacts it in place.
    for (unsigned i = 0; i < 64; i += 2)
        table.remove(keys[i]);
    EXPECT_EQ(32u, table.size());

    for (unsigned i = 0; i < 64; i += 2) {
        auto result = table.add<ConstructingStringViewTranslator>(StringView { keys[i] }, noFunctor);
        EXPECT_TRUE(result.isNewEntry);
    }
    EXPECT_EQ(64u, table.size());

    unsigned count = 0;
    for (auto it = table.begin(); it != table.end(); ++it) {
        EXPECT_TRUE(table.contains(*it));
        ++count;
    }
    EXPECT_EQ(64u, count);
    for (const auto& k : keys)
        EXPECT_TRUE(table.contains<ConstructingStringViewTranslator>(StringView { k }));
}

TEST(WTF_OrderedHashSet, HashTraitsEmptyAndDeletedValuesAreOrdinaryKeys)
{
    // HashTraits<int> reserves 0 as empty and -1 as deleted for WTF::HashTable. This table must
    // treat them like any other value.
    OrderedHashSet<int> set;
    EXPECT_FALSE(set.contains(0));
    EXPECT_FALSE(set.contains(-1));
    EXPECT_TRUE(set.add(0).isNewEntry);
    EXPECT_TRUE(set.add(-1).isNewEntry);
    EXPECT_TRUE(set.add(1).isNewEntry);
    EXPECT_FALSE(set.add(0).isNewEntry);
    EXPECT_FALSE(set.add(-1).isNewEntry);
    EXPECT_EQ(3u, set.size());
    EXPECT_EQ(Vector<int>({ 0, -1, 1 }), contentsOf(set));

    EXPECT_TRUE(set.remove(-1));
    EXPECT_FALSE(set.remove(-1));
    EXPECT_FALSE(set.contains(-1));
    EXPECT_TRUE(set.contains(0));
    EXPECT_EQ(Vector<int>({ 0, 1 }), contentsOf(set));

    EXPECT_EQ(0, set.take(0));
    EXPECT_FALSE(set.contains(0));
    EXPECT_TRUE(set.add(-1).isNewEntry);
    EXPECT_TRUE(set.add(0).isNewEntry);
    EXPECT_EQ(Vector<int>({ 1, -1, 0 }), contentsOf(set));

    for (int i = -100; i <= 100; ++i)
        set.add(i);
    EXPECT_EQ(201u, set.size());
    for (int i = -100; i <= 100; ++i)
        EXPECT_TRUE(set.contains(i));

    OrderedHashSet<int*> pointers;
    int* deletedPointer = reinterpret_cast<int*>(-1);
    EXPECT_TRUE(pointers.add(nullptr).isNewEntry);
    EXPECT_TRUE(pointers.add(deletedPointer).isNewEntry);
    EXPECT_TRUE(pointers.contains(nullptr));
    EXPECT_TRUE(pointers.contains(deletedPointer));
    EXPECT_TRUE(pointers.remove(nullptr));
    EXPECT_FALSE(pointers.contains(nullptr));
    EXPECT_TRUE(pointers.contains(deletedPointer));
}

TEST(WTF_OrderedHashSet, IsValidValue)
{
    EXPECT_TRUE(OrderedHashSet<int>::isValidValue(0));
    EXPECT_TRUE(OrderedHashSet<int>::isValidValue(-1));
    EXPECT_TRUE(OrderedHashSet<int*>::isValidValue(nullptr));
    EXPECT_TRUE(OrderedHashSet<unsigned>::isValidValue(std::numeric_limits<unsigned>::max()));

    // StringHash cannot hash a null String or the deleted-value sentinel.
    EXPECT_TRUE(OrderedHashSet<String>::isValidValue("key"_s));
    EXPECT_TRUE(OrderedHashSet<String>::isValidValue(emptyString()));
    EXPECT_FALSE(OrderedHashSet<String>::isValidValue(String()));
    // Destroying a String holding the deleted-value sentinel would deref an invalid pointer.
    static NeverDestroyed<String> deletedString(WTF::HashTableDeletedValue);
    EXPECT_FALSE(OrderedHashSet<String>::isValidValue(deletedString.get()));
    EXPECT_FALSE((OrderedHashSet<String, ASCIICaseInsensitiveHash>::isValidValue(String())));
}

TEST(WTF_OrderedHashSet, ValueWithoutReservedValues)
{
    CountedValue::liveCount = 0;
    {
        CountedOrderedHashSet set;
        for (int i = 0; i < 100; ++i)
            EXPECT_TRUE(set.add(CountedValue(i)).isNewEntry);
        EXPECT_FALSE(set.add(CountedValue(50)).isNewEntry);
        EXPECT_EQ(100, CountedValue::liveCount);

        for (int i = 0; i < 100; i += 3)
            EXPECT_TRUE(set.remove(CountedValue(i)));
        EXPECT_EQ(66u, set.size());
        EXPECT_EQ(66, CountedValue::liveCount);

        CountedOrderedHashSet copy = set;
        EXPECT_EQ(132, CountedValue::liveCount);
        int expected = 0;
        for (auto& value : copy) {
            if (!(expected % 3))
                ++expected;
            EXPECT_EQ(expected, value.value());
            ++expected;
        }

        EXPECT_EQ(1, set.take(CountedValue(1)).value());
        EXPECT_TRUE(set.removeIf([](const CountedValue& value) {
            return value.value() > 50;
        }));
        EXPECT_EQ(33u, set.size());
        EXPECT_EQ(33 + 66, CountedValue::liveCount);
        EXPECT_EQ(INT_MIN, set.take(CountedValue(1)).value());

        set.clear();
        EXPECT_EQ(66, CountedValue::liveCount);
    }
    EXPECT_EQ(0, CountedValue::liveCount);
}

TEST(WTF_OrderedHashSet, EveryValueDestroyedExactlyOnce)
{
    // CountedValue's destructor flags a second destruction of the same storage; the counts flag a
    // leak. Cycle through every path that moves or destroys entries.
    CountedValue::liveCount = 0;
    {
        CountedOrderedHashSet set;
        for (int round = 0; round < 8; ++round) {
            for (int i = 0; i < 300; ++i)
                set.add(CountedValue(round * 1000 + i));
            set.removeIf([&](const CountedValue& value) {
                return value.value() % 4 != round % 4;
            });
            for (int i = 0; i < 300; i += 2)
                set.remove(CountedValue(round * 1000 + i));
            EXPECT_EQ(static_cast<int>(set.size()), CountedValue::liveCount);
        }

        CountedOrderedHashSet moved = WTF::move(set);
        EXPECT_EQ(static_cast<int>(moved.size()), CountedValue::liveCount);
        CountedOrderedHashSet assigned;
        assigned.add(CountedValue(-5));
        assigned = moved;
        EXPECT_EQ(static_cast<int>(moved.size() * 2), CountedValue::liveCount);
        while (!assigned.isEmpty())
            assigned.takeAny();
        EXPECT_EQ(static_cast<int>(moved.size()), CountedValue::liveCount);
    }
    EXPECT_EQ(0, CountedValue::liveCount);
}

TEST(WTF_OrderedHashSet, ProbeSequenceCrossesDeletedBuckets)
{
    // Five entries in an eight-bucket table leave room for one more before any rehash, so the
    // deleted buckets below stay in place for the lookups and the add that follow.
    OrderedHashSet<int, CollidingIntHash> set;
    for (int i = 0; i < 5; ++i)
        set.add(i);

    // Values added after the removed ones sit further along the same probe sequence.
    set.remove(1);
    set.remove(3);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(i != 1 && i != 3, set.contains(i));
    EXPECT_EQ(Vector<int>({ 0, 2, 4 }), contentsOf(set));

    // Adding a present value must keep probing past a deleted bucket rather than reuse it.
    EXPECT_FALSE(set.add(4).isNewEntry);
    EXPECT_EQ(3u, set.size());

    // A new value reuses a deleted bucket, and the values beyond it stay reachable.
    EXPECT_TRUE(set.add(10).isNewEntry);
    for (int value : { 0, 2, 4, 10 })
        EXPECT_TRUE(set.contains(value));
    EXPECT_FALSE(set.contains(1));
    EXPECT_FALSE(set.contains(3));
    EXPECT_EQ(Vector<int>({ 0, 2, 4, 10 }), contentsOf(set));

    // The entries array is full now, so this add rebuilds the table.
    EXPECT_TRUE(set.add(11).isNewEntry);
    EXPECT_TRUE(set.add(12).isNewEntry);
    EXPECT_EQ(Vector<int>({ 0, 2, 4, 10, 11, 12 }), contentsOf(set));

    for (int value : { 0, 2, 4, 10, 11, 12 })
        EXPECT_TRUE(set.remove(value));
    EXPECT_TRUE(set.isEmpty());
    EXPECT_TRUE(set.begin() == set.end());
    EXPECT_TRUE(set.add(1).isNewEntry);
    EXPECT_EQ(Vector<int>({ 1 }), contentsOf(set));
}

TEST(WTF_OrderedHashSet, IterationSkipsDeletedEntries)
{
    // Covers deleted entries at the front, back, and in runs, including runs that span more than
    // one word of deleted-entry bits.
    OrderedHashSet<int> set;
    set.reserveInitialCapacity(200);
    for (int i = 0; i < 150; ++i)
        set.add(i);
    for (int i = 0; i < 5; ++i)
        set.remove(i);
    for (int i = 20; i < 90; ++i)
        set.remove(i);
    for (int i = 140; i < 150; ++i)
        set.remove(i);

    Vector<int> expected;
    for (int i = 5; i < 20; ++i)
        expected.append(i);
    for (int i = 90; i < 140; ++i)
        expected.append(i);
    EXPECT_EQ(expected, contentsOf(set));

    expected.reverse();
    EXPECT_EQ(expected, reverseContentsOf(set));

    EXPECT_EQ(5, set.first());
    EXPECT_EQ(139, set.last());
}

TEST(WTF_OrderedHashSet, ReverseIterationAfterAllRemoved)
{
    OrderedHashSet<int> set;
    for (int i = 0; i < 4; ++i)
        set.add(i);
    for (int i = 0; i < 4; ++i)
        set.remove(i);
    EXPECT_TRUE(set.rbegin() == set.rend());
    EXPECT_TRUE(set.begin() == set.end());
}

TEST(WTF_OrderedHashSet, TakeMovesOutValue)
{
    // take() moves the value out of the table; the table must still find the entry's bucket, which
    // it does by hashing the key before the move.
    OrderedHashSet<String> set;
    for (unsigned i = 0; i < 100; ++i)
        set.add(makeString("key"_s, i));

    for (unsigned i = 0; i < 100; i += 2) {
        String taken = set.take(makeString("key"_s, i));
        EXPECT_EQ(makeString("key"_s, i), taken);
    }
    EXPECT_EQ(50u, set.size());
    for (unsigned i = 0; i < 100; ++i)
        EXPECT_EQ(!!(i % 2), set.contains(makeString("key"_s, i)));

    while (set.size() > 1) {
        String first = set.first();
        EXPECT_EQ(first, set.takeAny());
        EXPECT_FALSE(set.contains(first));
    }
    EXPECT_EQ("key99"_s, set.first());

    // Taking a value that is not present returns the empty value.
    EXPECT_TRUE(set.take("missing"_s).isNull());

    // A moved-from RefPtr is null, which the string hash cannot be applied to.
    OrderedHashSet<RefPtr<StringImpl>> impls;
    StringImpl* raw = nullptr;
    for (int i = 0; i < 20; ++i) {
        RefPtr<StringImpl> impl = String::number(i).releaseImpl();
        if (i == 7)
            raw = impl.get();
        impls.add(WTF::move(impl));
    }
    RefPtr<StringImpl> taken = impls.take(raw);
    EXPECT_EQ(raw, taken.get());
    EXPECT_FALSE(impls.contains(raw));
    EXPECT_EQ(19u, impls.size());
    for (int i = 0; i < 20; ++i)
        EXPECT_EQ(i != 7, impls.contains(String::number(i).impl()));
}

TEST(WTF_OrderedHashSet, RemoveIfThenReinsert)
{
    // removeIf() leaves deleted buckets behind when it does not shrink; adds and lookups that
    // follow have to cope with them.
    OrderedHashSet<int, CollidingIntHash> set;
    for (int i = 0; i < 5; ++i)
        set.add(i);
    EXPECT_TRUE(set.removeIf([](int value) {
        return value == 1 || value == 4;
    }));
    EXPECT_FALSE(set.removeIf([](int) {
        return false;
    }));
    EXPECT_EQ(Vector<int>({ 0, 2, 3 }), contentsOf(set));
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(i != 1 && i != 4, set.contains(i));
    EXPECT_FALSE(set.add(3).isNewEntry);
    EXPECT_TRUE(set.add(4).isNewEntry);
    EXPECT_TRUE(set.add(1).isNewEntry);
    EXPECT_EQ(Vector<int>({ 0, 2, 3, 4, 1 }), contentsOf(set));
    for (int i = 0; i < 5; ++i)
        EXPECT_TRUE(set.contains(i));
}

TEST(WTF_OrderedHashSet, LargeTableAcrossLoadFactorChange)
{
    // Tables above 1024 buckets switch to a lower load factor.
    OrderedHashSet<unsigned> set;
    for (unsigned i = 0; i < 5000; ++i)
        EXPECT_TRUE(set.add(i * 7919).isNewEntry);
    EXPECT_EQ(5000u, set.size());
    for (unsigned i = 0; i < 5000; ++i)
        EXPECT_TRUE(set.contains(i * 7919));
    for (unsigned i = 0; i < 5000; ++i) {
        if (i % 10)
            EXPECT_TRUE(set.remove(i * 7919));
    }
    EXPECT_EQ(500u, set.size());
    unsigned expected = 0;
    for (unsigned value : set) {
        EXPECT_EQ(expected * 7919, value);
        expected += 10;
    }
    for (unsigned i = 0; i < 5000; ++i)
        EXPECT_EQ(!(i % 10), set.contains(i * 7919));
}

TEST(WTF_OrderedHashSet, RandomizedAgainstModel)
{
    // A shuffled mix of adds, removes, takes and removeIfs over a small key space, checked against
    // a plain vector after each step. Small keys keep the table cycling through compaction, growth
    // and shrinking while deleted buckets build up.
    constexpr int keyCount = 97;
    OrderedHashSet<int> set;
    Vector<int> model;
    uint32_t randomState = 20261112;
    auto nextRandom = [&] {
        randomState = randomState * 1664525 + 1013904223;
        return randomState >> 8;
    };

    for (unsigned step = 0; step < 6000; ++step) {
        int key = static_cast<int>(nextRandom() % keyCount) - 1;
        unsigned action = nextRandom() % 10;
        bool present = model.contains(key);
        if (action < 5) {
            EXPECT_EQ(!present, set.add(key).isNewEntry);
            if (!present)
                model.append(key);
        } else if (action < 8) {
            EXPECT_EQ(present, set.remove(key));
            model.removeFirst(key);
        } else if (action == 8) {
            if (present) {
                EXPECT_EQ(key, set.take(key));
                model.removeFirst(key);
            }
        } else if (!(nextRandom() % 8)) {
            int modulus = 2 + nextRandom() % 5;
            auto isMultiple = [&](int value) {
                return !(value % modulus);
            };
            bool expectChange = model.containsIf(isMultiple);
            EXPECT_EQ(expectChange, set.removeIf(isMultiple));
            model.removeAllMatching(isMultiple);
        }

        EXPECT_EQ(model.size(), set.size());
        if (!(step % 16)) {
            EXPECT_EQ(model, contentsOf(set));
            for (int k = -1; k < keyCount - 1; ++k)
                EXPECT_EQ(model.contains(k), set.contains(k));
        }
    }
    EXPECT_EQ(model, contentsOf(set));
}

TEST(WTF_OrderedHashSet, EqualIgnoringOrder)
{
    // OrderedHashSet intentionally does not define operator==, so that the
    // order-sensitive vs order-insensitive choice is explicit at the call
    // site. equalIgnoringOrder compares contents, matching HashSet::operator==
    // semantics.
    OrderedHashSet<String> a;
    a.add("alpha"_s);
    a.add("beta"_s);
    a.add("gamma"_s);

    OrderedHashSet<String> b;
    b.add("gamma"_s);
    b.add("alpha"_s);
    b.add("beta"_s);

    EXPECT_TRUE(equalIgnoringOrder(a, b));
    EXPECT_TRUE(equalIgnoringOrder(b, a));

    auto itA = a.begin();
    auto itB = b.begin();
    EXPECT_NE(*itA, *itB);
}

TEST(WTF_OrderedHashSet, EqualIgnoringOrderDifferentSize)
{
    OrderedHashSet<String> a;
    a.add("alpha"_s);
    a.add("beta"_s);

    OrderedHashSet<String> b;
    b.add("alpha"_s);

    EXPECT_FALSE(equalIgnoringOrder(a, b));
    EXPECT_FALSE(equalIgnoringOrder(b, a));
}

TEST(WTF_OrderedHashSet, EqualIgnoringOrderDifferentElements)
{
    OrderedHashSet<String> a;
    a.add("alpha"_s);
    a.add("beta"_s);

    OrderedHashSet<String> b;
    b.add("alpha"_s);
    b.add("delta"_s);

    EXPECT_FALSE(equalIgnoringOrder(a, b));
    EXPECT_FALSE(equalIgnoringOrder(b, a));
}

TEST(WTF_OrderedHashSet, EqualIgnoringOrderEmpty)
{
    OrderedHashSet<String> a;
    OrderedHashSet<String> b;
    EXPECT_TRUE(equalIgnoringOrder(a, b));

    a.add("key"_s);
    EXPECT_FALSE(equalIgnoringOrder(a, b));
}

TEST(WTF_OrderedHashSet, EmptyTableOperations)
{
    OrderedHashSet<int> set;
    EXPECT_FALSE(set.contains(0));
    EXPECT_TRUE(set.find(0) == set.end());
    EXPECT_FALSE(set.remove(0));
    EXPECT_FALSE(set.removeIf([](int) { return true; }));
    EXPECT_EQ(0, set.take(0));
    EXPECT_TRUE(set.begin() == set.end());
    EXPECT_TRUE(set.rbegin() == set.rend());
    EXPECT_EQ(0u, set.capacity());

    OrderedHashSet<int> copy = set;
    EXPECT_TRUE(copy.isEmpty());
    OrderedHashSet<int> moved = WTF::move(copy);
    EXPECT_TRUE(moved.isEmpty());

    EXPECT_TRUE(set.add(3).isNewEntry);
    EXPECT_TRUE(set.contains(3));
    set.clear();
    EXPECT_FALSE(set.contains(3));
    EXPECT_TRUE(set.add(4).isNewEntry);

    // A moved-from set is empty and usable.
    OrderedHashSet<int> other = WTF::move(set);
    EXPECT_FALSE(set.contains(4));
    EXPECT_TRUE(set.add(5).isNewEntry);
    EXPECT_EQ(Vector<int>({ 5 }), contentsOf(set));
    EXPECT_EQ(Vector<int>({ 4 }), contentsOf(other));
}

TEST(WTF_OrderedHashSet, RemoveByIteratorFromFindAddAndIncrement)
{
    auto run = [](auto& set) {
        for (int i = 0; i < 20; ++i)
            set.add(i);

        auto it = set.find(5);
        EXPECT_TRUE(set.remove(8));
        EXPECT_TRUE(set.remove(it));

        // The iterator was made for 10 and then moved to 11.
        it = set.find(10);
        ++it;
        EXPECT_TRUE(set.remove(it));

        auto result = set.add(100);
        EXPECT_TRUE(result.isNewEntry);
        EXPECT_TRUE(set.remove(result.iterator));

        result = set.add(3);
        EXPECT_FALSE(result.isNewEntry);
        EXPECT_TRUE(set.remove(result.iterator));

        EXPECT_EQ(15, set.take(15));

        Vector<int> expected;
        for (int i = 0; i < 20; ++i) {
            if (i != 3 && i != 5 && i != 8 && i != 11 && i != 15)
                expected.append(i);
        }
        EXPECT_EQ(expected, contentsOf(set));
        for (int i = 0; i < 20; ++i)
            EXPECT_EQ(expected.contains(i), set.contains(i));
        EXPECT_FALSE(set.contains(100));
    };
    OrderedHashSet<int> set;
    run(set);
    OrderedHashSet<int, CollidingIntHash> collidingSet;
    run(collidingSet);
}

TEST(WTF_OrderedHashSet, DecrementAndReverseIterationAcrossBitWords)
{
    OrderedHashSet<int> set;
    for (int i = 0; i < 300; ++i)
        set.add(i);
    Vector<int> expected;
    for (int i = 0; i < 300; ++i) {
        bool removed = (i >= 60 && i < 130) || i == 191 || i == 192 || i == 255 || i == 256 || !i || i == 299;
        if (removed)
            EXPECT_TRUE(set.remove(i));
        else
            expected.append(i);
    }
    EXPECT_EQ(expected, contentsOf(set));

    Vector<int> reversed;
    for (auto it = set.end(); it != set.begin();) {
        --it;
        reversed.append(*it);
    }
    auto expectedReversed = expected;
    expectedReversed.reverse();
    EXPECT_EQ(expectedReversed, reversed);
    EXPECT_EQ(expectedReversed, reverseContentsOf(set));
    EXPECT_EQ(1, set.first());
    EXPECT_EQ(298, set.last());
}

TEST(WTF_OrderedHashSet, CopyWithAndWithoutDeletedEntries)
{
    OrderedHashSet<int> ints;
    OrderedHashSet<String> strings;
    for (int i = 0; i < 40; ++i) {
        ints.add(i);
        strings.add(String::number(i));
    }

    auto checkCopies = [&](const Vector<int>& expected) {
        OrderedHashSet<int> intCopy = ints;
        OrderedHashSet<String> stringCopy = strings;
        EXPECT_EQ(expected, contentsOf(intCopy));
        EXPECT_EQ(expected.size(), stringCopy.size());
        unsigned index = 0;
        for (auto& string : stringCopy)
            EXPECT_EQ(String::number(expected[index++]), string);
        for (int i = -1; i <= 41; ++i) {
            EXPECT_EQ(expected.contains(i), intCopy.contains(i));
            EXPECT_EQ(expected.contains(i), stringCopy.contains(String::number(i)));
        }

        intCopy.add(1000);
        stringCopy.add("1000"_s);
        EXPECT_TRUE(intCopy.remove(expected.first()));
        EXPECT_TRUE(stringCopy.remove(String::number(expected.first())));
        EXPECT_FALSE(ints.contains(1000));
        EXPECT_FALSE(strings.contains("1000"_s));
        EXPECT_TRUE(ints.contains(expected.first()));
        EXPECT_TRUE(strings.contains(String::number(expected.first())));
    };

    Vector<int> expected;
    for (int i = 0; i < 40; ++i)
        expected.append(i);
    checkCopies(expected);

    for (int i = 0; i < 40; i += 3) {
        ints.remove(i);
        strings.remove(String::number(i));
        expected.removeFirst(i);
    }
    checkCopies(expected);

    OrderedHashSet<int> reserved;
    reserved.reserveInitialCapacity(1000);
    for (int i = 0; i < 10; ++i)
        reserved.add(i);
    OrderedHashSet<int> reservedCopy = reserved;
    EXPECT_EQ(contentsOf(reserved), contentsOf(reservedCopy));
    for (int i = 0; i < 10; ++i)
        EXPECT_TRUE(reservedCopy.contains(i));
}

TEST(WTF_OrderedHashSet, RemoveIfRemovingFewOrMost)
{
    // removeIf() either deletes each removed entry's bucket or rebuilds the bucket array, depending
    // on how many entries go, and falls back to one at a time when deleted entries already exist.
    for (bool removeMost : { false, true }) {
        for (bool hadDeletedEntries : { false, true }) {
            CountedValue::liveCount = 0;
            {
                CountedOrderedHashSet set;
                for (int i = 0; i < 500; ++i)
                    set.add(CountedValue(i));
                if (hadDeletedEntries)
                    set.remove(CountedValue(499));
                auto isRemoved = [&](int value) {
                    return removeMost ? !!(value % 10) : !(value % 10);
                };
                EXPECT_TRUE(set.removeIf([&](const CountedValue& value) {
                    return isRemoved(value.value());
                }));
                EXPECT_EQ(static_cast<int>(set.size()), CountedValue::liveCount);

                Vector<int> expected;
                for (int i = 0; i < 499 + !hadDeletedEntries; ++i) {
                    if (!isRemoved(i))
                        expected.append(i);
                }
                Vector<int> actual;
                for (auto& value : set)
                    actual.append(value.value());
                EXPECT_EQ(expected, actual);
                for (int i = 0; i < 500; ++i)
                    EXPECT_EQ(expected.contains(i), set.contains<CountedValueTranslator>(i));

                for (int i = 1000; i < 1100; ++i)
                    EXPECT_TRUE(set.add(CountedValue(i)).isNewEntry);
                EXPECT_EQ(static_cast<int>(set.size()), CountedValue::liveCount);
            }
            EXPECT_EQ(0, CountedValue::liveCount);
        }
    }

    OrderedHashSet<int, CollidingIntHash> colliding;
    for (int i = 0; i < 6; ++i)
        colliding.add(i);
    EXPECT_TRUE(colliding.removeIf([](int value) { return value != 4; }));
    EXPECT_EQ(Vector<int>({ 4 }), contentsOf(colliding));
    EXPECT_TRUE(colliding.contains(4));
    EXPECT_TRUE(colliding.add(1).isNewEntry);
    EXPECT_EQ(Vector<int>({ 4, 1 }), contentsOf(colliding));
}

namespace {

struct ObservingValue;
struct ObservingValueHash;
using ObservingSet = OrderedHashSet<ObservingValue, ObservingValueHash>;

struct ObservingValue {
    static ObservingSet* observedSet;
    static unsigned observedDestructionCount;

    explicit ObservingValue(int value)
        : value(value)
    {
    }
    ObservingValue(ObservingValue&& other)
        : value(std::exchange(other.value, -1))
    {
    }
    ObservingValue(const ObservingValue&) = default;
    ~ObservingValue();

    int value;
    bool alive { true };
};

ObservingSet* ObservingValue::observedSet = nullptr;
unsigned ObservingValue::observedDestructionCount = 0;

struct ObservingValueHash {
    static unsigned hash(const ObservingValue& value) { return IntHash<int>::hash(value.value); }
    static bool equal(const ObservingValue& a, const ObservingValue& b) { return a.value == b.value; }
};

struct ObservingValueTranslator {
    static unsigned hash(int value) { return IntHash<int>::hash(value); }
    static bool equal(const ObservingValue& a, int b) { return a.value == b; }
};

ObservingValue::~ObservingValue()
{
    // Destruction of an entry may run arbitrary code; whatever it finds in the table must still be
    // alive, and the entry being destroyed must no longer be found.
    if (value >= 0 && observedSet) {
        ++observedDestructionCount;
        for (auto& other : *observedSet)
            EXPECT_TRUE(other.alive);
        EXPECT_FALSE(observedSet->contains<ObservingValueTranslator>(value));
    }
    alive = false;
}

} // anonymous namespace

TEST(WTF_OrderedHashSet, DestructorSeesConsistentTable)
{
    for (unsigned variant = 0; variant < 3; ++variant) {
        ObservingSet set;
        for (int i = 0; i < 200; ++i)
            set.add(ObservingValue(i));
        ObservingValue::observedSet = &set;
        ObservingValue::observedDestructionCount = 0;

        // Variant 0 removes few entries, 1 removes most, and 2 has a deleted entry beforehand.
        if (variant == 2)
            EXPECT_TRUE(set.remove(ObservingValue(199)));
        auto isRemoved = [&](int value) {
            return variant == 1 ? !!(value % 10) : !(value % 10);
        };
        EXPECT_TRUE(set.removeIf([&](const ObservingValue& value) {
            return isRemoved(value.value);
        }));
        int removedByKey = variant == 1 ? 20 : 5;
        EXPECT_TRUE(set.remove(ObservingValue(removedByKey)));
        ObservingValue::observedSet = nullptr;

        unsigned expectedCount = 0;
        for (int i = 0; i < 200; ++i) {
            bool present = !isRemoved(i) && i != removedByKey && !(variant == 2 && i == 199);
            EXPECT_EQ(present, set.contains<ObservingValueTranslator>(i));
            expectedCount += present;
        }
        EXPECT_EQ(expectedCount, set.size());
        EXPECT_GE(ObservingValue::observedDestructionCount, 200 - expectedCount);
    }
}

TEST(WTF_OrderedHashSet, RelocationKeepsReferenceCounts)
{
    Vector<String> strings;
    for (int i = 0; i < 300; ++i)
        strings.append(makeString("value"_s, i));
    {
        OrderedHashSet<String> set;
        for (unsigned round = 0; round < 4; ++round) {
            for (auto& string : strings)
                set.add(string);
            for (unsigned i = round; i < strings.size(); i += 3)
                set.remove(strings[i]);
            set.removeIf([&](const String& string) {
                return string.length() == 6 + round % 2;
            });
        }
        OrderedHashSet<String> copy = set;
        for (auto& string : strings) {
            EXPECT_EQ(set.contains(string), copy.contains(string));
            unsigned expectedRefCount = 1 + 2 * set.contains(string);
            EXPECT_EQ(expectedRefCount, string.impl()->refCount());
        }
    }
    for (auto& string : strings)
        EXPECT_EQ(1u, string.impl()->refCount());
}

} // namespace TestWebKitAPI
