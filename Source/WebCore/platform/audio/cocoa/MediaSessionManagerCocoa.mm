/*
 * Copyright (C) 2013-2026 Apple Inc. All rights reserved.
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

#import "AudioUtilities.h"
#import "DeprecatedGlobalSettings.h"
#import "ImageAdapter.h"
#import "Logging.h"
#import "MediaPlayer.h"
#import "MediaStrategy.h"
#import "NowPlayingInfo.h"
#import "Page.h"
#import "PlatformMediaConfiguration.h"
#import "PlatformMediaSession.h"
#import "PlatformStrategies.h"
#import "Settings.h"
#import "SharedBuffer.h"
#import "VP9UtilitiesCocoa.h"
#import <pal/SessionID.h>
#import <pal/spi/cocoa/AudioToolboxSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Function.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

#import "MediaRemoteSoftLink.h"
#include <pal/cocoa/AVFoundationSoftLink.h>

static const size_t kLowPowerVideoBufferSize = 4096;

#if RELEASE_LOG_DISABLED
#define MEDIASESSIONMANAGER_RELEASE_LOG(formatString, ...)
#else
#define MEDIASESSIONMANAGER_RELEASE_LOG(formatString, ...) \
do { \
    if (willLog(WTFLogLevel::Always)) { \
        RELEASE_LOG_FORWARDABLE(Media, MEDIASESSIONMANAGERCOCOA_##formatString, ##__VA_ARGS__); \
        if (logger().hasEnabledInspector()) { \
            char buffer[1024] = { 0 }; \
            SAFE_SPRINTF(std::span { buffer }, MESSAGE_MEDIASESSIONMANAGERCOCOA_##formatString, ##__VA_ARGS__); \
            logger().toObservers(logChannel(), WTFLogLevel::Always, String::fromUTF8(buffer)); \
        } \
    } \
} while (0)
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaSessionManagerCocoa);

#if PLATFORM(MAC)
RefPtr<PlatformMediaSessionManager> PlatformMediaSessionManager::create(PageIdentifier pageIdentifier)
{
    return MediaSessionManagerCocoa::create(pageIdentifier);
}

Ref<MediaSessionManagerCocoa> MediaSessionManagerCocoa::create(PageIdentifier pageIdentifier)
{
    return adoptRef(*new MediaSessionManagerCocoa(pageIdentifier));
}
#endif // !PLATFORM(MAC)

MediaSessionManagerCocoa::MediaSessionManagerCocoa(PageIdentifier pageIdentifier)
    : PlatformMediaSessionManager(pageIdentifier)
    , m_nowPlayingManager(hasPlatformStrategies() ? platformStrategies()->mediaStrategy()->createNowPlayingManager() : nullptr)
    , m_nowPlayingUpdateTimer(RunLoop::mainSingleton(), "MediaSessionManagerCocoa::NowPlayingUpdateTimer"_s, this, &MediaSessionManagerCocoa::updateNowPlayingInfo)
    , m_delayCategoryChangeTimer(RunLoop::mainSingleton(), "MediaSessionManagerCocoa::DelayCategoryChangeTimer"_s, this, &MediaSessionManagerCocoa::possiblyChangeAudioCategory)
{
}

void MediaSessionManagerCocoa::updateSessionState()
{
    constexpr auto delayBeforeSettingCategoryNone = 2_s;
    int videoCount = 0;
    int videoAudioCount = 0;
    int audioCount = 0;
    int webAudioCount = 0;
    int audioMediaStreamTrackCount = 0;
    int domMediaSessionCount = 0;
    int captureCount = countActiveAudioCaptureSources();

    bool hasAudibleAudioOrVideoMediaType = false;
    bool hasAudibleVideoMediaType = false;
    bool isPlayingAudio = false;
    forEachSession([&] (auto& session) mutable {
        auto type = session.mediaType();
        switch (type) {
        case PlatformMediaSession::MediaType::None:
            break;
        case PlatformMediaSession::MediaType::DOMMediaSession:
            ++domMediaSessionCount;
            break;
        case PlatformMediaSession::MediaType::Video:
            ++videoCount;
            break;
        case PlatformMediaSession::MediaType::VideoAudio:
            ++videoAudioCount;
            if (session.canProduceAudio() && session.hasMediaStreamSource())
                ++audioMediaStreamTrackCount;
            break;
        case PlatformMediaSession::MediaType::Audio:
            ++audioCount;
            if (session.canProduceAudio() && session.hasMediaStreamSource())
                ++audioMediaStreamTrackCount;
            break;
        case PlatformMediaSession::MediaType::WebAudio:
            if (session.canProduceAudio()) {
                ++webAudioCount;
                isPlayingAudio |= session.isPlaying() && session.isAudible();
            }
            break;
        }

        if (!hasAudibleAudioOrVideoMediaType) {
            bool isPotentiallyAudible = session.isPlayingToWirelessPlaybackTarget()
            || ((type == PlatformMediaSession::MediaType::VideoAudio || type == PlatformMediaSession::MediaType::Audio)
                && session.isAudible()
                && (session.isPlaying() || session.preparingToPlay() || session.hasPlayedAudiblySinceLastInterruption()));
            if (isPotentiallyAudible) {
                hasAudibleAudioOrVideoMediaType = true;
                hasAudibleVideoMediaType |= type == PlatformMediaSession::MediaType::VideoAudio;
                isPlayingAudio |= session.isPlaying();
            }
        }
    });

    MEDIASESSIONMANAGER_RELEASE_LOG(UPDATESESSIONSTATE, captureCount, audioMediaStreamTrackCount, videoCount, audioCount, videoAudioCount, webAudioCount, domMediaSessionCount);

    Ref sharedSession = AudioSession::singleton();
    if (!m_defaultBufferSize)
        m_defaultBufferSize = sharedSession->preferredBufferSize();
    size_t bufferSize = m_defaultBufferSize.value();

    if (webAudioCount)
        bufferSize = AudioUtilities::renderQuantumSize;
    else if (captureCount || audioMediaStreamTrackCount) {
        // In case of audio capture or audio MediaStreamTrack playing, we want to grab 20 ms chunks to limit the latency so that it is not noticeable by users
        // while having a large enough buffer so that the audio rendering remains stable, hence a computation based on sample rate.
        bufferSize = roundUpToPowerOfTwo<size_t>(sharedSession->sampleRate() / 50);
    } else if (m_supportedAudioHardwareBufferSizes && DeprecatedGlobalSettings::lowPowerVideoAudioBufferSizeEnabled())
        bufferSize = m_supportedAudioHardwareBufferSizes.nearest(kLowPowerVideoBufferSize);

    sharedSession->setPreferredBufferSize(bufferSize);

    if (!DeprecatedGlobalSettings::shouldManageAudioSessionCategory())
        return;

    auto category = AudioSession::CategoryType::None;
    auto mode = AudioSession::Mode::Default;
    if (sharedSession->categoryOverride() != AudioSession::CategoryType::None)
        category = sharedSession->categoryOverride();
    else if (captureCount || (isPlayingAudio && sharedSession->category() == AudioSession::CategoryType::PlayAndRecord)) {
        category = AudioSession::CategoryType::PlayAndRecord;
        mode = AudioSession::Mode::VideoChat;
    } else if (hasAudibleVideoMediaType) {
        category = AudioSession::CategoryType::MediaPlayback;
#if PLATFORM(VISION) || PLATFORM(APPLETV)
        // On visionOS and tvOS, Mode::MoviePlayback is best tuned for web videos that contain music, speech, etc.
        mode = AudioSession::Mode::MoviePlayback;
#endif
    } else if (hasAudibleAudioOrVideoMediaType)
        category = AudioSession::CategoryType::MediaPlayback;
    else if (webAudioCount)
        category = AudioSession::CategoryType::AmbientSound;

    if (category == AudioSession::CategoryType::None && m_previousCategory != AudioSession::CategoryType::None && m_previousCategory != AudioSession::CategoryType::PlayAndRecord) {
        if (!m_delayCategoryChangeTimer.isActive()) {
            m_delayCategoryChangeTimer.startOneShot(delayBeforeSettingCategoryNone);
            ALWAYS_LOG(LOGIDENTIFIER, "setting timer");
        }
        category = m_previousCategory;
    } else
        m_delayCategoryChangeTimer.stop();

    if (mode == AudioSession::Mode::Default && category == AudioSession::CategoryType::PlayAndRecord)
        mode = AudioSession::Mode::VideoChat;

    RouteSharingPolicy policy = (category == AudioSession::CategoryType::MediaPlayback) ? RouteSharingPolicy::LongFormAudio : RouteSharingPolicy::Default;

    ALWAYS_LOG(LOGIDENTIFIER, "setting category = ", category, ", mode = ", mode, ", policy = ", policy, ", previous category = ", m_previousCategory);

    m_previousCategory = category;
    sharedSession->setCategory(category, mode, policy);

    forEachSession([&] (auto& session) {
        session.audioSessionCategoryChanged(category, mode, policy);
    });
}

void MediaSessionManagerCocoa::possiblyChangeAudioCategory()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_previousCategory = AudioSession::CategoryType::None;
    updateSessionState();
}

void MediaSessionManagerCocoa::resetSessionState()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_delayCategoryChangeTimer.stop();
    m_previousCategory = AudioSession::CategoryType::None;
    m_previousHadAudibleAudioOrVideoMediaType = false;
}

void MediaSessionManagerCocoa::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    if (type == PlatformMediaSession::InterruptionType::SystemInterruption) {
        forEachSession([] (auto& session) {
            session.setHasPlayedAudiblySinceLastInterruption(false);
        });
    }

    PlatformMediaSessionManager::beginInterruption(type);
}

String MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(MediaPlayer::PitchCorrectionAlgorithm pitchCorrectionAlgorithm, bool preservesPitch, double rate)
{
    if (!preservesPitch || !rate || rate == 1.)
        return AVAudioTimePitchAlgorithmVarispeed;

    switch (pitchCorrectionAlgorithm) {
    case MediaPlayer::PitchCorrectionAlgorithm::BestAllAround:
    case MediaPlayer::PitchCorrectionAlgorithm::BestForMusic:
        return AVAudioTimePitchAlgorithmSpectral;
    case MediaPlayer::PitchCorrectionAlgorithm::BestForSpeech:
        return AVAudioTimePitchAlgorithmTimeDomain;
    }
}

void MediaSessionManagerCocoa::scheduleSessionStatusUpdate()
{
    enqueueTaskOnMainThread([this, protectedThis = Ref { *this }] () mutable {
        if (m_nowPlayingManager) {
            m_nowPlayingManager->setSupportsSeeking(computeSupportsSeeking());
            updateNowPlayingInfo();
        }

        forEachSession([] (auto& session) {
            session.updateMediaUsageIfChanged();
        });
    });
}

void MediaSessionManagerCocoa::sessionWillBeginPlayback(PlatformMediaSessionInterface& session, CompletionHandler<void(bool)>&& completionHandler)
{
    PlatformMediaSessionManager::sessionWillBeginPlayback(session, [weakThis = ThreadSafeWeakPtr { *this }, completionHandler = WTF::move(completionHandler)](bool willBegin) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !willBegin) {
            completionHandler(false);
            return;
        }

        protectedThis->scheduleSessionStatusUpdate();
        completionHandler(true);
    });
}

void MediaSessionManagerCocoa::sessionDidEndRemoteScrubbing(PlatformMediaSessionInterface&)
{
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::addSession(PlatformMediaSessionInterface& session)
{
    if (m_nowPlayingManager)
        m_nowPlayingManager->addClient(*this);

    if (!m_audioHardwareListener) {
        m_audioHardwareListener = AudioHardwareListener::create(*this);
        m_supportedAudioHardwareBufferSizes = m_audioHardwareListener->supportedBufferSizes();
    }

    PlatformMediaSessionManager::addSession(session);
}

void MediaSessionManagerCocoa::removeSession(PlatformMediaSessionInterface& session)
{
    PlatformMediaSessionManager::removeSession(session);

    if (session.isActiveNowPlayingSession()) {
        session.setActiveNowPlayingSession(false);
        if (RefPtr page = Page::fromPageIdentifier(pageIdentifier()))
            page->hasActiveNowPlayingSessionChanged();
    }

    if (hasNoSession()) {
        if (m_nowPlayingManager)
            m_nowPlayingManager->removeClient(*this);
        m_audioHardwareListener = nullptr;
    }

    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::setCurrentSession(PlatformMediaSessionInterface& session)
{
    PlatformMediaSessionManager::setCurrentSession(session);
    if (m_nowPlayingManager)
        m_nowPlayingManager->updateSupportedCommands();
}

void MediaSessionManagerCocoa::sessionWillEndPlayback(PlatformMediaSessionInterface& session, DelayCallingUpdateNowPlaying delayCallingUpdateNowPlaying)
{
    PlatformMediaSessionManager::sessionWillEndPlayback(session, delayCallingUpdateNowPlaying);

    enqueueTaskOnMainThread([weakSession = WeakPtr { session }] {
        if (weakSession)
            weakSession->updateMediaUsageIfChanged();
    });

    if (delayCallingUpdateNowPlaying == DelayCallingUpdateNowPlaying::No)
        updateNowPlayingInfo();
    else {
        enqueueTaskOnMainThread([this, protectedThis = Ref { *this }] {
            updateNowPlayingInfo();
        });
    }

#if USE(AUDIO_SESSION)
    // De-activate the audio session if the last playing session is:
    // * An audio presentation
    // * Is in the ended state
    // * Has a short duration
    // This allows other applications to resume playback after an "alert-like" audio
    // is played by web content.

    if (!anyOfSessions([] (auto& session) { return session.state() == PlatformMediaSession::State::Playing; })
        && session.presentationType() == PlatformMediaSession::MediaType::Audio
        && session.isEnded()
        && session.isLongEnoughForMainContent())
        maybeDeactivateAudioSession();
#endif
}

void MediaSessionManagerCocoa::clientCharacteristicsChanged(PlatformMediaSessionInterface& session, bool)
{
    MEDIASESSIONMANAGER_RELEASE_LOG(CLIENTCHARACTERISTICSCHANGED, session.logIdentifier());
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::sessionCanProduceAudioChanged()
{
    MEDIASESSIONMANAGER_RELEASE_LOG(SESSIONCANPRODUCEAUDIOCHANGED);
    PlatformMediaSessionManager::sessionCanProduceAudioChanged();
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::addSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    ASSERT(m_nowPlayingManager);
    m_nowPlayingManager->addSupportedCommand(command);
}

void MediaSessionManagerCocoa::removeSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    ASSERT(m_nowPlayingManager);
    m_nowPlayingManager->removeSupportedCommand(command);
}

RemoteCommandListener::RemoteCommandsSet MediaSessionManagerCocoa::supportedCommands() const
{
    ASSERT(m_nowPlayingManager);
    return m_nowPlayingManager->supportedCommands();
}

void MediaSessionManagerCocoa::clearNowPlayingInfo()
{
    if (!isMediaRemoteFrameworkAvailable())
        return;

    if (canLoad_MediaRemote_MRMediaRemoteSetNowPlayingVisibility())
        MRMediaRemoteSetNowPlayingVisibility(MRMediaRemoteGetLocalOrigin(), MRNowPlayingClientVisibilityNeverVisible);

    MRMediaRemoteSetCanBeNowPlayingApplication(false);
    MRMediaRemoteSetNowPlayingInfo(nullptr);
    MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(MRMediaRemoteGetLocalOrigin(), kMRPlaybackStateStopped, mainDispatchQueueSingleton(), ^(MRMediaRemoteError error) {
#if LOG_DISABLED
        UNUSED_PARAM(error);
#else
        if (error)
            WTFLogAlways("MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(stopped) failed with error %d", error);
#endif
    });

#if USE(NOW_PLAYING_ACTIVITY_SUPPRESSION)
    updateNowPlayingSuppression(nullptr);
#endif
}

void MediaSessionManagerCocoa::setNowPlayingInfo(bool setAsNowPlayingApplication, bool shouldUpdateNowPlayingSuppression, const NowPlayingInfo& nowPlayingInfo)
{
    if (!isMediaRemoteFrameworkAvailable())
        return;

#if USE(NOW_PLAYING_ACTIVITY_SUPPRESSION)
    if (shouldUpdateNowPlayingSuppression)
        updateNowPlayingSuppression(&nowPlayingInfo);
#else
    ASSERT_UNUSED(shouldUpdateNowPlayingSuppression, !shouldUpdateNowPlayingSuppression);
#endif

    if (setAsNowPlayingApplication)
        MRMediaRemoteSetCanBeNowPlayingApplication(true);

    auto info = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtist, nowPlayingInfo.metadata.artist.createCFString().get());
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoAlbum, nowPlayingInfo.metadata.album.createCFString().get());
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoTitle, nowPlayingInfo.metadata.title.createCFString().get());

    if (std::isfinite(nowPlayingInfo.duration) && !std::isnan(nowPlayingInfo.duration)) {
        auto cfDuration = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &nowPlayingInfo.duration));
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoDuration, cfDuration.get());
    }

    double rate = nowPlayingInfo.isPlaying ? nowPlayingInfo.rate : 0;
    auto cfRate = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &rate));
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoPlaybackRate, cfRate.get());

    // FIXME: This is a workaround Control Center not updating the artwork when refreshed.
    // We force the identifier to be reloaded to the new artwork if available.
    auto lastUpdatedNowPlayingInfoUniqueIdentifier = nowPlayingInfo.metadata.artwork ? nowPlayingInfo.metadata.artwork->src.hash() : (nowPlayingInfo.uniqueIdentifier ? nowPlayingInfo.uniqueIdentifier->toUInt64() : 0);
    auto cfIdentifier = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &lastUpdatedNowPlayingInfoUniqueIdentifier));
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoUniqueIdentifier, cfIdentifier.get());

    if (std::isfinite(nowPlayingInfo.currentTime) && !std::isnan(nowPlayingInfo.currentTime) && nowPlayingInfo.supportsSeeking) {
        auto cfCurrentTime = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &nowPlayingInfo.currentTime));
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoElapsedTime, cfCurrentTime.get());
    }
    RetainPtr tiffImage = nowPlayingInfo.metadata.artwork && nowPlayingInfo.metadata.artwork->image ? Ref { *nowPlayingInfo.metadata.artwork->image }->adapter().tiffRepresentation() : nullptr;
    if (tiffImage) {
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkData, tiffImage.get());
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkMIMEType, @"image/tiff");
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkDataWidth, [NSNumber numberWithFloat:Ref { *nowPlayingInfo.metadata.artwork->image }->width()]);
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkDataHeight, [NSNumber numberWithFloat:Ref { *nowPlayingInfo.metadata.artwork->image }->height()]);
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkIdentifier, String::number(nowPlayingInfo.metadata.artwork->src.hash()).createCFString().get());
    }

    if (canLoad_MediaRemote_MRMediaRemoteSetParentApplication() && !nowPlayingInfo.metadata.sourceApplicationIdentifier.isEmpty())
        MRMediaRemoteSetParentApplication(MRMediaRemoteGetLocalOrigin(), nowPlayingInfo.metadata.sourceApplicationIdentifier.createCFString().get());

    MRPlaybackState playbackState = (nowPlayingInfo.isPlaying) ? kMRPlaybackStatePlaying : kMRPlaybackStatePaused;
    MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(MRMediaRemoteGetLocalOrigin(), playbackState, mainDispatchQueueSingleton(), ^(MRMediaRemoteError error) {
#if LOG_DISABLED
        UNUSED_PARAM(error);
#else
        if (error)
            WTFLogAlways("MRMediaRemoteSetNowPlayingApplicationPlaybackStateForOrigin(playing) failed with error %d", error);
#endif
    });
    MRMediaRemoteSetNowPlayingInfoWithMergePolicy(info.get(), MRMediaRemoteMergePolicyReplace);

    if (canLoad_MediaRemote_MRMediaRemoteSetNowPlayingVisibility()) {
        MRNowPlayingClientVisibility visibility = nowPlayingInfo.allowsNowPlayingControlsVisibility ? MRNowPlayingClientVisibilityAlwaysVisible : MRNowPlayingClientVisibilityNeverVisible;
        MRMediaRemoteSetNowPlayingVisibility(MRMediaRemoteGetLocalOrigin(), visibility);
    }
}

WeakPtr<PlatformMediaSessionInterface> MediaSessionManagerCocoa::nowPlayingEligibleSession()
{
    return bestEligibleSessionForRemoteControls([](auto& session) {
        return session.isNowPlayingEligible();
    }, PlatformMediaSession::PlaybackControlsPurpose::NowPlaying);
}

void MediaSessionManagerCocoa::updateActiveNowPlayingSession(RefPtr<PlatformMediaSessionInterface> activeNowPlayingSession)
{
    bool activeSessionChanged = false;
    forEachSession([&](auto& session) {
        bool newSessionState = &session == activeNowPlayingSession.get();
        if (session.isActiveNowPlayingSession() != newSessionState)
            activeSessionChanged = true;
        session.setActiveNowPlayingSession(newSessionState);
    });

    if (activeSessionChanged) {
        if (RefPtr page = Page::fromPageIdentifier(pageIdentifier()))
            page->hasActiveNowPlayingSessionChanged();

        adjustNowPlayingUpdateInterval();
    }
}

bool MediaSessionManagerCocoa::shouldUpdateNowPlaying(const NowPlayingInfo& nowPlayingInfo)
{
    ASSERT(isMediaRemoteFrameworkAvailable());

    if (!m_haveEverRegisteredAsNowPlayingApplication || !m_nowPlayingInfo) {
        INFO_LOG(LOGIDENTIFIER, "!haveEverRegisteredAsNowPlayingApplication || !nowPlayingInfo");
        return true;
    }

    if (m_nowPlayingInfo->uniqueIdentifier != nowPlayingInfo.uniqueIdentifier) {
        INFO_LOG(LOGIDENTIFIER, "uniqueIdentifier changed");
        return true;
    }

    if (m_nowPlayingInfo->metadata != nowPlayingInfo.metadata) {
        INFO_LOG(LOGIDENTIFIER, "metadata changed");
        return true;
    }

    auto duration = nowPlayingInfo.duration;
    if (std::isfinite(duration) && !std::isnan(duration) && m_nowPlayingInfo->duration != duration) {
        INFO_LOG(LOGIDENTIFIER, "duration changed");
        return true;
    }

    if (m_nowPlayingInfo->rate != nowPlayingInfo.rate) {
        INFO_LOG(LOGIDENTIFIER, "rate changed");
        return true;
    }

    if (m_nowPlayingInfo->supportsSeeking != nowPlayingInfo.supportsSeeking) {
        INFO_LOG(LOGIDENTIFIER, "supportsSeeking changed");
        return true;
    }

    if (m_nowPlayingInfo->isPlaying != nowPlayingInfo.isPlaying) {
        INFO_LOG(LOGIDENTIFIER, "isPlaying changed");
        return true;
    }

    if (m_nowPlayingInfo->allowsNowPlayingControlsVisibility != nowPlayingInfo.allowsNowPlayingControlsVisibility) {
        INFO_LOG(LOGIDENTIFIER, "allowsNowPlayingControlsVisibility changed");
        return true;
    }

    if (m_nowPlayingInfo->isVideo != nowPlayingInfo.isVideo) {
        INFO_LOG(LOGIDENTIFIER, "isVideo changed");
        return true;
    }

    if (std::isfinite(m_nowPlayingInfo->currentTime) != std::isfinite(nowPlayingInfo.currentTime)) {
        INFO_LOG(LOGIDENTIFIER, "currentTime changed");
        return true;
    }

    if (std::isnan(m_nowPlayingInfo->currentTime) != std::isnan(nowPlayingInfo.currentTime)) {
        INFO_LOG(LOGIDENTIFIER, "currentTime is valid changed");
        return true;
    }

    auto currentTime = nowPlayingInfo.currentTime;

    // Always update when currentTime changes while paused.
    if (nowPlayingInfo.supportsSeeking && !nowPlayingInfo.isPlaying) {
        bool didChange = m_nowPlayingInfo->currentTime != currentTime;
        INFO_LOG_IF(didChange, LOGIDENTIFIER, "paused and current time changed");
        return didChange;
    }

    if (std::isfinite(currentTime) && !std::isnan(currentTime) && std::abs(m_nowPlayingInfo->currentTime - currentTime) > m_nowPlayingUpdateInterval.value()) {
        INFO_LOG(LOGIDENTIFIER, "current time changed enough");
        return true;
    }

    return false;
}

void MediaSessionManagerCocoa::adjustNowPlayingUpdateInterval()
{
    RefPtr session = nowPlayingEligibleSession().get();
    if (!session)
        return;

    auto interval = [&] {
        constexpr Seconds defaultNowPlayingUpdateInterval = 5_s;

        auto nowPlayingInfo = session->nowPlayingInfo();
        if (!nowPlayingInfo)
            return defaultNowPlayingUpdateInterval;

        auto duration = nowPlayingInfo->duration;
        if (!duration || !std::isfinite(duration) || std::isnan(duration))
            return defaultNowPlayingUpdateInterval;

        Seconds minNowPlayingUpdateInterval = Seconds(duration / 4);
        if (m_nowPlayingUpdateInterval > minNowPlayingUpdateInterval)
            return minNowPlayingUpdateInterval;

        return defaultNowPlayingUpdateInterval;
    }();

    if (interval != m_nowPlayingUpdateInterval) {
        m_nowPlayingUpdateInterval = interval;
        ALWAYS_LOG(LOGIDENTIFIER, m_nowPlayingUpdateInterval.value());
    }
}

void MediaSessionManagerCocoa::setNowPlayingUpdateInterval(double interval)
{
    ALWAYS_LOG(LOGIDENTIFIER, interval);
    m_nowPlayingUpdateInterval = Seconds(interval);
    adjustNowPlayingUpdateInterval();
}

double MediaSessionManagerCocoa::nowPlayingUpdateInterval()
{
    return m_nowPlayingUpdateInterval.value();
}

void MediaSessionManagerCocoa::updateNowPlayingInfo()
{
    ASSERT(m_nowPlayingManager);

    if (!isMediaRemoteFrameworkAvailable())
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    std::optional<NowPlayingInfo> nowPlayingInfo;
    RefPtr session = nowPlayingEligibleSession().get();
    if (session)
        nowPlayingInfo = session->nowPlayingInfo();

    if (!nowPlayingInfo) {

        m_nowPlayingUpdateTimer.stop();

        if (m_registeredAsNowPlayingApplication) {
            ALWAYS_LOG(LOGIDENTIFIER, "clearing now playing info");
            m_nowPlayingManager->clearNowPlayingInfo();
        }

        m_registeredAsNowPlayingApplication = false;
        m_nowPlayingActive = false;
        m_lastUpdatedNowPlayingTitle = emptyString();
        m_lastUpdatedNowPlayingDuration = NAN;
        m_lastUpdatedNowPlayingElapsedTime = NAN;
        m_lastUpdatedNowPlayingInfoUniqueIdentifier = std::nullopt;
        m_nowPlayingInfo = std::nullopt;

        updateActiveNowPlayingSession(nullptr);
        return;
    }

    if (!m_nowPlayingInfo)
        adjustNowPlayingUpdateInterval();

    m_nowPlayingUpdateTimer.startOneShot(m_nowPlayingUpdateInterval);

    double currentTime = nowPlayingInfo->currentTime;
    if (!shouldUpdateNowPlaying(*nowPlayingInfo)) {
        INFO_LOG(LOGIDENTIFIER, "Skipping update at ", currentTime);
        return;
    }

    m_haveEverRegisteredAsNowPlayingApplication = true;

    if (m_nowPlayingManager->setNowPlayingInfo(*nowPlayingInfo)) {
#ifdef RELEASE_LOG_DISABLED
        String src = "src"_s;
        String title = "title"_s;
#else
        String src = nowPlayingInfo->metadata.artwork ? nowPlayingInfo->metadata.artwork->src : String();
        String title = nowPlayingInfo->metadata.title;
#endif
        ALWAYS_LOG(LOGIDENTIFIER, "title = \"", title, "\", isPlaying = ", nowPlayingInfo->isPlaying, ", duration = ", nowPlayingInfo->duration, ", now = ", currentTime, ", id = ", (nowPlayingInfo->uniqueIdentifier ? nowPlayingInfo->uniqueIdentifier->toUInt64() : 0), ", registered = ", m_registeredAsNowPlayingApplication, ", src = \"", src, "\"");
    }
    if (!m_registeredAsNowPlayingApplication)
        m_registeredAsNowPlayingApplication = true;

    updateActiveNowPlayingSession(session);

    if (!m_nowPlayingInfo || nowPlayingInfo->metadata != m_nowPlayingInfo->metadata)
        nowPlayingMetadataChanged(nowPlayingInfo->metadata);

    if (!nowPlayingInfo->metadata.title.isEmpty())
        m_lastUpdatedNowPlayingTitle = nowPlayingInfo->metadata.title;

    double duration = nowPlayingInfo->duration;
    if (std::isfinite(duration) && !std::isnan(duration))
        m_lastUpdatedNowPlayingDuration = duration;

    m_lastUpdatedNowPlayingInfoUniqueIdentifier = nowPlayingInfo->uniqueIdentifier;

    if (std::isfinite(currentTime) && !std::isnan(currentTime) && nowPlayingInfo->supportsSeeking)
        m_lastUpdatedNowPlayingElapsedTime = currentTime;

    m_nowPlayingActive = nowPlayingInfo->allowsNowPlayingControlsVisibility;
    m_nowPlayingInfo = nowPlayingInfo;

    END_BLOCK_OBJC_EXCEPTIONS
}

void MediaSessionManagerCocoa::audioOutputDeviceChanged()
{
    ASSERT(m_audioHardwareListener);
    m_supportedAudioHardwareBufferSizes = m_audioHardwareListener->supportedBufferSizes();
    m_defaultBufferSize = AudioSession::singleton().preferredBufferSize();
    AudioSession::singleton().audioOutputDeviceChanged();
    updateSessionState();
}

#if PLATFORM(MAC)
std::optional<bool> MediaSessionManagerCocoa::supportsSpatialAudioPlaybackForConfiguration(const PlatformMediaConfiguration& configuration)
{
    ASSERT(configuration.audio);
    if (!configuration.audio)
        return { false };

    auto supportsSpatialAudioPlayback = this->supportsSpatialAudioPlayback();
    if (supportsSpatialAudioPlayback.has_value())
        return supportsSpatialAudioPlayback;

    auto calculateSpatialAudioSupport = [](const PlatformMediaConfiguration& configuration) {
        if (!PAL::canLoad_AudioToolbox_AudioGetDeviceSpatialPreferencesForContentType())
            return false;

        SpatialAudioPreferences spatialAudioPreferences { };
        auto contentType = configuration.video ? kAudioSpatialContentType_Audiovisual : kAudioSpatialContentType_AudioOnly;

        if (noErr != PAL::AudioGetDeviceSpatialPreferencesForContentType(nullptr, static_cast<SpatialContentTypeID>(contentType), &spatialAudioPreferences))
            return false;

        if (!spatialAudioPreferences.spatialAudioSourceCount)
            return false;

        auto channelCount = configuration.audio->channels.toDouble();
        if (channelCount <= 0)
            return true;

        for (auto& source : std::span { spatialAudioPreferences.spatialAudioSources }.first(spatialAudioPreferences.spatialAudioSourceCount)) {
            if (source == kSpatialAudioSource_Multichannel && channelCount > 2)
                return true;
            if (source == kSpatialAudioSource_MonoOrStereo && channelCount >= 1)
                return true;
        }

        return false;
    };

    setSupportsSpatialAudioPlayback(calculateSpatialAudioSupport(configuration));

    return this->supportsSpatialAudioPlayback();
}
#endif

#if USE(NOW_PLAYING_ACTIVITY_SUPPRESSION)

static id<MRNowPlayingActivityUIControllable> nowPlayingActivityController()
{
    static id<MRNowPlayingActivityUIControllable> controller = RetainPtr([getMRUIControllerProviderClassSingleton() nowPlayingActivityController]).leakRef();
    return controller;
}

void MediaSessionManagerCocoa::updateNowPlayingSuppression(const NowPlayingInfo* nowPlayingInfo)
{
    if (!nowPlayingInfo || !nowPlayingInfo->isVideo) {
        RELEASE_LOG(Media, "MediaSessionManagerCocoa::updateNowPlayingSuppression: clearing suppressPresentationOverBundleIdentifiers (hasNowPlayingInfo=%d, isVideo=%d)", !!nowPlayingInfo, nowPlayingInfo && nowPlayingInfo->isVideo);
        [nowPlayingActivityController() suppressPresentationOverBundleIdentifiers:nil];
    } else {
        RetainPtr parentApplicationBundleIdentifier = applicationBundleIdentifier().createNSString();
        RetainPtr presentingApplicationBundleIdentifier = nowPlayingInfo->metadata.sourceApplicationIdentifier.createNSString();
        NSSet *bundleIdentifiers = [NSSet setWithArray:@[parentApplicationBundleIdentifier.get(), presentingApplicationBundleIdentifier.get()]];
        RELEASE_LOG(Media, "MediaSessionManagerCocoa::updateNowPlayingSuppression: setting suppressPresentationOverBundleIdentifiers to %@", bundleIdentifiers);
        [nowPlayingActivityController() suppressPresentationOverBundleIdentifiers:bundleIdentifiers];
    }
}

#endif // USE(NOW_PLAYING_ACTIVITY_SUPPRESSION)

} // namespace WebCore

#endif // USE(AUDIO_SESSION) && PLATFORM(COCOA)
