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

#include "config.h"
#include "DMABufVideoSinkGStreamer.h"

#if ENABLE(VIDEO)

#include "GStreamerCommon.h"
#include "GStreamerVideoSinkCommon.h"
#include <gst/allocators/gstdmabuf.h>
#include <mutex>
#include <wtf/glib/WTFGType.h>

#if USE(GBM)
#include "DRMDeviceManager.h"
#endif

using namespace WebCore;

enum {
    PROP_0,
    PROP_STATS,
    PROP_LAST
};

struct _WebKitDMABufVideoSinkPrivate {
    GRefPtr<GstElement> appSink;
    MediaPlayerPrivateGStreamer* mediaPlayerPrivate;
};

GST_DEBUG_CATEGORY_STATIC(webkit_dmabuf_video_sink_debug);
#define GST_CAT_DEFAULT webkit_dmabuf_video_sink_debug

#define GST_WEBKIT_DMABUF_SINK_CAPS_FORMAT_LIST "{ RGBA, RGBx, BGRA, BGRx, I420, YV12, A420, NV12, NV21, Y444, Y41B, Y42B, VUYA, P010_10LE, P010_10BE, P016_LE, P016BE }"

static GstStaticPadTemplate sinkTemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
#if GST_CHECK_VERSION(1, 24, 0)
        GST_VIDEO_DMA_DRM_CAPS_MAKE ";"
#endif
        GST_VIDEO_CAPS_MAKE(GST_WEBKIT_DMABUF_SINK_CAPS_FORMAT_LIST)
        ));

// TODO: this is a list of remaining YUV formats we want to support, but don't currently work (due to improper handling in TextureMapper):
//     YUY2, YVYU, UYVY, VYUY, AYUV

#define webkit_dmabuf_video_sink_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitDMABufVideoSink, webkit_dmabuf_video_sink, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT(webkit_dmabuf_video_sink_debug, "webkitdmabufvideosink", 0, "DMABuf video sink element"))

// WEBKIT_GST_DMABUF_SINK_FORCED_FALLBACK_CAPS_FORMAT env can be used to force a specific format to be used as the only supported format
// by this sink. This is most useful for testing and debugging the rendering pipeline behavior for a given format.
static const char* forcedFallbackCapsFormat()
{
    static char s_format[64] { };
    static std::once_flag s_flag;
    std::call_once(s_flag,
        [&] {
            const char* format = g_getenv("WEBKIT_GST_DMABUF_SINK_FORCED_FALLBACK_CAPS_FORMAT");
            if (format)
                g_strlcpy(const_cast<char*>(s_format), format, 64);
            else
                s_format[0] = 0;
        });

    if (!s_format[0])
        return nullptr;
    return s_format;
}

static void webKitDMABufVideoSinkConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));

    WebKitDMABufVideoSink* sink = WEBKIT_DMABUF_VIDEO_SINK(object);

    sink->priv->appSink = makeGStreamerElement("appsink", "webkit-dmabuf-video-appsink");
    ASSERT(sink->priv->appSink);
    g_object_set(sink->priv->appSink.get(), "enable-last-sample", FALSE, "emit-signals", TRUE, "max-buffers", 1, nullptr);

    gst_bin_add(GST_BIN_CAST(sink), sink->priv->appSink.get());

    // This sink handles dmabuf data or raw data of any format in the supported format list.
    // The dmabuf and raw data types are the two types of data we can handle via this sink (in combination with functionality in
    // MediaPlayerPrivateGStreamer). The format list corresponds to the formats we are able to then handle in the graphics pipeline.
    // In case of dmabuf data, that dmabuf is handled most optimally and just relayed to the graphics pipeline.
    // In case of raw data, dmabuf objects are produced on the spot and filled with that data, and then pushed to the graphics pipeline.

    static GstStaticCaps s_dmabufCaps = GST_STATIC_CAPS(
#if GST_CHECK_VERSION(1, 24, 0)
        GST_VIDEO_DMA_DRM_CAPS_MAKE ";"
#endif
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_DMABUF, GST_WEBKIT_DMABUF_SINK_CAPS_FORMAT_LIST));
    static GstStaticCaps s_fallbackCaps = GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE(GST_WEBKIT_DMABUF_SINK_CAPS_FORMAT_LIST));

    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_new_empty());
    {
        if (forcedFallbackCapsFormat())
            caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, forcedFallbackCapsFormat(), nullptr);
        else {
            gst_caps_append(caps.get(), gst_static_caps_get(&s_dmabufCaps));
            gst_caps_append(caps.get(), gst_static_caps_get(&s_fallbackCaps));
        }
    }
    g_object_set(sink->priv->appSink.get(), "caps", caps.get(), nullptr);

    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(sink->priv->appSink.get(), "sink"));
    gst_element_add_pad(GST_ELEMENT_CAST(sink), gst_ghost_pad_new("sink", pad.get()));
}

void webKitDMABufVideoSinkFinalize(GObject* object)
{
    ASSERT(isMainThread());

    WebKitDMABufVideoSink* sink = WEBKIT_DMABUF_VIDEO_SINK(object);
    WebKitDMABufVideoSinkPrivate* priv = sink->priv;

    if (priv->mediaPlayerPrivate)
        g_signal_handlers_disconnect_by_data(priv->appSink.get(), priv->mediaPlayerPrivate);

    GST_DEBUG_OBJECT(object, "WebKitDMABufVideoSink finalized.");

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void webKitDMABufVideoSinkGetProperty(GObject* object, guint propertyId, GValue* value, GParamSpec* paramSpec)
{
    WebKitDMABufVideoSink* sink = WEBKIT_DMABUF_VIDEO_SINK(object);

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

static void webkit_dmabuf_video_sink_class_init(WebKitDMABufVideoSinkClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);

    objectClass->constructed = webKitDMABufVideoSinkConstructed;
    objectClass->finalize = webKitDMABufVideoSinkFinalize;
    objectClass->get_property = webKitDMABufVideoSinkGetProperty;

    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&sinkTemplate));
    gst_element_class_set_static_metadata(elementClass, "WebKit DMABuf video sink", "Sink/Video", "Renders video", "Zan Dobersek <zdobersek@igalia.com>");

    g_object_class_install_property(objectClass, PROP_STATS, g_param_spec_boxed("stats",
        nullptr, nullptr, GST_TYPE_STRUCTURE, static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

bool webKitDMABufVideoSinkIsEnabled()
{
    static bool s_disabled = false;
#if USE(GBM)
    static std::once_flag s_flag;
    std::call_once(s_flag, [&] {
        const char* value = g_getenv("WEBKIT_GST_DMABUF_SINK_DISABLED");
        auto valueSpan = span(value);
        s_disabled = value && (equalLettersIgnoringASCIICase(valueSpan, "true"_s) || equalLettersIgnoringASCIICase(valueSpan, "1"_s));
        if (!s_disabled && !DRMDeviceManager::singleton().mainGBMDeviceNode(DRMDeviceManager::NodeType::Render)) {
            WTFLogAlways("Unable to access the GBM device, disabling DMABuf video sink.");
            s_disabled = true;
        }
    });
#else
    s_disabled = true;
#endif
    return !s_disabled;
}

bool webKitDMABufVideoSinkProbePlatform()
{
    return webkitGstCheckVersion(1, 20, 0) && isGStreamerPluginAvailable("app");
}

void webKitDMABufVideoSinkSetMediaPlayerPrivate(WebKitDMABufVideoSink* sink, MediaPlayerPrivateGStreamer* player)
{
    WebKitDMABufVideoSinkPrivate* priv = sink->priv;

    priv->mediaPlayerPrivate = player;
    webKitVideoSinkSetMediaPlayerPrivate(priv->appSink.get(), priv->mediaPlayerPrivate);
}

#undef GST_CAT_DEFAULT

#endif // ENABLE(VIDEO)
