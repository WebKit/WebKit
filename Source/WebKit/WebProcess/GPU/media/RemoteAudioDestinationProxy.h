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
#include "RemoteAudioDestinationIdentifier.h"
#include <WebCore/AudioIOCallback.h>
#include <wtf/CrossThreadQueue.h>
#include <wtf/MediaTime.h>
#include <wtf/Threading.h>

#if PLATFORM(COCOA)
#include "SharedRingBufferStorage.h"
#include <WebCore/AudioDestinationCocoa.h>
#else
#include <WebCore/AudioDestination.h>
#endif

namespace WebCore {
class CARingBuffer;
class WebAudioBufferList;
}

namespace WebKit {

class RemoteAudioDestinationProxy
#if PLATFORM(COCOA)
    : public WebCore::AudioDestinationCocoa
    , public SharedRingBufferStorage::Client
#else
    : public WebCore::AudioDestination
#endif
    , public GPUProcessConnection::Client
    , public IPC::Connection::ThreadMessageReceiver {
    WTF_MAKE_NONCOPYABLE(RemoteAudioDestinationProxy);
public:
    using AudioIOCallback = WebCore::AudioIOCallback;
    using WebCore::AudioDestination::ref;
    using WebCore::AudioDestination::deref;

    static Ref<AudioDestination> create(AudioIOCallback&, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate);

    RemoteAudioDestinationProxy(AudioIOCallback&, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate);
    ~RemoteAudioDestinationProxy();

    void didReceiveMessageFromGPUProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }

#if PLATFORM(COCOA)
    void requestBuffer(double sampleTime, uint64_t hostTime, uint64_t numberOfFrames, CompletionHandler<void(uint64_t boundsStartFrame, uint64_t boundsEndFrame)>&&);
#endif

private:
    void start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&&) final;
    void stop(CompletionHandler<void(bool)>&&) final;

    void connectToGPUProcess();

    // GPUProcessConnection::Client.
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

#if !PLATFORM(COCOA)
    bool isPlaying() final { return false; }
    void setIsPlaying(bool) { }
    float sampleRate() const final { return 0; }
    unsigned framesPerBuffer() const final { return 0; }
    unsigned numberOfOutputChannels() const { return m_numberOfOutputChannels; }
#endif

#if PLATFORM(COCOA)
    void storageChanged(SharedMemory*) final;
#endif

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // IPC::Connection::ThreadMessageReceiver
    void dispatchToThread(Function<void()>&&) final;
    void refMessageReceiver() final { WebCore::AudioDestination::ref(); }
    void derefMessageReceiver() final { WebCore::AudioDestination::deref(); }

    RemoteAudioDestinationIdentifier m_destinationID;

#if PLATFORM(COCOA)
    uint64_t m_numberOfFrames { 0 };
    std::unique_ptr<WebCore::CARingBuffer> m_ringBuffer;
    std::unique_ptr<WebCore::WebAudioBufferList> m_audioBufferList;
    uint64_t m_currentFrame { 0 };
#else
    unsigned m_numberOfOutputChannels;
#endif

    String m_inputDeviceId;
    unsigned m_numberOfInputChannels;

    Function<void(Function<void()>&&)> m_dispatchToRenderThread;
    RefPtr<Thread> m_renderThread;
    CrossThreadQueue<Function<void()>> m_threadTaskQueue;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
