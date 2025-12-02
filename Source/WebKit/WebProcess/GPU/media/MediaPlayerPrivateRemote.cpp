/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaPlayerPrivateRemote.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "AudioTrackPrivateRemoteConfiguration.h"
#include "Logging.h"
#include "RemoteAudioSourceProvider.h"
#include "RemoteLegacyCDM.h"
#include "RemoteLegacyCDMFactory.h"
#include "RemoteLegacyCDMSession.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxyMessages.h"
#include "RemoteMediaResourceManagerMessages.h"
#include "SandboxExtension.h"
#include "TextTrackPrivateRemoteConfiguration.h"
#include "VideoLayerRemote.h"
#include "VideoTrackPrivateRemoteConfiguration.h"
#include "WebProcess.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaStrategy.h>
#include <WebCore/MessageClientForTesting.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/TextTrackRepresentation.h>
#include <WebCore/VideoLayerManager.h>
#include <wtf/HashMap.h>
#include <wtf/Locker.h>
#include <wtf/MachSendRight.h>
#include <wtf/MainThread.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>

#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMInstance.h"
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include <WebCore/LegacyCDM.h>
#endif

#if ENABLE(MEDIA_SOURCE)
#include "RemoteMediaSourceIdentifier.h"
#endif

#if PLATFORM(COCOA)
#include <WebCore/PixelBufferConformerCV.h>
#include <WebCore/VideoFrameCV.h>
#include <WebCore/VideoLayerManagerObjC.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetContextSerialized.h"
#endif

namespace WebCore {
#if !RELEASE_LOG_DISABLED
extern WTFLogChannel LogMedia;
#endif
}

namespace WebKit {
using namespace WebCore;

#ifdef ALWAYS_LOG_UNIMPLEMENTED_METHODS
#undef notImplemented
#define notImplemented() do { \
    static bool havePrinted = false; \
    if (!havePrinted) { \
        WTFLogAlways("@@@ UNIMPLEMENTED: %s", WTF_PRETTY_FUNCTION); \
        havePrinted = true; \
    } \
} while (0)
#endif

MediaPlayerPrivateRemote::TimeProgressEstimator::TimeProgressEstimator(const MediaPlayerPrivateRemote& parent)
    : m_parent(parent)
{
}

MediaTime MediaPlayerPrivateRemote::TimeProgressEstimator::currentTime() const
{
    Locker locker { m_lock };
    return currentTimeWithLockHeld();
}

MediaTime MediaPlayerPrivateRemote::TimeProgressEstimator::currentTimeWithLockHeld() const
{
    assertIsHeld(m_lock);
    if (!m_timeIsProgressing || m_forceUseCachedTime)
        return m_cachedMediaTime;

    auto calculatedCurrentTime = m_cachedMediaTime + MediaTime::createWithDouble(m_rate * (MonotonicTime::now() - m_cachedMediaTimeQueryTime).seconds());
    calculatedCurrentTime = std::min(std::max(calculatedCurrentTime, MediaTime::zeroTime()), protectedParent()->duration());
    if (m_rate >= 0)
        calculatedCurrentTime = std::max(m_lastReturnedTime.value_or(calculatedCurrentTime), calculatedCurrentTime);
    else
        calculatedCurrentTime = std::min(m_lastReturnedTime.value_or(calculatedCurrentTime), calculatedCurrentTime);
    m_lastReturnedTime = calculatedCurrentTime;
    return calculatedCurrentTime;
}

MediaTime MediaPlayerPrivateRemote::TimeProgressEstimator::cachedTime() const
{
    Locker locker { m_lock };
    return m_cachedMediaTime;
}

MediaTime MediaPlayerPrivateRemote::TimeProgressEstimator::cachedTimeWithLockHeld() const
{
    assertIsHeld(m_lock);
    return m_cachedMediaTime;
}

void MediaPlayerPrivateRemote::TimeProgressEstimator::forceUseOfCachedTimeUntilNextSetTime()
{
    Locker locker { m_lock };
    m_forceUseCachedTime = true;
}

bool MediaPlayerPrivateRemote::TimeProgressEstimator::timeIsProgressing() const
{
    return m_timeIsProgressing;
}

void MediaPlayerPrivateRemote::TimeProgressEstimator::pause()
{
    Locker locker { m_lock };
    if (!m_timeIsProgressing)
        return;
    auto now = MonotonicTime::now();
    m_cachedMediaTime += MediaTime::createWithDouble(m_rate * (now - m_cachedMediaTimeQueryTime).value());
    m_cachedMediaTimeQueryTime = now;
    m_timeIsProgressing = false;
}

void MediaPlayerPrivateRemote::TimeProgressEstimator::setTime(const MediaTimeUpdateData& timeData)
{
    Locker locker { m_lock };
    m_cachedMediaTime = timeData.currentTime;
    m_cachedMediaTimeQueryTime = timeData.wallTime;
    m_timeIsProgressing = timeData.timeIsProgressing;
    if (!m_timeIsProgressing)
        m_lastReturnedTime.reset();
    m_forceUseCachedTime = false;
}

void MediaPlayerPrivateRemote::TimeProgressEstimator::setRate(double value)
{
    Locker locker { m_lock };
    m_rate = value;
}

MediaPlayerPrivateRemote::MediaPlayerPrivateRemote(MediaPlayer& player, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, MediaPlayerIdentifier playerIdentifier, RemoteMediaPlayerManager& manager)
    : m_currentTimeEstimator(*this)
#if !RELEASE_LOG_DISABLED
    , m_logger(player.mediaPlayerLogger())
    , m_logIdentifier(player.mediaPlayerLogIdentifier())
#endif
    , m_player(player)
#if PLATFORM(COCOA)
    , m_videoLayerManager(makeUniqueRef<VideoLayerManagerObjC>(logger(), logIdentifier()))
#endif
    , m_manager(manager)
    , m_remoteEngineIdentifier(engineIdentifier)
    , m_id(playerIdentifier)
    , m_documentSecurityOrigin(player.documentSecurityOrigin())
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

MediaPlayerPrivateRemote::~MediaPlayerPrivateRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);
#if PLATFORM(COCOA)
    m_videoLayerManager->didDestroyVideoLayer();
#endif
    protectedManager()->deleteRemoteMediaPlayer(m_id);

#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
    if (RefPtr audioSourceProvider = m_audioSourceProvider)
        audioSourceProvider->close();
#endif

    for (auto& request : std::exchange(m_layerHostingContextRequests, { }))
        request({ });

    // Shutdown any stale MediaResources.
    // This condition can happen if the MediaPlayer gets reloaded half-way.
    ensureOnMainThread([resources = std::exchange(m_mediaResources, { })] {
        for (auto&& resource : resources)
            RefPtr { resource.value }->shutdown();
    });
}

void MediaPlayerPrivateRemote::prepareForPlayback(bool privateMode, MediaPlayer::Preload preload, bool preservesPitch, bool prepareToPlay, bool prepareToRender)
{
    auto player = m_player.get();
    if (!player)
        return;

    auto scale = player->playerContentsScale();
    auto preferredDynamicRangeMode = player->preferredDynamicRangeMode();
    auto platformDynamicRangeLimit = player->platformDynamicRangeLimit();
    auto presentationSize = player->presentationSize();
    auto pitchCorrectionAlgorithm = player->pitchCorrectionAlgorithm();
    auto isFullscreen = player->isInFullscreenOrPictureInPicture();

    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::PrepareForPlayback(privateMode, preload, preservesPitch, pitchCorrectionAlgorithm, prepareToPlay, prepareToRender, presentationSize, scale, isFullscreen, preferredDynamicRangeMode, platformDynamicRangeLimit), m_id);
}

void MediaPlayerPrivateRemote::load(const URL& url, const LoadOptions& options)
{
    std::optional<SandboxExtension::Handle> sandboxExtensionHandle;
    if (url.protocolIsFile()) {
        SandboxExtension::Handle handle;
        auto fileSystemPath = url.fileSystemPath();

        auto createExtension = [&] {
#if HAVE(AUDIT_TOKEN)
            if (auto auditToken = protectedManager()->protectedGPUProcessConnection()->auditToken()) {
                if (auto createdHandle = SandboxExtension::createHandleForReadByAuditToken(fileSystemPath, auditToken.value())) {
                    handle = WTFMove(*createdHandle);
                    return true;
                }
                return false;
            }
#endif
            if (auto createdHandle = SandboxExtension::createHandle(fileSystemPath, SandboxExtension::Type::ReadOnly)) {
                handle = WTFMove(*createdHandle);
                return true;
            }
            return false;
        };

        if (!createExtension()) {
            WTFLogAlways("Unable to create sandbox extension handle for GPUProcess url.\n");
            m_cachedState.networkState = MediaPlayer::NetworkState::FormatError;
            if (RefPtr player = m_player.get())
                player->networkStateChanged();
            return;
        }

        sandboxExtensionHandle = WTFMove(handle);
    }

    protectedConnection()->sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::Load(url, WTFMove(sandboxExtensionHandle), options), [weakThis = ThreadSafeWeakPtr { *this }](auto&& configuration) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto player = protectedThis->m_player.get();
        if (!player)
            return;

        protectedThis->updateConfiguration(WTFMove(configuration));
        player->mediaEngineUpdated();
    }, m_id);
}

void MediaPlayerPrivateRemote::cancelLoad()
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::CancelLoad(), m_id);
}

void MediaPlayerPrivateRemote::prepareToPlay()
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::PrepareToPlay(), m_id);
}

void MediaPlayerPrivateRemote::play()
{
    m_cachedState.paused = false;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::Play(), m_id);
}

void MediaPlayerPrivateRemote::pause()
{
    m_cachedState.paused = true;
    m_currentTimeEstimator.pause();
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::Pause(), m_id);
}

void MediaPlayerPrivateRemote::setPreservesPitch(bool preservesPitch)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetPreservesPitch(preservesPitch), m_id);
}

void MediaPlayerPrivateRemote::setPitchCorrectionAlgorithm(WebCore::MediaPlayer::PitchCorrectionAlgorithm algorithm)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetPitchCorrectionAlgorithm(algorithm), m_id);
}

void MediaPlayerPrivateRemote::setVolumeLocked(bool volumeLocked)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetVolumeLocked(volumeLocked), m_id);
}

void MediaPlayerPrivateRemote::setVolumeDouble(double volume)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetVolume(volume), m_id);
}

void MediaPlayerPrivateRemote::setMuted(bool muted)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetMuted(muted), m_id);
}

void MediaPlayerPrivateRemote::setPreload(MediaPlayer::Preload preload)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetPreload(preload), m_id);
}

void MediaPlayerPrivateRemote::setPrivateBrowsingMode(bool privateMode)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetPrivateBrowsingMode(privateMode), m_id);
}

MediaTime MediaPlayerPrivateRemote::duration() const
{
#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        return mediaSourcePrivate->duration();
#endif

    ASSERT(isMainRunLoop());
    return m_cachedState.duration;
}

MediaTime MediaPlayerPrivateRemote::currentTime() const
{
    Locker locker { m_currentTimeEstimator.lock() };
    return currentTimeWithLockHeld();
}

MediaTime MediaPlayerPrivateRemote::currentTimeWithLockHeld() const
{
#if ENABLE(MEDIA_SOURCE)
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate; mediaSourcePrivate && timeIsProgressing()) {
        if (!mediaSourcePrivate->hasBufferedData())
            return m_currentTimeEstimator.cachedTimeWithLockHeld();
        MediaTime currentTime = m_currentTimeEstimator.currentTimeWithLockHeld();
        if (currentTime >= duration())
            return duration();
        auto ranges = mediaSourcePrivate->buffered();
        // Handle the most common case.
        MediaTime startGap;
        if (ranges.start(ranges.length() - 1) <= currentTime) {
            if (currentTime <= ranges.maximumBufferedTime())
                return currentTime; // We are in the buffered range.
            startGap = ranges.maximumBufferedTime();
        } else {
            unsigned i = 0;
            for (; i < ranges.length(); i++) {
                if (ranges.start(i) <= currentTime && currentTime <= ranges.end(i))
                    return currentTime; // We are in the buffered range.
                // We allow currentTime to be in a buffered gap smaller than the fudge factor.
                if (i < ranges.length() - 1 && (ranges.start(i + 1) - ranges.end(i)) <= mediaSourcePrivate->timeFudgeFactor() && ranges.start(i + 1) >= currentTime)
                    return currentTime;
                if (ranges.start(i) > currentTime)
                    break;
            }
            startGap = ranges.end(i - 1);
        }
        // The GPU's time is the reference time, it can't go backward.
        return std::max(startGap, m_currentTimeEstimator.cachedTimeWithLockHeld());
    }
#endif
    return m_currentTimeEstimator.currentTimeWithLockHeld();
}

bool MediaPlayerPrivateRemote::timeIsProgressing() const
{
    return m_currentTimeEstimator.timeIsProgressing();
}

void MediaPlayerPrivateRemote::willSeekToTarget(const MediaTime& time)
{
    Locker locker { m_currentTimeEstimator.lock() };
    MediaPlayerPrivateInterface::willSeekToTarget(time);
}

MediaTime MediaPlayerPrivateRemote::pendingSeekTime() const
{
    ASSERT_NOT_REACHED();
    return MediaTime::invalidTime();
}

MediaTime MediaPlayerPrivateRemote::currentOrPendingSeekTime() const
{
    Locker locker { m_currentTimeEstimator.lock() };

    auto pendingSeekTime = MediaPlayerPrivateInterface::pendingSeekTime();
    if (pendingSeekTime.isValid())
        return pendingSeekTime;
    return currentTimeWithLockHeld();
}

void MediaPlayerPrivateRemote::seekToTarget(const WebCore::SeekTarget& target)
{
    ALWAYS_LOG(LOGIDENTIFIER, target);
    m_seeking = true;
    m_currentTimeEstimator.setTime({ target.time, false, MonotonicTime::now() });
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SeekToTarget(target), m_id);
}

bool MediaPlayerPrivateRemote::didLoadingProgress() const
{
    ASSERT_NOT_REACHED_WITH_MESSAGE("Should always be using didLoadingProgressAsync");
    return false;
}

void MediaPlayerPrivateRemote::didLoadingProgressAsync(MediaPlayer::DidLoadingProgressCompletionHandler&& callback) const
{
    protectedConnection()->sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::DidLoadingProgress(), WTFMove(callback), m_id);
}

bool MediaPlayerPrivateRemote::hasVideo() const
{
    return m_cachedState.hasVideo;
}

bool MediaPlayerPrivateRemote::hasAudio() const
{
    return m_cachedState.hasAudio;
}

const PlatformTimeRanges& MediaPlayerPrivateRemote::buffered() const
{
    return m_cachedBufferedTimeRanges;
}

MediaPlayer::MovieLoadType MediaPlayerPrivateRemote::movieLoadType() const
{
    return m_cachedState.movieLoadType;
}

void MediaPlayerPrivateRemote::networkStateChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    if (RefPtr player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateRemote::setReadyState(MediaPlayer::ReadyState readyState)
{
    // Can be called by the MediaSourcePrivateRemote on its WorkQueue.
    ALWAYS_LOG(LOGIDENTIFIER, readyState);
    ensureOnMainRunLoop([protectedThis = Ref { *this }, this, readyState] {
        if (std::exchange(m_readyState, readyState) == readyState)
            return;
        if (readyState > MediaPlayer::ReadyState::HaveCurrentData && m_readyState == MediaPlayer::ReadyState::HaveCurrentData)
            ALWAYS_LOG(LOGIDENTIFIER, "stall detected");
        if (RefPtr player = m_player.get())
            player->readyStateChanged();
    });
}

void MediaPlayerPrivateRemote::readyStateChanged(RemoteMediaPlayerState&& state, MediaPlayer::ReadyState readyState)
{
    assertIsMainRunLoop();

    ALWAYS_LOG(LOGIDENTIFIER, readyState);

    updateCachedState(WTFMove(state));
    setReadyState(readyState);

}

void MediaPlayerPrivateRemote::volumeChanged(double volume)
{
    m_volume = volume;
    if (RefPtr player = m_player.get())
        player->volumeChanged(volume);
}

void MediaPlayerPrivateRemote::muteChanged(bool muted)
{
    m_muted = muted;
    if (RefPtr player = m_player.get())
        player->muteChanged(muted);
}

void MediaPlayerPrivateRemote::seeked(MediaTimeUpdateData&& timeData)
{
    ALWAYS_LOG(LOGIDENTIFIER, "currentTime:", timeData.currentTime, " timeIsProgressing:", timeData.timeIsProgressing);
    m_seeking = false;
    m_currentTimeEstimator.setTime(timeData);
    if (RefPtr player = m_player.get())
        player->seeked(timeData.currentTime);
}

void MediaPlayerPrivateRemote::timeChanged(RemoteMediaPlayerState&& state, MediaTimeUpdateData&& timeData)
{
    ALWAYS_LOG(LOGIDENTIFIER, "currentTime:", timeData.currentTime, " timeIsProgressing:", timeData.timeIsProgressing);
    updateCachedState(WTFMove(state));
    m_currentTimeEstimator.setTime(timeData);
    if (RefPtr player = m_player.get())
        player->timeChanged();
}

void MediaPlayerPrivateRemote::durationChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    if (RefPtr player = m_player.get())
        player->durationChanged();
}

bool MediaPlayerPrivateRemote::seeking() const
{
    return m_seeking;
}

void MediaPlayerPrivateRemote::rateChanged(double rate, MediaTimeUpdateData&& timeData)
{
    INFO_LOG(LOGIDENTIFIER, "rate:", rate, " currentTime:", timeData.currentTime, " timeIsProgressing:", timeData.timeIsProgressing);
    m_rate = rate;
    m_currentTimeEstimator.setRate(rate);
    m_currentTimeEstimator.setTime(timeData);
    // Force to use the cached time so that the next call to currentTime() will return the cached time.
    // Time will progress following the next call to currentTimeChanged.
    m_currentTimeEstimator.forceUseOfCachedTimeUntilNextSetTime();
    if (RefPtr player = m_player.get())
        player->rateChanged();
}

void MediaPlayerPrivateRemote::playbackStateChanged(bool paused, MediaTimeUpdateData&& timeData)
{
    INFO_LOG(LOGIDENTIFIER, "currentTime:", timeData.currentTime, " timeIsProgressing:", timeData.timeIsProgressing);
    m_cachedState.paused = paused;
    m_currentTimeEstimator.setTime(timeData);
    if (RefPtr player = m_player.get())
        player->playbackStateChanged();
}

void MediaPlayerPrivateRemote::engineFailedToLoad(int64_t platformErrorCode)
{
    m_platformErrorCode = platformErrorCode;
    if (RefPtr player = m_player.get())
        player->remoteEngineFailedToLoad();
}

void MediaPlayerPrivateRemote::characteristicChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    if (RefPtr player = m_player.get())
        player->characteristicChanged();
}

void MediaPlayerPrivateRemote::sizeChanged(WebCore::FloatSize naturalSize)
{
    m_cachedState.naturalSize = naturalSize;
    if (RefPtr player = m_player.get())
        player->sizeChanged();
}

void MediaPlayerPrivateRemote::currentTimeChanged(MediaTimeUpdateData&& timeData)
{
    INFO_LOG(LOGIDENTIFIER, "currentTime:", timeData.currentTime, " timeIsProgressing:", timeData.timeIsProgressing, " seeking:", bool(m_seeking));
    if (m_seeking)
        return;
    auto oldCachedTime = m_currentTimeEstimator.cachedTime();
    auto oldTimeIsProgressing = m_currentTimeEstimator.timeIsProgressing();
    auto reverseJump = timeData.currentTime < oldCachedTime;
    if (reverseJump)
        ALWAYS_LOG(LOGIDENTIFIER, "time jumped backwards, was ", oldCachedTime, ", is now ", timeData.currentTime);

    m_currentTimeEstimator.setTime(timeData);

    if (reverseJump
        || (timeData.timeIsProgressing != oldTimeIsProgressing && timeData.currentTime != oldCachedTime && !m_cachedState.paused)) {
        if (RefPtr player = m_player.get())
            player->timeChanged();
    }
}

void MediaPlayerPrivateRemote::firstVideoFrameAvailable()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr player = m_player.get())
        player->firstVideoFrameAvailable();
}

void MediaPlayerPrivateRemote::renderingModeChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (RefPtr player = m_player.get())
        player->renderingModeChanged();
}

String MediaPlayerPrivateRemote::engineDescription() const
{
    return m_configuration.engineDescription;
}

bool MediaPlayerPrivateRemote::supportsScanning() const
{
    return m_configuration.supportsScanning;
}

bool MediaPlayerPrivateRemote::supportsFullscreen() const
{
    return m_configuration.supportsFullscreen;
}

bool MediaPlayerPrivateRemote::supportsPictureInPicture() const
{
    return m_configuration.supportsPictureInPicture;
}

bool MediaPlayerPrivateRemote::supportsAcceleratedRendering() const
{
    return m_configuration.supportsAcceleratedRendering;
}

void MediaPlayerPrivateRemote::acceleratedRenderingStateChanged()
{
    if (RefPtr player = m_player.get()) {
        protectedConnection()->send(Messages::RemoteMediaPlayerProxy::AcceleratedRenderingStateChanged(player->renderingCanBeAccelerated()), m_id);
    }
}

void MediaPlayerPrivateRemote::updateConfiguration(RemoteMediaPlayerConfiguration&& configuration)
{
    m_configuration = WTFMove(configuration);
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
bool MediaPlayerPrivateRemote::canPlayToWirelessPlaybackTarget() const
{
    return m_configuration.canPlayToWirelessPlaybackTarget;
}
#endif

void MediaPlayerPrivateRemote::updateCachedState(RemoteMediaPlayerState&& state)
{
    const Seconds playbackQualityMetricsTimeout = 30_s;

    m_cachedState.duration = state.duration;
    m_cachedState.minTimeSeekable = state.minTimeSeekable;
    m_cachedState.maxTimeSeekable = state.maxTimeSeekable;
    m_cachedState.networkState = state.networkState;
    m_cachedState.paused = state.paused;
    if (m_cachedState.naturalSize != state.naturalSize)
        sizeChanged(state.naturalSize);
    m_cachedState.movieLoadType = state.movieLoadType;
    m_cachedState.wirelessPlaybackTargetType = state.wirelessPlaybackTargetType;
    m_cachedState.wirelessPlaybackTargetName = state.wirelessPlaybackTargetName;

    m_cachedState.startDate = state.startDate;
    m_cachedState.startTime = state.startTime;
    m_cachedState.languageOfPrimaryAudioTrack = state.languageOfPrimaryAudioTrack;
    m_cachedState.maxFastForwardRate = state.maxFastForwardRate;
    m_cachedState.minFastReverseRate = state.minFastReverseRate;
    m_cachedState.seekableTimeRangesLastModifiedTime = state.seekableTimeRangesLastModifiedTime;
    m_cachedState.liveUpdateInterval = state.liveUpdateInterval;
    m_cachedState.canSaveMediaData = state.canSaveMediaData;
    m_cachedState.hasAudio = state.hasAudio;
    m_cachedState.hasVideo = state.hasVideo;

    if (state.videoMetrics)
        m_cachedState.videoMetrics = state.videoMetrics;
    if (m_videoPlaybackMetricsUpdateInterval && (MonotonicTime::now() - m_lastPlaybackQualityMetricsQueryTime) > playbackQualityMetricsTimeout)
        updateVideoPlaybackMetricsUpdateInterval(0_s);

    m_cachedState.hasClosedCaptions = state.hasClosedCaptions;
    m_cachedState.hasAvailableVideoFrame = state.hasAvailableVideoFrame;
    m_cachedState.wirelessVideoPlaybackDisabled = state.wirelessVideoPlaybackDisabled;
    m_cachedState.didPassCORSAccessCheck = state.didPassCORSAccessCheck;
    m_cachedState.documentIsCrossOrigin = state.documentIsCrossOrigin;

    if (state.bufferedRanges)
        m_cachedBufferedTimeRanges = *state.bufferedRanges;
}

void MediaPlayerPrivateRemote::updatePlaybackQualityMetrics(VideoPlaybackQualityMetrics&& metrics)
{
    m_cachedState.videoMetrics = WTFMove(metrics);
}

bool MediaPlayerPrivateRemote::shouldIgnoreIntrinsicSize()
{
    return m_configuration.shouldIgnoreIntrinsicSize;
}

void MediaPlayerPrivateRemote::prepareForRendering()
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::PrepareForRendering(), m_id);
}

void MediaPlayerPrivateRemote::setPageIsVisible(bool visible)
{
    if (m_pageIsVisible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);

    m_pageIsVisible = visible;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetPageIsVisible(visible), m_id);
}

void MediaPlayerPrivateRemote::setShouldMaintainAspectRatio(bool maintainRatio)
{
    if (maintainRatio == m_shouldMaintainAspectRatio)
        return;

    m_shouldMaintainAspectRatio = maintainRatio;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetShouldMaintainAspectRatio(maintainRatio), m_id);
}

void MediaPlayerPrivateRemote::setShouldDisableSleep(bool disable)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetShouldDisableSleep(disable), m_id);
}

FloatSize MediaPlayerPrivateRemote::naturalSize() const
{
    return m_cachedState.naturalSize;
}

void MediaPlayerPrivateRemote::addRemoteAudioTrack(AudioTrackPrivateRemoteConfiguration&& configuration)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    m_audioTracks.erase(configuration.trackId);

    auto addResult = m_audioTracks.emplace(configuration.trackId, AudioTrackPrivateRemote::create(protectedManager()->protectedGPUProcessConnection(), m_id, WTFMove(configuration)));
    ASSERT(addResult.second);

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return;
#endif

    if (RefPtr player = m_player.get())
        player->addAudioTrack(addResult.first->second);
}

void MediaPlayerPrivateRemote::removeRemoteAudioTrack(TrackID trackID)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_audioTracks.contains(trackID));

    if (auto it = m_audioTracks.find(trackID); it != m_audioTracks.end()) {
        if (RefPtr player = m_player.get())
            player->removeAudioTrack(it->second);
        m_audioTracks.erase(trackID);
    }
}

void MediaPlayerPrivateRemote::remoteAudioTrackConfigurationChanged(TrackID trackID, AudioTrackPrivateRemoteConfiguration&& configuration)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    if (auto it = m_audioTracks.find(trackID); it != m_audioTracks.end()) {
        Ref track = it->second;
        bool idChanged = track->id() != configuration.trackId;
        track->updateConfiguration(WTFMove(configuration));
        if (idChanged) {
            auto node = m_audioTracks.extract(it);
            node.key() = track->id();
            m_audioTracks.insert(WTFMove(node));
        }
    }
}

void MediaPlayerPrivateRemote::addRemoteTextTrack(TextTrackPrivateRemoteConfiguration&& configuration)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    m_textTracks.erase(configuration.trackId);

    auto addResult = m_textTracks.emplace(configuration.trackId, TextTrackPrivateRemote::create(protectedManager()->protectedGPUProcessConnection(), m_id, WTFMove(configuration)));
    ASSERT(addResult.second);

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return;
#endif

    if (RefPtr player = m_player.get())
        player->addTextTrack(addResult.first->second);
}

void MediaPlayerPrivateRemote::removeRemoteTextTrack(TrackID trackID)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end()) {
        if (RefPtr player = m_player.get())
            player->removeTextTrack(it->second);
        m_textTracks.erase(trackID);
    }
}

void MediaPlayerPrivateRemote::remoteTextTrackConfigurationChanged(TrackID trackID, TextTrackPrivateRemoteConfiguration&& configuration)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end()) {
        Ref track = it->second;
        bool idChanged = track->id() != configuration.trackId;
        track->updateConfiguration(WTFMove(configuration));
        if (idChanged) {
            auto node = m_textTracks.extract(it);
            node.key() = track->id();
            m_textTracks.insert(WTFMove(node));
        }
    }
}

void MediaPlayerPrivateRemote::parseWebVTTFileHeader(TrackID trackID, String&& header)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->parseWebVTTFileHeader(WTFMove(header));
}

void MediaPlayerPrivateRemote::parseWebVTTCueData(TrackID trackID, std::span<const uint8_t> data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->parseWebVTTCueData(WTFMove(data));
}

void MediaPlayerPrivateRemote::parseWebVTTCueDataStruct(TrackID trackID, ISOWebVTTCue&& data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->parseWebVTTCueDataStruct(WTFMove(data));
}

void MediaPlayerPrivateRemote::addDataCue(TrackID trackID, MediaTime&& start, MediaTime&& end, std::span<const uint8_t> data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->addDataCue(WTFMove(start), WTFMove(end), WTFMove(data));
}

#if ENABLE(DATACUE_VALUE)
void MediaPlayerPrivateRemote::addDataCueWithType(TrackID trackID, MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& data, String&& type)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->addDataCueWithType(WTFMove(start), WTFMove(end), WTFMove(data), WTFMove(type));
}

void MediaPlayerPrivateRemote::updateDataCue(TrackID trackID, MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->updateDataCue(WTFMove(start), WTFMove(end), WTFMove(data));
}

void MediaPlayerPrivateRemote::removeDataCue(TrackID trackID, MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->removeDataCue(WTFMove(start), WTFMove(end), WTFMove(data));
}
#endif

void MediaPlayerPrivateRemote::addGenericCue(TrackID trackID, GenericCueData&& cueData)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->addGenericCue(InbandGenericCue::create(WTFMove(cueData)));
}

void MediaPlayerPrivateRemote::updateGenericCue(TrackID trackID, GenericCueData&& cueData)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->updateGenericCue(InbandGenericCue::create(WTFMove(cueData)));
}

void MediaPlayerPrivateRemote::removeGenericCue(TrackID trackID, GenericCueData&& cueData)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        Ref { it->second }->removeGenericCue(InbandGenericCue::create(WTFMove(cueData)));
}

void MediaPlayerPrivateRemote::addRemoteVideoTrack(VideoTrackPrivateRemoteConfiguration&& configuration)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    m_videoTracks.erase(configuration.trackId);

    auto addResult = m_videoTracks.emplace(configuration.trackId, VideoTrackPrivateRemote::create(protectedManager()->protectedGPUProcessConnection(), m_id, WTFMove(configuration)));
    ASSERT(addResult.second);

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return;
#endif

    if (RefPtr player = m_player.get())
        player->addVideoTrack(addResult.first->second);
}

void MediaPlayerPrivateRemote::removeRemoteVideoTrack(TrackID trackID)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_videoTracks.contains(trackID));

    if (auto it = m_videoTracks.find(trackID); it != m_videoTracks.end()) {
        if (RefPtr player = m_player.get())
            player->removeVideoTrack(it->second);
        m_videoTracks.erase(trackID);
    }
}

void MediaPlayerPrivateRemote::remoteVideoTrackConfigurationChanged(TrackID trackID, VideoTrackPrivateRemoteConfiguration&& configuration)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_videoTracks.contains(trackID));

    if (auto it = m_videoTracks.find(trackID); it != m_videoTracks.end()) {
        Ref track = it->second;
        bool idChanged = track->id() != configuration.trackId;
        track->updateConfiguration(WTFMove(configuration));
        if (idChanged) {
            auto node = m_videoTracks.extract(it);
            node.key() = track->id();
            m_videoTracks.insert(WTFMove(node));
        }
    }
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateRemote::load(const URL& url, const LoadOptions& options, MediaSourcePrivateClient& client)
{
    if (m_remoteEngineIdentifier == MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE
        || (platformStrategies()->mediaStrategy()->mockMediaSourceEnabled() && m_remoteEngineIdentifier == MediaPlayerEnums::MediaEngineIdentifier::MockMSE)) {

        RefPtr mediaSourcePrivate = downcast<MediaSourcePrivateRemote>(client.mediaSourcePrivate());
        RemoteMediaSourceIdentifier identifier = [&] {
            if (mediaSourcePrivate) {
                mediaSourcePrivate->setPlayer(this);
                return mediaSourcePrivate->identifier();
            }
            return RemoteMediaSourceIdentifier::generate();
        }();
        protectedConnection()->sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::LoadMediaSource(url, options, identifier), [weakThis = ThreadSafeWeakPtr { *this }](RemoteMediaPlayerConfiguration&& configuration) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            auto player = protectedThis->m_player.get();
            if (!player)
                return;

            protectedThis->updateConfiguration(WTFMove(configuration));
            player->mediaEngineUpdated();
        }, m_id);
        if (mediaSourcePrivate) {
            m_mediaSourcePrivate = WTFMove(mediaSourcePrivate);
            // MediaSource can only be re-opened after RemoteMediaPlayerProxy::LoadMediaSource has been called.
            client.reOpen();
        } else
            m_mediaSourcePrivate = MediaSourcePrivateRemote::create(protectedManager()->protectedGPUProcessConnection(), identifier, protectedManager()->checkedTypeCache(m_remoteEngineIdentifier), *this, client);
        return;
    }

    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto player = protectedThis->m_player.get();
        if (!player)
            return;

        protectedThis->m_cachedState.networkState = MediaPlayer::NetworkState::FormatError;
        player->networkStateChanged();
    });
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateRemote::load(MediaStreamPrivate&)
{
    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto player = protectedThis->m_player.get();
        if (!player)
            return;

        protectedThis->m_cachedState.networkState = MediaPlayer::NetworkState::FormatError;
        player->networkStateChanged();
    });
}
#endif

PlatformLayer* MediaPlayerPrivateRemote::platformLayer() const
{
#if PLATFORM(COCOA)
    if (!m_videoLayer && m_layerHostingContext.contextID) {
        auto expandedVideoLayerSize = expandedIntSize(videoLayerSize());
        m_videoLayer = createVideoLayerRemote(const_cast<MediaPlayerPrivateRemote&>(*this), m_layerHostingContext.contextID, m_videoFullscreenGravity, expandedVideoLayerSize);
        m_videoLayerManager->setVideoLayer(m_videoLayer.get(), expandedVideoLayerSize);
    }
    return m_videoLayerManager->videoInlineLayer();
#else
    return nullptr;
#endif
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

void MediaPlayerPrivateRemote::setVideoFullscreenLayer(PlatformLayer* videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
#if PLATFORM(COCOA)
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), nullptr);
#endif
}

void MediaPlayerPrivateRemote::updateVideoFullscreenInlineImage()
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::UpdateVideoFullscreenInlineImage(), m_id);
}

void MediaPlayerPrivateRemote::setVideoFullscreenFrame(const WebCore::FloatRect& rect)
{
#if PLATFORM(COCOA)
    ALWAYS_LOG(LOGIDENTIFIER, "width = ", rect.size().width(), ", height = ", rect.size().height());
    m_videoLayerManager->setVideoFullscreenFrame(rect);
#endif
}

void MediaPlayerPrivateRemote::setVideoFullscreenGravity(WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    m_videoFullscreenGravity = gravity;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetVideoFullscreenGravity(gravity), m_id);
}

void MediaPlayerPrivateRemote::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode mode)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetVideoFullscreenMode(mode), m_id);
}

void MediaPlayerPrivateRemote::videoFullscreenStandbyChanged()
{
    auto player = m_player.get();
    if (!player)
        return;

    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::VideoFullscreenStandbyChanged(player->isVideoFullscreenStandby()), m_id);
}
#endif

#if PLATFORM(IOS_FAMILY)
NSArray* MediaPlayerPrivateRemote::timedMetadata() const
{
    notImplemented();
    return nullptr;
}

String MediaPlayerPrivateRemote::accessLog() const
{
    auto sendResult = protectedConnection()->sendSync(Messages::RemoteMediaPlayerProxy::AccessLog(), m_id);
    auto [log] = sendResult.takeReplyOr(emptyString());
    return log;
}

String MediaPlayerPrivateRemote::errorLog() const
{
    auto sendResult = protectedConnection()->sendSync(Messages::RemoteMediaPlayerProxy::ErrorLog(), m_id);
    auto [log] = sendResult.takeReplyOr(emptyString());
    return log;
}
#endif

void MediaPlayerPrivateRemote::setBufferingPolicy(MediaPlayer::BufferingPolicy policy)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetBufferingPolicy(policy), m_id);
}

bool MediaPlayerPrivateRemote::canSaveMediaData() const
{
    return m_cachedState.canSaveMediaData;
}

MediaTime MediaPlayerPrivateRemote::getStartDate() const
{
    return m_cachedState.startDate;
}

MediaTime MediaPlayerPrivateRemote::startTime() const
{
    return m_cachedState.startTime;
}

void MediaPlayerPrivateRemote::setRateDouble(double rate)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetRate(rate), m_id);
}

bool MediaPlayerPrivateRemote::hasClosedCaptions() const
{
    return m_cachedState.hasClosedCaptions;
}

double MediaPlayerPrivateRemote::maxFastForwardRate() const
{
    return m_cachedState.maxFastForwardRate;
}

double MediaPlayerPrivateRemote::minFastReverseRate() const
{
    return m_cachedState.minFastReverseRate;
}

MediaTime MediaPlayerPrivateRemote::maxTimeSeekable() const
{
    return m_cachedState.maxTimeSeekable;
}

MediaTime MediaPlayerPrivateRemote::minTimeSeekable() const
{
    return m_cachedState.minTimeSeekable;
}

double MediaPlayerPrivateRemote::seekableTimeRangesLastModifiedTime() const
{
    return m_cachedState.seekableTimeRangesLastModifiedTime;
}

double MediaPlayerPrivateRemote::liveUpdateInterval() const
{
    return m_cachedState.liveUpdateInterval;
}

unsigned long long MediaPlayerPrivateRemote::totalBytes() const
{
    notImplemented();
    return 0;
}

void MediaPlayerPrivateRemote::setPresentationSize(const IntSize& size)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetPresentationSize(size), m_id);
}

void MediaPlayerPrivateRemote::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateRemote::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled())
        return;

    RefPtr videoFrame = videoFrameForCurrentTime();
    if (!videoFrame)
        return;
    context.drawVideoFrame(*videoFrame, rect, ImageOrientation::Orientation::None, false);
}

RefPtr<WebCore::VideoFrame> MediaPlayerPrivateRemote::videoFrameForCurrentTime()
{
    if (readyState() < MediaPlayer::ReadyState::HaveCurrentData)
        return { };

#if PLATFORM(COCOA)
    if (m_videoFrameGatheredWithVideoFrameMetadata)
        return m_videoFrameGatheredWithVideoFrameMetadata;
#endif

    auto sendResult = protectedConnection()->sendSync(Messages::RemoteMediaPlayerProxy::VideoFrameForCurrentTimeIfChanged(), m_id);
    if (!sendResult.succeeded())
        return nullptr;

    auto [result, changed] = sendResult.takeReply();
    if (changed) {
        if (result)
            m_videoFrameForCurrentTime = RemoteVideoFrameProxy::create(protectedConnection(), protectedVideoFrameObjectHeapProxy(), WTFMove(*result));
        else
            m_videoFrameForCurrentTime = nullptr;
    }
    return m_videoFrameForCurrentTime;
}

#if !PLATFORM(COCOA)
RefPtr<NativeImage> MediaPlayerPrivateRemote::nativeImageForCurrentTime()
{
    notImplemented();
    return nullptr;
}

DestinationColorSpace MediaPlayerPrivateRemote::colorSpace()
{
    notImplemented();
    return DestinationColorSpace::SRGB();
}
#endif

bool MediaPlayerPrivateRemote::hasAvailableVideoFrame() const
{
    return m_cachedState.hasAvailableVideoFrame;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
String MediaPlayerPrivateRemote::wirelessPlaybackTargetName() const
{
    return m_cachedState.wirelessPlaybackTargetName;
}

MediaPlayer::WirelessPlaybackTargetType MediaPlayerPrivateRemote::wirelessPlaybackTargetType() const
{
    return m_cachedState.wirelessPlaybackTargetType;
}

bool MediaPlayerPrivateRemote::wirelessVideoPlaybackDisabled() const
{
    return m_cachedState.wirelessVideoPlaybackDisabled;
}

void MediaPlayerPrivateRemote::setWirelessVideoPlaybackDisabled(bool disabled)
{
    // Update the cache state so we don't have to make this a synchronous message send to avoid a
    // race condition with the web process fetching the new state immediately after change.
    m_cachedState.wirelessVideoPlaybackDisabled = disabled;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetWirelessVideoPlaybackDisabled(disabled), m_id);
}

void MediaPlayerPrivateRemote::currentPlaybackTargetIsWirelessChanged(bool isCurrentPlaybackTargetWireless)
{
    m_isCurrentPlaybackTargetWireless = isCurrentPlaybackTargetWireless;
    if (RefPtr player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless);
}

bool MediaPlayerPrivateRemote::isCurrentPlaybackTargetWireless() const
{
    return m_isCurrentPlaybackTargetWireless;
}

void MediaPlayerPrivateRemote::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetWirelessPlaybackTarget(MediaPlaybackTargetContextSerialized { target.get() }), m_id);
}

void MediaPlayerPrivateRemote::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetShouldPlayToPlaybackTarget(shouldPlay), m_id);
}
#endif

bool MediaPlayerPrivateRemote::didPassCORSAccessCheck() const
{
    return m_cachedState.didPassCORSAccessCheck;
}

std::optional<bool> MediaPlayerPrivateRemote::isCrossOrigin(const SecurityOrigin& origin) const
{
    if (origin.data() == m_documentSecurityOrigin)
        return m_cachedState.documentIsCrossOrigin;

    if (auto result = m_isCrossOriginCache.get(origin.data()))
        return result;

    auto sendResult = protectedConnection()->sendSync(Messages::RemoteMediaPlayerProxy::IsCrossOrigin(origin.data()), m_id);
    auto [crossOrigin] = sendResult.takeReplyOr(std::nullopt);
    if (crossOrigin)
        m_isCrossOriginCache.add(origin.data(), crossOrigin);
    return crossOrigin;
}

MediaTime MediaPlayerPrivateRemote::mediaTimeForTimeValue(const MediaTime& timeValue) const
{
    notImplemented();
    return timeValue;
}

unsigned MediaPlayerPrivateRemote::decodedFrameCount() const
{
    notImplemented();
    return 0;
}

unsigned MediaPlayerPrivateRemote::droppedFrameCount() const
{
    notImplemented();
    return 0;
}

unsigned MediaPlayerPrivateRemote::audioDecodedByteCount() const
{
    notImplemented();
    return 0;
}

unsigned MediaPlayerPrivateRemote::videoDecodedByteCount() const
{
    notImplemented();
    return 0;
}

#if ENABLE(WEB_AUDIO)
AudioSourceProvider* MediaPlayerPrivateRemote::audioSourceProvider()
{
#if PLATFORM(COCOA)
    if (!m_audioSourceProvider)
        m_audioSourceProvider = RemoteAudioSourceProvider::create(m_id, *this);

    return m_audioSourceProvider.get();
#else
    notImplemented();
    return nullptr;
#endif
}
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RefPtr<LegacyCDMSession> MediaPlayerPrivateRemote::createSession(const String&, LegacyCDMSessionClient&)
{
    notImplemented();
    return nullptr;
}

void MediaPlayerPrivateRemote::setCDM(LegacyCDM* cdm)
{
    if (!cdm)
        return;

    if (RefPtr remoteCDM = WebProcess::singleton().protectedLegacyCDMFactory()->findCDM(cdm->protectedCDMPrivate().get()))
        remoteCDM->setPlayerId(m_id);
}

void MediaPlayerPrivateRemote::setCDMSession(LegacyCDMSession* session)
{
    if (!session || session->type() != CDMSessionTypeRemote) {
        protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetLegacyCDMSession(std::nullopt), m_id);
        return;
    }

    auto* remoteSession = downcast<RemoteLegacyCDMSession>(session);
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetLegacyCDMSession(remoteSession->identifier()), m_id);
}

void MediaPlayerPrivateRemote::keyAdded()
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::KeyAdded(), m_id);
}

void MediaPlayerPrivateRemote::mediaPlayerKeyNeeded(std::span<const uint8_t> message)
{
    if (RefPtr player = m_player.get())
        player->keyNeeded(SharedBuffer::create(message));
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateRemote::cdmInstanceAttached(CDMInstance& instance)
{
    if (auto* remoteInstance = dynamicDowncast<RemoteCDMInstance>(instance))
        protectedConnection()->send(Messages::RemoteMediaPlayerProxy::CdmInstanceAttached(remoteInstance->identifier()), m_id);
}

void MediaPlayerPrivateRemote::cdmInstanceDetached(CDMInstance& instance)
{
    if (auto* remoteInstance = dynamicDowncast<RemoteCDMInstance>(instance))
        protectedConnection()->send(Messages::RemoteMediaPlayerProxy::CdmInstanceDetached(remoteInstance->identifier()), m_id);
}

void MediaPlayerPrivateRemote::attemptToDecryptWithInstance(CDMInstance& instance)
{
    if (auto* remoteInstance = dynamicDowncast<RemoteCDMInstance>(instance))
        protectedConnection()->send(Messages::RemoteMediaPlayerProxy::AttemptToDecryptWithInstance(remoteInstance->identifier()), m_id);
}

void MediaPlayerPrivateRemote::waitingForKeyChanged(bool waitingForKey)
{
    m_waitingForKey = waitingForKey;
    if (RefPtr player = m_player.get())
        player->waitingForKeyChanged();
}

void MediaPlayerPrivateRemote::initializationDataEncountered(const String& initDataType, std::span<const uint8_t> initData)
{
    auto initDataBuffer = ArrayBuffer::create(initData);
    if (RefPtr player = m_player.get())
        player->initializationDataEncountered(initDataType, WTFMove(initDataBuffer));
}

bool MediaPlayerPrivateRemote::waitingForKey() const
{
    return m_waitingForKey;
}
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateRemote::setShouldContinueAfterKeyNeeded(bool should)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetShouldContinueAfterKeyNeeded(should), m_id);
}
#endif

void MediaPlayerPrivateRemote::setTextTrackRepresentation(WebCore::TextTrackRepresentation* representation)
{
#if PLATFORM(COCOA)
    auto* representationLayer = representation ? representation->platformLayer() : nil;
    m_videoLayerManager->setTextTrackRepresentationLayer(representationLayer);
#endif
}

void MediaPlayerPrivateRemote::syncTextTrackBounds()
{
#if PLATFORM(COCOA)
    m_videoLayerManager->syncTextTrackBounds();
#endif
}

void MediaPlayerPrivateRemote::tracksChanged()
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::TracksChanged(), m_id);
}

String MediaPlayerPrivateRemote::languageOfPrimaryAudioTrack() const
{
    return m_cachedState.languageOfPrimaryAudioTrack;
}

size_t MediaPlayerPrivateRemote::extraMemoryCost() const
{
    notImplemented();
    return 0;
}

void MediaPlayerPrivateRemote::reportGPUMemoryFootprint(uint64_t footPrint)
{
    if (RefPtr player = m_player.get())
        player->reportGPUMemoryFootprint(footPrint);
}

void MediaPlayerPrivateRemote::updateVideoPlaybackMetricsUpdateInterval(const Seconds& interval)
{
    m_videoPlaybackMetricsUpdateInterval = interval;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetVideoPlaybackMetricsUpdateInterval(m_videoPlaybackMetricsUpdateInterval.value()), m_id);
}

std::optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateRemote::videoPlaybackQualityMetrics()
{
    const Seconds maximumPlaybackQualityMetricsSampleTimeDelta = 0.25_s;

    auto now = MonotonicTime::now();
    auto timeSinceLastQuery = now - m_lastPlaybackQualityMetricsQueryTime;
    if (!m_videoPlaybackMetricsUpdateInterval)
        updateVideoPlaybackMetricsUpdateInterval(1_s);
    else if (std::abs((timeSinceLastQuery - m_videoPlaybackMetricsUpdateInterval).value()) > maximumPlaybackQualityMetricsSampleTimeDelta.value())
        updateVideoPlaybackMetricsUpdateInterval(timeSinceLastQuery);

    m_lastPlaybackQualityMetricsQueryTime = now;

    return m_cachedState.videoMetrics;
}

void MediaPlayerPrivateRemote::notifyTrackModeChanged()
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::NotifyTrackModeChanged(), m_id);
}

void MediaPlayerPrivateRemote::notifyActiveSourceBuffersChanged()
{
    // FIXME: this just rounds trip up and down to activeSourceBuffersChanged(). Should this call ::activeSourceBuffersChanged directly?
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::NotifyActiveSourceBuffersChanged(), m_id);
}

bool MediaPlayerPrivateRemote::inVideoFullscreenOrPictureInPicture() const
{
#if PLATFORM(COCOA) && ENABLE(VIDEO_PRESENTATION_MODE)
    return !!m_videoLayerManager->videoFullscreenLayer();
#else
    return false;
#endif
}

void MediaPlayerPrivateRemote::applicationWillResignActive()
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::ApplicationWillResignActive(), m_id);
}

void MediaPlayerPrivateRemote::applicationDidBecomeActive()
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::ApplicationDidBecomeActive(), m_id);
}

void MediaPlayerPrivateRemote::setPreferredDynamicRangeMode(WebCore::DynamicRangeMode mode)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetPreferredDynamicRangeMode(mode), m_id);
}

void MediaPlayerPrivateRemote::setPlatformDynamicRangeLimit(WebCore::PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetPlatformDynamicRangeLimit(platformDynamicRangeLimit), m_id);
}

bool MediaPlayerPrivateRemote::performTaskAtTime(WTF::Function<void(const MediaTime&)>&& task, const MediaTime& mediaTime)
{
    auto asyncReplyHandler = [weakThis = ThreadSafeWeakPtr { *this }, task = WTFMove(task)](std::optional<MediaTime> currentTime) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !currentTime)
            return;

        task(*currentTime);
    };

    protectedConnection()->sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::PerformTaskAtTime(mediaTime), WTFMove(asyncReplyHandler), m_id);

    return true;
}

bool MediaPlayerPrivateRemote::playAtHostTime(const MonotonicTime& time)
{
    if (!m_configuration.supportsPlayAtHostTime)
        return false;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::PlayAtHostTime(time), m_id);
    return true;
}

bool MediaPlayerPrivateRemote::pauseAtHostTime(const MonotonicTime& time)
{
    if (!m_configuration.supportsPauseAtHostTime)
        return false;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::PauseAtHostTime(time), m_id);
    return true;
}

std::optional<VideoFrameMetadata> MediaPlayerPrivateRemote::videoFrameMetadata()
{
    auto videoFrameMetadata = std::exchange(m_videoFrameMetadata, { });
    return videoFrameMetadata;
}

void MediaPlayerPrivateRemote::startVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = true;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::StartVideoFrameMetadataGathering(), m_id);
}

void MediaPlayerPrivateRemote::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
#if PLATFORM(COCOA)
    m_videoFrameGatheredWithVideoFrameMetadata = nullptr;
#endif
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::StopVideoFrameMetadataGathering(), m_id);
}

void MediaPlayerPrivateRemote::playerContentBoxRectChanged(const LayoutRect& contentRect)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::PlayerContentBoxRectChanged(contentRect), m_id);
}

void MediaPlayerPrivateRemote::setShouldDisableHDR(bool shouldDisable)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetShouldDisableHDR(shouldDisable), m_id);
}

void MediaPlayerPrivateRemote::requestResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier, WebCore::ResourceRequest&& request, WebCore::PlatformMediaResourceLoader::LoadOptions options)
{
    assertIsMainRunLoop();

    ASSERT(!m_mediaResources.contains(remoteMediaResourceIdentifier));

    RefPtr player = m_player.get();
    RefPtr resource = player ? player->mediaResourceLoader()->requestResource(WTFMove(request), options) : nullptr;

    if (!resource) {
        // FIXME: Get the error from MediaResourceLoader::requestResource.
        protectedConnection()->send(Messages::RemoteMediaResourceManager::LoadFailed(remoteMediaResourceIdentifier, { ResourceError::Type::Cancellation }), 0);
        return;
    }
    // PlatformMediaResource owns the PlatformMediaResourceClient
    resource->setClient(adoptRef(*new RemoteMediaResourceProxy(connection(), *resource, remoteMediaResourceIdentifier)));
    m_mediaResources.add(remoteMediaResourceIdentifier, WTFMove(resource));
}

void MediaPlayerPrivateRemote::sendH2Ping(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    RefPtr player = m_player.get();
    if (!player) {
        completionHandler(makeUnexpected(internalError(url)));
        return;
    }
    player->mediaResourceLoader()->sendH2Ping(url, WTFMove(completionHandler));
}

void MediaPlayerPrivateRemote::removeResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier)
{
    assertIsMainRunLoop();

    // The client(RemoteMediaResourceProxy) will be destroyed as well
    if (auto resource = m_mediaResources.take(remoteMediaResourceIdentifier))
        resource->shutdown();
}

void MediaPlayerPrivateRemote::resourceNotSupported()
{
    if (RefPtr player = m_player.get())
        player->resourceNotSupported();
}

void MediaPlayerPrivateRemote::activeSourceBuffersChanged()
{
    if (RefPtr player = m_player.get())
        player->activeSourceBuffersChanged();
}

#if PLATFORM(IOS_FAMILY)
void MediaPlayerPrivateRemote::getRawCookies(const URL& url, WebCore::MediaPlayerClient::GetRawCookiesCallback&& completionHandler) const
{
    if (RefPtr player = m_player.get())
        player->getRawCookies(url, WTFMove(completionHandler));
}
#endif

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaPlayerPrivateRemote::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

void MediaPlayerPrivateRemote::requestHostingContext(LayerHostingContextCallback&& completionHandler)
{
    if (m_layerHostingContext.contextID) {
        completionHandler(m_layerHostingContext);
        return;
    }

    m_layerHostingContextRequests.append(WTFMove(completionHandler));
    protectedConnection()->sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::RequestHostingContext(), [weakThis = ThreadSafeWeakPtr { *this }] (WebCore::HostingContext context) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setLayerHostingContext(WTFMove(context));
    }, m_id);
}

WebCore::HostingContext MediaPlayerPrivateRemote::hostingContext() const
{
    return m_layerHostingContext;
}

void MediaPlayerPrivateRemote::setLayerHostingContext(WebCore::HostingContext&& hostingContext)
{
    if (m_layerHostingContext.contextID == hostingContext.contextID)
        return;

    m_layerHostingContext = WTFMove(hostingContext);
#if PLATFORM(COCOA)
    m_videoLayer = nullptr;
#endif

    for (auto& request : std::exchange(m_layerHostingContextRequests, { }))
        request(m_layerHostingContext);
}

#if ENABLE(MEDIA_SOURCE)
RefPtr<AudioTrackPrivateRemote> MediaPlayerPrivateRemote::audioTrackPrivateRemote(TrackID trackID) const
{
    Locker locker { m_lock };
    if (auto it = m_audioTracks.find(trackID); it != m_audioTracks.end())
        return it->second.ptr();
    return nullptr;
}

RefPtr<VideoTrackPrivateRemote> MediaPlayerPrivateRemote::videoTrackPrivateRemote(TrackID trackID) const
{
    Locker locker { m_lock };
    if (auto it = m_videoTracks.find(trackID); it != m_videoTracks.end())
        return it->second.ptr();
    return nullptr;
}

RefPtr<TextTrackPrivateRemote> MediaPlayerPrivateRemote::textTrackPrivateRemote(TrackID trackID) const
{
    Locker locker { m_lock };
    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        return it->second.ptr();
    return nullptr;
}
#endif

void MediaPlayerPrivateRemote::setShouldCheckHardwareSupport(bool value)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetShouldCheckHardwareSupport(value), m_id);
}


#if HAVE(SPATIAL_TRACKING_LABEL)
String MediaPlayerPrivateRemote::defaultSpatialTrackingLabel() const
{
    return m_defaultSpatialTrackingLabel;
}

void MediaPlayerPrivateRemote::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    if (defaultSpatialTrackingLabel == m_defaultSpatialTrackingLabel)
        return;

    m_defaultSpatialTrackingLabel = defaultSpatialTrackingLabel;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetDefaultSpatialTrackingLabel(m_defaultSpatialTrackingLabel), m_id);
}

String MediaPlayerPrivateRemote::spatialTrackingLabel() const
{
    return m_spatialTrackingLabel;
}

void MediaPlayerPrivateRemote::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (spatialTrackingLabel == m_spatialTrackingLabel)
        return;

    m_spatialTrackingLabel = spatialTrackingLabel;
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetSpatialTrackingLabel(m_spatialTrackingLabel), m_id);
}
#endif


#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
void MediaPlayerPrivateRemote::prefersSpatialAudioExperienceChanged()
{
    if (RefPtr player = m_player.get())
        protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetPrefersSpatialAudioExperience(player->prefersSpatialAudioExperience()), m_id);
}
#endif

void MediaPlayerPrivateRemote::soundStageSizeDidChange()
{
    if (RefPtr player = m_player.get())
        protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetSoundStageSize(player->soundStageSize()), m_id);
}

void MediaPlayerPrivateRemote::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::IsInFullscreenOrPictureInPictureChanged(isInFullscreenOrPictureInPicture), m_id);
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
bool MediaPlayerPrivateRemote::supportsLinearMediaPlayer() const
{
    using namespace WebCore;

    switch (m_remoteEngineIdentifier) {
    case MediaPlayerMediaEngineIdentifier::AVFoundation:
    case MediaPlayerMediaEngineIdentifier::AVFoundationMSE:
    case MediaPlayerMediaEngineIdentifier::CocoaWebM:
        return true;
    case MediaPlayerMediaEngineIdentifier::AVFoundationMediaStream:
        // FIXME: MediaStream doesn't support LinearMediaPlayer yet but should.
        return false;
    case MediaPlayerMediaEngineIdentifier::AVFoundationCF:
    case MediaPlayerMediaEngineIdentifier::GStreamer:
    case MediaPlayerMediaEngineIdentifier::GStreamerMSE:
    case MediaPlayerMediaEngineIdentifier::HolePunch:
    case MediaPlayerMediaEngineIdentifier::MediaFoundation:
    case MediaPlayerMediaEngineIdentifier::MockMSE:
    case MediaPlayerMediaEngineIdentifier::WirelessPlayback:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}
#endif

void MediaPlayerPrivateRemote::audioOutputDeviceChanged()
{
    if (RefPtr player = m_player.get())
        protectedConnection()->send(Messages::RemoteMediaPlayerProxy::AudioOutputDeviceChanged { player->audioOutputDeviceId() }, m_id);
}

void MediaPlayerPrivateRemote::commitAllTransactions(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

Ref<RemoteMediaPlayerManager> MediaPlayerPrivateRemote::protectedManager() const
{
    return m_manager.get().releaseNonNull();
}

#if PLATFORM(IOS_FAMILY)
void MediaPlayerPrivateRemote::sceneIdentifierDidChange()
{
    if (RefPtr player = m_player.get())
        protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetSceneIdentifier(player->sceneIdentifier()), m_id);
}
#endif

void MediaPlayerPrivateRemote::setMessageClientForTesting(WeakPtr<MessageClientForTesting> client)
{
    m_internalMessageClient = WTFMove(client);
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetHasMessageClientForTesting(!!m_internalMessageClient), m_id);
}

void MediaPlayerPrivateRemote::sendInternalMessage(const WebCore::MessageForTesting& message)
{
    if (RefPtr client = m_internalMessageClient.get()) {
        client->sendInternalMessage(message);
        return;
    }

    // We were sent a message, but no internal message client exists. Notify the
    // GPU process that we have no internal message client.
    protectedConnection()->send(Messages::RemoteMediaPlayerProxy::SetHasMessageClientForTesting(false), m_id);
}

void MediaPlayerPrivateRemote::gpuProcessConnectionDidClose()
{
    assertIsMainRunLoop();

    for (auto&& resource : std::exchange(m_mediaResources, { }))
        RefPtr { resource.value }->shutdown();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
