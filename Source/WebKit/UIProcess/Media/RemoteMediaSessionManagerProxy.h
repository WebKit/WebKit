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
#include "RemoteAudioSessionConfiguration.h"
#include "WebProcessProxy.h"
#include <WebCore/AudioHardwareListener.h>
#include <WebCore/AudioSession.h>
#include <WebCore/MediaSessionIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

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

namespace WebCore {
class PlatformMediaSessionInterface;
}

namespace WebKit {

class RemoteMediaSessionManagerAudioHardwareListener;
class RemoteMediaSessionProxy;
class WebProcessProxy;
struct RemoteMediaSessionState;

class RemoteMediaSessionManagerProxy
    : public WebCore::REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS
#if USE(AUDIO_SESSION)
    , public WebCore::AudioSession
#endif
    , public IPC::MessageReceiver
    , public IPC::MessageSender {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSessionManagerProxy);
public:
    USING_CAN_MAKE_WEAKPTR(MessageReceiver);

    static RefPtr<RemoteMediaSessionManagerProxy> create(WebCore::PageIdentifier, WebProcessProxy&);

    virtual ~RemoteMediaSessionManagerProxy();

#if USE(AUDIO_SESSION)
    void ref() const final { AudioSession::ref(); }
    void deref() const final { AudioSession::deref(); }
#else
    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }
#endif

    const Ref<WebProcessProxy> process() const { return m_process; }

private:
    RemoteMediaSessionManagerProxy(WebCore::PageIdentifier, WebProcessProxy&);

    // Messages
    void addMediaSession(RemoteMediaSessionState&&);
    void removeMediaSession(RemoteMediaSessionState&&);
    void setCurrentMediaSession(RemoteMediaSessionState&&);
    void updateMediaSessionState();

#if PLATFORM(COCOA)
    void remoteAudioHardwareDidBecomeActive();
    void remoteAudioHardwareDidBecomeInactive();
    void remoteAudioOutputDeviceChanged(uint64_t bufferSizeMinimum, uint64_t bufferSizeMaximum);
#endif

#if USE(AUDIO_SESSION)
    void remoteAudioConfigurationChanged(RemoteAudioSessionConfiguration&&);

    // AudioSession
    void setCategory(CategoryType, Mode, WebCore::RouteSharingPolicy) final;
    CategoryType category() const final { return m_category; }
    Mode mode() const final { return m_mode; }

    WebCore::RouteSharingPolicy routeSharingPolicy() const final { return m_routeSharingPolicy; }
    String routingContextUID() const final { return m_audioConfiguration.routingContextUID; }

    float sampleRate() const final { return m_audioConfiguration.sampleRate; }
    size_t bufferSize() const final { return m_audioConfiguration.bufferSize; }
    size_t numberOfOutputChannels() const final { return m_audioConfiguration.numberOfOutputChannels; }
    size_t maximumNumberOfOutputChannels() const final { return m_audioConfiguration.maximumNumberOfOutputChannels; }
    size_t outputLatency() const final { return m_audioConfiguration.outputLatency; }

    bool tryToSetActiveInternal(bool) final;

    size_t preferredBufferSize() const final { return m_audioConfiguration.preferredBufferSize; }
    void setPreferredBufferSize(size_t) final;

    CategoryType categoryOverride() const final  { return m_audioConfiguration.categoryOverride; }
#endif

    RefPtr<WebCore::PlatformMediaSessionInterface> findSession(RemoteMediaSessionState&);
    Ref<RemoteMediaSessionManagerAudioHardwareListener> ensureAudioHardwareListenerProxy(WebCore::AudioHardwareListener::Client&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final;
#endif

    const Ref<WebProcessProxy> m_process;
    WebCore::PageIdentifier m_topPageID;
    HashMap<WebCore::MediaSessionIdentifier, Ref<RemoteMediaSessionProxy>> m_sessionProxies;

#if PLATFORM(COCOA)
    RefPtr<RemoteMediaSessionManagerAudioHardwareListener> m_audioHardwareListenerProxy;
#endif

#if USE(AUDIO_SESSION)
    CategoryType m_category { CategoryType::None };
    Mode m_mode { Mode::Default };
    WebCore::RouteSharingPolicy m_routeSharingPolicy { WebCore::RouteSharingPolicy::Default };
    mutable RemoteAudioSessionConfiguration m_audioConfiguration;
#endif

    bool m_isInterruptedForTesting { false };
};

#if !RELEASE_LOG_DISABLED
inline ASCIILiteral RemoteMediaSessionManagerProxy::logClassName() const { return "RemoteMediaSessionManagerProxy"_s; }
#endif

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
