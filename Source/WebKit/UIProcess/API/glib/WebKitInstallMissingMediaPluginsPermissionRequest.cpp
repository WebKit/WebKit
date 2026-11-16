/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "WebKitInstallMissingMediaPluginsPermissionRequest.h"

#if !ENABLE(2022_GLIB_API)

#include "WebKitPermissionRequest.h"
#include <wtf/glib/WTFGType.h>

/**
 * WebKitInstallMissingMediaPluginsPermissionRequest:
 * @See_also: #WebKitPermissionRequest, #WebKitWebView
 *
 * Previously, a permission request for installing missing media plugins.
 *
 * WebKitInstallMissingMediaPluginsPermissionRequest will no longer ever be created, so
 * you can remove any code that attempts to handle it.
 *
 * Since: 2.10
 *
 * Deprecated: 2.40
 */

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface*);

struct _WebKitInstallMissingMediaPluginsPermissionRequestPrivate {
};

WEBKIT_DEFINE_TYPE_WITH_CODE(
    WebKitInstallMissingMediaPluginsPermissionRequest, webkit_install_missing_media_plugins_permission_request, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(WEBKIT_TYPE_PERMISSION_REQUEST, webkit_permission_request_interface_init))

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface* iface)
{
    iface->allow = [](auto*) { };
    iface->deny = [](auto*) { };
}

static void webkit_install_missing_media_plugins_permission_request_class_init(WebKitInstallMissingMediaPluginsPermissionRequestClass*)
{
}

/**
 * webkit_install_missing_media_plugins_permission_request_get_description:
 * @request: a #WebKitInstallMissingMediaPluginsPermissionRequest
 *
 * This function returns an empty string.
 *
 * Returns: an empty string
 *
 * Since: 2.10
 *
 * Deprecated: 2.40
 */
const char* webkit_install_missing_media_plugins_permission_request_get_description(WebKitInstallMissingMediaPluginsPermissionRequest*)
{
    return "";
}

#endif // !ENABLE(2022_GLIB_API)
