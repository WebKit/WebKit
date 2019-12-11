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

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_WEBRTC_VIDEO_ENCODER (gst_webrtc_video_encoder_get_type())

G_DECLARE_DERIVABLE_TYPE (GstWebrtcVideoEncoder, gst_webrtc_video_encoder, GST, WEBRTC_VIDEO_ENCODER, GstBin)

struct _GstWebrtcVideoEncoderClass
{
  GstBinClass parent_class;
};

G_END_DECLS
