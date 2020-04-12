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

#include "GLContext.h"
#include "GStreamerCommon.h"
#include "MediaPlayerPrivateGStreamer.h"
#include <gst/app/gstappsink.h>
#include <wtf/glib/WTFGType.h>

#if USE(GLX)
#include "GLContextGLX.h"
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif

#if USE(EGL)
#include "GLContextEGL.h"
#include <gst/gl/egl/gstgldisplay_egl.h>
#endif

#if PLATFORM(X11)
#include "PlatformDisplayX11.h"
#endif

#if PLATFORM(WAYLAND)
#include "PlatformDisplayWayland.h"
#endif

#if USE(WPE_RENDERER)
#include "PlatformDisplayLibWPE.h"
#endif

// gstglapi.h may include eglplatform.h and it includes X.h, which
// defines None, breaking MediaPlayer::None enum
#if PLATFORM(X11) && GST_GL_HAVE_PLATFORM_EGL
#undef None
#endif // PLATFORM(X11) && GST_GL_HAVE_PLATFORM_EGL

using namespace WebCore;

enum {
    PROP_0,
    PROP_STATS,
    PROP_LAST
};

struct _WebKitGLVideoSinkPrivate {
    GRefPtr<GstElement> appSink;
    GRefPtr<GstGLContext> glContext;
    GRefPtr<GstGLDisplay> glDisplay;
    GRefPtr<GstContext> glDisplayElementContext;
    GRefPtr<GstContext> glAppElementContext;
    MediaPlayerPrivateGStreamer* mediaPlayerPrivate;
};

GST_DEBUG_CATEGORY_STATIC(webkit_gl_video_sink_debug);
#define GST_CAT_DEFAULT webkit_gl_video_sink_debug

#define GST_GL_CAPS_FORMAT "{ RGBx, RGBA, I420, Y444, YV12, Y41B, Y42B, NV12, NV21, VUYA }"
static GstStaticPadTemplate sinkTemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

#define webkit_gl_video_sink_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitGLVideoSink, webkit_gl_video_sink, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT(webkit_gl_video_sink_debug, "webkitglvideosink", 0, "GL video sink element"))

static void webKitGLVideoSinkConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));

    WebKitGLVideoSink* sink = WEBKIT_GL_VIDEO_SINK(object);

    sink->priv->appSink = gst_element_factory_make("appsink", "webkit-gl-video-appsink");
    ASSERT(sink->priv->appSink);
    g_object_set(sink->priv->appSink.get(), "enable-last-sample", FALSE, "emit-signals", TRUE, "max-buffers", 1, nullptr);

    GstElement* upload = gst_element_factory_make("glupload", nullptr);
    GstElement* colorconvert = gst_element_factory_make("glcolorconvert", nullptr);
    ASSERT(upload);
    ASSERT(colorconvert);
    gst_bin_add_many(GST_BIN_CAST(sink), upload, colorconvert, sink->priv->appSink.get(), nullptr);

    // Workaround until we can depend on GStreamer 1.16.2.
    // https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/commit/8d32de090554cf29fe359f83aa46000ba658a693
    // Forcing a color conversion to RGBA here allows glupload to internally use
    // an uploader that adds a VideoMeta, through the TextureUploadMeta caps
    // feature, without needing the patch above. However this specific caps
    // feature is going to be removed from GStreamer so it is considered a
    // short-term workaround. This code path most likely will have a negative
    // performance impact on embedded platforms as well. Downstream embedders
    // are highly encouraged to cherry-pick the patch linked above in their BSP
    // and set the WEBKIT_GST_NO_RGBA_CONVERSION environment variable until
    // GStreamer 1.16.2 is released.
    // See also https://bugs.webkit.org/show_bug.cgi?id=201422
    GRefPtr<GstCaps> caps;
    if (webkitGstCheckVersion(1, 16, 2) || getenv("WEBKIT_GST_NO_RGBA_CONVERSION"))
        caps = adoptGRef(gst_caps_from_string("video/x-raw, format = (string) " GST_GL_CAPS_FORMAT));
    else {
        GST_INFO_OBJECT(sink, "Forcing RGBA as GStreamer is not new enough.");
        caps = adoptGRef(gst_caps_from_string("video/x-raw, format = (string) RGBA"));
    }
    gst_caps_set_features(caps.get(), 0, gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_GL_MEMORY, nullptr));
    g_object_set(sink->priv->appSink.get(), "caps", caps.get(), nullptr);

    gst_element_link_many(upload, colorconvert, sink->priv->appSink.get(), nullptr);

    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(upload, "sink"));
    gst_element_add_pad(GST_ELEMENT_CAST(sink), gst_ghost_pad_new("sink", pad.get()));
}

void webKitGLVideoSinkFinalize(GObject* object)
{
    ASSERT(isMainThread());

    WebKitGLVideoSink* sink = WEBKIT_GL_VIDEO_SINK(object);
    WebKitGLVideoSinkPrivate* priv = sink->priv;

    if (priv->mediaPlayerPrivate)
        g_signal_handlers_disconnect_by_data(priv->appSink.get(), priv->mediaPlayerPrivate);

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static bool ensureGstGLContext(WebKitGLVideoSink* sink)
{
    WebKitGLVideoSinkPrivate* priv = sink->priv;

    if (priv->glContext)
        return true;

    auto& sharedDisplay = PlatformDisplay::sharedDisplayForCompositing();

    // The floating ref removal support was added in https://bugzilla.gnome.org/show_bug.cgi?id=743062.
    bool shouldAdoptRef = webkitGstCheckVersion(1, 14, 0);
    if (!priv->glDisplay) {
#if PLATFORM(X11)
#if USE(GLX)
        if (is<PlatformDisplayX11>(sharedDisplay)) {
            GST_DEBUG_OBJECT(sink, "Creating X11 shared GL display");
            if (shouldAdoptRef)
                priv->glDisplay = adoptGRef(GST_GL_DISPLAY(gst_gl_display_x11_new_with_display(downcast<PlatformDisplayX11>(sharedDisplay).native())));
            else
                priv->glDisplay = GST_GL_DISPLAY(gst_gl_display_x11_new_with_display(downcast<PlatformDisplayX11>(sharedDisplay).native()));
        }
#elif USE(EGL)
        if (is<PlatformDisplayX11>(sharedDisplay)) {
            GST_DEBUG_OBJECT(sink, "Creating X11 shared EGL display");
            if (shouldAdoptRef)
                priv->glDisplay = adoptGRef(GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(downcast<PlatformDisplayX11>(sharedDisplay).eglDisplay())));
            else
                priv->glDisplay = GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(downcast<PlatformDisplayX11>(sharedDisplay).eglDisplay()));
        }
#endif
#endif

#if PLATFORM(WAYLAND)
        if (is<PlatformDisplayWayland>(sharedDisplay)) {
            GST_DEBUG_OBJECT(sink, "Creating Wayland shared display");
            if (shouldAdoptRef)
                priv->glDisplay = adoptGRef(GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(downcast<PlatformDisplayWayland>(sharedDisplay).eglDisplay())));
            else
                priv->glDisplay = GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(downcast<PlatformDisplayWayland>(sharedDisplay).eglDisplay()));
        }
#endif

#if USE(WPE_RENDERER)
        if (is<PlatformDisplayLibWPE>(sharedDisplay)) {
            GST_DEBUG_OBJECT(sink, "Creating WPE shared EGL display");
            if (shouldAdoptRef)
                priv->glDisplay = adoptGRef(GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(downcast<PlatformDisplayLibWPE>(sharedDisplay).eglDisplay())));
            else
                priv->glDisplay = GST_GL_DISPLAY(gst_gl_display_egl_new_with_egl_display(downcast<PlatformDisplayLibWPE>(sharedDisplay).eglDisplay()));
        }
#endif

        ASSERT(priv->glDisplay);
    }

    GLContext* sharedContext = sharedDisplay.sharingGLContext();
    if (!sharedContext) {
        GST_ELEMENT_ERROR(GST_ELEMENT_CAST(sink), RESOURCE, NOT_FOUND, (("WebKit shared GL context unavailable")),
            ("The WebKit shared GL context somehow disappeared. Video textures rendering will fail."));
        return false;
    }

    // EGL and GLX are mutually exclusive, no need for ifdefs here.
    GstGLPlatform glPlatform = sharedContext->isEGLContext() ? GST_GL_PLATFORM_EGL : GST_GL_PLATFORM_GLX;

#if USE(OPENGL_ES)
    GstGLAPI glAPI = GST_GL_API_GLES2;
#elif USE(OPENGL)
    GstGLAPI glAPI = GST_GL_API_OPENGL;
#else
    ASSERT_NOT_REACHED();
#endif

    PlatformGraphicsContextGL contextHandle = sharedContext->platformContext();
    if (!contextHandle)
        return false;

    if (shouldAdoptRef)
        priv->glContext = adoptGRef(gst_gl_context_new_wrapped(priv->glDisplay.get(), reinterpret_cast<guintptr>(contextHandle), glPlatform, glAPI));
    else
        priv->glContext = gst_gl_context_new_wrapped(priv->glDisplay.get(), reinterpret_cast<guintptr>(contextHandle), glPlatform, glAPI);

    // Activate and fill the GStreamer wrapped context with the Webkit's shared one.
    auto* previousActiveContext = GLContext::current();
    sharedContext->makeContextCurrent();
    if (gst_gl_context_activate(priv->glContext.get(), TRUE)) {
        GUniqueOutPtr<GError> error;
        if (!gst_gl_context_fill_info(priv->glContext.get(), &error.outPtr()))
            GST_WARNING("Failed to fill in GStreamer context: %s", error->message);
        gst_gl_context_activate(priv->glContext.get(), FALSE);
    } else
        GST_WARNING("Failed to activate GStreamer context %" GST_PTR_FORMAT, priv->glContext.get());
    if (previousActiveContext)
        previousActiveContext->makeContextCurrent();

    return true;
}

GRefPtr<GstContext> requestGLContext(WebKitGLVideoSink* sink, const char* contextType)
{
    WebKitGLVideoSinkPrivate* priv = sink->priv;
    if (!ensureGstGLContext(sink))
        return nullptr;

    if (!g_strcmp0(contextType, GST_GL_DISPLAY_CONTEXT_TYPE)) {
        GstContext* displayContext = gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
        gst_context_set_gl_display(displayContext, priv->glDisplay.get());
        return adoptGRef(displayContext);
    }

    if (!g_strcmp0(contextType, "gst.gl.app_context")) {
        GstContext* appContext = gst_context_new("gst.gl.app_context", TRUE);
        GstStructure* structure = gst_context_writable_structure(appContext);
#if GST_CHECK_VERSION(1, 12, 0)
        gst_structure_set(structure, "context", GST_TYPE_GL_CONTEXT, priv->glContext.get(), nullptr);
#else
        gst_structure_set(structure, "context", GST_GL_TYPE_CONTEXT, priv->glContext.get(), nullptr);
#endif
        return adoptGRef(appContext);
    }

    return nullptr;
}

static GstStateChangeReturn webKitGLVideoSinkChangeState(GstElement* element, GstStateChange transition)
{
    WebKitGLVideoSink* sink = WEBKIT_GL_VIDEO_SINK(element);
    WebKitGLVideoSinkPrivate* priv = sink->priv;

#if GST_CHECK_VERSION(1, 14, 0)
    GST_DEBUG_OBJECT(element, "%s", gst_state_change_get_name(transition));
#endif

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
#if GST_CHECK_VERSION(1, 14, 0)
    case GST_STATE_CHANGE_READY_TO_READY:
#endif
    case GST_STATE_CHANGE_READY_TO_PAUSED: {
        if (!priv->glDisplayElementContext)
            priv->glDisplayElementContext = requestGLContext(sink, GST_GL_DISPLAY_CONTEXT_TYPE);

        if (priv->glDisplayElementContext)
            gst_element_set_context(GST_ELEMENT_CAST(sink), priv->glDisplayElementContext.get());

        if (!priv->glAppElementContext)
            priv->glAppElementContext = requestGLContext(sink, "gst.gl.app_context");

        if (priv->glAppElementContext)
            gst_element_set_context(GST_ELEMENT_CAST(sink), priv->glAppElementContext.get());
        break;
    }
    default:
        break;
    }

    return GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
}

static void webKitGLVideoSinkGetProperty(GObject* object, guint propertyId, GValue* value, GParamSpec* paramSpec)
{
    WebKitGLVideoSink* sink = WEBKIT_GL_VIDEO_SINK(object);

    switch (propertyId) {
    case PROP_STATS:
        if (webkitGstCheckVersion(1, 17, 0)) {
            GUniqueOutPtr<GstStructure> stats;
            g_object_get(sink->priv->appSink.get(), "stats", &stats.outPtr(), nullptr);
            gst_value_set_structure(value, stats.get());
        }
        break;
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

    g_object_class_install_property(objectClass, PROP_STATS, g_param_spec_boxed("stats", "Statistics",
        "Sink Statistics", GST_TYPE_STRUCTURE, static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    elementClass->change_state = GST_DEBUG_FUNCPTR(webKitGLVideoSinkChangeState);
}

void webKitGLVideoSinkSetMediaPlayerPrivate(WebKitGLVideoSink* sink, MediaPlayerPrivateGStreamer* player)
{
    WebKitGLVideoSinkPrivate* priv = sink->priv;

    priv->mediaPlayerPrivate = player;
    g_signal_connect(priv->appSink.get(), "new-sample", G_CALLBACK(+[](GstElement* sink, MediaPlayerPrivateGStreamer* player) -> GstFlowReturn {
        GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));
        player->triggerRepaint(sample.get());
        return GST_FLOW_OK;
    }), player);
    g_signal_connect(priv->appSink.get(), "new-preroll", G_CALLBACK(+[](GstElement* sink, MediaPlayerPrivateGStreamer* player) -> GstFlowReturn {
        GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_preroll(GST_APP_SINK(sink)));
        player->triggerRepaint(sample.get());
        return GST_FLOW_OK;
    }), player);

    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(priv->appSink.get(), "sink"));
    gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_PUSH | GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_FLUSH), [] (GstPad*, GstPadProbeInfo* info,  gpointer userData) -> GstPadProbeReturn {
        // In some platforms (e.g. OpenMAX on the Raspberry Pi) when a resolution change occurs the
        // pipeline has to be drained before a frame with the new resolution can be decoded.
        // In this context, it's important that we don't hold references to any previous frame
        // (e.g. m_sample) so that decoding can continue.
        // We are also not supposed to keep the original frame after a flush.
        if (info->type & GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM) {
            if (GST_QUERY_TYPE(GST_PAD_PROBE_INFO_QUERY(info)) != GST_QUERY_DRAIN)
                return GST_PAD_PROBE_OK;
            GST_DEBUG("Acting upon DRAIN query");
        }
        if (info->type & GST_PAD_PROBE_TYPE_EVENT_FLUSH) {
            if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT(info)) != GST_EVENT_FLUSH_START)
                return GST_PAD_PROBE_OK;
            GST_DEBUG("Acting upon flush-start event");
        }

        auto* player = static_cast<MediaPlayerPrivateGStreamer*>(userData);
        player->flushCurrentBuffer();
        return GST_PAD_PROBE_OK;
    }, player, nullptr);
}

bool webKitGLVideoSinkProbePlatform()
{
    auto& sharedDisplay = PlatformDisplay::sharedDisplayForCompositing();
    GLContext* sharedContext = sharedDisplay.sharingGLContext();
    if (!sharedContext) {
        GST_WARNING("WebKit shared GL context is not available.");
        return false;
    }

    return isGStreamerPluginAvailable("app") && isGStreamerPluginAvailable("opengl");
}

#endif
