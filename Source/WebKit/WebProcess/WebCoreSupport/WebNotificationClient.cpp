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
#include "WebNotificationClient.h"

#if ENABLE(NOTIFICATIONS)

#include "NotificationPermissionRequestManager.h"
#include "WebNotificationManager.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/ScriptExecutionContext.h>

namespace WebKit {
using namespace WebCore;

WebNotificationClient::WebNotificationClient(WebPage* page)
    : m_page(page)
{
    ASSERT(isMainRunLoop());
}

WebNotificationClient::~WebNotificationClient()
{
    ASSERT(isMainRunLoop());
}

bool WebNotificationClient::show(Notification& notification)
{
    bool result;
    callOnMainRunLoopAndWait([&result, protectedNotification = Ref { notification }, page = m_page]() {
        result = WebProcess::singleton().supplement<WebNotificationManager>()->show(protectedNotification.get(), page);
    });
    return result;
}

void WebNotificationClient::cancel(Notification& notification)
{
    callOnMainRunLoopAndWait([protectedNotification = Ref { notification }, page = m_page]() {
        WebProcess::singleton().supplement<WebNotificationManager>()->cancel(protectedNotification.get(), page);
    });
}

void WebNotificationClient::notificationObjectDestroyed(Notification& notification)
{
    // Make sure this Notification object is deallocated on this thread by holding one last ref() to it here
    auto lastNotificationRef = Ref { notification };

    callOnMainRunLoopAndWait([&notification, page = m_page]() {
        WebProcess::singleton().supplement<WebNotificationManager>()->didDestroyNotification(notification, page);
    });
}

void WebNotificationClient::notificationControllerDestroyed()
{
    callOnMainRunLoop([this] {
        delete this;
    });
}

void WebNotificationClient::requestPermission(ScriptExecutionContext& context, PermissionHandler&& permissionHandler)
{
    // Only Window clients can request permission
    ASSERT(isMainRunLoop());
    ASSERT(m_page);

    auto* securityOrigin = context.securityOrigin();
    if (!securityOrigin)
        return permissionHandler(NotificationClient::Permission::Denied);
    m_page->notificationPermissionRequestManager()->startRequest(securityOrigin->data(), WTFMove(permissionHandler));
}

NotificationClient::Permission WebNotificationClient::checkPermission(ScriptExecutionContext* context)
{
    if (!context)
        return NotificationClient::Permission::Denied;
    if (!context->isDocument() && !context->isServiceWorkerGlobalScope())
        return NotificationClient::Permission::Denied;

    auto* origin = context->securityOrigin();
    if (!origin)
        return NotificationClient::Permission::Denied;

    NotificationClient::Permission resultPermission;
    callOnMainRunLoopAndWait([&resultPermission, origin = origin->data().toString().isolatedCopy()] {
        resultPermission = WebProcess::singleton().supplement<WebNotificationManager>()->policyForOrigin(origin);
    });
    return resultPermission;
}

} // namespace WebKit

#endif // ENABLE(NOTIFICATIONS)
