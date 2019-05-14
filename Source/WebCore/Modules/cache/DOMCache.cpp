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
#include "DOMCache.h"

#include "CacheQueryOptions.h"
#include "FetchResponse.h"
#include "HTTPParsers.h"
#include "JSFetchRequest.h"
#include "JSFetchResponse.h"
#include "ReadableStreamChunk.h"
#include "ScriptExecutionContext.h"
#include <wtf/CompletionHandler.h>
#include <wtf/URL.h>

namespace WebCore {
using namespace WebCore::DOMCacheEngine;

DOMCache::DOMCache(ScriptExecutionContext& context, String&& name, uint64_t identifier, Ref<CacheStorageConnection>&& connection)
    : ActiveDOMObject(&context)
    , m_name(WTFMove(name))
    , m_identifier(identifier)
    , m_connection(WTFMove(connection))
{
    suspendIfNeeded();
    m_connection->reference(m_identifier);
}

DOMCache::~DOMCache()
{
    if (!m_isStopped)
        m_connection->dereference(m_identifier);
}

void DOMCache::match(RequestInfo&& info, CacheQueryOptions&& options, Ref<DeferredPromise>&& promise)
{
    doMatch(WTFMove(info), WTFMove(options), [promise = WTFMove(promise)](ExceptionOr<FetchResponse*>&& result) mutable {
        if (result.hasException()) {
            promise->reject(result.releaseException());
            return;
        }
        if (!result.returnValue()) {
            promise->resolve();
            return;
        }
        promise->resolve<IDLInterface<FetchResponse>>(*result.returnValue());
    });
}

void DOMCache::doMatch(RequestInfo&& info, CacheQueryOptions&& options, MatchCallback&& callback)
{
    if (UNLIKELY(!scriptExecutionContext()))
        return;

    auto requestOrException = requestFromInfo(WTFMove(info), options.ignoreMethod);
    if (requestOrException.hasException()) {
        callback(nullptr);
        return;
    }
    auto request = requestOrException.releaseReturnValue();

    queryCache(request.get(), WTFMove(options), [this, callback = WTFMove(callback)](ExceptionOr<Vector<CacheStorageRecord>>&& result) mutable {
        if (result.hasException()) {
            callback(result.releaseException());
            return;
        }
        if (result.returnValue().isEmpty()) {
            callback(nullptr);
            return;
        }
        callback(result.returnValue()[0].response->clone(*scriptExecutionContext()).releaseReturnValue().ptr());
    });
}

Vector<Ref<FetchResponse>> DOMCache::cloneResponses(const Vector<CacheStorageRecord>& records)
{
    auto& context = *scriptExecutionContext();
    return WTF::map(records, [&context] (const auto& record) {
        return record.response->clone(context).releaseReturnValue();
    });
}

void DOMCache::matchAll(Optional<RequestInfo>&& info, CacheQueryOptions&& options, MatchAllPromise&& promise)
{
    if (UNLIKELY(!scriptExecutionContext()))
        return;

    RefPtr<FetchRequest> request;
    if (info) {
        auto requestOrException = requestFromInfo(WTFMove(info.value()), options.ignoreMethod);
        if (requestOrException.hasException()) {
            promise.resolve({ });
            return;
        }
        request = requestOrException.releaseReturnValue();
    }

    if (!request) {
        retrieveRecords(URL { }, [this, promise = WTFMove(promise)](Optional<Exception>&& exception) mutable {
            if (exception) {
                promise.reject(WTFMove(exception.value()));
                return;
            }
            promise.resolve(cloneResponses(m_records));
        });
        return;
    }
    queryCache(request.releaseNonNull(), WTFMove(options), [this, promise = WTFMove(promise)](ExceptionOr<Vector<CacheStorageRecord>>&& result) mutable {
        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }
        promise.resolve(cloneResponses(result.releaseReturnValue()));
    });
}

void DOMCache::add(RequestInfo&& info, DOMPromiseDeferred<void>&& promise)
{
    addAll(Vector<RequestInfo> { WTFMove(info) }, WTFMove(promise));
}

static inline bool hasResponseVaryStarHeaderValue(const FetchResponse& response)
{
    auto varyValue = response.headers().internalHeaders().get(WebCore::HTTPHeaderName::Vary);
    bool hasStar = false;
    varyValue.split(',', [&](StringView view) {
        if (!hasStar && stripLeadingAndTrailingHTTPSpaces(view) == "*")
            hasStar = true;
    });
    return hasStar;
}

class FetchTasksHandler : public RefCounted<FetchTasksHandler> {
public:
    static Ref<FetchTasksHandler> create(Ref<DOMCache>&& domCache, CompletionHandler<void(ExceptionOr<Vector<Record>>&&)>&& callback) { return adoptRef(*new FetchTasksHandler(WTFMove(domCache), WTFMove(callback))); }

    ~FetchTasksHandler()
    {
        if (m_callback)
            m_callback(WTFMove(m_records));
    }

    const Vector<Record>& records() const { return m_records; }

    size_t addRecord(Record&& record)
    {
        ASSERT(!isDone());
        m_records.append(WTFMove(record));
        return m_records.size() - 1;
    }

    void addResponseBody(size_t position, FetchResponse& response, DOMCacheEngine::ResponseBody&& data)
    {
        ASSERT(!isDone());
        auto& record = m_records[position];
        record.responseBodySize = m_domCache->connection().computeRecordBodySize(response, data);
        record.responseBody = WTFMove(data);
    }

    bool isDone() const { return !m_callback; }

    void error(Exception&& exception)
    {
        if (auto callback = WTFMove(m_callback))
            callback(WTFMove(exception));
    }

private:
    FetchTasksHandler(Ref<DOMCache>&& domCache, CompletionHandler<void(ExceptionOr<Vector<Record>>&&)>&& callback)
        : m_domCache(WTFMove(domCache))
        , m_callback(WTFMove(callback))
    {
    }

    Ref<DOMCache> m_domCache;
    Vector<Record> m_records;
    CompletionHandler<void(ExceptionOr<Vector<Record>>&&)> m_callback;
};

ExceptionOr<Ref<FetchRequest>> DOMCache::requestFromInfo(RequestInfo&& info, bool ignoreMethod)
{
    RefPtr<FetchRequest> request;
    if (WTF::holds_alternative<RefPtr<FetchRequest>>(info)) {
        request = WTF::get<RefPtr<FetchRequest>>(info).releaseNonNull();
        if (request->method() != "GET" && !ignoreMethod)
            return Exception { TypeError, "Request method is not GET"_s };
    } else
        request = FetchRequest::create(*scriptExecutionContext(), WTFMove(info), { }).releaseReturnValue();

    if (!protocolIsInHTTPFamily(request->url()))
        return Exception { TypeError, "Request url is not HTTP/HTTPS"_s };

    return request.releaseNonNull();
}

void DOMCache::addAll(Vector<RequestInfo>&& infos, DOMPromiseDeferred<void>&& promise)
{
    if (UNLIKELY(!scriptExecutionContext()))
        return;

    Vector<Ref<FetchRequest>> requests;
    requests.reserveInitialCapacity(infos.size());
    for (auto& info : infos) {
        bool ignoreMethod = false;
        auto requestOrException = requestFromInfo(WTFMove(info), ignoreMethod);
        if (requestOrException.hasException()) {
            promise.reject(requestOrException.releaseException());
            return;
        }
        requests.uncheckedAppend(requestOrException.releaseReturnValue());
    }

    auto taskHandler = FetchTasksHandler::create(*this, [this, promise = WTFMove(promise)](ExceptionOr<Vector<Record>>&& result) mutable {
        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }
        batchPutOperation(result.releaseReturnValue(), [promise = WTFMove(promise)](ExceptionOr<void>&& result) mutable {
            promise.settle(WTFMove(result));
        });
    });

    for (auto& request : requests) {
        auto& requestReference = request.get();
        FetchResponse::fetch(*scriptExecutionContext(), requestReference, [this, request = WTFMove(request), taskHandler = taskHandler.copyRef()](ExceptionOr<FetchResponse&>&& result) mutable {

            if (taskHandler->isDone())
                return;

            if (result.hasException()) {
                taskHandler->error(result.releaseException());
                return;
            }

            auto& response = result.releaseReturnValue();

            if (!response.ok()) {
                taskHandler->error(Exception { TypeError, "Response is not OK"_s });
                return;
            }

            if (hasResponseVaryStarHeaderValue(response)) {
                taskHandler->error(Exception { TypeError, "Response has a '*' Vary header value"_s });
                return;
            }

            if (response.status() == 206) {
                taskHandler->error(Exception { TypeError, "Response is a 206 partial"_s });
                return;
            }

            CacheQueryOptions options;
            for (const auto& record : taskHandler->records()) {
                if (DOMCacheEngine::queryCacheMatch(request->resourceRequest(), record.request, record.response, options)) {
                    taskHandler->error(Exception { InvalidStateError, "addAll cannot store several matching requests"_s});
                    return;
                }
            }
            size_t recordPosition = taskHandler->addRecord(toConnectionRecord(request.get(), response, nullptr));

            response.consumeBodyReceivedByChunk([taskHandler = WTFMove(taskHandler), recordPosition, data = SharedBuffer::create(), response = makeRef(response)] (ExceptionOr<ReadableStreamChunk*>&& result) mutable {
                if (taskHandler->isDone())
                    return;

                if (result.hasException()) {
                    taskHandler->error(result.releaseException());
                    return;
                }

                if (auto chunk = result.returnValue())
                    data->append(reinterpret_cast<const char*>(chunk->data), chunk->size);
                else
                    taskHandler->addResponseBody(recordPosition, response, WTFMove(data));
            });
        });
    }
}

void DOMCache::putWithResponseData(DOMPromiseDeferred<void>&& promise, Ref<FetchRequest>&& request, Ref<FetchResponse>&& response, ExceptionOr<RefPtr<SharedBuffer>>&& responseBody)
{
    if (responseBody.hasException()) {
        promise.reject(responseBody.releaseException());
        return;
    }

    DOMCacheEngine::ResponseBody body;
    if (auto buffer = responseBody.releaseReturnValue())
        body = buffer.releaseNonNull();
    batchPutOperation(request.get(), response.get(), WTFMove(body), [promise = WTFMove(promise)](ExceptionOr<void>&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void DOMCache::put(RequestInfo&& info, Ref<FetchResponse>&& response, DOMPromiseDeferred<void>&& promise)
{
    if (UNLIKELY(!scriptExecutionContext()))
        return;

    bool ignoreMethod = false;
    auto requestOrException = requestFromInfo(WTFMove(info), ignoreMethod);
    if (requestOrException.hasException()) {
        promise.reject(requestOrException.releaseException());
        return;
    }
    auto request = requestOrException.releaseReturnValue();

    if (auto exception = response->loadingException()) {
        promise.reject(*exception);
        return;
    }

    if (hasResponseVaryStarHeaderValue(response.get())) {
        promise.reject(Exception { TypeError, "Response has a '*' Vary header value"_s });
        return;
    }

    if (response->status() == 206) {
        promise.reject(Exception { TypeError, "Response is a 206 partial"_s });
        return;
    }

    if (response->isDisturbedOrLocked()) {
        promise.reject(Exception { TypeError, "Response is disturbed or locked"_s });
        return;
    }

    if (response->isBlobFormData()) {
        promise.reject(Exception { NotSupportedError, "Not implemented"_s });
        return;
    }

    // FIXME: for efficiency, we should load blobs directly instead of going through the readableStream path.
    if (response->isBlobBody())
        response->readableStream(*scriptExecutionContext()->execState());

    if (response->isBodyReceivedByChunk()) {
        auto& responseRef = response.get();
        responseRef.consumeBodyReceivedByChunk([promise = WTFMove(promise), request = WTFMove(request), response = WTFMove(response), data = SharedBuffer::create(), pendingActivity = makePendingActivity(*this), this](auto&& result) mutable {

            if (result.hasException()) {
                this->putWithResponseData(WTFMove(promise), WTFMove(request), WTFMove(response), result.releaseException().isolatedCopy());
                return;
            }

            if (auto chunk = result.returnValue())
                data->append(reinterpret_cast<const char*>(chunk->data), chunk->size);
            else
                this->putWithResponseData(WTFMove(promise), WTFMove(request), WTFMove(response), RefPtr<SharedBuffer> { WTFMove(data) });
        });
        return;
    }

    batchPutOperation(request.get(), response.get(), response->consumeBody(), [promise = WTFMove(promise)](ExceptionOr<void>&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void DOMCache::remove(RequestInfo&& info, CacheQueryOptions&& options, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    if (UNLIKELY(!scriptExecutionContext()))
        return;

    auto requestOrException = requestFromInfo(WTFMove(info), options.ignoreMethod);
    if (requestOrException.hasException()) {
        promise.resolve(false);
        return;
    }

    batchDeleteOperation(requestOrException.releaseReturnValue(), WTFMove(options), [promise = WTFMove(promise)](ExceptionOr<bool>&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

static inline Ref<FetchRequest> copyRequestRef(const CacheStorageRecord& record)
{
    return record.request.copyRef();
}

void DOMCache::keys(Optional<RequestInfo>&& info, CacheQueryOptions&& options, KeysPromise&& promise)
{
    if (UNLIKELY(!scriptExecutionContext()))
        return;

    RefPtr<FetchRequest> request;
    if (info) {
        auto requestOrException = requestFromInfo(WTFMove(info.value()), options.ignoreMethod);
        if (requestOrException.hasException()) {
            promise.resolve(Vector<Ref<FetchRequest>> { });
            return;
        }
        request = requestOrException.releaseReturnValue();
    }

    if (!request) {
        retrieveRecords(URL { }, [this, promise = WTFMove(promise)](Optional<Exception>&& exception) mutable {
            if (exception) {
                promise.reject(WTFMove(exception.value()));
                return;
            }
            promise.resolve(WTF::map(m_records, copyRequestRef));
        });
        return;
    }

    queryCache(request.releaseNonNull(), WTFMove(options), [promise = WTFMove(promise)](ExceptionOr<Vector<CacheStorageRecord>>&& result) mutable {
        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }

        promise.resolve(WTF::map(result.releaseReturnValue(), copyRequestRef));
    });
}

void DOMCache::retrieveRecords(const URL& url, WTF::Function<void(Optional<Exception>&&)>&& callback)
{
    URL retrieveURL = url;
    retrieveURL.removeQueryAndFragmentIdentifier();

    m_connection->retrieveRecords(m_identifier, retrieveURL, [this, pendingActivity = makePendingActivity(*this), callback = WTFMove(callback)](RecordsOrError&& result) {
        if (m_isStopped)
            return;

        if (!result.has_value()) {
            callback(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), result.error()));
            return;
        }

        updateRecords(WTFMove(result.value()));
        callback(WTF::nullopt);
    });
}

void DOMCache::queryCache(Ref<FetchRequest>&& request, CacheQueryOptions&& options, WTF::Function<void(ExceptionOr<Vector<CacheStorageRecord>>&&)>&& callback)
{
    auto url = request->url();
    retrieveRecords(url, [this, request = WTFMove(request), options = WTFMove(options), callback = WTFMove(callback)](Optional<Exception>&& exception) mutable {
        if (exception) {
            callback(WTFMove(exception.value()));
            return;
        }
        callback(queryCacheWithTargetStorage(request.get(), options, m_records));
    });
}

static inline bool queryCacheMatch(const FetchRequest& request, const FetchRequest& cachedRequest, const ResourceResponse& cachedResponse, const CacheQueryOptions& options)
{
    // We need to pass the resource request with all correct headers hence why we call resourceRequest().
    return DOMCacheEngine::queryCacheMatch(request.resourceRequest(), cachedRequest.resourceRequest(), cachedResponse, options);
}

Vector<CacheStorageRecord> DOMCache::queryCacheWithTargetStorage(const FetchRequest& request, const CacheQueryOptions& options, const Vector<CacheStorageRecord>& targetStorage)
{
    if (!options.ignoreMethod && request.method() != "GET")
        return { };

    Vector<CacheStorageRecord> records;
    for (auto& record : targetStorage) {
        if (queryCacheMatch(request, record.request.get(), record.response->resourceResponse(), options))
            records.append({ record.identifier, record.updateResponseCounter, record.request.copyRef(), record.response.copyRef() });
    }
    return records;
}

void DOMCache::batchDeleteOperation(const FetchRequest& request, CacheQueryOptions&& options, WTF::Function<void(ExceptionOr<bool>&&)>&& callback)
{
    m_connection->batchDeleteOperation(m_identifier, request.internalRequest(), WTFMove(options), [this, pendingActivity = makePendingActivity(*this), callback = WTFMove(callback)](RecordIdentifiersOrError&& result) {
        if (m_isStopped)
            return;

        if (!result.has_value()) {
            callback(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), result.error()));
            return;
        }
        callback(!result.value().isEmpty());
    });
}

Record DOMCache::toConnectionRecord(const FetchRequest& request, FetchResponse& response, DOMCacheEngine::ResponseBody&& responseBody)
{
    auto cachedResponse = response.resourceResponse();
    ResourceRequest cachedRequest = request.internalRequest();
    cachedRequest.setHTTPHeaderFields(request.headers().internalHeaders());

    ASSERT(!cachedRequest.isNull());
    ASSERT(!cachedResponse.isNull());

    auto sizeWithPadding = response.bodySizeWithPadding();
    if (!sizeWithPadding) {
        sizeWithPadding = m_connection->computeRecordBodySize(response, responseBody);
        response.setBodySizeWithPadding(sizeWithPadding);
    }

    return { 0, 0,
        request.headers().guard(), WTFMove(cachedRequest), request.fetchOptions(), request.internalRequestReferrer(),
        response.headers().guard(), WTFMove(cachedResponse), WTFMove(responseBody), sizeWithPadding
    };
}

void DOMCache::batchPutOperation(const FetchRequest& request, FetchResponse& response, DOMCacheEngine::ResponseBody&& responseBody, WTF::Function<void(ExceptionOr<void>&&)>&& callback)
{
    Vector<Record> records;
    records.append(toConnectionRecord(request, response, WTFMove(responseBody)));

    batchPutOperation(WTFMove(records), WTFMove(callback));
}

void DOMCache::batchPutOperation(Vector<Record>&& records, WTF::Function<void(ExceptionOr<void>&&)>&& callback)
{
    m_connection->batchPutOperation(m_identifier, WTFMove(records), [this, pendingActivity = makePendingActivity(*this), callback = WTFMove(callback)](RecordIdentifiersOrError&& result) {
        if (m_isStopped)
            return;
        if (!result.has_value()) {
            callback(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), result.error()));
            return;
        }
        callback({ });
    });
}

void DOMCache::updateRecords(Vector<Record>&& records)
{
    ASSERT(scriptExecutionContext());
    Vector<CacheStorageRecord> newRecords;

    for (auto& record : records) {
        size_t index = m_records.findMatching([&](const auto& item) { return item.identifier == record.identifier; });
        if (index != notFound) {
            auto& current = m_records[index];
            if (current.updateResponseCounter != record.updateResponseCounter) {
                auto response = FetchResponse::create(*scriptExecutionContext(), WTF::nullopt, record.responseHeadersGuard, WTFMove(record.response));
                response->setBodyData(WTFMove(record.responseBody), record.responseBodySize);

                current.response = WTFMove(response);
                current.updateResponseCounter = record.updateResponseCounter;
            }
            newRecords.append(WTFMove(current));
        } else {
            auto requestHeaders = FetchHeaders::create(record.requestHeadersGuard, HTTPHeaderMap { record.request.httpHeaderFields() });
            auto request = FetchRequest::create(*scriptExecutionContext(), WTF::nullopt, WTFMove(requestHeaders),  WTFMove(record.request), WTFMove(record.options), WTFMove(record.referrer));

            auto response = FetchResponse::create(*scriptExecutionContext(), WTF::nullopt, record.responseHeadersGuard, WTFMove(record.response));
            response->setBodyData(WTFMove(record.responseBody), record.responseBodySize);

            newRecords.append(CacheStorageRecord { record.identifier, record.updateResponseCounter, WTFMove(request), WTFMove(response) });
        }
    }
    m_records = WTFMove(newRecords);
}

void DOMCache::stop()
{
    if (m_isStopped)
        return;
    m_isStopped = true;
    m_connection->dereference(m_identifier);
}

const char* DOMCache::activeDOMObjectName() const
{
    return "Cache";
}

bool DOMCache::canSuspendForDocumentSuspension() const
{
    return m_records.isEmpty() && !hasPendingActivity();
}


} // namespace WebCore
