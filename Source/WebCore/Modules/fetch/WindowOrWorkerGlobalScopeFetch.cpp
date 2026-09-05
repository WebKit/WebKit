/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "WindowOrWorkerGlobalScopeFetch.h"

#include "CachedResourceRequestInitiatorTypes.h"
#include "ContentSecurityPolicy.h"
#include "DeferredFetchRegistry.h"
#include "DocumentQuirks.h"
#include "EventLoop.h"
#include "FetchBody.h"
#include "FetchLaterResult.h"
#include "FetchResponse.h"
#include "FormData.h"
#include "FrameLoader.h"
#include "HTTPParsers.h"
#include "JSDOMConvertAny.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFetchResponse.h"
#include "JSValueInWrappedObjectInlines.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "OriginAccessPatterns.h"
#include "QuotaExceededError.h"
#include "ResourceLoaderOptions.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include "SecurityPolicy.h"
#include "UserGestureIndicator.h"
#include "WorkerGlobalScope.h"
#include <limits>
#include <wtf/CheckedArithmetic.h>

namespace WebCore {

using FetchResponsePromise = DOMPromiseDeferred<IDLInterface<FetchResponse>>;

// https://fetch.spec.whatwg.org/#dom-global-fetch
static void doFetch(ScriptExecutionContext& scope, FetchRequest::Info&& input, FetchRequest::Init&& init, FetchResponsePromise&& promise)
{
    auto requestOrException = FetchRequest::create(scope, WTF::move(input), WTF::move(init));
    if (requestOrException.hasException()) {
        promise.reject(requestOrException.releaseException());
        return;
    }

    auto request = requestOrException.releaseReturnValue();
    if (request->signal().aborted()) {
        auto reason = request->signal().reason().getValue();
        if (reason.isUndefined())
            promise.reject(Exception { ExceptionCode::AbortError, "Request signal is aborted"_s });
        else
            promise.rejectType<IDLAny>(reason);

        return;
    }

    FetchResponse::fetch(scope, request.get(), [promise = WTF::move(promise), scope = Ref { scope }, userGestureToken = UserGestureIndicator::currentUserGesture()]<typename Result> (Result&& result) mutable {
        scope->eventLoop().queueTask(TaskSource::Networking, [promise = WTF::move(promise), userGestureToken = WTF::move(userGestureToken), result = std::forward<Result>(result)] () mutable {
            if (!userGestureToken || userGestureToken->hasExpired(UserGestureToken::maximumIntervalForUserGestureForwardingForFetch()) || !userGestureToken->processingUserGesture()) {
                promise.settle(WTF::move(result));
                return;
            }
            UserGestureIndicator gestureIndicator(userGestureToken, userGestureToken->scope(), UserGestureToken::ShouldPropagateToMicroTask::Yes);
            promise.settle(WTF::move(result));
        });
    }, cachedResourceRequestInitiatorTypes().fetch);
}

void WindowOrWorkerGlobalScopeFetch::fetch(DOMWindow& window, FetchRequest::Info&& input, FetchRequest::Init&& init, Ref<DeferredPromise>&& promise)
{
    if (RefPtr document = window.documentIfLocal(); document && document->quirks().shouldBlockFetchWithNewlineAndLessThan()) {
        if (auto* string = std::get_if<String>(&input); string && string->contains('\n') && string->contains('<'))
            return promise->reject(ExceptionCode::InvalidStateError);
    }

    RefPtr localWindow = dynamicDowncast<LocalDOMWindow>(window);
    if (!localWindow) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }
    RefPtr document = localWindow->document();
    if (!document) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }
    doFetch(*document, WTF::move(input), WTF::move(init), WTF::move(promise));
}

void WindowOrWorkerGlobalScopeFetch::fetch(WorkerGlobalScope& scope, FetchRequest::Info&& input, FetchRequest::Init&& init, Ref<DeferredPromise>&& promise)
{
    doFetch(scope, WTF::move(input), WTF::move(init), WTF::move(promise));
}

// https://fetch.spec.whatwg.org/#dom-window-fetchlater
ExceptionOr<Ref<FetchLaterResult>> WindowOrWorkerGlobalScopeFetch::fetchLater(DOMWindow& window, FetchRequest::Info&& input, DeferredRequestInit&& init)
{
    RefPtr localWindow = dynamicDowncast<LocalDOMWindow>(window);
    if (!localWindow)
        return Exception { ExceptionCode::InvalidStateError, "fetchLater() called on a non-local window"_s };

    RefPtr document = localWindow->document();
    if (!document || !document->isFullyActive())
        return Exception { ExceptionCode::InvalidStateError, "fetchLater() requires a fully active Document"_s };
    Ref<ScriptExecutionContext> context = *document;

    // activateAfter must be non-negative.
    if (init.activateAfter && *init.activateAfter < 0)
        return Exception { ExceptionCode::RangeError, "activateAfter must be a non-negative number"_s };

    // Force keepalive on. This makes FetchRequest reject ReadableStream bodies
    // for us and ensures NetworkResourceLoader::abort() transfers the load to
    // the keep-alive pool if the document is destroyed while it is in flight.
    init.keepalive = true;

    auto requestOrException = FetchRequest::create(context.get(), WTF::move(input), FetchRequest::Init { init });
    if (requestOrException.hasException())
        return requestOrException.releaseException();
    Ref request = requestOrException.releaseReturnValue();

    if (!request->url().protocolIsInHTTPFamily() || !shouldTreatAsPotentiallyTrustworthy(request->url()))
        return Exception { ExceptionCode::TypeError, "fetchLater() only supports HTTP(S) URLs on potentially trustworthy origins"_s };

    // If the AbortSignal is already aborted, throw synchronously.
    if (request->signal().aborted())
        return Exception { ExceptionCode::AbortError, "Request signal is aborted"_s };

    // Enforce CSP connect-src.
    if (!document->shouldBypassMainWorldContentSecurityPolicy()
        && !protect(document->contentSecurityPolicy())->allowConnectToSource(request->url(), document->currentParserSourcePosition())) {
        // Treat as a silent network failure per Beacon precedent: return a
        // FetchLaterResult that never activates.
        return FetchLaterResult::create();
    }

    // Build the ResourceRequest we will hand to the network stack on activation.
    ResourceRequest resourceRequest = request->resourceRequest();

    // Compute quota accounting BEFORE FrameLoader adds Referer/Origin/User-Agent/
    // Accept/etc. per fetch spec "total request length":
    // https://fetch.spec.whatwg.org/#request-deferred-fetching-total-request-length
    //   totalRequestLength = |URL bytes| + sum(|name| + |value|) over author's
    //                        header list + |body bytes|
    // Extra headers added later by the loader are not charged against the quota.
    RefPtr<FormData> body = resourceRequest.httpBody();
    uint64_t bodyBytes = body ? body->lengthInBytes() : 0;
    URL urlWithoutFragment = resourceRequest.url();
    urlWithoutFragment.removeFragmentIdentifier();
    CheckedUint64 checkedRequestBytes = urlWithoutFragment.string().length();
    checkedRequestBytes += request->referrer().length();
    for (auto& header : resourceRequest.httpHeaderFields()) {
        checkedRequestBytes += header.key.length();
        checkedRequestBytes += header.value.length();
    }
    checkedRequestBytes += bodyBytes;
    uint64_t requestBytes = checkedRequestBytes.hasOverflowed() ? std::numeric_limits<uint64_t>::max() : checkedRequestBytes.value();
    HTTPHeaderMap originalRequestHeaders = resourceRequest.httpHeaderFields();

    // Populate request headers now (Referer, Origin, User-Agent, etc.) while
    // the frame and document are fully active. When we activate the request
    // later, we go directly through LoaderStrategy::startPingLoad and won't
    // have another chance to add these.
    RefPtr frame = localWindow->frame();
    if (frame) {
        String referrer = SecurityPolicy::generateReferrerHeader(document->referrerPolicy(), resourceRequest.url(), frame->loader().outgoingReferrerURL(), OriginAccessPatternsForWebProcess::singleton());
        if (!referrer.isEmpty())
            resourceRequest.setHTTPReferrer(referrer);
        frame->loader().updateRequestAndAddExtraFields(resourceRequest, IsMainResource::No);
    }

    // Build ResourceLoaderOptions. Mirror what NavigatorBeacon uses but keep
    // the mode/credentials/cache/redirect/referrerPolicy the request already
    // computed.
    ResourceLoaderOptions options;
    options.credentials = request->fetchOptions().credentials;
    options.mode = request->fetchOptions().mode;
    options.cache = request->fetchOptions().cache;
    options.redirect = request->fetchOptions().redirect;
    options.referrerPolicy = request->fetchOptions().referrerPolicy;
    options.destination = request->fetchOptions().destination;
    options.integrity = request->fetchOptions().integrity;
    options.keepAlive = true;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::DoPolicyCheck;

    auto& registry = DeferredFetchRegistry::ensure(*document);
    auto reportingOrigin = SecurityOriginData::fromURL(resourceRequest.url());
    // Quota exceeded is reported with:
    //   quota     = remaining bytes available for this reporting origin
    //   requested = total bytes of THIS request (URL + author headers + body)
    if (requestBytes > registry.availableBytesFor(reportingOrigin)) {
        return Exception { ExceptionCode::QuotaExceededError,
            QuotaExceededError::create("fetchLater() exceeded per-origin quota"_s,
                QuotaExceededErrorOptions {
                    .quota = static_cast<double>(registry.availableBytesFor(reportingOrigin)),
                    .requested = static_cast<double>(requestBytes),
                }) };
    }

    std::optional<Seconds> activateAfter;
    if (init.activateAfter)
        activateAfter = Seconds::fromMilliseconds(*init.activateAfter);

    auto result = registry.addDeferredFetch(WTF::move(resourceRequest), WTF::move(originalRequestHeaders), WTF::move(options), WTF::move(body), requestBytes, &request->signal(), activateAfter);
    if (!result) {
        return Exception { ExceptionCode::QuotaExceededError,
            QuotaExceededError::create("fetchLater() exceeded per-origin quota"_s,
                QuotaExceededErrorOptions {
                    .quota = static_cast<double>(registry.availableBytesFor(reportingOrigin)),
                    .requested = static_cast<double>(requestBytes),
                }) };
    }

    return result.releaseNonNull();
}

}
