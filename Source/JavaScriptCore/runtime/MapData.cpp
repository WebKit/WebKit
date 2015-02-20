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

#include "config.h"
#include "MapData.h"

#include "CopiedAllocator.h"
#include "CopyVisitorInlines.h"
#include "ExceptionHelpers.h"
#include "JSCJSValueInlines.h"
#include "SlotVisitorInlines.h"

#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MathExtras.h>


namespace JSC {

const ClassInfo MapData::s_info = { "MapData", 0, 0, 0, CREATE_METHOD_TABLE(MapData) };

static const int32_t minimumMapSize = 8;

MapData::MapData(VM& vm)
    : Base(vm, vm.mapDataStructure.get())
    , m_capacity(0)
    , m_size(0)
    , m_deletedCount(0)
    , m_iteratorCount(0)
    , m_entries(0)
{
}

MapData::Entry* MapData::find(CallFrame* callFrame, KeyType key)
{
    if (key.value.isString()) {
        auto iter = m_stringKeyedTable.find(asString(key.value)->value(callFrame).impl());
        if (iter == m_stringKeyedTable.end())
            return 0;
        return &m_entries[iter->value];
    }
    if (key.value.isCell()) {
        auto iter = m_cellKeyedTable.find(key.value.asCell());
        if (iter == m_cellKeyedTable.end())
            return 0;
        return &m_entries[iter->value];
    }

    auto iter = m_valueKeyedTable.find(JSValue::encode(key.value));
    if (iter == m_valueKeyedTable.end())
        return 0;
    return &m_entries[iter->value];
}

bool MapData::contains(CallFrame* callFrame, KeyType key)
{
    return find(callFrame, key);
}

template <typename Map, typename Key> MapData::Entry* MapData::add(CallFrame* callFrame, Map& map, Key key, KeyType keyValue)
{
    typename Map::iterator location = map.find(key);
    if (location != map.end())
        return &m_entries[location->value];
    
    if (!ensureSpaceForAppend(callFrame))
        return 0;

    auto result = map.add(key, m_size);
    RELEASE_ASSERT(result.isNewEntry);
    Entry* entry = &m_entries[m_size++];
    new (entry) Entry();
    entry->key.set(callFrame->vm(), this, keyValue.value);
    return entry;
}

void MapData::set(CallFrame* callFrame, KeyType key, JSValue value)
{
    Entry* location = add(callFrame, key);
    if (!location)
        return;
    location->value.set(callFrame->vm(), this, value);
}
    
MapData::Entry* MapData::add(CallFrame* callFrame, KeyType key)
{
    if (key.value.isString())
        return add(callFrame, m_stringKeyedTable, asString(key.value)->value(callFrame).impl(), key);
    if (key.value.isCell())
        return add(callFrame, m_cellKeyedTable, key.value.asCell(), key);
    return add(callFrame, m_valueKeyedTable, JSValue::encode(key.value), key);
}

JSValue MapData::get(CallFrame* callFrame, KeyType key)
{
    if (Entry* entry = find(callFrame, key))
        return entry->value.get();
    return JSValue();
}

bool MapData::remove(CallFrame* callFrame, KeyType key)
{
    int32_t location;
    if (key.value.isString()) {
        auto iter = m_stringKeyedTable.find(asString(key.value)->value(callFrame).impl());
        if (iter == m_stringKeyedTable.end())
            return false;
        location = iter->value;
        m_stringKeyedTable.remove(iter);
    } else if (key.value.isCell()) {
        auto iter = m_cellKeyedTable.find(key.value.asCell());
        if (iter == m_cellKeyedTable.end())
            return false;
        location = iter->value;
        m_cellKeyedTable.remove(iter);
    } else {
        auto iter = m_valueKeyedTable.find(JSValue::encode(key.value));
        if (iter == m_valueKeyedTable.end())
            return false;
        location = iter->value;
        m_valueKeyedTable.remove(iter);
    }
    m_entries[location].key.clear();
    m_entries[location].value.clear();
    m_deletedCount++;
    return true;
}

void MapData::replaceAndPackBackingStore(Entry* destination, int32_t newCapacity)
{
    ASSERT(shouldPack());
    int32_t newEnd = 0;
    RELEASE_ASSERT(newCapacity > 0);
    for (int32_t i = 0; i < m_size; i++) {
        Entry& entry = m_entries[i];
        if (!entry.key)
            continue;
        ASSERT(newEnd < newCapacity);
        destination[newEnd] = entry;

        // We overwrite the old entry with a forwarding index for the new entry,
        // so that we can fix up our hash tables below without doing additional
        // hash lookups
        entry.value.setWithoutWriteBarrier(jsNumber(newEnd));
        newEnd++;
    }

    // Fixup for the hashmaps
    for (auto ptr = m_valueKeyedTable.begin(); ptr != m_valueKeyedTable.end(); ++ptr)
        ptr->value = m_entries[ptr->value].value.get().asInt32();
    for (auto ptr = m_cellKeyedTable.begin(); ptr != m_cellKeyedTable.end(); ++ptr)
        ptr->value = m_entries[ptr->value].value.get().asInt32();
    for (auto ptr = m_stringKeyedTable.begin(); ptr != m_stringKeyedTable.end(); ++ptr)
        ptr->value = m_entries[ptr->value].value.get().asInt32();

    ASSERT((m_size - newEnd) == m_deletedCount);
    m_deletedCount = 0;

    m_capacity = newCapacity;
    m_size = newEnd;
    m_entries = destination;

}

void MapData::replaceBackingStore(Entry* destination, int32_t newCapacity)
{
    ASSERT(!shouldPack());
    RELEASE_ASSERT(newCapacity > 0);
    ASSERT(newCapacity >= m_capacity);
    memcpy(destination, m_entries, sizeof(Entry) * m_size);
    m_capacity = newCapacity;
    m_entries = destination;
}

CheckedBoolean MapData::ensureSpaceForAppend(CallFrame* callFrame)
{
    if (m_capacity > m_size)
        return true;

    size_t requiredSize = std::max(m_capacity + (m_capacity / 2) + 1, minimumMapSize);
    void* newStorage = 0;
    DeferGC defer(*callFrame->heap());
    if (!callFrame->heap()->tryAllocateStorage(this, requiredSize * sizeof(Entry), &newStorage)) {
        throwOutOfMemoryError(callFrame);
        return false;
    }
    Entry* newEntries = static_cast<Entry*>(newStorage);
    if (shouldPack())
        replaceAndPackBackingStore(newEntries, requiredSize);
    else
        replaceBackingStore(newEntries, requiredSize);
    callFrame->heap()->writeBarrier(this);
    return true;
}

void MapData::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    Base::visitChildren(cell, visitor);
    MapData* thisObject = jsCast<MapData*>(cell);
    Entry* entries = thisObject->m_entries;
    if (!entries)
        return;
    size_t size = thisObject->m_size;
    if (thisObject->m_deletedCount) {
        for (size_t i = 0; i < size; i++) {
            if (!entries[i].key)
                continue;
            visitor.append(&entries[i].key);
            visitor.append(&entries[i].value);
        }
    } else
        visitor.appendValues(&entries[0].key, size * (sizeof(Entry) / sizeof(WriteBarrier<Unknown>)));

    visitor.copyLater(thisObject, MapBackingStoreCopyToken, entries, thisObject->capacityInBytes());
}

void MapData::copyBackingStore(JSCell* cell, CopyVisitor& visitor, CopyToken token)
{
    MapData* thisObject = jsCast<MapData*>(cell);
    if (token == MapBackingStoreCopyToken && visitor.checkIfShouldCopy(thisObject->m_entries)) {
        Entry* oldEntries = thisObject->m_entries;
        Entry* newEntries = static_cast<Entry*>(visitor.allocateNewSpace(thisObject->capacityInBytes()));
        if (thisObject->shouldPack())
            thisObject->replaceAndPackBackingStore(newEntries, thisObject->m_capacity);
        else
            thisObject->replaceBackingStore(newEntries, thisObject->m_capacity);
        visitor.didCopy(oldEntries, thisObject->capacityInBytes());
    }
    Base::copyBackingStore(cell, visitor, token);
}

void MapData::destroy(JSCell* cell)
{
    static_cast<MapData*>(cell)->~MapData();
}

}
