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
#include <wtf/LayeredHashMap.h>

#include <array>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

namespace {

struct TestKey {
    unsigned value { 0 };
    bool operator==(const TestKey& other) const { return value == other.value; }
    // Deliberately collision-heavy, so that entries pile into shared probe runs and the
    // ordering the map has to maintain across a rehash actually gets exercised.
    unsigned hash() const { return value & 0x7; }
};

// Reserving UINT_MAX rather than the default-constructed key leaves zero usable below, and
// makes the map answer to traits it did not choose for itself.
struct TestKeyTraits : HashTraits<TestKey> {
    static TestKey emptyValue() { return TestKey { UINT_MAX }; }
};

using TestMap = LayeredHashMap<TestKey, unsigned, DefaultHash<TestKey>, TestKeyTraits>;

// Enough keys to force several rehashes out of the smallest table.
constexpr unsigned keysPerLayer = 40;
constexpr unsigned keysPerSibling = 16;

} // anonymous namespace

TEST(WTF_LayeredHashMap, NestedLayers)
{
    constexpr unsigned layerCount = 6;

    TestMap map;
    for (unsigned layer = 0; layer < layerCount; ++layer) {
        map.startLayer();
        for (unsigned i = 0; i < keysPerLayer; ++i) {
            unsigned key = layer * keysPerLayer + i;
            EXPECT_FALSE(map.find(TestKey { key }));
            map.add(TestKey { key }, key * 3);
        }
        // Everything inserted so far, in this layer and every enclosing one, is still visible.
        for (unsigned key = 0; key <= layer * keysPerLayer + keysPerLayer - 1; ++key)
            EXPECT_EQ(map.find(TestKey { key }).value_or(0), key * 3);
    }

    // Dropping a layer removes exactly that layer's keys and leaves the enclosing ones findable,
    // which is the property that breaks if a rehash reorders entries within a probe run.
    for (unsigned layer = layerCount; layer--;) {
        map.dropLastLayer();
        for (unsigned i = 0; i < keysPerLayer; ++i)
            EXPECT_FALSE(map.find(TestKey { layer * keysPerLayer + i }));
        for (unsigned key = 0; key < layer * keysPerLayer; ++key)
            EXPECT_EQ(map.find(TestKey { key }).value_or(0), key * 3);
    }
    EXPECT_EQ(map.layerCount(), 0U);
}

TEST(WTF_LayeredHashMap, AddDoesNotUpdate)
{
    TestMap map;
    map.startLayer();
    map.add(TestKey { 1 }, 11);
    map.clearEntries();
    EXPECT_FALSE(map.find(TestKey { 1 }));
    EXPECT_EQ(map.layerCount(), 1U);
    map.add(TestKey { 1 }, 12);
    EXPECT_EQ(map.find(TestKey { 1 }).value_or(0), 12U);

    // A key that is already present cannot be updated, since the entry may belong to an enclosing
    // layer that has to get it back.
    EXPECT_FALSE(map.add(TestKey { 1 }, 13));
    EXPECT_EQ(map.find(TestKey { 1 }).value_or(0), 12U);
    map.dropLastLayer();
    EXPECT_FALSE(map.find(TestKey { 1 }));
}

TEST(WTF_LayeredHashMap, FindOrAdd)
{
    // findOrAdd answers with the entry it left in place, and with nothing when it was the one to
    // put the entry there.
    TestMap map;
    map.startLayer();
    EXPECT_FALSE(map.findOrAdd(TestKey { 5 }, 50));
    EXPECT_EQ(map.findOrAdd(TestKey { 5 }, 60).value_or(0), 50U);
    EXPECT_EQ(map.find(TestKey { 5 }).value_or(0), 50U);
    map.dropLastLayer();
    EXPECT_FALSE(map.find(TestKey { 5 }));
}

TEST(WTF_LayeredHashMap, SiblingLayers)
{
    // A tree walk pushes and pops at the same depth once per sibling, so a layer is opened over a
    // table that a drop has already punched holes in, and the rehashes that matter happen after
    // those drops rather than during a monotone run of pushes. Keep one enclosing layer's keys
    // live throughout to catch a rehash that loses them.
    constexpr unsigned siblingCount = 24;
    constexpr unsigned enclosingKeyBase = 1000000;

    TestMap map;
    map.startLayer();
    for (unsigned i = 0; i < keysPerSibling; ++i)
        map.add(TestKey { enclosingKeyBase + i }, i);

    for (unsigned sibling = 0; sibling < siblingCount; ++sibling) {
        map.startLayer();
        for (unsigned i = 0; i < keysPerSibling; ++i) {
            unsigned key = sibling * keysPerSibling + i;
            EXPECT_FALSE(map.find(TestKey { key }));
            EXPECT_TRUE(map.add(TestKey { key }, key * 5));
        }
        for (unsigned i = 0; i < keysPerSibling; ++i) {
            unsigned key = sibling * keysPerSibling + i;
            EXPECT_EQ(map.find(TestKey { key }).value_or(1), key * 5);
            EXPECT_EQ(map.find(TestKey { enclosingKeyBase + i }).value_or(~0U), i);
        }
        map.dropLastLayer();
        // The sibling that just ended is gone, and every earlier sibling stayed gone.
        for (unsigned earlier = 0; earlier <= sibling; ++earlier) {
            for (unsigned i = 0; i < keysPerSibling; ++i)
                EXPECT_FALSE(map.find(TestKey { earlier * keysPerSibling + i }));
        }
        for (unsigned i = 0; i < keysPerSibling; ++i)
            EXPECT_EQ(map.find(TestKey { enclosingKeyBase + i }).value_or(~0U), i);
    }

    map.dropLastLayer();
    EXPECT_EQ(map.layerCount(), 0U);
}

TEST(WTF_LayeredHashMap, ClearEntriesKeepsLayers)
{
    // Clearing from a layer nested inside others that are still holding entries removes every
    // entry but keeps the layers, and what comes after still unwinds to an empty map.
    constexpr unsigned layerCount = 4;

    TestMap map;
    for (unsigned layer = 0; layer < layerCount; ++layer) {
        map.startLayer();
        for (unsigned i = 0; i < keysPerSibling; ++i)
            EXPECT_TRUE(map.add(TestKey { layer * keysPerSibling + i }, layer + 1));
    }
    map.clearEntries();
    EXPECT_EQ(map.layerCount(), layerCount);
    for (unsigned key = 0; key < layerCount * keysPerSibling; ++key)
        EXPECT_FALSE(map.find(TestKey { key }));

    // Enough refilling to rehash over the region the clear emptied, then unwound one layer at a
    // time.
    map.startLayer();
    for (unsigned i = 0; i < keysPerLayer; ++i)
        EXPECT_TRUE(map.add(TestKey { i }, i + 100));
    for (unsigned i = 0; i < keysPerLayer; ++i)
        EXPECT_EQ(map.find(TestKey { i }).value_or(0), i + 100);
    while (map.layerCount())
        map.dropLastLayer();
    for (unsigned i = 0; i < keysPerLayer; ++i)
        EXPECT_FALSE(map.find(TestKey { i }));
}

TEST(WTF_LayeredHashMap, RandomizedAgainstModel)
{
    // A randomized walk cross-checked against a plain list-of-layers model. The other tests fix
    // the shape of the collisions; varying how pushes, pops, and inserts interleave is what
    // decides where a drop punches a hole into a probe run that a later lookup has to cross.
    constexpr unsigned keyCount = 64;
    TestMap map;
    Vector<Vector<unsigned>> model;
    uint32_t randomState = 20260918;
    auto nextRandom = [&] {
        randomState = randomState * 1664525 + 1013904223;
        return randomState >> 8;
    };

    auto checkAgainstModel = [&] {
        std::array<unsigned, keyCount> expected { };
        for (const Vector<unsigned>& layer : model) {
            for (unsigned key : layer)
                expected[key] = key + 1;
        }
        for (unsigned key = 0; key < keyCount; ++key)
            EXPECT_EQ(map.find(TestKey { key }).value_or(0), expected[key]);
    };

    for (unsigned step = 0; step < 500; ++step) {
        unsigned action = nextRandom() % 16;
        if (model.isEmpty() || !action) {
            model.append(Vector<unsigned> { });
            map.startLayer();
        } else if (action == 1) {
            model.removeLast();
            map.dropLastLayer();
        } else if (action == 2) {
            for (Vector<unsigned>& layer : model)
                layer.shrink(0);
            map.clearEntries();
        } else {
            unsigned key = nextRandom() % keyCount;
            bool live = false;
            for (const Vector<unsigned>& layer : model)
                live |= layer.contains(key);
            EXPECT_EQ(map.add(TestKey { key }, key + 1), !live);
            if (!live)
                model.last().append(key);
        }
        checkAgainstModel();
    }

    // Unwind what the walk above left open, which is the one drop sequence a deep interleave of
    // pushes, pops and clears never reaches on its own.
    while (!model.isEmpty()) {
        model.removeLast();
        map.dropLastLayer();
        checkAgainstModel();
    }
    EXPECT_EQ(map.layerCount(), 0U);
}

TEST(WTF_LayeredHashMap, DefaultTraits)
{
    // With the default traits the default-constructed key is the reserved empty one, so the keys
    // below all avoid it.
    LayeredHashMap<TestKey, unsigned> map;
    map.startLayer();
    for (unsigned key = 1; key <= 64; ++key)
        EXPECT_TRUE(map.add(TestKey { key }, key * 7));
    for (unsigned key = 1; key <= 64; ++key)
        EXPECT_EQ(map.find(TestKey { key }).value_or(0), key * 7);
    map.dropLastLayer();
    EXPECT_EQ(map.layerCount(), 0U);
}

} // namespace TestWebKitAPI
