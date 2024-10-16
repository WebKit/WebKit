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

#pragma once

#include "EpochTimeStamp.h"
#include "IDLTypes.h"
#include "JSDOMPromiseDeferredForward.h"
#include "ServiceWorkerIdentifier.h"
#include <JavaScriptCore/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class FetchEvent;
class FetchResponse;
class PushSubscription;
class ScriptExecutionContext;
class ServiceWorkerGlobalScope;
class ServiceWorkerClient;

template<typename IDLType> class DOMPromiseDeferred;

class WEBCORE_TESTSUPPORT_EXPORT ServiceWorkerInternals : public RefCounted<ServiceWorkerInternals>, public CanMakeWeakPtr<ServiceWorkerInternals> {
public:
    static Ref<ServiceWorkerInternals> create(ServiceWorkerGlobalScope& globalScope, ServiceWorkerIdentifier identifier) { return adoptRef(*new ServiceWorkerInternals { globalScope, identifier }); }
    ~ServiceWorkerInternals();

    void setOnline(bool isOnline);
    void terminate();

    void waitForFetchEventToFinish(FetchEvent&, DOMPromiseDeferred<IDLInterface<FetchResponse>>&&);
    Ref<FetchEvent> createBeingDispatchedFetchEvent(ScriptExecutionContext&);
    Ref<FetchResponse> createOpaqueWithBlobBodyResponse(ScriptExecutionContext&);

    void schedulePushEvent(const String&, RefPtr<DeferredPromise>&&);
    void schedulePushSubscriptionChangeEvent(PushSubscription* newSubscription, PushSubscription* oldSubscription);
    Vector<String> fetchResponseHeaderList(FetchResponse&);

    String processName() const;

    bool isThrottleable() const;

    int processIdentifier() const;

    void lastNavigationWasAppInitiated(Ref<DeferredPromise>&&);
    
    RefPtr<PushSubscription> createPushSubscription(const String& endpoint, std::optional<EpochTimeStamp> expirationTime, const ArrayBuffer& serverVAPIDPublicKey, const ArrayBuffer& clientECDHPublicKey, const ArrayBuffer& auth);

    bool fetchEventIsSameSite(FetchEvent&);

    String serviceWorkerClientInternalIdentifier(const ServiceWorkerClient&);
    void setAsInspected(bool);
    void enableConsoleMessageReporting(ScriptExecutionContext&);
    void logReportedConsoleMessage(ScriptExecutionContext&, const String&);

private:
    ServiceWorkerInternals(ServiceWorkerGlobalScope&, ServiceWorkerIdentifier);

    ServiceWorkerIdentifier m_identifier;
    RefPtr<DeferredPromise> m_lastNavigationWasAppInitiatedPromise;
    UncheckedKeyHashMap<uint64_t, RefPtr<DeferredPromise>> m_pushEventPromises;
    uint64_t m_pushEventCounter { 0 };
};

} // namespace WebCore
