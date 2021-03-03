/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaRecorderPrivate.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

#include "DataReference.h"
#include "GPUProcessConnection.h"
#include "RemoteMediaRecorderManagerMessages.h"
#include "RemoteMediaRecorderMessages.h"
#include "WebProcess.h"
#include <WebCore/CARingBuffer.h>
#include <WebCore/MediaStreamPrivate.h>
#include <WebCore/MediaStreamTrackPrivate.h>
#include <WebCore/RealtimeIncomingVideoSourceCocoa.h>
#include <WebCore/RemoteVideoSample.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/WebAudioBufferList.h>

#include <pal/cf/CoreMediaSoftLink.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;

MediaRecorderPrivate::MediaRecorderPrivate(MediaStreamPrivate& stream, const MediaRecorderPrivateOptions& options)
    : m_identifier(MediaRecorderIdentifier::generate())
    , m_stream(makeRef(stream))
    , m_connection(WebProcess::singleton().ensureGPUProcessConnection().connection())
    , m_options(options)
    , m_hasVideo(stream.hasVideo())
{
}

void MediaRecorderPrivate::startRecording(StartRecordingCallback&& callback)
{
    // FIXME: we will need to implement support for multiple audio/video tracks
    // Currently we only choose the first track as the recorded track.

    auto selectedTracks = MediaRecorderPrivate::selectTracks(m_stream);
    if (selectedTracks.audioTrack)
        m_ringBuffer = makeUnique<CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(std::bind(&MediaRecorderPrivate::storageChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));

    m_connection->sendWithAsyncReply(Messages::RemoteMediaRecorderManager::CreateRecorder { m_identifier, !!selectedTracks.audioTrack, !!selectedTracks.videoTrack, m_options }, [this, weakThis = makeWeakPtr(this), audioTrack = makeRefPtr(selectedTracks.audioTrack), videoTrack = makeRefPtr(selectedTracks.videoTrack), callback = WTFMove(callback)](auto&& exception, String&& mimeType, unsigned audioBitRate, unsigned videoBitRate) mutable {
        if (!weakThis) {
            callback(Exception { InvalidStateError }, 0, 0);
            return;
        }
        if (exception) {
            callback(Exception { exception->code, WTFMove(exception->message) }, 0, 0);
            return;
        }
        if (!m_isStopped) {
            if (audioTrack)
                setAudioSource(&audioTrack->source());
            if (videoTrack)
                setVideoSource(&videoTrack->source());
        }
        callback(WTFMove(mimeType), audioBitRate, videoBitRate);
    }, 0);
}

MediaRecorderPrivate::~MediaRecorderPrivate()
{
    m_connection->send(Messages::RemoteMediaRecorderManager::ReleaseRecorder { m_identifier }, 0);
}

void MediaRecorderPrivate::videoSampleAvailable(MediaSample& sample)
{
    std::unique_ptr<RemoteVideoSample> remoteSample;
    if (shouldMuteVideo()) {
        if (!m_blackFrame) {
            auto blackFrameDescription = CMSampleBufferGetFormatDescription(sample.platformSample().sample.cmSampleBuffer);
            auto dimensions = CMVideoFormatDescriptionGetDimensions(blackFrameDescription);
            auto blackFrame = createBlackPixelBuffer(dimensions.width, dimensions.height);
            // FIXME: We convert to get an IOSurface. We could optimize this.
            m_blackFrame = convertToBGRA(blackFrame.get());
        }
        remoteSample = RemoteVideoSample::create(m_blackFrame.get(), sample.presentationTime(), sample.videoRotation());
    } else {
        m_blackFrame = nullptr;
        remoteSample = RemoteVideoSample::create(sample);
        if (!remoteSample) {
            // FIXME: Optimize this code path.
            auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(sample.platformSample().sample.cmSampleBuffer));
            auto newPixelBuffer = convertToBGRA(pixelBuffer);
            remoteSample = RemoteVideoSample::create(newPixelBuffer.get(), sample.presentationTime(), sample.videoRotation());
        }
    }

    if (remoteSample)
        m_connection->send(Messages::RemoteMediaRecorder::VideoSampleAvailable { WTFMove(*remoteSample) }, m_identifier);
}

void MediaRecorderPrivate::audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t numberOfFrames)
{
    if (m_description != description) {
        ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
        m_description = *WTF::get<const AudioStreamBasicDescription*>(description.platformDescription().description);

        // Allocate a ring buffer large enough to contain 2 seconds of audio.
        m_numberOfFrames = m_description.sampleRate() * 2;
        m_ringBuffer->allocate(m_description.streamDescription(), m_numberOfFrames);
        m_silenceAudioBuffer = nullptr;
    }

    ASSERT(is<WebAudioBufferList>(audioData));

    if (shouldMuteAudio()) {
        if (!m_silenceAudioBuffer)
            m_silenceAudioBuffer = makeUnique<WebAudioBufferList>(m_description, numberOfFrames);
        else
            m_silenceAudioBuffer->setSampleCount(numberOfFrames);
        m_silenceAudioBuffer->zeroFlatBuffer();
        m_ringBuffer->store(m_silenceAudioBuffer->list(), numberOfFrames, time.timeValue());
    } else
        m_ringBuffer->store(downcast<WebAudioBufferList>(audioData).list(), numberOfFrames, time.timeValue());
    m_connection->send(Messages::RemoteMediaRecorder::AudioSamplesAvailable { time, numberOfFrames }, m_identifier);
}

void MediaRecorderPrivate::storageChanged(SharedMemory* storage, const WebCore::CAAudioStreamDescription& format, size_t frameCount)
{
    SharedMemory::Handle handle;
    if (storage)
        storage->createHandle(handle, SharedMemory::Protection::ReadOnly);

    // FIXME: Send the actual data size with IPCHandle.
#if OS(DARWIN) || OS(WINDOWS)
    uint64_t dataSize = handle.size();
#else
    uint64_t dataSize = 0;
#endif
    m_connection->send(Messages::RemoteMediaRecorder::AudioSamplesStorageChanged { SharedMemory::IPCHandle { WTFMove(handle), dataSize }, format, frameCount }, m_identifier);
}

void MediaRecorderPrivate::fetchData(CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&, const String& mimeType, double)>&& completionHandler)
{
    m_connection->sendWithAsyncReply(Messages::RemoteMediaRecorder::FetchData { }, [completionHandler = WTFMove(completionHandler), mimeType = mimeType()](auto&& data, double timeCode) mutable {
        RefPtr<SharedBuffer> buffer;
        if (data.size())
            buffer = SharedBuffer::create(data.data(), data.size());
        completionHandler(WTFMove(buffer), mimeType, timeCode);
    }, m_identifier);
}

void MediaRecorderPrivate::stopRecording(CompletionHandler<void()>&& completionHandler)
{
    m_isStopped = true;
    m_connection->sendWithAsyncReply(Messages::RemoteMediaRecorder::StopRecording { }, WTFMove(completionHandler), m_identifier);
}

void MediaRecorderPrivate::pauseRecording(CompletionHandler<void()>&& completionHandler)
{
    m_connection->sendWithAsyncReply(Messages::RemoteMediaRecorder::Pause { }, WTFMove(completionHandler), m_identifier);
}

void MediaRecorderPrivate::resumeRecording(CompletionHandler<void()>&& completionHandler)
{
    m_connection->sendWithAsyncReply(Messages::RemoteMediaRecorder::Resume { }, WTFMove(completionHandler), m_identifier);
}

const String& MediaRecorderPrivate::mimeType() const
{
    static NeverDestroyed<const String> audioMP4(MAKE_STATIC_STRING_IMPL("audio/mp4"));
    static NeverDestroyed<const String> videoMP4(MAKE_STATIC_STRING_IMPL("video/mp4"));
    return m_hasVideo ? videoMP4 : audioMP4;
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)
