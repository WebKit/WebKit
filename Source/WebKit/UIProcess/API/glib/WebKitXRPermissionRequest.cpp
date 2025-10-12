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

#if ENABLE(WEBXR)
static PlatformXR::Device::FeatureList toFeatureList(WebKitXRSessionFeatures features)
{
    PlatformXR::Device::FeatureList result;
    if (features & WEBKIT_XR_SESSION_FEATURES_VIEWER)
        result.append(PlatformXR::SessionFeature::ReferenceSpaceTypeViewer);
    if (features & WEBKIT_XR_SESSION_FEATURES_LOCAL)
        result.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocal);
    if (features & WEBKIT_XR_SESSION_FEATURES_LOCAL_FLOOR)
        result.append(PlatformXR::SessionFeature::ReferenceSpaceTypeLocalFloor);
    if (features & WEBKIT_XR_SESSION_FEATURES_BOUNDED_FLOOR)
        result.append(PlatformXR::SessionFeature::ReferenceSpaceTypeBoundedFloor);
    if (features & WEBKIT_XR_SESSION_FEATURES_UNBOUNDED)
        result.append(PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded);
#if ENABLE(WEBXR_HANDS)
    if (features & WEBKIT_XR_SESSION_FEATURES_HAND_TRACKING)
        result.append(PlatformXR::SessionFeature::HandTracking);
#endif
    return result;
}
#endif // ENABLE(WEBXR)

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
    WebKitXRSessionFeatures newlyGrantedFeatures;
    WebKitXRSessionFeatures previouslyGrantedFeatures;
    WebKitXRSessionFeatures consentRequiredFeatures;
    WebKitXRSessionFeatures consentOptionalFeatures;
    WebKitXRSessionFeatures requiredFeaturesRequested;
    WebKitXRSessionFeatures optionalFeaturesRequested;
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
        priv->completionHandler(toFeatureList(static_cast<WebKitXRSessionFeatures>(priv->newlyGrantedFeatures | priv->previouslyGrantedFeatures | priv->consentRequiredFeatures)));
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
 * webkit_xr_permission_request_get_security_origin:
 * @request: a #WebKitXRPermissionRequest
 *
 * Gets the security origin that initiated the permission request.
 *
 * Returns: (transfer none): the #WebKitSecurityOrigin that initiated the request
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
 * webkit_xr_permission_request_get_session_mode:
 * @request: a #WebKitXRPermissionRequest
 *
 * Gets the session mode for which permission is being requested.
 *
 * Returns: a #WebKitXRSessionMode
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

/**
 * webkit_xr_permission_request_get_granted_features:
 * @request: a #WebKitXRPermissionRequest
 *
 * Gets the features requested by the origin for the XR device, which
 * are either granted by default or have been explicitly granted by
 * the user.
 *
 * Returns: a #WebKitXRSessionFeatures flag combination
 *
 * Since: 2.52
 */
WebKitXRSessionFeatures webkit_xr_permission_request_get_granted_features(WebKitXRPermissionRequest* request)
{
#if ENABLE(WEBXR)
    g_return_val_if_fail(WEBKIT_IS_XR_PERMISSION_REQUEST(request), static_cast<WebKitXRSessionFeatures>(0));
    return request->priv->previouslyGrantedFeatures;
#else
    notImplemented();
    return static_cast<WebKitXRSessionFeatures>(0);
#endif
}

/**
 * webkit_xr_permission_request_get_consent_required_features:
 * @request: a #WebKitXRPermissionRequest
 *
 * Gets the required features that need user consent.
 *
 * These features are automatically granted if the request is allowed with
 * webkit_permission_request_allow().
 *
 * Returns: a #WebKitXRSessionFeatures flag combination
 *
 * Since: 2.52
 */
WebKitXRSessionFeatures webkit_xr_permission_request_get_consent_required_features(WebKitXRPermissionRequest* request)
{
#if ENABLE(WEBXR)
    g_return_val_if_fail(WEBKIT_IS_XR_PERMISSION_REQUEST(request), static_cast<WebKitXRSessionFeatures>(0));
    return request->priv->consentRequiredFeatures;
#else
    notImplemented();
    return static_cast<WebKitXRSessionFeatures>(0);
#endif
}

/**
 * webkit_xr_permission_request_get_consent_optional_features:
 * @request: a #WebKitXRPermissionRequest
 *
 * Gets the optional features that need user consent.
 *
 * These features can be granted by calling
 * webkit_xr_permission_request_set_granted_optional_features()
 * before allowing the request with webkit_permission_request_allow().
 *
 * Returns: a #WebKitXRSessionFeatures flag combination
 *
 * Since: 2.52
 */
WebKitXRSessionFeatures webkit_xr_permission_request_get_consent_optional_features(WebKitXRPermissionRequest* request)
{
#if ENABLE(WEBXR)
    g_return_val_if_fail(WEBKIT_IS_XR_PERMISSION_REQUEST(request), static_cast<WebKitXRSessionFeatures>(0));
    return request->priv->consentOptionalFeatures;
#else
    notImplemented();
    return static_cast<WebKitXRSessionFeatures>(0);
#endif
}

/**
 * webkit_xr_permission_request_get_required_features_requested:
 * @request: a #WebKitXRPermissionRequest
 *
 * Gets the full set of required features requested by the web application.
 *
 * This includes both already granted features and those requiring consent.
 *
 * Returns: a #WebKitXRSessionFeatures flag combination
 *
 * Since: 2.52
 */
WebKitXRSessionFeatures webkit_xr_permission_request_get_required_features_requested(WebKitXRPermissionRequest* request)
{
#if ENABLE(WEBXR)
    g_return_val_if_fail(WEBKIT_IS_XR_PERMISSION_REQUEST(request), static_cast<WebKitXRSessionFeatures>(0));
    return request->priv->requiredFeaturesRequested;
#else
    notImplemented();
    return static_cast<WebKitXRSessionFeatures>(0);
#endif
}

/**
 * webkit_xr_permission_request_get_optional_features_requested:
 * @request: a #WebKitXRPermissionRequest
 *
 * Gets the full set of optional features requested by the web application.
 *
 * This includes both already granted features and those requiring consent.
 *
 * Returns: a #WebKitXRSessionFeatures flag combination
 *
 * Since: 2.52
 *
 */
WebKitXRSessionFeatures webkit_xr_permission_request_get_optional_features_requested(WebKitXRPermissionRequest* request)
{
#if ENABLE(WEBXR)
    g_return_val_if_fail(WEBKIT_IS_XR_PERMISSION_REQUEST(request), static_cast<WebKitXRSessionFeatures>(0));
    return request->priv->optionalFeaturesRequested;
#else
    notImplemented();
    return static_cast<WebKitXRSessionFeatures>(0);
#endif
}

/**
 * webkit_xr_permission_request_set_granted_optional_features:
 * @request: a #WebKitXRPermissionRequest
 * @granted: granted features
 *
 * Sets which optional features should be granted if the permission request is allowed.
 *
 * This function should be called with a subset of the features from
 * webkit_xr_permission_request_get_consent_optional_features() before calling
 * webkit_permission_request_allow(). If the request is denied, no features are
 * granted, regardless of what is set here.
 *
 * Since: 2.52
 *
 */
void webkit_xr_permission_request_set_granted_optional_features(WebKitXRPermissionRequest* request, WebKitXRSessionFeatures granted)
{
#if ENABLE(WEBXR)
    g_return_if_fail(WEBKIT_IS_XR_PERMISSION_REQUEST(request));
    request->priv->newlyGrantedFeatures = granted;
#else
    notImplemented();
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

static WebKitXRSessionFeatures toWebKitXRSessionFeatures(const PlatformXR::Device::FeatureList& features)
{
    unsigned result = 0;
    for (auto& feature : features) {
        switch (feature) {
        case PlatformXR::SessionFeature::ReferenceSpaceTypeViewer:
            result |= WEBKIT_XR_SESSION_FEATURES_VIEWER;
            break;
        case PlatformXR::SessionFeature::ReferenceSpaceTypeLocal:
            result |= WEBKIT_XR_SESSION_FEATURES_LOCAL;
            break;
        case PlatformXR::SessionFeature::ReferenceSpaceTypeLocalFloor:
            result |= WEBKIT_XR_SESSION_FEATURES_LOCAL_FLOOR;
            break;
        case PlatformXR::SessionFeature::ReferenceSpaceTypeBoundedFloor:
            result |= WEBKIT_XR_SESSION_FEATURES_BOUNDED_FLOOR;
            break;
        case PlatformXR::SessionFeature::ReferenceSpaceTypeUnbounded:
            result |= WEBKIT_XR_SESSION_FEATURES_UNBOUNDED;
            break;
#if ENABLE(WEBXR_HANDS)
        case PlatformXR::SessionFeature::HandTracking:
            result |= WEBKIT_XR_SESSION_FEATURES_HAND_TRACKING;
            break;
#endif
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return static_cast<WebKitXRSessionFeatures>(result);
}

WebKitXRPermissionRequest* webkitXRPermissionRequestCreate(const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    WebKitXRPermissionRequest* xrPermissionRequest = WEBKIT_XR_PERMISSION_REQUEST(g_object_new(WEBKIT_TYPE_XR_PERMISSION_REQUEST, nullptr));
    xrPermissionRequest->priv->securityOrigin = webkitSecurityOriginCreate(WebCore::SecurityOriginData { securityOriginData });
    xrPermissionRequest->priv->mode = toWebKitXRSessionMode(mode);
    xrPermissionRequest->priv->completionHandler = WTFMove(completionHandler);
    xrPermissionRequest->priv->previouslyGrantedFeatures = toWebKitXRSessionFeatures(granted);
    xrPermissionRequest->priv->consentRequiredFeatures = toWebKitXRSessionFeatures(consentRequired);
    xrPermissionRequest->priv->consentOptionalFeatures = toWebKitXRSessionFeatures(consentOptional);
    xrPermissionRequest->priv->requiredFeaturesRequested = toWebKitXRSessionFeatures(requiredFeaturesRequested);
    xrPermissionRequest->priv->optionalFeaturesRequested = toWebKitXRSessionFeatures(optionalFeaturesRequested);
    return xrPermissionRequest;
}
#endif
