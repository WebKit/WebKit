/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE_NOTIFICATIONS
#include "NotificationPresenter.h"

#include "WebKit.h"
#include "WebNotification.h"
#include "WebNotificationPermissionCallback.h"
#include "WebSecurityOrigin.h"
#include "WebTestDelegate.h"
#include "googleurl/src/gurl.h"
#include <public/Platform.h>
#include <public/WebString.h>
#include <public/WebURL.h>

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

namespace {

WebString identifierForNotification(const WebNotification& notification)
{
    if (notification.isHTML())
        return notification.url().spec().utf16();
    return notification.title();
}

void deferredDisplayDispatch(void* context)
{
    WebNotification* notification = static_cast<WebNotification*>(context);
    notification->dispatchDisplayEvent();
    delete notification;
}

}

NotificationPresenter::NotificationPresenter()
    : m_delegate(0)
{
}

NotificationPresenter::~NotificationPresenter()
{
}

void NotificationPresenter::grantPermission(const WebString& origin)
{
    m_allowedOrigins.insert(origin.utf8());
}

bool NotificationPresenter::simulateClick(const WebString& title)
{
    string id(title.utf8());
    if (m_activeNotifications.find(id) == m_activeNotifications.end())
        return false;

    const WebNotification& notification = m_activeNotifications.find(id)->second;
    WebNotification eventTarget(notification);
    eventTarget.dispatchClickEvent();
    return true;
}

// The output from all these methods matches what DumpRenderTree produces.
bool NotificationPresenter::show(const WebNotification& notification)
{
    WebString identifier = identifierForNotification(notification);
    if (!notification.replaceId().isEmpty()) {
        string replaceId(notification.replaceId().utf8());
        if (m_replacements.find(replaceId) != m_replacements.end())
            m_delegate->printMessage(string("REPLACING NOTIFICATION ") + m_replacements.find(replaceId)->second + "\n");

        m_replacements[replaceId] = identifier.utf8();
    }

    if (notification.isHTML())
        m_delegate->printMessage(string("DESKTOP NOTIFICATION: contents at ") + string(notification.url().spec()) + "\n");
    else {
        m_delegate->printMessage("DESKTOP NOTIFICATION:");
        m_delegate->printMessage(notification.direction() == WebTextDirectionRightToLeft ? "(RTL)" : "");
        m_delegate->printMessage(" icon ");
        m_delegate->printMessage(notification.iconURL().isEmpty() ? "" : notification.iconURL().spec().data());
        m_delegate->printMessage(", title ");
        m_delegate->printMessage(notification.title().isEmpty() ? "" : notification.title().utf8().data());
        m_delegate->printMessage(", text ");
        m_delegate->printMessage(notification.body().isEmpty() ? "" : notification.body().utf8().data());
        m_delegate->printMessage("\n");
    }

    string id(identifier.utf8());
    m_activeNotifications[id] = notification;

    Platform::current()->callOnMainThread(deferredDisplayDispatch, new WebNotification(notification));
    return true;
}

void NotificationPresenter::cancel(const WebNotification& notification)
{
    WebString identifier = identifierForNotification(notification);
    m_delegate->printMessage(string("DESKTOP NOTIFICATION CLOSED: ") + string(identifier.utf8()) + "\n");
    WebNotification eventTarget(notification);
    eventTarget.dispatchCloseEvent(false);

    string id(identifier.utf8());
    m_activeNotifications.erase(id);
}

void NotificationPresenter::objectDestroyed(const WebKit::WebNotification& notification)
{
    WebString identifier = identifierForNotification(notification);
    string id(identifier.utf8());
    m_activeNotifications.erase(id);
}

WebNotificationPresenter::Permission NotificationPresenter::checkPermission(const WebSecurityOrigin& origin)
{
    // Check with the layout test controller
    WebString originString = origin.toString();
    bool allowed = m_allowedOrigins.find(string(originString.utf8())) != m_allowedOrigins.end();
    return allowed ? WebNotificationPresenter::PermissionAllowed
        : WebNotificationPresenter::PermissionDenied;
}

void NotificationPresenter::requestPermission(
    const WebSecurityOrigin& origin,
    WebNotificationPermissionCallback* callback)
{
    m_delegate->printMessage("DESKTOP NOTIFICATION PERMISSION REQUESTED: " + string(origin.toString().utf8()) + "\n");
    callback->permissionRequestComplete();
}

}

#endif // ENABLE_NOTIFICATIONS
