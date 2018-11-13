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
#include "MemoryIDBBackingStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorInfo.h"
#include "IDBGetAllRecordsData.h"
#include "IDBGetRecordData.h"
#include "IDBGetResult.h"
#include "IDBIndexInfo.h"
#include "IDBIterateCursorData.h"
#include "IDBKeyRangeData.h"
#include "Logging.h"
#include "MemoryIndexCursor.h"
#include "MemoryObjectStore.h"
#include "MemoryObjectStoreCursor.h"

namespace WebCore {
namespace IDBServer {

// The IndexedDB spec states the value you can get from the key generator is 2^53
static uint64_t maxGeneratedKeyValue = 0x20000000000000;

std::unique_ptr<MemoryIDBBackingStore> MemoryIDBBackingStore::create(const IDBDatabaseIdentifier& identifier)
{
    return std::make_unique<MemoryIDBBackingStore>(identifier);
}

MemoryIDBBackingStore::MemoryIDBBackingStore(const IDBDatabaseIdentifier& identifier)
    : m_identifier(identifier)
{
}

MemoryIDBBackingStore::~MemoryIDBBackingStore() = default;

IDBError MemoryIDBBackingStore::getOrEstablishDatabaseInfo(IDBDatabaseInfo& info)
{
    if (!m_databaseInfo)
        m_databaseInfo = std::make_unique<IDBDatabaseInfo>(m_identifier.databaseName(), 0);

    info = *m_databaseInfo;
    return IDBError { };
}

void MemoryIDBBackingStore::setDatabaseInfo(const IDBDatabaseInfo& info)
{
    // It is not valid to directly set database info on a backing store that hasn't already set its own database info.
    ASSERT(m_databaseInfo);

    m_databaseInfo = std::make_unique<IDBDatabaseInfo>(info);
}

IDBError MemoryIDBBackingStore::beginTransaction(const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::beginTransaction");

    if (m_transactions.contains(info.identifier()))
        return IDBError { InvalidStateError, "Backing store asked to create transaction it already has a record of" };

    auto transaction = MemoryBackingStoreTransaction::create(*this, info);

    // VersionChange transactions are scoped to "every object store".
    if (transaction->isVersionChange()) {
        for (auto& objectStore : m_objectStoresByIdentifier.values())
            transaction->addExistingObjectStore(*objectStore);
    } else if (transaction->isWriting()) {
        for (auto& iterator : m_objectStoresByName) {
            if (info.objectStores().contains(iterator.key))
                transaction->addExistingObjectStore(*iterator.value);
        }
    }

    m_transactions.set(info.identifier(), WTFMove(transaction));

    return IDBError { };
}

IDBError MemoryIDBBackingStore::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::abortTransaction - %s", transactionIdentifier.loggingString().utf8().data());

    auto transaction = m_transactions.take(transactionIdentifier);
    if (!transaction)
        return IDBError { InvalidStateError, "Backing store asked to abort transaction it didn't have record of" };

    transaction->abort();

    return IDBError { };
}

IDBError MemoryIDBBackingStore::commitTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::commitTransaction - %s", transactionIdentifier.loggingString().utf8().data());

    auto transaction = m_transactions.take(transactionIdentifier);
    if (!transaction)
        return IDBError { InvalidStateError, "Backing store asked to commit transaction it didn't have record of" };

    transaction->commit();

    return IDBError { };
}

IDBError MemoryIDBBackingStore::createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::createObjectStore - adding OS %s with ID %" PRIu64, info.name().utf8().data(), info.identifier());

    ASSERT(m_databaseInfo);
    if (m_databaseInfo->hasObjectStore(info.name()))
        return IDBError { ConstraintError };

    ASSERT(!m_objectStoresByIdentifier.contains(info.identifier()));
    auto objectStore = MemoryObjectStore::create(info);

    m_databaseInfo->addExistingObjectStore(info);

    auto rawTransaction = m_transactions.get(transactionIdentifier);
    ASSERT(rawTransaction);
    ASSERT(rawTransaction->isVersionChange());

    rawTransaction->addNewObjectStore(objectStore.get());
    registerObjectStore(WTFMove(objectStore));

    return IDBError { };
}

IDBError MemoryIDBBackingStore::deleteObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::deleteObjectStore");

    ASSERT(m_databaseInfo);
    if (!m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier))
        return IDBError { ConstraintError };

    auto transaction = m_transactions.get(transactionIdentifier);
    ASSERT(transaction);
    ASSERT(transaction->isVersionChange());

    auto objectStore = takeObjectStoreByIdentifier(objectStoreIdentifier);
    ASSERT(objectStore);
    if (!objectStore)
        return IDBError { ConstraintError };

    m_databaseInfo->deleteObjectStore(objectStore->info().name());
    transaction->objectStoreDeleted(*objectStore);

    return IDBError { };
}

IDBError MemoryIDBBackingStore::renameObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::renameObjectStore");

    ASSERT(m_databaseInfo);
    if (!m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier))
        return IDBError { ConstraintError };

    auto transaction = m_transactions.get(transactionIdentifier);
    ASSERT(transaction);
    ASSERT(transaction->isVersionChange());

    auto objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    ASSERT(objectStore);
    if (!objectStore)
        return IDBError { ConstraintError };

    String oldName = objectStore->info().name();
    objectStore->rename(newName);
    transaction->objectStoreRenamed(*objectStore, oldName);

    m_databaseInfo->renameObjectStore(objectStoreIdentifier, newName);

    return IDBError { };
}

IDBError MemoryIDBBackingStore::clearObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::clearObjectStore");
    ASSERT(objectStoreIdentifier);

    ASSERT_UNUSED(transactionIdentifier, m_transactions.contains(transactionIdentifier));

#ifndef NDEBUG
    auto transaction = m_transactions.get(transactionIdentifier);
    ASSERT(transaction->isWriting());
#endif

    auto objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError { ConstraintError };

    objectStore->clear();

    return IDBError { };
}

IDBError MemoryIDBBackingStore::createIndex(const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::createIndex");

    auto rawTransaction = m_transactions.get(transactionIdentifier);
    ASSERT(rawTransaction);
    ASSERT(rawTransaction->isVersionChange());

    auto* objectStore = m_objectStoresByIdentifier.get(info.objectStoreIdentifier());
    if (!objectStore)
        return IDBError { ConstraintError };

    return objectStore->createIndex(*rawTransaction, info);
}

IDBError MemoryIDBBackingStore::deleteIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::deleteIndex");

    auto rawTransaction = m_transactions.get(transactionIdentifier);
    ASSERT(rawTransaction);
    ASSERT(rawTransaction->isVersionChange());

    auto* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError { ConstraintError };

    return objectStore->deleteIndex(*rawTransaction, indexIdentifier);
}

IDBError MemoryIDBBackingStore::renameIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::renameIndex");

    ASSERT(m_databaseInfo);
    auto* objectStoreInfo = m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
    if (!objectStoreInfo)
        return IDBError { ConstraintError };

    auto* indexInfo = objectStoreInfo->infoForExistingIndex(indexIdentifier);
    if (!indexInfo)
        return IDBError { ConstraintError };

    auto transaction = m_transactions.get(transactionIdentifier);
    ASSERT(transaction);
    ASSERT(transaction->isVersionChange());

    auto objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    ASSERT(objectStore);
    if (!objectStore)
        return IDBError { ConstraintError };

    auto* index = objectStore->indexForIdentifier(indexIdentifier);
    ASSERT(index);
    if (!index)
        return IDBError { ConstraintError };

    String oldName = index->info().name();
    objectStore->renameIndex(*index, newName);
    transaction->indexRenamed(*index, oldName);

    indexInfo->rename(newName);

    return IDBError { };
}

void MemoryIDBBackingStore::removeObjectStoreForVersionChangeAbort(MemoryObjectStore& objectStore)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::removeObjectStoreForVersionChangeAbort");

    if (!m_objectStoresByIdentifier.contains(objectStore.info().identifier()))
        return;

    ASSERT(m_objectStoresByIdentifier.get(objectStore.info().identifier()) == &objectStore);

    unregisterObjectStore(objectStore);
}

void MemoryIDBBackingStore::restoreObjectStoreForVersionChangeAbort(Ref<MemoryObjectStore>&& objectStore)
{
    registerObjectStore(WTFMove(objectStore));
}

IDBError MemoryIDBBackingStore::keyExistsInObjectStore(const IDBResourceIdentifier&, uint64_t objectStoreIdentifier, const IDBKeyData& keyData, bool& keyExists)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::keyExistsInObjectStore");

    ASSERT(objectStoreIdentifier);

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    RELEASE_ASSERT(objectStore);

    keyExists = objectStore->containsRecord(keyData);
    return IDBError { };
}

IDBError MemoryIDBBackingStore::deleteRange(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::deleteRange");

    ASSERT(objectStoreIdentifier);

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError { UnknownError, "No backing store transaction found to delete from"_s };

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError { UnknownError, "No backing store object store found"_s };

    objectStore->deleteRange(range);
    return IDBError { };
}

IDBError MemoryIDBBackingStore::addRecord(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo& objectStoreInfo, const IDBKeyData& keyData, const IDBValue& value)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::addRecord");

    ASSERT(objectStoreInfo.identifier());

    auto transaction = m_transactions.get(transactionIdentifier);
    if (!transaction)
        return IDBError { UnknownError, "No backing store transaction found to put record"_s };

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreInfo.identifier());
    if (!objectStore)
        return IDBError { UnknownError, "No backing store object store found to put record"_s };

    return objectStore->addRecord(*transaction, keyData, value);
}

IDBError MemoryIDBBackingStore::getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData& range, IDBGetRecordDataType type, IDBGetResult& outValue)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::getRecord");

    ASSERT(objectStoreIdentifier);

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError { UnknownError, "No backing store transaction found to get record"_s };

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError { UnknownError, "No backing store object store found"_s };

    switch (type) {
    case IDBGetRecordDataType::KeyAndValue:
        outValue = objectStore->valueForKeyRange(range);
        break;
    case IDBGetRecordDataType::KeyOnly:
        outValue = objectStore->lowestKeyWithRecordInRange(range);
        break;
    }

    return IDBError { };
}

IDBError MemoryIDBBackingStore::getAllRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData& getAllRecordsData, IDBGetAllResult& result)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::getAllRecords");

    ASSERT(getAllRecordsData.objectStoreIdentifier);

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError { UnknownError, "No backing store transaction found to get all records"_s };

    auto* objectStore = m_objectStoresByIdentifier.get(getAllRecordsData.objectStoreIdentifier);
    if (!objectStore)
        return IDBError { UnknownError, "No backing store object store found"_s };

    if (getAllRecordsData.indexIdentifier) {
        auto* index = objectStore->indexForIdentifier(getAllRecordsData.indexIdentifier);
        if (!index)
            return IDBError { UnknownError, "No backing store index found"_s };

        index->getAllRecords(getAllRecordsData.keyRangeData, getAllRecordsData.count, getAllRecordsData.getAllType, result);
    } else
        objectStore->getAllRecords(getAllRecordsData.keyRangeData, getAllRecordsData.count, getAllRecordsData.getAllType, result);

    return IDBError { };
}

IDBError MemoryIDBBackingStore::getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType recordType, const IDBKeyRangeData& range, IDBGetResult& outValue)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::getIndexRecord");

    ASSERT(objectStoreIdentifier);

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError { UnknownError, "No backing store transaction found to get record"_s };

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError { UnknownError, "No backing store object store found"_s };

    outValue = objectStore->indexValueForKeyRange(indexIdentifier, recordType, range);
    return IDBError { };
}

IDBError MemoryIDBBackingStore::getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData& range, uint64_t& outCount)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::getCount");

    ASSERT(objectStoreIdentifier);

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError { UnknownError, "No backing store transaction found to get count"_s };

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError { UnknownError, "No backing store object store found"_s };

    outCount = objectStore->countForKeyRange(indexIdentifier, range);

    return IDBError { };
}

IDBError MemoryIDBBackingStore::generateKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t& keyNumber)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::generateKeyNumber");
    ASSERT(objectStoreIdentifier);
    ASSERT_UNUSED(transactionIdentifier, m_transactions.contains(transactionIdentifier));
    ASSERT_UNUSED(transactionIdentifier, m_transactions.get(transactionIdentifier)->isWriting());

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    RELEASE_ASSERT(objectStore);

    keyNumber = objectStore->currentKeyGeneratorValue();
    if (keyNumber > maxGeneratedKeyValue)
        return IDBError { ConstraintError, "Cannot generate new key value over 2^53 for object store operation" };

    objectStore->setKeyGeneratorValue(keyNumber + 1);

    return IDBError { };
}

IDBError MemoryIDBBackingStore::revertGeneratedKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t keyNumber)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::revertGeneratedKeyNumber");
    ASSERT(objectStoreIdentifier);
    ASSERT_UNUSED(transactionIdentifier, m_transactions.contains(transactionIdentifier));
    ASSERT_UNUSED(transactionIdentifier, m_transactions.get(transactionIdentifier)->isWriting());

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    RELEASE_ASSERT(objectStore);

    objectStore->setKeyGeneratorValue(keyNumber);

    return IDBError { };
}

IDBError MemoryIDBBackingStore::maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, double newKeyNumber)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::maybeUpdateKeyGeneratorNumber");
    ASSERT(objectStoreIdentifier);
    ASSERT_UNUSED(transactionIdentifier, m_transactions.contains(transactionIdentifier));
    ASSERT_UNUSED(transactionIdentifier, m_transactions.get(transactionIdentifier)->isWriting());

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    RELEASE_ASSERT(objectStore);

    if (newKeyNumber < objectStore->currentKeyGeneratorValue())
        return IDBError { };

    uint64_t newKeyInteger(newKeyNumber);
    if (newKeyInteger <= uint64_t(newKeyNumber))
        ++newKeyInteger;

    ASSERT(newKeyInteger > uint64_t(newKeyNumber));

    objectStore->setKeyGeneratorValue(newKeyInteger);

    return IDBError { };
}

IDBError MemoryIDBBackingStore::openCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo& info, IDBGetResult& outData)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::openCursor");

    ASSERT(!MemoryCursor::cursorForIdentifier(info.identifier()));

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError { UnknownError, "No backing store transaction found in which to open a cursor"_s };

    switch (info.cursorSource()) {
    case IndexedDB::CursorSource::ObjectStore: {
        auto* objectStore = m_objectStoresByIdentifier.get(info.sourceIdentifier());
        if (!objectStore)
            return IDBError { UnknownError, "No backing store object store found"_s };

        MemoryCursor* cursor = objectStore->maybeOpenCursor(info);
        if (!cursor)
            return IDBError { UnknownError, "Could not create object store cursor in backing store"_s };

        cursor->currentData(outData);
        break;
    }
    case IndexedDB::CursorSource::Index:
        auto* objectStore = m_objectStoresByIdentifier.get(info.objectStoreIdentifier());
        if (!objectStore)
            return IDBError { UnknownError, "No backing store object store found"_s };

        auto* index = objectStore->indexForIdentifier(info.sourceIdentifier());
        if (!index)
            return IDBError { UnknownError, "No backing store index found"_s };

        MemoryCursor* cursor = index->maybeOpenCursor(info);
        if (!cursor)
            return IDBError { UnknownError, "Could not create index cursor in backing store"_s };

        cursor->currentData(outData);
        break;
    }

    return IDBError { };
}

IDBError MemoryIDBBackingStore::iterateCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBIterateCursorData& data, IDBGetResult& outData)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::iterateCursor");

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError { UnknownError, "No backing store transaction found in which to iterate cursor"_s };

    auto* cursor = MemoryCursor::cursorForIdentifier(cursorIdentifier);
    if (!cursor)
        return IDBError { UnknownError, "No backing store cursor found in which to iterate cursor"_s };

    cursor->iterate(data.keyData, data.primaryKeyData, data.count, outData);

    return IDBError { };
}

void MemoryIDBBackingStore::registerObjectStore(Ref<MemoryObjectStore>&& objectStore)
{
    ASSERT(!m_objectStoresByIdentifier.contains(objectStore->info().identifier()));
    ASSERT(!m_objectStoresByName.contains(objectStore->info().name()));

    auto identifier = objectStore->info().identifier();
    m_objectStoresByName.set(objectStore->info().name(), &objectStore.get());
    m_objectStoresByIdentifier.set(identifier, WTFMove(objectStore));
}

void MemoryIDBBackingStore::unregisterObjectStore(MemoryObjectStore& objectStore)
{
    ASSERT(m_objectStoresByIdentifier.contains(objectStore.info().identifier()));
    ASSERT(m_objectStoresByName.contains(objectStore.info().name()));

    m_objectStoresByName.remove(objectStore.info().name());
    m_objectStoresByIdentifier.remove(objectStore.info().identifier());
}

RefPtr<MemoryObjectStore> MemoryIDBBackingStore::takeObjectStoreByIdentifier(uint64_t identifier)
{
    auto objectStoreByIdentifier = m_objectStoresByIdentifier.take(identifier);
    if (!objectStoreByIdentifier)
        return nullptr;

    auto objectStore = m_objectStoresByName.take(objectStoreByIdentifier->info().name());
    ASSERT_UNUSED(objectStore, objectStore);

    return objectStoreByIdentifier;
}

IDBObjectStoreInfo* MemoryIDBBackingStore::infoForObjectStore(uint64_t objectStoreIdentifier)
{
    ASSERT(m_databaseInfo);
    return m_databaseInfo->infoForExistingObjectStore(objectStoreIdentifier);
}

void MemoryIDBBackingStore::deleteBackingStore()
{
    // The in-memory IDB backing store doesn't need to do any cleanup when it is deleted.
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
