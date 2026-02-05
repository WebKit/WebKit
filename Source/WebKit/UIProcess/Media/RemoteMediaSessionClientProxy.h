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

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "RemoteMediaSessionState.h"
#include <WebCore/PlatformMediaSessionInterface.h>
#include <WebCore/PlatformMediaSessionTypes.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class RemoteMediaSessionManagerProxy;

class RemoteMediaSessionClientProxy final
    : public WebCore::PlatformMediaSessionClient
    , public RefCounted<RemoteMediaSessionClientProxy> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSessionClientProxy);
public:
    RemoteMediaSessionClientProxy(const RemoteMediaSessionState&, RemoteMediaSessionManagerProxy&);
    virtual ~RemoteMediaSessionClientProxy();

    WebCore::MediaSessionIdentifier sessionIdentifier() const { return m_state.sessionIdentifier; }

    void updateState(const RemoteMediaSessionState& state) { m_state = state; }

#if !RELEASE_LOG_DISABLED
    uint64_t logIdentifier() const { return m_state.logIdentifier; }
#endif

    WebCore::PageIdentifier pageIdentifier() const { return m_state.pageIdentifier; }

    bool isRemoteSessionClientProxy() const final { return true; }

protected:

    RefPtr<WebCore::MediaSessionManagerInterface> sessionManager() const final;

    WebCore::PlatformMediaSessionMediaType mediaType() const final { return m_state.mediaType; }
    WebCore::PlatformMediaSessionMediaType presentationType() const final { return m_state.presentationType; }
    WebCore::PlatformMediaSessionDisplayType displayType() const final { return m_state.displayType; }

    void resumeAutoplaying() final;
    void mayResumePlayback(bool shouldResume) final;
    void suspendPlayback() final;

    bool canReceiveRemoteControlCommands() const final { return m_state.canReceiveRemoteControlCommands; }
    void didReceiveRemoteControlCommand(WebCore::PlatformMediaSessionRemoteControlCommandType, const WebCore::PlatformMediaSessionRemoteCommandArgument&) final;
    bool supportsSeeking() const final { return m_state.supportsSeeking; }

    bool canProduceAudio() const final { return m_state.canProduceAudio; }
    bool isSuspended() const final { return m_state.isSuspended; }
    bool isPlaying() const final { return m_state.isPlaying; }
    bool isAudible() const final { return m_state.isAudible; }
    bool isEnded() const final { return m_state.isEnded; }
    MediaTime mediaSessionDuration() const final { return m_state.duration; }

    bool shouldOverrideBackgroundPlaybackRestriction(WebCore::PlatformMediaSessionInterruptionType) const final;
    bool shouldOverrideBackgroundLoadingRestriction() const final { return m_state.shouldOverrideBackgroundLoadingRestriction; }

    bool isPlayingToWirelessPlaybackTarget() const final { return m_state.isPlayingToWirelessPlaybackTarget; }
    void setShouldPlayToPlaybackTarget(bool) final;

    bool isPlayingOnSecondScreen() const final { return m_state.isPlayingOnSecondScreen; }

    std::optional<WebCore::MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const final { return m_state.groupIdentifier; }

    bool hasMediaStreamSource() const final { return m_state.hasMediaStreamSource; }

    bool shouldOverridePauseDuringRouteChange() const final { return m_state.shouldOverridePauseDuringRouteChange; }

    bool isNowPlayingEligible() const final { return m_state.isNowPlayingEligible; }
    std::optional<WebCore::NowPlayingInfo> nowPlayingInfo() const final { return m_state.nowPlayingInfo; }

    WeakPtr<WebCore::PlatformMediaSessionInterface> selectBestMediaSession(const Vector<WeakPtr<WebCore::PlatformMediaSessionInterface>>&, WebCore::PlatformMediaSessionPlaybackControlsPurpose) { return nullptr; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
#endif

private:
    WeakPtr<RemoteMediaSessionManagerProxy> m_manager;
    RemoteMediaSessionState m_state;
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteMediaSessionClientProxy)
static bool isType(const WebCore::PlatformMediaSessionClient& session) { return session.isRemoteSessionClientProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
