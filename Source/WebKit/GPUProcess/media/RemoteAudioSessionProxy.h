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
#include <WebCore/ProcessIdentifier.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteAudioSessionProxyManager;

class RemoteAudioSessionProxy
    : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static UniqueRef<RemoteAudioSessionProxy> create(GPUConnectionToWebProcess&);
    virtual ~RemoteAudioSessionProxy();

    WebCore::ProcessIdentifier processIdentifier();
    RemoteAudioSessionConfiguration configuration();

    WebCore::AudioSession::CategoryType category() const { return m_category; };
    WebCore::RouteSharingPolicy routeSharingPolicy() const { return m_routeSharingPolicy; }
    size_t preferredBufferSize() const { return m_preferredBufferSize; }
    bool isActive() const { return m_active; }

    void configurationChanged();
    void beginInterruption();
    void endInterruption(WebCore::AudioSession::MayResume);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

private:
    friend UniqueRef<RemoteAudioSessionProxy> WTF::makeUniqueRefWithoutFastMallocCheck<RemoteAudioSessionProxy>(GPUConnectionToWebProcess&);
    explicit RemoteAudioSessionProxy(GPUConnectionToWebProcess&);

    // Messages
    void setCategory(WebCore::AudioSession::CategoryType, WebCore::RouteSharingPolicy);
    void setPreferredBufferSize(uint64_t);
    using SetActiveCompletion = CompletionHandler<void(bool)>;
    void tryToSetActive(bool, SetActiveCompletion&&);
    void setIsPlayingToBluetoothOverride(std::optional<bool>&& value);

    RemoteAudioSessionProxyManager& audioSessionManager();
    IPC::Connection& connection();

    GPUConnectionToWebProcess& m_gpuConnection;
    WebCore::AudioSession::CategoryType m_category { WebCore::AudioSession::CategoryType::None };
    WebCore::RouteSharingPolicy m_routeSharingPolicy { WebCore::RouteSharingPolicy::Default };
    size_t m_preferredBufferSize { 0 };
    bool m_active { false };
    bool m_isPlayingToBluetoothOverrideChanged { false };
};

}

#endif
