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

#include "config.h"
#include "RemoteMediaSessionManagerProxy.h"

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "GPUProcessProxy.h"
#include "MessageSenderInlines.h"
#include "RemoteMediaSessionManagerMessages.h"
#include "RemoteMediaSessionManagerProxyMessages.h"
#include "RemoteMediaSessionProxy.h"
#include "RemoteMediaSessionState.h"
#include "SharedPreferencesForWebProcess.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/MediaSessionManagerClient.h>
#include <WebCore/PlatformMediaSessionInterface.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

#if PLATFORM(COCOA)
class RemoteMediaSessionManagerAudioHardwareListener final
    : public WebCore::AudioHardwareListener
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteMediaSessionManagerAudioHardwareListener> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSessionManagerAudioHardwareListener);
public:
    static Ref<RemoteMediaSessionManagerAudioHardwareListener> create(WebCore::AudioHardwareListener::Client& client)
    {
        return adoptRef(*new RemoteMediaSessionManagerAudioHardwareListener(client));
    }
    ~RemoteMediaSessionManagerAudioHardwareListener() = default;

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    RemoteMediaSessionManagerAudioHardwareListener(WebCore::AudioHardwareListener::Client& client)
        : WebCore::AudioHardwareListener(client)
    {
    }

    void audioHardwareDidBecomeActive()
    {
        setHardwareActivity(WebCore::AudioHardwareActivityType::IsActive);
        m_client.audioHardwareDidBecomeActive();
    }

    void audioHardwareDidBecomeInactive()
    {
        setHardwareActivity(WebCore::AudioHardwareActivityType::IsInactive);
        m_client.audioHardwareDidBecomeInactive();
    }

    void audioOutputDeviceChanged(uint64_t bufferSizeMinimum, uint64_t bufferSizeMaximum)
    {
        setSupportedBufferSizes({ bufferSizeMinimum, bufferSizeMaximum });
        m_client.audioOutputDeviceChanged();
    }
};
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionManagerAudioHardwareListener);

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionManagerProxy);

#if USE(AUDIO_SESSION)
// Routes the UI-process singleton's audio-session activation to the GPU process, on behalf of the
// web process that owns the triggering session. NowPlaying routing will be added in a follow-up.
class RemoteMediaSessionManagerProxyClient final : public WebCore::MediaSessionManagerClient {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSessionManagerProxyClient);
public:
    explicit RemoteMediaSessionManagerProxyClient(RemoteMediaSessionManagerProxy& manager)
        : m_manager(manager) { }

private:
    Ref<GenericPromise> tryToSetAudioSessionActive(bool active, WebCore::PlatformMediaSessionInterface* session) final
    {
        RefPtr manager = m_manager.get();
        if (!manager)
            return GenericPromise::createAndReject();

        RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
        if (!gpuProcess)
            return GenericPromise::createAndReject();

        std::optional<WebCore::ProcessIdentifier> target;
        if (active) {
            // Activate the audio session for the process that owns the triggering session.
            if (!session)
                return GenericPromise::createAndReject();

            for (auto& entry : manager->m_sessionProxies) {
                if (entry.value.ptr() == session) {
                    target = entry.key.processIdentifier();
                    break;
                }
            }
            if (!target)
                return GenericPromise::createAndReject();

        } else {
            // Deactivate the same process we activated, so the GPU's per-process aggregation stays
            // balanced regardless of which session is current now. Nothing activated is a no-op.
            target = std::exchange(manager->m_activatedTargetProcess, std::nullopt);
            if (!target)
                return GenericPromise::createAndResolve();
        }

        return gpuProcess->tryToSetAudioSessionActiveForProcess(*target, active)->whenSettled(RunLoop::mainSingleton(), [protectedManager = Ref { *manager }, active, target](auto&& result) -> Ref<GenericPromise> {
            bool succeeded = result.has_value();
            if (active && succeeded)
                protectedManager->m_activatedTargetProcess = target;

            // A failed deactivation means the target proxy is already gone: treat as a no-op success.
            if (succeeded || !active)
                return GenericPromise::createAndResolve();

            return GenericPromise::createAndReject();
        });
    }

    void hasActiveNowPlayingSessionChanged(WebCore::PlatformMediaSessionInterface*) final
    {
        // FIXME: route to the top-level WebPageProxy for the session's page (follow-up).
    }

    WeakPtr<RemoteMediaSessionManagerProxy> m_manager;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionManagerProxyClient);
#endif // USE(AUDIO_SESSION)

static WeakPtr<RemoteMediaSessionManagerProxy>& NODELETE singletonWeakPtr()
{
    static NeverDestroyed<WeakPtr<RemoteMediaSessionManagerProxy>> singleton;
    return singleton;
}

Ref<RemoteMediaSessionManagerProxy> RemoteMediaSessionManagerProxy::singleton()
{
    static NeverDestroyed<Ref<RemoteMediaSessionManagerProxy>> instance { adoptRef(*new RemoteMediaSessionManagerProxy()) };
    singletonWeakPtr() = instance.get();
    return instance.get();
}

RefPtr<RemoteMediaSessionManagerProxy> RemoteMediaSessionManagerProxy::singletonIfCreated()
{
    return singletonWeakPtr().get();
}

RemoteMediaSessionManagerProxy::RemoteMediaSessionManagerProxy()
    : REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS(std::nullopt) // No need to access a WebCore::Page in the UI process
{
#if USE(AUDIO_SESSION)
    // The UI-process singleton is the audio-session authority under site isolation, so it must
    // deactivate the shared session once no web process needs it. Page::ensureMediaSessionManager
    // sets this on a page's manager, but the singleton has no Page, so set it here.
    setShouldDeactivateAudioSession(true);
    AudioSession::setSharedSession(*this);
    setClient(makeUnique<RemoteMediaSessionManagerProxyClient>(*this));
#endif

#if PLATFORM(IOS_FAMILY) || ENABLE(ROUTING_ARBITRATION)
    WebCore::DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(true);
#endif

#if PLATFORM(COCOA)
    WebCore::AudioHardwareListener::setCreationFunction([protectedThis = Ref { *this }] (WebCore::AudioHardwareListener::Client& client) {
        return protectedThis->ensureAudioHardwareListenerProxy(client);
    });
#endif
}

RemoteMediaSessionManagerProxy::~RemoteMediaSessionManagerProxy()
{
}

void RemoteMediaSessionManagerProxy::addMediaSession(IPC::Connection& connection, RemoteMediaSessionState&& state)
{
    Ref process = WebProcessProxy::fromConnection(connection);

    auto addResult = m_sessionProxies.ensure({ state.sessionIdentifier, process->coreProcessIdentifier() }, [&] {
        return RemoteMediaSessionProxy::create(state, process);
    });

    Ref session = addResult.iterator->value.get();
    if (!addResult.isNewEntry)
        session->updateState(state);

    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::addSession(session);
}

void RemoteMediaSessionManagerProxy::removeMediaSession(IPC::Connection& connection, RemoteMediaSessionState&& state)
{
    if (RefPtr session = findAndUpdateSession(connection, state))
        removeSession(*session);
    m_sessionProxies.remove({ state.sessionIdentifier, WebProcessProxy::fromConnection(connection)->coreProcessIdentifier() });
}

void RemoteMediaSessionManagerProxy::webProcessWillShutDown(WebCore::ProcessIdentifier processIdentifier)
{
    Vector<WebCore::ProcessQualified<WebCore::MediaSessionIdentifier>> staleKeys;
    for (auto& key : m_sessionProxies.keys()) {
        if (key.processIdentifier() == processIdentifier)
            staleKeys.append(key);
    }

    for (auto& key : staleKeys) {
        if (RefPtr session = m_sessionProxies.get(key))
            removeSession(*session);
        m_sessionProxies.remove(key);
    }

    // Audio-capture-source counts (getUserMedia) are tracked per page outside m_sessionProxies, so drop
    // this process's entries too; otherwise countActiveAudioCaptureSources() stays inflated and the audio
    // session keeps a record category on behalf of a process that's gone. Re-derive state if anything changed.
    if (m_audioCaptureSourceCountsByPage.removeIf([processIdentifier](auto& entry) {
        return entry.key.processIdentifier() == processIdentifier;
    }))
        updateSessionState();

#if USE(AUDIO_SESSION)
    // If we had activated the audio session on behalf of this now-gone process, forget it: its GPU
    // audio-session proxy was torn down with the process, and removeSession() above already drives the
    // shared session inactive once nothing else needs it. This keeps a later deactivation from being
    // misattributed to a dead process.
    if (m_activatedTargetProcess == processIdentifier)
        m_activatedTargetProcess = std::nullopt;
#endif
}

void RemoteMediaSessionManagerProxy::setCurrentMediaSession(IPC::Connection& connection, RemoteMediaSessionState&& state)
{
    if (RefPtr session = findAndUpdateSession(connection, state))
        setCurrentSession(*session);
}

void RemoteMediaSessionManagerProxy::refreshSessionStates(IPC::Connection& connection, const Vector<RemoteMediaSessionState>& sessions)
{
    Ref process = WebProcessProxy::fromConnection(connection);
    for (auto& state : sessions) {
        if (RefPtr session = m_sessionProxies.get({ state.sessionIdentifier, process->coreProcessIdentifier() }))
            session->updateState(state);
    }
}

void RemoteMediaSessionManagerProxy::updateMediaSessionStates(IPC::Connection& connection, WebCore::PageIdentifier pageIdentifier, Vector<RemoteMediaSessionState>&& sessions, uint64_t audioCaptureSourceCount, CompletionHandler<void(WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy)>&& completionHandler)
{
    refreshSessionStates(connection, sessions);

    WebCore::ProcessQualified<WebCore::PageIdentifier> key { WTF::move(pageIdentifier), WebProcessProxy::fromConnection(connection)->coreProcessIdentifier() };
    if (!audioCaptureSourceCount)
        m_audioCaptureSourceCountsByPage.remove(key);
    else
        m_audioCaptureSourceCountsByPage.set(key, audioCaptureSourceCount);

    updateSessionState();
#if USE(AUDIO_SESSION)
    completionHandler(m_category, m_mode, m_routeSharingPolicy);
#else
    completionHandler(WebCore::AudioSessionCategory::None, WebCore::AudioSessionMode::Default, WebCore::RouteSharingPolicy::Default);
#endif
}

int RemoteMediaSessionManagerProxy::countActiveAudioCaptureSources()
{
    uint64_t total = 0;
    for (auto count : m_audioCaptureSourceCountsByPage.values())
        total += count;
    return static_cast<int>(total);
}

void RemoteMediaSessionManagerProxy::mediaSessionStateChanged(IPC::Connection& connection, WebKit::RemoteMediaSessionState&& state)
{
    findAndUpdateSession(connection, state);
}

void RemoteMediaSessionManagerProxy::setCurrentSession(WebCore::PlatformMediaSessionInterface& session)
{
    if (!m_isInSetCurrentSession) {
        SetForScope isInSetCurrentSessionRestorer(m_isInSetCurrentSession, true);

        for (Ref proxy : m_sessionProxies.values()) {
            auto currentMediaSessionInProcess = proxy.ptr() == &session ? std::optional(proxy->sessionIdentifier()) : std::nullopt;
            proxy->send(Messages::RemoteMediaSessionManager::SetCurrentMediaSession(currentMediaSessionInProcess));
        }
    }

    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::addSession(session);
}

void RemoteMediaSessionManagerProxy::mediaSessionWillBeginPlayback(IPC::Connection& connection, RemoteMediaSessionState&& state, CompletionHandler<void(bool, WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy)>&& completionHandler)
{
    // Reply with the current audio session category so the WebContent process applies it before
    // resuming playback. This makes the play() promise observe the up-to-date category — the
    // capture count has already been folded in via a prior UpdateMediaSessionStates round-trip.
    auto reply = [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](bool granted) mutable {
#if USE(AUDIO_SESSION)
        completionHandler(granted, protectedThis->m_category, protectedThis->m_mode, protectedThis->m_routeSharingPolicy);
#else
        completionHandler(granted, WebCore::AudioSessionCategory::None, WebCore::AudioSessionMode::Default, WebCore::RouteSharingPolicy::Default);
#endif
    };

    RefPtr session = findAndUpdateSession(connection, state);
    if (!session) {
        reply(false);
        return;
    }

    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::sessionWillBeginPlayback(*session, WTF::move(reply));
}

void RemoteMediaSessionManagerProxy::addMediaSessionRestriction(WebCore::PlatformMediaSessionMediaType type, WebCore::MediaSessionRestrictions restrictions)
{
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::addRestriction(type, restrictions);
}

void RemoteMediaSessionManagerProxy::removeMediaSessionRestriction(WebCore::PlatformMediaSessionMediaType type, WebCore::MediaSessionRestrictions restrictions)
{
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::removeRestriction(type, restrictions);
}

void RemoteMediaSessionManagerProxy::resetMediaSessionRestrictions()
{
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::resetRestrictions();
}

#if USE(AUDIO_SESSION)
void RemoteMediaSessionManagerProxy::remoteAudioConfigurationChanged(RemoteAudioSessionConfiguration&& configuration)
{
    m_audioConfiguration = WTF::move(configuration);
}

void RemoteMediaSessionManagerProxy::setCategory(CategoryType type, Mode mode, WebCore::RouteSharingPolicy policy)
{
#if PLATFORM(COCOA)
    if (type == m_category && mode == m_mode && policy == m_routeSharingPolicy)
        return;

    m_category = type;
    m_mode = mode;
    m_routeSharingPolicy = policy;

    for (Ref session : m_sessionProxies.values())
        session->send(Messages::RemoteMediaSessionManager::SetAudioSessionCategory(type, mode, policy));
#else
    UNUSED_PARAM(type);
    UNUSED_PARAM(policy);
#endif
}

Ref<WebCore::AudioSession::SetActivePromise> RemoteMediaSessionManagerProxy::tryToSetActiveInternal(bool active)
{
    if (active && m_isInterruptedForTesting)
        return SetActivePromise::createAndReject();

    return client().tryToSetAudioSessionActive(active, currentSession().get());
}

void RemoteMediaSessionManagerProxy::setPreferredBufferSize(size_t size)
{
    if (m_audioConfiguration.preferredBufferSize == size)
        return;

    m_audioConfiguration.preferredBufferSize = size;

    for (Ref session : m_sessionProxies.values())
        session->send(Messages::RemoteMediaSessionManager::SetAudioSessionPreferredBufferSize(size));
}
#endif

#if PLATFORM(COCOA)
void RemoteMediaSessionManagerProxy::remoteAudioHardwareDidBecomeActive()
{
    if (m_audioHardwareListenerProxy)
        Ref { *m_audioHardwareListenerProxy }->audioHardwareDidBecomeActive();
}

void RemoteMediaSessionManagerProxy::remoteAudioHardwareDidBecomeInactive()
{
    if (m_audioHardwareListenerProxy)
        Ref { *m_audioHardwareListenerProxy }->audioHardwareDidBecomeInactive();
}

void RemoteMediaSessionManagerProxy::remoteAudioOutputDeviceChanged(uint64_t bufferSizeMinimum, uint64_t bufferSizeMaximum)
{
    if (m_audioHardwareListenerProxy)
        Ref { *m_audioHardwareListenerProxy }->audioOutputDeviceChanged(bufferSizeMinimum, bufferSizeMaximum);
}

Ref<RemoteMediaSessionManagerAudioHardwareListener> RemoteMediaSessionManagerProxy::ensureAudioHardwareListenerProxy(WebCore::AudioHardwareListener::Client& client)
{
    if (!m_audioHardwareListenerProxy)
        m_audioHardwareListenerProxy = RemoteMediaSessionManagerAudioHardwareListener::create(client);
    return *m_audioHardwareListenerProxy;
}
#endif

RefPtr<WebCore::PlatformMediaSessionInterface> RemoteMediaSessionManagerProxy::findAndUpdateSession(IPC::Connection& connection, const RemoteMediaSessionState& state)
{
    RefPtr session = m_sessionProxies.get({ state.sessionIdentifier, WebProcessProxy::fromConnection(connection)->coreProcessIdentifier() });
    if (session)
        session->updateState(state);
    return session;
}

std::optional<SharedPreferencesForWebProcess> RemoteMediaSessionManagerProxy::sharedPreferencesForWebProcess(IPC::Connection& connection) const
{
    return WebProcessProxy::fromConnection(connection)->sharedPreferencesForWebProcess();
}

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
