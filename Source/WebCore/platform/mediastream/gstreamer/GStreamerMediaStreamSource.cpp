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
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(GSTREAMER_WEBRTC)
#include "RealtimeIncomingAudioSourceGStreamer.h"
#include "RealtimeIncomingVideoSourceGStreamer.h"
#endif

#include <gst/app/gstappsrc.h>
#include <gst/base/gstflowcombiner.h>
#include <wtf/UUID.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/MakeString.h>

using namespace WebCore;

static GstStaticPadTemplate videoSrcTemplate = GST_STATIC_PAD_TEMPLATE("video_src%u", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("video/x-raw;video/x-h264;video/x-vp8;video/x-vp9;video/x-av1"));

static GstStaticPadTemplate audioSrcTemplate = GST_STATIC_PAD_TEMPLATE("audio_src%u", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("audio/x-raw;audio/x-opus;audio/G722;audio/x-alaw;audio/x-mulaw"));

GST_DEBUG_CATEGORY_STATIC(webkitMediaStreamSrcDebug);
#define GST_CAT_DEFAULT webkitMediaStreamSrcDebug

WARN_UNUSED_RETURN GRefPtr<GstTagList> mediaStreamTrackPrivateGetTags(const RefPtr<MediaStreamTrackPrivate>& track)
{
    auto tagList = adoptGRef(gst_tag_list_new_empty());

    if (!track->label().isEmpty())
        gst_tag_list_add(tagList.get(), GST_TAG_MERGE_APPEND, GST_TAG_TITLE, track->label().utf8().data(), nullptr);

    GST_DEBUG("Track tags: %" GST_PTR_FORMAT, tagList.get());
    return tagList;
}

GstStream* webkitMediaStreamNew(const RefPtr<MediaStreamTrackPrivate>& track)
{
    if (!track)
        return nullptr;

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

    StringBuilder builder;
    builder.append(track->id());
    if (!track->enabled())
        builder.append("-disabled"_s);

    auto trackId = builder.toString();
    auto* stream = gst_stream_new(trackId.ascii().data(), caps.get(), type, GST_STREAM_FLAG_SELECT);
    auto tags = mediaStreamTrackPrivateGetTags(track);
    gst_stream_set_tags(stream, tags.get());
    return stream;
}

static void webkitMediaStreamSrcCharacteristicsChanged(WebKitMediaStreamSrc*);

class WebKitMediaStreamObserver : public MediaStreamPrivateObserver {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebKitMediaStreamObserver);
public:
    virtual ~WebKitMediaStreamObserver() { };
    WebKitMediaStreamObserver(GstElement* src)
        : m_src(src) { }

    void characteristicsChanged() final
    {
        if (!m_src)
            return;

        webkitMediaStreamSrcCharacteristicsChanged(WEBKIT_MEDIA_STREAM_SRC_CAST(m_src));
    }
    void activeStatusChanged() final;

    void didAddTrack(MediaStreamTrackPrivate& track) final
    {
        if (m_src)
            webkitMediaStreamSrcAddTrack(WEBKIT_MEDIA_STREAM_SRC_CAST(m_src), &track);
    }

    void didRemoveTrack(MediaStreamTrackPrivate&) final;

private:
    GstElement* m_src;
};

static void webkitMediaStreamSrcEnsureStreamCollectionPosted(WebKitMediaStreamSrc*);


class InternalSource final : public MediaStreamTrackPrivateObserver,
    public RealtimeMediaSourceObserver,
    public RealtimeMediaSource::AudioSampleObserver,
    public RealtimeMediaSource::VideoFrameObserver,
    public CanMakeCheckedPtr<InternalSource> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(InternalSource);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InternalSource);
public:
    InternalSource(GstElement* parent, MediaStreamTrackPrivate& track, const String& padName, bool consumerIsVideoPlayer)
        : m_parent(parent)
        , m_track(&track)
        , m_padName(padName)
        , m_consumerIsVideoPlayer(consumerIsVideoPlayer)
    {
        m_isIncomingVideoSource = m_track->source().isIncomingVideoSource();
        m_isVideoTrack = m_track->isVideo();

        static uint64_t audioCounter = 0;
        static uint64_t videoCounter = 0;
        String elementName;
        if (track.isAudio()) {
            m_audioTrack = AudioTrackPrivateMediaStream::create(track);
            elementName = makeString("audiosrc"_s, audioCounter);
            audioCounter++;
        } else {
            RELEASE_ASSERT(m_isVideoTrack);
            m_videoTrack = VideoTrackPrivateMediaStream::create(track);
            elementName = makeString("videosrc"_s, videoCounter);
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

        createGstStream();

#if GST_CHECK_VERSION(1, 22, 0)
        auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
        gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_QUERY_UPSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad*, GstPadProbeInfo* info, InternalSource*) -> GstPadProbeReturn {
            auto* query = GST_PAD_PROBE_INFO_QUERY(info);
            switch (GST_QUERY_TYPE(query)) {
            case GST_QUERY_SELECTABLE:
                gst_query_set_selectable(query, TRUE);
                return GST_PAD_PROBE_HANDLED;
            default:
                break;
            }
            return GST_PAD_PROBE_OK;
        }), nullptr, nullptr);
#endif
    }

    void replaceTrack(RefPtr<MediaStreamTrackPrivate>&& newTrack)
    {
        ASSERT(m_track);
        ASSERT(m_track->type() == newTrack->type());
        stopObserving();
        m_track = WTFMove(newTrack);
        startObserving();
    }

    void connectIncomingTrack()
    {
#if USE(GSTREAMER_WEBRTC)
        if (!m_track)
            return;
        auto& trackSource = m_track->source();
        int clientId;
        auto client = GRefPtr<GstElement>(m_src);
        if (trackSource.isIncomingAudioSource()) {
            auto& source = static_cast<RealtimeIncomingAudioSourceGStreamer&>(trackSource);
            if (source.hasClient(client)) {
                GST_DEBUG_OBJECT(m_src.get(), "Incoming audio track already registered.");
                return;
            }
            clientId = source.registerClient(WTFMove(client));
        } else {
            RELEASE_ASSERT((trackSource.isIncomingVideoSource()));
            auto& source = static_cast<RealtimeIncomingVideoSourceGStreamer&>(trackSource);
            if (source.hasClient(client)) {
                GST_DEBUG_OBJECT(m_src.get(), "Incoming video track already registered.");
                return;
            }
            clientId = source.registerClient(WTFMove(client));
        }

        m_webrtcSourceClientId = clientId;

        auto incomingSource = static_cast<RealtimeIncomingSourceGStreamer*>(&trackSource);
        auto srcPad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
        gst_pad_add_probe(srcPad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_EVENT_UPSTREAM | GST_PAD_PROBE_TYPE_QUERY_UPSTREAM), reinterpret_cast<GstPadProbeCallback>(+[](GstPad* pad, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
            auto weakSource = static_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(userData);
            auto incomingSource = weakSource->get();
            if (!incomingSource)
                return GST_PAD_PROBE_REMOVE;
            auto src = adoptGRef(gst_pad_get_parent_element(pad));
            if (GST_IS_QUERY(info->data)) {
                switch (GST_QUERY_TYPE(GST_PAD_PROBE_INFO_QUERY(info))) {
                case GST_QUERY_CAPS:
                    return GST_PAD_PROBE_OK;
                default:
                    break;
                }
                GST_DEBUG_OBJECT(src.get(), "Proxying query %" GST_PTR_FORMAT " to appsink peer", GST_PAD_PROBE_INFO_QUERY(info));
            } else
                GST_DEBUG_OBJECT(src.get(), "Proxying event %" GST_PTR_FORMAT " to appsink peer", GST_PAD_PROBE_INFO_EVENT(info));

            if (incomingSource->isIncomingAudioSource()) {
                auto& source = static_cast<RealtimeIncomingAudioSourceGStreamer&>(*incomingSource);
                if (GST_IS_EVENT(info->data))
                    source.handleUpstreamEvent(GRefPtr<GstEvent>(GST_PAD_PROBE_INFO_EVENT(info)));
                else if (source.handleUpstreamQuery(GST_PAD_PROBE_INFO_QUERY(info)))
                    return GST_PAD_PROBE_HANDLED;
            } else if (incomingSource->isIncomingVideoSource()) {
                auto& source = static_cast<RealtimeIncomingVideoSourceGStreamer&>(*incomingSource);
                if (GST_IS_EVENT(info->data))
                    source.handleUpstreamEvent(GRefPtr<GstEvent>(GST_PAD_PROBE_INFO_EVENT(info)));
                else if (source.handleUpstreamQuery(GST_PAD_PROBE_INFO_QUERY(info)))
                    return GST_PAD_PROBE_HANDLED;
            }
            return GST_PAD_PROBE_OK;
        }), new ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer> { incomingSource }, reinterpret_cast<GDestroyNotify>(+[](gpointer data) {
            delete static_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(data);
        }));
#endif
    }

    virtual ~InternalSource()
    {
        stopObserving();

        // Flushing unlocks the basesrc in case its hasn't emitted its first buffer yet.
        flush();

        if (m_src)
            g_signal_handlers_disconnect_matched(m_src.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);

#if USE(GSTREAMER_WEBRTC)
        if (!m_webrtcSourceClientId)
            return;

        if (!m_track)
            return;
        auto& trackSource = m_track->source();
        if (trackSource.isIncomingAudioSource()) {
            auto& source = static_cast<RealtimeIncomingAudioSourceGStreamer&>(trackSource);
            source.unregisterClient(*m_webrtcSourceClientId);
        } else if (trackSource.isIncomingVideoSource()) {
            auto& source = static_cast<RealtimeIncomingVideoSourceGStreamer&>(trackSource);
            source.unregisterClient(*m_webrtcSourceClientId);
        }
#endif
    }

    WARN_UNUSED_RETURN RefPtr<MediaStreamTrackPrivate> track() const { return m_track; }
    const String& padName() const { return m_padName; }
    GstElement* get() const { return m_src.get(); }

    void startObserving()
    {
        if (m_isObserving)
            return;

        if (!m_track)
            return;

        GST_DEBUG_OBJECT(m_src.get(), "Starting observation of track %s", m_track->id().ascii().data());
        m_track->addObserver(*this);
        if (m_track->isAudio())
            m_track->source().addAudioSampleObserver(*this);
        else if (m_track->isVideo())
            m_track->source().addVideoFrameObserver(*this);
        m_isObserving = true;
    }

    void stopObserving()
    {
        if (!m_isObserving)
            return;

        if (!m_track)
            return;

        GST_DEBUG_OBJECT(m_src.get(), "Stopping observation of track %s", m_track->id().ascii().data());
        m_isObserving = false;

        if (m_track->isAudio())
            m_track->source().removeAudioSampleObserver(*this);
        else if (m_track->isVideo())
            m_track->source().removeVideoFrameObserver(*this);
        m_track->removeObserver(*this);
    }

    void configureAudioTrack(float volume, bool isMuted, bool isPlaying)
    {
        if (!m_track)
            return;
        ASSERT(m_track->isAudio());
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

        if (!m_track)
            return;
        trackEnded(*m_track);
    }

    void pushSample(GRefPtr<GstSample>&& sample, [[maybe_unused]] const ASCIILiteral logMessage)
    {
        ASSERT(m_src);
        if (!m_src || !m_isObserving)
            return;

        GST_TRACE_OBJECT(m_src.get(), "%s", logMessage.characters());

        bool drop = m_enoughData;
        auto* buffer = gst_sample_get_buffer(sample.get());
        auto* caps = gst_sample_get_caps(sample.get());
        if (!GST_CLOCK_TIME_IS_VALID(m_firstBufferPts)) {
            m_firstBufferPts = GST_BUFFER_PTS(buffer);
            auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
            gst_pad_set_offset(pad.get(), -m_firstBufferPts);
        }

        if (m_isVideoTrack && drop)
            drop = doCapsHaveType(caps, "video") || GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

        if (drop) {
            m_needsDiscont = true;
            GST_TRACE_OBJECT(m_src.get(), "%s queue full already... not pushing", m_isVideoTrack ? "Video" : "Audio");
            return;
        }

        if (m_needsDiscont) {
            GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DISCONT);
            m_needsDiscont = false;
        }

        gst_app_src_push_sample(GST_APP_SRC(m_src.get()), sample.get());
    }

    void trackStarted(MediaStreamTrackPrivate&) final { };
    void trackMutedChanged(MediaStreamTrackPrivate&) final { };
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { };
    void readyStateChanged(MediaStreamTrackPrivate&) final { };

    void dataFlowStarted(MediaStreamTrackPrivate&) final
    {
        connectIncomingTrack();
    }

    void trackEnded(MediaStreamTrackPrivate&) final
    {
        GST_INFO_OBJECT(m_src.get(), "Track ended");
        sourceStopped();
        m_isEnded = true;
        webkitMediaStreamSrcEnsureStreamCollectionPosted(WEBKIT_MEDIA_STREAM_SRC(m_parent));
    }

    void sourceStopped() final
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

    void trackEnabledChanged(MediaStreamTrackPrivate& track) final
    {
        GST_INFO_OBJECT(m_src.get(), "Track enabled: %s, resetting stream", boolForPrinting(track.enabled()));

        createGstStream();
        webkitMediaStreamSrcEnsureStreamCollectionPosted(WEBKIT_MEDIA_STREAM_SRC(m_parent));

        if (track.isVideo()) {
            m_enoughData = false;
            m_needsDiscont = true;
            if (!track.enabled())
                pushBlackFrame();
            else
                flush();
        }
    }

    void videoFrameAvailable(VideoFrame& videoFrame, VideoFrameTimeMetadata) final
    {
        if (!m_parent || !m_isObserving)
            return;

        auto videoFrameSize = videoFrame.presentationSize();
        IntSize captureSize(videoFrameSize.width(), videoFrameSize.height());

        auto gstVideoFrame = static_cast<VideoFrameGStreamer*>(&videoFrame);
        GRefPtr<GstSample> sample = gstVideoFrame->sample();

        // Video encoders require a multiple of two frame size. At least x264enc does anyway.
        if (!m_consumerIsVideoPlayer && !m_isIncomingVideoSource && (captureSize.width() % 2 || captureSize.height() % 2)) {
            captureSize.setWidth(roundUpToMultipleOf(2, captureSize.width()));
            captureSize.setHeight(roundUpToMultipleOf(2, captureSize.height()));
            sample = gstVideoFrame->resizedSample(captureSize);
        }

        if (!m_track)
            return;

        auto settings = m_track->settings();
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

            auto orientation = makeString(videoMirrored ? "flip-"_s : ""_s, "rotate-"_s, m_videoRotation);
            GST_DEBUG_OBJECT(m_src.get(), "Pushing orientation tag: %s", orientation.utf8().data());
            auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
            gst_pad_push_event(pad.get(), gst_event_new_tag(gst_tag_list_new(GST_TAG_IMAGE_ORIENTATION, orientation.utf8().data(), nullptr)));
        }

        if (!m_configuredSize.isEmpty() && m_lastKnownSize != m_configuredSize) {
            GST_DEBUG_OBJECT(m_src.get(), "Video size changed from %dx%d to %dx%d", m_lastKnownSize.width(), m_lastKnownSize.height(), m_configuredSize.width(), m_configuredSize.height());
            m_lastKnownSize = m_configuredSize;
        }

        if (m_track->enabled()) {
            pushSample(WTFMove(sample), "Pushing video frame from enabled track"_s);
            return;
        }

        pushBlackFrame();
    }

    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData& audioData, const AudioStreamDescription&, size_t) final
    {
        if (!m_parent || !m_isObserving)
            return;

        if (!m_track)
            return;

        const auto& data = static_cast<const GStreamerAudioData&>(audioData);
        if (m_track->enabled()) {
            GRefPtr<GstSample> sample = data.getSample();
            pushSample(WTFMove(sample), "Pushing audio sample from enabled track"_s);
            return;
        }

        pushSilentSample();
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

    GUniquePtr<GstStructure> queryAdditionalStats()
    {
        GUniquePtr<GstStructure> stats;
        auto query = adoptGRef(gst_query_new_custom(GST_QUERY_CUSTOM, gst_structure_new_empty("webkit-video-decoder-stats")));
        auto pad = adoptGRef(gst_element_get_static_pad(m_src.get(), "src"));
        if (gst_pad_peer_query(pad.get(), query.get()))
            stats.reset(gst_structure_copy(gst_query_get_structure(query.get())));

        if (!stats)
            stats.reset(gst_structure_new_empty("webkit-video-decoder-stats"));

        gst_structure_set(stats.get(), "track-identifier", G_TYPE_STRING, m_track->id().utf8().data(), nullptr);
        return stats;
    }

    bool isEnded() const { return m_isEnded; }

    GstStream* stream() const { return m_stream.get(); }

private:
    // CheckedPtr interface
    uint32_t ptrCount() const final { return CanMakeCheckedPtr::ptrCount(); }
    uint32_t ptrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::ptrCountWithoutThreadCheck(); }
    void incrementPtrCount() const final { CanMakeCheckedPtr::incrementPtrCount(); }
    void decrementPtrCount() const final { CanMakeCheckedPtr::decrementPtrCount(); }

    void flush()
    {
        GST_DEBUG_OBJECT(m_src.get(), "Flushing");
        gst_element_send_event(m_src.get(), gst_event_new_flush_start());
        gst_element_send_event(m_src.get(), gst_event_new_flush_stop(FALSE));
    }

    void pushBlackFrame()
    {
        auto width = m_lastKnownSize.width() ? m_lastKnownSize.width() : 320;
        auto height = m_lastKnownSize.height() ? m_lastKnownSize.height() : 240;

        if (!m_track)
            return;

        int frameRateNumerator, frameRateDenominator;
        gst_util_double_to_fraction(m_track->settings().frameRate(), &frameRateNumerator, &frameRateDenominator);

        if (!m_blackFrameCaps)
            m_blackFrameCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "I420", "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));
        else {
            auto* structure = gst_caps_get_structure(m_blackFrameCaps.get(), 0);
            int currentWidth, currentHeight;
            gst_structure_get(structure, "width", G_TYPE_INT, &currentWidth, "height", G_TYPE_INT, &currentHeight, nullptr);
            if (currentWidth != width || currentHeight != height)
                m_blackFrameCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "I420", "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));
        }

        GstVideoInfo info;
        gst_video_info_from_caps(&info, m_blackFrameCaps.get());

        VideoFrameTimeMetadata metadata;
        metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();
        auto buffer = adoptGRef(webkitGstBufferSetVideoFrameTimeMetadata(gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&info), nullptr), metadata));
        {
            GstMappedBuffer data(buffer, GST_MAP_WRITE);
            auto yOffset = GST_VIDEO_INFO_PLANE_OFFSET(&info, 1);
            memset(data.data(), 0, yOffset);
            memset(data.data() + yOffset, 128, data.size() - yOffset);
        }
        gst_buffer_add_video_meta_full(buffer.get(), GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_INFO_FORMAT(&info), GST_VIDEO_INFO_WIDTH(&info),
            GST_VIDEO_INFO_HEIGHT(&info), GST_VIDEO_INFO_N_PLANES(&info), info.offset, info.stride);
        GST_BUFFER_DTS(buffer.get()) = GST_BUFFER_PTS(buffer.get()) = gst_element_get_current_running_time(m_parent);
        auto sample = adoptGRef(gst_sample_new(buffer.get(), m_blackFrameCaps.get(), nullptr, nullptr));
        pushSample(WTFMove(sample), "Pushing black video frame"_s);
    }

    void pushSilentSample()
    {
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
        if (!m_silentSampleCaps) {
            GstAudioInfo info;
            gst_audio_info_set_format(&info, GST_AUDIO_FORMAT_F32LE, 44100, 1, nullptr);
            m_silentSampleCaps = adoptGRef(gst_audio_info_to_caps(&info));
        }

        auto buffer = adoptGRef(gst_buffer_new_and_alloc(512));
        GST_BUFFER_DTS(buffer.get()) = GST_BUFFER_PTS(buffer.get()) = gst_element_get_current_running_time(m_parent);
        GstAudioInfo info;
        gst_audio_info_from_caps(&info, m_silentSampleCaps.get());
        {
            GstMappedBuffer map(buffer.get(), GST_MAP_WRITE);
            webkitGstAudioFormatFillSilence(info.finfo, map.data(), map.size());
        }
        auto sample = adoptGRef(gst_sample_new(buffer.get(), m_silentSampleCaps.get(), nullptr, nullptr));
        pushSample(WTFMove(sample), "Pushing audio silence from disabled track"_s);
    }

    void createGstStream()
    {
        m_stream = adoptGRef(webkitMediaStreamNew(track()));
    }

    GstElement* m_parent { nullptr };
    RefPtr<MediaStreamTrackPrivate> m_track;
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
    GRefPtr<GstCaps> m_blackFrameCaps;
    GRefPtr<GstCaps> m_silentSampleCaps;
    VideoFrame::Rotation m_videoRotation { VideoFrame::Rotation::None };
    bool m_videoMirrored { false };
    bool m_isEnded { false };
    Condition m_eosCondition;
    Lock m_eosLock;
    bool m_eosPending WTF_GUARDED_BY_LOCK(m_eosLock) { false };
    std::optional<int> m_webrtcSourceClientId;
    bool m_consumerIsVideoPlayer { false };
    bool m_isIncomingVideoSource { false };
    GRefPtr<GstStream> m_stream;
    bool m_isVideoTrack { false };
};

struct _WebKitMediaStreamSrcPrivate {
    CString uri;
    Vector<std::unique_ptr<InternalSource>> sources;
    std::unique_ptr<WebKitMediaStreamObserver> mediaStreamObserver;
    RefPtr<MediaStreamPrivate> stream;
    Vector<RefPtr<MediaStreamTrackPrivate>> tracks;
    GUniquePtr<GstFlowCombiner> flowCombiner;
    Atomic<unsigned> audioPadCounter;
    Atomic<unsigned> videoPadCounter;
    unsigned groupId;
};

enum {
    PROP_0,
    PROP_IS_LIVE,
    PROP_LAST
};

void WebKitMediaStreamObserver::activeStatusChanged()
{
    auto element = WEBKIT_MEDIA_STREAM_SRC_CAST(m_src);
    auto isActive = element->priv->stream->active();
    GST_DEBUG_OBJECT(element, "MediaStream active status changed to %s", boolForPrinting(isActive));
    if (isActive)
        return;
    webkitMediaStreamSrcEnsureStreamCollectionPosted(element);
}

void WebKitMediaStreamObserver::didRemoveTrack(MediaStreamTrackPrivate& track)
{
    if (!m_src)
        return;

    auto self = WEBKIT_MEDIA_STREAM_SRC_CAST(m_src);
    auto priv = self->priv;

    // Lookup the corresponding InternalSource and take it from the storage.
    auto index = priv->sources.findIf([&](auto& item) {
        auto itemTrack = item->track();
        if (!itemTrack)
            return true;
        return itemTrack->id() == track.id();
    });
    std::unique_ptr<InternalSource> source = WTFMove(priv->sources[index]);
    priv->sources.remove(index);

    // Remove track from internal storage, so that the new stream collection will not reference it.
    priv->tracks.removeFirstMatching([&](auto& item) -> bool {
        return item->id() == track.id();
    });

    // Properly stop data flow. The source stops observing notifications from WebCore.
    source->signalEndOfStream();

    auto element = GST_ELEMENT_CAST(self);
    {
        auto locker = GstStateLocker(element);
        auto* appSrc = source->get();
        gst_element_set_locked_state(appSrc, true);
        gst_element_set_state(appSrc, GST_STATE_NULL);
        gst_bin_remove(GST_BIN_CAST(self), appSrc);
        gst_element_set_locked_state(appSrc, false);
    }

    auto pad = adoptGRef(gst_element_get_static_pad(element, source->padName().ascii().data()));
    if (auto proxyPad = adoptGRef(GST_PAD_CAST(gst_proxy_pad_get_internal(GST_PROXY_PAD(pad.get())))))
        gst_flow_combiner_remove_pad(priv->flowCombiner.get(), proxyPad.get());

    gst_pad_set_active(pad.get(), FALSE);
    gst_element_remove_pad(element, pad.get());

    // Make sure that the video.videoWidth is reset to 0.
    webkitMediaStreamSrcEnsureStreamCollectionPosted(self);
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
    GST_DEBUG_CATEGORY_INIT(webkitMediaStreamSrcDebug, "webkitmediastreamsrc", 0, "mediastreamsrc element");

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
    priv->groupId = gst_util_group_id_next();

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

        for (auto& source : priv->sources)
            source->stopObserving();

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
    GstStateChangeReturn result;
    bool noPreroll = false;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY: {
        auto locker = GstObjectLocker(self);
        for (auto& item : self->priv->sources)
            item->startObserving();
        break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED: {
        noPreroll = true;
        break;
    }
    default:
        break;
    }

    result = GST_ELEMENT_CLASS(webkit_media_stream_src_parent_class)->change_state(element, transition);
    if (result == GST_STATE_CHANGE_FAILURE) {
        GST_DEBUG_OBJECT(element, "%s : %s", gst_state_change_get_name(transition), gst_element_state_change_return_get_name(result));
        return result;
    }

    switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY: {
        auto locker = GstObjectLocker(self);
        gst_flow_combiner_reset(self->priv->flowCombiner.get());
        break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL: {
        // Explicitely NOT stopping internal sources observation here because the state transition
        // can be triggered from a non-main thread, specially when mediastreamsrc is used by
        // GstTranscoder.
        break;
    }
    default:
        break;
    }

    if (noPreroll && result == GST_STATE_CHANGE_SUCCESS)
        result = GST_STATE_CHANGE_NO_PREROLL;

    GST_DEBUG_OBJECT(element, "%s : %s", gst_state_change_get_name(transition), gst_element_state_change_return_get_name(result));
    return result;
}

static gboolean webkitMediaStreamSrcQuery(GstElement* element, GstQuery* query)
{
    gboolean result = GST_ELEMENT_CLASS(parent_class)->query(element, query);

    if (GST_QUERY_TYPE(query) != GST_QUERY_SCHEDULING)
        return result;

    GstSchedulingFlags flags;
    int minSize, maxSize, align;

    gst_query_parse_scheduling(query, &flags, &minSize, &maxSize, &align);
    gst_query_set_scheduling(query, static_cast<GstSchedulingFlags>(flags | GST_SCHEDULING_FLAG_BANDWIDTH_LIMITED), minSize, maxSize, align);
    return TRUE;
}

static void webkit_media_stream_src_class_init(WebKitMediaStreamSrcClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass* gstElementClass = GST_ELEMENT_CLASS(klass);

    gobjectClass->constructed = webkitMediaStreamSrcConstructed;
    gobjectClass->dispose = webkitMediaStreamSrcDispose;
    gobjectClass->get_property = webkitMediaStreamSrcGetProperty;
    gobjectClass->set_property = webkitMediaStreamSrcSetProperty;

    g_object_class_install_property(gobjectClass, PROP_IS_LIVE, g_param_spec_boolean("is-live", nullptr, nullptr,
        TRUE, static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    gstElementClass->change_state = GST_DEBUG_FUNCPTR(webkitMediaStreamSrcChangeState);

    // In GStreamer 1.20 and older urisourcebin mishandles source elements with dynamic pads. This
    // is not an issue in 1.22.
    if (webkitGstCheckVersion(1, 22, 0))
        gstElementClass->query = GST_DEBUG_FUNCPTR(webkitMediaStreamSrcQuery);

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
        webkitMediaStreamSrcEnsureStreamCollectionPosted(self);

        auto tags = mediaStreamTrackPrivateGetTags(source->track());
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

static GRefPtr<GstStreamCollection> webkitMediaStreamSrcCreateStreamCollection(WebKitMediaStreamSrc* self)
{
    auto priv = self->priv;
    auto locker = GstObjectLocker(self);
    auto upstreamId = priv->stream ? priv->stream->id() : createVersion4UUIDString();
    auto streamCollection = adoptGRef(gst_stream_collection_new(upstreamId.ascii().data()));
    for (auto& source : priv->sources) {
        if (source->isEnded())
            continue;
        GRefPtr<GstStream> stream = source->stream();
        gst_stream_collection_add_stream(streamCollection.get(), stream.leakRef());
    }
    return streamCollection;
}

static void webkitMediaStreamSrcEnsureStreamCollectionPosted(WebKitMediaStreamSrc* self)
{
    GST_DEBUG_OBJECT(self, "Posting stream collection");
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    callOnMainThreadAndWait([element = GRefPtr<GstElement>(GST_ELEMENT_CAST(self))] {
        auto self = WEBKIT_MEDIA_STREAM_SRC_CAST(element.get());
        auto streamCollection = webkitMediaStreamSrcCreateStreamCollection(self);
        GST_DEBUG_OBJECT(self, "Posting stream collection message containing %u streams", gst_stream_collection_get_size(streamCollection.get()));
        gst_element_post_message(element.get(), gst_message_new_stream_collection(GST_OBJECT_CAST(self), streamCollection.get()));
    });
    GST_DEBUG_OBJECT(self, "Stream collection posted");
}

struct ProbeData {
    GRefPtr<GstElement> element;
    GRefPtr<GstTagList> tags;
    RealtimeMediaSource::Type sourceType;
    GRefPtr<GstEvent> streamStartEvent;
    GRefPtr<GstStreamCollection> collection;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ProbeData);

static GstPadProbeReturn webkitMediaStreamSrcPadProbeCb(GstPad* pad, GstPadProbeInfo* info, ProbeData* data)
{
    GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
    [[maybe_unused]] WebKitMediaStreamSrc* self = WEBKIT_MEDIA_STREAM_SRC_CAST(data->element.get());

    GST_DEBUG_OBJECT(self, "Event %" GST_PTR_FORMAT, event);
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_STREAM_START: {
        GST_DEBUG_OBJECT(self, "Replacing stream-start event");
        auto sequenceNumber = gst_event_get_seqnum(event);
        gst_event_unref(event);
        data->streamStartEvent = adoptGRef(gst_event_make_writable(data->streamStartEvent.leakRef()));
        gst_event_set_seqnum(data->streamStartEvent.get(), sequenceNumber);
        info->data = gst_event_ref(data->streamStartEvent.get());
        return GST_PAD_PROBE_OK;
    }
    case GST_EVENT_CAPS: {
        if (data->collection) {
            auto collection = WTFMove(data->collection);
            GST_DEBUG_OBJECT(self, "Pushing stream-collection event");
            gst_pad_push_event(pad, gst_event_new_stream_collection(collection.get()));
            gst_pad_push_event(pad, gst_event_new_tag(data->tags.leakRef()));
            if (data->sourceType == RealtimeMediaSource::Type::Video) {
                GST_DEBUG_OBJECT(self, "Requesting a key-frame");
                gst_pad_send_event(pad, gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, 1));
            }
        }
        return GST_PAD_PROBE_OK;
    }
    default:
        break;
    }

    return GST_PAD_PROBE_OK;
}

void webkitMediaStreamSrcAddTrack(WebKitMediaStreamSrc* self, MediaStreamTrackPrivate* track, bool consumerIsVideoPlayer)
{
    ASCIILiteral sourceType;
    unsigned counter;
    GstStaticPadTemplate* padTemplate;

    if (track->isAudio()) {
        padTemplate = &audioSrcTemplate;
        sourceType = "audio"_s;
        counter = self->priv->audioPadCounter.exchangeAdd(1);
    } else {
        RELEASE_ASSERT(track->isVideo());
        padTemplate = &videoSrcTemplate;
        sourceType = "video"_s;
        counter = self->priv->videoPadCounter.exchangeAdd(1);
    }

    GST_DEBUG_OBJECT(self, "Setup %s source for track %s", sourceType.characters(), track->id().utf8().data());

    auto padName = makeString(sourceType, "_src"_s, counter);
    auto source = makeUnique<InternalSource>(GST_ELEMENT_CAST(self), *track, padName, consumerIsVideoPlayer);
    auto* element = source->get();
    gst_bin_add(GST_BIN_CAST(self), element);

    auto stream = source->stream();
    source->startObserving();
    self->priv->sources.append(WTFMove(source));
    self->priv->tracks.append(track);

    auto pad = adoptGRef(gst_element_get_static_pad(element, "src"));
    auto data = createProbeData();
    data->tags = mediaStreamTrackPrivateGetTags(RefPtr(track));
    data->element = GST_ELEMENT_CAST(self);
    data->sourceType = track->source().type();
    data->collection = webkitMediaStreamSrcCreateStreamCollection(self);
    data->streamStartEvent = adoptGRef(gst_event_new_stream_start(gst_stream_get_stream_id(stream)));
    gst_event_set_group_id(data->streamStartEvent.get(), self->priv->groupId);
    gst_event_set_stream(data->streamStartEvent.get(), stream);

    GRefPtr stickyStreamStartEvent = data->streamStartEvent;

    gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(webkitMediaStreamSrcPadProbeCb),
        data, reinterpret_cast<GDestroyNotify>(destroyProbeData));

#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> objectPath(gst_object_get_path_string(GST_OBJECT_CAST(self)));
    GST_DEBUG_OBJECT(self, "%s Ghosting %" GST_PTR_FORMAT, objectPath.get(), pad.get());
#endif

    auto ghostPad = webkitGstGhostPadFromStaticTemplate(padTemplate, padName.ascii().data(), pad.get());
    gst_pad_store_sticky_event(ghostPad, stickyStreamStartEvent.get());
    gst_pad_set_active(ghostPad, TRUE);
    gst_element_add_pad(GST_ELEMENT_CAST(self), ghostPad);

    auto proxyPad = adoptGRef(GST_PAD_CAST(gst_proxy_pad_get_internal(GST_PROXY_PAD(ghostPad))));
    gst_flow_combiner_add_pad(self->priv->flowCombiner.get(), proxyPad.get());
    gst_pad_set_chain_function(proxyPad.get(), static_cast<GstPadChainFunction>(webkitMediaStreamSrcChain));
    gst_pad_set_event_function(proxyPad.get(), static_cast<GstPadEventFunction>([](GstPad* pad, GstObject* parent, GstEvent* event) {
        switch (GST_EVENT_TYPE(event)) {
        case GST_EVENT_RECONFIGURE: {
            auto self = WEBKIT_MEDIA_STREAM_SRC_CAST(parent);
            auto locker = GstObjectLocker(self);
            gst_flow_combiner_reset(self->priv->flowCombiner.get());
            break;
        }
        default:
            break;
        }
        return gst_pad_event_default(pad, parent, event);
    }));

    gst_pad_set_active(pad.get(), TRUE);
    gst_element_sync_state_with_parent(element);
}

void webkitMediaStreamSrcReplaceTrack(WebKitMediaStreamSrc* self, RefPtr<MediaStreamTrackPrivate>&& newTrack)
{
    ASSERT(newTrack);
    ASSERT(self->priv->sources.size() == 1);
    self->priv->sources[0]->replaceTrack(WTFMove(newTrack));
}

void webkitMediaStreamSrcSignalEndOfStream(WebKitMediaStreamSrc* self)
{
    GST_DEBUG_OBJECT(self, "Signaling EOS");
    for (auto& source : self->priv->sources)
        source->signalEndOfStream();
    self->priv->sources.clear();
}

void webkitMediaStreamSrcCharacteristicsChanged([[maybe_unused]] WebKitMediaStreamSrc* self)
{
    GST_DEBUG_OBJECT(self, "MediaStream characteristics changed");
}

void webkitMediaStreamSrcSetStream(WebKitMediaStreamSrc* self, MediaStreamPrivate* stream, bool isVideoPlayer)
{
    ASSERT(WEBKIT_IS_MEDIA_STREAM_SRC(self));
    ASSERT(!self->priv->stream);
    self->priv->stream = stream;

    GST_DEBUG_OBJECT(self, "Associating with MediaStream");
    self->priv->stream->addObserver(*self->priv->mediaStreamObserver.get());
    auto tracks = stream->tracks();
    for (auto& track : tracks) {
        if (!isVideoPlayer && track->isVideo())
            continue;
        webkitMediaStreamSrcAddTrack(self, track.ptr(), isVideoPlayer);
    }

    // Posting an initial empty stream collection while the element hasn't exposed pads yet triggers
    // a critical warning in urisourcebin.
    if (self->priv->sources.isEmpty())
        return;

    webkitMediaStreamSrcEnsureStreamCollectionPosted(self);
}

void webkitMediaStreamSrcConfigureAudioTracks(WebKitMediaStreamSrc* self, float volume, bool isMuted, bool isPlaying)
{
    for (auto& source : self->priv->sources) {
        auto track = source->track();
        if (UNLIKELY(!track))
            continue;
        if (track->isAudio())
            source->configureAudioTrack(volume, isMuted, isPlaying);
    }
}

GstElement* webkitMediaStreamSrcNew()
{
    return GST_ELEMENT_CAST(g_object_new(webkit_media_stream_src_get_type(), nullptr));
}

#undef GST_CAT_DEFAULT

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
