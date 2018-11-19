/*
 * Copyright (C) 2018 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitDeviceInfoPermissionRequest.h"

#include "DeviceIdHashSaltStorage.h"
#include "UserMediaPermissionCheckProxy.h"
#include "WebKitDeviceInfoPermissionRequestPrivate.h"
#include "WebKitPermissionRequest.h"
#include "WebsiteDataStore.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * SECTION: WebKitDeviceInfoPermissionRequest
 * @Short_description: A permission request for accessing user's audio/video devices.
 * @Title: WebKitDeviceInfoPermissionRequest
 * @See_also: #WebKitPermissionRequest, #WebKitWebView
 *
 * WebKitUserMediaPermissionRequest represents a request for
 * permission to whether WebKit should be allowed to access the user's
 * devices information when requested through the enumeraceDevices API.
 *
 * When a WebKitDeviceInfoPermissionRequest is not handled by the user,
 * it is denied by default.
 *
 * Since: 2.24
 */

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface*);

struct _WebKitDeviceInfoPermissionRequestPrivate {
    RefPtr<UserMediaPermissionCheckProxy> request;
    RefPtr<DeviceIdHashSaltStorage> deviceIdHashSaltStorage;
    bool madeDecision;
};

WEBKIT_DEFINE_TYPE_WITH_CODE(
    WebKitDeviceInfoPermissionRequest, webkit_device_info_permission_request, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(WEBKIT_TYPE_PERMISSION_REQUEST, webkit_permission_request_interface_init))

static void webkitDeviceInfoPermissionRequestAllow(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_DEVICE_INFO_PERMISSION_REQUEST(request));

    auto* priv = WEBKIT_DEVICE_INFO_PERMISSION_REQUEST(request)->priv;

    if (!priv->deviceIdHashSaltStorage) {
        priv->request->setUserMediaAccessInfo(false);
        return;
    }

    // Only one decision at a time.
    if (priv->madeDecision)
        return;

    priv->madeDecision = true;
    priv->request->setUserMediaAccessInfo(true);
}

static void webkitDeviceInfoPermissionRequestDeny(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_DEVICE_INFO_PERMISSION_REQUEST(request));

    auto* priv = WEBKIT_DEVICE_INFO_PERMISSION_REQUEST(request)->priv;

    if (!priv->deviceIdHashSaltStorage) {
        priv->request->setUserMediaAccessInfo(false);
        return;
    }

    // Only one decision at a time.
    if (priv->madeDecision)
        return;

    priv->madeDecision = true;
    priv->request->setUserMediaAccessInfo(false);
}

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface* iface)
{
    iface->allow = webkitDeviceInfoPermissionRequestAllow;
    iface->deny = webkitDeviceInfoPermissionRequestDeny;
}

static void webkitDeviceInfoPermissionRequestDispose(GObject* object)
{
    // Default behaviour when no decision has been made is denying the request.
    webkitDeviceInfoPermissionRequestDeny(WEBKIT_PERMISSION_REQUEST(object));
    G_OBJECT_CLASS(webkit_device_info_permission_request_parent_class)->dispose(object);
}

static void webkit_device_info_permission_request_class_init(WebKitDeviceInfoPermissionRequestClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = webkitDeviceInfoPermissionRequestDispose;
}

WebKitDeviceInfoPermissionRequest* webkitDeviceInfoPermissionRequestCreate(UserMediaPermissionCheckProxy& request, DeviceIdHashSaltStorage* deviceIdHashSaltStorage)
{
    auto* deviceInfoPermissionRequest = WEBKIT_DEVICE_INFO_PERMISSION_REQUEST(g_object_new(WEBKIT_TYPE_DEVICE_INFO_PERMISSION_REQUEST, nullptr));

    deviceInfoPermissionRequest->priv->request = &request;
    deviceInfoPermissionRequest->priv->deviceIdHashSaltStorage = deviceIdHashSaltStorage;
    return deviceInfoPermissionRequest;
}
