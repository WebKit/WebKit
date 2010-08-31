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
#include "NotificationPresenter.h"

#include "base/task.h" // FIXME: Remove this
#include "googleurl/src/gurl.h"
#include "public/WebNotification.h"
#include "public/WebNotificationPermissionCallback.h"
#include "public/WebSecurityOrigin.h"
#include "public/WebString.h"
#include "public/WebURL.h"
#include "webkit/support/webkit_support.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;

static void deferredDisplayDispatch(WebNotification notification)
{
    notification.dispatchDisplayEvent();
}

void NotificationPresenter::grantPermission(const WebString& origin)
{
    // Make sure it's in the form of an origin.
    GURL url(origin);
    m_allowedOrigins.add(WTF::String(url.GetOrigin().spec().c_str()));
}

// The output from all these methods matches what DumpRenderTree produces.
bool NotificationPresenter::show(const WebNotification& notification)
{
    if (!notification.replaceId().isEmpty()) {
        WTF::String replaceId(notification.replaceId().data(), notification.replaceId().length());
        if (m_replacements.find(replaceId) != m_replacements.end())
            printf("REPLACING NOTIFICATION %s\n",
                   m_replacements.find(replaceId)->second.utf8().data());

        WebString identifier = notification.isHTML() ?
            notification.url().spec().utf16() : notification.title();
        m_replacements.set(replaceId, WTF::String(identifier.data(), identifier.length()));
    }

    if (notification.isHTML()) {
        printf("DESKTOP NOTIFICATION: contents at %s\n",
               notification.url().spec().data());
    } else {
        printf("DESKTOP NOTIFICATION:%s icon %s, title %s, text %s\n",
               notification.dir() == "rtl" ? "(RTL)" : "",
               notification.iconURL().isEmpty() ? "" :
               notification.iconURL().spec().data(),
               notification.title().isEmpty() ? "" :
               notification.title().utf8().data(),
               notification.body().isEmpty() ? "" :
               notification.body().utf8().data());
    }

    WebNotification eventTarget(notification);
    webkit_support::PostTaskFromHere(NewRunnableFunction(&deferredDisplayDispatch, eventTarget));
    return true;
}

void NotificationPresenter::cancel(const WebNotification& notification)
{
    WebString identifier;
    if (notification.isHTML())
        identifier = notification.url().spec().utf16();
    else
        identifier = notification.title();

    printf("DESKTOP NOTIFICATION CLOSED: %s\n", identifier.utf8().data());
    WebNotification eventTarget(notification);
    eventTarget.dispatchCloseEvent(false);
}

void NotificationPresenter::objectDestroyed(const WebKit::WebNotification& notification)
{
    // Nothing to do.  Not storing the objects.
}

WebNotificationPresenter::Permission NotificationPresenter::checkPermission(const WebURL& url)
{
    // Check with the layout test controller
    WTF::String origin = WTF::String(static_cast<GURL>(url).GetOrigin().spec().c_str());
    bool allowed = m_allowedOrigins.find(origin) != m_allowedOrigins.end();
    return allowed ? WebNotificationPresenter::PermissionAllowed
        : WebNotificationPresenter::PermissionDenied;
}

void NotificationPresenter::requestPermission(
    const WebSecurityOrigin& origin,
    WebNotificationPermissionCallback* callback)
{
    printf("DESKTOP NOTIFICATION PERMISSION REQUESTED: %s\n",
           origin.toString().utf8().data());
    callback->permissionRequestComplete();
}
