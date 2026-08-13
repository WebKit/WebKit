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

        if (!active) {
            // The base class only asks to deactivate once no session needs the audio session anymore
            // (hasNoSession), so drop activation for every web process we activated.
            manager->deactivateAllAudioSessions();
            return GenericPromise::createAndResolve();
        }

        // Activate the audio session for the web process that owns the triggering session.
        if (!session)
            return GenericPromise::createAndReject();

        auto target = manager->processForSession(*session);
        if (!target)
            return GenericPromise::createAndReject();

        return manager->enqueueAudioSessionActivation(*target, true);
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

void RemoteMediaSessionManagerProxy::addMediaSession(IPC::Connection& connection, RemoteMediaSessionState&& state, CompletionHandler<void(WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy)>&& completionHandler)
{
    Ref process = WebProcessProxy::fromConnection(connection);

    auto addResult = m_sessionProxies.ensure({ state.sessionIdentifier, process->coreProcessIdentifier() }, [&] {
        return RemoteMediaSessionProxy::create(state, process);
    });

    Ref session = addResult.iterator->value.get();
    if (!addResult.isNewEntry)
        session->updateState(state);

    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::addSession(session);

#if USE(AUDIO_SESSION)
    completionHandler(m_category, m_mode, m_routeSharingPolicy);
#else
    completionHandler(WebCore::AudioSessionCategory::None, WebCore::AudioSessionMode::Default, WebCore::RouteSharingPolicy::Default);
#endif
}

void RemoteMediaSessionManagerProxy::removeMediaSession(IPC::Connection& connection, RemoteMediaSessionState&& state)
{
    auto processIdentifier = WebProcessProxy::fromConnection(connection)->coreProcessIdentifier();
    if (RefPtr session = findAndUpdateSession(connection, state))
        removeSession(*session);
    m_sessionProxies.remove({ state.sessionIdentifier, processIdentifier });

#if USE(AUDIO_SESSION)
    // If that was the process's last session and it is not still capturing audio, drive its audio session
    // inactive and clear the per-process activation state now -- decoupled from the global
    // maybeDeactivateAudioSession() path (gated on m_becameActive, which can't track per-process state).
    // Otherwise a reused process (same-origin navigation) keeps a stale "active" flag and the next document's
    // sessionWillBeginPlayback skips activation even though the process's audio session was reset.
    bool processStillHasSession = false;
    for (auto& key : m_sessionProxies.keys()) {
        if (key.processIdentifier() == processIdentifier) {
            processStillHasSession = true;
            break;
        }
    }
    if (!processStillHasSession && !processRequiresAudioSession(processIdentifier)) {
        auto it = m_activationByProcess.find(processIdentifier);
        if (it != m_activationByProcess.end() && it->value.active)
            enqueueAudioSessionActivation(processIdentifier, false)->whenSettled(RunLoop::mainSingleton(), [](auto&&) { });
    }
#endif
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
    // Drop any override that process had set, so it no longer affects the category.
    if (m_categoryOverridesByPage.removeIf([processIdentifier](auto& entry) {
        return entry.key.processIdentifier() == processIdentifier;
    }))
        updateSessionState();

    // The process is gone; drop the activation state we tracked for it (keyed by ProcessIdentifier, so a
    // reused process starts clean). The pending activations' waiters are AutoRejectProducers, so removing the
    // entry rejects any still-pending promises as it is destroyed. setAudioSessionActiveForProcess() relies on
    // this removal (it uses find(), not ensure(), so it never resurrects a departed process's entry).
    m_activationByProcess.remove(processIdentifier);
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

void RemoteMediaSessionManagerProxy::updateMediaSessionStates(IPC::Connection& connection, WebCore::PageIdentifier pageIdentifier, Vector<RemoteMediaSessionState>&& sessions, uint64_t audioCaptureSourceCount, WebCore::AudioSessionCategory categoryOverride, CompletionHandler<void(WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy)>&& completionHandler)
{
    refreshSessionStates(connection, sessions);

    auto process = WebProcessProxy::fromConnection(connection)->coreProcessIdentifier();
    WebCore::ProcessQualified<WebCore::PageIdentifier> key { WTF::move(pageIdentifier), process };
#if USE(AUDIO_SESSION)
    uint64_t previousPageCaptureCount = m_audioCaptureSourceCountsByPage.get(key);
#endif
    if (!audioCaptureSourceCount)
        m_audioCaptureSourceCountsByPage.remove(key);
    else
        m_audioCaptureSourceCountsByPage.set(key, audioCaptureSourceCount);

#if USE(AUDIO_SESSION)
    // Each page reports its own override. Keep them separate so that a page without one does not clear
    // the override another page has set.
    if (categoryOverride == WebCore::AudioSessionCategory::None)
        m_categoryOverridesByPage.remove(key);
    else
        m_categoryOverridesByPage.set(key, categoryOverride);
#else
    UNUSED_PARAM(categoryOverride);
#endif

    updateSessionState();

#if USE(AUDIO_SESSION)
    // Drive audio-session activation for this process's audio capture: activeAudioSessionRequired() can't see
    // capture from the UI process (only the count is forwarded, not the AudioCaptureSource objects), so key off
    // the capture-count transition -- activate when it starts, deactivate when it stops and nothing else needs it.
    if (audioCaptureSourceCount && !previousPageCaptureCount)
        enqueueAudioSessionActivation(process, true)->whenSettled(RunLoop::mainSingleton(), [](auto&&) { });
    else if (!audioCaptureSourceCount && previousPageCaptureCount && !processRequiresAudioSession(process))
        enqueueAudioSessionActivation(process, false)->whenSettled(RunLoop::mainSingleton(), [](auto&&) { });

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

    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::setCurrentSession(session);
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

#if USE(AUDIO_SESSION)
    // Mirror this process's real audio-session active state onto the shared session's active flag (which
    // the base sessionWillBeginPlayback reads via AudioSession::singleton().isActive() on its fast-activation
    // path) before the base activation gate, so the gate decides on this process's reality. The value is the
    // authoritative per-process state the GPU process reports (via setAudioSessionActiveForProcess()) plus the
    // activation chain (NOT a WebContent-supplied bool); the per-process entry the base reads via hasActiveAudioSession()
    // is already up to date. Key by the connection's process (trusted), not by any value the web process
    // sent. setActive() only sets the flag; it fires no observers.
    auto processIdentifier = WebProcessProxy::fromConnection(connection)->coreProcessIdentifier();
    auto it = m_activationByProcess.find(processIdentifier);
    AudioSession::setActive(it != m_activationByProcess.end() && it->value.active);
#endif

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
WebCore::AudioSessionCategory RemoteMediaSessionManagerProxy::categoryOverride() const
{
    // Use the override of any page that has set one. Pages without an override have no entry here.
    for (auto category : m_categoryOverridesByPage.values()) {
        if (category != WebCore::AudioSessionCategory::None)
            return category;
    }
    return WebCore::AudioSessionCategory::None;
}

void RemoteMediaSessionManagerProxy::remoteAudioConfigurationChanged(RemoteAudioSessionConfiguration&& configuration)
{
    // configuration.isActive is NOT trusted for the activation gate: it is relayed by the (untrusted)
    // WebContent process. Each process's active state is derived from the GPU process via
    // setAudioSessionActiveForProcess(); this message carries only the descriptive configuration
    // (category, sample rate, buffer size, routing, ...).
    m_audioConfiguration = WTF::move(configuration);
}

void RemoteMediaSessionManagerProxy::setAudioSessionActiveForProcess(WebCore::ProcessIdentifier process, bool active)
{
    // Correct the cached active state of a process the UI process is already tracking, pushed by the GPU
    // process (see GPUProcessProxy::audioSessionActiveStateChangedForProcess). Use find(), NOT ensure():
    // creating an entry here could resurrect one that webProcessWillShutDown() removed while an activation
    // IPC was still in flight, and the stale reply would then crash sendNextActivationIPC()'s completion on
    // an empty chain. A process we aren't tracking has no cache to correct -- its next gate just activates.
    auto it = m_activationByProcess.find(process);
    if (it == m_activationByProcess.end())
        return;
    // Don't clobber an in-flight UI-driven activation/deactivation: while its chain is pending the
    // optimistic state governs (that is what lets stop->replay re-activate), and the chain reverts on
    // failure / otherwise converges to what the GPU process reports. Adopt the GPU process's value only
    // when the process's chain is quiescent.
    auto& state = it->value;
    if (state.ipcInFlight || !state.pendingChain.isEmpty())
        return;
    state.active = active;
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

void RemoteMediaSessionManagerProxy::reevaluateAudioSessionActivation(WebCore::PlatformMediaSessionInterface& session)
{
    scheduleUpdateSessionState();

    auto process = processForSession(session);
    if (!process)
        return;

    // Gate on the session's own process, not the manager-wide activeAudioSessionRequired(): otherwise an
    // unrelated process holding an audible session would let this process -- driven by its own untrusted
    // state update -- activate the shared session and cache active=true for itself, short-circuiting its
    // later playback-activation gate. Mirrors remoteProcessDidResume().
    if (!processRequiresAudioSession(*process))
        return;

    enqueueAudioSessionActivation(*process, true);
}

void RemoteMediaSessionManagerProxy::remoteProcessWillSuspend(IPC::Connection& connection)
{
    // The content process is suspending and no longer drives the shared audio session, so release it here;
    // otherwise a suspended process would keep the system session active (matching the pre-site-isolation
    // behavior where the web process released it on suspend).
    auto process = WebProcessProxy::fromConnection(connection)->coreProcessIdentifier();
    enqueueAudioSessionActivation(process, false);
}

void RemoteMediaSessionManagerProxy::remoteProcessDidResume(IPC::Connection& connection)
{
    auto process = WebProcessProxy::fromConnection(connection)->coreProcessIdentifier();
    if (processRequiresAudioSession(process))
        enqueueAudioSessionActivation(process, true);
}

bool RemoteMediaSessionManagerProxy::hasActiveAudioSession(WebCore::PlatformMediaSessionInterface& session) const
{
    // Track activation per web process instead of with the base class's single flag: under site
    // isolation one manager serves every process, so a session in one process must not make
    // sessionWillBeginPlayback treat a session in another process as already active (which would skip
    // activating the latter). Emulates the non-site-isolation model where each process has its own manager.
    auto process = processForSession(session);
    if (!process)
        return false;
    auto it = m_activationByProcess.find(*process);
    return it != m_activationByProcess.end() && it->value.active;
}

std::optional<WebCore::ProcessIdentifier> RemoteMediaSessionManagerProxy::processForSession(const WebCore::PlatformMediaSessionInterface& session) const
{
    for (auto& entry : m_sessionProxies) {
        if (entry.value.ptr() == &session)
            return entry.key.processIdentifier();
    }
    return std::nullopt;
}

bool RemoteMediaSessionManagerProxy::processRequiresAudioSession(WebCore::ProcessIdentifier process) const
{
    for (auto& entry : m_audioCaptureSourceCountsByPage) {
        if (entry.key.processIdentifier() == process && entry.value)
            return true;
    }
    for (auto& entry : m_sessionProxies) {
        if (entry.key.processIdentifier() != process)
            continue;
        Ref session = entry.value;
        if (session->activeAudioSessionRequired())
            return true;
    }
    return false;
}

Ref<WebCore::AudioSession::SetActivePromise> RemoteMediaSessionManagerProxy::enqueueAudioSessionActivation(WebCore::ProcessIdentifier process, bool active)
{
    auto& state = m_activationByProcess.ensure(process, [] {
        return ProcessActivationState { };
    }).iterator->value;

    SetActivePromise::AutoRejectProducer producer;
    Ref promise = producer.promise();

    // Optimistically reflect the requested state so hasActiveAudioSession() sees it without waiting
    // for the GPU process round-trip. This is what lets a stop->replay re-activate: the replay's gate sees the
    // process as inactive (set when the deactivation was enqueued) even while that deactivation IPC is
    // still in flight. Mirrors RemoteAudioSession's optimistic setActive.
    state.active = active;

    // Keep at most one activation IPC per process in flight, coalescing consecutive same-state requests
    // against the tail; a different-state request appends a new entry dispatched when the previous reply
    // arrives. This serializes an in-flight deactivation and a following activation so the last state wins.
    if (!state.pendingChain.isEmpty() && state.pendingChain.last().active == active) {
        state.pendingChain.last().waiters.append(WTF::move(producer));
        return promise;
    }

    PendingActivation entry { active, { } };
    entry.waiters.append(WTF::move(producer));
    state.pendingChain.append(WTF::move(entry));

    sendNextActivationIPC(process);
    return promise;
}

void RemoteMediaSessionManagerProxy::deactivateAllAudioSessions()
{
    Vector<WebCore::ProcessIdentifier> processes;
    for (auto& entry : m_activationByProcess) {
        if (entry.value.active)
            processes.append(entry.key);
    }
    for (auto process : processes)
        enqueueAudioSessionActivation(process, false);
}

void RemoteMediaSessionManagerProxy::sendNextActivationIPC(WebCore::ProcessIdentifier process)
{
    auto it = m_activationByProcess.find(process);
    if (it == m_activationByProcess.end() || it->value.ipcInFlight || it->value.pendingChain.isEmpty())
        return;

    bool active = it->value.pendingChain.first().active;
    it->value.ipcInFlight = true;

    auto processResult = [protectedThis = Ref { *this }, process, active](bool succeeded) mutable {
        auto it = protectedThis->m_activationByProcess.find(process);
        if (it == protectedThis->m_activationByProcess.end())
            return;

        // The chain should be non-empty while an IPC we sent is outstanding. An empty chain here means a
        // stale reply for an entry that was reset (e.g. removed at process shutdown then re-created) --
        // unexpected, so ASSERT to catch it in debug (this assert is what surfaced the ensure()-resurrection
        // bug on the bots), but tolerate it in release rather than take from an empty chain.
        ASSERT(!it->value.pendingChain.isEmpty());
        if (it->value.pendingChain.isEmpty())
            return;

        ASSERT(it->value.pendingChain.first().active == active);
        auto pending = it->value.pendingChain.takeFirst();
        it->value.ipcInFlight = false;

        // If the request failed and nothing newer superseded it, revert the optimistic state.
        if (!succeeded && it->value.pendingChain.isEmpty())
            it->value.active = !active;

        protectedThis->sendNextActivationIPC(process);

        for (auto& waiter : pending.waiters) {
            if (succeeded)
                waiter.resolve();
            else
                waiter.reject();
        }
    };

    RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
    if (!gpuProcess) {
        processResult(false);
        return;
    }

    gpuProcess->tryToSetAudioSessionActiveForProcess(process, active)->whenSettled(RunLoop::mainSingleton(),
        [processResult = WTF::move(processResult)](auto&& result) mutable {
            processResult(result.has_value());
        });
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
