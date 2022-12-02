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
#include "IDBServer.h"

#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "Logging.h"
#include "MemoryIDBBackingStore.h"
#include "SQLiteDatabase.h"
#include "SQLiteDatabaseTracker.h"
#include "SQLiteFileSystem.h"
#include "SQLiteIDBBackingStore.h"
#include "SQLiteStatement.h"
#include "SecurityOrigin.h"
#include "StorageQuotaManager.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/Locker.h>
#include <wtf/MainThread.h>

namespace WebCore {
namespace IDBServer {

IDBServer::IDBServer(PAL::SessionID sessionID, const String& databaseDirectoryPath, StorageQuotaManagerSpaceRequester&& spaceRequester, Lock& lock)
    : m_sessionID(sessionID)
    , m_spaceRequester(WTFMove(spaceRequester))
    , m_lock(lock)
{
    ASSERT(!isMainThread());
    ASSERT(databaseDirectoryPath.isSafeToSendToAnotherThread());

    m_databaseDirectoryPath = databaseDirectoryPath;
    upgradeFilesIfNecessary();
}

IDBServer::~IDBServer()
{
    ASSERT(!isMainThread());

    for (auto& database : m_uniqueIDBDatabaseMap.values())
        database->immediateClose();
}

void IDBServer::registerConnection(IDBConnectionToClient& connection)
{
    ASSERT(!isMainThread());
    ASSERT(!m_connectionMap.contains(connection.identifier()));
    m_connectionMap.set(connection.identifier(), &connection);
}

void IDBServer::unregisterConnection(IDBConnectionToClient& connection)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());
    ASSERT(m_connectionMap.contains(connection.identifier()));
    ASSERT(m_connectionMap.get(connection.identifier()) == &connection);

    connection.connectionToClientClosed();

    m_connectionMap.remove(connection.identifier());
}

void IDBServer::registerTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());
    ASSERT(!m_transactions.contains(transaction.info().identifier()));
    m_transactions.set(transaction.info().identifier(), &transaction);
}

void IDBServer::unregisterTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());
    ASSERT(m_transactions.contains(transaction.info().identifier()));
    ASSERT(m_transactions.get(transaction.info().identifier()) == &transaction);

    m_transactions.remove(transaction.info().identifier());
}

void IDBServer::registerConnection(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(!m_databaseConnections.contains(connection.identifier()));
    m_databaseConnections.set(connection.identifier(), &connection);
}

void IDBServer::unregisterConnection(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(m_databaseConnections.contains(connection.identifier()));
    m_databaseConnections.remove(connection.identifier());
}

UniqueIDBDatabase& IDBServer::getOrCreateUniqueIDBDatabase(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(!isMainThread());

    auto uniqueIDBDatabase = m_uniqueIDBDatabaseMap.add(identifier, nullptr);
    if (uniqueIDBDatabase.isNewEntry)
        uniqueIDBDatabase.iterator->value = makeUnique<UniqueIDBDatabase>(*this, identifier);

    return *uniqueIDBDatabase.iterator->value;
}

std::unique_ptr<IDBBackingStore> IDBServer::createBackingStore(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(!isMainThread());
    if (m_databaseDirectoryPath.isEmpty())
        return makeUnique<MemoryIDBBackingStore>(identifier);

    ASSERT(!m_sessionID.isEphemeral());
    if (identifier.isTransient())
        return makeUnique<MemoryIDBBackingStore>(identifier);

    return makeUnique<SQLiteIDBBackingStore>(identifier, upgradedDatabaseDirectory(identifier));
}

void IDBServer::openDatabase(const IDBRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::openDatabase");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto& uniqueIDBDatabase = getOrCreateUniqueIDBDatabase(requestData.databaseIdentifier());

    auto connection = m_connectionMap.get(requestData.requestIdentifier().connectionIdentifier());
    if (!connection) {
        // If the connection back to the client is gone, there's no way to open the database as
        // well as no way to message back failure.
        return;
    }

    uniqueIDBDatabase.openDatabaseConnection(*connection, requestData);
}

void IDBServer::deleteDatabase(const IDBRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::deleteDatabase - %s", requestData.databaseIdentifier().loggingString().utf8().data());
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto connection = m_connectionMap.get(requestData.requestIdentifier().connectionIdentifier());
    if (!connection) {
        // If the connection back to the client is gone, there's no way to delete the database as
        // well as no way to message back failure.
        return;
    }

    auto* database = m_uniqueIDBDatabaseMap.get(requestData.databaseIdentifier());
    if (!database)
        database = &getOrCreateUniqueIDBDatabase(requestData.databaseIdentifier());

    database->handleDelete(*connection, requestData);
    if (database->tryClose())
        m_uniqueIDBDatabaseMap.remove(database->identifier());
}

void IDBServer::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::abortTransaction");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        // If there is no transaction there is nothing to abort.
        // We also have no access to a connection over which to message failure-to-abort.
        return;
    }

    transaction->abort();
}

void IDBServer::createObjectStore(const IDBRequestData& requestData, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "IDBServer::createObjectStore");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->createObjectStore(requestData, info);
}

void IDBServer::deleteObjectStore(const IDBRequestData& requestData, const String& objectStoreName)
{
    LOG(IndexedDB, "IDBServer::deleteObjectStore");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->deleteObjectStore(requestData, objectStoreName);
}

void IDBServer::renameObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBServer::renameObjectStore");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->renameObjectStore(requestData, objectStoreIdentifier, newName);
}

void IDBServer::clearObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "IDBServer::clearObjectStore");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->clearObjectStore(requestData, objectStoreIdentifier);
}

void IDBServer::createIndex(const IDBRequestData& requestData, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBServer::createIndex");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->createIndex(requestData, info);
}

void IDBServer::deleteIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBServer::deleteIndex");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void IDBServer::renameIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBServer::renameIndex");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void IDBServer::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "IDBServer::putOrAdd");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->putOrAdd(requestData, keyData, value, overwriteMode);
}

void IDBServer::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "IDBServer::getRecord");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->getRecord(requestData, getRecordData);
}

void IDBServer::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData)
{
    LOG(IndexedDB, "IDBServer::getAllRecords");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->getAllRecords(requestData, getAllRecordsData);
}

void IDBServer::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBServer::getCount");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->getCount(requestData, keyRangeData);
}

void IDBServer::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBServer::deleteRecord");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->deleteRecord(requestData, keyRangeData);
}

void IDBServer::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBServer::openCursor");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->openCursor(requestData, info);
}

void IDBServer::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "IDBServer::iterateCursor");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->iterateCursor(requestData, data);
}

void IDBServer::establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "IDBServer::establishTransaction");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto* databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    auto* database = databaseConnection->database();
    databaseConnection->establishTransaction(info);
    if (database->tryClose())
        m_uniqueIDBDatabaseMap.remove(database->identifier());
}

void IDBServer::commitTransaction(const IDBResourceIdentifier& transactionIdentifier, uint64_t pendingRequestCount)
{
    LOG(IndexedDB, "IDBServer::commitTransaction");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        // If there is no transaction there is nothing to commit.
        // We also have no access to a connection over which to message failure-to-commit.
        return;
    }

    transaction->commit(pendingRequestCount);
}

void IDBServer::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::didFinishHandlingVersionChangeTransaction - %s", transactionIdentifier.loggingString().utf8().data());
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto* connection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!connection)
        return;

    connection->didFinishHandlingVersionChange(transactionIdentifier);
}

void IDBServer::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBServer::databaseConnectionPendingClose - %" PRIu64, databaseConnectionIdentifier);
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->connectionPendingCloseFromClient();
}

void IDBServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBServer::databaseConnectionClosed - %" PRIu64, databaseConnectionIdentifier);
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto* databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    auto* database = databaseConnection->database();
    databaseConnection->connectionClosedFromClient();
    if (database->tryClose())
        m_uniqueIDBDatabaseMap.remove(database->identifier());
}

void IDBServer::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const std::optional<IDBResourceIdentifier>& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::abortOpenAndUpgradeNeeded");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    if (transactionIdentifier) {
        if (auto transaction = m_transactions.get(*transactionIdentifier))
            transaction->abortWithoutCallback();
    }

    auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->connectionClosedFromClient();
}

void IDBServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    LOG(IndexedDB, "IDBServer::didFireVersionChangeEvent");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    if (auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier))
        databaseConnection->didFireVersionChangeEvent(requestIdentifier, connectionClosed);
}

void IDBServer::openDBRequestCancelled(const IDBRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::openDBRequestCancelled");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto* uniqueIDBDatabase = m_uniqueIDBDatabaseMap.get(requestData.databaseIdentifier());
    if (!uniqueIDBDatabase)
        return;

    uniqueIDBDatabase->openDBRequestCancelled(requestData.requestIdentifier());
    if (uniqueIDBDatabase->tryClose())
        m_uniqueIDBDatabaseMap.remove(uniqueIDBDatabase->identifier());
}

static void getDatabaseNameAndVersionFromOriginDirectory(const String& directory, HashSet<String>& excludedDatabasePaths, Vector<IDBDatabaseNameAndVersion>& result)
{
    Vector<String> databaseDirectoryNames = FileSystem::listDirectory(directory);
    for (auto& databaseDirectoryName : databaseDirectoryNames) {
        auto fullDatabasePath = SQLiteIDBBackingStore::fullDatabasePathForDirectory(FileSystem::pathByAppendingComponent(directory, databaseDirectoryName));
        if (excludedDatabasePaths.contains(fullDatabasePath))
            continue;

        if (auto nameAndVersion = SQLiteIDBBackingStore::databaseNameAndVersionFromFile(fullDatabasePath))
            result.append(WTFMove(*nameAndVersion));
    }
}

void IDBServer::getAllDatabaseNamesAndVersions(IDBConnectionIdentifier serverConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, const ClientOrigin& origin)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    Vector<IDBDatabaseNameAndVersion> result;
    HashSet<String> visitedDatabasePaths;

    for (auto& database : m_uniqueIDBDatabaseMap.values()) {
        if (database->identifier().origin() != origin)
            continue;

        auto path = database->filePath();
        if (!path.isEmpty())
            visitedDatabasePaths.add(path);

        if (auto nameAndVersion = database->nameAndVersion())
            result.append(WTFMove(*nameAndVersion));
    }

    auto oldDirectory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, m_databaseDirectoryPath, "v0"_s);
    getDatabaseNameAndVersionFromOriginDirectory(oldDirectory, visitedDatabasePaths, result);

    auto directory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, m_databaseDirectoryPath, "v1"_s);
    getDatabaseNameAndVersionFromOriginDirectory(directory, visitedDatabasePaths, result);

    auto connection = m_connectionMap.get(serverConnectionIdentifier);
    if (!connection)
        return;

    connection->didGetAllDatabaseNamesAndVersions(requestIdentifier, WTFMove(result));
}

static void collectOriginsForVersion(const String& versionPath, HashSet<WebCore::SecurityOriginData>& securityOrigins)
{
    for (auto& databaseIdentifier : FileSystem::listDirectory(versionPath)) {
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier)) {
            securityOrigins.add(WTFMove(*securityOrigin));
        
            for (auto& databaseIdentifier : FileSystem::listDirectory(FileSystem::pathByAppendingComponent(versionPath, databaseIdentifier))) {
                if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
                    securityOrigins.add(WTFMove(*securityOrigin));
            }
        }
    }
}

HashSet<SecurityOriginData> IDBServer::getOrigins() const
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    if (m_databaseDirectoryPath.isEmpty())
        return { };

    HashSet<WebCore::SecurityOriginData> securityOrigins;
    collectOriginsForVersion(FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, "v0"_s), securityOrigins);
    collectOriginsForVersion(FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, "v1"_s), securityOrigins);

    return securityOrigins;
}

void IDBServer::closeAndDeleteDatabasesModifiedSince(WallTime modificationTime)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    // If the modification time is in the future, don't both doing anything.
    if (modificationTime > WallTime::now())
        return;

    HashSet<UniqueIDBDatabase*> openDatabases;
    for (auto& database : m_uniqueIDBDatabaseMap.values())
        database->immediateClose();

    m_uniqueIDBDatabaseMap.clear();

    if (!m_databaseDirectoryPath.isEmpty()) {
        removeDatabasesModifiedSinceForVersion(modificationTime, "v0"_s);
        removeDatabasesModifiedSinceForVersion(modificationTime, "v1"_s);
    }
}

void IDBServer::closeDatabasesForOrigins(const Vector<SecurityOriginData>& targetOrigins, Function<bool(const SecurityOriginData&, const ClientOrigin&)>&& filter)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    HashSet<UniqueIDBDatabase*> openDatabases;
    for (auto& database : m_uniqueIDBDatabaseMap.values()) {
        const auto& databaseOrigin = database->identifier().origin();
        bool filtered = WTF::anyOf(targetOrigins, [&databaseOrigin, &filter](auto& targetOrigin) {
            return filter(targetOrigin, databaseOrigin);
        });
        if (filtered)
            openDatabases.add(database.get());
    }

    for (auto& database : openDatabases) {
        database->immediateClose();
        m_uniqueIDBDatabaseMap.remove(database->identifier());
    }
}

void IDBServer::closeAndDeleteDatabasesForOrigins(const Vector<SecurityOriginData>& origins)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    closeDatabasesForOrigins(origins, [](const SecurityOriginData& origin, const ClientOrigin& databaseOrigin) -> bool {
        return databaseOrigin.isRelated(origin);
    });

    if (!m_databaseDirectoryPath.isEmpty()) {
        removeDatabasesWithOriginsForVersion(origins, "v0"_s);
        removeDatabasesWithOriginsForVersion(origins, "v1"_s);
    }
}

static void removeAllDatabasesForFullOriginPath(const String& originPath, WallTime modifiedSince)
{
    LOG(IndexedDB, "removeAllDatabasesForOriginPath with originPath %s", originPath.utf8().data());
    Vector<String> databaseNames = FileSystem::listDirectory(originPath);

    for (auto& databaseName : databaseNames) {
        auto databasePath = FileSystem::pathByAppendingComponent(originPath, databaseName);
        String databaseFile = FileSystem::pathByAppendingComponent(databasePath, "IndexedDB.sqlite3"_s);
        if (modifiedSince > -WallTime::infinity() && FileSystem::fileExists(databaseFile)) {
            auto modificationTime = FileSystem::fileModificationTime(databaseFile);
            if (!modificationTime)
                continue;

            if (modificationTime.value() < modifiedSince)
                continue;
        }

        // Deleting this database means we need to delete all files that represent it.
        // This includes:
        //     - The directory itself, which is named after the database.
        //     - IndexedDB.sqlite3 and related SQLite files.
        //     - Blob files that we stored in the directory.
        //
        // To be conservative, we should *not* try to delete files that are unexpected;
        // We should only delete files we think we put there.
        for (auto& fileName : FileSystem::listDirectory(databasePath)) {
            // IndexedDB blob files are named "N.blob" where N is a decimal integer,
            // so those are the only blob files we should be trying to delete.
            auto fileNameLength = fileName.length();
            if (fileNameLength < 6)
                continue;
            if (!fileName.endsWith(".blob"_s))
                continue;

            bool validFileName = true;
            for (unsigned i = 0; i < fileNameLength - 5; ++i) {
                if (!isASCIIDigit(fileName[i])) {
                    validFileName = false;
                    break;
                }
            }

            if (validFileName)
                FileSystem::deleteFile(FileSystem::pathByAppendingComponent(databasePath, fileName));
        }

        // Now delete IndexedDB.sqlite3 and related SQLite files.
        SQLiteFileSystem::deleteDatabaseFile(databaseFile);

        // And finally, if we can, delete the empty directory.
        FileSystem::deleteEmptyDirectory(databasePath);
    }

    // If no databases remain for this origin, we can delete the origin directory as well.
    FileSystem::deleteEmptyDirectory(originPath);
}

static void removeAllDatabasesForOriginPath(const String& originPath, WallTime modifiedSince)
{
    String databaseIdentifier = FileSystem::lastComponentOfPathIgnoringTrailingSlash(originPath);
    if (!SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
        return;
    
    auto directoryNames = FileSystem::listDirectory(originPath);
    for (auto& databaseIdentifier : directoryNames) {
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
            removeAllDatabasesForFullOriginPath(FileSystem::pathByAppendingComponent(originPath, databaseIdentifier), modifiedSince);
    }
    
    removeAllDatabasesForFullOriginPath(originPath, modifiedSince);
}

void IDBServer::removeDatabasesModifiedSinceForVersion(WallTime modifiedSince, const String& version)
{
    String versionPath = FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, version);
    for (auto& databaseIdentifier : FileSystem::listDirectory(versionPath)) {
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
            removeAllDatabasesForOriginPath(FileSystem::pathByAppendingComponent(versionPath, databaseIdentifier), modifiedSince);
    }
}

void IDBServer::removeDatabasesWithOriginsForVersion(const Vector<SecurityOriginData> &origins, const String& version)
{
    String versionPath = FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, version);
    for (const auto& origin : origins) {
        String originPath = FileSystem::pathByAppendingComponent(versionPath, origin.databaseIdentifier());
        removeAllDatabasesForOriginPath(originPath, -WallTime::infinity());
        
        for (auto& topOrigin : FileSystem::listDirectory(versionPath)) {
            auto topOriginPath = FileSystem::pathByAppendingComponent(versionPath, topOrigin);
            originPath = FileSystem::pathByAppendingComponent(topOriginPath, origin.databaseIdentifier());
            removeAllDatabasesForOriginPath(originPath, -WallTime::infinity());
        }
    }
}

void IDBServer::renameOrigin(const WebCore::SecurityOriginData& oldOrigin, const WebCore::SecurityOriginData& newOrigin)
{
    Vector<SecurityOriginData> targetOrigins = { oldOrigin };
    closeDatabasesForOrigins(targetOrigins, [](const SecurityOriginData& targetOrigin, const ClientOrigin& databaseOrigin) -> bool {
        return databaseOrigin.topOrigin == targetOrigin;
    });

    auto versionPath = FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, "v1"_s);
    auto oldOriginPath = FileSystem::pathByAppendingComponent(versionPath, oldOrigin.databaseIdentifier());
    auto newOriginPath = FileSystem::pathByAppendingComponent(versionPath, newOrigin.databaseIdentifier());
    if (FileSystem::fileExists(oldOriginPath))
        FileSystem::moveFile(oldOriginPath, newOriginPath);
}

void IDBServer::requestSpace(const ClientOrigin& origin, uint64_t taskSize, CompletionHandler<void(bool)>&& completionHandler) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    StorageQuotaManager::Decision result = StorageQuotaManager::Decision::Deny;

    // Release lock because space requesting could be blocked.
    m_lock.unlock();
    result = m_spaceRequester(origin, taskSize);
    m_lock.lock();

    completionHandler(result == StorageQuotaManager::Decision::Grant);
}

uint64_t IDBServer::diskUsage(const String& rootDirectory, const ClientOrigin& origin)
{
    ASSERT(!isMainThread());

    auto oldVersionOriginDirectory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, rootDirectory, "v0"_s);
    auto newVersionOriginDirectory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, rootDirectory, "v1"_s);
    return SQLiteIDBBackingStore::databasesSizeForDirectory(oldVersionOriginDirectory) + SQLiteIDBBackingStore::databasesSizeForDirectory(newVersionOriginDirectory);
}

void IDBServer::upgradeFilesIfNecessary()
{
    if (m_databaseDirectoryPath.isEmpty() || !FileSystem::fileExists(m_databaseDirectoryPath))
        return;

    String newVersionDirectory = FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, "v1"_s);
    if (!FileSystem::fileExists(newVersionDirectory))
        FileSystem::makeAllDirectories(newVersionDirectory);
}

String IDBServer::upgradedDatabaseDirectory(const WebCore::IDBDatabaseIdentifier& identifier)
{
    String oldOriginDirectory = identifier.databaseDirectoryRelativeToRoot(m_databaseDirectoryPath, "v0"_s);
    String oldDatabaseDirectory = FileSystem::pathByAppendingComponent(oldOriginDirectory, SQLiteIDBBackingStore::encodeDatabaseName(identifier.databaseName()));
    String newOriginDirectory = identifier.databaseDirectoryRelativeToRoot(m_databaseDirectoryPath, "v1"_s);
    String fileNameHash = SQLiteFileSystem::computeHashForFileName(identifier.databaseName());
    String newDatabaseDirectory = FileSystem::pathByAppendingComponent(newOriginDirectory, fileNameHash);
    FileSystem::makeAllDirectories(newDatabaseDirectory);

    if (FileSystem::fileExists(oldDatabaseDirectory)) {
        FileSystem::moveFile(oldDatabaseDirectory, newDatabaseDirectory);
        FileSystem::deleteEmptyDirectory(oldOriginDirectory);
    }

    return newDatabaseDirectory;
}

bool IDBServer::hasDatabaseActivitiesOnMainThread() const
{
    ASSERT(isMainThread());
    ASSERT(m_lock.isHeld());

    if (m_sessionID.isEphemeral())
        return false;

    for (auto& database : m_uniqueIDBDatabaseMap.values()) {
        if (!database->identifier().isTransient() && database->hasActiveTransactions())
            return true;
    }
    
    return false;
}

void IDBServer::stopDatabaseActivitiesOnMainThread()
{
    ASSERT(isMainThread());
    ASSERT(m_lock.isHeld());

    // Only stop non-ephemeral IDBServer that can hold database file lock.
    if (m_sessionID.isEphemeral())
        return;

    for (auto& database : m_uniqueIDBDatabaseMap.values()) {
        // Only stop databases with non-ephemeral backing store that can hold database file lock.
        if (!database->identifier().isTransient())
            database->abortActiveTransactions();
    }
}

} // namespace IDBServer
} // namespace WebCore
