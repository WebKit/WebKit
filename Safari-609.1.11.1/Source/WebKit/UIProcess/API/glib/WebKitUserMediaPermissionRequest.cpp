/*
 * Copyright (C) 2014 Igalia S.L
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
#include "WebKitUserMediaPermissionRequest.h"

#include "UserMediaPermissionRequestProxy.h"
#include "WebKitPermissionRequest.h"
#include "WebKitUserMediaPermissionRequestPrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * SECTION: WebKitUserMediaPermissionRequest
 * @Short_description: A permission request for accessing user's audio/video devices.
 * @Title: WebKitUserMediaPermissionRequest
 * @See_also: #WebKitPermissionRequest, #WebKitWebView
 *
 * WebKitUserMediaPermissionRequest represents a request for
 * permission to decide whether WebKit should be allowed to access the user's
 * audio and video source devices when requested through the getUserMedia API.
 *
 * When a WebKitUserMediaPermissionRequest is not handled by the user,
 * it is denied by default.
 *
 * Since: 2.8
 */

enum {
    PROP_0,
    PROP_IS_FOR_AUDIO_DEVICE,
    PROP_IS_FOR_VIDEO_DEVICE
};

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface*);

struct _WebKitUserMediaPermissionRequestPrivate {
    RefPtr<UserMediaPermissionRequestProxy> request;
    bool madeDecision;
};

WEBKIT_DEFINE_TYPE_WITH_CODE(
    WebKitUserMediaPermissionRequest, webkit_user_media_permission_request, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(WEBKIT_TYPE_PERMISSION_REQUEST, webkit_permission_request_interface_init))

static void webkitUserMediaPermissionRequestAllow(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request));

    WebKitUserMediaPermissionRequestPrivate* priv = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request)->priv;

    // Only one decision at a time.
    if (priv->madeDecision)
        return;

    priv->madeDecision = true;

    auto videoDeviceUIDs = priv->request->videoDeviceUIDs();
    auto audioDeviceUIDs = priv->request->audioDeviceUIDs();

    auto videoDevice = !videoDeviceUIDs.isEmpty() ? videoDeviceUIDs[0] : emptyString();
    auto audioDevice = !audioDeviceUIDs.isEmpty() ? audioDeviceUIDs[0] : emptyString();

    priv->request->allow(audioDevice, videoDevice);
}

static void webkitUserMediaPermissionRequestDeny(WebKitPermissionRequest* request)
{
    ASSERT(WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request));

    WebKitUserMediaPermissionRequestPrivate* priv = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request)->priv;

    // Only one decision at a time.
    if (priv->madeDecision)
        return;

    priv->madeDecision = true;
    priv->request->deny(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason::PermissionDenied);
}

static void webkit_permission_request_interface_init(WebKitPermissionRequestIface* iface)
{
    iface->allow = webkitUserMediaPermissionRequestAllow;
    iface->deny = webkitUserMediaPermissionRequestDeny;
}

static void webkitUserMediaPermissionRequestDispose(GObject* object)
{
    // Default behaviour when no decision has been made is denying the request.
    webkitUserMediaPermissionRequestDeny(WEBKIT_PERMISSION_REQUEST(object));
    G_OBJECT_CLASS(webkit_user_media_permission_request_parent_class)->dispose(object);
}

/**
 * webkit_user_media_permission_is_for_audio_device:
 * @request: a #WebKitUserMediaPermissionRequest
 *
 * Returns: %TRUE if access to an audio device was requested.
 *
 * Since: 2.8
 */
gboolean webkit_user_media_permission_is_for_audio_device(WebKitUserMediaPermissionRequest* request)
{
    g_return_val_if_fail(request->priv->request, FALSE);
    return request->priv->request->requiresAudioCapture();
}

/**
 * webkit_user_media_permission_is_for_video_device:
 * @request: a #WebKitUserMediaPermissionRequest
 *
 * Returns: %TRUE if access to a video device was requested.
 *
 * Since: 2.8
 */
gboolean webkit_user_media_permission_is_for_video_device(WebKitUserMediaPermissionRequest* request)
{
    g_return_val_if_fail(request->priv->request, FALSE);
    return request->priv->request->requiresVideoCapture();
}

static void webkitUserMediaPermissionRequestGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitUserMediaPermissionRequest* request = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(object);

    switch (propId) {
    case PROP_IS_FOR_AUDIO_DEVICE:
        g_value_set_boolean(value, webkit_user_media_permission_is_for_audio_device(request));
        break;
    case PROP_IS_FOR_VIDEO_DEVICE:
        g_value_set_boolean(value, webkit_user_media_permission_is_for_video_device(request));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_user_media_permission_request_class_init(WebKitUserMediaPermissionRequestClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = webkitUserMediaPermissionRequestDispose;
    objectClass->get_property = webkitUserMediaPermissionRequestGetProperty;

    /**
     * WebKitUserPermissionRequest:is-for-audio-device:
     *
     * Whether the media device to which the permission was requested has a microphone or not.
     *
     * Since: 2.8
     */
    g_object_class_install_property(objectClass, PROP_IS_FOR_AUDIO_DEVICE,
        g_param_spec_boolean("is-for-audio-device", _("Is for audio device"),
            _("Whether the media device to which the permission was requested has a microphone or not."),
            FALSE,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitUserPermissionRequest:is-for-video-device:
     *
     * Whether the media device to which the permission was requested has a video capture capability or not.
     *
     * Since: 2.8
     */
    g_object_class_install_property(objectClass, PROP_IS_FOR_VIDEO_DEVICE,
        g_param_spec_boolean("is-for-video-device", _("Is for video device"),
            _("Whether the media device to which the permission was requested has a video capture capability or not."),
            FALSE,
            WEBKIT_PARAM_READABLE));
}

WebKitUserMediaPermissionRequest* webkitUserMediaPermissionRequestCreate(UserMediaPermissionRequestProxy& request, API::SecurityOrigin& userMediaDocumentOrigin, API::SecurityOrigin& topLevelDocumentOrigin)
{
    WebKitUserMediaPermissionRequest* usermediaPermissionRequest = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(g_object_new(WEBKIT_TYPE_USER_MEDIA_PERMISSION_REQUEST, nullptr));

    // FIXME: store SecurityOrigins
    UNUSED_PARAM(userMediaDocumentOrigin);
    UNUSED_PARAM(topLevelDocumentOrigin);

    usermediaPermissionRequest->priv->request = &request;
    return usermediaPermissionRequest;
}
