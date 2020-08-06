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
#include <WebCore/RemoteVideoSample.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/WebAudioBufferList.h>

namespace WebKit {
using namespace WebCore;

MediaRecorderPrivate::MediaRecorderPrivate(MediaStreamPrivate& stream)
    : m_identifier(MediaRecorderIdentifier::generate())
    , m_stream(makeRef(stream))
    , m_connection(WebProcess::singleton().ensureGPUProcessConnection().connection())
{
}

void MediaRecorderPrivate::startRecording(ErrorCallback&& errorCallback)
{
    // FIXME: we will need to implement support for multiple audio/video tracks
    // Currently we only choose the first track as the recorded track.

    auto selectedTracks = MediaRecorderPrivate::selectTracks(m_stream);
    if (selectedTracks.audioTrack)
        m_ringBuffer = makeUnique<CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(this));

    m_connection->sendWithAsyncReply(Messages::RemoteMediaRecorderManager::CreateRecorder { m_identifier, !!selectedTracks.audioTrack, !!selectedTracks.videoTrack }, [this, weakThis = makeWeakPtr(this), audioTrack = makeRefPtr(selectedTracks.audioTrack), videoTrack = makeRefPtr(selectedTracks.videoTrack), errorCallback = WTFMove(errorCallback)](auto&& exception) mutable {
        if (!weakThis) {
            errorCallback({ });
            return;
        }
        if (exception) {
            errorCallback(Exception { exception->code, WTFMove(exception->message) });
            return;
        }
        if (audioTrack)
            setAudioSource(&audioTrack->source());
        if (videoTrack)
            setVideoSource(&videoTrack->source());
        errorCallback({ });
    }, 0);
}

MediaRecorderPrivate::~MediaRecorderPrivate()
{
    setAudioSource(nullptr);
    setVideoSource(nullptr);
    m_connection->send(Messages::RemoteMediaRecorderManager::ReleaseRecorder { m_identifier }, 0);
}

void MediaRecorderPrivate::videoSampleAvailable(MediaSample& sample)
{
    if (auto remoteSample = RemoteVideoSample::create(sample))
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
    }

    ASSERT(is<WebAudioBufferList>(audioData));
    m_ringBuffer->store(downcast<WebAudioBufferList>(audioData).list(), numberOfFrames, time.timeValue());
    uint64_t startFrame;
    uint64_t endFrame;
    m_ringBuffer->getCurrentFrameBounds(startFrame, endFrame);
    m_connection->send(Messages::RemoteMediaRecorder::AudioSamplesAvailable { time, numberOfFrames, startFrame, endFrame }, m_identifier);
}

void MediaRecorderPrivate::storageChanged(SharedMemory* storage)
{
    SharedMemory::Handle handle;
    if (storage)
        storage->createHandle(handle, SharedMemory::Protection::ReadOnly);
    m_connection->send(Messages::RemoteMediaRecorder::AudioSamplesStorageChanged { handle, m_description, static_cast<uint64_t>(m_numberOfFrames) }, m_identifier);
}

void MediaRecorderPrivate::fetchData(CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&, const String& mimeType)>&& completionHandler)
{
    m_connection->sendWithAsyncReply(Messages::RemoteMediaRecorder::FetchData { }, [completionHandler = WTFMove(completionHandler)](auto&& data, auto&& mimeType) mutable {
        RefPtr<SharedBuffer> buffer;
        if (data.size())
            buffer = SharedBuffer::create(data.data(), data.size());
        completionHandler(WTFMove(buffer), mimeType);
    }, m_identifier);
}

void MediaRecorderPrivate::stopRecording()
{
    setAudioSource(nullptr);
    setVideoSource(nullptr);
    m_connection->send(Messages::RemoteMediaRecorder::StopRecording { }, m_identifier);
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)
