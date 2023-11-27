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

#include "FetchEvent.h"
#include "FetchRequest.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFetchResponse.h"
#include "NotificationPayload.h"
#include "PushSubscription.h"
#include "PushSubscriptionData.h"
#include "SWContextManager.h"
#include "ServiceWorkerClient.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerRegistration.h"
#include <wtf/ProcessID.h>

namespace WebCore {

ServiceWorkerInternals::ServiceWorkerInternals(ServiceWorkerGlobalScope& globalScope, ServiceWorkerIdentifier identifier)
    : m_identifier(identifier)
{
    globalScope.setIsProcessingUserGestureForTesting(true);
}

ServiceWorkerInternals::~ServiceWorkerInternals() = default;

void ServiceWorkerInternals::setOnline(bool isOnline)
{
    callOnMainThread([identifier = m_identifier, isOnline] () {
        if (auto* proxy = SWContextManager::singleton().serviceWorkerThreadProxy(identifier))
            proxy->notifyNetworkStateChange(isOnline);
    });
}

void ServiceWorkerInternals::terminate()
{
    callOnMainThread([identifier = m_identifier] () {
        SWContextManager::singleton().terminateWorker(identifier, Seconds::infinity(), [] { });
    });
}

void ServiceWorkerInternals::schedulePushEvent(const String& message, RefPtr<DeferredPromise>&& promise)
{
    auto counter = ++m_pushEventCounter;
    m_pushEventPromises.add(counter, WTFMove(promise));

    std::optional<Vector<uint8_t>> data;
    if (!message.isNull()) {
        auto utf8 = message.utf8();
        data = Vector<uint8_t> { reinterpret_cast<const uint8_t*>(utf8.data()), utf8.length()};
    }
    callOnMainThread([identifier = m_identifier, data = WTFMove(data), weakThis = WeakPtr { *this }, counter]() mutable {
        SWContextManager::singleton().firePushEvent(identifier, WTFMove(data), std::nullopt, [identifier, weakThis = WTFMove(weakThis), counter](bool result, std::optional<NotificationPayload>&&) mutable {
            if (auto* proxy = SWContextManager::singleton().serviceWorkerThreadProxy(identifier)) {
                proxy->thread().runLoop().postTaskForMode([weakThis = WTFMove(weakThis), counter, result](auto&) {
                    if (!weakThis)
                        return;
                    if (auto promise = weakThis->m_pushEventPromises.take(counter))
                        promise->resolve<IDLBoolean>(result);
                }, WorkerRunLoop::defaultMode());
            }
        });
    });
}

void ServiceWorkerInternals::schedulePushSubscriptionChangeEvent(PushSubscription* newSubscription, PushSubscription* oldSubscription)
{
    std::optional<PushSubscriptionData> newSubscriptionData;
    std::optional<PushSubscriptionData> oldSubscriptionData;

    if (newSubscription)
        newSubscriptionData = newSubscription->data().isolatedCopy();
    if (oldSubscription)
        oldSubscriptionData = oldSubscription->data().isolatedCopy();

    callOnMainThread([identifier = m_identifier, newSubscriptionData = WTFMove(newSubscriptionData), oldSubscriptionData = WTFMove(oldSubscriptionData)]() mutable {
        SWContextManager::singleton().firePushSubscriptionChangeEvent(identifier, WTFMove(newSubscriptionData), WTFMove(oldSubscriptionData));
    });
}

void ServiceWorkerInternals::waitForFetchEventToFinish(FetchEvent& event, DOMPromiseDeferred<IDLInterface<FetchResponse>>&& promise)
{
    event.onResponse([promise = WTFMove(promise), event = Ref { event }] (auto&& result) mutable {
        if (!result.has_value()) {
            String description;
            if (auto& error = result.error())
                description = error->localizedDescription();
            promise.reject(ExceptionCode::TypeError, description);
            return;
        }
        promise.resolve(WTFMove(result.value()));
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
    auto blob = Blob::create(&context);
    auto formData = FormData::create();
    formData->appendBlob(blob->url());

    ResourceResponse response;
    response.setType(ResourceResponse::Type::Cors);
    response.setTainting(ResourceResponse::Tainting::Opaque);
    auto fetchResponse = FetchResponse::create(&context, FetchBody::fromFormData(context, WTFMove(formData)), FetchHeaders::Guard::Response, WTFMove(response));
    fetchResponse->initializeOpaqueLoadIdentifierForTesting();
    return fetchResponse;
}

Vector<String> ServiceWorkerInternals::fetchResponseHeaderList(FetchResponse& response)
{
    return WTF::map(response.internalResponseHeaders(), [](auto& keyValue) {
        return keyValue.key;
    });
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

int ServiceWorkerInternals::processIdentifier() const
{
    return getCurrentProcessID();
}

void ServiceWorkerInternals::lastNavigationWasAppInitiated(Ref<DeferredPromise>&& promise)
{
    ASSERT(!m_lastNavigationWasAppInitiatedPromise);
    m_lastNavigationWasAppInitiatedPromise = WTFMove(promise);
    callOnMainThread([identifier = m_identifier, weakThis = WeakPtr { *this }]() mutable {
        if (auto* proxy = SWContextManager::singleton().serviceWorkerThreadProxy(identifier)) {
            proxy->thread().runLoop().postTaskForMode([weakThis = WTFMove(weakThis), appInitiated = proxy->lastNavigationWasAppInitiated()](auto&) {
                if (!weakThis || !weakThis->m_lastNavigationWasAppInitiatedPromise)
                    return;

                weakThis->m_lastNavigationWasAppInitiatedPromise->resolve<IDLBoolean>(appInitiated);
                weakThis->m_lastNavigationWasAppInitiatedPromise = nullptr;
            }, WorkerRunLoop::defaultMode());
        }
    });
}

RefPtr<PushSubscription> ServiceWorkerInternals::createPushSubscription(const String& endpoint, std::optional<EpochTimeStamp> expirationTime, const ArrayBuffer& serverVAPIDPublicKey, const ArrayBuffer& clientECDHPublicKey, const ArrayBuffer& auth)
{
    auto myEndpoint = endpoint;
    Vector<uint8_t> myServerVAPIDPublicKey { static_cast<const uint8_t*>(serverVAPIDPublicKey.data()), serverVAPIDPublicKey.byteLength() };
    Vector<uint8_t> myClientECDHPublicKey { static_cast<const uint8_t*>(clientECDHPublicKey.data()), clientECDHPublicKey.byteLength() };
    Vector<uint8_t> myAuth { static_cast<const uint8_t*>(auth.data()), auth.byteLength() };

    return PushSubscription::create(PushSubscriptionData { { }, WTFMove(myEndpoint), expirationTime, WTFMove(myServerVAPIDPublicKey), WTFMove(myClientECDHPublicKey), WTFMove(myAuth) });
}

bool ServiceWorkerInternals::fetchEventIsSameSite(FetchEvent& event)
{
    return event.request().internalRequest().isSameSite();
}

String ServiceWorkerInternals::serviceWorkerClientInternalIdentifier(const ServiceWorkerClient& client)
{
    return client.identifier().toString();
}

void ServiceWorkerInternals::setAsInspected(bool isInspected)
{
    SWContextManager::singleton().setAsInspected(m_identifier, isInspected);
}

void ServiceWorkerInternals::enableConsoleMessageReporting(ScriptExecutionContext& context)
{
    downcast<ServiceWorkerGlobalScope>(context).enableConsoleMessageReporting();
}

void ServiceWorkerInternals:: logReportedConsoleMessage(ScriptExecutionContext& context, const String& value)
{
    downcast<ServiceWorkerGlobalScope>(context).addConsoleMessage(MessageSource::Storage, MessageLevel::Info, value, 0);
}

} // namespace WebCore
