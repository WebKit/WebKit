/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MediaSessionManagerIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "MediaPlaybackTargetCocoa.h"
#import "MediaPlayer.h"
#import "PlatformMediaSession.h"
#import "RuntimeApplicationChecks.h"
#import "SystemMemory.h"
#import "WebCoreThreadRun.h"
#import <wtf/MainThread.h>
#import <wtf/RAMSize.h>
#import <wtf/RetainPtr.h>

namespace WebCore {

std::unique_ptr<PlatformMediaSessionManager> PlatformMediaSessionManager::create()
{
    auto manager = std::unique_ptr<MediaSessionManageriOS>(new MediaSessionManageriOS);
    MediaSessionHelper::sharedHelper().addClient(*manager);
    return WTFMove(manager);
}

MediaSessionManageriOS::MediaSessionManageriOS()
    : MediaSessionManagerCocoa()
{
    AudioSession::sharedSession().addInterruptionObserver(*this);
}

MediaSessionManageriOS::~MediaSessionManageriOS()
{
    if (m_isMonitoringWirelessRoutes)
        MediaSessionHelper::sharedHelper().stopMonitoringWirelessRoutes();
    MediaSessionHelper::sharedHelper().removeClient(*this);
    AudioSession::sharedSession().removeInterruptionObserver(*this);
}

void MediaSessionManageriOS::resetRestrictions()
{
    static const size_t systemMemoryRequiredForVideoInBackgroundTabs = 1024 * 1024 * 1024;

    ALWAYS_LOG(LOGIDENTIFIER);

    MediaSessionManagerCocoa::resetRestrictions();

    if (ramSize() < systemMemoryRequiredForVideoInBackgroundTabs) {
        ALWAYS_LOG(LOGIDENTIFIER, "restricting video in background tabs because system memory = ", ramSize());
        addRestriction(PlatformMediaSession::MediaType::Video, BackgroundTabPlaybackRestricted);
    }

    addRestriction(PlatformMediaSession::MediaType::Video, BackgroundProcessPlaybackRestricted);
    addRestriction(PlatformMediaSession::MediaType::VideoAudio, ConcurrentPlaybackNotPermitted | BackgroundProcessPlaybackRestricted | SuspendedUnderLockPlaybackRestricted);
}

bool MediaSessionManageriOS::hasWirelessTargetsAvailable()
{
    return MediaSessionHelper::sharedHelper().isExternalOutputDeviceAvailable();
}

void MediaSessionManageriOS::configureWireLessTargetMonitoring()
{
#if !PLATFORM(WATCHOS)
    bool requiresMonitoring = anyOfSessions([] (auto& session) {
        return session.requiresPlaybackTargetRouteMonitoring();
    });

    if (requiresMonitoring == m_isMonitoringWirelessRoutes)
        return;

    m_isMonitoringWirelessRoutes = requiresMonitoring;

    ALWAYS_LOG(LOGIDENTIFIER, "requiresMonitoring = ", requiresMonitoring);

    if (requiresMonitoring)
        MediaSessionHelper::sharedHelper().startMonitoringWirelessRoutes();
    else
        MediaSessionHelper::sharedHelper().stopMonitoringWirelessRoutes();
#endif
}

void MediaSessionManageriOS::providePresentingApplicationPIDIfNecessary()
{
#if HAVE(CELESTIAL)
    if (m_havePresentedApplicationPID)
        return;
    m_havePresentedApplicationPID = true;
    MediaSessionHelper::sharedHelper().providePresentingApplicationPID(presentingApplicationPID());
#endif
}

void MediaSessionManageriOS::mediaServerConnectionDied()
{
    ALWAYS_LOG(LOGIDENTIFIER, m_havePresentedApplicationPID);

    if (!m_havePresentedApplicationPID)
        return;

    m_havePresentedApplicationPID = false;
    taskQueue().enqueueTask([] () {
        providePresentingApplicationPID();
    });
}

void MediaSessionManageriOS::providePresentingApplicationPID()
{
    MediaSessionHelper::sharedHelper().providePresentingApplicationPID(presentingApplicationPID());
}

bool MediaSessionManageriOS::sessionWillBeginPlayback(PlatformMediaSession& session)
{
    if (!MediaSessionManagerCocoa::sessionWillBeginPlayback(session))
        return false;

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    if (!m_playbackTarget) {
        m_playbackTarget = MediaPlaybackTargetCocoa::create();
        m_playbackTargetSupportsAirPlayVideo = m_playbackTarget->supportsRemoteVideoPlayback();
    }

    ALWAYS_LOG(LOGIDENTIFIER, "Playback Target Supports AirPlay Video = ", m_playbackTargetSupportsAirPlayVideo);
    if (m_playbackTargetSupportsAirPlayVideo)
        session.setPlaybackTarget(*m_playbackTarget.copyRef());
    session.setShouldPlayToPlaybackTarget(m_playbackTargetSupportsAirPlayVideo);
#endif

    return true;
}

void MediaSessionManageriOS::sessionWillEndPlayback(PlatformMediaSession& session, DelayCallingUpdateNowPlaying delayCallingUpdateNowPlaying)
{
    MediaSessionManagerCocoa::sessionWillEndPlayback(session, delayCallingUpdateNowPlaying);

#if USE(AUDIO_SESSION)
    if (isApplicationInBackground() && !anyOfSessions([] (auto& session) { return session.state() == PlatformMediaSession::Playing; }))
        maybeDeactivateAudioSession();
#endif
}

void MediaSessionManageriOS::externalOutputDeviceAvailableDidChange(HasAvailableTargets haveTargets)
{
    ALWAYS_LOG(LOGIDENTIFIER, haveTargets);

    forEachSession([haveTargets] (auto& session) {
        session.externalOutputDeviceAvailableDidChange(haveTargets == HasAvailableTargets::Yes);
    });
}

void MediaSessionManageriOS::isPlayingToAutomotiveHeadUnitDidChange(PlayingToAutomotiveHeadUnit playingToAutomotiveHeadUnit)
{
    setIsPlayingToAutomotiveHeadUnit(playingToAutomotiveHeadUnit == PlayingToAutomotiveHeadUnit::Yes);
}

void MediaSessionManageriOS::activeAudioRouteDidChange(ShouldPause shouldPause)
{
    ALWAYS_LOG(LOGIDENTIFIER, shouldPause);

    if (shouldPause != ShouldPause::Yes)
        return;

    forEachSession([](auto& session) {
        if (session.canProduceAudio() && !session.shouldOverridePauseDuringRouteChange())
            session.pauseSession();
    });
}

void MediaSessionManageriOS::activeVideoRouteDidChange(SupportsAirPlayVideo supportsAirPlayVideo, Ref<MediaPlaybackTarget>&& playbackTarget)
{
    ALWAYS_LOG(LOGIDENTIFIER, supportsAirPlayVideo);

#if !PLATFORM(WATCHOS)
    m_playbackTarget = playbackTarget.ptr();
    m_playbackTargetSupportsAirPlayVideo = supportsAirPlayVideo == SupportsAirPlayVideo::Yes;
#endif

    auto nowPlayingSession = nowPlayingEligibleSession();
    if (!nowPlayingSession)
        return;

    nowPlayingSession->setShouldPlayToPlaybackTarget(supportsAirPlayVideo == SupportsAirPlayVideo::Yes);
    nowPlayingSession->setPlaybackTarget(WTFMove(playbackTarget));
}

void MediaSessionManageriOS::applicationWillEnterForeground(SuspendedUnderLock isSuspendedUnderLock)
{
    if (willIgnoreSystemInterruptions())
        return;

    MediaSessionManagerCocoa::applicationWillEnterForeground(isSuspendedUnderLock == SuspendedUnderLock::Yes);
}

void MediaSessionManageriOS::applicationDidBecomeActive()
{
    if (willIgnoreSystemInterruptions())
        return;

    MediaSessionManagerCocoa::applicationDidBecomeActive();
}

void MediaSessionManageriOS::applicationDidEnterBackground(SuspendedUnderLock isSuspendedUnderLock)
{
    if (willIgnoreSystemInterruptions())
        return;

    MediaSessionManagerCocoa::applicationDidEnterBackground(isSuspendedUnderLock == SuspendedUnderLock::Yes);
}

void MediaSessionManageriOS::applicationWillBecomeInactive()
{
    if (willIgnoreSystemInterruptions())
        return;

    MediaSessionManagerCocoa::applicationWillBecomeInactive();
}

} // namespace WebCore


#endif // PLATFORM(IOS_FAMILY)
