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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "DataReference.h"
#include "GPUProcessConnection.h"
#include "RemoteMediaRecorderManagerMessages.h"
#include "RemoteMediaRecorderMessages.h"
#include "RemoteVideoFrameProxy.h"
#include "WebProcess.h"
#include <WebCore/CARingBuffer.h>
#include <WebCore/CVUtilities.h>
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
    , m_stream(stream)
    , m_connection(WebProcess::singleton().ensureGPUProcessConnection().connection())
    , m_options(options)
    , m_hasVideo(stream.hasVideo())
{
    WebProcess::singleton().ensureGPUProcessConnection().addClient(*this);
}

void MediaRecorderPrivate::startRecording(StartRecordingCallback&& callback)
{
    // FIXME: we will need to implement support for multiple audio/video tracks
    // Currently we only choose the first track as the recorded track.

    auto selectedTracks = MediaRecorderPrivate::selectTracks(m_stream);
    if (selectedTracks.audioTrack)
        m_ringBuffer = makeUnique<CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(std::bind(&MediaRecorderPrivate::storageChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));

    m_connection->sendWithAsyncReply(Messages::RemoteMediaRecorderManager::CreateRecorder { m_identifier, !!selectedTracks.audioTrack, !!selectedTracks.videoTrack, m_options }, [this, weakThis = WeakPtr { *this }, audioTrack = RefPtr { selectedTracks.audioTrack }, videoTrack = RefPtr { selectedTracks.videoTrack }, callback = WTFMove(callback)](auto&& exception, String&& mimeType, unsigned audioBitRate, unsigned videoBitRate) mutable {
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
    WebProcess::singleton().ensureGPUProcessConnection().removeClient(*this);
}

void MediaRecorderPrivate::videoSampleAvailable(MediaSample& sample, VideoSampleMetadata)
{
    std::optional<RemoteVideoFrameReadReference> remoteVideoFrameReadReference;
    std::unique_ptr<RemoteVideoSample> remoteSample;
    if (shouldMuteVideo()) {
        // FIXME: We could optimize sending black frames by only sending width/height.
        if (!m_blackFrame) {
            auto size = sample.presentationSize();
            m_blackFrame = createBlackPixelBuffer(static_cast<size_t>(size.width()), static_cast<size_t>(size.height()));
        }
        remoteSample = RemoteVideoSample::create(m_blackFrame.get(), sample.presentationTime(), sample.videoRotation(), RemoteVideoSample::ShouldCheckForIOSurface::No);
    } else {
        m_blackFrame = nullptr;

        if (is<RemoteVideoFrameProxy>(sample)) {
            remoteVideoFrameReadReference = downcast<RemoteVideoFrameProxy>(sample).read();
            remoteSample = RemoteVideoSample::create(nullptr, sample.presentationTime(), sample.videoRotation(), RemoteVideoSample::ShouldCheckForIOSurface::No);
        } else
            remoteSample = RemoteVideoSample::create(sample, RemoteVideoSample::ShouldCheckForIOSurface::No);
    }

    if (is<RemoteVideoFrameProxy>(sample))
        remoteVideoFrameReadReference = downcast<RemoteVideoFrameProxy>(sample).read();
    else if (!remoteSample->surface()) {
        // buffer is not IOSurface, we need to copy to shared video frame.
        if (!copySharedVideoFrame(remoteSample->imageBuffer()))
            return;
    }

    m_connection->send(Messages::RemoteMediaRecorder::VideoSampleAvailable { WTFMove(*remoteSample), remoteVideoFrameReadReference }, m_identifier);
}


bool MediaRecorderPrivate::copySharedVideoFrame(CVPixelBufferRef pixelBuffer)
{
    if (!pixelBuffer)
        return false;
    return m_sharedVideoFrameWriter.write(pixelBuffer,
        [this](auto& semaphore) { m_connection->send(Messages::RemoteMediaRecorder::SetSharedVideoFrameSemaphore { semaphore }, m_identifier); },
        [this](auto& handle) { m_connection->send(Messages::RemoteMediaRecorder::SetSharedVideoFrameMemory { handle }, m_identifier); }
    );
}

void MediaRecorderPrivate::audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t numberOfFrames)
{
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    if (m_description != description) {
        ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
        m_description = *std::get<const AudioStreamBasicDescription*>(description.platformDescription().description);

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
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

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

void MediaRecorderPrivate::fetchData(CompletionHandler<void(RefPtr<WebCore::FragmentedSharedBuffer>&&, const String& mimeType, double)>&& completionHandler)
{
    m_connection->sendWithAsyncReply(Messages::RemoteMediaRecorder::FetchData { }, [completionHandler = WTFMove(completionHandler), mimeType = mimeType()](auto&& data, double timeCode) mutable {
        // FIXME: If completion handler is called following a GPUProcess connection being closed, we should fail the MediaRecorder.
        RefPtr<FragmentedSharedBuffer> buffer;
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

void MediaRecorderPrivate::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    m_sharedVideoFrameWriter.disable();
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
