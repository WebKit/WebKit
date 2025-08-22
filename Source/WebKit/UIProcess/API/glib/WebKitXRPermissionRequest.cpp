/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "WebKitXRPermissionRequest.h"

#include "WebKitPermissionRequest.h"
#include "WebKitSecurityOriginPrivate.h"
#include "WebKitXRPermissionRequestPrivate.h"
#include <wtf/glib/WTFGType.h>

#if !ENABLE(WEBXR)
#include <WebCore/NotImplemented.h>
#endif

#if !ENABLE(2022_GLIB_API)
typedef WebKitPermissionRequestIface WebKitPermissionRequestInterface;
#endif

/**
 * WebKitXRPermissionRequest:
 * @See_also: #WebKitPermissionRequest, #WebKitWebView
 *
 * A permission request for accessing virtual reality (VR) and
 * augmented reality (AR) devices, including sensors and head-mounted
 * displays.
 *
 * WebKitXRPermissionRequest represents a request for permission to
 * decide whether WebKit can initialize an XR session through the
 * WebXR API.
 *
 * When a WebKitXRPermissionRequest is not handled by the user,
 * it is denied by default.
 *
 * Since: 2.52
 */

static void webkit_permission_request_interface_init(WebKitPermissionRequestInterface*);

struct _WebKitXRPermissionRequestPrivate {
#if ENABLE(WEBXR)
    WebKitSecurityOrigin* securityOrigin;
    WebKitXRSessionMode mode;
    PlatformXR::Device::FeatureList grantedFeatures;
    CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)> completionHandler;
#endif
};

WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(
    WebKitXRPermissionRequest, webkit_xr_permission_request, G_TYPE_OBJECT, GObject,
    G_IMPLEMENT_INTERFACE(WEBKIT_TYPE_PERMISSION_REQUEST, webkit_permission_request_interface_init))

#if ENABLE(WEBXR)
static void webkitXRPermissionRequestAllow(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_XR_PERMISSION_REQUEST(request));

    WebKitXRPermissionRequestPrivate* priv = WEBKIT_XR_PERMISSION_REQUEST(request)->priv;

    if (priv->completionHandler)
        priv->completionHandler(WTFMove(priv->grantedFeatures));
}

static void webkitXRPermissionRequestDeny(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_XR_PERMISSION_REQUEST(request));

    WebKitXRPermissionRequestPrivate* priv = WEBKIT_XR_PERMISSION_REQUEST(request)->priv;

    if (priv->completionHandler)
        priv->completionHandler(std::nullopt);
}
#endif

static void webkit_permission_request_interface_init(WebKitPermissionRequestInterface* iface)
{
#if ENABLE(WEBXR)
    iface->allow = webkitXRPermissionRequestAllow;
    iface->deny = webkitXRPermissionRequestDeny;
#endif
}

static void webkitXRPermissionRequestDispose(GObject* object)
{
#if ENABLE(WEBXR)
    // Default behaviour when no decision has been made is denying the request.
    webkitXRPermissionRequestDeny(WEBKIT_PERMISSION_REQUEST(object));
    WebKitXRPermissionRequestPrivate* priv = WEBKIT_XR_PERMISSION_REQUEST(object)->priv;
    g_clear_pointer(&priv->securityOrigin, webkit_security_origin_unref);
#endif
    G_OBJECT_CLASS(webkit_xr_permission_request_parent_class)->dispose(object);
}

static void webkit_xr_permission_request_class_init(WebKitXRPermissionRequestClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = webkitXRPermissionRequestDispose;
}

/**
 * webkit_xr_permission_request_get_security_origin
 * @request: a #WebKitXRPermissionRequest
 *
 * Get the origin of this request.
 *
 * Returns: (transfer none): A #WebKitSecurityOrigin of @request
 *
 * Since: 2.52
 */
WebKitSecurityOrigin* webkit_xr_permission_request_get_security_origin(WebKitXRPermissionRequest* request)
{
#if ENABLE(WEBXR)
    g_return_val_if_fail(WEBKIT_IS_XR_PERMISSION_REQUEST(request), nullptr);
    return request->priv->securityOrigin;
#else
    notImplemented();
    return nullptr;
#endif
}

/**
 * webkit_xr_permission_request_get_session_mode
 * @request: a #WebKitXRPermissionRequest
 *
 * Get the XRSessionMode of this request.
 *
 * Returns: The #WebKitXRSessionMode of @request
 *
 * Since: 2.52
 */
WebKitXRSessionMode webkit_xr_permission_request_get_session_mode(WebKitXRPermissionRequest* request)
{
#if ENABLE(WEBXR)
    g_return_val_if_fail(WEBKIT_IS_XR_PERMISSION_REQUEST(request), WEBKIT_XR_SESSION_MODE_INLINE);
    return request->priv->mode;
#else
    notImplemented();
    return WEBKIT_XR_SESSION_MODE_INLINE;
#endif
}

#if ENABLE(WEBXR)
static WebKitXRSessionMode toWebKitXRSessionMode(PlatformXR::SessionMode mode)
{
    switch (mode) {
    case PlatformXR::SessionMode::ImmersiveVr:
        return WEBKIT_XR_SESSION_MODE_IMMERSIVE_VR;
    case PlatformXR::SessionMode::ImmersiveAr:
        return WEBKIT_XR_SESSION_MODE_IMMERSIVE_AR;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return WEBKIT_XR_SESSION_MODE_INLINE;
}

WebKitXRPermissionRequest* webkitXRPermissionRequestCreate(const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    WebKitXRPermissionRequest* xrPermissionRequest = WEBKIT_XR_PERMISSION_REQUEST(g_object_new(WEBKIT_TYPE_XR_PERMISSION_REQUEST, nullptr));
    xrPermissionRequest->priv->securityOrigin = webkitSecurityOriginCreate(WebCore::SecurityOriginData { securityOriginData });
    xrPermissionRequest->priv->mode = toWebKitXRSessionMode(mode);
    xrPermissionRequest->priv->grantedFeatures = granted;
    xrPermissionRequest->priv->completionHandler = WTFMove(completionHandler);
    return xrPermissionRequest;
}
#endif
