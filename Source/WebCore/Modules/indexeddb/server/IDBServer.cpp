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

#if ENABLE(INDEXED_DATABASE)

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

Ref<IDBServer> IDBServer::create(PAL::SessionID sessionID, IDBBackingStoreTemporaryFileHandler& fileHandler, QuotaManagerGetter&& quotaManagerGetter)
{
    return adoptRef(*new IDBServer(sessionID, fileHandler, WTFMove(quotaManagerGetter)));
}

Ref<IDBServer> IDBServer::create(PAL::SessionID sessionID, const String& databaseDirectoryPath, IDBBackingStoreTemporaryFileHandler& fileHandler, QuotaManagerGetter&& quotaManagerGetter)
{
    return adoptRef(*new IDBServer(sessionID, databaseDirectoryPath, fileHandler, WTFMove(quotaManagerGetter)));
}

IDBServer::IDBServer(PAL::SessionID sessionID, IDBBackingStoreTemporaryFileHandler& fileHandler, QuotaManagerGetter&& quotaManagerGetter)
    : CrossThreadTaskHandler("IndexedDatabase Server", AutodrainedPoolForRunLoop::Use)
    , m_sessionID(sessionID)
    , m_backingStoreTemporaryFileHandler(fileHandler)
    , m_quotaManagerGetter(WTFMove(quotaManagerGetter))
{
}

IDBServer::IDBServer(PAL::SessionID sessionID, const String& databaseDirectoryPath, IDBBackingStoreTemporaryFileHandler& fileHandler, QuotaManagerGetter&& quotaManagerGetter)
    : CrossThreadTaskHandler("IndexedDatabase Server", AutodrainedPoolForRunLoop::Use)
    , m_sessionID(sessionID)
    , m_databaseDirectoryPath(databaseDirectoryPath)
    , m_backingStoreTemporaryFileHandler(fileHandler)
    , m_quotaManagerGetter(WTFMove(quotaManagerGetter))
{
    LOG(IndexedDB, "IDBServer created at path %s", databaseDirectoryPath.utf8().data());
    postDatabaseTask(createCrossThreadTask(*this, &IDBServer::upgradeFilesIfNecessary));
}

void IDBServer::registerConnection(IDBConnectionToClient& connection)
{
    ASSERT(!m_connectionMap.contains(connection.identifier()));
    m_connectionMap.set(connection.identifier(), &connection);
}

void IDBServer::unregisterConnection(IDBConnectionToClient& connection)
{
    ASSERT(m_connectionMap.contains(connection.identifier()));
    ASSERT(m_connectionMap.get(connection.identifier()) == &connection);

    connection.connectionToClientClosed();

    m_connectionMap.remove(connection.identifier());
}

void IDBServer::registerTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    ASSERT(!m_transactions.contains(transaction.info().identifier()));
    m_transactions.set(transaction.info().identifier(), &transaction);
}

void IDBServer::unregisterTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    ASSERT(m_transactions.contains(transaction.info().identifier()));
    ASSERT(m_transactions.get(transaction.info().identifier()) == &transaction);

    m_transactions.remove(transaction.info().identifier());
}

void IDBServer::registerDatabaseConnection(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(!m_databaseConnections.contains(connection.identifier()));
    m_databaseConnections.set(connection.identifier(), &connection);
}

void IDBServer::unregisterDatabaseConnection(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(m_databaseConnections.contains(connection.identifier()));
    m_databaseConnections.remove(connection.identifier());
}

UniqueIDBDatabase& IDBServer::getOrCreateUniqueIDBDatabase(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(isMainThread());

    auto uniqueIDBDatabase = m_uniqueIDBDatabaseMap.add(identifier, nullptr);
    if (uniqueIDBDatabase.isNewEntry)
        uniqueIDBDatabase.iterator->value = makeUnique<UniqueIDBDatabase>(*this, identifier);

    return *uniqueIDBDatabase.iterator->value;
}

std::unique_ptr<IDBBackingStore> IDBServer::createBackingStore(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(!isMainThread());

    auto databaseDirectoryPath = this->databaseDirectoryPathIsolatedCopy();
    if (databaseDirectoryPath.isEmpty())
        return MemoryIDBBackingStore::create(m_sessionID, identifier);

    return makeUnique<SQLiteIDBBackingStore>(m_sessionID, identifier, databaseDirectoryPath, m_backingStoreTemporaryFileHandler);
}

void IDBServer::openDatabase(const IDBRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::openDatabase");

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
    LOG(IndexedDB, "IDBServer::deleteDatabase - %s", requestData.databaseIdentifier().debugString().utf8().data());
    ASSERT(isMainThread());

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
}

std::unique_ptr<UniqueIDBDatabase> IDBServer::closeAndTakeUniqueIDBDatabase(UniqueIDBDatabase& database)
{
    LOG(IndexedDB, "IDBServer::closeUniqueIDBDatabase");
    ASSERT(isMainThread());

    auto uniquePointer = m_uniqueIDBDatabaseMap.take(database.identifier());
    ASSERT(uniquePointer);

    return uniquePointer;
}

void IDBServer::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::abortTransaction");

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

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->createObjectStore(requestData, info);
}

void IDBServer::deleteObjectStore(const IDBRequestData& requestData, const String& objectStoreName)
{
    LOG(IndexedDB, "IDBServer::deleteObjectStore");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->deleteObjectStore(requestData, objectStoreName);
}

void IDBServer::renameObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBServer::renameObjectStore");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->renameObjectStore(requestData, objectStoreIdentifier, newName);
}

void IDBServer::clearObjectStore(const IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    LOG(IndexedDB, "IDBServer::clearObjectStore");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->clearObjectStore(requestData, objectStoreIdentifier);
}

void IDBServer::createIndex(const IDBRequestData& requestData, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBServer::createIndex");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->createIndex(requestData, info);
}

void IDBServer::deleteIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBServer::deleteIndex");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void IDBServer::renameIndex(const IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBServer::renameIndex");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void IDBServer::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "IDBServer::putOrAdd");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->putOrAdd(requestData, keyData, value, overwriteMode);
}

void IDBServer::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "IDBServer::getRecord");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->getRecord(requestData, getRecordData);
}

void IDBServer::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData)
{
    LOG(IndexedDB, "IDBServer::getAllRecords");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->getAllRecords(requestData, getAllRecordsData);
}

void IDBServer::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBServer::getCount");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->getCount(requestData, keyRangeData);
}

void IDBServer::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBServer::deleteRecord");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->deleteRecord(requestData, keyRangeData);
}

void IDBServer::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBServer::openCursor");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->openCursor(requestData, info);
}

void IDBServer::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "IDBServer::iterateCursor");

    auto transaction = m_transactions.get(requestData.transactionIdentifier());
    if (!transaction)
        return;

    transaction->iterateCursor(requestData, data);
}

void IDBServer::establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "IDBServer::establishTransaction");

    auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->establishTransaction(info);
}

void IDBServer::commitTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::commitTransaction");

    auto transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        // If there is no transaction there is nothing to commit.
        // We also have no access to a connection over which to message failure-to-commit.
        return;
    }

    transaction->commit();
}

void IDBServer::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::didFinishHandlingVersionChangeTransaction - %s", transactionIdentifier.loggingString().utf8().data());

    auto* connection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!connection)
        return;

    connection->didFinishHandlingVersionChange(transactionIdentifier);
}

void IDBServer::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBServer::databaseConnectionPendingClose - %" PRIu64, databaseConnectionIdentifier);

    auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->connectionPendingCloseFromClient();
}

void IDBServer::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBServer::databaseConnectionClosed - %" PRIu64, databaseConnectionIdentifier);

    auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->connectionClosedFromClient();
}

void IDBServer::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::abortOpenAndUpgradeNeeded");

    auto transaction = m_transactions.get(transactionIdentifier);
    if (transaction)
        transaction->abortWithoutCallback();

    auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->connectionClosedFromClient();
}

void IDBServer::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier)
{
    LOG(IndexedDB, "IDBServer::didFireVersionChangeEvent");

    if (auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier))
        databaseConnection->didFireVersionChangeEvent(requestIdentifier);
}

void IDBServer::openDBRequestCancelled(const IDBRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::openDBRequestCancelled");
    ASSERT(isMainThread());

    auto* uniqueIDBDatabase = m_uniqueIDBDatabaseMap.get(requestData.databaseIdentifier());
    if (!uniqueIDBDatabase)
        return;

    uniqueIDBDatabase->openDBRequestCancelled(requestData.requestIdentifier());
}

void IDBServer::confirmDidCloseFromServer(uint64_t databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBServer::confirmDidCloseFromServer");

    if (auto databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier))
        databaseConnection->confirmDidCloseFromServer();
}

void IDBServer::getAllDatabaseNames(uint64_t serverConnectionIdentifier, const SecurityOriginData& mainFrameOrigin, const SecurityOriginData& openingOrigin, uint64_t callbackID)
{
    postDatabaseTask(createCrossThreadTask(*this, &IDBServer::performGetAllDatabaseNames, serverConnectionIdentifier, mainFrameOrigin, openingOrigin, callbackID));
}

void IDBServer::performGetAllDatabaseNames(uint64_t serverConnectionIdentifier, const SecurityOriginData& mainFrameOrigin, const SecurityOriginData& openingOrigin, uint64_t callbackID)
{
    auto databaseDirectoryPath = this->databaseDirectoryPathIsolatedCopy();
    String oldDirectory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(mainFrameOrigin, openingOrigin, databaseDirectoryPath, "v0");
    Vector<String> files = FileSystem::listDirectory(oldDirectory, "*"_s);
    Vector<String> databases;
    for (auto& file : files) {
        String encodedName = FileSystem::lastComponentOfPathIgnoringTrailingSlash(file);
        databases.append(SQLiteIDBBackingStore::databaseNameFromEncodedFilename(encodedName));
    }

    String directory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(mainFrameOrigin, openingOrigin, databaseDirectoryPath, "v1");
    files = FileSystem::listDirectory(directory, "*"_s);
    for (auto& file : files) {
        auto databaseName = SQLiteIDBBackingStore::databaseNameFromFile(SQLiteIDBBackingStore::fullDatabasePathForDirectory(file));
        if (!databaseName.isEmpty())
            databases.append(databaseName);
    }

    postDatabaseTaskReply(createCrossThreadTask(*this, &IDBServer::didGetAllDatabaseNames, serverConnectionIdentifier, callbackID, databases));
}

void IDBServer::didGetAllDatabaseNames(uint64_t serverConnectionIdentifier, uint64_t callbackID, const Vector<String>& databaseNames)
{
    auto connection = m_connectionMap.get(serverConnectionIdentifier);
    if (!connection)
        return;

    connection->didGetAllDatabaseNames(callbackID, databaseNames);
}

void IDBServer::postDatabaseTask(CrossThreadTask&& task)
{
    postTask(WTFMove(task));
}

void IDBServer::postDatabaseTaskReply(CrossThreadTask&& task)
{
    postTaskReply(WTFMove(task));
}

static uint64_t generateDeleteCallbackID()
{
    ASSERT(isMainThread());
    static uint64_t currentID = 0;
    return ++currentID;
}

void IDBServer::closeAndDeleteDatabasesModifiedSince(WallTime modificationTime, Function<void ()>&& completionHandler)
{
    uint64_t callbackID = generateDeleteCallbackID();
    auto addResult = m_deleteDatabaseCompletionHandlers.add(callbackID, WTFMove(completionHandler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    // If the modification time is in the future, don't both doing anything.
    if (modificationTime > WallTime::now()) {
        postDatabaseTaskReply(createCrossThreadTask(*this, &IDBServer::didPerformCloseAndDeleteDatabases, callbackID));
        return;
    }

    HashSet<UniqueIDBDatabase*> openDatabases;
    for (auto& database : m_uniqueIDBDatabaseMap.values())
        openDatabases.add(database.get());

    for (auto& database : openDatabases)
        database->immediateCloseForUserDelete();

    postDatabaseTask(createCrossThreadTask(*this, &IDBServer::performCloseAndDeleteDatabasesModifiedSince, modificationTime, callbackID));
}

void IDBServer::closeAndDeleteDatabasesForOrigins(const Vector<SecurityOriginData>& origins, Function<void ()>&& completionHandler)
{
    uint64_t callbackID = generateDeleteCallbackID();
    auto addResult = m_deleteDatabaseCompletionHandlers.add(callbackID, WTFMove(completionHandler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    HashSet<UniqueIDBDatabase*> openDatabases;
    for (auto& database : m_uniqueIDBDatabaseMap.values()) {
        const auto& identifier = database->identifier();
        for (auto& origin : origins) {
            if (identifier.isRelatedToOrigin(origin)) {
                openDatabases.add(database.get());
                break;
            }
        }
    }

    for (auto& database : openDatabases)
        database->immediateCloseForUserDelete();

    postDatabaseTask(createCrossThreadTask(*this, &IDBServer::performCloseAndDeleteDatabasesForOrigins, origins, callbackID));
}

static void removeAllDatabasesForFullOriginPath(const String& originPath, WallTime modifiedSince)
{
    LOG(IndexedDB, "removeAllDatabasesForOriginPath with originPath %s", originPath.utf8().data());
    Vector<String> databasePaths = FileSystem::listDirectory(originPath, "*");

    for (auto& databasePath : databasePaths) {
        String databaseFile = FileSystem::pathByAppendingComponent(databasePath, "IndexedDB.sqlite3");
        if (modifiedSince > -WallTime::infinity() && FileSystem::fileExists(databaseFile)) {
            auto modificationTime = FileSystem::getFileModificationTime(databaseFile);
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
        //
        // IndexedDB blob files are named "N.blob" where N is a decimal integer,
        // so those are the only blob files we should be trying to delete.
        for (auto& blobPath : FileSystem::listDirectory(databasePath, "[0-9]*.blob")) {
            // Globbing can't give us only filenames starting with 1-or-more digits.
            // The above globbing gives us files that start with a digit and ends with ".blob", but there might be non-digits in between.
            // We need to validate that each filename contains only digits before deleting it, as any other files are not ones we put there.
            String filename = FileSystem::pathGetFileName(blobPath);
            auto filenameLength = filename.length();

            ASSERT(filenameLength >= 6);
            ASSERT(filename.endsWith(".blob"));

            if (filename.length() < 6)
                continue;
            if (!filename.endsWith(".blob"))
                continue;

            bool validFilename = true;
            for (unsigned i = 0; i < filenameLength - 5; ++i) {
                if (!isASCIIDigit(filename[i])) {
                    validFilename = false;
                    break;
                }
            }

            if (validFilename)
                FileSystem::deleteFile(blobPath);
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
    
    auto directories = FileSystem::listDirectory(originPath, "*"_s);
    for (auto& directory : directories) {
        String databaseIdentifier = FileSystem::lastComponentOfPathIgnoringTrailingSlash(directory);
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
            removeAllDatabasesForFullOriginPath(directory, modifiedSince);
    }
    
    removeAllDatabasesForFullOriginPath(originPath, modifiedSince);
}

void IDBServer::removeDatabasesModifiedSinceForVersion(WallTime modifiedSince, const String& version)
{
    String versionPath = FileSystem::pathByAppendingComponent(databaseDirectoryPathIsolatedCopy(), version);
    for (auto& originPath : FileSystem::listDirectory(versionPath, "*")) {
        String databaseIdentifier = FileSystem::lastComponentOfPathIgnoringTrailingSlash(originPath);
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
            removeAllDatabasesForOriginPath(originPath, modifiedSince);
    }
}

void IDBServer::performCloseAndDeleteDatabasesModifiedSince(WallTime modifiedSince, uint64_t callbackID)
{
    if (!databaseDirectoryPathIsolatedCopy().isEmpty()) {
        removeDatabasesModifiedSinceForVersion(modifiedSince, "v0");
        removeDatabasesModifiedSinceForVersion(modifiedSince, "v1");
    }

    postDatabaseTaskReply(createCrossThreadTask(*this, &IDBServer::didPerformCloseAndDeleteDatabases, callbackID));
}

void IDBServer::removeDatabasesWithOriginsForVersion(const Vector<SecurityOriginData> &origins, const String& version)
{
    String versionPath = FileSystem::pathByAppendingComponent(databaseDirectoryPathIsolatedCopy(), version);
    for (const auto& origin : origins) {
        String originPath = FileSystem::pathByAppendingComponent(versionPath, origin.databaseIdentifier());
        removeAllDatabasesForOriginPath(originPath, -WallTime::infinity());
        
        for (auto& topOriginPath : FileSystem::listDirectory(versionPath, "*")) {
            originPath = FileSystem::pathByAppendingComponent(topOriginPath, origin.databaseIdentifier());
            removeAllDatabasesForOriginPath(originPath, -WallTime::infinity());
        }
    }
}
    
void IDBServer::performCloseAndDeleteDatabasesForOrigins(const Vector<SecurityOriginData>& origins, uint64_t callbackID)
{
    if (!databaseDirectoryPathIsolatedCopy().isEmpty()) {
        removeDatabasesWithOriginsForVersion(origins, "v0");
        removeDatabasesWithOriginsForVersion(origins, "v1");
    }

    postDatabaseTaskReply(createCrossThreadTask(*this, &IDBServer::didPerformCloseAndDeleteDatabases, callbackID));
}

void IDBServer::didPerformCloseAndDeleteDatabases(uint64_t callbackID)
{
    for (auto& user : m_quotaUsers.values())
        user->resetSpaceUsed();

    auto callback = m_deleteDatabaseCompletionHandlers.take(callbackID);
    ASSERT(callback);
    callback();
}

IDBServer::QuotaUser::QuotaUser(IDBServer& server, StorageQuotaManager* manager, ClientOrigin&& origin)
    : m_server(server)
    , m_manager(makeWeakPtr(manager))
    , m_origin(WTFMove(origin))
    , m_isInitialized(m_server.m_sessionID.isEphemeral())
{
    if (manager)
        manager->addUser(*this);
}

IDBServer::QuotaUser::~QuotaUser()
{
    if (m_manager)
        m_manager->removeUser(*this);
}

void IDBServer::QuotaUser::resetSpaceUsed()
{
    m_spaceUsed = 0;
    m_estimatedSpaceIncrease = 0;

    if (!m_manager)
        return;

    if (m_server.m_sessionID.isEphemeral())
        return;

    if (!m_isInitialized)
        return;

    ASSERT(!m_initializationCallback);

    m_isInitialized = false;

    // Do add/remove to trigger call to whenInitialized.
    m_manager->removeUser(*this);
    m_manager->addUser(*this);
}

void IDBServer::QuotaUser::increaseSpaceUsed(uint64_t size)
{
    if (!m_isInitialized)
        return;
    ASSERT(m_spaceUsed + size > m_spaceUsed);
    m_spaceUsed += size;
}
void IDBServer::QuotaUser::decreaseSpaceUsed(uint64_t size)
{
    if (!m_isInitialized)
        return;
    ASSERT(m_spaceUsed >= size);
    m_spaceUsed -= size;
}

void IDBServer::QuotaUser::whenInitialized(CompletionHandler<void()>&& callback)
{
    if (m_isInitialized) {
        callback();
        return;
    }
    m_initializationCallback = WTFMove(callback);
    m_server.startComputingSpaceUsedForOrigin(m_origin);
}

void IDBServer::QuotaUser::initializeSpaceUsed(uint64_t spaceUsed)
{
    ASSERT(m_isInitialized || !m_estimatedSpaceIncrease);
    m_spaceUsed = spaceUsed;
    m_isInitialized = true;

    if (auto callback = WTFMove(m_initializationCallback))
        callback();
}

IDBServer::QuotaUser& IDBServer::ensureQuotaUser(const ClientOrigin& origin)
{
    return *m_quotaUsers.ensure(origin, [this, &origin] {
        return makeUnique<QuotaUser>(*this, m_quotaManagerGetter(m_sessionID, origin), ClientOrigin { origin });
    }).iterator->value;
}

void IDBServer::startComputingSpaceUsedForOrigin(const ClientOrigin& origin)
{
    ASSERT(!m_sessionID.isEphemeral());
    postDatabaseTask(createCrossThreadTask(*this, &IDBServer::computeSpaceUsedForOrigin, origin));
}

void IDBServer::computeSpaceUsedForOrigin(const ClientOrigin& origin)
{
    ASSERT(!isMainThread());

    auto databaseDirectoryPath = this->databaseDirectoryPathIsolatedCopy();
    auto oldVersionOriginDirectory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin.topOrigin, origin.clientOrigin, databaseDirectoryPath, "v0");
    auto newVersionOriginDirectory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin.topOrigin, origin.clientOrigin, databaseDirectoryPath, "v1");
    auto size = SQLiteIDBBackingStore::databasesSizeForDirectory(oldVersionOriginDirectory) + SQLiteIDBBackingStore::databasesSizeForDirectory(newVersionOriginDirectory);

    postDatabaseTaskReply(createCrossThreadTask(*this, &IDBServer::finishComputingSpaceUsedForOrigin, origin, size));
}

void IDBServer::finishComputingSpaceUsedForOrigin(const ClientOrigin& origin, uint64_t spaceUsed)
{
    ensureQuotaUser(origin).initializeSpaceUsed(spaceUsed);
}

void IDBServer::requestSpace(const ClientOrigin& origin, uint64_t taskSize, CompletionHandler<void(StorageQuotaManager::Decision)>&& callback)
{
    auto* quotaManager = ensureQuotaUser(origin).manager();
    if (!quotaManager) {
        callback(StorageQuotaManager::Decision::Deny);
        return;
    }

    quotaManager->requestSpace(taskSize, WTFMove(callback));
}

void IDBServer::resetSpaceUsed(const ClientOrigin& origin)
{
    if (auto* user = m_quotaUsers.get(origin))
        user->resetSpaceUsed();
}

void IDBServer::increaseSpaceUsed(const ClientOrigin& origin, uint64_t size)
{
    ensureQuotaUser(origin).increaseSpaceUsed(size);
}

void IDBServer::decreaseSpaceUsed(const ClientOrigin& origin, uint64_t size)
{
    ensureQuotaUser(origin).decreaseSpaceUsed(size);
}

void IDBServer::increasePotentialSpaceUsed(const ClientOrigin& origin, uint64_t taskSize)
{
    ensureQuotaUser(origin).increasePotentialSpaceUsed(taskSize);
}

void IDBServer::decreasePotentialSpaceUsed(const ClientOrigin& origin, uint64_t spaceUsed)
{
    ensureQuotaUser(origin).decreasePotentialSpaceUsed(spaceUsed);
}

void IDBServer::upgradeFilesIfNecessary()
{
    auto databaseDirectoryPath = this->databaseDirectoryPathIsolatedCopy();
    if (databaseDirectoryPath.isEmpty() || !FileSystem::fileExists(databaseDirectoryPath))
        return;

    String newVersionDirectory = FileSystem::pathByAppendingComponent(databaseDirectoryPath, "v1");
    if (!FileSystem::fileExists(newVersionDirectory))
        FileSystem::makeAllDirectories(newVersionDirectory);
}

void IDBServer::tryStop(ShouldForceStop shouldForceStop)
{
    // Only stop non-ephemeral IDBServers that can hold locked database files.
    if (m_sessionID.isEphemeral())
        return;

    suspendAndWait();
    if (shouldForceStop == ShouldForceStop::No && SQLiteDatabaseTracker::hasTransactionInProgress()) {
        CrossThreadTaskHandler::resume();
        return;
    }

    for (auto& database : m_uniqueIDBDatabaseMap.values())
        database->finishActiveTransactions();
}

void IDBServer::resume()
{
    if (m_sessionID.isEphemeral())
        return;

    CrossThreadTaskHandler::resume();
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
