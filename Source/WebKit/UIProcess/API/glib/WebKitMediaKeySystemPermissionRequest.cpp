/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "WebKitMediaKeySystemPermissionRequest.h"

#include "MediaKeySystemPermissionRequest.h"
#include "WebKitMediaKeySystemPermissionRequestPrivate.h"
#include "WebKitPermissionRequest.h"
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * SECTION: WebKitMediaKeySystemPermissionRequest
 * @Short_description: A permission request for using an EME Content Decryption Module
 * @Title: WebKitMediaKeySystemPermissionRequest
 * @See_also: #WebKitPermissionRequest, #WebKitWebView
 *
 * WebKitMediaKeySystemPermissionRequest represents a request for permission to decide whether
 * WebKit should use the given CDM to access protected media when requested through the
 * MediaKeySystem API.
 *
 * When a WebKitMediaKeySystemPermissionRequest is not handled by the user,
 * it is denied by default.
 *
 * When handling this permission request the application may perform additional installation of the
 * requested CDM, unless it is already present on the host system.
 */

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface*);

struct _WebKitMediaKeySystemPermissionRequestPrivate {
    RefPtr<MediaKeySystemPermissionRequest> request;
    bool madeDecision;
    CString keySystem;
};

WEBKIT_DEFINE_TYPE_WITH_CODE(
    WebKitMediaKeySystemPermissionRequest, webkit_media_key_system_permission_request, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(WEBKIT_TYPE_PERMISSION_REQUEST, webkit_permission_request_interface_init))

static void webkitMediaKeySystemPermissionRequestAllow(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(request));

    WebKitMediaKeySystemPermissionRequestPrivate* priv = WEBKIT_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(request)->priv;

    // Only one decision at a time.
    if (priv->madeDecision)
        return;

    priv->request->complete(true);
    priv->madeDecision = true;
}

static void webkitMediaKeySystemPermissionRequestDeny(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(request));

    WebKitMediaKeySystemPermissionRequestPrivate* priv = WEBKIT_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(request)->priv;

    // Only one decision at a time.
    if (priv->madeDecision)
        return;

    priv->request->complete(false);
    priv->madeDecision = true;
}

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface* iface)
{
    iface->allow = webkitMediaKeySystemPermissionRequestAllow;
    iface->deny = webkitMediaKeySystemPermissionRequestDeny;
}

static void webkitMediaKeySystemPermissionRequestDispose(GObject* object)
{
    // Default behaviour when no decision has been made is denying the request.
    webkitMediaKeySystemPermissionRequestDeny(WEBKIT_PERMISSION_REQUEST(object));
    G_OBJECT_CLASS(webkit_media_key_system_permission_request_parent_class)->dispose(object);
}

static void webkit_media_key_system_permission_request_class_init(WebKitMediaKeySystemPermissionRequestClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = webkitMediaKeySystemPermissionRequestDispose;
}

WebKitMediaKeySystemPermissionRequest* webkitMediaKeySystemPermissionRequestCreate(Ref<MediaKeySystemPermissionRequest>&& request)
{
    WebKitMediaKeySystemPermissionRequest* permissionRequest = WEBKIT_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(g_object_new(WEBKIT_TYPE_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST, NULL));
    permissionRequest->priv->request = WTFMove(request);
    return permissionRequest;
}

/**
 * webkit_media_key_system_permission_get_name:
 * @request: a #WebKitMediaKeySystemPermissionRequest
 *
 * Get the key system for which access permission is being requested.
 *
 * Returns: the key system name for @request
 *
 * Since: 2.32
 */
const gchar*
webkit_media_key_system_permission_get_name(WebKitMediaKeySystemPermissionRequest* request)
{
    auto* priv = request->priv;
    if (priv->keySystem.isNull())
        priv->keySystem = priv->request->keySystem().utf8().data();
    return priv->keySystem.data();
}
