
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

struct CrossThreadRecordData {
    uint64_t identifier;
    uint64_t updateResponseCounter;

    FetchHeaders::Guard requestHeadersGuard;
    ResourceRequest request;

    FetchOptions options;
    String referrer;

    FetchHeaders::Guard responseHeadersGuard;
    ResourceResponse::CrossThreadData response;
    ResponseBody responseBody;
    uint64_t responseBodySize;
};

static CrossThreadRecordData toCrossThreadRecordData(Record&& record)
{
    return CrossThreadRecordData {
        record.identifier,
        record.updateResponseCounter,
        record.requestHeadersGuard,
        WTFMove(record.request).isolatedCopy(),
        WTFMove(record.options).isolatedCopy(),
        WTFMove(record.referrer).isolatedCopy(),
        record.responseHeadersGuard,
        record.response.crossThreadData(),
        isolatedResponseBody(record.responseBody),
        record.responseBodySize
    };
}

static Record fromCrossThreadRecordData(CrossThreadRecordData&& data)
{
    return Record {
        data.identifier,
        data.updateResponseCounter,
        data.requestHeadersGuard,
        WTFMove(data.request),
        WTFMove(data.options),
        WTFMove(data.referrer),
        data.responseHeadersGuard,
        ResourceResponse::fromCrossThreadData(WTFMove(data.response)),
        WTFMove(data.responseBody),
        data.responseBodySize
    };
}

static inline Vector<CrossThreadRecordData> recordsDataFromRecords(Vector<Record>&& records)
{
    return WTF::map(WTFMove(records), toCrossThreadRecordData);
}

static inline Expected<Vector<CrossThreadRecordData>, Error> recordsDataOrErrorFromRecords(RecordsOrError&& result)
{
    if (!result.has_value())
        return makeUnexpected(WTFMove(result).error());

    return recordsDataFromRecords(WTFMove(result).value());
}

static inline Vector<Record> recordsFromRecordsData(Vector<CrossThreadRecordData>&& recordsData)
{
    return WTF::map(WTFMove(recordsData), fromCrossThreadRecordData);
}

static inline RecordsOrError recordsOrErrorFromRecordsData(Expected<Vector<CrossThreadRecordData>, Error>&& recordsData)
{
    if (!recordsData.has_value())
        return makeUnexpected(recordsData.error());
    return recordsFromRecordsData(WTFMove(recordsData.value()));
}

class StoppedCacheStorageConnection final : public CacheStorageConnection {
public:
    static Ref<CacheStorageConnection> create() { return adoptRef(*new StoppedCacheStorageConnection); }

private:
    void open(const ClientOrigin&, const String&, DOMCacheEngine::CacheIdentifierCallback&& callback) final { callback(makeUnexpected(DOMCacheEngine::Error::Stopped)); }
    void remove(DOMCacheIdentifier, DOMCacheEngine::RemoveCacheIdentifierCallback&& callback)  final { callback(makeUnexpected(DOMCacheEngine::Error::Stopped)); }
    void retrieveCaches(const ClientOrigin&, uint64_t, DOMCacheEngine::CacheInfosCallback&& callback)  final { callback(makeUnexpected(DOMCacheEngine::Error::Stopped)); }
    void retrieveRecords(DOMCacheIdentifier, RetrieveRecordsOptions&&, DOMCacheEngine::RecordsCallback&& callback)  final { callback(makeUnexpected(DOMCacheEngine::Error::Stopped)); }
    void batchDeleteOperation(DOMCacheIdentifier, const ResourceRequest&, CacheQueryOptions&&, DOMCacheEngine::RecordIdentifiersCallback&& callback)  final { callback(makeUnexpected(DOMCacheEngine::Error::Stopped)); }
    void batchPutOperation(DOMCacheIdentifier, Vector<DOMCacheEngine::Record>&&, DOMCacheEngine::RecordIdentifiersCallback&& callback)  final { callback(makeUnexpected(DOMCacheEngine::Error::Stopped)); }
    void reference(DOMCacheIdentifier)  final { }
    void dereference(DOMCacheIdentifier)  final { }
};

static Ref<CacheStorageConnection> createMainThreadConnection(WorkerGlobalScope& scope)
{
    RefPtr<CacheStorageConnection> mainThreadConnection;
    callOnMainThreadAndWait([workerThread = Ref { scope.thread() }, &mainThreadConnection]() mutable {
        if (!workerThread->runLoop().terminated())
            mainThreadConnection = workerThread->workerLoaderProxy().createCacheStorageConnection();
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

void WorkerCacheStorageConnection::open(const ClientOrigin& origin, const String& cacheName, CacheIdentifierCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_openCachePendingRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([workerThread = Ref { m_scope.thread() }, mainThreadConnection = m_mainThreadConnection, requestIdentifier, origin = origin.isolatedCopy(), cacheName = cacheName.isolatedCopy()] () mutable {
        mainThreadConnection->open(origin, cacheName, [workerThread = WTFMove(workerThread), requestIdentifier] (const CacheIdentifierOrError& result) mutable {
            workerThread->runLoop().postTaskForMode([requestIdentifier, result] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().openCompleted(requestIdentifier, result);
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::openCompleted(uint64_t requestIdentifier, const CacheIdentifierOrError& result)
{
    if (auto callback = m_openCachePendingRequests.take(requestIdentifier))
        callback(result);
}

void WorkerCacheStorageConnection::remove(DOMCacheIdentifier cacheIdentifier, RemoveCacheIdentifierCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_removeCachePendingRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([workerThread = Ref { m_scope.thread() }, mainThreadConnection = m_mainThreadConnection, requestIdentifier, cacheIdentifier] () mutable {
        mainThreadConnection->remove(cacheIdentifier, [workerThread = WTFMove(workerThread), requestIdentifier] (const auto& result) mutable {
            workerThread->runLoop().postTaskForMode([requestIdentifier, result] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().removeCompleted(requestIdentifier, result);
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::removeCompleted(uint64_t requestIdentifier, const RemoveCacheIdentifierOrError& result)
{
    if (auto callback = m_removeCachePendingRequests.take(requestIdentifier))
        callback(result);
}

void WorkerCacheStorageConnection::retrieveCaches(const ClientOrigin& origin, uint64_t updateCounter, CacheInfosCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_retrieveCachesPendingRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([workerThread = Ref { m_scope.thread() }, mainThreadConnection = m_mainThreadConnection, requestIdentifier, origin = origin.isolatedCopy(), updateCounter] () mutable {
        mainThreadConnection->retrieveCaches(origin, updateCounter, [workerThread = WTFMove(workerThread), requestIdentifier] (CacheInfosOrError&& result) mutable {
            CacheInfosOrError isolatedResult;
            if (!result.has_value())
                isolatedResult = WTFMove(result);
            else
                isolatedResult = WTFMove(result).value().isolatedCopy();

            workerThread->runLoop().postTaskForMode([requestIdentifier, result = WTFMove(isolatedResult)] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().retrieveCachesCompleted(requestIdentifier, WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::retrieveCachesCompleted(uint64_t requestIdentifier, CacheInfosOrError&& result)
{
    if (auto callback = m_retrieveCachesPendingRequests.take(requestIdentifier))
        callback(WTFMove(result));
}

void WorkerCacheStorageConnection::retrieveRecords(DOMCacheIdentifier cacheIdentifier, RetrieveRecordsOptions&& options, RecordsCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_retrieveRecordsPendingRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([workerThread = Ref { m_scope.thread() }, mainThreadConnection = m_mainThreadConnection, requestIdentifier, cacheIdentifier, options = WTFMove(options).isolatedCopy()]() mutable {
        mainThreadConnection->retrieveRecords(cacheIdentifier, WTFMove(options), [workerThread = WTFMove(workerThread), requestIdentifier](RecordsOrError&& result) mutable {
            workerThread->runLoop().postTaskForMode([result = recordsDataOrErrorFromRecords(WTFMove(result)), requestIdentifier] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().retrieveRecordsCompleted(requestIdentifier, recordsOrErrorFromRecordsData(WTFMove(result)));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::retrieveRecordsCompleted(uint64_t requestIdentifier, RecordsOrError&& result)
{
    if (auto callback = m_retrieveRecordsPendingRequests.take(requestIdentifier))
        callback(WTFMove(result));
}

void WorkerCacheStorageConnection::batchDeleteOperation(DOMCacheIdentifier cacheIdentifier, const ResourceRequest& request, CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_batchDeleteAndPutPendingRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([workerThread = Ref { m_scope.thread() }, mainThreadConnection = m_mainThreadConnection, requestIdentifier, cacheIdentifier, request = request.isolatedCopy(), options]() mutable {
        mainThreadConnection->batchDeleteOperation(cacheIdentifier, request, WTFMove(options), [workerThread = WTFMove(workerThread), requestIdentifier](RecordIdentifiersOrError&& result) mutable {
            workerThread->runLoop().postTaskForMode([requestIdentifier, result = WTFMove(result)] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().deleteRecordsCompleted(requestIdentifier, WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::deleteRecordsCompleted(uint64_t requestIdentifier, Expected<Vector<uint64_t>, Error>&& result)
{
    if (auto callback = m_batchDeleteAndPutPendingRequests.take(requestIdentifier))
        callback(WTFMove(result));
}

void WorkerCacheStorageConnection::batchPutOperation(DOMCacheIdentifier cacheIdentifier, Vector<DOMCacheEngine::Record>&& records, DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_batchDeleteAndPutPendingRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([workerThread = Ref { m_scope.thread() }, mainThreadConnection = m_mainThreadConnection, requestIdentifier, cacheIdentifier, recordsData = recordsDataFromRecords(WTFMove(records))]() mutable {
        mainThreadConnection->batchPutOperation(cacheIdentifier, recordsFromRecordsData(WTFMove(recordsData)), [workerThread = WTFMove(workerThread), requestIdentifier] (RecordIdentifiersOrError&& result) mutable {
            workerThread->runLoop().postTaskForMode([requestIdentifier, result = WTFMove(result)] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().putRecordsCompleted(requestIdentifier, WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::putRecordsCompleted(uint64_t requestIdentifier, Expected<Vector<uint64_t>, Error>&& result)
{
    if (auto callback = m_batchDeleteAndPutPendingRequests.take(requestIdentifier))
        callback(WTFMove(result));
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

void WorkerCacheStorageConnection::clearPendingRequests()
{
    auto openCachePendingRequests = WTFMove(m_openCachePendingRequests);
    for (auto& callback : openCachePendingRequests.values())
        callback(makeUnexpected(DOMCacheEngine::Error::Stopped));

    auto removeCachePendingRequests = WTFMove(m_removeCachePendingRequests);
    for (auto& callback : removeCachePendingRequests.values())
        callback(makeUnexpected(DOMCacheEngine::Error::Stopped));

    auto retrieveCachesPendingRequests = WTFMove(m_retrieveCachesPendingRequests);
    for (auto& callback : retrieveCachesPendingRequests.values())
        callback(makeUnexpected(DOMCacheEngine::Error::Stopped));

    auto retrieveRecordsPendingRequests = WTFMove(m_retrieveRecordsPendingRequests);
    for (auto& callback : retrieveRecordsPendingRequests.values())
        callback(makeUnexpected(DOMCacheEngine::Error::Stopped));

    auto batchDeleteAndPutPendingRequests = WTFMove(m_batchDeleteAndPutPendingRequests);
    for (auto& callback : batchDeleteAndPutPendingRequests.values())
        callback(makeUnexpected(DOMCacheEngine::Error::Stopped));
}

} // namespace WebCore
