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
#include "EventLoop.h"
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
    doMatch(WTFMove(info), WTFMove(options), [this, protectedThis = makeRef(*this), promise = WTFMove(promise)](ExceptionOr<RefPtr<FetchResponse>>&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTFMove(promise), result = WTFMove(result)]() mutable {
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
    });
}

static Ref<FetchResponse> createResponse(ScriptExecutionContext& context, const DOMCacheEngine::Record& record)
{
    auto resourceResponse = record.response;
    resourceResponse.setSource(ResourceResponse::Source::DOMCache);
    auto response = FetchResponse::create(context, WTF::nullopt, record.responseHeadersGuard, WTFMove(resourceResponse));
    response->setBodyData(copyResponseBody(record.responseBody), record.responseBodySize);
    return response;
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

    auto request = requestOrException.releaseReturnValue()->resourceRequest();
    queryCache(WTFMove(request), options, ShouldRetrieveResponses::Yes, [this, callback = WTFMove(callback)](auto&& result) mutable {
        if (result.hasException()) {
            callback(result.releaseException());
            return;
        }

        RefPtr<FetchResponse> response;
        if (!result.returnValue().isEmpty())
            response = createResponse(*scriptExecutionContext(), result.returnValue()[0]);

        callback(WTFMove(response));
    });
}

Vector<Ref<FetchResponse>> DOMCache::cloneResponses(const Vector<DOMCacheEngine::Record>& records)
{
    return WTF::map(records, [this] (const auto& record) {
        return createResponse(*scriptExecutionContext(), record);
    });
}

void DOMCache::matchAll(Optional<RequestInfo>&& info, CacheQueryOptions&& options, MatchAllPromise&& promise)
{
    if (UNLIKELY(!scriptExecutionContext()))
        return;

    ResourceRequest resourceRequest;
    if (info) {
        auto requestOrException = requestFromInfo(WTFMove(info.value()), options.ignoreMethod);
        if (requestOrException.hasException()) {
            promise.resolve({ });
            return;
        }
        resourceRequest = requestOrException.releaseReturnValue()->resourceRequest();
    }

    queryCache(WTFMove(resourceRequest), options, ShouldRetrieveResponses::Yes, [this, promise = WTFMove(promise)](auto&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, promise = WTFMove(promise), result = WTFMove(result)]() mutable {
            if (result.hasException()) {
                promise.reject(result.releaseException());
                return;
            }
            promise.resolve(cloneResponses(result.releaseReturnValue()));
        });
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

    if (!request->url().protocolIsInHTTPFamily())
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

    auto taskHandler = FetchTasksHandler::create(*this, [this, protectedThis = makeRef(*this), promise = WTFMove(promise)](ExceptionOr<Vector<Record>>&& result) mutable {
        if (result.hasException()) {
            queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTFMove(promise), exception = result.releaseException()]() mutable {
                promise.reject(WTFMove(exception));
            });
            return;
        }
        batchPutOperation(result.releaseReturnValue(), [this, protectedThis = WTFMove(protectedThis), promise = WTFMove(promise)](ExceptionOr<void>&& result) mutable {
            queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTFMove(promise), result = WTFMove(result)]() mutable {
                promise.settle(WTFMove(result));
            });
        });
    });

    for (auto& request : requests) {
        auto& requestReference = request.get();
        FetchResponse::fetch(*scriptExecutionContext(), requestReference, [this, request = WTFMove(request), taskHandler](ExceptionOr<FetchResponse&>&& result) mutable {

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
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTFMove(promise), exception = responseBody.releaseException()]() mutable {
            promise.reject(WTFMove(exception));
        });
        return;
    }

    DOMCacheEngine::ResponseBody body;
    if (auto buffer = responseBody.releaseReturnValue())
        body = buffer.releaseNonNull();
    batchPutOperation(request.get(), response.get(), WTFMove(body), [this, protectedThis = makeRef(*this), promise = WTFMove(promise)](ExceptionOr<void>&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTFMove(promise), result = WTFMove(result)]() mutable {
            promise.settle(WTFMove(result));
        });
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

    batchPutOperation(request.get(), response.get(), response->consumeBody(), [this, protectedThis = makeRef(*this), promise = WTFMove(promise)](ExceptionOr<void>&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTFMove(promise), result = WTFMove(result)]() mutable {
            promise.settle(WTFMove(result));
        });
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

    batchDeleteOperation(requestOrException.releaseReturnValue(), WTFMove(options), [this, protectedThis = makeRef(*this), promise = WTFMove(promise)](ExceptionOr<bool>&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTFMove(promise), result = WTFMove(result)]() mutable {
            promise.settle(WTFMove(result));
        });
    });
}

static Ref<FetchRequest> createRequest(ScriptExecutionContext& context, const DOMCacheEngine::Record& record)
{
    auto requestHeaders = FetchHeaders::create(record.requestHeadersGuard, HTTPHeaderMap { record.request.httpHeaderFields() });
    return FetchRequest::create(context, WTF::nullopt, WTFMove(requestHeaders),  ResourceRequest { record.request }, FetchOptions { record.options }, String { record.referrer });
}

void DOMCache::keys(Optional<RequestInfo>&& info, CacheQueryOptions&& options, KeysPromise&& promise)
{
    if (UNLIKELY(!scriptExecutionContext()))
        return;

    ResourceRequest resourceRequest;
    if (info) {
        auto requestOrException = requestFromInfo(WTFMove(info.value()), options.ignoreMethod);
        if (requestOrException.hasException()) {
            promise.resolve(Vector<Ref<FetchRequest>> { });
            return;
        }
        resourceRequest = requestOrException.releaseReturnValue()->resourceRequest();
    }

    queryCache(WTFMove(resourceRequest), options, ShouldRetrieveResponses::No, [this, promise = WTFMove(promise)](auto&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, promise = WTFMove(promise), result = WTFMove(result)]() mutable {
            if (result.hasException()) {
                promise.reject(result.releaseException());
                return;
            }

            auto records = result.releaseReturnValue();
            promise.resolve(WTF::map(records, [this](auto& record) {
                return createRequest(*scriptExecutionContext(), record);
            }));
        });
    });
}

void DOMCache::queryCache(ResourceRequest&& request, const CacheQueryOptions& options, ShouldRetrieveResponses shouldRetrieveResponses, RecordsCallback&& callback)
{
    RetrieveRecordsOptions retrieveOptions { WTFMove(request), options.ignoreSearch, options.ignoreMethod, options.ignoreVary, shouldRetrieveResponses == ShouldRetrieveResponses::Yes };
    m_connection->retrieveRecords(m_identifier, retrieveOptions, [this, pendingActivity = makePendingActivity(*this), callback = WTFMove(callback)](auto&& result) mutable {
        if (m_isStopped) {
            callback(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), DOMCacheEngine::Error::Stopped));
            return;
        }

        if (!result.has_value()) {
            callback(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), result.error()));
            return;
        }

        callback(WTFMove(result.value()));
    });

}

void DOMCache::batchDeleteOperation(const FetchRequest& request, CacheQueryOptions&& options, CompletionHandler<void(ExceptionOr<bool>&&)>&& callback)
{
    m_connection->batchDeleteOperation(m_identifier, request.internalRequest(), WTFMove(options), [this, pendingActivity = makePendingActivity(*this), callback = WTFMove(callback)](RecordIdentifiersOrError&& result) mutable {
        if (m_isStopped) {
            callback(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), DOMCacheEngine::Error::Stopped));
            return;
        }

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

void DOMCache::batchPutOperation(const FetchRequest& request, FetchResponse& response, DOMCacheEngine::ResponseBody&& responseBody, CompletionHandler<void(ExceptionOr<void>&&)>&& callback)
{
    Vector<Record> records;
    records.append(toConnectionRecord(request, response, WTFMove(responseBody)));

    batchPutOperation(WTFMove(records), WTFMove(callback));
}

void DOMCache::batchPutOperation(Vector<Record>&& records, CompletionHandler<void(ExceptionOr<void>&&)>&& callback)
{
    m_connection->batchPutOperation(m_identifier, WTFMove(records), [this, pendingActivity = makePendingActivity(*this), callback = WTFMove(callback)](auto&& result) mutable {
        if (m_isStopped) {
            callback(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), DOMCacheEngine::Error::Stopped));
            return;
        }

        if (!result.has_value()) {
            callback(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), result.error()));
            return;
        }
        callback({ });
    });
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

} // namespace WebCore
