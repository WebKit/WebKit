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

#include "RemoteMediaSessionClientProxy.h"
#include "RemoteMediaSessionManagerMessages.h"
#include "RemoteMediaSessionManagerProxyMessages.h"
#include "RemoteMediaSessionProxy.h"
#include "RemoteMediaSessionState.h"
#include "WebPage.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformMediaSessionInterface.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <algorithm>
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

RefPtr<RemoteMediaSessionManagerProxy> RemoteMediaSessionManagerProxy::create(WebCore::PageIdentifier identifier, WebProcessProxy& process)
{
    return adoptRef(new RemoteMediaSessionManagerProxy(identifier, process));
}

RemoteMediaSessionManagerProxy::RemoteMediaSessionManagerProxy(WebCore::PageIdentifier identifier, WebProcessProxy& process)
    : REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS(identifier)
    , m_process(process)
    , m_topPageID(identifier)
{
#if USE(AUDIO_SESSION)
    AudioSession::setSharedSession(*this);
#endif

#if PLATFORM(COCOA)
    WebCore::AudioHardwareListener::setCreationFunction([protectedThis = Ref { *this }] (WebCore::AudioHardwareListener::Client& client) {
        return protectedThis->ensureAudioHardwareListenerProxy(client);
    });
#endif

    process.addMessageReceiver(Messages::RemoteMediaSessionManagerProxy::messageReceiverName(), m_topPageID, *this);
}

RemoteMediaSessionManagerProxy::~RemoteMediaSessionManagerProxy()
{
    m_process->removeMessageReceiver(Messages::RemoteMediaSessionManagerProxy::messageReceiverName(), m_topPageID);
}

void RemoteMediaSessionManagerProxy::addMediaSession(RemoteMediaSessionState&& state)
{
    auto addResult = m_sessionProxies.ensure(state.sessionIdentifier, [&] {
        return RemoteMediaSessionProxy::create(state, *this);
    });

    Ref session = addResult.iterator->value.get();
    if (!addResult.isNewEntry)
        session->updateState(state);

    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::addSession(session);
}

void RemoteMediaSessionManagerProxy::removeMediaSession(RemoteMediaSessionState&& state)
{
    if (RefPtr session = findSession(state))
        removeSession(*session);
}

void RemoteMediaSessionManagerProxy::setCurrentMediaSession(RemoteMediaSessionState&& state)
{
    if (RefPtr session = findSession(state))
        setCurrentSession(*session);
}

void RemoteMediaSessionManagerProxy::updateMediaSessionState()
{
    updateSessionState();
}

#if USE(AUDIO_SESSION)
void RemoteMediaSessionManagerProxy::remoteAudioConfigurationChanged(RemoteAudioSessionConfiguration&& configuration)
{
    m_audioConfiguration = WTFMove(configuration);
}

void RemoteMediaSessionManagerProxy::setCategory(CategoryType type, Mode mode, WebCore::RouteSharingPolicy policy)
{
#if PLATFORM(COCOA)
    if (type == m_category && mode == m_mode && policy == m_routeSharingPolicy)
        return;

    m_category = type;
    m_mode = mode;
    m_routeSharingPolicy = policy;

    send(Messages::RemoteMediaSessionManager::SetAudioSessionCategory(type, mode, policy), { });
#else
    UNUSED_PARAM(type);
    UNUSED_PARAM(policy);
#endif
}

bool RemoteMediaSessionManagerProxy::tryToSetActiveInternal(bool active)
{
    if (active && m_isInterruptedForTesting)
        return false;

/*
    FIXME: A call to `AudioSession::singleton().tryToSetActive` in the WebProcess ends up in
    FIXME: `RemoteAudioSession::tryToSetActiveInternal`, which sends sync IPC to the GPU process.
    FIXME: This is necessary because the return value, whether or not the audio session was activated,
    FIXME: is used by `MediaSessionManagerInterface::sessionWillBeginPlayback` to know whether to
    FIXME: allow playback to begin. Sync IPC from the UI process isn't a good idea generally, but
    FIXME: sync IPC from the UI to the WebProcess and then to the GPU process is a terrible idea,
    FIXME: so figure out how to restructure the logic to not require it.
    auto sendResult = sendSync(Messages::RemoteMediaSessionManager::TryToSetAudioSessionActive(active), { });
    auto [succeeded] = sendResult.takeReplyOr(false);
 */
    bool succeeded = true;
    if (succeeded)
        m_audioConfiguration.isActive = active;
    return succeeded;
}

void RemoteMediaSessionManagerProxy::setPreferredBufferSize(size_t size)
{
    if (m_audioConfiguration.preferredBufferSize == size)
        return;

    m_audioConfiguration.preferredBufferSize = size;
    send(Messages::RemoteMediaSessionManager::SetAudioSessionPreferredBufferSize(size), { });
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

RefPtr<WebCore::PlatformMediaSessionInterface> RemoteMediaSessionManagerProxy::findSession(RemoteMediaSessionState& state)
{
    return firstSessionMatching([state](auto& session) {
        return session.mediaSessionIdentifier() == state.sessionIdentifier;
    }).get();
}

IPC::Connection* RemoteMediaSessionManagerProxy::messageSenderConnection() const
{
    return &m_process->connection();
}

uint64_t RemoteMediaSessionManagerProxy::messageSenderDestinationID() const
{
    return m_topPageID.toUInt64();
}

std::optional<SharedPreferencesForWebProcess> RemoteMediaSessionManagerProxy::sharedPreferencesForWebProcess() const
{
    return m_process->sharedPreferencesForWebProcess();
}

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
