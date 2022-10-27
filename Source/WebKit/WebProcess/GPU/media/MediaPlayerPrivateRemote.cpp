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

#if ENABLE(GPU_PROCESS)

#include "Logging.h"
#include "RemoteAudioSourceProvider.h"
#include "RemoteLegacyCDM.h"
#include "RemoteLegacyCDMFactory.h"
#include "RemoteLegacyCDMSession.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxyMessages.h"
#include "RemoteMediaResourceManagerMessages.h"
#include "SandboxExtension.h"
#include "VideoLayerRemote.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/TextTrackRepresentation.h>
#include <WebCore/VideoLayerManager.h>
#include <wtf/HashMap.h>
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

MediaPlayerPrivateRemote::MediaPlayerPrivateRemote(MediaPlayer* player, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, MediaPlayerIdentifier playerIdentifier, RemoteMediaPlayerManager& manager)
    :
#if !RELEASE_LOG_DISABLED
      m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
    ,
#endif
      m_player(*player)
    , m_mediaResourceLoader(*player->createResourceLoader())
#if PLATFORM(COCOA)
    , m_videoLayerManager(makeUniqueRef<VideoLayerManagerObjC>(logger(), logIdentifier()))
#endif
    , m_manager(manager)
    , m_remoteEngineIdentifier(engineIdentifier)
    , m_id(playerIdentifier)
    , m_documentSecurityOrigin(player->documentSecurityOrigin())
{
    ALWAYS_LOG(LOGIDENTIFIER);

    acceleratedRenderingStateChanged();
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
}

void MediaPlayerPrivateRemote::prepareForPlayback(bool privateMode, MediaPlayer::Preload preload, bool preservesPitch, bool prepare)
{
    RefPtr player = m_player.get();
    if (!player)
        return;

    auto scale = player->playerContentsScale();
    auto preferredDynamicRangeMode = m_player->preferredDynamicRangeMode();
    auto presentationSize = player->presentationSize();

    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::PrepareForPlayback(privateMode, preload, preservesPitch, prepare, presentationSize, scale, preferredDynamicRangeMode), [weakThis = WeakPtr { *this }, this](auto inlineLayerHostingContextId) mutable {
        if (!weakThis)
            return;

        if (!inlineLayerHostingContextId)
            return;

        RefPtr player = m_player.get();
        if (!player)
            return;

        auto presentationSize = player->presentationSize();
        m_videoLayer = createVideoLayerRemote(this, inlineLayerHostingContextId.value(), m_videoFullscreenGravity, presentationSize);
#if PLATFORM(COCOA)
        m_videoLayerManager->setVideoLayer(m_videoLayer.get(), presentationSize);
#endif
    }, m_id);
}

void MediaPlayerPrivateRemote::load(const URL& url, const ContentType& contentType, const String& keySystem)
{
    std::optional<SandboxExtension::Handle> sandboxExtensionHandle;
    if (url.isLocalFile()) {
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
            if (RefPtr player = m_player.get())
                player->networkStateChanged();
            return;
        }

        sandboxExtensionHandle = WTFMove(handle);
    }

    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::Load(url, sandboxExtensionHandle, contentType, keySystem, m_player->requiresRemotePlayback()), [weakThis = WeakPtr { *this }, this](auto&& configuration) {
        if (!weakThis)
            return;

        RefPtr player = m_player.get();
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
    auto now = MonotonicTime::now();
    m_cachedMediaTime += MediaTime::createWithDouble(m_rate * (now - m_cachedMediaTimeQueryTime).value());
    m_cachedMediaTimeQueryTime = now;
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

MediaTime MediaPlayerPrivateRemote::durationMediaTime() const
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return m_mediaSourcePrivate->duration();
#endif

    return m_cachedState.duration;
}

MediaTime MediaPlayerPrivateRemote::currentMediaTime() const
{
    if (!m_timeIsProgressing)
        return m_cachedMediaTime;

    auto calculatedCurrentTime = m_cachedMediaTime + MediaTime::createWithDouble(m_rate * (MonotonicTime::now() - m_cachedMediaTimeQueryTime).seconds());
    return std::min(std::max(calculatedCurrentTime, MediaTime::zeroTime()), durationMediaTime());
}

void MediaPlayerPrivateRemote::seek(const MediaTime& time)
{
    m_seeking = true;
    m_cachedMediaTime = time;
    connection().send(Messages::RemoteMediaPlayerProxy::Seek(time), m_id);
}

void MediaPlayerPrivateRemote::seekWithTolerance(const MediaTime& time, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
{
    m_seeking = true;
    m_cachedMediaTime = time;
    connection().send(Messages::RemoteMediaPlayerProxy::SeekWithTolerance(time, negativeTolerance, positiveTolerance), m_id);
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

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateRemote::buffered() const
{
    if (!m_cachedBufferedTimeRanges)
        return makeUnique<PlatformTimeRanges>();

    return makeUnique<PlatformTimeRanges>(*m_cachedBufferedTimeRanges);
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
    m_cachedState.readyState = readyState;
    if (RefPtr player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateRemote::readyStateChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    if (RefPtr player = m_player.get()) {
        player->readyStateChanged();
        checkAcceleratedRenderingState();
    }
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

void MediaPlayerPrivateRemote::timeChanged(RemoteMediaPlayerState&& state)
{
    m_seeking = false;
    updateCachedState(WTFMove(state));
    if (RefPtr player = m_player.get())
        player->timeChanged();
}

void MediaPlayerPrivateRemote::durationChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    if (RefPtr player = m_player.get())
        player->durationChanged();
}

void MediaPlayerPrivateRemote::rateChanged(double rate)
{
    m_rate = rate;
    if (RefPtr player = m_player.get())
        player->rateChanged();
}

void MediaPlayerPrivateRemote::playbackStateChanged(bool paused, MediaTime&& mediaTime, MonotonicTime&& wallTime)
{
    m_cachedState.paused = paused;
    m_cachedMediaTime = mediaTime;
    m_cachedMediaTimeQueryTime = wallTime;
    if (RefPtr player = m_player.get())
        player->playbackStateChanged();
}

void MediaPlayerPrivateRemote::engineFailedToLoad(long platformErrorCode)
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

void MediaPlayerPrivateRemote::currentTimeChanged(const MediaTime& mediaTime, const MonotonicTime& queryTime, bool timeIsProgressing)
{
    auto reverseJump = mediaTime < m_cachedMediaTime;
    if (reverseJump)
        ALWAYS_LOG(LOGIDENTIFIER, "time jumped backwards, was ", m_cachedMediaTime, ", is now ", mediaTime);

    m_timeIsProgressing = timeIsProgressing;
    m_cachedMediaTime = mediaTime;
    m_cachedMediaTimeQueryTime = queryTime;

    if (reverseJump) {
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
    if (m_player) {
        m_renderingCanBeAccelerated = m_player->renderingCanBeAccelerated();
        connection().send(Messages::RemoteMediaPlayerProxy::AcceleratedRenderingStateChanged(m_renderingCanBeAccelerated), m_id);
    }
    renderingModeChanged();
}

void MediaPlayerPrivateRemote::checkAcceleratedRenderingState()
{
    if (m_player) {
        bool renderingCanBeAccelerated = m_player->renderingCanBeAccelerated();
        if (m_renderingCanBeAccelerated != renderingCanBeAccelerated)
            acceleratedRenderingStateChanged();
    }
}

void MediaPlayerPrivateRemote::updateConfiguration(RemoteMediaPlayerConfiguration&& configuration)
{
    m_configuration = WTFMove(configuration);
    // player->renderingCanBeAccelerated() result is dependent on m_configuration.supportsAcceleratedRendering value.
    checkAcceleratedRenderingState();
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
    m_cachedState.readyState = state.readyState;
    m_cachedState.paused = state.paused;
    m_cachedState.naturalSize = state.naturalSize;
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
    m_cachedState.hasSingleSecurityOrigin = state.hasSingleSecurityOrigin;
    m_cachedState.didPassCORSAccessCheck = state.didPassCORSAccessCheck;
    m_cachedState.wouldTaintDocumentSecurityOrigin = state.wouldTaintDocumentSecurityOrigin;

    if (state.bufferedRanges.length())
        m_cachedBufferedTimeRanges = makeUnique<PlatformTimeRanges>(state.bufferedRanges);
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

void MediaPlayerPrivateRemote::addRemoteAudioTrack(TrackPrivateRemoteIdentifier identifier, AudioTrackPrivateRemoteConfiguration&& configuration)
{
    auto addResult = m_audioTracks.ensure(identifier, [&] {
        return AudioTrackPrivateRemote::create(m_manager.gpuProcessConnection(), m_id, identifier, WTFMove(configuration));
    });
    ASSERT(addResult.isNewEntry);

    if (!addResult.isNewEntry)
        return;

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return;
#endif

    if (RefPtr player = m_player.get())
        m_player->addAudioTrack(addResult.iterator->value);
}

void MediaPlayerPrivateRemote::removeRemoteAudioTrack(TrackPrivateRemoteIdentifier identifier)
{
    ASSERT(m_audioTracks.contains(identifier));

    if (auto* track = m_audioTracks.get(identifier)) {
        if (RefPtr player = m_player.get())
            player->removeAudioTrack(*track);
        m_audioTracks.remove(identifier);
    }
}

void MediaPlayerPrivateRemote::remoteAudioTrackConfigurationChanged(TrackPrivateRemoteIdentifier identifier, AudioTrackPrivateRemoteConfiguration&& configuration)
{
    ASSERT(m_audioTracks.contains(identifier));

    if (auto track = m_audioTracks.get(identifier))
        track->updateConfiguration(WTFMove(configuration));
}

void MediaPlayerPrivateRemote::addRemoteTextTrack(TrackPrivateRemoteIdentifier identifier, TextTrackPrivateRemoteConfiguration&& configuration)
{
    auto addResult = m_textTracks.ensure(identifier, [&] {
        return TextTrackPrivateRemote::create(m_manager.gpuProcessConnection(), m_id, identifier, WTFMove(configuration));
    });
    ASSERT(addResult.isNewEntry);

    if (!addResult.isNewEntry)
        return;

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return;
#endif

    if (RefPtr player = m_player.get())
        player->addTextTrack(addResult.iterator->value);
}

void MediaPlayerPrivateRemote::removeRemoteTextTrack(TrackPrivateRemoteIdentifier identifier)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto* track = m_textTracks.get(identifier)) {
        if (RefPtr player = m_player.get())
            player->removeTextTrack(*track);
        m_textTracks.remove(identifier);
    }
}

void MediaPlayerPrivateRemote::remoteTextTrackConfigurationChanged(TrackPrivateRemoteIdentifier identifier, TextTrackPrivateRemoteConfiguration&& configuration)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->updateConfiguration(WTFMove(configuration));
}

void MediaPlayerPrivateRemote::parseWebVTTFileHeader(TrackPrivateRemoteIdentifier identifier, String&& header)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->parseWebVTTFileHeader(WTFMove(header));
}

void MediaPlayerPrivateRemote::parseWebVTTCueData(TrackPrivateRemoteIdentifier identifier, IPC::DataReference&& data)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->parseWebVTTCueData(WTFMove(data));
}

void MediaPlayerPrivateRemote::parseWebVTTCueDataStruct(TrackPrivateRemoteIdentifier identifier, ISOWebVTTCue&& data)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->parseWebVTTCueDataStruct(WTFMove(data));
}

void MediaPlayerPrivateRemote::addDataCue(TrackPrivateRemoteIdentifier identifier, MediaTime&& start, MediaTime&& end, IPC::DataReference&& data)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->addDataCue(WTFMove(start), WTFMove(end), WTFMove(data));
}

#if ENABLE(DATACUE_VALUE)
void MediaPlayerPrivateRemote::addDataCueWithType(TrackPrivateRemoteIdentifier identifier, MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& data, String&& type)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->addDataCueWithType(WTFMove(start), WTFMove(end), WTFMove(data), WTFMove(type));
}

void MediaPlayerPrivateRemote::updateDataCue(TrackPrivateRemoteIdentifier identifier, MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& data)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->updateDataCue(WTFMove(start), WTFMove(end), WTFMove(data));
}

void MediaPlayerPrivateRemote::removeDataCue(TrackPrivateRemoteIdentifier identifier, MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&& data)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->removeDataCue(WTFMove(start), WTFMove(end), WTFMove(data));
}
#endif

void MediaPlayerPrivateRemote::addGenericCue(TrackPrivateRemoteIdentifier identifier, GenericCueData&& cueData)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->addGenericCue(InbandGenericCue::create(WTFMove(cueData)));
}

void MediaPlayerPrivateRemote::updateGenericCue(TrackPrivateRemoteIdentifier identifier, GenericCueData&& cueData)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->updateGenericCue(InbandGenericCue::create(WTFMove(cueData)));
}

void MediaPlayerPrivateRemote::removeGenericCue(TrackPrivateRemoteIdentifier identifier, GenericCueData&& cueData)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto track = m_textTracks.get(identifier))
        track->removeGenericCue(InbandGenericCue::create(WTFMove(cueData)));
}

void MediaPlayerPrivateRemote::addRemoteVideoTrack(TrackPrivateRemoteIdentifier identifier, VideoTrackPrivateRemoteConfiguration&& configuration)
{
    auto addResult = m_videoTracks.ensure(identifier, [&] {
        return VideoTrackPrivateRemote::create(m_manager.gpuProcessConnection(), m_id, identifier, WTFMove(configuration));
    });
    ASSERT(addResult.isNewEntry);

    if (!addResult.isNewEntry)
        return;

#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate)
        return;
#endif

    if (RefPtr player = m_player.get())
        player->addVideoTrack(addResult.iterator->value);
}

void MediaPlayerPrivateRemote::removeRemoteVideoTrack(TrackPrivateRemoteIdentifier identifier)
{
    ASSERT(m_videoTracks.contains(identifier));

    if (auto* track = m_videoTracks.get(identifier)) {
        if (RefPtr player = m_player.get())
            player->removeVideoTrack(*track);
        m_videoTracks.remove(identifier);
    }
}

void MediaPlayerPrivateRemote::remoteVideoTrackConfigurationChanged(TrackPrivateRemoteIdentifier identifier, VideoTrackPrivateRemoteConfiguration&& configuration)
{
    ASSERT(m_videoTracks.contains(identifier));

    if (auto track = m_videoTracks.get(identifier))
        track->updateConfiguration(WTFMove(configuration));
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateRemote::load(const URL& url, const ContentType& contentType, MediaSourcePrivateClient& client)
{
    if (m_remoteEngineIdentifier == MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE) {
        auto identifier = RemoteMediaSourceIdentifier::generate();
        connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::LoadMediaSource(url, contentType, DeprecatedGlobalSettings::webMParserEnabled(), identifier), [weakThis = WeakPtr { *this }, this](auto&& configuration) {
            if (!weakThis)
                return;

            RefPtr player = m_player.get();
            if (!player)
                return;

            updateConfiguration(WTFMove(configuration));
            player->mediaEngineUpdated();
        }, m_id);
        m_mediaSourcePrivate = MediaSourcePrivateRemote::create(m_manager.gpuProcessConnection(), identifier, m_manager.typeCache(m_remoteEngineIdentifier), *this, client);

        return;
    }

    callOnMainRunLoop([weakThis = WeakPtr { *this }, this] {
        if (!weakThis)
            return;

        RefPtr player = m_player.get();
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
    callOnMainRunLoop([weakThis = WeakPtr { *this }, this] {
        if (!weakThis)
            return;

        RefPtr player = m_player.get();
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
    // A Fullscreen layer has been set, this could update the value returned by player->renderingCanBeAccelerated().
    checkAcceleratedRenderingState();
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
    RefPtr player = m_player.get();
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

MediaTime MediaPlayerPrivateRemote::maxMediaTimeSeekable() const
{
    return m_cachedState.maxTimeSeekable;
}

MediaTime MediaPlayerPrivateRemote::minMediaTimeSeekable() const
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

#if PLATFORM(COCOA)
void MediaPlayerPrivateRemote::setVideoInlineSizeFenced(const FloatSize& size, const WTF::MachSendRight& machSendRight)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetVideoInlineSizeFenced(size, machSendRight), m_id);
}
#endif

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

    FloatRect imageRect { FloatPoint::zero(), nativeImage->size() };
    context.drawNativeImage(*nativeImage, imageRect.size(), rect, imageRect);
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
    if (!sendResult)
        return nullptr;

    auto [result, changed] = sendResult.takeReply();
    if (changed) {
        if (result)
            m_videoFrameForCurrentTime = RemoteVideoFrameProxy::create(connection(), videoFrameObjectHeapProxy(), WTFMove(*result));
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
    if (RefPtr player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless);
}

bool MediaPlayerPrivateRemote::isCurrentPlaybackTargetWireless() const
{
    return m_isCurrentPlaybackTargetWireless;
}

void MediaPlayerPrivateRemote::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetWirelessPlaybackTarget(target->targetContext()), m_id);
}

void MediaPlayerPrivateRemote::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetShouldPlayToPlaybackTarget(shouldPlay), m_id);
}
#endif

bool MediaPlayerPrivateRemote::hasSingleSecurityOrigin() const
{
    return m_cachedState.hasSingleSecurityOrigin;
}

bool MediaPlayerPrivateRemote::didPassCORSAccessCheck() const
{
    return m_cachedState.didPassCORSAccessCheck;
}

std::optional<bool> MediaPlayerPrivateRemote::wouldTaintOrigin(const SecurityOrigin& origin) const
{
    if (origin.data() == m_documentSecurityOrigin)
        return m_cachedState.wouldTaintDocumentSecurityOrigin;

    if (auto result = m_wouldTaintOriginCache.get(origin.data()))
        return result;

    auto sendResult = connection().sendSync(Messages::RemoteMediaPlayerProxy::WouldTaintOrigin(origin.data()), m_id);
    auto [wouldTaint] = sendResult.takeReplyOr(std::nullopt);
    if (wouldTaint)
        m_wouldTaintOriginCache.add(origin.data(), wouldTaint);
    return wouldTaint;
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

void MediaPlayerPrivateRemote::mediaPlayerKeyNeeded(IPC::DataReference&& message)
{
    if (RefPtr player = m_player.get())
        player->keyNeeded(SharedBuffer::create(message));
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateRemote::cdmInstanceAttached(CDMInstance& instance)
{
    if (is<RemoteCDMInstance>(instance))
        connection().send(Messages::RemoteMediaPlayerProxy::CdmInstanceAttached(downcast<RemoteCDMInstance>(instance).identifier()), m_id);
}

void MediaPlayerPrivateRemote::cdmInstanceDetached(CDMInstance& instance)
{
    if (is<RemoteCDMInstance>(instance))
        connection().send(Messages::RemoteMediaPlayerProxy::CdmInstanceDetached(downcast<RemoteCDMInstance>(instance).identifier()), m_id);
}

void MediaPlayerPrivateRemote::attemptToDecryptWithInstance(CDMInstance& instance)
{
    if (is<RemoteCDMInstance>(instance))
        connection().send(Messages::RemoteMediaPlayerProxy::AttemptToDecryptWithInstance(downcast<RemoteCDMInstance>(instance).identifier()), m_id);
}

void MediaPlayerPrivateRemote::waitingForKeyChanged(bool waitingForKey)
{
    m_waitingForKey = waitingForKey;
    if (RefPtr player = m_player.get())
        player->waitingForKeyChanged();
}

void MediaPlayerPrivateRemote::initializationDataEncountered(const String& initDataType, IPC::DataReference&& initData)
{
    auto initDataBuffer = ArrayBuffer::create(initData.data(), initData.size());
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
    connection().send(Messages::RemoteMediaPlayerProxy::SetShouldContinueAfterKeyNeeded(should), m_id);
}
#endif

bool MediaPlayerPrivateRemote::requiresTextTrackRepresentation() const
{
#if PLATFORM(COCOA)
    return m_videoLayerManager->requiresTextTrackRepresentation();
#else
    return false;
#endif
}

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

#if ENABLE(AVF_CAPTIONS)
void MediaPlayerPrivateRemote::notifyTrackModeChanged()
{
    connection().send(Messages::RemoteMediaPlayerProxy::NotifyTrackModeChanged(), m_id);
}
#endif

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

bool MediaPlayerPrivateRemote::performTaskAtMediaTime(WTF::Function<void()>&& completionHandler, const MediaTime& mediaTime)
{
    auto asyncReplyHandler = [weakThis = WeakPtr { *this }, this, completionHandler = WTFMove(completionHandler)](std::optional<MediaTime> currentTime, std::optional<MonotonicTime> queryTime) mutable {
        if (!weakThis || !currentTime || !queryTime)
            return;

        m_cachedMediaTime = *currentTime;
        m_cachedMediaTimeQueryTime = *queryTime;
        completionHandler();
    };

    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::PerformTaskAtMediaTime(mediaTime, MonotonicTime::now()), WTFMove(asyncReplyHandler), m_id);

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
    // The client(RemoteMediaResourceProxy) will be destroyed as well
    m_mediaResources.remove(remoteMediaResourceIdentifier);
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

} // namespace WebKit

#endif
