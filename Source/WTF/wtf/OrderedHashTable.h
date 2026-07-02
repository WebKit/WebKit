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
#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>
#include <wtf/Compiler.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/HashTraits.h>
#include <wtf/StdLibExtras.h>

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

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
class OrderedHashTable final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(OrderedHashTable);
public:
    using KeyType = Key;
    using ValueType = Value;

    struct AddResult {
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
        if (other.m_liveCount) {
            uint32_t newBucketCount = bucketCountForKeyCount(other.m_liveCount);
            initializeBuckets(newBucketCount);
            allocateEntries(entriesCapacityFromBucketCount(newBucketCount));
            for (uint32_t i = 0; i < other.m_entriesLength; ++i) {
                if (!isDeletedEntry(other.m_entries[i])) {
                    new (NotNull, std::addressof(m_entries[m_entriesLength])) ValueType(other.m_entries[i]);
                    insertIntoFreshBuckets(m_entriesLength);
                    ++m_entriesLength;
                    ++m_liveCount;
                }
            }
        }
    }

    OrderedHashTable(OrderedHashTable&& other) noexcept
        : m_buckets(std::exchange(other.m_buckets, nullptr))
        , m_entries(std::exchange(other.m_entries, nullptr))
        , m_bucketCount(std::exchange(other.m_bucketCount, 0))
        , m_entriesCapacity(std::exchange(other.m_entriesCapacity, 0))
        , m_entriesLength(std::exchange(other.m_entriesLength, 0))
        , m_liveCount(std::exchange(other.m_liveCount, 0))
    {
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

    iterator begin() LIFETIME_BOUND
    {
        return iterator(this, 0);
    }

    iterator end() LIFETIME_BOUND
    {
        return iterator(this, m_entriesLength);
    }

    const_iterator begin() const LIFETIME_BOUND
    {
        return const_iterator(this, 0);
    }

    const_iterator end() const LIFETIME_BOUND
    {
        return const_iterator(this, m_entriesLength);
    }

    reverse_iterator rbegin() LIFETIME_BOUND
    {
        return reverse_iterator(this, m_entriesLength);
    }

    reverse_iterator rend() LIFETIME_BOUND
    {
        return reverse_iterator(this, 0);
    }

    const_reverse_iterator rbegin() const LIFETIME_BOUND
    {
        return const_reverse_iterator(this, m_entriesLength);
    }

    const_reverse_iterator rend() const LIFETIME_BOUND
    {
        return const_reverse_iterator(this, 0);
    }

    ValueType* lookup(const KeyType& key)
    {
        if (!m_buckets)
            return nullptr;
        uint32_t mask = m_bucketCount - 1;
        uint32_t probeCount = 0;
        uint32_t i = HashFunctions::hash(key) & mask;
        while (true) {
            uint32_t index = m_buckets[i];
            if (index == emptyBucket)
                return nullptr;
            if (!isDeletedEntry(m_entries[index]) && HashFunctions::equal(Extractor::extract(m_entries[index]), key))
                return &m_entries[index];
            ++probeCount;
            i = (i + probeCount) & mask;
        }
    }

    const ValueType* lookup(const KeyType& key) const
    {
        return const_cast<OrderedHashTable*>(this)->lookup(key);
    }

    template<typename HashTranslator, typename T>
    ValueType* lookup(const T& key)
    {
        if (!m_buckets)
            return nullptr;
        uint32_t mask = m_bucketCount - 1;
        uint32_t probeCount = 0;
        uint32_t i = HashTranslator::hash(key) & mask;
        while (true) {
            uint32_t index = m_buckets[i];
            if (index == emptyBucket)
                return nullptr;
            if (!isDeletedEntry(m_entries[index]) && HashTranslator::equal(Extractor::extract(m_entries[index]), key))
                return &m_entries[index];
            ++probeCount;
            i = (i + probeCount) & mask;
        }
    }

    template<typename HashTranslator, typename T>
    const ValueType* lookup(const T& key) const
    {
        return const_cast<OrderedHashTable*>(this)->template lookup<HashTranslator>(key);
    }

    iterator find(const KeyType& key) LIFETIME_BOUND
    {
        if (!m_buckets)
            return end();
        uint32_t mask = m_bucketCount - 1;
        uint32_t probeCount = 0;
        uint32_t i = HashFunctions::hash(key) & mask;
        while (true) {
            uint32_t index = m_buckets[i];
            if (index == emptyBucket)
                return end();
            if (!isDeletedEntry(m_entries[index]) && HashFunctions::equal(Extractor::extract(m_entries[index]), key))
                return iterator(this, index);
            ++probeCount;
            i = (i + probeCount) & mask;
        }
    }

    const_iterator find(const KeyType& key) const LIFETIME_BOUND
    {
        if (!m_buckets)
            return end();
        uint32_t mask = m_bucketCount - 1;
        uint32_t probeCount = 0;
        uint32_t i = HashFunctions::hash(key) & mask;
        while (true) {
            uint32_t index = m_buckets[i];
            if (index == emptyBucket)
                return end();
            if (!isDeletedEntry(m_entries[index]) && HashFunctions::equal(Extractor::extract(m_entries[index]), key))
                return const_iterator(this, index);
            ++probeCount;
            i = (i + probeCount) & mask;
        }
    }

    template<typename HashTranslator, typename T>
    iterator find(const T& key) LIFETIME_BOUND
    {
        if (!m_buckets)
            return end();
        uint32_t mask = m_bucketCount - 1;
        uint32_t probeCount = 0;
        uint32_t i = HashTranslator::hash(key) & mask;
        while (true) {
            uint32_t index = m_buckets[i];
            if (index == emptyBucket)
                return end();
            if (!isDeletedEntry(m_entries[index]) && HashTranslator::equal(Extractor::extract(m_entries[index]), key))
                return iterator(this, index);
            ++probeCount;
            i = (i + probeCount) & mask;
        }
    }

    template<typename HashTranslator, typename T>
    const_iterator find(const T& key) const LIFETIME_BOUND
    {
        return const_cast<OrderedHashTable*>(this)->template find<HashTranslator>(key);
    }

    bool contains(const KeyType& key) const
    {
        return lookup(key) != nullptr;
    }

    template<typename HashTranslator, typename T>
    bool contains(const T& key) const
    {
        return const_cast<OrderedHashTable*>(this)->template lookup<HashTranslator>(key) != nullptr;
    }

    AddResult add(const KeyType& key, NOESCAPE const auto& valueFunctor)
    {
        return internalAdd(key, valueFunctor);
    }

    AddResult add(KeyType&& key, NOESCAPE const auto& valueFunctor)
    {
        return internalAdd(std::forward<KeyType>(key), valueFunctor);
    }

    template<typename HashTranslator, typename K>
    AddResult add(K&& key, NOESCAPE const auto& valueFunctor)
    {
        if (!m_buckets) {
            initializeBuckets(initialBucketCount);
            allocateEntries(entriesCapacityFromBucketCount(initialBucketCount));
        }

        uint32_t hash = HashTranslator::hash(key);
        uint32_t mask = m_bucketCount - 1;
        uint32_t probeCount = 0;
        uint32_t i = hash & mask;
        uint32_t insertSlot = UINT32_MAX;

        while (true) {
            uint32_t index = m_buckets[i];
            if (index == emptyBucket) {
                if (insertSlot == UINT32_MAX)
                    insertSlot = i;
                break;
            }
            if (isDeletedEntry(m_entries[index])) {
                if (insertSlot == UINT32_MAX)
                    insertSlot = i;
            } else if (HashTranslator::equal(Extractor::extract(m_entries[index]), key))
                return { iterator(this, index), false };
            ++probeCount;
            i = (i + probeCount) & mask;
        }

        if (m_entriesLength == m_entriesCapacity) {
            rehashForAdd();
            insertSlot = probeForEmpty(hash);
        }

        uint32_t newIndex = m_entriesLength;
        // translate call may assume that entry is empty-initialized.
        if constexpr (Traits::emptyValueIsZero)
            zeroBytes(m_entries[newIndex]);
        else
            Traits::template constructEmptyValue<Traits>(m_entries[newIndex]);
        HashTranslator::translate(m_entries[newIndex], std::forward<K>(key), valueFunctor);
        m_buckets[insertSlot] = newIndex;
        auto result = AddResult { iterator(this, newIndex), true };
        ++m_entriesLength;
        ++m_liveCount;
        return result;
    }

    void remove(iterator it)
    {
        ASSERT(it.m_table == this);
        ASSERT(it.m_index < m_entriesLength);
        removeEntryAtIndex(it.m_index);
    }

    void remove(const_iterator it)
    {
        ASSERT(it.m_table == this);
        ASSERT(it.m_index < m_entriesLength);
        removeEntryAtIndex(it.m_index);
    }

    void remove(const KeyType& key)
    {
        auto it = find(key);
        if (it != end())
            remove(it);
    }

    bool removeIf(NOESCAPE const auto& functor)
    {
        // Defer shrinkIfNeeded until after the loop. Shrinking reallocates m_entries
        // and resets m_entriesLength, which would invalidate iteration.
        bool changed = false;
        for (uint32_t i = 0; i < m_entriesLength; ++i) {
            if (!isDeletedEntry(m_entries[i]) && functor(m_entries[i])) {
                hashTraitsDeleteBucket<Traits>(m_entries[i]);
                --m_liveCount;
                changed = true;
            }
        }
        if (changed)
            shrinkIfNeeded();
        return changed;
    }

    void clear()
    {
        deallocateAll();
        m_buckets = nullptr;
        m_entries = nullptr;
        m_bucketCount = 0;
        m_entriesCapacity = 0;
        m_entriesLength = 0;
        m_liveCount = 0;
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
            m_buckets = nullptr;
            m_entries = nullptr;
            initializeBuckets(newBucketCount);
            allocateEntries(entriesCapacityFromBucketCount(newBucketCount));
        }
    }

    // Internal accessors used by iterators
    ValueType* entries() { return m_entries; }
    const ValueType* entries() const { return m_entries; }
    uint32_t entriesLength() const { return m_entriesLength; }

    bool isDeletedEntry(const ValueType& value) const
    {
        return KeyTraits::isDeletedValue(Extractor::extract(value));
    }

private:
    static constexpr uint32_t emptyBucket = UINT32_MAX;
    static constexpr unsigned initialBucketCount = 8;

    static constexpr uint32_t entriesCapacityFromBucketCount(uint32_t bucketCount)
    {
        // Match WTF::HashTable's load factor policy (HashTable.h):
        //   small tables (<= 1024 buckets): 3/4 max load
        //   large tables (>  1024 buckets): 1/2 max load
        // Larger tables use a lower load factor so probe sequences stay short
        // once the bucket array exceeds the L1/L2 footprint.
        constexpr uint32_t maxSmallTableCapacity = 1024;
        if (bucketCount <= maxSmallTableCapacity)
            return (bucketCount * 3) / 4;
        return bucketCount / 2;
    }

    static constexpr uint32_t bucketCountForKeyCount(uint32_t keyCount)
    {
        uint32_t bucketCount = initialBucketCount;
        while (entriesCapacityFromBucketCount(bucketCount) < keyCount)
            bucketCount <<= 1;
        return bucketCount;
    }

    // Assumes no key equals the one being inserted and no deleted slots appear
    // in the probe path — valid immediately after initializeBuckets() or rehash().
    void insertIntoFreshBuckets(uint32_t entryIndex)
    {
        uint32_t insertSlot = probeForEmpty(HashFunctions::hash(Extractor::extract(m_entries[entryIndex])));
        m_buckets[insertSlot] = entryIndex;
    }

    uint32_t probeForEmpty(uint32_t hash) const
    {
        uint32_t mask = m_bucketCount - 1;
        uint32_t probeCount = 0;
        uint32_t i = hash & mask;
        while (m_buckets[i] != emptyBucket) {
            ++probeCount;
            i = (i + probeCount) & mask;
        }
        return i;
    }

    void initializeBuckets(uint32_t count)
    {
        m_bucketCount = count;
        m_buckets = static_cast<uint32_t*>(Malloc::malloc(count * sizeof(uint32_t)));
        std::fill_n(m_buckets, count, emptyBucket);
    }

    void allocateEntries(uint32_t cap)
    {
        m_entriesCapacity = cap;
        m_entries = static_cast<ValueType*>(Malloc::malloc(cap * sizeof(ValueType)));
    }

    void deallocateAll()
    {
        if (m_entries) {
            for (uint32_t i = 0; i < m_entriesLength; ++i) {
                if (!isDeletedEntry(m_entries[i]))
                    m_entries[i].~ValueType();
            }
            Malloc::free(m_entries);
        }
        if (m_buckets)
            Malloc::free(m_buckets);
    }

    template<typename K>
    AddResult internalAdd(K&& key, NOESCAPE const auto& valueFunctor)
    {
        if (!m_buckets) {
            initializeBuckets(initialBucketCount);
            allocateEntries(entriesCapacityFromBucketCount(initialBucketCount));
        }

        uint32_t hash = HashFunctions::hash(key);
        uint32_t mask = m_bucketCount - 1;
        uint32_t probeCount = 0;
        uint32_t i = hash & mask;
        uint32_t insertSlot = UINT32_MAX;

        while (true) {
            uint32_t index = m_buckets[i];
            if (index == emptyBucket) {
                if (insertSlot == UINT32_MAX)
                    insertSlot = i;
                break;
            }
            if (isDeletedEntry(m_entries[index])) {
                if (insertSlot == UINT32_MAX)
                    insertSlot = i;
            } else if (HashFunctions::equal(Extractor::extract(m_entries[index]), key))
                return { iterator(this, index), false };
            ++probeCount;
            i = (i + probeCount) & mask;
        }

        if (m_entriesLength == m_entriesCapacity) {
            rehashForAdd();
            insertSlot = probeForEmpty(hash);
        }

        uint32_t newIndex = m_entriesLength;
        new (NotNull, std::addressof(m_entries[newIndex])) ValueType(valueFunctor());
        m_buckets[insertSlot] = newIndex;
        auto result = AddResult { iterator(this, newIndex), true };
        ++m_entriesLength;
        ++m_liveCount;
        return result;
    }

    void removeEntryAtIndex(uint32_t index)
    {
        ASSERT(index < m_entriesLength);
        ASSERT(!isDeletedEntry(m_entries[index]));

        // The bucket slot keeps pointing at this entry; probes skip it via
        // isDeletedEntry, so no second probe is needed here.
        hashTraitsDeleteBucket<Traits>(m_entries[index]);

        --m_liveCount;

        shrinkIfNeeded();
    }

    void rehashForAdd()
    {
        // Entries array is full. Grow if mostly live, otherwise compact in place.
        if (m_liveCount >= m_entriesCapacity * 3 / 4)
            rehash(m_bucketCount << 1);
        else
            compactInPlace();
    }

    void shrinkIfNeeded()
    {
        if (m_bucketCount <= initialBucketCount)
            return;
        if (m_liveCount >= m_entriesLength / 4)
            return;
        rehash(std::max<uint32_t>(m_bucketCount >> 1, initialBucketCount));
    }

    void compactInPlace()
    {
        uint32_t writeIndex = 0;
        for (uint32_t readIndex = 0; readIndex < m_entriesLength; ++readIndex) {
            if (isDeletedEntry(m_entries[readIndex]))
                continue;
            if (readIndex != writeIndex) {
                new (NotNull, std::addressof(m_entries[writeIndex])) ValueType(WTF::move(m_entries[readIndex]));
                m_entries[readIndex].~ValueType();
            }
            ++writeIndex;
        }
        m_entriesLength = writeIndex;
        m_liveCount = writeIndex;

        std::fill_n(m_buckets, m_bucketCount, emptyBucket);
        for (uint32_t i = 0; i < m_entriesLength; ++i)
            insertIntoFreshBuckets(i);
    }

    void rehash(uint32_t newBucketCount)
    {
        uint32_t* oldBuckets = m_buckets;
        ValueType* oldEntries = m_entries;
        uint32_t oldLength = m_entriesLength;

        uint32_t newCapacity = entriesCapacityFromBucketCount(newBucketCount);

        initializeBuckets(newBucketCount);
        allocateEntries(newCapacity);

        m_entriesLength = 0;
        m_liveCount = 0;

        for (uint32_t i = 0; i < oldLength; ++i) {
            if (!isDeletedEntry(oldEntries[i])) {
                new (NotNull, std::addressof(m_entries[m_entriesLength])) ValueType(WTF::move(oldEntries[i]));
                oldEntries[i].~ValueType();
                insertIntoFreshBuckets(m_entriesLength);
                ++m_entriesLength;
                ++m_liveCount;
            }
        }

        Malloc::free(oldEntries);
        Malloc::free(oldBuckets);
    }

    uint32_t* m_buckets { nullptr };
    ValueType* m_entries { nullptr };
    uint32_t m_bucketCount { 0 };
    uint32_t m_entriesCapacity { 0 };
    uint32_t m_entriesLength { 0 };
    uint32_t m_liveCount { 0 };
};

template<typename TableType, typename ValueType>
class OrderedHashTableIterator {
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
        ASSERT(m_index < m_table->entriesLength());
        return &m_table->entries()[m_index];
    }

    ValueType& operator*() const { return *get(); }
    ValueType* operator->() const { return get(); }

    OrderedHashTableIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT(m_index < m_table->entriesLength());
        ++m_index;
        skipDeleted();
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
        ASSERT(m_index > 0);
        --m_index;
        while (m_index > 0 && m_table->isDeletedEntry(m_table->entries()[m_index]))
            --m_index;
        ASSERT(!m_table->isDeletedEntry(m_table->entries()[m_index]));
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
        , m_index(index)
    {
        skipDeleted();
    }

    void skipDeleted()
    {
        while (m_index < m_table->entriesLength() && m_table->isDeletedEntry(m_table->entries()[m_index]))
            ++m_index;
    }

    TableType* m_table { nullptr };
    uint32_t m_index { 0 };
};

template<typename TableType, typename ValueType>
class OrderedHashTableConstIterator {
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
    {
    }

    const ValueType* get() const
    {
        ASSERT(m_table);
        ASSERT(m_index < m_table->entriesLength());
        return &m_table->entries()[m_index];
    }

    const ValueType& operator*() const { return *get(); }
    const ValueType* operator->() const { return get(); }

    OrderedHashTableConstIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT(m_index < m_table->entriesLength());
        ++m_index;
        skipDeleted();
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
        ASSERT(m_index > 0);
        --m_index;
        while (m_index > 0 && m_table->isDeletedEntry(m_table->entries()[m_index]))
            --m_index;
        ASSERT(!m_table->isDeletedEntry(m_table->entries()[m_index]));
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
        , m_index(index)
    {
        skipDeleted();
    }

    void skipDeleted()
    {
        while (m_index < m_table->entriesLength() && m_table->isDeletedEntry(m_table->entries()[m_index]))
            ++m_index;
    }

    const TableType* m_table { nullptr };
    uint32_t m_index { 0 };
};

template<typename TableType, typename ValueType>
class OrderedHashTableReverseIterator {
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
        ASSERT(m_index > 0);
        ASSERT(m_index <= m_table->entriesLength());
        return &m_table->entries()[m_index - 1];
    }

    ValueType& operator*() const { return *get(); }
    ValueType* operator->() const { return get(); }

    OrderedHashTableReverseIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT(m_index > 0);
        --m_index;
        skipDeleted();
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
        , m_index(index)
    {
        skipDeleted();
    }

    void skipDeleted()
    {
        while (m_index > 0 && m_table->isDeletedEntry(m_table->entries()[m_index - 1]))
            --m_index;
    }

    TableType* m_table { nullptr };
    uint32_t m_index { 0 };
};

template<typename TableType, typename ValueType>
class OrderedHashTableConstReverseIterator {
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
        ASSERT(m_index > 0);
        ASSERT(m_index <= m_table->entriesLength());
        return &m_table->entries()[m_index - 1];
    }

    const ValueType& operator*() const { return *get(); }
    const ValueType* operator->() const { return get(); }

    OrderedHashTableConstReverseIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT(m_index > 0);
        --m_index;
        skipDeleted();
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
        , m_index(index)
    {
        skipDeleted();
    }

    void skipDeleted()
    {
        while (m_index > 0 && m_table->isDeletedEntry(m_table->entries()[m_index - 1]))
            --m_index;
    }

    const TableType* m_table { nullptr };
    uint32_t m_index { 0 };
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
