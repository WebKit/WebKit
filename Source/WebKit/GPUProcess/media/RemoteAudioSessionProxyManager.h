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

#include <WebCore/AudioSession.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/WeakHashSet.h>

namespace WebKit {

class GPUProcess;
class RemoteAudioSessionProxy;

class RemoteAudioSessionProxyManager
    : public WebCore::AudioSession::InterruptionObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteAudioSessionProxyManager();
    ~RemoteAudioSessionProxyManager();

    void addProxy(RemoteAudioSessionProxy&);
    void removeProxy(RemoteAudioSessionProxy&);

    void setCategoryForProcess(RemoteAudioSessionProxy&, WebCore::AudioSession::CategoryType, WebCore::RouteSharingPolicy);
    void setPreferredBufferSizeForProcess(RemoteAudioSessionProxy&, size_t);

    bool tryToSetActiveForProcess(RemoteAudioSessionProxy&, bool);

    const WebCore::AudioSession& session() const { return m_session; }

private:
    void beginAudioSessionInterruption() final;
    void endAudioSessionInterruption(WebCore::AudioSession::MayResume) final;

    UniqueRef<WebCore::AudioSession> m_session;
    WeakHashSet<RemoteAudioSessionProxy> m_proxies;
};

}

#endif
