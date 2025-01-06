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
#include "BackgroundFetchRegistration.h"

#include "BackgroundFetchManager.h"
#include "BackgroundFetchRecordInformation.h"
#include "CacheQueryOptions.h"
#include "EventNames.h"
#include "FetchRequest.h"
#include "FetchResponse.h"
#include "FetchResponseBodyLoader.h"
#include "JSBackgroundFetchRecord.h"
#include "Node.h"
#include "RetrieveRecordsOptions.h"
#include "SWClientConnection.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerRegistrationBackgroundFetchAPI.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
class BackgroundFetchResponseBodyLoader;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::BackgroundFetchResponseBodyLoader> : std::true_type { };
}

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(BackgroundFetchRegistration);

void BackgroundFetchRegistration::updateIfExisting(ScriptExecutionContext& context, const BackgroundFetchInformation& information)
{
    RefPtr container = context.serviceWorkerContainer();
    RefPtr registration = container ? container->registration(information.registrationIdentifier) : nullptr;
    RefPtr manager = registration ? ServiceWorkerRegistrationBackgroundFetchAPI::backgroundFetchIfCreated(*registration) : nullptr;
    if (auto backgroundFetchRegistration = manager ? manager->existingBackgroundFetchRegistration(information.identifier) : nullptr)
        backgroundFetchRegistration->updateInformation(information);
}

Ref<BackgroundFetchRegistration> BackgroundFetchRegistration::create(ScriptExecutionContext& context, BackgroundFetchInformation&& information)
{
    auto registration = adoptRef(*new BackgroundFetchRegistration(context, WTFMove(information)));
    registration->suspendIfNeeded();
    return registration;
}

BackgroundFetchRegistration::BackgroundFetchRegistration(ScriptExecutionContext& context, BackgroundFetchInformation&& information)
    : ActiveDOMObject(&context)
    , m_information(WTFMove(information))
{
}

BackgroundFetchRegistration::~BackgroundFetchRegistration()
{
}

void BackgroundFetchRegistration::abort(ScriptExecutionContext& context, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    SWClientConnection::fromScriptExecutionContext(context)->abortBackgroundFetch(registrationIdentifier(), id(), [promise = WTFMove(promise)](auto&& result) mutable {
        promise.resolve(result);
    });
}

static ExceptionOr<ResourceRequest> requestFromInfo(ScriptExecutionContext& context, std::optional<BackgroundFetchRegistration::RequestInfo>&& info)
{
    if (!info)
        return ResourceRequest { };

    ResourceRequest resourceRequest;
    auto requestOrException = FetchRequest::create(context, WTFMove(*info), { });
    if (requestOrException.hasException())
        return requestOrException.releaseException();

    return requestOrException.releaseReturnValue()->resourceRequest();
}

class BackgroundFetchResponseBodyLoader : public FetchResponseBodyLoader, public CanMakeWeakPtr<BackgroundFetchResponseBodyLoader> {
public:
    BackgroundFetchResponseBodyLoader(ScriptExecutionContext& context, FetchResponse& response, BackgroundFetchRecordIdentifier recordIdentifier)
        : FetchResponseBodyLoader(response)
        , m_connection(SWClientConnection::fromScriptExecutionContext(context))
        , m_recordIdentifier(recordIdentifier)
    {
    }

private:
    void start() final
    {
        m_connection->retrieveRecordResponseBody(m_recordIdentifier, [weakThis = WeakPtr { *this }](auto&& result) {
            if (!weakThis || !weakThis->m_response)
                return;

            Ref protectedResponse = *weakThis->m_response;

            if (!result.has_value()) {
                weakThis->m_response = nullptr;
                protectedResponse->receivedError(WTFMove(result.error()));
                return;
            }

            auto buffer = WTFMove(result.value());
            if (!buffer) {
                weakThis->m_response = nullptr;
                protectedResponse->didSucceed({ });
                return;
            }

            protectedResponse->receivedData(buffer.releaseNonNull());
        });
    }

    void stop() final
    {
        m_response = nullptr;
    }

    Ref<SWClientConnection> m_connection;
    BackgroundFetchRecordIdentifier m_recordIdentifier;
};

static Ref<BackgroundFetchRecord> createRecord(ScriptExecutionContext& context, BackgroundFetchRecordInformation&& information)
{
    auto recordIdentifier = information.identifier;
    auto record = BackgroundFetchRecord::create(context, WTFMove(information));
    SWClientConnection::fromScriptExecutionContext(context)->retrieveRecordResponse(recordIdentifier, [weakContext = WeakPtr { context }, record, recordIdentifier](auto&& result) {
        if (!weakContext)
            return;

        if (result.hasException()) {
            record->settleResponseReadyPromise(result.releaseException());
            return;
        }

        auto response = FetchResponse::create(weakContext.get(), { }, FetchHeaders::Guard::Immutable, { });
        response->setReceivedInternalResponse(result.releaseReturnValue(), FetchOptions::Credentials::Omit);
        response->setBodyLoader(makeUniqueRef<BackgroundFetchResponseBodyLoader>(*weakContext, response.get(), recordIdentifier));
        record->settleResponseReadyPromise(WTFMove(response));
    });
    return record;
}

void BackgroundFetchRegistration::match(ScriptExecutionContext& context, RequestInfo&& info, const CacheQueryOptions& options, DOMPromiseDeferred<IDLInterface<BackgroundFetchRecord>>&& promise)
{
    if (!recordsAvailable()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Records are not available"_s });
        return;
    }

    auto requestOrException = requestFromInfo(context, WTFMove(info));
    if (requestOrException.hasException()) {
        promise.reject(requestOrException.releaseException());
        return;
    }

    bool shouldRetrieveResponses = false;
    RetrieveRecordsOptions retrieveOptions { requestOrException.releaseReturnValue(), context.crossOriginEmbedderPolicy(), *context.securityOrigin(), options.ignoreSearch, options.ignoreMethod, options.ignoreVary, shouldRetrieveResponses };

    SWClientConnection::fromScriptExecutionContext(context)->matchBackgroundFetch(registrationIdentifier(), id(), WTFMove(retrieveOptions), [weakContext = WeakPtr { context }, promise = WTFMove(promise)](auto&& results) mutable {
        if (!weakContext)
            return;

        if (!results.size()) {
            promise.reject(Exception { ExceptionCode::TypeError, "No matching record"_s });
            return;
        }

        promise.resolve(createRecord(*weakContext, WTFMove(results[0])));
    });
}

void BackgroundFetchRegistration::matchAll(ScriptExecutionContext& context, std::optional<RequestInfo>&& info, const CacheQueryOptions& options, DOMPromiseDeferred<IDLSequence<IDLInterface<BackgroundFetchRecord>>>&& promise)
{
    if (!recordsAvailable()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Records are not available"_s });
        return;
    }

    auto requestOrException = requestFromInfo(context, WTFMove(info));
    if (requestOrException.hasException()) {
        promise.reject(requestOrException.releaseException());
        return;
    }

    bool shouldRetrieveResponses = false;
    RetrieveRecordsOptions retrieveOptions { requestOrException.releaseReturnValue(), context.crossOriginEmbedderPolicy(), *context.securityOrigin(), options.ignoreSearch, options.ignoreMethod, options.ignoreVary, shouldRetrieveResponses };

    SWClientConnection::fromScriptExecutionContext(context)->matchBackgroundFetch(registrationIdentifier(), id(), WTFMove(retrieveOptions), [weakContext = WeakPtr { context }, promise = WTFMove(promise)](auto&& results) mutable {
        if (!weakContext)
            return;

        auto records = WTF::map(results, [&weakContext](auto& result) {
            return createRecord(*weakContext, WTFMove(result));
        });

        promise.resolve(WTFMove(records));
    });
}

void BackgroundFetchRegistration::updateInformation(const BackgroundFetchInformation& information)
{
    ASSERT(m_information.registrationIdentifier == information.registrationIdentifier);
    ASSERT(m_information.identifier == information.identifier);
    ASSERT(m_information.recordsAvailable);

    if (m_information.downloaded == information.downloaded && m_information.uploaded == information.uploaded && m_information.result == information.result && m_information.failureReason == information.failureReason)
        return;
    
    m_information.uploadTotal = information.uploadTotal;
    m_information.uploaded = information.uploaded;
    m_information.downloadTotal = information.downloadTotal;
    m_information.downloaded = information.downloaded;
    m_information.result = information.result;
    m_information.failureReason = information.failureReason;
    m_information.recordsAvailable = information.recordsAvailable;
    
    dispatchEvent(Event::create(eventNames().progressEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void BackgroundFetchRegistration::stop()
{
}

bool BackgroundFetchRegistration::virtualHasPendingActivity() const
{
    return m_information.recordsAvailable;
}

} // namespace WebCore
