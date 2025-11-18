/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "UIProcess/Media/RemoteMediaSessionManagerProxy.h"
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "RemoteMediaSessionClientProxy.h"
#include "RemoteMediaSessionManagerProxy.h"
#include "RemoteMediaSessionState.h"
#include <WebCore/PlatformMediaSession.h>

namespace WebKit {
class RemoteMediaSessionManagerProxy;

class RemoteMediaSessionProxy final
    : public WebCore::PlatformMediaSession {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSessionProxy);
public:
    static Ref<RemoteMediaSessionProxy> create(RemoteMediaSessionState& state, RemoteMediaSessionManagerProxy& manager)
    {
        return adoptRef(*new RemoteMediaSessionProxy(state, manager));
    }

    ~RemoteMediaSessionProxy();

    void updateState(const RemoteMediaSessionState&);

private:
    RemoteMediaSessionProxy(const RemoteMediaSessionState&, RemoteMediaSessionManagerProxy&);

    WebCore::MediaSessionIdentifier sessionIdentifier() const { return m_state.sessionIdentifier; }
    WebCore::PageIdentifier pageIdentifier() const { return m_state.pageIdentifier; }

    bool isPlayingToWirelessPlaybackTarget() const final { return m_state.isPlayingToWirelessPlaybackTarget; }

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void setShouldPlayToPlaybackTarget(bool) final;
#endif

    bool isPlayingOnSecondScreen() const final { return m_state.isPlayingOnSecondScreen; }

    std::optional<WebCore::MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const final { return m_state.groupIdentifier; }

    bool hasMediaStreamSource() const final { return m_state.hasMediaStreamSource; }

    bool shouldOverridePauseDuringRouteChange() const final { return m_state.shouldOverridePauseDuringRouteChange; }

    bool isNowPlayingEligible() const final { return m_state.isNowPlayingEligible; }
    std::optional<WebCore::NowPlayingInfo> nowPlayingInfo() const final { return m_state.nowPlayingInfo; }

    WeakPtr<WebCore::PlatformMediaSessionInterface> selectBestMediaSession(const Vector<WeakPtr<WebCore::PlatformMediaSessionInterface>>&, WebCore::PlatformMediaSessionPlaybackControlsPurpose) { return nullptr; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
    Ref<const Logger> protectedLogger() const { return logger(); }
    uint64_t logIdentifier() const { return m_state.logIdentifier; }
#endif

    WeakPtr<RemoteMediaSessionManagerProxy> m_manager;
    RemoteMediaSessionState m_state;
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
#endif
};

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
