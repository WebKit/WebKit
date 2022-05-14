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

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include <gst/gst.h>
#include <gst/video/gstvideosink.h>

namespace WebCore {
class MediaPlayerPrivateGStreamer;
}

G_BEGIN_DECLS

#define WEBKIT_TYPE_VIDEO_SINK_ELEMENT            (webkit_video_sink_element_get_type ())
#define WEBKIT_VIDEO_SINK_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_VIDEO_SINK_ELEMENT, WebKitVideoSinkElement))
#define WEBKIT_VIDEO_SINK_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_VIDEO_SINK_ELEMENT, WebKitGideoSinkElementClass))
#define WEBKIT_IS_VIDEO_SINK_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_VIDEO_SINK_ELEMENT))
#define WEBKIT_IS_VIDEO_SINK_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_VIDEO_SINK_ELEMENT))

typedef struct _WebKitVideoSinkElement        WebKitVideoSinkElement;
typedef struct _WebKitVideoSinkElementClass   WebKitVideoSinkElementClass;
typedef struct _WebKitVideoSinkElementPrivate WebKitVideoSinkElementPrivate;

struct _WebKitVideoSinkElement {
    GstVideoSink parent;

    WebKitVideoSinkElementPrivate *priv;
};

struct _WebKitVideoSinkElementClass {
    GstVideoSinkClass parentClass;
};

GType webkit_video_sink_element_get_type(void);

void webKitVideoSinkElementSetMediaPlayerPrivate(WebKitVideoSinkElement*, WebCore::MediaPlayerPrivateGStreamer*);
void webKitVideoSinkElementSetCaps(WebKitVideoSinkElement*, GRefPtr<GstCaps>&&);

G_END_DECLS

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
