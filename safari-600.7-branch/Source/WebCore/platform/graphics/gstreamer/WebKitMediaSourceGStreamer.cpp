/*
 *  Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *  Copyright (C) 2013 Collabora Ltd.
 *  Copyright (C) 2013 Orange
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
#include "WebKitMediaSourceGStreamer.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include "GStreamerUtilities.h"
#include "NotImplemented.h"
#include "TimeRanges.h"
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>
#include <wtf/gobject/GMainLoopSource.h>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>

typedef struct _Source {
    GstElement* appsrc;
    guint sourceid;        /* To control the GSource */
    GstPad* srcpad;
    gboolean padAdded;

    guint64 offset;
    guint64 size;
    gboolean paused;

    GMainLoopSource start;
    GMainLoopSource stop;
    GMainLoopSource needData;
    GMainLoopSource enoughData;
    GMainLoopSource seek;

    guint64 requestedOffset;
} Source;


#define WEBKIT_MEDIA_SRC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_MEDIA_SRC, WebKitMediaSrcPrivate))

struct _WebKitMediaSrcPrivate {
    gchar* uri;
    Source sourceVideo;
    Source sourceAudio;
    WebCore::MediaPlayer* player;
    GstElement* playbin;
    gint64 duration;
    gboolean seekable;
    gboolean noMorePad;
    // TRUE if appsrc's version is >= 0.10.27, see
    // https://bugzilla.gnome.org/show_bug.cgi?id=609423
    gboolean haveAppSrc27;
    guint nbSource;
};

enum {
    PropLocation = 1,
    ProLast
};

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src_%u", GST_PAD_SRC, GST_PAD_SOMETIMES, GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(webkit_media_src_debug);
#define GST_CAT_DEFAULT webkit_media_src_debug

static void webKitMediaSrcUriHandlerInit(gpointer gIface, gpointer ifaceData);
static void webKitMediaSrcFinalize(GObject*);
static void webKitMediaSrcSetProperty(GObject*, guint propertyId, const GValue*, GParamSpec*);
static void webKitMediaSrcGetProperty(GObject*, guint propertyId, GValue*, GParamSpec*);
static GstStateChangeReturn webKitMediaSrcChangeState(GstElement*, GstStateChange);
static gboolean webKitMediaSrcQueryWithParent(GstPad*, GstObject*, GstQuery*);

static void webKitMediaVideoSrcNeedDataCb(GstAppSrc*, guint, gpointer);
static void webKitMediaVideoSrcEnoughDataCb(GstAppSrc*, gpointer);
static gboolean webKitMediaVideoSrcSeekDataCb(GstAppSrc*, guint64, gpointer);
static void webKitMediaAudioSrcNeedDataCb(GstAppSrc*, guint, gpointer);
static void webKitMediaAudioSrcEnoughDataCb(GstAppSrc*, gpointer);
static gboolean webKitMediaAudioSrcSeekDataCb(GstAppSrc*, guint64, gpointer);
static GstAppSrcCallbacks appsrcCallbacksVideo = {
    webKitMediaVideoSrcNeedDataCb,
    webKitMediaVideoSrcEnoughDataCb,
    webKitMediaVideoSrcSeekDataCb,
    { 0 }
};
static GstAppSrcCallbacks appsrcCallbacksAudio = {
    webKitMediaAudioSrcNeedDataCb,
    webKitMediaAudioSrcEnoughDataCb,
    webKitMediaAudioSrcSeekDataCb,
    { 0 }
};
#define webkit_media_src_parent_class parent_class
// We split this out into another macro to avoid a check-webkit-style error.
#define WEBKIT_MEDIA_SRC_CATEGORY_INIT GST_DEBUG_CATEGORY_INIT(webkit_media_src_debug, "webkitmediasrc", 0, "websrc element");
G_DEFINE_TYPE_WITH_CODE(WebKitMediaSrc, webkit_media_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, webKitMediaSrcUriHandlerInit);
    WEBKIT_MEDIA_SRC_CATEGORY_INIT);

static void webkit_media_src_class_init(WebKitMediaSrcClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);

    oklass->finalize = webKitMediaSrcFinalize;
    oklass->set_property = webKitMediaSrcSetProperty;
    oklass->get_property = webKitMediaSrcGetProperty;

    gst_element_class_add_pad_template(eklass, gst_static_pad_template_get(&srcTemplate));

    gst_element_class_set_metadata(eklass, "WebKit Media source element", "Source", "Handles Blob uris", "Stephane Jadaud <sjadaud@sii.fr>");

    /* Allows setting the uri using the 'location' property, which is used
     * for example by gst_element_make_from_uri() */
    g_object_class_install_property(oklass,
        PropLocation,
        g_param_spec_string("location", "location", "Location to read from", 0,
        (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    eklass->change_state = webKitMediaSrcChangeState;

    g_type_class_add_private(klass, sizeof(WebKitMediaSrcPrivate));
}

static void webKitMediaSrcAddSrc(WebKitMediaSrc* src, GstElement* element)
{
    GstPad* ghostPad;
    WebKitMediaSrcPrivate* priv = src->priv;

    if (!gst_bin_add(GST_BIN(src), element)) {
        GST_DEBUG_OBJECT(src, "Src element not added");
        return;
    }
    GRefPtr<GstPad> targetsrc = adoptGRef(gst_element_get_static_pad(element, "src"));
    if (!targetsrc) {
        GST_DEBUG_OBJECT(src, "Pad not found");
        return;
    }

    gst_element_sync_state_with_parent(element);
    GUniquePtr<gchar> name(g_strdup_printf("src_%u", priv->nbSource));
    ghostPad = WebCore::webkitGstGhostPadFromStaticTemplate(&srcTemplate, name.get(), targetsrc.get());
    gst_pad_set_active(ghostPad, TRUE);

    priv->nbSource++;

    if (priv->sourceVideo.appsrc == element)
        priv->sourceVideo.srcpad = ghostPad;
    else if (priv->sourceAudio.appsrc == element)
        priv->sourceAudio.srcpad = ghostPad;

    GST_OBJECT_FLAG_SET(ghostPad, GST_PAD_FLAG_NEED_PARENT);
    gst_pad_set_query_function(ghostPad, webKitMediaSrcQueryWithParent);
}

static void webkit_media_src_init(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = WEBKIT_MEDIA_SRC_GET_PRIVATE(src);
    src->priv = priv;
    new (priv) WebKitMediaSrcPrivate();

    priv->sourceVideo.appsrc = gst_element_factory_make("appsrc", "videoappsrc");
    gst_app_src_set_callbacks(GST_APP_SRC(priv->sourceVideo.appsrc), &appsrcCallbacksVideo, src, 0);
    webKitMediaSrcAddSrc(src, priv->sourceVideo.appsrc);

    priv->sourceAudio.appsrc = gst_element_factory_make("appsrc", "audioappsrc");
    gst_app_src_set_callbacks(GST_APP_SRC(priv->sourceAudio.appsrc), &appsrcCallbacksAudio, src, 0);
    webKitMediaSrcAddSrc(src, priv->sourceAudio.appsrc);
}

static void webKitMediaSrcFinalize(GObject* object)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(object);
    WebKitMediaSrcPrivate* priv = src->priv;

    g_free(priv->uri);
    priv->~WebKitMediaSrcPrivate();

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void webKitMediaSrcSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* pspec)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(object);
    switch (propId) {
    case PropLocation:
        gst_uri_handler_set_uri(reinterpret_cast<GstURIHandler*>(src), g_value_get_string(value), 0);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void webKitMediaSrcGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* pspec)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(object);
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_OBJECT_LOCK(src);
    switch (propId) {
    case PropLocation:
        g_value_set_string(value, priv->uri);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    GST_OBJECT_UNLOCK(src);
}

// must be called on main thread and with object unlocked
static void webKitMediaVideoSrcStop(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;
    gboolean seeking;

    GST_OBJECT_LOCK(src);

    seeking = priv->sourceVideo.seek.isActive();

    priv->sourceVideo.start.cancel();

    priv->player = 0;
    priv->playbin = 0;

    priv->sourceVideo.needData.cancel();
    priv->sourceVideo.enoughData.cancel();
    priv->sourceVideo.seek.cancel();

    priv->sourceVideo.paused = FALSE;
    priv->sourceVideo.offset = 0;
    priv->seekable = FALSE;

    priv->duration = 0;
    priv->nbSource = 0;

    GST_OBJECT_UNLOCK(src);

    if (priv->sourceVideo.appsrc) {
        gst_app_src_set_caps(GST_APP_SRC(priv->sourceVideo.appsrc), 0);
        if (!seeking)
            gst_app_src_set_size(GST_APP_SRC(priv->sourceVideo.appsrc), -1);
    }

    GST_DEBUG_OBJECT(src, "Stopped request");
}

static void webKitMediaAudioSrcStop(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;
    gboolean seeking;

    GST_OBJECT_LOCK(src);

    seeking = priv->sourceAudio.seek.isActive();

    priv->sourceAudio.start.cancel();

    priv->player = 0;

    priv->playbin = 0;

    priv->sourceAudio.needData.cancel();
    priv->sourceAudio.enoughData.cancel();
    priv->sourceAudio.seek.cancel();

    priv->sourceAudio.paused = FALSE;

    priv->sourceAudio.offset = 0;

    priv->seekable = FALSE;

    priv->duration = 0;
    priv->nbSource = 0;

    GST_OBJECT_UNLOCK(src);

    if (priv->sourceAudio.appsrc) {
        gst_app_src_set_caps(GST_APP_SRC(priv->sourceAudio.appsrc), 0);
        if (!seeking)
            gst_app_src_set_size(GST_APP_SRC(priv->sourceAudio.appsrc), -1);
    }

    GST_DEBUG_OBJECT(src, "Stopped request");
}

// must be called on main thread and with object unlocked
static void webKitMediaVideoSrcStart(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_OBJECT_LOCK(src);
    if (!priv->uri) {
        GST_ERROR_OBJECT(src, "No URI provided");
        GST_OBJECT_UNLOCK(src);
        webKitMediaVideoSrcStop(src);
        return;
    }

    GST_OBJECT_UNLOCK(src);
    GST_DEBUG_OBJECT(src, "Started request");
}

// must be called on main thread and with object unlocked
static void webKitMediaAudioSrcStart(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_OBJECT_LOCK(src);
    if (!priv->uri) {
        GST_ERROR_OBJECT(src, "No URI provided");
        GST_OBJECT_UNLOCK(src);
        webKitMediaAudioSrcStop(src);
        return;
    }

    GST_OBJECT_UNLOCK(src);
    GST_DEBUG_OBJECT(src, "Started request");
}

static GstStateChangeReturn webKitMediaSrcChangeState(GstElement* element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(element);
    WebKitMediaSrcPrivate* priv = src->priv;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!priv->sourceVideo.appsrc && !priv->sourceAudio.appsrc) {
            gst_element_post_message(element,
                gst_missing_element_message_new(element, "appsrc"));
            GST_ELEMENT_ERROR(src, CORE, MISSING_PLUGIN, (0), ("no appsrc"));
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(ret == GST_STATE_CHANGE_FAILURE)) {
        GST_DEBUG_OBJECT(src, "State change failed");
        return ret;
    }

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        GST_DEBUG_OBJECT(src, "READY->PAUSED");
        GST_OBJECT_LOCK(src);

        gst_object_ref(src);
        priv->sourceVideo.start.schedule("[WebKit] webKitMediaVideoSrcStart", std::function<void()>(std::bind(webKitMediaVideoSrcStart, src)), G_PRIORITY_DEFAULT,
            [src] { gst_object_unref(src); });

        gst_object_ref(src);
        priv->sourceAudio.start.schedule("[WebKit] webKitMediaAudioSrcStart", std::function<void()>(std::bind(webKitMediaAudioSrcStart, src)), G_PRIORITY_DEFAULT,
            [src] { gst_object_unref(src); });

        GST_OBJECT_UNLOCK(src);
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        GST_DEBUG_OBJECT(src, "PAUSED->READY");
        GST_OBJECT_LOCK(src);

        gst_object_ref(src);
        priv->sourceVideo.stop.schedule("[WebKit] webKitMediaVideoSrcStop", std::function<void()>(std::bind(webKitMediaVideoSrcStop, src)), G_PRIORITY_DEFAULT,
            [src] { gst_object_unref(src); });

        gst_object_ref(src);
        priv->sourceAudio.stop.schedule("[WebKit] webKitMediaAudioSrcStop", std::function<void()>(std::bind(webKitMediaAudioSrcStop, src)), G_PRIORITY_DEFAULT,
            [src] { gst_object_unref(src); });

        GST_OBJECT_UNLOCK(src);
        break;
    default:
        break;
    }

    return ret;
}

static gboolean webKitMediaSrcQueryWithParent(GstPad* pad, GstObject* parent, GstQuery* query)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(GST_ELEMENT(parent));
    gboolean result = FALSE;

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_DURATION: {
        GstFormat format;
        gst_query_parse_duration(query, &format, NULL);

        GST_DEBUG_OBJECT(src, "duration query in format %s", gst_format_get_name(format));
        GST_OBJECT_LOCK(src);
        if ((format == GST_FORMAT_TIME) && (src->priv->duration > 0)) {
            gst_query_set_duration(query, format, src->priv->duration);
            result = TRUE;
        }
        GST_OBJECT_UNLOCK(src);
        break;
    }
    case GST_QUERY_URI: {
        GST_OBJECT_LOCK(src);
        gst_query_set_uri(query, src->priv->uri);
        GST_OBJECT_UNLOCK(src);
        result = TRUE;
        break;
    }
    default: {
        GRefPtr<GstPad> target = adoptGRef(gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(pad)));
        // Forward the query to the proxy target pad.
        if (target)
            result = gst_pad_query(target.get(), query);
        break;
    }
    }

    return result;
}

// uri handler interface
static GstURIType webKitMediaSrcUriGetType(GType)
{
    return GST_URI_SRC;
}

const gchar* const* webKitMediaSrcGetProtocols(GType)
{
    static const char* protocols[] = {"mediasourceblob", 0 };
    return protocols;
}

static gchar* webKitMediaSrcGetUri(GstURIHandler* handler)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(handler);
    gchar* ret;

    GST_OBJECT_LOCK(src);
    ret = g_strdup(src->priv->uri);
    GST_OBJECT_UNLOCK(src);
    return ret;
}

static gboolean webKitMediaSrcSetUri(GstURIHandler* handler, const gchar* uri, GError**)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(handler);
    WebKitMediaSrcPrivate* priv = src->priv;
    if (GST_STATE(src) >= GST_STATE_PAUSED) {
        GST_ERROR_OBJECT(src, "URI can only be set in states < PAUSED");
        return FALSE;
    }

    GST_OBJECT_LOCK(src);
    g_free(priv->uri);
    priv->uri = 0;
    if (!uri) {
        GST_OBJECT_UNLOCK(src);
        return TRUE;
    }

    WebCore::URL url(WebCore::URL(), uri);

    priv->uri = g_strdup(url.string().utf8().data());
    GST_OBJECT_UNLOCK(src);
    return TRUE;
}

static void webKitMediaSrcUriHandlerInit(gpointer gIface, gpointer)
{
    GstURIHandlerInterface* iface = (GstURIHandlerInterface *) gIface;

    iface->get_type = webKitMediaSrcUriGetType;
    iface->get_protocols = webKitMediaSrcGetProtocols;
    iface->get_uri = webKitMediaSrcGetUri;
    iface->set_uri = webKitMediaSrcSetUri;
}

// appsrc callbacks
static void webKitMediaVideoSrcNeedDataMainCb(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_OBJECT_LOCK(src);
    priv->sourceVideo.paused = FALSE;
    GST_OBJECT_UNLOCK(src);
}

static void webKitMediaAudioSrcNeedDataMainCb(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_OBJECT_LOCK(src);
    priv->sourceAudio.paused = FALSE;
    GST_OBJECT_UNLOCK(src);
}

static void webKitMediaVideoSrcNeedDataCb(GstAppSrc*, guint length, gpointer userData)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(userData);
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Need more data: %u", length);

    GST_OBJECT_LOCK(src);
    if (priv->sourceVideo.needData.isScheduled() || !priv->sourceVideo.paused) {
        GST_OBJECT_UNLOCK(src);
        return;
    }

    gst_object_ref(src);
    priv->sourceVideo.needData.schedule("[WebKit] webKitMediaVideoSrcNeedDataMainCb", std::function<void()>(std::bind(webKitMediaVideoSrcNeedDataMainCb, src)), G_PRIORITY_DEFAULT,
        [src] { gst_object_unref(src); });
    GST_OBJECT_UNLOCK(src);
}

static void webKitMediaAudioSrcNeedDataCb(GstAppSrc*, guint length, gpointer userData)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(userData);
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Need more data: %u", length);

    GST_OBJECT_LOCK(src);
    if (priv->sourceAudio.needData.isScheduled() || !priv->sourceAudio.paused) {
        GST_OBJECT_UNLOCK(src);
        return;
    }

    gst_object_ref(src);
    priv->sourceAudio.needData.schedule("[WebKit] webKitMediaAudioSrcNeedDataMainCb", std::function<void()>(std::bind(webKitMediaAudioSrcNeedDataMainCb, src)), G_PRIORITY_DEFAULT,
        [src] { gst_object_unref(src); });
    GST_OBJECT_UNLOCK(src);
}

static void webKitMediaVideoSrcEnoughDataMainCb(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_OBJECT_LOCK(src);
    priv->sourceVideo.paused = TRUE;
    GST_OBJECT_UNLOCK(src);
}

static void webKitMediaAudioSrcEnoughDataMainCb(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_OBJECT_LOCK(src);
    priv->sourceAudio.paused = TRUE;
    GST_OBJECT_UNLOCK(src);
}

static void webKitMediaVideoSrcEnoughDataCb(GstAppSrc*, gpointer userData)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(userData);
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Have enough data");

    GST_OBJECT_LOCK(src);
    if (priv->sourceVideo.enoughData.isScheduled() || priv->sourceVideo.paused) {
        GST_OBJECT_UNLOCK(src);
        return;
    }

    gst_object_ref(src);
    priv->sourceVideo.enoughData.schedule("[WebKit] webKitMediaVideoSrcEnoughDataMainCb", std::function<void()>(std::bind(webKitMediaVideoSrcEnoughDataMainCb, src)), G_PRIORITY_DEFAULT,
        [src] { gst_object_unref(src); });

    GST_OBJECT_UNLOCK(src);
}

static void webKitMediaAudioSrcEnoughDataCb(GstAppSrc*, gpointer userData)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(userData);
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Have enough data");

    GST_OBJECT_LOCK(src);
    if (priv->sourceAudio.enoughData.isScheduled() || priv->sourceAudio.paused) {
        GST_OBJECT_UNLOCK(src);
        return;
    }

    gst_object_ref(src);
    priv->sourceAudio.enoughData.schedule("[WebKit] webKitMediaAudioSrcEnoughDataMainCb", std::function<void()>(std::bind(webKitMediaAudioSrcEnoughDataMainCb, src)), G_PRIORITY_DEFAULT,
        [src] { gst_object_unref(src); });

    GST_OBJECT_UNLOCK(src);
}

static void webKitMediaVideoSrcSeekMainCb(WebKitMediaSrc*)
{
    notImplemented();
}


static void webKitMediaAudioSrcSeekMainCb(WebKitMediaSrc*)
{
    notImplemented();
}

static gboolean webKitMediaVideoSrcSeekDataCb(GstAppSrc*, guint64 offset, gpointer userData)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(userData);
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Seeking to offset: %" G_GUINT64_FORMAT, offset);
    GST_OBJECT_LOCK(src);
    if (offset == priv->sourceVideo.offset && priv->sourceVideo.requestedOffset == priv->sourceVideo.offset) {
        GST_OBJECT_UNLOCK(src);
        return TRUE;
    }

    if (!priv->seekable) {
        GST_OBJECT_UNLOCK(src);
        return FALSE;
    }
    if (offset > priv->sourceVideo.size) {
        GST_OBJECT_UNLOCK(src);
        return FALSE;
    }

    GST_DEBUG_OBJECT(src, "Doing range-request seek");
    priv->sourceVideo.requestedOffset = offset;

    gst_object_ref(src);
    priv->sourceVideo.seek.schedule("[WebKit] webKitMediaVideoSrcSeekMainCb", std::function<void()>(std::bind(webKitMediaVideoSrcSeekMainCb, src)), G_PRIORITY_DEFAULT,
        [src] { gst_object_unref(src); });

    GST_OBJECT_UNLOCK(src);

    return TRUE;
}

static gboolean webKitMediaAudioSrcSeekDataCb(GstAppSrc*, guint64 offset, gpointer userData)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(userData);
    WebKitMediaSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Seeking to offset: %" G_GUINT64_FORMAT, offset);
    GST_OBJECT_LOCK(src);
    if (offset == priv->sourceAudio.offset && priv->sourceAudio.requestedOffset == priv->sourceAudio.offset) {
        GST_OBJECT_UNLOCK(src);
        return TRUE;
    }

    if (!priv->seekable) {
        GST_OBJECT_UNLOCK(src);
        return FALSE;
    }
    if (offset > priv->sourceAudio.size) {
        GST_OBJECT_UNLOCK(src);
        return FALSE;
    }

    GST_DEBUG_OBJECT(src, "Doing range-request seek");
    priv->sourceAudio.requestedOffset = offset;

    gst_object_ref(src);
    priv->sourceAudio.seek.schedule("[WebKit] webKitMediaAudioSrcSeekMainCb", std::function<void()>(std::bind(webKitMediaAudioSrcSeekMainCb, src)), G_PRIORITY_DEFAULT,
        [src] { gst_object_unref(src); });

    GST_OBJECT_UNLOCK(src);

    return TRUE;
}

void webKitMediaSrcSetMediaPlayer(WebKitMediaSrc* src, WebCore::MediaPlayer* player)
{
    WebKitMediaSrcPrivate* priv = src->priv;
    priv->player = player;
}

void webKitMediaSrcSetPlayBin(WebKitMediaSrc* src, GstElement* playBin)
{
    WebKitMediaSrcPrivate* priv = src->priv;
    priv->playbin = playBin;
}

MediaSourceClientGstreamer::MediaSourceClientGstreamer(WebKitMediaSrc* src)
    : m_src(static_cast<WebKitMediaSrc*>(gst_object_ref(src)))
{
}

MediaSourceClientGstreamer::~MediaSourceClientGstreamer()
{
    gst_object_unref(m_src);
}

void MediaSourceClientGstreamer::didReceiveDuration(double duration)
{
    WebKitMediaSrcPrivate* priv = m_src->priv;
    GST_DEBUG_OBJECT(m_src, "Received duration: %lf", duration);

    GST_OBJECT_LOCK(m_src);
    priv->duration = duration >= 0.0 ? static_cast<gint64>(duration*GST_SECOND) : 0;
    GST_OBJECT_UNLOCK(m_src);
}

void MediaSourceClientGstreamer::didReceiveData(const char* data, int length, String type)
{
    WebKitMediaSrcPrivate* priv = m_src->priv;
    GstFlowReturn ret = GST_FLOW_OK;
    GstBuffer * buffer;

    if (type.startsWith("video")) {
        if (priv->noMorePad == FALSE && priv->sourceVideo.padAdded == TRUE) {
            gst_element_no_more_pads(GST_ELEMENT(m_src));
            priv->noMorePad = TRUE;
        }
        if (priv->noMorePad == FALSE && priv->sourceVideo.padAdded == FALSE) {
            gst_element_add_pad(GST_ELEMENT(m_src), priv->sourceVideo.srcpad);
            priv->sourceVideo.padAdded = TRUE;
        }
        GST_OBJECT_LOCK(m_src);
        buffer = WebCore::createGstBufferForData(data, length);
        GST_OBJECT_UNLOCK(m_src);

        ret = gst_app_src_push_buffer(GST_APP_SRC(priv->sourceVideo.appsrc), buffer);
    } else if (type.startsWith("audio")) {
        if (priv->noMorePad == FALSE && priv->sourceAudio.padAdded == TRUE) {
            gst_element_no_more_pads(GST_ELEMENT(m_src));
            priv->noMorePad = TRUE;
        }
        if (priv->noMorePad == FALSE && priv->sourceAudio.padAdded == FALSE) {
            gst_element_add_pad(GST_ELEMENT(m_src), priv->sourceAudio.srcpad);
            priv->sourceAudio.padAdded = TRUE;
        }
        GST_OBJECT_LOCK(m_src);
        buffer = WebCore::createGstBufferForData(data, length);
        GST_OBJECT_UNLOCK(m_src);

        ret = gst_app_src_push_buffer(GST_APP_SRC(priv->sourceAudio.appsrc), buffer);
    }

    if (ret != GST_FLOW_OK && ret != GST_FLOW_EOS)
        GST_ELEMENT_ERROR(m_src, CORE, FAILED, (0), (0));
}

void MediaSourceClientGstreamer::didFinishLoading(double)
{
    WebKitMediaSrcPrivate* priv = m_src->priv;

    GST_DEBUG_OBJECT(m_src, "Have EOS");

    GST_OBJECT_LOCK(m_src);
    if (!priv->sourceVideo.seek.isActive()) {
        GST_OBJECT_UNLOCK(m_src);
        gst_app_src_end_of_stream(GST_APP_SRC(priv->sourceVideo.appsrc));
    } else
        GST_OBJECT_UNLOCK(m_src);

    GST_OBJECT_LOCK(m_src);
    if (!priv->sourceAudio.seek.isActive()) {
        GST_OBJECT_UNLOCK(m_src);
        gst_app_src_end_of_stream(GST_APP_SRC(priv->sourceAudio.appsrc));
    } else
        GST_OBJECT_UNLOCK(m_src);
}

void MediaSourceClientGstreamer::didFail()
{
    gst_app_src_end_of_stream(GST_APP_SRC(m_src->priv->sourceVideo.appsrc));
    gst_app_src_end_of_stream(GST_APP_SRC(m_src->priv->sourceAudio.appsrc));
}

#endif // USE(GSTREAMER)

