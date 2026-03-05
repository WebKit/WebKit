/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#include "BackgroundFetch.h"

#include "BackgroundFetchInformation.h"
#include "BackgroundFetchRecordInformation.h"
#include "CacheQueryOptions.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "DOMCacheEngine.h"
#include "ExceptionData.h"
#include "Logging.h"
#include "RetrieveRecordsOptions.h"
#include "SWServerRegistration.h"
#include "WebCorePersistentCoders.h"
#include <algorithm>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/persistence/PersistentCoders.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BackgroundFetch);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BackgroundFetch::Record);

static const unsigned backgroundFetchCurrentVersion = 1;

BackgroundFetch::BackgroundFetch(SWServerRegistration& registration, const String& identifier, Vector<BackgroundFetchRequest>&& requests, BackgroundFetchOptions&& options, Ref<BackgroundFetchStore>&& store, NotificationCallback&& notificationCallback)
    : m_identifier(identifier)
    , m_options(WTF::move(options))
    , m_registrationKey(registration.key())
    , m_registrationIdentifier(registration.identifier())
    , m_store(WTF::move(store))
    , m_notificationCallback(WTF::move(notificationCallback))
    , m_origin { m_registrationKey.topOrigin(), SecurityOriginData::fromURL(m_registrationKey.scope()) }
{
    size_t index = 0;
    m_records = WTF::map(WTF::move(requests), [&](auto&& request) {
        return Record::create(*this, WTF::move(request), index++);
    });
}

BackgroundFetch::BackgroundFetch(SWServerRegistration& registration, String&& identifier, BackgroundFetchOptions&& options, Ref<BackgroundFetchStore>&& store, NotificationCallback&& notificationCallback, bool pausedFlag)
    : m_identifier(WTF::move(identifier))
    , m_options(WTF::move(options))
    , m_registrationKey(registration.key())
    , m_registrationIdentifier(registration.identifier())
    , m_pausedFlag(pausedFlag)
    , m_store(WTF::move(store))
    , m_notificationCallback(WTF::move(notificationCallback))
    , m_origin { m_registrationKey.topOrigin(), SecurityOriginData::fromURL(m_registrationKey.scope()) }
{
}

BackgroundFetch::~BackgroundFetch()
{
    abort();
}

BackgroundFetchInformation BackgroundFetch::information() const
{
    return { m_registrationIdentifier, m_identifier, m_uploadTotal, m_currentUploadSize, downloadTotal(), m_currentDownloadSize, m_result, m_failureReason, m_recordsAvailableFlag };
}

void BackgroundFetch::match(const RetrieveRecordsOptions& options, MatchBackgroundFetchCallback&& callback)
{
    WebCore::CacheQueryOptions queryOptions { options.ignoreSearch, options.ignoreMethod, options.ignoreVary };

    Vector<Ref<Record>> records;
    for (auto& record : m_records) {
        if (options.request.url().isNull() || record->isMatching(options.request, queryOptions))
            records.append(record);
    }

    callback(WTF::move(records));
}

void BackgroundFetch::pause()
{
    if (m_pausedFlag)
        return;

    m_pausedFlag = true;
    for (auto& record : m_records)
        record->pause();

    doStore([](auto) { });
}

void BackgroundFetch::resume(const CreateLoaderCallback& createLoaderCallback)
{
    if (!m_pausedFlag)
        return;

    m_pausedFlag = false;
    for (auto& record : m_records)
        record->complete(createLoaderCallback);

    doStore([](auto) { });
}

bool BackgroundFetch::abort()
{
    if (m_abortFlag)
        return false;

    m_abortFlag = true;
    m_isActive = false;

    for (auto& record : m_records)
        record->abort();

    updateBackgroundFetchStatus(BackgroundFetchResult::Failure, BackgroundFetchFailureReason::Aborted);
    return true;
}

void BackgroundFetch::perform(const CreateLoaderCallback& createLoaderCallback)
{
    m_currentDownloadSize = 0;
    for (auto& record : m_records)
        record->complete(createLoaderCallback);
}

void BackgroundFetch::storeResponse(size_t index, bool shouldClearResponseBody, ResourceResponse&& response)
{
    UNUSED_PARAM(index);
    ASSERT(index < m_records.size());
    if (!response.isSuccessful()) {
        updateBackgroundFetchStatus(BackgroundFetchResult::Failure, BackgroundFetchFailureReason::BadStatus);
        return;
    }
    if (shouldClearResponseBody) {
        ASSERT(m_currentDownloadSize >= m_records[index]->responseDataSize());
        m_currentDownloadSize -= m_records[index]->responseDataSize();
        m_records[index]->clearResponseDataSize();
    }

    doStore([weakThis = WeakPtr { *this }](auto result) {
        if (weakThis)
            weakThis->handleStoreResult(result);
    }, shouldClearResponseBody ? std::make_optional(index) : std::nullopt);
}

void BackgroundFetch::storeResponseBodyChunk(size_t index, const SharedBuffer& data)
{
    ASSERT(index < m_records.size());
    m_currentDownloadSize += data.size();
    if (downloadTotal() && m_currentDownloadSize >= downloadTotal()) {
        updateBackgroundFetchStatus(BackgroundFetchResult::Failure, BackgroundFetchFailureReason::DownloadTotalExceeded);
        return;
    }

    updateBackgroundFetchStatus(m_result, m_failureReason);
    m_store->storeFetchResponseBodyChunk(m_registrationKey, m_identifier, index, data, [weakThis = WeakPtr { *this }](auto result) {
        if (weakThis)
            weakThis->handleStoreResult(result);
    });
}

void BackgroundFetch::didSendData(uint64_t size)
{
    m_currentUploadSize += size;
    updateBackgroundFetchStatus(m_result, m_failureReason);
}

void BackgroundFetch::didFinishRecord(const ResourceError& error)
{
    if (error.isNull()) {
        recordIsCompleted();
        return;
    }
    // FIXME: We probably want to handle recoverable errors (We could use NetworkStateNotifier). For now, all errors are terminal.
    updateBackgroundFetchStatus(BackgroundFetchResult::Failure, BackgroundFetchFailureReason::FetchError);
}

void BackgroundFetch::handleStoreResult(BackgroundFetchStore::StoreResult result)
{
    switch (result) {
    case BackgroundFetchStore::StoreResult::OK:
        return;
    case BackgroundFetchStore::StoreResult::QuotaError:
        updateBackgroundFetchStatus(BackgroundFetchResult::Failure, BackgroundFetchFailureReason::QuotaExceeded);
        return;
    case BackgroundFetchStore::StoreResult::InternalError:
        updateBackgroundFetchStatus(BackgroundFetchResult::Failure, BackgroundFetchFailureReason::FetchError);
        return;
    }
}

void BackgroundFetch::recordIsCompleted()
{
    if (std::ranges::any_of(m_records, [](auto& record) { return !record->isCompleted(); }))
        return;
    updateBackgroundFetchStatus(BackgroundFetchResult::Success, BackgroundFetchFailureReason::EmptyString);
}

void BackgroundFetch::updateBackgroundFetchStatus(BackgroundFetchResult result, BackgroundFetchFailureReason failureReason)
{
    if (m_result != BackgroundFetchResult::EmptyString)
        return;
    ASSERT(m_failureReason == BackgroundFetchFailureReason::EmptyString);

    m_result = result;
    m_failureReason = failureReason;
    m_isActive = m_result == BackgroundFetchResult::EmptyString && m_failureReason == BackgroundFetchFailureReason::EmptyString;
    m_notificationCallback(*this);
}

void BackgroundFetch::setRecords(Vector<Ref<Record>>&& records)
{
    ASSERT(!m_currentDownloadSize);
    m_records = WTF::move(records);
    for (auto& record : m_records)
        m_currentDownloadSize += record->responseDataSize();
}

void BackgroundFetch::unsetRecordsAvailableFlag()
{
    if (!m_recordsAvailableFlag)
        return;

    m_recordsAvailableFlag = false;
    m_store->clearFetch(m_registrationKey, m_identifier);
    m_notificationCallback(*this);
}

BackgroundFetch::Record::Record(BackgroundFetch& fetch, BackgroundFetchRequest&& request, size_t index)
    : m_fetch(fetch)
    , m_fetchIdentifier(fetch.m_identifier)
    , m_registrationKey(fetch.m_registrationKey)
    , m_request(WTF::move(request))
    , m_index(index)
{
}

BackgroundFetch::Record::~Record()
{
    auto callbacks = std::exchange(m_responseCallbacks, { });
    for (auto& callback : callbacks)
        callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "Record is gone"_s }));

    auto bodyCallbacks = std::exchange(m_responseBodyCallbacks, { });
    for (auto& callback : bodyCallbacks)
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Record is gone"_s }));
}

bool BackgroundFetch::Record::isMatching(const ResourceRequest& request, const CacheQueryOptions& options) const
{
    return DOMCacheEngine::queryCacheMatch(request, m_request.internalRequest, m_response, options);
}

BackgroundFetchRecordInformation BackgroundFetch::Record::information() const
{
    return BackgroundFetchRecordInformation { identifier(), m_request.internalRequest, m_request.options, m_request.guard, m_request.httpHeaders, m_request.referrer };
}

void BackgroundFetch::Record::complete(const CreateLoaderCallback& createLoaderCallback)
{
    ASSERT(!m_loader);

    m_loader = createLoaderCallback(*this, m_request, m_responseDataSize, m_fetch->m_origin);
    if (!m_loader)
        abort();
}

void BackgroundFetch::Record::pause()
{
    if (RefPtr loader = m_loader) {
        loader->abort();
        m_loader = nullptr;
    }
}

void BackgroundFetch::Record::abort()
{
    if (m_isAborted)
        return;

    m_isAborted = true;

    auto callbacks = std::exchange(m_responseCallbacks, { });
    for (auto& callback : callbacks)
        callback(makeUnexpected(ExceptionData { ExceptionCode::AbortError, "Background fetch was aborted"_s }));

    auto bodyCallbacks = std::exchange(m_responseBodyCallbacks, { });
    for (auto& callback : bodyCallbacks)
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Background fetch was aborted"_s, ResourceError::Type::Cancellation }));

    if (RefPtr loader = m_loader) {
        loader->abort();
        m_loader = nullptr;
    }
}

void BackgroundFetch::Record::didSendData(uint64_t size)
{
    if (RefPtr fetch = m_fetch.get())
        fetch->didSendData(size);
}

// https://wicg.github.io/background-fetch/#extract-content-range-values
static ParsedContentRange extractContentRangeValues(const ResourceResponse& response)
{
    return ParsedContentRange(response.httpHeaderField(HTTPHeaderName::ContentRange));
}

// https://wicg.github.io/background-fetch/#validate-a-partial-response
static bool validatePartialResponse(size_t rangeStart, const ResourceResponse& partialResponse, const ResourceResponse& previousResponse)
{
    auto parsedContentRange = extractContentRangeValues(partialResponse);
    if (!parsedContentRange.isValid())
        return false;

    if (static_cast<size_t>(parsedContentRange.firstBytePosition()) != rangeStart)
        return false;

    auto previousETag = previousResponse.httpHeaderField(HTTPHeaderName::ETag);
    if (!previousETag.isEmpty() && previousETag != partialResponse.httpHeaderField(HTTPHeaderName::ETag))
        return false;

    auto previousLastMotified = previousResponse.httpHeaderField(HTTPHeaderName::LastModified);
    if (!previousLastMotified.isEmpty() && previousLastMotified != partialResponse.httpHeaderField(HTTPHeaderName::LastModified))
        return false;

    if (previousResponse.httpStatusCode() == 206) {
        auto parsedPreviousContentRange = extractContentRangeValues(previousResponse);
        if (!parsedPreviousContentRange.isValid())
            return false;
        if (parsedPreviousContentRange.instanceLength() != ParsedContentRange::unknownLength && parsedPreviousContentRange.instanceLength() != parsedContentRange.instanceLength())
            return false;
    }
    return true;
}

void BackgroundFetch::Record::didReceiveResponse(ResourceResponse&& response)
{
    bool shouldClearResponseBody = false;
    if (response.httpStatusCode() == 206) {
        if (!validatePartialResponse(m_responseDataSize, response, m_response)) {
            didFinish(ResourceError { String { }, 0, response.url(), "Validation of partial response failed"_s, ResourceError::Type::AccessControl });
            return;
        }
    } else if (m_responseDataSize)
        shouldClearResponseBody = true;

    if (!m_responseDataSize || response.httpStatusCode() != 206)
        m_response = response;

    auto callbacks = std::exchange(m_responseCallbacks, { });
    for (auto& callback : callbacks)
        callback(ResourceResponse { m_response });
    if (RefPtr fetch = m_fetch.get())
        fetch->storeResponse(m_index, shouldClearResponseBody, WTF::move(response));
}

void BackgroundFetch::Record::didReceiveResponseBodyChunk(const SharedBuffer& data)
{
    m_responseDataSize += data.size();
    if (RefPtr fetch = m_fetch.get())
        fetch->storeResponseBodyChunk(m_index, data);

    if (!m_responseBodyCallbacks.isEmpty()) {
        RefPtr buffer = SharedBuffer::create(data.span());
        for (auto& callback : m_responseBodyCallbacks)
            callback(buffer.copyRef());
    }
}

void BackgroundFetch::Record::didFinish(const ResourceError& error)
{
    setAsCompleted();

    auto callbacks = std::exchange(m_responseCallbacks, { });
    for (auto& callback : callbacks)
        callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "Fetch failed"_s }));

    auto bodyCallbacks = std::exchange(m_responseBodyCallbacks, { });
    for (auto& callback : bodyCallbacks) {
        if (!error.isNull())
            callback(makeUnexpected(error));
        else
            callback(RefPtr<SharedBuffer> { });
    }

    if (RefPtr fetch = m_fetch.get())
        fetch->didFinishRecord(error);
}

void BackgroundFetch::Record::retrieveResponse(BackgroundFetchStore&, RetrieveRecordResponseCallback&& callback)
{
    if (m_isAborted) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::AbortError, "Background fetch was aborted"_s }));
        return;
    }

    if (!m_response.isNull()) {
        callback(ResourceResponse { m_response });
        return;
    }

    if (m_isCompleted) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "Fetch failed"_s }));
        return;
    }

    m_responseCallbacks.append(WTF::move(callback));
}

void BackgroundFetch::Record::retrieveRecordResponseBody(BackgroundFetchStore& store, RetrieveRecordResponseBodyCallback&& callback)
{
    if (m_isAborted) {
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Background fetch was aborted"_s, ResourceError::Type::Cancellation }));
        return;
    }

    ASSERT(!m_response.isNull());

    store.retrieveResponseBody(m_registrationKey, m_fetchIdentifier, m_index, [weakThis = WeakPtr { *this }, callback = WTF::move(callback)](auto&& result) mutable {
        if (!result.has_value()) {
            callback(makeUnexpected(WTF::move(result.error())));
            return;
        }

        callback(WTF::move(result.value()));

        if (!weakThis) {
            callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Record is gone"_s }));
            return;
        }

        if (weakThis->m_isAborted) {
            callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Background fetch was aborted"_s, ResourceError::Type::Cancellation }));
            return;
        }

        if (weakThis->m_isCompleted) {
            callback(RefPtr<SharedBuffer> { });
            return;
        }
        weakThis->m_responseBodyCallbacks.append(WTF::move(callback));
    });
}

void BackgroundFetch::doStore(CompletionHandler<void(BackgroundFetchStore::StoreResult)>&& callback, std::optional<size_t> responseBodyIndexToClear)
{
    WTF::Persistence::Encoder encoder;
    encoder << backgroundFetchCurrentVersion;
    encoder << m_registrationKey.topOrigin();
    encoder << m_registrationKey.scope();
    encoder << m_identifier;
    encoder << m_options.downloadTotal;
    encoder << m_options.title;
    encoder << m_options.icons.value_or(Vector<ImageResource> { });

    encoder << m_pausedFlag;

    encoder << (uint64_t)m_records.size();
    for (auto& record : m_records) {
        encoder << record->request().internalRequest;
        encoder << record->request().options;
        encoder << record->request().guard;
        encoder << record->request().httpHeaders;
        encoder << record->request().referrer;
        encoder << record->request().cspResponseHeaders.value();
        encoder << record->response();
        encoder << record->isCompleted();
    }

    m_store->storeFetch(m_registrationKey, m_identifier, m_options.downloadTotal, m_uploadTotal, responseBodyIndexToClear, { encoder.span() }, WTF::move(callback));
}

RefPtr<BackgroundFetch> BackgroundFetch::createFromStore(std::span<const uint8_t> data, SWServer& server, Ref<BackgroundFetchStore>&& store, NotificationCallback&& notificationCallback)
{
    WTF::Persistence::Decoder decoder(data);
    std::optional<unsigned> version;
    decoder >> version;
    if (!version)
        return nullptr;
    if (version != backgroundFetchCurrentVersion) {
        RELEASE_LOG_ERROR(ServiceWorker, "BackgroundFetch::createFromStore version mismatch");
        return nullptr;
    }

    std::optional<SecurityOriginData> registrationKeyOrigin;
    decoder >> registrationKeyOrigin;
    if (!registrationKeyOrigin)
        return nullptr;

    std::optional<URL> registrationKeyScope;
    decoder >> registrationKeyScope;
    if (!registrationKeyScope)
        return nullptr;

    RefPtr registration = server.getRegistration({ WTF::move(*registrationKeyOrigin), WTF::move(*registrationKeyScope) });
    if (!registration) {
        RELEASE_LOG_ERROR(ServiceWorker, "BackgroundFetch::createFromStore missing registration");
        return nullptr;
    }

    std::optional<String> identifier;
    decoder >> identifier;
    if (!identifier)
        return nullptr;

    std::optional<uint64_t> downloadTotal;
    decoder >> downloadTotal;
    if (!downloadTotal)
        return nullptr;

    std::optional<String> title;
    decoder >> title;
    if (!title)
        return nullptr;

    std::optional<Vector<ImageResource>> icons;
    decoder >> icons;
    if (!icons)
        return nullptr;

    std::optional<bool> pausedFlag;
    decoder >> pausedFlag;
    if (!pausedFlag)
        return nullptr;

    BackgroundFetchOptions options { { WTF::move(*icons), WTF::move(*title) }, *downloadTotal };
    auto fetch = BackgroundFetch::create(*registration, WTF::move(*identifier), WTF::move(options), WTF::move(store), WTF::move(notificationCallback), *pausedFlag);

    std::optional<uint64_t> recordSize;
    decoder >> recordSize;
    if (!recordSize)
        return nullptr;

    Vector<Ref<Record>> records;
    records.reserveInitialCapacity(*recordSize);

    for (uint64_t index = 0; index < *recordSize; ++index) {
        std::optional<WebCore::ResourceRequest> internalRequest;
        decoder >> internalRequest;
        if (!internalRequest)
            return nullptr;

        WebCore::FetchOptions options;
        if (!WebCore::FetchOptions::decodePersistent(decoder, options))
            return nullptr;

        std::optional<WebCore::FetchHeaders::Guard> requestHeadersGuard;
        decoder >> requestHeadersGuard;
        if (!requestHeadersGuard)
            return nullptr;

        std::optional<WebCore::HTTPHeaderMap> httpHeaders;
        decoder >> httpHeaders;
        if (!httpHeaders)
            return nullptr;

        std::optional<String> referrer;
        decoder >> referrer;
        if (!referrer)
            return nullptr;

        std::optional<ContentSecurityPolicyResponseHeaders> responseHeaders;
        decoder >> responseHeaders;
        if (!responseHeaders)
            return nullptr;

        std::optional<ResourceResponse> unusedResponseData;
        decoder >> unusedResponseData;
        if (!unusedResponseData)
            return nullptr;

        std::optional<bool> isCompleted;
        decoder >> isCompleted;
        if (!isCompleted)
            return nullptr;

        auto record = Record::create(fetch.get(), { WTF::move(*internalRequest), WTF::move(options), *requestHeadersGuard, WTF::move(*httpHeaders), WTF::move(*referrer), WTF::move(*responseHeaders) }, index);
        if (*isCompleted)
            record->setAsCompleted();
        records.append(WTF::move(record));
    }
    fetch->setRecords(WTF::move(records));

    return RefPtr { WTF::move(fetch) };
}

} // namespace WebCore
