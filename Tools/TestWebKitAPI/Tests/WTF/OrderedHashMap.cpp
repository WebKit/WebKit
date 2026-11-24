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
#include <wtf/OrderedHashMap.h>

#include "Helpers/Test.h"
#include "MoveOnly.h"
#include <string>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_OrderedHashMap, EmptyMap)
{
    OrderedHashMap<int, int> map;
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(0u, map.size());
    EXPECT_TRUE(map.begin() == map.end());
}

TEST(WTF_OrderedHashMap, BasicAddAndFind)
{
    OrderedHashMap<int, int> map;
    auto result = map.add(1, 100);
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_EQ(1, result.iterator->key);
    EXPECT_EQ(100, result.iterator->value);
    EXPECT_EQ(1u, map.size());

    auto result2 = map.add(1, 200);
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(100, result2.iterator->value); // Original value preserved
    EXPECT_EQ(1u, map.size());
}

TEST(WTF_OrderedHashMap, Set)
{
    OrderedHashMap<int, int> map;
    map.set(1, 100);
    EXPECT_EQ(100, map.get(1));

    map.set(1, 200);
    EXPECT_EQ(200, map.get(1)); // Value updated
    EXPECT_EQ(1u, map.size()); // Size unchanged
}

TEST(WTF_OrderedHashMap, Ensure)
{
    OrderedHashMap<int, int> map;
    auto result1 = map.ensure(1, [] {
        return 100;
    });
    EXPECT_TRUE(result1.isNewEntry);
    EXPECT_EQ(100, result1.iterator->value);

    auto result2 = map.ensure(1, [] {
        return 200;
    });
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(100, result2.iterator->value); // Functor not called
}

TEST(WTF_OrderedHashMap, Contains)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    EXPECT_TRUE(map.contains(1));
    EXPECT_TRUE(map.contains(2));
    EXPECT_FALSE(map.contains(3));
}

TEST(WTF_OrderedHashMap, Get)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    EXPECT_EQ(100, map.get(1));
    EXPECT_EQ(0, map.get(999)); // Default for missing key
}

TEST(WTF_OrderedHashMap, GetOptional)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    auto result = map.getOptional(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(100, *result);

    auto missing = map.getOptional(999);
    EXPECT_FALSE(missing.has_value());
}

TEST(WTF_OrderedHashMap, InsertionOrderPreserved)
{
    OrderedHashMap<int, int> map;
    map.add(3, 300);
    map.add(1, 100);
    map.add(4, 400);
    map.add(1, 999); // Duplicate, should not change order
    map.add(5, 500);
    map.add(2, 200);

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(5u, keys.size());
    EXPECT_EQ(3, keys[0]);
    EXPECT_EQ(1, keys[1]);
    EXPECT_EQ(4, keys[2]);
    EXPECT_EQ(5, keys[3]);
    EXPECT_EQ(2, keys[4]);
}

TEST(WTF_OrderedHashMap, InsertionOrderPreservedAfterDeletion)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);
    map.add(4, 400);
    map.add(5, 500);

    map.remove(3);

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(4u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]);
    EXPECT_EQ(4, keys[2]);
    EXPECT_EQ(5, keys[3]);
}

TEST(WTF_OrderedHashMap, SetDoesNotChangeOrder)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);

    map.set(2, 999);

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(3u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]); // Same position
    EXPECT_EQ(3, keys[2]);
    EXPECT_EQ(999, map.get(2)); // Value updated
}

TEST(WTF_OrderedHashMap, RemoveByKey)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    EXPECT_TRUE(map.remove(1));
    EXPECT_FALSE(map.contains(1));
    EXPECT_EQ(1u, map.size());
    EXPECT_FALSE(map.remove(999)); // Non-existent key
}

TEST(WTF_OrderedHashMap, RemoveByIterator)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    auto it = map.find(1);
    EXPECT_TRUE(map.remove(it));
    EXPECT_FALSE(map.contains(1));
    EXPECT_EQ(1u, map.size());
}

TEST(WTF_OrderedHashMap, Take)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    auto value = map.take(1);
    EXPECT_EQ(100, value);
    EXPECT_FALSE(map.contains(1));
    EXPECT_EQ(1u, map.size());

    auto missing = map.take(999);
    EXPECT_EQ(0, missing); // Default for missing
}

TEST(WTF_OrderedHashMap, TakeOptional)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);

    auto result = map.takeOptional(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(100, *result);
    EXPECT_FALSE(map.contains(1));

    auto missing = map.takeOptional(999);
    EXPECT_FALSE(missing.has_value());
}

TEST(WTF_OrderedHashMap, TakeFirst)
{
    OrderedHashMap<int, int> map;
    map.add(3, 300);
    map.add(1, 100);
    map.add(2, 200);

    auto value = map.takeFirst();
    EXPECT_EQ(300, value); // First in insertion order
    EXPECT_EQ(2u, map.size());
}

TEST(WTF_OrderedHashMap, Clear)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.clear();

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(0u, map.size());
    EXPECT_TRUE(map.begin() == map.end());
}

TEST(WTF_OrderedHashMap, Swap)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);
    map1.add(2, 200);

    OrderedHashMap<int, int> map2;
    map2.add(3, 300);

    map1.swap(map2);

    EXPECT_EQ(1u, map1.size());
    EXPECT_TRUE(map1.contains(3));
    EXPECT_EQ(2u, map2.size());
    EXPECT_TRUE(map2.contains(1));
    EXPECT_TRUE(map2.contains(2));
}

TEST(WTF_OrderedHashMap, CopyConstruction)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);
    map1.add(2, 200);
    map1.add(3, 300);

    OrderedHashMap<int, int> map2(map1);

    EXPECT_EQ(3u, map2.size());

    // Check insertion order is preserved in copy
    Vector<int> keys;
    for (auto& pair : map2)
        keys.append(pair.key);

    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]);
    EXPECT_EQ(3, keys[2]);
}

TEST(WTF_OrderedHashMap, MoveConstruction)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);
    map1.add(2, 200);

    OrderedHashMap<int, int> map2(WTF::move(map1));

    EXPECT_EQ(2u, map2.size());
    EXPECT_TRUE(map2.contains(1));
    EXPECT_TRUE(map2.contains(2));
    EXPECT_TRUE(map1.isEmpty());
}

TEST(WTF_OrderedHashMap, CopyAssignment)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);
    map1.add(2, 200);

    OrderedHashMap<int, int> map2;
    map2.add(9, 900);
    map2 = map1;

    EXPECT_EQ(2u, map2.size());
    EXPECT_TRUE(map2.contains(1));
    EXPECT_TRUE(map2.contains(2));
    EXPECT_FALSE(map2.contains(9));
}

TEST(WTF_OrderedHashMap, MoveAssignment)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);

    OrderedHashMap<int, int> map2;
    map2.add(9, 900);
    map2 = WTF::move(map1);

    EXPECT_EQ(1u, map2.size());
    EXPECT_TRUE(map2.contains(1));
    EXPECT_TRUE(map1.isEmpty());
}

TEST(WTF_OrderedHashMap, KeysIteration)
{
    OrderedHashMap<int, int> map;
    map.add(3, 300);
    map.add(1, 100);
    map.add(2, 200);

    Vector<int> keys;
    for (auto& key : map.keys())
        keys.append(key);

    EXPECT_EQ(3u, keys.size());
    EXPECT_EQ(3, keys[0]);
    EXPECT_EQ(1, keys[1]);
    EXPECT_EQ(2, keys[2]);
}

TEST(WTF_OrderedHashMap, ValuesIteration)
{
    OrderedHashMap<int, int> map;
    map.add(3, 300);
    map.add(1, 100);
    map.add(2, 200);

    Vector<int> values;
    for (auto& value : map.values())
        values.append(value);

    EXPECT_EQ(3u, values.size());
    EXPECT_EQ(300, values[0]);
    EXPECT_EQ(100, values[1]);
    EXPECT_EQ(200, values[2]);
}

TEST(WTF_OrderedHashMap, RehashPreservesOrder)
{
    OrderedHashMap<int, int> map;
    // Insert enough entries to trigger rehashing
    Vector<int> expectedOrder;
    for (int i = 0; i < 100; ++i) {
        map.add(i, i * 10);
        expectedOrder.append(i);
    }

    Vector<int> actualOrder;
    for (auto& pair : map)
        actualOrder.append(pair.key);

    EXPECT_EQ(expectedOrder, actualOrder);
}

TEST(WTF_OrderedHashMap, DeleteAndReinsert)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);

    map.remove(2);
    map.add(2, 250); // Re-add goes at end

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(3u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(3, keys[1]);
    EXPECT_EQ(2, keys[2]); // Re-inserted at end
    EXPECT_EQ(250, map.get(2));
}

TEST(WTF_OrderedHashMap, ManyDeletesAndInserts)
{
    OrderedHashMap<int, int> map;
    for (int i = 0; i < 50; ++i)
        map.add(i, i);

    // Delete even numbers
    for (int i = 0; i < 50; i += 2)
        map.remove(i);

    EXPECT_EQ(25u, map.size());

    // Check remaining are in order
    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    for (int i = 0; i < 25; ++i)
        EXPECT_EQ(i * 2 + 1, keys[i]);
}

TEST(WTF_OrderedHashMap, StringKeys)
{
    OrderedHashMap<String, int> map;
    map.add("banana"_s, 2);
    map.add("apple"_s, 1);
    map.add("cherry"_s, 3);

    Vector<String> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(3u, keys.size());
    EXPECT_EQ("banana"_s, keys[0]);
    EXPECT_EQ("apple"_s, keys[1]);
    EXPECT_EQ("cherry"_s, keys[2]);
}

TEST(WTF_OrderedHashMap, InitializerList)
{
    OrderedHashMap<int, int> map {
        { 3, 300 },
        { 1, 100 },
        { 2, 200 }
    };

    EXPECT_EQ(3u, map.size());

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(3, keys[0]);
    EXPECT_EQ(1, keys[1]);
    EXPECT_EQ(2, keys[2]);
}

TEST(WTF_OrderedHashMap, ReserveCapacity)
{
    OrderedHashMap<int, int> map;
    map.reserveInitialCapacity(100);

    for (int i = 0; i < 100; ++i)
        map.add(i, i);

    EXPECT_EQ(100u, map.size());

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(i, keys[i]);
}

TEST(WTF_OrderedHashMap, ConstIteration)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    const auto& constMap = map;
    Vector<int> keys;
    for (auto& pair : constMap)
        keys.append(pair.key);

    EXPECT_EQ(2u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]);
}

TEST(WTF_OrderedHashMap, FindReturnsCorrectIterator)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);

    auto it = map.find(2);
    EXPECT_TRUE(it != map.end());
    EXPECT_EQ(2, it->key);
    EXPECT_EQ(200, it->value);

    auto missing = map.find(999);
    EXPECT_TRUE(missing == map.end());
}

TEST(WTF_OrderedHashMap, IteratorComparison)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);

    EXPECT_TRUE(map.begin() != map.end());
    EXPECT_FALSE(map.begin() == map.end());

    OrderedHashMap<int, int>::const_iterator constBegin = map.begin();
    EXPECT_TRUE(constBegin == map.begin());
    EXPECT_TRUE(constBegin != map.end());
}

TEST(WTF_OrderedHashMap, CompactInPlacePreservesOrderAndCapacity)
{
    // Fills the initial entries array (capacity=6, bucketCount=8), deletes most,
    // then adds to trigger rehashForAdd on a table that cannot shrink below
    // initialBucketCount, forcing the compactInPlace branch.
    OrderedHashMap<int, int> map;
    for (int i = 0; i < 6; ++i)
        map.add(i, i * 10);
    unsigned initialCapacity = map.capacity();

    map.remove(1);
    map.remove(2);
    map.remove(3);
    map.remove(4);
    EXPECT_EQ(2u, map.size());
    EXPECT_EQ(initialCapacity, map.capacity());

    map.add(100, 1000);
    EXPECT_EQ(3u, map.size());
    EXPECT_EQ(initialCapacity, map.capacity()); // Reused the same allocation.

    Vector<int> keys;
    Vector<int> values;
    for (auto& pair : map) {
        keys.append(pair.key);
        values.append(pair.value);
    }
    EXPECT_EQ(3u, keys.size());
    EXPECT_EQ(0, keys[0]);
    EXPECT_EQ(5, keys[1]);
    EXPECT_EQ(100, keys[2]);
    EXPECT_EQ(0, values[0]);
    EXPECT_EQ(50, values[1]);
    EXPECT_EQ(1000, values[2]);

    EXPECT_EQ(0, map.get(0));
    EXPECT_EQ(50, map.get(5));
    EXPECT_EQ(1000, map.get(100));
    EXPECT_FALSE(map.contains(1));
    EXPECT_FALSE(map.contains(4));
}

TEST(WTF_OrderedHashMap, CompactInPlaceOnLargerTable)
{
    OrderedHashMap<int, int> map;
    map.reserveInitialCapacity(12);
    unsigned reservedCapacity = map.capacity();
    EXPECT_GE(reservedCapacity, 12u);

    for (int i = 0; i < 12; ++i)
        map.add(i, i * 10);
    EXPECT_EQ(12u, map.size());
    EXPECT_EQ(reservedCapacity, map.capacity());

    // Keep liveCount at 3 so add hits compactInPlace (not shrink, not grow).
    for (int i = 0; i < 9; ++i)
        map.remove(i);
    EXPECT_EQ(3u, map.size());
    EXPECT_EQ(reservedCapacity, map.capacity());

    map.add(100, 1000);
    EXPECT_EQ(4u, map.size());
    EXPECT_EQ(reservedCapacity, map.capacity());

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);
    EXPECT_EQ(4u, keys.size());
    EXPECT_EQ(9, keys[0]);
    EXPECT_EQ(10, keys[1]);
    EXPECT_EQ(11, keys[2]);
    EXPECT_EQ(100, keys[3]);

    EXPECT_EQ(90, map.get(9));
    EXPECT_EQ(100, map.get(10));
    EXPECT_EQ(110, map.get(11));
    EXPECT_EQ(1000, map.get(100));
}

TEST(WTF_OrderedHashMap, CompactInPlaceWithStringValues)
{
    // Non-trivial value type exercises move/destroy paths inside compactInPlace.
    OrderedHashMap<int, String> map;
    for (int i = 0; i < 6; ++i)
        map.add(i, makeString("v"_s, i));
    unsigned initialCapacity = map.capacity();

    map.remove(1);
    map.remove(2);
    map.remove(3);
    map.remove(4);

    map.add(100, "new"_s);
    EXPECT_EQ(initialCapacity, map.capacity());
    EXPECT_EQ(3u, map.size());

    Vector<int> keys;
    Vector<String> values;
    for (auto& pair : map) {
        keys.append(pair.key);
        values.append(pair.value);
    }
    EXPECT_EQ(3u, keys.size());
    EXPECT_EQ(0, keys[0]);
    EXPECT_EQ(5, keys[1]);
    EXPECT_EQ(100, keys[2]);
    EXPECT_EQ("v0"_s, values[0]);
    EXPECT_EQ("v5"_s, values[1]);
    EXPECT_EQ("new"_s, values[2]);

    EXPECT_EQ("v0"_s, map.get(0));
    EXPECT_EQ("v5"_s, map.get(5));
    EXPECT_EQ("new"_s, map.get(100));
}

TEST(WTF_OrderedHashMap, RemoveIfAcrossShrinkThreshold)
{
    // Fill past initialBucketCount so shrinkIfNeeded() can actually rehash, then
    // removeIf enough entries to cross the 1/4 shrink threshold. Prior to the fix
    // that deferred shrink, this would rehash mid-iteration and skip entries.
    OrderedHashMap<int, int> map;
    for (int i = 0; i < 64; ++i)
        map.add(i, i * 10);
    EXPECT_EQ(64u, map.size());

    bool changed = map.removeIf([](auto& entry) {
        return entry.key >= 8;
    });
    EXPECT_TRUE(changed);
    EXPECT_EQ(8u, map.size());

    Vector<int> keys;
    Vector<int> values;
    for (auto& entry : map) {
        keys.append(entry.key);
        values.append(entry.value);
    }
    EXPECT_EQ(8u, keys.size());
    for (unsigned i = 0; i < keys.size(); ++i) {
        EXPECT_EQ(static_cast<int>(i), keys[i]);
        EXPECT_EQ(static_cast<int>(i) * 10, values[i]);
    }

    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(map.contains(i));
        EXPECT_EQ(i * 10, map.get(i));
    }
    for (int i = 8; i < 64; ++i)
        EXPECT_FALSE(map.contains(i));
}

TEST(WTF_OrderedHashMap, MoveOnlyValue)
{
    OrderedHashMap<int, MoveOnly> map;
    auto addResult = map.add(1, MoveOnly(100));
    EXPECT_TRUE(addResult.isNewEntry);
    EXPECT_EQ(100u, addResult.iterator->value.value());

    map.set(2, MoveOnly(200));
    map.add(3, MoveOnly(300));
    EXPECT_EQ(3u, map.size());
    EXPECT_EQ(100u, map.find(1)->value.value());
    EXPECT_EQ(200u, map.find(2)->value.value());
    EXPECT_EQ(300u, map.find(3)->value.value());

    auto ensured = map.ensure(4, [] { return MoveOnly(400); });
    EXPECT_TRUE(ensured.isNewEntry);
    EXPECT_EQ(400u, ensured.iterator->value.value());

    auto taken = map.take(2);
    EXPECT_EQ(200u, taken.value());
    EXPECT_EQ(3u, map.size());
    EXPECT_FALSE(map.contains(2));

    // Insertion order should still be 1, 3, 4 after removing 2.
    Vector<int> keysInOrder;
    for (auto& entry : map)
        keysInOrder.append(entry.key);
    EXPECT_EQ(3u, keysInOrder.size());
    EXPECT_EQ(1, keysInOrder[0]);
    EXPECT_EQ(3, keysInOrder[1]);
    EXPECT_EQ(4, keysInOrder[2]);
}

TEST(WTF_OrderedHashMap, MoveOnlyValueRehash)
{
    // Force enough additions to trigger rehash with move-only values, exercising
    // the move construction in the rehash path.
    OrderedHashMap<int, MoveOnly> map;
    for (unsigned i = 0; i < 64; ++i)
        map.add(i, MoveOnly(i * 7));
    EXPECT_EQ(64u, map.size());
    for (unsigned i = 0; i < 64; ++i)
        EXPECT_EQ(i * 7, map.find(i)->value.value());
}

TEST(WTF_OrderedHashMap, HashTranslatorFindContainsGet)
{
    OrderedHashMap<String, unsigned> map;
    map.add("alpha"_s, 1u);
    map.add("beta"_s, 2u);
    map.add("gamma"_s, 3u);

    auto it = map.find<StringViewHashTranslator>(StringView { "beta"_s });
    EXPECT_TRUE(it != map.end());
    EXPECT_EQ(2u, it->value);

    EXPECT_TRUE(map.contains<StringViewHashTranslator>(StringView { "alpha"_s }));
    EXPECT_TRUE(map.contains<StringViewHashTranslator>(StringView { "gamma"_s }));
    EXPECT_FALSE(map.contains<StringViewHashTranslator>(StringView { "delta"_s }));

    EXPECT_EQ(1u, map.get<StringViewHashTranslator>(StringView { "alpha"_s }));
    EXPECT_EQ(3u, map.get<StringViewHashTranslator>(StringView { "gamma"_s }));
    EXPECT_EQ(0u, map.get<StringViewHashTranslator>(StringView { "missing"_s }));
}

namespace {

// Counts constructions and destructions, flags a second destruction of the same storage, and has
// neither a default constructor nor a deleted-value constructor.
class CountedKey {
public:
    static int liveCount;

    explicit CountedKey(int value)
        : m_value(value)
    {
        ++liveCount;
    }
    CountedKey(const CountedKey& other)
        : m_value(other.m_value)
    {
        ++liveCount;
    }
    CountedKey(CountedKey&& other)
        : m_value(other.m_value)
    {
        ++liveCount;
    }
    ~CountedKey()
    {
        EXPECT_NE(m_value, destroyedMarker);
        m_value = destroyedMarker;
        --liveCount;
    }
    CountedKey& operator=(const CountedKey&) = delete;

    int value() const { return m_value; }
    friend bool operator==(const CountedKey& a, const CountedKey& b) { return a.m_value == b.m_value; }

private:
    static constexpr int destroyedMarker = 0x5ead5ead;
    int m_value;
};

int CountedKey::liveCount = 0;

struct CountedKeyHash {
    static unsigned hash(const CountedKey& key) { return IntHash<int>::hash(key.value()); }
    static bool equal(const CountedKey& a, const CountedKey& b) { return a == b; }
};

using CountedKVP = KeyValuePair<CountedKey, int>;
using CountedKVPTraits = WTF::KeyValuePairHashTraits<WTF::GenericHashTraits<CountedKey>, HashTraits<int>>;

struct CountedKVPTranslator {
    static unsigned hash(int value) { return IntHash<int>::hash(value); }
    static bool equal(const CountedKey& a, int b) { return a.value() == b; }
    static void translate(CountedKVP* location, int value, NOESCAPE const auto&)
    {
        std::construct_at(location, CountedKey(value), value * 10);
    }
};

using CountedOrderedHashTable = WTF::OrderedHashTable<CountedKey, CountedKVP, WTF::KeyValuePairKeyExtractor<CountedKVP>, CountedKeyHash, CountedKVPTraits, WTF::GenericHashTraits<CountedKey>, WTF::HashTableMalloc>;
using CountedOrderedHashMap = OrderedHashMap<CountedKey, int, CountedKeyHash, WTF::GenericHashTraits<CountedKey>>;

template<typename Map>
Vector<std::pair<int, int>> contentsOf(const Map& map)
{
    Vector<std::pair<int, int>> result;
    for (auto& entry : map)
        result.append({ entry.key, entry.value });
    return result;
}

} // anonymous namespace

TEST(WTF_OrderedHashMap, HashTranslatorAddConstructsIntoRawStorage)
{
    CountedKey::liveCount = 0;
    auto noFunctor = [] {
        return CountedKVP { CountedKey(-1), -1 };
    };
    {
        CountedOrderedHashTable table;
        for (int i = 0; i < 32; ++i)
            EXPECT_TRUE(table.add<CountedKVPTranslator>(i, noFunctor).isNewEntry);
        EXPECT_EQ(32u, table.size());

        // Re-adding through the translator after removals lands in storage whose previous
        // occupant was destroyed.
        for (int i = 0; i < 32; i += 2)
            EXPECT_TRUE(table.remove(CountedKey(i)));
        EXPECT_EQ(16u, table.size());
        EXPECT_EQ(16, CountedKey::liveCount);
        for (int i = 0; i < 32; i += 2)
            EXPECT_TRUE(table.add<CountedKVPTranslator>(i, noFunctor).isNewEntry);
        EXPECT_EQ(32u, table.size());

        int seen = 0;
        for (auto it = table.begin(); it != table.end(); ++it) {
            EXPECT_EQ(it->key.value() * 10, it->value);
            ++seen;
        }
        EXPECT_EQ(32, seen);
        EXPECT_EQ(32, CountedKey::liveCount);
    }
    EXPECT_EQ(0, CountedKey::liveCount);
}

TEST(WTF_OrderedHashMap, HashTraitsEmptyAndDeletedValuesAreOrdinaryKeys)
{
    // HashTraits<int> reserves 0 as empty and -1 as deleted for WTF::HashTable. This table must
    // treat them like any other key.
    OrderedHashMap<int, int> map;
    EXPECT_EQ(0, map.get(0));
    EXPECT_FALSE(map.contains(-1));
    EXPECT_TRUE(map.add(0, 100).isNewEntry);
    EXPECT_TRUE(map.add(-1, 200).isNewEntry);
    EXPECT_FALSE(map.add(0, 300).isNewEntry);
    EXPECT_FALSE(map.set(-1, 400).isNewEntry);
    EXPECT_EQ(100, map.get(0));
    EXPECT_EQ(400, map.get(-1));
    Vector<std::pair<int, int>> expected { { 0, 100 }, { -1, 400 } };
    EXPECT_EQ(expected, contentsOf(map));

    EXPECT_EQ(400, map.take(-1));
    EXPECT_FALSE(map.contains(-1));
    EXPECT_TRUE(map.remove(0));
    EXPECT_FALSE(map.remove(0));
    EXPECT_TRUE(map.isEmpty());

    EXPECT_TRUE(map.ensure(-1, [] {
        return 5;
    }).isNewEntry);
    EXPECT_TRUE(map.ensure(0, [] {
        return 6;
    }).isNewEntry);
    expected = { { -1, 5 }, { 0, 6 } };
    EXPECT_EQ(expected, contentsOf(map));
}

TEST(WTF_OrderedHashMap, IsValidKey)
{
    EXPECT_TRUE((OrderedHashMap<int, int>::isValidKey(0)));
    EXPECT_TRUE((OrderedHashMap<int, int>::isValidKey(-1)));

    // StringHash cannot hash a null String or the deleted-value sentinel.
    EXPECT_TRUE((OrderedHashMap<String, int>::isValidKey("key"_s)));
    EXPECT_FALSE((OrderedHashMap<String, int>::isValidKey(String())));
    // Destroying a String holding the deleted-value sentinel would deref an invalid pointer.
    static NeverDestroyed<String> deletedString(WTF::HashTableDeletedValue);
    EXPECT_FALSE((OrderedHashMap<String, int>::isValidKey(deletedString.get())));
}

TEST(WTF_OrderedHashMap, KeyWithoutReservedValues)
{
    CountedKey::liveCount = 0;
    {
        CountedOrderedHashMap map;
        for (int i = 0; i < 100; ++i)
            EXPECT_TRUE(map.add(CountedKey(i), i * 2).isNewEntry);
        EXPECT_FALSE(map.add(CountedKey(10), 0).isNewEntry);
        EXPECT_EQ(20, map.get(CountedKey(10)));
        EXPECT_EQ(100, CountedKey::liveCount);

        for (int i = 0; i < 100; i += 2)
            EXPECT_TRUE(map.remove(CountedKey(i)));
        EXPECT_EQ(50u, map.size());
        EXPECT_EQ(50, CountedKey::liveCount);

        EXPECT_EQ(2, map.take(CountedKey(1)));
        EXPECT_EQ(std::optional<int>(6), map.takeOptional(CountedKey(3)));
        EXPECT_EQ(std::nullopt, map.takeOptional(CountedKey(3)));
        EXPECT_TRUE(map.removeIf([](auto& entry) {
            return entry.key.value() > 50;
        }));
        EXPECT_EQ(23u, map.size());

        CountedOrderedHashMap copy = map;
        EXPECT_EQ(46, CountedKey::liveCount);
        int expected = 5;
        for (auto& entry : copy) {
            EXPECT_EQ(expected, entry.key.value());
            EXPECT_EQ(expected * 2, entry.value);
            expected += 2;
        }
        EXPECT_EQ(51, expected);

        map.clear();
        EXPECT_EQ(23, CountedKey::liveCount);
    }
    EXPECT_EQ(0, CountedKey::liveCount);
}

TEST(WTF_OrderedHashMap, TakeKeepsKeyIntact)
{
    // take() moves the mapped value out and must still be able to find and clear the entry.
    OrderedHashMap<String, String> map;
    for (unsigned i = 0; i < 64; ++i)
        map.add(makeString("key"_s, i), makeString("value"_s, i));
    for (unsigned i = 0; i < 64; i += 3)
        EXPECT_EQ(makeString("value"_s, i), map.take(makeString("key"_s, i)));
    for (unsigned i = 0; i < 64; ++i)
        EXPECT_EQ(!!(i % 3), map.contains(makeString("key"_s, i)));
    while (!map.isEmpty()) {
        String firstKey = map.begin()->key;
        String firstValue = map.begin()->value;
        EXPECT_EQ(firstValue, map.takeFirst());
        EXPECT_FALSE(map.contains(firstKey));
    }
}

TEST(WTF_OrderedHashMap, RandomizedAgainstModel)
{
    // A shuffled mix of set, add, remove, take and removeIf over a small key space, checked
    // against a plain vector after each step.
    constexpr int keyCount = 89;
    OrderedHashMap<int, int> map;
    Vector<std::pair<int, int>> model;
    uint32_t randomState = 20261113;
    auto nextRandom = [&] {
        randomState = randomState * 1664525 + 1013904223;
        return randomState >> 8;
    };
    auto modelFind = [&](int key) -> std::pair<int, int>* {
        for (auto& entry : model) {
            if (entry.first == key)
                return &entry;
        }
        return nullptr;
    };
    auto modelRemove = [&](int key) {
        model.removeFirstMatching([&](auto& entry) {
            return entry.first == key;
        });
    };

    for (unsigned step = 0; step < 6000; ++step) {
        int key = static_cast<int>(nextRandom() % keyCount) - 1;
        int value = static_cast<int>(step);
        unsigned action = nextRandom() % 10;
        auto* existing = modelFind(key);
        if (action < 3) {
            EXPECT_EQ(!existing, map.add(key, value).isNewEntry);
            if (!existing)
                model.append({ key, value });
        } else if (action < 5) {
            EXPECT_EQ(!existing, map.set(key, value).isNewEntry);
            if (existing)
                existing->second = value;
            else
                model.append({ key, value });
        } else if (action < 8) {
            EXPECT_EQ(!!existing, map.remove(key));
            modelRemove(key);
        } else if (action == 8) {
            EXPECT_EQ(existing ? std::optional<int>(existing->second) : std::nullopt, map.takeOptional(key));
            modelRemove(key);
        } else if (!(nextRandom() % 8)) {
            int modulus = 2 + nextRandom() % 5;
            auto modelMatches = [&](auto& entry) {
                return !(entry.first % modulus);
            };
            auto mapMatches = [&](auto& entry) {
                return !(entry.key % modulus);
            };
            bool expectChange = model.containsIf(modelMatches);
            EXPECT_EQ(expectChange, map.removeIf(mapMatches));
            model.removeAllMatching(modelMatches);
        }

        EXPECT_EQ(model.size(), map.size());
        if (!(step % 16)) {
            EXPECT_EQ(model, contentsOf(map));
            for (int k = -1; k < keyCount - 1; ++k) {
                auto* entry = modelFind(k);
                EXPECT_EQ(entry ? std::optional<int>(entry->second) : std::nullopt, map.getOptional(k));
            }
        }
    }
    EXPECT_EQ(model, contentsOf(map));
}

TEST(WTF_OrderedHashMap, EqualIgnoringOrder)
{
    // OrderedHashMap intentionally does not define operator==, so that the
    // order-sensitive vs order-insensitive choice is explicit at the call
    // site. equalIgnoringOrder compares contents, matching HashMap::operator==
    // semantics.
    OrderedHashMap<String, unsigned> a;
    a.add("alpha"_s, 1);
    a.add("beta"_s, 2);
    a.add("gamma"_s, 3);

    OrderedHashMap<String, unsigned> b;
    b.add("gamma"_s, 3);
    b.add("alpha"_s, 1);
    b.add("beta"_s, 2);

    EXPECT_TRUE(equalIgnoringOrder(a, b));
    EXPECT_TRUE(equalIgnoringOrder(b, a));

    // Sanity: the two really are in different iteration order.
    auto itA = a.begin();
    auto itB = b.begin();
    EXPECT_NE(itA->key, itB->key);
}

TEST(WTF_OrderedHashMap, EqualIgnoringOrderDifferentSize)
{
    OrderedHashMap<String, unsigned> a;
    a.add("alpha"_s, 1);
    a.add("beta"_s, 2);

    OrderedHashMap<String, unsigned> b;
    b.add("alpha"_s, 1);

    EXPECT_FALSE(equalIgnoringOrder(a, b));
    EXPECT_FALSE(equalIgnoringOrder(b, a));
}

TEST(WTF_OrderedHashMap, EqualIgnoringOrderDifferentValues)
{
    OrderedHashMap<String, unsigned> a;
    a.add("alpha"_s, 1);
    a.add("beta"_s, 2);

    OrderedHashMap<String, unsigned> b;
    b.add("alpha"_s, 1);
    b.add("beta"_s, 99);

    EXPECT_FALSE(equalIgnoringOrder(a, b));
    EXPECT_FALSE(equalIgnoringOrder(b, a));
}

TEST(WTF_OrderedHashMap, EqualIgnoringOrderEmpty)
{
    OrderedHashMap<String, unsigned> a;
    OrderedHashMap<String, unsigned> b;
    EXPECT_TRUE(equalIgnoringOrder(a, b));

    a.add("key"_s, 0);
    EXPECT_FALSE(equalIgnoringOrder(a, b));
}

} // namespace TestWebKitAPI
