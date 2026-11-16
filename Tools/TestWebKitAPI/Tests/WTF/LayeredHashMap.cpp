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

#include <utility>
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

    // add() leaves a present entry alone.
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
    // the shape of the collisions; varying how pushes, pops, inserts, and overlays interleave is
    // what decides where a drop punches a hole into a probe run that a later lookup has to cross.
    constexpr unsigned keyCount = 64;
    TestMap map;
    Vector<Vector<std::pair<unsigned, unsigned>>> model;
    uint32_t randomState = 20260918;
    auto nextRandom = [&] {
        randomState = randomState * 1664525 + 1013904223;
        return randomState >> 8;
    };

    auto modelFind = [&](unsigned key) -> unsigned {
        for (unsigned layer = model.size(); layer--;) {
            for (unsigned i = model[layer].size(); i--;) {
                if (model[layer][i].first == key)
                    return model[layer][i].second;
            }
        }
        return 0;
    };

    auto checkAgainstModel = [&] {
        EXPECT_EQ(map.layerCount(), model.size());
        for (unsigned key = 0; key < keyCount; ++key)
            EXPECT_EQ(map.find(TestKey { key }).value_or(0), modelFind(key));
    };

    for (unsigned step = 0; step < 2000; ++step) {
        unsigned action = nextRandom() % 16;
        if (model.isEmpty() || !action) {
            model.constructAndAppend();
            map.startLayer();
        } else if (action == 1) {
            model.removeLast();
            map.dropLastLayer();
        } else if (action == 2 && !(nextRandom() % 4)) {
            for (auto& layer : model)
                layer.shrink(0);
            map.clearEntries();
        } else if (action == 3 && !(nextRandom() % 8)) {
            model.clear();
            map.clear();
        } else if (action < 6) {
            unsigned key = nextRandom() % keyCount;
            unsigned value = step + 1;
            map.overlay(TestKey { key }, value);
            model.last().append({ key, value });
        } else {
            unsigned key = nextRandom() % keyCount;
            unsigned value = step + 1;
            unsigned existing = modelFind(key);
            EXPECT_EQ(map.findOrAdd(TestKey { key }, value).value_or(0), existing);
            if (!existing)
                model.last().append({ key, value });
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

TEST(WTF_LayeredHashMap, OverlayShadowsEnclosingLayer)
{
    TestMap map;
    map.startLayer();
    EXPECT_TRUE(map.add(TestKey { 1 }, 10));

    map.startLayer();
    map.overlay(TestKey { 1 }, 20);
    EXPECT_EQ(map.find(TestKey { 1 }).value_or(0), 20U);
    EXPECT_EQ(map.findOrAdd(TestKey { 1 }, 30).value_or(0), 20U);
    EXPECT_FALSE(map.add(TestKey { 1 }, 30));

    map.startLayer();
    map.overlay(TestKey { 1 }, 40);
    EXPECT_EQ(map.find(TestKey { 1 }).value_or(0), 40U);

    map.dropLastLayer();
    EXPECT_EQ(map.find(TestKey { 1 }).value_or(0), 20U);
    map.dropLastLayer();
    EXPECT_EQ(map.find(TestKey { 1 }).value_or(0), 10U);
    map.dropLastLayer();
    EXPECT_FALSE(map.find(TestKey { 1 }));
    EXPECT_EQ(map.layerCount(), 0U);
}

TEST(WTF_LayeredHashMap, OverlayWithinOneLayer)
{
    // Overlaying the same key repeatedly in one layer keeps only the newest value visible, and one
    // drop undoes all of them, including the one that introduced the key.
    TestMap map;
    map.startLayer();
    EXPECT_TRUE(map.add(TestKey { 2 }, 1));

    map.startLayer();
    map.overlay(TestKey { 3 }, 100);
    for (unsigned value = 2; value <= 10; ++value) {
        map.overlay(TestKey { 2 }, value);
        map.overlay(TestKey { 3 }, value + 100);
        EXPECT_EQ(map.find(TestKey { 2 }).value_or(0), value);
        EXPECT_EQ(map.find(TestKey { 3 }).value_or(0), value + 100);
    }
    map.dropLastLayer();
    EXPECT_EQ(map.find(TestKey { 2 }).value_or(0), 1U);
    EXPECT_FALSE(map.find(TestKey { 3 }));

    map.dropLastLayer();
    EXPECT_FALSE(map.find(TestKey { 2 }));
}

TEST(WTF_LayeredHashMap, OverlayInLayerThatAddedKey)
{
    TestMap map;
    map.startLayer();
    EXPECT_TRUE(map.add(TestKey { 6 }, 1));
    map.overlay(TestKey { 6 }, 2);
    EXPECT_EQ(map.find(TestKey { 6 }).value_or(0), 2U);

    map.startLayer();
    EXPECT_TRUE(map.add(TestKey { 7 }, 3));
    map.overlay(TestKey { 7 }, 4);
    map.overlay(TestKey { 6 }, 5);
    map.dropLastLayer();
    EXPECT_FALSE(map.find(TestKey { 7 }));
    EXPECT_EQ(map.find(TestKey { 6 }).value_or(0), 2U);

    map.dropLastLayer();
    EXPECT_FALSE(map.find(TestKey { 6 }));
    EXPECT_EQ(map.layerCount(), 0U);
}

TEST(WTF_LayeredHashMap, OverlayAcrossRehash)
{
    // Every layer overlays every key of the layers below it before adding keys of its own, so the
    // rehashes those additions force have to rebuild slots holding a stack of shadowed entries.
    constexpr unsigned layerCount = 5;

    TestMap map;
    for (unsigned layer = 0; layer < layerCount; ++layer) {
        map.startLayer();
        for (unsigned key = 0; key < layer * keysPerLayer; ++key)
            map.overlay(TestKey { key }, key + layer * 1000);
        for (unsigned i = 0; i < keysPerLayer; ++i) {
            unsigned key = layer * keysPerLayer + i;
            EXPECT_TRUE(map.add(TestKey { key }, key + layer * 1000));
        }
        for (unsigned key = 0; key < (layer + 1) * keysPerLayer; ++key)
            EXPECT_EQ(map.find(TestKey { key }).value_or(0), key + layer * 1000);
    }

    for (unsigned layer = layerCount; layer--;) {
        map.dropLastLayer();
        for (unsigned i = 0; i < keysPerLayer; ++i)
            EXPECT_FALSE(map.find(TestKey { layer * keysPerLayer + i }));
        if (!layer)
            break;
        for (unsigned key = 0; key < layer * keysPerLayer; ++key)
            EXPECT_EQ(map.find(TestKey { key }).value_or(0), key + (layer - 1) * 1000);
    }
    EXPECT_EQ(map.layerCount(), 0U);
}

TEST(WTF_LayeredHashMap, OverlayOfNewKeyRehashes)
{
    // overlay() of keys no layer holds yet is what grows the table here, while the enclosing
    // layer's keys sit shadowed in the same probe runs.
    TestMap map;
    map.startLayer();
    for (unsigned i = 0; i < keysPerSibling; ++i)
        EXPECT_TRUE(map.add(TestKey { i }, i + 1));

    map.startLayer();
    for (unsigned i = 0; i < keysPerSibling; ++i)
        map.overlay(TestKey { i }, i + 100);
    for (unsigned key = keysPerSibling; key < keysPerSibling + keysPerLayer; ++key) {
        EXPECT_FALSE(map.find(TestKey { key }));
        map.overlay(TestKey { key }, key + 100);
    }
    for (unsigned key = 0; key < keysPerSibling + keysPerLayer; ++key)
        EXPECT_EQ(map.find(TestKey { key }).value_or(0), key + 100);

    map.dropLastLayer();
    for (unsigned i = 0; i < keysPerSibling; ++i)
        EXPECT_EQ(map.find(TestKey { i }).value_or(0), i + 1);
    for (unsigned key = keysPerSibling; key < keysPerSibling + keysPerLayer; ++key)
        EXPECT_FALSE(map.find(TestKey { key }));
    map.dropLastLayer();
    EXPECT_EQ(map.layerCount(), 0U);
}

TEST(WTF_LayeredHashMap, ClearEntriesDropsOverlays)
{
    TestMap map;
    map.startLayer();
    EXPECT_TRUE(map.add(TestKey { 4 }, 1));
    map.startLayer();
    map.overlay(TestKey { 4 }, 2);
    map.clearEntries();
    EXPECT_EQ(map.layerCount(), 2U);
    EXPECT_FALSE(map.find(TestKey { 4 }));

    map.overlay(TestKey { 4 }, 3);
    EXPECT_EQ(map.find(TestKey { 4 }).value_or(0), 3U);
    map.dropLastLayer();
    EXPECT_FALSE(map.find(TestKey { 4 }));
    EXPECT_TRUE(map.add(TestKey { 4 }, 5));
    EXPECT_EQ(map.find(TestKey { 4 }).value_or(0), 5U);
    map.dropLastLayer();
    EXPECT_FALSE(map.find(TestKey { 4 }));
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

TEST(WTF_LayeredHashMap, Clear)
{
    constexpr unsigned layerCount = 3;

    TestMap map;
    for (unsigned layer = 0; layer < layerCount; ++layer) {
        map.startLayer();
        for (unsigned i = 0; i < keysPerLayer; ++i)
            EXPECT_TRUE(map.add(TestKey { layer * keysPerLayer + i }, i + 1));
    }
    map.clear();
    EXPECT_EQ(map.layerCount(), 0U);
    for (unsigned key = 0; key < layerCount * keysPerLayer; ++key)
        EXPECT_FALSE(map.find(TestKey { key }));

    map.startLayer();
    for (unsigned i = 0; i < keysPerLayer; ++i)
        EXPECT_TRUE(map.add(TestKey { i }, i + 200));
    for (unsigned i = 0; i < keysPerLayer; ++i)
        EXPECT_EQ(map.find(TestKey { i }).value_or(0), i + 200);
    map.dropLastLayer();
    EXPECT_EQ(map.layerCount(), 0U);
    for (unsigned i = 0; i < keysPerLayer; ++i)
        EXPECT_FALSE(map.find(TestKey { i }));
}

TEST(WTF_LayeredHashMap, WellDistributedHash)
{
    // TestKey's hash leaves every bit above the lowest three clear, so only a hash that spreads
    // across the whole table exercises the initial-slot masking as the table grows.
    constexpr unsigned layerCount = 4;
    constexpr unsigned keysPerSpreadLayer = 1000;

    LayeredHashMap<unsigned, unsigned> map;
    for (unsigned layer = 0; layer < layerCount; ++layer) {
        map.startLayer();
        for (unsigned i = 0; i < keysPerSpreadLayer; ++i) {
            unsigned key = layer * keysPerSpreadLayer + i + 1;
            EXPECT_FALSE(map.find(key));
            EXPECT_TRUE(map.add(key, key * 3));
        }
        for (unsigned key = 1; key <= (layer + 1) * keysPerSpreadLayer; ++key)
            EXPECT_EQ(map.find(key).value_or(0), key * 3);
    }

    for (unsigned layer = layerCount; layer--;) {
        map.dropLastLayer();
        for (unsigned i = 0; i < keysPerSpreadLayer; ++i)
            EXPECT_FALSE(map.find(layer * keysPerSpreadLayer + i + 1));
        for (unsigned key = 1; key <= layer * keysPerSpreadLayer; ++key)
            EXPECT_EQ(map.find(key).value_or(0), key * 3);
    }
    EXPECT_EQ(map.layerCount(), 0U);
}

} // namespace TestWebKitAPI
