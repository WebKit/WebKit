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
#include "UniqueIDBDatabase.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "AsyncRequest.h"
#include "AsyncTask.h"
#include "DataReference.h"
#include "DatabaseProcess.h"
#include "DatabaseProcessIDBConnection.h"
#include "Logging.h"
#include "UniqueIDBDatabaseBackingStoreSQLite.h"
#include "WebCrossThreadCopier.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IDBDatabaseMetadata.h>
#include <WebCore/IDBGetResult.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBKeyRangeData.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

String UniqueIDBDatabase::calculateAbsoluteDatabaseFilename(const String& absoluteDatabaseDirectory)
{
    return pathByAppendingComponent(absoluteDatabaseDirectory, "IndexedDB.sqlite3");
}

UniqueIDBDatabase::UniqueIDBDatabase(const UniqueIDBDatabaseIdentifier& identifier)
    : m_identifier(identifier)
    , m_acceptingNewRequests(true)
    , m_didGetMetadataFromBackingStore(false)
{
    m_inMemory = !canShareDatabases(identifier.openingOrigin(), identifier.mainFrameOrigin());
    if (m_inMemory)
        return;

    // Each unique Indexed Database exists in a directory named for the database, which exists in a directory representing its opening origin.
    m_databaseRelativeDirectory = pathByAppendingComponent(databaseFilenameIdentifier(identifier.openingOrigin()), filenameForDatabaseName());

    DatabaseProcess::shared().ensureIndexedDatabaseRelativePathExists(m_databaseRelativeDirectory);
}

UniqueIDBDatabase::~UniqueIDBDatabase()
{
    ASSERT(!m_acceptingNewRequests);
    ASSERT(m_pendingMetadataRequests.isEmpty());
}

String UniqueIDBDatabase::filenameForDatabaseName() const
{
    ASSERT(!m_identifier.databaseName().isNull());

    if (m_identifier.databaseName().isEmpty())
        return "%00";

    String filename = encodeForFileName(m_identifier.databaseName());
    filename.replace('.', "%2E");

    return filename;
}

String UniqueIDBDatabase::databaseFilenameIdentifier(const SecurityOriginData& originData) const
{
    RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::create(originData.protocol, originData.host, originData.port);
    return securityOrigin->databaseIdentifier();
}

bool UniqueIDBDatabase::canShareDatabases(const SecurityOriginData& openingOrigin, const SecurityOriginData& mainFrameOrigin) const
{
    // For now, an origin's database access is predicated on equality with the other origin.
    // We might need to make this more nuanced later.
    return openingOrigin == mainFrameOrigin;
}

void UniqueIDBDatabase::registerConnection(DatabaseProcessIDBConnection& connection)
{
    ASSERT(!m_connections.contains(&connection));
    m_connections.add(&connection);
}

void UniqueIDBDatabase::unregisterConnection(DatabaseProcessIDBConnection& connection)
{
    ASSERT(m_connections.contains(&connection));
    m_connections.remove(&connection);

    if (m_connections.isEmpty()) {
        shutdown(UniqueIDBDatabaseShutdownType::NormalShutdown);
        DatabaseProcess::shared().removeUniqueIDBDatabase(*this);
    }
}

void UniqueIDBDatabase::shutdown(UniqueIDBDatabaseShutdownType type)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests)
        return;

    m_acceptingNewRequests = false;

    // Balanced by an adoptRef in ::didShutdownBackingStore()
    ref();

    {
        MutexLocker locker(m_databaseTaskMutex);
        m_databaseTasks.clear();
    }

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::shutdownBackingStore, type, absoluteDatabaseDirectory()), DatabaseTaskType::Shutdown);
}

void UniqueIDBDatabase::shutdownBackingStore(UniqueIDBDatabaseShutdownType type, const String& databaseDirectory)
{
    ASSERT(!isMainThread());

    m_backingStore.clear();

    if (type == UniqueIDBDatabaseShutdownType::DeleteShutdown) {
        String dbFilename = UniqueIDBDatabase::calculateAbsoluteDatabaseFilename(databaseDirectory);
        LOG(IDB, "UniqueIDBDatabase::shutdownBackingStore deleting file '%s' on disk", dbFilename.utf8().data());
        deleteFile(dbFilename);
        deleteEmptyDirectory(databaseDirectory);
    }

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didShutdownBackingStore, type), DatabaseTaskType::Shutdown);
}

void UniqueIDBDatabase::didShutdownBackingStore(UniqueIDBDatabaseShutdownType type)
{
    ASSERT(isMainThread());

    // Balanced by a ref in ::shutdown()
    RefPtr<UniqueIDBDatabase> protector(adoptRef(this));

    // Empty out remaining main thread tasks.
    while (performNextMainThreadTaskWithoutAdoptRef()) {
    }

    // No more requests will be handled, so abort all outstanding requests.
    for (const auto& it : m_pendingMetadataRequests)
        it->requestAborted();
    for (const auto& it : m_pendingTransactionRequests)
        it.value->requestAborted();
    for (const auto& it : m_pendingDatabaseTasks)
        it.value->requestAborted();

    m_pendingMetadataRequests.clear();
    m_pendingTransactionRequests.clear();
    m_pendingDatabaseTasks.clear();

    if (m_pendingShutdownTask)
        m_pendingShutdownTask->completeRequest(type);

    m_pendingShutdownTask = nullptr;
}

void UniqueIDBDatabase::deleteDatabase(std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        // Someone else has already shutdown this database, so we can't request a delete.
        callOnMainThread([successCallback]() {
            successCallback(false);
        });
        return;
    }

    RefPtr<UniqueIDBDatabase> protector(this);
    m_pendingShutdownTask = AsyncRequestImpl<UniqueIDBDatabaseShutdownType>::create([this, protector, successCallback](UniqueIDBDatabaseShutdownType type) {
        // If the shutdown just completed was a Delete shutdown then we succeeded.
        // If not report failure instead of trying again.
        successCallback(type == UniqueIDBDatabaseShutdownType::DeleteShutdown);
    }, [this, protector, successCallback]() {
        successCallback(false);
    });

    shutdown(UniqueIDBDatabaseShutdownType::DeleteShutdown);
}

void UniqueIDBDatabase::getOrEstablishIDBDatabaseMetadata(std::function<void(bool, const IDBDatabaseMetadata&)> completionCallback)
{
    ASSERT(isMainThread());

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
    RefPtr<AsyncRequest> request = AsyncRequestImpl<>::create([completionCallback, this]() {
        completionCallback(!!m_metadata, m_metadata ? *m_metadata : IDBDatabaseMetadata());
    }, [completionCallback]() {
        // The boolean flag to the completion callback represents whether the
        // attempt to get/establish metadata succeeded or failed.
        // Since we're aborting the attempt, it failed, so we always pass in false.
        completionCallback(false, IDBDatabaseMetadata());
    });

    m_pendingMetadataRequests.append(request.release());

    if (shouldOpenBackingStore)
        postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::openBackingStoreAndReadMetadata, m_identifier, absoluteDatabaseDirectory()));
}

void UniqueIDBDatabase::openBackingStoreAndReadMetadata(const UniqueIDBDatabaseIdentifier& identifier, const String& databaseDirectory)
{
    ASSERT(!isMainThread());
    ASSERT(!m_backingStore);

    if (m_inMemory) {
        LOG_ERROR("Support for in-memory databases not yet implemented");
        return;
    }

    m_backingStore = UniqueIDBDatabaseBackingStoreSQLite::create(identifier, databaseDirectory);
    std::unique_ptr<IDBDatabaseMetadata> metadata = m_backingStore->getOrEstablishMetadata();

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didOpenBackingStoreAndReadMetadata, metadata ? *metadata : IDBDatabaseMetadata(), !!metadata));
}

void UniqueIDBDatabase::didOpenBackingStoreAndReadMetadata(const IDBDatabaseMetadata& metadata, bool success)
{
    ASSERT(isMainThread());
    ASSERT(!m_metadata);

    m_didGetMetadataFromBackingStore = true;

    if (success)
        m_metadata = std::make_unique<IDBDatabaseMetadata>(metadata);

    while (!m_pendingMetadataRequests.isEmpty()) {
        RefPtr<AsyncRequest> request = m_pendingMetadataRequests.takeFirst();
        request->completeRequest();
    }
}

void UniqueIDBDatabase::openTransaction(const IDBIdentifier& transactionIdentifier, const Vector<int64_t>& objectStoreIDs, IndexedDB::TransactionMode mode, std::function<void(bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createAsyncTask(*this, &UniqueIDBDatabase::openBackingStoreTransaction, transactionIdentifier, objectStoreIDs, mode), successCallback);
}

void UniqueIDBDatabase::beginTransaction(const IDBIdentifier& transactionIdentifier, std::function<void(bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createAsyncTask(*this, &UniqueIDBDatabase::beginBackingStoreTransaction, transactionIdentifier), successCallback);
}

void UniqueIDBDatabase::commitTransaction(const IDBIdentifier& transactionIdentifier, std::function<void(bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createAsyncTask(*this, &UniqueIDBDatabase::commitBackingStoreTransaction, transactionIdentifier), successCallback);
}

void UniqueIDBDatabase::resetTransaction(const IDBIdentifier& transactionIdentifier, std::function<void(bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createAsyncTask(*this, &UniqueIDBDatabase::resetBackingStoreTransaction, transactionIdentifier), successCallback);
}

void UniqueIDBDatabase::rollbackTransaction(const IDBIdentifier& transactionIdentifier, std::function<void(bool)> successCallback)
{
    postTransactionOperation(transactionIdentifier, createAsyncTask(*this, &UniqueIDBDatabase::rollbackBackingStoreTransaction, transactionIdentifier), successCallback);
}

void UniqueIDBDatabase::postTransactionOperation(const IDBIdentifier& transactionIdentifier, std::unique_ptr<AsyncTask> task, std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    if (m_pendingTransactionRequests.contains(transactionIdentifier)) {
        LOG_ERROR("Attempting to queue an operation for a transaction that already has an operation pending. Each transaction should only have one operation pending at a time.");
        successCallback(false);
        return;
    }

    postDatabaseTask(std::move(task));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([successCallback](bool success) {
        successCallback(success);
    }, [successCallback]() {
        successCallback(false);
    });

    m_pendingTransactionRequests.add(transactionIdentifier, request.release());
}

void UniqueIDBDatabase::didCompleteTransactionOperation(const IDBIdentifier& transactionIdentifier, bool success)
{
    ASSERT(isMainThread());

    RefPtr<AsyncRequest> request = m_pendingTransactionRequests.take(transactionIdentifier);
    if (!request)
        return;

    request->completeRequest(success);
}

void UniqueIDBDatabase::changeDatabaseVersion(const IDBIdentifier& transactionIdentifier, uint64_t newVersion, std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

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
    }, [this, oldVersion, successCallback]() {
        m_metadata->version = oldVersion;
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::changeDatabaseVersionInBackingStore, requestID, transactionIdentifier, newVersion));
}

void UniqueIDBDatabase::didChangeDatabaseVersion(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void UniqueIDBDatabase::didCreateObjectStore(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void UniqueIDBDatabase::didDeleteObjectStore(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void UniqueIDBDatabase::didClearObjectStore(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void UniqueIDBDatabase::didCreateIndex(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void UniqueIDBDatabase::didDeleteIndex(uint64_t requestID, bool success)
{
    didCompleteBoolRequest(requestID, success);
}

void UniqueIDBDatabase::didCompleteBoolRequest(uint64_t requestID, bool success)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(success);
}

void UniqueIDBDatabase::createObjectStore(const IDBIdentifier& transactionIdentifier, const IDBObjectStoreMetadata& metadata, std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

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
    }, [this, addedObjectStoreID, successCallback]() {
        m_metadata->objectStores.remove(addedObjectStoreID);
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::createObjectStoreInBackingStore, requestID, transactionIdentifier, metadata));
}

void UniqueIDBDatabase::deleteObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

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
    }, [this, metadata, successCallback]() {
        m_metadata->objectStores.set(metadata.id, metadata);
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::deleteObjectStoreInBackingStore, requestID, transactionIdentifier, objectStoreID));
}

void UniqueIDBDatabase::clearObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([this, successCallback](bool success) {
        successCallback(success);
    }, [this, successCallback]() {
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::clearObjectStoreInBackingStore, requestID, transactionIdentifier, objectStoreID));
}

void UniqueIDBDatabase::createIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBIndexMetadata& metadata, std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

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
    }, [this, objectStoreID, addedIndexID, successCallback]() {
        auto objectStoreFind = m_metadata->objectStores.find(objectStoreID);
        if (objectStoreFind != m_metadata->objectStores.end())
            objectStoreFind->value.indexes.remove(addedIndexID);
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::createIndexInBackingStore, requestID, transactionIdentifier, objectStoreID, metadata));
}

void UniqueIDBDatabase::deleteIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

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
    }, [this, objectStoreID, metadata, successCallback]() {
        auto objectStoreFind = m_metadata->objectStores.find(objectStoreID);
        if (objectStoreFind != m_metadata->objectStores.end())
            objectStoreFind->value.indexes.set(metadata.id, metadata);
        successCallback(false);
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::deleteIndexInBackingStore, requestID, transactionIdentifier, objectStoreID, indexID));
}

void UniqueIDBDatabase::putRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyData& keyData, const IPC::DataReference& value, int64_t putMode, const Vector<int64_t>& indexIDs, const Vector<Vector<IDBKeyData>>& indexKeys, std::function<void(const IDBKeyData&, uint32_t, const String&)> callback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        callback(IDBKeyData(), INVALID_STATE_ERR, "Unable to put record into database because it has shut down");
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<const IDBKeyData&, uint32_t, const String&>::create([this, callback](const IDBKeyData& keyData, uint32_t errorCode, const String& errorMessage) {
        callback(keyData, errorCode, errorMessage);
    }, [this, callback]() {
        callback(IDBKeyData(), INVALID_STATE_ERR, "Unable to put record into database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::putRecordInBackingStore, requestID, transactionIdentifier, m_metadata->objectStores.get(objectStoreID), keyData, value.vector(), putMode, indexIDs, indexKeys));
}

void UniqueIDBDatabase::getRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyRangeData& keyRangeData, IndexedDB::CursorType cursorType, std::function<void(const IDBGetResult&, uint32_t, const String&)> callback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        callback(IDBGetResult(), INVALID_STATE_ERR, "Unable to get record from database because it has shut down");
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<const IDBGetResult&, uint32_t, const String&>::create([this, callback](const IDBGetResult& result, uint32_t errorCode, const String& errorMessage) {
        callback(result, errorCode, errorMessage);
    }, [this, callback]() {
        callback(IDBGetResult(), INVALID_STATE_ERR, "Unable to get record from database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::getRecordFromBackingStore, requestID, transactionIdentifier, m_metadata->objectStores.get(objectStoreID), indexID, keyRangeData, cursorType));
}

void UniqueIDBDatabase::openCursor(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, const IDBKeyRangeData& keyRangeData, std::function<void(int64_t, const IDBKeyData&, const IDBKeyData&, PassRefPtr<SharedBuffer>, uint32_t, const String&)> callback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        callback(0, nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to open cursor in database because it has shut down");
        return;
    }

    ASSERT(m_metadata->objectStores.contains(objectStoreID));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<int64_t, const IDBKeyData&, const IDBKeyData&, PassRefPtr<SharedBuffer>, uint32_t, const String&>::create([this, callback](int64_t cursorID, const IDBKeyData& key, const IDBKeyData& primaryKey, PassRefPtr<SharedBuffer> value, uint32_t errorCode, const String& errorMessage) {
        callback(cursorID, key, primaryKey, value, errorCode, errorMessage);
    }, [this, callback]() {
        callback(0, nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to get record from database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::openCursorInBackingStore, requestID, transactionIdentifier, objectStoreID, indexID, cursorDirection, cursorType, taskType, keyRangeData));
}

void UniqueIDBDatabase::cursorAdvance(const IDBIdentifier& cursorIdentifier, uint64_t count, std::function<void(const IDBKeyData&, const IDBKeyData&, PassRefPtr<SharedBuffer>, uint32_t, const String&)> callback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        callback(nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to advance cursor in database because it has shut down");
        return;
    }

    RefPtr<AsyncRequest> request = AsyncRequestImpl<const IDBKeyData&, const IDBKeyData&, PassRefPtr<SharedBuffer>, uint32_t, const String&>::create([this, callback](const IDBKeyData& key, const IDBKeyData& primaryKey, PassRefPtr<SharedBuffer> value, uint32_t errorCode, const String& errorMessage) {
        callback(key, primaryKey, value, errorCode, errorMessage);
    }, [this, callback]() {
        callback(nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to advance cursor in database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::advanceCursorInBackingStore, requestID, cursorIdentifier, count));
}

void UniqueIDBDatabase::cursorIterate(const IDBIdentifier& cursorIdentifier, const IDBKeyData& key, std::function<void(const IDBKeyData&, const IDBKeyData&, PassRefPtr<SharedBuffer>, uint32_t, const String&)> callback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        callback(nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to iterate cursor in database because it has shut down");
        return;
    }

    RefPtr<AsyncRequest> request = AsyncRequestImpl<const IDBKeyData&, const IDBKeyData&, PassRefPtr<SharedBuffer>, uint32_t, const String&>::create([this, callback](const IDBKeyData& key, const IDBKeyData& primaryKey, PassRefPtr<SharedBuffer> value, uint32_t errorCode, const String& errorMessage) {
        callback(key, primaryKey, value, errorCode, errorMessage);
    }, [this, callback]() {
        callback(nullptr, nullptr, nullptr, INVALID_STATE_ERR, "Unable to iterate cursor in database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::iterateCursorInBackingStore, requestID, cursorIdentifier, key));
}

void UniqueIDBDatabase::count(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyRangeData& keyRangeData, std::function<void(int64_t, uint32_t, const String&)> callback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        callback(0, INVALID_STATE_ERR, "Unable to get count from database because it has shut down");
        return;
    }

    RefPtr<AsyncRequest> request = AsyncRequestImpl<int64_t, uint32_t, const String&>::create([this, callback](int64_t count, uint32_t errorCode, const String& errorMessage) {
        callback(count, errorCode, errorMessage);
    }, [this, callback]() {
        callback(0, INVALID_STATE_ERR, "Unable to get count from database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::countInBackingStore, requestID, transactionIdentifier, objectStoreID, indexID, keyRangeData));
}

void UniqueIDBDatabase::deleteRange(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyRangeData& keyRangeData, std::function<void(uint32_t, const String&)> callback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        callback(INVALID_STATE_ERR, "Unable to deleteRange from database because it has shut down");
        return;
    }

    RefPtr<AsyncRequest> request = AsyncRequestImpl<uint32_t, const String&>::create([callback](uint32_t errorCode, const String& errorMessage) {
        callback(errorCode, errorMessage);
    }, [callback]() {
        callback(INVALID_STATE_ERR, "Unable to deleteRange from database");
    });

    uint64_t requestID = request->requestID();
    m_pendingDatabaseTasks.add(requestID, request.release());

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::deleteRangeInBackingStore, requestID, transactionIdentifier, objectStoreID, keyRangeData));
}

void UniqueIDBDatabase::openBackingStoreTransaction(const IDBIdentifier& transactionIdentifier, const Vector<int64_t>& objectStoreIDs, IndexedDB::TransactionMode mode)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->establishTransaction(transactionIdentifier, objectStoreIDs, mode);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void UniqueIDBDatabase::beginBackingStoreTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->beginTransaction(transactionIdentifier);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void UniqueIDBDatabase::commitBackingStoreTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->commitTransaction(transactionIdentifier);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void UniqueIDBDatabase::resetBackingStoreTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->resetTransaction(transactionIdentifier);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void UniqueIDBDatabase::rollbackBackingStoreTransaction(const IDBIdentifier& transactionIdentifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->rollbackTransaction(transactionIdentifier);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, transactionIdentifier, success));
}

void UniqueIDBDatabase::changeDatabaseVersionInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, uint64_t newVersion)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->changeDatabaseVersion(transactionIdentifier, newVersion);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didChangeDatabaseVersion, requestID, success));
}

void UniqueIDBDatabase::createObjectStoreInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, const IDBObjectStoreMetadata& metadata)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->createObjectStore(transactionIdentifier, metadata);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCreateObjectStore, requestID, success));
}

void UniqueIDBDatabase::deleteObjectStoreInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->deleteObjectStore(transactionIdentifier, objectStoreID);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didDeleteObjectStore, requestID, success));
}

void UniqueIDBDatabase::clearObjectStoreInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->clearObjectStore(transactionIdentifier, objectStoreID);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didClearObjectStore, requestID, success));
}

void UniqueIDBDatabase::createIndexInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBIndexMetadata& metadata)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->createIndex(transactionIdentifier, objectStoreID, metadata);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCreateIndex, requestID, success));
}

void UniqueIDBDatabase::deleteIndexInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->deleteIndex(transactionIdentifier, objectStoreID, indexID);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didDeleteIndex, requestID, success));
}

void UniqueIDBDatabase::putRecordInBackingStore(uint64_t requestID, const IDBIdentifier& transaction, const IDBObjectStoreMetadata& objectStoreMetadata, const IDBKeyData& inputKeyData, const Vector<uint8_t>& value, int64_t putMode, const Vector<int64_t>& indexIDs, const Vector<Vector<IDBKeyData>>& indexKeys)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool keyWasGenerated = false;
    IDBKeyData key;
    int64_t keyNumber = 0;

    if (putMode != IDBDatabaseBackend::CursorUpdate && objectStoreMetadata.autoIncrement && inputKeyData.isNull) {
        if (!m_backingStore->generateKeyNumber(transaction, objectStoreMetadata.id, keyNumber)) {
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error checking for key existence")));
            return;
        }
        key.setNumberValue(keyNumber);
        keyWasGenerated = true;
    } else
        key = inputKeyData;

    if (putMode == IDBDatabaseBackend::AddOnly) {
        bool keyExists;
        if (!m_backingStore->keyExistsInObjectStore(transaction, objectStoreMetadata.id, key, keyExists)) {
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error checking for key existence")));
            return;
        }
        if (keyExists) {
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::ConstraintError, ASCIILiteral("Key already exists in the object store")));
            return;
        }
    }

    // The spec says that even if we're about to overwrite the record, perform the steps to delete it first.
    // This is important because formally deleting it from from the object store also removes it from the appropriate indexes.
    if (!m_backingStore->deleteRecord(transaction, objectStoreMetadata.id, key)) {
        postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Replacing an existing key in backing store, unable to delete previous record.")));
        return;
    }

    if (!m_backingStore->putRecord(transaction, objectStoreMetadata.id, key, value.data(), value.size())) {
        postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error putting a record")));
        return;
    }

    ASSERT(indexIDs.size() == indexKeys.size());
    for (size_t i = 0; i < indexIDs.size(); ++i) {
        for (size_t j = 0; j < indexKeys[i].size(); ++j) {
            if (!m_backingStore->putIndexRecord(transaction, objectStoreMetadata.id, indexIDs[i], key, indexKeys[i][j])) {
                postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error writing index key")));
                return;
            }
        }
    }

    m_backingStore->notifyCursorsOfChanges(transaction, objectStoreMetadata.id);

    if (putMode != IDBDatabaseBackend::CursorUpdate && objectStoreMetadata.autoIncrement && key.type == IDBKey::NumberType) {
        if (!m_backingStore->updateKeyGeneratorNumber(transaction, objectStoreMetadata.id, keyNumber, keyWasGenerated)) {
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error updating key generator")));
            return;
        }
    }

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, key, 0, String(StringImpl::empty())));
}

void UniqueIDBDatabase::didPutRecordInBackingStore(uint64_t requestID, const IDBKeyData& keyData, uint32_t errorCode, const String& errorMessage)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(keyData, errorCode, errorMessage);
}

void UniqueIDBDatabase::getRecordFromBackingStore(uint64_t requestID, const IDBIdentifier& transaction, const IDBObjectStoreMetadata& objectStoreMetadata, int64_t indexID, const IDBKeyRangeData& keyRangeData, IndexedDB::CursorType cursorType)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    RefPtr<IDBKeyRange> keyRange = keyRangeData.maybeCreateIDBKeyRange();
    ASSERT(keyRange);
    if (!keyRange) {
        postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("Invalid IDBKeyRange requested from backing store")));
        return;
    }

    if (indexID == IDBIndexMetadata::InvalidId) {
        // IDBObjectStore get record
        RefPtr<SharedBuffer> result;

        if (keyRange->isOnlyKey()) {
            if (!m_backingStore->getKeyRecordFromObjectStore(transaction, objectStoreMetadata.id, *keyRange->lower(), result))
                postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get key record from object store in backing store")));
            else {
                IDBGetResult getResult = result ? IDBGetResult(result.release(), keyRange->lower(), objectStoreMetadata.keyPath) : IDBGetResult();
                postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, getResult, 0, String(StringImpl::empty())));
            }

            return;
        }

        RefPtr<IDBKey> resultKey;

        if (!m_backingStore->getKeyRangeRecordFromObjectStore(transaction, objectStoreMetadata.id, *keyRange, result, resultKey))
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get key range record from object store in backing store")));
        else {
            IDBGetResult getResult = result ? IDBGetResult(result.release(), resultKey.release(), objectStoreMetadata.keyPath) : IDBGetResult();
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, getResult, 0, String(StringImpl::empty())));
        }

        return;
    }

    // IDBIndex get record

    IDBGetResult result;
    if (!m_backingStore->getIndexRecord(transaction, objectStoreMetadata.id, indexID, keyRangeData, cursorType, result)) {
        postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get index record from backing store")));
        return;
    }

    // A get request that meets the following conditions needs to know the object store keypath
    // to inject the result key into the result value object.
    if (objectStoreMetadata.autoIncrement && !objectStoreMetadata.keyPath.isNull())
        result.keyPath = objectStoreMetadata.keyPath;

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, result, 0, String(StringImpl::empty())));
}

void UniqueIDBDatabase::didGetRecordFromBackingStore(uint64_t requestID, const IDBGetResult& result, uint32_t errorCode, const String& errorMessage)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(result, errorCode, errorMessage);
}

void UniqueIDBDatabase::openCursorInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, const IDBKeyRangeData& keyRange)
{
    ASSERT(!isMainThread());
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

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didOpenCursorInBackingStore, requestID, cursorID, key, primaryKey, valueBuffer, errorCode, errorMessage));
}

void UniqueIDBDatabase::didOpenCursorInBackingStore(uint64_t requestID, int64_t cursorID, const IDBKeyData& key, const IDBKeyData& primaryKey, const Vector<uint8_t>& valueBuffer, uint32_t errorCode, const String& errorMessage)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(cursorID, key, primaryKey, SharedBuffer::create(valueBuffer.data(), valueBuffer.size()), errorCode, errorMessage);
}

void UniqueIDBDatabase::advanceCursorInBackingStore(uint64_t requestID, const IDBIdentifier& cursorIdentifier, uint64_t count)
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

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didAdvanceCursorInBackingStore, requestID, key, primaryKey, valueBuffer, errorCode, errorMessage));
}

void UniqueIDBDatabase::didAdvanceCursorInBackingStore(uint64_t requestID, const IDBKeyData& key, const IDBKeyData& primaryKey, const Vector<uint8_t>& valueBuffer, uint32_t errorCode, const String& errorMessage)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(key, primaryKey, SharedBuffer::create(valueBuffer.data(), valueBuffer.size()), errorCode, errorMessage);
}

void UniqueIDBDatabase::iterateCursorInBackingStore(uint64_t requestID, const IDBIdentifier& cursorIdentifier, const IDBKeyData& iterateKey)
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

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didIterateCursorInBackingStore, requestID, key, primaryKey, valueBuffer, errorCode, errorMessage));
}

void UniqueIDBDatabase::didIterateCursorInBackingStore(uint64_t requestID, const IDBKeyData& key, const IDBKeyData& primaryKey, const Vector<uint8_t>& valueBuffer, uint32_t errorCode, const String& errorMessage)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(key, primaryKey, SharedBuffer::create(valueBuffer.data(), valueBuffer.size()), errorCode, errorMessage);
}

void UniqueIDBDatabase::countInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const IDBKeyRangeData& keyRangeData)
{
    int64_t count;

    if (!m_backingStore->count(transactionIdentifier, objectStoreID, indexID, keyRangeData, count)) {
        LOG_ERROR("Failed to get count from backing store.");
        postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCountInBackingStore, requestID, 0, IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get count from backing store")));
    }

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCountInBackingStore, requestID, count, 0, String(StringImpl::empty())));
}

void UniqueIDBDatabase::didCountInBackingStore(uint64_t requestID, int64_t count, uint32_t errorCode, const String& errorMessage)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(count, errorCode, errorMessage);
}

void UniqueIDBDatabase::deleteRangeInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const IDBKeyRangeData& keyRangeData)
{
    if (!m_backingStore->deleteRange(transactionIdentifier, objectStoreID, keyRangeData)) {
        LOG_ERROR("Failed to delete range from backing store.");
        postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didDeleteRangeInBackingStore, requestID, IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get count from backing store")));
    }

    m_backingStore->notifyCursorsOfChanges(transactionIdentifier, objectStoreID);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didDeleteRangeInBackingStore, requestID, 0, String(StringImpl::empty())));
}

void UniqueIDBDatabase::didDeleteRangeInBackingStore(uint64_t requestID, uint32_t errorCode, const String& errorMessage)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(errorCode, errorMessage);
}

String UniqueIDBDatabase::absoluteDatabaseDirectory() const
{
    ASSERT(isMainThread());
    return DatabaseProcess::shared().absoluteIndexedDatabasePathFromDatabaseRelativePath(m_databaseRelativeDirectory);
}

void UniqueIDBDatabase::postMainThreadTask(std::unique_ptr<AsyncTask> task, DatabaseTaskType taskType)
{
    ASSERT(!isMainThread());

    if (!m_acceptingNewRequests && taskType == DatabaseTaskType::Normal)
        return;

    MutexLocker locker(m_mainThreadTaskMutex);

    m_mainThreadTasks.append(std::move(task));

    // Balanced by an adoptRef() in ::performNextMainThreadTask
    ref();
    RunLoop::main()->dispatch(bind(&UniqueIDBDatabase::performNextMainThreadTask, this));
}

void UniqueIDBDatabase::performNextMainThreadTask()
{
    ASSERT(isMainThread());

    // Balanced by a ref() in ::postMainThreadTask
    RefPtr<UniqueIDBDatabase> protector(adoptRef(this));

    performNextMainThreadTaskWithoutAdoptRef();
}

bool UniqueIDBDatabase::performNextMainThreadTaskWithoutAdoptRef()
{
    bool moreTasks;

    std::unique_ptr<AsyncTask> task;
    {
        MutexLocker locker(m_mainThreadTaskMutex);

        // This database might be shutting down, in which case the task queue might be empty.
        if (m_mainThreadTasks.isEmpty())
            return false;

        task = m_mainThreadTasks.takeFirst();
        moreTasks = !m_mainThreadTasks.isEmpty();
    }

    task->performTask();

    return moreTasks;
}

void UniqueIDBDatabase::postDatabaseTask(std::unique_ptr<AsyncTask> task, DatabaseTaskType taskType)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests && taskType == DatabaseTaskType::Normal)
        return;

    MutexLocker locker(m_databaseTaskMutex);

    m_databaseTasks.append(std::move(task));

    DatabaseProcess::shared().queue().dispatch(bind(&UniqueIDBDatabase::performNextDatabaseTask, this));
}

void UniqueIDBDatabase::performNextDatabaseTask()
{
    ASSERT(!isMainThread());

    // It is possible that this database might be shutting down on the main thread.
    // In this case, immediately after releasing m_databaseTaskMutex, this database might get deleted.
    // We take a ref() to make sure the database is still live while this last task is performed.
    RefPtr<UniqueIDBDatabase> protector(this);

    std::unique_ptr<AsyncTask> task;
    {
        MutexLocker locker(m_databaseTaskMutex);

        // This database might be shutting down on the main thread, in which case the task queue might be empty.
        if (m_databaseTasks.isEmpty())
            return;

        task = m_databaseTasks.takeFirst();
    }

    task->performTask();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
