/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitNotificationPermissionRequest.h"

#include "NotificationPermissionRequest.h"
#include "WebKitNotificationPermissionRequestPrivate.h"
#include "WebKitPermissionRequest.h"
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * SECTION: WebKitNotificationPermissionRequest
 * @Short_description: A permission request for displaying web notifications
 * @Title: WebKitNotificationPermissionRequest
 * @See_also: #WebKitPermissionRequest, #WebKitWebView
 *
 * WebKitNotificationPermissionRequest represents a request for
 * permission to decide whether WebKit should provide the user with
 * notifications through the Web Notification API.
 *
 * When a WebKitNotificationPermissionRequest is not handled by the user,
 * it is denied by default.
 *
 * Since: 2.8
 */

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface*);

struct _WebKitNotificationPermissionRequestPrivate {
    RefPtr<NotificationPermissionRequest> request;
    bool madeDecision;
};

WEBKIT_DEFINE_TYPE_WITH_CODE(
    WebKitNotificationPermissionRequest, webkit_notification_permission_request, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(WEBKIT_TYPE_PERMISSION_REQUEST, webkit_permission_request_interface_init))

static void webkitNotificationPermissionRequestAllow(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request));

    WebKitNotificationPermissionRequestPrivate* priv = WEBKIT_NOTIFICATION_PERMISSION_REQUEST(request)->priv;

    // Only one decision at a time.
    if (priv->madeDecision)
        return;

    priv->request->allow();
    priv->madeDecision = true;
}

static void webkitNotificationPermissionRequestDeny(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request));

    WebKitNotificationPermissionRequestPrivate* priv = WEBKIT_NOTIFICATION_PERMISSION_REQUEST(request)->priv;

    // Only one decision at a time.
    if (priv->madeDecision)
        return;

    priv->request->deny();
    priv->madeDecision = true;
}

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface* iface)
{
    iface->allow = webkitNotificationPermissionRequestAllow;
    iface->deny = webkitNotificationPermissionRequestDeny;
}

static void webkitNotificationPermissionRequestDispose(GObject* object)
{
    // Default behaviour when no decision has been made is denying the request.
    webkitNotificationPermissionRequestDeny(WEBKIT_PERMISSION_REQUEST(object));
    G_OBJECT_CLASS(webkit_notification_permission_request_parent_class)->dispose(object);
}

static void webkit_notification_permission_request_class_init(WebKitNotificationPermissionRequestClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = webkitNotificationPermissionRequestDispose;
}

WebKitNotificationPermissionRequest* webkitNotificationPermissionRequestCreate(NotificationPermissionRequest* request)
{
    WebKitNotificationPermissionRequest* notificationPermissionRequest = WEBKIT_NOTIFICATION_PERMISSION_REQUEST(g_object_new(WEBKIT_TYPE_NOTIFICATION_PERMISSION_REQUEST, nullptr));
    notificationPermissionRequest->priv->request = request;
    return notificationPermissionRequest;
}
