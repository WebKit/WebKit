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

#if ENABLE(GPU_PROCESS)
#include "MediaPlayerPrivateRemote.h"

#include "AudioTrackPrivateRemote.h"
#include "Logging.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxyMessages.h"
#include "SandboxExtension.h"
#include "VideoTrackPrivateRemote.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/PlatformTimeRanges.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>

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

MediaPlayerPrivateRemote::MediaPlayerPrivateRemote(MediaPlayer* player, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, MediaPlayerPrivateRemoteIdentifier playerIdentifier, RemoteMediaPlayerManager& manager, const RemoteMediaPlayerConfiguration& configuration)
    : m_player(player)
    , m_mediaResourceLoader(player->createResourceLoader())
    , m_manager(manager)
    , m_remoteEngineIdentifier(engineIdentifier)
    , m_id(playerIdentifier)
    , m_configuration(configuration)
#if !RELEASE_LOG_DISABLED
    , m_logger(&player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
#endif
{
    INFO_LOG(LOGIDENTIFIER);
}

MediaPlayerPrivateRemote::~MediaPlayerPrivateRemote()
{
    INFO_LOG(LOGIDENTIFIER);
    m_manager.deleteRemoteMediaPlayer(m_id);
}

void MediaPlayerPrivateRemote::prepareForPlayback(bool privateMode, MediaPlayer::Preload preload, bool preservesPitch, bool prepare)
{
    auto layoutRect = m_player->playerContentBoxRect();
    auto scale = m_player->playerContentsScale();

    connection().sendWithAsyncReply(Messages::RemoteMediaPlayerProxy::PrepareForPlayback(privateMode, preload, preservesPitch, prepare, layoutRect, scale), [weakThis = makeWeakPtr(*this), this](auto contextId) mutable {
        if (!weakThis)
            return;

        if (!contextId)
            return;

        m_videoLayer = LayerHostingContext::createPlatformLayerForHostingContext(contextId.value());
    }, m_id);
}

void MediaPlayerPrivateRemote::MediaPlayerPrivateRemote::load(const URL& url, const ContentType& contentType, const String& keySystem)
{
    Optional<SandboxExtension::Handle> sandboxExtensionHandle;
    if (url.isLocalFile()) {
        SandboxExtension::Handle handle;
        auto fileSystemPath = url.fileSystemPath();
        bool createdExtension = false;

#if HAVE(SANDBOX_ISSUE_READ_EXTENSION_TO_PROCESS_BY_AUDIT_TOKEN)
        auto auditToken = m_manager.gpuProcessConnection().auditToken();
        ASSERT(auditToken);
        if (auditToken)
            createdExtension = SandboxExtension::createHandleForReadByAuditToken(fileSystemPath, auditToken.value(), handle);
        else
#endif
        createdExtension = SandboxExtension::createHandle(fileSystemPath, SandboxExtension::Type::ReadOnly, handle);

        if (!createdExtension) {
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
    return m_hasVideo;
}

bool MediaPlayerPrivateRemote::hasAudio() const
{
    return m_hasAudio;
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
    return m_movieLoadType;
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

void MediaPlayerPrivateRemote::characteristicChanged(bool hasAudio, bool hasVideo, WebCore::MediaPlayerEnums::MovieLoadType movieLoadType)
{
    m_movieLoadType = movieLoadType;
    m_hasAudio = hasAudio;
    m_hasVideo = hasVideo;
    m_player->characteristicChanged();
}

void MediaPlayerPrivateRemote::sizeChanged(WebCore::FloatSize naturalSize)
{
    m_cachedState.naturalSize = naturalSize;
    m_player->sizeChanged();
}

void MediaPlayerPrivateRemote::firstVideoFrameAvailable()
{
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

bool MediaPlayerPrivateRemote::canPlayToWirelessPlaybackTarget() const
{
    return m_configuration.canPlayToWirelessPlaybackTarget;
}

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

void MediaPlayerPrivateRemote::setVideoFullscreenFrame(WebCore::FloatRect rect)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetVideoFullscreenFrame(rect), m_id);
}

void MediaPlayerPrivateRemote::setVideoFullscreenGravity(WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetVideoFullscreenGravity(gravity), m_id);
}

void MediaPlayerPrivateRemote::acceleratedRenderingStateChanged()
{
    connection().send(Messages::RemoteMediaPlayerProxy::AcceleratedRenderingStateChanged(m_player->supportsAcceleratedRendering()), m_id);
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

void MediaPlayerPrivateRemote::removeRemoteAudioTrack(TrackPrivateRemoteIdentifier id)
{
    ASSERT(m_audioTracks.contains(id));

    if (auto* track = m_audioTracks.get(id)) {
        m_player->removeAudioTrack(*track);
        m_audioTracks.remove(id);
    }
}

void MediaPlayerPrivateRemote::remoteAudioTrackConfigurationChanged(TrackPrivateRemoteIdentifier id, TrackPrivateRemoteConfiguration&& configuration)
{
    ASSERT(m_audioTracks.contains(id));

    if (const auto& track = m_audioTracks.get(id))
        track->updateConfiguration(WTFMove(configuration));
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

    if (const auto& track = m_videoTracks.get(id))
        track->updateConfiguration(WTFMove(configuration));
}

// FIXME: Unimplemented

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateRemote::load(const String&, MediaSourcePrivateClient*)
{
    notImplemented();
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
    notImplemented();
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
    return m_videoLayer.get();
}

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
void MediaPlayerPrivateRemote::setVideoFullscreenLayer(PlatformLayer*, WTF::Function<void()>&&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::updateVideoFullscreenInlineImage()
{
    notImplemented();
}

void MediaPlayerPrivateRemote::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::videoFullscreenStandbyChanged()
{
    notImplemented();
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
    notImplemented();
    return emptyString();
}

String MediaPlayerPrivateRemote::errorLog() const
{
    notImplemented();
    return emptyString();
}
#endif

void MediaPlayerPrivateRemote::setBufferingPolicy(MediaPlayer::BufferingPolicy)
{
    notImplemented();
}

bool MediaPlayerPrivateRemote::canSaveMediaData() const
{
    notImplemented();
    return false;
}

MediaTime MediaPlayerPrivateRemote::getStartDate() const
{
    notImplemented();
    return MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateRemote::startTime() const
{
    notImplemented();
    return MediaTime::zeroTime();
}

void MediaPlayerPrivateRemote::setRateDouble(double)
{
    notImplemented();
}

bool MediaPlayerPrivateRemote::hasClosedCaptions() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateRemote::setClosedCaptionsVisible(bool)
{
    notImplemented();
}

double MediaPlayerPrivateRemote::maxFastForwardRate() const
{
    notImplemented();
    return 0;
}

double MediaPlayerPrivateRemote::minFastReverseRate() const
{
    notImplemented();
    return 0;
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
    notImplemented();
    return 0;
}

double MediaPlayerPrivateRemote::liveUpdateInterval() const
{
    notImplemented();
    return 0;
}

unsigned long long MediaPlayerPrivateRemote::totalBytes() const
{
    notImplemented();
    return 0;
}

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
    notImplemented();
    return false;
}

#if USE(NATIVE_FULLSCREEN_VIDEO)
void MediaPlayerPrivateRemote::enterFullscreen()
{
    notImplemented();
}

void MediaPlayerPrivateRemote::exitFullscreen()
{
    notImplemented();
}
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
String MediaPlayerPrivateRemote::wirelessPlaybackTargetName() const
{
    notImplemented();
    return emptyString();
}

MediaPlayer::WirelessPlaybackTargetType MediaPlayerPrivateRemote::wirelessPlaybackTargetType() const
{
    notImplemented();
    return MediaPlayer::TargetTypeNone;
}

bool MediaPlayerPrivateRemote::wirelessVideoPlaybackDisabled() const
{
    notImplemented();
    return true;
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

void MediaPlayerPrivateRemote::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    connection().send(Messages::RemoteMediaPlayerProxy::SetShouldPlayToPlaybackTarget(shouldPlay), m_id);
}
#endif

#if USE(NATIVE_FULLSCREEN_VIDEO)
bool MediaPlayerPrivateRemote::canEnterFullscreen() const
{
    notImplemented();
    return false;
}
#endif

bool MediaPlayerPrivateRemote::shouldMaintainAspectRatio() const
{
    notImplemented();
    return true;
}

bool MediaPlayerPrivateRemote::hasSingleSecurityOrigin() const
{
    notImplemented();
    return false;
}

bool MediaPlayerPrivateRemote::didPassCORSAccessCheck() const
{
    notImplemented();
    return false;
}

Optional<bool> MediaPlayerPrivateRemote::wouldTaintOrigin(const SecurityOrigin&) const
{
    notImplemented();
    return WTF::nullopt;
}

MediaTime MediaPlayerPrivateRemote::mediaTimeForTimeValue(const MediaTime& timeValue) const
{
    notImplemented();
    return timeValue;
}

double MediaPlayerPrivateRemote::maximumDurationToCacheMediaTime() const
{
    return .2; // FIXME: get this value from the media engine when it is created.
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

void MediaPlayerPrivateRemote::setCDMSession(LegacyCDMSession*)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::keyAdded()
{
    connection().send(Messages::RemoteMediaPlayerProxy::KeyAdded(), m_id);
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateRemote::cdmInstanceAttached(CDMInstance&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::cdmInstanceDetached(CDMInstance&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::attemptToDecryptWithInstance(CDMInstance&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::waitingForKeyChanged()
{
    m_player->waitingForKeyChanged();
}

bool MediaPlayerPrivateRemote::waitingForKey() const
{
    notImplemented();
    return false;
}
#endif

#if ENABLE(VIDEO_TRACK)
bool MediaPlayerPrivateRemote::requiresTextTrackRepresentation() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateRemote::setTextTrackRepresentation(TextTrackRepresentation*)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::syncTextTrackBounds()
{
    notImplemented();
}

void MediaPlayerPrivateRemote::tracksChanged()
{
    notImplemented();
}
#endif

#if USE(GSTREAMER)
void MediaPlayerPrivateRemote::simulateAudioInterruption()
{
    notImplemented();
}
#endif

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
    notImplemented();
    return emptyString();
}

size_t MediaPlayerPrivateRemote::extraMemoryCost() const
{
    notImplemented();
    return 0;
}

unsigned long long MediaPlayerPrivateRemote::fileSize() const
{
    notImplemented();
    return 0;
}

bool MediaPlayerPrivateRemote::ended() const
{
    notImplemented();
    return false;
}

Optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateRemote::videoPlaybackQualityMetrics()
{
    notImplemented();
    return WTF::nullopt;
}

#if ENABLE(AVF_CAPTIONS)
void MediaPlayerPrivateRemote::notifyTrackModeChanged()
{
    notImplemented();
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

bool MediaPlayerPrivateRemote::performTaskAtMediaTime(WTF::Function<void()>&&, MediaTime)
{
    notImplemented();
    return false;
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
