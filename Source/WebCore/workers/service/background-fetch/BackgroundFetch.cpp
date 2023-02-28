/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include "BackgroundFetchInformation.h"
#include "BackgroundFetchRecordInformation.h"
#include "CacheQueryOptions.h"
#include "ExceptionData.h"
#include "SWServerRegistration.h"

namespace WebCore {

BackgroundFetch::BackgroundFetch(SWServerRegistration& registration, const String& identifier, Vector<BackgroundFetchRequest>&& requests, BackgroundFetchOptions&& options, Ref<BackgroundFetchStore>&& store, NotificationCallback&& notificationCallback)
    : m_identifier(identifier)
    , m_options(WTFMove(options))
    , m_registrationKey(registration.key())
    , m_registrationIdentifier(registration.identifier())
    , m_store(WTFMove(store))
    , m_notificationCallback(WTFMove(notificationCallback))
    , m_origin { m_registrationKey.topOrigin(), SecurityOriginData::fromURL(m_registrationKey.scope()) }
{
    size_t index = 0;
    m_records.reserveInitialCapacity(requests.size());
    for (auto& request : requests)
        m_records.uncheckedAppend(Record::create(*this, WTFMove(request), index++));
    doStore();
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

    callback(WTFMove(records));
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

void BackgroundFetch::storeResponse(size_t index, ResourceResponse&& response)
{
    UNUSED_PARAM(index);
    ASSERT(index < m_records.size());
    if (!response.isSuccessful()) {
        updateBackgroundFetchStatus(BackgroundFetchResult::Failure, BackgroundFetchFailureReason::BadStatus);
        return;
    }
    doStore();
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

void BackgroundFetch::didFinishRecord(size_t index, const ResourceError& error)
{
    ASSERT(index < m_records.size());
    if (error.isNull()) {
        recordIsCompleted(index);
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

void BackgroundFetch::recordIsCompleted(size_t index)
{
    m_records[index]->setAsCompleted();
    if (anyOf(m_records, [](auto& record) { return !record->isCompleted(); }))
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
    m_notificationCallback(information());
}

void BackgroundFetch::unsetRecordsAvailableFlag()
{
    ASSERT(m_recordsAvailableFlag);
    m_recordsAvailableFlag = false;
    m_store->clearFetch(m_registrationKey, m_identifier);
    m_notificationCallback(information());
}

BackgroundFetch::Record::Record(BackgroundFetch& fetch, BackgroundFetchRequest&& request, size_t index)
    : m_fetch(fetch)
    , m_identifier(BackgroundFetchRecordIdentifier::generate())
    , m_fetchIdentifier(fetch.m_identifier)
    , m_registrationKey(fetch.m_registrationKey)
    , m_request(WTFMove(request))
    , m_index(index)
{
}

BackgroundFetch::Record::~Record()
{
    auto callbacks = std::exchange(m_responseCallbacks, { });
    for (auto& callback : callbacks)
        callback(makeUnexpected(ExceptionData { TypeError, "Record is gone"_s }));

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
    return BackgroundFetchRecordInformation { m_identifier, m_request.internalRequest, m_request.options, m_request.guard, m_request.httpHeaders, m_request.referrer };
}

void BackgroundFetch::Record::complete(const CreateLoaderCallback& createLoaderCallback)
{
    ASSERT(!m_loader);
    // FIXME: Handle Range headers
    m_loader = createLoaderCallback(*this, ResourceRequest { m_request.internalRequest }, FetchOptions { m_request.options }, m_fetch->m_origin);
    if (!m_loader)
        abort();
}

void BackgroundFetch::Record::abort()
{
    if (m_isAborted)
        return;

    m_isAborted = true;

    auto callbacks = std::exchange(m_responseCallbacks, { });
    for (auto& callback : callbacks)
        callback(makeUnexpected(ExceptionData { AbortError, "Background fetch was aborted"_s }));

    auto bodyCallbacks = std::exchange(m_responseBodyCallbacks, { });
    for (auto& callback : bodyCallbacks)
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Background fetch was aborted"_s, ResourceError::Type::Cancellation }));

    if (!m_loader)
        return;
    m_loader->abort();
    m_loader = nullptr;
}

void BackgroundFetch::Record::didSendData(uint64_t size)
{
    if (m_fetch)
        m_fetch->didSendData(size);
}

void BackgroundFetch::Record::didReceiveResponse(ResourceResponse&& response)
{
    m_response = response;
    auto callbacks = std::exchange(m_responseCallbacks, { });
    for (auto& callback : callbacks)
        callback(ResourceResponse { m_response });
    if (m_fetch)
        m_fetch->storeResponse(m_index, WTFMove(response));
}

void BackgroundFetch::Record::didReceiveResponseBodyChunk(const SharedBuffer& data)
{
    m_responseDataSize += data.size();
    if (m_fetch)
        m_fetch->storeResponseBodyChunk(m_index, data);

    if (!m_responseBodyCallbacks.isEmpty()) {
        RefPtr buffer = SharedBuffer::create(data.data(), data.size());
        for (auto& callback : m_responseBodyCallbacks)
            callback(buffer.copyRef());
    }
}

void BackgroundFetch::Record::didFinish(const ResourceError& error)
{
    auto callbacks = std::exchange(m_responseCallbacks, { });
    for (auto& callback : callbacks)
        callback(makeUnexpected(ExceptionData { TypeError, "Fetch failed"_s }));

    auto bodyCallbacks = std::exchange(m_responseBodyCallbacks, { });
    for (auto& callback : bodyCallbacks) {
        if (!error.isNull())
            callback(makeUnexpected(error));
        else
            callback(RefPtr<SharedBuffer> { });
    }

    if (m_fetch)
        m_fetch->didFinishRecord(m_index, error);
}

void BackgroundFetch::Record::retrieveResponse(BackgroundFetchStore&, RetrieveRecordResponseCallback&& callback)
{
    if (m_isAborted) {
        callback(makeUnexpected(ExceptionData { AbortError, "Background fetch was aborted"_s }));
        return;
    }

    if (!m_response.isNull()) {
        callback(ResourceResponse { m_response });
        return;
    }

    m_responseCallbacks.append(WTFMove(callback));
}

void BackgroundFetch::Record::retrieveRecordResponseBody(BackgroundFetchStore& store, RetrieveRecordResponseBodyCallback&& callback)
{
    if (m_isAborted) {
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Background fetch was aborted"_s, ResourceError::Type::Cancellation }));
        return;
    }

    ASSERT(!m_response.isNull());

    store.retrieveResponseBody(m_registrationKey, m_fetchIdentifier, m_index, [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](auto&& result) mutable {
        if (!result.has_value()) {
            callback(makeUnexpected(WTFMove(result.error())));
            return;
        }

        callback(WTFMove(result.value()));

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
        weakThis->m_responseBodyCallbacks.append(WTFMove(callback));
    });
}

void BackgroundFetch::doStore()
{
    // FIXME: TODO.
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
