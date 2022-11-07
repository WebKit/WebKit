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
#include "MediaStreamPrivate.h"
#include "VideoFrameGStreamer.h"
#include "VideoFrameMetadataGStreamer.h"
#include "VideoTrackPrivateMediaStream.h"

#if USE(GSTREAMER_WEBRTC)
#include "RealtimeIncomingAudioSourceGStreamer.h"
#include "RealtimeIncomingVideoSourceGStreamer.h"
#endif

#include <gst/app/gstappsrc.h>
#include <gst/base/gstflowcombiner.h>
#include <wtf/UUID.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

static GstStaticPadTemplate videoSrcTemplate = GST_STATIC_PAD_TEMPLATE("video_src%u", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("video/x-raw;video/x-h264;video/x-vp8;video/x-vp9;application/x-rtp, media=(string)video"));

static GstStaticPadTemplate audioSrcTemplate = GST_STATIC_PAD_TEMPLATE("audio_src%u", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("audio/x-raw(ANY);application/x-rtp, media=(string)audio"));

GST_DEBUG_CATEGORY_STATIC(webkitMediaStreamSrcDebug);
#define GST_CAT_DEFAULT webkitMediaStreamSrcDebug

GRefPtr<GstTagList> mediaStreamTrackPrivateGetTags(const MediaStreamTrackPrivate* track)
{
    auto tagList = adoptGRef(gst_tag_list_new_empty());

    if (!track->label().isEmpty())
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, GST_TAG_TITLE, track->label().utf8().data(), nullptr);

    if (track->isAudio())
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, WEBKIT_MEDIA_TRACK_TAG_KIND, static_cast<int>(AudioTrackPrivate::Kind::Main), nullptr);
    else if (track->isVideo()) {
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

    if (track->isAudio()) {
        caps = adoptGRef(gst_static_pad_template_get_caps(&audioSrcTemplate));
        type = GST_STREAM_TYPE_AUDIO;
    } else {
        RELEASE_ASSERT((track->isVideo()));
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

static void webkitMediaStreamSrcEnsureStreamCollectionPosted(WebKitMediaStreamSrc*);

class InternalSource final : public MediaStreamTrackPrivate::Observer,
    public RealtimeMediaSource::Observer,
    public RealtimeMediaSource::AudioSampleObserver,
    public RealtimeMediaSource::VideoFrameObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InternalSource(GstElement* parent, MediaStreamTrackPrivate& track, const String& padName)
        : m_parent(parent)
        , m_track(track)
        , m_trackEnabled(track.enabled())
        , m_padName(padName)
    {
        static uint64_t audioCounter = 0;
        static uint64_t videoCounter = 0;
        String elementName;
        if (track.isAudio()) {
            m_audioTrack = AudioTrackPrivateMediaStream::create(track);
            elementName = makeString("audiosrc", audioCounter);
            audioCounter++;
        } else {
            RELEASE_ASSERT(track.isVideo());
            m_videoTrack = VideoTrackPrivateMediaStream::create(track);
            elementName = makeString("videosrc", videoCounter);
            videoCounter++;
        }

        bool isCaptureTrack = track.isCaptureTrack();
        m_src = makeGStreamerElement("appsrc", elementName.ascii().data());

        g_object_set(m_src.get(), "is-live", TRUE, "format", GST_FORMAT_TIME, "emit-signals", TRUE, "min-percent", 100,
            "do-timestamp", isCaptureTrack, nullptr);
        g_signal_connect(m_src.get(), "enough-data", G_CALLBACK(+[](GstElement*, InternalSource* data) {
            data->m_enoughData = true;
        }), this);
        g_signal_connect(m_src.get(), "need-data", G_CALLBACK(+[](GstElement*, unsigned, InternalSource* data) {
            data->m_enoughData = false;
        }), this);

#if USE(GSTREAMER_WEBRTC)
        auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
        gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_UPSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad*, GstPadProbeInfo* info, InternalSource* internalSource) -> GstPadProbeReturn {
            auto& trackSource = internalSource->m_track.source();
            if (trackSource.isIncomingAudioSource()) {
                auto& source = static_cast<RealtimeIncomingAudioSourceGStreamer&>(trackSource);
                source.handleUpstreamEvent(GRefPtr<GstEvent>(GST_PAD_PROBE_INFO_EVENT(info)));
            } else if (trackSource.isIncomingVideoSource()) {
                auto& source = static_cast<RealtimeIncomingVideoSourceGStreamer&>(trackSource);
                source.handleUpstreamEvent(GRefPtr<GstEvent>(GST_PAD_PROBE_INFO_EVENT(info)));
            }
            return GST_PAD_PROBE_OK;
        }), this, nullptr);
#endif
    }

    virtual ~InternalSource()
    {
        stopObserving();

        // Flushing unlocks the basesrc in case its hasn't emitted its first buffer yet.
        flush();

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
        if (m_track.isAudio())
            m_track.source().addAudioSampleObserver(*this);
        else if (m_track.isVideo())
            m_track.source().addVideoFrameObserver(*this);
        m_isObserving = true;
    }

    void stopObserving()
    {
        if (!m_isObserving)
            return;

        GST_DEBUG_OBJECT(m_src.get(), "Stopping track/source observation");
        m_isObserving = false;

        if (m_track.isAudio())
            m_track.source().removeAudioSampleObserver(*this);
        else if (m_track.isVideo())
            m_track.source().removeVideoFrameObserver(*this);
        m_track.removeObserver(*this);
    }

    void configureAudioTrack(float volume, bool isMuted, bool isPlaying)
    {
        ASSERT(m_track.isAudio());
        m_audioTrack->setVolume(volume);
        m_audioTrack->setMuted(isMuted);
        m_audioTrack->setEnabled(m_audioTrack->streamTrack().enabled());
        if (isPlaying)
            m_audioTrack->play();
    }

    void signalEndOfStream()
    {
        if (m_src)
            gst_app_src_end_of_stream(GST_APP_SRC(m_src.get()));
        callOnMainThreadAndWait([&] {
            stopObserving();
        });
    }

    void pushSample(GstSample* sample)
    {
        ASSERT(m_src);
        if (!m_src || !m_isObserving)
            return;

        auto* parent = WEBKIT_MEDIA_STREAM_SRC(m_parent);
        webkitMediaStreamSrcEnsureStreamCollectionPosted(parent);

        bool drop = m_enoughData;
        auto* buffer = gst_sample_get_buffer(sample);
        auto* caps = gst_sample_get_caps(sample);
        if (!GST_CLOCK_TIME_IS_VALID(m_firstBufferPts)) {
            m_firstBufferPts = GST_BUFFER_PTS(buffer);
            auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
            gst_pad_set_offset(pad.get(), -m_firstBufferPts);
        }

        if (m_track.isVideo() && drop)
            drop = doCapsHaveType(caps, "video") || GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

        if (drop) {
            m_needsDiscont = true;
            GST_TRACE_OBJECT(m_src.get(), "%s queue full already... not pushing", m_track.isVideo() ? "Video" : "Audio");
            return;
        }

        if (m_needsDiscont) {
            GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DISCONT);
            m_needsDiscont = false;
        }

        gst_app_src_push_sample(GST_APP_SRC(m_src.get()), sample);
    }

    void trackStarted(MediaStreamTrackPrivate&) final { };
    void trackMutedChanged(MediaStreamTrackPrivate&) final { };
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { };
    void readyStateChanged(MediaStreamTrackPrivate&) final { };

    void trackEnded(MediaStreamTrackPrivate&) final
    {
        stopObserving();

        {
            auto locker = GstObjectLocker(m_src.get());
            if (GST_STATE(m_src.get()) < GST_STATE_PAUSED)
                return;
        }

        {
            Locker locker { m_eosLock };
            m_eosPending = true;
            m_eosCondition.waitFor(m_eosLock, 50_ms);
        }
    }

    void trackEnabledChanged(MediaStreamTrackPrivate&) final
    {
        m_trackEnabled.store(m_track.enabled());
        GST_INFO_OBJECT(m_src.get(), "Track enabled: %s", boolForPrinting(m_track.enabled()));
        if (m_track.isVideo()) {
            m_enoughData = false;
            m_needsDiscont = true;
            flush();
            if (!m_track.enabled())
                pushBlackFrame();
        }
    }

    void handleDownstreamEvent(GRefPtr<GstEvent>&& event) final
    {
        auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
        gst_pad_push_event(pad.get(), event.leakRef());
    }

    void videoFrameAvailable(VideoFrame& videoFrame, VideoFrameTimeMetadata) final
    {
        if (!m_parent || !m_isObserving)
            return;

        auto videoFrameSize = videoFrame.presentationSize();
        IntSize captureSize(videoFrameSize.width(), videoFrameSize.height());

        auto settings = m_track.settings();
        m_configuredSize.setWidth(settings.width());
        m_configuredSize.setHeight(settings.height());

        if (!m_configuredSize.width())
            m_configuredSize.setWidth(captureSize.width());
        if (!m_configuredSize.height())
            m_configuredSize.setHeight(captureSize.height());

        auto videoRotation = videoFrame.rotation();
        bool videoMirrored = videoFrame.isMirrored();
        if (m_videoRotation != videoRotation || m_videoMirrored != videoMirrored) {
            m_videoRotation = videoRotation;
            m_videoMirrored = videoMirrored;

            auto orientation = makeString(videoMirrored ? "flip-" : "", "rotate-", m_videoRotation);
            GST_DEBUG_OBJECT(m_src.get(), "Pushing orientation tag: %s", orientation.utf8().data());
            auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
            gst_pad_push_event(pad.get(), gst_event_new_tag(gst_tag_list_new(GST_TAG_IMAGE_ORIENTATION, orientation.utf8().data(), nullptr)));
        }

        auto* gstSample = static_cast<VideoFrameGStreamer*>(&videoFrame)->sample();
        if (!m_configuredSize.isEmpty() && m_lastKnownSize != m_configuredSize) {
            GST_DEBUG_OBJECT(m_src.get(), "Video size changed from %dx%d to %dx%d", m_lastKnownSize.width(), m_lastKnownSize.height(), m_configuredSize.width(), m_configuredSize.height());
            m_lastKnownSize = m_configuredSize;
            updateBlackFrame();
        }

        if (!m_blackFrame)
            updateBlackFrame();

        if (m_trackEnabled.load()) {
            GST_TRACE_OBJECT(m_src.get(), "Pushing video frame from enabled track");
            pushSample(gstSample);
            return;
        }

        pushBlackFrame();
    }

    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData& audioData, const AudioStreamDescription&, size_t) final
    {
        if (!m_parent)
            return;

        const auto& data = static_cast<const GStreamerAudioData&>(audioData);
        auto sample = data.getSample();
        if (m_trackEnabled.load()) {
            GST_TRACE_OBJECT(m_parent, "Pushing audio sample from enabled track");
            pushSample(sample.get());
            return;
        }

        if (!m_silentSample) {
            DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
            auto size = gst_buffer_get_size(gst_sample_get_buffer(sample.get()));
            auto buffer = adoptGRef(gst_buffer_new_and_alloc(size));
            GstMappedBuffer map(buffer.get(), GST_MAP_WRITE);
            GstAudioInfo info;
            gst_audio_info_set_format(&info, GST_AUDIO_FORMAT_F32LE, 44100, 1, nullptr);
            webkitGstAudioFormatFillSilence(info.finfo, map.data(), map.size());
            auto caps = adoptGRef(gst_audio_info_to_caps(&info));
            m_silentSample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
        }

        GST_TRACE_OBJECT(m_parent, "Pushing audio silence from disabled track");
        pushSample(m_silentSample.get());
    }

    Lock* eosLocker() { return &m_eosLock; }
    void notifyEOS()
    {
        assertIsHeld(m_eosLock);
        m_eosPending = false;
        m_eosCondition.notifyAll();
    }

    bool eosPending() const
    {
        assertIsHeld(m_eosLock);
        return m_eosPending;
    }

    std::optional<uint64_t> queryDecodedVideoFramesCount()
    {
        auto query = adoptGRef(gst_query_new_custom(GST_QUERY_CUSTOM, gst_structure_new_empty("webkit-video-decoder-stats")));
        auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
        if (gst_pad_peer_query(pad.get(), query.get())) {
            uint64_t decodedFramesCount;
            if (gst_structure_get_uint64(gst_query_get_structure(query.get()), "decoded-frames", &decodedFramesCount))
                return decodedFramesCount;
        }

        return { };
    }

private:
    void flush()
    {
        GST_DEBUG_OBJECT(m_src.get(), "Flushing");
        gst_element_send_event(m_src.get(), gst_event_new_flush_start());
        gst_element_send_event(m_src.get(), gst_event_new_flush_stop(FALSE));
    }

    void updateBlackFrame()
    {
        GST_DEBUG_OBJECT(m_src.get(), "Updating black video frame");
        auto width = m_lastKnownSize.width() ? m_lastKnownSize.width() : 320;
        auto height = m_lastKnownSize.height() ? m_lastKnownSize.height() : 240;
        auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "I420", "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, nullptr));

        GstVideoInfo info;
        gst_video_info_from_caps(&info, caps.get());

        auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&info), nullptr));
        {
            GstMappedBuffer data(buffer, GST_MAP_WRITE);
            auto yOffset = GST_VIDEO_INFO_PLANE_OFFSET(&info, 1);
            memset(data.data(), 0, yOffset);
            memset(data.data() + yOffset, 128, data.size() - yOffset);
        }
        gst_buffer_add_video_meta_full(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_I420, width, height, 3, info.offset, info.stride);
        m_blackFrame = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    }

    void pushBlackFrame()
    {
        GST_TRACE_OBJECT(m_src.get(), "Pushing black video frame");
        VideoFrameTimeMetadata metadata;
        metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();
        auto* buffer = webkitGstBufferSetVideoFrameTimeMetadata(gst_sample_get_buffer(m_blackFrame.get()), metadata);
        GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer) = gst_element_get_current_running_time(m_parent);
        // TODO: Use gst_sample_set_buffer() after bumping GStreamer dependency to 1.16.
        auto* caps = gst_sample_get_caps(m_blackFrame.get());
        m_blackFrame = adoptGRef(gst_sample_new(buffer, caps, nullptr, nullptr));
        pushSample(m_blackFrame.get());
    }

    GstElement* m_parent { nullptr };
    MediaStreamTrackPrivate& m_track;
    Atomic<bool> m_trackEnabled;
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
    GRefPtr<GstSample> m_silentSample;
    VideoFrame::Rotation m_videoRotation { VideoFrame::Rotation::None };
    bool m_videoMirrored { false };

    Condition m_eosCondition;
    Lock m_eosLock;
    bool m_eosPending WTF_GUARDED_BY_LOCK(m_eosLock) { false };
};

struct _WebKitMediaStreamSrcPrivate {
    CString uri;
    Vector<std::unique_ptr<InternalSource>> sources;
    std::unique_ptr<WebKitMediaStreamObserver> mediaStreamObserver;
    RefPtr<MediaStreamPrivate> stream;
    Vector<RefPtr<MediaStreamTrackPrivate>> tracks;
    GUniquePtr<GstFlowCombiner> flowCombiner;
    GRefPtr<GstStreamCollection> streamCollection;
    Atomic<bool> streamCollectionPosted;
    Atomic<unsigned> audioPadCounter;
    Atomic<unsigned> videoPadCounter;
};

enum {
    PROP_0,
    PROP_IS_LIVE,
    PROP_LAST
};

static void webkitMediaStreamSrcPostStreamCollection(WebKitMediaStreamSrc*);

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

    // Make sure that the video.videoWidth is reset to 0.
    webkitMediaStreamSrcPostStreamCollection(element);

    // Properly stop data flow. The source stops observing notifications from WebCore, so we need to
    // explicitly call trackEnded().
    source->signalEndOfStream();
    source->trackEnded(track);
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
    {
        WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC_CAST(object);
        auto locker = GstObjectLocker(self);
        auto* priv = self->priv;
        if (priv->stream) {
            priv->stream->removeObserver(*priv->mediaStreamObserver);
            priv->stream = nullptr;
        }
    }

    GST_CALL_PARENT(G_OBJECT_CLASS, dispose, (object));
}

static GstStateChangeReturn webkitMediaStreamSrcChangeState(GstElement* element, GstStateChange transition)
{
    GST_DEBUG_OBJECT(element, "%s", gst_state_change_get_name(transition));
    WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC_CAST(element);
    GstStateChangeReturn result = GST_ELEMENT_CLASS(webkit_media_stream_src_parent_class)->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED: {
        auto locker = GstObjectLocker(self);
        for (auto& item : self->priv->sources)
            item->startObserving();
        result = GST_STATE_CHANGE_NO_PREROLL;
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY: {
        auto locker = GstObjectLocker(self);
        gst_flow_combiner_reset(self->priv->flowCombiner.get());
        break;
    }
    default:
        break;
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
    auto element = adoptGRef(GST_ELEMENT_CAST(gst_object_get_parent(parent)));
    auto* self = WEBKIT_MEDIA_STREAM_SRC_CAST(element.get());
    GUniquePtr<char> name(gst_pad_get_name(pad));
    auto padName = String::fromLatin1(name.get());

    for (auto& source : self->priv->sources) {
        if (source->padName() != padName)
            continue;

        Locker locker { *source->eosLocker() };
        if (!source->eosPending())
            continue;

        // Make sure that the video.videoWidth is reset to 0.
        webkitMediaStreamSrcPostStreamCollection(self);
        auto tags = mediaStreamTrackPrivateGetTags(&source->track());
        gst_pad_push_event(pad, gst_event_new_tag(tags.leakRef()));

        {
            auto locker = GstStateLocker(element.get());
            auto* appSrc = source->get();
            gst_element_set_locked_state(appSrc, true);
            gst_element_set_state(appSrc, GST_STATE_NULL);
            gst_bin_remove(GST_BIN_CAST(self), appSrc);
            gst_element_set_locked_state(appSrc, false);
        }

        if (auto proxyPad = adoptGRef(GST_PAD_CAST(gst_proxy_pad_get_internal(GST_PROXY_PAD(pad)))))
            gst_flow_combiner_remove_pad(self->priv->flowCombiner.get(), proxyPad.get());

        gst_pad_set_active(pad, FALSE);
        gst_element_remove_pad(element.get(), pad);
        source->notifyEOS();
        return GST_FLOW_EOS;
    }

    GstFlowReturn chainResult = gst_proxy_pad_chain_default(pad, GST_OBJECT_CAST(element.get()), buffer);
    GstFlowReturn result = gst_flow_combiner_update_pad_flow(self->priv->flowCombiner.get(), pad, chainResult);

    if (result == GST_FLOW_FLUSHING)
        return chainResult;

    return result;
}

static void webkitMediaStreamSrcPostStreamCollection(WebKitMediaStreamSrc* self)
{
    auto* priv = self->priv;

    {
        auto locker = GstObjectLocker(self);
        if (priv->stream && (!priv->stream->active() || !priv->stream->hasTracks()))
            return;

        auto upstreamId = priv->stream ? priv->stream->id() : createVersion4UUIDString();
        priv->streamCollection = adoptGRef(gst_stream_collection_new(upstreamId.ascii().data()));
        for (auto& track : priv->tracks) {
            if (!track->isActive())
                continue;
            gst_stream_collection_add_stream(priv->streamCollection.get(), webkitMediaStreamNew(track.get()));
        }
    }

    GST_DEBUG_OBJECT(self, "Posting stream collection");
    gst_element_post_message(GST_ELEMENT_CAST(self), gst_message_new_stream_collection(GST_OBJECT_CAST(self), priv->streamCollection.get()));
}

static void webkitMediaStreamSrcEnsureStreamCollectionPosted(WebKitMediaStreamSrc* self)
{
    if (self->priv->streamCollectionPosted.load())
        return;

    self->priv->streamCollectionPosted.store(true);
    GST_DEBUG_OBJECT(self, "Posting stream collection");
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    callOnMainThreadAndWait([element = GRefPtr<GstElement>(GST_ELEMENT_CAST(self))] {
        webkitMediaStreamSrcPostStreamCollection(WEBKIT_MEDIA_STREAM_SRC_CAST(element.get()));
    });
    GST_DEBUG_OBJECT(self, "Stream collection posted");
}

static void webkitMediaStreamSrcAddPad(WebKitMediaStreamSrc* self, GstPad* target, GstStaticPadTemplate* padTemplate, GRefPtr<GstTagList>&& tags, const String& padName)
{
    GST_DEBUG_OBJECT(self, "%s Ghosting %" GST_PTR_FORMAT, gst_object_get_path_string(GST_OBJECT_CAST(self)), target);

    auto* ghostPad = webkitGstGhostPadFromStaticTemplate(padTemplate, padName.ascii().data(), target);
    gst_pad_set_active(ghostPad, TRUE);
    gst_element_add_pad(GST_ELEMENT_CAST(self), ghostPad);

    auto proxyPad = adoptGRef(GST_PAD_CAST(gst_proxy_pad_get_internal(GST_PROXY_PAD(ghostPad))));
    gst_flow_combiner_add_pad(self->priv->flowCombiner.get(), proxyPad.get());
    gst_pad_set_chain_function(proxyPad.get(), static_cast<GstPadChainFunction>(webkitMediaStreamSrcChain));
    gst_pad_set_event_function(proxyPad.get(), static_cast<GstPadEventFunction>([](GstPad* pad, GstObject* parent, GstEvent* event) {
        switch (GST_EVENT_TYPE(event)) {
        case GST_EVENT_RECONFIGURE: {
            auto* self = WEBKIT_MEDIA_STREAM_SRC_CAST(parent);
            auto locker = GstObjectLocker(self);
            gst_flow_combiner_reset(self->priv->flowCombiner.get());
            break;
        }
        default:
            break;
        }
        return gst_pad_event_default(pad, parent, event);
    }));

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

        webkitMediaStreamSrcAddPad(self, pad, data->padTemplate, WTFMove(data->tags), data->padName);
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

    if (track->isAudio()) {
        padTemplate = &audioSrcTemplate;
        sourceType = "audio";
        counter = self->priv->audioPadCounter.exchangeAdd(1);
    } else {
        RELEASE_ASSERT(track->isVideo());
        padTemplate = &videoSrcTemplate;
        sourceType = "video";
        counter = self->priv->videoPadCounter.exchangeAdd(1);
    }

#if USE(GSTREAMER_WEBRTC)
    if (track->source().isIncomingAudioSource()) {
        auto& source = static_cast<RealtimeIncomingAudioSourceGStreamer&>(track->source());
        source.registerClient();
    } else if (track->source().isIncomingVideoSource()) {
        auto& source = static_cast<RealtimeIncomingVideoSourceGStreamer&>(track->source());
        source.registerClient();
    }
#endif

    GST_DEBUG_OBJECT(self, "Setup %s source for track %s, only track: %s", sourceType, track->id().utf8().data(), boolForPrinting(onlyTrack));

    auto padName = makeString(sourceType, "_src", counter);
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
        webkitMediaStreamSrcAddPad(self, pad.get(), padTemplate, WTFMove(tags), source->padName());
    }
    gst_element_sync_state_with_parent(element);

    source->startObserving();
    self->priv->sources.append(WTFMove(source));
    self->priv->tracks.append(track);
}

void webkitMediaStreamSrcSignalEndOfStream(WebKitMediaStreamSrc* self)
{
    GST_DEBUG_OBJECT(self, "Signaling EOS");
    for (auto& source : self->priv->sources)
        source->signalEndOfStream();
    self->priv->sources.clear();
}

void webkitMediaStreamSrcSetStream(WebKitMediaStreamSrc* self, MediaStreamPrivate* stream, bool isVideoPlayer)
{
    ASSERT(WEBKIT_IS_MEDIA_STREAM_SRC(self));
    ASSERT(!self->priv->stream);
    self->priv->stream = stream;

    GST_DEBUG_OBJECT(self, "Associating with MediaStream");
    self->priv->stream->addObserver(*self->priv->mediaStreamObserver.get());
    auto tracks = stream->tracks();
    bool onlyTrack = tracks.size() == 1;
    for (auto& track : tracks) {
        if (!isVideoPlayer && track->isVideo())
            continue;
        webkitMediaStreamSrcAddTrack(self, track.ptr(), onlyTrack);
    }
    webkitMediaStreamSrcPostStreamCollection(self);
}

void webkitMediaStreamSrcConfigureAudioTracks(WebKitMediaStreamSrc* self, float volume, bool isMuted, bool isPlaying)
{
    for (auto& source : self->priv->sources) {
        if (source->track().isAudio())
            source->configureAudioTrack(volume, isMuted, isPlaying);
    }
}

GstElement* webkitMediaStreamSrcNew()
{
    return GST_ELEMENT_CAST(g_object_new(webkit_media_stream_src_get_type(), nullptr));
}

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
