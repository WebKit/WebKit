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
void NotificationPermissionRequestManager::startRequest(const SecurityOriginData& securityOrigin, RefPtr<NotificationPermissionCallback>&& callback)
{
    auto permission = permissionLevel(securityOrigin);
    if (permission != NotificationClient::Permission::Default) {
        if (callback)
            callback->handleEvent(permission);
        return;
    }

    auto addResult = m_requestsPerOrigin.add(securityOrigin, Vector<RefPtr<WebCore::NotificationPermissionCallback>> { });
    addResult.iterator->value.append(WTFMove(callback));
    if (!addResult.isNewEntry)
        return;

    uint64_t requestID = generateRequestID();
    m_idToOriginMap.set(requestID, securityOrigin);

    // FIXME: This should use sendWithAsyncReply().
    m_page->send(Messages::WebPageProxy::RequestNotificationPermission(requestID, securityOrigin.toString()));
}
#endif

NotificationClient::Permission NotificationPermissionRequestManager::permissionLevel(const SecurityOriginData& securityOrigin)
{
#if ENABLE(NOTIFICATIONS)
    if (!m_page->corePage()->settings().notificationsEnabled())
        return NotificationClient::Permission::Denied;
    
    return WebProcess::singleton().supplement<WebNotificationManager>()->policyForOrigin(securityOrigin.toString());
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

    auto origin = m_idToOriginMap.take(requestID);
    if (origin.isEmpty())
        return;

    WebProcess::singleton().supplement<WebNotificationManager>()->didUpdateNotificationDecision(origin.toString(), allowed);

    auto callbacks = m_requestsPerOrigin.take(origin);
    for (auto& callback : callbacks) {
        if (!callback)
            return;
        callback->handleEvent(allowed ? NotificationClient::Permission::Granted : NotificationClient::Permission::Denied);
    }
#else
    UNUSED_PARAM(requestID);
    UNUSED_PARAM(allowed);
#endif    
}

} // namespace WebKit
