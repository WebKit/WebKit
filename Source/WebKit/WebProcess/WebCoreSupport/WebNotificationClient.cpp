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
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/NotificationData.h>
#include <WebCore/Page.h>
#include <WebCore/ScriptExecutionContext.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebNotificationClient);

WebNotificationClient::WebNotificationClient(WebPage* page)
    : m_page(page)
{
    ASSERT(isMainRunLoop());
}

WebNotificationClient::~WebNotificationClient()
{
    ASSERT(isMainRunLoop());
}

bool WebNotificationClient::show(ScriptExecutionContext& context, NotificationData&& notification, RefPtr<NotificationResources>&& resources, CompletionHandler<void()>&& callback)
{
    bool result;
    callOnMainRunLoopAndWait([&result, notification = WTFMove(notification).isolatedCopy(), resources = WTFMove(resources), page = m_page, contextIdentifier = context.identifier(), callbackIdentifier = context.addNotificationCallback(WTFMove(callback))]() mutable {
        result = WebProcess::singleton().supplement<WebNotificationManager>()->show(WTFMove(notification), WTFMove(resources), RefPtr { page.get() }.get(), [contextIdentifier, callbackIdentifier] {
            ScriptExecutionContext::ensureOnContextThread(contextIdentifier, [callbackIdentifier](auto& context) {
                if (auto callback = context.takeNotificationCallback(callbackIdentifier))
                    callback();
            });
        });
    });
    return result;
}

void WebNotificationClient::cancel(NotificationData&& notification)
{
    callOnMainRunLoopAndWait([notification = WTFMove(notification).isolatedCopy(), page = m_page]() mutable {
        WebProcess::singleton().supplement<WebNotificationManager>()->cancel(WTFMove(notification), RefPtr { page.get() }.get());
    });
}

void WebNotificationClient::notificationObjectDestroyed(NotificationData&& notification)
{
    callOnMainRunLoopAndWait([notification = WTFMove(notification).isolatedCopy(), page = m_page]() mutable {
        WebProcess::singleton().supplement<WebNotificationManager>()->didDestroyNotification(WTFMove(notification), RefPtr { page.get() }.get());
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

    if (!isMainRunLoop() || !context.isDocument() || WebProcess::singleton().sessionID().isEphemeral())
        return permissionHandler(NotificationClient::Permission::Denied);

    RefPtr securityOrigin = context.securityOrigin();
    if (!securityOrigin)
        return permissionHandler(NotificationClient::Permission::Denied);

    // Add origin to list of origins that have requested permission to use the Notifications API.
    m_notificationPermissionRequesters.add(securityOrigin->data());

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    if (DeprecatedGlobalSettings::builtInNotificationsEnabled()) {
        auto handler = [permissionHandler = WTFMove(permissionHandler)](bool granted) mutable {
            permissionHandler(granted ? NotificationPermission::Granted : NotificationPermission::Denied);
        };
        WebProcess::singleton().supplement<WebNotificationManager>()->requestPermission(WebCore::SecurityOriginData { securityOrigin->data() }, RefPtr { m_page.get() }, WTFMove(handler));
        return;
    }
#endif

    Ref { *m_page }->notificationPermissionRequestManager()->startRequest(securityOrigin->data(), WTFMove(permissionHandler));
}

NotificationClient::Permission WebNotificationClient::checkPermission(ScriptExecutionContext* context)
{
    if (!context || (!context->isDocument() && !context->isServiceWorkerGlobalScope()))
        return NotificationClient::Permission::Denied;

    RefPtr origin = context->securityOrigin();
    if (!origin)
        return NotificationClient::Permission::Denied;

    bool hasRequestedPermission = m_notificationPermissionRequesters.contains(origin->data());
    if (WebProcess::singleton().sessionID().isEphemeral())
        return hasRequestedPermission ? NotificationClient::Permission::Denied : NotificationClient::Permission::Default;

    NotificationClient::Permission resultPermission;
    if (RefPtr document = dynamicDowncast<Document>(*context)) {
        ASSERT(isMainRunLoop());
        RefPtr page = document->page() ? WebPage::fromCorePage(*document->page()) : nullptr;
        resultPermission = WebProcess::singleton().supplement<WebNotificationManager>()->policyForOrigin(origin->data().toString(), page.get());
    } else {
        callOnMainRunLoopAndWait([&resultPermission, origin = origin->data().toString().isolatedCopy()] {
            resultPermission = WebProcess::singleton().supplement<WebNotificationManager>()->policyForOrigin(origin);
        });
    }

    // To reduce fingerprinting, if the origin has not requested permission to use the
    // Notifications API, and the permission state is "denied", return "default" instead.
    if (resultPermission == NotificationClient::Permission::Denied && !hasRequestedPermission)
        return NotificationClient::Permission::Default;

    return resultPermission;
}

void WebNotificationClient::clearNotificationPermissionState()
{
    m_notificationPermissionRequesters.clear();
}

} // namespace WebKit

#endif // ENABLE(NOTIFICATIONS)
