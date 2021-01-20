/*
 *  Copyright (C) 2020 Igalia S.L.
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

#if USE(GSTREAMER)

#include <gst/gst.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_AUDIO_SINK            (webkit_audio_sink_get_type())
#define WEBKIT_AUDIO_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_AUDIO_SINK, WebKitAudioSink))
#define WEBKIT_AUDIO_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_AUDIO_SINK, WebKitAudioSinkClass))
#define WEBKIT_IS_AUDIO_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_AUDIO_SINK))
#define WEBKIT_IS_AUDIO_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_AUDIO_SINK))

typedef struct _WebKitAudioSink        WebKitAudioSink;
typedef struct _WebKitAudioSinkClass   WebKitAudioSinkClass;
typedef struct _WebKitAudioSinkPrivate WebKitAudioSinkPrivate;

struct _WebKitAudioSink {
    GstBin parent;

    WebKitAudioSinkPrivate *priv;
};

struct _WebKitAudioSinkClass {
    GstBinClass parentClass;
};

GType webkit_audio_sink_get_type(void);

G_END_DECLS

GstElement* webkitAudioSinkNew();

#endif // USE(GSTREAMER)
