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

#include "GPUProcessConnection.h"
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
    , public GPUProcessConnection::Client
    , IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static UniqueRef<RemoteAudioSession> create(WebProcess&);
    ~RemoteAudioSession();

private:
    friend UniqueRef<RemoteAudioSession> WTF::makeUniqueRefWithoutFastMallocCheck<RemoteAudioSession>(WebProcess&);
    explicit RemoteAudioSession(WebProcess&);
    IPC::Connection& ensureConnection();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void configurationChanged(RemoteAudioSessionConfiguration&&);

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    // AudioSession
    void setCategory(CategoryType, WebCore::RouteSharingPolicy) final;
    CategoryType category() const final;

    WebCore::RouteSharingPolicy routeSharingPolicy() const final { return m_routeSharingPolicy; }
    String routingContextUID() const final { return configuration().routingContextUID; }

    float sampleRate() const final { return configuration().sampleRate; }
    size_t bufferSize() const final { return configuration().bufferSize; }
    size_t numberOfOutputChannels() const final { return configuration().numberOfOutputChannels; }
    size_t maximumNumberOfOutputChannels() const final { return configuration().maximumNumberOfOutputChannels; }

    bool tryToSetActiveInternal(bool) final;

    size_t preferredBufferSize() const final { return configuration().preferredBufferSize; }
    void setPreferredBufferSize(size_t) final;
        
    void addConfigurationChangeObserver(ConfigurationChangeObserver&) final;
    void removeConfigurationChangeObserver(ConfigurationChangeObserver&) final;

    void setIsPlayingToBluetoothOverride(std::optional<bool>) final;

    bool isMuted() const final { return configuration().isMuted; }

    bool isActive() const final { return configuration().isActive; }

    void beginInterruptionForTesting() final;
    void endInterruptionForTesting() final;

    const RemoteAudioSessionConfiguration& configuration() const;
    RemoteAudioSessionConfiguration& configuration();
    void initializeConfigurationIfNecessary();


    WebProcess& m_process;

    WeakHashSet<ConfigurationChangeObserver> m_configurationChangeObservers;
    CategoryType m_category { CategoryType::None };
    WebCore::RouteSharingPolicy m_routeSharingPolicy { WebCore::RouteSharingPolicy::Default };
    bool m_isPlayingToBluetoothOverrideChanged { false };
    std::optional<RemoteAudioSessionConfiguration> m_configuration;
    WeakPtr<GPUProcessConnection> m_gpuProcessConnection;
};

}

#endif
