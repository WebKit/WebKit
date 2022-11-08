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

#include "InProcessIDBServer.h"

#include <WebCore/ClientOrigin.h>
#include <WebCore/IDBConnectionToClient.h>
#include <WebCore/IDBConnectionToServer.h>
#include <WebCore/IDBCursorInfo.h>
#include <WebCore/IDBGetRecordData.h>
#include <WebCore/IDBIterateCursorData.h>
#include <WebCore/IDBKeyRangeData.h>
#include <WebCore/IDBOpenDBRequest.h>
#include <WebCore/IDBRequestData.h>
#include <WebCore/IDBResultData.h>
#include <WebCore/IDBTransactionInfo.h>
#include <WebCore/IDBValue.h>
#include <WebCore/StorageQuotaManager.h>
#include <wtf/threads/BinarySemaphore.h>

using namespace WebCore;

static constexpr uint64_t defaultPerOriginQuota =  1000 * MB;

Ref<InProcessIDBServer> InProcessIDBServer::create(PAL::SessionID sessionID)
{
    ASSERT(sessionID.isEphemeral());

    return adoptRef(*new InProcessIDBServer(sessionID));
}

Ref<InProcessIDBServer> InProcessIDBServer::create(PAL::SessionID sessionID, const String& databaseDirectoryPath)
{
    ASSERT(!sessionID.isEphemeral());

    return adoptRef(*new InProcessIDBServer(sessionID, databaseDirectoryPath));
}

InProcessIDBServer::~InProcessIDBServer()
{
    BinarySemaphore semaphore;
    dispatchTask([this, &semaphore] {
        {
            Locker locker { m_serverLock };
            m_server = nullptr;
        }
        m_connectionToClient = nullptr;
        semaphore.signal();
    });
    semaphore.wait();
}

StorageQuotaManager* InProcessIDBServer::quotaManager(const ClientOrigin& origin)
{
    return m_quotaManagers.ensure(origin, [] {
        return StorageQuotaManager::create(defaultPerOriginQuota, [] {
            return 0;
        }, [](uint64_t quota, uint64_t currentSpace, uint64_t spaceIncrease, auto callback) {
            callback(quota + currentSpace + spaceIncrease);
        });
    }).iterator->value.get();
}

static inline IDBServer::IDBServer::StorageQuotaManagerSpaceRequester storageQuotaManagerSpaceRequester(InProcessIDBServer& server)
{
    return [server = &server, weakServer = WeakPtr { server }](const ClientOrigin& origin, uint64_t spaceRequested) mutable {
        auto* storageQuotaManager = weakServer ? server->quotaManager(origin) : nullptr;
        return storageQuotaManager ? storageQuotaManager->requestSpaceOnBackgroundThread(spaceRequested) : StorageQuotaManager::Decision::Deny;
    };
}

InProcessIDBServer::InProcessIDBServer(PAL::SessionID sessionID, const String& databaseDirectoryPath)
    : m_queue(WorkQueue::create("com.apple.WebKit.IndexedDBServer"))
{
    ASSERT(isMainThread());
    m_connectionToServer = IDBClient::IDBConnectionToServer::create(*this);
    dispatchTask([this, protectedThis = Ref { *this }, sessionID, directory = databaseDirectoryPath.isolatedCopy(), spaceRequester = storageQuotaManagerSpaceRequester(*this)] () mutable {
        m_connectionToClient = IDBServer::IDBConnectionToClient::create(*this);

        Locker locker { m_serverLock };
        m_server = makeUnique<IDBServer::IDBServer>(sessionID, directory, WTFMove(spaceRequester), m_serverLock);
        m_server->registerConnection(*m_connectionToClient);
    });
}

IDBConnectionIdentifier InProcessIDBServer::identifier() const
{
    // An instance of InProcessIDBServer always has a 1:1 relationship with its instance of IDBServer.
    // Therefore the connection identifier between the two can always be "1".
    return Process::identifier();
}

IDBClient::IDBConnectionToServer& InProcessIDBServer::connectionToServer() const
{
    return *m_connectionToServer;
}

IDBServer::IDBConnectionToClient& InProcessIDBServer::connectionToClient() const
{
    return *m_connectionToClient;
}

void InProcessIDBServer::deleteDatabase(const WebCore::IDBRequestData& requestData)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->deleteDatabase(requestData);
    });
}

void InProcessIDBServer::didDeleteDatabase(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didDeleteDatabase(resultData);
    });
}

void InProcessIDBServer::openDatabase(const WebCore::IDBRequestData& requestData)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->openDatabase(requestData);
    });
}

void InProcessIDBServer::didOpenDatabase(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didOpenDatabase(resultData);
    });
}

void InProcessIDBServer::didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, transactionIdentifier = transactionIdentifier.isolatedCopy(), error = error.isolatedCopy()] {
        m_connectionToServer->didAbortTransaction(transactionIdentifier, error);
    });
}

void InProcessIDBServer::didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, transactionIdentifier = transactionIdentifier.isolatedCopy(), error = error.isolatedCopy()] {
        m_connectionToServer->didCommitTransaction(transactionIdentifier, error);
    });
}

void InProcessIDBServer::didCreateObjectStore(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didCreateObjectStore(resultData);
    });
}

void InProcessIDBServer::didDeleteObjectStore(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didDeleteObjectStore(resultData);
    });
}

void InProcessIDBServer::didRenameObjectStore(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didRenameObjectStore(resultData);
    });
}

void InProcessIDBServer::didClearObjectStore(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didClearObjectStore(resultData);
    });
}

void InProcessIDBServer::didCreateIndex(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didCreateIndex(resultData);
    });
}

void InProcessIDBServer::didDeleteIndex(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didDeleteIndex(resultData);
    });
}

void InProcessIDBServer::didRenameIndex(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didRenameIndex(resultData);
    });
}

void InProcessIDBServer::didPutOrAdd(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didPutOrAdd(resultData);
    });
}

void InProcessIDBServer::didGetRecord(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didGetRecord(resultData);
    });
}

void InProcessIDBServer::didGetAllRecords(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didGetAllRecords(resultData);
    });
}

void InProcessIDBServer::didGetCount(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didGetCount(resultData);
    });
}

void InProcessIDBServer::didDeleteRecord(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didDeleteRecord(resultData);
    });
}

void InProcessIDBServer::didOpenCursor(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didOpenCursor(resultData);
    });
}

void InProcessIDBServer::didIterateCursor(const IDBResultData& resultData)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy()] {
        m_connectionToServer->didIterateCursor(resultData);
    });
}

void InProcessIDBServer::abortTransaction(const WebCore::IDBResourceIdentifier& resourceIdentifier)
{
    dispatchTask([this, protectedThis = Ref { *this }, resourceIdentifier = resourceIdentifier.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->abortTransaction(resourceIdentifier);
    });
}

void InProcessIDBServer::commitTransaction(const WebCore::IDBResourceIdentifier& resourceIdentifier, uint64_t pendingCountRequest)
{
    dispatchTask([this, protectedThis = Ref { *this }, resourceIdentifier = resourceIdentifier.isolatedCopy(), pendingCountRequest] {
        Locker locker { m_serverLock };
        m_server->commitTransaction(resourceIdentifier, pendingCountRequest);
    });
}

void InProcessIDBServer::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    dispatchTask([this, protectedThis = Ref { *this }, databaseConnectionIdentifier, transactionIdentifier = transactionIdentifier.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->didFinishHandlingVersionChangeTransaction(databaseConnectionIdentifier, transactionIdentifier);
    });
}

void InProcessIDBServer::createObjectStore(const WebCore::IDBRequestData& resultData, const IDBObjectStoreInfo& info)
{
    dispatchTask([this, protectedThis = Ref { *this }, resultData = resultData.isolatedCopy(), info = info.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->createObjectStore(resultData, info);
    });
}

void InProcessIDBServer::deleteObjectStore(const WebCore::IDBRequestData& requestData, const String& objectStoreName)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), objectStoreName = objectStoreName.isolatedCopy()] () mutable {
        Locker locker { m_serverLock };
        m_server->deleteObjectStore(requestData, objectStoreName);
    });
}

void InProcessIDBServer::renameObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), objectStoreIdentifier, newName = newName.isolatedCopy()] () mutable {
        Locker locker { m_serverLock };
        m_server->renameObjectStore(requestData, objectStoreIdentifier, newName);
    });
}

void InProcessIDBServer::clearObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), objectStoreIdentifier] {
        Locker locker { m_serverLock };
        m_server->clearObjectStore(requestData, objectStoreIdentifier);
    });
}

void InProcessIDBServer::createIndex(const WebCore::IDBRequestData& requestData, const IDBIndexInfo& info)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), info = info.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->createIndex(requestData, info);
    });
}

void InProcessIDBServer::deleteIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), objectStoreIdentifier, indexName = indexName.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->deleteIndex(requestData, objectStoreIdentifier, indexName);
    });
}

void InProcessIDBServer::renameIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), objectStoreIdentifier, indexIdentifier, newName = newName.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
    });
}

void InProcessIDBServer::putOrAdd(const WebCore::IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, const IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), keyData = keyData.isolatedCopy(), value = value.isolatedCopy(), overwriteMode] {
        Locker locker { m_serverLock };
        m_server->putOrAdd(requestData, keyData, value, overwriteMode);
    });
}

void InProcessIDBServer::getRecord(const WebCore::IDBRequestData& requestData, const IDBGetRecordData& getRecordData)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), getRecordData = getRecordData.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->getRecord(requestData, getRecordData);
    });
}

void InProcessIDBServer::getAllRecords(const WebCore::IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), getAllRecordsData = getAllRecordsData.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->getAllRecords(requestData, getAllRecordsData);
    });
}

void InProcessIDBServer::getCount(const WebCore::IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), keyRangeData = keyRangeData.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->getCount(requestData, keyRangeData);
    });
}

void InProcessIDBServer::deleteRecord(const WebCore::IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), keyRangeData = keyRangeData.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->deleteRecord(requestData, keyRangeData);
    });
}

void InProcessIDBServer::openCursor(const WebCore::IDBRequestData& requestData, const IDBCursorInfo& info)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), info = info.isolatedCopy()] () mutable {
        Locker locker { m_serverLock };
        m_server->openCursor(requestData, info);
    });
}

void InProcessIDBServer::iterateCursor(const WebCore::IDBRequestData& requestData, const IDBIterateCursorData& data)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy(), data = data.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->iterateCursor(requestData, data);
    });
}

void InProcessIDBServer::establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo& info)
{
    dispatchTask([this, protectedThis = Ref { *this }, databaseConnectionIdentifier, info = info.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->establishTransaction(databaseConnectionIdentifier, info);
    });
}

void InProcessIDBServer::fireVersionChangeEvent(IDBServer::UniqueIDBDatabaseConnection& connection, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, databaseConnectionIdentifier = connection.identifier(), requestIdentifier = requestIdentifier.isolatedCopy(), requestedVersion] {
        m_connectionToServer->fireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier, requestedVersion);
    });
}

void InProcessIDBServer::didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, transactionIdentifier = transactionIdentifier.isolatedCopy(), error = error.isolatedCopy()] {
        m_connectionToServer->didStartTransaction(transactionIdentifier, error);
    });
}

void InProcessIDBServer::didCloseFromServer(IDBServer::UniqueIDBDatabaseConnection& connection, const IDBError& error)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, databaseConnectionIdentifier = connection.identifier(), error = error.isolatedCopy()] {
        m_connectionToServer->didCloseFromServer(databaseConnectionIdentifier, error);
    });
}

void InProcessIDBServer::notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, requestIdentifier = requestIdentifier.isolatedCopy(), oldVersion, newVersion] {
        m_connectionToServer->notifyOpenDBRequestBlocked(requestIdentifier, oldVersion, newVersion);
    });
}

void InProcessIDBServer::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    dispatchTask([this, protectedThis = Ref { *this }, databaseConnectionIdentifier] {
        Locker locker { m_serverLock };
        m_server->databaseConnectionPendingClose(databaseConnectionIdentifier);
    });
}

void InProcessIDBServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    dispatchTask([this, protectedThis = Ref { *this }, databaseConnectionIdentifier] {
        Locker locker { m_serverLock };
        m_server->databaseConnectionClosed(databaseConnectionIdentifier);
    });
}

void InProcessIDBServer::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const std::optional<WebCore::IDBResourceIdentifier>& transactionIdentifier)
{
    std::optional<WebCore::IDBResourceIdentifier> transactionIdentifierCopy;
    if (transactionIdentifier)
        transactionIdentifierCopy = transactionIdentifier->isolatedCopy();
    dispatchTask([this, protectedThis = Ref { *this }, databaseConnectionIdentifier, transactionIdentifier = WTFMove(transactionIdentifierCopy)] {
        Locker locker { m_serverLock };
        m_server->abortOpenAndUpgradeNeeded(databaseConnectionIdentifier, transactionIdentifier);
    });
}

void InProcessIDBServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    dispatchTask([this, protectedThis = Ref { *this }, databaseConnectionIdentifier, requestIdentifier = requestIdentifier.isolatedCopy(), connectionClosed] {
        Locker locker { m_serverLock };
        m_server->didFireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier, connectionClosed);
    });
}

void InProcessIDBServer::openDBRequestCancelled(const WebCore::IDBRequestData& requestData)
{
    dispatchTask([this, protectedThis = Ref { *this }, requestData = requestData.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->openDBRequestCancelled(requestData);
    });
}

void InProcessIDBServer::getAllDatabaseNamesAndVersions(const WebCore::IDBResourceIdentifier& requestIdentifier, const ClientOrigin& origin)
{
    dispatchTask([this, protectedThis = Ref { *this }, identifier = m_connectionToServer->identifier(), requestIdentifier, origin = origin.isolatedCopy()] {
        Locker locker { m_serverLock };
        m_server->getAllDatabaseNamesAndVersions(identifier, requestIdentifier, origin);
    });
}

void InProcessIDBServer::didGetAllDatabaseNamesAndVersions(const WebCore::IDBResourceIdentifier& requestIdentifier, Vector<WebCore::IDBDatabaseNameAndVersion>&& databases)
{
    dispatchTaskReply([this, protectedThis = Ref { *this }, requestIdentifier, databases = crossThreadCopy(WTFMove(databases))]() mutable {
        m_connectionToServer->didGetAllDatabaseNamesAndVersions(requestIdentifier, WTFMove(databases));
    });
}

void InProcessIDBServer::closeAndDeleteDatabasesModifiedSince(WallTime modificationTime)
{
    dispatchTask([this, protectedThis = Ref { *this }, modificationTime] {
        Locker locker { m_serverLock };
        m_server->closeAndDeleteDatabasesModifiedSince(modificationTime);
    });
}

void InProcessIDBServer::dispatchTask(Function<void()>&& function)
{
    ASSERT(isMainThread());
    m_queue->dispatch(WTFMove(function));
}

void InProcessIDBServer::dispatchTaskReply(Function<void()>&& function)
{
    ASSERT(!isMainThread());
    callOnMainThread(WTFMove(function));
}
