/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_PUSH_NOTIFICATIONS)

#include "NotificationManagerMessageHandler.h"
#include "WebPushDaemonConnection.h"
#include "WebPushDaemonConnectionConfiguration.h"
#include "WebPushMessage.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/NotificationDirection.h>
#include <WebCore/PushSubscriptionData.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class NotificationResources;
class SecurityOriginData;
}

namespace WebKit {

namespace WebPushD {
enum class MessageType : uint8_t;
}

class NetworkNotificationManager : public NotificationManagerMessageHandler {
    WTF_MAKE_TZONE_ALLOCATED(NetworkNotificationManager);
public:
    NetworkNotificationManager(const String& webPushMachServiceName, WebPushD::WebPushDaemonConnectionConfiguration&&);

    void setPushAndNotificationsEnabledForOrigin(const WebCore::SecurityOriginData&, bool, CompletionHandler<void()>&&);
    void getPendingPushMessage(CompletionHandler<void(const std::optional<WebPushMessage>&)>&&);
    void getPendingPushMessages(CompletionHandler<void(const Vector<WebPushMessage>&)>&&);

    void subscribeToPushService(URL&& scopeURL, Vector<uint8_t>&& applicationServerKey, CompletionHandler<void(Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&&)>&&);
    void unsubscribeFromPushService(URL&& scopeURL, std::optional<WebCore::PushSubscriptionIdentifier>, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&&);
    void getPushSubscription(URL&& scopeURL, CompletionHandler<void(Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&&)>&&);

    void requestPermission(WebCore::SecurityOriginData&&, CompletionHandler<void(bool)>&&) final;
    void getPermissionState(WebCore::SecurityOriginData&&, CompletionHandler<void(WebCore::PushPermissionState)>&&) final;

    void incrementSilentPushCount(WebCore::SecurityOriginData&&, CompletionHandler<void(unsigned)>&&);
    void removeAllPushSubscriptions(CompletionHandler<void(unsigned)>&&);
    void removePushSubscriptionsForOrigin(WebCore::SecurityOriginData&&, CompletionHandler<void(unsigned)>&&);

    void showNotification(const WebCore::NotificationData&, RefPtr<WebCore::NotificationResources>&&, CompletionHandler<void()>&&);
    void getNotifications(const URL& registrationURL, const String& tag, CompletionHandler<void(Expected<Vector<WebCore::NotificationData>, WebCore::ExceptionData>&&)>&&);
    void clearNotifications(const Vector<WTF::UUID>& notificationIDs) final;

    void getAppBadgeForTesting(CompletionHandler<void(std::optional<uint64_t>)>&&);
    void setAppBadge(const WebCore::SecurityOriginData&, std::optional<uint64_t> badge) final;

private:
    void showNotification(IPC::Connection&, const WebCore::NotificationData&, RefPtr<WebCore::NotificationResources>&&, CompletionHandler<void()>&&) final;
    void cancelNotification(WebCore::SecurityOriginData&&, const WTF::UUID& notificationID) final;
    void didDestroyNotification(const WTF::UUID& notificationID) final;
    void pageWasNotifiedOfNotificationPermission() final { }
    void getPermissionStateSync(WebCore::SecurityOriginData&&, CompletionHandler<void(WebCore::PushPermissionState)>&&) final;

    RefPtr<WebPushD::Connection> protectedConnection() const;

    RefPtr<WebPushD::Connection> m_connection;
};

} // namespace WebKit

#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)
