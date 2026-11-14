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

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Compiler.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/HashTraits.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/VectorTraits.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
class OrderedHashTable;

template<typename TableType, typename ValueType>
class OrderedHashTableIterator;

template<typename TableType, typename ValueType>
class OrderedHashTableConstIterator;

template<typename TableType, typename ValueType>
class OrderedHashTableReverseIterator;

template<typename TableType, typename ValueType>
class OrderedHashTableConstReverseIterator;

template<typename T>
struct OrderedHashTableEntryTraits {
    static constexpr bool canMoveWithMemcpy = VectorTraits<T>::canMoveWithMemcpy;
    static constexpr bool canCopyWithMemcpy = VectorTraits<T>::canCopyWithMemcpy;
};

template<typename Key, typename Value>
struct OrderedHashTableEntryTraits<KeyValuePair<Key, Value>> {
    static constexpr bool canMoveWithMemcpy = VectorTraits<Key>::canMoveWithMemcpy && VectorTraits<Value>::canMoveWithMemcpy;
    static constexpr bool canCopyWithMemcpy = VectorTraits<Key>::canCopyWithMemcpy && VectorTraits<Value>::canCopyWithMemcpy;
};

template<typename T>
struct OrderedHashTableKeyIsCheapToCompare : std::bool_constant<std::is_scalar_v<T>> { };

template<typename T, typename U, typename V>
struct OrderedHashTableKeyIsCheapToCompare<Ref<T, U, V>> : std::true_type { };

template<typename T, typename U, typename V>
struct OrderedHashTableKeyIsCheapToCompare<RefPtr<T, U, V>> : std::true_type { };

template<typename T, typename U>
struct OrderedHashTableKeyIsCheapToCompare<CheckedPtr<T, U>> : std::true_type { };

template<typename T, typename U>
struct OrderedHashTableKeyIsCheapToCompare<CheckedRef<T, U>> : std::true_type { };

// Every table with no allocation points here, so lookups need no null check: the probe reads one
// empty bucket and stops. It is never written, because adding to a table with no entry capacity
// allocates first.
alignas(uint64_t) inline constexpr uint32_t orderedHashTableEmptyBuckets[1] { };

// Entries live in a dense array in insertion order, and the bucket array maps a hash to an entry
// index. Which buckets are empty or deleted, and which entries are deleted, is tracked outside the
// entries, so a key type needs no reserved empty or deleted value.
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
class OrderedHashTable final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(OrderedHashTable);
public:
    using KeyType = Key;
    using ValueType = Value;

    struct WTF_VIEW_TYPE AddResult {
        using IteratorType = OrderedHashTableIterator<OrderedHashTable, ValueType>;
        IteratorType iterator;
        bool isNewEntry;
    };

    using iterator = OrderedHashTableIterator<OrderedHashTable, ValueType>;
    using const_iterator = OrderedHashTableConstIterator<OrderedHashTable, ValueType>;
    using reverse_iterator = OrderedHashTableReverseIterator<OrderedHashTable, ValueType>;
    using const_reverse_iterator = OrderedHashTableConstReverseIterator<OrderedHashTable, ValueType>;

    OrderedHashTable() = default;

    OrderedHashTable(const OrderedHashTable& other)
    {
        if (!other.m_liveCount)
            return;
        allocateTable(bucketCountForKeyCount(other.m_liveCount));

        // With no deleted entries, every entry keeps its index, so the bucket array is still valid.
        if (m_bucketCount == other.m_bucketCount && !other.hasDeletedEntries()) {
            if constexpr (OrderedHashTableEntryTraits<ValueType>::canCopyWithMemcpy)
                memcpy(static_cast<void*>(m_entries), static_cast<const void*>(other.m_entries), other.m_entriesLength * sizeof(ValueType)); // NOLINT
            else
                std::uninitialized_copy_n(other.m_entries, other.m_entriesLength, m_entries);
            std::copy_n(other.m_buckets, m_bucketCount, m_buckets);
            m_entriesLength = other.m_entriesLength;
            m_liveCount = other.m_liveCount;
            return;
        }

        other.forEachLiveRun(other.m_entriesLength, [&](uint32_t start, uint32_t end) {
            for (uint32_t i = start; i < end; ++i) {
                new (NotNull, std::addressof(m_entries[m_entriesLength])) ValueType(other.m_entries[i]);
                insertIntoFreshBuckets(m_entriesLength);
                ++m_entriesLength;
            }
        });
        m_liveCount = m_entriesLength;
    }

    OrderedHashTable(OrderedHashTable&& other) noexcept
    {
        swap(other);
    }

    OrderedHashTable& operator=(const OrderedHashTable& other)
    {
        if (this != &other) {
            OrderedHashTable tmp(other);
            swap(tmp);
        }
        return *this;
    }

    OrderedHashTable& operator=(OrderedHashTable&& other) noexcept
    {
        OrderedHashTable tmp(WTF::move(other));
        swap(tmp);
        return *this;
    }

    ~OrderedHashTable()
    {
        deallocateAll();
    }

    void swap(OrderedHashTable& other)
    {
        std::swap(m_buckets, other.m_buckets);
        std::swap(m_entries, other.m_entries);
        std::swap(m_bucketCount, other.m_bucketCount);
        std::swap(m_entriesCapacity, other.m_entriesCapacity);
        std::swap(m_entriesLength, other.m_entriesLength);
        std::swap(m_liveCount, other.m_liveCount);
    }

    unsigned size() const { return m_liveCount; }
    unsigned capacity() const { return m_entriesCapacity; }
    bool isEmpty() const { return !m_liveCount; }

    iterator begin() LIFETIME_BOUND { return iterator(this, 0); }
    iterator end() LIFETIME_BOUND { return iterator(this, m_entriesLength); }
    const_iterator begin() const LIFETIME_BOUND { return const_iterator(this, 0); }
    const_iterator end() const LIFETIME_BOUND { return const_iterator(this, m_entriesLength); }

    reverse_iterator rbegin() LIFETIME_BOUND { return reverse_iterator(this, m_entriesLength); }
    reverse_iterator rend() LIFETIME_BOUND { return reverse_iterator(this, 0); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return const_reverse_iterator(this, m_entriesLength); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return const_reverse_iterator(this, 0); }

    ValueType* lookup(const KeyType& key) { return lookup<KeyTranslator>(key); }
    const ValueType* lookup(const KeyType& key) const { return lookup<KeyTranslator>(key); }

    template<typename HashTranslator, typename T>
    ValueType* lookup(const T& key)
    {
        uint32_t index = findBucket<HashTranslator>(key).index;
        if (index == notFound)
            return nullptr;
        return &m_entries[index];
    }

    template<typename HashTranslator, typename T>
    const ValueType* lookup(const T& key) const
    {
        return const_cast<OrderedHashTable*>(this)->template lookup<HashTranslator>(key);
    }

    iterator find(const KeyType& key) LIFETIME_BOUND { return find<KeyTranslator>(key); }
    const_iterator find(const KeyType& key) const LIFETIME_BOUND { return find<KeyTranslator>(key); }

    template<typename HashTranslator, typename T>
    iterator find(const T& key) LIFETIME_BOUND
    {
        auto [bucket, index] = findBucket<HashTranslator>(key);
        if (index == notFound)
            return end();
        return iterator(this, index, bucket);
    }

    template<typename HashTranslator, typename T>
    const_iterator find(const T& key) const LIFETIME_BOUND
    {
        return const_cast<OrderedHashTable*>(this)->template find<HashTranslator>(key);
    }

    bool contains(const KeyType& key) const { return contains<KeyTranslator>(key); }

    template<typename HashTranslator, typename T>
    bool contains(const T& key) const
    {
        return findBucket<HashTranslator>(key).index != notFound;
    }

    AddResult add(const KeyType& key, NOESCAPE const auto& valueFunctor)
    {
        return internalAdd<KeyTranslator>(key, [&](ValueType* location) ALWAYS_INLINE_LAMBDA {
            new (NotNull, location) ValueType(valueFunctor());
        });
    }

    AddResult add(KeyType&& key, NOESCAPE const auto& valueFunctor)
    {
        return internalAdd<KeyTranslator>(key, [&](ValueType* location) ALWAYS_INLINE_LAMBDA {
            new (NotNull, location) ValueType(valueFunctor());
        });
    }

    // HashTranslator::translate() receives uninitialized storage and must construct into it.
    template<typename HashTranslator, typename K>
    AddResult add(K&& key, NOESCAPE const auto& valueFunctor)
    {
        return internalAdd<HashTranslator>(key, [&](ValueType* location) ALWAYS_INLINE_LAMBDA {
            HashTranslator::translate(location, std::forward<K>(key), valueFunctor);
        });
    }

    void remove(iterator it)
    {
        ASSERT(it.m_table == this);
        removeEntryAt(bucketHoldingEntry(it.m_index, it.m_bucket), it.m_index);
    }

    void remove(const_iterator it)
    {
        ASSERT(it.m_table == this);
        removeEntryAt(bucketHoldingEntry(it.m_index, it.m_bucket), it.m_index);
    }

    bool remove(const KeyType& key)
    {
        auto [bucket, index] = findBucket<KeyTranslator>(key);
        if (index == notFound)
            return false;
        removeEntryAt(bucket, index);
        return true;
    }

    ValueType take(const_iterator it)
    {
        ASSERT(it.m_table == this);
        uint32_t index = it.m_index;
        uint32_t bucket = bucketHoldingEntry(index, it.m_bucket);
        ValueType result = WTF::move(m_entries[index]);
        removeEntryAt(bucket, index);
        return result;
    }

    bool removeIf(NOESCAPE const auto& functor)
    {
        if (hasDeletedEntries())
            return removeIfOneAtATime(functor);

        // Every deleted bit set from here on is a pending removal. Pending entries stay constructed
        // and reachable from their buckets until the functor is done, and none is destroyed until
        // no bucket refers to any of them.
        uint32_t removedCount = 0;
        for (uint32_t i = 0; i < m_entriesLength; ++i) {
            if (!functor(m_entries[i]))
                continue;
            markEntryDeleted(i);
            ++removedCount;
        }
        if (!removedCount)
            return false;

        m_liveCount -= removedCount;
        if (removedCount <= m_liveCount) {
            forEachRun<true>(m_entriesLength, [&](uint32_t start, uint32_t end) {
                for (uint32_t i = start; i < end; ++i)
                    m_buckets[probeForBucketHolding(i)] = deletedBucket;
            });
        } else {
            clearBuckets();
            forEachLiveRun(m_entriesLength, [&](uint32_t start, uint32_t end) {
                for (uint32_t i = start; i < end; ++i)
                    insertIntoFreshBuckets(i);
            });
        }
        if constexpr (!std::is_trivially_destructible_v<ValueType>) {
            forEachRun<true>(m_entriesLength, [&](uint32_t start, uint32_t end) {
                std::destroy(m_entries + start, m_entries + end);
            });
        }
        shrinkIfNeeded();
        return true;
    }

    void clear()
    {
        deallocateAll();
        resetToEmpty();
    }

    void reserveInitialCapacity(unsigned keyCount)
    {
        ASSERT(isEmpty());
        ASSERT(!m_entriesLength);
        if (!keyCount)
            return;
        uint32_t newBucketCount = bucketCountForKeyCount(keyCount);
        if (newBucketCount > m_bucketCount) {
            deallocateAll();
            allocateTable(newBucketCount);
        }
    }

    static bool isValidKey(const KeyType& key)
    {
        if constexpr (HashFunctions::safeToCompareToEmptyOrDeleted)
            return true;
        else {
            // Any key can be stored, but some hash functions cannot hash their traits' empty or deleted value (StringHash on a null String).
            return !KeyTraits::isDeletedValue(key) && !isHashTraitsEmptyValue<KeyTraits>(key);
        }
    }

    // Internal accessors used by iterators
    ValueType* entries() { return m_entries; }
    const ValueType* entries() const { return m_entries; }
    uint32_t entriesLength() const { return m_entriesLength; }

    bool isDeletedEntryAt(uint32_t index) const
    {
        ASSERT(index < m_entriesLength);
        return deletedEntryBits()[index / bitsPerWord] & (1ULL << (index % bitsPerWord));
    }

    // The smallest live index at or after index, or entriesLength() if there is none.
    uint32_t liveIndexAtOrAfter(uint32_t index) const
    {
        ASSERT(index <= m_entriesLength);
        if (!hasDeletedEntries())
            return index;
        return findNextBit<false>(index, m_entriesLength);
    }

    // The largest end <= the given end such that end is 0 or entry end - 1 is live.
    uint32_t liveEndAtOrBefore(uint32_t end) const
    {
        ASSERT(end <= m_entriesLength);
        if (!end || !hasDeletedEntries())
            return end;
        uint32_t last = end - 1;
        uint32_t wordIndex = last / bitsPerWord;
        uint64_t liveBits = ~deletedEntryBits()[wordIndex] & (~0ULL >> (bitsPerWord - 1 - last % bitsPerWord));
        while (!liveBits) {
            if (!wordIndex)
                return 0;
            --wordIndex;
            liveBits = ~deletedEntryBits()[wordIndex];
        }
        return wordIndex * bitsPerWord + (bitsPerWord - clz(liveBits));
    }

private:
    // A bucket's low indexBits hold its entry's index plus bucketIndexBias, so that the two values
    // below it can mark a bucket empty or deleted, and so that an empty bucket array is all zero
    // bits. For keys that are costly to compare, the remaining high bits hold a tag computed from
    // the key's hash, which rejects most probes that land on another key's bucket without reading
    // that key's entry. For keys that compare in a register, reading the entry is about as cheap
    // as checking a tag, so they get no tag and the full 32 bits of index.
    static constexpr bool usesTags = !OrderedHashTableKeyIsCheapToCompare<KeyType>::value;
    static constexpr uint32_t emptyBucket = 0;
    static constexpr uint32_t deletedBucket = 1;
    static constexpr uint32_t bucketIndexBias = 2;
    static constexpr uint32_t indexBits = usesTags ? 27 : 32;
    static constexpr uint32_t indexMask = usesTags ? (1U << indexBits) - 1 : UINT32_MAX;
    static constexpr uint32_t tagMask = ~indexMask;

    // Unbiasing a bucket that is empty, deleted, or carries another key's tag gives a value above
    // this, so one unsigned comparison tells whether a bucket may hold the key being probed for.
    static constexpr uint32_t maxEntryIndex = indexMask - bucketIndexBias;

    static constexpr uint32_t notFound = UINT32_MAX;
    static constexpr unsigned initialBucketCount = 8;
    static constexpr uint32_t maxBucketCount = usesTags ? 1U << indexBits : 1U << 30;
    static constexpr uint32_t bitsPerWord = 64;

    static_assert(alignof(ValueType) <= alignof(std::max_align_t));

    struct KeyTranslator {
        static unsigned hash(const KeyType& key) { return HashFunctions::hash(key); }
        static bool equal(const KeyType& a, const KeyType& b) { return HashFunctions::equal(a, b); }
    };

    struct BucketAndIndex {
        uint32_t bucket;
        uint32_t index;
    };

    // The multiply makes every tag bit depend on every hash bit. Taking the high hash bits directly
    // would give StringHash keys a constant tag, since string hashes leave those bits zero. The
    // lowest tag bit is always set, so an empty or deleted bucket never matches a tag.
    static uint32_t tagForHash(uint32_t hash)
    {
        if constexpr (usesTags)
            return ((hash * 0x9E3779B1U) & tagMask) | (indexMask + 1);
        else
            return 0;
    }
    static uint32_t bucketValue(uint32_t index, uint32_t tag) { return (index + bucketIndexBias) | tag; }

    static constexpr uint32_t entriesCapacityFromBucketCount(uint32_t bucketCount)
    {
        // Same load factor policy as WTF::HashTable: larger tables run emptier so probe sequences
        // stay short once the bucket array no longer fits in L1/L2.
        constexpr uint32_t maxSmallTableCapacity = 1024;
        if (bucketCount <= maxSmallTableCapacity)
            return (bucketCount * 3) / 4;
        return bucketCount / 2;
    }
    static_assert(entriesCapacityFromBucketCount(maxBucketCount) - 1 + bucketIndexBias <= indexMask);

    static uint32_t bucketCountForKeyCount(uint32_t keyCount)
    {
        uint32_t bucketCount = initialBucketCount;
        while (entriesCapacityFromBucketCount(bucketCount) < keyCount) {
            RELEASE_ASSERT(bucketCount < maxBucketCount);
            bucketCount <<= 1;
        }
        return bucketCount;
    }

    static constexpr uint32_t deletedEntryWordCount(uint32_t entriesCapacity)
    {
        return (entriesCapacity + bitsPerWord - 1) / bitsPerWord;
    }

    // One allocation holds the buckets, then the deleted entry bits, then the entries.
    static size_t entriesOffset(uint32_t bucketCount, uint32_t entriesCapacity)
    {
        size_t metadataSize = static_cast<size_t>(bucketCount) * sizeof(uint32_t) + static_cast<size_t>(deletedEntryWordCount(entriesCapacity)) * sizeof(uint64_t);
        return roundUpToMultipleOf<alignof(ValueType)>(metadataSize);
    }

    bool hasDeletedEntries() const { return m_liveCount != m_entriesLength; }

    uint64_t* deletedEntryBits() const { return reinterpret_cast<uint64_t*>(m_buckets + m_bucketCount); }

    void markEntryDeleted(uint32_t index)
    {
        deletedEntryBits()[index / bitsPerWord] |= 1ULL << (index % bitsPerWord);
    }

    // The smallest index in [from, length) whose deleted bit equals deleted, or length.
    template<bool deleted>
    uint32_t findNextBit(uint32_t from, uint32_t length) const
    {
        if (from >= length)
            return length;
        const uint64_t* bits = deletedEntryBits();
        uint32_t wordIndex = from / bitsPerWord;
        uint64_t word = (deleted ? bits[wordIndex] : ~bits[wordIndex]) & (~0ULL << (from % bitsPerWord));
        while (!word) {
            ++wordIndex;
            if (wordIndex * bitsPerWord >= length)
                return length;
            word = deleted ? bits[wordIndex] : ~bits[wordIndex];
        }
        return std::min(wordIndex * bitsPerWord + ctz(word), length);
    }

    // Calls functor(start, end) for each maximal run of indices in [0, length), in order, whose
    // deleted bit equals deleted.
    template<bool deleted>
    void forEachRun(uint32_t length, NOESCAPE const auto& functor) const
    {
        for (uint32_t start = findNextBit<deleted>(0, length); start < length;) {
            uint32_t end = findNextBit<!deleted>(start, length);
            functor(start, end);
            start = findNextBit<deleted>(end, length);
        }
    }

    void forEachLiveRun(uint32_t length, NOESCAPE const auto& functor) const
    {
        if (!hasDeletedEntries()) {
            if (length)
                functor(0, length);
            return;
        }
        forEachRun<false>(length, functor);
    }

    // Moves the live entries of from[0, length) to the front of to, in order, and returns how many
    // were moved. from and to may be the same array.
    uint32_t moveLiveEntries(ValueType* to, ValueType* from, uint32_t length) const
    {
        uint32_t count = 0;
        forEachLiveRun(length, [&](uint32_t start, uint32_t end) {
            uint32_t runLength = end - start;
            ValueType* destination = to + count;
            ValueType* source = from + start;
            count += runLength;
            if (destination == source)
                return;
            if constexpr (OrderedHashTableEntryTraits<ValueType>::canMoveWithMemcpy)
                memmove(static_cast<void*>(destination), static_cast<void*>(source), runLength * sizeof(ValueType)); // NOLINT
            else {
                for (uint32_t i = 0; i < runLength; ++i) {
                    new (NotNull, destination + i) ValueType(WTF::move(source[i]));
                    std::destroy_at(source + i);
                }
            }
        });
        return count;
    }

    void clearBuckets()
    {
        zeroSpan(std::span { m_buckets, m_bucketCount });
    }

    void clearBucketsAndDeletedEntryBits()
    {
        clearBuckets();
        zeroSpan(std::span { deletedEntryBits(), deletedEntryWordCount(m_entriesCapacity) });
    }

    template<typename HashTranslator, typename T>
    ALWAYS_INLINE BucketAndIndex findBucket(const T& key) const
    {
        uint32_t hash = HashTranslator::hash(key);
        uint32_t tag = tagForHash(hash);
        uint32_t mask = m_bucketCount - 1;
        uint32_t i = hash & mask;
        for (uint32_t probeCount = 1;; ++probeCount) {
            uint32_t bucket = m_buckets[i];
            uint32_t index = (bucket ^ tag) - bucketIndexBias;
            if (index <= maxEntryIndex) {
                if (HashTranslator::equal(Extractor::extract(m_entries[index]), key))
                    return { i, index };
            } else if (bucket == emptyBucket)
                return { notFound, notFound };
            i = (i + probeCount) & mask;
        }
    }

    // The hint is a bucket recorded when the iterator was made. It is used only after checking that
    // it still refers to this entry.
    uint32_t bucketHoldingEntry(uint32_t index, uint32_t hint) const
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(index < m_entriesLength);
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isDeletedEntryAt(index));
        if (hint < m_bucketCount && (m_buckets[hint] & indexMask) == index + bucketIndexBias)
            return hint;
        return probeForBucketHolding(index);
    }

    // The hash table has no way to reach a bucket from its entry other than probing for it again,
    // so the entry's key must still be intact.
    uint32_t probeForBucketHolding(uint32_t index) const
    {
        uint32_t hash = HashFunctions::hash(Extractor::extract(m_entries[index]));
        uint32_t target = bucketValue(index, tagForHash(hash));
        uint32_t mask = m_bucketCount - 1;
        uint32_t i = hash & mask;
        for (uint32_t probeCount = 1; m_buckets[i] != target; ++probeCount) {
            RELEASE_ASSERT(m_buckets[i] != emptyBucket);
            i = (i + probeCount) & mask;
        }
        return i;
    }

    // Assumes no key equals the one being inserted and no deleted bucket appears in the probe
    // path, which holds right after the bucket array has been cleared.
    void insertIntoFreshBuckets(uint32_t entryIndex)
    {
        uint32_t hash = HashFunctions::hash(Extractor::extract(m_entries[entryIndex]));
        m_buckets[probeForEmpty(hash)] = bucketValue(entryIndex, tagForHash(hash));
    }

    uint32_t probeForEmpty(uint32_t hash) const
    {
        uint32_t mask = m_bucketCount - 1;
        uint32_t i = hash & mask;
        for (uint32_t probeCount = 1; m_buckets[i] != emptyBucket; ++probeCount)
            i = (i + probeCount) & mask;
        return i;
    }

    void allocateTable(uint32_t bucketCount)
    {
        RELEASE_ASSERT(bucketCount <= maxBucketCount);
        ASSERT(bucketCount >= initialBucketCount);
        ASSERT(hasOneBitSet(bucketCount));
        uint32_t entriesCapacity = entriesCapacityFromBucketCount(bucketCount);
        size_t offset = entriesOffset(bucketCount, entriesCapacity);
        auto* table = static_cast<uint8_t*>(Malloc::malloc(Checked<size_t>(offset) + Checked<size_t>(entriesCapacity) * sizeof(ValueType)));
        m_buckets = reinterpret_cast<uint32_t*>(table);
        m_entries = reinterpret_cast<ValueType*>(table + offset);
        m_bucketCount = bucketCount;
        m_entriesCapacity = entriesCapacity;
        clearBucketsAndDeletedEntryBits();
    }

    void resetToEmpty()
    {
        m_buckets = const_cast<uint32_t*>(orderedHashTableEmptyBuckets);
        m_entries = nullptr;
        m_bucketCount = 1;
        m_entriesCapacity = 0;
        m_entriesLength = 0;
        m_liveCount = 0;
    }

    void deallocateAll()
    {
        if (!m_entriesCapacity)
            return;
        if constexpr (!std::is_trivially_destructible_v<ValueType>) {
            forEachLiveRun(m_entriesLength, [&](uint32_t start, uint32_t end) {
                std::destroy(m_entries + start, m_entries + end);
            });
        }
        Malloc::free(m_buckets);
    }

    template<typename HashTranslator, typename T>
    AddResult internalAdd(const T& key, NOESCAPE const auto& constructEntry)
    {
        uint32_t hash = HashTranslator::hash(key);
        uint32_t tag = tagForHash(hash);
        uint32_t mask = m_bucketCount - 1;
        uint32_t i = hash & mask;
        uint32_t insertBucket = notFound;
        for (uint32_t probeCount = 1;; ++probeCount) {
            uint32_t bucket = m_buckets[i];
            uint32_t index = (bucket ^ tag) - bucketIndexBias;
            if (index <= maxEntryIndex) {
                if (HashTranslator::equal(Extractor::extract(m_entries[index]), key))
                    return { iterator(this, index, i), false };
            } else if (bucket <= deletedBucket) {
                if (insertBucket == notFound)
                    insertBucket = i;
                if (bucket == emptyBucket)
                    break;
            }
            i = (i + probeCount) & mask;
        }

        if (m_entriesLength == m_entriesCapacity) {
            rehashForAdd();
            insertBucket = probeForEmpty(hash);
        }

        uint32_t newIndex = m_entriesLength;
        constructEntry(std::addressof(m_entries[newIndex]));
        m_buckets[insertBucket] = bucketValue(newIndex, tag);
        ++m_entriesLength;
        ++m_liveCount;
        return { iterator(this, newIndex, insertBucket), true };
    }

    void destroyEntryAt(uint32_t index)
    {
        markEntryDeleted(index);
        --m_liveCount;
        std::destroy_at(std::addressof(m_entries[index]));
    }

    void removeEntryAt(uint32_t bucket, uint32_t index)
    {
        ASSERT(index < m_entriesLength);
        ASSERT((m_buckets[bucket] & indexMask) == index + bucketIndexBias);
        m_buckets[bucket] = deletedBucket;
        destroyEntryAt(index);
        shrinkIfNeeded();
    }

    bool removeIfOneAtATime(NOESCAPE const auto& functor)
    {
        // Shrinking reallocates m_entries and resets m_entriesLength, so it has to wait until the
        // loop is done.
        bool changed = false;
        for (uint32_t i = 0; i < m_entriesLength; ++i) {
            if (isDeletedEntryAt(i) || !functor(m_entries[i]))
                continue;
            m_buckets[probeForBucketHolding(i)] = deletedBucket;
            destroyEntryAt(i);
            changed = true;
        }
        if (changed)
            shrinkIfNeeded();
        return changed;
    }

    void rehashForAdd()
    {
        // Entries array is full. Grow if mostly live, otherwise compact in place.
        if (m_liveCount >= m_entriesCapacity * 3 / 4)
            rehash(m_entriesCapacity ? m_bucketCount << 1 : initialBucketCount);
        else
            compactInPlace();
    }

    void shrinkIfNeeded()
    {
        if (m_bucketCount <= initialBucketCount)
            return;
        if (m_liveCount >= m_entriesLength / 4)
            return;
        uint32_t bucketCount = std::max<uint32_t>(bucketCountForKeyCount(m_liveCount * 2), initialBucketCount);
        rehash(std::min(bucketCount, m_bucketCount >> 1));
    }

    void compactInPlace()
    {
        m_entriesLength = moveLiveEntries(m_entries, m_entries, m_entriesLength);
        ASSERT(m_entriesLength == m_liveCount);
        clearBucketsAndDeletedEntryBits();
        for (uint32_t i = 0; i < m_entriesLength; ++i)
            insertIntoFreshBuckets(i);
    }

    void rehash(uint32_t newBucketCount)
    {
        uint32_t* oldTable = m_buckets;
        bool oldTableIsAllocated = m_entriesCapacity;
        ValueType* oldEntries = m_entries;
        uint32_t oldLength = m_entriesLength;

        // moveLiveEntries() reads the old deleted bits, so the new table is installed only after.
        OrderedHashTable newTable;
        newTable.allocateTable(newBucketCount);
        newTable.m_entriesLength = moveLiveEntries(newTable.m_entries, oldEntries, oldLength);
        newTable.m_liveCount = newTable.m_entriesLength;
        ASSERT(newTable.m_liveCount == m_liveCount);
        for (uint32_t i = 0; i < newTable.m_entriesLength; ++i)
            newTable.insertIntoFreshBuckets(i);

        resetToEmpty();
        swap(newTable);
        if (oldTableIsAllocated)
            Malloc::free(oldTable);
    }

    uint32_t* m_buckets { const_cast<uint32_t*>(orderedHashTableEmptyBuckets) };
    ValueType* m_entries { nullptr };
    uint32_t m_bucketCount { 1 };
    uint32_t m_entriesCapacity { 0 };
    uint32_t m_entriesLength { 0 };
    uint32_t m_liveCount { 0 };
};

template<typename TableType, typename ValueType>
class WTF_VIEW_TYPE OrderedHashTableIterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = ValueType;
    using difference_type = ptrdiff_t;
    using pointer = ValueType*;
    using reference = ValueType&;

    OrderedHashTableIterator() = default;

    ValueType* get() const
    {
        ASSERT(m_table);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index < m_table->entriesLength());
        return &m_table->entries()[m_index];
    }

    ValueType& operator*() const { return *get(); }
    ValueType* operator->() const { return get(); }

    OrderedHashTableIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT(m_index < m_table->entriesLength());
        m_index = m_table->liveIndexAtOrAfter(m_index + 1);
        return *this;
    }

    OrderedHashTableIterator operator++(int)
    {
        auto result = *this;
        ++*this;
        return result;
    }

    OrderedHashTableIterator& operator--()
    {
        ASSERT(m_table);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index > 0);
        uint32_t end = m_table->liveEndAtOrBefore(m_index);
        m_index = end ? end - 1 : 0;
        ASSERT(!m_table->isDeletedEntryAt(m_index));
        return *this;
    }

    OrderedHashTableIterator operator--(int)
    {
        auto result = *this;
        --*this;
        return result;
    }

    friend bool operator==(const OrderedHashTableIterator& a, const OrderedHashTableIterator& b)
    {
        return a.m_table == b.m_table && a.m_index == b.m_index;
    }

private:
    friend TableType;
    template<typename, typename> friend class OrderedHashTableConstIterator;
    // Give OrderedHashMap/OrderedHashSet access to m_index for remove(iterator)
    template<typename, typename, typename, typename, typename, typename, typename> friend class OrderedHashTable;

    OrderedHashTableIterator(TableType* table, uint32_t index)
        : m_table(table)
        , m_index(table->liveIndexAtOrAfter(index))
    {
    }

    OrderedHashTableIterator(TableType* table, uint32_t index, uint32_t bucket)
        : m_table(table)
        , m_index(index)
        , m_bucket(bucket)
    {
    }

    TableType* m_table { nullptr };
    uint32_t m_index { 0 };
    uint32_t m_bucket { UINT32_MAX };
};

template<typename TableType, typename ValueType>
class WTF_VIEW_TYPE OrderedHashTableConstIterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = ValueType;
    using difference_type = ptrdiff_t;
    using pointer = const ValueType*;
    using reference = const ValueType&;

    OrderedHashTableConstIterator() = default;

    OrderedHashTableConstIterator(const OrderedHashTableIterator<std::remove_const_t<TableType>, ValueType>& other)
        : m_table(other.m_table)
        , m_index(other.m_index)
        , m_bucket(other.m_bucket)
    {
    }

    const ValueType* get() const
    {
        ASSERT(m_table);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index < m_table->entriesLength());
        return &m_table->entries()[m_index];
    }

    const ValueType& operator*() const { return *get(); }
    const ValueType* operator->() const { return get(); }

    OrderedHashTableConstIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT(m_index < m_table->entriesLength());
        m_index = m_table->liveIndexAtOrAfter(m_index + 1);
        return *this;
    }

    OrderedHashTableConstIterator operator++(int)
    {
        auto result = *this;
        ++*this;
        return result;
    }

    OrderedHashTableConstIterator& operator--()
    {
        ASSERT(m_table);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index > 0);
        uint32_t end = m_table->liveEndAtOrBefore(m_index);
        m_index = end ? end - 1 : 0;
        ASSERT(!m_table->isDeletedEntryAt(m_index));
        return *this;
    }

    OrderedHashTableConstIterator operator--(int)
    {
        auto result = *this;
        --*this;
        return result;
    }

    friend bool operator==(const OrderedHashTableConstIterator& a, const OrderedHashTableConstIterator& b)
    {
        return a.m_table == b.m_table && a.m_index == b.m_index;
    }

private:
    friend TableType;
    template<typename, typename, typename, typename, typename, typename, typename> friend class OrderedHashTable;

    OrderedHashTableConstIterator(const TableType* table, uint32_t index)
        : m_table(table)
        , m_index(table->liveIndexAtOrAfter(index))
    {
    }

    const TableType* m_table { nullptr };
    uint32_t m_index { 0 };
    uint32_t m_bucket { UINT32_MAX };
};

template<typename TableType, typename ValueType>
class WTF_VIEW_TYPE OrderedHashTableReverseIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using difference_type = ptrdiff_t;
    using pointer = ValueType*;
    using reference = ValueType&;

    OrderedHashTableReverseIterator() = default;

    ValueType* get() const
    {
        ASSERT(m_table);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index > 0);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index <= m_table->entriesLength());
        return &m_table->entries()[m_index - 1];
    }

    ValueType& operator*() const { return *get(); }
    ValueType* operator->() const { return get(); }

    OrderedHashTableReverseIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index > 0);
        m_index = m_table->liveEndAtOrBefore(m_index - 1);
        return *this;
    }

    OrderedHashTableReverseIterator operator++(int)
    {
        auto result = *this;
        ++*this;
        return result;
    }

    friend bool operator==(const OrderedHashTableReverseIterator& a, const OrderedHashTableReverseIterator& b)
    {
        return a.m_table == b.m_table && a.m_index == b.m_index;
    }

private:
    friend TableType;
    template<typename, typename> friend class OrderedHashTableConstReverseIterator;

    OrderedHashTableReverseIterator(TableType* table, uint32_t index)
        : m_table(table)
        , m_index(table->liveEndAtOrBefore(index))
    {
    }

    TableType* m_table { nullptr };
    uint32_t m_index { 0 };
};

template<typename TableType, typename ValueType>
class WTF_VIEW_TYPE OrderedHashTableConstReverseIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using difference_type = ptrdiff_t;
    using pointer = const ValueType*;
    using reference = const ValueType&;

    OrderedHashTableConstReverseIterator() = default;

    OrderedHashTableConstReverseIterator(const OrderedHashTableReverseIterator<std::remove_const_t<TableType>, ValueType>& other)
        : m_table(other.m_table)
        , m_index(other.m_index)
    {
    }

    const ValueType* get() const
    {
        ASSERT(m_table);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index > 0);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index <= m_table->entriesLength());
        return &m_table->entries()[m_index - 1];
    }

    const ValueType& operator*() const { return *get(); }
    const ValueType* operator->() const { return get(); }

    OrderedHashTableConstReverseIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT_WITH_SECURITY_IMPLICATION(m_index > 0);
        m_index = m_table->liveEndAtOrBefore(m_index - 1);
        return *this;
    }

    OrderedHashTableConstReverseIterator operator++(int)
    {
        auto result = *this;
        ++*this;
        return result;
    }

    friend bool operator==(const OrderedHashTableConstReverseIterator& a, const OrderedHashTableConstReverseIterator& b)
    {
        return a.m_table == b.m_table && a.m_index == b.m_index;
    }

private:
    friend TableType;

    OrderedHashTableConstReverseIterator(const TableType* table, uint32_t index)
        : m_table(table)
        , m_index(table->liveEndAtOrBefore(index))
    {
    }

    const TableType* m_table { nullptr };
    uint32_t m_index { 0 };
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
