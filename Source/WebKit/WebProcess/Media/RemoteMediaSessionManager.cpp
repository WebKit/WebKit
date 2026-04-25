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
#include "RemoteMediaSessionManager.h"

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "Logging.h"
#include "MessageSenderInlines.h"
#include "RemoteAudioSessionConfiguration.h"
#include "RemoteMediaSessionManagerMessages.h"
#include "RemoteMediaSessionManagerProxyMessages.h"
#include "RemoteMediaSessionState.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/AudioSession.h>
#include <WebCore/PlatformMediaSession.h>
#include <algorithm>
#include <ranges>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionManager);

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionState);

RefPtr<RemoteMediaSessionManager> RemoteMediaSessionManager::create(WebPage& topPage, WebPage& localPage)
{
    return adoptRef(new RemoteMediaSessionManager(topPage, localPage));
}

RemoteMediaSessionManager::RemoteMediaSessionManager(WebPage& topPage, WebPage& localPage)
    : REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS(localPage.identifier())
    , m_topPage(topPage)
    , m_localPage(localPage)
    , m_topPageID(topPage.identifier())
    , m_localPageID(localPage.identifier())
{
    WebProcess::singleton().addMessageReceiver(Messages::RemoteMediaSessionManager::messageReceiverName(), m_localPageID, *this);

    localPage.send(Messages::WebPageProxy::AddRemoteMediaSessionManager(m_localPageID));

#if USE(AUDIO_SESSION)
    Ref sharedSession = WebCore::AudioSession::singleton();
    RemoteAudioSessionConfiguration configuration = {
        sharedSession->routingContextUID(),
        sharedSession->sampleRate(),
        sharedSession->bufferSize(),
        sharedSession->numberOfOutputChannels(),
        sharedSession->maximumNumberOfOutputChannels(),
        sharedSession->preferredBufferSize(),
        sharedSession->outputLatency(),
        sharedSession->isMuted(),
        sharedSession->isActive(),
        sharedSession->sceneIdentifier(),
        sharedSession->soundStageSize(),
        sharedSession->categoryOverride(),
    };
    send(Messages::RemoteMediaSessionManagerProxy::RemoteAudioConfigurationChanged(WTF::move(configuration)));
#endif
}

RemoteMediaSessionManager::~RemoteMediaSessionManager()
{
    if (RefPtr page = m_localPage.get())
        page->send(Messages::WebPageProxy::RemoveRemoteMediaSessionManager(m_localPageID));
    WebProcess::singleton().removeMessageReceiver(Messages::RemoteMediaSessionManager::messageReceiverName(), m_localPageID);
}

void RemoteMediaSessionManager::addSession(WebCore::PlatformMediaSessionInterface& session)
{
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::addSession(session);
    send(Messages::RemoteMediaSessionManagerProxy::AddMediaSession(currentSessionState(session)));
}

void RemoteMediaSessionManager::removeSession(WebCore::PlatformMediaSessionInterface& session)
{
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::removeSession(session);

    if (!m_cachedSessionState.contains(session.mediaSessionIdentifier()))
        return;

    m_cachedSessionState.remove(session.mediaSessionIdentifier());

    send(Messages::RemoteMediaSessionManagerProxy::RemoveMediaSession(currentSessionState(session)));
}

void RemoteMediaSessionManager::setCurrentSession(WebCore::PlatformMediaSessionInterface& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), ", size = ", sessions().computeSize());
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::setCurrentSession(session);

    send(Messages::RemoteMediaSessionManagerProxy::SetCurrentMediaSession(currentSessionState(session)));
}

void RemoteMediaSessionManager::sessionWillBeginPlayback(WebCore::PlatformMediaSessionInterface& session, CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::RemoteMediaSessionManagerProxy::MediaSessionWillBeginPlayback(currentSessionState(session)), WTF::move(completionHandler));
}

void RemoteMediaSessionManager::addRestriction(WebCore::PlatformMediaSessionMediaType type, WebCore::MediaSessionRestrictions restrictions)
{
    send(Messages::RemoteMediaSessionManagerProxy::AddMediaSessionRestriction(type, restrictions));
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::addRestriction(type, restrictions);
}

void RemoteMediaSessionManager::removeRestriction(WebCore::PlatformMediaSessionMediaType type, WebCore::MediaSessionRestrictions restrictions)
{
    send(Messages::RemoteMediaSessionManagerProxy::RemoveMediaSessionRestriction(type, restrictions));
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::removeRestriction(type, restrictions);
}

void RemoteMediaSessionManager::resetRestrictions()
{
    send(Messages::RemoteMediaSessionManagerProxy::ResetMediaSessionRestrictions());
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::resetRestrictions();
}

void RemoteMediaSessionManager::updateSessionState()
{
    send(Messages::RemoteMediaSessionManagerProxy::UpdateMediaSessionState());
}

void RemoteMediaSessionManager::sessionStateChanged(WebCore::PlatformMediaSessionInterface& session)
{
    send(Messages::RemoteMediaSessionManagerProxy::MediaSessionStateChanged(currentSessionState(session)));
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::sessionStateChanged(session);
}

RefPtr<WebCore::PlatformMediaSessionInterface> RemoteMediaSessionManager::sessionWithIdentifier(WebCore::MediaSessionIdentifier identifier)
{
    return firstSessionMatching([identifier](auto& session) {
        return session.mediaSessionIdentifier() == identifier;
    }).get();
}

void RemoteMediaSessionManager::clientShouldResumeAutoplaying(WebCore::MediaSessionIdentifier identifier)
{
    if (RefPtr session = sessionWithIdentifier(identifier))
        protect(session->client())->resumeAutoplaying();
}

void RemoteMediaSessionManager::clientMayResumePlayback(WebCore::MediaSessionIdentifier identifier, bool shouldResume)
{
    if (RefPtr session = sessionWithIdentifier(identifier))
        protect(session->client())->mayResumePlayback(shouldResume);
}

void RemoteMediaSessionManager::clientShouldSuspendPlayback(WebCore::MediaSessionIdentifier identifier)
{
    if (RefPtr session = sessionWithIdentifier(identifier))
        protect(session->client())->suspendPlayback();
}

void RemoteMediaSessionManager::clientSetShouldPlayToPlaybackTarget(WebCore::MediaSessionIdentifier identifier, bool shouldPlay)
{
    if (RefPtr session = sessionWithIdentifier(identifier))
        protect(session->client())->setShouldPlayToPlaybackTarget(shouldPlay);
}

void RemoteMediaSessionManager::clientDidReceiveRemoteControlCommand(WebCore::MediaSessionIdentifier identifier, WebCore::PlatformMediaSessionRemoteControlCommandType command, WebCore::PlatformMediaSessionRemoteCommandArgument argument)
{
    if (RefPtr session = sessionWithIdentifier(identifier))
        protect(session->client())->didReceiveRemoteControlCommand(command, argument);
}

void RemoteMediaSessionManager::setCurrentMediaSession(std::optional<WebCore::MediaSessionIdentifier> identifier)
{
    if (!identifier)
        return;

    if (RefPtr session = sessionWithIdentifier(identifier.value()))
        REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::setCurrentSession(*session);
}

RemoteMediaSessionState& RemoteMediaSessionManager::currentSessionState(const WebCore::PlatformMediaSessionInterface& session)
{
    auto addResult = m_cachedSessionState.ensure(session.mediaSessionIdentifier(), [&] {
        return makeUniqueRef<RemoteMediaSessionState>(fullSessionState(session));
    });

    if (!addResult.isNewEntry)
        updateCachedSessionState(session, addResult.iterator->value);

    return addResult.iterator->value;
}

#if PLATFORM(COCOA)
void RemoteMediaSessionManager::audioHardwareDidBecomeActive()
{
    send(Messages::RemoteMediaSessionManagerProxy::RemoteAudioHardwareDidBecomeActive());
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::audioHardwareDidBecomeActive();
}

void RemoteMediaSessionManager::audioHardwareDidBecomeInactive()
{
    send(Messages::RemoteMediaSessionManagerProxy::RemoteAudioHardwareDidBecomeInactive());
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::audioHardwareDidBecomeInactive();
}

void RemoteMediaSessionManager::audioOutputDeviceChanged()
{
    auto supportedBufferSizes = audioHardwareListener()->supportedBufferSizes();
    send(Messages::RemoteMediaSessionManagerProxy::RemoteAudioOutputDeviceChanged(supportedBufferSizes.minimum, supportedBufferSizes.maximum));
    REMOTE_MEDIA_SESSION_MANAGER_BASE_CLASS::audioOutputDeviceChanged();
}
#endif

#if USE(AUDIO_SESSION)
void RemoteMediaSessionManager::setAudioSessionCategory(WebCore::AudioSessionCategory type, WebCore::AudioSessionMode mode, WebCore::RouteSharingPolicy policy)
{
    WebCore::AudioSession::singleton().setCategory(type, mode, policy);
}

void RemoteMediaSessionManager::setAudioSessionPreferredBufferSize(uint64_t preferredBufferSize)
{
    WebCore::AudioSession::singleton().setPreferredBufferSize(preferredBufferSize);
}

void RemoteMediaSessionManager::tryToSetAudioSessionActive(bool active)
{
    WebCore::AudioSession::singleton().tryToSetActive(active);
}
#endif

void RemoteMediaSessionManager::updateCachedSessionState(const WebCore::PlatformMediaSessionInterface& session, RemoteMediaSessionState& state)
{
    state.mediaType = session.mediaType();
    state.presentationType = session.presentationType();
    state.displayType = session.displayType();
    state.state = session.state();
    state.stateToRestore = session.stateToRestore();
    state.interruptionType = session.interruptionType();

    state.duration = session.duration();
    state.nowPlayingInfo = session.nowPlayingInfo();
    state.shouldOverrideBackgroundLoadingRestriction = session.shouldOverrideBackgroundLoadingRestriction();
    state.isPlayingToWirelessPlaybackTarget = session.isPlayingToWirelessPlaybackTarget();
    state.isPlayingOnSecondScreen = session.isPlayingOnSecondScreen();
    state.hasMediaStreamSource = session.hasMediaStreamSource();
    state.shouldOverridePauseDuringRouteChange = session.shouldOverridePauseDuringRouteChange();
    state.isNowPlayingEligible = session.isNowPlayingEligible();
    state.canProduceAudio = session.canProduceAudio();
    state.isSuspended = session.isSuspended();
    state.isPlaying = session.isPlaying();
    state.isAudible = session.isAudible();
    state.isEnded = session.isEnded();
    state.canReceiveRemoteControlCommands = session.canReceiveRemoteControlCommands();
    state.supportsSeeking = session.supportsSeeking();
    state.hasPlayedAudiblySinceLastInterruption = session.hasPlayedAudiblySinceLastInterruption();
    state.isLongEnoughForMainContent = session.isLongEnoughForMainContent();
    state.blockedBySystemInterruption = session.blockedBySystemInterruption();
    state.activeAudioSessionRequired = session.activeAudioSessionRequired();
    state.preparingToPlay = session.preparingToPlay();
    state.isActiveNowPlayingSession = session.isActiveNowPlayingSession();

#if PLATFORM(IOS_FAMILY)
    state.requiresPlaybackTargetRouteMonitoring = session.requiresPlaybackTargetRouteMonitoring();
#endif
}

RemoteMediaSessionState RemoteMediaSessionManager::fullSessionState(const WebCore::PlatformMediaSessionInterface& session)
{
    return {
        .pageIdentifier = m_localPageID,
        .sessionIdentifier = session.mediaSessionIdentifier(),
#if !RELEASE_LOG_DISABLED
        .logIdentifier = session.logIdentifier(),
#endif
        .mediaType = session.mediaType(),
        .presentationType = session.presentationType(),
        .displayType = session.displayType(),

        .state = session.state(),
        .stateToRestore = session.stateToRestore(),
        .interruptionType = session.interruptionType(),

        .duration = session.duration(),

        .groupIdentifier = session.mediaSessionGroupIdentifier(),
        .nowPlayingInfo = session.nowPlayingInfo(),

        .shouldOverrideBackgroundLoadingRestriction = session.shouldOverrideBackgroundLoadingRestriction(),
        .isPlayingToWirelessPlaybackTarget = session.isPlayingToWirelessPlaybackTarget(),
        .isPlayingOnSecondScreen = session.isPlayingOnSecondScreen(),
        .hasMediaStreamSource = session.hasMediaStreamSource(),
        .shouldOverridePauseDuringRouteChange = session.shouldOverridePauseDuringRouteChange(),
        .isNowPlayingEligible = session.isNowPlayingEligible(),
        .canProduceAudio = session.canProduceAudio(),
        .isSuspended = session.isSuspended(),
        .isPlaying = session.isPlaying(),
        .isAudible = session.isAudible(),
        .isEnded = session.isEnded(),
        .canReceiveRemoteControlCommands = session.canReceiveRemoteControlCommands(),
        .supportsSeeking = session.supportsSeeking(),
        .hasPlayedAudiblySinceLastInterruption = session.hasPlayedAudiblySinceLastInterruption(),
        .isLongEnoughForMainContent = session.isLongEnoughForMainContent(),
        .blockedBySystemInterruption = session.blockedBySystemInterruption(),
        .activeAudioSessionRequired = session.activeAudioSessionRequired(),
        .preparingToPlay = session.preparingToPlay(),
        .isActiveNowPlayingSession = session.isActiveNowPlayingSession(),

#if PLATFORM(IOS_FAMILY)
        .requiresPlaybackTargetRouteMonitoring = session.requiresPlaybackTargetRouteMonitoring(),
#endif
    };
}
IPC::Connection* RemoteMediaSessionManager::messageSenderConnection() const
{
    return WebProcess::singleton().parentProcessConnection();
}

uint64_t RemoteMediaSessionManager::messageSenderDestinationID() const
{
    return m_topPageID.toUInt64();
}

std::optional<SharedPreferencesForWebProcess> RemoteMediaSessionManager::sharedPreferencesForWebProcess() const
{
    return WebProcess::singleton().sharedPreferencesForWebProcess();
}

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
