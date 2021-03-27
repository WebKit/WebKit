/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
 * Author: Philippe Normand <philn@igalia.com>
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
#include "GStreamerMediaStreamSource.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "AudioTrackPrivate.h"
#include "GStreamerAudioData.h"
#include "GStreamerCommon.h"
#include "MediaSampleGStreamer.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrackPrivate.h"
#include "VideoTrackPrivate.h"

#include <gst/app/gstappsrc.h>
#include <wtf/UUID.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

static void webkitMediaStreamSrcPushVideoSample(WebKitMediaStreamSrc*, GstSample*);
static void webkitMediaStreamSrcPushAudioSample(WebKitMediaStreamSrc*, const GRefPtr<GstSample>&);
static void webkitMediaStreamSrcTrackEnded(WebKitMediaStreamSrc*, MediaStreamTrackPrivate&);
static void webkitMediaStreamSrcRemoveTrackByType(WebKitMediaStreamSrc*, RealtimeMediaSource::Type);
static void webkitMediaStreamSrcRemoveTrackObserver(WebKitMediaStreamSrc*, MediaStreamTrackPrivate&);
static void webkitMediaStreamSrcPostStreamCollection(WebKitMediaStreamSrc*);
static void webkitMediaStreamSrcRemovePad(WebKitMediaStreamSrc*, const char* padName);

static GstStaticPadTemplate videoSrcTemplate = GST_STATIC_PAD_TEMPLATE("video_src", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("video/x-raw;video/x-h264;video/x-vp8"));

static GstStaticPadTemplate audioSrcTemplate = GST_STATIC_PAD_TEMPLATE("audio_src", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("audio/x-raw(ANY);"));

GST_DEBUG_CATEGORY_STATIC(webkitMediaStreamSrcDebug);
#define GST_CAT_DEFAULT webkitMediaStreamSrcDebug

GRefPtr<GstTagList> mediaStreamTrackPrivateGetTags(MediaStreamTrackPrivate* track)
{
    auto tagList = adoptGRef(gst_tag_list_new_empty());

    if (!track->label().isEmpty())
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, GST_TAG_TITLE, track->label().utf8().data(), nullptr);

    if (track->type() == RealtimeMediaSource::Type::Audio)
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_KIND, static_cast<int>(AudioTrackPrivate::Kind::Main), nullptr);
    else if (track->type() == RealtimeMediaSource::Type::Video) {
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_KIND, static_cast<int>(VideoTrackPrivate::Kind::Main), nullptr);

        auto& settings = track->settings();
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_WIDTH, settings.width(),
            WEBKIT_MEDIA_TRACK_TAG_HEIGHT, settings.height(), nullptr);
    }

    GST_DEBUG("Track tags: %" GST_PTR_FORMAT, tagList.get());
    return tagList.leakRef();
}

GstStream* webkitMediaStreamNew(MediaStreamTrackPrivate* track)
{
    GRefPtr<GstCaps> caps;
    GstStreamType type;

    if (track->type() == RealtimeMediaSource::Type::Audio) {
        caps = adoptGRef(gst_static_pad_template_get_caps(&audioSrcTemplate));
        type = GST_STREAM_TYPE_AUDIO;
    } else if (track->type() == RealtimeMediaSource::Type::Video) {
        caps = adoptGRef(gst_static_pad_template_get_caps(&videoSrcTemplate));
        type = GST_STREAM_TYPE_VIDEO;
    } else {
        GST_FIXME("Handle %d type", static_cast<int>(track->type()));
        return nullptr;
    }

    auto* stream = gst_stream_new(track->id().utf8().data(), caps.get(), type, GST_STREAM_FLAG_SELECT);
    auto tags = mediaStreamTrackPrivateGetTags(track);
    gst_stream_set_tags(stream, tags.leakRef());
    return stream;
}

class WebKitMediaStreamTrackObserver
    : public MediaStreamTrackPrivate::Observer
    , public RealtimeMediaSource::AudioSampleObserver
    , public RealtimeMediaSource::VideoSampleObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~WebKitMediaStreamTrackObserver() { };
    WebKitMediaStreamTrackObserver(GstElement* src)
        : m_src(src) { }
    void trackStarted(MediaStreamTrackPrivate&) final { };

    void trackEnded(MediaStreamTrackPrivate& track) final
    {
        if (m_src)
            webkitMediaStreamSrcTrackEnded(WEBKIT_MEDIA_STREAM_SRC(m_src), track);
    }

    void trackEnabledChanged(MediaStreamTrackPrivate& track) final
    {
        m_enabled = track.enabled();
    }

    void trackMutedChanged(MediaStreamTrackPrivate&) final { };
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { };
    void readyStateChanged(MediaStreamTrackPrivate&) final { };

    void videoSampleAvailable(MediaSample& sample) final
    {
        if (!m_enabled || !m_src)
            return;

        auto* gstSample = static_cast<MediaSampleGStreamer*>(&sample)->platformSample().sample.gstSample;
        webkitMediaStreamSrcPushVideoSample(WEBKIT_MEDIA_STREAM_SRC(m_src), gstSample);
    }

    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData& audioData, const AudioStreamDescription&, size_t) final
    {
        if (!m_enabled || !m_src)
            return;

        auto data = static_cast<const GStreamerAudioData&>(audioData);
        webkitMediaStreamSrcPushAudioSample(WEBKIT_MEDIA_STREAM_SRC(m_src), data.getSample());
    }

private:
    GstElement* m_src { nullptr };
    bool m_enabled { true };
};

class WebKitMediaStreamObserver : public MediaStreamPrivate::Observer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~WebKitMediaStreamObserver() { };
    WebKitMediaStreamObserver(GstElement* src)
        : m_src(src) { }

    void characteristicsChanged() final
    {
        if (m_src)
            GST_DEBUG_OBJECT(m_src, "renegotiation should happen");
    }
    void activeStatusChanged() final { }

    void didAddTrack(MediaStreamTrackPrivate& track) final
    {
        if (m_src)
            webkitMediaStreamSrcAddTrack(WEBKIT_MEDIA_STREAM_SRC(m_src), &track, false);
    }

    void didRemoveTrack(MediaStreamTrackPrivate& track) final
    {
        if (!m_src)
            return;

        webkitMediaStreamSrcRemoveTrackObserver(WEBKIT_MEDIA_STREAM_SRC(m_src), track);
        webkitMediaStreamSrcRemoveTrackByType(WEBKIT_MEDIA_STREAM_SRC(m_src), track.type());
    }

private:
    GstElement* m_src;
};

class InternalSource {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InternalSource(bool isCaptureTrack, RealtimeMediaSource::Type type)
        : m_type(type)
    {
        m_src = gst_element_factory_make("appsrc", nullptr);
        RELEASE_ASSERT_WITH_MESSAGE(GST_IS_APP_SRC(m_src.get()), "GStreamer appsrc element not found. Please make sure to install gst-plugins-base");

        g_object_set(m_src.get(), "is-live", TRUE, "format", GST_FORMAT_TIME, "emit-signals", TRUE, "min-percent", 100,
            "do-timestamp", isCaptureTrack, nullptr);
        g_signal_connect(m_src.get(), "enough-data", G_CALLBACK(+[](GstElement*, InternalSource* data) {
            data->m_enoughData = true;
        }), this);
        g_signal_connect(m_src.get(), "need-data", G_CALLBACK(+[](GstElement*, unsigned, InternalSource* data) {
            data->m_enoughData = false;
        }), this);
    }

    ~InternalSource()
    {
        g_signal_handlers_disconnect_matched(m_src.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);

        if (auto parent = adoptGRef(gst_object_get_parent(GST_OBJECT_CAST(m_src.get())))) {
            GST_STATE_LOCK(GST_ELEMENT_CAST(parent.get()));
            gst_element_set_locked_state(m_src.get(), true);
            gst_element_set_state(m_src.get(), GST_STATE_NULL);
            gst_bin_remove(GST_BIN_CAST(parent.get()), m_src.get());
            gst_element_set_locked_state(m_src.get(), false);
            GST_STATE_UNLOCK(GST_ELEMENT_CAST(parent.get()));

            const char* padName = m_type == RealtimeMediaSource::Type::Video ? "video_src" : "audio_src";
            webkitMediaStreamSrcRemovePad(WEBKIT_MEDIA_STREAM_SRC(parent.get()), padName);
        }
    }

    GstElement* get() const { return m_src.get(); }

    void pushSample(GstSample* sample)
    {
        ASSERT(m_src);
        if (!m_src)
            return;

        bool drop = m_enoughData;
        auto* buffer = gst_sample_get_buffer(sample);
        auto* caps = gst_sample_get_caps(sample);
        if (!GST_CLOCK_TIME_IS_VALID(m_firstBufferPts)) {
            m_firstBufferPts = GST_BUFFER_PTS(buffer);
            auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
            gst_pad_set_offset(pad.get(), -m_firstBufferPts);
        }

        if (m_type == RealtimeMediaSource::Type::Video && drop)
            drop = doCapsHaveType(caps, "video/x-raw") || GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

        if (drop) {
            m_needsDiscont = true;
            GST_INFO_OBJECT(m_src.get(), "%s queue full already... not pushing", m_type == RealtimeMediaSource::Type::Video ? "Video" : "Audio");
            return;
        }

        if (m_needsDiscont) {
            GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DISCONT);
            m_needsDiscont = false;
        }

        gst_app_src_push_sample(GST_APP_SRC(m_src.get()), sample);
    }

private:
    RealtimeMediaSource::Type m_type { RealtimeMediaSource::Type::None };
    GRefPtr<GstElement> m_src;
    GstClockTime m_firstBufferPts { GST_CLOCK_TIME_NONE };
    bool m_enoughData { false };
    bool m_needsDiscont { false };
};

struct _WebKitMediaStreamSrcPrivate {
    CString uri;
    Optional<InternalSource> audioSrc;
    Optional<InternalSource> videoSrc;
    std::unique_ptr<WebKitMediaStreamTrackObserver> mediaStreamTrackObserver;
    std::unique_ptr<WebKitMediaStreamObserver> mediaStreamObserver;
    RefPtr<MediaStreamPrivate> stream;
    Vector<RefPtr<MediaStreamTrackPrivate>> tracks;
    GUniquePtr<GstFlowCombiner> flowCombiner;
    GRefPtr<GstStreamCollection> streamCollection;
};

enum {
    PROP_0,
    PROP_IS_LIVE,
    PROP_LAST
};

static GstURIType webkitMediaStreamSrcUriGetType(GType)
{
    return GST_URI_SRC;
}

static const char* const* webkitMediaStreamSrcUriGetProtocols(GType)
{
    static const char* protocols[] = { "mediastream", nullptr };
    return protocols;
}

static char* webkitMediaStreamSrcUriGetUri(GstURIHandler* handler)
{
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC(handler);
    return g_strdup(self->priv->uri.data());
}

static gboolean webkitMediaStreamSrcUriSetUri(GstURIHandler* handler, const char* uri, GError**)
{
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC(handler);
    self->priv->uri = CString(uri);
    return TRUE;
}

static void webkitMediaStreamSrcUriHandlerInit(gpointer gIface, gpointer)
{
    auto* iface = static_cast<GstURIHandlerInterface*>(gIface);
    iface->get_type = webkitMediaStreamSrcUriGetType;
    iface->get_protocols = webkitMediaStreamSrcUriGetProtocols;
    iface->get_uri = webkitMediaStreamSrcUriGetUri;
    iface->set_uri = webkitMediaStreamSrcUriSetUri;
}

#define doInit \
    G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, webkitMediaStreamSrcUriHandlerInit); \
    GST_DEBUG_CATEGORY_INIT(webkitMediaStreamSrcDebug, "webkitmediastreamsrc", 0, "mediastreamsrc element"); \
    gst_tag_register_static(WEBKIT_MEDIA_TRACK_TAG_WIDTH, GST_TAG_FLAG_META, G_TYPE_INT, "Webkit MediaStream width", "Webkit MediaStream width", gst_tag_merge_use_first); \
    gst_tag_register_static(WEBKIT_MEDIA_TRACK_TAG_HEIGHT, GST_TAG_FLAG_META, G_TYPE_INT, "Webkit MediaStream height", "Webkit MediaStream height", gst_tag_merge_use_first); \
    gst_tag_register_static(WEBKIT_MEDIA_TRACK_TAG_KIND, GST_TAG_FLAG_META, G_TYPE_INT, "Webkit MediaStream Kind", "Webkit MediaStream Kind", gst_tag_merge_use_first);

#define webkit_media_stream_src_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitMediaStreamSrc, webkit_media_stream_src, GST_TYPE_BIN, doInit)

static void webkitMediaStreamSrcSetProperty(GObject* object, guint propertyId, const GValue*, GParamSpec* pspec)
{
    switch (propertyId) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkitMediaStreamSrcGetProperty(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    switch (propertyId) {
    case PROP_IS_LIVE:
        g_value_set_boolean(value, TRUE);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkitMediaStreamSrcConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC(object);
    auto* priv = self->priv;

    priv->mediaStreamTrackObserver = makeUnique<WebKitMediaStreamTrackObserver>(GST_ELEMENT_CAST(self));
    priv->mediaStreamObserver = makeUnique<WebKitMediaStreamObserver>(GST_ELEMENT_CAST(self));
    priv->flowCombiner = GUniquePtr<GstFlowCombiner>(gst_flow_combiner_new());

    // https://bugs.webkit.org/show_bug.cgi?id=214150
    ASSERT(GST_OBJECT_REFCOUNT(self) == 1);
    ASSERT(g_object_is_floating(self));
}

static void webkitMediaStreamSrcRemoveTrackObserver(WebKitMediaStreamSrc* self, MediaStreamTrackPrivate& track)
{
    auto* priv = self->priv;
    switch (track.type()) {
    case RealtimeMediaSource::Type::Video:
        track.source().removeVideoSampleObserver(*priv->mediaStreamTrackObserver);
        break;
    case RealtimeMediaSource::Type::Audio:
        track.source().removeAudioSampleObserver(*priv->mediaStreamTrackObserver);
        break;
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }
    track.removeObserver(*priv->mediaStreamTrackObserver);
}

static void stopObservingTracks(WebKitMediaStreamSrc* self)
{
    GST_OBJECT_LOCK(self);
    auto* priv = self->priv;
    for (auto& track : priv->tracks)
        webkitMediaStreamSrcRemoveTrackObserver(self, *track);
    GST_OBJECT_UNLOCK(self);
}

static void webkitMediaStreamSrcDispose(GObject* object)
{
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC(object);

    stopObservingTracks(self);

    GST_OBJECT_LOCK(self);
    auto* priv = self->priv;
    if (priv->stream) {
        priv->stream->removeObserver(*priv->mediaStreamObserver);
        priv->stream = nullptr;
    }
    GST_OBJECT_UNLOCK(self);

    GST_CALL_PARENT(G_OBJECT_CLASS, dispose, (object));
}

static GstStateChangeReturn webkitMediaStreamSrcChangeState(GstElement* element, GstStateChange transition)
{
    GST_DEBUG_OBJECT(element, "%s", gst_state_change_get_name(transition));

    if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
        stopObservingTracks(WEBKIT_MEDIA_STREAM_SRC(element));

    GstStateChangeReturn result = GST_ELEMENT_CLASS(webkit_media_stream_src_parent_class)->change_state(element, transition);

    if (transition == GST_STATE_CHANGE_READY_TO_PAUSED)
        result = GST_STATE_CHANGE_NO_PREROLL;

    return result;
}

static void webkit_media_stream_src_class_init(WebKitMediaStreamSrcClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass* gstElementClass = GST_ELEMENT_CLASS(klass);

    gobjectClass->constructed = webkitMediaStreamSrcConstructed;
    gobjectClass->dispose = webkitMediaStreamSrcDispose;
    gobjectClass->get_property = webkitMediaStreamSrcGetProperty;
    gobjectClass->set_property = webkitMediaStreamSrcSetProperty;

    g_object_class_install_property(gobjectClass, PROP_IS_LIVE, g_param_spec_boolean("is-live", "Is Live", "Let playbin3 know we are a live source.",
        TRUE, static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    gstElementClass->change_state = GST_DEBUG_FUNCPTR(webkitMediaStreamSrcChangeState);
    gst_element_class_add_pad_template(gstElementClass, gst_static_pad_template_get(&videoSrcTemplate));
    gst_element_class_add_pad_template(gstElementClass, gst_static_pad_template_get(&audioSrcTemplate));
}

static GstFlowReturn webkitMediaStreamSrcChain(GstPad* pad, GstObject* parent, GstBuffer* buffer)
{
    GRefPtr<GstElement> element = adoptGRef(GST_ELEMENT_CAST(gst_object_get_parent(parent)));
    GstFlowReturn chainResult = gst_proxy_pad_chain_default(pad, GST_OBJECT_CAST(element.get()), buffer);
    auto* self = WEBKIT_MEDIA_STREAM_SRC(element.get());
    GstFlowReturn result = gst_flow_combiner_update_pad_flow(self->priv->flowCombiner.get(), pad, chainResult);

    if (result == GST_FLOW_FLUSHING)
        return chainResult;

    return result;
}

static void webkitMediaStreamSrcAddPad(WebKitMediaStreamSrc* self, GstPad* target, GstStaticPadTemplate* padTemplate, GRefPtr<GstTagList>&& tags, RealtimeMediaSource::Type sourceType)
{
    GST_DEBUG_OBJECT(self, "%s Ghosting %" GST_PTR_FORMAT, gst_object_get_path_string(GST_OBJECT_CAST(self)), target);

    const char* padName = sourceType == RealtimeMediaSource::Type::Audio ? "audio_src" : "video_src";
    auto* ghostPad = webkitGstGhostPadFromStaticTemplate(padTemplate, padName, target);
    gst_pad_set_active(ghostPad, TRUE);
    gst_element_add_pad(GST_ELEMENT_CAST(self), ghostPad);

    auto proxyPad = adoptGRef(GST_PAD_CAST(gst_proxy_pad_get_internal(GST_PROXY_PAD(ghostPad))));
    gst_flow_combiner_add_pad(self->priv->flowCombiner.get(), proxyPad.get());
    gst_pad_set_chain_function(proxyPad.get(), static_cast<GstPadChainFunction>(webkitMediaStreamSrcChain));

    gst_pad_push_event(target, gst_event_new_tag(tags.leakRef()));
}

static void webkitMediaStreamSrcRemovePad(WebKitMediaStreamSrc* self, const char* padName)
{
    auto pad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT_CAST(self), padName));
    if (!pad)
        return;

    auto proxyPad = adoptGRef(GST_PAD_CAST(gst_proxy_pad_get_internal(GST_PROXY_PAD(pad.get()))));
    if (proxyPad)
        gst_flow_combiner_remove_pad(self->priv->flowCombiner.get(), proxyPad.get());

    gst_pad_set_active(pad.get(), FALSE);
    gst_element_remove_pad(GST_ELEMENT_CAST(self), pad.get());
}

struct ProbeData {
    ProbeData(GstElement* element, GstStaticPadTemplate* padTemplate, GRefPtr<GstTagList>&& tags, const char* trackId, RealtimeMediaSource::Type sourceType)
        : element(element)
        , padTemplate(padTemplate)
        , tags(WTFMove(tags))
        , trackId(g_strdup(trackId))
        , sourceType(sourceType)
    {
    }

    GRefPtr<GstElement> element;
    GstStaticPadTemplate* padTemplate;
    GRefPtr<GstTagList> tags;
    GUniquePtr<char> trackId;
    RealtimeMediaSource::Type sourceType;
};

static GstPadProbeReturn webkitMediaStreamSrcPadProbeCb(GstPad* pad, GstPadProbeInfo* info, ProbeData* data)
{
    GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC(data->element.get());

    GST_DEBUG_OBJECT(self, "Event %" GST_PTR_FORMAT, event);
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_STREAM_START: {
        const char* streamId;
        gst_event_parse_stream_start(event, &streamId);
        if (!g_strcmp0(streamId, data->trackId.get())) {
            GST_INFO_OBJECT(pad, "Event has been sticked already");
            return GST_PAD_PROBE_REMOVE;
        }

        auto* streamStart = gst_event_new_stream_start(data->trackId.get());
        gst_event_set_group_id(streamStart, 1);
        gst_pad_push_event(pad, streamStart);

        webkitMediaStreamSrcAddPad(self, pad, data->padTemplate, WTFMove(data->tags), data->sourceType);
        callOnMainThreadAndWait([element = data->element] {
            webkitMediaStreamSrcPostStreamCollection(WEBKIT_MEDIA_STREAM_SRC(element.get()));
        });
        return GST_PAD_PROBE_REMOVE;
    }
    default:
        break;
    }

    return GST_PAD_PROBE_OK;
}

static void webkitMediaStreamSrcSetupSrc(WebKitMediaStreamSrc* self, MediaStreamTrackPrivate* track, GstElement* element, GstStaticPadTemplate* padTemplate, bool onlyTrack)
{
    const char* sourceType = "unknown";
    switch (track->source().type()) {
    case RealtimeMediaSource::Type::Audio:
        sourceType = "audio";
        break;
    case RealtimeMediaSource::Type::Video:
        sourceType = "video";
        break;
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }

    GST_DEBUG_OBJECT(self, "Setup %s source for track %s, only track: %s", sourceType, track->id().utf8().data(), boolForPrinting(onlyTrack));
    gst_bin_add(GST_BIN_CAST(self), element);

    auto pad = adoptGRef(gst_element_get_static_pad(element, "src"));
    auto tags = mediaStreamTrackPrivateGetTags(track);
    if (!onlyTrack) {
        auto* data = new ProbeData(GST_ELEMENT_CAST(self), padTemplate, WTFMove(tags), track->id().utf8().data(), track->source().type());
        gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(webkitMediaStreamSrcPadProbeCb), data, [](gpointer data) {
            delete reinterpret_cast<ProbeData*>(data);
        });
    } else {
        gst_pad_set_active(pad.get(), TRUE);
        webkitMediaStreamSrcAddPad(self, pad.get(), padTemplate, WTFMove(tags), track->source().type());
    }

    auto* priv = self->priv;
    track->addObserver(*priv->mediaStreamTrackObserver);
    switch (track->type()) {
    case RealtimeMediaSource::Type::Audio:
        track->source().addAudioSampleObserver(*priv->mediaStreamTrackObserver);
        break;
    case RealtimeMediaSource::Type::Video:
        track->source().addVideoSampleObserver(*priv->mediaStreamTrackObserver);
        break;
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }

    gst_element_sync_state_with_parent(element);
}

static void webkitMediaStreamSrcPostStreamCollection(WebKitMediaStreamSrc* self)
{
    auto* priv = self->priv;

    GST_OBJECT_LOCK(self);
    String upstreamId;
    if (priv->stream)
        upstreamId = priv->stream->id();
    else
        upstreamId = createCanonicalUUIDString();

    priv->streamCollection = adoptGRef(gst_stream_collection_new(upstreamId.ascii().data()));
    for (auto& track : priv->tracks)
        gst_stream_collection_add_stream(priv->streamCollection.get(), webkitMediaStreamNew(track.get()));

    GST_OBJECT_UNLOCK(self);

    GST_DEBUG_OBJECT(self, "Posting stream collection");
    gst_element_post_message(GST_ELEMENT_CAST(self), gst_message_new_stream_collection(GST_OBJECT_CAST(self), priv->streamCollection.get()));
}

void webkitMediaStreamSrcAddTrack(WebKitMediaStreamSrc* self, MediaStreamTrackPrivate* track, bool onlyTrack)
{
    auto* priv = self->priv;
    if (track->type() == RealtimeMediaSource::Type::Audio) {
        priv->audioSrc.emplace(track->isCaptureTrack(), track->type());
        webkitMediaStreamSrcSetupSrc(self, track, priv->audioSrc->get(), &audioSrcTemplate, onlyTrack);
    } else if (track->type() == RealtimeMediaSource::Type::Video) {
        priv->videoSrc.emplace(track->isCaptureTrack(), track->type());
        webkitMediaStreamSrcSetupSrc(self, track, priv->videoSrc->get(), &videoSrcTemplate, onlyTrack);
    } else
        GST_INFO_OBJECT(self, "Unsupported track type: %d", static_cast<int>(track->type()));

    self->priv->tracks.append(track);
}

static void webkitMediaStreamSrcRemoveTrackByType(WebKitMediaStreamSrc* self, RealtimeMediaSource::Type trackType)
{
    if (trackType == RealtimeMediaSource::Type::Audio)
        self->priv->audioSrc.reset();
    else if (trackType == RealtimeMediaSource::Type::Video)
        self->priv->videoSrc.reset();
    else
        GST_INFO_OBJECT(self, "Unsupported track type: %d", static_cast<int>(trackType));
}

void webkitMediaStreamSrcSetStream(WebKitMediaStreamSrc* self, MediaStreamPrivate* stream)
{
    ASSERT(WEBKIT_IS_MEDIA_STREAM_SRC(self));
    ASSERT(!self->priv->stream);
    self->priv->stream = stream;

    self->priv->stream->addObserver(*self->priv->mediaStreamObserver.get());
    auto tracks = stream->tracks();
    bool onlyTrack = tracks.size() == 1;
    for (auto& track : tracks)
        webkitMediaStreamSrcAddTrack(self, track.get(), onlyTrack);

    webkitMediaStreamSrcPostStreamCollection(self);
    gst_element_no_more_pads(GST_ELEMENT_CAST(self));
}

static void webkitMediaStreamSrcPushVideoSample(WebKitMediaStreamSrc* self, GstSample* sample)
{
    if (self->priv->videoSrc)
        self->priv->videoSrc->pushSample(sample);
}

static void webkitMediaStreamSrcPushAudioSample(WebKitMediaStreamSrc* self, const GRefPtr<GstSample>& sample)
{
    if (self->priv->audioSrc)
        self->priv->audioSrc->pushSample(sample.get());
}

static void webkitMediaStreamSrcTrackEnded(WebKitMediaStreamSrc* self, MediaStreamTrackPrivate& track)
{
    GRefPtr<GstPad> pad;

    GST_DEBUG_OBJECT(self, "Track %s ended", track.label().utf8().data());
    GST_OBJECT_LOCK(self);
    GstElement* element = GST_ELEMENT_CAST(self);
    if (element->numpads == 1)
        pad = GST_PAD_CAST(element->srcpads->data);
    else {
        for (auto* item = element->srcpads; item; item = item->next) {
            auto* currentPad = GST_PAD_CAST(item->data);
            auto streamStart = adoptGRef(gst_pad_get_sticky_event(currentPad, GST_EVENT_STREAM_START, 0));
            if (!streamStart)
                continue;

            const char* streamId;
            gst_event_parse_stream_start(streamStart.get(), &streamId);
            if (track.id() == streamId) {
                pad = currentPad;
                break;
            }
        }
    }
    GST_OBJECT_UNLOCK(self);

    if (!pad) {
        GST_ERROR_OBJECT(self, "No pad found for %s", track.id().utf8().data());
        return;
    }

    // Make sure that the video.videoWidth is reset to 0
    webkitMediaStreamSrcPostStreamCollection(self);
    auto tags = mediaStreamTrackPrivateGetTags(&track);
    gst_pad_push_event(pad.get(), gst_event_new_tag(tags.leakRef()));
    gst_pad_push_event(pad.get(), gst_event_new_eos());
}

GstElement* webkitMediaStreamSrcNew()
{
    return GST_ELEMENT_CAST(g_object_new(webkit_media_stream_src_get_type(), nullptr));
}

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
