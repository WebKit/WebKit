/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "GStreamerCommon.h"

#define WEBKIT_TYPE_WEBRTC_VIDEO_ENCODER (webkit_webrtc_video_encoder_get_type())
#define WEBKIT_WEBRTC_VIDEO_ENCODER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEBRTC_VIDEO_ENCODER, WebKitWebrtcVideoEncoder))
#define WEBKIT_WEBRTC_VIDEO_ENCODER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_WEBRTC_VIDEO_ENCODER, WebKitWebrtcVideoEncoderClass))
#define WEBKIT_IS_WEBRTC_VIDEO_ENCODER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEBRTC_VIDEO_ENCODER))
#define WEBKIT_IS_WEBRTC_VIDEO_ENCODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_WEBRTC_VIDEO_ENCODER))

typedef struct _WebKitWebrtcVideoEncoder WebKitWebrtcVideoEncoder;
typedef struct _WebKitWebrtcVideoEncoderClass WebKitWebrtcVideoEncoderClass;
typedef struct _WebKitWebrtcVideoEncoderPrivate WebKitWebrtcVideoEncoderPrivate;

struct _WebKitWebrtcVideoEncoder {
    GstBin parent;

    WebKitWebrtcVideoEncoderPrivate* priv;
};

struct _WebKitWebrtcVideoEncoderClass {
    GstBinClass parentClass;
};

GType webkit_webrtc_video_encoder_get_type(void);

bool webrtcVideoEncoderSupportsFormat(WebKitWebrtcVideoEncoder*, const GRefPtr<GstCaps>&);
bool webrtcVideoEncoderSetFormat(WebKitWebrtcVideoEncoder*, GRefPtr<GstCaps>&&);
