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
#include "RemoteAudioSessionConfiguration.h"
#include <WebCore/AudioHardwareListener.h>
#include <WebCore/AudioSession.h>
#include <WebCore/MediaSessionIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessQualified.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/NativePromise.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

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
class RemoteMediaSessionManagerProxyClient;
class RemoteMediaSessionProxy;
class WebPageProxy;
class WebProcessProxy;
struct RemoteMediaSessionState;
struct SharedPreferencesForWebProcess;

class RemoteMediaSessionManagerProxy
    : public WebCore::REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS
#if USE(AUDIO_SESSION)
    , public WebCore::AudioSession
#endif
    , public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSessionManagerProxy);
    friend class RemoteMediaSessionManagerProxyClient;
public:
    USING_CAN_MAKE_WEAKPTR(MessageReceiver);

    static Ref<RemoteMediaSessionManagerProxy> singleton();
    static RefPtr<RemoteMediaSessionManagerProxy> singletonIfCreated();

    virtual ~RemoteMediaSessionManagerProxy();

    void webProcessWillShutDown(WebCore::ProcessIdentifier);

#if USE(AUDIO_SESSION)
    // Called by a RemoteMediaSessionProxy when its session's state changes. Under site isolation the content
    // process keeps only an optimistic local audio-session state and does not drive the GPU (see
    // RemoteAudioSession::sendNextActivationIPC); the UI process is the sole activation driver, so it activates
    // the given session's process here when the session requires an active audio session (e.g. WebAudio, which
    // becomes audible only after begin).
    void reevaluateAudioSessionActivation(WebCore::PlatformMediaSessionInterface&);

    // Called by GPUProcessProxy with the GPU-authoritative per-process audio-session active state (the
    // trusted source), so the activation gate never depends on a value sent by the WebContent process.
    void setAudioSessionActiveForProcess(WebCore::ProcessIdentifier, bool active);
#endif

    // IPC::MessageReceiver, WebCore::AudioSession.
    void ref() const final { WebCore::REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::ref(); }
    void deref() const final { WebCore::REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::deref(); }

#if USE(AUDIO_SESSION)
    // WebCore::AudioSession.
    ThreadSafeWeakPtrControlBlock& controlBlock() const final { return REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::controlBlock(); }
    uint32_t weakRefCount() const final { return REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::weakRefCount(); }
#endif

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

private:
    RemoteMediaSessionManagerProxy();

    // Messages
    void addMediaSession(IPC::Connection&, RemoteMediaSessionState&&, CompletionHandler<void(WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy)>&&);
    void removeMediaSession(IPC::Connection&, RemoteMediaSessionState&&);
    void setCurrentMediaSession(IPC::Connection&, RemoteMediaSessionState&&);
    void updateMediaSessionStates(IPC::Connection&, WebCore::PageIdentifier, Vector<RemoteMediaSessionState>&&, uint64_t audioCaptureSourceCount, WebCore::AudioSessionCategory categoryOverride, CompletionHandler<void(WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy)>&&);
    void mediaSessionStateChanged(IPC::Connection&, WebKit::RemoteMediaSessionState&&);
    void mediaSessionWillBeginPlayback(IPC::Connection&, RemoteMediaSessionState&&, CompletionHandler<void(bool, WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy)>&&);

    void setCurrentSession(WebCore::PlatformMediaSessionInterface&) final;

    void addMediaSessionRestriction(WebCore::PlatformMediaSessionMediaType, WebCore::MediaSessionRestrictions);
    void removeMediaSessionRestriction(WebCore::PlatformMediaSessionMediaType, WebCore::MediaSessionRestrictions);
    void resetMediaSessionRestrictions();

    int countActiveAudioCaptureSources() final;

#if PLATFORM(COCOA)
    void remoteAudioHardwareDidBecomeActive();
    void remoteAudioHardwareDidBecomeInactive();
    void remoteAudioOutputDeviceChanged(uint64_t bufferSizeMinimum, uint64_t bufferSizeMaximum);
#endif

#if USE(AUDIO_SESSION)
    void remoteAudioConfigurationChanged(RemoteAudioSessionConfiguration&&);
    void remoteProcessWillSuspend(IPC::Connection&);
    void remoteProcessDidResume(IPC::Connection&);

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

    Ref<SetActivePromise> tryToSetActiveInternal(bool) final;
    bool hasActiveAudioSession(WebCore::PlatformMediaSessionInterface&) const final;
    std::optional<WebCore::ProcessIdentifier> processForSession(const WebCore::PlatformMediaSessionInterface&) const;
    bool processRequiresAudioSession(WebCore::ProcessIdentifier) const;
    Ref<SetActivePromise> enqueueAudioSessionActivation(WebCore::ProcessIdentifier, bool active);
    void deactivateAllAudioSessions();
    void sendNextActivationIPC(WebCore::ProcessIdentifier);

    size_t preferredBufferSize() const final { return m_audioConfiguration.preferredBufferSize; }
    void setPreferredBufferSize(size_t) final;

    CategoryType categoryOverride() const final;
#endif

    RefPtr<WebCore::PlatformMediaSessionInterface> findAndUpdateSession(IPC::Connection&, const RemoteMediaSessionState&);
    void refreshSessionStates(IPC::Connection&, const Vector<RemoteMediaSessionState>&);
    Ref<RemoteMediaSessionManagerAudioHardwareListener> ensureAudioHardwareListenerProxy(WebCore::AudioHardwareListener::Client&);

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess(IPC::Connection&) const;

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final;
#endif

    HashMap<WebCore::ProcessQualified<WebCore::MediaSessionIdentifier>, Ref<RemoteMediaSessionProxy>> m_sessionProxies;
    HashMap<WebCore::ProcessQualified<WebCore::PageIdentifier>, uint64_t> m_audioCaptureSourceCountsByPage;
#if USE(AUDIO_SESSION)
    HashMap<WebCore::ProcessQualified<WebCore::PageIdentifier>, WebCore::AudioSessionCategory> m_categoryOverridesByPage;
#endif

#if PLATFORM(COCOA)
    RefPtr<RemoteMediaSessionManagerAudioHardwareListener> m_audioHardwareListenerProxy;
#endif

#if USE(AUDIO_SESSION)
    CategoryType m_category { CategoryType::None };
    Mode m_mode { Mode::Default };
    WebCore::RouteSharingPolicy m_routeSharingPolicy { WebCore::RouteSharingPolicy::Default };
    mutable RemoteAudioSessionConfiguration m_audioConfiguration;

    struct PendingActivation {
        bool active;
        Vector<WebCore::AudioSession::SetActivePromise::AutoRejectProducer> waiters;
    };
    struct ProcessActivationState {
        bool active { false };
        Deque<PendingActivation> pendingChain;
        bool ipcInFlight { false };
    };
    HashMap<WebCore::ProcessIdentifier, ProcessActivationState> m_activationByProcess;
#endif

    bool m_isInterruptedForTesting { false };
    bool m_isInSetCurrentSession { false };
};

#if !RELEASE_LOG_DISABLED
inline ASCIILiteral RemoteMediaSessionManagerProxy::logClassName() const { return "RemoteMediaSessionManagerProxy"_s; }
#endif

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
