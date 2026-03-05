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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteMediaSessionState.h"
#include "SharedPreferencesForWebProcess.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/MediaSessionIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakListHashSet.h>

#if PLATFORM(IOS_FAMILY)
#include <WebCore/MediaSessionManagerIOS.h>
#define REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS MediaSessionManageriOS
#elif PLATFORM(COCOA)
#include <WebCore/MediaSessionManagerCocoa.h>
#define REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS MediaSessionManagerCocoa
#else
#include <WebCore/PlatformMediaSessionManager.h>
#define REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS PlatformMediaSessionManager
#endif

namespace WebKit {

class WebPage;

class RemoteMediaSessionManager
    : public WebCore::REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS
    , public IPC::MessageReceiver
    , public IPC::MessageSender {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSessionManager);
public:
    static RefPtr<RemoteMediaSessionManager> create(WebPage& topPage, WebPage& localPage);

    virtual ~RemoteMediaSessionManager();

    void ref() const final { WebCore::REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::ref(); }
    void deref() const final { WebCore::REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::deref(); }

protected:
    RemoteMediaSessionManager(WebPage& topPage, WebPage& localPage);

    // Messages
    void clientShouldResumeAutoplaying(WebCore::MediaSessionIdentifier);
    void clientMayResumePlayback(WebCore::MediaSessionIdentifier, bool);
    void clientShouldSuspendPlayback(WebCore::MediaSessionIdentifier);
    void clientSetShouldPlayToPlaybackTarget(WebCore::MediaSessionIdentifier, bool);
    void clientDidReceiveRemoteControlCommand(WebCore::MediaSessionIdentifier, WebCore::PlatformMediaSessionRemoteControlCommandType, WebCore::PlatformMediaSessionRemoteCommandArgument);
    void setCurrentMediaSession(std::optional<WebCore::MediaSessionIdentifier>);

#if USE(AUDIO_SESSION)
    void setAudioSessionCategory(WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy);
    void setAudioSessionPreferredBufferSize(uint64_t);
    void tryToSetAudioSessionActive(bool);
#endif

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

private:
    RemoteMediaSessionState& currentSessionState(const WebCore::PlatformMediaSessionInterface&);
    RemoteMediaSessionState fullSessionState(const WebCore::PlatformMediaSessionInterface&);
    void updateCachedSessionState(const WebCore::PlatformMediaSessionInterface&, RemoteMediaSessionState&);

    void addSession(WebCore::PlatformMediaSessionInterface&) final;
    void removeSession(WebCore::PlatformMediaSessionInterface&) final;
    void setCurrentSession(WebCore::PlatformMediaSessionInterface&) final;
    void sessionWillBeginPlayback(WebCore::PlatformMediaSessionInterface&, CompletionHandler<void(bool)>&&) final;
    void updateSessionState() final;
    void sessionStateChanged(WebCore::PlatformMediaSessionInterface&) final;

    void addRestriction(WebCore::PlatformMediaSessionMediaType, WebCore::MediaSessionRestrictions) final;
    void removeRestriction(WebCore::PlatformMediaSessionMediaType, WebCore::MediaSessionRestrictions) final;
    void resetRestrictions() final;

    RefPtr<WebCore::PlatformMediaSessionInterface> sessionWithIdentifier(WebCore::MediaSessionIdentifier);

#if PLATFORM(COCOA)
    // AudioHardwareListenerClient
    void audioHardwareDidBecomeActive() final;
    void audioHardwareDidBecomeInactive() final;
    void audioOutputDeviceChanged() final;
#endif

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final;
#endif

    WeakPtr<WebPage> m_topPage;
    WeakPtr<WebPage> m_localPage;
    WebCore::PageIdentifier m_topPageID;
    WebCore::PageIdentifier m_localPageID;
    HashMap<WebCore::MediaSessionIdentifier, UniqueRef<RemoteMediaSessionState>> m_cachedSessionState;
};

#if !RELEASE_LOG_DISABLED
inline ASCIILiteral RemoteMediaSessionManager::logClassName() const { return "RemoteMediaSessionManager"_s; }
#endif

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

