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

#include "MessageSenderInlines.h"
#include "RemoteMediaSessionManagerMessages.h"
#include "RemoteMediaSessionManagerProxyMessages.h"
#include "RemoteMediaSessionProxy.h"
#include "RemoteMediaSessionState.h"
#include "SharedPreferencesForWebProcess.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/PlatformMediaSessionInterface.h>
#include <WebCore/PlatformMediaSessionManager.h>
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
    // This manager has no audio session of its own to drive: each content process activates its own with
    // the GPU process. Being the UI process's shared session keeps the configuration this process reports
    // in sync with the content processes and stops a real one being created here.
    AudioSession::setSharedSession(*this);
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
    auto processIdentifier = WebProcessProxy::fromConnection(connection)->coreProcessIdentifier();
    if (RefPtr session = findAndUpdateSession(connection, state))
        removeSession(*session);
    m_sessionProxies.remove({ state.sessionIdentifier, processIdentifier });
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

void RemoteMediaSessionManagerProxy::updateMediaSessionStates(IPC::Connection& connection, WebCore::PageIdentifier pageIdentifier, Vector<RemoteMediaSessionState>&& sessions, uint64_t audioCaptureSourceCount)
{
    refreshSessionStates(connection, sessions);

    auto process = WebProcessProxy::fromConnection(connection)->coreProcessIdentifier();
    WebCore::ProcessQualified<WebCore::PageIdentifier> key { WTF::move(pageIdentifier), process };
    if (!audioCaptureSourceCount)
        m_audioCaptureSourceCountsByPage.remove(key);
    else
        m_audioCaptureSourceCountsByPage.set(key, audioCaptureSourceCount);
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

void RemoteMediaSessionManagerProxy::mediaSessionWillBeginPlayback(IPC::Connection& connection, RemoteMediaSessionState&& state)
{
    // The content process decided whether playback may begin; this runs the part that needs every
    // process's sessions: making this one current, and the concurrent playback restriction.
    if (RefPtr session = findAndUpdateSession(connection, state))
        REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::sessionWillBeginPlayback(*session, [](bool) { });
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
    // configuration.isActive is NOT trusted: it is relayed by the (untrusted) WebContent process. This
    // message carries only the descriptive configuration (category, sample rate, buffer size, routing, ...).
    m_audioConfiguration = WTF::move(configuration);
}

Ref<WebCore::AudioSession::SetActivePromise> RemoteMediaSessionManagerProxy::tryToSetActiveInternal(bool)
{
    // Each content process activates its own audio session with the GPU process, so the UI process has
    // no session of its own to activate.
    return SetActivePromise::createAndResolve();
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
