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
#include <WebCore/NotificationClient.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
enum class NotificationDirection : uint8_t;
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

    void setProvider(std::unique_ptr<API::NotificationProvider>&&);
    HashMap<String, bool> notificationPermissions();

    void show(WebPageProxy*, const String& title, const String& body, const String& iconURL, const String& tag, const String& lang, WebCore::NotificationDirection, const String& originString, uint64_t pageNotificationID);
    void cancel(WebPageProxy*, uint64_t pageNotificationID);
    void clearNotifications(WebPageProxy*);
    void clearNotifications(WebPageProxy*, const Vector<uint64_t>& pageNotificationIDs);
    void didDestroyNotification(WebPageProxy*, uint64_t pageNotificationID);

    void providerDidShowNotification(uint64_t notificationID);
    void providerDidClickNotification(uint64_t notificationID);
    void providerDidCloseNotifications(API::Array* notificationIDs);
    void providerDidUpdateNotificationPolicy(const API::SecurityOrigin*, bool allowed);
    void providerDidRemoveNotificationPolicies(API::Array* origins);

    uint64_t notificationLocalIDForTesting(WebNotification*);

    using API::Object::ref;
    using API::Object::deref;

private:
    explicit WebNotificationManagerProxy(WebProcessPool*);

    typedef bool (*NotificationFilterFunction)(WebCore::PageIdentifier pageID, uint64_t pageNotificationID, WebCore::PageIdentifier desiredPageID, const Vector<uint64_t>& desiredPageNotificationIDs);
    void clearNotifications(WebPageProxy*, const Vector<uint64_t>& pageNotificationIDs, NotificationFilterFunction);

    // WebContextSupplement
    void processPoolDestroyed() override;
    void refWebContextSupplement() override;
    void derefWebContextSupplement() override;

    std::unique_ptr<API::NotificationProvider> m_provider;
    // Pair comprised of web page ID and the web process's notification ID
    HashMap<uint64_t, std::pair<WebCore::PageIdentifier, uint64_t>> m_globalNotificationMap;
    // Key pair comprised of web page ID and the web process's notification ID; value pair comprised of global notification ID, and notification object
    HashMap<std::pair<WebCore::PageIdentifier, uint64_t>, std::pair<uint64_t, RefPtr<WebNotification>>> m_notifications;
};

} // namespace WebKit
