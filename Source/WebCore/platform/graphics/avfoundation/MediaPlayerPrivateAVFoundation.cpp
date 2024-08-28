/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#include "MediaPlayerPrivateAVFoundation.h"

#include "DocumentLoader.h"
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "InbandTextTrackPrivateAVF.h"
#include "InbandTextTrackPrivateClient.h"
#include "Logging.h"
#include "PlatformLayer.h"
#include "PlatformTimeRanges.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include <CoreMedia/CoreMedia.h>
#include <JavaScriptCore/DataView.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint16Array.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SoftLinking.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>

namespace WebCore {

MediaPlayerPrivateAVFoundation::MediaPlayerPrivateAVFoundation(MediaPlayer* player)
    : m_player(player)
    , m_networkState(MediaPlayer::NetworkState::Empty)
    , m_readyState(MediaPlayer::ReadyState::HaveNothing)
    , m_preload(MediaPlayer::Preload::Auto)
#if !RELEASE_LOG_DISABLED
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
#endif
    , m_cachedDuration(MediaTime::invalidTime())
    , m_maxTimeLoadedAtLastDidLoadingProgress(MediaTime::invalidTime())
    , m_delayCharacteristicsChangedNotification(0)
    , m_mainThreadCallPending(false)
    , m_assetIsPlayable(false)
    , m_visible(false)
    , m_loadingMetadata(false)
    , m_isAllowedToRender(false)
    , m_cachedHasAudio(false)
    , m_cachedHasVideo(false)
    , m_cachedHasCaptions(false)
    , m_ignoreLoadStateChanges(false)
    , m_haveReportedFirstVideoFrame(false)
    , m_characteristicsChanged(false)
    , m_shouldMaintainAspectRatio(true)
    , m_seeking(false)
{
    INFO_LOG(LOGIDENTIFIER);
}

MediaPlayerPrivateAVFoundation::~MediaPlayerPrivateAVFoundation()
{
    INFO_LOG(LOGIDENTIFIER);
    setIgnoreLoadStateChanges(true);
}

MediaPlayerPrivateAVFoundation::MediaRenderingMode MediaPlayerPrivateAVFoundation::currentRenderingMode() const
{
    if (platformLayer())
        return MediaRenderingMode::MediaRenderingToLayer;

    if (hasContextRenderer())
        return MediaRenderingMode::MediaRenderingToContext;

    return MediaRenderingMode::MediaRenderingNone;
}

MediaPlayerPrivateAVFoundation::MediaRenderingMode MediaPlayerPrivateAVFoundation::preferredRenderingMode() const
{
    if (assetStatus() == MediaPlayerAVAssetStatusUnknown)
        return MediaRenderingMode::MediaRenderingNone;

    if (m_readyState >= MediaPlayer::ReadyState::HaveMetadata && !haveBeenAskedToPaint())
        return MediaRenderingMode::MediaRenderingToLayer;

    RefPtr player = m_player.get();
    if (supportsAcceleratedRendering() && player && player->renderingCanBeAccelerated())
        return MediaRenderingMode::MediaRenderingToLayer;

    return MediaRenderingMode::MediaRenderingToContext;
}

void MediaPlayerPrivateAVFoundation::setUpVideoRendering()
{
    if (!isReadyForVideoSetup())
        return;

    MediaRenderingMode currentMode = currentRenderingMode();
    MediaRenderingMode preferredMode = preferredRenderingMode();

    if (currentMode == preferredMode && currentMode != MediaRenderingMode::MediaRenderingNone)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, preferredMode);

    switch (preferredMode) {
    case MediaRenderingMode::MediaRenderingNone:
        tearDownVideoRendering();
        break;

    case MediaRenderingMode::MediaRenderingToContext:
        destroyVideoLayer();
        createContextVideoRenderer();
        break;

    case MediaRenderingMode::MediaRenderingToLayer:
        destroyContextVideoRenderer();
        createVideoLayer();
        break;
    }

    // If using a movie layer, inform the client so the compositing tree is updated.
    if (currentMode == MediaRenderingMode::MediaRenderingToLayer || preferredMode == MediaRenderingMode::MediaRenderingToLayer)
        setNeedsRenderingModeChanged();
}

void MediaPlayerPrivateAVFoundation::setNeedsRenderingModeChanged()
{
    if (m_needsRenderingModeChanged)
        return;
    m_needsRenderingModeChanged = true;

    ALWAYS_LOG(LOGIDENTIFIER);

    queueTaskOnEventLoop([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->renderingModeChanged();
    });
}

void MediaPlayerPrivateAVFoundation::renderingModeChanged()
{
    ASSERT(m_needsRenderingModeChanged);
    m_needsRenderingModeChanged = false;
    if (RefPtr player = m_player.get())
        player->renderingModeChanged();
}

void MediaPlayerPrivateAVFoundation::tearDownVideoRendering()
{
    INFO_LOG(LOGIDENTIFIER);

    destroyContextVideoRenderer();

    if (platformLayer())
        destroyVideoLayer();
}

bool MediaPlayerPrivateAVFoundation::hasSetUpVideoRendering() const
{
    return hasLayerRenderer() || hasContextRenderer();
}

void MediaPlayerPrivateAVFoundation::load(const String& url)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    setNetworkState(m_preload == MediaPlayer::Preload::None ? MediaPlayer::NetworkState::Idle : MediaPlayer::NetworkState::Loading);
    setReadyState(MediaPlayer::ReadyState::HaveNothing);

    m_assetURL = URL({ }, url);
    m_requestedOrigin = SecurityOrigin::create(m_assetURL);

    // Don't do any more work if the url is empty.
    if (!url.length())
        return;

    setPreload(m_preload);
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateAVFoundation::load(const URL&, const ContentType&, MediaSourcePrivateClient&)
{
    setNetworkState(MediaPlayer::NetworkState::FormatError);
}
#endif


void MediaPlayerPrivateAVFoundation::playabilityKnown()
{
    INFO_LOG(LOGIDENTIFIER, "metadata loaded = ", assetStatus() > MediaPlayerAVAssetStatusLoading);

    if (m_assetIsPlayable)
        return;

    // Nothing more to do if we already have all of the item's metadata.
    if (assetStatus() > MediaPlayerAVAssetStatusLoading)
        return;

    // At this point we are supposed to load metadata. It is OK to ask the asset to load the same 
    // information multiple times, because if it has already been loaded the completion handler 
    // will just be called synchronously.
    m_loadingMetadata = true;
    beginLoadingMetadata();
}

void MediaPlayerPrivateAVFoundation::prepareToPlay()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    setPreload(MediaPlayer::Preload::Auto);
}

void MediaPlayerPrivateAVFoundation::play()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    platformPlay();
}

void MediaPlayerPrivateAVFoundation::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    platformPause();
}

MediaTime MediaPlayerPrivateAVFoundation::duration() const
{
    if (m_cachedDuration.isValid())
        return m_cachedDuration;

    MediaTime duration = platformDuration();
    if (!duration || duration.isInvalid())
        return MediaTime::zeroTime();

    m_cachedDuration = duration;

    return m_cachedDuration;
}

void MediaPlayerPrivateAVFoundation::seekToTarget(const SeekTarget& target)
{
    if (m_seeking) {
        ALWAYS_LOG(LOGIDENTIFIER, "saving pending seek");
        m_pendingSeek = [this, target]() {
            seekToTarget(target);
        };
        return;
    }
    m_seeking = true;

    if (!metaDataAvailable())
        return;

    SeekTarget adjustedTarget = target;
    if (target.time > duration())
        adjustedTarget.time = duration();

    if (currentTextTrack())
        currentTextTrack()->beginSeeking();

    ALWAYS_LOG(LOGIDENTIFIER, "seeking to ", adjustedTarget.time);

    m_lastSeekTime = adjustedTarget.time;
    seekToTargetInternal(adjustedTarget);
}

bool MediaPlayerPrivateAVFoundation::paused() const
{
    if (!metaDataAvailable())
        return true;

    return platformPaused();
}

bool MediaPlayerPrivateAVFoundation::seeking() const
{
    if (!metaDataAvailable())
        return false;

    return m_seeking;
}

FloatSize MediaPlayerPrivateAVFoundation::naturalSize() const
{
    if (!metaDataAvailable())
        return IntSize();

    // In spite of the name of this method, return the natural size transformed by the 
    // initial movie scale because the spec says intrinsic size is:
    //
    //    ... the dimensions of the resource in CSS pixels after taking into account the resource's 
    //    dimensions, aspect ratio, clean aperture, resolution, and so forth, as defined for the 
    //    format used by the resource

    return m_cachedNaturalSize;
}

void MediaPlayerPrivateAVFoundation::setNaturalSize(FloatSize size)
{
    FloatSize oldSize = m_cachedNaturalSize;
    m_cachedNaturalSize = size;
    if (oldSize != m_cachedNaturalSize) {
        INFO_LOG(LOGIDENTIFIER, "was ", oldSize.width(), " x ", oldSize.height(), ", is ", size.width(), " x ", size.height());
        if (RefPtr player = m_player.get())
            player->sizeChanged();
    }
}

void MediaPlayerPrivateAVFoundation::setHasVideo(bool b)
{
    if (m_cachedHasVideo != b) {
        m_cachedHasVideo = b;
        characteristicsChanged();
    }
}

void MediaPlayerPrivateAVFoundation::setHasAudio(bool b)
{
    if (m_cachedHasAudio != b) {
        m_cachedHasAudio = b;
        characteristicsChanged();
    }
}

void MediaPlayerPrivateAVFoundation::setHasClosedCaptions(bool b)
{
    if (m_cachedHasCaptions != b) {
        m_cachedHasCaptions = b;
        characteristicsChanged();
    }
}

void MediaPlayerPrivateAVFoundation::setNetworkState(MediaPlayer::NetworkState state)
{
    if (state == m_networkState)
        return;

    m_networkState = state;
    if (RefPtr player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateAVFoundation::setReadyState(MediaPlayer::ReadyState state)
{
    if (state == m_readyState)
        return;

    auto oldState = std::exchange(m_readyState, state);
    RefPtr player = m_player.get();
    if (player)
        player->readyStateChanged();

    if (oldState >= MediaPlayer::ReadyState::HaveMetadata)
        return;

    // Many state methods in MediaPlayerPrivateAVFoundation will return defaults
    // if queried before reaching HaveMetadata. Re-fire their changed events after
    // the ready state moves beyond HaveMetadata so the correct values are reflected
    // upwards to clients.
    if (!m_cachedNaturalSize.isEmpty() && player)
        player->sizeChanged();
}

void MediaPlayerPrivateAVFoundation::characteristicsChanged()
{
    if (m_delayCharacteristicsChangedNotification) {
        m_characteristicsChanged = true;
        return;
    }

    m_characteristicsChanged = false;
    if (RefPtr player = m_player.get())
        player->characteristicChanged();
}

void MediaPlayerPrivateAVFoundation::setDelayCharacteristicsChangedNotification(bool delay)
{
    if (delay) {
        m_delayCharacteristicsChangedNotification++;
        return;
    }
    
    ASSERT(m_delayCharacteristicsChangedNotification);
    m_delayCharacteristicsChangedNotification--;
    if (!m_delayCharacteristicsChangedNotification && m_characteristicsChanged)
        characteristicsChanged();
}

const PlatformTimeRanges& MediaPlayerPrivateAVFoundation::buffered() const
{
    return platformBufferedTimeRanges();
}

MediaTime MediaPlayerPrivateAVFoundation::maxTimeSeekable() const
{
    if (!metaDataAvailable())
        return MediaTime::zeroTime();

    if (!m_cachedMaxTimeSeekable)
        m_cachedMaxTimeSeekable = platformMaxTimeSeekable();

    return m_cachedMaxTimeSeekable;
}

MediaTime MediaPlayerPrivateAVFoundation::minTimeSeekable() const
{
    if (!metaDataAvailable())
        return MediaTime::zeroTime();

    if (!m_cachedMinTimeSeekable)
        m_cachedMinTimeSeekable = platformMinTimeSeekable();

    return m_cachedMinTimeSeekable;
}

MediaTime MediaPlayerPrivateAVFoundation::maxTimeLoaded() const
{
    if (!metaDataAvailable())
        return MediaTime::zeroTime();

    if (!m_cachedMaxTimeLoaded)
        m_cachedMaxTimeLoaded = platformMaxTimeLoaded();

    return m_cachedMaxTimeLoaded;   
}

bool MediaPlayerPrivateAVFoundation::didLoadingProgress() const
{
    if (!duration())
        return false;
    MediaTime currentMaxTimeLoaded = maxTimeLoaded();
    bool didLoadingProgress = currentMaxTimeLoaded != m_maxTimeLoadedAtLastDidLoadingProgress;
    m_maxTimeLoadedAtLastDidLoadingProgress = currentMaxTimeLoaded;

    return didLoadingProgress;
}

bool MediaPlayerPrivateAVFoundation::isReadyForVideoSetup() const
{
    // AVFoundation will not return true for firstVideoFrameAvailable until
    // an AVPlayerLayer has been added to the AVPlayerItem, so allow video setup
    // here if a video track to trigger allocation of a AVPlayerLayer.
    return (m_isAllowedToRender || m_cachedHasVideo) && m_readyState >= MediaPlayer::ReadyState::HaveMetadata && m_visible;
}

void MediaPlayerPrivateAVFoundation::prepareForRendering()
{
    if (m_isAllowedToRender)
        return;
    m_isAllowedToRender = true;

    setUpVideoRendering();

    if (currentRenderingMode() == MediaRenderingMode::MediaRenderingToLayer || preferredRenderingMode() == MediaRenderingMode::MediaRenderingToLayer)
        setNeedsRenderingModeChanged();
}

bool MediaPlayerPrivateAVFoundation::supportsFullscreen() const
{
    // FIXME: WebVideoFullscreenController assumes a QTKit/QuickTime media engine
#if ENABLE(FULLSCREEN_API) || (PLATFORM(IOS_FAMILY) && HAVE(AVKIT))
    return true;
#else
    return false;
#endif
}

void MediaPlayerPrivateAVFoundation::setResolvedURL(URL&& resolvedURL)
{
    m_resolvedURL = WTFMove(resolvedURL);
    m_resolvedOrigin = SecurityOrigin::create(m_resolvedURL);
}

void MediaPlayerPrivateAVFoundation::updateStates()
{
    if (m_ignoreLoadStateChanges)
        return;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    MediaPlayer::NetworkState newNetworkState = m_networkState;
    MediaPlayer::ReadyState newReadyState = m_readyState;
    bool firstVideoFrameBecomeAvailable = false;

    if (m_loadingMetadata)
        newNetworkState = MediaPlayer::NetworkState::Loading;
    else {
        // -loadValuesAsynchronouslyForKeys:completionHandler: has invoked its handler; test status of keys and determine state.
        AssetStatus assetStatus = this->assetStatus();
        ItemStatus itemStatus = playerItemStatus();

        m_assetIsPlayable = (assetStatus == MediaPlayerAVAssetStatusPlayable);
        if (m_readyState < MediaPlayer::ReadyState::HaveMetadata && assetStatus > MediaPlayerAVAssetStatusLoading) {
            if (m_assetIsPlayable) {
                if (assetStatus >= MediaPlayerAVAssetStatusLoaded)
                    newReadyState = MediaPlayer::ReadyState::HaveMetadata;
                if (itemStatus <= MediaPlayerAVPlayerItemStatusUnknown) {
                    if (assetStatus == MediaPlayerAVAssetStatusFailed || m_preload > MediaPlayer::Preload::MetaData || isLiveStream()) {
                        // The asset is playable but doesn't support inspection prior to playback (eg. streaming files),
                        // or we are supposed to prepare for playback immediately, so create the player item now.
                        newNetworkState = MediaPlayer::NetworkState::Loading;
                        prepareToPlay();
                    } else
                        newNetworkState = MediaPlayer::NetworkState::Idle;
                }
            } else
                newNetworkState = assetStatus == MediaPlayerAVAssetStatusNetworkError ? MediaPlayer::NetworkState::NetworkError : MediaPlayer::NetworkState::FormatError;
        }

        if (!hasAvailableVideoFrame())
            m_haveReportedFirstVideoFrame = false;
        else if (!m_haveReportedFirstVideoFrame && m_cachedHasVideo) {
            m_haveReportedFirstVideoFrame = true;
            firstVideoFrameBecomeAvailable = true;
        }

        if (assetStatus >= MediaPlayerAVAssetStatusLoaded && itemStatus > MediaPlayerAVPlayerItemStatusUnknown) {
            switch (itemStatus) {
            case MediaPlayerAVPlayerItemStatusDoesNotExist:
            case MediaPlayerAVPlayerItemStatusUnknown:
            case MediaPlayerAVPlayerItemStatusFailed:
                break;

            case MediaPlayerAVPlayerItemStatusPlaybackLikelyToKeepUp:
            case MediaPlayerAVPlayerItemStatusPlaybackBufferFull:
                // If the status becomes PlaybackBufferFull, loading stops and the status will not
                // progress to LikelyToKeepUp. Set the readyState to HAVE_ENOUGH_DATA, on the
                // presumption that if the playback buffer is full, playback will probably not stall.
                newReadyState = MediaPlayer::ReadyState::HaveEnoughData;
                break;

            case MediaPlayerAVPlayerItemStatusReadyToPlay:
                if (m_readyState != MediaPlayer::ReadyState::HaveEnoughData && (!m_cachedHasVideo || m_haveReportedFirstVideoFrame) && maxTimeLoaded() > currentTime())
                    newReadyState = MediaPlayer::ReadyState::HaveFutureData;
                break;

            case MediaPlayerAVPlayerItemStatusPlaybackBufferEmpty:
                newReadyState = MediaPlayer::ReadyState::HaveCurrentData;
                break;
            }

            if (itemStatus == MediaPlayerAVPlayerItemStatusPlaybackBufferFull)
                newNetworkState = MediaPlayer::NetworkState::Idle;
            else if (itemStatus == MediaPlayerAVPlayerItemStatusFailed)
                newNetworkState = MediaPlayer::NetworkState::DecodeError;
            else if (itemStatus != MediaPlayerAVPlayerItemStatusPlaybackBufferFull && itemStatus >= MediaPlayerAVPlayerItemStatusReadyToPlay)
                newNetworkState = (maxTimeLoaded() >= duration()) ? MediaPlayer::NetworkState::Loaded : MediaPlayer::NetworkState::Loading;
        }
    }

    if (firstVideoFrameBecomeAvailable) {
        if (m_readyState < MediaPlayer::ReadyState::HaveCurrentData)
            newReadyState = MediaPlayer::ReadyState::HaveCurrentData;
        if (RefPtr player = m_player.get())
            player->firstVideoFrameAvailable();
    }

    if (m_networkState != newNetworkState)
        ALWAYS_LOG(LOGIDENTIFIER, "entered with networkState ", m_networkState, ", exiting with ", newNetworkState);
    if (m_readyState != newReadyState)
        ALWAYS_LOG(LOGIDENTIFIER, "entered with readyState ", m_readyState, ", exiting with ", newReadyState);

    setNetworkState(newNetworkState);
    setReadyState(newReadyState);

    if (isReadyForVideoSetup() && currentRenderingMode() != preferredRenderingMode())
        setUpVideoRendering();
}

void MediaPlayerPrivateAVFoundation::setPageIsVisible(bool visible)
{
    if (m_visible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);

    m_visible = visible;
    if (visible)
        setUpVideoRendering();

    platformSetVisible(visible);
}

void MediaPlayerPrivateAVFoundation::acceleratedRenderingStateChanged()
{
    // Set up or change the rendering path if necessary.
    setUpVideoRendering();
}

void MediaPlayerPrivateAVFoundation::setShouldMaintainAspectRatio(bool maintainAspectRatio)
{
    if (maintainAspectRatio == m_shouldMaintainAspectRatio)
        return;

    m_shouldMaintainAspectRatio = maintainAspectRatio;
    updateVideoLayerGravity();
}

void MediaPlayerPrivateAVFoundation::metadataLoaded()
{
    m_loadingMetadata = false;
    resolvedURLChanged();
    tracksChanged();
}

void MediaPlayerPrivateAVFoundation::rateChanged()
{
    if (RefPtr player = m_player.get())
        player->rateChanged();
}

void MediaPlayerPrivateAVFoundation::loadedTimeRangesChanged()
{
    m_cachedMaxTimeLoaded = MediaTime::zeroTime();
    invalidateCachedDuration();
    if (RefPtr player = m_player.get())
        player->bufferedTimeRangesChanged();
}

void MediaPlayerPrivateAVFoundation::seekableTimeRangesChanged()
{
    m_cachedMaxTimeSeekable = MediaTime::zeroTime();
    m_cachedMinTimeSeekable = MediaTime::zeroTime();
    if (RefPtr player = m_player.get())
        player->seekableTimeRangesChanged();
}

void MediaPlayerPrivateAVFoundation::timeChanged(const MediaTime& time)
{
    UNUSED_PARAM(time);
    INFO_LOG(LOGIDENTIFIER, "- ", time);
}

void MediaPlayerPrivateAVFoundation::seekCompleted(bool finished)
{
    UNUSED_PARAM(finished);
    ALWAYS_LOG(LOGIDENTIFIER, "finished = ", finished);

    m_seeking = false;

    Function<void()> pendingSeek;
    std::swap(pendingSeek, m_pendingSeek);

    if (pendingSeek) {
        ALWAYS_LOG(LOGIDENTIFIER, "issuing pending seek");
        pendingSeek();
        return;
    }

    if (!finished)
        return;

    if (currentTextTrack())
        currentTextTrack()->endSeeking();

    updateStates();

    if (RefPtr player = m_player.get()) {
        player->seeked(m_lastSeekTime);
        player->timeChanged();
    }
}

void MediaPlayerPrivateAVFoundation::didEnd()
{
    // Hang onto the current time and use it as duration from now on since we are definitely at
    // the end of the movie. Do this because the initial duration is sometimes an estimate.
    MediaTime now = currentTime();
    ALWAYS_LOG(LOGIDENTIFIER, "currentTime: ", now, ", seeking: ", m_seeking);
    if (now > MediaTime::zeroTime() && !m_seeking)
        m_cachedDuration = now;

    updateStates();
    if (RefPtr player = m_player.get())
        player->timeChanged();
}

void MediaPlayerPrivateAVFoundation::invalidateCachedDuration()
{
    m_cachedDuration = MediaTime::invalidTime();
}

MediaPlayer::MovieLoadType MediaPlayerPrivateAVFoundation::movieLoadType() const
{
    if (!metaDataAvailable() || assetStatus() == MediaPlayerAVAssetStatusUnknown)
        return MediaPlayer::MovieLoadType::Unknown;

    if (isHLS())
        return MediaPlayer::MovieLoadType::HttpLiveStream;

    return MediaPlayer::MovieLoadType::Download;
}

void MediaPlayerPrivateAVFoundation::setPreload(MediaPlayer::Preload preload)
{
    ALWAYS_LOG(LOGIDENTIFIER, " - ", static_cast<int>(preload));
    m_preload = preload;
    if (m_assetURL.isEmpty())
        return;

    if (m_preload >= MediaPlayer::Preload::MetaData && assetStatus() == MediaPlayerAVAssetStatusDoesNotExist)
        createAVAssetForURL(m_assetURL);

    // Don't force creation of the player and player item unless we already know that the asset is playable. If we aren't
    // there yet, or if we already know it is not playable, creating them now won't help.
    if (m_preload == MediaPlayer::Preload::Auto && m_assetIsPlayable) {
        createAVPlayerItem();
        createAVPlayer();
    }
}

void MediaPlayerPrivateAVFoundation::configureInbandTracks()
{
    RefPtr<InbandTextTrackPrivateAVF> trackToEnable;
    
    synchronizeTextTrackState();

    // AVFoundation can only emit cues for one track at a time, so enable the first track that is showing, or the first that
    // is hidden if none are showing. Otherwise disable all tracks.
    for (unsigned i = 0; i < m_textTracks.size(); ++i) {
        RefPtr<InbandTextTrackPrivateAVF> track = m_textTracks[i];
        if (track->mode() == InbandTextTrackPrivate::Mode::Showing) {
            trackToEnable = track;
            break;
        }
        if (track->mode() == InbandTextTrackPrivate::Mode::Hidden)
            trackToEnable = track;
    }

    setCurrentTextTrack(trackToEnable.get());
}

void MediaPlayerPrivateAVFoundation::trackModeChanged()
{
    configureInbandTracks();
}

void MediaPlayerPrivateAVFoundation::clearTextTracks()
{
    auto player = this->player();
    for (auto& track : m_textTracks) {
        if (player)
            player->removeTextTrack(*track);
        track->disconnect();
    }
    m_textTracks.clear();
}

void MediaPlayerPrivateAVFoundation::processNewAndRemovedTextTracks(const Vector<RefPtr<InbandTextTrackPrivateAVF>>& removedTextTracks)
{
    auto player = this->player();
    if (removedTextTracks.size()) {
        for (unsigned i = 0; i < m_textTracks.size(); ) {
            if (!removedTextTracks.contains(m_textTracks[i])) {
                ++i;
                continue;
            }
            if (player)
                player->removeTextTrack(*m_textTracks[i]);
            m_textTracks.remove(i);
        }
    }

    unsigned trackCount = m_textTracks.size();
    unsigned inBandCount = 0;
    for (unsigned i = 0; i < trackCount; ++i) {
        RefPtr<InbandTextTrackPrivateAVF> track = m_textTracks[i];

        if (track->textTrackCategory() == InbandTextTrackPrivateAVF::OutOfBand)
            continue;

        track->setTextTrackIndex(inBandCount);
        ++inBandCount;
        if (track->hasBeenReported())
            continue;

        track->setHasBeenReported(true);
        if (player)
            player->addTextTrack(*track);
    }

    if (trackCount != m_textTracks.size())
        INFO_LOG(LOGIDENTIFIER, "found ", m_textTracks.size(), " text tracks");
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaPlayerPrivateAVFoundation::playbackTargetIsWirelessChanged()
{
    if (RefPtr player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(player->isCurrentPlaybackTargetWireless());
}
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
bool MediaPlayerPrivateAVFoundation::extractKeyURIKeyIDAndCertificateFromInitData(Uint8Array* initData, String& keyURI, String& keyID, RefPtr<Uint8Array>& certificate)
{
    // initData should have the following layout:
    // [4 bytes: keyURI length][N bytes: keyURI][4 bytes: contentID length], [N bytes: contentID], [4 bytes: certificate length][N bytes: certificate]
    if (initData->byteLength() < 4)
        return false;

    RefPtr<ArrayBuffer> initDataBuffer = initData->unsharedBuffer();

    // Use a DataView to read uint32 values from the buffer, as Uint32Array requires the reads be aligned on 4-byte boundaries. 
    auto initDataView = JSC::DataView::create(initDataBuffer.copyRef(), 0, initDataBuffer->byteLength());
    uint32_t offset = 0;
    bool status = true;

    uint32_t keyURILength = initDataView->get<uint32_t>(offset, true, &status);
    offset += 4;
    if (!status || offset + keyURILength > initData->length())
        return false;

    auto keyURIArray = Uint16Array::tryCreate(initDataBuffer.copyRef(), offset, keyURILength);
    if (!keyURIArray)
        return false;

    keyURI = spanReinterpretCast<const UChar>(keyURIArray->span().first(keyURILength));
    offset += keyURILength;

    uint32_t keyIDLength = initDataView->get<uint32_t>(offset, true, &status);
    offset += 4;
    if (!status || offset + keyIDLength > initData->length())
        return false;

    auto keyIDArray = Uint8Array::tryCreate(initDataBuffer.copyRef(), offset, keyIDLength);
    if (!keyIDArray)
        return false;

    keyID = spanReinterpretCast<const UChar>(keyIDArray->span().first(keyIDLength));
    offset += keyIDLength;

    uint32_t certificateLength = initDataView->get<uint32_t>(offset, true, &status);
    offset += 4;
    if (!status || offset + certificateLength > initData->length())
        return false;

    certificate = Uint8Array::tryCreate(WTFMove(initDataBuffer), offset, certificateLength);
    if (!certificate)
        return false;

    return true;
}
#endif

bool MediaPlayerPrivateAVFoundation::canSaveMediaData() const
{
    URL url = resolvedURL();

    if (url.protocolIsFile())
        return true;

    if (!url.protocolIsInHTTPFamily())
        return false;

    if (isLiveStream())
        return false;

    return true;
}

bool MediaPlayerPrivateAVFoundation::shouldEnableInheritURIQueryComponent() const
{
    static NeverDestroyed<const AtomString> iTunesInheritsURIQueryComponent(MAKE_STATIC_STRING_IMPL("x-itunes-inherit-uri-query-component"));
    auto player = this->player();
    return player && player->doesHaveAttribute(iTunesInheritsURIQueryComponent);
}

void MediaPlayerPrivateAVFoundation::queueTaskOnEventLoop(Function<void()>&& task)
{
    ASSERT(isMainThread());
    if (RefPtr player = m_player.get())
        player->queueTaskOnEventLoop(WTFMove(task));
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaPlayerPrivateAVFoundation::logChannel() const
{
    return LogMedia;
}
#endif


String convertEnumerationToString(MediaPlayerPrivateAVFoundation::MediaRenderingMode enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("MediaRenderingNone"),
        MAKE_STATIC_STRING_IMPL("MediaRenderingToContext"),
        MAKE_STATIC_STRING_IMPL("MediaRenderingToLayer"),
    };
    static_assert(static_cast<size_t>(MediaPlayerPrivateAVFoundation::MediaRenderingMode::MediaRenderingNone) == 0, "MediaRenderingMode::MediaRenderingNone is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayerPrivateAVFoundation::MediaRenderingMode::MediaRenderingToContext) == 1, "MediaRenderingMode::MediaRenderingToContext is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayerPrivateAVFoundation::MediaRenderingMode::MediaRenderingToLayer) == 2, "MediaRenderingMode::MediaRenderingToLayer is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

} // namespace WebCore

#endif
