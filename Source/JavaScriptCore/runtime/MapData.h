/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "JSCell.h"
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>

namespace JSC {

class ExecState;

template<typename Entry>
class MapDataImpl {
public:
    enum : int32_t {
        minimumMapSize = 8
    };

    struct const_iterator {
        const_iterator(const MapDataImpl*);
        ~const_iterator();
        const WTF::KeyValuePair<JSValue, JSValue> operator*() const;
        JSValue key() const { RELEASE_ASSERT(!atEnd()); return m_mapData->m_entries[m_index].key().get(); }
        JSValue value() const { RELEASE_ASSERT(!atEnd()); return m_mapData->m_entries[m_index].value().get(); }
        void operator++() { ASSERT(!atEnd()); internalIncrement(); }
        static const_iterator end(const MapDataImpl*);
        bool operator!=(const const_iterator& other);
        bool operator==(const const_iterator& other);
        void finish() { m_index = std::numeric_limits<int32_t>::max(); }

        bool ensureSlot();

    private:
        // This is a bit gnarly. We use an index of -1 to indicate the
        // "end()" iterator. By casting to unsigned we can immediately
        // test if both iterators are at the end of their iteration.
        // We need this in order to keep the common case (eg. iter != end())
        // fast.
        bool atEnd() const { return static_cast<size_t>(m_index) >= static_cast<size_t>(m_mapData->m_size); }
        void internalIncrement();
        const MapDataImpl* m_mapData;
        int32_t m_index;
    };

    struct KeyType {
        ALWAYS_INLINE KeyType() { }
        KeyType(JSValue);
        JSValue value;
    };

    MapDataImpl();

    void set(ExecState*, JSCell* owner, KeyType, JSValue);
    JSValue get(ExecState*, KeyType);
    bool remove(ExecState*, KeyType);
    bool contains(ExecState*, KeyType);
    size_t size(ExecState*) const { return m_size - m_deletedCount; }

    const_iterator begin() const { return const_iterator(this); }
    const_iterator end() const { return const_iterator::end(this); }

    void clear();

    void visitChildren(JSCell* owner, SlotVisitor&);
    void copyBackingStore(CopyVisitor&, CopyToken);

private:
    typedef WTF::UnsignedWithZeroKeyHashTraits<int32_t> IndexTraits;

    typedef HashMap<JSCell*, int32_t, typename WTF::DefaultHash<JSCell*>::Hash, WTF::HashTraits<JSCell*>, IndexTraits> CellKeyedMap;
    typedef HashMap<EncodedJSValue, int32_t, EncodedJSValueHash, EncodedJSValueHashTraits, IndexTraits> ValueKeyedMap;
    typedef HashMap<StringImpl*, int32_t, typename WTF::DefaultHash<StringImpl*>::Hash, WTF::HashTraits<StringImpl*>, IndexTraits> StringKeyedMap;
    typedef HashMap<AtomicStringImpl*, int32_t, typename WTF::DefaultHash<AtomicStringImpl*>::Hash, WTF::HashTraits<AtomicStringImpl*>, IndexTraits> SymbolKeyedMap;

    size_t capacityInBytes() { return m_capacity * sizeof(Entry); }

    ALWAYS_INLINE Entry* find(ExecState*, KeyType);
    ALWAYS_INLINE Entry* add(ExecState*, JSCell* owner, KeyType);
    template <typename Map, typename Key> ALWAYS_INLINE Entry* add(ExecState*, JSCell* owner, Map&, Key, KeyType);

    ALWAYS_INLINE bool shouldPack() const { return m_deletedCount && !m_iteratorCount; }
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
    mutable int32_t m_iteratorCount;
    Entry* m_entries;
};

template<typename Entry>
ALWAYS_INLINE MapDataImpl<Entry>::MapDataImpl()
    : m_capacity(0)
    , m_size(0)
    , m_deletedCount(0)
    , m_iteratorCount(0)
    , m_entries(nullptr)
{
}

template<typename Entry>
ALWAYS_INLINE void MapDataImpl<Entry>::clear()
{
    m_cellKeyedTable.clear();
    m_valueKeyedTable.clear();
    m_stringKeyedTable.clear();
    m_symbolKeyedTable.clear();
    m_capacity = 0;
    m_size = 0;
    m_deletedCount = 0;
    m_entries = nullptr;
}

template<typename Entry>
ALWAYS_INLINE MapDataImpl<Entry>::KeyType::KeyType(JSValue v)
{
    if (!v.isDouble()) {
        value = v;
        return;
    }
    double d = v.asDouble();
    if (std::isnan(d) || (std::signbit(d) && d == 0.0)) {
        value = v;
        return;
    }

    int i = static_cast<int>(v.asDouble());
    if (i != d)
        value = v;
    else
        value = jsNumber(i);
}

template<typename Entry>
ALWAYS_INLINE void MapDataImpl<Entry>::const_iterator::internalIncrement()
{
    Entry* entries = m_mapData->m_entries;
    size_t index = m_index + 1;
    size_t end = m_mapData->m_size;
    while (index < end && !entries[index].key())
        index++;
    m_index = index;
}

template<typename Entry>
ALWAYS_INLINE bool MapDataImpl<Entry>::const_iterator::ensureSlot()
{
    // When an iterator exists outside of host cost it is possible for
    // the containing map to be modified
    Entry* entries = m_mapData->m_entries;
    size_t index = m_index;
    size_t end = m_mapData->m_size;
    if (index < end && entries[index].key())
        return true;
    internalIncrement();
    return static_cast<size_t>(m_index) < end;
}

template<typename Entry>
ALWAYS_INLINE MapDataImpl<Entry>::const_iterator::const_iterator(const MapDataImpl<Entry>* mapData)
    : m_mapData(mapData)
    , m_index(-1)
{
    internalIncrement();
}

template<typename Entry>
ALWAYS_INLINE MapDataImpl<Entry>::const_iterator::~const_iterator()
{
    m_mapData->m_iteratorCount--;
}

template<typename Entry>
ALWAYS_INLINE const WTF::KeyValuePair<JSValue, JSValue> MapDataImpl<Entry>::const_iterator::operator*() const
{
    Entry* entry = &m_mapData->m_entries[m_index];
    return WTF::KeyValuePair<JSValue, JSValue>(entry->key().get(), entry->value().get());
}

template<typename Entry>
ALWAYS_INLINE auto MapDataImpl<Entry>::const_iterator::end(const MapDataImpl<Entry>* mapData) -> const_iterator
{
    const_iterator result(mapData);
    result.m_index = -1;
    return result;
}

template<typename Entry>
ALWAYS_INLINE bool MapDataImpl<Entry>::const_iterator::operator!=(const const_iterator& other)
{
    ASSERT(other.m_mapData == m_mapData);
    if (atEnd() && other.atEnd())
        return false;
    return m_index != other.m_index;
}

template<typename Entry>
ALWAYS_INLINE bool MapDataImpl<Entry>::const_iterator::operator==(const const_iterator& other)
{
    return !(*this != other);
}

}

#endif /* !defined(MapData_h) */
