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

#include "AudioTrackPrivateMediaStream.h"
#include "GStreamerAudioData.h"
#include "GStreamerCommon.h"
#include "MediaSampleGStreamer.h"
#include "MediaStreamPrivate.h"
#include "VideoFrameMetadataGStreamer.h"
#include "VideoTrackPrivateMediaStream.h"

#include <gst/app/gstappsrc.h>
#include <wtf/UUID.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

static GstStaticPadTemplate videoSrcTemplate = GST_STATIC_PAD_TEMPLATE("video_src%u", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("video/x-raw;video/x-h264;video/x-vp8"));

static GstStaticPadTemplate audioSrcTemplate = GST_STATIC_PAD_TEMPLATE("audio_src%u", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("audio/x-raw(ANY);"));

GST_DEBUG_CATEGORY_STATIC(webkitMediaStreamSrcDebug);
#define GST_CAT_DEFAULT webkitMediaStreamSrcDebug

GRefPtr<GstTagList> mediaStreamTrackPrivateGetTags(const MediaStreamTrackPrivate* track)
{
    auto tagList = adoptGRef(gst_tag_list_new_empty());

    if (!track->label().isEmpty())
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, GST_TAG_TITLE, track->label().utf8().data(), nullptr);

    if (track->hasAudio())
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_KIND, static_cast<int>(AudioTrackPrivate::Kind::Main), nullptr);
    else if (track->hasVideo()) {
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_KIND, static_cast<int>(VideoTrackPrivate::Kind::Main), nullptr);

        auto& settings = track->settings();
        if (settings.width())
            gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_WIDTH, settings.width(), nullptr);
        if (settings.height())
            gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_HEIGHT, settings.height(), nullptr);
    }

    GST_DEBUG("Track tags: %" GST_PTR_FORMAT, tagList.get());
    return tagList.leakRef();
}

GstStream* webkitMediaStreamNew(MediaStreamTrackPrivate* track)
{
    GRefPtr<GstCaps> caps;
    GstStreamType type;

    if (track->hasAudio()) {
        caps = adoptGRef(gst_static_pad_template_get_caps(&audioSrcTemplate));
        type = GST_STREAM_TYPE_AUDIO;
    } else {
        RELEASE_ASSERT((track->hasVideo()));
        caps = adoptGRef(gst_static_pad_template_get_caps(&videoSrcTemplate));
        type = GST_STREAM_TYPE_VIDEO;
    }

    auto* stream = gst_stream_new(track->id().utf8().data(), caps.get(), type, GST_STREAM_FLAG_SELECT);
    auto tags = mediaStreamTrackPrivateGetTags(track);
    gst_stream_set_tags(stream, tags.leakRef());
    return stream;
}

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
            webkitMediaStreamSrcAddTrack(WEBKIT_MEDIA_STREAM_SRC_CAST(m_src), &track, false);
    }

    void didRemoveTrack(MediaStreamTrackPrivate&) final;

private:
    GstElement* m_src;
};

class InternalSource final : public MediaStreamTrackPrivate::Observer,
    public RealtimeMediaSource::AudioSampleObserver,
    public RealtimeMediaSource::VideoSampleObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InternalSource(GstElement* parent, MediaStreamTrackPrivate& track, const String& padName)
        : m_parent(parent)
        , m_track(track)
        , m_padName(padName)
    {
        const char* elementName = nullptr;
        if (track.hasAudio()) {
            m_audioTrack = AudioTrackPrivateMediaStream::create(track);
            elementName = "audiosrc";
        } else if (track.hasVideo()) {
            m_videoTrack = VideoTrackPrivateMediaStream::create(track);
            elementName = "videosrc";
        } else
            ASSERT_NOT_REACHED();

        bool isCaptureTrack = track.isCaptureTrack();
        m_src = makeGStreamerElement("appsrc", elementName);

        g_object_set(m_src.get(), "is-live", TRUE, "format", GST_FORMAT_TIME, "emit-signals", TRUE, "min-percent", 100,
            "do-timestamp", isCaptureTrack, nullptr);
        g_signal_connect(m_src.get(), "enough-data", G_CALLBACK(+[](GstElement*, InternalSource* data) {
            data->m_enoughData = true;
        }), this);
        g_signal_connect(m_src.get(), "need-data", G_CALLBACK(+[](GstElement*, unsigned, InternalSource* data) {
            data->m_enoughData = false;
        }), this);
    }

    virtual ~InternalSource()
    {
        stopObserving();

        if (m_src)
            g_signal_handlers_disconnect_matched(m_src.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    }

    const MediaStreamTrackPrivate& track() const { return m_track; }
    const String& padName() const { return m_padName; }
    GstElement* get() const { return m_src.get(); }

    void startObserving()
    {
        if (m_isObserving)
            return;

        GST_DEBUG_OBJECT(m_src.get(), "Starting track/source observation");
        m_track.addObserver(*this);
        if (m_track.hasAudio())
            m_track.source().addAudioSampleObserver(*this);
        else if (m_track.hasVideo())
            m_track.source().addVideoSampleObserver(*this);
        m_isObserving = true;
    }

    void stopObserving()
    {
        if (!m_isObserving)
            return;

        GST_DEBUG_OBJECT(m_src.get(), "Stopping track/source observation");
        m_isObserving = false;
        if (m_track.hasAudio())
            m_track.source().removeAudioSampleObserver(*this);
        else if (m_track.hasVideo())
            m_track.source().removeVideoSampleObserver(*this);
        m_track.removeObserver(*this);
    }

    void configureAudioTrack(float volume, bool isMuted, bool isPlaying)
    {
        ASSERT(m_track.hasAudio());
        m_audioTrack->setVolume(volume);
        m_audioTrack->setMuted(isMuted);
        m_audioTrack->setEnabled(m_audioTrack->streamTrack().enabled() && !m_audioTrack->streamTrack().muted());
        if (isPlaying)
            m_audioTrack->play();
    }

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

        if (m_track.hasVideo() && drop)
            drop = doCapsHaveType(caps, "video/x-raw") || GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

        if (drop) {
            m_needsDiscont = true;
            GST_INFO_OBJECT(m_src.get(), "%s queue full already... not pushing", m_track.hasVideo() ? "Video" : "Audio");
            return;
        }

        if (m_needsDiscont) {
            GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DISCONT);
            m_needsDiscont = false;
        }

        gst_app_src_push_sample(GST_APP_SRC(m_src.get()), sample);
    }

    void trackStarted(MediaStreamTrackPrivate&) final { };
    void trackEnded(MediaStreamTrackPrivate&) final;
    void trackMutedChanged(MediaStreamTrackPrivate&) final { };
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { };
    void readyStateChanged(MediaStreamTrackPrivate&) final { };

    void trackEnabledChanged(MediaStreamTrackPrivate&) final
    {
        GST_INFO_OBJECT(m_src.get(), "Track enabled: %s", boolForPrinting(m_track.enabled()));
        if (m_blackFrame && !m_track.enabled()) {
            m_enoughData = false;
            m_needsDiscont = true;
            gst_element_send_event(m_src.get(), gst_event_new_flush_start());
            gst_element_send_event(m_src.get(), gst_event_new_flush_stop(FALSE));
            pushBlackFrame();
        }
    }

    void videoSampleAvailable(MediaSample& sample, VideoSampleMetadata) final
    {
        if (!m_parent)
            return;

        auto sampleSize = sample.presentationSize();
        IntSize captureSize(sampleSize.width(), sampleSize.height());

        auto settings = m_track.settings();
        m_configuredSize.setWidth(settings.width());
        m_configuredSize.setHeight(settings.height());

        if (!m_configuredSize.width())
            m_configuredSize.setWidth(captureSize.width());
        if (!m_configuredSize.height())
            m_configuredSize.setHeight(captureSize.height());

        auto* mediaSample = static_cast<MediaSampleGStreamer*>(&sample);
        auto videoRotation = sample.videoRotation();
        bool videoMirrored = sample.videoMirrored();
        if (m_videoRotation != videoRotation || m_videoMirrored != videoMirrored) {
            m_videoRotation = videoRotation;
            m_videoMirrored = videoMirrored;

            auto orientation = makeString(videoMirrored ? "flip-" : "", "rotate-", m_videoRotation);
            GST_DEBUG_OBJECT(m_src.get(), "Pushing orientation tag: %s", orientation.utf8().data());
            auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
            gst_pad_push_event(pad.get(), gst_event_new_tag(gst_tag_list_new(GST_TAG_IMAGE_ORIENTATION, orientation.utf8().data(), nullptr)));
        }

        auto* gstSample = mediaSample->platformSample().sample.gstSample;
        if (!m_configuredSize.isEmpty() && m_lastKnownSize != m_configuredSize) {
            m_lastKnownSize = m_configuredSize;
            updateBlackFrame(gst_sample_get_caps(gstSample));
        }

        if (m_track.enabled()) {
            GST_TRACE_OBJECT(m_src.get(), "Pushing video frame from enabled track");
            pushSample(gstSample);
            return;
        }

        pushBlackFrame();
    }

    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData& audioData, const AudioStreamDescription&, size_t) final
    {
        // TODO: We likely need to emit silent audio frames in case the track is disabled.
        if (!m_track.enabled() || !m_parent)
            return;

        const auto& data = static_cast<const GStreamerAudioData&>(audioData);
        auto sample = data.getSample();
        pushSample(sample.get());
    }

private:
    void updateBlackFrame(const GstCaps* sampleCaps)
    {
        GST_DEBUG_OBJECT(m_src.get(), "Updating black video frame");
        auto caps = adoptGRef(gst_caps_copy(sampleCaps));
        gst_caps_set_simple(caps.get(), "format", G_TYPE_STRING, "I420", nullptr);

        GstVideoInfo info;
        gst_video_info_from_caps(&info, caps.get());

        auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&info), nullptr));
        {
            GstMappedBuffer data(buffer, GST_MAP_WRITE);
            auto yOffset = GST_VIDEO_INFO_PLANE_OFFSET(&info, 1);
            memset(data.data(), 0, yOffset);
            memset(data.data() + yOffset, 128, data.size() - yOffset);
        }
        gst_buffer_add_video_meta_full(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_I420, m_lastKnownSize.width(), m_lastKnownSize.height(), 3, info.offset, info.stride);
        m_blackFrame = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    }

    void pushBlackFrame()
    {
        GST_TRACE_OBJECT(m_src.get(), "Pushing black video frame");
        VideoSampleMetadata metadata;
        metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();
        auto* buffer = webkitGstBufferSetVideoSampleMetadata(gst_sample_get_buffer(m_blackFrame.get()), metadata);
        // TODO: Use gst_sample_set_buffer() after bumping GStreamer dependency to 1.16.
        auto* caps = gst_sample_get_caps(m_blackFrame.get());
        m_blackFrame = adoptGRef(gst_sample_new(buffer, caps, nullptr, nullptr));
        pushSample(m_blackFrame.get());
    }

    GstElement* m_parent { nullptr };
    MediaStreamTrackPrivate& m_track;
    GRefPtr<GstElement> m_src;
    GstClockTime m_firstBufferPts { GST_CLOCK_TIME_NONE };
    bool m_enoughData { false };
    bool m_needsDiscont { false };
    String m_padName;
    bool m_isObserving { false };
    RefPtr<AudioTrackPrivateMediaStream> m_audioTrack;
    RefPtr<VideoTrackPrivateMediaStream> m_videoTrack;
    IntSize m_configuredSize;
    IntSize m_lastKnownSize;
    GRefPtr<GstSample> m_blackFrame;
    MediaSample::VideoRotation m_videoRotation { MediaSample::VideoRotation::None };
    bool m_videoMirrored { false };
};

struct _WebKitMediaStreamSrcPrivate {
    CString uri;
    Vector<std::unique_ptr<InternalSource>> sources;
    std::unique_ptr<WebKitMediaStreamObserver> mediaStreamObserver;
    RefPtr<MediaStreamPrivate> stream;
    Vector<RefPtr<MediaStreamTrackPrivate>> tracks;
    GUniquePtr<GstFlowCombiner> flowCombiner;
    GRefPtr<GstStreamCollection> streamCollection;
    Atomic<unsigned> audioPadCounter;
    Atomic<unsigned> videoPadCounter;
};

enum {
    PROP_0,
    PROP_IS_LIVE,
    PROP_LAST
};

static void webkitMediaStreamSrcTrackEnded(WebKitMediaStreamSrc*, InternalSource&);

void WebKitMediaStreamObserver::didRemoveTrack(MediaStreamTrackPrivate& track)
{
    if (!m_src)
        return;

    auto* element = WEBKIT_MEDIA_STREAM_SRC_CAST(m_src);
    auto* priv = element->priv;

    // Lookup the corresponding InternalSource and take it from the storage.
    auto index = priv->sources.findIf([&](auto& item) {
        return item->track().id() == track.id();
    });
    std::unique_ptr<InternalSource> source = WTFMove(priv->sources[index]);
    priv->sources.remove(index);

    // Remove track from internal storage, so that the new stream collection will not reference it.
    priv->tracks.removeFirstMatching([&](auto& item) {
        return item->id() == track.id();
    });

    // Remove corresponding source pad, emit new stream collection.
    webkitMediaStreamSrcTrackEnded(element, *source);
}

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
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC_CAST(handler);
    return g_strdup(self->priv->uri.data());
}

static gboolean webkitMediaStreamSrcUriSetUri(GstURIHandler* handler, const char* uri, GError**)
{
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC_CAST(handler);
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
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC_CAST(object);
    auto* priv = self->priv;

    GST_OBJECT_FLAG_SET(GST_OBJECT_CAST(self), static_cast<GstElementFlags>(GST_ELEMENT_FLAG_SOURCE | static_cast<GstElementFlags>(GST_BIN_FLAG_STREAMS_AWARE)));
    gst_bin_set_suppressed_flags(GST_BIN_CAST(self), static_cast<GstElementFlags>(GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK));

    priv->mediaStreamObserver = makeUnique<WebKitMediaStreamObserver>(GST_ELEMENT_CAST(self));
    priv->flowCombiner = GUniquePtr<GstFlowCombiner>(gst_flow_combiner_new());

    // https://bugs.webkit.org/show_bug.cgi?id=214150
    ASSERT(GST_OBJECT_REFCOUNT(self) == 1);
    ASSERT(g_object_is_floating(self));
}

static void webkitMediaStreamSrcDispose(GObject* object)
{
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC_CAST(object);

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
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC_CAST(element);

    if (transition == GST_STATE_CHANGE_PAUSED_TO_READY) {
        GST_OBJECT_LOCK(self);
        for (auto& item : self->priv->sources)
            item->stopObserving();
        GST_OBJECT_UNLOCK(self);
    }

    GstStateChangeReturn result = GST_ELEMENT_CLASS(webkit_media_stream_src_parent_class)->change_state(element, transition);

    if (transition == GST_STATE_CHANGE_READY_TO_PAUSED) {
        GST_OBJECT_LOCK(self);
        for (auto& item : self->priv->sources)
            item->startObserving();
        GST_OBJECT_UNLOCK(self);
        result = GST_STATE_CHANGE_NO_PREROLL;
    }

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
    auto* self = WEBKIT_MEDIA_STREAM_SRC_CAST(element.get());
    GstFlowReturn result = gst_flow_combiner_update_pad_flow(self->priv->flowCombiner.get(), pad, chainResult);

    if (result == GST_FLOW_FLUSHING)
        return chainResult;

    return result;
}

static void webkitMediaStreamSrcPostStreamCollection(WebKitMediaStreamSrc* self)
{
    auto* priv = self->priv;

    GST_OBJECT_LOCK(self);
    if (priv->stream && (!priv->stream->active() || !priv->stream->hasTracks())) {
        GST_OBJECT_UNLOCK(self);
        return;
    }

    auto upstreamId = priv->stream ? priv->stream->id() : createVersion4UUIDString();
    priv->streamCollection = adoptGRef(gst_stream_collection_new(upstreamId.ascii().data()));
    for (auto& track : priv->tracks) {
        if (!track->isActive())
            continue;
        gst_stream_collection_add_stream(priv->streamCollection.get(), webkitMediaStreamNew(track.get()));
    }

    GST_OBJECT_UNLOCK(self);

    GST_DEBUG_OBJECT(self, "Posting stream collection");
    gst_element_post_message(GST_ELEMENT_CAST(self), gst_message_new_stream_collection(GST_OBJECT_CAST(self), priv->streamCollection.get()));
}

static void webkitMediaStreamSrcAddPad(WebKitMediaStreamSrc* self, GstPad* target, GstStaticPadTemplate* padTemplate, GRefPtr<GstTagList>&& tags, RealtimeMediaSource::Type sourceType, const String& padName)
{
    ASSERT(sourceType != RealtimeMediaSource::Type::None);
    if (sourceType == RealtimeMediaSource::Type::None)
        return;

    GST_DEBUG_OBJECT(self, "%s Ghosting %" GST_PTR_FORMAT, gst_object_get_path_string(GST_OBJECT_CAST(self)), target);

    auto* ghostPad = webkitGstGhostPadFromStaticTemplate(padTemplate, padName.ascii().data(), target);
    gst_pad_set_active(ghostPad, TRUE);
    gst_element_add_pad(GST_ELEMENT_CAST(self), ghostPad);

    auto proxyPad = adoptGRef(GST_PAD_CAST(gst_proxy_pad_get_internal(GST_PROXY_PAD(ghostPad))));
    gst_flow_combiner_add_pad(self->priv->flowCombiner.get(), proxyPad.get());
    gst_pad_set_chain_function(proxyPad.get(), static_cast<GstPadChainFunction>(webkitMediaStreamSrcChain));

    gst_pad_push_event(target, gst_event_new_tag(tags.leakRef()));
}

struct ProbeData {
    ProbeData(GstElement* element, GstStaticPadTemplate* padTemplate, GRefPtr<GstTagList>&& tags, const char* trackId, RealtimeMediaSource::Type sourceType, const String& padName)
        : element(element)
        , padTemplate(padTemplate)
        , tags(WTFMove(tags))
        , trackId(g_strdup(trackId))
        , sourceType(sourceType)
        , padName(padName)
    {
    }

    GRefPtr<GstElement> element;
    GstStaticPadTemplate* padTemplate;
    GRefPtr<GstTagList> tags;
    GUniquePtr<char> trackId;
    RealtimeMediaSource::Type sourceType;
    String padName;
};

static GstPadProbeReturn webkitMediaStreamSrcPadProbeCb(GstPad* pad, GstPadProbeInfo* info, ProbeData* data)
{
    GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC_CAST(data->element.get());

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

        webkitMediaStreamSrcAddPad(self, pad, data->padTemplate, WTFMove(data->tags), data->sourceType, data->padName);
        callOnMainThreadAndWait([element = data->element] {
            webkitMediaStreamSrcPostStreamCollection(WEBKIT_MEDIA_STREAM_SRC_CAST(element.get()));
        });
        return GST_PAD_PROBE_REMOVE;
    }
    default:
        break;
    }

    return GST_PAD_PROBE_OK;
}

void webkitMediaStreamSrcAddTrack(WebKitMediaStreamSrc* self, MediaStreamTrackPrivate* track, bool onlyTrack)
{
    const char* sourceType;
    unsigned counter;
    GstStaticPadTemplate* padTemplate;
    if (track->hasAudio()) {
        padTemplate = &audioSrcTemplate;
        sourceType = "audio";
        counter = self->priv->audioPadCounter.exchangeAdd(1);
    } else {
        RELEASE_ASSERT(track->hasVideo());
        padTemplate = &videoSrcTemplate;
        sourceType = "video";
        counter = self->priv->videoPadCounter.exchangeAdd(1);
    }

    auto padName = makeString(sourceType, "_src", counter);
    GST_DEBUG_OBJECT(self, "Setup %s source for track %s, only track: %s", sourceType, track->id().utf8().data(), boolForPrinting(onlyTrack));

    auto source = makeUnique<InternalSource>(GST_ELEMENT_CAST(self), *track, padName);
    auto* element = source->get();
    gst_bin_add(GST_BIN_CAST(self), element);

    auto pad = adoptGRef(gst_element_get_static_pad(element, "src"));
    auto tags = mediaStreamTrackPrivateGetTags(track);
    if (!onlyTrack) {
        auto* data = new ProbeData(GST_ELEMENT_CAST(self), padTemplate, WTFMove(tags), track->id().utf8().data(), track->source().type(), source->padName());
        gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(webkitMediaStreamSrcPadProbeCb), data, [](gpointer data) {
            delete reinterpret_cast<ProbeData*>(data);
        });
    } else {
        gst_pad_set_active(pad.get(), TRUE);
        webkitMediaStreamSrcAddPad(self, pad.get(), padTemplate, WTFMove(tags), track->source().type(), source->padName());
    }
    gst_element_sync_state_with_parent(element);

    source->startObserving();
    self->priv->sources.append(WTFMove(source));
    self->priv->tracks.append(track);
}

void webkitMediaStreamSrcSetStream(WebKitMediaStreamSrc* self, MediaStreamPrivate* stream, bool isVideoPlayer)
{
    ASSERT(WEBKIT_IS_MEDIA_STREAM_SRC(self));
    ASSERT(!self->priv->stream);
    self->priv->stream = stream;

    self->priv->stream->addObserver(*self->priv->mediaStreamObserver.get());
    auto tracks = stream->tracks();
    bool onlyTrack = tracks.size() == 1;
    for (auto& track : tracks) {
        if (!isVideoPlayer && track->hasVideo())
            continue;
        webkitMediaStreamSrcAddTrack(self, track.get(), onlyTrack);
    }
    webkitMediaStreamSrcPostStreamCollection(self);
}

static void webkitMediaStreamSrcTrackEnded(WebKitMediaStreamSrc* self, InternalSource& source)
{
    GRefPtr<GstPad> pad;

    auto& track = source.track();
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

    GST_STATE_LOCK(GST_ELEMENT_CAST(self));
    gst_element_set_locked_state(source.get(), true);
    gst_element_set_state(source.get(), GST_STATE_NULL);
    gst_bin_remove(GST_BIN_CAST(self), source.get());
    gst_element_set_locked_state(source.get(), false);
    GST_STATE_UNLOCK(GST_ELEMENT_CAST(self));

    auto proxyPad = adoptGRef(GST_PAD_CAST(gst_proxy_pad_get_internal(GST_PROXY_PAD(pad.get()))));
    if (proxyPad)
        gst_flow_combiner_remove_pad(self->priv->flowCombiner.get(), proxyPad.get());

    gst_pad_set_active(pad.get(), FALSE);
    gst_element_remove_pad(GST_ELEMENT_CAST(self), pad.get());
}

void InternalSource::trackEnded(MediaStreamTrackPrivate&)
{
    if (m_parent)
        webkitMediaStreamSrcTrackEnded(WEBKIT_MEDIA_STREAM_SRC_CAST(m_parent), *this);

    stopObserving();
}

void webkitMediaStreamSrcConfigureAudioTracks(WebKitMediaStreamSrc* self, float volume, bool isMuted, bool isPlaying)
{
    for (auto& source : self->priv->sources) {
        if (source->track().hasAudio())
            source->configureAudioTrack(volume, isMuted, isPlaying);
    }
}

GstElement* webkitMediaStreamSrcNew()
{
    return GST_ELEMENT_CAST(g_object_new(webkit_media_stream_src_get_type(), nullptr));
}

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
