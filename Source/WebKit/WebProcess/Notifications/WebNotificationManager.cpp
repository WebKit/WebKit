/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "WebNotificationManager.h"

#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"
#include "WebProcessCreationParameters.h"

#if ENABLE(NOTIFICATIONS)
#include "NetworkProcessConnection.h"
#include "NotificationManagerMessageHandlerMessages.h"
#include "ServiceWorkerNotificationHandler.h"
#include "WebNotification.h"
#include "WebNotificationManagerMessages.h"
#include "WebPageProxyMessages.h"
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/Document.h>
#include <WebCore/Notification.h>
#include <WebCore/NotificationData.h>
#include <WebCore/Page.h>
#include <WebCore/SWContextManager.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/UserGestureIndicator.h>
#endif

namespace WebKit {
using namespace WebCore;

const char* WebNotificationManager::supplementName()
{
    return "WebNotificationManager";
}

WebNotificationManager::WebNotificationManager(WebProcess& process)
    : m_process(process)
{
#if ENABLE(NOTIFICATIONS)
    m_process.addMessageReceiver(Messages::WebNotificationManager::messageReceiverName(), *this);
#endif
}

WebNotificationManager::~WebNotificationManager()
{
}

void WebNotificationManager::initialize(const WebProcessCreationParameters& parameters)
{
#if ENABLE(NOTIFICATIONS)
    m_permissionsMap = parameters.notificationPermissions;
#else
    UNUSED_PARAM(parameters);
#endif
}

void WebNotificationManager::didUpdateNotificationDecision(const String& originString, bool allowed)
{
#if ENABLE(NOTIFICATIONS)
    if (!originString.isEmpty())
        m_permissionsMap.set(originString, allowed);
#else
    UNUSED_PARAM(originString);
    UNUSED_PARAM(allowed);
#endif
}

void WebNotificationManager::didRemoveNotificationDecisions(const Vector<String>& originStrings)
{
#if ENABLE(NOTIFICATIONS)
    for (auto& originString : originStrings) {
        if (!originString.isEmpty())
            m_permissionsMap.remove(originString);
    }
#else
    UNUSED_PARAM(originStrings);
#endif
}

NotificationClient::Permission WebNotificationManager::policyForOrigin(const String& originString) const
{
#if ENABLE(NOTIFICATIONS)
    if (originString.isEmpty())
        return NotificationClient::Permission::Default;

    auto it = m_permissionsMap.find(originString);
    if (it != m_permissionsMap.end())
        return it->value ? NotificationClient::Permission::Granted : NotificationClient::Permission::Denied;
#else
    UNUSED_PARAM(originString);
#endif
    
    return NotificationClient::Permission::Default;
}

void WebNotificationManager::removeAllPermissionsForTesting()
{
#if ENABLE(NOTIFICATIONS)
    m_permissionsMap.clear();
#endif
}

#if ENABLE(NOTIFICATIONS)
static bool sendMessage(Notification& notification, WebPage* page, const Function<bool(IPC::Connection&, uint64_t)>& sendMessage)
{
    if (!page && !notification.scriptExecutionContext())
        return false;

#if ENABLE(BUILT_IN_NOTIFICATIONS)
    if (DeprecatedGlobalSettings::builtInNotificationsEnabled())
        return sendMessage(WebProcess::singleton().ensureNetworkProcessConnection().connection(), WebProcess::singleton().sessionID().toUInt64());
#endif

    std::optional<WebCore::PageIdentifier> pageIdentifier;
    if (page)
        pageIdentifier = page->identifier();
#if ENABLE(SERVICE_WORKER)
    else if (auto* connection = SWContextManager::singleton().connection()) {
        // Pageless notification messages are, by default, on behalf of a service worker.
        // So use the service worker connection's page identifier.
        pageIdentifier = connection->pageIdentifier();
    }
#endif

    ASSERT(pageIdentifier);
    return sendMessage(*WebProcess::singleton().parentProcessConnection(), pageIdentifier->toUInt64());
}

template<typename U> bool WebNotificationManager::sendNotificationMessage(U&& message, Notification& notification, WebPage* page)
{
    return sendMessage(notification, page, [&] (auto& connection, auto destinationIdentifier) {
        return connection.send(WTFMove(message), destinationIdentifier);
    });
}

template<typename U> bool WebNotificationManager::sendNotificationMessageWithAsyncReply(U&& message, Notification& notification, WebPage* page, CompletionHandler<void()>&& callback)
{
    return sendMessage(notification, page, [&] (auto& connection, auto destinationIdentifier) {
        return connection.sendWithAsyncReply(WTFMove(message), WTFMove(callback), destinationIdentifier);
    });
}
#endif // ENABLE(NOTIFICATIONS)

bool WebNotificationManager::show(Notification& notification, WebPage* page, CompletionHandler<void()>&& callback)
{
#if ENABLE(NOTIFICATIONS)
    LOG(Notifications, "WebProcess %i going to show notification %s", getpid(), notification.identifier().toString().utf8().data());

    ASSERT(isMainRunLoop());
    if (page && !page->corePage()->settings().notificationsEnabled())
        return false;

    if (!sendNotificationMessageWithAsyncReply(Messages::NotificationManagerMessageHandler::ShowNotification(notification.data(), notification.resources()), notification, page, WTFMove(callback)))
        return false;

    if (!notification.isPersistent()) {
        ASSERT(!m_nonPersistentNotifications.contains(notification.identifier()));
        m_nonPersistentNotifications.add(notification.identifier(), notification);
    }
    return true;
#else
    UNUSED_PARAM(notification);
    UNUSED_PARAM(page);
    return false;
#endif
}

void WebNotificationManager::cancel(Notification& notification, WebPage* page)
{
    ASSERT(isMainRunLoop());

#if ENABLE(NOTIFICATIONS)
    auto identifier = notification.identifier();
    ASSERT(notification.isPersistent() || !m_nonPersistentNotifications.contains(identifier) || m_nonPersistentNotifications.get(identifier) == &notification);

    if (!sendNotificationMessage(Messages::NotificationManagerMessageHandler::CancelNotification(identifier), notification, page))
        return;
#else
    UNUSED_PARAM(notification);
    UNUSED_PARAM(page);
#endif
}

void WebNotificationManager::didDestroyNotification(Notification& notification, WebPage* page)
{
    ASSERT(isMainRunLoop());

#if ENABLE(NOTIFICATIONS)
    Ref protectedNotification { notification };

    auto identifier = notification.identifier();
    RefPtr takenNotification = !notification.isPersistent() ? m_nonPersistentNotifications.take(identifier) : nullptr;
    ASSERT(!takenNotification  || takenNotification == &notification);

    sendNotificationMessage(Messages::NotificationManagerMessageHandler::DidDestroyNotification(identifier), notification, page);
#else
    UNUSED_PARAM(notification);
    UNUSED_PARAM(page);
#endif
}

void WebNotificationManager::didShowNotification(const UUID& notificationID)
{
    ASSERT(isMainRunLoop());

    LOG(Notifications, "WebProcess %i DID SHOW notification %s", getpid(), notificationID.toString().utf8().data());

#if ENABLE(NOTIFICATIONS)
    RefPtr<Notification> notification = m_nonPersistentNotifications.get(notificationID);
    if (!notification)
        return;

    notification->dispatchShowEvent();
#else
    UNUSED_PARAM(notificationID);
#endif
}

void WebNotificationManager::didClickNotification(const UUID& notificationID)
{
    ASSERT(isMainRunLoop());

    LOG(Notifications, "WebProcess %i DID CLICK notification %s", getpid(), notificationID.toString().utf8().data());

#if ENABLE(NOTIFICATIONS)
    RefPtr<Notification> notification = m_nonPersistentNotifications.get(notificationID);
    if (!notification)
        return;

    LOG(Notifications, "WebProcess %i handling click event for notification %s", getpid(), notificationID.toString().utf8().data());

    // Indicate that this event is being dispatched in reaction to a user's interaction with a platform notification.
    UserGestureIndicator indicator(ProcessingUserGesture);
    notification->dispatchClickEvent();
#else
    UNUSED_PARAM(notificationID);
#endif
}

void WebNotificationManager::didCloseNotifications(const Vector<UUID>& notificationIDs)
{
    ASSERT(isMainRunLoop());

#if ENABLE(NOTIFICATIONS)
    size_t count = notificationIDs.size();
    for (size_t i = 0; i < count; ++i) {
        auto notificationID = notificationIDs[i];

        RefPtr<Notification> notification = m_nonPersistentNotifications.take(notificationID);
        if (!notification)
            continue;

        notification->dispatchCloseEvent();
    }
#else
    UNUSED_PARAM(notificationIDs);
#endif
}

} // namespace WebKit
