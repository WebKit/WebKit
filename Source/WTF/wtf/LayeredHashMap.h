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
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/MathExtras.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/VectorTraits.h>

namespace WTF {

// A hash map whose entries are grouped into layers, where dropping every entry added since the
// most recent startLayer() costs only as much as the number of those entries. A client that pairs
// startLayer()/dropLastLayer() with descent and ascent of the dominator tree therefore holds
// exactly the entries defined in dominating blocks, so a lookup needs no dominance test.
//
// A key can be present at most once, and entries cannot be updated in place: fold whatever would
// have been an update into the key instead.
template<typename KeyArg, typename MappedArg, typename HashArg = DefaultHash<KeyArg>, typename KeyTraitsArg = HashTraits<KeyArg>, typename MappedTraitsArg = HashTraits<MappedArg>>
class LayeredHashMap {
    WTF_MAKE_NONCOPYABLE(LayeredHashMap);
public:
    using KeyType = KeyArg;
    using MappedType = MappedArg;
    using HashFunctions = HashArg;
    using KeyTraits = KeyTraitsArg;
    using MappedTraits = MappedTraitsArg;

    LayeredHashMap()
    {
        static_assert(KeyTraits::minimumTableSize > 0);
        allocateTable(KeyTraits::minimumTableSize);
    }

    void startLayer()
    {
        m_layerHeads.append(nullptr);
    }

    void dropLastLayer()
    {
        ASSERT(!m_layerHeads.isEmpty());
        for (Entry* entry = m_layerHeads.last(); entry;) {
            Entry* next = entry->layerNeighbor;
            *entry = Entry();
            --m_entryCount;
            entry = next;
        }
        m_layerHeads.removeLast();
    }

    unsigned layerCount() const { return m_layerHeads.size(); }

    void clearEntries()
    {
        if (!m_entryCount)
            return;
        m_table.fill(Entry());
        m_layerHeads.fill(nullptr);
        m_entryCount = 0;
    }

    void clear()
    {
        clearEntries();
        m_layerHeads.shrink(0);
    }

    std::optional<MappedType> find(const KeyType& key)
    {
        ASSERT(!isEmptyKey(key));
        const Entry& entry = *findSlot(key, HashFunctions::hash(key));
        if (entry.isEmpty())
            return std::nullopt;
        return entry.value;
    }

    std::optional<MappedType> findOrAdd(const KeyType& key, const MappedType& value)
    {
        ASSERT(!m_layerHeads.isEmpty());
        ASSERT(!isEmptyKey(key));
        unsigned hash = HashFunctions::hash(key);
        Entry* slot = findSlot(key, hash);
        if (!slot->isEmpty())
            return slot->value;
        *slot = Entry { key, value, m_layerHeads.last(), hash };
        m_layerHeads.last() = slot;
        ++m_entryCount;
        rehashIfNeeded();
        return std::nullopt;
    }

    bool add(const KeyType& key, const MappedType& value)
    {
        return !findOrAdd(key, value);
    }

private:
    static bool isEmptyKey(const KeyType& key) { return isHashTraitsEmptyValue<KeyTraits>(key); }

    struct Entry {
        KeyType key { KeyTraits::emptyValue() };
        MappedType value { MappedTraits::emptyValue() };
        Entry* layerNeighbor { nullptr };
        unsigned hash { 0 };

        bool isEmpty() const { return isEmptyKey(key); }
    };
    static_assert(VectorTraits<Entry>::needsInitialization);
    static_assert(hasOneBitSet(KeyTraits::minimumTableSize), "findSlot() masks instead of dividing.");

    void allocateTable(unsigned capacity)
    {
        m_table.clear();
        m_table.grow(capacity);
        m_mask = capacity - 1;
        m_entryCount = 0;
    }

    Entry* findSlot(const KeyType& key, unsigned hash)
    {
        unsigned index = hash & m_mask;
        for (unsigned probeCount = 1;; ++probeCount) {
            Entry& entry = m_table[index];
            if (entry.isEmpty())
                return &entry;
            if (entry.hash == hash && HashFunctions::equal(entry.key, key))
                return &entry;
            index = (index + probeCount) & m_mask;
        }
    }

    void rehashIfNeeded()
    {
        if (m_entryCount * 4 < m_table.size() * 3)
            return;

        Vector<Entry> oldTable = WTF::move(m_table);
        Vector<Entry*> oldHeads = WTF::move(m_layerHeads);
        allocateTable(oldTable.size() * 2);
        m_layerHeads.grow(oldHeads.size());

        for (unsigned layer = 0; layer < oldHeads.size(); ++layer) {
            m_layerHeads[layer] = nullptr;
            for (Entry* entry = oldHeads[layer]; entry;) {
                Entry* next = entry->layerNeighbor;
                Entry* slot = findSlot(entry->key, entry->hash);
                *slot = *entry;
                slot->layerNeighbor = m_layerHeads[layer];
                m_layerHeads[layer] = slot;
                ++m_entryCount;
                entry = next;
            }
        }
    }

    unsigned m_mask { 0 };
    unsigned m_entryCount { 0 };
    Vector<Entry> m_table;
    Vector<Entry*> m_layerHeads;
};

} // namespace WTF

using WTF::LayeredHashMap;
