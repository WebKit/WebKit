/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
#include "NotificationPermissionRequestManager.h"

#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/Notification.h>
#include <WebCore/Page.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>

#if ENABLE(NOTIFICATIONS)
#include "WebNotificationManager.h"
#endif

namespace WebKit {
using namespace WebCore;

#if ENABLE(NOTIFICATIONS)
static uint64_t generateRequestID()
{
    static uint64_t uniqueRequestID = 1;
    return uniqueRequestID++;
}
#endif

Ref<NotificationPermissionRequestManager> NotificationPermissionRequestManager::create(WebPage* page)
{
    return adoptRef(*new NotificationPermissionRequestManager(page));
}

#if ENABLE(NOTIFICATIONS)
NotificationPermissionRequestManager::NotificationPermissionRequestManager(WebPage* page)
    : m_page(page)
{
}
#else
NotificationPermissionRequestManager::NotificationPermissionRequestManager(WebPage*)
{
}
#endif

#if ENABLE(NOTIFICATIONS)
void NotificationPermissionRequestManager::startRequest(SecurityOrigin* origin, RefPtr<NotificationPermissionCallback>&& callback)
{
    auto permission = permissionLevel(origin);
    if (permission != NotificationClient::Permission::Default) {
        if (callback)
            callback->handleEvent(permission);
        return;
    }

    uint64_t requestID = generateRequestID();
    m_originToIDMap.set(origin, requestID);
    m_idToOriginMap.set(requestID, origin);
    m_idToCallbackMap.set(requestID, WTFMove(callback));
    m_page->send(Messages::WebPageProxy::RequestNotificationPermission(requestID, origin->toString()));
}
#endif

void NotificationPermissionRequestManager::cancelRequest(SecurityOrigin* origin)
{
#if ENABLE(NOTIFICATIONS)
    uint64_t id = m_originToIDMap.take(origin);
    if (!id)
        return;
    
    m_idToOriginMap.remove(id);
    m_idToCallbackMap.remove(id);
#else
    UNUSED_PARAM(origin);
#endif
}

bool NotificationPermissionRequestManager::hasPendingPermissionRequests(SecurityOrigin* origin) const
{
#if ENABLE(NOTIFICATIONS)
    return m_originToIDMap.contains(origin);
#else
    UNUSED_PARAM(origin);
    return false;
#endif
}

NotificationClient::Permission NotificationPermissionRequestManager::permissionLevel(SecurityOrigin* securityOrigin)
{
#if ENABLE(NOTIFICATIONS)
    if (!m_page->corePage()->settings().notificationsEnabled())
        return NotificationClient::Permission::Denied;
    
    return WebProcess::singleton().supplement<WebNotificationManager>()->policyForOrigin(securityOrigin);
#else
    UNUSED_PARAM(securityOrigin);
    return NotificationClient::Permission::Denied;
#endif
}

void NotificationPermissionRequestManager::setPermissionLevelForTesting(const String& originString, bool allowed)
{
#if ENABLE(NOTIFICATIONS)
    WebProcess::singleton().supplement<WebNotificationManager>()->didUpdateNotificationDecision(originString, allowed);
#else
    UNUSED_PARAM(originString);
    UNUSED_PARAM(allowed);
#endif
}

void NotificationPermissionRequestManager::removeAllPermissionsForTesting()
{
#if ENABLE(NOTIFICATIONS)
    WebProcess::singleton().supplement<WebNotificationManager>()->removeAllPermissionsForTesting();
#endif
}

void NotificationPermissionRequestManager::didReceiveNotificationPermissionDecision(uint64_t requestID, bool allowed)
{
#if ENABLE(NOTIFICATIONS)
    if (!isRequestIDValid(requestID))
        return;

    RefPtr<WebCore::SecurityOrigin> origin = m_idToOriginMap.take(requestID);
    if (!origin)
        return;

    m_originToIDMap.remove(origin);

    WebProcess::singleton().supplement<WebNotificationManager>()->didUpdateNotificationDecision(origin->toString(), allowed);

    RefPtr<NotificationPermissionCallback> callback = m_idToCallbackMap.take(requestID);
    if (!callback)
        return;
    
    callback->handleEvent(allowed ? NotificationClient::Permission::Granted : NotificationClient::Permission::Denied);
#else
    UNUSED_PARAM(requestID);
    UNUSED_PARAM(allowed);
#endif    
}

} // namespace WebKit
