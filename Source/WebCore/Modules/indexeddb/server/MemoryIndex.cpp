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
#include "MemoryIndex.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBError.h"
#include "IDBGetAllResult.h"
#include "IDBGetResult.h"
#include "IDBKeyRangeData.h"
#include "IndexKey.h"
#include "Logging.h"
#include "MemoryBackingStoreTransaction.h"
#include "MemoryIndexCursor.h"
#include "MemoryObjectStore.h"
#include "ThreadSafeDataBuffer.h"

namespace WebCore {
namespace IDBServer {

Ref<MemoryIndex> MemoryIndex::create(const IDBIndexInfo& info, MemoryObjectStore& objectStore)
{
    return adoptRef(*new MemoryIndex(info, objectStore));
}

MemoryIndex::MemoryIndex(const IDBIndexInfo& info, MemoryObjectStore& objectStore)
    : m_info(info)
    , m_objectStore(objectStore)
{
}

MemoryIndex::~MemoryIndex() = default;

void MemoryIndex::cursorDidBecomeClean(MemoryIndexCursor& cursor)
{
    m_cleanCursors.add(&cursor);
}

void MemoryIndex::cursorDidBecomeDirty(MemoryIndexCursor& cursor)
{
    m_cleanCursors.remove(&cursor);
}

void MemoryIndex::objectStoreCleared()
{
    auto transaction = m_objectStore.writeTransaction();
    ASSERT(transaction);

    transaction->indexCleared(*this, WTFMove(m_records));

    notifyCursorsOfAllRecordsChanged();
}

void MemoryIndex::notifyCursorsOfValueChange(const IDBKeyData& indexKey, const IDBKeyData& primaryKey)
{
    for (auto* cursor : copyToVector(m_cleanCursors))
        cursor->indexValueChanged(indexKey, primaryKey);
}

void MemoryIndex::notifyCursorsOfAllRecordsChanged()
{
    for (auto* cursor : copyToVector(m_cleanCursors))
        cursor->indexRecordsAllChanged();

    ASSERT(m_cleanCursors.isEmpty());
}

void MemoryIndex::clearIndexValueStore()
{
    ASSERT(m_objectStore.writeTransaction());
    ASSERT(m_objectStore.writeTransaction()->isAborting());

    m_records = nullptr;
}

void MemoryIndex::replaceIndexValueStore(std::unique_ptr<IndexValueStore>&& valueStore)
{
    ASSERT(m_objectStore.writeTransaction());
    ASSERT(m_objectStore.writeTransaction()->isAborting());

    m_records = WTFMove(valueStore);
}

IDBGetResult MemoryIndex::getResultForKeyRange(IndexedDB::IndexRecordType type, const IDBKeyRangeData& range) const
{
    LOG(IndexedDB, "MemoryIndex::getResultForKeyRange - %s", range.loggingString().utf8().data());

    if (!m_records)
        return { };

    IDBKeyData keyToLookFor;
    if (range.isExactlyOneKey())
        keyToLookFor = range.lowerKey;
    else
        keyToLookFor = m_records->lowestKeyWithRecordInRange(range);

    if (keyToLookFor.isNull())
        return { };

    const IDBKeyData* keyValue = m_records->lowestValueForKey(keyToLookFor);

    if (!keyValue)
        return { };

    return type == IndexedDB::IndexRecordType::Key ? IDBGetResult(*keyValue) : IDBGetResult(m_objectStore.valueForKeyRange(*keyValue));
}

uint64_t MemoryIndex::countForKeyRange(const IDBKeyRangeData& inRange)
{
    LOG(IndexedDB, "MemoryIndex::countForKeyRange");

    if (!m_records)
        return 0;

    uint64_t count = 0;
    IDBKeyRangeData range = inRange;
    while (true) {
        auto key = m_records->lowestKeyWithRecordInRange(range);
        if (key.isNull())
            break;

        count += m_records->countForKey(key);

        range.lowerKey = key;
        range.lowerOpen = true;
    }

    return count;
}

void MemoryIndex::getAllRecords(const IDBKeyRangeData& keyRangeData, Optional<uint32_t> count, IndexedDB::GetAllType type, IDBGetAllResult& result) const
{
    LOG(IndexedDB, "MemoryIndex::getAllRecords");

    result = { type };

    if (!m_records)
        return;

    uint32_t targetCount;
    if (count && count.value())
        targetCount = count.value();
    else
        targetCount = std::numeric_limits<uint32_t>::max();

    IDBKeyRangeData range = keyRangeData;
    uint32_t currentCount = 0;
    while (currentCount < targetCount) {
        auto key = m_records->lowestKeyWithRecordInRange(range);
        if (key.isNull())
            return;

        range.lowerKey = key;
        range.lowerOpen = true;

        auto allValues = m_records->allValuesForKey(key, targetCount - currentCount);
        for (auto& keyValue : allValues) {
            if (type == IndexedDB::GetAllType::Keys) {
                IDBKeyData keyCopy { keyValue };
                result.addKey(WTFMove(keyCopy));
            } else
                result.addValue(m_objectStore.valueForKeyRange(keyValue));
        }

        currentCount += allValues.size();
    }
}


IDBError MemoryIndex::putIndexKey(const IDBKeyData& valueKey, const IndexKey& indexKey)
{
    LOG(IndexedDB, "MemoryIndex::provisionalPutIndexKey");

    if (!m_records) {
        m_records = std::make_unique<IndexValueStore>(m_info.unique());
        notifyCursorsOfAllRecordsChanged();
    }

    if (!m_info.multiEntry()) {
        IDBKeyData key = indexKey.asOneKey();
        IDBError result = m_records->addRecord(key, valueKey);
        notifyCursorsOfValueChange(key, valueKey);
        return result;
    }

    Vector<IDBKeyData> keys = indexKey.multiEntry();

    if (m_info.unique()) {
        for (auto& key : keys) {
            if (m_records->contains(key))
                return IDBError(ConstraintError);
        }
    }

    for (auto& key : keys) {
        auto error = m_records->addRecord(key, valueKey);
        ASSERT_UNUSED(error, error.isNull());
        notifyCursorsOfValueChange(key, valueKey);
    }

    return IDBError { };
}

void MemoryIndex::removeRecord(const IDBKeyData& valueKey, const IndexKey& indexKey)
{
    LOG(IndexedDB, "MemoryIndex::removeRecord");

    ASSERT(m_records);

    if (!m_info.multiEntry()) {
        IDBKeyData key = indexKey.asOneKey();
        m_records->removeRecord(key, valueKey);
        notifyCursorsOfValueChange(key, valueKey);
        return;
    }

    Vector<IDBKeyData> keys = indexKey.multiEntry();
    for (auto& key : keys) {
        m_records->removeRecord(key, valueKey);
        notifyCursorsOfValueChange(key, valueKey);
    }
}

void MemoryIndex::removeEntriesWithValueKey(const IDBKeyData& valueKey)
{
    LOG(IndexedDB, "MemoryIndex::removeEntriesWithValueKey");

    if (!m_records)
        return;

    m_records->removeEntriesWithValueKey(*this, valueKey);
}

MemoryIndexCursor* MemoryIndex::maybeOpenCursor(const IDBCursorInfo& info)
{
    auto result = m_cursors.add(info.identifier(), nullptr);
    if (!result.isNewEntry)
        return nullptr;

    result.iterator->value = std::make_unique<MemoryIndexCursor>(*this, info);
    return result.iterator->value.get();
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
