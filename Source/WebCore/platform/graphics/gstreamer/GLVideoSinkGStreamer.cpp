/*
 * Copyright (C) 2019 Igalia S.L
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

#include "config.h"
#include "GLVideoSinkGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER_GL)

#include "GStreamerCommon.h"
#include "GStreamerVideoSinkCommon.h"
#define GST_USE_UNSTABLE_API
#include <gst/gl/gstglmemory.h>
#undef GST_USE_UNSTABLE_API
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

enum {
    PROP_0,
    PROP_STATS,
    PROP_LAST
};

struct _WebKitGLVideoSinkPrivate {
    GRefPtr<GstElement> appSink;
    MediaPlayerPrivateGStreamer* mediaPlayerPrivate;
};

GST_DEBUG_CATEGORY_STATIC(webkit_gl_video_sink_debug);
#define GST_CAT_DEFAULT webkit_gl_video_sink_debug

#define GST_GL_CAPS_FORMAT "{ A420, RGBx, RGBA, I420, Y444, YV12, Y41B, Y42B, NV12, NV21, VUYA }"
static GstStaticPadTemplate sinkTemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

#define webkit_gl_video_sink_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitGLVideoSink, webkit_gl_video_sink, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT(webkit_gl_video_sink_debug, "webkitglvideosink", 0, "GL video sink element"))

static void webKitGLVideoSinkConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));

    WebKitGLVideoSink* sink = WEBKIT_GL_VIDEO_SINK(object);

    GST_OBJECT_FLAG_SET(GST_OBJECT_CAST(sink), GST_ELEMENT_FLAG_SINK);
    gst_bin_set_suppressed_flags(GST_BIN_CAST(sink), static_cast<GstElementFlags>(GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK));

    sink->priv->appSink = makeGStreamerElement("appsink", "webkit-gl-video-appsink");
    ASSERT(sink->priv->appSink);
    g_object_set(sink->priv->appSink.get(), "enable-last-sample", FALSE, "emit-signals", TRUE, "max-buffers", 1, nullptr);

    auto* imxVideoConvertG2D =
        []() -> GstElement*
        {
            auto elementFactor = adoptGRef(gst_element_factory_find("imxvideoconvert_g2d"));
            if (elementFactor)
                return gst_element_factory_create(elementFactor.get(), nullptr);
            return nullptr;
        }();
    if (imxVideoConvertG2D)
        gst_bin_add(GST_BIN_CAST(sink), imxVideoConvertG2D);

    GstElement* upload = makeGStreamerElement("glupload", nullptr);
    GstElement* colorconvert = makeGStreamerElement("glcolorconvert", nullptr);

    ASSERT(upload);
    ASSERT(colorconvert);
    gst_bin_add_many(GST_BIN_CAST(sink), upload, colorconvert, sink->priv->appSink.get(), nullptr);

    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string("video/x-raw, format = (string) " GST_GL_CAPS_FORMAT));
    gst_caps_set_features(caps.get(), 0, gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_GL_MEMORY, nullptr));
    g_object_set(sink->priv->appSink.get(), "caps", caps.get(), nullptr);

    if (imxVideoConvertG2D)
        gst_element_link(imxVideoConvertG2D, upload);
    gst_element_link(upload, colorconvert);

    gst_element_link(colorconvert, sink->priv->appSink.get());

    GstElement* sinkElement =
        [&] {
            if (imxVideoConvertG2D)
                return imxVideoConvertG2D;
            return upload;
        }();
    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(sinkElement, "sink"));
    gst_element_add_pad(GST_ELEMENT_CAST(sink), gst_ghost_pad_new("sink", pad.get()));
}

void webKitGLVideoSinkFinalize(GObject* object)
{
    ASSERT(isMainThread());

    WebKitGLVideoSink* sink = WEBKIT_GL_VIDEO_SINK(object);
    WebKitGLVideoSinkPrivate* priv = sink->priv;

    if (priv->mediaPlayerPrivate)
        g_signal_handlers_disconnect_by_data(priv->appSink.get(), priv->mediaPlayerPrivate);

    GST_DEBUG_OBJECT(object, "WebKitGLVideoSink finalized.");

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void webKitGLVideoSinkGetProperty(GObject* object, guint propertyId, GValue* value, GParamSpec* paramSpec)
{
    WebKitGLVideoSink* sink = WEBKIT_GL_VIDEO_SINK(object);

    switch (propertyId) {
    case PROP_STATS: {
        GUniqueOutPtr<GstStructure> stats;
        g_object_get(sink->priv->appSink.get(), "stats", &stats.outPtr(), nullptr);
        gst_value_set_structure(value, stats.get());
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, paramSpec);
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

static void webkit_gl_video_sink_class_init(WebKitGLVideoSinkClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);

    objectClass->constructed = webKitGLVideoSinkConstructed;
    objectClass->finalize = webKitGLVideoSinkFinalize;
    objectClass->get_property = webKitGLVideoSinkGetProperty;

    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&sinkTemplate));
    gst_element_class_set_static_metadata(elementClass, "WebKit GL video sink", "Sink/Video", "Renders video", "Philippe Normand <philn@igalia.com>");

    g_object_class_install_property(objectClass, PROP_STATS, g_param_spec_boxed("stats",
        nullptr, nullptr, GST_TYPE_STRUCTURE, static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

void webKitGLVideoSinkSetMediaPlayerPrivate(WebKitGLVideoSink* sink, MediaPlayerPrivateGStreamer* player)
{
    WebKitGLVideoSinkPrivate* priv = sink->priv;

    priv->mediaPlayerPrivate = player;
    webKitVideoSinkSetMediaPlayerPrivate(priv->appSink.get(), priv->mediaPlayerPrivate);
}

bool webKitGLVideoSinkProbePlatform()
{
    return isGStreamerPluginAvailable("app") && isGStreamerPluginAvailable("opengl");
}

#endif // ENABLE(VIDEO) && USE(GSTREAMER_GL)
