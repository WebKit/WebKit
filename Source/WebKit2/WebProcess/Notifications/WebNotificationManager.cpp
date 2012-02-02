/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "WebPage.h"
#include "WebProcess.h"

#if ENABLE(NOTIFICATIONS)
#include "WebNotification.h"
#include "WebNotificationManagerProxyMessages.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Document.h>
#include <WebCore/Notification.h>
#include <WebCore/Page.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#endif

using namespace WebCore;

namespace WebKit {

#if ENABLE(NOTIFICATIONS)
static uint64_t generateNotificationID()
{
    static uint64_t uniqueNotificationID = 1;
    return uniqueNotificationID++;
}
#endif

WebNotificationManager::WebNotificationManager(WebProcess* process)
    : m_process(process)
{
}

WebNotificationManager::~WebNotificationManager()
{
}

void WebNotificationManager::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebNotificationManagerMessage(connection, messageID, arguments);
}

void WebNotificationManager::initialize(const HashMap<String, bool>& permissions)
{
#if ENABLE(NOTIFICATIONS)
    m_permissionsMap = permissions;
#endif
}

void WebNotificationManager::didUpdateNotificationDecision(const String& originString, bool allowed)
{
#if ENABLE(NOTIFICATIONS)
    m_permissionsMap.set(originString, allowed);
#endif
}

void WebNotificationManager::didRemoveNotificationDecisions(const Vector<String>& originStrings)
{
#if ENABLE(NOTIFICATIONS)
    size_t count = originStrings.size();
    for (size_t i = 0; i < count; ++i)
        m_permissionsMap.remove(originStrings[i]);
#endif
}

NotificationPresenter::Permission WebNotificationManager::policyForOrigin(WebCore::SecurityOrigin *origin) const
{
#if ENABLE(NOTIFICATIONS)
    if (!origin)
        return NotificationPresenter::PermissionNotAllowed;
    
    HashMap<String, bool>::const_iterator it = m_permissionsMap.find(origin->toString());
    if (it != m_permissionsMap.end())
        return it->second ? NotificationPresenter::PermissionAllowed : NotificationPresenter::PermissionDenied;
#endif
    
    return NotificationPresenter::PermissionNotAllowed;
}

bool WebNotificationManager::show(Notification* notification, WebPage* page)
{
#if ENABLE(NOTIFICATIONS)
    if (!notification || !page->corePage()->settings()->notificationsEnabled())
        return true;
    
    uint64_t notificationID = generateNotificationID();
    m_notificationMap.set(notification, notificationID);
    m_notificationIDMap.set(notificationID, notification);
    
    NotificationContextMap::iterator it = m_notificationContextMap.find(notification->scriptExecutionContext());
    if (it == m_notificationContextMap.end()) {
        pair<NotificationContextMap::iterator, bool> addedPair = m_notificationContextMap.add(notification->scriptExecutionContext(), Vector<uint64_t>());
        it = addedPair.first;
    }
    it->second.append(notificationID);
    
    m_process->connection()->send(Messages::WebPageProxy::ShowNotification(notification->contents().title, notification->contents().body, notification->scriptExecutionContext()->securityOrigin()->toString(), notificationID), page->pageID());
    return true;
#else
    return false;
#endif
}

void WebNotificationManager::cancel(Notification* notification, WebPage* page)
{
#if ENABLE(NOTIFICATIONS)
    if (!notification || !page->corePage()->settings()->notificationsEnabled())
        return;
    
    uint64_t notificationID = m_notificationMap.get(notification);
    if (!notificationID)
        return;
    
    m_process->connection()->send(Messages::WebNotificationManagerProxy::Cancel(notificationID), page->pageID());
#endif
}

void WebNotificationManager::clearNotifications(WebCore::ScriptExecutionContext* context, WebPage* page)
{
#if ENABLE(NOTIFICATIONS)
    NotificationContextMap::iterator it = m_notificationContextMap.find(context);
    if (it == m_notificationContextMap.end())
        return;
    
    m_process->connection()->send(Messages::WebNotificationManagerProxy::ClearNotifications(it->second), page->pageID());
    m_notificationContextMap.remove(it);
#endif
}

void WebNotificationManager::didDestroyNotification(Notification* notification, WebPage* page)
{
#if ENABLE(NOTIFICATIONS)
    uint64_t notificationID = m_notificationMap.take(notification);
    if (!notificationID)
        return;

    m_notificationIDMap.take(notificationID);
    m_process->connection()->send(Messages::WebNotificationManagerProxy::DidDestroyNotification(notificationID), page->pageID());
#endif
}

void WebNotificationManager::didShowNotification(uint64_t notificationID)
{
#if ENABLE(NOTIFICATIONS)
    if (!isNotificationIDValid(notificationID))
        return;
    
    RefPtr<Notification> notification = m_notificationIDMap.get(notificationID);
    if (!notification)
        return;

    notification->dispatchShowEvent();
#endif
}

void WebNotificationManager::didClickNotification(uint64_t notificationID)
{
#if ENABLE(NOTIFICATIONS)
    if (!isNotificationIDValid(notificationID))
        return;

    RefPtr<Notification> notification = m_notificationIDMap.get(notificationID);
    if (!notification)
        return;

    notification->dispatchClickEvent();
#endif
}

void WebNotificationManager::didCloseNotifications(const Vector<uint64_t>& notificationIDs)
{
#if ENABLE(NOTIFICATIONS)
    size_t count = notificationIDs.size();
    for (size_t i = 0; i < count; ++i) {
        uint64_t notificationID = notificationIDs[i];
        if (!isNotificationIDValid(notificationID))
            continue;

        RefPtr<Notification> notification = m_notificationIDMap.get(notificationID);
        if (!notification)
            continue;

        NotificationContextMap::iterator it = m_notificationContextMap.find(notification->scriptExecutionContext());
        ASSERT(it != m_notificationContextMap.end());
        size_t index = it->second.find(notificationID);
        ASSERT(index != notFound);
        it->second.remove(index);

        notification->dispatchCloseEvent();
    }
#endif
}

} // namespace WebKit
