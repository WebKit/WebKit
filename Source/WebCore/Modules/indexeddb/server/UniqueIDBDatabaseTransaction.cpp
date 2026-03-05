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
#include "UniqueIDBDatabaseTransaction.h"

#include "IDBIterateCursorData.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "Logging.h"
#include "UniqueIDBDatabase.h"
#include "UniqueIDBDatabaseConnection.h"
#include "UniqueIDBDatabaseManager.h"
#include <algorithm>

namespace WebCore {
namespace IDBServer {

Ref<UniqueIDBDatabaseTransaction> UniqueIDBDatabaseTransaction::create(UniqueIDBDatabaseConnection& connection, const IDBTransactionInfo& info)
{
    return adoptRef(*new UniqueIDBDatabaseTransaction(connection, info));
}

UniqueIDBDatabaseTransaction::UniqueIDBDatabaseTransaction(UniqueIDBDatabaseConnection& connection, const IDBTransactionInfo& info)
    : m_databaseConnection(connection)
    , m_transactionInfo(info)
{
    ASSERT(database());

    if (m_transactionInfo.mode() == IDBTransactionMode::Versionchange)
        m_originalDatabaseInfo = makeUnique<IDBDatabaseInfo>(protect(database())->info());

    RefPtr databaseConnection = m_databaseConnection.get();
    if (!databaseConnection)
        return;

    if (CheckedPtr manager = databaseConnection->manager())
        manager->registerTransaction(*this);
}

UniqueIDBDatabaseTransaction::~UniqueIDBDatabaseTransaction()
{
    RefPtr databaseConnection = m_databaseConnection.get();
    if (!databaseConnection)
        return;

    if (CheckedPtr manager = databaseConnection->manager())
        manager->unregisterTransaction(*this);
}

UniqueIDBDatabaseConnection* UniqueIDBDatabaseTransaction::databaseConnection() const
{
    return m_databaseConnection.get();
}

IDBDatabaseInfo* UniqueIDBDatabaseTransaction::originalDatabaseInfo() const
{
    ASSERT(m_transactionInfo.mode() == IDBTransactionMode::Versionchange);
    return m_originalDatabaseInfo.get();
}

void UniqueIDBDatabaseTransaction::abort()
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::abort");
    
    CheckedPtr database = this->database();
    if (!database)
        return;

    database->abortTransaction(*this, [weakThis = WeakPtr { *this }](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::abort (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (RefPtr connection = protectedThis->m_databaseConnection.get())
            connection->didAbortTransaction(*protectedThis, error);
    });
}

void UniqueIDBDatabaseTransaction::abortWithoutCallback()
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::abortWithoutCallback");

    if (RefPtr databaseConnection = m_databaseConnection.get())
        databaseConnection->abortTransactionWithoutCallback(*this);
}

UniqueIDBDatabase* UniqueIDBDatabaseTransaction::database() const
{
    return m_databaseConnection ? m_databaseConnection->database() : nullptr;
}

bool UniqueIDBDatabaseTransaction::isVersionChange() const
{
    return m_transactionInfo.mode() == IDBTransactionMode::Versionchange;
}

bool UniqueIDBDatabaseTransaction::isReadOnly() const
{
    return m_transactionInfo.mode() == IDBTransactionMode::Readonly;
}   

bool UniqueIDBDatabaseTransaction::shouldAbortDueToUnhandledRequestError(uint64_t handledRequestResultsCount) const
{
    if (handledRequestResultsCount > m_requestResults.size()) {
        RELEASE_LOG_ERROR(IndexedDB, "%p - UniqueIDBDatabaseTransaction::shouldAbortDueToUnhandledRequestError: finished request count (%" PRIu64 ") is bigger than total request count %zu", this, handledRequestResultsCount, m_requestResults.size());
        return false;
    }

    auto pendingRequestResults = m_requestResults.subspan(handledRequestResultsCount);
    return std::ranges::any_of(pendingRequestResults, [&](auto& error) {
        return !error.isNull();
    });
}

void UniqueIDBDatabaseTransaction::commit(uint64_t handledRequestResultsCount)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::commit");

    CheckedPtr database = this->database();
    if (!database)
        return;

    database->commitTransaction(*this, handledRequestResultsCount, [weakThis = WeakPtr { *this }](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::commit (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (RefPtr connection = protectedThis->m_databaseConnection.get())
            connection->didCommitTransaction(*protectedThis, error);
    });
}

void UniqueIDBDatabaseTransaction::createObjectStore(const IDBRequestData& requestData, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::createObjectStore");

    RELEASE_ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    CheckedPtr database = this->database();
    if (!database)
        return;

    database->createObjectStore(*this, info, [weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::createObjectStore (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        if (error.isNull())
            databaseConnection->didCreateObjectStore(IDBResultData::createObjectStoreSuccess(requestData.requestIdentifier()));
        else
            databaseConnection->didCreateObjectStore(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::deleteObjectStore(const IDBRequestData& requestData, const String& objectStoreName)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteObjectStore");

    RELEASE_ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    CheckedPtr database = this->database();
    if (!database)
        return;

    database->deleteObjectStore(*this, objectStoreName, [weakThis = WeakPtr { *this }, requestData](const IDBError& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteObjectStore (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        protectedThis->m_requestResults.append(error);

        if (error.isNull())
            databaseConnection->didDeleteObjectStore(IDBResultData::deleteObjectStoreSuccess(requestData.requestIdentifier()));
        else
            databaseConnection->didDeleteObjectStore(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::renameObjectStore(const IDBRequestData& requestData, IDBObjectStoreIdentifier objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::renameObjectStore");

    RELEASE_ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    CheckedPtr database = this->database();
    if (!database)
        return;

    database->renameObjectStore(*this, objectStoreIdentifier, newName, [weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::renameObjectStore (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        if (error.isNull())
            databaseConnection->didRenameObjectStore(IDBResultData::renameObjectStoreSuccess(requestData.requestIdentifier()));
        else
            databaseConnection->didRenameObjectStore(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::clearObjectStore(const IDBRequestData& requestData, IDBObjectStoreIdentifier objectStoreIdentifier)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::clearObjectStore");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    CheckedPtr database = this->database();
    if (!database)
        return;

    database->clearObjectStore(*this, objectStoreIdentifier, [weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::clearObjectStore (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        protectedThis->m_requestResults.append(error);

        if (error.isNull())
            databaseConnection->didClearObjectStore(IDBResultData::clearObjectStoreSuccess(requestData.requestIdentifier()));
        else
            databaseConnection->didClearObjectStore(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::deleteIndex(const IDBRequestData& requestData, IDBObjectStoreIdentifier objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteIndex");

    RELEASE_ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    CheckedPtr database = this->database();
    if (!database)
        return;
    
    database->deleteIndex(*this, objectStoreIdentifier, indexName, [weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteIndex (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        if (error.isNull())
            databaseConnection->didDeleteIndex(IDBResultData::deleteIndexSuccess(requestData.requestIdentifier()));
        else
            databaseConnection->didDeleteIndex(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::renameIndex(const IDBRequestData& requestData, IDBObjectStoreIdentifier objectStoreIdentifier, IDBIndexIdentifier indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::renameIndex");

    RELEASE_ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    CheckedPtr database = this->database();
    if (!database)
        return;
    
    database->renameIndex(*this, objectStoreIdentifier, indexIdentifier, newName, [weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::renameIndex (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        if (error.isNull())
            databaseConnection->didRenameIndex(IDBResultData::renameIndexSuccess(requestData.requestIdentifier()));
        else
            databaseConnection->didRenameIndex(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}


void UniqueIDBDatabaseTransaction::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, const IndexIDToIndexKeyMap& indexKeys, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::putOrAdd");

    ASSERT(!isReadOnly());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    CheckedPtr database = this->database();
    if (!database)
        return;
    
    database->putOrAdd(requestData, keyData, value, indexKeys, overwriteMode, [weakThis = WeakPtr { *this }, requestData](auto& error, const IDBKeyData& key) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::putOrAdd (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        protectedThis->m_requestResults.append(error);

        if (error.isNull())
            protect(databaseConnection->connectionToClient())->didPutOrAdd(IDBResultData::putOrAddSuccess(requestData.requestIdentifier(), key));
        else
            protect(databaseConnection->connectionToClient())->didPutOrAdd(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getRecord");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    CheckedPtr database = this->database();
    if (!database)
        return;
    
    database->getRecord(requestData, getRecordData, [weakThis = WeakPtr { *this }, requestData](auto& error, const IDBGetResult& result) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getRecord (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        protectedThis->m_requestResults.append(error);

        if (error.isNull())
            protect(databaseConnection->connectionToClient())->didGetRecord(IDBResultData::getRecordSuccess(requestData.requestIdentifier(), result));
        else
            protect(databaseConnection->connectionToClient())->didGetRecord(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getAllRecords");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    CheckedPtr database = this->database();
    if (!database)
        return;
    
    database->getAllRecords(requestData, getAllRecordsData, [weakThis = WeakPtr { *this }, requestData](auto& error, const IDBGetAllResult& result) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getAllRecords (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        protectedThis->m_requestResults.append(error);

        if (error.isNull())
            protect(databaseConnection->connectionToClient())->didGetAllRecords(IDBResultData::getAllRecordsSuccess(requestData.requestIdentifier(), result));
        else
            protect(databaseConnection->connectionToClient())->didGetAllRecords(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getCount");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    CheckedPtr database = this->database();
    if (!database)
        return;
    
    database->getCount(requestData, keyRangeData, [weakThis = WeakPtr { *this }, requestData](auto& error, uint64_t count) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getCount (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        protectedThis->m_requestResults.append(error);

        if (error.isNull())
            protect(databaseConnection->connectionToClient())->didGetCount(IDBResultData::getCountSuccess(requestData.requestIdentifier(), count));
        else
            protect(databaseConnection->connectionToClient())->didGetCount(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteRecord");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    CheckedPtr database = this->database();
    if (!database)
        return;
    
    database->deleteRecord(requestData, keyRangeData, [weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteRecord (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        protectedThis->m_requestResults.append(error);

        if (error.isNull())
            protect(databaseConnection->connectionToClient())->didDeleteRecord(IDBResultData::deleteRecordSuccess(requestData.requestIdentifier()));
        else
            protect(databaseConnection->connectionToClient())->didDeleteRecord(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::openCursor");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    CheckedPtr database = this->database();
    if (!database)
        return;

    database->openCursor(requestData, info, [weakThis = WeakPtr { *this }, requestData](auto& error, const IDBGetResult& result) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::openCursor (callback)");

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        protectedThis->m_requestResults.append(error);

        if (error.isNull())
            protect(databaseConnection->connectionToClient())->didOpenCursor(IDBResultData::openCursorSuccess(requestData.requestIdentifier(), result));
        else
            protect(databaseConnection->connectionToClient())->didOpenCursor(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::iterateCursor");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    CheckedPtr database = this->database();
    if (!database)
        return;
    
    database->iterateCursor(requestData, data, [weakThis = WeakPtr { *this }, requestData, option = data.option](auto& error, const IDBGetResult& result) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::iterateCursor (callback)");

        if (option == IndexedDB::CursorIterateOption::DoNotReply)
            return;

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr databaseConnection = protectedThis->m_databaseConnection.get();
        if (!databaseConnection)
            return;

        protectedThis->m_requestResults.append(error);

        if (error.isNull())
            protect(databaseConnection->connectionToClient())->didIterateCursor(IDBResultData::iterateCursorSuccess(requestData.requestIdentifier(), result));
        else
            protect(databaseConnection->connectionToClient())->didIterateCursor(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

const Vector<IDBObjectStoreIdentifier>& UniqueIDBDatabaseTransaction::objectStoreIdentifiers()
{
    if (!m_objectStoreIdentifiers.isEmpty())
        return m_objectStoreIdentifiers;

    CheckedPtr database = this->database();
    if (!database)
        return m_objectStoreIdentifiers;

    auto& info = database->info();
    for (const auto& objectStoreName : info.objectStoreNames()) {
        auto objectStoreInfo = info.infoForExistingObjectStore(objectStoreName);
        ASSERT(objectStoreInfo);
        if (!objectStoreInfo)
            continue;

        if (m_transactionInfo.objectStores().contains(objectStoreName))
            m_objectStoreIdentifiers.append(objectStoreInfo->identifier());
    }

    return m_objectStoreIdentifiers;
}

void UniqueIDBDatabaseTransaction::didActivateInBackingStore(const IDBError& error)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::didActivateInBackingStore");

    if (RefPtr connection = m_databaseConnection.get())
        protect(connection->connectionToClient())->didStartTransaction(m_transactionInfo.identifier(), error);
}

void UniqueIDBDatabaseTransaction::createIndex(const IDBRequestData& requestData, const IDBIndexInfo& indexInfo)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::createIndex");

    ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    CheckedPtr database = this->database();
    if (!database)
        return;

    ASSERT(!requestData.requestIdentifier().isEmpty());
    if (!m_createIndexRequestIdentifier.isEmpty()) {
        RELEASE_LOG_ERROR(IndexedDB, "%p - UniqueIDBDatabaseTransaction::createIndex: ignored create index request since there is one in progress", this);
        return;
    }

    m_createIndexRequestIdentifier = requestData.requestIdentifier();
    database->createIndexAsync(*this, indexInfo);
}

void UniqueIDBDatabaseTransaction::didCreateIndexAsync(const IDBError& error)
{
    auto requestIdentifier = std::exchange(m_createIndexRequestIdentifier, { });
    if (requestIdentifier.isEmpty())
        return;

    RefPtr databaseConnection = m_databaseConnection.get();
    if (!databaseConnection)
        return;

    auto result = error.isNull() ? IDBResultData::createIndexSuccess(requestIdentifier) : IDBResultData::error(requestIdentifier, error);
    databaseConnection->didCreateIndex(result);
}

bool UniqueIDBDatabaseTransaction::generateIndexKeyForRecord(const IDBIndexInfo& indexInfo, const std::optional<IDBKeyPath>& keyPath, const IDBKeyData& key, const IDBValue& value, std::optional<int64_t> recordID)
{
    RefPtr databaseConnection = m_databaseConnection.get();
    if (!databaseConnection || m_createIndexRequestIdentifier.isEmpty())
        return false;

    ++m_pendingGenerateIndexKeyRequests;
    protect(databaseConnection->connectionToClient())->generateIndexKeyForRecord(m_createIndexRequestIdentifier, indexInfo, keyPath, key, value, recordID);
    return true;
}

void UniqueIDBDatabaseTransaction::didGenerateIndexKeyForRecord(IDBResourceIdentifier requestIdentifier, const IDBIndexInfo& indexInfo, const IDBKeyData& key, const IndexKey& value, std::optional<int64_t> recordID)
{
    ASSERT(isVersionChange());

    if (m_createIndexRequestIdentifier != requestIdentifier)
        return;

    --m_pendingGenerateIndexKeyRequests;
    CheckedPtr database = this->database();
    if (!database)
        return;

    database->didGenerateIndexKeyForRecord(*this, indexInfo, key, value, recordID);
}

void UniqueIDBDatabaseTransaction::addOpenRequestResult(const IDBError& error)
{
    m_requestResults.append(error);
}

} // namespace IDBServer
} // namespace WebCore
