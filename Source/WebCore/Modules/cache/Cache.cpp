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
#include "Cache.h"

#include "CacheQueryOptions.h"
#include "FetchResponse.h"
#include "HTTPParsers.h"
#include "JSFetchRequest.h"
#include "JSFetchResponse.h"
#include "ScriptExecutionContext.h"
#include "URL.h"

namespace WebCore {

Cache::Cache(ScriptExecutionContext& context, String&& name, uint64_t identifier, Ref<CacheStorageConnection>&& connection)
    : ActiveDOMObject(&context)
    , m_name(WTFMove(name))
    , m_identifier(identifier)
    , m_connection(WTFMove(connection))
{
    suspendIfNeeded();
}

Cache::~Cache()
{
}

void Cache::match(RequestInfo&& info, CacheQueryOptions&& options, Ref<DeferredPromise>&& promise)
{
    doMatch(WTFMove(info), WTFMove(options), WTFMove(promise), MatchType::OnlyFirst);
}

void Cache::matchAll(std::optional<RequestInfo>&& info, CacheQueryOptions&& options, Ref<DeferredPromise>&& promise)
{
    doMatch(WTFMove(info), WTFMove(options), WTFMove(promise), MatchType::All);
}

void Cache::doMatch(std::optional<RequestInfo>&& info, CacheQueryOptions&& options, Ref<DeferredPromise>&& promise, MatchType matchType)
{
    RefPtr<FetchRequest> request;
    if (info) {
        if (WTF::holds_alternative<RefPtr<FetchRequest>>(info.value())) {
            request = WTF::get<RefPtr<FetchRequest>>(info.value()).releaseNonNull();
            if (request->method() != "GET" && !options.ignoreMethod) {
                if (matchType == MatchType::OnlyFirst) {
                    promise->resolve();
                    return;
                }
                promise->resolve<IDLSequence<IDLInterface<FetchResponse>>>(Vector<Ref<FetchResponse>> { });
                return;
            }
        } else {
            if (UNLIKELY(!scriptExecutionContext()))
                return;
            request = FetchRequest::create(*scriptExecutionContext(), WTFMove(info.value()), { }).releaseReturnValue();
        }
    }

    if (!request) {
        ASSERT(matchType == MatchType::All);
        retrieveRecords([this, promise = WTFMove(promise)]() {
            Vector<Ref<FetchResponse>> responses;
            responses.reserveInitialCapacity(m_records.size());
            for (auto& record : m_records)
                responses.uncheckedAppend(record.response->cloneForJS());
            promise->resolve<IDLSequence<IDLInterface<FetchResponse>>>(responses);
        });
        return;
    }
    queryCache(request.releaseNonNull(), WTFMove(options), [matchType, promise = WTFMove(promise)](const Vector<CacheStorageRecord>& records) mutable {
        if (matchType == MatchType::OnlyFirst) {
            if (records.size()) {
                promise->resolve<IDLInterface<FetchResponse>>(records[0].response->cloneForJS());
                return;
            }
            promise->resolve();
            return;
        }

        Vector<Ref<FetchResponse>> responses;
        responses.reserveInitialCapacity(records.size());
        for (auto& record : records)
            responses.uncheckedAppend(record.response->cloneForJS());
        promise->resolve<IDLSequence<IDLInterface<FetchResponse>>>(responses);
    });
}

void Cache::add(RequestInfo&&, DOMPromiseDeferred<void>&& promise)
{
    promise.reject(Exception { NotSupportedError, ASCIILiteral("Not implemented")});
}

void Cache::addAll(Vector<RequestInfo>&&, DOMPromiseDeferred<void>&& promise)
{
    promise.reject(Exception { NotSupportedError, ASCIILiteral("Not implemented")});
}

void Cache::put(RequestInfo&& info, Ref<FetchResponse>&& response, DOMPromiseDeferred<void>&& promise)
{
    RefPtr<FetchRequest> request;
    if (WTF::holds_alternative<RefPtr<FetchRequest>>(info)) {
        request = WTF::get<RefPtr<FetchRequest>>(info).releaseNonNull();
        if (request->method() != "GET") {
            promise.reject(Exception { TypeError, ASCIILiteral("Request method is not GET") });
            return;
        }
    } else {
        if (UNLIKELY(!scriptExecutionContext()))
            return;
        request = FetchRequest::create(*scriptExecutionContext(), WTFMove(info), { }).releaseReturnValue();
    }

    if (!protocolIsInHTTPFamily(request->url())) {
        promise.reject(Exception { TypeError, ASCIILiteral("Request url is not HTTP/HTTPS") });
        return;
    }

    // FIXME: This is inefficient, we should be able to split and trim whitespaces at the same time.
    auto varyValue = response->headers().internalHeaders().get(WebCore::HTTPHeaderName::Vary);
    Vector<String> varyHeaderNames;
    varyValue.split(',', false, varyHeaderNames);
    for (auto& name : varyHeaderNames) {
        if (stripLeadingAndTrailingHTTPSpaces(name) == "*") {
            promise.reject(Exception { TypeError, ASCIILiteral("Response has a '*' Vary header value") });
            return;
        }
    }

    if (response->isDisturbed()) {
        promise.reject(Exception { TypeError, ASCIILiteral("Response is disturbed or locked") });
        return;
    }

    if (response->status() == 206) {
        promise.reject(Exception { TypeError, ASCIILiteral("Response is a 206 partial") });
        return;
    }

    // FIXME: Add support for being loaded responses.
    if (response->isLoading()) {
        promise.reject(Exception { NotSupportedError, ASCIILiteral("Caching a loading Response is not yet supported") });
        return;
    }

    // FIXME: Add support for ReadableStream.
    if (response->isReadableStreamBody()) {
        promise.reject(Exception { NotSupportedError, ASCIILiteral("Caching a Response with data stored in a ReadableStream is not yet supported") });
        return;
    }

    batchPutOperation(*request, response.get(), [promise = WTFMove(promise)](ExceptionOr<void>&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void Cache::remove(RequestInfo&& info, CacheQueryOptions&& options, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    RefPtr<FetchRequest> request;
    if (WTF::holds_alternative<RefPtr<FetchRequest>>(info)) {
        request = WTF::get<RefPtr<FetchRequest>>(info).releaseNonNull();
        if (request->method() != "GET" && !options.ignoreMethod) {
            promise.resolve(false);
            return;
        }
    } else {
        if (UNLIKELY(!scriptExecutionContext()))
            return;
        request = FetchRequest::create(*scriptExecutionContext(), WTFMove(info), { }).releaseReturnValue();
    }

    batchDeleteOperation(*request, WTFMove(options), [promise = WTFMove(promise)](ExceptionOr<bool>&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void Cache::keys(std::optional<RequestInfo>&& info, CacheQueryOptions&& options, KeysPromise&& promise)
{
    RefPtr<FetchRequest> request;
    if (info) {
        if (WTF::holds_alternative<RefPtr<FetchRequest>>(info.value())) {
            request = WTF::get<RefPtr<FetchRequest>>(info.value()).releaseNonNull();
            if (request->method() != "GET" && !options.ignoreMethod) {
                promise.resolve(Vector<Ref<FetchRequest>> { });
                return;
            }
        } else {
            if (UNLIKELY(!scriptExecutionContext()))
                return;
            request = FetchRequest::create(*scriptExecutionContext(), WTFMove(info.value()), { }).releaseReturnValue();
        }
    }

    if (!request) {
        retrieveRecords([this, promise = WTFMove(promise)]() mutable {
            Vector<Ref<FetchRequest>> requests;
            requests.reserveInitialCapacity(m_records.size());
            for (auto& record : m_records)
                requests.uncheckedAppend(record.request.copyRef());
            promise.resolve(requests);
        });
        return;
    }

    queryCache(request.releaseNonNull(), WTFMove(options), [promise = WTFMove(promise)](const Vector<CacheStorageRecord>& records) mutable {
        Vector<Ref<FetchRequest>> requests;
        requests.reserveInitialCapacity(records.size());
        for (auto& record : records)
            requests.uncheckedAppend(record.request.copyRef());
        promise.resolve(requests);
    });
}

void Cache::retrieveRecords(WTF::Function<void()>&& callback)
{
    setPendingActivity(this);
    m_connection->retrieveRecords(m_identifier, [this, callback = WTFMove(callback)](Vector<CacheStorageConnection::Record>&& records) {
        if (!m_isStopped) {
            updateRecords(WTFMove(records));
            callback();
        }
        unsetPendingActivity(this);
    });
}

void Cache::queryCache(Ref<FetchRequest>&& request, CacheQueryOptions&& options, WTF::Function<void(const Vector<CacheStorageRecord>&)>&& callback)
{
    retrieveRecords([this, request = WTFMove(request), options = WTFMove(options), callback = WTFMove(callback)]() mutable {
        callback(queryCacheWithTargetStorage(request.get(), options, m_records));
    });
}

static inline bool queryCacheMatch(const FetchRequest& request, const FetchRequest& cachedRequest, const ResourceResponse& cachedResponse, const CacheQueryOptions& options)
{
    // We need to pass the resource request with all correct headers hence why we call resourceRequest().
    return CacheStorageConnection::queryCacheMatch(request.resourceRequest(), cachedRequest.resourceRequest(), cachedResponse, options);
}

Vector<CacheStorageRecord> Cache::queryCacheWithTargetStorage(const FetchRequest& request, const CacheQueryOptions& options, const Vector<CacheStorageRecord>& targetStorage)
{
    if (!options.ignoreMethod && request.method() != "GET")
        return { };

    Vector<CacheStorageRecord> records;
    for (auto& record : targetStorage) {
        if (queryCacheMatch(request, record.request.get(), record.response->resourceResponse(), options))
            records.append({ record.identifier, record.request.copyRef(), record.response.copyRef() });
    }
    return records;
}

void Cache::batchDeleteOperation(const FetchRequest& request, CacheQueryOptions&& options, WTF::Function<void(ExceptionOr<bool>&&)>&& callback)
{
    setPendingActivity(this);
    m_connection->batchDeleteOperation(m_identifier, request.internalRequest(), WTFMove(options), [this, callback = WTFMove(callback)](Vector<uint64_t>&& records, CacheStorageConnection::Error error) {
        if (!m_isStopped)
            callback(CacheStorageConnection::exceptionOrResult(!records.isEmpty(), error));

        unsetPendingActivity(this);
    });
}

static inline CacheStorageConnection::Record toConnectionRecord(const FetchRequest& request, FetchResponse& response)
{
    // FIXME: Add a setHTTPHeaderFields on ResourceResponseBase.
    ResourceResponse cachedResponse = response.resourceResponse();
    for (auto& header : response.headers().internalHeaders())
        cachedResponse.setHTTPHeaderField(header.key, header.value);

    ResourceRequest cachedRequest = request.internalRequest();
    cachedRequest.setHTTPHeaderFields(request.headers().internalHeaders());

    return { 0,
        request.headers().guard(), WTFMove(cachedRequest), request.fetchOptions(), request.internalRequestReferrer(),
        response.headers().guard(), WTFMove(cachedResponse), response.consumeBody()
    };
}

void Cache::batchPutOperation(const FetchRequest& request, FetchResponse& response, WTF::Function<void(ExceptionOr<void>&&)>&& callback)
{
    Vector<CacheStorageConnection::Record> records;
    records.append(toConnectionRecord(request, response));

    setPendingActivity(this);
    m_connection->batchPutOperation(m_identifier, WTFMove(records), [this, callback = WTFMove(callback)](Vector<uint64_t>&&, CacheStorageConnection::Error error) {
        if (!m_isStopped)
            callback(CacheStorageConnection::errorToException(error));

        unsetPendingActivity(this);
    });
}

void Cache::updateRecords(Vector<CacheStorageConnection::Record>&& records)
{
    ASSERT(scriptExecutionContext());
    Vector<CacheStorageRecord> newRecords;

    for (auto& record : records) {
        size_t index = m_records.findMatching([&](const auto& item) { return item.identifier == record.identifier; });
        if (index != notFound)
            newRecords.append(WTFMove(m_records[index]));
        else {
            auto requestHeaders = FetchHeaders::create(record.requestHeadersGuard, HTTPHeaderMap { record.request.httpHeaderFields() });
            FetchRequest::InternalRequest internalRequest = { WTFMove(record.request), WTFMove(record.options), WTFMove(record.referrer) };
            auto request = FetchRequest::create(*scriptExecutionContext(), std::nullopt, WTFMove(requestHeaders), WTFMove(internalRequest));

            auto responseHeaders = FetchHeaders::create(record.responseHeadersGuard, HTTPHeaderMap { record.response.httpHeaderFields() });
            auto response = FetchResponse::create(*scriptExecutionContext(), std::nullopt, WTFMove(responseHeaders), WTFMove(record.response));
            response->setBodyData(WTFMove(record.responseBody));

            newRecords.append(CacheStorageRecord { record.identifier, WTFMove(request), WTFMove(response) });
        }
    }
    m_records = WTFMove(newRecords);
}

void Cache::stop()
{
    m_isStopped = true;
}

const char* Cache::activeDOMObjectName() const
{
    return "Cache";
}

bool Cache::canSuspendForDocumentSuspension() const
{
    return m_records.isEmpty() && !hasPendingActivity();
}


} // namespace WebCore
