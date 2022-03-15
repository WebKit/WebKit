/*
 *  Copyright (C) 2022 Igalia, S.L
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

#pragma once

#if ENABLE(VIDEO)

#include <gst/gst.h>

namespace WebCore {
class MediaPlayerPrivateGStreamer;
}

G_BEGIN_DECLS

#define WEBKIT_TYPE_DMABUF_VIDEO_SINK            (webkit_dmabuf_video_sink_get_type ())
#define WEBKIT_DMABUF_VIDEO_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_DMABUF_VIDEO_SINK, WebKitDMABufVideoSink))
#define WEBKIT_DMABUF_VIDEO_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_DMABUF_VIDEO_SINK, WebKitDMABufVideoSinkClass))
#define WEBKIT_IS_DMABUF_VIDEO_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_DMABUF_VIDEO_SINK))
#define WEBKIT_IS_DMABUF_VIDEO_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_DMABUF_VIDEO_SINK))

typedef struct _WebKitDMABufVideoSink        WebKitDMABufVideoSink;
typedef struct _WebKitDMABufVideoSinkClass   WebKitDMABufVideoSinkClass;
typedef struct _WebKitDMABufVideoSinkPrivate WebKitDMABufVideoSinkPrivate;

struct _WebKitDMABufVideoSink {
    GstBin parent;

    WebKitDMABufVideoSinkPrivate *priv;
};

struct _WebKitDMABufVideoSinkClass {
    GstBinClass parentClass;
};

GType webkit_dmabuf_video_sink_get_type(void);

bool webKitDMABufVideoSinkIsEnabled();
bool webKitDMABufVideoSinkProbePlatform();
void webKitDMABufVideoSinkSetMediaPlayerPrivate(WebKitDMABufVideoSink*, WebCore::MediaPlayerPrivateGStreamer*);

G_END_DECLS

#endif // ENABLE(VIDEO)
