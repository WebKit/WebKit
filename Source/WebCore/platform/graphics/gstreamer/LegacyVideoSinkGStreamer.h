/*
 *  Copyright (C) 2007 OpenedHand
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

#include <glib-object.h>
#include <gst/video/gstvideosink.h>

#define WEBKIT_TYPE_LEGACY_VIDEO_SINK webkit_legacy_video_sink_get_type()

#define WEBKIT_LEGACY_VIDEO_SINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_LEGACY_VIDEO_SINK, WebKitLegacyVideoSink))
#define WEBKIT_LEGACY_VIDEO_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_LEGACY_VIDEO_SINK, WebKitLegacyVideoSinkClass))
#define WEBKIT_IS_LEGACY_VIDEO_SINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_LEGACY_VIDEO_SINK))
#define WEBKIT_IS_LEGACY_VIDEO_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_LEGACY_VIDEO_SINK))
#define WEBKIT_LEGACY_VIDEO_SINK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), WEBKIT_TYPE_LEGACY_VIDEO_SINK, WebKitLegacyVideoSinkClass))

typedef struct _WebKitLegacyVideoSink WebKitLegacyVideoSink;
typedef struct _WebKitLegacyVideoSinkClass WebKitLegacyVideoSinkClass;
typedef struct _WebKitLegacyVideoSinkPrivate WebKitLegacyVideoSinkPrivate;

struct _WebKitLegacyVideoSink {
    GstVideoSink parent;
    WebKitLegacyVideoSinkPrivate* priv;
};

struct _WebKitLegacyVideoSinkClass {
    GstVideoSinkClass parent_class;
};

GType webkit_legacy_video_sink_get_type() G_GNUC_CONST;

GstElement* webkitLegacyVideoSinkNew();

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
