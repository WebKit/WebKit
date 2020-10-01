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

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)

#include "MessageReceiver.h"
#include "RemoteAudioSessionConfiguration.h"
#include <WebCore/AudioSession.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class GPUProcessConnection;
class WebProcess;

class RemoteAudioSession final
    : public WebCore::AudioSession
    , IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static UniqueRef<RemoteAudioSession> create(WebProcess&);
    ~RemoteAudioSession();

private:
    friend UniqueRef<RemoteAudioSession> WTF::makeUniqueRefWithoutFastMallocCheck<RemoteAudioSession>(WebProcess&, RemoteAudioSessionConfiguration&&);
    RemoteAudioSession(WebProcess&, RemoteAudioSessionConfiguration&&);
    IPC::Connection& connection();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void configurationChanged(RemoteAudioSessionConfiguration&&);

    // AudioSession
    void setCategory(CategoryType, WebCore::RouteSharingPolicy) final;
    void setPreferredBufferSize(size_t) final;
    bool tryToSetActiveInternal(bool) final;

    CategoryType category() const final { return m_configuration.category; }
    WebCore::RouteSharingPolicy routeSharingPolicy() const final { return m_configuration.routeSharingPolicy; }
    String routingContextUID() const final { return m_configuration.routingContextUID; }
    float sampleRate() const final { return m_configuration.sampleRate; }
    size_t bufferSize() const final { return m_configuration.bufferSize; }
    size_t numberOfOutputChannels() const final { return m_configuration.numberOfOutputChannels; }
    size_t preferredBufferSize() const final { return m_configuration.preferredBufferSize; }
    bool isMuted() const final { return m_configuration.isMuted; }
    bool isActive() const final { return m_configuration.isActive; }

    WebProcess& m_process;
    RemoteAudioSessionConfiguration m_configuration;
};

}

#endif
