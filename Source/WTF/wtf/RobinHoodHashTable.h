/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright 2018 The Rust Project Developers.
 *
 * Permission is hereby granted, free of charge, to any
 * person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the
 * Software without restriction, including without
 * limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice
 * shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <wtf/HashTable.h>

namespace WTF {

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
class RobinHoodHashTable;

// 95% load factor. This a bit regress "insertion" performance, while it keeps lookup performance sane.
// RobinHoodHashTable can work with 95% load factor because of maintained probe distance.
struct MemoryCompactLookupOnlyRobinHoodHashTableSizePolicy {
    static constexpr unsigned maxLoadNumerator = 19;
    static constexpr unsigned maxLoadDenominator = 20;
    static constexpr unsigned minLoad = 6;
};

// 90% load factor. RobinHoodHashTable can work with such a high load-factor.
// Observed performance is slightly worse than HashTable (75% for small table, 50% for large table).
struct MemoryCompactRobinHoodHashTableSizePolicy {
    static constexpr unsigned maxLoadNumerator = 9;
    static constexpr unsigned maxLoadDenominator = 10;
    static constexpr unsigned minLoad = 6;
};

// 75% load factor, this maintains the performance same to HashTable, still the load factor
// is higher (HashTable uses 75% for small table, 50 for large table).
struct FastRobinHoodHashTableSizePolicy {
    static constexpr unsigned maxLoadNumerator = 3;
    static constexpr unsigned maxLoadDenominator = 4;
    static constexpr unsigned minLoad = 6;
};

// RobinHood HashTable has several limitations compared to usual HashTable, that's why this is not a default one.
// RobinHood HashTable computes hash much more frequently. This means that the Key should cache computed hash.
// But our default HashTable does not cache hash value because of memory usage. This design means that Key type
// should have hash value intrusively (e.g. WTF::String holds hash value internally).
//
// If the above requirements meet your use case, then you can try RobinHood HashTable.
// The largest benefit is that we can use significantly high load-factor (90% - 95%)!
//
// The algorithm is RobinHood-Hashing + backward shift deletion, described in [1,2].
//
// Naive RobinHoodHashTable has some cases which cause O(N^2) when iterating it and inserting it to some new RobinHoodHashTable
// without reserving capacity and this is because of high load-factor and exposed hash-ordering at iteration[3]. To mitigate it,
// we calculate hash for each table, and do XOR with the hash value to make hash-ordering different for each table.
//
// [1]: https://codecapsule.com/2013/11/11/robin-hood-hashing/
// [2]: https://codecapsule.com/2013/11/17/robin-hood-hashing-backward-shift-deletion/
// [3]: https://accidentallyquadratic.tumblr.com/post/153545455987/rust-hash-iteration-reinsertion
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
class RobinHoodHashTable {
public:
    using HashTableType = RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>;
    using iterator = HashTableIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
    using const_iterator = HashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
    using ValueTraits = Traits;
    using KeyType = Key;
    using ValueType = Value;
    using IdentityTranslatorType = IdentityHashTranslator<ValueTraits, HashFunctions>;
    using AddResult = HashTableAddResult<iterator>;

    static constexpr unsigned probeDistanceThreshold = 128;

    static_assert(!KeyTraits::hasIsReleasedWeakValueFunction);
    static_assert(HashFunctions::hasHashInValue);

    RobinHoodHashTable() = default;

    ~RobinHoodHashTable()
    {
        invalidateIterators(this);
        if (m_table)
            deallocateTable(m_table, tableSize());
    }

    RobinHoodHashTable(const RobinHoodHashTable&);
    void swap(RobinHoodHashTable&);
    RobinHoodHashTable& operator=(const RobinHoodHashTable&);

    RobinHoodHashTable(RobinHoodHashTable&&);
    RobinHoodHashTable& operator=(RobinHoodHashTable&&);

    // When the hash table is empty, just return the same iterator for end as for begin.
    // This is more efficient because we don't have to skip all the empty and deleted
    // buckets, and iterating an empty table is a common case that's worth optimizing.
    iterator begin() { return isEmpty() ? end() : makeIterator(m_table); }
    iterator end() { return makeKnownGoodIterator(m_table + tableSize()); }
    const_iterator begin() const { return isEmpty() ? end() : makeConstIterator(m_table); }
    const_iterator end() const { return makeKnownGoodConstIterator(m_table + tableSize()); }

    iterator random()
    {
        if (isEmpty())
            return end();

        while (true) {
            auto& bucket = m_table[weakRandomUint32() & tableSizeMask()];
            if (!isEmptyBucket(bucket))
                return makeKnownGoodIterator(&bucket);
        }
    }

    const_iterator random() const { return static_cast<const_iterator>(const_cast<RobinHoodHashTable*>(this)->random()); }

    unsigned size() const { return keyCount(); }
    unsigned capacity() const { return tableSize(); }
    bool isEmpty() const { return !keyCount(); }

    void reserveInitialCapacity(unsigned keyCount)
    {
        ASSERT(!m_table);
        ASSERT(!tableSize());

        unsigned minimumTableSize = KeyTraits::minimumTableSize;
        unsigned newTableSize = std::max(minimumTableSize, computeBestTableSize(keyCount));

        m_table = allocateTable(newTableSize);
        m_tableSize = newTableSize;
        m_keyCount = 0;
        m_tableHash = computeTableHash(m_table);
        m_willExpand = false;
        internalCheckTableConsistency();
    }

    AddResult add(const ValueType& value) { return add<IdentityTranslatorType>(Extractor::extract(value), value); }
    AddResult add(ValueType&& value) { return add<IdentityTranslatorType>(Extractor::extract(value), WTFMove(value)); }

    // A special version of add() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table.
    template<typename HashTranslator, typename T, typename Extra> AddResult add(T&& key, Extra&&);
    template<typename HashTranslator, typename T, typename Extra> AddResult addPassingHashCode(T&& key, Extra&&);

    iterator find(const KeyType& key) { return find<IdentityTranslatorType>(key); }
    const_iterator find(const KeyType& key) const { return find<IdentityTranslatorType>(key); }
    bool contains(const KeyType& key) const { return contains<IdentityTranslatorType>(key); }

    template<typename HashTranslator, typename T> iterator find(const T&);
    template<typename HashTranslator, typename T> const_iterator find(const T&) const;
    template<typename HashTranslator, typename T> bool contains(const T&) const;

    void remove(const KeyType&);
    void remove(iterator);
    void removeWithoutEntryConsistencyCheck(iterator);
    void removeWithoutEntryConsistencyCheck(const_iterator);
    void clear();

    static bool isEmptyBucket(const ValueType& value) { return isHashTraitsEmptyValue<KeyTraits>(Extractor::extract(value)); }
    static bool isEmptyOrDeletedBucket(const ValueType& value) { return isEmptyBucket(value); }

    ValueType* lookup(const Key& key) { return lookup<IdentityTranslatorType>(key); }
    template<typename HashTranslator, typename T> ValueType* lookup(const T&);
    template<typename HashTranslator, typename T> ValueType* inlineLookup(const T&);

#if ASSERT_ENABLED
        void checkTableConsistency() const;
#else
        static void checkTableConsistency() { }
#endif

#if CHECK_HASHTABLE_CONSISTENCY
        void internalCheckTableConsistency() const { checkTableConsistency(); }
        void internalCheckTableConsistencyExceptSize() const { checkTableConsistencyExceptSize(); }
#else
        static void internalCheckTableConsistencyExceptSize() { }
        static void internalCheckTableConsistency() { }
#endif

    static constexpr bool shouldExpand(uint64_t keyCount, uint64_t tableSize)
    {
        return keyCount * maxLoadDenominator >= tableSize * maxLoadNumerator;
    }

private:
    static ValueType* allocateTable(unsigned size);
    static void deallocateTable(ValueType* table, unsigned size);

    using LookupType = std::pair<ValueType*, bool>;

    template<typename HashTranslator, typename T> void checkKey(const T&);

    void maintainProbeDistanceForAdd(ValueType&&, unsigned index, unsigned distance, unsigned size, unsigned sizeMask, unsigned tableHash);

    void removeAndInvalidateWithoutEntryConsistencyCheck(ValueType*);
    void removeAndInvalidate(ValueType*);
    void remove(ValueType*);

    static unsigned computeTableHash(ValueType* table) { return DefaultHash<ValueType*>::hash(table); }

    static constexpr unsigned computeBestTableSize(unsigned keyCount);
    bool shouldExpand() const
    {
        unsigned keyCount = this->keyCount();
        unsigned tableSize = this->tableSize();
        if (shouldExpand(keyCount, tableSize))
            return true;
        // If probe-length exceeds probeDistanceThreshold, and 50%~ is filled, expand the table.
        return m_willExpand && keyCount * 2 >= tableSize;
    }
    bool shouldShrink() const { return keyCount() * minLoad < tableSize() && tableSize() > KeyTraits::minimumTableSize; }
    void expand();
    void shrink() { rehash(tableSize() / 2); }
    void shrinkToBestSize();

    void rehash(unsigned newTableSize);
    void reinsert(ValueType&&);

    static void initializeBucket(ValueType& bucket);
    static void deleteBucket(ValueType& bucket) { hashTraitsDeleteBucket<Traits>(bucket); }

    static constexpr unsigned desiredIndex(unsigned hash, unsigned sizeMask)
    {
        return hash & sizeMask;
    }

    static constexpr unsigned probeDistance(unsigned hash, unsigned index, unsigned size, unsigned sizeMask)
    {
        return (index + size - desiredIndex(hash, sizeMask)) & sizeMask;
    }

    iterator makeIterator(ValueType* pos) { return iterator(this, pos, m_table + tableSize()); }
    const_iterator makeConstIterator(ValueType* pos) const { return const_iterator(this, pos, m_table + tableSize()); }
    iterator makeKnownGoodIterator(ValueType* pos) { return iterator(this, pos, m_table + tableSize(), HashItemKnownGood); }
    const_iterator makeKnownGoodConstIterator(ValueType* pos) const { return const_iterator(this, pos, m_table + tableSize(), HashItemKnownGood); }

#if ASSERT_ENABLED
        void checkTableConsistencyExceptSize() const;
#else
        static void checkTableConsistencyExceptSize() { }
#endif

    static constexpr unsigned maxLoadNumerator = SizePolicy::maxLoadNumerator;
    static constexpr unsigned maxLoadDenominator = SizePolicy::maxLoadDenominator;
    static constexpr unsigned minLoad = SizePolicy::minLoad;

    unsigned tableSize() const { return m_tableSize; }
    unsigned tableSizeMask() const { return m_tableSize - 1; }
    unsigned keyCount() const { return m_keyCount; }
    unsigned tableHash() const { return m_tableHash; }

    ValueType* m_table { nullptr };
    unsigned m_tableSize { 0 };
    unsigned m_keyCount { 0 };
    unsigned m_tableHash { 0 };
    bool m_willExpand { false };

#if CHECK_HASHTABLE_ITERATORS
public:
    // All access to m_iterators should be guarded with m_mutex.
    mutable const_iterator* m_iterators { nullptr };
    // Use std::unique_ptr so HashTable can still be memmove'd or memcpy'ed.
    mutable std::unique_ptr<Lock> m_mutex { makeUnique<Lock>() };
#endif
};

#if !ASSERT_ENABLED

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
template<typename HashTranslator, typename T>
inline void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::checkKey(const T&)
{
}

#else // ASSERT_ENABLED

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
template<typename HashTranslator, typename T>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::checkKey(const T& key)
{
    if (!HashFunctions::safeToCompareToEmptyOrDeleted)
        return;
    ASSERT(!HashTranslator::equal(KeyTraits::emptyValue(), key));
    typename std::aligned_storage<sizeof(ValueType), std::alignment_of<ValueType>::value>::type deletedValueBuffer;
    ValueType* deletedValuePtr = reinterpret_cast_ptr<ValueType*>(&deletedValueBuffer);
    ValueType& deletedValue = *deletedValuePtr;
    Traits::constructDeletedValue(deletedValue);
    ASSERT(!HashTranslator::equal(Extractor::extract(deletedValue), key));
}

#endif // ASSERT_ENABLED

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
template<typename HashTranslator, typename T>
inline auto RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::lookup(const T& key) -> ValueType*
{
    return inlineLookup<HashTranslator>(key);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
template<typename HashTranslator, typename T>
ALWAYS_INLINE auto RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::inlineLookup(const T& key) -> ValueType*
{
    checkKey<HashTranslator>(key);

    ValueType* table = m_table;
    if (!table)
        return nullptr;

    unsigned size = tableSize();
    unsigned sizeMask = tableSizeMask();
    unsigned tableHash = this->tableHash();
    unsigned hash = HashTranslator::hash(key) ^ tableHash;
    unsigned index = desiredIndex(hash, sizeMask);
    unsigned distance = 0;

    while (true) {
        ValueType* entry = m_table + index;
        if (isEmptyBucket(*entry))
            return nullptr;

        // RobinHoodHashTable maintains this invariant so that we can stop
        // probing when distance exceeds probing distance of the existing entry.
        auto& entryKey = Extractor::extract(*entry);
        unsigned entryHash = IdentityTranslatorType::hash(entryKey) ^ tableHash;
        if (distance > probeDistance(entryHash, index, size, sizeMask))
            return nullptr;

        if (entryHash == hash && HashTranslator::equal(entryKey, key))
            return entry;

        index = (index + 1) & sizeMask;
        ++distance;
    }
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
inline void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::initializeBucket(ValueType& bucket)
{
    HashTableBucketInitializer<Traits::emptyValueIsZero>::template initialize<Traits>(bucket);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
template<typename HashTranslator, typename T, typename Extra>
ALWAYS_INLINE auto RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::add(T&& key, Extra&& extra) -> AddResult
{
    checkKey<HashTranslator>(key);

    invalidateIterators(this);

    // We should expand before potentially inserting an entry. If we expand a table after inserting an entry,
    // we need to re-look up entry from the table since the inserted entry position is not stable during rehasing.
    if (shouldExpand())
        expand();

    internalCheckTableConsistency();

    ASSERT(m_table);

    unsigned size = tableSize();
    unsigned sizeMask = tableSizeMask();
    unsigned tableHash = this->tableHash();
    unsigned hash = HashTranslator::hash(key) ^ tableHash;
    unsigned index = desiredIndex(hash, sizeMask);
    unsigned distance = 0;

    ValueType* entry = nullptr;
    while (true) {
        entry = m_table + index;
        if (isEmptyBucket(*entry)) {
            if (distance >= probeDistanceThreshold)
                m_willExpand = true;
            HashTranslator::translate(*entry, std::forward<T>(key), std::forward<Extra>(extra));
            break;
        }

        auto& entryKey = Extractor::extract(*entry);
        unsigned entryHash = IdentityTranslatorType::hash(entryKey) ^ tableHash;
        unsigned entryDistance = probeDistance(entryHash, index, size, sizeMask);
        if (distance > entryDistance) {
            if (distance >= probeDistanceThreshold)
                m_willExpand = true;
            // Start swapping existing entry to maintain probe-distance invariant.
            ValueType existingEntry = WTFMove(*entry);
            entry->~ValueType();
            initializeBucket(*entry);
            HashTranslator::translate(*entry, std::forward<T>(key), std::forward<Extra>(extra));
            maintainProbeDistanceForAdd(WTFMove(existingEntry), index, entryDistance, size, sizeMask, tableHash);
            break;
        }

        if (entryHash == hash && HashTranslator::equal(entryKey, key))
            return AddResult(makeKnownGoodIterator(entry), false);

        index = (index + 1) & sizeMask;
        ++distance;
    }

    m_keyCount += 1;

    internalCheckTableConsistency();

    return AddResult(makeKnownGoodIterator(entry), true);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
ALWAYS_INLINE void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::maintainProbeDistanceForAdd(ValueType&& value, unsigned index, unsigned distance, unsigned size, unsigned sizeMask, unsigned tableHash)
{
    using std::swap; // For C++ ADL.
    index = (index + 1) & sizeMask;
    ++distance;

    while (true) {
        ValueType* entry = m_table + index;
        if (isEmptyBucket(*entry)) {
            ValueTraits::assignToEmpty(*entry, WTFMove(value));
            return;
        }

        unsigned entryHash = IdentityTranslatorType::hash(Extractor::extract(*entry)) ^ tableHash;
        unsigned entryDistance = probeDistance(entryHash, index, size, sizeMask);
        if (distance > entryDistance) {
            swap(value, *entry);
            distance = entryDistance;
        }

        index = (index + 1) & sizeMask;
        ++distance;
    }
}


template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
template<typename HashTranslator, typename T, typename Extra>
inline auto RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::addPassingHashCode(T&& key, Extra&& extra) -> AddResult
{
    checkKey<HashTranslator>(key);

    invalidateIterators(this);

    // We should expand before potentially inserting an entry. If we expand a table after inserting an entry,
    // we need to re-look up entry from the table since the inserted entry position is not stable during rehasing.
    if (shouldExpand())
        expand();

    internalCheckTableConsistency();

    ASSERT(m_table);

    unsigned size = tableSize();
    unsigned sizeMask = tableSizeMask();
    unsigned tableHash = this->tableHash();
    unsigned originalHash = HashTranslator::hash(key);
    unsigned hash = originalHash ^ tableHash;
    unsigned index = desiredIndex(hash, sizeMask);
    unsigned distance = 0;

    ValueType* entry = nullptr;
    while (true) {
        entry = m_table + index;
        if (isEmptyBucket(*entry)) {
            if (distance >= probeDistanceThreshold)
                m_willExpand = true;
            HashTranslator::translate(*entry, std::forward<T>(key), std::forward<Extra>(extra), originalHash);
            break;
        }

        auto& entryKey = Extractor::extract(*entry);
        unsigned entryHash = IdentityTranslatorType::hash(entryKey) ^ tableHash;
        unsigned entryDistance = probeDistance(entryHash, index, size, sizeMask);
        if (distance > entryDistance) {
            if (distance >= probeDistanceThreshold)
                m_willExpand = true;
            // Start swapping existing entry to maintain probe-distance invariant.
            ValueType existingEntry = WTFMove(*entry);
            entry->~ValueType();
            initializeBucket(*entry);
            HashTranslator::translate(*entry, std::forward<T>(key), std::forward<Extra>(extra), originalHash);
            maintainProbeDistanceForAdd(WTFMove(existingEntry), index, entryDistance, size, sizeMask, tableHash);
            break;
        }

        if (entryHash == hash && HashTranslator::equal(entryKey, key))
            return AddResult(makeKnownGoodIterator(entry), false);

        index = (index + 1) & sizeMask;
        ++distance;
    }

    m_keyCount += 1;

    internalCheckTableConsistency();

    return AddResult(makeKnownGoodIterator(entry), true);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
inline void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::reinsert(ValueType&& value)
{
    using std::swap; // For C++ ADL.
    unsigned size = tableSize();
    unsigned sizeMask = tableSizeMask();
    unsigned tableHash = this->tableHash();
    unsigned hash = IdentityTranslatorType::hash(Extractor::extract(value)) ^ tableHash;
    unsigned index = desiredIndex(hash, sizeMask);
    unsigned distance = 0;

    while (true) {
        ValueType* entry = m_table + index;
        if (isEmptyBucket(*entry)) {
            ValueTraits::assignToEmpty(*entry, WTFMove(value));
            return;
        }

        unsigned entryHash = IdentityTranslatorType::hash(Extractor::extract(*entry)) ^ tableHash;
        unsigned entryDistance = probeDistance(entryHash, index, size, sizeMask);
        if (distance > entryDistance) {
            swap(value, *entry);
            distance = entryDistance;
        }

        index = (index + 1) & sizeMask;
        ++distance;
    }
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
template <typename HashTranslator, typename T>
auto RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::find(const T& key) -> iterator
{
    if (!m_table)
        return end();

    ValueType* entry = lookup<HashTranslator>(key);
    if (!entry)
        return end();

    return makeKnownGoodIterator(entry);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
template <typename HashTranslator, typename T>
auto RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::find(const T& key) const -> const_iterator
{
    if (!m_table)
        return end();

    ValueType* entry = const_cast<RobinHoodHashTable*>(this)->lookup<HashTranslator>(key);
    if (!entry)
        return end();

    return makeKnownGoodConstIterator(entry);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
template <typename HashTranslator, typename T>
bool RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::contains(const T& key) const
{
    if (!m_table)
        return false;

    return const_cast<RobinHoodHashTable*>(this)->lookup<HashTranslator>(key);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::removeAndInvalidateWithoutEntryConsistencyCheck(ValueType* pos)
{
    invalidateIterators(this);
    remove(pos);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::removeAndInvalidate(ValueType* pos)
{
    invalidateIterators(this);
    internalCheckTableConsistency();
    remove(pos);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::remove(ValueType* pos)
{
    // This is removal via "backward-shift-deletion". This basically shift existing entries to removed empty entry place so that we make
    // the table as if no removal happened so far. This decreases distance-to-initial-bucket (DIB) of the subsequent entries by 1. This maintains
    // DIB of the table low and relatively constant even if we have many removals, compared to using tombstones.
    // https://codecapsule.com/2013/11/17/robin-hood-hashing-backward-shift-deletion/
    deleteBucket(*pos);
    initializeBucket(*pos);
    m_keyCount -= 1;

    unsigned size = tableSize();
    unsigned sizeMask = tableSizeMask();
    unsigned tableHash = this->tableHash();
    unsigned indexPrevious = pos - m_table;
    unsigned index = (indexPrevious + 1) & sizeMask;

    while (true) {
        Value* previousEntry = m_table + indexPrevious;
        Value* entry = m_table + index;
        if (isEmptyBucket(*entry))
            break;

        ASSERT(isEmptyBucket(*previousEntry));
        auto& entryKey = Extractor::extract(*entry);
        unsigned entryHash = IdentityTranslatorType::hash(entryKey) ^ tableHash;
        if (!probeDistance(entryHash, index, size, sizeMask))
            break;

        ValueTraits::assignToEmpty(*previousEntry, WTFMove(*entry));
        entry->~ValueType();
        initializeBucket(*entry);

        indexPrevious = index;
        index = (index + 1) & sizeMask;
    }

    if (shouldShrink())
        shrink();

    internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
inline void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::remove(iterator it)
{
    if (it == end())
        return;

    removeAndInvalidate(const_cast<ValueType*>(it.m_iterator.m_position));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
inline void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::removeWithoutEntryConsistencyCheck(iterator it)
{
    if (it == end())
        return;

    removeAndInvalidateWithoutEntryConsistencyCheck(const_cast<ValueType*>(it.m_iterator.m_position));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
inline void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::removeWithoutEntryConsistencyCheck(const_iterator it)
{
    if (it == end())
        return;

    removeAndInvalidateWithoutEntryConsistencyCheck(const_cast<ValueType*>(it.m_position));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
inline void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::remove(const KeyType& key)
{
    remove(find(key));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
auto RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::allocateTable(unsigned size) -> ValueType*
{
    // would use a template member function with explicit specializations here, but
    // gcc doesn't appear to support that
    if constexpr (Traits::emptyValueIsZero)
        return reinterpret_cast_ptr<ValueType*>(static_cast<char*>(HashTableMalloc::zeroedMalloc(size * sizeof(ValueType))));

    ValueType* result = reinterpret_cast_ptr<ValueType*>(static_cast<char*>(HashTableMalloc::malloc(size * sizeof(ValueType))));
    for (unsigned i = 0; i < size; i++)
        initializeBucket(result[i]);
    return result;
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::deallocateTable(ValueType* table, unsigned size)
{
    for (unsigned i = 0; i < size; ++i)
        table[i].~ValueType();
    HashTableMalloc::free(reinterpret_cast<char*>(table));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::expand()
{
    unsigned newSize;
    unsigned oldSize = tableSize();
    if (!oldSize)
        newSize = KeyTraits::minimumTableSize;
    else
        newSize = oldSize * 2;

    rehash(newSize);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
constexpr unsigned RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::computeBestTableSize(unsigned keyCount)
{
    unsigned bestTableSize = WTF::roundUpToPowerOfTwo(keyCount);

    if (shouldExpand(keyCount, bestTableSize))
        bestTableSize *= 2;

    auto aboveThresholdForEagerExpansion = [](double loadFactor, unsigned keyCount, unsigned tableSize)
    {
        // Here is the rationale behind this calculation, using 3/4 load-factor.
        // With maxLoad at 3/4 and minLoad at 1/6, our average load is 11/24.
        // If we are getting half-way between 11/24 and 3/4, we double the size
        // to avoid being too close to loadMax and bring the ratio close to 11/24. This
        // give us a load in the bounds [9/24, 15/24).
        double maxLoadRatio = loadFactor;
        double minLoadRatio = 1.0 / minLoad;
        double averageLoadRatio = (maxLoadRatio + minLoadRatio) / 2;
        double halfWayBetweenAverageAndMaxLoadRatio = (averageLoadRatio + maxLoadRatio) / 2;
        return keyCount >= tableSize * halfWayBetweenAverageAndMaxLoadRatio;
    };

    constexpr double loadFactor = static_cast<double>(maxLoadNumerator) / maxLoadDenominator;
    if (aboveThresholdForEagerExpansion(loadFactor, keyCount, bestTableSize))
        bestTableSize *= 2;
    unsigned minimumTableSize = KeyTraits::minimumTableSize;
    return std::max(bestTableSize, minimumTableSize);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::shrinkToBestSize()
{
    unsigned minimumTableSize = KeyTraits::minimumTableSize;
    rehash(std::max(minimumTableSize, computeBestTableSize(keyCount())));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::rehash(unsigned newTableSize)
{
    internalCheckTableConsistencyExceptSize();

    unsigned oldTableSize = tableSize();
    ValueType* oldTable = m_table;

    m_table = allocateTable(newTableSize);
    m_tableSize = newTableSize;
    m_tableHash = computeTableHash(m_table);
    m_willExpand = false;

    for (unsigned i = 0; i < oldTableSize; ++i) {
        auto* oldEntry = oldTable + i;
        if (!isEmptyBucket(*oldEntry))
            reinsert(WTFMove(*oldEntry));
        oldEntry->~ValueType();
    }

    if (oldTable)
        HashTableMalloc::free(reinterpret_cast<char*>(oldTable));

    internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::clear()
{
    invalidateIterators(this);
    if (!m_table)
        return;

    unsigned oldTableSize = tableSize();
    m_tableSize = 0;
    m_keyCount = 0;
    m_tableHash = 0;
    m_willExpand = false;
    deallocateTable(std::exchange(m_table, nullptr), oldTableSize);
    internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::RobinHoodHashTable(const RobinHoodHashTable& other)
{
    if (!other.m_tableSize || !other.m_keyCount)
        return;

    m_table = allocateTable(other.m_tableSize);
    m_tableSize = other.m_tableSize;
    m_keyCount = other.m_keyCount;
    m_tableHash = computeTableHash(m_table);
    m_willExpand = other.m_willExpand;

    for (unsigned index = 0; index < other.m_tableSize; ++index) {
        ValueType& otherEntry = other.m_table[index];
        if (!isEmptyBucket(otherEntry)) {
            ValueType entry(otherEntry);
            reinsert(WTFMove(entry));
        }
    }
    internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::swap(RobinHoodHashTable& other)
{
    using std::swap; // For C++ ADL.
    invalidateIterators(this);
    invalidateIterators(&other);

    swap(m_table, other.m_table);
    swap(m_tableSize, other.m_tableSize);
    swap(m_keyCount, other.m_keyCount);
    swap(m_tableHash, other.m_tableHash);
    swap(m_willExpand, other.m_willExpand);

    internalCheckTableConsistency();
    other.internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
auto RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::operator=(const RobinHoodHashTable& other) -> RobinHoodHashTable&
{
    RobinHoodHashTable tmp(other);
    swap(tmp);
    return *this;
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
inline RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::RobinHoodHashTable(RobinHoodHashTable&& other)
{
    invalidateIterators(&other);

    m_table = std::exchange(other.m_table, nullptr);
    m_tableSize = std::exchange(other.m_tableSize, 0);
    m_keyCount = std::exchange(other.m_keyCount, 0);
    m_tableHash = std::exchange(other.m_tableHash, 0);
    m_willExpand = std::exchange(other.m_willExpand, false);

    internalCheckTableConsistency();
    other.internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
inline auto RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::operator=(RobinHoodHashTable&& other) -> RobinHoodHashTable&
{
    RobinHoodHashTable temp(WTFMove(other));
    swap(temp);
    return *this;
}


#if ASSERT_ENABLED

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::checkTableConsistency() const
{
    checkTableConsistencyExceptSize();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy>
void RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy>::checkTableConsistencyExceptSize() const
{
    if (!m_table)
        return;

    unsigned count = 0;
    unsigned tableSize = this->tableSize();
    for (unsigned i = 0; i < tableSize; ++i) {
        ValueType* entry = m_table + i;
        if (isEmptyBucket(*entry))
            continue;

        auto& key = Extractor::extract(*entry);
        const_iterator it = find(key);
        ASSERT(entry == it.m_position);
        ++count;

        ValueCheck<Key>::checkConsistency(key);
    }

    ASSERT(count == keyCount());
    ASSERT(this->tableSize() >= KeyTraits::minimumTableSize);
    ASSERT(tableSizeMask());
    ASSERT(this->tableSize() == tableSizeMask() + 1);
}

#endif // ASSERT_ENABLED

struct MemoryCompactLookupOnlyRobinHoodHashTableTraits {
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    using TableType = RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, MemoryCompactLookupOnlyRobinHoodHashTableSizePolicy>;
};

struct MemoryCompactRobinHoodHashTableTraits {
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    using TableType = RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, MemoryCompactRobinHoodHashTableSizePolicy>;
};

struct FastRobinHoodHashTableTraits {
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    using TableType = RobinHoodHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, FastRobinHoodHashTableSizePolicy>;
};

} // namespace WTF
