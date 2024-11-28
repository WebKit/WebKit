/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "MediaRecorderPrivateAVFImpl.h"

#if ENABLE(MEDIA_RECORDER)

#include "AudioStreamDescription.h"
#include "CAAudioStreamDescription.h"
#include "CVUtilities.h"
#include "ContentType.h"
#include "Document.h"
#include "Logging.h"
#include "MediaRecorderPrivateEncoder.h"
#include "MediaStreamPrivate.h"
#include "RealtimeIncomingVideoSourceCocoa.h"
#include "SharedBuffer.h"
#include "VideoFrameCV.h"
#include "WebAudioBufferList.h"

#include "CoreVideoSoftLink.h"
#include <pal/cf/CoreMediaSoftLink.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaRecorderPrivateAVFImpl);

bool MediaRecorderPrivateAVFImpl::isTypeSupported(Document& document, ContentType& mimeType)
{
    auto containerType = mimeType.containerType();
    if (containerType.isEmpty())
        return true;

    if (equalLettersIgnoringASCIICase(containerType, "audio/mp4"_s) || equalLettersIgnoringASCIICase(containerType, "video/mp4"_s)) {
        for (auto& codec : mimeType.codecs()) {
            // FIXME: We should further validate parameters.
            if (!startsWithLettersIgnoringASCIICase(codec, "avc1"_s)
#if ENABLE(AV1)
                && !(codec.startsWith("av01."_s) && document.settings().webRTCAV1CodecEnabled())
#endif
#if ENABLE(WEB_RTC)
                && !((codec.startsWith("hev1."_s) || codec.startsWith("hvc1."_s)) && document.settings().webRTCH265CodecEnabled())
#endif
#if HAVE(AVASSETWRITER_WITH_OPUS_SUPPORTED)
                && !codec.startsWith("opus"_s)
#endif
                && !startsWithLettersIgnoringASCIICase(codec, "mp4a"_s))
                return false;
        }
        return true;
    }
#if ENABLE(MEDIA_RECORDER_WEBM)
    if (!document.settings().mediaRecorderEnabledWebM())
        return false;
    if (!equalLettersIgnoringASCIICase(containerType, "audio/webm"_s) && !equalLettersIgnoringASCIICase(containerType, "video/webm"_s))
        return false;

    for (auto& codec : mimeType.codecs()) {
        // FIXME: We should further validate parameters.
        bool isVP90 = (codec.startsWith("vp09"_s) || equal(codec, "vp9"_s) || equal(codec, "vp9.0"_s)) && !codec.startsWith("vp09.02"_s);
        bool isVP92 = codec.startsWith("vp09.02"_s);
        bool isVP8 = codec.startsWith("vp08"_s) || equal(codec, "vp8"_s) || equal(codec, "vp8.0"_s);
        bool isOpus = codec == "opus"_s;
        if (!(isVP90 && document.settings().webRTCVP9Profile0CodecEnabled()) && !(isVP92 && document.settings().webRTCVP9Profile2CodecEnabled()) && !isVP8 && !isOpus)
            return false;
    }
    return true;
#else
    UNUSED_PARAM(document);
    return false;
#endif
}

std::unique_ptr<MediaRecorderPrivateAVFImpl> MediaRecorderPrivateAVFImpl::create(MediaStreamPrivate& stream, const MediaRecorderPrivateOptions& originalOptions)
{
    // FIXME: we will need to implement support for multiple audio/video tracks
    // Currently we only choose the first track as the recorded track.
    // FIXME: We would better to throw an exception to JavaScript if writer creation fails.

    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream);

    auto options = originalOptions;
    if (options.mimeType.isEmpty())
        options.mimeType = !!selectedTracks.videoTrack ? "video/mp4"_s : "audio/mp4"_s;
    RefPtr writer = MediaRecorderPrivateEncoder::create(!!selectedTracks.audioTrack, !!selectedTracks.videoTrack, options);
    if (!writer)
        return nullptr;

    auto recorder = std::unique_ptr<MediaRecorderPrivateAVFImpl>(new MediaRecorderPrivateAVFImpl(writer.releaseNonNull()));
    if (selectedTracks.audioTrack) {
        recorder->setAudioSource(&selectedTracks.audioTrack->source());
        recorder->checkTrackState(*selectedTracks.audioTrack);
    }
    if (selectedTracks.videoTrack) {
        recorder->setVideoSource(&selectedTracks.videoTrack->source());
        recorder->checkTrackState(*selectedTracks.videoTrack);
    }
    return recorder;
}

MediaRecorderPrivateAVFImpl::MediaRecorderPrivateAVFImpl(Ref<MediaRecorderPrivateEncoder>&& muxer)
    : m_encoder(WTFMove(muxer))
{
}

MediaRecorderPrivateAVFImpl::~MediaRecorderPrivateAVFImpl()
{
    m_encoder->close();
}

void MediaRecorderPrivateAVFImpl::startRecording(StartRecordingCallback&& callback)
{
    // FIMXE: In case of of audio recording, we should wait for the audio compression to start to give back the exact bit rate.
    callback(String(m_encoder->mimeType()), m_encoder->audioBitRate(), m_encoder->videoBitRate());
}

void MediaRecorderPrivateAVFImpl::videoFrameAvailable(VideoFrame& videoFrame, VideoFrameTimeMetadata)
{
    if (shouldMuteVideo()) {
        if (!m_blackFrame) {
            auto size = videoFrame.presentationSize();
            m_blackFrame = VideoFrameCV::create(videoFrame.presentationTime(), videoFrame.isMirrored(), videoFrame.rotation(), createBlackPixelBuffer(size.width(), size.height()));
        }
        m_encoder->appendVideoFrame(*m_blackFrame);
        return;
    }

    m_blackFrame = nullptr;
    m_encoder->appendVideoFrame(videoFrame);
}

void MediaRecorderPrivateAVFImpl::audioSamplesAvailable(const MediaTime& mediaTime, const PlatformAudioData& data, const AudioStreamDescription& description, size_t sampleCount)
{
    ASSERT(is<WebAudioBufferList>(data));
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);

    if (shouldMuteAudio()) {
        if (!m_audioBuffer || m_description != toCAAudioStreamDescription(description)) {
            m_description = toCAAudioStreamDescription(description);
            m_audioBuffer = makeUnique<WebAudioBufferList>(*m_description, sampleCount);
        } else
            m_audioBuffer->setSampleCount(sampleCount);
        m_audioBuffer->zeroFlatBuffer();
        m_encoder->appendAudioSampleBuffer(*m_audioBuffer, description, mediaTime, sampleCount);
        return;
    }

    m_encoder->appendAudioSampleBuffer(data, description, mediaTime, sampleCount);
}

void MediaRecorderPrivateAVFImpl::stopRecording(CompletionHandler<void()>&& completionHandler)
{
    m_encoder->stopRecording();
    completionHandler();
}

void MediaRecorderPrivateAVFImpl::pauseRecording(CompletionHandler<void()>&& completionHandler)
{
    m_encoder->pause();
    completionHandler();
}

void MediaRecorderPrivateAVFImpl::resumeRecording(CompletionHandler<void()>&& completionHandler)
{
    m_encoder->resume();
    completionHandler();
}

void MediaRecorderPrivateAVFImpl::fetchData(FetchDataCallback&& completionHandler)
{
    m_encoder->fetchData([completionHandler = WTFMove(completionHandler), mimeType = mimeType()](RefPtr<FragmentedSharedBuffer>&& buffer, auto timeCode) mutable {
        completionHandler(WTFMove(buffer), mimeType, timeCode);
    });
}

String MediaRecorderPrivateAVFImpl::mimeType() const
{
    return m_encoder->mimeType();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
