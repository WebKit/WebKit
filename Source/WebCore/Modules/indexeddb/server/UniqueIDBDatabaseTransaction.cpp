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
#include "IDBResultData.h"
#include "Logging.h"
#include "UniqueIDBDatabase.h"
#include "UniqueIDBDatabaseConnection.h"
#include "UniqueIDBDatabaseManager.h"

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
        m_originalDatabaseInfo = makeUnique<IDBDatabaseInfo>(database()->info());

    if (!m_databaseConnection)
        return;

    if (auto* manager = m_databaseConnection->manager())
        manager->registerTransaction(*this);
}

UniqueIDBDatabaseTransaction::~UniqueIDBDatabaseTransaction()
{
    if (!m_databaseConnection)
        return;

    if (auto* manager = m_databaseConnection->manager())
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
    
    auto* database = this->database();
    if (!database)
        return;

    database->abortTransaction(*this, [this, weakThis = WeakPtr { *this }](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::abort (callback)");

        if (weakThis && m_databaseConnection)
            m_databaseConnection->didAbortTransaction(*this, error);
    });
}

void UniqueIDBDatabaseTransaction::abortWithoutCallback()
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::abortWithoutCallback");

    if (m_databaseConnection)
        m_databaseConnection->abortTransactionWithoutCallback(*this);
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

void UniqueIDBDatabaseTransaction::commit(uint64_t pendingRequestCount)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::commit");

    auto* database = this->database();
    if (!database)
        return;

    std::optional<IDBError> errorInPendingRequests;
    while (pendingRequestCount--) {
        auto error = m_requestResults.takeLast();
        if (!error.isNull()) {
            errorInPendingRequests = error;
            break;
        }
    }

    if (errorInPendingRequests) {
        database->abortTransaction(*this, [this, weakThis = WeakPtr { *this }, &errorInPendingRequests](auto&) {
            LOG(IndexedDB, "UniqueIDBDatabaseTransaction::commit with error (callback)");

            if (weakThis && m_databaseConnection)
                m_databaseConnection->didCommitTransaction(*this, *errorInPendingRequests);
        });
        return;
    }

    database->commitTransaction(*this, [this, weakThis = WeakPtr { *this }](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::commit (callback)");

        if (weakThis && m_databaseConnection)
            m_databaseConnection->didCommitTransaction(*this, error);
    });
}

void UniqueIDBDatabaseTransaction::createObjectStore(const IDBRequestData& requestData, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::createObjectStore");

    ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    auto* database = this->database();
    if (!database)
        return;

    database->createObjectStore(*this, info, [this, weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::createObjectStore (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->didCreateObjectStore(IDBResultData::createObjectStoreSuccess(requestData.requestIdentifier()));
        else
            m_databaseConnection->didCreateObjectStore(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::deleteObjectStore(const IDBRequestData& requestData, const String& objectStoreName)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteObjectStore");

    ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    auto* database = this->database();
    if (!database)
        return;

    database->deleteObjectStore(*this, objectStoreName, [this, weakThis = WeakPtr { *this }, requestData](const IDBError& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteObjectStore (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->didDeleteObjectStore(IDBResultData::deleteObjectStoreSuccess(requestData.requestIdentifier()));
        else
            m_databaseConnection->didDeleteObjectStore(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::renameObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::renameObjectStore");

    ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    auto* database = this->database();
    if (!database)
        return;

    database->renameObjectStore(*this, objectStoreIdentifier, newName, [this, weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::renameObjectStore (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->didRenameObjectStore(IDBResultData::renameObjectStoreSuccess(requestData.requestIdentifier()));
        else
            m_databaseConnection->didRenameObjectStore(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::clearObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::clearObjectStore");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    auto* database = this->database();
    if (!database)
        return;

    database->clearObjectStore(*this, objectStoreIdentifier, [this, weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::clearObjectStore (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->didClearObjectStore(IDBResultData::clearObjectStoreSuccess(requestData.requestIdentifier()));
        else
            m_databaseConnection->didClearObjectStore(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::createIndex(const IDBRequestData& requestData, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::createIndex");

    ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    auto* database = this->database();
    if (!database)
        return;

    database->createIndex(*this, info, [this, weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::createIndex (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->didCreateIndex(IDBResultData::createIndexSuccess(requestData.requestIdentifier()));
        else
            m_databaseConnection->didCreateIndex(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::deleteIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteIndex");

    ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    auto* database = this->database();
    if (!database)
        return;
    
    database->deleteIndex(*this, objectStoreIdentifier, indexName, [this, weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::createIndex (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->didDeleteIndex(IDBResultData::deleteIndexSuccess(requestData.requestIdentifier()));
        else
            m_databaseConnection->didDeleteIndex(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::renameIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::renameIndex");

    ASSERT(isVersionChange());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    auto* database = this->database();
    if (!database)
        return;
    
    database->renameIndex(*this, objectStoreIdentifier, indexIdentifier, newName, [this, weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::renameIndex (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->didRenameIndex(IDBResultData::renameIndexSuccess(requestData.requestIdentifier()));
        else
            m_databaseConnection->didRenameIndex(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}


void UniqueIDBDatabaseTransaction::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::putOrAdd");

    ASSERT(!isReadOnly());
    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    auto* database = this->database();
    if (!database)
        return;
    
    database->putOrAdd(requestData, keyData, value, overwriteMode, [this, weakThis = WeakPtr { *this }, requestData](auto& error, const IDBKeyData& key) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::putOrAdd (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->connectionToClient().didPutOrAdd(IDBResultData::putOrAddSuccess(requestData.requestIdentifier(), key));
        else
            m_databaseConnection->connectionToClient().didPutOrAdd(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getRecord");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    auto* database = this->database();
    if (!database)
        return;
    
    database->getRecord(requestData, getRecordData, [this, weakThis = WeakPtr { *this }, requestData](auto& error, const IDBGetResult& result) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getRecord (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->connectionToClient().didGetRecord(IDBResultData::getRecordSuccess(requestData.requestIdentifier(), result));
        else
            m_databaseConnection->connectionToClient().didGetRecord(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getAllRecords");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    auto* database = this->database();
    if (!database)
        return;
    
    database->getAllRecords(requestData, getAllRecordsData, [this, weakThis = WeakPtr { *this }, requestData](auto& error, const IDBGetAllResult& result) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getAllRecords (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->connectionToClient().didGetAllRecords(IDBResultData::getAllRecordsSuccess(requestData.requestIdentifier(), result));
        else
            m_databaseConnection->connectionToClient().didGetAllRecords(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getCount");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    auto* database = this->database();
    if (!database)
        return;
    
    database->getCount(requestData, keyRangeData, [this, weakThis = WeakPtr { *this }, requestData](auto& error, uint64_t count) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::getCount (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->connectionToClient().didGetCount(IDBResultData::getCountSuccess(requestData.requestIdentifier(), count));
        else
            m_databaseConnection->connectionToClient().didGetCount(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteRecord");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    auto* database = this->database();
    if (!database)
        return;
    
    database->deleteRecord(requestData, keyRangeData, [this, weakThis = WeakPtr { *this }, requestData](auto& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::deleteRecord (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->connectionToClient().didDeleteRecord(IDBResultData::deleteRecordSuccess(requestData.requestIdentifier()));
        else
            m_databaseConnection->connectionToClient().didDeleteRecord(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::openCursor");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());
    
    auto* database = this->database();
    if (!database)
        return;

    database->openCursor(requestData, info, [this, weakThis = WeakPtr { *this }, requestData](auto& error, const IDBGetResult& result) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::openCursor (callback)");

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->connectionToClient().didOpenCursor(IDBResultData::openCursorSuccess(requestData.requestIdentifier(), result));
        else
            m_databaseConnection->connectionToClient().didOpenCursor(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

void UniqueIDBDatabaseTransaction::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::iterateCursor");

    ASSERT(m_transactionInfo.identifier() == requestData.transactionIdentifier());

    auto* database = this->database();
    if (!database)
        return;
    
    database->iterateCursor(requestData, data, [this, weakThis = WeakPtr { *this }, requestData, option = data.option](auto& error, const IDBGetResult& result) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::iterateCursor (callback)");

        if (option == IndexedDB::CursorIterateOption::DoNotReply)
            return;

        if (!weakThis || !m_databaseConnection)
            return;

        m_requestResults.append(error);

        if (error.isNull())
            m_databaseConnection->connectionToClient().didIterateCursor(IDBResultData::iterateCursorSuccess(requestData.requestIdentifier(), result));
        else
            m_databaseConnection->connectionToClient().didIterateCursor(IDBResultData::error(requestData.requestIdentifier(), error));
    });
}

const Vector<uint64_t>& UniqueIDBDatabaseTransaction::objectStoreIdentifiers()
{
    if (!m_objectStoreIdentifiers.isEmpty())
        return m_objectStoreIdentifiers;

    auto* database = this->database();
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

    if (m_databaseConnection)
        m_databaseConnection->connectionToClient().didStartTransaction(m_transactionInfo.identifier(), error);
}

} // namespace IDBServer
} // namespace WebCore
