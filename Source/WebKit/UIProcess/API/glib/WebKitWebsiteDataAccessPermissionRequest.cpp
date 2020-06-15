/*
 * Copyright (C) 2020 Igalia S.L
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
#include "WebKitWebsiteDataAccessPermissionRequest.h"

#include "WebKitPermissionRequest.h"
#include "WebKitWebsiteDataAccessPermissionRequestPrivate.h"
#include <wtf/CompletionHandler.h>
#include <wtf/glib/WTFGType.h>

/**
 * SECTION: WebKitWebsiteDataAccessPermissionRequest
 * @Short_description: A permission request for accessing website data from third-party domains
 * @Title: WebKitWebsiteDataAccessPermissionRequest
 * @See_also: #WebKitPermissionRequest, #WebKitWebView
 *
 * WebKitWebsiteDataAccessPermissionRequest represents a request for
 * permission to allow a third-party domain access its cookies.
 *
 * When a WebKitWebsiteDataAccessPermissionRequest is not handled by the user,
 * it is denied by default.
 *
 * Since: 2.30
 */

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface*);

struct _WebKitWebsiteDataAccessPermissionRequestPrivate {
    CString requestingDomain;
    CString currentDomain;
    CompletionHandler<void(bool)> completionHandler;
};

WEBKIT_DEFINE_TYPE_WITH_CODE(
    WebKitWebsiteDataAccessPermissionRequest, webkit_website_data_access_permission_request, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(WEBKIT_TYPE_PERMISSION_REQUEST, webkit_permission_request_interface_init))

static void webkitWebsiteDataAccessPermissionRequestAllow(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request));

    auto* priv = WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request)->priv;
    if (priv->completionHandler)
        priv->completionHandler(true);
}

static void webkitWebsiteDataAccessPermissionRequestDeny(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request));

    auto* priv = WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request)->priv;
    if (priv->completionHandler)
        priv->completionHandler(false);
}

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface* iface)
{
    iface->allow = webkitWebsiteDataAccessPermissionRequestAllow;
    iface->deny = webkitWebsiteDataAccessPermissionRequestDeny;
}

static void webkitWebsiteDataAccessPermissionRequestDispose(GObject* object)
{
    // Default behaviour when no decision has been made is denying the request.
    webkitWebsiteDataAccessPermissionRequestDeny(WEBKIT_PERMISSION_REQUEST(object));
    G_OBJECT_CLASS(webkit_website_data_access_permission_request_parent_class)->dispose(object);
}

static void webkit_website_data_access_permission_request_class_init(WebKitWebsiteDataAccessPermissionRequestClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = webkitWebsiteDataAccessPermissionRequestDispose;
}

WebKitWebsiteDataAccessPermissionRequest* webkitWebsiteDataAccessPermissionRequestCreate(const WebCore::RegistrableDomain& requestingDomain, const WebCore::RegistrableDomain& currentDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* websiteDataPermissionRequest = WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(g_object_new(WEBKIT_TYPE_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST, nullptr));
    websiteDataPermissionRequest->priv->requestingDomain = requestingDomain.string().utf8();
    websiteDataPermissionRequest->priv->currentDomain = currentDomain.string().utf8();
    websiteDataPermissionRequest->priv->completionHandler = WTFMove(completionHandler);
    return websiteDataPermissionRequest;
}

/**
 * webkit_website_data_access_permission_request_get_requesting_domain:
 * @request: a #WebKitWebsiteDataAccessPermissionRequest
 *
 * Get the domain requesting permission to access its cookies while browsing the current domain.
 *
 * Returns: the requesting domain name
 *
 * Since: 2.30
 */
const char* webkit_website_data_access_permission_request_get_requesting_domain(WebKitWebsiteDataAccessPermissionRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request), nullptr);

    return request->priv->requestingDomain.data();
}

/**
 * webkit_website_data_access_permission_request_get_current_domain:
 * @request: a #WebKitWebsiteDataAccessPermissionRequest
 *
 * Get the current domain being browsed.
 *
 * Returns: the current domain name
 *
 * Since: 2.30
 */
const char* webkit_website_data_access_permission_request_get_current_domain(WebKitWebsiteDataAccessPermissionRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request), nullptr);

    return request->priv->currentDomain.data();
}
