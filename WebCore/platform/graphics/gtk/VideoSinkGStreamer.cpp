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

/**
 * SECTION:webkit-video-sink
 * @short_description: GStreamer video sink
 *
 * #WebKitVideoSink is a GStreamer sink element that sends
 * data to a #cairo_surface_t.
 */

#include "config.h"
#include "VideoSinkGStreamer.h"

#include <glib.h>
#include <gst/gst.h>
#include <gst/video/video.h>

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE("sink",
        GST_PAD_SINK, GST_PAD_ALWAYS,
        GST_STATIC_CAPS(GST_VIDEO_CAPS_RGBx ";" GST_VIDEO_CAPS_BGRx));

GST_DEBUG_CATEGORY_STATIC(webkit_video_sink_debug);
#define GST_CAT_DEFAULT webkit_video_sink_debug

static GstElementDetails webkit_video_sink_details =
  GST_ELEMENT_DETAILS((gchar*) "WebKit video sink",
                      (gchar*) "Sink/Video",
                      (gchar*) "Sends video data from a GStreamer pipeline to a Cairo surface",
                      (gchar*) "Alp Toker <alp@atoker.com>");

enum {
    REPAINT_REQUESTED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_SURFACE
};

static guint webkit_video_sink_signals[LAST_SIGNAL] = { 0, };

struct _WebKitVideoSinkPrivate {
    cairo_surface_t* surface;
    GAsyncQueue* async_queue;
    gboolean rgb_ordering;
    int width;
    int height;
    int fps_n;
    int fps_d;
    int par_n;
    int par_d;
};

#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (webkit_video_sink_debug, \
                             "webkitsink", \
                             0, \
                             "webkit video sink")

GST_BOILERPLATE_FULL(WebKitVideoSink,
                     webkit_video_sink,
                     GstBaseSink,
                     GST_TYPE_BASE_SINK,
                     _do_init);

static void
webkit_video_sink_base_init(gpointer g_class)
{
    GstElementClass* element_class = GST_ELEMENT_CLASS(g_class);

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sinktemplate));
    gst_element_class_set_details(element_class, &webkit_video_sink_details);
}

static void
webkit_video_sink_init(WebKitVideoSink* sink, WebKitVideoSinkClass* klass)
{
    WebKitVideoSinkPrivate* priv;

    sink->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE(sink, WEBKIT_TYPE_VIDEO_SINK, WebKitVideoSinkPrivate);
    priv->async_queue = g_async_queue_new();
}

static gboolean
webkit_video_sink_idle_func(gpointer data)
{
    WebKitVideoSink* sink = WEBKIT_VIDEO_SINK(data);
    WebKitVideoSinkPrivate* priv = sink->priv;
    GstBuffer* buffer;

    if (!priv->async_queue)
        return FALSE;

    buffer = (GstBuffer*)g_async_queue_try_pop(priv->async_queue);
    if (buffer == NULL || G_UNLIKELY(!GST_IS_BUFFER(buffer)))
        return FALSE;

    // TODO: consider priv->rgb_ordering?
    cairo_surface_t* src = cairo_image_surface_create_for_data(GST_BUFFER_DATA(buffer), CAIRO_FORMAT_RGB24, priv->width, priv->height, (4 * priv->width + 3) & ~ 3);

    // TODO: We copy the data twice right now. This could be easily improved.
    cairo_t* cr = cairo_create(priv->surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, src, 0, 0);
    cairo_surface_destroy(src);
    cairo_rectangle(cr, 0, 0, priv->width, priv->height);
    cairo_fill(cr);
    cairo_destroy(cr);

    gst_buffer_unref(buffer);

    g_signal_emit(sink, webkit_video_sink_signals[REPAINT_REQUESTED], 0);

    return FALSE;
}

static GstFlowReturn
webkit_video_sink_render(GstBaseSink* bsink, GstBuffer* buffer)
{
    WebKitVideoSink* sink = WEBKIT_VIDEO_SINK(bsink);
    WebKitVideoSinkPrivate* priv = sink->priv;

    g_async_queue_push(priv->async_queue, gst_buffer_ref(buffer));
    g_idle_add_full(G_PRIORITY_HIGH_IDLE, webkit_video_sink_idle_func, sink, NULL);

    return GST_FLOW_OK;
}

static gboolean
webkit_video_sink_set_caps(GstBaseSink* bsink, GstCaps* caps)
{
    WebKitVideoSink* sink = WEBKIT_VIDEO_SINK(bsink);
    WebKitVideoSinkPrivate* priv = sink->priv;
    GstStructure* structure;
    gboolean ret;
    const GValue* fps;
    const GValue* par;
    gint width, height;
    int red_mask;

    GstCaps* intersection = gst_caps_intersect(gst_static_pad_template_get_caps(&sinktemplate), caps);

    if (gst_caps_is_empty(intersection))
        return FALSE;

    gst_caps_unref(intersection);

    structure = gst_caps_get_structure(caps, 0);

    ret = gst_structure_get_int(structure, "width", &width);
    ret &= gst_structure_get_int(structure, "height", &height);
    fps = gst_structure_get_value(structure, "framerate");
    ret &= (fps != NULL);

    par = gst_structure_get_value(structure, "pixel-aspect-ratio");

    if (!ret)
        return FALSE;

    priv->width = width;
    priv->height = height;

    /* We dont yet use fps or pixel aspect into but handy to have */
    priv->fps_n = gst_value_get_fraction_numerator(fps);
    priv->fps_d = gst_value_get_fraction_denominator(fps);

    if (par) {
        priv->par_n = gst_value_get_fraction_numerator(par);
        priv->par_d = gst_value_get_fraction_denominator(par);
    } else
        priv->par_n = priv->par_d = 1;

    gst_structure_get_int(structure, "red_mask", &red_mask);
    priv->rgb_ordering = (red_mask == static_cast<int>(0xff000000));

    return TRUE;
}

static void
webkit_video_sink_dispose(GObject* object)
{
    WebKitVideoSink* sink = WEBKIT_VIDEO_SINK(object);
    WebKitVideoSinkPrivate* priv = sink->priv;

    if (priv->surface) {
        cairo_surface_destroy(priv->surface);
        priv->surface = NULL;
    }

    if (priv->async_queue) {
        g_async_queue_unref(priv->async_queue);
        priv->async_queue = NULL;
    }

    G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
webkit_video_sink_finalize(GObject* object)
{
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
webkit_video_sink_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    WebKitVideoSink* sink = WEBKIT_VIDEO_SINK(object);
    WebKitVideoSinkPrivate* priv = sink->priv;

    switch (prop_id) {
    case PROP_SURFACE:
        if (priv->surface)
            cairo_surface_destroy(priv->surface);
        priv->surface = cairo_surface_reference((cairo_surface_t*)g_value_get_pointer(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
webkit_video_sink_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    WebKitVideoSink* sink = WEBKIT_VIDEO_SINK(object);

    switch (prop_id) {
    case PROP_SURFACE:
        g_value_set_pointer(value, sink->priv->surface);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean
webkit_video_sink_stop(GstBaseSink* base_sink)
{
    WebKitVideoSinkPrivate* priv = WEBKIT_VIDEO_SINK(base_sink)->priv;

    g_async_queue_lock(priv->async_queue);

    /* Remove all remaining objects from the queue */
    while(GstBuffer* buffer = (GstBuffer*)g_async_queue_try_pop_unlocked(priv->async_queue))
        gst_buffer_unref(buffer);

    g_async_queue_unlock(priv->async_queue);

    return TRUE;
}

static void
webkit_video_sink_class_init(WebKitVideoSinkClass* klass)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSinkClass* gstbase_sink_class = GST_BASE_SINK_CLASS(klass);

    g_type_class_add_private(klass, sizeof(WebKitVideoSinkPrivate));

    gobject_class->set_property = webkit_video_sink_set_property;
    gobject_class->get_property = webkit_video_sink_get_property;

    gobject_class->dispose = webkit_video_sink_dispose;
    gobject_class->finalize = webkit_video_sink_finalize;

    gstbase_sink_class->render = webkit_video_sink_render;
    gstbase_sink_class->preroll = webkit_video_sink_render;
    gstbase_sink_class->stop = webkit_video_sink_stop;
    gstbase_sink_class->set_caps = webkit_video_sink_set_caps;

    webkit_video_sink_signals[REPAINT_REQUESTED] = g_signal_new("repaint-requested",
            G_TYPE_FROM_CLASS(klass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    g_object_class_install_property(
        gobject_class, PROP_SURFACE,
        g_param_spec_pointer("surface", "surface", "Target cairo_surface_t*",
                             (GParamFlags)(G_PARAM_READWRITE)));
}

/**
 * webkit_video_sink_new:
 * @surface: a #cairo_surface_t
 *
 * Creates a new GStreamer video sink which uses @surface as the target
 * for sinking a video stream from GStreamer.
 *
 * Return value: a #GstElement for the newly created video sink
 */
GstElement*
webkit_video_sink_new(cairo_surface_t* surface)
{
    return (GstElement*)g_object_new(WEBKIT_TYPE_VIDEO_SINK, "surface", surface, NULL);
}

void
webkit_video_sink_set_surface(WebKitVideoSink* sink, cairo_surface_t* surface)
{
    WebKitVideoSinkPrivate* priv;

    sink->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE(sink, WEBKIT_TYPE_VIDEO_SINK, WebKitVideoSinkPrivate);
    if (priv->surface)
        cairo_surface_destroy(priv->surface);
    priv->surface = cairo_surface_reference(surface);
}
