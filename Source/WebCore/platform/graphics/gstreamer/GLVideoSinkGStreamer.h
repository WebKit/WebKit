/*
 *  Copyright (C) 2019 Igalia, S.L
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

#if ENABLE(VIDEO) && USE(GSTREAMER_GL)

#include <gst/gst.h>

namespace WebCore {
class MediaPlayerPrivateGStreamer;
}

G_BEGIN_DECLS

#define WEBKIT_TYPE_GL_VIDEO_SINK            (webkit_gl_video_sink_get_type ())
#define WEBKIT_GL_VIDEO_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_GL_VIDEO_SINK, WebKitGLVideoSink))
#define WEBKIT_GL_VIDEO_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_GL_VIDEO_SINK, WebKitGLVideoSinkClass))
#define WEBKIT_IS_GL_VIDEO_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_GL_VIDEO_SINK))
#define WEBKIT_IS_GL_VIDEO_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_GL_VIDEO_SINK))

typedef struct _WebKitGLVideoSink        WebKitGLVideoSink;
typedef struct _WebKitGLVideoSinkClass   WebKitGLVideoSinkClass;
typedef struct _WebKitGLVideoSinkPrivate WebKitGLVideoSinkPrivate;

struct _WebKitGLVideoSink {
    GstBin parent;

    WebKitGLVideoSinkPrivate *priv;
};

struct _WebKitGLVideoSinkClass {
    GstBinClass parentClass;
};

GType webkit_gl_video_sink_get_type(void);

bool webKitGLVideoSinkProbePlatform();
void webKitGLVideoSinkSetMediaPlayerPrivate(WebKitGLVideoSink*, WebCore::MediaPlayerPrivateGStreamer*);

G_END_DECLS

#endif // ENABLE(VIDEO) && USE(GSTREAMER_GL)
