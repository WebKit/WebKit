/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#import "MediaSessionManagerCocoa.h"

#if USE(AUDIO_SESSION) && PLATFORM(COCOA)

#import "AudioSession.h"
#import "DeprecatedGlobalSettings.h"
#import "HTMLMediaElement.h"
#import "Logging.h"
#import "MediaPlayer.h"
#import "MediaStrategy.h"
#import "NowPlayingInfo.h"
#import "PlatformMediaSession.h"
#import "PlatformStrategies.h"
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Function.h>

#import "MediaRemoteSoftLink.h"

static const size_t kWebAudioBufferSize = 128;
static const size_t kLowPowerVideoBufferSize = 4096;

namespace WebCore {

#if PLATFORM(MAC)
std::unique_ptr<PlatformMediaSessionManager> PlatformMediaSessionManager::create()
{
    return makeUnique<MediaSessionManagerCocoa>();
}
#endif // !PLATFORM(MAC)

MediaSessionManagerCocoa::MediaSessionManagerCocoa()
    : m_systemSleepListener(PAL::SystemSleepListener::create(*this))
{
}

void MediaSessionManagerCocoa::updateSessionState()
{
    int videoCount = 0;
    int videoAudioCount = 0;
    int audioCount = 0;
    int webAudioCount = 0;
    int captureCount = countActiveAudioCaptureSources();
    bool hasAudibleAudioOrVideoMediaType = false;
    forEachSession([&] (auto& session) mutable {
        auto type = session.mediaType();
        switch (type) {
        case PlatformMediaSession::MediaType::None:
            break;
        case PlatformMediaSession::MediaType::Video:
            ++videoCount;
            break;
        case PlatformMediaSession::MediaType::VideoAudio:
            ++videoAudioCount;
            break;
        case PlatformMediaSession::MediaType::Audio:
            ++audioCount;
            break;
        case PlatformMediaSession::MediaType::WebAudio:
            ++webAudioCount;
            break;
        }

        if (!hasAudibleAudioOrVideoMediaType) {
            if ((type == PlatformMediaSession::MediaType::VideoAudio || type == PlatformMediaSession::MediaType::Audio) && session.canProduceAudio() && session.hasPlayedSinceLastInterruption())
                hasAudibleAudioOrVideoMediaType = true;
            if (session.isPlayingToWirelessPlaybackTarget())
                hasAudibleAudioOrVideoMediaType = true;
        }
    });

    ALWAYS_LOG(LOGIDENTIFIER, "types: "
        "AudioCapture(", captureCount, "), "
        "Video(", videoCount, "), "
        "Audio(", audioCount, "), "
        "VideoAudio(", videoAudioCount, "), "
        "WebAudio(", webAudioCount, ")");

    if (webAudioCount)
        AudioSession::sharedSession().setPreferredBufferSize(kWebAudioBufferSize);
    // In case of audio capture, we want to grab 20 ms chunks to limit the latency so that it is not noticeable by users
    // while having a large enough buffer so that the audio rendering remains stable, hence a computation based on sample rate.
    else if (captureCount)
        AudioSession::sharedSession().setPreferredBufferSize(AudioSession::sharedSession().sampleRate() / 50);
    else if ((videoAudioCount || audioCount) && DeprecatedGlobalSettings::lowPowerVideoAudioBufferSizeEnabled()) {
        size_t bufferSize;
        if (m_audioHardwareListener && m_audioHardwareListener->outputDeviceSupportsLowPowerMode())
            bufferSize = kLowPowerVideoBufferSize;
        else
            bufferSize = kWebAudioBufferSize;

        AudioSession::sharedSession().setPreferredBufferSize(bufferSize);
    }

    if (!DeprecatedGlobalSettings::shouldManageAudioSessionCategory())
        return;

    RouteSharingPolicy policy = RouteSharingPolicy::Default;
    AudioSession::CategoryType category = AudioSession::None;
    if (captureCount)
        category = AudioSession::PlayAndRecord;
    else if (hasAudibleAudioOrVideoMediaType) {
        category = AudioSession::MediaPlayback;
        policy = RouteSharingPolicy::LongFormAudio;
    } else if (webAudioCount)
        category = AudioSession::AmbientSound;

    ALWAYS_LOG(LOGIDENTIFIER, "setting category = ", category, ", policy = ", policy);
    AudioSession::sharedSession().setCategory(category, policy);
}

void MediaSessionManagerCocoa::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    if (type == PlatformMediaSession::InterruptionType::SystemInterruption) {
        forEachSession([] (auto& session) {
            session.clearHasPlayedSinceLastInterruption();
        });
    }

    PlatformMediaSessionManager::beginInterruption(type);
}

void MediaSessionManagerCocoa::prepareToSendUserMediaPermissionRequest()
{
    providePresentingApplicationPIDIfNecessary();
}

void MediaSessionManagerCocoa::scheduleSessionStatusUpdate()
{
    m_taskQueue.enqueueTask([this] () mutable {
        updateNowPlayingInfo();

        forEachSession([] (auto& session) {
            session.updateMediaUsageIfChanged();
        });
    });
}

bool MediaSessionManagerCocoa::sessionWillBeginPlayback(PlatformMediaSession& session)
{
    if (!PlatformMediaSessionManager::sessionWillBeginPlayback(session))
        return false;

    scheduleSessionStatusUpdate();
    return true;
}

void MediaSessionManagerCocoa::sessionDidEndRemoteScrubbing(PlatformMediaSession&)
{
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::addSession(PlatformMediaSession& session)
{
    if (!m_remoteCommandListener)
        m_remoteCommandListener = RemoteCommandListener::create(*this);

    if (!m_audioHardwareListener)
        m_audioHardwareListener = AudioHardwareListener::create(*this);

    PlatformMediaSessionManager::addSession(session);
}

void MediaSessionManagerCocoa::removeSession(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::removeSession(session);

    if (hasNoSession()) {
        m_remoteCommandListener = nullptr;
        m_audioHardwareListener = nullptr;
    }

    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::setCurrentSession(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::setCurrentSession(session);

    if (m_remoteCommandListener)
        m_remoteCommandListener->updateSupportedCommands();
}

void MediaSessionManagerCocoa::sessionWillEndPlayback(PlatformMediaSession& session, DelayCallingUpdateNowPlaying delayCallingUpdateNowPlaying)
{
    PlatformMediaSessionManager::sessionWillEndPlayback(session, delayCallingUpdateNowPlaying);

    m_taskQueue.enqueueTask([weakSession = makeWeakPtr(session)] {
        if (weakSession)
            weakSession->updateMediaUsageIfChanged();
    });

    if (delayCallingUpdateNowPlaying == DelayCallingUpdateNowPlaying::No)
        updateNowPlayingInfo();
    else {
        m_taskQueue.enqueueTask([this] {
            updateNowPlayingInfo();
        });
    }
}

void MediaSessionManagerCocoa::clientCharacteristicsChanged(PlatformMediaSession& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::sessionCanProduceAudioChanged()
{
    PlatformMediaSessionManager::sessionCanProduceAudioChanged();
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::clearNowPlayingInfo()
{
    if (canLoad_MediaRemote_MRMediaRemoteSetNowPlayingVisibility())
        MRMediaRemoteSetNowPlayingVisibility(MRMediaRemoteGetLocalOrigin(), MRNowPlayingClientVisibilityNeverVisible);

    MRMediaRemoteSetCanBeNowPlayingApplication(false);
    MRMediaRemoteSetNowPlayingInfo(nullptr);
    MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(MRMediaRemoteGetLocalOrigin(), kMRPlaybackStateStopped, dispatch_get_main_queue(), ^(MRMediaRemoteError error) {
#if LOG_DISABLED
        UNUSED_PARAM(error);
#else
        if (error)
            WTFLogAlways("MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(stopped) failed with error %d", error);
#endif
    });
}

void MediaSessionManagerCocoa::setNowPlayingInfo(bool setAsNowPlayingApplication, const NowPlayingInfo& nowPlayingInfo)
{
    if (setAsNowPlayingApplication)
        MRMediaRemoteSetCanBeNowPlayingApplication(true);

    auto info = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    if (!nowPlayingInfo.title.isEmpty())
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoTitle, nowPlayingInfo.title.createCFString().get());

    if (std::isfinite(nowPlayingInfo.duration) && nowPlayingInfo.duration != MediaPlayer::invalidTime()) {
        auto cfDuration = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &nowPlayingInfo.duration));
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoDuration, cfDuration.get());
    }

    double rate = nowPlayingInfo.isPlaying ? 1 : 0;
    auto cfRate = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &rate));
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoPlaybackRate, cfRate.get());

    auto lastUpdatedNowPlayingInfoUniqueIdentifier = nowPlayingInfo.uniqueIdentifier.toUInt64();
    auto cfIdentifier = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &lastUpdatedNowPlayingInfoUniqueIdentifier));
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoUniqueIdentifier, cfIdentifier.get());

    if (std::isfinite(nowPlayingInfo.currentTime) && nowPlayingInfo.currentTime != MediaPlayer::invalidTime() && nowPlayingInfo.supportsSeeking) {
        auto cfCurrentTime = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &nowPlayingInfo.currentTime));
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoElapsedTime, cfCurrentTime.get());
    }

    if (canLoad_MediaRemote_MRMediaRemoteSetParentApplication() && !nowPlayingInfo.sourceApplicationIdentifier.isEmpty())
        MRMediaRemoteSetParentApplication(MRMediaRemoteGetLocalOrigin(), nowPlayingInfo.sourceApplicationIdentifier.createCFString().get());

    MRPlaybackState playbackState = (nowPlayingInfo.isPlaying) ? kMRPlaybackStatePlaying : kMRPlaybackStatePaused;
    MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(MRMediaRemoteGetLocalOrigin(), playbackState, dispatch_get_main_queue(), ^(MRMediaRemoteError error) {
#if LOG_DISABLED
        UNUSED_PARAM(error);
#else
        if (error)
            WTFLogAlways("MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(playing) failed with error %d", error);
#endif
    });
    MRMediaRemoteSetNowPlayingInfo(info.get());

    if (canLoad_MediaRemote_MRMediaRemoteSetNowPlayingVisibility()) {
        MRNowPlayingClientVisibility visibility = nowPlayingInfo.allowsNowPlayingControlsVisibility ? MRNowPlayingClientVisibilityAlwaysVisible : MRNowPlayingClientVisibilityNeverVisible;
        MRMediaRemoteSetNowPlayingVisibility(MRMediaRemoteGetLocalOrigin(), visibility);
    }
}

PlatformMediaSession* MediaSessionManagerCocoa::nowPlayingEligibleSession()
{
    // FIXME: Fix this layering violation.
    if (auto element = HTMLMediaElement::bestMediaElementForShowingPlaybackControlsManager(MediaElementSession::PlaybackControlsPurpose::NowPlaying))
        return &element->mediaSession();

    return nullptr;
}

void MediaSessionManagerCocoa::updateNowPlayingInfo()
{
    if (!isMediaRemoteFrameworkAvailable())
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    Optional<NowPlayingInfo> nowPlayingInfo;
    if (auto* session = nowPlayingEligibleSession())
        nowPlayingInfo = session->nowPlayingInfo();

    if (!nowPlayingInfo) {

        if (m_registeredAsNowPlayingApplication) {
            ALWAYS_LOG(LOGIDENTIFIER, "clearing now playing info");
            platformStrategies()->mediaStrategy().clearNowPlayingInfo();
        }

        m_registeredAsNowPlayingApplication = false;
        m_nowPlayingActive = false;
        m_lastUpdatedNowPlayingTitle = emptyString();
        m_lastUpdatedNowPlayingDuration = NAN;
        m_lastUpdatedNowPlayingElapsedTime = NAN;
        m_lastUpdatedNowPlayingInfoUniqueIdentifier = { };

        return;
    }

    m_haveEverRegisteredAsNowPlayingApplication = true;
    platformStrategies()->mediaStrategy().setNowPlayingInfo(!m_registeredAsNowPlayingApplication, *nowPlayingInfo);

    if (!m_registeredAsNowPlayingApplication) {
        m_registeredAsNowPlayingApplication = true;
        providePresentingApplicationPIDIfNecessary();
    }

    ALWAYS_LOG(LOGIDENTIFIER, "title = \"", nowPlayingInfo->title, "\", isPlaying = ", nowPlayingInfo->isPlaying, ", duration = ", nowPlayingInfo->duration, ", now = ", nowPlayingInfo->currentTime, ", id = ", nowPlayingInfo->uniqueIdentifier.toUInt64(), ", registered = ", m_registeredAsNowPlayingApplication);

    if (!nowPlayingInfo->title.isEmpty())
        m_lastUpdatedNowPlayingTitle = nowPlayingInfo->title;

    double duration = nowPlayingInfo->duration;
    if (std::isfinite(duration) && duration != MediaPlayer::invalidTime())
        m_lastUpdatedNowPlayingDuration = duration;

    m_lastUpdatedNowPlayingInfoUniqueIdentifier = nowPlayingInfo->uniqueIdentifier;

    double currentTime = nowPlayingInfo->currentTime;
    if (std::isfinite(currentTime) && currentTime != MediaPlayer::invalidTime() && nowPlayingInfo->supportsSeeking)
        m_lastUpdatedNowPlayingElapsedTime = currentTime;

    m_nowPlayingActive = nowPlayingInfo->allowsNowPlayingControlsVisibility;

    END_BLOCK_OBJC_EXCEPTIONS
}

void MediaSessionManagerCocoa::audioOutputDeviceChanged()
{
    AudioSession::sharedSession().audioOutputDeviceChanged();
    updateSessionState();
}

} // namespace WebCore

#endif // USE(AUDIO_SESSION) && PLATFORM(COCOA)
