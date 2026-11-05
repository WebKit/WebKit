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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include <wtf/HashTable.h>

#include <random>
#include <unordered_set>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/RobinHoodHashTable.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using WTF::DefaultHash;
using WTF::GenericHashTraits;
using WTF::HashTable;
using WTF::HashTraits;
using WTF::ShouldValidateKey;
using WTF::StatusByte;

struct TestIdentityExtractor {
    template<typename T> static const T& extract(const T& value) { return value; }
};

using IntTable = HashTable<int, int, TestIdentityExtractor, DefaultHash<int>, HashTraits<int>, HashTraits<int>>;

// Reads HashTable's layout for int values: 16 bytes of metadata, then slots, then status bytes.
template<typename Table>
class Layout {
public:
    explicit Layout(const Table& table)
        : m_slots(table.end().get() - table.capacity())
        , m_capacity(table.capacity())
    {
    }

    const StatusByte* statusBytes() const { return reinterpret_cast<const StatusByte*>(m_slots + m_capacity); }
    StatusByte statusByte(unsigned index) const { return statusBytes()[index]; }
    unsigned deletedCount() const { return reinterpret_cast<const unsigned*>(m_slots)[-4]; }
    unsigned keyCount() const { return reinterpret_cast<const unsigned*>(m_slots)[-3]; }
    unsigned indexOf(const void* entry) const { return static_cast<const typename Table::ValueType*>(entry) - m_slots; }
    const typename Table::ValueType& slot(unsigned index) const { return m_slots[index]; }

private:
    const typename Table::ValueType* m_slots;
    unsigned m_capacity;
};

static unsigned homeSlot(int key, unsigned capacity)
{
    return DefaultHash<int>::hash(key) & (capacity - 1);
}

static Vector<int> keysWithHomeSlot(unsigned slot, unsigned capacity, unsigned count, int firstCandidate = 1)
{
    Vector<int> keys;
    for (int key = firstCandidate; keys.size() < count; ++key) {
        if (homeSlot(key, capacity) == slot)
            keys.append(key);
    }
    return keys;
}

static Vector<int> iterationOrder(const IntTable& table)
{
    Vector<int> result;
    for (int value : table)
        result.append(value);
    return result;
}

struct Tracked {
    static constexpr unsigned aliveMagic = 0x5a5a1234;
    static constexpr unsigned deadMagic = 0xdeadbeef;
    static int liveCount;

    explicit Tracked(int key)
        : key(key)
    {
        ++liveCount;
    }

    Tracked(const Tracked& other)
        : key(other.key)
    {
        EXPECT_EQ(other.magic, aliveMagic);
        ++liveCount;
    }

    Tracked(Tracked&& other)
        : key(other.key)
    {
        EXPECT_EQ(other.magic, aliveMagic);
        ++liveCount;
    }

    Tracked& operator=(const Tracked&) = delete;
    Tracked& operator=(Tracked&&) = delete;

    ~Tracked()
    {
        EXPECT_EQ(magic, aliveMagic);
        magic = deadMagic;
        --liveCount;
    }

    int key;
    unsigned magic { aliveMagic };
};

int Tracked::liveCount = 0;

struct TrackedExtractor {
    static const int& extract(const Tracked& value) { return value.key; }
};

struct TrackedTraits : GenericHashTraits<Tracked> { };

using TrackedTable = HashTable<int, Tracked, TrackedExtractor, DefaultHash<int>, TrackedTraits, HashTraits<int>>;

struct MoveOnlyKey {
    explicit MoveOnlyKey(int value)
        : value(value)
    {
    }

    MoveOnlyKey(MoveOnlyKey&& other)
        : value(std::exchange(other.value, 0))
    {
    }

    MoveOnlyKey(const MoveOnlyKey&) = delete;
    MoveOnlyKey& operator=(const MoveOnlyKey&) = delete;
    MoveOnlyKey& operator=(MoveOnlyKey&&) = delete;

    friend bool operator==(const MoveOnlyKey&, const MoveOnlyKey&) = default;

    int value;
};

struct MoveOnlyKeyHash {
    static unsigned hash(const MoveOnlyKey& key) { return WTF::intHash(static_cast<uint32_t>(key.value)); }
    static bool equal(const MoveOnlyKey& a, const MoveOnlyKey& b) { return a == b; }
};

struct MoveOnlyKeyTraits : GenericHashTraits<MoveOnlyKey> { };

using MoveOnlyTable = HashTable<MoveOnlyKey, MoveOnlyKey, TestIdentityExtractor, MoveOnlyKeyHash, MoveOnlyKeyTraits, MoveOnlyKeyTraits>;

static std::array<bool, 128> fakeWeakTargetIsAlive;

struct FakeWeakKey {
    static int liveCount;

    explicit FakeWeakKey(int id)
        : id(id)
    {
        ++liveCount;
    }

    FakeWeakKey(const FakeWeakKey& other)
        : id(other.id)
    {
        ++liveCount;
    }

    ~FakeWeakKey() { --liveCount; }

    FakeWeakKey& operator=(const FakeWeakKey&) = delete;

    int id;
};

int FakeWeakKey::liveCount = 0;

struct FakeWeakKeyHash {
    static unsigned hash(const FakeWeakKey& key) { return WTF::intHash(static_cast<uint32_t>(key.id)); }
    static unsigned hash(int id) { return WTF::intHash(static_cast<uint32_t>(id)); }
    static bool equal(const FakeWeakKey& a, const FakeWeakKey& b) { return a.id == b.id; }
    static bool equal(const FakeWeakKey& a, int id) { return a.id == id; }
};

struct FakeWeakKeyTraits : GenericHashTraits<FakeWeakKey> {
    static constexpr bool hasIsWeakNullValueFunction = true;
    static bool isWeakNullValue(const FakeWeakKey& key) { return key.id > 0 && !fakeWeakTargetIsAlive[key.id]; }
};

using FakeWeakTable = HashTable<FakeWeakKey, FakeWeakKey, TestIdentityExtractor, FakeWeakKeyHash, FakeWeakKeyTraits, FakeWeakKeyTraits>;

struct DoublingTranslator {
    static unsigned hash(int half) { return DefaultHash<int>::hash(half * 2); }
    static bool equal(int stored, int half) { return stored == half * 2; }
    static void translate(int* location, int half, const auto& functor)
    {
        EXPECT_EQ(functor(), half * 2);
        std::construct_at(location, functor());
    }
    static void translate(int* location, int half, const auto& functor, unsigned hash)
    {
        EXPECT_EQ(hash, DoublingTranslator::hash(half));
        translate(location, half, functor);
    }
};

TEST(WTF_HashTable, AddFindRemoveAtManySizes)
{
    for (unsigned count : { 1u, 2u, 5u, 6u, 7u, 8u, 15u, 16u, 17u, 31u, 100u, 1000u, 10000u }) {
        IntTable table;
        for (unsigned i = 1; i <= count; ++i) {
            auto result = table.add(static_cast<int>(i * 3));
            EXPECT_TRUE(result.isNewEntry);
            EXPECT_EQ(*result.iterator, static_cast<int>(i * 3));
            EXPECT_FALSE(table.add(static_cast<int>(i * 3)).isNewEntry);
        }
        EXPECT_EQ(table.size(), count);
        table.checkTableConsistency();

        for (unsigned i = 1; i <= count; ++i) {
            EXPECT_TRUE(table.contains(static_cast<int>(i * 3)));
            EXPECT_FALSE(table.contains(static_cast<int>(i * 3 + 1)));
            auto it = table.find(static_cast<int>(i * 3));
            ASSERT_TRUE(it != table.end());
            EXPECT_EQ(*it, static_cast<int>(i * 3));
            EXPECT_EQ(table.lookup(static_cast<int>(i * 3)), it.get());
        }
        EXPECT_TRUE(table.find(1) == table.end());
        EXPECT_FALSE(table.lookup(1));

        for (unsigned i = 1; i <= count; i += 2)
            table.remove<ShouldValidateKey::Yes>(static_cast<int>(i * 3));
        table.checkTableConsistency();
        EXPECT_EQ(table.size(), count / 2);
        for (unsigned i = 1; i <= count; ++i)
            EXPECT_EQ(table.contains(static_cast<int>(i * 3)), !(i % 2));

        table.remove<ShouldValidateKey::Yes>(1);
        EXPECT_EQ(table.size(), count / 2);
        table.clear();
        EXPECT_TRUE(table.isEmpty());
        EXPECT_FALSE(table.contains(6));
    }
}

TEST(WTF_HashTable, EmptyTable)
{
    IntTable table;
    EXPECT_TRUE(table.isNullStorage());
    EXPECT_TRUE(table.begin() == table.end());
    EXPECT_TRUE(table.random() == table.end());
    EXPECT_FALSE(table.contains(1));
    EXPECT_TRUE(table.find(1) == table.end());
    EXPECT_EQ(table.byteSize(), 0u);
    table.remove<ShouldValidateKey::Yes>(1);
    table.checkTableConsistency();
}

TEST(WTF_HashTable, ByteSizeCoversStatusBytesMetadataAndSlots)
{
    IntTable table;
    table.add(1);
    EXPECT_EQ(table.capacity(), 8u);
    EXPECT_EQ(table.byteSize(), 8 + 16 + 8 * sizeof(int));

    for (int i = 2; i <= 30; ++i)
        table.add(i);
    EXPECT_EQ(table.capacity(), 64u);
    EXPECT_EQ(table.byteSize(), 64 + 16 + 64 * sizeof(int));
}

TEST(WTF_HashTable, LifetimesBalance)
{
    Tracked::liveCount = 0;
    {
        TrackedTable table;
        for (int i = 1; i <= 1000; ++i) {
            table.add(Tracked { i });
            EXPECT_EQ(Tracked::liveCount, static_cast<int>(table.size()));
        }
        EXPECT_FALSE(table.add(Tracked { 7 }).isNewEntry);
        EXPECT_EQ(Tracked::liveCount, 1000);

        for (int i = 1; i <= 1000; i += 3)
            table.remove<ShouldValidateKey::Yes>(i);
        EXPECT_EQ(Tracked::liveCount, static_cast<int>(table.size()));

        table.removeIf([](Tracked& value) {
            return value.key % 2;
        });
        EXPECT_EQ(Tracked::liveCount, static_cast<int>(table.size()));

        for (int i = 1; i <= 990; ++i)
            table.remove<ShouldValidateKey::Yes>(i);
        EXPECT_EQ(Tracked::liveCount, static_cast<int>(table.size()));
        table.checkTableConsistency();

        {
            TrackedTable copy(table);
            EXPECT_EQ(Tracked::liveCount, 2 * static_cast<int>(table.size()));
        }
        EXPECT_EQ(Tracked::liveCount, static_cast<int>(table.size()));

        auto taken = table.takeIf<0>([](const Tracked& value) {
            return value.key > 995;
        });
        EXPECT_EQ(Tracked::liveCount, static_cast<int>(table.size() + taken.size()));
        taken.clear();

        for (int i = 1; i <= 100; ++i)
            table.add(Tracked { i });
        table.clear();
        EXPECT_EQ(Tracked::liveCount, 0);

        for (int i = 1; i <= 100; ++i)
            table.add(Tracked { i });
    }
    EXPECT_EQ(Tracked::liveCount, 0);
}

TEST(WTF_HashTable, MoveOnlyValues)
{
    MoveOnlyTable table;
    for (int i = 1; i <= 500; ++i)
        EXPECT_TRUE(table.add(MoveOnlyKey { i }).isNewEntry);
    EXPECT_FALSE(table.add(MoveOnlyKey { 42 }).isNewEntry);
    EXPECT_EQ(table.size(), 500u);
    table.checkTableConsistency();

    for (int i = 1; i <= 500; ++i) {
        auto it = table.find(MoveOnlyKey { i });
        ASSERT_TRUE(it != table.end());
        EXPECT_EQ(it->value, i);
    }

    for (int i = 1; i <= 400; ++i)
        table.remove<ShouldValidateKey::Yes>(MoveOnlyKey { i });
    EXPECT_EQ(table.size(), 100u);

    auto taken = table.takeIf<0>([](const MoveOnlyKey& key) {
        return key.value > 450;
    });
    EXPECT_EQ(taken.size(), 50u);
    for (auto& key : taken)
        EXPECT_GT(key.value, 450);
    EXPECT_EQ(table.size(), 50u);

    MoveOnlyTable moved(WTF::move(table));
    EXPECT_EQ(moved.size(), 50u);
    EXPECT_TRUE(table.isEmpty());
    EXPECT_TRUE(moved.contains(MoveOnlyKey { 401 }));
    moved.checkTableConsistency();
}

TEST(WTF_HashTable, RemoveWritesDeleted)
{
    IntTable table;
    for (int i = 1; i <= 11; ++i)
        table.add(i);
    ASSERT_EQ(table.capacity(), 16u);

    Layout layout(table);
    for (int i = 1; i <= 8; ++i) {
        unsigned index = layout.indexOf(table.find(i).get());
        table.remove<ShouldValidateKey::Yes>(i);
        ASSERT_EQ(table.capacity(), 16u);
        EXPECT_TRUE(layout.statusByte(index).isDeleted());
        EXPECT_EQ(layout.deletedCount(), static_cast<unsigned>(i));
        EXPECT_FALSE(table.contains(i));
        table.checkTableConsistency();
    }
    for (int i = 9; i <= 11; ++i)
        EXPECT_TRUE(table.contains(i));
}

TEST(WTF_HashTable, AddReusesDeletedSlotInProbeSequence)
{
    constexpr unsigned capacity = 8;
    Vector<int> keys = keysWithHomeSlot(0, capacity, 3);
    IntTable table;
    for (int key : keys)
        table.add(key);
    ASSERT_EQ(table.capacity(), capacity);

    // Colliding keys take the triangular probe sequence 0, 1, 3.
    Layout layout(table);
    EXPECT_EQ(layout.indexOf(table.find(keys[0]).get()), 0u);
    EXPECT_EQ(layout.indexOf(table.find(keys[1]).get()), 1u);
    EXPECT_EQ(layout.indexOf(table.find(keys[2]).get()), 3u);

    table.remove<ShouldValidateKey::Yes>(keys[0]);
    EXPECT_TRUE(layout.statusByte(0).isDeleted());
    EXPECT_EQ(layout.deletedCount(), 1u);
    EXPECT_FALSE(table.contains(keys[0]));
    EXPECT_TRUE(table.contains(keys[1]));
    EXPECT_TRUE(table.contains(keys[2]));
    table.checkTableConsistency();

    int newKey = keysWithHomeSlot(0, capacity, 1, keys.last() + 1)[0];
    EXPECT_TRUE(table.add(newKey).isNewEntry);
    EXPECT_EQ(layout.indexOf(table.find(newKey).get()), 0u);
    EXPECT_EQ(layout.deletedCount(), 0u);
    EXPECT_TRUE(table.contains(keys[2]));
    table.checkTableConsistency();
}

TEST(WTF_HashTable, ExpandAndShrink)
{
    IntTable table;
    Vector<unsigned> capacities;
    for (int i = 1; i <= 24; ++i) {
        table.add(i);
        capacities.append(table.capacity());
        table.checkTableConsistency();
    }
    EXPECT_EQ(capacities[0], 8u);
    EXPECT_EQ(capacities[4], 8u);
    EXPECT_EQ(capacities[5], 16u);
    EXPECT_EQ(capacities[10], 16u);
    EXPECT_EQ(capacities[11], 32u);
    EXPECT_EQ(capacities[22], 32u);
    EXPECT_EQ(capacities[23], 64u);

    for (int i = 24; i > 11; --i)
        table.remove<ShouldValidateKey::Yes>(i);
    EXPECT_EQ(table.capacity(), 64u);
    table.remove<ShouldValidateKey::Yes>(11);
    EXPECT_EQ(table.capacity(), 32u);
    for (int i = 10; i > 6; --i)
        table.remove<ShouldValidateKey::Yes>(i);
    EXPECT_EQ(table.capacity(), 32u);
    table.remove<ShouldValidateKey::Yes>(6);
    EXPECT_EQ(table.capacity(), 16u);
    for (int i = 5; i > 3; --i)
        table.remove<ShouldValidateKey::Yes>(i);
    EXPECT_EQ(table.capacity(), 16u);
    table.remove<ShouldValidateKey::Yes>(3);
    EXPECT_EQ(table.capacity(), 8u);
    EXPECT_TRUE(table.contains(1));
    EXPECT_TRUE(table.contains(2));
    table.checkTableConsistency();

    for (int i = 1; i <= 1000; ++i)
        table.add(i);
    table.removeIf([](int value) {
        return value > 3;
    });
    EXPECT_EQ(table.size(), 3u);
    EXPECT_EQ(table.capacity(), 8u);
    table.checkTableConsistency();
}

TEST(WTF_HashTable, SameSizeRehashClearsTombstones)
{
    constexpr unsigned capacity = 64;
    Vector<int> keys;
    for (int key = 1; key <= 47; ++key)
        keys.append(key);
    IntTable table;
    for (int key : keys)
        table.add(key);
    ASSERT_EQ(table.capacity(), capacity);

    Layout layout(table);
    std::unordered_set<int> removed;
    for (unsigned i = 0; i < 30; ++i) {
        table.remove<ShouldValidateKey::Yes>(keys[i]);
        removed.insert(keys[i]);
    }
    EXPECT_EQ(layout.deletedCount(), 30u);
    EXPECT_EQ(table.size(), 17u);
    table.checkTableConsistency();

    // A key whose home slot is EMPTY consumes it instead of reusing a DELETED slot, which triggers the rehash.
    int newKey = keys.last() + 1;
    while (!layout.statusByte(homeSlot(newKey, capacity)).isEmpty())
        ++newKey;
    table.add(newKey);
    EXPECT_EQ(table.capacity(), capacity);
    Layout rehashedLayout(table);
    EXPECT_EQ(rehashedLayout.deletedCount(), 0u);
    EXPECT_EQ(table.size(), 18u);
    EXPECT_TRUE(table.contains(newKey));
    for (int key : keys)
        EXPECT_EQ(table.contains(key), !removed.contains(key));
    table.checkTableConsistency();
}

TEST(WTF_HashTable, RemoveIfAndTakeIf)
{
    IntTable table;
    for (int i = 1; i <= 200; ++i)
        table.add(i);

    EXPECT_FALSE(table.removeIf([](int) {
        return false;
    }));
    EXPECT_EQ(table.size(), 200u);

    EXPECT_TRUE(table.removeIf([](int value) {
        return !(value % 4);
    }));
    EXPECT_EQ(table.size(), 150u);
    for (int i = 1; i <= 200; ++i)
        EXPECT_EQ(table.contains(i), !!(i % 4));
    table.checkTableConsistency();

    auto taken = table.takeIf<0>([](int value) {
        return value > 100;
    });
    EXPECT_EQ(taken.size(), 75u);
    for (int value : taken)
        EXPECT_GT(value, 100);
    EXPECT_EQ(table.size(), 75u);
    table.checkTableConsistency();

    IntTable empty;
    EXPECT_FALSE(empty.removeIf([](int) {
        return true;
    }));
}

TEST(WTF_HashTable, WeakNullKeys)
{
    fakeWeakTargetIsAlive.fill(true);
    FakeWeakKey::liveCount = 0;
    {
        FakeWeakTable table;
        for (int id = 1; id <= 10; ++id)
            table.add(FakeWeakKey { id });
        EXPECT_EQ(FakeWeakKey::liveCount, 10);

        for (int id : { 2, 4, 6 })
            fakeWeakTargetIsAlive[id] = false;
        EXPECT_EQ(table.size(), 10u);
        EXPECT_FALSE(table.isEmptyIgnoringNullReferences());

        unsigned visited = 0;
        for (auto& key : table) {
            EXPECT_TRUE(fakeWeakTargetIsAlive[key.id]);
            ++visited;
        }
        EXPECT_EQ(visited, 7u);
        table.checkTableConsistency();

        EXPECT_EQ(table.computeSize(), 7u);
        EXPECT_EQ(table.size(), 7u);
        EXPECT_EQ(FakeWeakKey::liveCount, 7);
        table.checkTableConsistency();

        for (int id = 1; id <= 10; ++id)
            fakeWeakTargetIsAlive[id] = false;
        EXPECT_TRUE(table.isEmptyIgnoringNullReferences());
        EXPECT_TRUE(table.begin() == table.end());
        table.removeWeakNullEntries();
        EXPECT_TRUE(table.isEmpty());
        EXPECT_EQ(FakeWeakKey::liveCount, 0);
    }

    fakeWeakTargetIsAlive.fill(true);
    {
        FakeWeakTable table;
        for (int id = 1; id <= 5; ++id)
            table.add(FakeWeakKey { id });
        for (int id = 1; id <= 3; ++id)
            fakeWeakTargetIsAlive[id] = false;
        table.add(FakeWeakKey { 6 });
        EXPECT_EQ(table.size(), 3u);
        EXPECT_EQ(FakeWeakKey::liveCount, 3);
        table.checkTableConsistency();
    }
    EXPECT_EQ(FakeWeakKey::liveCount, 0);

    fakeWeakTargetIsAlive.fill(true);
    {
        FakeWeakTable table;
        for (int id = 1; id <= 20; ++id)
            table.add(FakeWeakKey { id });
        EXPECT_EQ(table.capacity(), 32u);
        fakeWeakTargetIsAlive[20] = false;
        for (int id = 1; id <= 15; ++id)
            table.remove<ShouldValidateKey::Yes>(FakeWeakKey { id });
        EXPECT_LT(table.capacity(), 32u);
        EXPECT_EQ(table.size(), 4u);
        EXPECT_EQ(FakeWeakKey::liveCount, 4);
        EXPECT_TRUE(table.contains(FakeWeakKey { 16 }));
        EXPECT_FALSE(table.contains(FakeWeakKey { 20 }));
        table.checkTableConsistency();
    }
    EXPECT_EQ(FakeWeakKey::liveCount, 0);
}

TEST(WTF_HashTable, IteratorSkipsEmptyAndDeletedSlots)
{
    constexpr unsigned capacity = 64;
    Vector<int> keys = keysWithHomeSlot(0, capacity, 40);
    IntTable table;
    for (int key : keys)
        table.add(key);
    ASSERT_EQ(table.capacity(), capacity);

    std::unordered_set<int> expected(keys.begin(), keys.end());
    for (unsigned i = 0; i < keys.size(); i += 3) {
        table.remove<ShouldValidateKey::Yes>(keys[i]);
        expected.erase(keys[i]);
    }
    EXPECT_GT(Layout(table).deletedCount(), 0u);

    std::unordered_set<int> visited;
    for (auto it = table.begin(); it != table.end(); ++it)
        EXPECT_TRUE(visited.insert(*it).second);
    EXPECT_EQ(visited, expected);

    const IntTable& constTable = table;
    unsigned count = 0;
    for (int value : constTable) {
        EXPECT_TRUE(expected.contains(value));
        ++count;
    }
    EXPECT_EQ(count, expected.size());

    for (unsigned i = 0; i < 100; ++i)
        EXPECT_TRUE(expected.contains(*table.random()));

    for (int key : expected)
        table.remove<ShouldValidateKey::Yes>(key);
    EXPECT_TRUE(table.begin() == table.end());
}

TEST(WTF_HashTable, IterationOrderIsDeterministic)
{
    auto build = [] {
        IntTable table;
        for (int i = 1; i <= 300; ++i)
            table.add(i * 7);
        for (int i = 1; i <= 300; i += 5)
            table.remove<ShouldValidateKey::Yes>(i * 7);
        for (int i = 301; i <= 350; ++i)
            table.add(i * 7);
        return table;
    };
    IntTable a = build();
    IntTable b = build();
    EXPECT_EQ(iterationOrder(a), iterationOrder(b));
    EXPECT_EQ(iterationOrder(a).size(), a.size());
}

template<typename Table>
static void expectIterationMatchesStatusByteScan(const Table& table, NOESCAPE const Invocable<bool(const typename Table::ValueType&)> auto& isLive)
{
    Layout layout(table);
    Vector<unsigned> expected;
    for (unsigned i = 0; i < table.capacity(); ++i) {
        if (layout.statusByte(i).isFull() && isLive(layout.slot(i)))
            expected.append(i);
    }

    Vector<unsigned> visited;
    for (auto it = table.begin(); it != table.end(); ++it)
        visited.append(layout.indexOf(it.get()));
    EXPECT_EQ(visited, expected);

    for (size_t i = 0; i < expected.size(); ++i) {
        auto it = table.find(layout.slot(expected[i]));
        ASSERT_TRUE(it != table.end());
        EXPECT_EQ(layout.indexOf(it.get()), expected[i]);
        ++it;
        if (i + 1 < expected.size()) {
            ASSERT_TRUE(it != table.end());
            EXPECT_EQ(layout.indexOf(it.get()), expected[i + 1]);
        } else
            EXPECT_TRUE(it == table.end());
    }
}

template<typename Table>
static void expectIterationMatchesStatusByteScan(const Table& table)
{
    expectIterationMatchesStatusByteScan(table, [](const auto&) {
        return true;
    });
}

TEST(WTF_HashTable, IterationOverDenseTables)
{
    for (unsigned count : { 1u, 2u, 6u, 7u, 8u, 12u, 15u, 16u, 17u, 24u, 48u, 96u, 700u, 3000u }) {
        IntTable table;
        for (unsigned i = 1; i <= count; ++i)
            table.add(static_cast<int>(i * 5));
        EXPECT_EQ(table.size(), count);
        expectIterationMatchesStatusByteScan(table);
    }
}

TEST(WTF_HashTable, IterationOverSparseTables)
{
    for (unsigned count : { 64u, 512u, 4096u }) {
        IntTable table;
        for (unsigned i = 1; i <= count; ++i)
            table.add(static_cast<int>(i));
        for (unsigned i = 1; i <= count; ++i) {
            if (i % 5)
                table.remove<ShouldValidateKey::Yes>(static_cast<int>(i));
        }
        expectIterationMatchesStatusByteScan(table);
    }

    for (unsigned capacity : { 64u, 1024u }) {
        Vector<int> keys = keysWithHomeSlot(0, capacity, capacity / 5);
        IntTable table;
        for (int key = 100000; table.capacity() < capacity; ++key)
            table.add(key);
        for (int key : keys)
            table.add(key);
        ASSERT_EQ(table.capacity(), capacity);
        for (int key = 100000; table.size() > keys.size(); ++key)
            table.remove<ShouldValidateKey::Yes>(key);
        ASSERT_EQ(table.capacity(), capacity);

        Layout layout(table);
        unsigned run = 0;
        unsigned longestRunWithoutFullSlots = 0;
        for (unsigned i = 0; i < capacity; ++i) {
            run = layout.statusByte(i).isFull() ? 0 : run + 1;
            longestRunWithoutFullSlots = std::max(longestRunWithoutFullSlots, run);
        }
        EXPECT_GE(longestRunWithoutFullSlots, 8u);
        expectIterationMatchesStatusByteScan(table);
    }
}

TEST(WTF_HashTable, IterationReachesFullLastSlot)
{
    for (unsigned capacity : { 64u, 128u, 1024u }) {
        IntTable table;
        for (int key = 100000; table.capacity() < capacity; ++key)
            table.add(key);
        table.add(keysWithHomeSlot(capacity - 1, capacity, 1)[0]);
        ASSERT_EQ(table.capacity(), capacity);
        Layout layout(table);
        EXPECT_TRUE(layout.statusByte(capacity - 1).isFull());
        expectIterationMatchesStatusByteScan(table);

        auto it = table.find(layout.slot(capacity - 1));
        ASSERT_TRUE(it != table.end());
        ++it;
        EXPECT_TRUE(it == table.end());
    }
}

TEST(WTF_HashTable, IterationSkipsRunsOfWeakNullKeys)
{
    fakeWeakTargetIsAlive.fill(true);
    {
        FakeWeakTable table;
        for (int id = 1; id <= 90; ++id)
            table.add(FakeWeakKey { id });
        unsigned capacity = table.capacity();
        ASSERT_GE(capacity, 32u);

        Layout layout(table);
        for (unsigned i = 0; i < capacity; ++i) {
            if (!layout.statusByte(i).isFull())
                continue;
            if (!(i % 8) || i % 8 == 7 || (i >= 8 && i < 16) || i >= capacity - 8)
                fakeWeakTargetIsAlive[layout.slot(i).id] = false;
        }
        EXPECT_EQ(table.size(), 90u);
        expectIterationMatchesStatusByteScan(table, [](const FakeWeakKey& key) {
            return fakeWeakTargetIsAlive[key.id];
        });

        for (int id = 1; id <= 90; ++id)
            fakeWeakTargetIsAlive[id] = false;
        EXPECT_TRUE(table.begin() == table.end());
        EXPECT_FALSE(table.isEmpty());
    }
    fakeWeakTargetIsAlive.fill(true);
    EXPECT_EQ(FakeWeakKey::liveCount, 0);
}

TEST(WTF_HashTable, IterationOverEmptyTables)
{
    IntTable table;
    expectIterationMatchesStatusByteScan(table);

    for (int i = 1; i <= 100; ++i)
        table.add(i);
    for (int i = 1; i <= 100; ++i)
        table.remove<ShouldValidateKey::Yes>(i);
    EXPECT_TRUE(table.isEmpty());
    EXPECT_TRUE(table.begin() == table.end());
    unsigned count = 0;
    for (int value : table) {
        UNUSED_PARAM(value);
        ++count;
    }
    EXPECT_EQ(count, 0u);
    expectIterationMatchesStatusByteScan(table);

    table.add(1);
    table.clear();
    EXPECT_TRUE(table.begin() == table.end());
}

TEST(WTF_HashTable, CopyMoveAndSwap)
{
    Tracked::liveCount = 0;
    {
        TrackedTable original;
        for (int i = 1; i <= 100; ++i)
            original.add(Tracked { i });
        for (int i = 1; i <= 100; i += 2)
            original.remove<ShouldValidateKey::Yes>(i);

        TrackedTable copy(original);
        EXPECT_EQ(copy.size(), 50u);
        EXPECT_EQ(Tracked::liveCount, 100);
        for (int i = 1; i <= 100; ++i)
            EXPECT_EQ(copy.contains(i), !(i % 2));
        copy.checkTableConsistency();

        TrackedTable assigned;
        assigned.add(Tracked { 1000 });
        assigned = copy;
        EXPECT_EQ(assigned.size(), 50u);
        EXPECT_FALSE(assigned.contains(1000));
        EXPECT_EQ(Tracked::liveCount, 150);

        TrackedTable moved(WTF::move(copy));
        EXPECT_EQ(moved.size(), 50u);
        EXPECT_TRUE(copy.isEmpty());
        EXPECT_TRUE(copy.isNullStorage());
        EXPECT_EQ(Tracked::liveCount, 150);

        TrackedTable other;
        other.add(Tracked { 5000 });
        other.swap(moved);
        EXPECT_EQ(other.size(), 50u);
        EXPECT_EQ(moved.size(), 1u);
        EXPECT_TRUE(moved.contains(5000));
        EXPECT_TRUE(other.contains(2));

        moved = WTF::move(other);
        EXPECT_EQ(moved.size(), 50u);
        EXPECT_EQ(Tracked::liveCount, 150);
        moved.checkTableConsistency();
        other.checkTableConsistency();

        TrackedTable empty;
        TrackedTable emptyCopy(empty);
        EXPECT_TRUE(emptyCopy.isNullStorage());
    }
    EXPECT_EQ(Tracked::liveCount, 0);
}

TEST(WTF_HashTable, CustomTranslatorConstructsInPlace)
{
    IntTable table;
    for (int half = 1; half <= 50; ++half) {
        auto result = table.add<DoublingTranslator, ShouldValidateKey::Yes>(half, [&] {
            return half * 2;
        });
        EXPECT_TRUE(result.isNewEntry);
        EXPECT_EQ(*result.iterator, half * 2);
    }
    for (int half = 51; half <= 100; ++half) {
        auto result = table.addPassingHashCode<DoublingTranslator, ShouldValidateKey::Yes>(half, [&] {
            return half * 2;
        });
        EXPECT_TRUE(result.isNewEntry);
    }
    EXPECT_FALSE((table.add<DoublingTranslator, ShouldValidateKey::Yes>(10, [] {
        return 20;
    }).isNewEntry));
    EXPECT_FALSE((table.addPassingHashCode<DoublingTranslator, ShouldValidateKey::Yes>(60, [] {
        return 120;
    }).isNewEntry));
    EXPECT_EQ(table.size(), 100u);
    for (int half = 1; half <= 100; ++half) {
        EXPECT_TRUE(table.contains(half * 2));
        EXPECT_TRUE((table.contains<DoublingTranslator, ShouldValidateKey::Yes>(half)));
    }
    table.checkTableConsistency();
}

TEST(WTF_HashTable, RandomOperationsStayConsistent)
{
    for (int keyRange : { 10, 100, 3000 }) {
        std::mt19937 random(keyRange);
        std::uniform_int_distribution<int> keys(1, keyRange);
        std::uniform_int_distribution<int> operations(0, 99);
        IntTable table;
        std::unordered_set<int> reference;
        for (unsigned step = 0; step < 30000; ++step) {
            int key = keys(random);
            int operation = operations(random);
            if (operation < 50)
                EXPECT_EQ(table.add(key).isNewEntry, reference.insert(key).second);
            else if (operation < 90) {
                table.remove<ShouldValidateKey::Yes>(key);
                reference.erase(key);
            } else if (operation < 99)
                EXPECT_EQ(table.contains(key), reference.contains(key));
            else {
                int modulus = 2 + key % 5;
                table.removeIf([&](int value) {
                    return !(value % modulus);
                });
                std::erase_if(reference, [&](int value) {
                    return !(value % modulus);
                });
            }
            ASSERT_EQ(table.size(), reference.size());
            if (!(step % 499))
                table.checkTableConsistency();
        }
        table.checkTableConsistency();
        std::unordered_set<int> contents;
        for (int value : table)
            contents.insert(value);
        EXPECT_EQ(contents, reference);
    }
}

struct IntHashWithHashInValue : DefaultHash<int> {
    static constexpr bool hasHashInValue = true;
};

TEST(WTF_HashTable, RobinHoodTableBasicOperations)
{
    using RobinHoodIntTable = WTF::MemoryCompactRobinHoodHashTableTraits::TableType<int, int, TestIdentityExtractor, IntHashWithHashInValue, HashTraits<int>, HashTraits<int>, WTF::HashTableMalloc>;
    RobinHoodIntTable table;
    for (int i = 1; i <= 100; ++i)
        EXPECT_TRUE(table.add<ShouldValidateKey::Yes>(i).isNewEntry);
    EXPECT_FALSE(table.add<ShouldValidateKey::Yes>(50).isNewEntry);
    table.remove(table.find<ShouldValidateKey::Yes>(50));

    int sum = 0;
    unsigned count = 0;
    for (int value : table) {
        sum += value;
        ++count;
    }
    EXPECT_EQ(count, 99u);
    EXPECT_EQ(sum, 5050 - 50);
    EXPECT_TRUE(table.contains<ShouldValidateKey::Yes>(99));
    EXPECT_FALSE(table.contains<ShouldValidateKey::Yes>(50));
}

TEST(WTF_HashTable, SentinelValuesAreOrdinaryKeys)
{
    IntTable table;
    EXPECT_TRUE(table.add(0).isNewEntry);
    EXPECT_TRUE(table.add(-1).isNewEntry);
    EXPECT_FALSE(table.add(0).isNewEntry);
    EXPECT_EQ(table.size(), 2u);
    EXPECT_TRUE(table.contains(0));
    EXPECT_TRUE(table.contains(-1));
    table.remove<ShouldValidateKey::Yes>(0);
    EXPECT_FALSE(table.contains(0));
    EXPECT_TRUE(table.contains(-1));
    for (int i = 1; i < 100; ++i)
        table.add(i);
    EXPECT_TRUE(table.contains(-1));
    IntTable copy = table;
    EXPECT_EQ(copy.size(), 100u);
    EXPECT_TRUE(copy.contains(-1));

    using PointerTable = HashTable<int*, int*, TestIdentityExtractor, DefaultHash<int*>, HashTraits<int*>, HashTraits<int*>>;
    std::array<int, 100> targets { };
    int* null = nullptr;
    PointerTable pointers;
    EXPECT_TRUE(pointers.add(null).isNewEntry);
    EXPECT_FALSE(pointers.add(null).isNewEntry);
    EXPECT_TRUE(pointers.contains(null));
    for (auto& target : targets)
        pointers.add(&target);
    EXPECT_EQ(pointers.size(), targets.size() + 1);
    EXPECT_TRUE(pointers.contains(null));
    pointers.remove<ShouldValidateKey::Yes>(null);
    EXPECT_FALSE(pointers.contains(null));
    EXPECT_EQ(pointers.size(), targets.size());
}

} // namespace TestWebKitAPI
