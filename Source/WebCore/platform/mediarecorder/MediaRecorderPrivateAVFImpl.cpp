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

#if ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

#include "AudioStreamDescription.h"
#include "CAAudioStreamDescription.h"
#include "Logging.h"
#include "MediaRecorderPrivateWriterCocoa.h"
#include "MediaSampleAVFObjC.h"
#include "MediaStreamPrivate.h"
#include "RealtimeIncomingVideoSourceCocoa.h"
#include "SharedBuffer.h"
#include "WebAudioBufferList.h"

#include "CoreVideoSoftLink.h"
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {
using namespace PAL;

std::unique_ptr<MediaRecorderPrivateAVFImpl> MediaRecorderPrivateAVFImpl::create(MediaStreamPrivate& stream, const MediaRecorderPrivateOptions& options)
{
    // FIXME: we will need to implement support for multiple audio/video tracks
    // Currently we only choose the first track as the recorded track.
    // FIXME: We would better to throw an exception to JavaScript if writer creation fails.

    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream);

    auto writer = MediaRecorderPrivateWriter::create(!!selectedTracks.audioTrack, !!selectedTracks.videoTrack, options);
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

MediaRecorderPrivateAVFImpl::MediaRecorderPrivateAVFImpl(Ref<MediaRecorderPrivateWriter>&& writer)
    : m_writer(WTFMove(writer))
{
}

MediaRecorderPrivateAVFImpl::~MediaRecorderPrivateAVFImpl()
{
}

void MediaRecorderPrivateAVFImpl::startRecording(StartRecordingCallback&& callback)
{
    // FIMXE: In case of of audio recording, we should wait for the audio compression to start to give back the exact bit rate.
    // FIXME: Add support to options.bitsPerSecond as well.
    callback(String(m_writer->mimeType()), m_writer->audioBitRate(), m_writer->videoBitRate());
}

void MediaRecorderPrivateAVFImpl::videoSampleAvailable(MediaSample& sampleBuffer)
{
    if (shouldMuteVideo()) {
        if (!m_blackFrame) {
            m_blackFrameDescription = CMSampleBufferGetFormatDescription(sampleBuffer.platformSample().sample.cmSampleBuffer);
            auto dimensions = CMVideoFormatDescriptionGetDimensions(m_blackFrameDescription.get());
            m_blackFrame = createBlackPixelBuffer(dimensions.width, dimensions.height);

            CMVideoFormatDescriptionRef formatDescription = nullptr;
            auto status = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, m_blackFrame.get(), &formatDescription);
            if (status != noErr) {
                RELEASE_LOG_ERROR(Media, "MediaRecorderPrivateAVFImpl::videoSampleAvailable ::unable to create a black frame description: %d", static_cast<int>(status));
                m_blackFrame = nullptr;
                return;
            }
            m_blackFrameDescription = adoptCF(formatDescription);
        }

        CMSampleBufferRef sample = nullptr;
        CMSampleTimingInfo timingInfo { kCMTimeInvalid, toCMTime(sampleBuffer.presentationTime()), toCMTime(sampleBuffer.decodeTime()) };
        auto status = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)m_blackFrame.get(), m_blackFrameDescription.get(), &timingInfo, &sample);

        if (status != noErr) {
            RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateAVFImpl::videoSampleAvailable - unable to create a black frame: %d", static_cast<int>(status));
            return;
        }
        auto newSample = adoptCF(sample);
        m_writer->appendVideoSampleBuffer(MediaSampleAVFObjC::create(newSample.get(), sampleBuffer.videoRotation(), sampleBuffer.videoMirrored()));
        return;
    }

    m_blackFrame = nullptr;
    m_blackFrameDescription = nullptr;
    m_writer->appendVideoSampleBuffer(sampleBuffer);
}

void MediaRecorderPrivateAVFImpl::audioSamplesAvailable(const WTF::MediaTime& mediaTime, const PlatformAudioData& data, const AudioStreamDescription& description, size_t sampleCount)
{
    ASSERT(is<WebAudioBufferList>(data));
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);

    if (shouldMuteAudio()) {
        if (!m_audioBuffer || m_description != toCAAudioStreamDescription(description)) {
            m_description = toCAAudioStreamDescription(description);
            m_audioBuffer = makeUnique<WebAudioBufferList>(m_description, sampleCount);
        } else
            m_audioBuffer->setSampleCount(sampleCount);
        m_audioBuffer->zeroFlatBuffer();
        m_writer->appendAudioSampleBuffer(*m_audioBuffer, description, mediaTime, sampleCount);
        return;
    }

    m_writer->appendAudioSampleBuffer(data, description, mediaTime, sampleCount);
}

void MediaRecorderPrivateAVFImpl::stopRecording()
{
    m_writer->stopRecording();
}

void MediaRecorderPrivateAVFImpl::pauseRecording(CompletionHandler<void()>&& completionHandler)
{
    m_writer->pause();
    completionHandler();
}

void MediaRecorderPrivateAVFImpl::resumeRecording(CompletionHandler<void()>&& completionHandler)
{
    m_writer->resume();
    completionHandler();
}

void MediaRecorderPrivateAVFImpl::fetchData(FetchDataCallback&& completionHandler)
{
    m_writer->fetchData([completionHandler = WTFMove(completionHandler), mimeType = mimeType()](auto&& buffer, auto timeCode) mutable {
        completionHandler(WTFMove(buffer), mimeType, timeCode);
    });
}

const String& MediaRecorderPrivateAVFImpl::mimeType() const
{
    return m_writer->mimeType();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)
