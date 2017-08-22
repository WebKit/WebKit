
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
#include "Document.h"
#include "Page.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerRunLoop.h"
#include "WorkerThread.h"

namespace WebCore {

struct CrossThreadRecordData {
    uint64_t identifier;
    uint64_t updateResponseCounter;

    FetchHeaders::Guard requestHeadersGuard;
    ResourceRequest request;

    FetchOptions options;
    String referrer;

    FetchHeaders::Guard responseHeadersGuard;
    ResourceResponse::CrossThreadData response;
    CacheStorageConnection::ResponseBody responseBody;
};

static CrossThreadRecordData toCrossThreadRecordData(const CacheStorageConnection::Record& record)
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
        CacheStorageConnection::isolatedResponseBody(record.responseBody)
    };
}

static CacheStorageConnection::Record fromCrossThreadRecordData(CrossThreadRecordData&& data)
{
    return CacheStorageConnection::Record {
        data.identifier,
        data.updateResponseCounter,
        data.requestHeadersGuard,
        WTFMove(data.request),
        WTFMove(data.options),
        WTFMove(data.referrer),
        data.responseHeadersGuard,
        ResourceResponse::fromCrossThreadData(WTFMove(data.response)),
        WTFMove(data.responseBody)
    };
}

Ref<WorkerCacheStorageConnection> WorkerCacheStorageConnection::create(WorkerGlobalScope& scope)
{
    auto connection = adoptRef(*new WorkerCacheStorageConnection(scope));
    connection->m_proxy.postTaskToLoader([protectedConnection = makeRef(connection.get())](ScriptExecutionContext& context) mutable {
        ASSERT(isMainThread());
        Document& document = downcast<Document>(context);

        ASSERT(document.page());
        protectedConnection->m_mainThreadConnection = document.page()->cacheStorageProvider().createCacheStorageConnection(document.page()->sessionID());
    });
    return connection;
}

WorkerCacheStorageConnection::WorkerCacheStorageConnection(WorkerGlobalScope& scope)
    : m_scope(scope)
    , m_proxy(m_scope.thread().workerLoaderProxy())
    , m_taskMode(WorkerRunLoop::defaultMode().isolatedCopy())
{
}

WorkerCacheStorageConnection::~WorkerCacheStorageConnection()
{
    if (m_mainThreadConnection)
        m_proxy.postTaskToLoader([mainThreadConnection = WTFMove(m_mainThreadConnection)](ScriptExecutionContext&) mutable { });
}

void WorkerCacheStorageConnection::doOpen(uint64_t requestIdentifier, const String& origin, const String& cacheName)
{
    m_proxy.postTaskToLoader([this, protectedThis = makeRef(*this), requestIdentifier, origin = origin.isolatedCopy(), cacheName = cacheName.isolatedCopy()](ScriptExecutionContext&) mutable {
        ASSERT(isMainThread());
        ASSERT(m_mainThreadConnection);

        m_mainThreadConnection->open(origin, cacheName, [this, protectedThis = WTFMove(protectedThis), requestIdentifier](uint64_t cacheIdentifier, Error error) mutable {
            m_proxy.postTaskForModeToWorkerGlobalScope([this, error, cacheIdentifier, protectedThis = WTFMove(protectedThis), requestIdentifier](ScriptExecutionContext& context) mutable {
                ASSERT_UNUSED(context, context.isWorkerGlobalScope());
                openCompleted(requestIdentifier, cacheIdentifier, error);
            }, m_taskMode);
        });
    });
}

void WorkerCacheStorageConnection::doRemove(uint64_t requestIdentifier, uint64_t cacheIdentifier)
{
    m_proxy.postTaskToLoader([this, protectedThis = makeRef(*this), requestIdentifier, cacheIdentifier](ScriptExecutionContext&) mutable {
        ASSERT(isMainThread());
        ASSERT(m_mainThreadConnection);

        m_mainThreadConnection->remove(cacheIdentifier, [this, protectedThis = WTFMove(protectedThis), requestIdentifier, cacheIdentifier](uint64_t removedCacheIdentifier, Error error) mutable {
            ASSERT_UNUSED(removedCacheIdentifier, removedCacheIdentifier == cacheIdentifier);
            m_proxy.postTaskForModeToWorkerGlobalScope([this, protectedThis = WTFMove(protectedThis), requestIdentifier, cacheIdentifier, error](ScriptExecutionContext& context) mutable {
                ASSERT_UNUSED(context, context.isWorkerGlobalScope());
                removeCompleted(requestIdentifier, cacheIdentifier, error);
            }, m_taskMode);
        });
    });
}

void WorkerCacheStorageConnection::doRetrieveCaches(uint64_t requestIdentifier, const String& origin)
{
    m_proxy.postTaskToLoader([this, protectedThis = makeRef(*this), requestIdentifier, origin = origin.isolatedCopy()](ScriptExecutionContext&) mutable {
        ASSERT(isMainThread());
        ASSERT(m_mainThreadConnection);

        m_mainThreadConnection->retrieveCaches(origin, [this, protectedThis = WTFMove(protectedThis), requestIdentifier](const Vector<CacheInfo>& caches) mutable {
            Vector<CacheInfo> isolatedCaches;
            isolatedCaches.reserveInitialCapacity(caches.size());
            for (const auto& cache : caches)
                isolatedCaches.uncheckedAppend(CacheInfo { cache.identifier, cache.name.isolatedCopy() });

            m_proxy.postTaskForModeToWorkerGlobalScope([this, protectedThis = WTFMove(protectedThis), caches = WTFMove(isolatedCaches), requestIdentifier](ScriptExecutionContext& context) mutable {
                ASSERT_UNUSED(context, context.isWorkerGlobalScope());
                updateCaches(requestIdentifier, WTFMove(caches));
            }, m_taskMode);
        });
    });
}

static inline Vector<CrossThreadRecordData> recordsDataFromRecords(const Vector<CacheStorageConnection::Record>& records)
{
    Vector<CrossThreadRecordData> recordsData;
    recordsData.reserveInitialCapacity(records.size());
    for (const auto& record : records)
        recordsData.uncheckedAppend(toCrossThreadRecordData(record));
    return recordsData;
}

static inline Vector<CacheStorageConnection::Record> recordsFromRecordsData(Vector<CrossThreadRecordData>&& recordsData)
{
    Vector<CacheStorageConnection::Record> records;
    records.reserveInitialCapacity(recordsData.size());
    for (auto& recordData : recordsData)
        records.uncheckedAppend(fromCrossThreadRecordData(WTFMove(recordData)));
    return records;
}

void WorkerCacheStorageConnection::doRetrieveRecords(uint64_t requestIdentifier, uint64_t cacheIdentifier)
{
    m_proxy.postTaskToLoader([this, protectedThis = makeRef(*this), requestIdentifier, cacheIdentifier](ScriptExecutionContext&) mutable {
        ASSERT(isMainThread());
        ASSERT(m_mainThreadConnection);

        m_mainThreadConnection->retrieveRecords(cacheIdentifier, [this, protectedThis = WTFMove(protectedThis), requestIdentifier](Vector<Record>&& records) mutable {
            m_proxy.postTaskForModeToWorkerGlobalScope([this, protectedThis = WTFMove(protectedThis), recordsData = recordsDataFromRecords(records), requestIdentifier](ScriptExecutionContext& context) mutable {
                ASSERT_UNUSED(context, context.isWorkerGlobalScope());
                updateRecords(requestIdentifier, recordsFromRecordsData(WTFMove(recordsData)));
            }, m_taskMode);
        });
    });
}

void WorkerCacheStorageConnection::doBatchDeleteOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, const ResourceRequest& request, CacheQueryOptions&& options)
{
    m_proxy.postTaskToLoader([this, protectedThis = makeRef(*this), requestIdentifier, cacheIdentifier, request = request.isolatedCopy(), options = options.isolatedCopy()](ScriptExecutionContext&) mutable {
        ASSERT(isMainThread());
        ASSERT(m_mainThreadConnection);

        m_mainThreadConnection->batchDeleteOperation(cacheIdentifier, request, WTFMove(options), [this, protectedThis = WTFMove(protectedThis), requestIdentifier](Vector<uint64_t>&& records, Error error) mutable {

            m_proxy.postTaskForModeToWorkerGlobalScope([this, protectedThis = WTFMove(protectedThis), records = WTFMove(records), error, requestIdentifier](ScriptExecutionContext& context) mutable {
                ASSERT_UNUSED(context, context.isWorkerGlobalScope());
                deleteRecordsCompleted(requestIdentifier, WTFMove(records), error);
            }, m_taskMode);
        });
    });
}

void WorkerCacheStorageConnection::doBatchPutOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, Vector<Record>&& records)
{
    m_proxy.postTaskToLoader([this, protectedThis = makeRef(*this), requestIdentifier, cacheIdentifier, recordsData = recordsDataFromRecords(records)](ScriptExecutionContext&) mutable {
        ASSERT(isMainThread());
        ASSERT(m_mainThreadConnection);

        m_mainThreadConnection->batchPutOperation(cacheIdentifier, recordsFromRecordsData(WTFMove(recordsData)), [this, protectedThis = WTFMove(protectedThis), requestIdentifier](Vector<uint64_t>&& records, Error error) mutable {

            m_proxy.postTaskForModeToWorkerGlobalScope([this, protectedThis = WTFMove(protectedThis), records = WTFMove(records), error, requestIdentifier](ScriptExecutionContext& context) mutable {
                ASSERT_UNUSED(context, context.isWorkerGlobalScope());
                putRecordsCompleted(requestIdentifier, WTFMove(records), error);
            }, m_taskMode);
        });
    });
}

} // namespace WebCore
