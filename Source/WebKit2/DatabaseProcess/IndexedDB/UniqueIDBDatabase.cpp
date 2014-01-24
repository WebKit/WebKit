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
#include "UniqueIDBDatabaseBackingStoreSQLite.h"
#include "WebCrossThreadCopier.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IDBDatabaseBackend.h>
#include <WebCore/IDBDatabaseMetadata.h>
#include <WebCore/IDBGetResult.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBKeyRangeData.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

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
        shutdown();
        DatabaseProcess::shared().removeUniqueIDBDatabase(*this);
    }
}

void UniqueIDBDatabase::shutdown()
{
    ASSERT(isMainThread());

    m_acceptingNewRequests = false;

    {
        MutexLocker locker(m_databaseTaskMutex);
        m_databaseTasks.clear();
    }

    {
        MutexLocker locker(m_mainThreadTaskMutex);
        m_mainThreadTasks.clear();
    }

    for (const auto& it : m_pendingMetadataRequests)
        it->requestAborted();

    for (const auto& it : m_pendingTransactionRequests)
        it.value->requestAborted();

    for (const auto& it : m_pendingDatabaseTasks)
        it.value->requestAborted();
        
    // Balanced by an adoptRef in ::didShutdownBackingStore()
    ref();

    DatabaseProcess::shared().queue().dispatch(bind(&UniqueIDBDatabase::shutdownBackingStore, this));
}

void UniqueIDBDatabase::shutdownBackingStore()
{
    ASSERT(!isMainThread());
    if (m_backingStore)
        m_backingStore.clear();

    RunLoop::main()->dispatch(bind(&UniqueIDBDatabase::didShutdownBackingStore, this));
}

void UniqueIDBDatabase::didShutdownBackingStore()
{
    ASSERT(isMainThread());

    // Balanced by a ref in ::shutdown()
    RefPtr<UniqueIDBDatabase> protector(adoptRef(this));
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

    // If this is the first unanswered metadata request, post a task to open the backing store and get metadata.
    if (m_pendingMetadataRequests.isEmpty())
        postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::openBackingStoreAndReadMetadata, m_identifier, absoluteDatabaseDirectory()));

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

void UniqueIDBDatabase::openTransaction(const IDBTransactionIdentifier& identifier, const Vector<int64_t>& objectStoreIDs, IndexedDB::TransactionMode mode, std::function<void(bool)> successCallback)
{
    postTransactionOperation(identifier, createAsyncTask(*this, &UniqueIDBDatabase::openBackingStoreTransaction, identifier, objectStoreIDs, mode), successCallback);
}

void UniqueIDBDatabase::beginTransaction(const IDBTransactionIdentifier& identifier, std::function<void(bool)> successCallback)
{
    postTransactionOperation(identifier, createAsyncTask(*this, &UniqueIDBDatabase::beginBackingStoreTransaction, identifier), successCallback);
}

void UniqueIDBDatabase::commitTransaction(const IDBTransactionIdentifier& identifier, std::function<void(bool)> successCallback)
{
    postTransactionOperation(identifier, createAsyncTask(*this, &UniqueIDBDatabase::commitBackingStoreTransaction, identifier), successCallback);
}

void UniqueIDBDatabase::resetTransaction(const IDBTransactionIdentifier& identifier, std::function<void(bool)> successCallback)
{
    postTransactionOperation(identifier, createAsyncTask(*this, &UniqueIDBDatabase::resetBackingStoreTransaction, identifier), successCallback);
}

void UniqueIDBDatabase::rollbackTransaction(const IDBTransactionIdentifier& identifier, std::function<void(bool)> successCallback)
{
    postTransactionOperation(identifier, createAsyncTask(*this, &UniqueIDBDatabase::rollbackBackingStoreTransaction, identifier), successCallback);
}

void UniqueIDBDatabase::postTransactionOperation(const IDBTransactionIdentifier& identifier, std::unique_ptr<AsyncTask> task, std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    if (m_pendingTransactionRequests.contains(identifier)) {
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

    m_pendingTransactionRequests.add(identifier, request.release());
}

void UniqueIDBDatabase::didCompleteTransactionOperation(const IDBTransactionIdentifier& identifier, bool success)
{
    ASSERT(isMainThread());

    RefPtr<AsyncRequest> request = m_pendingTransactionRequests.take(identifier);
    if (!request)
        return;

    request->completeRequest(success);
}

void UniqueIDBDatabase::changeDatabaseVersion(const IDBTransactionIdentifier& identifier, uint64_t newVersion, std::function<void(bool)> successCallback)
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

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::changeDatabaseVersionInBackingStore, requestID, identifier, newVersion));
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

void UniqueIDBDatabase::didCompleteBoolRequest(uint64_t requestID, bool success)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(success);
}

void UniqueIDBDatabase::createObjectStore(const IDBTransactionIdentifier& identifier, const IDBObjectStoreMetadata& metadata, std::function<void(bool)> successCallback)
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

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::createObjectStoreInBackingStore, requestID, identifier, metadata));
}

void UniqueIDBDatabase::deleteObjectStore(const IDBTransactionIdentifier& identifier, int64_t objectStoreID, std::function<void(bool)> successCallback)
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

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::deleteObjectStoreInBackingStore, requestID, identifier, objectStoreID));
}

void UniqueIDBDatabase::clearObjectStore(const IDBTransactionIdentifier& identifier, int64_t objectStoreID, std::function<void(bool)> successCallback)
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

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::clearObjectStoreInBackingStore, requestID, identifier, objectStoreID));
}

void UniqueIDBDatabase::putRecord(const IDBTransactionIdentifier& identifier, int64_t objectStoreID, const IDBKeyData& keyData, const IPC::DataReference& value, int64_t putMode, const Vector<int64_t>& indexIDs, const Vector<Vector<IDBKeyData>>& indexKeys, std::function<void(const IDBKeyData&, uint32_t, const String&)> callback)
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

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::putRecordInBackingStore, requestID, identifier, m_metadata->objectStores.get(objectStoreID), keyData, value.vector(), putMode, indexIDs, indexKeys));
}

void UniqueIDBDatabase::getRecord(const IDBTransactionIdentifier& identifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyRangeData& keyRangeData, WebCore::IndexedDB::CursorType cursorType, std::function<void(const WebCore::IDBGetResult&, uint32_t, const String&)> callback)
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

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::getRecordFromBackingStore, requestID, identifier, m_metadata->objectStores.get(objectStoreID), indexID, keyRangeData, cursorType));
}

void UniqueIDBDatabase::openBackingStoreTransaction(const IDBTransactionIdentifier& identifier, const Vector<int64_t>& objectStoreIDs, IndexedDB::TransactionMode mode)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->establishTransaction(identifier, objectStoreIDs, mode);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, identifier, success));
}

void UniqueIDBDatabase::beginBackingStoreTransaction(const IDBTransactionIdentifier& identifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->beginTransaction(identifier);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, identifier, success));
}

void UniqueIDBDatabase::commitBackingStoreTransaction(const IDBTransactionIdentifier& identifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->commitTransaction(identifier);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, identifier, success));
}

void UniqueIDBDatabase::resetBackingStoreTransaction(const IDBTransactionIdentifier& identifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->resetTransaction(identifier);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, identifier, success));
}

void UniqueIDBDatabase::rollbackBackingStoreTransaction(const IDBTransactionIdentifier& identifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->rollbackTransaction(identifier);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCompleteTransactionOperation, identifier, success));
}

void UniqueIDBDatabase::changeDatabaseVersionInBackingStore(uint64_t requestID, const IDBTransactionIdentifier& identifier, uint64_t newVersion)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->changeDatabaseVersion(identifier, newVersion);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didChangeDatabaseVersion, requestID, success));
}

void UniqueIDBDatabase::createObjectStoreInBackingStore(uint64_t requestID, const IDBTransactionIdentifier& identifier, const IDBObjectStoreMetadata& metadata)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->createObjectStore(identifier, metadata);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didCreateObjectStore, requestID, success));
}

void UniqueIDBDatabase::deleteObjectStoreInBackingStore(uint64_t requestID, const IDBTransactionIdentifier& identifier, int64_t objectStoreID)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->deleteObjectStore(identifier, objectStoreID);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didDeleteObjectStore, requestID, success));
}

void UniqueIDBDatabase::clearObjectStoreInBackingStore(uint64_t requestID, const IDBTransactionIdentifier& identifier, int64_t objectStoreID)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->clearObjectStore(identifier, objectStoreID);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didClearObjectStore, requestID, success));
}

void UniqueIDBDatabase::putRecordInBackingStore(uint64_t requestID, const IDBTransactionIdentifier& transaction, const IDBObjectStoreMetadata& objectStoreMetadata, const IDBKeyData& keyData, const Vector<uint8_t>& value, int64_t putMode, const Vector<int64_t>& indexIDs, const Vector<Vector<IDBKeyData>>& indexKeys)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool keyWasGenerated = false;
    RefPtr<IDBKey> key;

    if (putMode != IDBDatabaseBackend::CursorUpdate && objectStoreMetadata.autoIncrement && keyData.isNull) {
        key = m_backingStore->generateKey(transaction, objectStoreMetadata.id);
        keyWasGenerated = true;
    } else
        key = keyData.maybeCreateIDBKey();

    ASSERT(key);
    ASSERT(key->isValid());

    if (putMode == IDBDatabaseBackend::AddOnly) {
        bool keyExists;
        if (!m_backingStore->keyExistsInObjectStore(transaction, objectStoreMetadata.id, *key, keyExists)) {
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error checking for key existence")));
            return;
        }
        if (keyExists) {
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::ConstraintError, ASCIILiteral("Key already exists in the object store")));
            return;
        }
    }

    // FIXME: The LevelDB port performs "makeIndexWriters" here. Necessary?

    if (!m_backingStore->putRecord(transaction, objectStoreMetadata.id, *key, value.data(), value.size())) {
        postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error putting a record")));
        return;
    }

    // FIXME: The LevelDB port updates index keys here. Necessary?

    if (putMode != IDBDatabaseBackend::CursorUpdate && objectStoreMetadata.autoIncrement && key->type() == IDBKey::NumberType) {
        if (!m_backingStore->updateKeyGenerator(transaction, objectStoreMetadata.id, *key, keyWasGenerated)) {
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(), IDBDatabaseException::UnknownError, ASCIILiteral("Internal backing store error updating key generator")));
            return;
        }
    }

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didPutRecordInBackingStore, requestID, IDBKeyData(key.get()), 0, String(StringImpl::empty())));
}

void UniqueIDBDatabase::didPutRecordInBackingStore(uint64_t requestID, const IDBKeyData& keyData, uint32_t errorCode, const String& errorMessage)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(keyData, errorCode, errorMessage);
}

void UniqueIDBDatabase::getRecordFromBackingStore(uint64_t requestID, const IDBTransactionIdentifier& transaction, const WebCore::IDBObjectStoreMetadata& objectStoreMetadata, int64_t indexID, const WebCore::IDBKeyRangeData& keyRangeData, WebCore::IndexedDB::CursorType cursorType)
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
            else
                postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(result.release()), 0, String(StringImpl::empty())));

            return;
        }

        RefPtr<IDBKey> resultKey;

        if (!m_backingStore->getKeyRangeRecordFromObjectStore(transaction, objectStoreMetadata.id, *keyRange, result, resultKey))
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("Failed to get key range record from object store in backing store")));
        else
            postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(result.release(), resultKey.release(), IDBKeyPath()), 0, String(StringImpl::empty())));

        return;
    }

    // IDBIndex get record

    // FIXME: Implement index gets (<rdar://problem/15779642>)
    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didGetRecordFromBackingStore, requestID, IDBGetResult(), IDBDatabaseException::UnknownError, ASCIILiteral("'get' from indexes not supported yet")));
}

void UniqueIDBDatabase::didGetRecordFromBackingStore(uint64_t requestID, const IDBGetResult& result, uint32_t errorCode, const String& errorMessage)
{
    RefPtr<AsyncRequest> request = m_pendingDatabaseTasks.take(requestID);
    ASSERT(request);

    request->completeRequest(result, errorCode, errorMessage);
}

String UniqueIDBDatabase::absoluteDatabaseDirectory() const
{
    ASSERT(isMainThread());
    return DatabaseProcess::shared().absoluteIndexedDatabasePathFromDatabaseRelativePath(m_databaseRelativeDirectory);
}

void UniqueIDBDatabase::postMainThreadTask(std::unique_ptr<AsyncTask> task)
{
    ASSERT(!isMainThread());

    if (!m_acceptingNewRequests)
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

    std::unique_ptr<AsyncTask> task;
    {
        MutexLocker locker(m_mainThreadTaskMutex);

        // This database might be shutting down, in which case the task queue might be empty.
        if (m_mainThreadTasks.isEmpty())
            return;

        task = m_mainThreadTasks.takeFirst();
    }

    task->performTask();
}

void UniqueIDBDatabase::postDatabaseTask(std::unique_ptr<AsyncTask> task)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests)
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
