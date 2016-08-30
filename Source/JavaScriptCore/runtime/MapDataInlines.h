/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include "CopiedAllocator.h"
#include "CopyVisitorInlines.h"
#include "ExceptionHelpers.h"
#include "JSCJSValueInlines.h"
#include "MapData.h"
#include "SlotVisitorInlines.h"

namespace JSC {

template<typename Entry, typename JSIterator>
inline void MapDataImpl<Entry, JSIterator>::clear()
{
    m_cellKeyedTable.clear();
    m_valueKeyedTable.clear();
    m_stringKeyedTable.clear();
    m_capacity = 0;
    m_size = 0;
    m_deletedCount = 0;
    m_entries.clear();
    m_iterators.forEach([](JSIterator* iterator, JSIterator*) {
        iterator->iteratorData()->didRemoveAllEntries();
    });
}

template<typename Entry, typename JSIterator>
inline Entry* MapDataImpl<Entry, JSIterator>::find(ExecState* exec, KeyType key)
{
    if (key.value.isString()) {
        auto iter = m_stringKeyedTable.find(asString(key.value)->value(exec).impl());
        if (iter == m_stringKeyedTable.end())
            return 0;
        return &m_entries.get()[iter->value];
    }
    if (key.value.isCell()) {
        auto iter = m_cellKeyedTable.find(key.value.asCell());
        if (iter == m_cellKeyedTable.end())
            return 0;
        return &m_entries.get()[iter->value];
    }

    auto iter = m_valueKeyedTable.find(JSValue::encode(key.value));
    if (iter == m_valueKeyedTable.end())
        return 0;
    return &m_entries.get()[iter->value];
}

template<typename Entry, typename JSIterator>
inline bool MapDataImpl<Entry, JSIterator>::contains(ExecState* exec, KeyType key)
{
    return find(exec, key);
}

template<typename Entry, typename JSIterator>
template <typename Map, typename Key>
inline Entry* MapDataImpl<Entry, JSIterator>::add(ExecState* exec, JSCell* owner, Map& map, Key key, KeyType keyValue)
{
    typename Map::iterator location = map.find(key);
    if (location != map.end())
        return &m_entries.get()[location->value];

    if (!ensureSpaceForAppend(exec, owner))
        return 0;

    auto result = map.add(key, m_size);
    RELEASE_ASSERT(result.isNewEntry);
    Entry* entry = &m_entries.get()[m_size++];
    new (entry) Entry();
    entry->setKey(exec->vm(), owner, keyValue.value);
    return entry;
}

template<typename Entry, typename JSIterator>
inline void MapDataImpl<Entry, JSIterator>::set(ExecState* exec, JSCell* owner, KeyType key, JSValue value)
{
    Entry* location = add(exec, owner, key);
    if (!location)
        return;
    location->setValue(exec->vm(), owner, value);
}

template<typename Entry, typename JSIterator>
inline Entry* MapDataImpl<Entry, JSIterator>::add(ExecState* exec, JSCell* owner, KeyType key)
{
    if (key.value.isString())
        return add(exec, owner, m_stringKeyedTable, asString(key.value)->value(exec).impl(), key);
    if (key.value.isCell())
        return add(exec, owner, m_cellKeyedTable, key.value.asCell(), key);
    return add(exec, owner, m_valueKeyedTable, JSValue::encode(key.value), key);
}

template<typename Entry, typename JSIterator>
inline JSValue MapDataImpl<Entry, JSIterator>::get(ExecState* exec, KeyType key)
{
    if (Entry* entry = find(exec, key))
        return entry->value().get();
    return JSValue();
}

template<typename Entry, typename JSIterator>
inline bool MapDataImpl<Entry, JSIterator>::remove(ExecState* exec, KeyType key)
{
    int32_t location;
    if (key.value.isString()) {
        auto iter = m_stringKeyedTable.find(asString(key.value)->value(exec).impl());
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
    m_entries.get()[location].clear();
    m_deletedCount++;
    return true;
}

template<typename Entry, typename JSIterator>
inline void MapDataImpl<Entry, JSIterator>::replaceAndPackBackingStore(Entry* destination, int32_t newCapacity)
{
    ASSERT(shouldPack());
    int32_t newEnd = 0;
    RELEASE_ASSERT(newCapacity > 0);
    for (int32_t i = 0; i < m_size; i++) {
        Entry& entry = m_entries.get()[i];
        if (!entry.key()) {
            m_iterators.forEach([newEnd](JSIterator* iterator, JSIterator*) {
                iterator->iteratorData()->didRemoveEntry(newEnd);
            });
            continue;
        }
        ASSERT(newEnd < newCapacity);
        destination[newEnd] = entry;

        // We overwrite the old entry with a forwarding index for the new entry,
        // so that we can fix up our hash tables below without doing additional
        // hash lookups
        entry.setKeyWithoutWriteBarrier(jsNumber(newEnd));
        newEnd++;
    }

    // Fixup for the hashmaps
    for (auto ptr = m_valueKeyedTable.begin(); ptr != m_valueKeyedTable.end(); ++ptr)
        ptr->value = m_entries.get()[ptr->value].key().get().asInt32();
    for (auto ptr = m_cellKeyedTable.begin(); ptr != m_cellKeyedTable.end(); ++ptr)
        ptr->value = m_entries.get()[ptr->value].key().get().asInt32();
    for (auto ptr = m_stringKeyedTable.begin(); ptr != m_stringKeyedTable.end(); ++ptr)
        ptr->value = m_entries.get()[ptr->value].key().get().asInt32();

    ASSERT((m_size - newEnd) == m_deletedCount);
    m_deletedCount = 0;

    m_capacity = newCapacity;
    m_size = newEnd;
    m_entries.setWithoutBarrier(destination);
}

template<typename Entry, typename JSIterator>
inline void MapDataImpl<Entry, JSIterator>::replaceBackingStore(Entry* destination, int32_t newCapacity)
{
    ASSERT(!shouldPack());
    RELEASE_ASSERT(newCapacity > 0);
    ASSERT(newCapacity >= m_capacity);
    memcpy(destination, m_entries.get(), sizeof(Entry) * m_size);
    m_capacity = newCapacity;
    m_entries.setWithoutBarrier(destination);
}

template<typename Entry, typename JSIterator>
inline CheckedBoolean MapDataImpl<Entry, JSIterator>::ensureSpaceForAppend(ExecState* exec, JSCell* owner)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_capacity > m_size)
        return true;

    size_t requiredSize = std::max(m_capacity + (m_capacity / 2) + 1, static_cast<int32_t>(minimumMapSize));
    void* newStorage = nullptr;
    DeferGC defer(*exec->heap());
    if (!exec->heap()->tryAllocateStorage(owner, requiredSize * sizeof(Entry), &newStorage)) {
        throwOutOfMemoryError(exec, scope);
        return false;
    }
    Entry* newEntries = static_cast<Entry*>(newStorage);
    if (shouldPack())
        replaceAndPackBackingStore(newEntries, requiredSize);
    else
        replaceBackingStore(newEntries, requiredSize);
    exec->heap()->writeBarrier(owner);
    return true;
}

template<typename Entry, typename JSIterator>
inline void MapDataImpl<Entry, JSIterator>::visitChildren(JSCell* owner, SlotVisitor& visitor)
{
    Entry* entries = m_entries.get();
    if (!entries)
        return;
    if (m_deletedCount) {
        for (int32_t i = 0; i < m_size; i++) {
            if (!entries[i].key())
                continue;
            entries[i].visitChildren(visitor);
        }
    } else {
        // Guaranteed that all fields of Entry type is WriteBarrier<Unknown>.
        visitor.appendValues(reinterpret_cast<WriteBarrier<Unknown>*>(&entries[0]), m_size * (sizeof(Entry) / sizeof(WriteBarrier<Unknown>)));
    }

    visitor.copyLater(owner, MapBackingStoreCopyToken, m_entries.get(), capacityInBytes());
}

template<typename Entry, typename JSIterator>
inline void MapDataImpl<Entry, JSIterator>::copyBackingStore(CopyVisitor& visitor, CopyToken token)
{
    if (token == MapBackingStoreCopyToken && visitor.checkIfShouldCopy(m_entries.get())) {
        Entry* oldEntries = m_entries.get();
        Entry* newEntries = static_cast<Entry*>(visitor.allocateNewSpace(capacityInBytes()));
        if (shouldPack())
            replaceAndPackBackingStore(newEntries, m_capacity);
        else
            replaceBackingStore(newEntries, m_capacity);
        visitor.didCopy(oldEntries, capacityInBytes());
    }
}

template<typename Entry, typename JSIterator>
inline auto MapDataImpl<Entry, JSIterator>::createIteratorData(JSIterator* iterator) -> IteratorData
{
    m_iterators.set(iterator, iterator);
    return IteratorData(this);
}

}
