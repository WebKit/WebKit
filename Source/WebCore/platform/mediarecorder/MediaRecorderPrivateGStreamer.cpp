/*
 * Copyright (C) 2021, 2022 Igalia S.L
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
#include "MediaRecorderPrivateGStreamer.h"

#if USE(GSTREAMER) && ENABLE(MEDIA_RECORDER)

#include "ContentType.h"
#include "GStreamerCodecUtilities.h"
#include "GStreamerCommon.h"
#include "GStreamerMediaStreamSource.h"
#include "GStreamerRegistryScanner.h"
#include "IntSize.h"
#include "MediaRecorderPrivateOptions.h"
#include "MediaStreamPrivate.h"
#include "VideoEncoderPrivateGStreamer.h"
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaRecorderPrivateBackend);
WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaRecorderPrivateGStreamer);

GST_DEBUG_CATEGORY(webkit_media_recorder_debug);
#define GST_CAT_DEFAULT webkit_media_recorder_debug

std::unique_ptr<MediaRecorderPrivateGStreamer> MediaRecorderPrivateGStreamer::create(MediaStreamPrivate& stream, const MediaRecorderPrivateOptions& options)
{
    ensureGStreamerInitialized();
    registerWebKitGStreamerElements();
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_media_recorder_debug, "webkitmediarecorder", 0, "WebKit MediaStream recorder");
    });

    auto recorder = MediaRecorderPrivateBackend::create(stream, options);
    if (!recorder->preparePipeline())
        return nullptr;

    return makeUnique<MediaRecorderPrivateGStreamer>(recorder.releaseNonNull());
}

MediaRecorderPrivateGStreamer::MediaRecorderPrivateGStreamer(Ref<MediaRecorderPrivateBackend>&& recorder)
    : m_recorder(WTFMove(recorder))
{
    m_recorder->setSelectTracksCallback([this](auto selectedTracks) {
        if (selectedTracks.audioTrack) {
            setAudioSource(&selectedTracks.audioTrack->source());
            checkTrackState(*selectedTracks.audioTrack);
        }
        if (selectedTracks.videoTrack) {
            setVideoSource(&selectedTracks.videoTrack->source());
            checkTrackState(*selectedTracks.videoTrack);
        }
    });
}

void MediaRecorderPrivateGStreamer::startRecording(StartRecordingCallback&& callback)
{
    m_recorder->startRecording(WTFMove(callback));
}

void MediaRecorderPrivateGStreamer::stopRecording(CompletionHandler<void()>&& completionHandler)
{
    m_recorder->stopRecording(WTFMove(completionHandler));
}

void MediaRecorderPrivateGStreamer::fetchData(FetchDataCallback&& completionHandler)
{
    m_recorder->fetchData(WTFMove(completionHandler));
}

void MediaRecorderPrivateGStreamer::pauseRecording(CompletionHandler<void()>&& completionHandler)
{
    m_recorder->pauseRecording(WTFMove(completionHandler));
}

void MediaRecorderPrivateGStreamer::resumeRecording(CompletionHandler<void()>&& completionHandler)
{
    m_recorder->resumeRecording(WTFMove(completionHandler));
}

String MediaRecorderPrivateGStreamer::mimeType() const
{
    return m_recorder->mimeType();
}

bool MediaRecorderPrivateGStreamer::isTypeSupported(const ContentType& contentType)
{
    auto& scanner = GStreamerRegistryScanner::singleton();
    bool isSupported = scanner.isContentTypeSupported(GStreamerRegistryScanner::Configuration::Encoding, contentType, { }, GStreamerRegistryScanner::CaseSensitiveCodecName::No) > MediaPlayerEnums::SupportsType::IsNotSupported;

    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/7670
    if (isSupported && !contentType.containerType().endsWith("mp4"_s) && !webkitGstCheckVersion(1, 24, 9))
        isSupported = false;
    return isSupported;
}

MediaRecorderPrivateBackend::MediaRecorderPrivateBackend(MediaStreamPrivate& stream, const MediaRecorderPrivateOptions& options)
    : m_stream(stream)
    , m_options(options)
    , m_mimeType(options.mimeType)
    , m_positionTimer([&] {
        positionUpdated();
    })
{
    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream);
    GST_DEBUG("Stream topology: hasVideo: %d, hasAudio: %d", selectedTracks.videoTrack != nullptr, selectedTracks.audioTrack != nullptr);
    GST_DEBUG("Original mimeType: \"%s\"", options.mimeType.ascii().data());
    auto contentType = ContentType(options.mimeType);
    auto containerType = contentType.containerType();
    auto codecs = contentType.codecs();
    if (containerType.endsWith("webm"_s)) {
        containerType = selectedTracks.videoTrack ? "video/webm"_s : "audio/webm"_s;
        if (codecs.isEmpty()) {
            if (selectedTracks.videoTrack)
                codecs.append("vp8"_s);
            else
                codecs.append("opus"_s);
        }
    } else {
        containerType = selectedTracks.videoTrack ? "video/mp4"_s : "audio/mp4"_s;
        if (codecs.isEmpty()) {
            if (selectedTracks.videoTrack)
                codecs.append("avc1.4d002a"_s);
            if (selectedTracks.audioTrack)
                codecs.append("mp4a"_s);
        }
    }

    StringBuilder builder;
    builder.append(containerType);
    if (!codecs.isEmpty()) {
        builder.append("; codecs="_s);
        builder.append(interleave(codecs, ","_s));
    }
    m_mimeType = builder.toString();
    GST_DEBUG("New mimeType: \"%s\"", m_mimeType.ascii().data());
}

MediaRecorderPrivateBackend::~MediaRecorderPrivateBackend()
{
    m_selectTracksCallback.reset();
    if (m_src)
        webkitMediaStreamSrcSignalEndOfStream(WEBKIT_MEDIA_STREAM_SRC(m_src.get()));
    m_positionTimer.stop();
    if (!m_pipeline)
        return;
    g_signal_handlers_disconnect_by_data(m_pipeline.get(), this);
    gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
    unregisterPipeline(m_pipeline);
    disconnectSimpleBusMessageCallback(m_pipeline.get());
    m_pipeline.clear();
}

void MediaRecorderPrivateBackend::positionUpdated()
{
    int64_t position;
    if (!gst_element_query_position(m_pipeline.get(), GST_FORMAT_TIME, &position)) {
        GST_LOG_OBJECT(m_pipeline.get(), "Could not query position");
        return;
    }

    Locker locker { m_dataLock };
    m_position = fromGstClockTime(position);
}

void MediaRecorderPrivateBackend::startRecording(MediaRecorderPrivate::StartRecordingCallback&& callback)
{
    if (!m_pipeline)
        preparePipeline();
    GST_DEBUG_OBJECT(m_pipeline.get(), "Starting");
    callback(String(mimeType()), 0, 0);
    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
    m_positionTimer.startRepeating(100_ms);
}

void MediaRecorderPrivateBackend::stopRecording(CompletionHandler<void()>&& completionHandler)
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Stop requested");

    auto scopeExit = makeScopeExit([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });

    m_positionTimer.stop();

    GstState state = GST_STATE_VOID_PENDING;
    gst_element_get_state(m_pipeline.get(), &state, nullptr, GST_CLOCK_TIME_NONE);
    if (state != GST_STATE_VOID_PENDING && state < GST_STATE_PLAYING) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Pipeline is not in playing state, not sending EOS event");
        m_eos = true;
        return;
    }

    if (!webkitMediaStreamSrcHasPrerolled(WEBKIT_MEDIA_STREAM_SRC(m_src.get()))) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Source element hasn't prerolled yet, no need to send EOS event");
        m_eos = true;
        return;
    }

    GST_DEBUG_OBJECT(m_pipeline.get(), "Flushing");
    gst_element_send_event(m_pipeline.get(), gst_event_new_flush_start());
    gst_element_send_event(m_pipeline.get(), gst_event_new_flush_stop(FALSE));

    GST_DEBUG_OBJECT(m_pipeline.get(), "Emitting EOS event(s)");
    if (!gst_element_send_event(m_pipeline.get(), gst_event_new_eos())) {
        GST_WARNING_OBJECT(m_pipeline.get(), "EOS event wasn't handled");
        m_eos = true;
        return;
    }

    if (gst_app_sink_is_eos(GST_APP_SINK(m_sink.get()))) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Sink received EOS already");
        m_eos = true;
        return;
    }

    GST_DEBUG_OBJECT(m_pipeline.get(), "Waiting for EOS event");
    bool isEOS = false;
    unsigned count = 0;
    while (!isEOS) {
        Locker lock(m_eosLock);
        m_eosCondition.waitFor(m_eosLock, 200_ms, [weakThis = ThreadSafeWeakPtr { *this }]() -> bool {
            if (auto protectedThis = weakThis.get())
                return protectedThis->m_eos;
            return true;
        });
        isEOS = m_eos;
        if (count++ >= 10)
            break;
    }
    // FIXME: This workaround should be removed. See also https://bugs.webkit.org/show_bug.cgi?id=293124.
    if (count >= 10) {
        GST_WARNING_OBJECT(m_pipeline.get(), "EOS hasn't reached the sink after 2 seconds of waiting");
        return;
    }
    GST_DEBUG_OBJECT(m_pipeline.get(), "EOS event received on sink");
}

void MediaRecorderPrivateBackend::fetchData(MediaRecorderPrivate::FetchDataCallback&& completionHandler)
{
    callOnMainThread([this, weakThis = ThreadSafeWeakPtr { *this }, completionHandler = WTFMove(completionHandler), mimeType = this->mimeType()]() mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis) {
            completionHandler(SharedBuffer::create(), mimeType, 0);
            return;
        }
        double timeCode = 0;
        RefPtr<FragmentedSharedBuffer> buffer;
        {
            Locker locker { m_dataLock };
            GST_DEBUG_OBJECT(m_pipeline.get(), "Transfering %zu encoded bytes, mimeType: %s", m_data.size(), mimeType.ascii().data());
            buffer = m_data.take();
            timeCode = m_timeCode;
        }
        completionHandler(buffer.releaseNonNull(), mimeType, timeCode);
        {
            Locker locker { m_dataLock };
            if (m_position.isValid())
                m_timeCode = m_position.toDouble();
        }
    });
}

void MediaRecorderPrivateBackend::pauseRecording(CompletionHandler<void()>&& completionHandler)
{
    GST_INFO_OBJECT(m_pipeline.get(), "Pausing");
    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);

    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream());
    if (selectedTracks.audioTrack)
        selectedTracks.audioTrack->setMuted(true);
    if (selectedTracks.videoTrack)
        selectedTracks.videoTrack->setMuted(true);
    completionHandler();
}

void MediaRecorderPrivateBackend::resumeRecording(CompletionHandler<void()>&& completionHandler)
{
    GST_INFO_OBJECT(m_pipeline.get(), "Resuming");
    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream());
    if (selectedTracks.audioTrack)
        selectedTracks.audioTrack->setMuted(false);
    if (selectedTracks.videoTrack)
        selectedTracks.videoTrack->setMuted(false);
    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
    m_positionTimer.startRepeating(100_ms);
    completionHandler();
}

GRefPtr<GstEncodingContainerProfile> MediaRecorderPrivateBackend::containerProfile()
{
    auto selectedTracks = MediaRecorderPrivate::selectTracks(m_stream);
    auto mimeType = this->mimeType();
    if (!mimeType)
        mimeType = selectedTracks.videoTrack ? "video/mp4"_s : "audio/mp4"_s;

    GST_DEBUG("Creating container profile for mime-type %s", mimeType.ascii().data());
    auto contentType = ContentType(mimeType);
    auto& scanner = GStreamerRegistryScanner::singleton();
    if (scanner.isContentTypeSupported(GStreamerRegistryScanner::Configuration::Encoding, contentType, { }) == MediaPlayerEnums::SupportsType::IsNotSupported)
        return nullptr;

    auto mp4Variant = isGStreamerPluginAvailable("fmp4"_s) ? "iso-fragmented"_s : "iso"_s;
    StringBuilder containerCapsDescriptionBuilder;
    auto containerType = contentType.containerType();
    if (containerType.endsWith("mp4"_s))
        containerCapsDescriptionBuilder.append("video/quicktime, variant="_s, mp4Variant);
    else if (containerType.endsWith("webm"_s))
        containerCapsDescriptionBuilder.append(selectedTracks.videoTrack ? "video/webm"_s : "audio/webm"_s);
    else
        containerCapsDescriptionBuilder.append(containerType);

    auto containerCapsDescription = containerCapsDescriptionBuilder.toString();
    auto containerCaps = adoptGRef(gst_caps_from_string(containerCapsDescription.ascii().data()));
    GST_DEBUG("Creating container profile for caps %" GST_PTR_FORMAT, containerCaps.get());
    auto profile = adoptGRef(gst_encoding_container_profile_new(nullptr, nullptr, containerCaps.get(), nullptr));

    if (containerType.endsWith("mp4"_s)) {
        StringBuilder propertiesBuilder;
        propertiesBuilder.append("element-properties-map, map={["_s);
        if (mp4Variant == "iso-fragmented"_s)
            propertiesBuilder.append("isofmp4mux,fragment-duration=100000000,write-mfra=1"_s);
        else {
            GST_WARNING("isofmp4mux (shipped by gst-plugins-rs) is not available, falling back to mp4mux, duration on resulting file will be invalid");
            propertiesBuilder.append("mp4mux,fragment-duration=1000,fragment-mode=0,streamable=0,force-create-timecode-trak=1"_s);
        }
        propertiesBuilder.append("]}"_s);
        auto properties = propertiesBuilder.toString();
        gst_encoding_profile_set_element_properties(GST_ENCODING_PROFILE(profile.get()), gst_structure_from_string(properties.ascii().data(), nullptr));
    }

    auto codecs = contentType.codecs();
    if (selectedTracks.videoTrack) {
        if (codecs.isEmpty()) {
            if (containerType.endsWith("mp4"_s))
                m_videoCodec = "avc1.4d002a"_s;
            else if (containerType.endsWith("webm"_s))
                m_videoCodec = "vp8"_s;
            else {
                GST_ERROR("Unsupported container: %s", containerType.ascii().data());
                return nullptr;
            }
        } else
            m_videoCodec = codecs.first();
        auto [_, videoCaps] = GStreamerCodecUtilities::capsFromCodecString(m_videoCodec, { });
        GST_DEBUG("Creating video encoding profile for caps %" GST_PTR_FORMAT, videoCaps.get());
        gst_encoding_container_profile_add_profile(profile.get(), GST_ENCODING_PROFILE(gst_encoding_video_profile_new(videoCaps.get(), nullptr, nullptr, 1)));
    }

    if (selectedTracks.audioTrack) {
        String audioCapsName;
        if (codecs.contains("vorbis"_s))
            audioCapsName = "audio/x-vorbis"_s;
        else if (codecs.contains("opus"_s))
            audioCapsName = "audio/x-opus"_s;
        else if (codecs.findIf([](auto& codec) { return codec.startsWith("mp4a"_s); }) != notFound)
            audioCapsName = "audio/mpeg, mpegversion=4"_s;
        else if (containerType.endsWith("webm"_s))
            audioCapsName = "audio/x-vorbis"_s;
        else if (containerType.endsWith("mp4"_s))
            audioCapsName = "audio/mpeg, mpegversion=4"_s;
        else {
            GST_WARNING("Audio codec for %s not supported", contentType.raw().utf8().data());
            return nullptr;
        }

        RELEASE_ASSERT(!audioCapsName.isEmpty());
        auto audioCaps = adoptGRef(gst_caps_from_string(audioCapsName.utf8().data()));
        GST_DEBUG("Creating audio encoding profile for caps %" GST_PTR_FORMAT, audioCaps.get());
        m_audioEncodingProfile = adoptGRef(GST_ENCODING_PROFILE(gst_encoding_audio_profile_new(audioCaps.get(), nullptr, nullptr, 1)));

        auto& settings = selectedTracks.audioTrack->settings();
        if (settings.supportsSampleRate()) {
            // opusenc doesn't support the default 44.1 kHz sample rate, so fallback to 48 kHz. This
            // appears to be an unexpected behaviour from the encoding profile "restriction" API.
            // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/4054
            auto sampleRate = audioCapsName == "audio/x-opus"_s ? 48000 : settings.sampleRate();

            auto restrictionCaps = adoptGRef(gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, sampleRate, nullptr));
            GST_DEBUG("Setting audio restriction caps to %" GST_PTR_FORMAT, restrictionCaps.get());
            gst_encoding_profile_set_restriction(m_audioEncodingProfile.get(), restrictionCaps.leakRef());
        }

        gst_encoding_container_profile_add_profile(profile.get(), m_audioEncodingProfile.ref());
    }

    return profile;
}

void MediaRecorderPrivateBackend::setSource(GstElement* element)
{
    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream());
    auto* src = WEBKIT_MEDIA_STREAM_SRC(element);
    if (selectedTracks.audioTrack)
        webkitMediaStreamSrcAddTrack(src, selectedTracks.audioTrack);
    if (selectedTracks.videoTrack)
        webkitMediaStreamSrcAddTrack(src, selectedTracks.videoTrack);
    if (m_selectTracksCallback) {
        auto& callback = *m_selectTracksCallback;
        callback(selectedTracks);
    }
    m_src = element;
}

GstFlowReturn MediaRecorderPrivateBackend::handleSample(GstAppSink* sink, GRefPtr<GstSample>&& sample)
{
    if (sample)
        processSample(WTFMove(sample));

    if (gst_app_sink_is_eos(sink)) {
        notifyEOS();
        return GST_FLOW_EOS;
    }

    return GST_FLOW_OK;
}

void MediaRecorderPrivateBackend::setSink(GstElement* element)
{
    static GstAppSinkCallbacks callbacks = {
        [](GstAppSink*, gpointer userData) {
            auto backend = static_cast<MediaRecorderPrivateBackend*>(userData);
            GST_DEBUG_OBJECT(backend->m_pipeline.get(), "EOS received on sink");
            static_cast<MediaRecorderPrivateBackend*>(userData)->notifyEOS();
        },
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto sample = adoptGRef(gst_app_sink_pull_preroll(sink));
            return static_cast<MediaRecorderPrivateBackend*>(userData)->handleSample(sink, WTFMove(sample));
        },
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto sample = adoptGRef(gst_app_sink_pull_sample(sink));
            return static_cast<MediaRecorderPrivateBackend*>(userData)->handleSample(sink, WTFMove(sample));
        },
        // new_event
        nullptr,
#if GST_CHECK_VERSION(1, 24, 0)
        // propose_allocation
        nullptr,
#endif
        { nullptr }
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(element), &callbacks, this, nullptr);
    g_object_set(element, "enable-last-sample", FALSE, "max-buffers", 1, "async", FALSE, nullptr);
    m_sink = element;
}

void MediaRecorderPrivateBackend::configureAudioEncoder(GstElement* element)
{
    if (!gstObjectHasProperty(element, "bitrate"_s)) {
        GST_WARNING_OBJECT(m_pipeline.get(), "Audio encoder %" GST_PTR_FORMAT " has no bitrate property, skipping configuration", element);
        return;
    }

    int bitRate = 0;
    if (m_options.audioBitsPerSecond)
        bitRate = *m_options.audioBitsPerSecond;
    else if (m_options.bitsPerSecond)
        bitRate = *m_options.bitsPerSecond;

    if (bitRate)
        g_object_set(element, "bitrate", bitRate, nullptr);
}

void MediaRecorderPrivateBackend::configureVideoEncoder(GstElement* element)
{
    auto encoder = WEBKIT_VIDEO_ENCODER(element);
    videoEncoderSetCodec(encoder, m_videoCodec, { }, { }, true);

    auto bitrate = [options = m_options]() -> unsigned {
        if (options.videoBitsPerSecond)
            return *options.videoBitsPerSecond;
        if (options.bitsPerSecond)
            return *options.bitsPerSecond;
        return 0;
    }();

    if (bitrate)
        g_object_set(element, "bitrate", bitrate / 1024, nullptr);
}

bool MediaRecorderPrivateBackend::preparePipeline()
{
    auto profile = containerProfile();
    if (!profile)
        return false;

    static uint32_t nPipeline = 0;
    auto pipelineName = makeString("media-recorder-"_s, nPipeline++);
    m_pipeline = makeGStreamerElement("uritranscodebin"_s, pipelineName);
    if (!m_pipeline)
        return false;

    auto clock = adoptGRef(gst_system_clock_obtain());
    gst_pipeline_use_clock(GST_PIPELINE(m_pipeline.get()), clock.get());
    gst_element_set_base_time(m_pipeline.get(), 0);
    gst_element_set_start_time(m_pipeline.get(), GST_CLOCK_TIME_NONE);

    registerActivePipeline(m_pipeline);
    connectSimpleBusMessageCallback(m_pipeline.get(), [recorder = ThreadSafeWeakPtr { *this }](auto message) mutable {
        if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_EOS)
            return;
        RefPtr self = recorder.get();
        if (!self)
            return;
        self->notifyEOS();
    });

    g_signal_connect_swapped(m_pipeline.get(), "source-setup", G_CALLBACK(+[](MediaRecorderPrivateBackend* recorder, GstElement* sourceElement) {
        recorder->setSource(sourceElement);
    }), this);

    g_signal_connect_swapped(m_pipeline.get(), "element-setup", G_CALLBACK(+[](MediaRecorderPrivateBackend* recorder, GstElement* element) {
        if (WEBKIT_IS_VIDEO_ENCODER(element)) {
            recorder->configureVideoEncoder(element);
            return;
        }

        if (GST_IS_APP_SINK(element)) {
            recorder->setSink(element);
            return;
        }

        String elementClass = unsafeSpan(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (classifiers.contains("Audio"_s) && classifiers.contains("Codec"_s) && classifiers.contains("Encoder"_s))
            recorder->configureAudioEncoder(element);
    }), this);

    g_object_set(m_pipeline.get(), "source-uri", "mediastream://", "dest-uri", "appsink://", "profile", profile.get(), "avoid-reencoding", TRUE, nullptr);
    return true;
}

void MediaRecorderPrivateBackend::processSample(GRefPtr<GstSample>&& sample)
{
    auto* sampleBuffer = gst_sample_get_buffer(sample.get());
    GstMappedBuffer buffer(sampleBuffer, GST_MAP_READ);
    Locker locker { m_dataLock };

    GST_LOG_OBJECT(m_pipeline.get(), "Queueing %zu bytes of encoded data, caps: %" GST_PTR_FORMAT, buffer.size(), gst_sample_get_caps(sample.get()));
    m_data.append(buffer.span<uint8_t>());
}

void MediaRecorderPrivateBackend::notifyEOS()
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "EOS received");
    Locker lock(m_eosLock);
    m_eos = true;
    m_eosCondition.notifyAll();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER) && ENABLE(MEDIA_RECORDER)
