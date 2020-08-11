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

#include "WebKitInstallMissingMediaPluginsPermissionRequestPrivate.h"
#include "WebKitPermissionRequest.h"
#include "WebPageProxy.h"
#include <wtf/glib/WTFGType.h>

#if ENABLE(VIDEO)
#include <WebCore/PlatformDisplay.h>
#if PLATFORM(X11) && !USE(GTK4)
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#endif
#endif

using namespace WebKit;
using namespace WebCore;

/**
 * SECTION: WebKitInstallMissingMediaPluginsPermissionRequest
 * @Short_description: A permission request for installing missing media plugins
 * @Title: WebKitInstallMissingMediaPluginsPermissionRequest
 * @See_also: #WebKitPermissionRequest, #WebKitWebView
 *
 * WebKitInstallMissingMediaPluginsPermissionRequest represents a request for
 * permission to decide whether WebKit should try to start a helper application to
 * install missing media plugins when the media backend couldn't play a media because
 * the required plugins were not available.
 *
 * When a WebKitInstallMissingMediaPluginsPermissionRequest is not handled by the user,
 * it is allowed by default.
 *
 * Since: 2.10
 */

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface*);

struct _WebKitInstallMissingMediaPluginsPermissionRequestPrivate {
#if ENABLE(VIDEO) && !USE(GSTREAMER_FULL)
    RefPtr<InstallMissingMediaPluginsPermissionRequest> request;
#endif
    CString description;
    bool madeDecision;
};

WEBKIT_DEFINE_TYPE_WITH_CODE(
    WebKitInstallMissingMediaPluginsPermissionRequest, webkit_install_missing_media_plugins_permission_request, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(WEBKIT_TYPE_PERMISSION_REQUEST, webkit_permission_request_interface_init))

#if ENABLE(VIDEO) && !USE(GSTREAMER_FULL)
static GUniquePtr<GstInstallPluginsContext> createGstInstallPluginsContext(WebPageProxy& page)
{
#if PLATFORM(X11) && !USE(GTK4)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11) {
        GUniquePtr<GstInstallPluginsContext> context(gst_install_plugins_context_new());
        gst_install_plugins_context_set_xid(context.get(), GDK_WINDOW_XID(gtk_widget_get_window(page.viewWidget())));
        return context;
    }
#endif

    return nullptr;
}
#endif

static void webkitInstallMissingMediaPluginsPermissionRequestAllow(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request));

    WebKitInstallMissingMediaPluginsPermissionRequestPrivate* priv = WEBKIT_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request)->priv;

    // Only one decision at a time.
    if (priv->madeDecision)
        return;
#if ENABLE(VIDEO) && !USE(GSTREAMER_FULL)
    priv->request->allow(createGstInstallPluginsContext(priv->request->page()));
#endif
    priv->madeDecision = true;
}

static void webkitInstallMissingMediaPluginsPermissionRequestDeny(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request));

    WebKitInstallMissingMediaPluginsPermissionRequestPrivate* priv = WEBKIT_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request)->priv;

    // Only one decision at a time.
    if (priv->madeDecision)
        return;

#if ENABLE(VIDEO) && !USE(GSTREAMER_FULL)
    priv->request->deny();
#endif
    priv->madeDecision = true;
}

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface* iface)
{
    iface->allow = webkitInstallMissingMediaPluginsPermissionRequestAllow;
    iface->deny = webkitInstallMissingMediaPluginsPermissionRequestDeny;
}

static void webkitInstallMissingMediaPluginsPermissionRequestDispose(GObject* object)
{
    // Default behaviour when no decision has been made is allowing the request for backwards compatibility.
    webkitInstallMissingMediaPluginsPermissionRequestDeny(WEBKIT_PERMISSION_REQUEST(object));
    G_OBJECT_CLASS(webkit_install_missing_media_plugins_permission_request_parent_class)->dispose(object);
}

static void webkit_install_missing_media_plugins_permission_request_class_init(WebKitInstallMissingMediaPluginsPermissionRequestClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = webkitInstallMissingMediaPluginsPermissionRequestDispose;
}

#if ENABLE(VIDEO) && !USE(GSTREAMER_FULL)
WebKitInstallMissingMediaPluginsPermissionRequest* webkitInstallMissingMediaPluginsPermissionRequestCreate(InstallMissingMediaPluginsPermissionRequest& request)
{
    WebKitInstallMissingMediaPluginsPermissionRequest* permissionRequest = WEBKIT_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(g_object_new(WEBKIT_TYPE_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST, nullptr));
    permissionRequest->priv->request = &request;
    return permissionRequest;
}
#endif

/**
 * webkit_install_missing_media_plugins_permission_request_get_description:
 * @request: a #WebKitInstallMissingMediaPluginsPermissionRequest
 *
 * Gets the description about the missing plugins provided by the media backend when a media couldn't be played.
 *
 * Returns: a string with the description provided by the media backend.
 *
 * Since: 2.10
 */
const char* webkit_install_missing_media_plugins_permission_request_get_description(WebKitInstallMissingMediaPluginsPermissionRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request), nullptr);

#if ENABLE(VIDEO) && !USE(GSTREAMER_FULL)
    if (!request->priv->description.isNull())
        return request->priv->description.data();

    const auto& description = request->priv->request->description();
    ASSERT(!description.isEmpty());
    request->priv->description = description.utf8();
#endif
    return request->priv->description.data();
}
