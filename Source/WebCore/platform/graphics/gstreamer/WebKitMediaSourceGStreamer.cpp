/*
 *  Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *  Copyright (C) 2013 Collabora Ltd.
 *  Copyright (C) 2013 Orange
 *  Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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

#include "GStreamerUtilities.h"
#include "NotImplemented.h"
#include "TimeRanges.h"

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

typedef struct _Source Source;
struct _Source {
    GstElement* src;
    // Just for identification
    WebCore::SourceBufferPrivate* sourceBuffer;
};

struct _WebKitMediaSrcPrivate {
    GList* sources;
    gchar* location;
    GstClockTime duration;
    bool haveAppsrc;
    bool asyncStart;
    bool noMorePads;
};

enum {
    Prop0,
    PropLocation
};

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src_%u", GST_PAD_SRC,
    GST_PAD_SOMETIMES, GST_STATIC_CAPS_ANY);

#define WEBKIT_MEDIA_SRC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_MEDIA_SRC, WebKitMediaSrcPrivate))

GST_DEBUG_CATEGORY_STATIC(webkit_media_src_debug);
#define GST_CAT_DEFAULT webkit_media_src_debug

static void webKitMediaSrcUriHandlerInit(gpointer gIface, gpointer ifaceData);
static void webKitMediaSrcFinalize(GObject*);
static void webKitMediaSrcSetProperty(GObject*, guint propertyId, const GValue*, GParamSpec*);
static void webKitMediaSrcGetProperty(GObject*, guint propertyId, GValue*, GParamSpec*);
static GstStateChangeReturn webKitMediaSrcChangeState(GstElement*, GstStateChange);
static gboolean webKitMediaSrcQueryWithParent(GstPad*, GstObject*, GstQuery*);

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

    gst_element_class_set_static_metadata(eklass, "WebKit Media source element", "Source", "Handles Blob uris", "Stephane Jadaud <sjadaud@sii.fr>, Sebastian Dröge <sebastian@centricular.com>");

    /* Allows setting the uri using the 'location' property, which is used
     * for example by gst_element_make_from_uri() */
    g_object_class_install_property(oklass,
        PropLocation,
        g_param_spec_string("location", "location", "Location to read from", 0,
        (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    eklass->change_state = webKitMediaSrcChangeState;

    g_type_class_add_private(klass, sizeof(WebKitMediaSrcPrivate));
}

static void webkit_media_src_init(WebKitMediaSrc* src)
{
    src->priv = WEBKIT_MEDIA_SRC_GET_PRIVATE(src);
}

static void webKitMediaSrcFinalize(GObject* object)
{
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(object);
    WebKitMediaSrcPrivate* priv = src->priv;

    // TODO: Free sources
    g_free(priv->location);

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
        g_value_set_string(value, priv->location);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    GST_OBJECT_UNLOCK(src);
}

static void webKitMediaSrcDoAsyncStart(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;
    priv->asyncStart = true;
    GST_BIN_CLASS(parent_class)->handle_message(GST_BIN(src),
        gst_message_new_async_start(GST_OBJECT(src)));
}

static void webKitMediaSrcDoAsyncDone(WebKitMediaSrc* src)
{
    WebKitMediaSrcPrivate* priv = src->priv;
    if (priv->asyncStart) {
        GST_BIN_CLASS(parent_class)->handle_message(GST_BIN(src),
            gst_message_new_async_done(GST_OBJECT(src), GST_CLOCK_TIME_NONE));
        priv->asyncStart = false;
    }
}

static GstStateChangeReturn webKitMediaSrcChangeState(GstElement* element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    WebKitMediaSrc* src = WEBKIT_MEDIA_SRC(element);
    WebKitMediaSrcPrivate* priv = src->priv;

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        priv->noMorePads = false;
        webKitMediaSrcDoAsyncStart(src);
        break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(ret == GST_STATE_CHANGE_FAILURE)) {
        GST_DEBUG_OBJECT(src, "State change failed");
        webKitMediaSrcDoAsyncDone(src);
        return ret;
    }

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        ret = GST_STATE_CHANGE_ASYNC;
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        webKitMediaSrcDoAsyncDone(src);
        priv->noMorePads = false;
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
        gst_query_set_uri(query, src->priv->location);
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
    ret = g_strdup(src->priv->location);
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
    g_free(priv->location);
    priv->location = 0;
    if (!uri) {
        GST_OBJECT_UNLOCK(src);
        return TRUE;
    }

    WebCore::URL url(WebCore::URL(), uri);

    priv->location = g_strdup(url.string().utf8().data());
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

namespace WebCore {
MediaSourceClientGStreamer::MediaSourceClientGStreamer(WebKitMediaSrc* src)
    : m_src(adoptGRef(static_cast<WebKitMediaSrc*>(gst_object_ref(src))))
{
}

MediaSourceClientGStreamer::~MediaSourceClientGStreamer()
{
}

MediaSourcePrivate::AddStatus MediaSourceClientGStreamer::addSourceBuffer(PassRefPtr<SourceBufferPrivate> sourceBufferPrivate, const ContentType&)
{
    WebKitMediaSrcPrivate* priv = m_src->priv;

    if (priv->noMorePads) {
        GST_ERROR_OBJECT(m_src.get(), "Adding new source buffers after first data not supported yet");
        return MediaSourcePrivate::NotSupported;
    }

    GST_DEBUG_OBJECT(m_src.get(), "State %d", static_cast<int>(GST_STATE(m_src.get())));

    GST_OBJECT_LOCK(m_src.get());

    Source* source = g_new0(Source, 1);
    guint numberOfSources = g_list_length(priv->sources);
    GUniquePtr<gchar> srcName(g_strdup_printf("src%u", numberOfSources));

    source->src = gst_element_factory_make("appsrc", srcName.get());
    source->sourceBuffer = sourceBufferPrivate.get();

    GUniquePtr<gchar> padName(g_strdup_printf("src_%u", numberOfSources));
    priv->sources = g_list_prepend(priv->sources, source);
    GST_OBJECT_UNLOCK(m_src.get());

    priv->haveAppsrc = source->src;

    gst_bin_add(GST_BIN(m_src.get()), source->src);
    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(source->src, "src"));
    GstPad* ghostPad = gst_ghost_pad_new_from_template(padName.get(), pad.get(), gst_static_pad_template_get(&srcTemplate));
    gst_pad_set_query_function(ghostPad, webKitMediaSrcQueryWithParent);
    gst_pad_set_active(ghostPad, TRUE);
    gst_element_add_pad(GST_ELEMENT(m_src.get()), ghostPad);

    gst_element_sync_state_with_parent(source->src);

    return MediaSourcePrivate::Ok;
}

void MediaSourceClientGStreamer::durationChanged(const MediaTime& duration)
{
    WebKitMediaSrcPrivate* priv = m_src->priv;
    GstClockTime gstDuration = gst_util_uint64_scale(duration.timeValue(), GST_SECOND, duration.timeScale());

    GST_DEBUG_OBJECT(m_src.get(), "Received duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(gstDuration));

    GST_OBJECT_LOCK(m_src.get());
    priv->duration = gstDuration;
    GST_OBJECT_UNLOCK(m_src.get());
    gst_element_post_message(GST_ELEMENT(m_src.get()), gst_message_new_duration_changed(GST_OBJECT(m_src.get())));
}

SourceBufferPrivateClient::AppendResult MediaSourceClientGStreamer::append(PassRefPtr<SourceBufferPrivate> sourceBufferPrivate, const unsigned char* data, unsigned length)
{
    WebKitMediaSrcPrivate* priv = m_src->priv;
    GstFlowReturn ret = GST_FLOW_OK;
    GstBuffer* buffer;
    Source* source = 0;

    if (!priv->noMorePads) {
        priv->noMorePads = true;
        gst_element_no_more_pads(GST_ELEMENT(m_src.get()));
        webKitMediaSrcDoAsyncDone(m_src.get());
    }

    for (GList* iter = priv->sources; iter; iter = iter->next) {
        Source* tmp = static_cast<Source*>(iter->data);
        if (tmp->sourceBuffer == sourceBufferPrivate.get()) {
            source = tmp;
            break;
        }
    }

    if (!source || !source->src)
        return SourceBufferPrivateClient::ReadStreamFailed;

    buffer = gst_buffer_new_and_alloc(length);
    gst_buffer_fill(buffer, 0, data, length);

    ret = gst_app_src_push_buffer(GST_APP_SRC(source->src), buffer);
    GST_DEBUG_OBJECT(m_src.get(), "push buffer %d\n", static_cast<int>(ret));

    if (ret == GST_FLOW_OK)
        return SourceBufferPrivateClient::AppendSucceeded;

    return SourceBufferPrivateClient::ReadStreamFailed;
}

void MediaSourceClientGStreamer::markEndOfStream(MediaSourcePrivate::EndOfStreamStatus)
{
    WebKitMediaSrcPrivate* priv = m_src->priv;

    GST_DEBUG_OBJECT(m_src.get(), "Have EOS");

    if (!priv->noMorePads) {
        priv->noMorePads = true;
        gst_element_no_more_pads(GST_ELEMENT(m_src.get()));
        webKitMediaSrcDoAsyncDone(m_src.get());
    }

    for (GList* iter = priv->sources; iter; iter = iter->next) {
        Source* source = static_cast<Source*>(iter->data);
        if (source->src)
            gst_app_src_end_of_stream(GST_APP_SRC(source->src));
    }
}

void MediaSourceClientGStreamer::removedFromMediaSource(PassRefPtr<SourceBufferPrivate> sourceBufferPrivate)
{
    WebKitMediaSrcPrivate* priv = m_src->priv;
    Source* source = 0;

    for (GList* iter = priv->sources; iter; iter = iter->next) {
        Source* tmp = static_cast<Source*>(iter->data);
        if (tmp->sourceBuffer == sourceBufferPrivate.get()) {
            source = tmp;
            break;
        }
    }

    ASSERT(source && source->src);

    gst_app_src_end_of_stream(GST_APP_SRC(source->src));
}

};

namespace WTF {
template <> GRefPtr<WebKitMediaSrc> adoptGRef(WebKitMediaSrc* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(G_OBJECT(ptr)));
    return GRefPtr<WebKitMediaSrc>(ptr, GRefPtrAdopt);
}

template <> WebKitMediaSrc* refGPtr<WebKitMediaSrc>(WebKitMediaSrc* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<WebKitMediaSrc>(WebKitMediaSrc* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}
};

#endif // USE(GSTREAMER)

