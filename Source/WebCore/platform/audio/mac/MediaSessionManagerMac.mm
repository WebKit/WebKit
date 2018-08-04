/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#import "MediaSessionManagerMac.h"

#if PLATFORM(MAC)

#import "HTMLMediaElement.h"
#import "Logging.h"
#import "MediaPlayer.h"
#import "PlatformMediaSession.h"
#import <wtf/BlockObjCExceptions.h>

#import "MediaRemoteSoftLink.h"

using namespace WebCore;

namespace WebCore {

static MediaSessionManagerMac* platformMediaSessionManager = nullptr;

PlatformMediaSessionManager& PlatformMediaSessionManager::sharedManager()
{
    if (!platformMediaSessionManager)
        platformMediaSessionManager = new MediaSessionManagerMac;
    return *platformMediaSessionManager;
}

PlatformMediaSessionManager* PlatformMediaSessionManager::sharedManagerIfExists()
{
    return platformMediaSessionManager;
}

MediaSessionManagerMac::MediaSessionManagerMac()
    : MediaSessionManagerCocoa()
{
    resetRestrictions();
}

MediaSessionManagerMac::~MediaSessionManagerMac()
{
}

void MediaSessionManagerMac::scheduleUpdateNowPlayingInfo()
{
    if (!m_nowPlayingUpdateTaskQueue.hasPendingTasks())
        m_nowPlayingUpdateTaskQueue.enqueueTask(std::bind(&MediaSessionManagerMac::updateNowPlayingInfo, this));
}

bool MediaSessionManagerMac::sessionWillBeginPlayback(PlatformMediaSession& session)
{
    if (!PlatformMediaSessionManager::sessionWillBeginPlayback(session))
        return false;

    LOG(Media, "MediaSessionManagerMac::sessionWillBeginPlayback");
    scheduleUpdateNowPlayingInfo();
    return true;
}

void MediaSessionManagerMac::sessionDidEndRemoteScrubbing(const PlatformMediaSession&)
{
    scheduleUpdateNowPlayingInfo();
}

void MediaSessionManagerMac::removeSession(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::removeSession(session);
    LOG(Media, "MediaSessionManagerMac::removeSession");
    scheduleUpdateNowPlayingInfo();
}

void MediaSessionManagerMac::sessionWillEndPlayback(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::sessionWillEndPlayback(session);
    LOG(Media, "MediaSessionManagerMac::sessionWillEndPlayback");
    updateNowPlayingInfo();
}

void MediaSessionManagerMac::clientCharacteristicsChanged(PlatformMediaSession&)
{
    LOG(Media, "MediaSessionManagerMac::clientCharacteristicsChanged");
    scheduleUpdateNowPlayingInfo();
}

PlatformMediaSession* MediaSessionManagerMac::nowPlayingEligibleSession()
{
    if (auto element = HTMLMediaElement::bestMediaElementForShowingPlaybackControlsManager(MediaElementSession::PlaybackControlsPurpose::NowPlaying))
        return &element->mediaSession();

    return nullptr;
}

void MediaSessionManagerMac::updateNowPlayingInfo()
{
#if USE(MEDIAREMOTE)
    if (!isMediaRemoteFrameworkAvailable())
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    const PlatformMediaSession* currentSession = this->nowPlayingEligibleSession();

    LOG(Media, "MediaSessionManagerMac::updateNowPlayingInfo - currentSession = %p", currentSession);

    if (!currentSession) {
        if (canLoad_MediaRemote_MRMediaRemoteSetNowPlayingVisibility())
            MRMediaRemoteSetNowPlayingVisibility(MRMediaRemoteGetLocalOrigin(), MRNowPlayingClientVisibilityNeverVisible);

        LOG(Media, "MediaSessionManagerMac::updateNowPlayingInfo - clearing now playing info");

        MRMediaRemoteSetCanBeNowPlayingApplication(false);
        m_registeredAsNowPlayingApplication = false;

        MRMediaRemoteSetNowPlayingInfo(nullptr);
        m_nowPlayingActive = false;
        m_lastUpdatedNowPlayingTitle = emptyString();
        m_lastUpdatedNowPlayingDuration = NAN;
        m_lastUpdatedNowPlayingElapsedTime = NAN;
        m_lastUpdatedNowPlayingInfoUniqueIdentifier = 0;
        MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(MRMediaRemoteGetLocalOrigin(), kMRPlaybackStateStopped, dispatch_get_main_queue(), ^(MRMediaRemoteError error) {
#if LOG_DISABLED
            UNUSED_PARAM(error);
#else
            if (error)
                LOG(Media, "MediaSessionManagerMac::updateNowPlayingInfo - MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(stopped) failed with error %ud", error);
#endif
        });

        return;
    }

    if (!m_registeredAsNowPlayingApplication) {
        m_registeredAsNowPlayingApplication = true;
        MRMediaRemoteSetCanBeNowPlayingApplication(true);
    }

    String title = currentSession->title();
    double duration = currentSession->supportsSeeking() ? currentSession->duration() : MediaPlayer::invalidTime();
    double rate = currentSession->state() == PlatformMediaSession::Playing ? 1 : 0;
    auto info = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    if (!title.isEmpty()) {
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoTitle, title.createCFString().get());
        m_lastUpdatedNowPlayingTitle = title;
    }

    if (std::isfinite(duration) && duration != MediaPlayer::invalidTime()) {
        auto cfDuration = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &duration));
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoDuration, cfDuration.get());
        m_lastUpdatedNowPlayingDuration = duration;
    }

    auto cfRate = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &rate));
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoPlaybackRate, cfRate.get());

    m_lastUpdatedNowPlayingInfoUniqueIdentifier = currentSession->uniqueIdentifier();
    auto cfIdentifier = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &m_lastUpdatedNowPlayingInfoUniqueIdentifier));
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoUniqueIdentifier, cfIdentifier.get());

    double currentTime = currentSession->currentTime();
    if (std::isfinite(currentTime) && currentTime != MediaPlayer::invalidTime() && currentSession->supportsSeeking()) {
        auto cfCurrentTime = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &currentTime));
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoElapsedTime, cfCurrentTime.get());
        m_lastUpdatedNowPlayingElapsedTime = currentTime;
    }

    LOG(Media, "MediaSessionManagerMac::updateNowPlayingInfo - title = \"%s\", rate = %f, duration = %f, now = %f",
        title.utf8().data(), rate, duration, currentTime);

    String parentApplication = currentSession->sourceApplicationIdentifier();
    if (canLoad_MediaRemote_MRMediaRemoteSetParentApplication() && !parentApplication.isEmpty())
        MRMediaRemoteSetParentApplication(MRMediaRemoteGetLocalOrigin(), parentApplication.createCFString().get());

    m_nowPlayingActive = currentSession->allowsNowPlayingControlsVisibility();
    MRPlaybackState playbackState = (currentSession->state() == PlatformMediaSession::Playing) ? kMRPlaybackStatePlaying : kMRPlaybackStatePaused;
    MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(MRMediaRemoteGetLocalOrigin(), playbackState, dispatch_get_main_queue(), ^(MRMediaRemoteError error) {
#if LOG_DISABLED
        UNUSED_PARAM(error);
#else
        LOG(Media, "MediaSessionManagerMac::updateNowPlayingInfo - MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(playing) failed with error %ud", error);
#endif
    });
    MRMediaRemoteSetNowPlayingInfo(info.get());

    if (canLoad_MediaRemote_MRMediaRemoteSetNowPlayingVisibility()) {
        MRNowPlayingClientVisibility visibility = currentSession->allowsNowPlayingControlsVisibility() ? MRNowPlayingClientVisibilityAlwaysVisible : MRNowPlayingClientVisibilityNeverVisible;
        MRMediaRemoteSetNowPlayingVisibility(MRMediaRemoteGetLocalOrigin(), visibility);
    }
    END_BLOCK_OBJC_EXCEPTIONS
#endif // USE(MEDIAREMOTE)
}

} // namespace WebCore

#endif // PLATFORM(MAC)
