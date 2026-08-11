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

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/KeyValuePair.h>
#include <wtf/MathExtras.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template<typename IteratorType> struct InlineMapAddResult {
    IteratorType iterator;
    bool isNewEntry;
};

// An unchecked key map that stores entries inline with linear lookup for small sizes and
// switches to hashed heap storage only after growing past a certain point. The switchover
// happens when the map size exceeds the InlineCapacity template parameter. InlineCapacity
// does not need to be any special value such as a power of 2.
//
// InlineMap is to a large extent compatible with UncheckedKeyHashMap. Inserted keys are
// only checked for not being empty or deleted values in debug builds. In release builds
// such insertions would corrupt the map.

template<typename KeyArg, typename ValueArg, unsigned InlineCapacity, typename HashArg = PtrHash<KeyArg>, typename KeyTraitsArg = HashTraits<KeyArg>, typename MappedTraitsArg = HashTraits<ValueArg>>
class InlineMap {
public:
    using KeyType = KeyArg;
    using KeyTraits = KeyTraitsArg;
    using MappedType = ValueArg;
    using MappedTraits = MappedTraitsArg;
    using Entry = KeyValuePair<KeyType, MappedType>;
    using EntryTraits = KeyValuePairHashTraits<KeyTraits, MappedTraits>;
    using MappedTakeType = typename MappedTraits::TakeType;

    InlineMap()
        : m_size(0)
        , m_capacity(InlineCapacity)
    {
    }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

    InlineMap(InlineMap&& other)
        : m_size(other.m_size)
        , m_capacity(other.m_capacity)
    {
        if (isInline()) {
            for (unsigned i = 0; i < m_size; ++i)
                std::construct_at(&inlineStorage()[i], WTF::move(other.inlineStorage()[i]));
        } else {
            m_storage.hashedData.entries = std::exchange(other.m_storage.hashedData.entries, nullptr);
            m_storage.hashedData.deletedCount = std::exchange(other.m_storage.hashedData.deletedCount, 0);
        }
        other.m_size = 0;
        other.m_capacity = InlineCapacity;
    }

    InlineMap(const InlineMap& other)
        : m_size(other.m_size)
        , m_capacity(other.m_capacity)
    {
        if (isInline()) {
            for (unsigned i = 0; i < m_size; ++i)
                std::construct_at(&inlineStorage()[i], other.inlineStorage()[i]);
            return;
        }

        m_storage.hashedData.entries = allocateAndInitializeStorage(m_capacity);
        m_storage.hashedData.deletedCount = other.m_storage.hashedData.deletedCount;
        Entry* srcEntries = other.m_storage.hashedData.entries;
        Entry* destEntries = m_storage.hashedData.entries;

        for (unsigned i = 0; i < m_capacity; ++i) {
            if (isEmptyEntry(srcEntries[i]))
                continue;
            if constexpr (!EntryTraits::emptyValueIsZero)
                std::destroy_at(&destEntries[i]);
            if (isDeletedEntry(srcEntries[i]))
                constructDeletedEntry(destEntries[i]);
            else
                std::construct_at(&destEntries[i], srcEntries[i]);
        }
    }

    InlineMap& operator=(InlineMap&& other)
    {
        if (this != &other) {
            std::destroy_at(this);
            std::construct_at(this, WTF::move(other));
        }
        return *this;
    }

    InlineMap& operator=(const InlineMap& other)
    {
        if (this != &other) {
            InlineMap temp(other);
            swap(temp);
        }
        return *this;
    }

    ~InlineMap()
    {
        if (isInline())
            std::destroy_n(inlineStorage(), m_size);
        else {
            for (unsigned i = 0; i < m_capacity; ++i) {
                if (!isEmptyOrDeletedEntry(m_storage.hashedData.entries[i]))
                    std::destroy_at(&m_storage.hashedData.entries[i]);
            }
            FastMalloc::free(m_storage.hashedData.entries);
        }
    }

    class ValuesIterator;

    class iterator {
        friend class InlineMap;
        friend class ValuesIterator;
    public:
        iterator() = default;

        iterator(Entry* current, Entry* end)
            : m_current(current)
            , m_end(end)
        {
            skipEmpty();
        }

        Entry& operator*() const { return *m_current; }
        Entry* operator->() const { return m_current; }

        iterator& operator++()
        {
            ++m_current;
            skipEmpty();
            return *this;
        }

        bool operator==(const iterator& other) const { return m_current == other.m_current; }

        ValuesIterator values() const { return ValuesIterator(*this); }

    private:
        void skipEmpty()
        {
            while (m_current != m_end && isEmptyOrDeletedEntry(*m_current))
                ++m_current;
        }

        Entry* m_current { nullptr };
        Entry* m_end { nullptr };
    };

    class ValuesIterator {
    public:
        ValuesIterator() = default;
        ValuesIterator(const iterator& it)
            : m_impl(it)
        { }

        MappedType& operator*() const { return m_impl.m_current->value; }
        MappedType* operator->() const { return &m_impl.m_current->value; }

        ValuesIterator& operator++()
        {
            ++m_impl;
            return *this;
        }

        bool operator==(const ValuesIterator& other) const { return m_impl == other.m_impl; }

    private:
        iterator m_impl;
    };

    struct ValuesRange {
        ValuesIterator m_begin;
        ValuesIterator m_end;

        ValuesIterator begin() const { return m_begin; }
        ValuesIterator end() const { return m_end; }
    };

    using const_iterator = iterator;

    using AddResult = InlineMapAddResult<iterator>;

    template<typename K, typename V>
    AddResult add(K&& key, V&& value) LIFETIME_BOUND
    {
        ASSERT(!isEmptyKey(key));
        ASSERT(!KeyTraits::isDeletedValue(key));

        unsigned size = m_size;
        if (isInline()) [[likely]] {
            Entry* entryStorage = inlineStorage();
            Entry* end = entryStorage + size;
            for (unsigned i = 0; i < size; ++i) {
                if (HashArg::equal(entryStorage[i].key, key))
                    return { iterator { &entryStorage[i], end }, false };
            }

            if (size < m_capacity) {
                Entry* slot = &entryStorage[size];
                std::construct_at(slot, std::forward<K>(key), std::forward<V>(value));
                ++m_size;
                return { iterator { slot, entriesEnd() }, true };
            }

            // At capacity in linear mode: become hashed and fall through to the hashed add
            transitionToHashed();
        }

        if ((m_size + deletedCount()) * loadFactorDenominator >= m_capacity * loadFactorNumerator) [[unlikely]]
            expand();

        Entry* end = entriesEnd();
        Entry* slot = findKeyOrEmptyOrDeleted(key);
        if (!isEmptyOrDeletedEntry(*slot))
            return { iterator { slot, end }, false };

        // Slot is either empty or deleted - we can insert here
        if (isDeletedEntry(*slot))
            decrementDeletedCount();
        std::construct_at(slot, std::forward<K>(key), std::forward<V>(value));
        ++m_size;
        return { iterator { slot, end }, true };
    }

    template<typename K>
    bool contains(const K& key) const
    {
        if (!m_size)
            return false;

        if (isInline()) [[likely]] {
            Entry* entryStorage = const_cast<InlineMap*>(this)->inlineStorage();
            for (unsigned i = 0; i < m_size; ++i) {
                if (HashArg::equal(entryStorage[i].key, key))
                    return true;
            }
            return false;
        }

        return !isEmptyOrDeletedEntry(*findKeyOrEmpty(key));
    }

    template<typename K>
    iterator find(const K& key) LIFETIME_BOUND
    {
        if (!m_size)
            return iterator { nullptr, nullptr };

        if (isInline()) [[likely]] {
            Entry* entryStorage = inlineStorage();
            Entry* end = entryStorage + m_size;
            for (unsigned i = 0; i < m_size; ++i) {
                if (HashArg::equal(entryStorage[i].key, key))
                    return iterator { &entryStorage[i], end };
            }
            return iterator { end, end };
        }

        Entry* slot = findKeyOrEmpty(key);
        Entry* end = m_storage.hashedData.entries + m_capacity;
        if (isEmptyOrDeletedEntry(*slot))
            return iterator { end, end };
        return iterator { slot, end };
    }

    template<typename K>
    const_iterator find(const K& key) const LIFETIME_BOUND
    {
        return const_cast<InlineMap*>(this)->find(key);
    }

    template<typename K>
    bool remove(const K& key)
    {
        if (!m_size) [[unlikely]]
            return false;

        Entry* entryStorage = entries();

        if (isInline()) {
            // Find and swap with last entry. Inline mode should not use deleted entries.
            for (unsigned i = 0; i < m_size; ++i) {
                if (HashArg::equal(entryStorage[i].key, key)) [[unlikely]] {
                    std::destroy_at(&entryStorage[i]);
                    --m_size;
                    if (i != m_size) {
                        // Move last entry to fill the gap
                        std::construct_at(&entryStorage[i], WTF::move(entryStorage[m_size]));
                        std::destroy_at(&entryStorage[m_size]);
                    }
                    return true;
                }
            }
            return false;
        }

        // Hashed mode - mark as deleted
        Entry* slot = findKeyOrEmpty(key);
        if (isEmptyOrDeletedEntry(*slot))
            return false;

        std::destroy_at(slot);
        constructDeletedEntry(*slot);
        incrementDeletedCount();
        --m_size;

        if (shouldShrink())
            shrink();

        return true;
    }

    bool remove(iterator it)
    {
        if (it == end())
            return false;

        Entry* slot = it.m_current;

        if (isInline()) {
            std::destroy_at(slot);
            --m_size;
            Entry* lastEntry = &inlineStorage()[m_size];
            if (slot != lastEntry) {
                std::construct_at(slot, WTF::move(*lastEntry));
                std::destroy_at(lastEntry);
            }
            return true;
        }

        std::destroy_at(slot);
        constructDeletedEntry(*slot);
        incrementDeletedCount();
        --m_size;

        if (shouldShrink())
            shrink();

        return true;
    }

    template<typename K>
    MappedTakeType take(const K& key)
    {
        return take(find(key));
    }

    MappedTakeType take(iterator it)
    {
        if (it == end())
            return emptyTakeResult();

        auto result = MappedTraits::take(WTF::move(it->value));
        remove(it);
        return result;
    }

    iterator begin() LIFETIME_BOUND
    {
        if (!m_size)
            return iterator { nullptr, nullptr };

        return iterator { entries(), entriesEnd() };
    }

    const_iterator begin() const LIFETIME_BOUND
    {
        return const_cast<InlineMap*>(this)->begin();
    }

    iterator end() LIFETIME_BOUND
    {
        if (!m_size)
            return iterator { nullptr, nullptr };
        Entry* end = entriesEnd();
        return iterator { end, end };
    }

    const_iterator end() const LIFETIME_BOUND
    {
        return const_cast<InlineMap*>(this)->end();
    }

    ValuesRange values() LIFETIME_BOUND
    {
        return { begin().values(), end().values() };
    }

    ValuesRange values() const LIFETIME_BOUND
    {
        return const_cast<InlineMap*>(this)->values();
    }

    ALWAYS_INLINE unsigned size() const
    {
        return m_size;
    }

    ALWAYS_INLINE bool isEmpty() const { return !m_size; }

    void clear()
    {
        if (isInline()) {
            if (m_size) {
                std::destroy_n(inlineStorage(), m_size);
                m_size = 0;
            }
            return;
        }

        // Hashed mode: destroy live entries, free heap, transition to inline
        Entry* entryStorage = m_storage.hashedData.entries;
        for (unsigned i = 0; i < m_capacity; ++i) {
            if (!isEmptyOrDeletedEntry(entryStorage[i]))
                std::destroy_at(&entryStorage[i]);
        }
        FastMalloc::free(entryStorage);
        m_size = 0;
        m_capacity = InlineCapacity;
    }

    ALWAYS_INLINE void swap(InlineMap& other)
    {
        // For now, implement as three moves
        InlineMap temp(WTF::move(*this));
        *this = WTF::move(other);
        other = WTF::move(temp);
    }

    void reserveInitialCapacity(unsigned keyCount)
    {
        RELEASE_ASSERT(!m_size && m_capacity == InlineCapacity); // expecting a brand new map

        if (keyCount <= InlineCapacity)
            return;

        unsigned capacity = roundUpToPowerOfTwo(keyCount * loadFactorDenominator / loadFactorNumerator + 1);
        m_capacity = capacity;
        m_storage.hashedData.entries = allocateAndInitializeStorage(capacity);
        m_storage.hashedData.deletedCount = 0;
    }

private:
    friend class InlineMapAccessForTesting;

    ALWAYS_INLINE bool isInline() const { return m_capacity == InlineCapacity; }

    ALWAYS_INLINE Entry* inlineStorage()
    {
        return reinterpret_cast<Entry*>(&m_storage.inlineEntries);
    }

    ALWAYS_INLINE const Entry* inlineStorage() const
    {
        return reinterpret_cast<const Entry*>(&m_storage.inlineEntries);
    }

    ALWAYS_INLINE Entry* entries()
    {
        return isInline() ? inlineStorage() : m_storage.hashedData.entries;
    }

    ALWAYS_INLINE const Entry* entries() const
    {
        return isInline() ? inlineStorage() : m_storage.hashedData.entries;
    }

    ALWAYS_INLINE Entry* entriesEnd()
    {
        Entry* entryStorage = entries();
        return isInline()
            ? entryStorage + m_size
            : entryStorage + m_capacity;
    }

    ALWAYS_INLINE const Entry* entriesEnd() const
    {
        const Entry* entryStorage = entries();
        return isInline()
            ? entryStorage + m_size
            : entryStorage + m_capacity;
    }

    ALWAYS_INLINE static constexpr size_t allocationSizeForCapacity(size_t capacity)
    {
        return sizeof(Entry) * capacity;
    }

    ALWAYS_INLINE static bool isEmptyKey(const KeyType& key)
    {
        return isHashTraitsEmptyValue<KeyTraits>(key);
    }

    ALWAYS_INLINE static bool isEmptyEntry(const Entry& entry)
    {
        return isEmptyKey(entry.key);
    }

    ALWAYS_INLINE static bool isDeletedEntry(const Entry& entry)
    {
        return KeyTraits::isDeletedValue(entry.key);
    }

    ALWAYS_INLINE static bool isEmptyOrDeletedEntry(const Entry& entry)
    {
        return isEmptyEntry(entry) || isDeletedEntry(entry);
    }

    ALWAYS_INLINE static void constructDeletedEntry(Entry& entry)
    {
        if constexpr (EntryTraits::emptyValueIsZero) {
            // For zero-initialized types, we can zero the memory first, then set the deleted marker
            zeroBytes(entry);
            KeyTraits::constructDeletedValue(entry.key);
        } else {
            // Create the deleted key value separately to avoid construction/destruction cycle
            typename KeyTraits::TraitType deletedKey = KeyTraits::emptyValue();
            KeyTraits::constructDeletedValue(deletedKey);

            // Construct the Entry directly with the deleted key and empty value
            // This avoids constructing an empty Entry first and then overwriting it
            std::construct_at(std::addressof(entry), WTF::move(deletedKey), MappedTraits::emptyValue());
        }
    }

    ALWAYS_INLINE static void constructEmptyEntry(Entry& entry)
    {
        if constexpr (EntryTraits::emptyValueIsZero)
            zeroBytes(entry);
        else
            std::construct_at(std::addressof(entry), KeyTraits::emptyValue(), MappedTraits::emptyValue());
    }

    static constexpr unsigned loadFactorNumerator = 3;
    static constexpr unsigned loadFactorDenominator = 4;
    static constexpr unsigned minLoadInverse = 6;

    ALWAYS_INLINE static MappedTakeType emptyTakeResult()
    {
        if constexpr (requires { MappedTraits::emptyTakeValue(); })
            return MappedTraits::emptyTakeValue();
        else
            return MappedTraits::take(MappedTraits::emptyValue());
    }

    ALWAYS_INLINE unsigned deletedCount() const
    {
        ASSERT(!isInline());
        return m_storage.hashedData.deletedCount;
    }

    ALWAYS_INLINE void incrementDeletedCount()
    {
        ASSERT(!isInline());
        ++m_storage.hashedData.deletedCount;
    }

    ALWAYS_INLINE void decrementDeletedCount()
    {
        ASSERT(!isInline());
        ASSERT(m_storage.hashedData.deletedCount);
        --m_storage.hashedData.deletedCount;
    }

    bool shouldShrink() const
    {
        ASSERT(!isInline());
        return m_size * minLoadInverse < m_capacity;
    }

    bool shouldCompactOnly() const
    {
        ASSERT(!isInline());
        return m_size * minLoadInverse < m_capacity * 2;
    }

    ALWAYS_INLINE static Entry* allocateAndInitializeStorage(unsigned capacity)
    {
        if constexpr (EntryTraits::emptyValueIsZero)
            return static_cast<Entry*>(FastMalloc::zeroedMalloc(allocationSizeForCapacity(capacity)));

        Entry* storage = static_cast<Entry*>(FastMalloc::malloc(allocationSizeForCapacity(capacity)));
        for (unsigned i = 0; i < capacity; ++i)
            constructEmptyEntry(storage[i]);
        return storage;
    }

    ALWAYS_INLINE void rehashTo(unsigned newCapacity)
    {
        bool wasInline = isInline();
        unsigned oldIterCount = wasInline ? m_size : m_capacity;
        Entry* oldEntries = entries();

        Entry* newEntries = allocateAndInitializeStorage(newCapacity);

        for (unsigned i = 0; i < oldIterCount; ++i) {
            if (wasInline || !isEmptyOrDeletedEntry(oldEntries[i])) {
                Entry* slot = findKeyOrEmptyInStorage(oldEntries[i].key, newEntries, newCapacity);
                ASSERT(isEmptyEntry(*slot));
                std::construct_at(slot, WTF::move(oldEntries[i]));
            }
        }

        Entry* toFree = wasInline ? nullptr : oldEntries;
        m_capacity = newCapacity;
        m_storage.hashedData.entries = newEntries;
        m_storage.hashedData.deletedCount = 0;
        if (toFree)
            FastMalloc::free(toFree);
    }

    void transitionToHashed()
    {
        ASSERT(isInline());
        rehashTo(roundUpToPowerOfTwo(m_size * 2 * loadFactorDenominator / loadFactorNumerator));
    }

    void expand()
    {
        ASSERT(!isInline());
        if (shouldCompactOnly())
            rehashTo(m_capacity);
        else
            rehashTo(m_capacity * 2);
    }

    void shrink()
    {
        ASSERT(!isInline());
        if (m_size <= InlineCapacity) {
            transitionToInline();
            return;
        }
        rehashTo(m_capacity / 2);
    }

    void transitionToInline()
    {
        ASSERT(!isInline());
        ASSERT(m_size <= InlineCapacity);

        Entry* oldEntries = m_storage.hashedData.entries;
        unsigned oldCapacity = m_capacity;

        m_capacity = InlineCapacity;

        unsigned insertIndex = 0;
        for (unsigned i = 0; i < oldCapacity; ++i) {
            if (!isEmptyOrDeletedEntry(oldEntries[i])) {
                std::construct_at(&inlineStorage()[insertIndex], WTF::move(oldEntries[i]));
                ++insertIndex;
            }
        }
        ASSERT(insertIndex == m_size);

        FastMalloc::free(oldEntries);
    }

    template<typename K>
    ALWAYS_INLINE static Entry* findKeyOrEmptyInStorage(const K& key, Entry* data, unsigned capacity)
    {
        ASSERT(isPowerOfTwo(capacity));

        unsigned hash = HashArg::hash(key);
        unsigned mask = capacity - 1;
        unsigned bucket = hash & mask;
        unsigned probe = 0;

        while (true) {
            Entry* entry = &data[bucket];
            if (isEmptyEntry(*entry)) [[likely]]
                return entry;
            if (!isDeletedEntry(*entry) && HashArg::equal(entry->key, key))
                return entry;
            ++probe;
            bucket = (bucket + probe) & mask;
        }
    }

    // If the key is not found, returns the empty slot where the search ended even if
    // there were deleted slots along the way.
    template<typename K>
    Entry* findKeyOrEmpty(const K& key) const
    {
        ASSERT(!isInline());
        return findKeyOrEmptyInStorage(key, m_storage.hashedData.entries, m_capacity);
    }

    Entry* findKeyOrEmptyOrDeleted(const KeyType& key) const
    {
        ASSERT(!isInline());
        ASSERT(isPowerOfTwo(m_capacity));

        Entry* data = m_storage.hashedData.entries;
        unsigned hash = HashArg::hash(key);
        unsigned mask = m_capacity - 1;
        unsigned bucket = hash & mask;
        unsigned probe = 0;
        Entry* firstDeletedSlot = nullptr;

        while (true) {
            Entry* entry = &data[bucket];
            if (isEmptyEntry(*entry)) [[likely]]
                return firstDeletedSlot ? firstDeletedSlot : entry;
            if (isDeletedEntry(*entry)) {
                if (!firstDeletedSlot)
                    firstDeletedSlot = entry;
            } else if (HashArg::equal(entry->key, key))
                return entry;
            ++probe;
            bucket = (bucket + probe) & mask;
        }
    }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    unsigned m_size;
    unsigned m_capacity;
    union Storage {
        struct alignas(Entry) EntryBytes {
            std::byte data[sizeof(Entry)];
        };
        struct HashedModeData {
            Entry* entries;
            unsigned deletedCount;
        };
        std::array<EntryBytes, InlineCapacity> inlineEntries;
        HashedModeData hashedData;
    } m_storage;
};

class InlineMapAccessForTesting {
public:
    template<typename KeyArg, typename ValueArg, unsigned InlineCapacity, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    static bool isInline(InlineMap<KeyArg, ValueArg, InlineCapacity, HashArg, KeyTraitsArg, MappedTraitsArg>& map)
    {
        return map.isInline();
    }

    template<typename KeyArg, typename ValueArg, unsigned InlineCapacity, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    static unsigned capacity(InlineMap<KeyArg, ValueArg, InlineCapacity, HashArg, KeyTraitsArg, MappedTraitsArg>& map)
    {
        return map.m_capacity;
    }

    template<typename KeyArg, typename ValueArg, unsigned InlineCapacity, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    static unsigned deletedCount(InlineMap<KeyArg, ValueArg, InlineCapacity, HashArg, KeyTraitsArg, MappedTraitsArg>& map)
    {
        return map.deletedCount();
    }
};

} // namespace WTF

using WTF::InlineMap;
