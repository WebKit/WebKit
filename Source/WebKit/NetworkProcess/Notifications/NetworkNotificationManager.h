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

#if ENABLE(BUILT_IN_NOTIFICATIONS)

#include "NotificationManagerMessageHandler.h"
#include "WebPushDaemonConnection.h"
#include "WebPushDaemonConnectionConfiguration.h"
#include "WebPushMessage.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/NotificationDirection.h>
#include <WebCore/PushSubscriptionData.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class NotificationResources;
struct SecurityOriginData;
}

namespace WebKit {

namespace WebPushD {
enum class MessageType : uint8_t;
}

class NetworkSession;

class NetworkNotificationManager : public NotificationManagerMessageHandler {
    WTF_MAKE_FAST_ALLOCATED;
    friend class NetworkSession;
public:
    NetworkSession& networkSession() const { return m_networkSession; }

    void setPushAndNotificationsEnabledForOrigin(const WebCore::SecurityOriginData&, bool, CompletionHandler<void()>&&);
    void deletePushAndNotificationRegistration(const WebCore::SecurityOriginData&, CompletionHandler<void(const String&)>&&);
    void getOriginsWithPushAndNotificationPermissions(CompletionHandler<void(const Vector<WebCore::SecurityOriginData>&)>&&);
    void getPendingPushMessages(CompletionHandler<void(const Vector<WebPushMessage>&)>&&);

    void subscribeToPushService(URL&& scopeURL, Vector<uint8_t>&& applicationServerKey, CompletionHandler<void(Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&&)>&&);
    void unsubscribeFromPushService(URL&& scopeURL, std::optional<WebCore::PushSubscriptionIdentifier>, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&&);
    void getPushSubscription(URL&& scopeURL, CompletionHandler<void(Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&&)>&&);
    void getPushPermissionState(URL&& scopeURL, CompletionHandler<void(Expected<uint8_t, WebCore::ExceptionData>&&)>&&);
    void incrementSilentPushCount(WebCore::SecurityOriginData&&, CompletionHandler<void(unsigned)>&&);
    void removeAllPushSubscriptions(CompletionHandler<void(unsigned)>&&);
    void removePushSubscriptionsForOrigin(WebCore::SecurityOriginData&&, CompletionHandler<void(unsigned)>&&);

private:
    NetworkNotificationManager(NetworkSession&, const String& webPushMachServiceName, WebPushD::WebPushDaemonConnectionConfiguration&&);

    void requestSystemNotificationPermission(const String& originString, CompletionHandler<void(bool)>&&) final;
    void showNotification(IPC::Connection&, const WebCore::NotificationData&, RefPtr<WebCore::NotificationResources>&&, CompletionHandler<void()>&&) final;
    void cancelNotification(const UUID& notificationID) final;
    void clearNotifications(const Vector<UUID>& notificationIDs) final;
    void didDestroyNotification(const UUID& notificationID) final;

    NetworkSession& m_networkSession;
    std::unique_ptr<WebPushD::Connection> m_connection;

    template<WebPushD::MessageType messageType, typename... Args>
    void sendMessage(Args&&...) const;
    template<WebPushD::MessageType messageType, typename... Args, typename... ReplyArgs>
    void sendMessageWithReply(CompletionHandler<void(ReplyArgs...)>&&, Args&&...) const;
};

} // namespace WebKit

#endif // ENABLE(BUILT_IN_NOTIFICATIONS)
