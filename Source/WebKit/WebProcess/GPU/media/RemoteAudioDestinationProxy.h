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
#include "RemoteAudioBusData.h"
#include "RemoteAudioDestinationIdentifier.h"
#include "WebProcessSupplement.h"
#include <WebCore/AudioDestination.h>
#include <WebCore/AudioIOCallback.h>

#if PLATFORM(COCOA)
#include <WebCore/CARingBuffer.h>
#endif

namespace WebKit {

class RemoteAudioDestinationProxy : public WebCore::AudioDestination, private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RemoteAudioDestinationProxy);
public:
    using AudioBus = WebCore::AudioBus;
    using AudioIOCallback = WebCore::AudioIOCallback;

    static std::unique_ptr<AudioDestination> create(AudioIOCallback&, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate);

    RemoteAudioDestinationProxy(AudioIOCallback&, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate);
    ~RemoteAudioDestinationProxy();

    void didReceiveMessageFromGPUProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }

    void renderBuffer(const WebKit::RemoteAudioBusData&, CompletionHandler<void()>&&);
    void didChangeIsPlaying(bool isPlaying);

private:
    // WebCore::AudioDestination
    void start() override;
    void stop() override;
    bool isPlaying() override { return m_isPlaying; }
    float sampleRate() const override { return m_sampleRate; }
    unsigned framesPerBuffer() const override { return m_framesPerBuffer; }

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    AudioIOCallback& m_callback;
    float m_sampleRate { 0. };
    unsigned m_framesPerBuffer { 0 };
    RemoteAudioDestinationIdentifier m_destinationID;
    bool m_isPlaying { false };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
