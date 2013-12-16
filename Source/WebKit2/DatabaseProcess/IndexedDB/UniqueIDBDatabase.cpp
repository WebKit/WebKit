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
#include "DatabaseProcess.h"
#include "DatabaseProcessIDBConnection.h"
#include "UniqueIDBDatabaseBackingStoreSQLite.h"
#include "WebCrossThreadCopier.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IDBDatabaseMetadata.h>
#include <wtf/MainThread.h>

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

    while (!m_pendingMetadataRequests.isEmpty()) {
        auto request = m_pendingMetadataRequests.takeFirst();
        request->requestAborted();
    }
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

void UniqueIDBDatabase::openTransaction(const IDBTransactionIdentifier& identifier, std::function<void(bool)> successCallback)
{
    ASSERT(isMainThread());

    if (!m_acceptingNewRequests) {
        successCallback(false);
        return;
    }

    postDatabaseTask(createAsyncTask(*this, &UniqueIDBDatabase::openBackingStoreTransaction, identifier));

    RefPtr<AsyncRequest> request = AsyncRequestImpl<bool>::create([successCallback](bool success) {
        successCallback(success);
    }, [successCallback]() {
        successCallback(false);
    });

    m_pendingOpenTransactionRequests.set(identifier, request.release());
}

void UniqueIDBDatabase::openBackingStoreTransaction(const IDBTransactionIdentifier& identifier)
{
    ASSERT(!isMainThread());
    ASSERT(m_backingStore);

    bool success = m_backingStore->establishTransaction(identifier);

    postMainThreadTask(createAsyncTask(*this, &UniqueIDBDatabase::didOpenBackingStoreTransaction, identifier, success));
}

void UniqueIDBDatabase::didOpenBackingStoreTransaction(const IDBTransactionIdentifier& identifier, bool success)
{
    ASSERT(isMainThread());

    RefPtr<AsyncRequest> request = m_pendingOpenTransactionRequests.take(identifier);

    if (!request)
        return;

    request->completeRequest(success);
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

    RunLoop::main()->dispatch(bind(&UniqueIDBDatabase::performNextMainThreadTask, this));
}

void UniqueIDBDatabase::performNextMainThreadTask()
{
    ASSERT(isMainThread());

    std::unique_ptr<AsyncTask> task;
    {
        MutexLocker locker(m_mainThreadTaskMutex);
        ASSERT(!m_mainThreadTasks.isEmpty());
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
