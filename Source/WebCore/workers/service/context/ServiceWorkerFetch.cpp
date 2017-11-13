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

#include "EventNames.h"
#include "FetchEvent.h"
#include "FetchRequest.h"
#include "FetchResponse.h"
#include "ResourceRequest.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

namespace ServiceWorkerFetch {

static void processResponse(Ref<Client>&& client, FetchResponse* response)
{
    if (!response) {
        client->didFail();
        return;
    }
    auto protectedResponse = makeRef(*response);

    client->didReceiveResponse(response->resourceResponse());

    if (response->hasReadableStreamBody()) {
        // FIXME: We should send the body as chunks.
        response->consumeBodyFromReadableStream([client = WTFMove(client)] (ExceptionOr<RefPtr<SharedBuffer>>&& result) mutable {
            if (result.hasException()) {
                client->didFail();
                return;
            }
            client->didReceiveData(result.releaseReturnValue().releaseNonNull());
            client->didFinish();
        });
        return;
    }
    if (response->isLoading()) {
        // FIXME: We should send the body as chunks.
        response->consumeBodyWhenLoaded([client = WTFMove(client)] (ExceptionOr<RefPtr<SharedBuffer>>&& result) mutable {
            if (result.hasException()) {
                client->didFail();
                return;
            }
            client->didReceiveData(result.releaseReturnValue().releaseNonNull());
            client->didFinish();
        });
        return;
    }

    auto body = response->consumeBody();
    WTF::switchOn(body, [] (Ref<FormData>&) {
        // FIXME: Support FormData response bodies.
    }, [&] (Ref<SharedBuffer>& buffer) {
        client->didReceiveData(WTFMove(buffer));
    }, [] (std::nullptr_t&) {
    });

    client->didFinish();
}

Ref<FetchEvent> dispatchFetchEvent(Ref<Client>&& client, WorkerGlobalScope& globalScope, ResourceRequest&& request, FetchOptions&& options)
{
    ASSERT(globalScope.isServiceWorkerGlobalScope());

    // FIXME: Set request body and referrer.
    auto requestHeaders = FetchHeaders::create(FetchHeaders::Guard::Immutable, HTTPHeaderMap { request.httpHeaderFields() });
    auto fetchRequest = FetchRequest::create(globalScope, std::nullopt, WTFMove(requestHeaders),  WTFMove(request), WTFMove(options), { });

    // FIXME: Initialize other FetchEvent::Init fields.
    FetchEvent::Init init;
    init.request = WTFMove(fetchRequest);
    init.cancelable = true;
    auto event = FetchEvent::create(eventNames().fetchEvent, WTFMove(init), Event::IsTrusted::Yes);

    event->onResponse([client = client.copyRef()] (FetchResponse* response) mutable {
        processResponse(WTFMove(client), response);
    });

    globalScope.dispatchEvent(event);

    if (!event->respondWithEntered()) {
        if (event->defaultPrevented()) {
            client->didFail();
            return event;
        }
        client->didNotHandle();
        // FIXME: Handle soft update.
    }
    return event;
}

} // namespace ServiceWorkerFetch

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
