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
#include "BackgroundFetchManager.h"

#if ENABLE(SERVICE_WORKER)

#include "BackgroundFetchInformation.h"
#include "BackgroundFetchRequest.h"
#include "ContentSecurityPolicy.h"
#include "FetchRequest.h"
#include "JSBackgroundFetchRegistration.h"
#include "SWClientConnection.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerRegistration.h"

namespace WebCore {

BackgroundFetchManager::BackgroundFetchManager(ServiceWorkerRegistration& registration)
    : m_identifier(registration.identifier())
{
}

BackgroundFetchManager::~BackgroundFetchManager()
{
}

static ExceptionOr<Vector<Ref<FetchRequest>>> buildBackgroundFetchRequests(ScriptExecutionContext& context, BackgroundFetchManager::Requests&& backgroundFetchRequests)
{
    return switchOn(WTFMove(backgroundFetchRequests), [&context] (RefPtr<FetchRequest>&& request) -> ExceptionOr<Vector<Ref<FetchRequest>>> {
        auto result = FetchRequest::create(context, request.releaseNonNull(), { });
        if (result.hasException())
            return result.releaseException();
        if (result.returnValue()->mode() == FetchOptions::Mode::NoCors)
            return Exception { TypeError, "Request has no-cors mode"_s };
        return Vector<Ref<FetchRequest>> { result.releaseReturnValue() };
    }, [&context] (String&& url) -> ExceptionOr<Vector<Ref<FetchRequest>>> {
        auto result = FetchRequest::create(context, WTFMove(url), { });
        if (result.hasException())
            return result.releaseException();
        return Vector<Ref<FetchRequest>> { result.releaseReturnValue() };
    }, [&context] (Vector<BackgroundFetchManager::RequestInfo>&& requestInfos) -> ExceptionOr<Vector<Ref<FetchRequest>>> {
        std::optional<Exception> exception;
        Vector<Ref<FetchRequest>> requests;
        requests.reserveInitialCapacity(requestInfos.size());
        for (auto& requestInfo : requestInfos) {
            auto result = FetchRequest::create(context, WTFMove(requestInfo), { });
            if (result.hasException())
                return result.releaseException();
            if (result.returnValue()->mode() == FetchOptions::Mode::NoCors)
                return Exception { TypeError, "Request has no-cors mode"_s };
            
            // FIXME: Add support for readable stream bodies
            if (result.returnValue()->isReadableStreamBody())
                return Exception { NotSupportedError, "ReadableStream uploading is not supported"_s };
            
            requests.uncheckedAppend(result.releaseReturnValue());
        }
        return requests;
    });
}

Ref<BackgroundFetchRegistration> BackgroundFetchManager::backgroundFetchRegistrationInstance(ScriptExecutionContext& context, BackgroundFetchInformation&& data)
{
    auto identifier = data.identifier;
    auto result = m_backgroundFetchRegistrations.ensure(identifier, [&]() mutable {
        return BackgroundFetchRegistration::create(context, WTFMove(data));
    });

    auto registration = result.iterator->value;
    if (!result.isNewEntry)
        registration->updateInformation(data);
    return registration;
}

void BackgroundFetchManager::fetch(ScriptExecutionContext& context, const String& fetchIdentifier, Requests&& backgroundFetchRequests, BackgroundFetchOptions&& options, DOMPromiseDeferred<IDLInterface<BackgroundFetchRegistration>>&& promise)
{
    auto generatedRequests = buildBackgroundFetchRequests(context, WTFMove(backgroundFetchRequests));
    if (generatedRequests.hasException()) {
        promise.reject(generatedRequests.releaseException());
        return;
    }

    if (!generatedRequests.returnValue().size()) {
        promise.reject(Exception { TypeError, "No requests"_s });
        return;
    }

    auto requests = map(generatedRequests.releaseReturnValue(), [&](auto&& fetchRequest) -> BackgroundFetchRequest {
        Markable<ContentSecurityPolicyResponseHeaders, ContentSecurityPolicyResponseHeaders::MarkableTraits> responseHeaders;
        if (!context.shouldBypassMainWorldContentSecurityPolicy()) {
            if (auto* policy = context.contentSecurityPolicy())
                responseHeaders = policy->responseHeaders();
        }
        return { fetchRequest->resourceRequest(), fetchRequest->fetchOptions(), fetchRequest->headers().guard(), fetchRequest->headers().internalHeaders(), fetchRequest->internalRequestReferrer(), WTFMove(responseHeaders) };
    });
    SWClientConnection::fromScriptExecutionContext(context)->startBackgroundFetch(m_identifier, fetchIdentifier, WTFMove(requests), WTFMove(options), [weakThis = WeakPtr { *this }, weakContext = WeakPtr { context }, promise = WTFMove(promise)](auto&& result) mutable {
        if (!weakContext)
            return;
        weakContext->postTask([weakThis = WTFMove(weakThis), promise = WTFMove(promise), result = WTFMove(result)](auto& context) mutable {
            if (!weakThis)
                return;

            if (result.hasException()) {
                promise.reject(result.releaseException());
                return;
            }
            if (result.returnValue().identifier.isNull()) {
                promise.reject(Exception { TypeError, "An internal error occured"_s });
                return;
            }

            promise.resolve(weakThis->backgroundFetchRegistrationInstance(context, result.releaseReturnValue()));
        });

    });
}

void BackgroundFetchManager::get(ScriptExecutionContext& context, const String& fetchIdentifier, DOMPromiseDeferred<IDLNullable<IDLInterface<BackgroundFetchRegistration>>>&& promise)
{
    auto iterator = m_backgroundFetchRegistrations.find(fetchIdentifier);
    if (iterator == m_backgroundFetchRegistrations.end()) {
        promise.resolve(nullptr);
        return;
    }

    SWClientConnection::fromScriptExecutionContext(context)->backgroundFetchInformation(m_identifier, fetchIdentifier, [weakThis = WeakPtr { *this }, weakContext = WeakPtr { context }, promise = WTFMove(promise)](auto&& result) mutable {
        if (!weakContext)
            return;
        weakContext->postTask([weakThis = WTFMove(weakThis), promise = WTFMove(promise), result = WTFMove(result)](auto& context) mutable {
            if (!weakThis)
                return;

            if (result.hasException()) {
                promise.reject(result.releaseException());
                return;
            }

            RefPtr<BackgroundFetchRegistration> backgroundFetchRegistration;
            if (!result.returnValue().identifier.isNull())
                backgroundFetchRegistration = weakThis->backgroundFetchRegistrationInstance(context, result.releaseReturnValue());

            promise.resolve(backgroundFetchRegistration.get());
        });
    });
}

void BackgroundFetchManager::getIds(ScriptExecutionContext& context, DOMPromiseDeferred<IDLSequence<IDLDOMString>>&& promise)
{
    SWClientConnection::fromScriptExecutionContext(context)->backgroundFetchIdentifiers(m_identifier, [promise = WTFMove(promise)](auto&& result) mutable {
        promise.resolve(WTFMove(result));
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
