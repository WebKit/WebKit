/*
 * Copyright (C) 2013, 2015-2016 Apple Inc. All rights reserved.
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

#ifndef MapData_h
#define MapData_h

#include "CopyBarrier.h"
#include "JSCell.h"
#include "WeakGCMapInlines.h"
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace JSC {

class ExecState;
class VM;

template<typename Entry, typename JSIterator>
class MapDataImpl {
public:
    enum : int32_t {
        minimumMapSize = 8
    };

    class IteratorData {
    public:
        friend class MapDataImpl;

        IteratorData(const MapDataImpl*);
        bool next(WTF::KeyValuePair<JSValue, JSValue>&);

        // This function is called while packing a map's backing store. The
        // passed-in index is the new index the entry would have after packing.
        void didRemoveEntry(int32_t packedIndex)
        {
            if (isFinished())
                return;

            if (m_index <= packedIndex)
                return;

            --m_index;
        }

        void didRemoveAllEntries()
        {
            if (isFinished())
                return;
            m_index = 0;
        }

        void finish()
        {
            m_index = -1;
        }

    private:
        bool ensureSlot() const;
        bool isFinished() const { return m_index == -1; }
        int32_t refreshCursor() const;

        const MapDataImpl* m_mapData;
        mutable int32_t m_index;
    };
    STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IteratorData);

    struct KeyType {
        ALWAYS_INLINE KeyType() { }
        KeyType(JSValue);
        JSValue value;
    };

    MapDataImpl(VM&, JSCell* owner);

    void set(ExecState*, JSCell* owner, KeyType, JSValue);
    JSValue get(ExecState*, KeyType);
    bool remove(ExecState*, KeyType);
    bool contains(ExecState*, KeyType);
    size_t size(ExecState*) const { return m_size - m_deletedCount; }

    IteratorData createIteratorData(JSIterator*);

    void clear();

    void visitChildren(JSCell* owner, SlotVisitor&);
    void copyBackingStore(CopyVisitor&, CopyToken);

    size_t capacityInBytes() const { return m_capacity * sizeof(Entry); }

private:
    typedef WTF::UnsignedWithZeroKeyHashTraits<int32_t> IndexTraits;

    typedef HashMap<JSCell*, int32_t, typename WTF::DefaultHash<JSCell*>::Hash, WTF::HashTraits<JSCell*>, IndexTraits> CellKeyedMap;
    typedef HashMap<EncodedJSValue, int32_t, EncodedJSValueHash, EncodedJSValueHashTraits, IndexTraits> ValueKeyedMap;
    typedef HashMap<StringImpl*, int32_t, typename WTF::DefaultHash<StringImpl*>::Hash, WTF::HashTraits<StringImpl*>, IndexTraits> StringKeyedMap;
    typedef HashMap<SymbolImpl*, int32_t, typename WTF::PtrHash<SymbolImpl*>, WTF::HashTraits<SymbolImpl*>, IndexTraits> SymbolKeyedMap;

    ALWAYS_INLINE Entry* find(ExecState*, KeyType);
    ALWAYS_INLINE Entry* add(ExecState*, JSCell* owner, KeyType);
    template <typename Map, typename Key> ALWAYS_INLINE Entry* add(ExecState*, JSCell* owner, Map&, Key, KeyType);

    ALWAYS_INLINE bool shouldPack() const { return m_deletedCount; }
    CheckedBoolean ensureSpaceForAppend(ExecState*, JSCell* owner);

    ALWAYS_INLINE void replaceAndPackBackingStore(Entry* destination, int32_t newSize);
    ALWAYS_INLINE void replaceBackingStore(Entry* destination, int32_t newSize);

    CellKeyedMap m_cellKeyedTable;
    ValueKeyedMap m_valueKeyedTable;
    StringKeyedMap m_stringKeyedTable;
    SymbolKeyedMap m_symbolKeyedTable;
    int32_t m_capacity;
    int32_t m_size;
    int32_t m_deletedCount;
    JSCell* m_owner;
    CopyBarrier<Entry> m_entries;
    WeakGCMap<JSIterator*, JSIterator> m_iterators;
};

template<typename Entry, typename JSIterator>
ALWAYS_INLINE MapDataImpl<Entry, JSIterator>::MapDataImpl(VM& vm, JSCell* owner)
    : m_capacity(0)
    , m_size(0)
    , m_deletedCount(0)
    , m_owner(owner)
    , m_iterators(vm)
{
}

template<typename Entry, typename JSIterator>
ALWAYS_INLINE MapDataImpl<Entry, JSIterator>::KeyType::KeyType(JSValue v)
{
    if (!v.isDouble()) {
        value = v;
        return;
    }
    double d = v.asDouble();
    if (std::isnan(d)) {
        value = v;
        return;
    }

    int i = static_cast<int>(v.asDouble());
    if (i != d)
        value = v;
    else
        value = jsNumber(i);
}

template<typename Entry, typename JSIterator>
ALWAYS_INLINE MapDataImpl<Entry, JSIterator>::IteratorData::IteratorData(const MapDataImpl<Entry, JSIterator>* mapData)
    : m_mapData(mapData)
    , m_index(0)
{
}

template<typename Entry, typename JSIterator>
ALWAYS_INLINE bool MapDataImpl<Entry, JSIterator>::IteratorData::next(WTF::KeyValuePair<JSValue, JSValue>& pair)
{
    if (!ensureSlot())
        return false;
    Entry* entry = &m_mapData->m_entries.get()[m_index];
    pair = WTF::KeyValuePair<JSValue, JSValue>(entry->key().get(), entry->value().get());
    m_index += 1;
    return true;
}

// This is a bit gnarly. We use an index of -1 to indicate the
// finished state. By casting to unsigned we can immediately
// test if both iterators are at the end of their iteration.
template<typename Entry, typename JSIterator>
ALWAYS_INLINE bool MapDataImpl<Entry, JSIterator>::IteratorData::ensureSlot() const
{
    int32_t index = refreshCursor();
    return static_cast<size_t>(index) < static_cast<size_t>(m_mapData->m_size);
}

template<typename Entry, typename JSIterator>
ALWAYS_INLINE int32_t MapDataImpl<Entry, JSIterator>::IteratorData::refreshCursor() const
{
    if (isFinished())
        return m_index;

    Entry* entries = m_mapData->m_entries.get();
    size_t end = m_mapData->m_size;
    while (static_cast<size_t>(m_index) < end && !entries[m_index].key())
        m_index++;
    return m_index;
}

}

#endif /* !defined(MapData_h) */
