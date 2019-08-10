/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ServiceWorkerInternals.h"

#if ENABLE(SERVICE_WORKER)

#include "FetchEvent.h"
#include "JSFetchResponse.h"
#include "SWContextManager.h"

namespace WebCore {

ServiceWorkerInternals::ServiceWorkerInternals(ServiceWorkerIdentifier identifier)
    : m_identifier(identifier)
{
}

ServiceWorkerInternals::~ServiceWorkerInternals() = default;

void ServiceWorkerInternals::setOnline(bool isOnline)
{
    callOnMainThread([identifier = m_identifier, isOnline] () {
        if (auto* proxy = SWContextManager::singleton().workerByID(identifier))
            proxy->notifyNetworkStateChange(isOnline);
    });
}

void ServiceWorkerInternals::waitForFetchEventToFinish(FetchEvent& event, DOMPromiseDeferred<IDLInterface<FetchResponse>>&& promise)
{
    event.onResponse([promise = WTFMove(promise), event = makeRef(event)] (auto&& result) mutable {
        if (result.has_value())
            promise.resolve(WTFMove(result.value()));
        else
            promise.reject(TypeError, result.error().localizedDescription());
    });
}

Ref<FetchEvent> ServiceWorkerInternals::createBeingDispatchedFetchEvent(ScriptExecutionContext& context)
{
    auto event = FetchEvent::createForTesting(context);
    event->setEventPhase(Event::CAPTURING_PHASE);
    return event;
}

Ref<FetchResponse> ServiceWorkerInternals::createOpaqueWithBlobBodyResponse(ScriptExecutionContext& context)
{
    auto blob = Blob::create(context.sessionID());
    auto formData = FormData::create();
    formData->appendBlob(blob->url());

    ResourceResponse response;
    response.setType(ResourceResponse::Type::Cors);
    response.setTainting(ResourceResponse::Tainting::Opaque);
    auto fetchResponse = FetchResponse::create(context, FetchBody::fromFormData(context.sessionID(), formData), FetchHeaders::Guard::Response, WTFMove(response));
    fetchResponse->initializeOpaqueLoadIdentifierForTesting();
    return fetchResponse;
}

Vector<String> ServiceWorkerInternals::fetchResponseHeaderList(FetchResponse& response)
{
    Vector<String> headerNames;
    headerNames.reserveInitialCapacity(response.internalResponseHeaders().size());
    for (auto keyValue : response.internalResponseHeaders())
        headerNames.uncheckedAppend(keyValue.key);
    return headerNames;
}

#if !PLATFORM(MAC)
String ServiceWorkerInternals::processName() const
{
    return "none"_s;
}
#endif

bool ServiceWorkerInternals::isThrottleable() const
{
    auto* connection = SWContextManager::singleton().connection();
    return connection ? connection->isThrottleable() : true;
}

} // namespace WebCore

#endif
