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

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)

#include "Connection.h"
#include "GPUProcessConnection.h"
#include "IPCSemaphore.h"
#include "RemoteAudioDestinationIdentifier.h"
#include <WebCore/AudioDestinationResampler.h>
#include <WebCore/AudioIOCallback.h>
#include <wtf/CrossThreadQueue.h>
#include <wtf/MediaTime.h>
#include <wtf/Threading.h>

#if PLATFORM(COCOA)
#include "SharedCARingBuffer.h"
#endif

#if PLATFORM(COCOA)
namespace WebCore {
class WebAudioBufferList;
}
#endif

namespace WebKit {

class RemoteAudioDestinationProxy : public WebCore::AudioDestinationResampler, public GPUProcessConnection::Client, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteAudioDestinationProxy> {
    WTF_MAKE_NONCOPYABLE(RemoteAudioDestinationProxy);
public:
    using AudioIOCallback = WebCore::AudioIOCallback;

    static Ref<RemoteAudioDestinationProxy> create(AudioIOCallback&, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate);

    RemoteAudioDestinationProxy(AudioIOCallback&, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate);
    ~RemoteAudioDestinationProxy();

    void ref() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteAudioDestinationProxy>::ref(); }
    void deref() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteAudioDestinationProxy>::deref(); }
    ThreadSafeWeakPtrControlBlock& controlBlock() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteAudioDestinationProxy>::controlBlock(); }

private:
    void startRendering(CompletionHandler<void(bool)>&&) override;
    void stopRendering(CompletionHandler<void(bool)>&&) override;

    void startRenderingThread();
    void stopRenderingThread();
    void renderAudio(unsigned frameCount);

    IPC::Connection* connection();
    IPC::Connection* existingConnection();

    // GPUProcessConnection::Client.
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    uint32_t totalFrameCount() const;

    RemoteAudioDestinationIdentifier m_destinationID; // Call destinationID() getter to make sure the destinationID is valid.

    static uint8_t s_realtimeThreadCount;
    static constexpr uint8_t s_maximumConcurrentRealtimeThreads { 3 };

    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
#if PLATFORM(COCOA)
    std::unique_ptr<ProducerSharedCARingBuffer> m_ringBuffer;
    std::unique_ptr<WebCore::WebAudioBufferList> m_audioBufferList;
    uint64_t m_currentFrame { 0 };
#endif
    IPC::Semaphore m_renderSemaphore;

    String m_inputDeviceId;
    unsigned m_numberOfInputChannels;
    float m_remoteSampleRate;

    RefPtr<Thread> m_renderThread;
    RefPtr<WebCore::SharedMemory> m_frameCount;
    uint32_t m_lastFrameCount { 0 };
    std::atomic<bool> m_shouldStopThread { false };
    bool m_isRealtimeThread { false };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
