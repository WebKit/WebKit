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

#include <optional>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/MathExtras.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WTF {

// A hash map whose entries are grouped into layers, where dropping every entry added since the
// most recent startLayer() costs only as much as the number of those entries. A client that pairs
// startLayer()/dropLastLayer() with descent and ascent of the dominator tree therefore holds
// exactly the entries defined in dominating blocks, so a lookup needs no dominance test.
//
// add() and findOrAdd() never replace a present entry.
// overlay() shadows any existing entry until the current layer is dropped.
//
// No key value is reserved, so KeyTraits only supplies minimumTableSize.
template<typename KeyArg, typename MappedArg, typename HashArg = DefaultHash<KeyArg>, typename KeyTraitsArg = HashTraits<KeyArg>>
class LayeredHashMap {
    WTF_MAKE_NONCOPYABLE(LayeredHashMap);
public:
    using KeyType = KeyArg;
    using MappedType = MappedArg;
    using HashFunctions = HashArg;
    using KeyTraits = KeyTraitsArg;

    LayeredHashMap()
    {
        static_assert(KeyTraits::minimumTableSize > 0);
        m_table.fill(emptyIndex, KeyTraits::minimumTableSize);
        m_mask = KeyTraits::minimumTableSize - 1;
    }

    void startLayer()
    {
        m_layerStarts.append(m_entries.size());
    }

    void dropLastLayer()
    {
        unsigned layerStart = m_layerStarts.takeLast();
        for (unsigned index = m_entries.size(); index-- > layerStart;) {
            const Entry& entry = m_entries[index];
            unsigned& slot = slotHolding(entry.hash, index);
            slot = entry.previous;
            if (slot == emptyIndex)
                --m_keyCount;
        }
        m_entries.shrink(layerStart);
    }

    unsigned layerCount() const { return m_layerStarts.size(); }

    void clearEntries()
    {
        if (!m_keyCount)
            return;
        m_table.fill(emptyIndex);
        m_entries.shrink(0);
        m_layerStarts.fill(0);
        m_keyCount = 0;
    }

    void clear()
    {
        if (m_keyCount)
            m_table.fill(emptyIndex);
        m_entries.shrink(0);
        m_layerStarts.shrink(0);
        m_keyCount = 0;
    }

    std::optional<MappedType> find(const KeyType& key)
    {
        unsigned index = findSlot(key, HashFunctions::hash(key));
        if (index == emptyIndex)
            return std::nullopt;
        return m_entries[index].value;
    }

    std::optional<MappedType> findOrAdd(const KeyType& key, const MappedType& value)
    {
        ASSERT(layerCount());
        unsigned hash = HashFunctions::hash(key);
        unsigned& slot = findSlot(key, hash);
        if (slot != emptyIndex)
            return m_entries[slot].value;
        slot = m_entries.size();
        m_entries.append(Entry { key, value, hash, emptyIndex });
        ++m_keyCount;
        rehashIfNeeded();
        return std::nullopt;
    }

    bool add(const KeyType& key, const MappedType& value)
    {
        return !findOrAdd(key, value);
    }

    void overlay(const KeyType& key, const MappedType& value)
    {
        ASSERT(layerCount());
        unsigned hash = HashFunctions::hash(key);
        unsigned& slot = findSlot(key, hash);
        unsigned previous = slot;
        if (previous != emptyIndex && previous >= m_layerStarts.last()) {
            m_entries[previous].value = value;
            return;
        }
        slot = m_entries.size();
        m_entries.append(Entry { key, value, hash, previous });
        if (previous != emptyIndex)
            return;
        ++m_keyCount;
        rehashIfNeeded();
    }

private:
    static constexpr unsigned emptyIndex = UINT_MAX;

    // previous is the index of the entry for the same key that this one shadows, or emptyIndex.
    struct Entry {
        KeyType key;
        MappedType value;
        unsigned hash;
        unsigned previous;
    };
    static_assert(hasOneBitSet(KeyTraits::minimumTableSize), "findSlot() masks instead of dividing.");
    static_assert(!isValidCapacityForVector<Entry>(emptyIndex), "Vector must crash before an entry index reaches emptyIndex.");

    unsigned& findSlot(const KeyType& key, unsigned hash)
    {
        unsigned tableIndex = hash & m_mask;
        for (unsigned probeCount = 1;; ++probeCount) {
            unsigned& slot = m_table[tableIndex];
            if (slot == emptyIndex)
                return slot;
            const Entry& entry = m_entries[slot];
            if (entry.hash == hash && HashFunctions::equal(entry.key, key))
                return slot;
            tableIndex = (tableIndex + probeCount) & m_mask;
        }
    }

    unsigned& slotHolding(unsigned hash, unsigned index)
    {
        unsigned tableIndex = hash & m_mask;
        for (unsigned probeCount = 1;; ++probeCount) {
            unsigned& slot = m_table[tableIndex];
            ASSERT(slot != emptyIndex);
            if (slot == index)
                return slot;
            tableIndex = (tableIndex + probeCount) & m_mask;
        }
    }

    void rehashIfNeeded()
    {
        if (m_keyCount * 2 < m_table.size())
            return;

        unsigned capacity = Checked<unsigned>(m_table.size()) * 2;
        m_table.fill(emptyIndex, capacity);
        m_mask = capacity - 1;

        // dropLastLayer() can only empty a slot (rather than use a tombstone) if every key that probes
        // past it was added later in the old table, so slots have to be refilled in the order their
        // keys were first added.
        for (unsigned index = 0; index < m_entries.size(); ++index) {
            const Entry& entry = m_entries[index];
            if (entry.previous == emptyIndex)
                findSlot(entry.key, entry.hash) = index;
            else
                slotHolding(entry.hash, entry.previous) = index;
        }
    }

    unsigned m_mask { 0 };
    unsigned m_keyCount { 0 };
    Vector<unsigned> m_table;
    Vector<Entry> m_entries;
    Vector<unsigned> m_layerStarts;
};

} // namespace WTF

using WTF::LayeredHashMap;
