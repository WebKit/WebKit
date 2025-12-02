/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#pragma once

#include "ActiveDOMObject.h"
#include "CookieStoreManager.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferredForward.h"
#include "Notification.h"
#include "NotificationOptions.h"
#include "PushPermissionState.h"
#include "PushSubscription.h"
#include "PushSubscriptionOwner.h"
#include "SWClientConnection.h"
#include "ServiceWorkerRegistrationData.h"
#include "Supplementable.h"
#include "Timer.h"
#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class DeferredPromise;
class NavigationPreloadManager;
class ScriptExecutionContext;
class ServiceWorker;
class ServiceWorkerContainer;
class WebCoreOpaqueRoot;
struct CookieStoreGetOptions;

class ServiceWorkerRegistration final : public RefCounted<ServiceWorkerRegistration>, public Supplementable<ServiceWorkerRegistration>, public EventTarget, public ActiveDOMObject, public PushSubscriptionOwner {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED_EXPORT(ServiceWorkerRegistration, WEBCORE_EXPORT);
public:
    static Ref<ServiceWorkerRegistration> getOrCreate(ScriptExecutionContext&, Ref<ServiceWorkerContainer>&&, ServiceWorkerRegistrationData&&);

    WEBCORE_EXPORT ~ServiceWorkerRegistration();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    ServiceWorkerRegistrationIdentifier identifier() const { return m_registrationData.identifier; }

    ServiceWorker* installing();
    ServiceWorker* waiting();
    ServiceWorker* active();

    bool isActive() const final { return !!m_activeWorker; }

    ServiceWorker* getNewestWorker() const;

    const String& scope() const;

    ServiceWorkerUpdateViaCache updateViaCache() const;
    void setUpdateViaCache(ServiceWorkerUpdateViaCache);

    WallTime lastUpdateTime() const;
    void setLastUpdateTime(WallTime);

    bool needsUpdate() const { return lastUpdateTime() && (WallTime::now() - lastUpdateTime()) > 86400_s; }

    void update(Ref<DeferredPromise>&&);
    void unregister(Ref<DeferredPromise>&&);

    void subscribeToPushService(const Vector<uint8_t>& applicationServerKey, DOMPromiseDeferred<IDLInterface<PushSubscription>>&&);
    void unsubscribeFromPushService(std::optional<PushSubscriptionIdentifier>, DOMPromiseDeferred<IDLBoolean>&&);
    void getPushSubscription(DOMPromiseDeferred<IDLNullable<IDLInterface<PushSubscription>>>&&);
    void getPushPermissionState(DOMPromiseDeferred<IDLEnumeration<PushPermissionState>>&&);

    const ServiceWorkerRegistrationData& data() const { return m_registrationData; }

    void updateStateFromServer(ServiceWorkerRegistrationState, RefPtr<ServiceWorker>&&);
    void queueTaskToFireUpdateFoundEvent();

    NavigationPreloadManager& navigationPreload();
    ServiceWorkerContainer& container() { return m_container.get(); }

#if ENABLE(NOTIFICATION_EVENT)
    struct GetNotificationOptions {
        String tag;
    };

    void showNotification(ScriptExecutionContext&, String&& title, NotificationOptions&&, Ref<DeferredPromise>&&);
    void getNotifications(const GetNotificationOptions& filter, DOMPromiseDeferred<IDLSequence<IDLInterface<Notification>>>);
#endif

    CookieStoreManager& cookies();
    void addCookieChangeSubscriptions(Vector<CookieStoreGetOptions>&&, Ref<DeferredPromise>&&);
    void removeCookieChangeSubscriptions(Vector<CookieStoreGetOptions>&&, Ref<DeferredPromise>&&);
    void cookieChangeSubscriptions(Ref<DeferredPromise>&&);

private:
    ServiceWorkerRegistration(ScriptExecutionContext&, Ref<ServiceWorkerContainer>&&, ServiceWorkerRegistrationData&&);

    enum EventTargetInterfaceType eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
    using ActiveDOMObject::protectedScriptExecutionContext;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject.
    void stop() final;
    bool virtualHasPendingActivity() const final;

    ServiceWorkerRegistrationData m_registrationData;
    const Ref<ServiceWorkerContainer> m_container;

    RefPtr<ServiceWorker> m_installingWorker;
    RefPtr<ServiceWorker> m_waitingWorker;
    RefPtr<ServiceWorker> m_activeWorker;

    std::unique_ptr<NavigationPreloadManager> m_navigationPreload;

    RefPtr<CookieStoreManager> m_cookieStoreManager;
};

WebCoreOpaqueRoot root(ServiceWorkerRegistration*);

} // namespace WebCore
