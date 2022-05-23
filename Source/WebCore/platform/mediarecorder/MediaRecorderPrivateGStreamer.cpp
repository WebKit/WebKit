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

#if USE(GSTREAMER_TRANSCODER)

#include "ContentType.h"
#include "GStreamerCommon.h"
#include "GStreamerMediaStreamSource.h"
#include "GStreamerRegistryScanner.h"
#include "GStreamerVideoEncoder.h"
#include "MediaRecorderPrivateOptions.h"
#include "MediaStreamPrivate.h"
#include <gst/app/gstappsink.h>
#include <wtf/Scope.h>

namespace WebCore {

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

    auto recorder = makeUnique<MediaRecorderPrivateGStreamer>(stream, options);
    if (!recorder->preparePipeline())
        return nullptr;

    return recorder;
}

MediaRecorderPrivateGStreamer::MediaRecorderPrivateGStreamer(MediaStreamPrivate& stream, const MediaRecorderPrivateOptions& options)
    : m_stream(stream)
    , m_options(options)
{
}

MediaRecorderPrivateGStreamer::~MediaRecorderPrivateGStreamer()
{
    if (m_src)
        webkitMediaStreamSrcSignalEndOfStream(WEBKIT_MEDIA_STREAM_SRC(m_src.get()));
    if (m_transcoder) {
        m_pipeline.clear();
        m_transcoder.clear();
    }
}

void MediaRecorderPrivateGStreamer::startRecording(StartRecordingCallback&& callback)
{
    if (!m_pipeline)
        preparePipeline();
    GST_DEBUG_OBJECT(m_transcoder.get(), "Starting");
    callback(String(mimeType()), 0, 0);
    gst_transcoder_run_async(m_transcoder.get());
}

void MediaRecorderPrivateGStreamer::stopRecording(CompletionHandler<void()>&& completionHandler)
{
    GST_DEBUG_OBJECT(m_transcoder.get(), "Stop requested, pushing EOS event");

    auto scopeExit = makeScopeExit([this, completionHandler = WTFMove(completionHandler)]() mutable {
        m_pipeline.clear();
        GST_DEBUG_OBJECT(m_transcoder.get(), "Stopping");
        m_transcoder.clear();
        completionHandler();
    });

    if (!m_position) {
        GST_DEBUG_OBJECT(m_transcoder.get(), "Transcoder has not started yet, no need for EOS event");
        m_eos = true;
        return;
    }
    webkitMediaStreamSrcSignalEndOfStream(WEBKIT_MEDIA_STREAM_SRC(m_src.get()));

    bool isEOS = false;
    while (!isEOS) {
        LockHolder lock(m_eosLock);
        m_eosCondition.waitFor(m_eosLock, 200_ms, [weakThis = WeakPtr { this }]() -> bool {
            if (!weakThis)
                return true;
            return weakThis->m_eos;
        });
        isEOS = m_eos;
    }
}

void MediaRecorderPrivateGStreamer::fetchData(FetchDataCallback&& completionHandler)
{
    Locker locker { m_dataLock };
    GST_DEBUG_OBJECT(m_transcoder.get(), "Transfering %zu encoded bytes", m_data.size());
    auto buffer = m_data.take();
    completionHandler(WTFMove(buffer), mimeType(), m_position);
}

void MediaRecorderPrivateGStreamer::pauseRecording(CompletionHandler<void()>&& completionHandler)
{
    GST_INFO_OBJECT(m_transcoder.get(), "Pausing");
    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);

    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream());
    if (selectedTracks.audioTrack)
        selectedTracks.audioTrack->setMuted(true);
    if (selectedTracks.videoTrack)
        selectedTracks.videoTrack->setMuted(true);
    completionHandler();
}

void MediaRecorderPrivateGStreamer::resumeRecording(CompletionHandler<void()>&& completionHandler)
{
    GST_INFO_OBJECT(m_transcoder.get(), "Resuming");
    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream());
    if (selectedTracks.audioTrack)
        selectedTracks.audioTrack->setMuted(false);
    if (selectedTracks.videoTrack)
        selectedTracks.videoTrack->setMuted(false);
    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
    completionHandler();
}

const String& MediaRecorderPrivateGStreamer::mimeType() const
{
    static NeverDestroyed<const String> MP4AUDIOMIMETYPE(MAKE_STATIC_STRING_IMPL("audio/mp4"));
    static NeverDestroyed<const String> MP4VIDEOMIMETYPE(MAKE_STATIC_STRING_IMPL("video/mp4"));

    auto selectedTracks = MediaRecorderPrivate::selectTracks(m_stream);
    return selectedTracks.videoTrack ? MP4VIDEOMIMETYPE : MP4AUDIOMIMETYPE;
}

bool MediaRecorderPrivateGStreamer::isTypeSupported(const ContentType& contentType)
{
    auto& scanner = GStreamerRegistryScanner::singleton();
    return scanner.isContentTypeSupported(GStreamerRegistryScanner::Configuration::Encoding, contentType, { }) > MediaPlayerEnums::SupportsType::IsNotSupported;
}

GRefPtr<GstEncodingContainerProfile> MediaRecorderPrivateGStreamer::containerProfile()
{
    auto selectedTracks = MediaRecorderPrivate::selectTracks(m_stream);
    auto mimeType = this->mimeType();
    if (!mimeType)
        mimeType = selectedTracks.videoTrack ? "video/mp4"_s : "audio/mp4"_s;

    GST_DEBUG("Creating video profile for mime-type %s", mimeType.ascii().data());
    auto contentType = ContentType(mimeType);
    auto& scanner = GStreamerRegistryScanner::singleton();
    if (scanner.isContentTypeSupported(GStreamerRegistryScanner::Configuration::Encoding, contentType, { }) == MediaPlayerEnums::SupportsType::IsNotSupported)
        return nullptr;

    const char* containerCapsDescription = "";
    auto containerType = contentType.containerType();
    if (containerType.endsWith("mp4"_s))
        containerCapsDescription = "video/quicktime, variant=iso";
    else
        containerCapsDescription = containerType.utf8().data();

    auto containerCaps = adoptGRef(gst_caps_from_string(containerCapsDescription));
    auto profile = adoptGRef(gst_encoding_container_profile_new(nullptr, nullptr, containerCaps.get(), nullptr));

    if (containerType.endsWith("mp4"_s))
        gst_encoding_profile_set_element_properties(GST_ENCODING_PROFILE(profile.get()), gst_structure_from_string("element-properties-map, map={[mp4mux,fragment-duration=1000,fragment-mode=0,streamable=0,force-create-timecode-trak=1]}", nullptr));

    auto codecs = contentType.codecs();
    // TODO: Handle profiles.
    if (selectedTracks.videoTrack) {
        String videoCapsName;
        if (codecs.contains("vp8"_s))
            videoCapsName = "video/x-vp8"_s;
        else if (codecs.contains("vp9"_s))
            videoCapsName = "video/x-vp9"_s;
        else if (codecs.contains("av1"_s))
            videoCapsName = "video/x-av1"_s;
        else if (codecs.findIf([](auto& codec) { return codec.startsWith("avc1"_s); }) != notFound)
            videoCapsName = "video/x-h264"_s;
        else if (containerType.endsWith("webm"_s))
            videoCapsName = "video/x-vp8"_s;
        else if (containerType.endsWith("mp4"_s))
            videoCapsName = "video/x-h264"_s;
        else {
            GST_WARNING("Video codec for %s not supported", contentType.raw().utf8().data());
            return nullptr;
        }

        RELEASE_ASSERT(!videoCapsName.isEmpty());
        auto videoCaps = adoptGRef(gst_caps_new_empty_simple(videoCapsName.utf8().data()));
        GST_DEBUG("Creating video encoding profile for caps %" GST_PTR_FORMAT, videoCaps.get());
        m_videoEncodingProfile = adoptGRef(GST_ENCODING_PROFILE(gst_encoding_video_profile_new(videoCaps.get(), nullptr, nullptr, 1)));
        gst_encoding_container_profile_add_profile(profile.get(), m_videoEncodingProfile.get());
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
        gst_encoding_container_profile_add_profile(profile.get(), m_audioEncodingProfile.get());
    }

    return profile;
}

void MediaRecorderPrivateGStreamer::setSource(GstElement* element)
{
    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream());
    bool onlyTrack = (selectedTracks.audioTrack && !selectedTracks.videoTrack) || (selectedTracks.videoTrack && !selectedTracks.audioTrack);
    auto* src = WEBKIT_MEDIA_STREAM_SRC(element);
    if (selectedTracks.audioTrack) {
        webkitMediaStreamSrcAddTrack(src, selectedTracks.audioTrack, onlyTrack);
        setAudioSource(&selectedTracks.audioTrack->source());
        checkTrackState(*selectedTracks.audioTrack);
    }
    if (selectedTracks.videoTrack) {
        webkitMediaStreamSrcAddTrack(src, selectedTracks.videoTrack, onlyTrack);
        setVideoSource(&selectedTracks.videoTrack->source());
        checkTrackState(*selectedTracks.videoTrack);
    }
    m_src = element;
}

void MediaRecorderPrivateGStreamer::setSink(GstElement* element)
{
    static GstAppSinkCallbacks callbacks = {
        nullptr,
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto sample = adoptGRef(gst_app_sink_pull_preroll(sink));
            if (sample)
                static_cast<MediaRecorderPrivateGStreamer*>(userData)->processSample(WTFMove(sample));
            return gst_app_sink_is_eos(sink) ? GST_FLOW_EOS : GST_FLOW_OK;
        },
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto sample = adoptGRef(gst_app_sink_pull_sample(sink));
            if (sample)
                static_cast<MediaRecorderPrivateGStreamer*>(userData)->processSample(WTFMove(sample));
            return gst_app_sink_is_eos(sink) ? GST_FLOW_EOS : GST_FLOW_OK;
        },
        // new_event
        nullptr,
        { nullptr }
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(element), &callbacks, this, nullptr);
    g_object_set(element, "enable-last-sample", FALSE, "max-buffers", 1, "async", FALSE, nullptr);
    m_sink = element;
}

void MediaRecorderPrivateGStreamer::configureVideoEncoder(GstElement* element)
{
    auto format = adoptGRef(gst_encoding_profile_get_format(GST_ENCODING_PROFILE(m_videoEncodingProfile.get())));
    g_object_set(element, "format", format.get(), nullptr);

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

bool MediaRecorderPrivateGStreamer::preparePipeline()
{
    auto profile = containerProfile();
    if (!profile)
        return false;

    m_transcoder = adoptGRef(gst_transcoder_new_full("mediastream://", "appsink://", GST_ENCODING_PROFILE(profile.get())));
    gst_transcoder_set_avoid_reencoding(m_transcoder.get(), true);
    m_pipeline = gst_transcoder_get_pipeline(m_transcoder.get());

    g_signal_connect_swapped(m_pipeline.get(), "source-setup", G_CALLBACK(+[](MediaRecorderPrivateGStreamer* recorder, GstElement* sourceElement) {
        recorder->setSource(sourceElement);
    }), this);

    g_signal_connect_swapped(m_pipeline.get(), "element-setup", G_CALLBACK(+[](MediaRecorderPrivateGStreamer* recorder, GstElement* element) {
        if (WEBKIT_IS_WEBRTC_VIDEO_ENCODER(element)) {
            recorder->configureVideoEncoder(element);
            return;
        }

        if (!GST_IS_APP_SINK(element))
            return;

        recorder->setSink(element);
    }), this);

    m_signalAdapter = adoptGRef(gst_transcoder_get_sync_signal_adapter(m_transcoder.get()));
    g_signal_connect(m_signalAdapter.get(), "warning", G_CALLBACK(+[](GstTranscoder*, GError* error, GstStructure* details) {
        GST_WARNING("%s details: %" GST_PTR_FORMAT, error->message, details);
    }), nullptr);

    g_signal_connect_swapped(m_signalAdapter.get(), "done", G_CALLBACK(+[](MediaRecorderPrivateGStreamer* recorder) {
        recorder->notifyEOS();
    }), this);

    g_signal_connect_swapped(m_signalAdapter.get(), "position-updated", G_CALLBACK(+[](MediaRecorderPrivateGStreamer* recorder, GstClockTime position) {
        recorder->notifyPosition(position);
    }), this);

    return true;
}

void MediaRecorderPrivateGStreamer::processSample(GRefPtr<GstSample>&& sample)
{
    auto* sampleBuffer = gst_sample_get_buffer(sample.get());
    GstMappedBuffer buffer(sampleBuffer, GST_MAP_READ);
    Locker locker { m_dataLock };

    GST_LOG_OBJECT(m_transcoder.get(), "Queueing %zu bytes of encoded data, caps: %" GST_PTR_FORMAT, buffer.size(), gst_sample_get_caps(sample.get()));
    m_data.append(buffer.data(), buffer.size());
}

void MediaRecorderPrivateGStreamer::notifyEOS()
{
    GST_DEBUG("EOS received");
    LockHolder lock(m_eosLock);
    m_eos = true;
    m_eosCondition.notifyAll();
}

} // namespace WebCore

#endif // USE(GSTREAMER_TRANSCODER)
