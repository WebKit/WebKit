/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "LegacyUniqueIDBDatabase.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "AsyncRequest.h"
#include "DataReference.h"
#include "DatabaseProcess.h"
#include "DatabaseProcessIDBConnection.h"
#include "Logging.h"
#include "UniqueIDBDatabaseBackingStoreSQLite.h"
#include "WebCrossThreadCopier.h"
#include <WebCore/CrossThreadTask.h>
#include <WebCore/FileSystem.h>
#include <WebCore/IDBDatabaseMetadata.h>
#include <WebCore/IDBGetResult.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBKeyRangeData.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

String LegacyUniqueIDBDatabase::calculateAbsoluteDatabaseFilename(const String& absoluteDatabaseDirectory)
{
    return pathByAppendingComponent(absoluteDatabaseDirectory, "IndexedDB.sqlite3");
}

LegacyUniqueIDBDatabase::LegacyUniqueIDBDatabase(const LegacyUniqueIDBDatabaseIdentifier& identifier)
    : m_identifier(identifier)
    , m_acceptingNewRequests(true)
    , m_didGetMetadataFromBackingStore(false)
{
    m_inMemory = !canShareDatabases(identifier.openingOrigin(), identifier.mainFrameOrigin());
    if (m_inMemory)
        return;

    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********

    // Each unique Indexed Database exists in a directory named for the database, which exists in a directory representing its opening origin.
    m_databaseRelativeDirectory = pathByAppendingComponent(databaseFilenameIdentifier(identifier.openingOrigin()), filenameForDatabaseName());

    DatabaseProcess::singleton().ensureIndexedDatabaseRelativePathExists(m_databaseRelativeDirectory);
}

LegacyUniqueIDBDatabase::~LegacyUniqueIDBDatabase()
{
    ASSERT(!m_acceptingNewRequests);
    ASSERT(m_pendingMetadataRequests.isEmpty());
}

String LegacyUniqueIDBDatabase::filenameForDatabaseName() const
{
    ASSERT(!m_identifier.databaseName().isNull());

    if (m_identifier.databaseName().isEmpty())
        return "%00";

    String filename = encodeForFileName(m_identifier.databaseName());
    filename.replace('.', "%2E");

    return filename;
}

String LegacyUniqueIDBDatabase::databaseFilenameIdentifier(const SecurityOriginData& originData) const
{
    Ref<SecurityOrigin> securityOrigin(SecurityOrigin::create(originData.protocol, originData.host, originData.port));
    return securityOrigin.get().databaseIdentifier();
}

bool LegacyUniqueIDBDatabase::canShareDatabases(const SecurityOriginData& openingOrigin, const SecurityOriginData& mainFrameOrigin) const
{
    // For now, an origin's database access is predicated on equality with the other origin.
    // We might need to make this more nuanced later.
    return openingOrigin == mainFrameOrigin;
}

void LegacyUniqueIDBDatabase::registerConnection(DatabaseProcessIDBConnection& connection)
{
    ASSERT(!m_connections.contains(&connection));
    m_connections.add(&connection);
}

void LegacyUniqueIDBDatabase::unregisterConnection(DatabaseProcessIDBConnection& connection)
{
    ASSERT(m_connections.contains(&connection));
    resetAllTransactions(connection);
    m_connections.remove(&connection);

    if (m_connections.isEmpty() && m_pendingTransactionRollbacks.isEmpty()) {
        shutdown(LegacyUniqueIDBDatabaseShutdownType::NormalShutdown);
        DatabaseProcess::singleton().removeLegacyUniqueIDBDatabase(*this);
    }
}

void LegacyUniqueIDBDatabase::shutdown(LegacyUniqueIDBDatabaseShutdownType type)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests)
        return;

    m_acceptingNewRequests = false;

    // Balanced by an adoptRef in ::didShutdownBackingStore()
    ref();

    {
        LockHolder locker(m_databaseTaskMutex);
        m_databaseTasks.clear();
    }

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::shutdownBackingStore, type, absoluteDatabaseDirectory()), DatabaseTaskType::Shutdown);
}

void LegacyUniqueIDBDatabase::shutdownBackingStore(LegacyUniqueIDBDatabaseShutdownType type, const String& databaseDirectory)
{
    ASSERT(!RunLoop::isMain());

    m_backingStore = nullptr;

    if (type == LegacyUniqueIDBDatabaseShutdownType::DeleteShutdown) {
        String dbFilename = LegacyUniqueIDBDatabase::calculateAbsoluteDatabaseFilename(databaseDirectory);
        LOG(IDB, "LegacyUniqueIDBDatabase::shutdownBackingStore deleting file '%s' on disk", dbFilename.utf8().data());
        deleteFile(dbFilename);
        deleteEmptyDirectory(databaseDirectory);
    }

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didShutdownBackingStore, type), DatabaseTaskType::Shutdown);
}

void LegacyUniqueIDBDatabase::didShutdownBackingStore(LegacyUniqueIDBDatabaseShutdownType type)
{
    ASSERT(RunLoop::isMain());

    // Balanced by a ref in ::shutdown()
    RefPtr<LegacyUniqueIDBDatabase> protector(adoptRef(this));

    // Empty out remaining main thread tasks.
    while (performNextMainThreadTask()) {
    }

    // No more requests will be handled, so abort all outstanding requests.
    for (const auto& request : m_pendingMetadataRequests)
        request->requestAborted();
    for (const auto& request : m_pendingTransactionRequests.values())
        request->requestAborted();
    for (const auto& request : m_pendingDatabaseTasks.values())
        request->requestAborted();

    m_pendingMetadataRequests.clear();
    m_pendingTransactionRequests.clear();
    m_pendingDatabaseTasks.clear();

    if (m_pendingShutdownTask)
        m_pendingShutdownTask->completeRequest(type);

    m_pendingShutdownTask = nullptr;
}

void LegacyUniqueIDBDatabase::deleteDatabase(std::function<void (bool)> successCallback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        // Someone else has already shutdown this database, so we can't request a delete.
        callOnMainThread([successCallback] {
            successCallback(false);
        });
        return;
    }

    RefPtr<LegacyUniqueIDBDatabase> protector(this);
    m_pendingShutdownTask = AsyncRequestImpl<LegacyUniqueIDBDatabaseShutdownType>::create([this, protector, successCallback](LegacyUniqueIDBDatabaseShutdownType type) {
        // If the shutdown just completed was a Delete shutdown then we succeeded.
        // If not report failure instead of trying again.
        successCallback(type == LegacyUniqueIDBDatabaseShutdownType::DeleteShutdown);
    }, [this, protector, successCallback] {
        successCallback(false);
    });

    shutdown(LegacyUniqueIDBDatabaseShutdownType::DeleteShutdown);
}

void LegacyUniqueIDBDatabase::getOrEstablishIDBDatabaseMetadata(std::function<void (bool, const IDBDatabaseMetadata&)> completionCallback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        completionCallback(false, IDBDatabaseMetadata());
        return;
    }

    // If we've already retrieved metadata from the backing store, return it now.
    if (m_didGetMetadataFromBackingStore) {
        if (m_metadata)
            completionCallback(true, *m_metadata);
        else
            completionCallback(false, IDBDatabaseMetadata());

        return;
    }

    // If this is the first unanswered metadata request, then later we need to
    // post a task to open the backing store and get metadata.
    bool shouldOpenBackingStore = m_pendingMetadataRequests.isEmpty();

    // Then remember this metadata request to be answered later.
    RefPtr<AsyncRequest> request = AsyncRequestImpl<>::create([completionCallback, this] {
        completionCallback(!!m_metadata, m_metadata ? *m_metadata : IDBDatabaseMetadata());
    }, [completionCallback] {
        // The boolean flag to the completion callback represents whether the
        // attempt to get/establish metadata succeeded or failed.
        // Since we're aborting the attempt, it failed, so we always pass in false.
        completionCallback(false, IDBDatabaseMetadata());
    });

    m_pendingMetadataRequests.append(request.release());

    if (shouldOpenBackingStore)
        postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::openBackingStoreAndReadMetadata, m_identifier, absoluteDatabaseDirectory()));
}

void LegacyUniqueIDBDatabase::openBackingStoreAndReadMetadata(const LegacyUniqueIDBDatabaseIdentifier& identifier, const String& databaseDirectory)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(!m_backingStore);

    if (m_inMemory) {
        LOG_ERROR("Support for in-memory databases not yet implemented");
        return;
    }

    m_backingStore = UniqueIDBDatabaseBackingStoreSQLite::create(identifier, databaseDirectory);
    std::unique_ptr<IDBDatabaseMetadata> metadata = m_backingStore->getOrEstablishMetadata();

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didOpenBackingStoreAndReadMetadata, metadata ? *metadata : IDBDatabaseMetadata(), !!metadata));
}

void LegacyUniqueIDBDatabase::didOpenBackingStoreAndReadMetadata(const IDBDatabaseMetadata& metadata, bool success)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_metadata);

    m_didGetMetadataFromBackingStore = true;

    if (success)
        m_metadata = std::make_unique<IDBDatabaseMetadata>(metadata);

    while (!m_pendingMetadataRequests.isEmpty()) {
        RefPtr<AsyncRequest> request = m_pendingMetadataRequests.takeFirst();
        request->completeRequest();
    }
}

void LegacyUniqueIDBDatabase::openTransaction(const IDBIdentifier& transactionIdentifier, const Vector<int64_t>& objectStoreIDs, IndexedDB::TransactionMode mode, std::function<void (bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::openBackingStoreTransaction, transactionIdentifier, objectStoreIDs, mode), successCallback);
}

void LegacyUniqueIDBDatabase::beginTransaction(const IDBIdentifier& transactionIdentifier, std::function<void (bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::beginBackingStoreTransaction, transactionIdentifier), successCallback);
}

void LegacyUniqueIDBDatabase::commitTransaction(const IDBIdentifier& transactionIdentifier, std::function<void (bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::commitBackingStoreTransaction, transactionIdentifier), successCallback);
}

void LegacyUniqueIDBDatabase::resetTransaction(const IDBIdentifier& transactionIdentifier, std::function<void (bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::resetBackingStoreTransaction, transactionIdentifier), successCallback);
}

void LegacyUniqueIDBDatabase::rollbackTransaction(const IDBIdentifier& transactionIdentifier, std::function<void (bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::rollbackBackingStoreTransaction, transactionIdentifier), successCallback);
}

void LegacyUniqueIDBDatabase::postTransactionOperation(const IDBIdentifier& transactionIdentifier, std::unique_ptr<CrossThreadTask> task, std::function<void (bool)> successCallback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    if (m_pendingTransactionRequests.contains(transactionIdentifier)) {
        LOG_ERROR("Attempting to queue an operation for a transaction that already has an operation pending. Each transaction should only have one operation pending at a time.");
        successCallback(false);
        return;
    }

    postDatabaseTask(WTFMove(task));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([successCallback](bool success) {
        successCallback(success);
    }, [successCallback] {
        successCallback(false);
    });

    m_pendingTransactionRequests.add(transactionIdentifier, request.release());
}

void LegacyUniqueIDBDatabase::didCompleteTransactionOperation(const IDBIdentifier& transactionIdentifier, bool success)
{
    ASSERT(RunLoop::isMain());

    RefPtr<AsyncRequest> request = m_pendingTransactionRequests.take(transactionIdentifier);

    if (request)
        request->completeRequest(success);

    if (m_pendingTransactionRollbacks.contains(transactionIdentifier))
        finalizeRollback(transactionIdentifier);
}

void LegacyUniqueIDBDatabase::changeDatabaseVersion(const IDBIdentifier& transactionIdentifier, uint64_t newVersion, std::function<void (bool)> successCallback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    uint64_t oldVersion = m_metadata->version;
    m_metadata->version = newVersion;

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([this, oldVersion, successCallback](bool success) {
        if (!success)
            m_metadata->version = oldVersion;
        successCallback(success);
    }, [this, oldVersion, successCallback] {
        m_metadata->version = oldVersion;
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::changeDatabaseVersionInBackingStore, requestID, transactionIdentifier, newVersion));
}

void LegacyUniqueIDBDatabase::didChangeDatabaseVersion(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void LegacyUniqueIDBDatabase::didCreateObjectStore(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void LegacyUniqueIDBDatabase::didDeleteObjectStore(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void LegacyUniqueIDBDatabase::didClearObjectStore(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void LegacyUniqueIDBDatabase::didCreateIndex(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void LegacyUniqueIDBDatabase::didDeleteIndex(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void LegacyUniqueIDBDatabase::didCompleteBoolRequest(uint64_t requestID, bool success)
{
    m_pendingDatabaseTasks.take(requestID).get().completeRequest(success);
}

void LegacyUniqueIDBDatabase::createObjectStore(const IDBIdentifier& transactionIdentifier, const IDBObjectStoreMetadata& metadata, std::function<void (bool)> successCallback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    ASSERT(!m_metadata->objectStores.contains(metadata.id));
    m_metadata->objectStores.set(metadata.id, metadata);
    int64_t addedObjectStoreID = metadata.id;

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([this, addedObjectStoreID, successCallback](bool success) {
        if (!success)
            m_metadata->objectStores.remove(addedObjectStoreID);
        successCallback(success);
    }, [this, addedObjectStoreID, successCallback] {
        m_metadata->objectStores.remove(addedObjectStoreID);
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::createObjectStoreInBackingStore, requestID, transactionIdentifier, metadata));
}

void LegacyUniqueIDBDatabase::deleteObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, std::function<void (bool)> successCallback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));
    IDBObjectStoreMetadata metadata = m_metadata->objectStores.take(objectStoreID);

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([this, metadata, successCallback](bool success) {
        if (!success)
            m_metadata->objectStores.set(metadata.id, metadata);
        successCallback(success);
    }, [this, metadata, successCallback] {
        m_metadata->objectStores.set(metadata.id, metadata);
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::deleteObjectStoreInBackingStore, requestID, transactionIdentifier, objectStoreID));
}

void LegacyUniqueIDBDatabase::clearObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, std::function<void (bool)> successCallback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([this, successCallback](bool success) {
        successCallback(success);
    }, [this, successCallback] {
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::clearObjectStoreInBackingStore, requestID, transactionIdentifier, objectStoreID));
}

void LegacyUniqueIDBDatabase::createIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBIndexMetadata& metadata, std::function<void (bool)> successCallback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));
    ASSERT(!m_metadata->objectStores.get(objectStoreID).indexes.contains(metadata.id));
    m_metadata->objectStores.get(objectStoreID).indexes.set(metadata.id, metadata);
    int64_t addedIndexID = metadata.id;

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([this, objectStoreID, addedIndexID, successCallback](bool success) {
        if (!success) {
            auto objectStoreFind = m_metadata->objectStores.find(objectStoreID);
            if (objectStoreFind != m_metadata->objectStores.end())
                objectStoreFind->value.indexes.remove(addedIndexID);
        }
        successCallback(success);
    }, [this, objectStoreID, addedIndexID, successCallback] {
        auto objectStoreFind = m_metadata->objectStores.find(objectStoreID);
        if (objectStoreFind != m_metadata->objectStores.end())
            objectStoreFind->value.indexes.remove(addedIndexID);
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::createIndexInBackingStore, requestID, transactionIdentifier, objectStoreID, metadata));
}

void LegacyUniqueIDBDatabase::deleteIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, std::function<void (bool)> successCallback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));
    ASSERT(m_metadata->objectStores.get(objectStoreID).indexes.contains(indexID));

    IDBIndexMetadata metadata = m_metadata->objectStores.get(objectStoreID).indexes.take(indexID);

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([this, objectStoreID, metadata, successCallback](bool success) {
        if (!success) {
            auto objectStoreFind = m_metadata->objectStores.find(objectStoreID);
            if (objectStoreFind != m_metadata->objectStores.end())
                objectStoreFind->value.indexes.set(metadata.id, metadata);
        }
        successCallback(success);
    }, [this, objectStoreID, metadata, successCallback] {
        auto objectStoreFind = m_metadata->objectStores.find(objectStoreID);
        if (objectStoreFind != m_metadata->objectStores.end())
            objectStoreFind->value.indexes.set(metadata.id, metadata);
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::deleteIndexInBackingStore, requestID, transactionIdentifier, objectStoreID, indexID));
}

void LegacyUniqueIDBDatabase::putRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyData& keyData, const IPC::DataReference& value, int64_t putMode, const Vector<int64_t>& indexIDs, const Vector<Vector<IDBKeyData>>& indexKeys, std::function<void (const IDBKeyData&, uint32_t, const String&)> callback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        callback(IDBKeyData(), INVALID_STATE_ERR, "Unable to put record into database because it has shut down");
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<IDBKeyData, uint32_t, String>::create([this, callback](const IDBKeyData& keyData, uint32_t errorCode, const String& errorMessage) {
        callback(keyData, errorCode, errorMessage);
    }, [this, callback] {
        callback(IDBKeyData(), INVALID_STATE_ERR, "Unable to put record into database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::putRecordInBackingStore, requestID, transactionIdentifier, m_metadata->objectStores.get(objectStoreID), keyData, value.vector(), putMode, indexIDs, indexKeys));
}

void LegacyUniqueIDBDatabase::getRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyRangeData& keyRangeData, IndexedDB::CursorType cursorType, std::function<void (const IDBGetResult&, uint32_t, const String&)> callback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        callback(IDBGetResult(), INVALID_STATE_ERR, "Unable to get record from database because it has shut down");
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<IDBGetResult, uint32_t, String>::create([this, callback](const IDBGetResult& result, uint32_t errorCode, const String& errorMessage) {
        callback(result, errorCode, errorMessage);
    }, [this, callback] {
        callback(IDBGetResult(), INVALID_STATE_ERR, "Unable to get record from database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::getRecordFromBackingStore, requestID, transactionIdentifier, m_metadata->objectStores.get(objectStoreID), indexID, keyRangeData, cursorType));
}

void LegacyUniqueIDBDatabase::openCursor(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, const IDBKeyRangeData& keyRangeData, std::function<void (int64_t, const IDBKeyData&, const IDBKeyData&, PassRefPtr<SharedBuffer>, uint32_t, const String&)> callback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        callback(0, nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to open cursor in database because it has shut down");
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<int64_t, IDBKeyData, IDBKeyData, PassRefPtr<SharedBuffer>, uint32_t, String>::create([this, callback](int64_t cursorID, const IDBKeyData& key, const IDBKeyData& primaryKey, PassRefPtr<SharedBuffer> value, uint32_t errorCode, const String& errorMessage) {
        callback(cursorID, key, primaryKey, value, errorCode, errorMessage);
    }, [this, callback] {
        callback(0, nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to get record from database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::openCursorInBackingStore, requestID, transactionIdentifier, objectStoreID, indexID, cursorDirection, cursorType, taskType, keyRangeData));
}

void LegacyUniqueIDBDatabase::cursorAdvance(const IDBIdentifier& cursorIdentifier, uint64_t count, std::function<void (const IDBKeyData&, const IDBKeyData&, PassRefPtr<SharedBuffer>, uint32_t, const String&)> callback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        callback(nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to advance cursor in database because it has shut down");
        return;
    }

    RefPtr<AsyncRequest> request = AsyncRequestImpl<IDBKeyData, IDBKeyData, PassRefPtr<SharedBuffer>, uint32_t, String>::create([this, callback](const IDBKeyData& key, const IDBKeyData& primaryKey, PassRefPtr<SharedBuffer> value, uint32_t errorCode, const String& errorMessage) {
        callback(key, primaryKey, value, errorCode, errorMessage);
    }, [this, callback] {
        callback(nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to advance cursor in database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::advanceCursorInBackingStore, requestID, cursorIdentifier, count));
}

void LegacyUniqueIDBDatabase::cursorIterate(const IDBIdentifier& cursorIdentifier, const IDBKeyData& key, std::function<void (const IDBKeyData&, const IDBKeyData&, PassRefPtr<SharedBuffer>, uint32_t, const String&)> callback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        callback(nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to iterate cursor in database because it has shut down");
        return;
    }

    RefPtr<AsyncRequest> request = AsyncRequestImpl<IDBKeyData, IDBKeyData, PassRefPtr<SharedBuffer>, uint32_t, String>::create([this, callback](const IDBKeyData& key, const IDBKeyData& primaryKey, PassRefPtr<SharedBuffer> value, uint32_t errorCode, const String& errorMessage) {
        callback(key, primaryKey, value, errorCode, errorMessage);
    }, [this, callback] {
        callback(nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to iterate cursor in database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::iterateCursorInBackingStore, requestID, cursorIdentifier, key));
}

void LegacyUniqueIDBDatabase::count(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyRangeData& keyRangeData, std::function<void (int64_t, uint32_t, const String&)> callback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        callback(0, INVALID_STATE_ERR, "Unable to get count from database because it has shut down");
        return;
    }

    RefPtr<AsyncRequest> request = AsyncRequestImpl<int64_t, uint32_t, String>::create([this, callback](int64_t count, uint32_t errorCode, const String& errorMessage) {
        callback(count, errorCode, errorMessage);
    }, [this, callback] {
        callback(0, INVALID_STATE_ERR, "Unable to get count from database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::countInBackingStore, requestID, transactionIdentifier, objectStoreID, indexID, keyRangeData));
}

void LegacyUniqueIDBDatabase::deleteRange(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyRangeData& keyRangeData, std::function<void (uint32_t, const String&)> callback)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests) {
        callback(INVALID_STATE_ERR, "Unable to deleteRange from database because it has shut down");
        return;
    }

    RefPtr<AsyncRequest> request = AsyncRequestImpl<uint32_t, String>::create([callback](uint32_t errorCode, const String& errorMessage) {
        callback(errorCode, errorMessage);
    }, [callback] {
        callback(INVALID_STATE_ERR, "Unable to deleteRange from database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::deleteRangeInBackingStore, requestID, transactionIdentifier, objectStoreID, keyRangeData));
}

void LegacyUniqueIDBDatabase::openBackingStoreTransaction(const IDBIdentifier& transactionIdentifier, const Vector<int64_t>& objectStoreIDs, IndexedDB::TransactionMode mode)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->establishTransaction(transactionIdentifier, objectStoreIDs, mode);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didEstablishTransaction, transactionIdentifier, success));
    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void LegacyUniqueIDBDatabase::beginBackingStoreTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->beginTransaction(transactionIdentifier);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void LegacyUniqueIDBDatabase::commitBackingStoreTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->commitTransaction(transactionIdentifier);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void LegacyUniqueIDBDatabase::resetBackingStoreTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->resetTransaction(transactionIdentifier);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didResetTransaction, transactionIdentifier, success));
    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void LegacyUniqueIDBDatabase::rollbackBackingStoreTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->rollbackTransaction(transactionIdentifier);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void LegacyUniqueIDBDatabase::changeDatabaseVersionInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, uint64_t newVersion)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->changeDatabaseVersion(transactionIdentifier, newVersion);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didChangeDatabaseVersion, requestID, success));
}

void LegacyUniqueIDBDatabase::createObjectStoreInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, const IDBObjectStoreMetadata& metadata)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->createObjectStore(transactionIdentifier, metadata);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didCreateObjectStore, requestID, success));
}

void LegacyUniqueIDBDatabase::deleteObjectStoreInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->deleteObjectStore(transactionIdentifier, objectStoreID);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didDeleteObjectStore, requestID, success));
}

void LegacyUniqueIDBDatabase::clearObjectStoreInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->clearObjectStore(transactionIdentifier, objectStoreID);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didClearObjectStore, requestID, success));
}

void LegacyUniqueIDBDatabase::createIndexInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBIndexMetadata& metadata)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->createIndex(transactionIdentifier, objectStoreID, metadata);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didCreateIndex, requestID, success));
}

void LegacyUniqueIDBDatabase::deleteIndexInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool success = m_backingStore->deleteIndex(transactionIdentifier, objectStoreID, indexID);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didDeleteIndex, requestID, success));
}

void LegacyUniqueIDBDatabase::putRecordInBackingStore(uint64_t requestID, const IDBIdentifier& transaction, const IDBObjectStoreMetadata& objectStoreMetadata, const IDBKeyData& inputKeyData, const Vector<uint8_t>& value, int64_t putMode, const Vector<int64_t>& indexIDs, const Vector<Vector<IDBKeyData>>& indexKeys)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    bool keyWasGenerated = false;
    IDBKeyData key;
    int64_t keyNumber = 0;

    if (putMode != IDBDatabaseBackend::CursorUpdate && objectStoreMetadata.autoIncrement && inputKeyData.isNull()) {
        if (!m_backingStore->generateKeyNumber(transaction, objectStoreMetadata.id, keyNumber)) {
            postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error checking for key existence")));
            return;
        }
        key.setNumberValue(keyNumber);
        keyWasGenerated = true;
    } else
        key = inputKeyData;

    if (putMode == IDBDatabaseBackend::AddOnly) {
        bool keyExists;
        if (!m_backingStore->keyExistsInObjectStore(transaction, objectStoreMetadata.id, key, keyExists)) {
            postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error checking for key existence")));
            return;
        }
        if (keyExists) {
            postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::ConstraintError, ASCIILiteral("Key already exists in the object store")));
            return;
        }
    }

    // The spec says that even if we're about to overwrite the record, perform the steps to delete it first.
    // This is important because formally deleting it from from the object store also removes it from the appropriate indexes.
    if (!m_backingStore->deleteRecord(transaction, objectStoreMetadata.id, key)) {
        postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Replacing an existing key in backing store, unable to delete previous record.")));
        return;
    }

    if (!m_backingStore->putRecord(transaction, objectStoreMetadata.id, key, value.data(), value.size())) {
        postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error putting a record")));
        return;
    }

    ASSERT(indexIDs.size() == indexKeys.size());
    for (size_t i = 0; i < indexIDs.size(); ++i) {
        for (size_t j = 0; j < indexKeys[i].size(); ++j) {
            if (!m_backingStore->putIndexRecord(transaction, objectStoreMetadata.id, indexIDs[i], key, indexKeys[i][j])) {
                postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error writing index key")));
                return;
            }
        }
    }

    m_backingStore->notifyCursorsOfChanges(transaction, objectStoreMetadata.id);

    if (putMode != IDBDatabaseBackend::CursorUpdate && objectStoreMetadata.autoIncrement && key.type() == KeyType::Number) {
        if (!m_backingStore->updateKeyGeneratorNumber(transaction, objectStoreMetadata.id, keyNumber, keyWasGenerated)) {
            postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error updating key generator")));
            return;
        }
    }

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didPutRecordInBackingStore, requestID, key, 0, String(StringImpl::empty())));
}

void LegacyUniqueIDBDatabase::didPutRecordInBackingStore(uint64_t requestID, const IDBKeyData& keyData, uint32_t errorCode, const String& errorMessage)
{
    m_pendingDatabaseTasks.take(requestID).get().completeRequest(keyData, errorCode, errorMessage);
}

void LegacyUniqueIDBDatabase::getRecordFromBackingStore(uint64_t requestID, const IDBIdentifier& transaction, const IDBObjectStoreMetadata& objectStoreMetadata, int64_t indexID, const IDBKeyRangeData& keyRangeData, IndexedDB::CursorType cursorType)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    RefPtr<IDBKeyRange> keyRange = keyRangeData.maybeCreateIDBKeyRange();
    ASSERT(keyRange);
    if (!keyRange) {
        postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("Invalid IDBKeyRange requested from backing store")));
        return;
    }

    if (indexID == IDBIndexMetadata::InvalidId) {
        // IDBObjectStore get record
        RefPtr<SharedBuffer> result;

        if (keyRange->isOnlyKey()) {
            if (!m_backingStore->getKeyRecordFromObjectStore(transaction, objectStoreMetadata.id, *keyRange->lower(), result))
                postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get key record from object store in backing store")));
            else {
                IDBGetResult getResult = result ? IDBGetResult(result.release(), keyRange->lower(), objectStoreMetadata.keyPath) : IDBGetResult();
                postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didGetRecordFromBackingStore, requestID, getResult, 0, String(StringImpl::empty())));
            }

            return;
        }

        RefPtr<IDBKey> resultKey;

        if (!m_backingStore->getKeyRangeRecordFromObjectStore(transaction, objectStoreMetadata.id, *keyRange, result, resultKey))
            postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get key range record from object store in backing store")));
        else {
            IDBGetResult getResult = result ? IDBGetResult(result.release(), resultKey.release(), objectStoreMetadata.keyPath) : IDBGetResult();
            postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didGetRecordFromBackingStore, requestID, getResult, 0, String(StringImpl::empty())));
        }

        return;
    }

    // IDBIndex get record

    IDBGetResult result;
    if (!m_backingStore->getIndexRecord(transaction, objectStoreMetadata.id, indexID, keyRangeData, cursorType, result)) {
        postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get index record from backing store")));
        return;
    }

    // We must return a key path to know how to inject the result key into the result value object.
    result.setKeyPath(objectStoreMetadata.keyPath);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didGetRecordFromBackingStore, requestID, result, 0, String(StringImpl::empty())));
}

void LegacyUniqueIDBDatabase::didGetRecordFromBackingStore(uint64_t requestID, const IDBGetResult& result, uint32_t errorCode, const String& errorMessage)
{
    m_pendingDatabaseTasks.take(requestID).get().completeRequest(result, errorCode, errorMessage);
}

void LegacyUniqueIDBDatabase::openCursorInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, const IDBKeyRangeData& keyRange)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(m_backingStore);

    int64_t cursorID = 0;
    IDBKeyData key;
    IDBKeyData primaryKey;
    Vector<uint8_t> valueBuffer;
    int32_t errorCode = 0;
    String errorMessage;
    bool success = m_backingStore->openCursor(transactionIdentifier, objectStoreID, indexID, cursorDirection, cursorType, taskType, keyRange, cursorID, key, primaryKey, valueBuffer);

    if (!success) {
        errorCode = IDBDatabaseException::UnknownError;
        errorMessage = ASCIILiteral("Unknown error opening cursor in backing store");
    }

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didOpenCursorInBackingStore, requestID, cursorID, key, primaryKey, valueBuffer, errorCode, errorMessage));
}

void LegacyUniqueIDBDatabase::didOpenCursorInBackingStore(uint64_t requestID, int64_t cursorID, const IDBKeyData& key, const IDBKeyData& primaryKey, const Vector<uint8_t>& valueBuffer, uint32_t errorCode, const String& errorMessage)
{
    m_pendingDatabaseTasks.take(requestID).get().completeRequest(cursorID, key, primaryKey, SharedBuffer::create(valueBuffer.data(), valueBuffer.size()), errorCode, errorMessage);
}

void LegacyUniqueIDBDatabase::advanceCursorInBackingStore(uint64_t requestID, const IDBIdentifier& cursorIdentifier, uint64_t count)
{
    IDBKeyData key;
    IDBKeyData primaryKey;
    Vector<uint8_t> valueBuffer;
    int32_t errorCode = 0;
    String errorMessage;
    bool success = m_backingStore->advanceCursor(cursorIdentifier, count, key, primaryKey, valueBuffer);

    if (!success) {
        errorCode = IDBDatabaseException::UnknownError;
        errorMessage = ASCIILiteral("Unknown error advancing cursor in backing store");
    }

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didAdvanceCursorInBackingStore, requestID, key, primaryKey, valueBuffer, errorCode, errorMessage));
}

void LegacyUniqueIDBDatabase::didAdvanceCursorInBackingStore(uint64_t requestID, const IDBKeyData& key, const IDBKeyData& primaryKey, const Vector<uint8_t>& valueBuffer, uint32_t errorCode, const String& errorMessage)
{
    m_pendingDatabaseTasks.take(requestID).get().completeRequest(key, primaryKey, SharedBuffer::create(valueBuffer.data(), valueBuffer.size()), errorCode, errorMessage);
}

void LegacyUniqueIDBDatabase::iterateCursorInBackingStore(uint64_t requestID, const IDBIdentifier& cursorIdentifier, const IDBKeyData& iterateKey)
{
    IDBKeyData key;
    IDBKeyData primaryKey;
    Vector<uint8_t> valueBuffer;
    int32_t errorCode = 0;
    String errorMessage;
    bool success = m_backingStore->iterateCursor(cursorIdentifier, iterateKey, key, primaryKey, valueBuffer);

    if (!success) {
        errorCode = IDBDatabaseException::UnknownError;
        errorMessage = ASCIILiteral("Unknown error iterating cursor in backing store");
    }

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didIterateCursorInBackingStore, requestID, key, primaryKey, valueBuffer, errorCode, errorMessage));
}

void LegacyUniqueIDBDatabase::didIterateCursorInBackingStore(uint64_t requestID, const IDBKeyData& key, const IDBKeyData& primaryKey, const Vector<uint8_t>& valueBuffer, uint32_t errorCode, const String& errorMessage)
{
    m_pendingDatabaseTasks.take(requestID).get().completeRequest(key, primaryKey, SharedBuffer::create(valueBuffer.data(), valueBuffer.size()), errorCode, errorMessage);
}

void LegacyUniqueIDBDatabase::countInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyRangeData& keyRangeData)
{
    int64_t count;

    if (!m_backingStore->count(transactionIdentifier, objectStoreID, indexID, keyRangeData, count)) {
        LOG_ERROR("Failed to get count from backing store.");
        postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didCountInBackingStore, requestID, 0, IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get count from backing store")));

        return;
    }

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didCountInBackingStore, requestID, count, 0, String(StringImpl::empty())));
}

void LegacyUniqueIDBDatabase::didCountInBackingStore(uint64_t requestID, int64_t count, uint32_t errorCode, const String& errorMessage)
{
    m_pendingDatabaseTasks.take(requestID).get().completeRequest(count, errorCode, errorMessage);
}

void LegacyUniqueIDBDatabase::deleteRangeInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyRangeData& keyRangeData)
{
    if (!m_backingStore->deleteRange(transactionIdentifier, objectStoreID, keyRangeData)) {
        LOG_ERROR("Failed to delete range from backing store.");
        postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didDeleteRangeInBackingStore, requestID, IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get count from backing store")));

        return;
    }

    m_backingStore->notifyCursorsOfChanges(transactionIdentifier, objectStoreID);

    postMainThreadTask(createCrossThreadTask(*this, &LegacyUniqueIDBDatabase::didDeleteRangeInBackingStore, requestID, 0, String(StringImpl::empty())));
}

void LegacyUniqueIDBDatabase::didDeleteRangeInBackingStore(uint64_t requestID, uint32_t errorCode, const String& errorMessage)
{
    m_pendingDatabaseTasks.take(requestID).get().completeRequest(errorCode, errorMessage);
}

void LegacyUniqueIDBDatabase::didEstablishTransaction(const IDBIdentifier& transactionIdentifier, bool success)
{
    ASSERT(RunLoop::isMain());
    if (!success)
        return;

    auto transactions = m_establishedTransactions.add(&transactionIdentifier.connection(), HashSet<IDBIdentifier>());
    transactions.iterator->value.add(transactionIdentifier);
}

void LegacyUniqueIDBDatabase::didResetTransaction(const IDBIdentifier& transactionIdentifier, bool success)
{
    ASSERT(RunLoop::isMain());
    if (!success)
        return;

    auto transactions = m_establishedTransactions.find(&transactionIdentifier.connection());
    if (transactions != m_establishedTransactions.end())
        transactions.get()->value.remove(transactionIdentifier);
}

void LegacyUniqueIDBDatabase::resetAllTransactions(const DatabaseProcessIDBConnection& connection)
{
    ASSERT(RunLoop::isMain());
    auto transactions = m_establishedTransactions.find(&connection);
    if (transactions == m_establishedTransactions.end() || !m_acceptingNewRequests)
        return;

    for (auto& transactionIdentifier : transactions.get()->value) {
        m_pendingTransactionRollbacks.add(transactionIdentifier);
        if (!m_pendingTransactionRequests.contains(transactionIdentifier))
            finalizeRollback(transactionIdentifier);
    }
}

void LegacyUniqueIDBDatabase::finalizeRollback(const WebKit::IDBIdentifier& transactionId)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_pendingTransactionRollbacks.contains(transactionId));
    ASSERT(!m_pendingTransactionRequests.contains(transactionId));
    rollbackTransaction(transactionId, [this, transactionId](bool) {
        ASSERT(RunLoop::isMain());
        if (m_pendingTransactionRequests.contains(transactionId))
            return;

        ASSERT(m_pendingTransactionRollbacks.contains(transactionId));
        m_pendingTransactionRollbacks.remove(transactionId);
        resetTransaction(transactionId, [this, transactionId](bool) {
            if (m_acceptingNewRequests && m_connections.isEmpty() && m_pendingTransactionRollbacks.isEmpty()) {
                shutdown(LegacyUniqueIDBDatabaseShutdownType::NormalShutdown);
                DatabaseProcess::singleton().removeLegacyUniqueIDBDatabase(*this);
            }
        });
    });
}

String LegacyUniqueIDBDatabase::absoluteDatabaseDirectory() const
{
    ASSERT(RunLoop::isMain());
    return DatabaseProcess::singleton().absoluteIndexedDatabasePathFromDatabaseRelativePath(m_databaseRelativeDirectory);
}

void LegacyUniqueIDBDatabase::postMainThreadTask(std::unique_ptr<CrossThreadTask> task, DatabaseTaskType taskType)
{
    ASSERT(!RunLoop::isMain());

    if (!m_acceptingNewRequests && taskType == DatabaseTaskType::Normal)
        return;

    LockHolder locker(m_mainThreadTaskMutex);

    m_mainThreadTasks.append(WTFMove(task));

    RefPtr<LegacyUniqueIDBDatabase> database(this);
    RunLoop::main().dispatch([database] {
        database->performNextMainThreadTask();
    });
}

bool LegacyUniqueIDBDatabase::performNextMainThreadTask()
{
    ASSERT(RunLoop::isMain());

    bool moreTasks;

    std::unique_ptr<CrossThreadTask> task;
    {
        LockHolder locker(m_mainThreadTaskMutex);

        // This database might be shutting down, in which case the task queue might be empty.
        if (m_mainThreadTasks.isEmpty())
            return false;

        task = m_mainThreadTasks.takeFirst();
        moreTasks = !m_mainThreadTasks.isEmpty();
    }

    task->performTask();

    return moreTasks;
}

void LegacyUniqueIDBDatabase::postDatabaseTask(std::unique_ptr<CrossThreadTask> task, DatabaseTaskType taskType)
{
    ASSERT(RunLoop::isMain());

    if (!m_acceptingNewRequests && taskType == DatabaseTaskType::Normal)
        return;

    LockHolder locker(m_databaseTaskMutex);

    m_databaseTasks.append(WTFMove(task));

    RefPtr<LegacyUniqueIDBDatabase> database(this);
    DatabaseProcess::singleton().queue().dispatch([database] {
        database->performNextDatabaseTask();
    });
}

void LegacyUniqueIDBDatabase::performNextDatabaseTask()
{
    ASSERT(!RunLoop::isMain());

    // It is possible that this database might be shutting down on the main thread.
    // In this case, immediately after releasing m_databaseTaskMutex, this database might get deleted.
    // We take a ref() to make sure the database is still live while this last task is performed.
    RefPtr<LegacyUniqueIDBDatabase> protector(this);

    std::unique_ptr<CrossThreadTask> task;
    {
        LockHolder locker(m_databaseTaskMutex);

        // This database might be shutting down on the main thread, in which case the task queue might be empty.
        if (m_databaseTasks.isEmpty())
            return;

        task = m_databaseTasks.takeFirst();
    }

    task->performTask();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
