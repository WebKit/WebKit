/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "MessageReceiver.h"
#include "WebContextSupplement.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/NotificationClient.h>
#include <wtf/HashMap.h>
#include <wtf/UUID.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
struct NotificationData;
}

namespace API {
class Array;
class NotificationProvider;
class SecurityOrigin;
}

namespace WebKit {

class WebNotification;
class WebPageProxy;
class WebProcessPool;

class WebNotificationManagerProxy : public API::ObjectImpl<API::Object::Type::NotificationManager>, public WebContextSupplement {
public:

    static const char* supplementName();

    static Ref<WebNotificationManagerProxy> create(WebProcessPool*);

    static WebNotificationManagerProxy& sharedServiceWorkerManager();

    void setProvider(std::unique_ptr<API::NotificationProvider>&&);
    HashMap<String, bool> notificationPermissions();

    void show(WebPageProxy*, IPC::Connection&, const WebCore::NotificationData&);
    void cancel(WebPageProxy*, const UUID& pageNotificationID);
    void clearNotifications(WebPageProxy*);
    void clearNotifications(WebPageProxy*, const Vector<UUID>& pageNotificationIDs);
    void didDestroyNotification(WebPageProxy*, const UUID& pageNotificationID);

    void providerDidShowNotification(uint64_t notificationID);
    void providerDidClickNotification(uint64_t notificationID);
    void providerDidClickNotification(const UUID& notificationID);
    void providerDidCloseNotifications(API::Array* notificationIDs);
    void providerDidUpdateNotificationPolicy(const API::SecurityOrigin*, bool allowed);
    void providerDidRemoveNotificationPolicies(API::Array* origins);

    using API::Object::ref;
    using API::Object::deref;

private:
    explicit WebNotificationManagerProxy(WebProcessPool*);

    // WebContextSupplement
    void processPoolDestroyed() override;
    void refWebContextSupplement() override;
    void derefWebContextSupplement() override;

    std::unique_ptr<API::NotificationProvider> m_provider;

    HashMap<uint64_t, UUID> m_globalNotificationMap;
    HashMap<UUID, Ref<WebNotification>> m_notifications;
};

} // namespace WebKit
