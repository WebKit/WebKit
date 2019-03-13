/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
#include "MediaSessionManagerCocoa.h"

#if USE(AUDIO_SESSION) && PLATFORM(COCOA)

#include "AudioSession.h"
#include "DeprecatedGlobalSettings.h"
#include "HTMLMediaElement.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "PlatformMediaSession.h"
#include <wtf/BlockObjCExceptions.h>
#include <wtf/Function.h>

#include "MediaRemoteSoftLink.h"

using namespace WebCore;

static const size_t kWebAudioBufferSize = 128;
static const size_t kLowPowerVideoBufferSize = 4096;

#if PLATFORM(MAC)
static MediaSessionManagerCocoa* platformMediaSessionManager = nullptr;

PlatformMediaSessionManager& PlatformMediaSessionManager::sharedManager()
{
    if (!platformMediaSessionManager)
        platformMediaSessionManager = new MediaSessionManagerCocoa;
    return *platformMediaSessionManager;
}

PlatformMediaSessionManager* PlatformMediaSessionManager::sharedManagerIfExists()
{
    return platformMediaSessionManager;
}
#endif

void MediaSessionManagerCocoa::updateSessionState()
{
    ALWAYS_LOG(LOGIDENTIFIER, "types: "
        "Video(", count(PlatformMediaSession::Video), "), "
        "Audio(", count(PlatformMediaSession::Audio), "), "
        "VideoAudio(", count(PlatformMediaSession::VideoAudio), "), "
        "WebAudio(", count(PlatformMediaSession::WebAudio), ")");

    if (has(PlatformMediaSession::WebAudio))
        AudioSession::sharedSession().setPreferredBufferSize(kWebAudioBufferSize);
    // In case of audio capture, we want to grab 20 ms chunks to limit the latency so that it is not noticeable by users
    // while having a large enough buffer so that the audio rendering remains stable, hence a computation based on sample rate.
    else if (has(PlatformMediaSession::MediaStreamCapturingAudio))
        AudioSession::sharedSession().setPreferredBufferSize(AudioSession::sharedSession().sampleRate() / 50);
    else if ((has(PlatformMediaSession::VideoAudio) || has(PlatformMediaSession::Audio)) && DeprecatedGlobalSettings::lowPowerVideoAudioBufferSizeEnabled()) {
        // FIXME: <http://webkit.org/b/116725> Figure out why enabling the code below
        // causes media LayoutTests to fail on 10.8.

        size_t bufferSize;
        if (audioHardwareListener() && audioHardwareListener()->outputDeviceSupportsLowPowerMode())
            bufferSize = kLowPowerVideoBufferSize;
        else
            bufferSize = kWebAudioBufferSize;

        AudioSession::sharedSession().setPreferredBufferSize(bufferSize);
    }

    if (!DeprecatedGlobalSettings::shouldManageAudioSessionCategory())
        return;

    bool hasWebAudioType = false;
    bool hasAudibleAudioOrVideoMediaType = false;
    bool hasAudioCapture = anyOfSessions([&hasWebAudioType, &hasAudibleAudioOrVideoMediaType] (PlatformMediaSession& session, size_t) mutable {
        auto type = session.mediaType();
        if (type == PlatformMediaSession::WebAudio)
            hasWebAudioType = true;
        if ((type == PlatformMediaSession::VideoAudio || type == PlatformMediaSession::Audio) && session.canProduceAudio() && session.hasPlayedSinceLastInterruption())
            hasAudibleAudioOrVideoMediaType = true;
        if (session.isPlayingToWirelessPlaybackTarget())
            hasAudibleAudioOrVideoMediaType = true;
        return (type == PlatformMediaSession::MediaStreamCapturingAudio);
    });

    if (hasAudioCapture)
        AudioSession::sharedSession().setCategory(AudioSession::PlayAndRecord);
    else if (hasAudibleAudioOrVideoMediaType)
        AudioSession::sharedSession().setCategory(AudioSession::MediaPlayback);
    else if (hasWebAudioType)
        AudioSession::sharedSession().setCategory(AudioSession::AmbientSound);
    else
        AudioSession::sharedSession().setCategory(AudioSession::None);
}

void MediaSessionManagerCocoa::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    if (type == PlatformMediaSession::InterruptionType::SystemInterruption) {
        forEachSession([] (PlatformMediaSession& session, size_t) {
            session.clearHasPlayedSinceLastInterruption();
        });
    }

    PlatformMediaSessionManager::beginInterruption(type);
}

void MediaSessionManagerCocoa::prepareToSendUserMediaPermissionRequest()
{
    providePresentingApplicationPIDIfNecessary();
}

void MediaSessionManagerCocoa::scheduleUpdateNowPlayingInfo()
{
    if (!m_nowPlayingUpdateTaskQueue.hasPendingTasks())
        m_nowPlayingUpdateTaskQueue.enqueueTask(std::bind(&MediaSessionManagerCocoa::updateNowPlayingInfo, this));
}

bool MediaSessionManagerCocoa::sessionWillBeginPlayback(PlatformMediaSession& session)
{
    if (!PlatformMediaSessionManager::sessionWillBeginPlayback(session))
        return false;
    
    scheduleUpdateNowPlayingInfo();
    return true;
}

void MediaSessionManagerCocoa::sessionDidEndRemoteScrubbing(const PlatformMediaSession&)
{
    scheduleUpdateNowPlayingInfo();
}

void MediaSessionManagerCocoa::removeSession(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::removeSession(session);
    scheduleUpdateNowPlayingInfo();
}

void MediaSessionManagerCocoa::sessionWillEndPlayback(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::sessionWillEndPlayback(session);
    updateNowPlayingInfo();
}

void MediaSessionManagerCocoa::clientCharacteristicsChanged(PlatformMediaSession& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());
    scheduleUpdateNowPlayingInfo();
}

void MediaSessionManagerCocoa::sessionCanProduceAudioChanged(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::sessionCanProduceAudioChanged(session);
    scheduleUpdateNowPlayingInfo();
}

PlatformMediaSession* MediaSessionManagerCocoa::nowPlayingEligibleSession()
{
    if (auto element = HTMLMediaElement::bestMediaElementForShowingPlaybackControlsManager(MediaElementSession::PlaybackControlsPurpose::NowPlaying))
        return &element->mediaSession();

    return nullptr;
}

void MediaSessionManagerCocoa::updateNowPlayingInfo()
{
#if USE(MEDIAREMOTE)
    if (!isMediaRemoteFrameworkAvailable())
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    const PlatformMediaSession* currentSession = this->nowPlayingEligibleSession();

    ALWAYS_LOG(LOGIDENTIFIER, "currentSession: ", currentSession ? currentSession->logIdentifier() : nullptr);

    if (!currentSession) {
        if (canLoad_MediaRemote_MRMediaRemoteSetNowPlayingVisibility())
            MRMediaRemoteSetNowPlayingVisibility(MRMediaRemoteGetLocalOrigin(), MRNowPlayingClientVisibilityNeverVisible);

        ALWAYS_LOG(LOGIDENTIFIER, "clearing now playing info");

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
                ALWAYS_LOG(LOGIDENTIFIER, "MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(stopped) failed with error ", error);
#endif
        });

        return;
    }

    if (!m_registeredAsNowPlayingApplication) {
        m_registeredAsNowPlayingApplication = true;
        providePresentingApplicationPIDIfNecessary();
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

    ALWAYS_LOG(LOGIDENTIFIER, "title = \"", title, "\", rate = ", rate, ", duration = ", duration, ", now = ", currentTime);

    String parentApplication = currentSession->sourceApplicationIdentifier();
    if (canLoad_MediaRemote_MRMediaRemoteSetParentApplication() && !parentApplication.isEmpty())
        MRMediaRemoteSetParentApplication(MRMediaRemoteGetLocalOrigin(), parentApplication.createCFString().get());

    m_nowPlayingActive = currentSession->allowsNowPlayingControlsVisibility();
    MRPlaybackState playbackState = (currentSession->state() == PlatformMediaSession::Playing) ? kMRPlaybackStatePlaying : kMRPlaybackStatePaused;
    MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(MRMediaRemoteGetLocalOrigin(), playbackState, dispatch_get_main_queue(), ^(MRMediaRemoteError error) {
#if LOG_DISABLED
        UNUSED_PARAM(error);
#else
        ALWAYS_LOG(LOGIDENTIFIER, "MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(playing) failed with error ", error);
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

#endif // USE(AUDIO_SESSION)
