/*
 *  Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *  Copyright (C) 2013 Collabora Ltd.
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
#include "WebKitWebSourceGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "HTTPHeaderNames.h"
#include "MainThreadNotifier.h"
#include "MediaPlayer.h"
#include "PlatformMediaResourceLoader.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include <cstdint>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/missing-plugins.h>
#include <wtf/text/CString.h>

using namespace WebCore;

class CachedResourceStreamingClient final : public PlatformMediaResourceClient {
    WTF_MAKE_NONCOPYABLE(CachedResourceStreamingClient);
public:
    CachedResourceStreamingClient(WebKitWebSrc*, ResourceRequest&&);
    virtual ~CachedResourceStreamingClient();

    const HashSet<RefPtr<WebCore::SecurityOrigin>>& securityOrigins() const { return m_origins; }

private:
    void checkUpdateBlocksize(uint64_t bytesRead);

    // PlatformMediaResourceClient virtual methods.
    void responseReceived(PlatformMediaResource&, const ResourceResponse&, CompletionHandler<void(ShouldContinue)>&&) override;
    void dataReceived(PlatformMediaResource&, const char*, int) override;
    void accessControlCheckFailed(PlatformMediaResource&, const ResourceError&) override;
    void loadFailed(PlatformMediaResource&, const ResourceError&) override;
    void loadFinished(PlatformMediaResource&) override;

    static constexpr int s_growBlocksizeLimit { 1 };
    static constexpr int s_growBlocksizeCount { 1 };
    static constexpr int s_growBlocksizeFactor { 2 };
    static constexpr float s_reduceBlocksizeLimit { 0.20 };
    static constexpr int s_reduceBlocksizeCount { 2 };
    static constexpr float s_reduceBlocksizeFactor { 0.5 };
    int m_reduceBlocksizeCount { 0 };
    int m_increaseBlocksizeCount { 0 };

    GRefPtr<GstElement> m_src;
    ResourceRequest m_request;
    HashSet<RefPtr<WebCore::SecurityOrigin>> m_origins;
};

enum MainThreadSourceNotification {
    Start = 1 << 0,
    Stop = 1 << 1,
    NeedData = 1 << 2,
    EnoughData = 1 << 3,
    Seek = 1 << 4
};

#define WEBKIT_WEB_SRC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_SRC, WebKitWebSrcPrivate))
struct _WebKitWebSrcPrivate {
    GstAppSrc* appsrc;
    GstPad* srcpad;
    CString originalURI;
    CString redirectedURI;
    bool keepAlive;
    GUniquePtr<GstStructure> extraHeaders;
    bool compress;
    GUniquePtr<gchar> httpMethod;

    WebCore::MediaPlayer* player;

    RefPtr<PlatformMediaResourceLoader> loader;
    RefPtr<PlatformMediaResource> resource;

    bool didPassAccessControlCheck;

    guint64 offset;
    bool haveSize;
    guint64 size;
    gboolean seekable;
    bool paused;
    bool isSeeking;

    guint64 requestedOffset;

    uint64_t minimumBlocksize;

    RefPtr<MainThreadNotifier<MainThreadSourceNotification>> notifier;
};

enum {
    PROP_0,
    PROP_LOCATION,
    PROP_RESOLVED_LOCATION,
    PROP_KEEP_ALIVE,
    PROP_EXTRA_HEADERS,
    PROP_COMPRESS,
    PROP_METHOD
};

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src",
                                                                  GST_PAD_SRC,
                                                                  GST_PAD_ALWAYS,
                                                                  GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(webkit_web_src_debug);
#define GST_CAT_DEFAULT webkit_web_src_debug

static void webKitWebSrcUriHandlerInit(gpointer gIface, gpointer ifaceData);

static void webKitWebSrcDispose(GObject*);
static void webKitWebSrcFinalize(GObject*);
static void webKitWebSrcSetProperty(GObject*, guint propertyID, const GValue*, GParamSpec*);
static void webKitWebSrcGetProperty(GObject*, guint propertyID, GValue*, GParamSpec*);
static GstStateChangeReturn webKitWebSrcChangeState(GstElement*, GstStateChange);

static gboolean webKitWebSrcQueryWithParent(GstPad*, GstObject*, GstQuery*);

static void webKitWebSrcNeedData(WebKitWebSrc*);
static void webKitWebSrcEnoughData(WebKitWebSrc*);
static gboolean webKitWebSrcSeek(WebKitWebSrc*, guint64);

static GstAppSrcCallbacks appsrcCallbacks = {
    // need_data
    [](GstAppSrc*, guint, gpointer userData) {
        webKitWebSrcNeedData(WEBKIT_WEB_SRC(userData));
    },
    // enough_data
    [](GstAppSrc*, gpointer userData) {
        webKitWebSrcEnoughData(WEBKIT_WEB_SRC(userData));
    },
    // seek_data
    [](GstAppSrc*, guint64 offset, gpointer userData) -> gboolean {
        return webKitWebSrcSeek(WEBKIT_WEB_SRC(userData), offset);
    },
    { nullptr }
};

#define webkit_web_src_parent_class parent_class
// We split this out into another macro to avoid a check-webkit-style error.
#define WEBKIT_WEB_SRC_CATEGORY_INIT GST_DEBUG_CATEGORY_INIT(webkit_web_src_debug, "webkitwebsrc", 0, "websrc element");
G_DEFINE_TYPE_WITH_CODE(WebKitWebSrc, webkit_web_src, GST_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, webKitWebSrcUriHandlerInit);
                         WEBKIT_WEB_SRC_CATEGORY_INIT);

static void webkit_web_src_class_init(WebKitWebSrcClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);

    oklass->dispose = webKitWebSrcDispose;
    oklass->finalize = webKitWebSrcFinalize;
    oklass->set_property = webKitWebSrcSetProperty;
    oklass->get_property = webKitWebSrcGetProperty;

    gst_element_class_add_pad_template(eklass,
                                       gst_static_pad_template_get(&srcTemplate));
    gst_element_class_set_metadata(eklass, "WebKit Web source element", "Source", "Handles HTTP/HTTPS uris",
                               "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

    /* Allows setting the uri using the 'location' property, which is used
     * for example by gst_element_make_from_uri() */
    g_object_class_install_property(oklass, PROP_LOCATION,
        g_param_spec_string("location", "location", "Location to read from",
            nullptr, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(oklass, PROP_RESOLVED_LOCATION,
        g_param_spec_string("resolved-location", "Resolved location", "The location resolved by the server",
            nullptr, static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(oklass, PROP_KEEP_ALIVE,
        g_param_spec_boolean("keep-alive", "keep-alive", "Use HTTP persistent connections",
            FALSE, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(oklass, PROP_EXTRA_HEADERS,
        g_param_spec_boxed("extra-headers", "Extra Headers", "Extra headers to append to the HTTP request",
            GST_TYPE_STRUCTURE, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(oklass, PROP_COMPRESS,
        g_param_spec_boolean("compress", "Compress", "Allow compressed content encodings",
            FALSE, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(oklass, PROP_METHOD,
        g_param_spec_string("method", "method", "The HTTP method to use (default: GET)",
            nullptr, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    eklass->change_state = webKitWebSrcChangeState;

    g_type_class_add_private(klass, sizeof(WebKitWebSrcPrivate));
}

static void webkit_web_src_init(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = WEBKIT_WEB_SRC_GET_PRIVATE(src);

    src->priv = priv;
    new (priv) WebKitWebSrcPrivate();

    priv->notifier = MainThreadNotifier<MainThreadSourceNotification>::create();

    priv->haveSize = FALSE;
    priv->size = 0;

    priv->appsrc = GST_APP_SRC(gst_element_factory_make("appsrc", nullptr));
    if (!priv->appsrc) {
        GST_ERROR_OBJECT(src, "Failed to create appsrc");
        return;
    }

    gst_bin_add(GST_BIN(src), GST_ELEMENT(priv->appsrc));

    GRefPtr<GstPad> targetPad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT(priv->appsrc), "src"));
    priv->srcpad = webkitGstGhostPadFromStaticTemplate(&srcTemplate, "src", targetPad.get());

    gst_element_add_pad(GST_ELEMENT(src), priv->srcpad);

    GST_OBJECT_FLAG_SET(priv->srcpad, GST_PAD_FLAG_NEED_PARENT);
    gst_pad_set_query_function(priv->srcpad, webKitWebSrcQueryWithParent);

    gst_app_src_set_callbacks(priv->appsrc, &appsrcCallbacks, src, nullptr);
    gst_app_src_set_emit_signals(priv->appsrc, FALSE);
    gst_app_src_set_stream_type(priv->appsrc, GST_APP_STREAM_TYPE_SEEKABLE);

    // 512k is a abitrary number but we should choose a value
    // here to not pause/unpause the SoupMessage too often and
    // to make sure there's always some data available for
    // GStreamer to handle.
    gst_app_src_set_max_bytes(priv->appsrc, 512 * 1024);

    // Emit the need-data signal if the queue contains less
    // than 20% of data. Without this the need-data signal
    // is emitted when the queue is empty, we then dispatch
    // the soup message unpausing to the main loop and from
    // there unpause the soup message. This already takes
    // quite some time and libsoup even needs some more time
    // to actually provide data again. If we do all this
    // already if the queue is 20% empty, it's much more
    // likely that libsoup already provides new data before
    // the queue is really empty.
    // This might need tweaking for ports not using libsoup.
    g_object_set(priv->appsrc, "min-percent", 20, nullptr);

    gst_base_src_set_automatic_eos(GST_BASE_SRC(priv->appsrc), FALSE);

    gst_app_src_set_caps(priv->appsrc, nullptr);

    priv->minimumBlocksize = gst_base_src_get_blocksize(GST_BASE_SRC_CAST(priv->appsrc));
}

static void webKitWebSrcDispose(GObject* object)
{
    WebKitWebSrcPrivate* priv = WEBKIT_WEB_SRC(object)->priv;
    if (priv->notifier) {
        priv->notifier->invalidate();
        priv->notifier = nullptr;
    }

    GST_CALL_PARENT(G_OBJECT_CLASS, dispose, (object));
}

static void webKitWebSrcFinalize(GObject* object)
{
    WebKitWebSrcPrivate* priv = WEBKIT_WEB_SRC(object)->priv;

    priv->~WebKitWebSrcPrivate();

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void webKitWebSrcSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* pspec)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(object);

    switch (propID) {
    case PROP_LOCATION:
        gst_uri_handler_set_uri(reinterpret_cast<GstURIHandler*>(src), g_value_get_string(value), nullptr);
        break;
    case PROP_KEEP_ALIVE:
        src->priv->keepAlive = g_value_get_boolean(value);
        break;
    case PROP_EXTRA_HEADERS: {
        const GstStructure* s = gst_value_get_structure(value);
        src->priv->extraHeaders.reset(s ? gst_structure_copy(s) : nullptr);
        break;
    }
    case PROP_COMPRESS:
        src->priv->compress = g_value_get_boolean(value);
        break;
    case PROP_METHOD:
        src->priv->httpMethod.reset(g_value_dup_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
        break;
    }
}

static void webKitWebSrcGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* pspec)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(object);
    WebKitWebSrcPrivate* priv = src->priv;

    switch (propID) {
    case PROP_LOCATION:
        g_value_set_string(value, priv->originalURI.data());
        break;
    case PROP_RESOLVED_LOCATION:
        g_value_set_string(value, priv->redirectedURI.isNull() ? priv->originalURI.data() : priv->redirectedURI.data());
        break;
    case PROP_KEEP_ALIVE:
        g_value_set_boolean(value, priv->keepAlive);
        break;
    case PROP_EXTRA_HEADERS:
        gst_value_set_structure(value, priv->extraHeaders.get());
        break;
    case PROP_COMPRESS:
        g_value_set_boolean(value, priv->compress);
        break;
    case PROP_METHOD:
        g_value_set_string(value, priv->httpMethod.get());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
        break;
    }
}

static void webKitWebSrcStop(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = src->priv;

    if (priv->resource || (priv->loader && !priv->keepAlive)) {
        GRefPtr<WebKitWebSrc> protector = WTF::ensureGRef(src);
        priv->notifier->cancelPendingNotifications(MainThreadSourceNotification::NeedData | MainThreadSourceNotification::EnoughData | MainThreadSourceNotification::Seek);
        priv->notifier->notify(MainThreadSourceNotification::Stop, [protector, keepAlive = priv->keepAlive] {
            WebKitWebSrcPrivate* priv = protector->priv;

            if (priv->resource) {
                priv->resource->stop();
                priv->resource->setClient(nullptr);
                priv->resource = nullptr;
            }

            if (!keepAlive)
                priv->loader = nullptr;
        });
    }

    bool wasSeeking = std::exchange(priv->isSeeking, false);

    priv->paused = false;

    priv->offset = 0;

    if (!wasSeeking) {
        priv->requestedOffset = 0;
        priv->player = nullptr;
        priv->seekable = FALSE;
    }

    if (priv->appsrc) {
        gst_app_src_set_caps(priv->appsrc, nullptr);
        if (!wasSeeking)
            gst_app_src_set_size(priv->appsrc, -1);
    }

    GST_DEBUG_OBJECT(src, "Stopped request");
}

static bool webKitWebSrcSetExtraHeader(GQuark fieldId, const GValue* value, gpointer userData)
{
    GUniquePtr<gchar> fieldContent;

    if (G_VALUE_HOLDS_STRING(value))
        fieldContent.reset(g_value_dup_string(value));
    else {
        GValue dest = G_VALUE_INIT;

        g_value_init(&dest, G_TYPE_STRING);
        if (g_value_transform(value, &dest))
            fieldContent.reset(g_value_dup_string(&dest));
    }

    const gchar* fieldName = g_quark_to_string(fieldId);
    if (!fieldContent.get()) {
        GST_ERROR("extra-headers field '%s' contains no value or can't be converted to a string", fieldName);
        return false;
    }

    GST_DEBUG("Appending extra header: \"%s: %s\"", fieldName, fieldContent.get());
    ResourceRequest* request = static_cast<ResourceRequest*>(userData);
    request->setHTTPHeaderField(fieldName, fieldContent.get());
    return true;
}

static gboolean webKitWebSrcProcessExtraHeaders(GQuark fieldId, const GValue* value, gpointer userData)
{
    if (G_VALUE_TYPE(value) == GST_TYPE_ARRAY) {
        unsigned size = gst_value_array_get_size(value);

        for (unsigned i = 0; i < size; i++) {
            if (!webKitWebSrcSetExtraHeader(fieldId, gst_value_array_get_value(value, i), userData))
                return FALSE;
        }
        return TRUE;
    }

    if (G_VALUE_TYPE(value) == GST_TYPE_LIST) {
        unsigned size = gst_value_list_get_size(value);

        for (unsigned i = 0; i < size; i++) {
            if (!webKitWebSrcSetExtraHeader(fieldId, gst_value_list_get_value(value, i), userData))
                return FALSE;
        }
        return TRUE;
    }

    return webKitWebSrcSetExtraHeader(fieldId, value, userData);
}

static void webKitWebSrcStart(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = src->priv;
    ASSERT(priv->player);

    priv->didPassAccessControlCheck = false;

    if (priv->originalURI.isNull()) {
        GST_ERROR_OBJECT(src, "No URI provided");
        webKitWebSrcStop(src);
        return;
    }

    GST_DEBUG_OBJECT(src, "Fetching %s", priv->originalURI.data());
    URL url = URL(URL(), priv->originalURI.data());

    ResourceRequest request(url);
    request.setAllowCookies(true);
    request.setFirstPartyForCookies(url);

    request.setHTTPReferrer(priv->player->referrer());

    if (priv->httpMethod.get())
        request.setHTTPMethod(priv->httpMethod.get());

#if USE(SOUP)
    // By default, HTTP Accept-Encoding is disabled here as we don't
    // want the received response to be encoded in any way as we need
    // to rely on the proper size of the returned data on
    // didReceiveResponse.
    // If Accept-Encoding is used, the server may send the data in encoded format and
    // request.expectedContentLength() will have the "wrong" size (the size of the
    // compressed data), even though the data received in didReceiveData is uncompressed.
    // This is however useful to enable for adaptive streaming
    // scenarios, when the demuxer needs to download playlists.
    if (!priv->compress)
        request.setAcceptEncoding(false);
#endif

    // Let Apple web servers know we want to access their nice movie trailers.
    if (!g_ascii_strcasecmp("movies.apple.com", url.host().utf8().data())
        || !g_ascii_strcasecmp("trailers.apple.com", url.host().utf8().data()))
        request.setHTTPUserAgent("Quicktime/7.6.6");

    if (priv->requestedOffset) {
        GUniquePtr<gchar> val(g_strdup_printf("bytes=%" G_GUINT64_FORMAT "-", priv->requestedOffset));
        request.setHTTPHeaderField(HTTPHeaderName::Range, val.get());
    }
    priv->offset = priv->requestedOffset;

    GST_DEBUG_OBJECT(src, "Persistent connection support %s", priv->keepAlive ? "enabled" : "disabled");
    if (!priv->keepAlive) {
        request.setHTTPHeaderField(HTTPHeaderName::Connection, "close");
    }

    if (priv->extraHeaders)
        gst_structure_foreach(priv->extraHeaders.get(), webKitWebSrcProcessExtraHeaders, &request);

    // We always request Icecast/Shoutcast metadata, just in case ...
    request.setHTTPHeaderField(HTTPHeaderName::IcyMetadata, "1");

    GRefPtr<WebKitWebSrc> protector = WTF::ensureGRef(src);
    priv->notifier->notify(MainThreadSourceNotification::Start, [protector, request = WTFMove(request)] {
        WebKitWebSrcPrivate* priv = protector->priv;

        if (!priv->loader)
            priv->loader = priv->player->createResourceLoader();

        PlatformMediaResourceLoader::LoadOptions loadOptions = 0;
        if (request.url().protocolIsBlob())
            loadOptions |= PlatformMediaResourceLoader::LoadOption::BufferData;
        priv->resource = priv->loader->requestResource(ResourceRequest(request), loadOptions);
        if (priv->resource) {
            priv->resource->setClient(std::make_unique<CachedResourceStreamingClient>(protector.get(), ResourceRequest(request)));
            GST_DEBUG_OBJECT(protector.get(), "Started request");
        } else {
            GST_ERROR_OBJECT(protector.get(), "Failed to setup streaming client");
            priv->loader = nullptr;
            webKitWebSrcStop(protector.get());
        }
    });
}

static GstStateChangeReturn webKitWebSrcChangeState(GstElement* element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    WebKitWebSrc* src = WEBKIT_WEB_SRC(element);
    WebKitWebSrcPrivate* priv = src->priv;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!priv->appsrc) {
            gst_element_post_message(element,
                                     gst_missing_element_message_new(element, "appsrc"));
            GST_ELEMENT_ERROR(src, CORE, MISSING_PLUGIN, (nullptr), ("no appsrc"));
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
    {
        GST_DEBUG_OBJECT(src, "READY->PAUSED");
        webKitWebSrcStart(src);
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
        GST_DEBUG_OBJECT(src, "PAUSED->READY");
        webKitWebSrcStop(src);
        break;
    }
    default:
        break;
    }

    return ret;
}

static gboolean webKitWebSrcQueryWithParent(GstPad* pad, GstObject* parent, GstQuery* query)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(GST_ELEMENT(parent));
    WebKitWebSrcPrivate* priv = src->priv;
    gboolean result = FALSE;

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_URI: {
        gst_query_set_uri(query, priv->originalURI.data());
        if (!priv->redirectedURI.isNull())
            gst_query_set_uri_redirection(query, priv->redirectedURI.data());
        result = TRUE;
        break;
    }
    case GST_QUERY_SCHEDULING: {
        GstSchedulingFlags flags;
        int minSize, maxSize, align;

        gst_query_parse_scheduling(query, &flags, &minSize, &maxSize, &align);
        gst_query_set_scheduling(query, static_cast<GstSchedulingFlags>(flags | GST_SCHEDULING_FLAG_BANDWIDTH_LIMITED), minSize, maxSize, align);
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

static bool urlHasSupportedProtocol(const URL& url)
{
    return url.isValid() && (url.protocolIsInHTTPFamily() || url.protocolIsBlob());
}

// uri handler interface

static GstURIType webKitWebSrcUriGetType(GType)
{
    return GST_URI_SRC;
}

const gchar* const* webKitWebSrcGetProtocols(GType)
{
    static const char* protocols[] = {"webkit+http", "webkit+https", "webkit+blob", nullptr };
    return protocols;
}

static URL convertPlaybinURI(const char* uriString)
{
    URL url(URL(), uriString);
    ASSERT(url.protocol().substring(0, 7) == "webkit+");
    url.setProtocol(url.protocol().substring(7).toString());
    return url;
}

static gchar* webKitWebSrcGetUri(GstURIHandler* handler)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(handler);
    gchar* ret = g_strdup(src->priv->originalURI.data());
    return ret;
}

static gboolean webKitWebSrcSetUri(GstURIHandler* handler, const gchar* uri, GError** error)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(handler);
    WebKitWebSrcPrivate* priv = src->priv;

    if (GST_STATE(src) >= GST_STATE_PAUSED) {
        GST_ERROR_OBJECT(src, "URI can only be set in states < PAUSED");
        return FALSE;
    }

    priv->redirectedURI = CString();
    priv->originalURI = CString();
    if (!uri)
        return TRUE;

    URL url = convertPlaybinURI(uri);
    if (!urlHasSupportedProtocol(url)) {
        g_set_error(error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI, "Invalid URI '%s'", uri);
        return FALSE;
    }

    priv->originalURI = url.string().utf8();
    return TRUE;
}

static void webKitWebSrcUriHandlerInit(gpointer gIface, gpointer)
{
    GstURIHandlerInterface* iface = (GstURIHandlerInterface *) gIface;

    iface->get_type = webKitWebSrcUriGetType;
    iface->get_protocols = webKitWebSrcGetProtocols;
    iface->get_uri = webKitWebSrcGetUri;
    iface->set_uri = webKitWebSrcSetUri;
}

static void webKitWebSrcNeedData(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = src->priv;

    GST_LOG_OBJECT(src, "Need more data");

    if (!priv->paused)
        return;
    priv->paused = false;

    GRefPtr<WebKitWebSrc> protector = WTF::ensureGRef(src);
    priv->notifier->notify(MainThreadSourceNotification::NeedData, [protector] { });
}

static void webKitWebSrcEnoughData(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Have enough data");

    if (priv->paused)
        return;
    priv->paused = true;

    GRefPtr<WebKitWebSrc> protector = WTF::ensureGRef(src);
    priv->notifier->notify(MainThreadSourceNotification::EnoughData, [protector] {
        WebKitWebSrcPrivate* priv = protector->priv;
        if (priv->resource)
            priv->resource->stop();
    });
}

static gboolean webKitWebSrcSeek(WebKitWebSrc* src, guint64 offset)
{
    WebKitWebSrcPrivate* priv = src->priv;

    if (offset == priv->offset && priv->requestedOffset == priv->offset)
        return TRUE;

    if (!priv->seekable)
        return FALSE;

    priv->isSeeking = true;
    priv->requestedOffset = offset;

    GST_DEBUG_OBJECT(src, "Seeking to offset: %" G_GUINT64_FORMAT, src->priv->requestedOffset);

    GRefPtr<WebKitWebSrc> protector = WTF::ensureGRef(src);
    priv->notifier->notify(MainThreadSourceNotification::Seek, [protector] {
        webKitWebSrcStop(protector.get());
        webKitWebSrcStart(protector.get());
    });
    return TRUE;
}

void webKitWebSrcSetMediaPlayer(WebKitWebSrc* src, WebCore::MediaPlayer* player)
{
    ASSERT(player);
    src->priv->player = player;
}

bool webKitSrcPassedCORSAccessCheck(WebKitWebSrc* src)
{
    return src->priv->didPassAccessControlCheck;
}

CachedResourceStreamingClient::CachedResourceStreamingClient(WebKitWebSrc* src, ResourceRequest&& request)
    : m_src(GST_ELEMENT(src))
    , m_request(WTFMove(request))
{
}

CachedResourceStreamingClient::~CachedResourceStreamingClient() = default;

void CachedResourceStreamingClient::checkUpdateBlocksize(uint64_t bytesRead)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());
    WebKitWebSrcPrivate* priv = src->priv;

    uint64_t blocksize = gst_base_src_get_blocksize(GST_BASE_SRC_CAST(priv->appsrc));
    GST_LOG_OBJECT(src, "Checking to update blocksize. Read:%" PRIu64 " blocksize:%" PRIu64, bytesRead, blocksize);

    if (bytesRead >= blocksize * s_growBlocksizeLimit) {
        m_reduceBlocksizeCount = 0;
        m_increaseBlocksizeCount++;

        if (m_increaseBlocksizeCount >= s_growBlocksizeCount) {
            blocksize *= s_growBlocksizeFactor;
            GST_DEBUG_OBJECT(src, "Increased blocksize to %" PRIu64, blocksize);
            gst_base_src_set_blocksize(GST_BASE_SRC_CAST(priv->appsrc), blocksize);
            m_increaseBlocksizeCount = 0;
        }
    } else if (bytesRead < blocksize * s_reduceBlocksizeLimit) {
        m_reduceBlocksizeCount++;
        m_increaseBlocksizeCount = 0;

        if (m_reduceBlocksizeCount >= s_reduceBlocksizeCount) {
            blocksize *= s_reduceBlocksizeFactor;
            blocksize = std::max(blocksize, priv->minimumBlocksize);
            GST_DEBUG_OBJECT(src, "Decreased blocksize to %" PRIu64, blocksize);
            gst_base_src_set_blocksize(GST_BASE_SRC_CAST(priv->appsrc), blocksize);
            m_reduceBlocksizeCount = 0;
        }
    } else {
        m_reduceBlocksizeCount = 0;
        m_increaseBlocksizeCount = 0;
    }
}

void CachedResourceStreamingClient::responseReceived(PlatformMediaResource&, const ResourceResponse& response, CompletionHandler<void(ShouldContinue)>&& completionHandler)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());
    WebKitWebSrcPrivate* priv = src->priv;
    priv->didPassAccessControlCheck = priv->resource->didPassAccessControlCheck();

    GST_DEBUG_OBJECT(src, "Received response: %d", response.httpStatusCode());

    auto origin = SecurityOrigin::create(response.url());
    m_origins.add(WTFMove(origin));

    auto responseURI = response.url().string().utf8();
    if (priv->originalURI != responseURI)
        priv->redirectedURI = WTFMove(responseURI);

    if (response.httpStatusCode() >= 400) {
        GST_ELEMENT_ERROR(src, RESOURCE, READ, ("Received %d HTTP error code", response.httpStatusCode()), (nullptr));
        gst_app_src_end_of_stream(priv->appsrc);
        webKitWebSrcStop(src);
        return completionHandler(ShouldContinue::No);
    }

    if (priv->isSeeking) {
        GST_DEBUG_OBJECT(src, "Seek in progress, ignoring response");
        return completionHandler(ShouldContinue::Yes);
    }

    if (priv->requestedOffset) {
        // Seeking ... we expect a 206 == PARTIAL_CONTENT
        if (response.httpStatusCode() == 200) {
            // Range request didn't have a ranged response; resetting offset.
            priv->offset = 0;
        } else if (response.httpStatusCode() != 206) {
            // Range request completely failed.
            GST_ELEMENT_ERROR(src, RESOURCE, READ, ("Received unexpected %d HTTP status code", response.httpStatusCode()), (nullptr));
            gst_app_src_end_of_stream(priv->appsrc);
            webKitWebSrcStop(src);
            return completionHandler(ShouldContinue::No);
        }
    }

    long long length = response.expectedContentLength();
    if (length > 0 && priv->requestedOffset && response.httpStatusCode() == 206)
        length += priv->requestedOffset;

    priv->seekable = length > 0 && g_ascii_strcasecmp("none", response.httpHeaderField(HTTPHeaderName::AcceptRanges).utf8().data());

    GST_DEBUG_OBJECT(src, "Size: %lld, seekable: %s", length, priv->seekable ? "yes" : "no");
    // notify size/duration
    if (length > 0) {
        if (!priv->haveSize || (static_cast<long long>(priv->size) != length)) {
            priv->haveSize = TRUE;
            priv->size = length;
            gst_app_src_set_size(priv->appsrc, length);
        }
    } else {
        gst_app_src_set_size(priv->appsrc, -1);
        if (!priv->seekable)
            gst_app_src_set_stream_type(priv->appsrc, GST_APP_STREAM_TYPE_STREAM);
    }

    // Signal to downstream if this is an Icecast stream.
    GRefPtr<GstCaps> caps;
    String metadataIntervalAsString = response.httpHeaderField(HTTPHeaderName::IcyMetaInt);
    if (!metadataIntervalAsString.isEmpty()) {
        bool isMetadataIntervalParsed;
        int metadataInterval = metadataIntervalAsString.toInt(&isMetadataIntervalParsed);
        if (isMetadataIntervalParsed && metadataInterval > 0) {
            caps = adoptGRef(gst_caps_new_simple("application/x-icy", "metadata-interval", G_TYPE_INT, metadataInterval, nullptr));

            String contentType = response.httpHeaderField(HTTPHeaderName::ContentType);
            GST_DEBUG_OBJECT(src, "Response ContentType: %s", contentType.utf8().data());
            gst_caps_set_simple(caps.get(), "content-type", G_TYPE_STRING, contentType.utf8().data(), nullptr);

            gst_app_src_set_stream_type(priv->appsrc, GST_APP_STREAM_TYPE_STREAM);
        }
    }

    gst_app_src_set_caps(priv->appsrc, caps.get());

    // Emit a GST_EVENT_CUSTOM_DOWNSTREAM_STICKY event and message to let
    // GStreamer know about the HTTP headers sent and received.
    GstStructure* httpHeaders = gst_structure_new_empty("http-headers");
    gst_structure_set(httpHeaders, "uri", G_TYPE_STRING, priv->originalURI.data(),
        "http-status-code", G_TYPE_UINT, response.httpStatusCode(), nullptr);
    if (!priv->redirectedURI.isNull())
        gst_structure_set(httpHeaders, "redirection-uri", G_TYPE_STRING, priv->redirectedURI.data(), nullptr);
    GUniquePtr<GstStructure> headers(gst_structure_new_empty("request-headers"));
    for (const auto& header : m_request.httpHeaderFields())
        gst_structure_set(headers.get(), header.key.utf8().data(), G_TYPE_STRING, header.value.utf8().data(), nullptr);
    GST_DEBUG_OBJECT(src, "Request headers going downstream: %" GST_PTR_FORMAT, headers.get());
    gst_structure_set(httpHeaders, "request-headers", GST_TYPE_STRUCTURE, headers.get(), nullptr);
    headers.reset(gst_structure_new_empty("response-headers"));
    for (const auto& header : response.httpHeaderFields())
        gst_structure_set(headers.get(), header.key.utf8().data(), G_TYPE_STRING, header.value.utf8().data(), nullptr);
    gst_structure_set(httpHeaders, "response-headers", GST_TYPE_STRUCTURE, headers.get(), nullptr);
    GST_DEBUG_OBJECT(src, "Response headers going downstream: %" GST_PTR_FORMAT, headers.get());

    gst_element_post_message(GST_ELEMENT_CAST(src), gst_message_new_element(GST_OBJECT_CAST(src),
        gst_structure_copy(httpHeaders)));
    gst_pad_push_event(GST_BASE_SRC_PAD(priv->appsrc), gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_STICKY, httpHeaders));
    
    completionHandler(ShouldContinue::Yes);
}

void CachedResourceStreamingClient::dataReceived(PlatformMediaResource&, const char* data, int length)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());
    WebKitWebSrcPrivate* priv = src->priv;

    if (priv->isSeeking) {
        GST_DEBUG_OBJECT(src, "Seek in progress, ignoring data");
        return;
    }

    if (priv->offset < priv->requestedOffset) {
        // Range request failed; seeking manually.
        if (priv->offset + length <= priv->requestedOffset) {
            // Discard all the buffers coming before the requested seek position.
            priv->offset += length;
            return;
        }

        if (priv->offset + length > priv->requestedOffset) {
            guint64 offset = priv->requestedOffset - priv->offset;
            data += offset;
            length -= offset;
            priv->offset = priv->requestedOffset;
        }

        priv->requestedOffset = 0;
    }

    checkUpdateBlocksize(length);

    uint64_t startingOffset = priv->offset;

    if (priv->requestedOffset == priv->offset)
        priv->requestedOffset += length;
    priv->offset += length;
    // priv->size == 0 if received length on didReceiveResponse < 0.
    if (priv->size > 0 && priv->offset > priv->size) {
        GST_DEBUG_OBJECT(src, "Updating internal size from %" G_GUINT64_FORMAT " to %" G_GUINT64_FORMAT, priv->size, priv->offset);
        gst_app_src_set_size(priv->appsrc, priv->offset);
        priv->size = priv->offset;
    }

    // Now split the recv'd buffer into buffers that are of a size basesrc suggests. It is important not
    // to push buffers that are too large, otherwise incorrect buffering messages can be sent from the
    // pipeline.
    uint64_t bufferSize = static_cast<uint64_t>(length);
    uint64_t blockSize = static_cast<uint64_t>(gst_base_src_get_blocksize(GST_BASE_SRC_CAST(priv->appsrc)));
    GST_LOG_OBJECT(src, "Splitting the received buffer into %" PRIu64 " blocks", bufferSize / blockSize);
    for (uint64_t currentOffset = 0; currentOffset < bufferSize; currentOffset += blockSize) {
        uint64_t subBufferOffset = startingOffset + currentOffset;
        uint64_t currentOffsetSize = std::min(blockSize, bufferSize - currentOffset);

        GstBuffer* subBuffer = gst_buffer_new_wrapped(g_memdup(data + currentOffset, currentOffsetSize), currentOffsetSize);
        if (UNLIKELY(!subBuffer)) {
            GST_ELEMENT_ERROR(src, CORE, FAILED, ("Failed to allocate sub-buffer"), (nullptr));
            break;
        }

        GST_TRACE_OBJECT(src, "Sub-buffer bounds: %" PRIu64 " -- %" PRIu64, subBufferOffset, subBufferOffset + currentOffsetSize);
        GST_BUFFER_OFFSET(subBuffer) = subBufferOffset;
        GST_BUFFER_OFFSET_END(subBuffer) = subBufferOffset + currentOffsetSize;

        if (priv->isSeeking) {
            GST_TRACE_OBJECT(src, "Stopping buffer appends due to seek");
            // A seek has happened in the middle of us breaking the
            // incoming data up from a previous request. Stop pushing
            // buffers that are now from the incorrect offset.
            break;
        }

        // It may be tempting to use a GstBufferList here, but note
        // that there is a race condition in GstDownloadBuffer during
        // seek flushes that can cause decoders to read at incorrect
        // offsets.
        GstFlowReturn ret = gst_app_src_push_buffer(priv->appsrc, subBuffer);

        if (UNLIKELY(ret != GST_FLOW_OK && ret != GST_FLOW_EOS && ret != GST_FLOW_FLUSHING)) {
            GST_ELEMENT_ERROR(src, CORE, FAILED, (nullptr), (nullptr));
            break;
        }
    }
}

void CachedResourceStreamingClient::accessControlCheckFailed(PlatformMediaResource&, const ResourceError& error)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());
    GST_ELEMENT_ERROR(src, RESOURCE, READ, ("%s", error.localizedDescription().utf8().data()), (nullptr));
    gst_app_src_end_of_stream(src->priv->appsrc);
    webKitWebSrcStop(src);
}

void CachedResourceStreamingClient::loadFailed(PlatformMediaResource&, const ResourceError& error)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());

    if (!error.isCancellation()) {
        GST_ERROR_OBJECT(src, "Have failure: %s", error.localizedDescription().utf8().data());
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("%s", error.localizedDescription().utf8().data()), (nullptr));
    }

    gst_app_src_end_of_stream(src->priv->appsrc);
}

void CachedResourceStreamingClient::loadFinished(PlatformMediaResource&)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());
    WebKitWebSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "Have EOS");

    if (!priv->isSeeking)
        gst_app_src_end_of_stream(priv->appsrc);
}

bool webKitSrcWouldTaintOrigin(WebKitWebSrc* src, const SecurityOrigin& origin)
{
    WebKitWebSrcPrivate* priv = src->priv;

    auto* cachedResourceStreamingClient = reinterpret_cast<CachedResourceStreamingClient*>(priv->resource->client());
    for (auto& responseOrigin : cachedResourceStreamingClient->securityOrigins()) {
        if (!origin.canAccess(*responseOrigin))
            return true;
    }
    return false;
}

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
