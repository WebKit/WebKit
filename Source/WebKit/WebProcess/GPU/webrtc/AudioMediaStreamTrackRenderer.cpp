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
#include "AudioMediaStreamTrackRenderer.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "GPUProcessConnection.h"
#include "RemoteAudioMediaStreamTrackRendererManagerMessages.h"
#include "RemoteAudioMediaStreamTrackRendererMessages.h"
#include "WebProcess.h"
#include <WebCore/AudioMediaStreamTrackRenderer.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>

namespace WebKit {

std::unique_ptr<WebCore::AudioMediaStreamTrackRenderer> AudioMediaStreamTrackRenderer::create()
{
    return std::unique_ptr<AudioMediaStreamTrackRenderer>(new AudioMediaStreamTrackRenderer { WebProcess::singleton().ensureGPUProcessConnection().connection() });
}

AudioMediaStreamTrackRenderer::AudioMediaStreamTrackRenderer(Ref<IPC::Connection>&& connection)
    : m_connection(WTFMove(connection))
    , m_identifier(AudioMediaStreamTrackRendererIdentifier::generate())
    , m_ringBuffer(makeUnique<WebCore::CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(std::bind(&AudioMediaStreamTrackRenderer::storageChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))))
{
    initialize();
}

AudioMediaStreamTrackRenderer::~AudioMediaStreamTrackRenderer()
{
    WebProcess::singleton().ensureGPUProcessConnection().removeClient(*this);
    m_connection->send(Messages::RemoteAudioMediaStreamTrackRendererManager::ReleaseRenderer { m_identifier }, 0);
}

void AudioMediaStreamTrackRenderer::initialize()
{
    WebProcess::singleton().ensureGPUProcessConnection().addClient(*this);
    m_connection->send(Messages::RemoteAudioMediaStreamTrackRendererManager::CreateRenderer { m_identifier }, 0);
}

void AudioMediaStreamTrackRenderer::start()
{
    m_isPlaying = true;
    m_connection->send(Messages::RemoteAudioMediaStreamTrackRenderer::Start { }, m_identifier);
}

void AudioMediaStreamTrackRenderer::stop()
{
    m_isPlaying = false;
    m_connection->send(Messages::RemoteAudioMediaStreamTrackRenderer::Stop { }, m_identifier);
}

void AudioMediaStreamTrackRenderer::clear()
{
    m_connection->send(Messages::RemoteAudioMediaStreamTrackRenderer::Clear { }, m_identifier);
}

void AudioMediaStreamTrackRenderer::setVolume(float value)
{
    WebCore::AudioMediaStreamTrackRenderer::setVolume(value);
    m_connection->send(Messages::RemoteAudioMediaStreamTrackRenderer::SetVolume { value }, m_identifier);
}

void AudioMediaStreamTrackRenderer::pushSamples(const MediaTime& time, const WebCore::PlatformAudioData& audioData, const WebCore::AudioStreamDescription& description, size_t numberOfFrames)
{
    if (!m_isPlaying)
        return;

    if (m_description != description) {
        ASSERT(description.platformDescription().type == WebCore::PlatformDescription::CAAudioStreamBasicType);
        m_description = *WTF::get<const AudioStreamBasicDescription*>(description.platformDescription().description);

        // Allocate a ring buffer large enough to contain 2 seconds of audio.
        m_numberOfFrames = m_description.sampleRate() * 2;
        m_ringBuffer->allocate(m_description, m_numberOfFrames);
    }

    ASSERT(is<WebCore::WebAudioBufferList>(audioData));
    m_ringBuffer->store(downcast<WebCore::WebAudioBufferList>(audioData).list(), numberOfFrames, time.timeValue());
    m_connection->send(Messages::RemoteAudioMediaStreamTrackRenderer::AudioSamplesAvailable { time, numberOfFrames }, m_identifier);
}

void AudioMediaStreamTrackRenderer::storageChanged(SharedMemory* storage, const WebCore::CAAudioStreamDescription& format, size_t frameCount)
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
    m_connection->send(Messages::RemoteAudioMediaStreamTrackRenderer::AudioSamplesStorageChanged { SharedMemory::IPCHandle { WTFMove(handle), dataSize }, format, frameCount }, m_identifier);
}

void AudioMediaStreamTrackRenderer::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    crashed();
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
