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

MemoryObjectStore* MemoryIndex::objectStore()
{
    return m_objectStore.get();
}

void MemoryIndex::cursorDidBecomeClean(MemoryIndexCursor& cursor)
{
    m_cleanCursors.add(cursor);
}

void MemoryIndex::cursorDidBecomeDirty(MemoryIndexCursor& cursor)
{
    m_cleanCursors.remove(cursor);
}

void MemoryIndex::objectStoreCleared()
{
    if (CheckedPtr records = m_records.get()) {
        for (auto& key : records->allKeys()) {
            if (m_transactionModifiedRecords.contains(key))
                continue;
            if (auto valueKeys = records->valueKeys(key))
                m_transactionModifiedRecords.add(key, WTF::move(*valueKeys));
        }
    }
    m_records = nullptr;

    notifyCursorsOfAllRecordsChanged();
}

void MemoryIndex::notifyCursorsOfValueChange(const IDBKeyData& indexKey, const IDBKeyData& primaryKey)
{
    for (WeakPtr cursor : copyToVector(m_cleanCursors)) {
        if (RefPtr protectedCusor = cursor.get())
            protectedCusor->indexValueChanged(indexKey, primaryKey);
    }
}

void MemoryIndex::notifyCursorsOfAllRecordsChanged()
{
    for (WeakPtr cursor : copyToVector(m_cleanCursors)) {
        if (RefPtr protectedCusor = cursor.get())
            protectedCusor->indexRecordsAllChanged();
    }

    ASSERT(!m_cleanCursors.computeSize());
}

IDBGetResult MemoryIndex::getResultForKeyRange(IndexedDB::IndexRecordType type, const IDBKeyRangeData& range) const
{
    LOG(IndexedDB, "MemoryIndex::getResultForKeyRange - %s", range.loggingString().utf8().data());

    CheckedPtr records = m_records.get();
    if (!records)
        return { };

    IDBKeyData keyToLookFor;
    if (range.isExactlyOneKey())
        keyToLookFor = range.lowerKey;
    else
        keyToLookFor = records->lowestKeyWithRecordInRange(range);

    if (keyToLookFor.isNull())
        return { };

    const IDBKeyData* keyValue = records->lowestValueForKey(keyToLookFor);

    if (!keyValue)
        return { };

    RefPtr objectStore = m_objectStore.get();
    return type == IndexedDB::IndexRecordType::Key ? IDBGetResult(*keyValue) : IDBGetResult(*keyValue, objectStore->valueForKeyRange(*keyValue), objectStore->info().keyPath());
}

uint64_t MemoryIndex::countForKeyRange(const IDBKeyRangeData& inRange)
{
    LOG(IndexedDB, "MemoryIndex::countForKeyRange");

    CheckedPtr records = m_records.get();
    if (!records)
        return 0;

    uint64_t count = 0;
    IDBKeyRangeData range = inRange;
    while (true) {
        auto key = records->lowestKeyWithRecordInRange(range);
        if (key.isNull())
            break;

        count += records->countForKey(key);

        range.lowerKey = key;
        range.lowerOpen = true;
    }

    return count;
}

void MemoryIndex::getAllRecords(const IDBKeyRangeData& keyRangeData, std::optional<uint32_t> count, IndexedDB::GetAllType type, IDBGetAllResult& result) const
{
    LOG(IndexedDB, "MemoryIndex::getAllRecords");

    RefPtr objectStore = m_objectStore.get();
    result = { type, objectStore->info().keyPath() };

    CheckedPtr records = m_records.get();
    if (!records)
        return;

    uint32_t targetCount;
    if (count && count.value())
        targetCount = count.value();
    else
        targetCount = std::numeric_limits<uint32_t>::max();

    IDBKeyRangeData range = keyRangeData;
    uint32_t currentCount = 0;
    while (currentCount < targetCount) {
        auto key = records->lowestKeyWithRecordInRange(range);
        if (key.isNull())
            return;

        range.lowerKey = key;
        range.lowerOpen = true;

        auto allValues = records->allValuesForKey(key, targetCount - currentCount);
        for (auto& keyValue : allValues) {
            result.addKey(IDBKeyData(keyValue));
            if (type == IndexedDB::GetAllType::Values)
                result.addValue(objectStore->valueForKeyRange(keyValue));
        }

        currentCount += allValues.size();
    }
}

IDBError MemoryIndex::putIndexKey(const IDBKeyData& valueKey, const IndexKey& indexKey)
{
    LOG(IndexedDB, "MemoryIndex::provisionalPutIndexKey");

    if (!m_records) {
        m_records = makeUnique<IndexValueStore>(m_info.unique());
        notifyCursorsOfAllRecordsChanged();
    }

    if (!m_info.multiEntry()) {
        IDBKeyData key = indexKey.asOneKey();
        auto result = addIndexRecord(key, valueKey);
        notifyCursorsOfValueChange(key, valueKey);
        return result;
    }

    Vector<IDBKeyData> keys = indexKey.multiEntry();

    if (m_info.unique()) {
        CheckedRef records = *m_records;
        for (auto& key : keys) {
            if (records->contains(key))
                return IDBError(ExceptionCode::ConstraintError);
        }
    }

    for (auto& key : keys) {
        auto error = addIndexRecord(key, valueKey);
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
        removeIndexRecord(key, valueKey);
        notifyCursorsOfValueChange(key, valueKey);
        return;
    }

    Vector<IDBKeyData> keys = indexKey.multiEntry();
    for (auto& key : keys) {
        removeIndexRecord(key, valueKey);
        notifyCursorsOfValueChange(key, valueKey);
    }
}

void MemoryIndex::removeEntriesWithValueKey(const IDBKeyData& valueKey)
{
    LOG(IndexedDB, "MemoryIndex::removeEntriesWithValueKey");

    CheckedPtr records = m_records.get();
    if (!records)
        return;

    RELEASE_ASSERT(m_writeTransaction);
    if (!m_writeTransaction->isAborting()) {
        for (auto& indexKey : records->findKeysWithValueKey(valueKey)) {
            if (m_transactionModifiedRecords.contains(indexKey))
                continue;
            if (auto valueKeys = records->valueKeys(indexKey))
                m_transactionModifiedRecords.add(indexKey, WTF::move(*valueKeys));
        }
    }

    records->removeEntriesWithValueKey(*this, valueKey);
}

MemoryIndexCursor* MemoryIndex::maybeOpenCursor(const IDBCursorInfo& info, MemoryBackingStoreTransaction& transaction)
{
    if (transaction.isWriting()) {
        RefPtr objectStore = m_objectStore.get();
        if (!objectStore)
            return nullptr;

        if (objectStore->writeTransaction() != &transaction)
            return nullptr;
    }

    auto result = m_cursors.add(info.identifier(), nullptr);
    if (!result.isNewEntry)
        return nullptr;

    result.iterator->value = MemoryIndexCursor::create(*this, info, transaction);
    return result.iterator->value.get();
}

IDBError MemoryIndex::addIndexRecord(const IDBKeyData& indexKey, const IDBKeyData& valueKey)
{
    RELEASE_ASSERT(m_writeTransaction);

    if (!m_records)
        m_records = makeUnique<IndexValueStore>(m_info.unique());

    CheckedRef records = *m_records;
    if (!m_writeTransaction->isAborting() && !m_transactionModifiedRecords.contains(indexKey)) {
        if (!records->contains(indexKey))
            m_transactionModifiedRecords.add(indexKey, Vector<IDBKeyData> { });
        else if (auto valueKeys = records->valueKeys(indexKey))
            m_transactionModifiedRecords.add(indexKey, WTF::move(*valueKeys));
    }

    return records->addRecord(indexKey, valueKey);
}

void MemoryIndex::removeIndexRecord(const IDBKeyData& indexKey, const IDBKeyData& valueKey)
{
    CheckedPtr records = m_records.get();
    if (!records)
        return;

    RELEASE_ASSERT(m_writeTransaction);
    if (!m_writeTransaction->isAborting() && !m_transactionModifiedRecords.contains(indexKey)) {
        if (auto valueKeys = records->valueKeys(indexKey))
            m_transactionModifiedRecords.add(indexKey, WTF::move(*valueKeys));
    }

    return records->removeRecord(indexKey, valueKey);
}

void MemoryIndex::removeIndexRecord(const IDBKeyData& indexKey)
{
    if (CheckedPtr records = m_records.get())
        records->removeRecord(indexKey);
}

void MemoryIndex::writeTransactionStarted(MemoryBackingStoreTransaction& transaction)
{
    ASSERT(!m_writeTransaction);

    m_writeTransaction = transaction;
}

void MemoryIndex::writeTransactionFinished(MemoryBackingStoreTransaction& transaction)
{
    ASSERT_UNUSED(transaction, m_writeTransaction == &transaction);

    m_writeTransaction = nullptr;
    m_transactionModifiedRecords.clear();
}

void MemoryIndex::transactionAborted(MemoryBackingStoreTransaction& transaction)
{
    if (m_writeTransaction != &transaction)
        return;

    auto transactionModifiedRecords = std::exchange(m_transactionModifiedRecords, { });
    for (auto& [key, valueKeys] : transactionModifiedRecords) {
        removeIndexRecord(key);
        for (auto valueKey : valueKeys)
            addIndexRecord(key, valueKey);
    }
}

void MemoryIndex::transactionFinished(MemoryBackingStoreTransaction& transaction)
{
    m_cursors.removeIf([&](auto& pair) {
        return pair.value->transaction() == &transaction;
    });
}

} // namespace IDBServer
} // namespace WebCore
