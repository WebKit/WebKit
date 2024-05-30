
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WorkerCacheStorageConnection.h"

#include "CacheQueryOptions.h"
#include "CacheStorageProvider.h"
#include "ClientOrigin.h"
#include "Document.h"
#include "Logging.h"
#include "Page.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerRunLoop.h"
#include "WorkerThread.h"

namespace WebCore {
using namespace WebCore::DOMCacheEngine;

static inline CrossThreadRecordsOrError isolatedCopyCrossThreadRecordsOrError(CrossThreadRecordsOrError&& records)
{
    if (!records.has_value())
        return makeUnexpected(records.error());

    return crossThreadCopy(WTFMove(records.value()));
}

class StoppedCacheStorageConnection final : public CacheStorageConnection {
public:
    static Ref<CacheStorageConnection> create() { return adoptRef(*new StoppedCacheStorageConnection); }

private:
    Ref<OpenPromise> open(const ClientOrigin&, const String&) final { return OpenPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
    Ref<RemovePromise> remove(DOMCacheIdentifier) final { return RemovePromise::createAndReject(DOMCacheEngine::Error::Stopped); }
    Ref<RetrieveCachesPromise> retrieveCaches(const ClientOrigin&, uint64_t)  final { return RetrieveCachesPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
    Ref<RetrieveRecordsPromise> retrieveRecords(DOMCacheIdentifier, RetrieveRecordsOptions&&)  final { return RetrieveRecordsPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
    Ref<BatchPromise> batchDeleteOperation(DOMCacheIdentifier, const ResourceRequest&, CacheQueryOptions&&)  final { return BatchPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
    Ref<BatchPromise> batchPutOperation(DOMCacheIdentifier, Vector<DOMCacheEngine::CrossThreadRecord>&&)  final { return BatchPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
    void reference(DOMCacheIdentifier)  final { }
    void dereference(DOMCacheIdentifier)  final { }
    void lockStorage(const ClientOrigin&) final { }
    void unlockStorage(const ClientOrigin&) final { }
};

static Ref<CacheStorageConnection> createMainThreadConnection(WorkerGlobalScope& scope)
{
    RefPtr<CacheStorageConnection> mainThreadConnection;
    callOnMainThreadAndWait([workerThread = Ref { scope.thread() }, &mainThreadConnection]() mutable {
        auto* workerLoaderProxy = workerThread->workerLoaderProxy();
        if (!workerThread->runLoop().terminated() && workerLoaderProxy)
            mainThreadConnection = workerLoaderProxy->createCacheStorageConnection();
        if (!mainThreadConnection) {
            RELEASE_LOG_INFO(ServiceWorker, "Creating stopped WorkerCacheStorageConnection");
            mainThreadConnection = StoppedCacheStorageConnection::create();
        }
    });
    return mainThreadConnection.releaseNonNull();
}

WorkerCacheStorageConnection::WorkerCacheStorageConnection(WorkerGlobalScope& scope)
    : m_scope(scope)
    , m_mainThreadConnection(createMainThreadConnection(scope))
{
}

WorkerCacheStorageConnection::~WorkerCacheStorageConnection()
{
    callOnMainThread([mainThreadConnection = WTFMove(m_mainThreadConnection)]() mutable { });
}

auto WorkerCacheStorageConnection::open(const ClientOrigin& origin, const String& cacheName) -> Ref<OpenPromise>
{
    return invokeAsync(RunLoop::main(), [mainThreadConnection = m_mainThreadConnection, origin = origin.isolatedCopy(), cacheName = cacheName.isolatedCopy()] () mutable {
        return mainThreadConnection->open(origin, cacheName);
    });
}

auto WorkerCacheStorageConnection::remove(DOMCacheIdentifier cacheIdentifier) -> Ref<RemovePromise>
{
    return invokeAsync(RunLoop::main(), [mainThreadConnection = m_mainThreadConnection, cacheIdentifier] {
        return mainThreadConnection->remove(cacheIdentifier);
    });
}

auto WorkerCacheStorageConnection::retrieveCaches(const ClientOrigin& origin, uint64_t updateCounter) -> Ref<RetrieveCachesPromise>
{
    return invokeAsync(RunLoop::main(), [mainThreadConnection = m_mainThreadConnection, origin = origin.isolatedCopy(), updateCounter] {
        return mainThreadConnection->retrieveCaches(origin, updateCounter);
    });
}

auto WorkerCacheStorageConnection::retrieveRecords(DOMCacheIdentifier cacheIdentifier, RetrieveRecordsOptions&& options) -> Ref<RetrieveRecordsPromise>
{
    return invokeAsync(RunLoop::main(), [mainThreadConnection = m_mainThreadConnection, cacheIdentifier, options = WTFMove(options).isolatedCopy()] () mutable {
        return mainThreadConnection->retrieveRecords(cacheIdentifier, WTFMove(options));
    });
}

auto WorkerCacheStorageConnection::batchDeleteOperation(DOMCacheIdentifier cacheIdentifier, const ResourceRequest& request, CacheQueryOptions&& options) -> Ref<BatchPromise>
{
    return invokeAsync(RunLoop::main(), [mainThreadConnection = m_mainThreadConnection, cacheIdentifier, request = request.isolatedCopy(), options] () mutable {
        return mainThreadConnection->batchDeleteOperation(cacheIdentifier, request, WTFMove(options));
    });
}

auto WorkerCacheStorageConnection::batchPutOperation(DOMCacheIdentifier cacheIdentifier, Vector<DOMCacheEngine::CrossThreadRecord>&& records) -> Ref<BatchPromise>
{
    return invokeAsync(RunLoop::main(), [mainThreadConnection = m_mainThreadConnection, cacheIdentifier, records = crossThreadCopy(WTFMove(records))] () mutable {
        return mainThreadConnection->batchPutOperation(cacheIdentifier, WTFMove(records));
    });
}

void WorkerCacheStorageConnection::reference(DOMCacheIdentifier cacheIdentifier)
{
    callOnMainThread([mainThreadConnection = m_mainThreadConnection, cacheIdentifier]() {
        mainThreadConnection->reference(cacheIdentifier);
    });
}

void WorkerCacheStorageConnection::dereference(DOMCacheIdentifier cacheIdentifier)
{
    callOnMainThread([mainThreadConnection = m_mainThreadConnection, cacheIdentifier]() {
        mainThreadConnection->dereference(cacheIdentifier);
    });
}

void WorkerCacheStorageConnection::lockStorage(const ClientOrigin& origin)
{
    callOnMainThread([mainThreadConnection = m_mainThreadConnection, origin = origin.isolatedCopy()]() {
        mainThreadConnection->lockStorage(origin);
    });
}

void WorkerCacheStorageConnection::unlockStorage(const ClientOrigin& origin)
{
    callOnMainThread([mainThreadConnection = m_mainThreadConnection, origin = origin.isolatedCopy()]() {
        mainThreadConnection->unlockStorage(origin);
    });
}

} // namespace WebCore
