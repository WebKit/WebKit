/*
 *  Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *  Copyright (C) 2013 Collabora Ltd.
 *  Copyright (C) 2019 Igalia S.L.
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
#include <cstdint>
#include <wtf/Condition.h>
#include <wtf/Scope.h>
#include <wtf/text/CString.h>

using namespace WebCore;

class CachedResourceStreamingClient final : public PlatformMediaResourceClient {
    WTF_MAKE_NONCOPYABLE(CachedResourceStreamingClient);
public:
    CachedResourceStreamingClient(WebKitWebSrc*, ResourceRequest&&);
    virtual ~CachedResourceStreamingClient();

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
};

enum MainThreadSourceNotification {
    Start = 1 << 0,
    Stop = 1 << 1,
};

#define WEBKIT_WEB_SRC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_SRC, WebKitWebSrcPrivate))
struct _WebKitWebSrcPrivate {
    CString originalURI;
    CString redirectedURI;
    bool keepAlive;
    GUniquePtr<GstStructure> extraHeaders;
    bool compress;
    GUniquePtr<gchar> httpMethod;
    WebCore::MediaPlayer* player;
    RefPtr<PlatformMediaResourceLoader> loader;
    RefPtr<PlatformMediaResource> resource;
    RefPtr<MainThreadNotifier<MainThreadSourceNotification>> notifier;
    bool didPassAccessControlCheck;
    bool wereHeadersReceived;
    Condition headersCondition;
    Lock responseLock;
    bool wasResponseReceived;
    Condition responseCondition;
    bool doesHaveEOS;
    bool isFlushing { false };
    uint64_t readPosition;
    uint64_t requestedPosition;
    uint64_t stopPosition;
    bool isDurationSet;
    bool haveSize;
    uint64_t size;
    bool isSeekable;
    bool isSeeking;
    bool wasSeeking { false };
    uint64_t minimumBlocksize;
    Lock adapterLock;
    Condition adapterCondition;
    uint64_t queueSize { 0 };
    GRefPtr<GstAdapter> adapter;
    GRefPtr<GstEvent> httpHeadersEvent;
    GUniquePtr<GstStructure> httpHeaders;
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

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(webkit_web_src_debug);
#define GST_CAT_DEFAULT webkit_web_src_debug

static void webKitWebSrcUriHandlerInit(gpointer gIface, gpointer ifaceData);

static void webKitWebSrcDispose(GObject*);
static void webKitWebSrcFinalize(GObject*);
static void webKitWebSrcSetProperty(GObject*, guint propertyID, const GValue*, GParamSpec*);
static void webKitWebSrcGetProperty(GObject*, guint propertyID, GValue*, GParamSpec*);
static GstStateChangeReturn webKitWebSrcChangeState(GstElement*, GstStateChange);
static GstFlowReturn webKitWebSrcCreate(GstPushSrc*, GstBuffer**);
static gboolean webKitWebSrcStart(GstBaseSrc*);
static gboolean webKitWebSrcStop(GstBaseSrc*);
static gboolean webKitWebSrcGetSize(GstBaseSrc*, guint64* size);
static gboolean webKitWebSrcIsSeekable(GstBaseSrc*);
static gboolean webKitWebSrcDoSeek(GstBaseSrc*, GstSegment*);
static gboolean webKitWebSrcQuery(GstBaseSrc*, GstQuery*);
static gboolean webKitWebSrcUnLock(GstBaseSrc*);
static gboolean webKitWebSrcUnLockStop(GstBaseSrc*);
static void webKitWebSrcSetContext(GstElement*, GstContext*);

#define webkit_web_src_parent_class parent_class
// We split this out into another macro to avoid a check-webkit-style error.
#define WEBKIT_WEB_SRC_CATEGORY_INIT GST_DEBUG_CATEGORY_INIT(webkit_web_src_debug, "webkitwebsrc", 0, "websrc element");
G_DEFINE_TYPE_WITH_CODE(WebKitWebSrc, webkit_web_src, GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, webKitWebSrcUriHandlerInit);
    WEBKIT_WEB_SRC_CATEGORY_INIT);

static void webkit_web_src_class_init(WebKitWebSrcClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);

    oklass->dispose = webKitWebSrcDispose;
    oklass->finalize = webKitWebSrcFinalize;
    oklass->set_property = webKitWebSrcSetProperty;
    oklass->get_property = webKitWebSrcGetProperty;

    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_static_pad_template(eklass, &srcTemplate);

    gst_element_class_set_metadata(eklass, "WebKit Web source element", "Source", "Handles HTTP/HTTPS uris",
        "Philippe Normand <philn@igalia.com>");

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

    eklass->change_state = GST_DEBUG_FUNCPTR(webKitWebSrcChangeState);
    eklass->set_context = GST_DEBUG_FUNCPTR(webKitWebSrcSetContext);

    GstBaseSrcClass* baseSrcClass = GST_BASE_SRC_CLASS(klass);
    baseSrcClass->start = GST_DEBUG_FUNCPTR(webKitWebSrcStart);
    baseSrcClass->stop = GST_DEBUG_FUNCPTR(webKitWebSrcStop);
    baseSrcClass->unlock = GST_DEBUG_FUNCPTR(webKitWebSrcUnLock);
    baseSrcClass->unlock_stop = GST_DEBUG_FUNCPTR(webKitWebSrcUnLockStop);
    baseSrcClass->get_size = GST_DEBUG_FUNCPTR(webKitWebSrcGetSize);
    baseSrcClass->is_seekable = GST_DEBUG_FUNCPTR(webKitWebSrcIsSeekable);
    baseSrcClass->do_seek = GST_DEBUG_FUNCPTR(webKitWebSrcDoSeek);
    baseSrcClass->query = GST_DEBUG_FUNCPTR(webKitWebSrcQuery);

    GstPushSrcClass* pushSrcClass = GST_PUSH_SRC_CLASS(klass);
    pushSrcClass->create = GST_DEBUG_FUNCPTR(webKitWebSrcCreate);

    g_type_class_add_private(klass, sizeof(WebKitWebSrcPrivate));
}


static void webkitWebSrcReset(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = WEBKIT_WEB_SRC_GET_PRIVATE(src);

    priv->haveSize = false;
    priv->wereHeadersReceived = false;
    priv->isSeekable = false;
    priv->readPosition = 0;
    priv->requestedPosition = 0;
    priv->stopPosition = -1;
    priv->size = 0;
}

static void webkit_web_src_init(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = WEBKIT_WEB_SRC_GET_PRIVATE(src);

    src->priv = priv;
    new (priv) WebKitWebSrcPrivate();

    priv->notifier = MainThreadNotifier<MainThreadSourceNotification>::create();
    priv->adapter = adoptGRef(gst_adapter_new());
    priv->minimumBlocksize = gst_base_src_get_blocksize(GST_BASE_SRC_CAST(src));

    webkitWebSrcReset(src);
    gst_base_src_set_automatic_eos(GST_BASE_SRC_CAST(src), FALSE);
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

static void webKitWebSrcSetContext(GstElement* element, GstContext* context)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(element);
    WebKitWebSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "context type: %s", gst_context_get_context_type(context));
    if (gst_context_has_context_type(context, WEBKIT_WEB_SRC_PLAYER_CONTEXT_TYPE_NAME)) {
        const GValue* value = gst_structure_get_value(gst_context_get_structure(context), "player");
        priv->player = reinterpret_cast<MediaPlayer*>(g_value_get_pointer(value));
    }
    GST_ELEMENT_CLASS(parent_class)->set_context(element, context);
}

static GstFlowReturn webKitWebSrcCreate(GstPushSrc* pushSrc, GstBuffer** buffer)
{
    GstBaseSrc* baseSrc = GST_BASE_SRC_CAST(pushSrc);
    WebKitWebSrc* src = WEBKIT_WEB_SRC(baseSrc);
    WebKitWebSrcPrivate* priv = src->priv;

    GST_TRACE_OBJECT(src, "readPosition = %" G_GUINT64_FORMAT " requestedPosition = %" G_GUINT64_FORMAT, priv->readPosition, priv->requestedPosition);

    if (priv->requestedPosition != priv->readPosition) {
        {
            LockHolder adapterLocker(priv->adapterLock);
            GST_DEBUG_OBJECT(src, "Seeking, flushing adapter");
            // Discard all the buffers coming before the requested seek position.
            gst_adapter_flush(priv->adapter.get(), priv->queueSize);
            priv->queueSize = 0;
        }
        uint64_t requestedPosition = priv->requestedPosition;
        webKitWebSrcStop(baseSrc);
        priv->requestedPosition = requestedPosition;
        webKitWebSrcStart(baseSrc);
    }

    {
        LockHolder locker(priv->responseLock);
        priv->responseCondition.wait(priv->responseLock, [priv] () {
            return priv->wasResponseReceived || priv->isFlushing;
        });
    }

    GST_TRACE_OBJECT(src, "flushing: %s, doesHaveEOS: %s, queueSize: %" G_GSIZE_FORMAT, boolForPrinting(priv->isFlushing), boolForPrinting(priv->doesHaveEOS), priv->queueSize);

    if (priv->isFlushing) {
        GST_DEBUG_OBJECT(src, "Flushing");
        return GST_FLOW_FLUSHING;
    }

    if (priv->doesHaveEOS) {
        GST_DEBUG_OBJECT(src, "EOS");
        return GST_FLOW_EOS;
    }

    unsigned size = gst_base_src_get_blocksize(baseSrc);
    bool isAdapterDrained = false;
    {
        LockHolder adapterLocker(priv->adapterLock);
        unsigned retries = 0;
        size_t available = gst_adapter_available_fast(priv->adapter.get());
        while (available < size && !isAdapterDrained) {
            priv->adapterCondition.waitFor(priv->adapterLock, Seconds(1));
            retries++;
            available = gst_adapter_available_fast(priv->adapter.get());
            if (available && available < size)
                size = available;
            else if (retries > 3)
                isAdapterDrained = true;
        }
    }

    if (isAdapterDrained) {
        GST_DEBUG_OBJECT(src, "Adapter still empty after 3 seconds of waiting, assuming EOS");
        return GST_FLOW_EOS;
    }

    if (priv->haveSize && !priv->isDurationSet) {
        GST_DEBUG_OBJECT(src, "Setting duration to %" G_GUINT64_FORMAT, priv->size);
        baseSrc->segment.duration = priv->size;
        priv->isDurationSet = true;
        gst_element_post_message(GST_ELEMENT_CAST(src), gst_message_new_duration_changed(GST_OBJECT_CAST(src)));
    }

    if (priv->httpHeadersEvent)
        gst_pad_push_event(GST_BASE_SRC_PAD(baseSrc), priv->httpHeadersEvent.leakRef());

    {
        GST_TRACE_OBJECT(src, "Taking %u bytes from adapter", size);
        LockHolder adapterLocker(priv->adapterLock);
        if (size) {
            *buffer = gst_adapter_take_buffer_fast(priv->adapter.get(), size);
            RELEASE_ASSERT(*buffer);

            priv->queueSize -= size;

            GST_BUFFER_OFFSET(*buffer) = baseSrc->segment.position;
            GST_BUFFER_OFFSET_END(*buffer) = GST_BUFFER_OFFSET(*buffer) + size;
            GST_TRACE_OBJECT(src, "Buffer bounds set to %" G_GUINT64_FORMAT "-%" G_GUINT64_FORMAT, GST_BUFFER_OFFSET(*buffer), GST_BUFFER_OFFSET_END(*buffer));
            GST_TRACE_OBJECT(src, "doesHaveEOS: %s, wasSeeking: %s, seeking: %s, size: %u", boolForPrinting(priv->doesHaveEOS), boolForPrinting(priv->wasSeeking), boolForPrinting(priv->isSeeking), size);
            if (priv->haveSize && GST_BUFFER_OFFSET_END(*buffer) >= priv->size) {
                if (priv->wasSeeking)
                    priv->wasSeeking = false;
                else
                    priv->doesHaveEOS = true;
            } else if (priv->wasSeeking)
                priv->wasSeeking = false;
        } else
            GST_ERROR_OBJECT(src, "Empty adapter?");
    }

    return GST_FLOW_OK;
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

static gboolean webKitWebSrcStart(GstBaseSrc* baseSrc)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(baseSrc);
    WebKitWebSrcPrivate* priv = src->priv;

    if (!priv->player) {
        GRefPtr<GstQuery> query = adoptGRef(gst_query_new_context(WEBKIT_WEB_SRC_PLAYER_CONTEXT_TYPE_NAME));
        if (gst_pad_peer_query(GST_BASE_SRC_PAD(baseSrc), query.get())) {
            GstContext* context;

            gst_query_parse_context(query.get(), &context);
            gst_element_set_context(GST_ELEMENT_CAST(src), context);
        } else
            gst_element_post_message(GST_ELEMENT_CAST(src), gst_message_new_need_context(GST_OBJECT_CAST(src), WEBKIT_WEB_SRC_PLAYER_CONTEXT_TYPE_NAME));
    }

    RELEASE_ASSERT(priv->player);

    priv->wereHeadersReceived = false;
    priv->wasResponseReceived = false;
    priv->isDurationSet = false;
    priv->doesHaveEOS = false;
    priv->isFlushing = false;

    priv->didPassAccessControlCheck = false;

    if (priv->originalURI.isNull()) {
        GST_ERROR_OBJECT(src, "No URI provided");
        webKitWebSrcStop(baseSrc);
        return FALSE;
    }

    if (priv->requestedPosition == priv->stopPosition) {
        GST_DEBUG_OBJECT(src, "Empty segment, signaling EOS");
        priv->doesHaveEOS = true;
        return FALSE;
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

    if (priv->requestedPosition) {
        GUniquePtr<char> formatedRange(g_strdup_printf("bytes=%" G_GUINT64_FORMAT "-", priv->requestedPosition));
        GST_DEBUG_OBJECT(src, "Range request: %s", formatedRange.get());
        request.setHTTPHeaderField(HTTPHeaderName::Range, formatedRange.get());
    }
    priv->readPosition = priv->requestedPosition;

    GST_DEBUG_OBJECT(src, "Persistent connection support %s", priv->keepAlive ? "enabled" : "disabled");
    if (!priv->keepAlive)
        request.setHTTPHeaderField(HTTPHeaderName::Connection, "close");

    if (priv->extraHeaders)
        gst_structure_foreach(priv->extraHeaders.get(), webKitWebSrcProcessExtraHeaders, &request);

    // We always request Icecast/Shoutcast metadata, just in case ...
    request.setHTTPHeaderField(HTTPHeaderName::IcyMetadata, "1");

    GRefPtr<WebKitWebSrc> protector = WTF::ensureGRef(src);
    priv->notifier->notifyAndWait(MainThreadSourceNotification::Start, [protector, request = WTFMove(request)] {
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
        }
    });

    GST_DEBUG_OBJECT(src, "Resource loader started");
    return TRUE;
}

static void webKitWebSrcCloseSession(WebKitWebSrc* src)
{
    WebKitWebSrcPrivate* priv = src->priv;
    GRefPtr<WebKitWebSrc> protector = WTF::ensureGRef(src);

    priv->notifier->notifyAndWait(MainThreadSourceNotification::Stop, [protector, keepAlive = priv->keepAlive] {
        WebKitWebSrcPrivate* priv = protector->priv;

        GST_DEBUG_OBJECT(protector.get(), "Stopping resource loader");

        if (priv->resource) {
            priv->resource->stop();
            priv->resource->setClient(nullptr);
            priv->resource = nullptr;
        }

        if (!keepAlive)
            priv->loader = nullptr;
    });

    GST_DEBUG_OBJECT(src, "Resource loader stopped");
}

static gboolean webKitWebSrcStop(GstBaseSrc* baseSrc)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(baseSrc);
    WebKitWebSrcPrivate* priv = src->priv;

    if (priv->resource || (priv->loader && !priv->keepAlive))
        webKitWebSrcCloseSession(src);

    {
        LockHolder adapterLocker(priv->adapterLock);
        gst_adapter_clear(priv->adapter.get());
        priv->queueSize = 0;
    }

    webkitWebSrcReset(src);
    GST_DEBUG_OBJECT(src, "Stopped request");
    return TRUE;
}

static gboolean webKitWebSrcGetSize(GstBaseSrc* baseSrc, guint64* size)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(baseSrc);
    WebKitWebSrcPrivate* priv = src->priv;

    GST_DEBUG_OBJECT(src, "haveSize: %s, size: %" G_GUINT64_FORMAT, boolForPrinting(priv->haveSize), priv->size);
    if (priv->haveSize) {
        *size = priv->size;
        return TRUE;
    }

    return FALSE;
}

static gboolean webKitWebSrcIsSeekable(GstBaseSrc* baseSrc)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(baseSrc);

    GST_DEBUG_OBJECT(src, "isSeekable: %s", boolForPrinting(src->priv->isSeekable));
    return src->priv->isSeekable;
}

static gboolean webKitWebSrcDoSeek(GstBaseSrc* baseSrc, GstSegment* segment)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(baseSrc);
    WebKitWebSrcPrivate* priv = src->priv;
    LockHolder locker(priv->responseLock);

    GST_DEBUG_OBJECT(src, "Seek segment: (%" G_GUINT64_FORMAT "-%" G_GUINT64_FORMAT ")", segment->start, segment->stop);
    if (priv->readPosition == segment->start && priv->requestedPosition == priv->readPosition && priv->stopPosition == segment->stop) {
        GST_DEBUG_OBJECT(src, "Seek to current read/end position and no seek pending");
        return TRUE;
    }

    if (priv->wereHeadersReceived && !priv->isSeekable) {
        GST_WARNING_OBJECT(src, "Not seekable");
        return FALSE;
    }

    if (segment->rate < 0.0 || segment->format != GST_FORMAT_BYTES) {
        GST_WARNING_OBJECT(src, "Invalid seek segment");
        return FALSE;
    }

    if (priv->haveSize && segment->start >= priv->size)
        GST_WARNING_OBJECT(src, "Potentially seeking behind end of file, might EOS immediately");

    priv->isSeeking = true;
    priv->requestedPosition = segment->start;
    priv->stopPosition = segment->stop;
    return TRUE;
}

static gboolean webKitWebSrcQuery(GstBaseSrc* baseSrc, GstQuery* query)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(baseSrc);
    WebKitWebSrcPrivate* priv = src->priv;
    gboolean result = FALSE;

    if (GST_QUERY_TYPE(query) == GST_QUERY_URI) {
        gst_query_set_uri(query, priv->originalURI.data());
        if (!priv->redirectedURI.isNull())
            gst_query_set_uri_redirection(query, priv->redirectedURI.data());
        result = TRUE;
    }

    if (!result)
        result = GST_BASE_SRC_CLASS(parent_class)->query(baseSrc, query);

    if (GST_QUERY_TYPE(query) == GST_QUERY_SCHEDULING) {
        GstSchedulingFlags flags;
        int minSize, maxSize, align;

        gst_query_parse_scheduling(query, &flags, &minSize, &maxSize, &align);
        gst_query_set_scheduling(query, static_cast<GstSchedulingFlags>(flags | GST_SCHEDULING_FLAG_BANDWIDTH_LIMITED), minSize, maxSize, align);
    }

    return result;
}

static gboolean webKitWebSrcUnLock(GstBaseSrc* baseSrc)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(baseSrc);
    LockHolder locker(src->priv->responseLock);

    GST_DEBUG_OBJECT(src, "Unlock");
    src->priv->isFlushing = true;
    src->priv->responseCondition.notifyOne();
    return TRUE;
}

static gboolean webKitWebSrcUnLockStop(GstBaseSrc* baseSrc)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(baseSrc);
    LockHolder locker(src->priv->responseLock);
    GST_DEBUG_OBJECT(src, "Unlock stop");
    src->priv->isFlushing = false;

    return TRUE;
}

static GstStateChangeReturn webKitWebSrcChangeState(GstElement* element, GstStateChange transition)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(element);

#if GST_CHECK_VERSION(1, 14, 0)
    GST_DEBUG_OBJECT(src, gst_state_change_get_name(transition));
#endif
    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
        webKitWebSrcCloseSession(src);
        break;
    default:
        break;
    }

    return GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
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
    static const char* protocols[] = {"http", "https", "blob", nullptr };
    return protocols;
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

    if (priv->originalURI.length()) {
        GST_ERROR_OBJECT(src, "URI can only be set in states < PAUSED");
        return FALSE;
    }

    URL url(URL(), uri);
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
    GstBaseSrc* baseSrc = GST_BASE_SRC_CAST(src);
    WebKitWebSrcPrivate* priv = src->priv;

    uint64_t blocksize = gst_base_src_get_blocksize(baseSrc);
    GST_LOG_OBJECT(src, "Checking to update blocksize. Read: %" PRIu64 ", current blocksize: %" PRIu64, bytesRead, blocksize);

    if (bytesRead >= blocksize * s_growBlocksizeLimit) {
        m_reduceBlocksizeCount = 0;
        m_increaseBlocksizeCount++;

        if (m_increaseBlocksizeCount >= s_growBlocksizeCount) {
            blocksize *= s_growBlocksizeFactor;
            GST_DEBUG_OBJECT(src, "Increased blocksize to %" PRIu64, blocksize);
            gst_base_src_set_blocksize(baseSrc, blocksize);
            m_increaseBlocksizeCount = 0;
        }
    } else if (bytesRead < blocksize * s_reduceBlocksizeLimit) {
        m_reduceBlocksizeCount++;
        m_increaseBlocksizeCount = 0;

        if (m_reduceBlocksizeCount >= s_reduceBlocksizeCount) {
            blocksize *= s_reduceBlocksizeFactor;
            blocksize = std::max(blocksize, priv->minimumBlocksize);
            GST_DEBUG_OBJECT(src, "Decreased blocksize to %" PRIu64, blocksize);
            gst_base_src_set_blocksize(baseSrc, blocksize);
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

    auto responseURI = response.url().string().utf8();
    if (priv->originalURI != responseURI)
        priv->redirectedURI = WTFMove(responseURI);

    uint64_t length = response.expectedContentLength();
    if (length > 0 && priv->requestedPosition && response.httpStatusCode() == 206)
        length += priv->requestedPosition;

    priv->httpHeaders.reset(gst_structure_new_empty("http-headers"));
    gst_structure_set(priv->httpHeaders.get(), "uri", G_TYPE_STRING, priv->originalURI.data(),
        "http-status-code", G_TYPE_UINT, response.httpStatusCode(), nullptr);
    if (!priv->redirectedURI.isNull())
        gst_structure_set(priv->httpHeaders.get(), "redirection-uri", G_TYPE_STRING, priv->redirectedURI.data(), nullptr);
    GUniquePtr<GstStructure> headers(gst_structure_new_empty("request-headers"));
    for (const auto& header : m_request.httpHeaderFields())
        gst_structure_set(headers.get(), header.key.utf8().data(), G_TYPE_STRING, header.value.utf8().data(), nullptr);
    GST_DEBUG_OBJECT(src, "Request headers going downstream: %" GST_PTR_FORMAT, headers.get());
    gst_structure_set(priv->httpHeaders.get(), "request-headers", GST_TYPE_STRUCTURE, headers.get(), nullptr);
    headers.reset(gst_structure_new_empty("response-headers"));
    for (const auto& header : response.httpHeaderFields()) {
        bool ok = false;
        uint64_t convertedValue = header.value.toUInt64(&ok);
        if (ok)
            gst_structure_set(headers.get(), header.key.utf8().data(), G_TYPE_UINT64, convertedValue, nullptr);
        else
            gst_structure_set(headers.get(), header.key.utf8().data(), G_TYPE_STRING, header.value.utf8().data(), nullptr);
    }
    auto contentLengthFieldName(httpHeaderNameString(HTTPHeaderName::ContentLength).toString());
    if (!gst_structure_has_field(headers.get(), contentLengthFieldName.utf8().data()))
        gst_structure_set(headers.get(), contentLengthFieldName.utf8().data(), G_TYPE_UINT64, static_cast<uint64_t>(length), nullptr);
    gst_structure_set(priv->httpHeaders.get(), "response-headers", GST_TYPE_STRUCTURE, headers.get(), nullptr);
    GST_DEBUG_OBJECT(src, "Response headers going downstream: %" GST_PTR_FORMAT, headers.get());

    priv->httpHeadersEvent = adoptGRef(gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_STICKY, gst_structure_copy(priv->httpHeaders.get())));

    auto scopeExit = makeScopeExit([&] {
        GstStructure* structure = gst_structure_copy(src->priv->httpHeaders.get());
        gst_element_post_message(GST_ELEMENT_CAST(src), gst_message_new_element(GST_OBJECT_CAST(src), structure));
    });

    if (response.httpStatusCode() >= 400) {
        GST_ELEMENT_ERROR(src, RESOURCE, READ, ("Received %d HTTP error code", response.httpStatusCode()), (nullptr));
        priv->doesHaveEOS = true;
        webKitWebSrcStop(GST_BASE_SRC_CAST(src));
        completionHandler(ShouldContinue::No);
        return;
    }

    if (priv->requestedPosition) {
        // Seeking ... we expect a 206 == PARTIAL_CONTENT
        if (response.httpStatusCode() == 200) {
            // Range request didn't have a ranged response; resetting offset.
            priv->readPosition = 0;
        } else if (response.httpStatusCode() != 206) {
            // Range request completely failed.
            GST_ELEMENT_ERROR(src, RESOURCE, READ, ("Received unexpected %d HTTP status code", response.httpStatusCode()), (nullptr));
            priv->doesHaveEOS = true;
            webKitWebSrcStop(GST_BASE_SRC_CAST(src));
            completionHandler(ShouldContinue::No);
            return;
        } else {
            GST_DEBUG_OBJECT(src, "Range request succeeded");
            priv->isSeeking = false;
            priv->wasSeeking = true;
        }
    }

    priv->isSeekable = length > 0 && g_ascii_strcasecmp("none", response.httpHeaderField(HTTPHeaderName::AcceptRanges).utf8().data());

    GST_DEBUG_OBJECT(src, "Size: %" G_GUINT64_FORMAT ", isSeekable: %s", length, boolForPrinting(priv->isSeekable));
    if (length > 0) {
        if (!priv->haveSize || priv->size != length) {
            priv->haveSize = true;
            priv->size = length;
            priv->isDurationSet = false;
        }
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
        }
    }

    if (caps) {
        GST_DEBUG_OBJECT(src, "Set caps to %" GST_PTR_FORMAT, caps.get());
        gst_base_src_set_caps(GST_BASE_SRC_CAST(src), caps.get());
    }

    {
        LockHolder locker(priv->responseLock);
        priv->wereHeadersReceived = true;
        priv->headersCondition.notifyOne();
    }
    completionHandler(ShouldContinue::Yes);
}

void CachedResourceStreamingClient::dataReceived(PlatformMediaResource&, const char* data, int length)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());
    GstBaseSrc* baseSrc = GST_BASE_SRC_CAST(src);
    WebKitWebSrcPrivate* priv = src->priv;

    GST_LOG_OBJECT(src, "Have %d bytes of data", length);
    LockHolder locker(priv->responseLock);

    uint64_t newPosition = priv->readPosition + length;
    if (LIKELY (priv->requestedPosition == priv->readPosition))
        priv->requestedPosition = newPosition;
    priv->readPosition = newPosition;

    uint64_t newSize = 0;
    if (priv->haveSize && (newPosition > priv->size)) {
        GST_DEBUG_OBJECT(src, "Got position previous estimated content size (%" G_GINT64_FORMAT " > %" G_GINT64_FORMAT ")", newPosition, priv->size);
        newSize = newPosition;
    } else if (!priv->haveSize) {
        GST_DEBUG_OBJECT(src, "Got initial response without Content-Length, assuming response size as duration.");
        newSize = length;
        priv->haveSize = true;
    }

    if (newSize) {
        priv->size = newSize;
        baseSrc->segment.duration = priv->size;
        gst_element_post_message(GST_ELEMENT_CAST(src), gst_message_new_duration_changed(GST_OBJECT_CAST(src)));
    }

    gst_element_post_message(GST_ELEMENT_CAST(src), gst_message_new_element(GST_OBJECT_CAST(src),
        gst_structure_new("webkit-network-statistics", "read-position", G_TYPE_UINT64, priv->readPosition, "size", G_TYPE_UINT64, priv->size, nullptr)));

    checkUpdateBlocksize(length);

    if (!priv->wasResponseReceived)
        priv->wasResponseReceived = true;
    priv->responseCondition.notifyOne();

    {
        LockHolder adapterLocker(priv->adapterLock);
        GstBuffer* buffer = gst_buffer_new_wrapped(g_memdup(data, length), length);
        priv->queueSize += length;
        gst_adapter_push(priv->adapter.get(), buffer);
        priv->adapterCondition.notifyOne();
    }
}

void CachedResourceStreamingClient::accessControlCheckFailed(PlatformMediaResource&, const ResourceError& error)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());
    GST_ELEMENT_ERROR(src, RESOURCE, READ, ("%s", error.localizedDescription().utf8().data()), (nullptr));
    src->priv->doesHaveEOS = true;
    webKitWebSrcStop(GST_BASE_SRC_CAST(src));
}

void CachedResourceStreamingClient::loadFailed(PlatformMediaResource&, const ResourceError& error)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());

    if (!error.isCancellation()) {
        GST_ERROR_OBJECT(src, "Have failure: %s", error.localizedDescription().utf8().data());
        GST_ELEMENT_ERROR(src, RESOURCE, FAILED, ("%s", error.localizedDescription().utf8().data()), (nullptr));
    }

    src->priv->doesHaveEOS = true;
}

void CachedResourceStreamingClient::loadFinished(PlatformMediaResource&)
{
    WebKitWebSrc* src = WEBKIT_WEB_SRC(m_src.get());
    WebKitWebSrcPrivate* priv = src->priv;

    if (priv->isSeeking && !priv->isFlushing)
        priv->isSeeking = false;
}

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
