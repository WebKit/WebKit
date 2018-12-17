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
#include "MemoryObjectStore.h"

#if ENABLE(INDEXED_DATABASE)

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

MemoryIndex* MemoryObjectStore::indexForIdentifier(uint64_t identifier)
{
    ASSERT(identifier);
    return m_indexesByIdentifier.get(identifier);
}

void MemoryObjectStore::writeTransactionStarted(MemoryBackingStoreTransaction& transaction)
{
    LOG(IndexedDB, "MemoryObjectStore::writeTransactionStarted");

    ASSERT(!m_writeTransaction);
    m_writeTransaction = &transaction;
}

void MemoryObjectStore::writeTransactionFinished(MemoryBackingStoreTransaction& transaction)
{
    LOG(IndexedDB, "MemoryObjectStore::writeTransactionFinished");

    ASSERT_UNUSED(transaction, m_writeTransaction == &transaction);
    m_writeTransaction = nullptr;
}

IDBError MemoryObjectStore::createIndex(MemoryBackingStoreTransaction& transaction, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "MemoryObjectStore::createIndex");

    if (!m_writeTransaction || !m_writeTransaction->isVersionChange() || m_writeTransaction != &transaction)
        return IDBError(ConstraintError);

    ASSERT(!m_indexesByIdentifier.contains(info.identifier()));
    auto index = MemoryIndex::create(info, *this);

    // If there was an error populating the new index, then the current records in the object store violate its contraints
    auto error = populateIndexWithExistingRecords(index.get());
    if (!error.isNull())
        return error;

    m_info.addExistingIndex(info);
    transaction.addNewIndex(index.get());
    registerIndex(WTFMove(index));

    return IDBError { };
}

void MemoryObjectStore::maybeRestoreDeletedIndex(Ref<MemoryIndex>&& index)
{
    LOG(IndexedDB, "MemoryObjectStore::maybeRestoreDeletedIndex");

    if (m_info.hasIndex(index->info().name()))
        return;

    m_info.addExistingIndex(index->info());

    ASSERT(!m_indexesByIdentifier.contains(index->info().identifier()));
    index->clearIndexValueStore();
    auto error = populateIndexWithExistingRecords(index.get());

    // Since this index was installed in the object store before this transaction started,
    // assuming things were in a valid state then, we should definitely be able to successfully
    // repopulate the index with the object store's pre-transaction records.
    ASSERT_UNUSED(error, error.isNull());

    registerIndex(WTFMove(index));
}

RefPtr<MemoryIndex> MemoryObjectStore::takeIndexByIdentifier(uint64_t indexIdentifier)
{
    auto indexByIdentifier = m_indexesByIdentifier.take(indexIdentifier);
    if (!indexByIdentifier)
        return nullptr;

    auto index = m_indexesByName.take(indexByIdentifier->info().name());
    ASSERT(index);

    return index;
}

IDBError MemoryObjectStore::deleteIndex(MemoryBackingStoreTransaction& transaction, uint64_t indexIdentifier)
{
    LOG(IndexedDB, "MemoryObjectStore::deleteIndex");

    if (!m_writeTransaction || !m_writeTransaction->isVersionChange() || m_writeTransaction != &transaction)
        return IDBError(ConstraintError);
    
    auto index = takeIndexByIdentifier(indexIdentifier);
    ASSERT(index);
    if (!index)
        return IDBError(ConstraintError);

    m_info.deleteIndex(indexIdentifier);
    transaction.indexDeleted(*index);

    return IDBError { };
}

void MemoryObjectStore::deleteAllIndexes(MemoryBackingStoreTransaction& transaction)
{
    Vector<uint64_t> indexIdentifiers;
    indexIdentifiers.reserveInitialCapacity(m_indexesByName.size());

    for (auto& index : m_indexesByName.values())
        indexIdentifiers.uncheckedAppend(index->info().identifier());

    for (auto identifier : indexIdentifiers)
        deleteIndex(transaction, identifier);
}

bool MemoryObjectStore::containsRecord(const IDBKeyData& key)
{
    if (!m_keyValueStore)
        return false;

    return m_keyValueStore->contains(key);
}

void MemoryObjectStore::clear()
{
    LOG(IndexedDB, "MemoryObjectStore::clear");
    ASSERT(m_writeTransaction);

    m_writeTransaction->objectStoreCleared(*this, WTFMove(m_keyValueStore), WTFMove(m_orderedKeys));
    for (auto& index : m_indexesByIdentifier.values())
        index->objectStoreCleared();

    for (auto& cursor : m_cursors.values())
        cursor->objectStoreCleared();
}

void MemoryObjectStore::replaceKeyValueStore(std::unique_ptr<KeyValueMap>&& store, std::unique_ptr<IDBKeyDataSet>&& orderedKeys)
{
    ASSERT(m_writeTransaction);
    ASSERT(m_writeTransaction->isAborting());

    m_keyValueStore = WTFMove(store);
    m_orderedKeys = WTFMove(orderedKeys);
}

void MemoryObjectStore::deleteRecord(const IDBKeyData& key)
{
    LOG(IndexedDB, "MemoryObjectStore::deleteRecord");

    ASSERT(m_writeTransaction);

    if (!m_keyValueStore) {
        m_writeTransaction->recordValueChanged(*this, key, nullptr);
        return;
    }

    ASSERT(m_orderedKeys);

    auto iterator = m_keyValueStore->find(key);
    if (iterator == m_keyValueStore->end()) {
        m_writeTransaction->recordValueChanged(*this, key, nullptr);
        return;
    }

    m_writeTransaction->recordValueChanged(*this, key, &iterator->value);
    m_keyValueStore->remove(iterator);
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

IDBError MemoryObjectStore::addRecord(MemoryBackingStoreTransaction& transaction, const IDBKeyData& keyData, const IDBValue& value)
{
    LOG(IndexedDB, "MemoryObjectStore::addRecord");

    ASSERT(m_writeTransaction);
    ASSERT_UNUSED(transaction, m_writeTransaction == &transaction);
    ASSERT(!m_keyValueStore || !m_keyValueStore->contains(keyData));
    ASSERT(!m_orderedKeys || m_orderedKeys->find(keyData) == m_orderedKeys->end());

    if (!m_keyValueStore) {
        ASSERT(!m_orderedKeys);
        m_keyValueStore = std::make_unique<KeyValueMap>();
        m_orderedKeys = std::make_unique<IDBKeyDataSet>();
    }

    auto mapResult = m_keyValueStore->set(keyData, value.data());
    ASSERT(mapResult.isNewEntry);
    auto listResult = m_orderedKeys->insert(keyData);
    ASSERT(listResult.second);

    // If there was an error indexing this addition, then revert it.
    auto error = updateIndexesForPutRecord(keyData, value.data());
    if (!error.isNull()) {
        m_keyValueStore->remove(mapResult.iterator);
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

IDBError MemoryObjectStore::updateIndexesForPutRecord(const IDBKeyData& key, const ThreadSafeDataBuffer& value)
{
    JSLockHolder locker(UniqueIDBDatabase::databaseThreadVM());

    auto jsValue = deserializeIDBValueToJSValue(UniqueIDBDatabase::databaseThreadExecState(), value);
    if (jsValue.isUndefinedOrNull())
        return IDBError { };

    IDBError error;
    Vector<std::pair<MemoryIndex*, IndexKey>> changedIndexRecords;

    for (auto& index : m_indexesByName.values()) {
        IndexKey indexKey;
        generateIndexKeyForValue(UniqueIDBDatabase::databaseThreadExecState(), index->info(), jsValue, indexKey);

        if (indexKey.isNull())
            continue;

        error = index->putIndexKey(key, indexKey);
        if (!error.isNull())
            break;

        changedIndexRecords.append(std::make_pair(index.get(), indexKey));
    }

    // If any of the index puts failed, revert all of the ones that went through.
    if (!error.isNull()) {
        for (auto& record : changedIndexRecords)
            record.first->removeRecord(key, record.second);
    }

    return error;
}

IDBError MemoryObjectStore::populateIndexWithExistingRecords(MemoryIndex& index)
{
    if (!m_keyValueStore)
        return IDBError { };

    JSLockHolder locker(UniqueIDBDatabase::databaseThreadVM());

    for (const auto& iterator : *m_keyValueStore) {
        auto jsValue = deserializeIDBValueToJSValue(UniqueIDBDatabase::databaseThreadExecState(), iterator.value);
        if (jsValue.isUndefinedOrNull())
            return IDBError { };

        IndexKey indexKey;
        generateIndexKeyForValue(UniqueIDBDatabase::databaseThreadExecState(), index.info(), jsValue, indexKey);

        if (indexKey.isNull())
            continue;

        IDBError error = index.putIndexKey(iterator.key, indexKey);
        if (!error.isNull())
            return error;
    }

    return IDBError { };
}

uint64_t MemoryObjectStore::countForKeyRange(uint64_t indexIdentifier, const IDBKeyRangeData& inRange) const
{
    LOG(IndexedDB, "MemoryObjectStore::countForKeyRange");

    if (indexIdentifier) {
        auto* index = m_indexesByIdentifier.get(indexIdentifier);
        ASSERT(index);
        return index->countForKeyRange(inRange);
    }

    if (!m_keyValueStore)
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
    if (!m_keyValueStore)
        return { };

    return m_keyValueStore->get(key);
}

ThreadSafeDataBuffer MemoryObjectStore::valueForKeyRange(const IDBKeyRangeData& keyRangeData) const
{
    LOG(IndexedDB, "MemoryObjectStore::valueForKey");

    IDBKeyData key = lowestKeyWithRecordInRange(keyRangeData);
    if (key.isNull())
        return ThreadSafeDataBuffer();

    ASSERT(m_keyValueStore);
    return m_keyValueStore->get(key);
}

void MemoryObjectStore::getAllRecords(const IDBKeyRangeData& keyRangeData, std::optional<uint32_t> count, IndexedDB::GetAllType type, IDBGetAllResult& result) const
{
    result = { type };

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

        if (type == IndexedDB::GetAllType::Keys)
            result.addKey(WTFMove(key));
        else
            result.addValue(valueForKey(key));

        ++currentCount;
    }
}

IDBGetResult MemoryObjectStore::indexValueForKeyRange(uint64_t indexIdentifier, IndexedDB::IndexRecordType recordType, const IDBKeyRangeData& range) const
{
    LOG(IndexedDB, "MemoryObjectStore::indexValueForKeyRange");

    auto* index = m_indexesByIdentifier.get(indexIdentifier);
    ASSERT(index);
    return index->getResultForKeyRange(recordType, range);
}

IDBKeyData MemoryObjectStore::lowestKeyWithRecordInRange(const IDBKeyRangeData& keyRangeData) const
{
    if (!m_keyValueStore)
        return { };

    if (keyRangeData.isExactlyOneKey() && m_keyValueStore->contains(keyRangeData.lowerKey))
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
        if (lowestInRange->compare(keyRangeData.upperKey) > 0)
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
    m_indexesByName.set(index->info().name(), &index.get());
    m_indexesByIdentifier.set(identifier, WTFMove(index));
}

void MemoryObjectStore::unregisterIndex(MemoryIndex& index)
{
    ASSERT(m_indexesByIdentifier.contains(index.info().identifier()));
    ASSERT(m_indexesByName.contains(index.info().name()));

    m_indexesByName.remove(index.info().name());
    m_indexesByIdentifier.remove(index.info().identifier());
}

MemoryObjectStoreCursor* MemoryObjectStore::maybeOpenCursor(const IDBCursorInfo& info)
{
    auto result = m_cursors.add(info.identifier(), nullptr);
    if (!result.isNewEntry)
        return nullptr;

    result.iterator->value = std::make_unique<MemoryObjectStoreCursor>(*this, info);
    return result.iterator->value.get();
}

void MemoryObjectStore::renameIndex(MemoryIndex& index, const String& newName)
{
    ASSERT(m_indexesByName.get(index.info().name()) == &index);
    ASSERT(!m_indexesByName.contains(newName));
    ASSERT(m_info.infoForExistingIndex(index.info().name()));

    m_info.infoForExistingIndex(index.info().name())->rename(newName);
    m_indexesByName.set(newName, m_indexesByName.take(index.info().name()));
    index.rename(newName);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
