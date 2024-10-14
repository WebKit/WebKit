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

#include "config.h"
#include "APIWebPushDaemonConnection.h"

#include "MessageSenderInlines.h"
#include "PushClientConnectionMessages.h"
#include "WebPushMessage.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/NotificationData.h>
#include <WebCore/PushPermissionState.h>
#include <WebCore/PushSubscriptionData.h>

namespace API {

WebPushDaemonConnection::WebPushDaemonConnection(const WTF::String& machServiceName, WebKit::WebPushD::WebPushDaemonConnectionConfiguration&& configuration)
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    : m_connection(WebKit::WebPushD::Connection::create(machServiceName.utf8(), WTFMove(configuration)))
#endif
{
}

void WebPushDaemonConnection::getPushPermissionState(const WTF::URL& scopeURL, CompletionHandler<void(WebCore::PushPermissionState)>&& completionHandler)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPushPermissionState(SecurityOriginData::fromURL(scopeURL)), WTFMove(completionHandler));
#else
    completionHandler(WebCore::PushPermissionState::Denied);
#endif
}

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
Ref<WebKit::WebPushD::Connection> WebPushDaemonConnection::protectedConnection() const
{
    return m_connection;
}
#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)

void WebPushDaemonConnection::requestPushPermission(const WTF::URL& scopeURL, CompletionHandler<void(bool)>&& completionHandler)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::RequestPushPermission(SecurityOriginData::fromURL(scopeURL)), WTFMove(completionHandler));
#else
    completionHandler(false);
#endif
}

void WebPushDaemonConnection::setAppBadge(const WTF::URL& scopeURL, std::optional<uint64_t> badge)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithoutUsingIPCConnection(Messages::PushClientConnection::SetAppBadge(SecurityOriginData::fromURL(scopeURL), badge));
#endif
}

void WebPushDaemonConnection::subscribeToPushService(const WTF::URL& scopeURL, const Vector<uint8_t>& applicationServerKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& completionHandler)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::SubscribeToPushService(scopeURL, applicationServerKey), WTFMove(completionHandler));
#else
    completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::UnknownError, "Cannot subscribe to push service"_s }));
#endif
}

void WebPushDaemonConnection::unsubscribeFromPushService(const WTF::URL& scopeURL, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& completionHandler)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::UnsubscribeFromPushService(scopeURL, std::nullopt), WTFMove(completionHandler));
#else
    completionHandler(false);
#endif
}

void WebPushDaemonConnection::getPushSubscription(const WTF::URL& scopeURL, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& completionHandler)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPushSubscription(WTFMove(scopeURL)), WTFMove(completionHandler));
#else
    completionHandler(std::optional<WebCore::PushSubscriptionData> { });
#endif
}

void WebPushDaemonConnection::getNextPendingPushMessage(CompletionHandler<void(const std::optional<WebKit::WebPushMessage>&)>&& completionHandler)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPendingPushMessage(), WTFMove(completionHandler));
#else
    completionHandler(std::nullopt);
#endif
}

void WebPushDaemonConnection::showNotification(const WebCore::NotificationData& notificationData, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::ShowNotification { notificationData, { } }, WTFMove(completionHandler));
#else
    completionHandler();
#endif
}

void WebPushDaemonConnection::getNotifications(const WTF::URL& scopeURL, const WTF::String& tag, CompletionHandler<void(const Expected<Vector<WebCore::NotificationData>, WebCore::ExceptionData>&)>&& completionHandler)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetNotifications { scopeURL, tag }, WTFMove(completionHandler));
#else
    completionHandler({ });
#endif
}

void WebPushDaemonConnection::cancelNotification(const WTF::URL& scopeURL, const WTF::UUID& uuid)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    protectedConnection()->sendWithoutUsingIPCConnection(Messages::PushClientConnection::CancelNotification(SecurityOriginData::fromURL(scopeURL), uuid));
#endif
}

} // namespace API

