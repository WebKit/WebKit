/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
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

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include <gst/gst.h>

namespace WebCore {
class MediaStreamPrivate;
class MediaStreamTrackPrivate;
}

#define WEBKIT_MEDIA_TRACK_TAG_WIDTH "webkit-media-stream-width"
#define WEBKIT_MEDIA_TRACK_TAG_HEIGHT "webkit-media-stream-height"
#define WEBKIT_MEDIA_TRACK_TAG_KIND "webkit-media-stream-kind"

#define WEBKIT_MEDIA_STREAM_SRC(o) (G_TYPE_CHECK_INSTANCE_CAST((o), WEBKIT_TYPE_MEDIA_STREAM_SRC, WebKitMediaStreamSrc))
#define WEBKIT_IS_MEDIA_STREAM_SRC(o) (G_TYPE_CHECK_INSTANCE_TYPE((o), WEBKIT_TYPE_MEDIA_STREAM_SRC))
#define WEBKIT_MEDIA_STREAM_SRC_CAST(o) ((WebKitMediaStreamSrc*) o)

#define WEBKIT_TYPE_MEDIA_STREAM_SRC (webkit_media_stream_src_get_type())
GType webkit_media_stream_src_get_type(void);

typedef struct _WebKitMediaStreamSrc WebKitMediaStreamSrc;
typedef struct _WebKitMediaStreamSrcClass WebKitMediaStreamSrcClass;
typedef struct _WebKitMediaStreamSrcPrivate WebKitMediaStreamSrcPrivate;

struct _WebKitMediaStreamSrc {
    GstBin parent;
    WebKitMediaStreamSrcPrivate* priv;
};

struct _WebKitMediaStreamSrcClass {
    GstBinClass parentClass;
};

GstElement* webkitMediaStreamSrcNew();
void webkitMediaStreamSrcSetStream(WebKitMediaStreamSrc*, WebCore::MediaStreamPrivate*, bool isVideoPlayer);
void webkitMediaStreamSrcAddTrack(WebKitMediaStreamSrc*, WebCore::MediaStreamTrackPrivate*, bool onlyTrack);
void webkitMediaStreamSrcConfigureAudioTracks(WebKitMediaStreamSrc*, float volume, bool isMuted, bool isPlaying);

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
