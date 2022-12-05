/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "FetchEvent.h"

#include "CachedResourceRequestInitiatorTypes.h"
#include "EventNames.h"
#include "FetchRequest.h"
#include "JSDOMPromise.h"
#include "JSFetchResponse.h"
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>

#if ENABLE(SERVICE_WORKER)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FetchEvent);

Ref<FetchEvent> FetchEvent::createForTesting(ScriptExecutionContext& context)
{
    FetchEvent::Init init;
    init.request = FetchRequest::create(context, { }, FetchHeaders::create(FetchHeaders::Guard::Immutable, { }), { }, { }, { });
    return FetchEvent::create(*context.globalObject(), eventNames().fetchEvent, WTFMove(init), Event::IsTrusted::Yes);
}

static inline Ref<DOMPromise> retrieveHandledPromise(JSC::JSGlobalObject& globalObject, RefPtr<DOMPromise>&& promise)
{
    if (promise)
        return promise.releaseNonNull();

    JSC::JSLockHolder lock(globalObject.vm());

    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(&globalObject);
    auto deferredPromise = DeferredPromise::create(jsDOMGlobalObject);
    return DOMPromise::create(jsDOMGlobalObject, *JSC::jsCast<JSC::JSPromise*>(deferredPromise->promise()));
}

FetchEvent::FetchEvent(JSC::JSGlobalObject& globalObject, const AtomString& type, Init&& initializer, IsTrusted isTrusted)
    : ExtendableEvent(type, initializer, isTrusted)
    , m_request(initializer.request.releaseNonNull())
    , m_clientId(WTFMove(initializer.clientId))
    , m_resultingClientId(WTFMove(initializer.resultingClientId))
    , m_handled(retrieveHandledPromise(globalObject, WTFMove(initializer.handled)))
{
}

FetchEvent::~FetchEvent()
{
    if (auto callback = WTFMove(m_onResponse)) {
        RELEASE_LOG_ERROR_IF(m_respondWithEntered, ServiceWorker, "Fetch event is destroyed without a response, respondWithEntered=%d, waitToRespond=%d, respondWithError=%d, respondPromise=%d", m_respondWithEntered, m_waitToRespond, m_respondWithError, !!m_respondPromise);
        callback(makeUnexpected(std::optional<ResourceError> { }));
    }
}

ResourceError FetchEvent::createResponseError(const URL& url, const String& errorMessage, ResourceError::IsSanitized isSanitized)
{
    return ResourceError { errorDomainWebKitServiceWorker, 0, url, makeString("FetchEvent.respondWith received an error: ", errorMessage), ResourceError::Type::General, isSanitized };

}

ExceptionOr<void> FetchEvent::respondWith(Ref<DOMPromise>&& promise)
{
    if (!isBeingDispatched())
        return Exception { InvalidStateError, "Event is not being dispatched"_s };

    if (m_respondWithEntered)
        return Exception { InvalidStateError, "Event respondWith flag is set"_s };

    m_respondPromise = WTFMove(promise);
    addExtendLifetimePromise(*m_respondPromise);

    auto isRegistered = m_respondPromise->whenSettled([this, protectedThis = Ref { *this }] {
        promiseIsSettled();
    });

    stopPropagation();
    stopImmediatePropagation();

    m_respondWithEntered = true;
    m_waitToRespond = true;

    if (isRegistered == DOMPromise::IsCallbackRegistered::No)
        respondWithError(createResponseError(m_request->url(), "FetchEvent unable to handle respondWith promise."_s, ResourceError::IsSanitized::Yes));

    return { };
}

void FetchEvent::onResponse(ResponseCallback&& callback)
{
    ASSERT(!m_onResponse);
    m_onResponse = WTFMove(callback);
}

void FetchEvent::respondWithError(ResourceError&& error)
{
    m_respondWithError = true;
    processResponse(makeUnexpected(WTFMove(error)));
}

void FetchEvent::processResponse(Expected<Ref<FetchResponse>, std::optional<ResourceError>>&& result)
{
    m_respondPromise = nullptr;
    m_waitToRespond = false;
    if (auto callback = WTFMove(m_onResponse))
        callback(WTFMove(result));
}

void FetchEvent::promiseIsSettled()
{
    if (m_respondPromise->status() == DOMPromise::Status::Rejected) {
        auto reason = m_respondPromise->result().toWTFString(m_respondPromise->globalObject());
        respondWithError(createResponseError(m_request->url(), reason, ResourceError::IsSanitized::Yes));
        return;
    }

    ASSERT(m_respondPromise->status() == DOMPromise::Status::Fulfilled);
    auto response = JSFetchResponse::toWrapped(m_respondPromise->globalObject()->vm(), m_respondPromise->result());
    if (!response) {
        respondWithError(createResponseError(m_request->url(), "Returned response is null."_s, ResourceError::IsSanitized::Yes));
        return;
    }

    if (response->isDisturbedOrLocked()) {
        respondWithError(createResponseError(m_request->url(), "Response is disturbed or locked."_s, ResourceError::IsSanitized::Yes));
        return;
    }

    processResponse(Ref { *response });
}

FetchEvent::PreloadResponsePromise& FetchEvent::preloadResponse(ScriptExecutionContext& context)
{
    if (!m_preloadResponsePromise) {
        m_preloadResponsePromise = makeUnique<PreloadResponsePromise>();
        if (!m_navigationPreloadIdentifier) {
            auto* globalObject = context.globalObject();
            if (globalObject) {
                JSC::Strong<JSC::Unknown> value { globalObject->vm(), JSC::jsUndefined() };
                m_preloadResponsePromise->resolve(value);
            }
            return *m_preloadResponsePromise;
        }
    }
    return *m_preloadResponsePromise;
}

void FetchEvent::navigationPreloadIsReady(ResourceResponse&& response)
{
    auto* globalObject = m_handled->globalObject();
    auto* context = globalObject ? globalObject->scriptExecutionContext() : nullptr;
    if (!context)
        return;

    if (!m_preloadResponsePromise)
        m_preloadResponsePromise = makeUnique<PreloadResponsePromise>();

    auto request = FetchRequest::create(*context, { }, FetchHeaders::create(), ResourceRequest { m_request->internalRequest() } , FetchOptions { m_request->fetchOptions() }, String { m_request->internalRequestReferrer() });
    request->setNavigationPreloadIdentifier(m_navigationPreloadIdentifier);

    auto fetchResponse = FetchResponse::createFetchResponse(*context, request.get(), { });
    fetchResponse->setReceivedInternalResponse(response, FetchOptions::Credentials::Include);
    fetchResponse->setIsNavigationPreload(true);

    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    JSC::Strong<JSC::Unknown> value { vm, toJS(globalObject, JSC::jsCast<JSDOMGlobalObject*>(globalObject), fetchResponse.get()) };
    m_preloadResponsePromise->resolve(value);

    // We postpone the load to leave some time for the service worker to use the preload before loading it.
    context->postTask([fetchResponse = WTFMove(fetchResponse), request = WTFMove(request)](auto& context) {
        if (!fetchResponse->isUsedForPreload())
            fetchResponse->startLoader(context, request.get(), cachedResourceRequestInitiatorTypes().navigation);
    });
}

void FetchEvent::navigationPreloadFailed(ResourceError&& error)
{
    if (!m_preloadResponsePromise)
        m_preloadResponsePromise = makeUnique<PreloadResponsePromise>();
    m_preloadResponsePromise->reject(Exception { TypeError, error.sanitizedDescription() });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
