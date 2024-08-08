/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaStrategy.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformLayer.h>
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

#if USE(NICOSIA)
#include <WebCore/NicosiaPlatformLayer.h>
#elif USE(COORDINATED_GRAPHICS)
#include <WebCore/TextureMapperPlatformLayerProxyProvider.h>
#elif USE(TEXTURE_MAPPER)
#include <WebCore/TextureMapperPlatformLayer.h>
#endif

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
    if (!m_timeIsProgressing)
        return m_cachedMediaTime;

    auto calculatedCurrentTime = m_cachedMediaTime + MediaTime::createWithDouble(m_rate * (MonotonicTime::now() - m_cachedMediaTimeQueryTime).seconds());
    return std::min(std::max(calculatedCurrentTime, MediaTime::zeroTime()), m_parent.duration());
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
}

void MediaPlayerPrivateRemote::TimeProgressEstimator::setRate(double value)
{
    Locker locker { m_lock };
    m_rate = value;
}

MediaPlayerPrivateRemote::MediaPlayerPrivateRemote(MediaPlayer* player, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, MediaPlayerIdentifier playerIdentifier, RemoteMediaPlayerManager& manager)
    : m_currentTimeEstimator(*this)
#if !RELEASE_LOG_DISABLED
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
#endif
    , m_player(*player)
    , m_mediaResourceLoader(player->createResourceLoader())
#if PLATFORM(COCOA)
    , m_videoLayerManager(makeUniqueRef<VideoLayerManagerObjC>(logger(), logIdentifier()))
#endif
    , m_manager(manager)
    , m_remoteEngineIdentifier(engineIdentifier)
    , m_id(playerIdentifier)
    , m_documentSecurityOrigin(player->documentSecurityOrigin())
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

MediaPlayerPrivateRemote::~MediaPlayerPrivateRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);
#if PLATFORM(COCOA)
    m_videoLayerManager->didDestroyVideoLayer();
#endif
    m_manager.deleteRemoteMediaPlayer(m_id);

#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
    if (m_audioSourceProvider)
        m_audioSourceProvider->close();
#endif

    for (auto& request : std::exchange(m_layerHostingContextIDRequests, { }))
        request({ });

    // Shutdown any stale MediaResources.
    // This condition can happen if the MediaPlayer gets reloaded half-way.
    ensureOnMainThread([resources = WTFMove(m_mediaResources)] {
        for (auto&& resource : resources)
            resource.value->shutdown();
    });
}

void MediaPlayerPrivateRemote::prepareForPlayback(bool privateMode, MediaPlayer::Preload preload, bool preservesPitch, bool prepareToPlay, bool prepareToRender)
{
    auto player = m_player.get();
    if (!player)
        return;

    auto scale = player->playerContentsScale();
    auto preferredDynamicRangeMode = player->preferredDynamicRangeMode();
    auto presentationSize = player->presentationSize();
    auto pitchCorrectionAlgorithm = player->pitchCorrectionAlgorithm();

    connection().send(Messages::RemoteMediaPlayerProxy::PrepareForPlayback(privateMode, preload, preservesPitch, pitchCorrectionAlgorithm, prepareToPlay, prepareToRender, presentationSize, scale, preferredDynamicRangeMode), m_id);
}

void MediaPlayerPrivateRemote::load(const URL& url, const ContentType& contentType, const String& keySystem)
{
    std::optional<SandboxExtension::Handle> sandboxExtensionHandle;
    if (url.protocolIsFile()) {
        SandboxExtension::Handle handle;
        auto fileSystemPath = url.fileSystemPath();

        auto createExtension = [&] {
#if HAVE(AUDIT_TOKEN)
            if (auto auditToken = m_manager.gpuProcessConnection().auditToken()) {
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
            if (auto player = m_player.get())
                player->networkStateChanged();
            return;
        }

        sandboxExtensionHandle = WTFMove(handle);
    }

    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::Load(url, WTFMove(sandboxExtensionHandle), contentType, keySystem, m_player.get()->requiresRemotePlayback()), [weakThis = ThreadSafeWeakPtr { *this }, this](auto&& configuration) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto player = m_player.get();
        if (!player)
            return;

        updateConfiguration(WTFMove(configuration));
        player->mediaEngineUpdated();
    }, m_id);
}

void MediaPlayerPrivateRemote::cancelLoad()
{
    connection().send(Messages::RemoteMediaPlayerProxy::CancelLoad(), m_id);
}

void MediaPlayerPrivateRemote::prepareToPlay()
{
    connection().send(Messages::RemoteMediaPlayerProxy::PrepareToPlay(), m_id);
}

void MediaPlayerPrivateRemote::play()
{
    m_cachedState.paused = false;
    connection().send(Messages::RemoteMediaPlayerProxy::Play(), m_id);
}

void MediaPlayerPrivateRemote::pause()
{
    m_cachedState.paused = true;
    m_currentTimeEstimator.pause();
    connection().send(Messages::RemoteMediaPlayerProxy::Pause(), m_id);
}

void MediaPlayerPrivateRemote::setPreservesPitch(bool preservesPitch)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetPreservesPitch(preservesPitch), m_id);
}

void MediaPlayerPrivateRemote::setPitchCorrectionAlgorithm(WebCore::MediaPlayer::PitchCorrectionAlgorithm algorithm)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetPitchCorrectionAlgorithm(algorithm), m_id);
}

void MediaPlayerPrivateRemote::setVolumeDouble(double volume)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetVolume(volume), m_id);
}

void MediaPlayerPrivateRemote::setMuted(bool muted)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetMuted(muted), m_id);
}

void MediaPlayerPrivateRemote::setPreload(MediaPlayer::Preload preload)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetPreload(preload), m_id);
}

void MediaPlayerPrivateRemote::setPrivateBrowsingMode(bool privateMode)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetPrivateBrowsingMode(privateMode), m_id);
}

MediaTime MediaPlayerPrivateRemote::duration() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return m_mediaSourcePrivate->duration();
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
    if (m_mediaSourcePrivate && timeIsProgressing()) {
        if (!m_mediaSourcePrivate->hasBufferedData())
            return m_currentTimeEstimator.cachedTimeWithLockHeld();
        MediaTime currentTime = m_currentTimeEstimator.currentTimeWithLockHeld();
        if (currentTime >= duration())
            return duration();
        auto ranges = m_mediaSourcePrivate->buffered();
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
                if (i < ranges.length() - 1 && (ranges.start(i + 1) - ranges.end(i)) <= m_mediaSourcePrivate->timeFudgeFactor() && ranges.start(i + 1) >= currentTime)
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
    connection().send(Messages::RemoteMediaPlayerProxy::SeekToTarget(target), m_id);
}

bool MediaPlayerPrivateRemote::didLoadingProgress() const
{
    ASSERT_NOT_REACHED_WITH_MESSAGE("Should always be using didLoadingProgressAsync");
    return false;
}

void MediaPlayerPrivateRemote::didLoadingProgressAsync(MediaPlayer::DidLoadingProgressCompletionHandler&& callback) const
{
    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::DidLoadingProgress(), WTFMove(callback), m_id);
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
    if (auto player = m_player.get())
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
        if (auto player = m_player.get())
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
    if (auto player = m_player.get())
        player->volumeChanged(volume);
}

void MediaPlayerPrivateRemote::muteChanged(bool muted)
{
    m_muted = muted;
    if (auto player = m_player.get())
        player->muteChanged(muted);
}

void MediaPlayerPrivateRemote::seeked(MediaTimeUpdateData&& timeData)
{
    ALWAYS_LOG(LOGIDENTIFIER, timeData.currentTime);
    m_seeking = false;
    m_currentTimeEstimator.setTime(timeData);
    if (auto player = m_player.get())
        player->seeked(timeData.currentTime);
}

void MediaPlayerPrivateRemote::timeChanged(RemoteMediaPlayerState&& state, MediaTimeUpdateData&& timeData)
{
    ALWAYS_LOG(LOGIDENTIFIER, timeData.currentTime);
    updateCachedState(WTFMove(state));
    m_currentTimeEstimator.setTime(timeData);
    if (auto player = m_player.get())
        player->timeChanged();
}

void MediaPlayerPrivateRemote::durationChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    if (auto player = m_player.get())
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
    if (auto player = m_player.get())
        player->rateChanged();
}

void MediaPlayerPrivateRemote::playbackStateChanged(bool paused, MediaTimeUpdateData&& timeData)
{
    INFO_LOG(LOGIDENTIFIER, timeData.currentTime);
    m_cachedState.paused = paused;
    m_currentTimeEstimator.setTime(timeData);
    if (auto player = m_player.get())
        player->playbackStateChanged();
}

void MediaPlayerPrivateRemote::engineFailedToLoad(int64_t platformErrorCode)
{
    m_platformErrorCode = platformErrorCode;
    if (auto player = m_player.get())
        player->remoteEngineFailedToLoad();
}

void MediaPlayerPrivateRemote::characteristicChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    if (auto player = m_player.get())
        player->characteristicChanged();
}

void MediaPlayerPrivateRemote::sizeChanged(WebCore::FloatSize naturalSize)
{
    m_cachedState.naturalSize = naturalSize;
    if (auto player = m_player.get())
        player->sizeChanged();
}

void MediaPlayerPrivateRemote::currentTimeChanged(MediaTimeUpdateData&& timeData)
{
    INFO_LOG(LOGIDENTIFIER, timeData.currentTime, " seeking:", bool(m_seeking));
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
        if (auto player = m_player.get())
            player->timeChanged();
    }
}

void MediaPlayerPrivateRemote::firstVideoFrameAvailable()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto player = m_player.get())
        player->firstVideoFrameAvailable();
}

void MediaPlayerPrivateRemote::renderingModeChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto player = m_player.get())
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
    if (auto player = m_player.get()) {
        connection().send(Messages::RemoteMediaPlayerProxy::AcceleratedRenderingStateChanged(player->renderingCanBeAccelerated()), m_id);
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
    m_cachedState.videoConfiguration = state.videoConfiguration;

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
    connection().send(Messages::RemoteMediaPlayerProxy::PrepareForRendering(), m_id);
}

void MediaPlayerPrivateRemote::setPageIsVisible(bool visible)
{
    if (m_pageIsVisible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);

    m_pageIsVisible = visible;
    connection().send(Messages::RemoteMediaPlayerProxy::SetPageIsVisible(visible), m_id);
}

void MediaPlayerPrivateRemote::setShouldMaintainAspectRatio(bool maintainRatio)
{
    if (maintainRatio == m_shouldMaintainAspectRatio)
        return;

    m_shouldMaintainAspectRatio = maintainRatio;
    connection().send(Messages::RemoteMediaPlayerProxy::SetShouldMaintainAspectRatio(maintainRatio), m_id);
}

void MediaPlayerPrivateRemote::setShouldDisableSleep(bool disable)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetShouldDisableSleep(disable), m_id);
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

    auto addResult = m_audioTracks.emplace(configuration.trackId, AudioTrackPrivateRemote::create(m_manager.gpuProcessConnection(), m_id, WTFMove(configuration)));
    ASSERT(addResult.second);

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return;
#endif

    if (auto player = m_player.get())
        player->addAudioTrack(addResult.first->second);
}

void MediaPlayerPrivateRemote::removeRemoteAudioTrack(TrackID trackID)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_audioTracks.contains(trackID));

    if (auto it = m_audioTracks.find(trackID); it != m_audioTracks.end()) {
        if (auto player = m_player.get())
            player->removeAudioTrack(it->second);
        m_audioTracks.erase(trackID);
    }
}

void MediaPlayerPrivateRemote::remoteAudioTrackConfigurationChanged(TrackID trackID, AudioTrackPrivateRemoteConfiguration&& configuration)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    if (auto it = m_audioTracks.find(trackID); it != m_audioTracks.end()) {
        AudioTrackPrivateRemote& track = it->second;
        bool idChanged = track.id() != configuration.trackId;
        track.updateConfiguration(WTFMove(configuration));
        if (idChanged) {
            auto node = m_audioTracks.extract(it);
            node.key() = track.id();
            m_audioTracks.insert(WTFMove(node));
        }
    }
}

void MediaPlayerPrivateRemote::addRemoteTextTrack(TextTrackPrivateRemoteConfiguration&& configuration)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    m_textTracks.erase(configuration.trackId);

    auto addResult = m_textTracks.emplace(configuration.trackId, TextTrackPrivateRemote::create(m_manager.gpuProcessConnection(), m_id, WTFMove(configuration)));
    ASSERT(addResult.second);

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return;
#endif

    if (auto player = m_player.get())
        player->addTextTrack(addResult.first->second);
}

void MediaPlayerPrivateRemote::removeRemoteTextTrack(TrackID trackID)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end()) {
        if (auto player = m_player.get())
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
        TextTrackPrivateRemote& track = it->second;
        bool idChanged = track.id() != configuration.trackId;
        track.updateConfiguration(WTFMove(configuration));
        if (idChanged) {
            auto node = m_textTracks.extract(it);
            node.key() = track.id();
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
        it->second->parseWebVTTFileHeader(WTFMove(header));
}

void MediaPlayerPrivateRemote::parseWebVTTCueData(TrackID trackID, std::span<const uint8_t> data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        it->second->parseWebVTTCueData(WTFMove(data));
}

void MediaPlayerPrivateRemote::parseWebVTTCueDataStruct(TrackID trackID, ISOWebVTTCue&& data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        it->second->parseWebVTTCueDataStruct(WTFMove(data));
}

void MediaPlayerPrivateRemote::addDataCue(TrackID trackID, MediaTime&& start, MediaTime&& end, std::span<const uint8_t> data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        it->second->addDataCue(WTFMove(start), WTFMove(end), WTFMove(data));
}

#if ENABLE(DATACUE_VALUE)
void MediaPlayerPrivateRemote::addDataCueWithType(TrackID trackID, MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& data, String&& type)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        it->second->addDataCueWithType(WTFMove(start), WTFMove(end), WTFMove(data), WTFMove(type));
}

void MediaPlayerPrivateRemote::updateDataCue(TrackID trackID, MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        it->second->updateDataCue(WTFMove(start), WTFMove(end), WTFMove(data));
}

void MediaPlayerPrivateRemote::removeDataCue(TrackID trackID, MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& data)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        it->second->removeDataCue(WTFMove(start), WTFMove(end), WTFMove(data));
}
#endif

void MediaPlayerPrivateRemote::addGenericCue(TrackID trackID, GenericCueData&& cueData)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        it->second->addGenericCue(InbandGenericCue::create(WTFMove(cueData)));
}

void MediaPlayerPrivateRemote::updateGenericCue(TrackID trackID, GenericCueData&& cueData)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        it->second->updateGenericCue(InbandGenericCue::create(WTFMove(cueData)));
}

void MediaPlayerPrivateRemote::removeGenericCue(TrackID trackID, GenericCueData&& cueData)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_textTracks.contains(trackID));

    if (auto it = m_textTracks.find(trackID); it != m_textTracks.end())
        it->second->removeGenericCue(InbandGenericCue::create(WTFMove(cueData)));
}

void MediaPlayerPrivateRemote::addRemoteVideoTrack(VideoTrackPrivateRemoteConfiguration&& configuration)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    m_videoTracks.erase(configuration.trackId);

    auto addResult = m_videoTracks.emplace(configuration.trackId, VideoTrackPrivateRemote::create(m_manager.gpuProcessConnection(), m_id, WTFMove(configuration)));
    ASSERT(addResult.second);

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return;
#endif

    if (auto player = m_player.get())
        player->addVideoTrack(addResult.first->second);
}

void MediaPlayerPrivateRemote::removeRemoteVideoTrack(TrackID trackID)
{
    assertIsMainRunLoop();
    Locker locker { m_lock };

    ASSERT(m_videoTracks.contains(trackID));

    if (auto it = m_videoTracks.find(trackID); it != m_videoTracks.end()) {
        if (auto player = m_player.get())
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
        VideoTrackPrivateRemote& track = it->second;
        bool idChanged = track.id() != configuration.trackId;
        track.updateConfiguration(WTFMove(configuration));
        if (idChanged) {
            auto node = m_videoTracks.extract(it);
            node.key() = track.id();
            m_videoTracks.insert(WTFMove(node));
        }
    }
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateRemote::load(const URL& url, const ContentType& contentType, MediaSourcePrivateClient& client)
{
    if (m_remoteEngineIdentifier == MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE
        || (platformStrategies()->mediaStrategy().mockMediaSourceEnabled() && m_remoteEngineIdentifier == MediaPlayerEnums::MediaEngineIdentifier::MockMSE)) {
        auto identifier = RemoteMediaSourceIdentifier::generate();
        connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::LoadMediaSource(url, contentType, DeprecatedGlobalSettings::webMParserEnabled(), identifier), [weakThis = ThreadSafeWeakPtr { *this }, this](RemoteMediaPlayerConfiguration&& configuration) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            auto player = m_player.get();
            if (!player)
                return;

            updateConfiguration(WTFMove(configuration));
            player->mediaEngineUpdated();
        }, m_id);
        m_mediaSourcePrivate = MediaSourcePrivateRemote::create(m_manager.gpuProcessConnection(), identifier, m_manager.typeCache(m_remoteEngineIdentifier), *this, client);

        return;
    }

    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }, this] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto player = m_player.get();
        if (!player)
            return;

        m_cachedState.networkState = MediaPlayer::NetworkState::FormatError;
        player->networkStateChanged();
    });
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateRemote::load(MediaStreamPrivate&)
{
    callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }, this] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto player = m_player.get();
        if (!player)
            return;

        m_cachedState.networkState = MediaPlayer::NetworkState::FormatError;
        player->networkStateChanged();
    });
}
#endif

PlatformLayer* MediaPlayerPrivateRemote::platformLayer() const
{
#if PLATFORM(COCOA)
    if (!m_videoLayer && m_layerHostingContextID) {
        auto expandedVideoLayerSize = expandedIntSize(videoLayerSize());
        m_videoLayer = createVideoLayerRemote(const_cast<MediaPlayerPrivateRemote*>(this), m_layerHostingContextID, m_videoFullscreenGravity, expandedVideoLayerSize);
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
    connection().send(Messages::RemoteMediaPlayerProxy::UpdateVideoFullscreenInlineImage(), m_id);
}

void MediaPlayerPrivateRemote::setVideoFullscreenFrame(WebCore::FloatRect rect)
{
#if PLATFORM(COCOA)
    ALWAYS_LOG(LOGIDENTIFIER, "width = ", rect.size().width(), ", height = ", rect.size().height());
    m_videoLayerManager->setVideoFullscreenFrame(rect);
#endif
}

void MediaPlayerPrivateRemote::setVideoFullscreenGravity(WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    m_videoFullscreenGravity = gravity;
    connection().send(Messages::RemoteMediaPlayerProxy::SetVideoFullscreenGravity(gravity), m_id);
}

void MediaPlayerPrivateRemote::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode mode)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetVideoFullscreenMode(mode), m_id);
}

void MediaPlayerPrivateRemote::videoFullscreenStandbyChanged()
{
    auto player = m_player.get();
    if (!player)
        return;

    connection().send(Messages::RemoteMediaPlayerProxy::VideoFullscreenStandbyChanged(player->isVideoFullscreenStandby()), m_id);
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
    auto sendResult = connection().sendSync(Messages::RemoteMediaPlayerProxy::AccessLog(), m_id);
    auto [log] = sendResult.takeReplyOr(emptyString());
    return log;
}

String MediaPlayerPrivateRemote::errorLog() const
{
    auto sendResult = connection().sendSync(Messages::RemoteMediaPlayerProxy::ErrorLog(), m_id);
    auto [log] = sendResult.takeReplyOr(emptyString());
    return log;
}
#endif

void MediaPlayerPrivateRemote::setBufferingPolicy(MediaPlayer::BufferingPolicy policy)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetBufferingPolicy(policy), m_id);
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
    connection().send(Messages::RemoteMediaPlayerProxy::SetRate(rate), m_id);
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
    connection().send(Messages::RemoteMediaPlayerProxy::SetPresentationSize(size), m_id);
}

void MediaPlayerPrivateRemote::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateRemote::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled())
        return;

    auto nativeImage = nativeImageForCurrentTime();
    if (!nativeImage)
        return;
    context.drawNativeImage(*nativeImage, rect, FloatRect { { }, nativeImage->size() });
}

#if !USE(AVFOUNDATION)
bool MediaPlayerPrivateRemote::copyVideoTextureToPlatformTexture(WebCore::GraphicsContextGL*, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool)
{
    notImplemented();
    return false;
}
#endif

#if PLATFORM(COCOA) && !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
void MediaPlayerPrivateRemote::willBeAskedToPaintGL()
{
    if (m_hasBeenAskedToPaintGL)
        return;

    m_hasBeenAskedToPaintGL = true;
    connection().send(Messages::RemoteMediaPlayerProxy::WillBeAskedToPaintGL(), m_id);
}
#endif

RefPtr<WebCore::VideoFrame> MediaPlayerPrivateRemote::videoFrameForCurrentTime()
{
    if (readyState() < MediaPlayer::ReadyState::HaveCurrentData)
        return { };

    auto sendResult = connection().sendSync(Messages::RemoteMediaPlayerProxy::VideoFrameForCurrentTimeIfChanged(), m_id);
    if (!sendResult.succeeded())
        return nullptr;

    auto [result, changed] = sendResult.takeReply();
    if (changed) {
        if (result)
            m_videoFrameForCurrentTime = RemoteVideoFrameProxy::create(protectedConnection(), videoFrameObjectHeapProxy(), WTFMove(*result));
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
    connection().send(Messages::RemoteMediaPlayerProxy::SetWirelessVideoPlaybackDisabled(disabled), m_id);
}

void MediaPlayerPrivateRemote::currentPlaybackTargetIsWirelessChanged(bool isCurrentPlaybackTargetWireless)
{
    m_isCurrentPlaybackTargetWireless = isCurrentPlaybackTargetWireless;
    if (auto player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless);
}

bool MediaPlayerPrivateRemote::isCurrentPlaybackTargetWireless() const
{
    return m_isCurrentPlaybackTargetWireless;
}

void MediaPlayerPrivateRemote::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetWirelessPlaybackTarget(MediaPlaybackTargetContextSerialized { target->targetContext() }), m_id);
}

void MediaPlayerPrivateRemote::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetShouldPlayToPlaybackTarget(shouldPlay), m_id);
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

    auto sendResult = connection().sendSync(Messages::RemoteMediaPlayerProxy::IsCrossOrigin(origin.data()), m_id);
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

double MediaPlayerPrivateRemote::maximumDurationToCacheMediaTime() const
{
    return m_configuration.maximumDurationToCacheMediaTime;
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
std::unique_ptr<LegacyCDMSession> MediaPlayerPrivateRemote::createSession(const String&, LegacyCDMSessionClient&)
{
    notImplemented();
    return nullptr;
}

void MediaPlayerPrivateRemote::setCDM(LegacyCDM* cdm)
{
    if (!cdm)
        return;

    auto remoteCDM = WebProcess::singleton().legacyCDMFactory().findCDM(cdm->cdmPrivate());
    if (!remoteCDM)
        return;

    remoteCDM->setPlayerId(m_id);
}

void MediaPlayerPrivateRemote::setCDMSession(LegacyCDMSession* session)
{
    if (!session || session->type() != CDMSessionTypeRemote) {
        connection().send(Messages::RemoteMediaPlayerProxy::SetLegacyCDMSession(std::nullopt), m_id);
        return;
    }

    auto remoteSession = static_cast<RemoteLegacyCDMSession*>(session);
    connection().send(Messages::RemoteMediaPlayerProxy::SetLegacyCDMSession(remoteSession->identifier()), m_id);
}

void MediaPlayerPrivateRemote::keyAdded()
{
    connection().send(Messages::RemoteMediaPlayerProxy::KeyAdded(), m_id);
}

void MediaPlayerPrivateRemote::mediaPlayerKeyNeeded(std::span<const uint8_t> message)
{
    if (auto player = m_player.get())
        player->keyNeeded(SharedBuffer::create(message));
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateRemote::cdmInstanceAttached(CDMInstance& instance)
{
    if (auto* remoteInstance = dynamicDowncast<RemoteCDMInstance>(instance))
        connection().send(Messages::RemoteMediaPlayerProxy::CdmInstanceAttached(remoteInstance->identifier()), m_id);
}

void MediaPlayerPrivateRemote::cdmInstanceDetached(CDMInstance& instance)
{
    if (auto* remoteInstance = dynamicDowncast<RemoteCDMInstance>(instance))
        connection().send(Messages::RemoteMediaPlayerProxy::CdmInstanceDetached(remoteInstance->identifier()), m_id);
}

void MediaPlayerPrivateRemote::attemptToDecryptWithInstance(CDMInstance& instance)
{
    if (auto* remoteInstance = dynamicDowncast<RemoteCDMInstance>(instance))
        connection().send(Messages::RemoteMediaPlayerProxy::AttemptToDecryptWithInstance(remoteInstance->identifier()), m_id);
}

void MediaPlayerPrivateRemote::waitingForKeyChanged(bool waitingForKey)
{
    m_waitingForKey = waitingForKey;
    if (auto player = m_player.get())
        player->waitingForKeyChanged();
}

void MediaPlayerPrivateRemote::initializationDataEncountered(const String& initDataType, std::span<const uint8_t> initData)
{
    auto initDataBuffer = ArrayBuffer::create(initData);
    if (auto player = m_player.get())
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
    connection().send(Messages::RemoteMediaPlayerProxy::SetShouldContinueAfterKeyNeeded(should), m_id);
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
    connection().send(Messages::RemoteMediaPlayerProxy::TracksChanged(), m_id);
}

void MediaPlayerPrivateRemote::beginSimulatedHDCPError()
{
    connection().send(Messages::RemoteMediaPlayerProxy::BeginSimulatedHDCPError(), m_id);
}

void MediaPlayerPrivateRemote::endSimulatedHDCPError()
{
    connection().send(Messages::RemoteMediaPlayerProxy::EndSimulatedHDCPError(), m_id);
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

void MediaPlayerPrivateRemote::updateVideoPlaybackMetricsUpdateInterval(const Seconds& interval)
{
    m_videoPlaybackMetricsUpdateInterval = interval;
    connection().send(Messages::RemoteMediaPlayerProxy::SetVideoPlaybackMetricsUpdateInterval(m_videoPlaybackMetricsUpdateInterval.value()), m_id);
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
    connection().send(Messages::RemoteMediaPlayerProxy::NotifyTrackModeChanged(), m_id);
}

void MediaPlayerPrivateRemote::notifyActiveSourceBuffersChanged()
{
    // FIXME: this just rounds trip up and down to activeSourceBuffersChanged(). Should this call ::activeSourceBuffersChanged directly?
    connection().send(Messages::RemoteMediaPlayerProxy::NotifyActiveSourceBuffersChanged(), m_id);
}

#if PLATFORM(COCOA)
bool MediaPlayerPrivateRemote::inVideoFullscreenOrPictureInPicture() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    return !!m_videoLayerManager->videoFullscreenLayer();
#else
    return false;
#endif
}
#endif

void MediaPlayerPrivateRemote::applicationWillResignActive()
{
    connection().send(Messages::RemoteMediaPlayerProxy::ApplicationWillResignActive(), m_id);
}

void MediaPlayerPrivateRemote::applicationDidBecomeActive()
{
    connection().send(Messages::RemoteMediaPlayerProxy::ApplicationDidBecomeActive(), m_id);
}

void MediaPlayerPrivateRemote::setPreferredDynamicRangeMode(WebCore::DynamicRangeMode mode)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetPreferredDynamicRangeMode(mode), m_id);
}

bool MediaPlayerPrivateRemote::performTaskAtTime(WTF::Function<void()>&& task, const MediaTime& mediaTime)
{
    auto asyncReplyHandler = [weakThis = ThreadSafeWeakPtr { *this }, task = WTFMove(task)](std::optional<MediaTime> currentTime) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !currentTime)
            return;

        task();
    };

    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::PerformTaskAtTime(mediaTime), WTFMove(asyncReplyHandler), m_id);

    return true;
}

bool MediaPlayerPrivateRemote::playAtHostTime(const MonotonicTime& time)
{
    if (!m_configuration.supportsPlayAtHostTime)
        return false;
    connection().send(Messages::RemoteMediaPlayerProxy::PlayAtHostTime(time), m_id);
    return true;
}

bool MediaPlayerPrivateRemote::pauseAtHostTime(const MonotonicTime& time)
{
    if (!m_configuration.supportsPauseAtHostTime)
        return false;
    connection().send(Messages::RemoteMediaPlayerProxy::PauseAtHostTime(time), m_id);
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
    connection().send(Messages::RemoteMediaPlayerProxy::StartVideoFrameMetadataGathering(), m_id);
}

void MediaPlayerPrivateRemote::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
#if PLATFORM(COCOA)
    m_videoFrameGatheredWithVideoFrameMetadata = nullptr;
#endif
    connection().send(Messages::RemoteMediaPlayerProxy::StopVideoFrameMetadataGathering(), m_id);
}

void MediaPlayerPrivateRemote::playerContentBoxRectChanged(const LayoutRect& contentRect)
{
    connection().send(Messages::RemoteMediaPlayerProxy::PlayerContentBoxRectChanged(contentRect), m_id);
}

void MediaPlayerPrivateRemote::setShouldDisableHDR(bool shouldDisable)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetShouldDisableHDR(shouldDisable), m_id);
}

void MediaPlayerPrivateRemote::requestResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier, WebCore::ResourceRequest&& request, WebCore::PlatformMediaResourceLoader::LoadOptions options)
{
    assertIsMainRunLoop();

    ASSERT(!m_mediaResources.contains(remoteMediaResourceIdentifier));
    auto resource = m_mediaResourceLoader->requestResource(WTFMove(request), options);

    if (!resource) {
        // FIXME: Get the error from MediaResourceLoader::requestResource.
        connection().send(Messages::RemoteMediaResourceManager::LoadFailed(remoteMediaResourceIdentifier, { ResourceError::Type::Cancellation }), 0);
        return;
    }
    // PlatformMediaResource owns the PlatformMediaResourceClient
    resource->setClient(adoptRef(*new RemoteMediaResourceProxy(connection(), *resource, remoteMediaResourceIdentifier)));
    m_mediaResources.add(remoteMediaResourceIdentifier, WTFMove(resource));
}

void MediaPlayerPrivateRemote::sendH2Ping(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    m_mediaResourceLoader->sendH2Ping(url, WTFMove(completionHandler));
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
    if (auto player = m_player.get())
        player->resourceNotSupported();
}

void MediaPlayerPrivateRemote::activeSourceBuffersChanged()
{
    if (auto player = m_player.get())
        player->activeSourceBuffersChanged();
}

#if PLATFORM(IOS_FAMILY)
void MediaPlayerPrivateRemote::getRawCookies(const URL& url, WebCore::MediaPlayerClient::GetRawCookiesCallback&& completionHandler) const
{
    if (auto player = m_player.get())
        player->getRawCookies(url, WTFMove(completionHandler));
}
#endif

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaPlayerPrivateRemote::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

void MediaPlayerPrivateRemote::requestHostingContextID(LayerHostingContextIDCallback&& completionHandler)
{
    if (m_layerHostingContextID) {
        completionHandler(m_layerHostingContextID);
        return;
    }

    m_layerHostingContextIDRequests.append(WTFMove(completionHandler));
    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::RequestHostingContextID(), [weakThis = ThreadSafeWeakPtr { *this }] (auto contextID) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setLayerHostingContextID(contextID);
    }, m_id);
}

LayerHostingContextID MediaPlayerPrivateRemote::hostingContextID() const
{
    return m_layerHostingContextID;
}

void MediaPlayerPrivateRemote::setLayerHostingContextID(LayerHostingContextID inID)
{
    if (m_layerHostingContextID == inID)
        return;

    m_layerHostingContextID = inID;
#if PLATFORM(COCOA)
    m_videoLayer = nullptr;
#endif

    for (auto& request : std::exchange(m_layerHostingContextIDRequests, { }))
        request(inID);
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
    connection().send(Messages::RemoteMediaPlayerProxy::SetShouldCheckHardwareSupport(value), m_id);
}


#if HAVE(SPATIAL_TRACKING_LABEL)
const String& MediaPlayerPrivateRemote::defaultSpatialTrackingLabel() const
{
    return m_defaultSpatialTrackingLabel;
}

void MediaPlayerPrivateRemote::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    if (defaultSpatialTrackingLabel == m_defaultSpatialTrackingLabel)
        return;

    m_defaultSpatialTrackingLabel = WTFMove(defaultSpatialTrackingLabel);
    connection().send(Messages::RemoteMediaPlayerProxy::SetDefaultSpatialTrackingLabel(m_defaultSpatialTrackingLabel), m_id);
}

const String& MediaPlayerPrivateRemote::spatialTrackingLabel() const
{
    return m_spatialTrackingLabel;
}

void MediaPlayerPrivateRemote::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (spatialTrackingLabel == m_spatialTrackingLabel)
        return;

    m_spatialTrackingLabel = WTFMove(spatialTrackingLabel);
    connection().send(Messages::RemoteMediaPlayerProxy::SetSpatialTrackingLabel(m_spatialTrackingLabel), m_id);
}
#endif

void MediaPlayerPrivateRemote::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
    connection().send(Messages::RemoteMediaPlayerProxy::IsInFullscreenOrPictureInPictureChanged(isInFullscreenOrPictureInPicture), m_id);
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
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}
#endif

void MediaPlayerPrivateRemote::videoPlaybackConfigurationChanged(const VideoPlaybackConfiguration& configuration)
{
    ALWAYS_LOG(LOGIDENTIFIER, configuration);

    m_cachedState.videoConfiguration = configuration;
    if (auto player = m_player.get())
        player->videoPlaybackConfigurationChanged();
}

void MediaPlayerPrivateRemote::commitAllTransactions(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
