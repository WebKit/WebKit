/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "RemoteLegacyCDM.h"
#include "RemoteLegacyCDMFactory.h"
#include "RemoteLegacyCDMSession.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxyMessages.h"
#include "SandboxExtension.h"
#include "VideoLayerRemote.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/TypedArrayType.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/ResourceError.h>
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

#if !PLATFORM(COCOA)
MediaPlayerPrivateRemote::MediaPlayerPrivateRemote(MediaPlayer* player, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, MediaPlayerPrivateRemoteIdentifier playerIdentifier, RemoteMediaPlayerManager& manager)
    : m_player(player)
#if !RELEASE_LOG_DISABLED
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
#endif
    , m_mediaResourceLoader(*player->createResourceLoader())
    , m_manager(manager)
    , m_remoteEngineIdentifier(engineIdentifier)
    , m_id(playerIdentifier)
{
    INFO_LOG(LOGIDENTIFIER);
}
#endif

MediaPlayerPrivateRemote::~MediaPlayerPrivateRemote()
{
    INFO_LOG(LOGIDENTIFIER);
#if PLATFORM(COCOA)
    m_videoLayerManager->didDestroyVideoLayer();
#endif
    m_manager.deleteRemoteMediaPlayer(m_id);
}

void MediaPlayerPrivateRemote::setConfiguration(RemoteMediaPlayerConfiguration&& configuration, WebCore::SecurityOriginData&& documentSecurityOrigin)
{
    m_configuration = WTFMove(configuration);
    m_documentSecurityOrigin = WTFMove(documentSecurityOrigin);
    m_player->mediaEngineUpdated();
}

void MediaPlayerPrivateRemote::prepareForPlayback(bool privateMode, MediaPlayer::Preload preload, bool preservesPitch, bool prepare)
{
    auto scale = m_player->playerContentsScale();

    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::PrepareForPlayback(privateMode, preload, preservesPitch, prepare, scale), [weakThis = makeWeakPtr(*this), this](auto inlineLayerHostingContextId) mutable {
        if (!weakThis)
            return;

        if (!inlineLayerHostingContextId)
            return;

        m_videoLayer = createVideoLayerRemote(this, inlineLayerHostingContextId.value());
#if PLATFORM(COCOA)
        m_videoLayerManager->setVideoLayer(m_videoLayer.get(), snappedIntRect(m_player->playerContentBoxRect()).size());
#endif
    }, m_id);
}

void MediaPlayerPrivateRemote::MediaPlayerPrivateRemote::load(const URL& url, const ContentType& contentType, const String& keySystem)
{
    Optional<SandboxExtension::Handle> sandboxExtensionHandle;
    if (url.isLocalFile()) {
        SandboxExtension::Handle handle;
        auto fileSystemPath = url.fileSystemPath();

        auto createExtension = [&] {
#if HAVE(AUDIT_TOKEN)
            if (auto auditToken = m_manager.gpuProcessConnection().auditToken())
                return SandboxExtension::createHandleForReadByAuditToken(fileSystemPath, auditToken.value(), handle);
#endif

            return SandboxExtension::createHandle(fileSystemPath, SandboxExtension::Type::ReadOnly, handle);
        };

        if (!createExtension()) {
            WTFLogAlways("Unable to create sandbox extension handle for GPUProcess url.\n");
            m_cachedState.networkState = MediaPlayer::NetworkState::FormatError;
            m_player->networkStateChanged();
            return;
        }

        sandboxExtensionHandle = WTFMove(handle);
    }

    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::Load(url, sandboxExtensionHandle, contentType, keySystem), [weakThis = makeWeakPtr(*this)](auto&& configuration) {
        if (weakThis)
            weakThis->m_configuration = WTFMove(configuration);
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
    connection().send(Messages::RemoteMediaPlayerProxy::Pause(), m_id);
}

void MediaPlayerPrivateRemote::setPreservesPitch(bool preservesPitch)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetPreservesPitch(preservesPitch), m_id);
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

MediaTime MediaPlayerPrivateRemote::currentMediaTime() const
{
    return m_cachedState.currentTime;
}

void MediaPlayerPrivateRemote::seek(const MediaTime& time)
{
    m_seeking = true;
    connection().send(Messages::RemoteMediaPlayerProxy::Seek(time), m_id);
}

void MediaPlayerPrivateRemote::seekWithTolerance(const MediaTime& time, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
{
    m_seeking = true;
    connection().send(Messages::RemoteMediaPlayerProxy::SeekWithTolerance(time, negativeTolerance, positiveTolerance), m_id);
}

bool MediaPlayerPrivateRemote::didLoadingProgress() const
{
    return m_cachedState.loadingProgressed;
}

bool MediaPlayerPrivateRemote::hasVideo() const
{
    return m_cachedState.hasVideo;
}

bool MediaPlayerPrivateRemote::hasAudio() const
{
    return m_cachedState.hasAudio;
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateRemote::seekable() const
{
    return makeUnique<PlatformTimeRanges>(m_cachedState.minTimeSeekable, m_cachedState.maxTimeSeekable);
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
    m_player->networkStateChanged();
}

void MediaPlayerPrivateRemote::readyStateChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    m_player->readyStateChanged();
}

void MediaPlayerPrivateRemote::volumeChanged(double volume)
{
    m_volume = volume;
    m_player->volumeChanged(volume);
}

void MediaPlayerPrivateRemote::muteChanged(bool muted)
{
    m_muted = muted;
    m_player->muteChanged(muted);
}

void MediaPlayerPrivateRemote::timeChanged(RemoteMediaPlayerState&& state)
{
    m_seeking = false;
    updateCachedState(WTFMove(state));
    m_player->timeChanged();
}

void MediaPlayerPrivateRemote::durationChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    m_player->durationChanged();
}

void MediaPlayerPrivateRemote::rateChanged(double rate)
{
    m_rate = rate;
    m_player->rateChanged();
}

void MediaPlayerPrivateRemote::playbackStateChanged(bool paused)
{
    m_cachedState.paused = paused;
    m_player->playbackStateChanged();
}

void MediaPlayerPrivateRemote::engineFailedToLoad(long platformErrorCode)
{
    m_platformErrorCode = platformErrorCode;
    m_player->remoteEngineFailedToLoad();
}

void MediaPlayerPrivateRemote::characteristicChanged(RemoteMediaPlayerState&& state)
{
    updateCachedState(WTFMove(state));
    m_player->characteristicChanged();
}

void MediaPlayerPrivateRemote::sizeChanged(WebCore::FloatSize naturalSize)
{
    m_cachedState.naturalSize = naturalSize;
    m_player->sizeChanged();
}

void MediaPlayerPrivateRemote::firstVideoFrameAvailable()
{
    INFO_LOG(LOGIDENTIFIER);
    m_player->firstVideoFrameAvailable();
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
    connection().send(Messages::RemoteMediaPlayerProxy::AcceleratedRenderingStateChanged(m_player->supportsAcceleratedRendering()), m_id);
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
bool MediaPlayerPrivateRemote::canPlayToWirelessPlaybackTarget() const
{
    return m_configuration.canPlayToWirelessPlaybackTarget;
}
#endif

void MediaPlayerPrivateRemote::updateCachedState(RemoteMediaPlayerState&& state)
{
    m_cachedState.currentTime = state.currentTime;
    m_cachedState.duration = state.duration;
    m_cachedState.minTimeSeekable = state.minTimeSeekable;
    m_cachedState.maxTimeSeekable = state.maxTimeSeekable;
    m_cachedState.networkState = state.networkState;
    m_cachedState.readyState = state.readyState;
    m_cachedState.paused = state.paused;
    m_cachedState.loadingProgressed = state.loadingProgressed;
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
    m_cachedState.hasClosedCaptions = state.hasClosedCaptions;
    m_cachedState.hasAvailableVideoFrame = state.hasAvailableVideoFrame;
    m_cachedState.wirelessVideoPlaybackDisabled = state.wirelessVideoPlaybackDisabled;
    m_cachedState.hasSingleSecurityOrigin = state.hasSingleSecurityOrigin;
    m_cachedState.didPassCORSAccessCheck = state.didPassCORSAccessCheck;

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

void MediaPlayerPrivateRemote::setVisible(bool visible)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetVisible(visible), m_id);
}

void MediaPlayerPrivateRemote::setShouldMaintainAspectRatio(bool maintainRatio)
{
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

void MediaPlayerPrivateRemote::addRemoteAudioTrack(TrackPrivateRemoteIdentifier identifier, TrackPrivateRemoteConfiguration&& configuration)
{
    auto addResult = m_audioTracks.ensure(identifier, [&] {
        return AudioTrackPrivateRemote::create(*this, identifier, WTFMove(configuration));
    });
    ASSERT(addResult.isNewEntry);

    if (!addResult.isNewEntry)
        return;

    m_player->addAudioTrack(addResult.iterator->value);
}

void MediaPlayerPrivateRemote::removeRemoteAudioTrack(TrackPrivateRemoteIdentifier identifier)
{
    ASSERT(m_audioTracks.contains(identifier));

    if (auto* track = m_audioTracks.get(identifier)) {
        m_player->removeAudioTrack(*track);
        m_audioTracks.remove(identifier);
    }
}

void MediaPlayerPrivateRemote::remoteAudioTrackConfigurationChanged(TrackPrivateRemoteIdentifier identifier, TrackPrivateRemoteConfiguration&& configuration)
{
    ASSERT(m_audioTracks.contains(identifier));

    if (auto track = m_audioTracks.get(identifier))
        track->updateConfiguration(WTFMove(configuration));
}

void MediaPlayerPrivateRemote::addRemoteTextTrack(TrackPrivateRemoteIdentifier identifier, TextTrackPrivateRemoteConfiguration&& configuration)
{
    auto addResult = m_textTracks.ensure(identifier, [&] {
        return TextTrackPrivateRemote::create(*this, identifier, WTFMove(configuration));
    });
    ASSERT(addResult.isNewEntry);

    if (!addResult.isNewEntry)
        return;

    m_player->addTextTrack(addResult.iterator->value);
}

void MediaPlayerPrivateRemote::removeRemoteTextTrack(TrackPrivateRemoteIdentifier identifier)
{
    ASSERT(m_textTracks.contains(identifier));

    if (auto* track = m_textTracks.get(identifier)) {
        m_player->removeTextTrack(*track);
        m_textTracks.remove(identifier);
    }
}

void MediaPlayerPrivateRemote::remoteTextTrackConfigurationChanged(TrackPrivateRemoteIdentifier id, TextTrackPrivateRemoteConfiguration&& configuration)
{
    ASSERT(m_textTracks.contains(id));

    if (auto track = m_textTracks.get(id))
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

void MediaPlayerPrivateRemote::addRemoteVideoTrack(TrackPrivateRemoteIdentifier identifier, TrackPrivateRemoteConfiguration&& configuration)
{
    auto addResult = m_videoTracks.ensure(identifier, [&] {
        return VideoTrackPrivateRemote::create(*this, identifier, WTFMove(configuration));
    });
    ASSERT(addResult.isNewEntry);

    if (!addResult.isNewEntry)
        return;

    m_player->addVideoTrack(addResult.iterator->value);
}

void MediaPlayerPrivateRemote::removeRemoteVideoTrack(TrackPrivateRemoteIdentifier id)
{
    ASSERT(m_videoTracks.contains(id));

    if (auto* track = m_videoTracks.get(id)) {
        m_player->removeVideoTrack(*track);
        m_videoTracks.remove(id);
    }
}

void MediaPlayerPrivateRemote::remoteVideoTrackConfigurationChanged(TrackPrivateRemoteIdentifier id, TrackPrivateRemoteConfiguration&& configuration)
{
    ASSERT(m_videoTracks.contains(id));

    if (auto track = m_videoTracks.get(id))
        track->updateConfiguration(WTFMove(configuration));
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateRemote::load(const String&, MediaSourcePrivateClient*)
{
    callOnMainThread([weakThis = makeWeakPtr(*this), this] {
        if (!weakThis)
            return;

        m_cachedState.networkState = MediaPlayer::NetworkState::FormatError;
        m_player->networkStateChanged();
    });
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateRemote::load(MediaStreamPrivate&)
{
    callOnMainThread([weakThis = makeWeakPtr(*this), this] {
        if (!weakThis)
            return;

        m_cachedState.networkState = MediaPlayer::NetworkState::FormatError;
        m_player->networkStateChanged();
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
    connection().send(Messages::RemoteMediaPlayerProxy::SetVideoFullscreenGravity(gravity), m_id);
}

void MediaPlayerPrivateRemote::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode mode)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetVideoFullscreenMode(mode), m_id);
}

void MediaPlayerPrivateRemote::videoFullscreenStandbyChanged()
{
    connection().send(Messages::RemoteMediaPlayerProxy::VideoFullscreenStandbyChanged(), m_id);
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
    String log;
    if (!connection().sendSync(Messages::RemoteMediaPlayerProxy::AccessLog(), Messages::RemoteMediaPlayerProxy::AccessLog::Reply(log), m_id))
        return emptyString();

    return log;
}

String MediaPlayerPrivateRemote::errorLog() const
{
    String log;
    if (!connection().sendSync(Messages::RemoteMediaPlayerProxy::ErrorLog(), Messages::RemoteMediaPlayerProxy::ErrorLog::Reply(log), m_id))
        return emptyString();

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

#if PLATFORM(COCOA)
void MediaPlayerPrivateRemote::setVideoInlineSizeFenced(const IntSize& size, const WTF::MachSendRight& machSendRight)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetVideoInlineSizeFenced(size, machSendRight), m_id);
}
#endif

void MediaPlayerPrivateRemote::paint(GraphicsContext&, const FloatRect&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::paintCurrentFrameInContext(GraphicsContext&, const FloatRect&)
{
    notImplemented();
}

bool MediaPlayerPrivateRemote::copyVideoTextureToPlatformTexture(GraphicsContextGLOpenGL*, PlatformGLObject, GCGLenum, GCGLint, GCGLenum, GCGLenum, GCGLenum, bool, bool)
{
    notImplemented();
    return false;
}

NativeImagePtr MediaPlayerPrivateRemote::nativeImageForCurrentTime()
{
    notImplemented();
    return nullptr;
}

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
    connection().send(Messages::RemoteMediaPlayerProxy::SetWirelessVideoPlaybackDisabled(disabled), m_id);
}

void MediaPlayerPrivateRemote::currentPlaybackTargetIsWirelessChanged(bool isCurrentPlaybackTargetWireless)
{
    m_isCurrentPlaybackTargetWireless = isCurrentPlaybackTargetWireless;
    m_player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless);
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

Optional<bool> MediaPlayerPrivateRemote::wouldTaintOrigin(const SecurityOrigin& origin) const
{
    if (origin.data() == m_documentSecurityOrigin)
        return m_cachedState.wouldTaintDocumentSecurityOrigin;

    if (auto result = m_wouldTaintOriginCache.get(origin.data()))
        return result;

    Optional<bool> wouldTaint;
    if (!connection().sendSync(Messages::RemoteMediaPlayerProxy::WouldTaintOrigin(origin.data()), Messages::RemoteMediaPlayerProxy::WouldTaintOrigin::Reply(wouldTaint), m_id))
        return WTF::nullopt;

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
    notImplemented();
    return nullptr;
}
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
std::unique_ptr<LegacyCDMSession> MediaPlayerPrivateRemote::createSession(const String&, LegacyCDMSessionClient*)
{
    notImplemented();
    return nullptr;
}

void MediaPlayerPrivateRemote::setCDM(LegacyCDM* cdm)
{
    if (!cdm)
        return;

    auto remoteCDM = m_manager.gpuProcessConnection().legacyCDMFactory().findCDM(cdm->cdmPrivate());
    if (!remoteCDM)
        return;

    remoteCDM->setPlayerId(m_id);
}

void MediaPlayerPrivateRemote::setCDMSession(LegacyCDMSession* session)
{
    if (!session || session->type() != CDMSessionTypeRemote) {
        connection().send(Messages::RemoteMediaPlayerProxy::SetLegacyCDMSession({ }), m_id);
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
    m_player->keyNeeded(Uint8Array::create(message.data(), message.size()).ptr());
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

void MediaPlayerPrivateRemote::waitingForKeyChanged()
{
    m_player->waitingForKeyChanged();
}

void MediaPlayerPrivateRemote::initializationDataEncountered(const String& initDataType, IPC::DataReference&& initData)
{
    auto initDataBuffer = ArrayBuffer::create(initData.data(), initData.size());
    m_player->initializationDataEncountered(initDataType, WTFMove(initDataBuffer));
}

bool MediaPlayerPrivateRemote::waitingForKey() const
{
    notImplemented();
    return false;
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

Optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateRemote::videoPlaybackQualityMetrics()
{
    notImplemented();
    return WTF::nullopt;
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

void MediaPlayerPrivateRemote::applicationWillResignActive()
{
    connection().send(Messages::RemoteMediaPlayerProxy::ApplicationWillResignActive(), m_id);
}

void MediaPlayerPrivateRemote::applicationDidBecomeActive()
{
    connection().send(Messages::RemoteMediaPlayerProxy::ApplicationDidBecomeActive(), m_id);
}

bool MediaPlayerPrivateRemote::performTaskAtMediaTime(WTF::Function<void()>&& completionHandler, const MediaTime& mediaTime)
{
    auto asyncReplyHandler = [weakThis = makeWeakPtr(*this), this, completionHandler = WTFMove(completionHandler)](Optional<MediaTime> currentTime) mutable {
        if (!weakThis || !currentTime)
            return;

        m_cachedState.currentTime = *currentTime;
        completionHandler();
    };

    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::PerformTaskAtMediaTime(mediaTime, WallTime::now()), WTFMove(asyncReplyHandler), m_id);

    return true;
}

void MediaPlayerPrivateRemote::requestResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier, WebCore::ResourceRequest&& request, WebCore::PlatformMediaResourceLoader::LoadOptions options, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!m_mediaResources.contains(remoteMediaResourceIdentifier));
    auto resource = m_mediaResourceLoader->requestResource(WTFMove(request), options);

    // PlatformMediaResource owns the PlatformMediaResourceClient
    resource->setClient(makeUnique<RemoteMediaResourceProxy>(connection(), *resource, remoteMediaResourceIdentifier));
    m_mediaResources.add(remoteMediaResourceIdentifier, WTFMove(resource));

    completionHandler();
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
    m_player->resourceNotSupported();
}

void MediaPlayerPrivateRemote::engineUpdated()
{
    m_player->mediaEngineUpdated();
}

void MediaPlayerPrivateRemote::activeSourceBuffersChanged()
{
    m_player->activeSourceBuffersChanged();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaPlayerPrivateRemote::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebKit

#endif
