/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "IndexValueStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBError.h"
#include "IDBKeyRangeData.h"

namespace WebCore {
namespace IDBServer {

IndexValueStore::IndexValueStore(bool unique)
    : m_unique(unique)
{
}

const IDBKeyData* IndexValueStore::lowestValueForKey(const IDBKeyData& key) const
{
    const auto& entry = m_records.get(key);
    if (!entry)
        return nullptr;

    return entry->getLowest();
}

uint64_t IndexValueStore::countForKey(const IDBKeyData& key) const
{
    const auto& entry = m_records.get(key);
    if (!entry)
        return 0;

    return entry->getCount();
}

bool IndexValueStore::contains(const IDBKeyData& key) const
{
    const auto& entry = m_records.get(key);
    if (!entry)
        return false;

    ASSERT(entry->getCount());

    return true;
}

IDBError IndexValueStore::addRecord(const IDBKeyData& indexKey, const IDBKeyData& valueKey)
{
    auto result = m_records.add(indexKey, nullptr);

    if (!result.isNewEntry && m_unique)
        return IDBError(IDBExceptionCode::ConstraintError);

    if (result.isNewEntry)
        result.iterator->value = std::make_unique<IndexValueEntry>(m_unique);

    result.iterator->value->addKey(valueKey);
    m_orderedKeys.insert(indexKey);

    return { };
}

void IndexValueStore::removeRecord(const IDBKeyData& indexKey, const IDBKeyData& valueKey)
{
    auto iterator = m_records.find(indexKey);
    if (!iterator->value)
        return;

    if (iterator->value->removeKey(valueKey))
        m_records.remove(iterator);
}

void IndexValueStore::removeEntriesWithValueKey(const IDBKeyData& valueKey)
{
    HashSet<IDBKeyData*> entryKeysToRemove;

    for (auto& entry : m_records) {
        if (entry.value->removeKey(valueKey))
            entryKeysToRemove.add(&entry.key);
    }

    for (auto* entry : entryKeysToRemove) {
        m_orderedKeys.erase(*entry);
        m_records.remove(*entry);
    }
}

IDBKeyData IndexValueStore::lowestKeyWithRecordInRange(const IDBKeyRangeData& range) const
{
    if (range.isExactlyOneKey())
        return m_records.contains(range.lowerKey) ? range.lowerKey : IDBKeyData();

    auto lowestInRange = m_orderedKeys.lower_bound(range.lowerKey);

    if (lowestInRange == m_orderedKeys.end())
        return { };

    if (range.lowerOpen && *lowestInRange == range.lowerKey)
        ++lowestInRange;

    if (lowestInRange == m_orderedKeys.end())
        return { };

    if (!range.upperKey.isNull()) {
        if (lowestInRange->compare(range.upperKey) > 0)
            return { };
        if (range.upperOpen && *lowestInRange == range.upperKey)
            return { };
    }

    return *lowestInRange;
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
