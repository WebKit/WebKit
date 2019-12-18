/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/PlatformTimeRanges.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;

MediaPlayerPrivateRemote::MediaPlayerPrivateRemote(MediaPlayer* player, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, MediaPlayerPrivateRemoteIdentifier playerIdentifier, RemoteMediaPlayerManager& manager)
    : m_player(player)
    , m_manager(manager)
    , m_remoteEngineIdentifier(engineIdentifier)
    , m_id(playerIdentifier)
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
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::PrepareForPlayback(m_id, privateMode, preload, preservesPitch, prepare), 0);
}

void MediaPlayerPrivateRemote::MediaPlayerPrivateRemote::load(const URL& url, const ContentType& contentType, const String& keySystem)
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::Load(m_id, url, contentType, keySystem), 0);
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateRemote::load(const String&, MediaSourcePrivateClient*)
{
    notImplemented();
    callOnMainThread([weakThis = makeWeakPtr(*this), this] {
        if (!weakThis)
            return;

        networkStateChanged(MediaPlayer::NetworkState::FormatError);
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

        networkStateChanged(MediaPlayer::NetworkState::FormatError);
    });
}
#endif

void MediaPlayerPrivateRemote::cancelLoad()
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::CancelLoad(m_id), 0);
}

void MediaPlayerPrivateRemote::prepareToPlay()
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::PrepareToPlay(m_id), 0);
}

PlatformLayer* MediaPlayerPrivateRemote::platformLayer() const
{
    return nullptr;
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

void MediaPlayerPrivateRemote::setVideoFullscreenFrame(FloatRect)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::setVideoFullscreenGravity(MediaPlayer::VideoGravity)
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

void MediaPlayerPrivateRemote::play()
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::Play(m_id), 0);
}

void MediaPlayerPrivateRemote::pause()
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::Pause(m_id), 0);
}

void MediaPlayerPrivateRemote::setBufferingPolicy(MediaPlayer::BufferingPolicy)
{
    notImplemented();
}

bool MediaPlayerPrivateRemote::supportsPictureInPicture() const
{
    notImplemented();
    return false;
}

bool MediaPlayerPrivateRemote::supportsFullscreen() const
{
    notImplemented();
    return false;
}

bool MediaPlayerPrivateRemote::supportsScanning() const
{
    notImplemented();
    return false;
}

bool MediaPlayerPrivateRemote::canSaveMediaData() const
{
    notImplemented();
    return false;
}

FloatSize MediaPlayerPrivateRemote::naturalSize() const
{
    notImplemented();
    return { };
}

bool MediaPlayerPrivateRemote::hasVideo() const
{
    notImplemented();
    return false;
}

bool MediaPlayerPrivateRemote::hasAudio() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateRemote::setVisible(bool)
{
    notImplemented();
}

MediaTime MediaPlayerPrivateRemote::currentMediaTime() const
{
    notImplemented();
    return MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateRemote::getStartDate() const
{
    notImplemented();
    return MediaTime::zeroTime();
}

void MediaPlayerPrivateRemote::seek(const MediaTime&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::seekWithTolerance(const MediaTime&, const MediaTime&, const MediaTime&)
{
    notImplemented();
}

bool MediaPlayerPrivateRemote::seeking() const
{
    notImplemented();
    return false;
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

void MediaPlayerPrivateRemote::setPreservesPitch(bool preservesPitch)
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::SetPreservesPitch(m_id, preservesPitch), 0);
}

void MediaPlayerPrivateRemote::setVolumeDouble(double volume)
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::SetVolume(m_id, volume), 0);
}

void MediaPlayerPrivateRemote::setMuted(bool muted)
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::SetMuted(m_id, muted), 0);
}

void MediaPlayerPrivateRemote::setPreload(MediaPlayer::Preload preload)
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::SetPreload(m_id, preload), 0);
}

void MediaPlayerPrivateRemote::setPrivateBrowsingMode(bool privateMode)
{
    m_manager.gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::SetPrivateBrowsingMode(m_id, privateMode), 0);
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

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateRemote::seekable() const
{
    notImplemented();
    return makeUnique<PlatformTimeRanges>();
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateRemote::buffered() const
{
    notImplemented();
    return makeUnique<PlatformTimeRanges>();
}

MediaTime MediaPlayerPrivateRemote::maxMediaTimeSeekable() const
{
    notImplemented();
    return MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateRemote::minMediaTimeSeekable() const
{
    notImplemented();
    return MediaTime::zeroTime();
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

bool MediaPlayerPrivateRemote::didLoadingProgress() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateRemote::setSize(const IntSize&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::paint(GraphicsContext&, const FloatRect&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::paintCurrentFrameInContext(GraphicsContext&, const FloatRect&)
{
    notImplemented();
}

bool MediaPlayerPrivateRemote::copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject, GC3Denum, GC3Dint, GC3Denum, GC3Denum, GC3Denum, bool, bool)
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

bool MediaPlayerPrivateRemote::canLoadPoster() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateRemote::setPoster(const String&)
{
    notImplemented();
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

void MediaPlayerPrivateRemote::setWirelessVideoPlaybackDisabled(bool)
{
    notImplemented();
}

bool MediaPlayerPrivateRemote::canPlayToWirelessPlaybackTarget() const
{
    notImplemented();
    return false;
}

bool MediaPlayerPrivateRemote::isCurrentPlaybackTargetWireless() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateRemote::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::setShouldPlayToPlaybackTarget(bool)
{
    notImplemented();
}
#endif

#if USE(NATIVE_FULLSCREEN_VIDEO)
bool MediaPlayerPrivateRemote::canEnterFullscreen() const
{
    notImplemented();
    return false;
}
#endif

bool MediaPlayerPrivateRemote::supportsAcceleratedRendering() const
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateRemote::acceleratedRenderingStateChanged()
{
    notImplemented();
}

bool MediaPlayerPrivateRemote::shouldMaintainAspectRatio() const
{
    notImplemented();
    return true;
}

void MediaPlayerPrivateRemote::setShouldMaintainAspectRatio(bool)
{
    notImplemented();
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

MediaPlayer::MovieLoadType MediaPlayerPrivateRemote::movieLoadType() const
{
    notImplemented();
    return MediaPlayer::MovieLoadType::Unknown;
}

void MediaPlayerPrivateRemote::prepareForRendering()
{
    notImplemented();
}

MediaTime MediaPlayerPrivateRemote::mediaTimeForTimeValue(const MediaTime& timeValue) const
{
    notImplemented();
    return timeValue;
}

double MediaPlayerPrivateRemote::maximumDurationToCacheMediaTime() const
{
    notImplemented();
    return 0;
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

String MediaPlayerPrivateRemote::engineDescription() const
{
    notImplemented();
    return emptyString();
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
    notImplemented();
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
    notImplemented();
}

void MediaPlayerPrivateRemote::endSimulatedHDCPError()
{
    notImplemented();
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
    notImplemented();
}

void MediaPlayerPrivateRemote::setShouldDisableSleep(bool)
{
    notImplemented();
}

void MediaPlayerPrivateRemote::applicationWillResignActive()
{
    notImplemented();
}

void MediaPlayerPrivateRemote::applicationDidBecomeActive()
{
    notImplemented();
}

bool MediaPlayerPrivateRemote::performTaskAtMediaTime(WTF::Function<void()>&&, MediaTime)
{
    notImplemented();
    return false;
}

bool MediaPlayerPrivateRemote::shouldIgnoreIntrinsicSize()
{
    notImplemented();
    return false;
}

void MediaPlayerPrivateRemote::networkStateChanged(WebCore::MediaPlayerEnums::NetworkState state)
{
    m_networkState = state;
    m_player->networkStateChanged();
}

void MediaPlayerPrivateRemote::readyStateChanged(WebCore::MediaPlayerEnums::ReadyState state)
{
    m_readyState = state;
    m_player->networkStateChanged();
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

void MediaPlayerPrivateRemote::timeChanged(MediaTime time)
{
    // FIXME: should we cache the current time here for some amount of time?
    m_player->timeChanged();
}

void MediaPlayerPrivateRemote::durationChanged(MediaTime duration)
{
    m_duration = duration;
    m_player->durationChanged();
}

void MediaPlayerPrivateRemote::rateChanged(double rate)
{
    m_rate = rate;
    m_player->rateChanged();
}

void MediaPlayerPrivateRemote::playbackStateChanged(bool paused)
{
    m_paused = paused;
    m_player->playbackStateChanged();
}

void MediaPlayerPrivateRemote::engineFailedToLoad(long platformErrorCode)
{
    m_platformErrorCode = platformErrorCode;
    m_player->remoteEngineFailedToLoad();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaPlayerPrivateRemote::logChannel() const
{
    return WebKit2LogWebRTC;
}
#endif

} // namespace WebCore

#endif
