/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "WebPushDaemonConnection.h"
#include <optional>

namespace WebCore {
enum class PushPermissionState : uint8_t;
struct ExceptionData;
struct NotificationData;
struct PushSubscriptionData;
}

namespace WebKit {
namespace WebPushD {
class Connection;
struct WebPushDaemonConnectionConfiguration;
}
struct WebPushMessage;
}

namespace API {

class WebPushDaemonConnection final : public ObjectImpl<Object::Type::WebPushDaemonConnection> {
public:
    WebPushDaemonConnection(const WTF::String& machServiceName, WebKit::WebPushD::WebPushDaemonConnectionConfiguration&&);

    void getPushPermissionState(const WTF::URL&, CompletionHandler<void(WebCore::PushPermissionState)>&&);
    void requestPushPermission(const WTF::URL&, CompletionHandler<void(bool)>&&);
    void setAppBadge(const WTF::URL&, std::optional<uint64_t>);
    void subscribeToPushService(const WTF::URL&, const Vector<uint8_t>& applicationServerKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&&);
    void unsubscribeFromPushService(const WTF::URL&, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&&);
    void getPushSubscription(const WTF::URL&, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&&);
    void getNextPendingPushMessage(CompletionHandler<void(const std::optional<WebKit::WebPushMessage>&)>&&);

    void showNotification(const WebCore::NotificationData&, CompletionHandler<void()>&&);
    void getNotifications(const WTF::URL&, const WTF::String& tag, CompletionHandler<void(const Expected<Vector<WebCore::NotificationData>, WebCore::ExceptionData>&)>&&);
    void cancelNotification(const WTF::URL&, const WTF::UUID&);

private:
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    Ref<WebKit::WebPushD::Connection> protectedConnection() const;

    Ref<WebKit::WebPushD::Connection> m_connection;
#endif
};

} // namespace API
