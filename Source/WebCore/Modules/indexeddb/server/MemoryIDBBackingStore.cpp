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
#include "IDBIndexInfo.h"
#include "IDBKeyRangeData.h"
#include "Logging.h"
#include "MemoryObjectStore.h"
#include "MemoryObjectStoreCursor.h"

namespace WebCore {
namespace IDBServer {

std::unique_ptr<MemoryIDBBackingStore> MemoryIDBBackingStore::create(const IDBDatabaseIdentifier& identifier)
{
    return std::make_unique<MemoryIDBBackingStore>(identifier);
}

MemoryIDBBackingStore::MemoryIDBBackingStore(const IDBDatabaseIdentifier& identifier)
    : m_identifier(identifier)
{
}

MemoryIDBBackingStore::~MemoryIDBBackingStore()
{
}

const IDBDatabaseInfo& MemoryIDBBackingStore::getOrEstablishDatabaseInfo()
{
    if (!m_databaseInfo)
        m_databaseInfo = std::make_unique<IDBDatabaseInfo>(m_identifier.databaseName(), 0);

    return *m_databaseInfo;
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
        return IDBError(IDBDatabaseException::InvalidStateError, "Backing store asked to create transaction it already has a record of");

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

    m_transactions.set(info.identifier(), WTF::move(transaction));

    return IDBError();
}

IDBError MemoryIDBBackingStore::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::abortTransaction - %s", transactionIdentifier.loggingString().utf8().data());

    auto transaction = m_transactions.take(transactionIdentifier);
    if (!transaction)
        return IDBError(IDBDatabaseException::InvalidStateError, "Backing store asked to abort transaction it didn't have record of");

    transaction->abort();

    return IDBError();
}

IDBError MemoryIDBBackingStore::commitTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::commitTransaction - %s", transactionIdentifier.loggingString().utf8().data());

    auto transaction = m_transactions.take(transactionIdentifier);
    if (!transaction)
        return IDBError(IDBDatabaseException::InvalidStateError, "Backing store asked to commit transaction it didn't have record of");

    transaction->commit();

    return IDBError();
}

IDBError MemoryIDBBackingStore::createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::createObjectStore - adding OS %s with ID %" PRIu64, info.name().utf8().data(), info.identifier());

    ASSERT(m_databaseInfo);
    if (m_databaseInfo->hasObjectStore(info.name()))
        return IDBError(IDBDatabaseException::ConstraintError);

    ASSERT(!m_objectStoresByIdentifier.contains(info.identifier()));
    auto objectStore = MemoryObjectStore::create(info);

    m_databaseInfo->addExistingObjectStore(info);

    auto rawTransaction = m_transactions.get(transactionIdentifier);
    ASSERT(rawTransaction);
    ASSERT(rawTransaction->isVersionChange());

    rawTransaction->addNewObjectStore(*objectStore);
    registerObjectStore(WTF::move(objectStore));

    return IDBError();
}

IDBError MemoryIDBBackingStore::deleteObjectStore(const IDBResourceIdentifier& transactionIdentifier, const String& objectStoreName)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::deleteObjectStore");

    ASSERT(m_databaseInfo);
    if (!m_databaseInfo->hasObjectStore(objectStoreName))
        return IDBError(IDBDatabaseException::ConstraintError);

    auto transaction = m_transactions.get(transactionIdentifier);
    ASSERT(transaction);
    ASSERT(transaction->isVersionChange());

    auto objectStore = takeObjectStoreByName(objectStoreName);
    ASSERT(objectStore);
    if (!objectStore)
        return IDBError(IDBDatabaseException::ConstraintError);

    m_databaseInfo->deleteObjectStore(objectStoreName);
    transaction->objectStoreDeleted(WTF::move(objectStore));

    return IDBError();
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
        return IDBError(IDBDatabaseException::ConstraintError);

    objectStore->clear();

    return IDBError();
}

IDBError MemoryIDBBackingStore::createIndex(const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::createIndex");

    auto rawTransaction = m_transactions.get(transactionIdentifier);
    ASSERT(rawTransaction);
    ASSERT(rawTransaction->isVersionChange());

    auto* objectStore = m_objectStoresByIdentifier.get(info.objectStoreIdentifier());
    if (!objectStore)
        return IDBError(IDBDatabaseException::ConstraintError);

    return objectStore->createIndex(*rawTransaction, info);
}

IDBError MemoryIDBBackingStore::deleteIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::deleteIndex");

    auto rawTransaction = m_transactions.get(transactionIdentifier);
    ASSERT(rawTransaction);
    ASSERT(rawTransaction->isVersionChange());

    auto* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError(IDBDatabaseException::ConstraintError);

    return objectStore->deleteIndex(*rawTransaction, indexName);
}

void MemoryIDBBackingStore::removeObjectStoreForVersionChangeAbort(MemoryObjectStore& objectStore)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::removeObjectStoreForVersionChangeAbort");

    ASSERT(m_objectStoresByIdentifier.contains(objectStore.info().identifier()));
    ASSERT(m_objectStoresByIdentifier.get(objectStore.info().identifier()) == &objectStore);

    unregisterObjectStore(objectStore);
}

void MemoryIDBBackingStore::restoreObjectStoreForVersionChangeAbort(std::unique_ptr<MemoryObjectStore>&& objectStore)
{
    registerObjectStore(WTF::move(objectStore));
}

IDBError MemoryIDBBackingStore::keyExistsInObjectStore(const IDBResourceIdentifier&, uint64_t objectStoreIdentifier, const IDBKeyData& keyData, bool& keyExists)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::keyExistsInObjectStore");

    ASSERT(objectStoreIdentifier);

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    RELEASE_ASSERT(objectStore);

    keyExists = objectStore->containsRecord(keyData);
    return IDBError();
}

IDBError MemoryIDBBackingStore::deleteRange(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData& range)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::deleteRange");

    ASSERT(objectStoreIdentifier);

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store transaction found to delete from"));

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store object store found"));

    objectStore->deleteRange(range);
    return IDBError();
}

IDBError MemoryIDBBackingStore::addRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData& keyData, const ThreadSafeDataBuffer& value)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::addRecord");

    ASSERT(objectStoreIdentifier);

    auto transaction = m_transactions.get(transactionIdentifier);
    if (!transaction)
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store transaction found to put record"));

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store object store found to put record"));

    return objectStore->addRecord(*transaction, keyData, value);
}

IDBError MemoryIDBBackingStore::getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData& range, ThreadSafeDataBuffer& outValue)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::getRecord");

    ASSERT(objectStoreIdentifier);

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store transaction found to get record"));

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store object store found"));

    outValue = objectStore->valueForKeyRange(range);
    return IDBError();
}

IDBError MemoryIDBBackingStore::getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType recordType, const IDBKeyRangeData& range, IDBGetResult& outValue)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::getIndexRecord");

    ASSERT(objectStoreIdentifier);

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store transaction found to get record"));

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store object store found"));

    outValue = objectStore->indexValueForKeyRange(indexIdentifier, recordType, range);
    return IDBError();
}

IDBError MemoryIDBBackingStore::getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData& range, uint64_t& outCount)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::getCount");

    ASSERT(objectStoreIdentifier);

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store transaction found to get count"));

    MemoryObjectStore* objectStore = m_objectStoresByIdentifier.get(objectStoreIdentifier);
    if (!objectStore)
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store object store found"));

    outCount = objectStore->countForKeyRange(indexIdentifier, range);

    return IDBError();
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
    objectStore->setKeyGeneratorValue(keyNumber + 1);

    return IDBError();
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
        return { };

    uint64_t newKeyInteger(newKeyNumber);
    if (newKeyInteger <= newKeyNumber)
        ++newKeyInteger;

    ASSERT(newKeyInteger > newKeyNumber);

    objectStore->setKeyGeneratorValue(newKeyInteger);

    return { };
}

IDBError MemoryIDBBackingStore::openCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo& info, IDBGetResult& outData)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::openCursor");

    ASSERT(!MemoryCursor::cursorForIdentifier(info.identifier()));

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store transaction found in which to open a cursor"));

    switch (info.cursorSource()) {
    case IndexedDB::CursorSource::ObjectStore: {
        auto* objectStore = m_objectStoresByIdentifier.get(info.sourceIdentifier());
        if (!objectStore)
            return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store object store found"));

        MemoryCursor* cursor = objectStore->maybeOpenCursor(info);
        if (!cursor)
            return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("Could not create object store cursor in backing store"));

        cursor->currentData(outData);
        break;
    }
    case IndexedDB::CursorSource::Index:
        auto* objectStore = m_objectStoresByIdentifier.get(info.objectStoreIdentifier());
        if (!objectStore)
            return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store object store found"));

        auto* index = objectStore->indexForIdentifier(info.sourceIdentifier());
        if (!index)
            return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store index found"));

        MemoryCursor* cursor = index->maybeOpenCursor(info);
        if (!cursor)
            return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("Could not create index cursor in backing store"));

        cursor->currentData(outData);
        break;
    }

    return { };
}

IDBError MemoryIDBBackingStore::iterateCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBKeyData& key, uint32_t count, IDBGetResult& outData)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::iterateCursor");

    if (!m_transactions.contains(transactionIdentifier))
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store transaction found in which to iterate cursor"));

    auto* cursor = MemoryCursor::cursorForIdentifier(cursorIdentifier);
    if (!cursor)
        return IDBError(IDBDatabaseException::UnknownError, ASCIILiteral("No backing store cursor found in which to iterate cursor"));

    cursor->iterate(key, count, outData);

    return { };
}

void MemoryIDBBackingStore::registerObjectStore(std::unique_ptr<MemoryObjectStore>&& objectStore)
{
    ASSERT(objectStore);
    ASSERT(!m_objectStoresByIdentifier.contains(objectStore->info().identifier()));
    ASSERT(!m_objectStoresByName.contains(objectStore->info().name()));

    m_objectStoresByName.set(objectStore->info().name(), objectStore.get());
    m_objectStoresByIdentifier.set(objectStore->info().identifier(), WTF::move(objectStore));
}

void MemoryIDBBackingStore::unregisterObjectStore(MemoryObjectStore& objectStore)
{
    ASSERT(m_objectStoresByIdentifier.contains(objectStore.info().identifier()));
    ASSERT(m_objectStoresByName.contains(objectStore.info().name()));

    m_objectStoresByName.remove(objectStore.info().name());
    m_objectStoresByIdentifier.remove(objectStore.info().identifier());
}

std::unique_ptr<MemoryObjectStore> MemoryIDBBackingStore::takeObjectStoreByName(const String& name)
{
    auto rawObjectStore = m_objectStoresByName.take(name);
    if (!rawObjectStore)
        return nullptr;

    auto objectStore = m_objectStoresByIdentifier.take(rawObjectStore->info().identifier());
    ASSERT(objectStore);

    return objectStore;
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
