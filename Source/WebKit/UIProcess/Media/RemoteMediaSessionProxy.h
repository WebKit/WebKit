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

    WebCore::MediaSessionIdentifier sessionIdentifier() const { return m_sessionState.sessionIdentifier; }
    WebCore::PageIdentifier pageIdentifier() const { return m_sessionState.pageIdentifier; }

    WeakPtr<RemoteMediaSessionManagerProxy> manager() const { return m_manager; }

private:
    RemoteMediaSessionProxy(const RemoteMediaSessionState&, RemoteMediaSessionManagerProxy&);

    WebCore::PlatformMediaSessionState state() const  final { return m_sessionState.state; }
    void setState(WebCore::PlatformMediaSessionState) final;

    bool isPlayingToWirelessPlaybackTarget() const final { return m_sessionState.isPlayingToWirelessPlaybackTarget; }

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void setShouldPlayToPlaybackTarget(bool) final;
#endif

    bool isPlayingOnSecondScreen() const final { return m_sessionState.isPlayingOnSecondScreen; }

    std::optional<WebCore::MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const final { return m_sessionState.groupIdentifier; }

    bool hasMediaStreamSource() const final { return m_sessionState.hasMediaStreamSource; }

    bool shouldOverridePauseDuringRouteChange() const final { return m_sessionState.shouldOverridePauseDuringRouteChange; }

    bool isNowPlayingEligible() const final { return m_sessionState.isNowPlayingEligible; }
    std::optional<WebCore::NowPlayingInfo> nowPlayingInfo() const final { return m_sessionState.nowPlayingInfo; }

    WeakPtr<WebCore::PlatformMediaSessionInterface> selectBestMediaSession(const Vector<WeakPtr<WebCore::PlatformMediaSessionInterface>>&, WebCore::PlatformMediaSessionPlaybackControlsPurpose) final;

    bool isRemoteSessionProxy() const final { return true; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
    uint64_t logIdentifier() const { return m_sessionState.logIdentifier; }
#endif

    WeakPtr<RemoteMediaSessionManagerProxy> m_manager;
    RemoteMediaSessionState m_sessionState;
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteMediaSessionProxy)
static bool isType(const WebCore::PlatformMediaSessionInterface& session) { return session.isRemoteSessionProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
