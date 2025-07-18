/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#import "MediaConfiguration.h"
#import "MediaPlaybackTargetCocoa.h"
#import "MediaPlayer.h"
#import "PlatformMediaSession.h"
#import "SystemMemory.h"
#import "WebCoreThreadRun.h"
#import <wtf/MainThread.h>
#import <wtf/RAMSize.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/TZoneMalloc.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaSessionManageriOS);

const std::unique_ptr<PlatformMediaSessionManager> PlatformMediaSessionManager::create()
{
    auto manager = std::unique_ptr<MediaSessionManageriOS>(new MediaSessionManageriOS);
    MediaSessionHelper::sharedHelper().addClient(*manager);
    return manager;
}

MediaSessionManageriOS::MediaSessionManageriOS()
    : MediaSessionManagerCocoa()
{
    AudioSession::singleton().addInterruptionObserver(*this);
}

MediaSessionManageriOS::~MediaSessionManageriOS()
{
    if (m_isMonitoringWirelessRoutes)
        MediaSessionHelper::sharedHelper().stopMonitoringWirelessRoutes();
    MediaSessionHelper::sharedHelper().removeClient(*this);
    AudioSession::singleton().removeInterruptionObserver(*this);
}

#if !PLATFORM(MACCATALYST)
void MediaSessionManageriOS::resetRestrictions()
{
    static const size_t systemMemoryRequiredForVideoInBackgroundTabs = 1024 * 1024 * 1024;

    ALWAYS_LOG(LOGIDENTIFIER);

    MediaSessionManagerCocoa::resetRestrictions();

    if (ramSize() < systemMemoryRequiredForVideoInBackgroundTabs) {
        ALWAYS_LOG(LOGIDENTIFIER, "restricting video in background tabs because system memory = ", ramSize());
        addRestriction(PlatformMediaSession::MediaType::Video, MediaSessionRestriction::BackgroundTabPlaybackRestricted);
    }

    addRestriction(PlatformMediaSession::MediaType::Video, MediaSessionRestriction::BackgroundProcessPlaybackRestricted);
    addRestriction(PlatformMediaSession::MediaType::WebAudio, MediaSessionRestriction::BackgroundProcessPlaybackRestricted);
    addRestriction(PlatformMediaSession::MediaType::VideoAudio, { MediaSessionRestriction::ConcurrentPlaybackNotPermitted, MediaSessionRestriction::BackgroundProcessPlaybackRestricted, MediaSessionRestriction::SuspendedUnderLockPlaybackRestricted });
}
#endif

bool MediaSessionManageriOS::hasWirelessTargetsAvailable()
{
    return MediaSessionHelper::sharedHelper().isExternalOutputDeviceAvailable();
}

bool MediaSessionManageriOS::isMonitoringWirelessTargets() const
{
    return m_isMonitoringWirelessRoutes;
}

void MediaSessionManageriOS::configureWirelessTargetMonitoring()
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

void MediaSessionManageriOS::providePresentingApplicationPIDIfNecessary(const std::optional<ProcessID>& pid)
{
#if HAVE(MEDIAEXPERIENCE_AVSYSTEMCONTROLLER)
    if (m_havePresentedApplicationPID || !pid)
        return;
    m_havePresentedApplicationPID = true;
    MediaSessionHelper::sharedHelper().providePresentingApplicationPID(*pid);
#else
    UNUSED_PARAM(pid);
#endif
}

void MediaSessionManageriOS::updatePresentingApplicationPIDIfNecessary(ProcessID pid)
{
#if HAVE(MEDIAEXPERIENCE_AVSYSTEMCONTROLLER)
    if (m_havePresentedApplicationPID)
        MediaSessionHelper::sharedHelper().providePresentingApplicationPID(pid, MediaSessionHelper::ShouldOverride::Yes);
#else
    UNUSED_PARAM(pid);
#endif
}

bool MediaSessionManageriOS::sessionWillBeginPlayback(PlatformMediaSessionInterface& session)
{
    if (!MediaSessionManagerCocoa::sessionWillBeginPlayback(session))
        return false;

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    auto playbackTargetSupportsAirPlayVideo = MediaSessionHelper::sharedHelper().activeVideoRouteSupportsAirPlayVideo();
    ALWAYS_LOG(LOGIDENTIFIER, "Playback Target Supports AirPlay Video = ", playbackTargetSupportsAirPlayVideo);
    if (auto target = MediaSessionHelper::sharedHelper().playbackTarget(); target && playbackTargetSupportsAirPlayVideo)
        session.setPlaybackTarget(*target);
    session.setShouldPlayToPlaybackTarget(playbackTargetSupportsAirPlayVideo);
#endif

    providePresentingApplicationPIDIfNecessary(session.presentingApplicationPID());

    return true;
}

void MediaSessionManageriOS::sessionWillEndPlayback(PlatformMediaSessionInterface& session, DelayCallingUpdateNowPlaying delayCallingUpdateNowPlaying)
{
    MediaSessionManagerCocoa::sessionWillEndPlayback(session, delayCallingUpdateNowPlaying);

#if USE(AUDIO_SESSION)
    if (isApplicationInBackground() && !anyOfSessions([] (auto& session) { return session.state() == PlatformMediaSession::State::Playing; }))
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

void MediaSessionManageriOS::activeAudioRouteSupportsSpatialPlaybackDidChange(SupportsSpatialAudioPlayback supportsSpatialPlayback)
{
    setSupportsSpatialAudioPlayback(supportsSpatialPlayback == SupportsSpatialAudioPlayback::Yes);
}

std::optional<bool> MediaSessionManagerCocoa::supportsSpatialAudioPlaybackForConfiguration(const MediaConfiguration& configuration)
{
    ASSERT(configuration.audio);

    // Only multichannel audio can be spatially rendered on iOS.
    if (!configuration.audio || configuration.audio->channels.toDouble() <= 2)
        return { false };

    auto supportsSpatialAudioPlayback = this->supportsSpatialAudioPlayback();
    if (supportsSpatialAudioPlayback.has_value())
        return supportsSpatialAudioPlayback;

    MediaSessionHelper::sharedHelper().updateActiveAudioRouteSupportsSpatialPlayback();

    return this->supportsSpatialAudioPlayback();
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

    RefPtr nowPlayingSession = nowPlayingEligibleSession().get();
    if (!nowPlayingSession)
        return;

    nowPlayingSession->setPlaybackTarget(WTFMove(playbackTarget));
    nowPlayingSession->setShouldPlayToPlaybackTarget(supportsAirPlayVideo == SupportsAirPlayVideo::Yes);
}

void MediaSessionManageriOS::uiApplicationWillEnterForeground(SuspendedUnderLock isSuspendedUnderLock)
{
    if (willIgnoreSystemInterruptions())
        return;

    MediaSessionManagerCocoa::applicationWillEnterForeground(isSuspendedUnderLock == SuspendedUnderLock::Yes);
}

void MediaSessionManageriOS::uiApplicationDidBecomeActive()
{
    if (willIgnoreSystemInterruptions())
        return;

    MediaSessionManagerCocoa::applicationDidBecomeActive();
}

void MediaSessionManageriOS::uiApplicationDidEnterBackground(SuspendedUnderLock isSuspendedUnderLock)
{
    if (willIgnoreSystemInterruptions())
        return;

    MediaSessionManagerCocoa::applicationDidEnterBackground(isSuspendedUnderLock == SuspendedUnderLock::Yes);
}

void MediaSessionManageriOS::uiApplicationWillBecomeInactive()
{
    if (willIgnoreSystemInterruptions())
        return;

    MediaSessionManagerCocoa::applicationWillBecomeInactive();
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
