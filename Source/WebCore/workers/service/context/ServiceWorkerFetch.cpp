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
#include "ServiceWorkerFetch.h"

#if ENABLE(SERVICE_WORKER)

#include "CrossOriginAccessControl.h"
#include "EventNames.h"
#include "FetchEvent.h"
#include "FetchRequest.h"
#include "FetchResponse.h"
#include "MIMETypeRegistry.h"
#include "ReadableStreamChunk.h"
#include "ResourceRequest.h"
#include "ServiceWorker.h"
#include "ServiceWorkerClientIdentifier.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerThread.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

namespace ServiceWorkerFetch {

// https://fetch.spec.whatwg.org/#http-fetch step 3.3
static inline Optional<ResourceError> validateResponse(const ResourceResponse& response, FetchOptions::Mode mode, FetchOptions::Redirect redirect)
{
    if (response.type() == ResourceResponse::Type::Error)
        return ResourceError { errorDomainWebKitInternal, 0, response.url(), "Response served by service worker is an error"_s, ResourceError::Type::General };

    if (mode != FetchOptions::Mode::NoCors && response.tainting() == ResourceResponse::Tainting::Opaque)
        return ResourceError { errorDomainWebKitInternal, 0, response.url(), "Response served by service worker is opaque"_s, ResourceError::Type::AccessControl };

    // Navigate mode induces manual redirect.
    if (redirect != FetchOptions::Redirect::Manual && mode != FetchOptions::Mode::Navigate && response.tainting() == ResourceResponse::Tainting::Opaqueredirect)
        return ResourceError { errorDomainWebKitInternal, 0, response.url(), "Response served by service worker is opaque redirect"_s, ResourceError::Type::AccessControl };

    if ((redirect != FetchOptions::Redirect::Follow || mode == FetchOptions::Mode::Navigate) && response.isRedirected())
        return ResourceError { errorDomainWebKitInternal, 0, response.url(), "Response served by service worker has redirections"_s, ResourceError::Type::AccessControl };

    return { };
}

static void processResponse(Ref<Client>&& client, Expected<Ref<FetchResponse>, ResourceError>&& result, FetchOptions::Mode mode, FetchOptions::Redirect redirect, const URL& requestURL, CertificateInfo&& certificateInfo)
{
    if (!result.has_value()) {
        client->didFail(result.error());
        return;
    }
    auto response = WTFMove(result.value());

    auto loadingError = response->loadingError();
    if (!loadingError.isNull()) {
        client->didFail(loadingError);
        return;
    }

    auto resourceResponse = response->resourceResponse();
    if (auto error = validateResponse(resourceResponse, mode, redirect)) {
        client->didFail(error.value());
        return;
    }

    if (resourceResponse.isRedirection() && resourceResponse.httpHeaderFields().contains(HTTPHeaderName::Location)) {
        client->didReceiveRedirection(resourceResponse);
        return;
    }

    // In case of main resource and mime type is the default one, we set it to text/html to pass more service worker WPT tests.
    // FIXME: We should refine our MIME type sniffing strategy for synthetic responses.
    if (mode == FetchOptions::Mode::Navigate) {
        if (resourceResponse.mimeType() == defaultMIMEType()) {
            resourceResponse.setMimeType("text/html"_s);
            resourceResponse.setTextEncodingName("UTF-8"_s);
        }

        if (!resourceResponse.certificateInfo())
            resourceResponse.setCertificateInfo(WTFMove(certificateInfo));
    }

    // As per https://fetch.spec.whatwg.org/#main-fetch step 9, copy request's url list in response's url list if empty.
    if (resourceResponse.url().isNull())
        resourceResponse.setURL(requestURL);

    client->didReceiveResponse(resourceResponse);

    if (response->isBodyReceivedByChunk()) {
        response->consumeBodyReceivedByChunk([client = WTFMove(client)] (auto&& result) mutable {
            if (result.hasException()) {
                client->didFail(FetchEvent::createResponseError(URL { }, result.exception().message()));
                return;
            }

            if (auto chunk = result.returnValue())
                client->didReceiveData(SharedBuffer::create(reinterpret_cast<const char*>(chunk->data), chunk->size));
            else
                client->didFinish();
        });
        return;
    }

    auto body = response->consumeBody();
    WTF::switchOn(body, [&] (Ref<FormData>& formData) {
        client->didReceiveFormDataAndFinish(WTFMove(formData));
    }, [&] (Ref<SharedBuffer>& buffer) {
        client->didReceiveData(WTFMove(buffer));
        client->didFinish();
    }, [&] (std::nullptr_t&) {
        client->didFinish();
    });
}

void dispatchFetchEvent(Ref<Client>&& client, ServiceWorkerGlobalScope& globalScope, Optional<ServiceWorkerClientIdentifier> clientId, ResourceRequest&& request, String&& referrer, FetchOptions&& options)
{
    auto requestHeaders = FetchHeaders::create(FetchHeaders::Guard::Immutable, HTTPHeaderMap { request.httpHeaderFields() });

    FetchOptions::Mode mode = options.mode;
    FetchOptions::Redirect redirect = options.redirect;

    bool isNavigation = options.mode == FetchOptions::Mode::Navigate;

    ASSERT(globalScope.registration().active());
    ASSERT(globalScope.registration().active()->identifier() == globalScope.thread().identifier());
    ASSERT(globalScope.registration().active()->state() == ServiceWorkerState::Activated);

    auto* formData = request.httpBody();
    Optional<FetchBody> body;
    if (formData && !formData->isEmpty()) {
        body = FetchBody::fromFormData(*formData);
        if (!body) {
            client->didNotHandle();
            return;
        }
    }
    // FIXME: loading code should set redirect mode to manual.
    if (isNavigation)
        options.redirect = FetchOptions::Redirect::Manual;

    URL requestURL = request.url();
    auto fetchRequest = FetchRequest::create(globalScope, WTFMove(body), WTFMove(requestHeaders),  WTFMove(request), WTFMove(options), WTFMove(referrer));

    FetchEvent::Init init;
    init.request = WTFMove(fetchRequest);
    if (isNavigation) {
        // FIXME: Set reservedClientId.
        if (clientId)
            init.targetClientId = clientId->toString();
    } else if (clientId)
        init.clientId = clientId->toString();
    init.cancelable = true;
    auto event = FetchEvent::create(eventNames().fetchEvent, WTFMove(init), Event::IsTrusted::Yes);

    CertificateInfo certificateInfo = globalScope.certificateInfo();

    event->onResponse([client, mode, redirect, requestURL, certificateInfo = WTFMove(certificateInfo)] (auto&& result) mutable {
        processResponse(WTFMove(client), WTFMove(result), mode, redirect, requestURL, WTFMove(certificateInfo));
    });

    globalScope.dispatchEvent(event);

    if (!event->respondWithEntered()) {
        if (event->defaultPrevented()) {
            client->didFail(ResourceError { errorDomainWebKitInternal, 0, requestURL, "Fetch event was canceled"_s });
            return;
        }
        client->didNotHandle();
    }

    globalScope.updateExtendedEventsSet(event.ptr());
}

} // namespace ServiceWorkerFetch

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
