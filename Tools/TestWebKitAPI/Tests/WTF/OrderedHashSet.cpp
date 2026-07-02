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
    EXPECT_STREQ("banana", values[0].utf8().data());
    EXPECT_STREQ("apple", values[1].utf8().data());
    EXPECT_STREQ("cherry", values[2].utf8().data());
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

// Mirrors the signature OrderedHashTable::add<HashTranslator> expects:
//   translate(location, key, valueFunctor).
// Uses plain assignment into `location` — the canonical WTF translator convention.
// This is the shape that will silently corrupt raw storage if the entries array
// isn't pre-initialized to an empty value before translate() runs.
struct AssigningStringViewTranslator {
    static unsigned hash(StringView view) { return StringHash::hash(view.toString()); }
    static bool equal(const String& a, StringView b) { return a == b; }
    static void translate(String& location, StringView view, NOESCAPE const auto&)
    {
        location = view.toString();
    }
};

using StringOrderedHashTable = WTF::OrderedHashTable<String, String, WTF::IdentityExtractor, DefaultHash<String>, HashTraits<String>, HashTraits<String>, WTF::HashTableMalloc>;

// A value type whose operator= asserts the LHS is in a constructed state
// (one of the known sentinels). Without a pre-translate constructEmptyValue,
// the translator's `location = ...` assigns into raw malloc storage and the
// m_state field will be random bits, nearly always tripping the assert.
// liveCount tracks Live-state instances only; Empty/Deleted sentinels are
// not counted, mirroring WTF's convention that sentinels hold no resources.
class TrackedValue {
public:
    enum State : uint32_t { Garbage = 0xDEADBEEF, Empty = 0xE11E, Deleted = 0xDEAD, Live = 0xA11E };

    static int liveCount;

    TrackedValue() : m_state(Empty), m_value(0) { }
    explicit TrackedValue(int v) : m_state(Live), m_value(v) { ++liveCount; }
    TrackedValue(const TrackedValue& other) : m_state(other.m_state), m_value(other.m_value)
    {
        if (m_state == Live)
            ++liveCount;
    }
    TrackedValue(TrackedValue&& other) noexcept : m_state(other.m_state), m_value(other.m_value)
    {
        // Move: dst increments, src keeps its state so its destructor balances.
        if (m_state == Live)
            ++liveCount;
    }
    TrackedValue(WTF::HashTableDeletedValueType) : m_state(Deleted), m_value(0) { }

    ~TrackedValue()
    {
        if (m_state == Live)
            --liveCount;
        m_state = Garbage;
    }

    TrackedValue& operator=(const TrackedValue& other)
    {
        // LHS must be a constructed object (Empty/Live/Deleted). Raw malloc
        // storage will have random bits, which almost never match a sentinel.
        EXPECT_TRUE(m_state == Empty || m_state == Live || m_state == Deleted);
        if (m_state == Live)
            --liveCount;
        m_state = other.m_state;
        m_value = other.m_value;
        if (m_state == Live)
            ++liveCount;
        return *this;
    }

    bool isHashTableDeletedValue() const { return m_state == Deleted; }
    bool operator==(const TrackedValue& other) const { return m_state == other.m_state && m_value == other.m_value; }

    int value() const { return m_value; }

private:
    uint32_t m_state;
    int m_value;
};

int TrackedValue::liveCount = 0;

struct TrackedValueHash {
    static unsigned hash(const TrackedValue& v) { return IntHash<int>::hash(v.value()); }
    static bool equal(const TrackedValue& a, const TrackedValue& b) { return a == b; }
};

struct TrackedValueTraits : WTF::SimpleClassHashTraits<TrackedValue> {
    // Force emptyValueIsZero=false so constructEmptyValue runs (not zeroBytes).
    static constexpr bool emptyValueIsZero = false;
};

struct TrackedValueTranslator {
    static unsigned hash(int v) { return IntHash<int>::hash(v); }
    static bool equal(const TrackedValue& a, int b) { return a.value() == b; }
    static void translate(TrackedValue& location, int v, NOESCAPE const auto&)
    {
        // Standard WTF translator convention: assign. LHS must be constructed.
        location = TrackedValue(v);
    }
};

using TrackedOrderedHashTable = WTF::OrderedHashTable<TrackedValue, TrackedValue, WTF::IdentityExtractor, TrackedValueHash, TrackedValueTraits, TrackedValueTraits, WTF::HashTableMalloc>;

}

TEST(WTF_OrderedHashSet, HashTranslatorAddConstructsEmptyBeforeAssign)
{
    // Without the pre-translate constructEmptyValue, `location = ...` inside the
    // translator would be invoked on raw malloc storage. TrackedValue::operator=
    // asserts its LHS is a real constructed object, so the bug would EXPECT fail.
    TrackedValue::liveCount = 0;
    auto noFunctor = []() ALWAYS_INLINE_LAMBDA -> TrackedValue { return TrackedValue(); };
    {
        TrackedOrderedHashTable table;
        for (int i = 0; i < 32; ++i) {
            auto r = table.add<TrackedValueTranslator>(i, noFunctor);
            EXPECT_TRUE(r.isNewEntry);
        }
        EXPECT_EQ(32u, table.size());

        // Force deletions then re-add; after internal compact/rehash the translator
        // path writes into slots whose prior occupants were destructed/moved-from —
        // each such slot must again be re-constructed-empty before translate.
        for (int i = 0; i < 32; i += 2)
            table.remove(TrackedValue(i));
        EXPECT_EQ(16u, table.size());
        for (int i = 0; i < 32; i += 2) {
            auto r = table.add<TrackedValueTranslator>(i, noFunctor);
            EXPECT_TRUE(r.isNewEntry);
        }
        EXPECT_EQ(32u, table.size());
    }
    // Every constructed instance must have been destructed.
    EXPECT_EQ(0, TrackedValue::liveCount);
}

TEST(WTF_OrderedHashSet, HashTranslatorAddIntoRawStorage)
{
    // Exercises OrderedHashTable::add<HashTranslator>. A correctly-implemented WTF
    // translator assigns into `location`; our entries array is raw malloc storage,
    // so without an in-place empty-value construction before translate, String's
    // assignment operator would deref garbage from the freshly malloc'd slot.
    StringOrderedHashTable table;
    auto noFunctor = []() ALWAYS_INLINE_LAMBDA -> String { return String(); };

    auto a = table.add<AssigningStringViewTranslator>(StringView { "alpha"_s }, noFunctor);
    EXPECT_TRUE(a.isNewEntry);
    auto b = table.add<AssigningStringViewTranslator>(StringView { "beta"_s }, noFunctor);
    EXPECT_TRUE(b.isNewEntry);
    auto aAgain = table.add<AssigningStringViewTranslator>(StringView { "alpha"_s }, noFunctor);
    EXPECT_FALSE(aAgain.isNewEntry);

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
    // Adding through the translator path past initial capacity must rehash (growing
    // the entries array and re-placement-newing into it) without disturbing any
    // in-flight empty-value state for the slot still being populated.
    StringOrderedHashTable table;
    auto noFunctor = []() ALWAYS_INLINE_LAMBDA -> String { return String(); };

    Vector<String> keys;
    for (unsigned i = 0; i < 64; ++i)
        keys.append(String::number(i));

    for (const auto& k : keys)
        table.add<AssigningStringViewTranslator>(StringView { k }, noFunctor);
    EXPECT_EQ(64u, table.size());

    // Delete half, then add again via the translator — forces compactInPlace to
    // run, after which the next translator add lands in a slot that was previously
    // a deleted entry. The fix must still apply in that path.
    for (unsigned i = 0; i < 64; i += 2)
        table.remove(keys[i]);
    EXPECT_EQ(32u, table.size());

    for (unsigned i = 0; i < 64; i += 2) {
        auto result = table.add<AssigningStringViewTranslator>(StringView { keys[i] }, noFunctor);
        EXPECT_TRUE(result.isNewEntry);
    }
    EXPECT_EQ(64u, table.size());

    // Verify all keys are present and iterable.
    unsigned count = 0;
    for (auto it = table.begin(); it != table.end(); ++it) {
        EXPECT_TRUE(table.contains(*it));
        ++count;
    }
    EXPECT_EQ(64u, count);
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

} // namespace TestWebKitAPI
