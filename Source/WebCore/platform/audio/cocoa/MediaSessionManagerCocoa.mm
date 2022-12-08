/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#import "HTMLMediaElement.h"
#import "Logging.h"
#import "MediaPlayer.h"
#import "MediaStrategy.h"
#import "NowPlayingInfo.h"
#import "PlatformMediaSession.h"
#import "PlatformStrategies.h"
#import "SharedBuffer.h"
#import "VP9UtilitiesCocoa.h"
#import <pal/SessionID.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Function.h>

#import "MediaRemoteSoftLink.h"

static const size_t kLowPowerVideoBufferSize = 4096;

namespace WebCore {

#if PLATFORM(MAC)
std::unique_ptr<PlatformMediaSessionManager> PlatformMediaSessionManager::create()
{
    return makeUnique<MediaSessionManagerCocoa>();
}
#endif // !PLATFORM(MAC)

MediaSessionManagerCocoa::MediaSessionManagerCocoa()
    : m_nowPlayingManager(platformStrategies()->mediaStrategy().createNowPlayingManager())
    , m_defaultBufferSize(AudioSession::sharedSession().preferredBufferSize())
    , m_delayCategoryChangeTimer(RunLoop::main(), this, &MediaSessionManagerCocoa::possiblyChangeAudioCategory)
{
    ensureCodecsRegistered();
}

void MediaSessionManagerCocoa::ensureCodecsRegistered()
{
#if ENABLE(VP9)
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (shouldEnableVP9Decoder())
            registerSupplementalVP9Decoder();
        if (shouldEnableVP8Decoder())
            registerWebKitVP8Decoder();
        if (shouldEnableVP9SWDecoder())
            registerWebKitVP9Decoder();
    });
#endif
}

#if ENABLE(MEDIA_SOURCE) && HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
static bool s_mediaSourceInlinePaintingEnabled = false;
void MediaSessionManagerCocoa::setMediaSourceInlinePaintingEnabled(bool enabled)
{
    s_mediaSourceInlinePaintingEnabled = enabled;
}

bool MediaSessionManagerCocoa::mediaSourceInlinePaintingEnabled()
{
    return s_mediaSourceInlinePaintingEnabled;
}
#endif

#if HAVE(AVCONTENTKEYSPECIFIER)
static bool s_sampleBufferContentKeySessionSupportEnabled = false;
void MediaSessionManagerCocoa::setSampleBufferContentKeySessionSupportEnabled(bool enabled)
{
    s_sampleBufferContentKeySessionSupportEnabled = enabled;
}

bool MediaSessionManagerCocoa::sampleBufferContentKeySessionSupportEnabled()
{
    return s_sampleBufferContentKeySessionSupportEnabled;
}
#endif


void MediaSessionManagerCocoa::updateSessionState()
{
    constexpr auto delayBeforeSettingCategoryNone = 2_s;
    int videoCount = 0;
    int videoAudioCount = 0;
    int audioCount = 0;
    int webAudioCount = 0;
    int audioMediaStreamTrackCount = 0;
    int captureCount = countActiveAudioCaptureSources();
    bool hasAudibleAudioOrVideoMediaType = false;
    bool isPlayingAudio = false;
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
                isPlayingAudio |= session.isPlaying();
            }
            break;
        }

        if (!hasAudibleAudioOrVideoMediaType) {
            bool isPotentiallyAudible = session.isPlayingToWirelessPlaybackTarget()
                || ((type == PlatformMediaSession::MediaType::VideoAudio || type == PlatformMediaSession::MediaType::Audio)
                    && session.canProduceAudio()
                    && (session.hasPlayedSinceLastInterruption() || session.preparingToPlay()));
            if (isPotentiallyAudible) {
                hasAudibleAudioOrVideoMediaType = true;
                isPlayingAudio |= session.isPlaying();
            }
        }
    });

    ALWAYS_LOG(LOGIDENTIFIER, "types: "
        "AudioCapture(", captureCount, "), "
        "AudioTrack(", audioMediaStreamTrackCount, "), "
        "Video(", videoCount, "), "
        "Audio(", audioCount, "), "
        "VideoAudio(", videoAudioCount, "), "
        "WebAudio(", webAudioCount, ")");

    size_t bufferSize = m_defaultBufferSize;
    if (webAudioCount)
        bufferSize = AudioUtilities::renderQuantumSize;
    else if (captureCount || audioMediaStreamTrackCount) {
        // In case of audio capture or audio MediaStreamTrack playing, we want to grab 20 ms chunks to limit the latency so that it is not noticeable by users
        // while having a large enough buffer so that the audio rendering remains stable, hence a computation based on sample rate.
        bufferSize = AudioSession::sharedSession().sampleRate() / 50;
    } else if (m_supportedAudioHardwareBufferSizes && DeprecatedGlobalSettings::lowPowerVideoAudioBufferSizeEnabled())
        bufferSize = m_supportedAudioHardwareBufferSizes.nearest(kLowPowerVideoBufferSize);

    AudioSession::sharedSession().setPreferredBufferSize(bufferSize);

    if (!DeprecatedGlobalSettings::shouldManageAudioSessionCategory())
        return;

    auto category = AudioSession::CategoryType::None;
    if (AudioSession::sharedSession().categoryOverride() != AudioSession::CategoryType::None)
        category = AudioSession::sharedSession().categoryOverride();
    else if (captureCount || (isPlayingAudio && AudioSession::sharedSession().category() == AudioSession::CategoryType::PlayAndRecord))
        category = AudioSession::CategoryType::PlayAndRecord;
    else if (hasAudibleAudioOrVideoMediaType)
        category = AudioSession::CategoryType::MediaPlayback;
    else if (webAudioCount)
        category = AudioSession::CategoryType::AmbientSound;

    if (category == AudioSession::CategoryType::None && m_previousCategory != AudioSession::CategoryType::None) {
        if (!m_delayCategoryChangeTimer.isActive()) {
            m_delayCategoryChangeTimer.startOneShot(delayBeforeSettingCategoryNone);
            ALWAYS_LOG(LOGIDENTIFIER, "setting timer");
        }
        category = m_previousCategory;
    } else
        m_delayCategoryChangeTimer.stop();

    RouteSharingPolicy policy = (category == AudioSession::CategoryType::MediaPlayback) ? RouteSharingPolicy::LongFormAudio : RouteSharingPolicy::Default;

    ALWAYS_LOG(LOGIDENTIFIER, "setting category = ", category, ", policy = ", policy, ", previous category = ", m_previousCategory);

    m_previousCategory = category;
    AudioSession::sharedSession().setCategory(category, policy);
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
    callOnMainThread([this] () mutable {
        m_nowPlayingManager->setSupportsSeeking(computeSupportsSeeking());
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
    m_nowPlayingManager->addClient(*this);

    if (!m_audioHardwareListener) {
        m_audioHardwareListener = AudioHardwareListener::create(*this);
        m_supportedAudioHardwareBufferSizes = m_audioHardwareListener->supportedBufferSizes();
    }

    PlatformMediaSessionManager::addSession(session);
}

void MediaSessionManagerCocoa::removeSession(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::removeSession(session);

    if (hasNoSession()) {
        m_nowPlayingManager->removeClient(*this);
        m_audioHardwareListener = nullptr;
    }

    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::setCurrentSession(PlatformMediaSession& session)
{
    PlatformMediaSessionManager::setCurrentSession(session);

    m_nowPlayingManager->updateSupportedCommands();
}

void MediaSessionManagerCocoa::sessionWillEndPlayback(PlatformMediaSession& session, DelayCallingUpdateNowPlaying delayCallingUpdateNowPlaying)
{
    PlatformMediaSessionManager::sessionWillEndPlayback(session, delayCallingUpdateNowPlaying);

    callOnMainThread([weakSession = WeakPtr { session }] {
        if (weakSession)
            weakSession->updateMediaUsageIfChanged();
    });

    if (delayCallingUpdateNowPlaying == DelayCallingUpdateNowPlaying::No)
        updateNowPlayingInfo();
    else {
        callOnMainThread([this] {
            updateNowPlayingInfo();
        });
    }
}

void MediaSessionManagerCocoa::clientCharacteristicsChanged(PlatformMediaSession& session, bool)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::sessionCanProduceAudioChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    PlatformMediaSessionManager::sessionCanProduceAudioChanged();
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerCocoa::addSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    m_nowPlayingManager->addSupportedCommand(command);
}

void MediaSessionManagerCocoa::removeSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    m_nowPlayingManager->removeSupportedCommand(command);
}

RemoteCommandListener::RemoteCommandsSet MediaSessionManagerCocoa::supportedCommands() const
{
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
    if (!isMediaRemoteFrameworkAvailable())
        return;

    if (setAsNowPlayingApplication)
        MRMediaRemoteSetCanBeNowPlayingApplication(true);

    auto info = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtist, nowPlayingInfo.artist.createCFString().get());
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoAlbum, nowPlayingInfo.album.createCFString().get());
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoTitle, nowPlayingInfo.title.createCFString().get());

    if (std::isfinite(nowPlayingInfo.duration) && nowPlayingInfo.duration != MediaPlayer::invalidTime()) {
        auto cfDuration = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &nowPlayingInfo.duration));
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoDuration, cfDuration.get());
    }

    double rate = nowPlayingInfo.isPlaying ? nowPlayingInfo.rate : 0;
    auto cfRate = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &rate));
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoPlaybackRate, cfRate.get());

    // FIXME: This is a workaround Control Center not updating the artwork when refreshed.
    // We force the identifier to be reloaded to the new artwork if available.
    auto lastUpdatedNowPlayingInfoUniqueIdentifier = nowPlayingInfo.artwork ? nowPlayingInfo.artwork->src.hash() : nowPlayingInfo.uniqueIdentifier.toUInt64();
    auto cfIdentifier = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &lastUpdatedNowPlayingInfoUniqueIdentifier));
    CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoUniqueIdentifier, cfIdentifier.get());

    if (std::isfinite(nowPlayingInfo.currentTime) && nowPlayingInfo.currentTime != MediaPlayer::invalidTime() && nowPlayingInfo.supportsSeeking) {
        auto cfCurrentTime = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &nowPlayingInfo.currentTime));
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoElapsedTime, cfCurrentTime.get());
    }
    RetainPtr tiffImage = nowPlayingInfo.artwork && nowPlayingInfo.artwork->image ? nowPlayingInfo.artwork->image->tiffRepresentation() : nullptr;
    if (tiffImage) {
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkData, tiffImage.get());
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkMIMEType, @"image/tiff");
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkDataWidth, [NSNumber numberWithFloat:nowPlayingInfo.artwork->image->width()]);
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkDataHeight, [NSNumber numberWithFloat:nowPlayingInfo.artwork->image->height()]);
        CFDictionarySetValue(info.get(), kMRMediaRemoteNowPlayingInfoArtworkIdentifier, String::number(nowPlayingInfo.artwork->src.hash()).createCFString().get());
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
    MRMediaRemoteSetNowPlayingInfoWithMergePolicy(info.get(), MRMediaRemoteMergePolicyReplace);

    if (canLoad_MediaRemote_MRMediaRemoteSetNowPlayingVisibility()) {
        MRNowPlayingClientVisibility visibility = nowPlayingInfo.allowsNowPlayingControlsVisibility ? MRNowPlayingClientVisibilityAlwaysVisible : MRNowPlayingClientVisibilityNeverVisible;
        MRMediaRemoteSetNowPlayingVisibility(MRMediaRemoteGetLocalOrigin(), visibility);
    }
}

PlatformMediaSession* MediaSessionManagerCocoa::nowPlayingEligibleSession()
{
    // FIXME: Fix this layering violation.
    if (auto element = HTMLMediaElement::bestMediaElementForRemoteControls(MediaElementSession::PlaybackControlsPurpose::NowPlaying))
        return &element->mediaSession();

    return nullptr;
}

void MediaSessionManagerCocoa::updateNowPlayingInfo()
{
    if (!isMediaRemoteFrameworkAvailable())
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    std::optional<NowPlayingInfo> nowPlayingInfo;
    if (auto* session = nowPlayingEligibleSession())
        nowPlayingInfo = session->nowPlayingInfo();

    if (!nowPlayingInfo) {

        if (m_registeredAsNowPlayingApplication) {
            ALWAYS_LOG(LOGIDENTIFIER, "clearing now playing info");
            m_nowPlayingManager->clearNowPlayingInfo();
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

    if (m_nowPlayingManager->setNowPlayingInfo(*nowPlayingInfo)) {
#ifdef LOG_DISABLED
        String src = "src"_s;
        String title = "title"_s;
#else
        String src = nowPlayingInfo->artwork ? nowPlayingInfo->artwork->src : String();
        String title = nowPlayingInfo->title;
#endif
        ALWAYS_LOG(LOGIDENTIFIER, "title = \"", title, "\", isPlaying = ", nowPlayingInfo->isPlaying, ", duration = ", nowPlayingInfo->duration, ", now = ", nowPlayingInfo->currentTime, ", id = ", nowPlayingInfo->uniqueIdentifier.toUInt64(), ", registered = ", m_registeredAsNowPlayingApplication, ", src = \"", src, "\"");
    }
    if (!m_registeredAsNowPlayingApplication) {
        m_registeredAsNowPlayingApplication = true;
        providePresentingApplicationPIDIfNecessary();
    }

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
    ASSERT(m_audioHardwareListener);
    m_supportedAudioHardwareBufferSizes = m_audioHardwareListener->supportedBufferSizes();
    m_defaultBufferSize = AudioSession::sharedSession().preferredBufferSize();
    AudioSession::sharedSession().audioOutputDeviceChanged();
    updateSessionState();
}

} // namespace WebCore

#endif // USE(AUDIO_SESSION) && PLATFORM(COCOA)
