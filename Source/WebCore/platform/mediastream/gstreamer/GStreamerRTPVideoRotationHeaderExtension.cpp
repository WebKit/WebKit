/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
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
#include "GStreamerRTPVideoRotationHeaderExtension.h"

#if USE(GSTREAMER_WEBRTC)

#include "VideoFrameMetadataGStreamer.h"
#include <gst/rtp/rtp.h>
#include <wtf/PrintStream.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

typedef struct _RTPVideoRotationHeaderExtensionPrivate {
} RTPVideoRotationHeaderExtensionPrivate;

typedef struct _RTPVideoRotationHeaderExtension {
    GstRTPHeaderExtension parent;
    RTPVideoRotationHeaderExtensionPrivate* priv;
} RTPVideoRotationHeaderExtension;

typedef struct _RTPVideoRotationHeaderExtensionClass {
    GstRTPHeaderExtensionClass parentClass;
} RTPVideoRotationHeaderExtensionClass;

GST_DEBUG_CATEGORY_STATIC(extensionDebug);
#define GST_CAT_DEFAULT extensionDebug

WEBKIT_DEFINE_TYPE_WITH_CODE(RTPVideoRotationHeaderExtension, webkit_gst_rtp_video_rotation_header_extension, GST_TYPE_RTP_HEADER_EXTENSION, GST_DEBUG_CATEGORY_INIT(extensionDebug, "webkitrtpvideorotation", 0, "RTP Video Header Extension"))

static GstRTPHeaderExtensionFlags extensionGetSupportedFlags(GstRTPHeaderExtension*)
{
    return static_cast<GstRTPHeaderExtensionFlags>(GST_RTP_HEADER_EXTENSION_ONE_BYTE | GST_RTP_HEADER_EXTENSION_TWO_BYTE);
}

static gsize extensionGetMaxSize(GstRTPHeaderExtension*, const GstBuffer*)
{
    return 1;
}

static gssize extensionWrite(GstRTPHeaderExtension* extension, const GstBuffer* inputBuffer, GstRTPHeaderExtensionFlags, GstBuffer*, guint8* data, gsize)
{
    auto [rotation, isMirrored] = webkitGstBufferGetVideoRotation(const_cast<GstBuffer*>(inputBuffer));
    gssize written = 1;

    switch (rotation) {
    case VideoFrame::Rotation::None:
        data[0] = 0x0;
        break;
    case VideoFrame::Rotation::Left:
        data[0] = 0x1;
        break;
    case VideoFrame::Rotation::UpsideDown:
        data[0] = 0x2;
        break;
    case VideoFrame::Rotation::Right:
        data[0] = 0x3;
        break;
    }

    if (isMirrored)
        data[0] |= 1 << 3;

    GST_TRACE_OBJECT(extension, "Wrote %" G_GSSIZE_FORMAT " bytes from video rotation %u (flipped: %s) to byte 0x%x", written, static_cast<unsigned>(rotation), WTF::boolForPrinting(isMirrored), data[0]);
    return written;
}

static gboolean extensionRead(GstRTPHeaderExtension* extension, GstRTPHeaderExtensionFlags, const guint8* data, gsize, GstBuffer* buffer)
{
    VideoFrame::Rotation rotation = VideoFrame::Rotation::None;
    uint8_t firstByte = data[0];

    switch (firstByte & 0x7) {
    case 0:
        break;
    case 1:
        rotation = VideoFrame::Rotation::Left;
        break;
    case 2:
        rotation = VideoFrame::Rotation::UpsideDown;
        break;
    case 3:
        rotation = VideoFrame::Rotation::Right;
        break;
    default:
        return FALSE;
    }

    bool isMirrored = (firstByte >> 3) & 0x1;
    GST_TRACE_OBJECT(extension, "Read byte 0x%x to video rotation %u (flipped: %s)", firstByte, static_cast<unsigned>(rotation), boolForPrinting(isMirrored));
    webkitGstBufferAddVideoFrameMetadata(buffer, { }, rotation, isMirrored, VideoFrameContentHint::None);
    return TRUE;
}

static void webkit_gst_rtp_video_rotation_header_extension_class_init(RTPVideoRotationHeaderExtensionClass* klass)
{
    auto rtpHeaderExtensionClass = GST_RTP_HEADER_EXTENSION_CLASS(klass);

    rtpHeaderExtensionClass->get_supported_flags = extensionGetSupportedFlags;
    rtpHeaderExtensionClass->get_max_size = extensionGetMaxSize;
    rtpHeaderExtensionClass->write = extensionWrite;
    rtpHeaderExtensionClass->read = extensionRead;

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), "3GPP Orientation RTP Header Extension", GST_RTP_HDREXT_ELEMENT_CLASS,
        "Read/write 3GPP Orientation from/to RTP packets", "Philippe Normand <philn@igalia.com>");
    gst_rtp_header_extension_class_set_uri(rtpHeaderExtensionClass, "urn:3gpp:video-orientation");
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC)
