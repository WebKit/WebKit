/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
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

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GStreamerMediaStreamSource.h"

#include "AudioTrackPrivate.h"
#include "GStreamerAudioData.h"
#include "GStreamerCommon.h"
#include "GStreamerVideoCaptureSource.h"
#include "MediaSampleGStreamer.h"
#include "VideoTrackPrivate.h"

#include <gst/app/gstappsrc.h>
#include <gst/base/gstflowcombiner.h>

#if GST_CHECK_VERSION(1, 10, 0)

namespace WebCore {

static void webkitMediaStreamSrcPushVideoSample(WebKitMediaStreamSrc* self, GstSample* gstsample);
static void webkitMediaStreamSrcPushAudioSample(WebKitMediaStreamSrc* self, GstSample* gstsample);
static void webkitMediaStreamSrcTrackEnded(WebKitMediaStreamSrc* self, MediaStreamTrackPrivate&);
static void webkitMediaStreamSrcRemoveTrackByType(WebKitMediaStreamSrc* self, RealtimeMediaSource::Type trackType);

static GstStaticPadTemplate videoSrcTemplate = GST_STATIC_PAD_TEMPLATE("video_src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("video/x-raw;video/x-h264;video/x-vp8"));

static GstStaticPadTemplate audioSrcTemplate = GST_STATIC_PAD_TEMPLATE("audio_src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("audio/x-raw(ANY);"));

static GstTagList* mediaStreamTrackPrivateGetTags(MediaStreamTrackPrivate* track)
{
    auto taglist = gst_tag_list_new_empty();

    if (!track->label().isEmpty()) {
        gst_tag_list_add(taglist, GST_TAG_MERGE_APPEND,
            GST_TAG_TITLE, track->label().utf8().data(), nullptr);
    }

    if (track->type() == RealtimeMediaSource::Type::Audio) {
        gst_tag_list_add(taglist, GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_KIND,
            static_cast<int>(AudioTrackPrivate::Kind::Main), nullptr);
    } else if (track->type() == RealtimeMediaSource::Type::Video) {
        gst_tag_list_add(taglist, GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_KIND,
            static_cast<int>(VideoTrackPrivate::Kind::Main), nullptr);

        if (track->isCaptureTrack()) {
            GStreamerVideoCaptureSource& source = static_cast<GStreamerVideoCaptureSource&>(
                track->source());

            gst_tag_list_add(taglist, GST_TAG_MERGE_APPEND,
                WEBKIT_MEDIA_TRACK_TAG_WIDTH, source.size().width(),
                WEBKIT_MEDIA_TRACK_TAG_HEIGHT, source.size().height(), nullptr);
        }
    }

    return taglist;
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

    auto gststream = (GstStream*)gst_stream_new(track->id().utf8().data(),
        caps.get(), type, GST_STREAM_FLAG_SELECT);
    auto tags = adoptGRef(mediaStreamTrackPrivateGetTags(track));
    gst_stream_set_tags(gststream, tags.get());

    return gststream;
}

class WebKitMediaStreamTrackObserver
    : public MediaStreamTrackPrivate::Observer {
public:
    virtual ~WebKitMediaStreamTrackObserver() { };
    WebKitMediaStreamTrackObserver(WebKitMediaStreamSrc* src)
        : m_mediaStreamSrc(src) { }
    void trackStarted(MediaStreamTrackPrivate&) final { };

    void trackEnded(MediaStreamTrackPrivate& track) final
    {
        webkitMediaStreamSrcTrackEnded(m_mediaStreamSrc, track);
    }

    void trackMutedChanged(MediaStreamTrackPrivate&) final { };
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { };
    void trackEnabledChanged(MediaStreamTrackPrivate&) final { };
    void readyStateChanged(MediaStreamTrackPrivate&) final { };

    void sampleBufferUpdated(MediaStreamTrackPrivate&, MediaSample& sample) final
    {
        auto gstsample = static_cast<MediaSampleGStreamer*>(&sample)->platformSample().sample.gstSample;

        webkitMediaStreamSrcPushVideoSample(m_mediaStreamSrc, gstsample);
    }

    void audioSamplesAvailable(MediaStreamTrackPrivate&, const MediaTime&, const PlatformAudioData& audioData, const AudioStreamDescription&, size_t) final
    {
        auto audiodata = static_cast<const GStreamerAudioData&>(audioData);

        webkitMediaStreamSrcPushAudioSample(m_mediaStreamSrc, audiodata.getSample());
    }

private:
    WebKitMediaStreamSrc* m_mediaStreamSrc;
};

class WebKitMediaStreamObserver
    : public MediaStreamPrivate::Observer {
public:
    virtual ~WebKitMediaStreamObserver() { };
    WebKitMediaStreamObserver(WebKitMediaStreamSrc* src)
        : m_mediaStreamSrc(src) { }

    void characteristicsChanged() final { GST_DEBUG_OBJECT(m_mediaStreamSrc.get(), "renegotiation should happen"); }
    void activeStatusChanged() final { }

    void didAddTrack(MediaStreamTrackPrivate& track) final
    {
        webkitMediaStreamSrcAddTrack(m_mediaStreamSrc.get(), &track, false);
    }

    void didRemoveTrack(MediaStreamTrackPrivate& track) final
    {
        webkitMediaStreamSrcRemoveTrackByType(m_mediaStreamSrc.get(), track.type());
    }

private:
    GRefPtr<WebKitMediaStreamSrc> m_mediaStreamSrc;
};

typedef struct _WebKitMediaStreamSrcClass WebKitMediaStreamSrcClass;
struct _WebKitMediaStreamSrc {
    GstBin parent_instance;

    gchar* uri;

    GstElement* audioSrc;
    GstClockTime firstAudioBufferPts;
    GstElement* videoSrc;
    GstClockTime firstFramePts;

    std::unique_ptr<WebKitMediaStreamTrackObserver> mediaStreamTrackObserver;
    std::unique_ptr<WebKitMediaStreamObserver> mediaStreamObserver;
    volatile gint npads;
    RefPtr<MediaStreamPrivate> stream;
    RefPtr<MediaStreamTrackPrivate> track;

    GstFlowCombiner* flowCombiner;
    GRefPtr<GstStreamCollection> streamCollection;
};

struct _WebKitMediaStreamSrcClass {
    GstBinClass parent_class;
};

enum {
    PROP_0,
    PROP_IS_LIVE,
    PROP_LAST
};

static GstURIType webkit_media_stream_src_uri_get_type(GType)
{
    return GST_URI_SRC;
}

static const gchar* const* webkit_media_stream_src_uri_get_protocols(GType)
{
    static const gchar* protocols[] = { "mediastream", nullptr };

    return protocols;
}

static gchar* webkit_media_stream_src_uri_get_uri(GstURIHandler* handler)
{
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC(handler);

    /* FIXME: make thread-safe */
    return g_strdup(self->uri);
}

static gboolean webkitMediaStreamSrcUriSetUri(GstURIHandler* handler, const gchar* uri,
    GError**)
{
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC(handler);
    self->uri = g_strdup(uri);

    return TRUE;
}

static void webkitMediaStreamSrcUriHandlerInit(gpointer g_iface, gpointer)
{
    GstURIHandlerInterface* iface = (GstURIHandlerInterface*)g_iface;

    iface->get_type = webkit_media_stream_src_uri_get_type;
    iface->get_protocols = webkit_media_stream_src_uri_get_protocols;
    iface->get_uri = webkit_media_stream_src_uri_get_uri;
    iface->set_uri = webkitMediaStreamSrcUriSetUri;
}

GST_DEBUG_CATEGORY_STATIC(webkitMediaStreamSrcDebug);
#define GST_CAT_DEFAULT webkitMediaStreamSrcDebug

#define doInit                                                                                                                                                              \
    G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, webkitMediaStreamSrcUriHandlerInit);                                                                                    \
    GST_DEBUG_CATEGORY_INIT(webkitMediaStreamSrcDebug, "webkitwebmediastreamsrc", 0, "mediastreamsrc element");                                                           \
    gst_tag_register_static(WEBKIT_MEDIA_TRACK_TAG_WIDTH, GST_TAG_FLAG_META, G_TYPE_INT, "Webkit MediaStream width", "Webkit MediaStream width", gst_tag_merge_use_first);    \
    gst_tag_register_static(WEBKIT_MEDIA_TRACK_TAG_HEIGHT, GST_TAG_FLAG_META, G_TYPE_INT, "Webkit MediaStream height", "Webkit MediaStream height", gst_tag_merge_use_first); \
    gst_tag_register_static(WEBKIT_MEDIA_TRACK_TAG_KIND, GST_TAG_FLAG_META, G_TYPE_INT, "Webkit MediaStream Kind", "Webkit MediaStream Kind", gst_tag_merge_use_first);

G_DEFINE_TYPE_WITH_CODE(WebKitMediaStreamSrc, webkit_media_stream_src, GST_TYPE_BIN, doInit);

static void webkitMediaStreamSrcSetProperty(GObject* object, guint prop_id,
    const GValue*, GParamSpec* pspec)
{
    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void webkitMediaStreamSrcGetProperty(GObject* object, guint prop_id, GValue* value,
    GParamSpec* pspec)
{
    switch (prop_id) {
    case PROP_IS_LIVE:
        g_value_set_boolean(value, TRUE);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void webkitMediaStreamSrcDispose(GObject* object)
{
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC(object);

    if (self->audioSrc) {
        gst_bin_remove(GST_BIN(self), self->audioSrc);
        self->audioSrc = nullptr;
    }

    if (self->videoSrc) {
        gst_bin_remove(GST_BIN(self), self->videoSrc);
        self->videoSrc = nullptr;
    }
}

static void webkitMediaStreamSrcFinalize(GObject* object)
{
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC(object);

    GST_OBJECT_LOCK(self);
    if (self->stream) {
        for (auto& track : self->stream->tracks())
            track->removeObserver(*self->mediaStreamTrackObserver.get());

        self->stream->removeObserver(*self->mediaStreamObserver);
        self->stream = nullptr;
    }
    GST_OBJECT_UNLOCK(self);

    g_clear_pointer(&self->uri, g_free);
    gst_flow_combiner_free(self->flowCombiner);
}

static GstStateChangeReturn webkitMediaStreamSrcChangeState(GstElement* element, GstStateChange transition)
{
    GstStateChangeReturn result;
    auto* self = WEBKIT_MEDIA_STREAM_SRC(element);

    if (transition == GST_STATE_CHANGE_PAUSED_TO_READY) {

        GST_OBJECT_LOCK(self);
        if (self->stream) {
            for (auto& track : self->stream->tracks())
                track->removeObserver(*self->mediaStreamTrackObserver.get());
        } else if (self->track)
            self->track->removeObserver(*self->mediaStreamTrackObserver.get());
        GST_OBJECT_UNLOCK(self);
    }

    result = GST_ELEMENT_CLASS(webkit_media_stream_src_parent_class)->change_state(element, transition);

    if (transition == GST_STATE_CHANGE_READY_TO_PAUSED)
        result = GST_STATE_CHANGE_NO_PREROLL;

    return result;
}

static void webkit_media_stream_src_class_init(WebKitMediaStreamSrcClass* klass)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* gstelement_klass = GST_ELEMENT_CLASS(klass);

    gobject_class->finalize = webkitMediaStreamSrcFinalize;
    gobject_class->dispose = webkitMediaStreamSrcDispose;
    gobject_class->get_property = webkitMediaStreamSrcGetProperty;
    gobject_class->set_property = webkitMediaStreamSrcSetProperty;

    g_object_class_install_property(gobject_class, PROP_IS_LIVE,
        g_param_spec_boolean("is-live", "Is Live",
            "Let playbin3 know we are a live source.",
            TRUE, (GParamFlags)(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    gstelement_klass->change_state = webkitMediaStreamSrcChangeState;
    gst_element_class_add_pad_template(gstelement_klass,
        gst_static_pad_template_get(&videoSrcTemplate));
    gst_element_class_add_pad_template(gstelement_klass,
        gst_static_pad_template_get(&audioSrcTemplate));
}

static void webkit_media_stream_src_init(WebKitMediaStreamSrc* self)
{
    self->mediaStreamTrackObserver = std::make_unique<WebKitMediaStreamTrackObserver>(self);
    self->mediaStreamObserver = std::make_unique<WebKitMediaStreamObserver>(self);
    self->flowCombiner = gst_flow_combiner_new();
    self->firstAudioBufferPts = GST_CLOCK_TIME_NONE;
    self->firstFramePts = GST_CLOCK_TIME_NONE;
}

typedef struct {
    WebKitMediaStreamSrc* self;
    RefPtr<MediaStreamTrackPrivate> track;
    GstStaticPadTemplate* pad_template;
} ProbeData;

static GstFlowReturn webkitMediaStreamSrcChain(GstPad* pad, GstObject* parent, GstBuffer* buffer)
{
    GstFlowReturn result, chain_result;
    GRefPtr<WebKitMediaStreamSrc> self = adoptGRef(WEBKIT_MEDIA_STREAM_SRC(gst_object_get_parent(parent)));

    chain_result = gst_proxy_pad_chain_default(pad, GST_OBJECT(self.get()), buffer);
    result = gst_flow_combiner_update_pad_flow(self.get()->flowCombiner, pad, chain_result);

    if (result == GST_FLOW_FLUSHING)
        return chain_result;

    return result;
}

static void webkitMediaStreamSrcAddPad(WebKitMediaStreamSrc* self, GstPad* target, GstStaticPadTemplate* pad_template)
{
    auto padname = makeString("src_", g_atomic_int_add(&(self->npads), 1));
    auto ghostpad = gst_ghost_pad_new_from_template(padname.utf8().data(), target,
        gst_static_pad_template_get(pad_template));

    GST_DEBUG_OBJECT(self, "%s Ghosting %" GST_PTR_FORMAT,
        gst_object_get_path_string(GST_OBJECT_CAST(self)),
        target);

    auto proxypad = adoptGRef(GST_PAD(gst_proxy_pad_get_internal(GST_PROXY_PAD(ghostpad))));
    gst_pad_set_active(ghostpad, TRUE);
    if (!gst_element_add_pad(GST_ELEMENT(self), GST_PAD(ghostpad))) {
        GST_ERROR_OBJECT(self, "Could not add pad %s:%s", GST_DEBUG_PAD_NAME(ghostpad));
        ASSERT_NOT_REACHED();

        return;
    }

    gst_flow_combiner_add_pad(self->flowCombiner, proxypad.get());
    gst_pad_set_chain_function(proxypad.get(),
        static_cast<GstPadChainFunction>(webkitMediaStreamSrcChain));
}

static GstPadProbeReturn webkitMediaStreamSrcPadProbeCb(GstPad* pad, GstPadProbeInfo* info, ProbeData* data)
{
    GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
    WebKitMediaStreamSrc* self = data->self;

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_STREAM_START: {
        const gchar* stream_id;
        GRefPtr<GstStream> stream = nullptr;

        gst_event_parse_stream_start(event, &stream_id);
        if (!g_strcmp0(stream_id, data->track->id().utf8().data())) {
            GST_INFO_OBJECT(pad, "Event has been sticked already");
            return GST_PAD_PROBE_OK;
        }

        auto stream_start = gst_event_new_stream_start(data->track->id().utf8().data());
        gst_event_set_group_id(stream_start, 1);
        gst_event_unref(event);

        gst_pad_push_event(pad, stream_start);
        gst_pad_push_event(pad, gst_event_new_tag(mediaStreamTrackPrivateGetTags(data->track.get())));

        webkitMediaStreamSrcAddPad(self, pad, data->pad_template);

        return GST_PAD_PROBE_HANDLED;
    }
    default:
        break;
    }

    return GST_PAD_PROBE_OK;
}

static gboolean webkitMediaStreamSrcSetupSrc(WebKitMediaStreamSrc* self,
    MediaStreamTrackPrivate* track, GstElement* element,
    GstStaticPadTemplate* pad_template, gboolean observe_track,
    bool onlyTrack)
{
    auto pad = adoptGRef(gst_element_get_static_pad(element, "src"));

    gst_bin_add(GST_BIN(self), element);

    if (!onlyTrack) {
        ProbeData* data = new ProbeData;
        data->self = WEBKIT_MEDIA_STREAM_SRC(self);
        data->pad_template = pad_template;
        data->track = track;

        gst_pad_add_probe(pad.get(), (GstPadProbeType)GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
            (GstPadProbeCallback)webkitMediaStreamSrcPadProbeCb, data,
            [](gpointer data) {
                delete (ProbeData*)data;
            });
    } else
        webkitMediaStreamSrcAddPad(self, pad.get(), pad_template);

    if (observe_track)
        track->addObserver(*self->mediaStreamTrackObserver.get());

    gst_element_sync_state_with_parent(element);
    return TRUE;
}

static gboolean webkitMediaStreamSrcSetupAppSrc(WebKitMediaStreamSrc* self,
    MediaStreamTrackPrivate* track, GstElement** element,
    GstStaticPadTemplate* pad_template, bool onlyTrack)
{
    *element = gst_element_factory_make("appsrc", nullptr);
    g_object_set(*element, "is-live", true, "format", GST_FORMAT_TIME, nullptr);

    return webkitMediaStreamSrcSetupSrc(self, track, *element, pad_template, TRUE, onlyTrack);
}

static void webkitMediaStreamSrcPostStreamCollection(WebKitMediaStreamSrc* self, MediaStreamPrivate* stream)
{
    GST_OBJECT_LOCK(self);
    self->streamCollection = adoptGRef(gst_stream_collection_new(stream->id().utf8().data()));
    for (auto& track : stream->tracks()) {
        auto gststream = webkitMediaStreamNew(track.get());

        gst_stream_collection_add_stream(self->streamCollection.get(), gststream);
    }
    GST_OBJECT_UNLOCK(self);

    gst_element_post_message(GST_ELEMENT(self),
        gst_message_new_stream_collection(GST_OBJECT(self), self->streamCollection.get()));
}

bool webkitMediaStreamSrcAddTrack(WebKitMediaStreamSrc* self, MediaStreamTrackPrivate* track, bool onlyTrack)
{
    bool res = false;
    if (track->type() == RealtimeMediaSource::Type::Audio)
        res = webkitMediaStreamSrcSetupAppSrc(self, track, &self->audioSrc, &audioSrcTemplate, onlyTrack);
    else if (track->type() == RealtimeMediaSource::Type::Video)
        res = webkitMediaStreamSrcSetupAppSrc(self, track, &self->videoSrc, &videoSrcTemplate, onlyTrack);
    else
        GST_INFO("Unsupported track type: %d", static_cast<int>(track->type()));

    if (onlyTrack && res)
        self->track = track;

    return false;
}

static void webkitMediaStreamSrcRemoveTrackByType(WebKitMediaStreamSrc* self, RealtimeMediaSource::Type trackType)
{
    if (trackType == RealtimeMediaSource::Type::Audio) {
        if (self->audioSrc) {
            gst_element_set_state(self->audioSrc, GST_STATE_NULL);
            gst_bin_remove(GST_BIN(self), self->audioSrc);
            self->audioSrc = nullptr;
        }
    } else if (trackType == RealtimeMediaSource::Type::Video) {
        if (self->videoSrc) {
            gst_element_set_state(self->videoSrc, GST_STATE_NULL);
            gst_bin_remove(GST_BIN(self), self->videoSrc);
            self->videoSrc = nullptr;
        }
    } else
        GST_INFO("Unsupported track type: %d", static_cast<int>(trackType));
}

bool webkitMediaStreamSrcSetStream(WebKitMediaStreamSrc* self, MediaStreamPrivate* stream)
{
    ASSERT(WEBKIT_IS_MEDIA_STREAM_SRC(self));

    webkitMediaStreamSrcRemoveTrackByType(self, RealtimeMediaSource::Type::Audio);
    webkitMediaStreamSrcRemoveTrackByType(self, RealtimeMediaSource::Type::Video);

    webkitMediaStreamSrcPostStreamCollection(self, stream);

    self->stream = stream;
    self->stream->addObserver(*self->mediaStreamObserver.get());
    for (auto& track : stream->tracks())
        webkitMediaStreamSrcAddTrack(self, track.get(), false);

    return TRUE;
}

static void webkitMediaStreamSrcPushVideoSample(WebKitMediaStreamSrc* self, GstSample* gstsample)
{
    if (self->videoSrc) {
        if (!GST_CLOCK_TIME_IS_VALID(self->firstFramePts)) {
            auto buffer = gst_sample_get_buffer(gstsample);

            self->firstFramePts = GST_BUFFER_PTS(buffer);
            auto pad = adoptGRef(gst_element_get_static_pad(self->videoSrc, "src"));
            gst_pad_set_offset(pad.get(), -self->firstFramePts);
        }

        gst_app_src_push_sample(GST_APP_SRC(self->videoSrc), gstsample);
    }
}

static void webkitMediaStreamSrcPushAudioSample(WebKitMediaStreamSrc* self, GstSample* gstsample)
{
    if (self->audioSrc) {
        if (!GST_CLOCK_TIME_IS_VALID(self->firstAudioBufferPts)) {
            auto buffer = gst_sample_get_buffer(gstsample);

            self->firstAudioBufferPts = GST_BUFFER_PTS(buffer);
            auto pad = adoptGRef(gst_element_get_static_pad(self->audioSrc, "src"));
            gst_pad_set_offset(pad.get(), -self->firstAudioBufferPts);
        }
        gst_app_src_push_sample(GST_APP_SRC(self->audioSrc), gstsample);
    }
}

static void webkitMediaStreamSrcTrackEnded(WebKitMediaStreamSrc* self,
    MediaStreamTrackPrivate& track)
{
    GRefPtr<GstPad> pad = nullptr;

    GST_OBJECT_LOCK(self);
    for (auto tmp = GST_ELEMENT(self)->srcpads; tmp; tmp = tmp->next) {
        GstPad* tmppad = GST_PAD(tmp->data);
        const gchar* stream_id;

        GstEvent* stream_start = gst_pad_get_sticky_event(tmppad, GST_EVENT_STREAM_START, 0);
        if (!stream_start)
            continue;

        gst_event_parse_stream_start(stream_start, &stream_id);
        if (String(stream_id) == track.id()) {
            pad = tmppad;
            break;
        }
    }
    GST_OBJECT_UNLOCK(self);

    if (!pad) {
        GST_ERROR_OBJECT(self, "No pad found for %s", track.id().utf8().data());

        return;
    }

    // Make sure that the video.videoWidth is reset to 0
    webkitMediaStreamSrcPostStreamCollection(self, self->stream.get());
    auto tags = mediaStreamTrackPrivateGetTags(&track);
    gst_pad_push_event(pad.get(), gst_event_new_tag(tags));
    gst_pad_push_event(pad.get(), gst_event_new_eos());
}

GstElement* webkitMediaStreamSrcNew(void)
{
    return GST_ELEMENT(g_object_new(webkit_media_stream_src_get_type(), nullptr));
}

} // WebCore
#endif // GST_CHECK_VERSION(1, 10, 0)
#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)
