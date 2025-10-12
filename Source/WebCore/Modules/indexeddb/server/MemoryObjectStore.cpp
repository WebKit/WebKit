/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "MemoryObjectStore.h"

#include "IDBBindingUtilities.h"
#include "IDBError.h"
#include "IDBGetAllResult.h"
#include "IDBKeyRangeData.h"
#include "IDBValue.h"
#include "IndexKey.h"
#include "Logging.h"
#include "MemoryBackingStoreTransaction.h"
#include "UniqueIDBDatabase.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSLock.h>

namespace WebCore {
using namespace JSC;
namespace IDBServer {

Ref<MemoryObjectStore> MemoryObjectStore::create(const IDBObjectStoreInfo& info)
{
    return adoptRef(*new MemoryObjectStore(info));
}

MemoryObjectStore::MemoryObjectStore(const IDBObjectStoreInfo& info)
    : m_info(info)
{
}

MemoryObjectStore::~MemoryObjectStore()
{
    m_writeTransaction = nullptr;
}

MemoryIndex* MemoryObjectStore::indexForIdentifier(IDBIndexIdentifier identifier)
{
    return m_indexesByIdentifier.get(identifier);
}

void MemoryObjectStore::transactionFinished(MemoryBackingStoreTransaction& transaction)
{
    if (transaction.isWriting())
        writeTransactionFinished(transaction);

    m_cursors.removeIf([&](auto& pair) {
        return pair.value->transaction() == &transaction;
    });

    for (auto& index : m_indexesByIdentifier.values())
        index->transactionFinished(transaction);
}

void MemoryObjectStore::writeTransactionStarted(MemoryBackingStoreTransaction& transaction)
{
    LOG(IndexedDB, "MemoryObjectStore::writeTransactionStarted");

    ASSERT(!m_writeTransaction);
    m_writeTransaction = transaction;
    m_keyGeneratorValueBeforeTransaction = m_keyGeneratorValue;

    for (auto& index : m_indexesByIdentifier.values())
        index->writeTransactionStarted(transaction);
}

void MemoryObjectStore::transactionAborted(MemoryBackingStoreTransaction& transaction)
{
    LOG(IndexedDB, "MemoryObjectStore::writeTransactionAborted");

    if (m_writeTransaction != &transaction)
        return;

    auto transactionModifiedRecords = std::exchange(m_transactionModifiedRecords, { });
    for (auto& [key, value] : transactionModifiedRecords) {
        deleteRecord(key);
        if (value.data())
            addRecordWithoutUpdatingIndexes(transaction, key, { value });
    }

    m_keyGeneratorValue = m_keyGeneratorValueBeforeTransaction;

    for (auto& index : m_indexesByIdentifier.values())
        index->transactionAborted(transaction);
}

void MemoryObjectStore::writeTransactionFinished(MemoryBackingStoreTransaction& transaction)
{
    LOG(IndexedDB, "MemoryObjectStore::writeTransactionFinished");

    ASSERT_UNUSED(transaction, m_writeTransaction == &transaction);
    m_writeTransaction = nullptr;

    m_transactionModifiedRecords.clear();
    for (auto& index : m_indexesByIdentifier.values())
        index->writeTransactionFinished(transaction);
}

MemoryBackingStoreTransaction* MemoryObjectStore::writeTransaction()
{
    return m_writeTransaction.get();
}

IDBError MemoryObjectStore::addIndex(MemoryBackingStoreTransaction& transaction, const IDBIndexInfo& indexInfo)
{
    if (!m_writeTransaction || !m_writeTransaction->isVersionChange() || m_writeTransaction != &transaction)
        return IDBError { ExceptionCode::ConstraintError, "Transaction state is invalid."_s };

    if (m_indexesByIdentifier.contains(indexInfo.identifier()))
        return IDBError { ExceptionCode::ConstraintError, "Index identifier already exists"_s };

    auto index = MemoryIndex::create(indexInfo, *this);
    index->writeTransactionStarted(transaction);
    m_info.addExistingIndex(indexInfo);
    transaction.addNewIndex(index.get());
    registerIndex(WTFMove(index));

    return IDBError { };
}

void MemoryObjectStore::revertAddIndex(MemoryBackingStoreTransaction& transaction, IDBIndexIdentifier indexIdentifier)
{
    if (!m_writeTransaction || !m_writeTransaction->isVersionChange() || m_writeTransaction != &transaction)
        return;

    m_info.deleteIndex(indexIdentifier);
    if (RefPtr index = takeIndexByIdentifier(indexIdentifier))
        transaction.removeNewIndex(*index);
}

IDBError MemoryObjectStore::updateIndexRecordsWithIndexKey(MemoryBackingStoreTransaction& transaction, const IDBIndexInfo& indexInfo, const IDBKeyData& key, const IndexKey& indexKey)
{
    if (!m_writeTransaction || !m_writeTransaction->isVersionChange() || m_writeTransaction != &transaction)
        return IDBError { ExceptionCode::ConstraintError, "Transaction state is invalid."_s };

    RefPtr index = m_indexesByIdentifier.get(indexInfo.identifier());
    if (!index)
        return IDBError { ExceptionCode::ConstraintError, "Index does not exist."_s };

    if (indexKey.isNull())
        return IDBError { };

    return index->putIndexKey(key, indexKey);
}

void MemoryObjectStore::forEachRecord(Function<void(const IDBKeyData& key, const IDBValue& value)>&& apply)
{
    for (auto& [key, value] : m_keyValueStore)
        apply(key, value);
}

void MemoryObjectStore::maybeRestoreDeletedIndex(Ref<MemoryIndex>&& index)
{
    LOG(IndexedDB, "MemoryObjectStore::maybeRestoreDeletedIndex");

    if (m_info.hasIndex(index->info().name()))
        return;

    m_info.addExistingIndex(index->info());

    ASSERT(!m_indexesByIdentifier.contains(index->info().identifier()));
    registerIndex(WTFMove(index));
}

RefPtr<MemoryIndex> MemoryObjectStore::takeIndexByIdentifier(IDBIndexIdentifier indexIdentifier)
{
    auto indexByIdentifier = m_indexesByIdentifier.take(indexIdentifier);
    if (!indexByIdentifier)
        return nullptr;

    auto index = m_indexesByName.take(indexByIdentifier->info().name());
    ASSERT(index);

    return index;
}

IDBError MemoryObjectStore::deleteIndex(MemoryBackingStoreTransaction& transaction, IDBIndexIdentifier indexIdentifier)
{
    LOG(IndexedDB, "MemoryObjectStore::deleteIndex");

    if (!m_writeTransaction || !m_writeTransaction->isVersionChange() || m_writeTransaction != &transaction)
        return IDBError(ExceptionCode::ConstraintError);
    
    auto index = takeIndexByIdentifier(indexIdentifier);
    ASSERT(index);
    if (!index)
        return IDBError(ExceptionCode::ConstraintError);

    m_info.deleteIndex(indexIdentifier);
    transaction.indexDeleted(*index);

    return IDBError { };
}

void MemoryObjectStore::deleteAllIndexes(MemoryBackingStoreTransaction& transaction)
{
    auto indexIdentifiers = WTF::map(m_indexesByName, [](auto& entry) {
        return entry.value->info().identifier();
    });
    for (auto identifier : indexIdentifiers)
        deleteIndex(transaction, identifier);
}

bool MemoryObjectStore::containsRecord(const IDBKeyData& key)
{
    return m_keyValueStore.contains(key);
}

void MemoryObjectStore::clear()
{
    LOG(IndexedDB, "MemoryObjectStore::clear");
    ASSERT(m_writeTransaction);

    for (auto& [key, value] : std::exchange(m_keyValueStore, { }))
        m_transactionModifiedRecords.add(key, value);
    m_orderedKeys = nullptr;

    for (auto& index : m_indexesByIdentifier.values())
        index->objectStoreCleared();

    for (auto& cursor : m_cursors.values())
        cursor->objectStoreCleared();
}

void MemoryObjectStore::deleteRecord(const IDBKeyData& key)
{
    LOG(IndexedDB, "MemoryObjectStore::deleteRecord");

    ASSERT(m_writeTransaction);

    auto iterator = m_keyValueStore.find(key);
    if (iterator == m_keyValueStore.end())
        return;

    ASSERT(m_orderedKeys);

    if (!m_writeTransaction->isAborting())
        m_transactionModifiedRecords.add(key, iterator->value);

    m_keyValueStore.remove(iterator);
    m_orderedKeys->erase(key);

    updateIndexesForDeleteRecord(key);
    updateCursorsForDeleteRecord(key);
}

void MemoryObjectStore::deleteRange(const IDBKeyRangeData& inputRange)
{
    LOG(IndexedDB, "MemoryObjectStore::deleteRange");

    ASSERT(m_writeTransaction);

    if (inputRange.isExactlyOneKey()) {
        deleteRecord(inputRange.lowerKey);
        return;
    }

    IDBKeyRangeData range = inputRange;
    while (true) {
        auto key = lowestKeyWithRecordInRange(range);
        if (key.isNull())
            break;

        deleteRecord(key);

        range.lowerKey = key;
        range.lowerOpen = true;
    }
}

void MemoryObjectStore::addRecordWithoutUpdatingIndexes(MemoryBackingStoreTransaction& transaction, const IDBKeyData& keyData, const IDBValue& value)
{
    if (m_writeTransaction != &transaction)
        return;

    ASSERT(m_writeTransaction && m_writeTransaction->isAborting());

    if (!m_orderedKeys)
        m_orderedKeys = makeUniqueWithoutFastMallocCheck<IDBKeyDataSet>();

    auto mapResult = m_keyValueStore.add(keyData, value.data());
    ASSERT_UNUSED(mapResult, mapResult.isNewEntry);
    auto listResult = m_orderedKeys->insert(keyData);
    ASSERT_UNUSED(listResult, listResult.second);

    updateCursorsForPutRecord(listResult.first);
}

IDBError MemoryObjectStore::addRecord(MemoryBackingStoreTransaction& transaction, const IDBKeyData& keyData, const IndexIDToIndexKeyMap& indexKeys, const IDBValue& value)
{
    LOG(IndexedDB, "MemoryObjectStore::addRecord");

    ASSERT(m_writeTransaction);
    ASSERT_UNUSED(transaction, m_writeTransaction == &transaction);
    ASSERT(!m_keyValueStore.contains(keyData));
    ASSERT(!m_orderedKeys || m_orderedKeys->find(keyData) == m_orderedKeys->end());

    if (!m_orderedKeys)
        m_orderedKeys = makeUniqueWithoutFastMallocCheck<IDBKeyDataSet>();

    if (!m_writeTransaction->isAborting()) {
        auto iterator = m_keyValueStore.find(keyData);
        m_transactionModifiedRecords.add(keyData, iterator != m_keyValueStore.end() ? iterator->value : ThreadSafeDataBuffer { });
    }

    auto mapResult = m_keyValueStore.add(keyData, value.data());
    ASSERT(mapResult.isNewEntry);
    auto listResult = m_orderedKeys->insert(keyData);
    ASSERT(listResult.second);

    // If there was an error indexing this addition, then revert it.
    auto error = updateIndexesForPutRecord(keyData, indexKeys);
    if (!error.isNull()) {
        m_keyValueStore.remove(mapResult.iterator);
        m_orderedKeys->erase(listResult.first);
    } else
        updateCursorsForPutRecord(listResult.first);

    return error;
}

void MemoryObjectStore::updateCursorsForPutRecord(IDBKeyDataSet::iterator iterator)
{
    for (auto& cursor : m_cursors.values())
        cursor->keyAdded(iterator);
}

void MemoryObjectStore::updateCursorsForDeleteRecord(const IDBKeyData& key)
{
    for (auto& cursor : m_cursors.values())
        cursor->keyDeleted(key);
}

void MemoryObjectStore::updateIndexesForDeleteRecord(const IDBKeyData& value)
{
    for (auto& index : m_indexesByName.values())
        index->removeEntriesWithValueKey(value);
}

IDBError MemoryObjectStore::updateIndexesForPutRecord(const IDBKeyData& key, const IndexIDToIndexKeyMap& indexKeys)
{
    IDBError error;
    Vector<std::pair<RefPtr<MemoryIndex>, IndexKey>> changedIndexRecords;

    for (const auto& [indexID, indexKey] : indexKeys) {
        RefPtr index = m_indexesByIdentifier.get(indexID);
        ASSERT(index);
        if (!index) {
            error = IDBError { ExceptionCode::InvalidStateError, "Missing index metadata"_s };
            break;
        }

        error = index->putIndexKey(key, indexKey);
        if (!error.isNull())
            break;

        changedIndexRecords.append(std::make_pair(WTFMove(index), indexKey));
    }

    // If any of the index puts failed, revert all of the ones that went through.
    if (!error.isNull()) {
        for (auto& record : changedIndexRecords)
            Ref { *record.first }->removeRecord(key, record.second);
    }

    return error;
}

uint64_t MemoryObjectStore::countForKeyRange(std::optional<IDBIndexIdentifier> indexIdentifier, const IDBKeyRangeData& inRange) const
{
    LOG(IndexedDB, "MemoryObjectStore::countForKeyRange");

    if (indexIdentifier) {
        RefPtr index = m_indexesByIdentifier.get(*indexIdentifier);
        ASSERT(index);
        return index->countForKeyRange(inRange);
    }

    if (m_keyValueStore.isEmpty())
        return 0;

    uint64_t count = 0;
    IDBKeyRangeData range = inRange;
    while (true) {
        auto key = lowestKeyWithRecordInRange(range);
        if (key.isNull())
            break;

        ++count;
        range.lowerKey = key;
        range.lowerOpen = true;
    }

    return count;
}

ThreadSafeDataBuffer MemoryObjectStore::valueForKey(const IDBKeyData& key) const
{
    return m_keyValueStore.get(key);
}

ThreadSafeDataBuffer MemoryObjectStore::valueForKeyRange(const IDBKeyRangeData& keyRangeData) const
{
    LOG(IndexedDB, "MemoryObjectStore::valueForKey");

    IDBKeyData key = lowestKeyWithRecordInRange(keyRangeData);
    if (key.isNull())
        return ThreadSafeDataBuffer();

    return m_keyValueStore.get(key);
}

void MemoryObjectStore::getAllRecords(const IDBKeyRangeData& keyRangeData, std::optional<uint32_t> count, IndexedDB::GetAllType type, IDBGetAllResult& result) const
{
    result = { type, m_info.keyPath() };

    uint32_t targetCount;
    if (count && count.value())
        targetCount = count.value();
    else
        targetCount = std::numeric_limits<uint32_t>::max();

    IDBKeyRangeData range = keyRangeData;
    uint32_t currentCount = 0;
    while (currentCount < targetCount) {
        IDBKeyData key = lowestKeyWithRecordInRange(range);
        if (key.isNull())
            return;

        range.lowerKey = key;
        range.lowerOpen = true;
        if (type == IndexedDB::GetAllType::Values)
            result.addValue(valueForKey(key));
        result.addKey(WTFMove(key));

        ++currentCount;
    }
}

IDBGetResult MemoryObjectStore::indexValueForKeyRange(IDBIndexIdentifier indexIdentifier, IndexedDB::IndexRecordType recordType, const IDBKeyRangeData& range) const
{
    LOG(IndexedDB, "MemoryObjectStore::indexValueForKeyRange");

    RefPtr index = m_indexesByIdentifier.get(indexIdentifier);
    ASSERT(index);
    return index->getResultForKeyRange(recordType, range);
}

IDBKeyData MemoryObjectStore::lowestKeyWithRecordInRange(const IDBKeyRangeData& keyRangeData) const
{
    if (m_keyValueStore.isEmpty())
        return { };

    if (keyRangeData.isExactlyOneKey() && m_keyValueStore.contains(keyRangeData.lowerKey))
        return keyRangeData.lowerKey;

    ASSERT(m_orderedKeys);

    auto lowestInRange = m_orderedKeys->lower_bound(keyRangeData.lowerKey);

    if (lowestInRange == m_orderedKeys->end())
        return { };

    if (keyRangeData.lowerOpen && *lowestInRange == keyRangeData.lowerKey)
        ++lowestInRange;

    if (lowestInRange == m_orderedKeys->end())
        return { };

    if (!keyRangeData.upperKey.isNull()) {
        if (*lowestInRange > keyRangeData.upperKey)
            return { };
        if (keyRangeData.upperOpen && *lowestInRange == keyRangeData.upperKey)
            return { };
    }

    return *lowestInRange;
}

void MemoryObjectStore::registerIndex(Ref<MemoryIndex>&& index)
{
    ASSERT(!m_indexesByIdentifier.contains(index->info().identifier()));
    ASSERT(!m_indexesByName.contains(index->info().name()));

    auto identifier = index->info().identifier();
    m_indexesByName.add(index->info().name(), &index.get());
    m_indexesByIdentifier.add(identifier, WTFMove(index));
}

MemoryObjectStoreCursor* MemoryObjectStore::maybeOpenCursor(const IDBCursorInfo& info, MemoryBackingStoreTransaction& transaction)
{
    if (transaction.isWriting() && m_writeTransaction != &transaction)
        return nullptr;

    auto result = m_cursors.add(info.identifier(), nullptr);
    if (!result.isNewEntry)
        return nullptr;

    result.iterator->value = MemoryObjectStoreCursor::create(*this, info, transaction);
    return result.iterator->value.get();
}

void MemoryObjectStore::renameIndex(MemoryIndex& index, const String& newName)
{
    ASSERT(m_indexesByName.get(index.info().name()) == &index);
    ASSERT(!m_indexesByName.contains(newName));
    ASSERT(m_info.infoForExistingIndex(index.info().name()));
    ASSERT(m_info.infoForExistingIndex(index.info().identifier()) == m_info.infoForExistingIndex(index.info().name()));

    m_info.infoForExistingIndex(index.info().identifier())->rename(newName);
    m_indexesByName.add(newName, m_indexesByName.take(index.info().name()));
    index.rename(newName);
}

} // namespace IDBServer
} // namespace WebCore
