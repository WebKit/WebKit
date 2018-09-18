
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

static CrossThreadRecordData toCrossThreadRecordData(const Record& record)
{
    return CrossThreadRecordData {
        record.identifier,
        record.updateResponseCounter,
        record.requestHeadersGuard,
        record.request.isolatedCopy(),
        record.options.isolatedCopy(),
        record.referrer.isolatedCopy(),
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

Ref<WorkerCacheStorageConnection> WorkerCacheStorageConnection::create(WorkerGlobalScope& scope)
{
    auto connection = adoptRef(*new WorkerCacheStorageConnection(scope));
    callOnMainThreadAndWait([workerThread = makeRef(scope.thread()), connection = connection.ptr()]() mutable {
        connection->m_mainThreadConnection = workerThread->workerLoaderProxy().createCacheStorageConnection();
    });
    ASSERT(connection->m_mainThreadConnection);
    return connection;
}

WorkerCacheStorageConnection::WorkerCacheStorageConnection(WorkerGlobalScope& scope)
    : m_scope(scope)
{
}

WorkerCacheStorageConnection::~WorkerCacheStorageConnection()
{
    if (m_mainThreadConnection)
        callOnMainThread([mainThreadConnection = WTFMove(m_mainThreadConnection)]() mutable { });
}

void WorkerCacheStorageConnection::doOpen(uint64_t requestIdentifier, const ClientOrigin& origin, const String& cacheName)
{
    callOnMainThread([workerThread = makeRef(m_scope.thread()), mainThreadConnection = m_mainThreadConnection, requestIdentifier, origin = origin.isolatedCopy(), cacheName = cacheName.isolatedCopy()] () mutable {
        mainThreadConnection->open(origin, cacheName, [workerThread = WTFMove(workerThread), requestIdentifier] (const CacheIdentifierOrError& result) mutable {
            workerThread->runLoop().postTaskForMode([requestIdentifier, result] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().openCompleted(requestIdentifier, result);
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::doRemove(uint64_t requestIdentifier, uint64_t cacheIdentifier)
{
    callOnMainThread([workerThread = makeRef(m_scope.thread()), mainThreadConnection = m_mainThreadConnection, requestIdentifier, cacheIdentifier] () mutable {
        mainThreadConnection->remove(cacheIdentifier, [workerThread = WTFMove(workerThread), requestIdentifier, cacheIdentifier] (const CacheIdentifierOrError& result) mutable {
            ASSERT_UNUSED(cacheIdentifier, !result.has_value() || !result.value().identifier || result.value().identifier == cacheIdentifier);
            workerThread->runLoop().postTaskForMode([requestIdentifier, result] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().removeCompleted(requestIdentifier, result);
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::doRetrieveCaches(uint64_t requestIdentifier, const ClientOrigin& origin, uint64_t updateCounter)
{
    callOnMainThread([workerThread = makeRef(m_scope.thread()), mainThreadConnection = m_mainThreadConnection, requestIdentifier, origin = origin.isolatedCopy(), updateCounter] () mutable {
        mainThreadConnection->retrieveCaches(origin, updateCounter, [workerThread = WTFMove(workerThread), requestIdentifier] (CacheInfosOrError&& result) mutable {
            CacheInfosOrError isolatedResult;
            if (!result.has_value())
                isolatedResult = WTFMove(result);
            else
                isolatedResult = result.value().isolatedCopy();

            workerThread->runLoop().postTaskForMode([requestIdentifier, result = WTFMove(isolatedResult)] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().updateCaches(requestIdentifier, WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::reference(uint64_t cacheIdentifier)
{
    callOnMainThread([mainThreadConnection = m_mainThreadConnection, cacheIdentifier]() {
        mainThreadConnection->reference(cacheIdentifier);
    });
}

void WorkerCacheStorageConnection::dereference(uint64_t cacheIdentifier)
{
    callOnMainThread([mainThreadConnection = m_mainThreadConnection, cacheIdentifier]() {
        mainThreadConnection->dereference(cacheIdentifier);
    });
}

static inline Vector<CrossThreadRecordData> recordsDataFromRecords(const Vector<Record>& records)
{
    return WTF::map(records, toCrossThreadRecordData);
}

static inline Expected<Vector<CrossThreadRecordData>, Error> recordsDataOrErrorFromRecords(const RecordsOrError& result)
{
    if (!result.has_value())
        return makeUnexpected(result.error());

    return recordsDataFromRecords(result.value());
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

void WorkerCacheStorageConnection::doRetrieveRecords(uint64_t requestIdentifier, uint64_t cacheIdentifier, const URL& url)
{
    callOnMainThread([workerThread = makeRef(m_scope.thread()), mainThreadConnection = m_mainThreadConnection, requestIdentifier, cacheIdentifier, url = url.isolatedCopy()]() mutable {
        mainThreadConnection->retrieveRecords(cacheIdentifier, url, [workerThread = WTFMove(workerThread), requestIdentifier](RecordsOrError&& result) mutable {
            workerThread->runLoop().postTaskForMode([result = recordsDataOrErrorFromRecords(result), requestIdentifier] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().updateRecords(requestIdentifier, recordsOrErrorFromRecordsData(WTFMove(result)));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::doBatchDeleteOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, const ResourceRequest& request, CacheQueryOptions&& options)
{
    callOnMainThread([workerThread = makeRef(m_scope.thread()), mainThreadConnection = m_mainThreadConnection, requestIdentifier, cacheIdentifier, request = request.isolatedCopy(), options = options.isolatedCopy()]() mutable {
        mainThreadConnection->batchDeleteOperation(cacheIdentifier, request, WTFMove(options), [workerThread = WTFMove(workerThread), requestIdentifier](RecordIdentifiersOrError&& result) mutable {
            workerThread->runLoop().postTaskForMode([requestIdentifier, result = WTFMove(result)] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().deleteRecordsCompleted(requestIdentifier, WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerCacheStorageConnection::doBatchPutOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, Vector<Record>&& records)
{
    callOnMainThread([workerThread = makeRef(m_scope.thread()), mainThreadConnection = m_mainThreadConnection, requestIdentifier, cacheIdentifier, recordsData = recordsDataFromRecords(records)]() mutable {
        mainThreadConnection->batchPutOperation(cacheIdentifier, recordsFromRecordsData(WTFMove(recordsData)), [workerThread = WTFMove(workerThread), requestIdentifier] (RecordIdentifiersOrError&& result) mutable {
            workerThread->runLoop().postTaskForMode([requestIdentifier, result = WTFMove(result)] (auto& scope) mutable {
                downcast<WorkerGlobalScope>(scope).cacheStorageConnection().putRecordsCompleted(requestIdentifier, WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

} // namespace WebCore
