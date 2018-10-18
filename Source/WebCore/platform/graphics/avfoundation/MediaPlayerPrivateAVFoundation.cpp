/*
 * Copyright (C) 2011-2014 Apple Inc. All rights reserved.
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

#include "DeprecatedGlobalSettings.h"
#include "DocumentLoader.h"
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "InbandTextTrackPrivateAVF.h"
#include "InbandTextTrackPrivateClient.h"
#include "Logging.h"
#include "PlatformLayer.h"
#include "PlatformTimeRanges.h"
#include "Settings.h"
#include "URL.h"
#include <CoreMedia/CoreMedia.h>
#include <JavaScriptCore/DataView.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint16Array.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SoftLinking.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/CString.h>

namespace WebCore {

MediaPlayerPrivateAVFoundation::MediaPlayerPrivateAVFoundation(MediaPlayer* player)
    : m_player(player)
    , m_queuedNotifications()
    , m_queueMutex()
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_preload(MediaPlayer::Auto)
#if !RELEASE_LOG_DISABLED
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
#endif
    , m_cachedDuration(MediaTime::invalidTime())
    , m_reportedDuration(MediaTime::invalidTime())
    , m_maxTimeLoadedAtLastDidLoadingProgress(MediaTime::invalidTime())
    , m_delayCallbacks(0)
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
    , m_playWhenFramesAvailable(false)
    , m_inbandTrackConfigurationPending(false)
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
        return MediaRenderingToLayer;

    if (hasContextRenderer())
        return MediaRenderingToContext;

    return MediaRenderingNone;
}

MediaPlayerPrivateAVFoundation::MediaRenderingMode MediaPlayerPrivateAVFoundation::preferredRenderingMode() const
{
    if (!m_player->visible() || assetStatus() == MediaPlayerAVAssetStatusUnknown)
        return MediaRenderingNone;

    if (supportsAcceleratedRendering() && m_player->client().mediaPlayerRenderingCanBeAccelerated(m_player))
        return MediaRenderingToLayer;

    return MediaRenderingToContext;
}

void MediaPlayerPrivateAVFoundation::setUpVideoRendering()
{
    if (!isReadyForVideoSetup())
        return;

    MediaRenderingMode currentMode = currentRenderingMode();
    MediaRenderingMode preferredMode = preferredRenderingMode();

    if (preferredMode == MediaRenderingNone)
        preferredMode = MediaRenderingToContext;

    if (currentMode == preferredMode && currentMode != MediaRenderingNone)
        return;

    if (currentMode != MediaRenderingNone)
        tearDownVideoRendering();

    switch (preferredMode) {
    case MediaRenderingNone:
    case MediaRenderingToContext:
        createContextVideoRenderer();
        break;

    case MediaRenderingToLayer:
        createVideoLayer();
        break;
    }

    // If using a movie layer, inform the client so the compositing tree is updated.
    if (currentMode == MediaRenderingToLayer || preferredMode == MediaRenderingToLayer)
        m_player->client().mediaPlayerRenderingModeChanged(m_player);
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

    setNetworkState(m_preload == MediaPlayer::None ? MediaPlayer::Idle : MediaPlayer::Loading);
    setReadyState(MediaPlayer::HaveNothing);

    m_assetURL = URL({ }, url);
    m_requestedOrigin = SecurityOrigin::create(m_assetURL);

    // Don't do any more work if the url is empty.
    if (!url.length())
        return;

    setPreload(m_preload);
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateAVFoundation::load(const String&, MediaSourcePrivateClient*)
{
    setNetworkState(MediaPlayer::FormatError);
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
    setPreload(MediaPlayer::Auto);
}

void MediaPlayerPrivateAVFoundation::play()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // If the file has video, don't request playback until the first frame of video is ready to display
    // or the audio may start playing before we can render video.
    if (!m_cachedHasVideo || hasAvailableVideoFrame())
        platformPlay();
    else {
        INFO_LOG(LOGIDENTIFIER, "waiting for first video frame");
        m_playWhenFramesAvailable = true;
    }
}

void MediaPlayerPrivateAVFoundation::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_playWhenFramesAvailable = false;
    platformPause();
}

MediaTime MediaPlayerPrivateAVFoundation::durationMediaTime() const
{
    if (m_cachedDuration.isValid())
        return m_cachedDuration;

    MediaTime duration = platformDuration();
    if (!duration || duration.isInvalid())
        return MediaTime::zeroTime();

    m_cachedDuration = duration;

    return m_cachedDuration;
}

void MediaPlayerPrivateAVFoundation::seek(const MediaTime& time)
{
    seekWithTolerance(time, MediaTime::zeroTime(), MediaTime::zeroTime());
}

void MediaPlayerPrivateAVFoundation::seekWithTolerance(const MediaTime& mediaTime, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
{
    MediaTime time = mediaTime;

    if (m_seeking) {
        ALWAYS_LOG(LOGIDENTIFIER, "saving pending seek");
        m_pendingSeek = [this, time, negativeTolerance, positiveTolerance]() {
            seekWithTolerance(time, negativeTolerance, positiveTolerance);
        };
        return;
    }
    m_seeking = true;

    if (!metaDataAvailable())
        return;

    if (time > durationMediaTime())
        time = durationMediaTime();

    if (currentTextTrack())
        currentTextTrack()->beginSeeking();

    ALWAYS_LOG(LOGIDENTIFIER, "seeking to  ", time);

    seekToTime(time, negativeTolerance, positiveTolerance);
}

bool MediaPlayerPrivateAVFoundation::paused() const
{
    if (!metaDataAvailable())
        return true;

    return rate() == 0;
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
        m_player->sizeChanged();
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
    m_player->networkStateChanged();
}

void MediaPlayerPrivateAVFoundation::setReadyState(MediaPlayer::ReadyState state)
{
    if (state == m_readyState)
        return;

    m_readyState = state;
    m_player->readyStateChanged();
}

void MediaPlayerPrivateAVFoundation::characteristicsChanged()
{
    if (m_delayCharacteristicsChangedNotification) {
        m_characteristicsChanged = true;
        return;
    }

    m_characteristicsChanged = false;
    m_player->characteristicChanged();
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

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateAVFoundation::buffered() const
{
    if (!m_cachedLoadedTimeRanges)
        m_cachedLoadedTimeRanges = platformBufferedTimeRanges();

    return std::make_unique<PlatformTimeRanges>(*m_cachedLoadedTimeRanges);
}

MediaTime MediaPlayerPrivateAVFoundation::maxMediaTimeSeekable() const
{
    if (!metaDataAvailable())
        return MediaTime::zeroTime();

    if (!m_cachedMaxTimeSeekable)
        m_cachedMaxTimeSeekable = platformMaxTimeSeekable();

    return m_cachedMaxTimeSeekable;
}

MediaTime MediaPlayerPrivateAVFoundation::minMediaTimeSeekable() const
{
    if (!metaDataAvailable())
        return MediaTime::zeroTime();

    if (!m_cachedMinTimeSeekable)
        m_cachedMinTimeSeekable = platformMinTimeSeekable();

    return m_cachedMinTimeSeekable;
}

double MediaPlayerPrivateAVFoundation::requestedRate() const
{
    return m_player->requestedRate();
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
    if (!durationMediaTime())
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
    return (m_isAllowedToRender || m_cachedHasVideo) && m_readyState >= MediaPlayer::HaveMetadata && m_player->visible();
}

void MediaPlayerPrivateAVFoundation::prepareForRendering()
{
    if (m_isAllowedToRender)
        return;
    m_isAllowedToRender = true;

    setUpVideoRendering();

    if (currentRenderingMode() == MediaRenderingToLayer || preferredRenderingMode() == MediaRenderingToLayer)
        m_player->client().mediaPlayerRenderingModeChanged(m_player);
}

bool MediaPlayerPrivateAVFoundation::supportsFullscreen() const
{
#if ENABLE(FULLSCREEN_API)
    return true;
#else
    // FIXME: WebVideoFullscreenController assumes a QTKit/QuickTime media engine
#if PLATFORM(IOS_FAMILY)
    if (DeprecatedGlobalSettings::avKitEnabled())
        return true;
#endif
    return false;
#endif
}

bool MediaPlayerPrivateAVFoundation::hasSingleSecurityOrigin() const
{
    if (m_resolvedOrigin && m_requestedOrigin)
        return m_resolvedOrigin->isSameSchemeHostPort(*m_requestedOrigin);
    return false;
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

    MediaPlayer::NetworkState newNetworkState = m_networkState;
    MediaPlayer::ReadyState newReadyState = m_readyState;

    if (m_loadingMetadata)
        newNetworkState = MediaPlayer::Loading;
    else {
        // -loadValuesAsynchronouslyForKeys:completionHandler: has invoked its handler; test status of keys and determine state.
        AssetStatus assetStatus = this->assetStatus();
        ItemStatus itemStatus = playerItemStatus();
        
        m_assetIsPlayable = (assetStatus == MediaPlayerAVAssetStatusPlayable);
        if (m_readyState < MediaPlayer::HaveMetadata && assetStatus > MediaPlayerAVAssetStatusLoading) {
            if (m_assetIsPlayable) {
                if (assetStatus >= MediaPlayerAVAssetStatusLoaded)
                    newReadyState = MediaPlayer::HaveMetadata;
                if (itemStatus <= MediaPlayerAVPlayerItemStatusUnknown) {
                    if (assetStatus == MediaPlayerAVAssetStatusFailed || m_preload > MediaPlayer::MetaData || isLiveStream()) {
                        // The asset is playable but doesn't support inspection prior to playback (eg. streaming files),
                        // or we are supposed to prepare for playback immediately, so create the player item now.
                        newNetworkState = MediaPlayer::Loading;
                        prepareToPlay();
                    } else
                        newNetworkState = MediaPlayer::Idle;
                }
            } else {
                // FIX ME: fetch the error associated with the @"playable" key to distinguish between format 
                // and network errors.
                newNetworkState = MediaPlayer::FormatError;
            }
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
                // progress to LikelyToKeepUp. Set the readyState to  HAVE_ENOUGH_DATA, on the
                // presumption that if the playback buffer is full, playback will probably not stall.
                newReadyState = MediaPlayer::HaveEnoughData;
                break;

            case MediaPlayerAVPlayerItemStatusReadyToPlay:
                if (m_readyState != MediaPlayer::HaveEnoughData && maxTimeLoaded() > currentMediaTime())
                    newReadyState = MediaPlayer::HaveFutureData;
                break;

            case MediaPlayerAVPlayerItemStatusPlaybackBufferEmpty:
                newReadyState = MediaPlayer::HaveCurrentData;
                break;
            }

            if (itemStatus == MediaPlayerAVPlayerItemStatusPlaybackBufferFull)
                newNetworkState = MediaPlayer::Idle;
            else if (itemStatus == MediaPlayerAVPlayerItemStatusFailed)
                newNetworkState = MediaPlayer::DecodeError;
            else if (itemStatus != MediaPlayerAVPlayerItemStatusPlaybackBufferFull && itemStatus >= MediaPlayerAVPlayerItemStatusReadyToPlay)
                newNetworkState = (maxTimeLoaded() == durationMediaTime()) ? MediaPlayer::Loaded : MediaPlayer::Loading;
        }
    }

    if (isReadyForVideoSetup() && currentRenderingMode() != preferredRenderingMode())
        setUpVideoRendering();

    if (!m_haveReportedFirstVideoFrame && m_cachedHasVideo && hasAvailableVideoFrame()) {
        if (m_readyState < MediaPlayer::HaveCurrentData)
            newReadyState = MediaPlayer::HaveCurrentData;
        m_haveReportedFirstVideoFrame = true;
        m_player->firstVideoFrameAvailable();
    } else if (!hasAvailableVideoFrame())
        m_haveReportedFirstVideoFrame = false;

    if (m_networkState != newNetworkState)
        ALWAYS_LOG(LOGIDENTIFIER, "entered with networkState ", m_networkState, ", exiting with ", newNetworkState);
    if (m_readyState != newReadyState)
        ALWAYS_LOG(LOGIDENTIFIER, "entered with readyState ", m_readyState, ", exiting with ", newReadyState);

    setNetworkState(newNetworkState);
    setReadyState(newReadyState);

    if (m_playWhenFramesAvailable && hasAvailableVideoFrame()) {
        m_playWhenFramesAvailable = false;
        platformPlay();
    }
}

void MediaPlayerPrivateAVFoundation::setSize(const IntSize&) 
{ 
}

void MediaPlayerPrivateAVFoundation::setVisible(bool visible)
{
    if (m_visible == visible)
        return;

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

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && PLATFORM(IOS_FAMILY)
    if (isCurrentPlaybackTargetWireless() && playerItemStatus() >= MediaPlayerAVPlayerItemStatusPlaybackBufferFull) {
        double rate = this->rate();
        if (rate != requestedRate()) {
            m_player->handlePlaybackCommand(rate ? PlatformMediaSession::PlayCommand : PlatformMediaSession::PauseCommand);
            return;
        }
    }
#endif

    m_player->rateChanged();
}

void MediaPlayerPrivateAVFoundation::loadedTimeRangesChanged()
{
    m_cachedLoadedTimeRanges = nullptr;
    m_cachedMaxTimeLoaded = MediaTime::zeroTime();
    invalidateCachedDuration();
}

void MediaPlayerPrivateAVFoundation::seekableTimeRangesChanged()
{
    m_cachedMaxTimeSeekable = MediaTime::zeroTime();
    m_cachedMinTimeSeekable = MediaTime::zeroTime();
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

    WTF::Function<void()> pendingSeek;
    std::swap(pendingSeek, m_pendingSeek);

    if (pendingSeek) {
        ALWAYS_LOG(LOGIDENTIFIER, "issuing pending seek");
        pendingSeek();
        return;
    }

    if (currentTextTrack())
        currentTextTrack()->endSeeking();

    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivateAVFoundation::didEnd()
{
    // Hang onto the current time and use it as duration from now on since we are definitely at
    // the end of the movie. Do this because the initial duration is sometimes an estimate.
    MediaTime now = currentMediaTime();
    if (now > MediaTime::zeroTime() && !m_seeking)
        m_cachedDuration = now;

    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivateAVFoundation::invalidateCachedDuration()
{
    m_cachedDuration = MediaTime::invalidTime();

    // For some media files, reported duration is estimated and updated as media is loaded
    // so report duration changed when the estimate is upated.
    MediaTime duration = this->durationMediaTime();
    if (duration != m_reportedDuration) {
        INFO_LOG(LOGIDENTIFIER, "- ", m_cachedDuration);
        if (m_reportedDuration.isValid())
            m_player->durationChanged();
        m_reportedDuration = duration;
    }
    
}

void MediaPlayerPrivateAVFoundation::repaint()
{
    m_player->repaint();
}

MediaPlayer::MovieLoadType MediaPlayerPrivateAVFoundation::movieLoadType() const
{
    if (!metaDataAvailable() || assetStatus() == MediaPlayerAVAssetStatusUnknown)
        return MediaPlayer::Unknown;

    if (isLiveStream())
        return MediaPlayer::LiveStream;

    return MediaPlayer::Download;
}

void MediaPlayerPrivateAVFoundation::setPreload(MediaPlayer::Preload preload)
{
    ALWAYS_LOG(LOGIDENTIFIER, " - ", static_cast<int>(preload));
    m_preload = preload;
    if (m_assetURL.isEmpty())
        return;

    setDelayCallbacks(true);

    if (m_preload >= MediaPlayer::MetaData && assetStatus() == MediaPlayerAVAssetStatusDoesNotExist) {
        createAVAssetForURL(m_assetURL);
        checkPlayability();
    }

    // Don't force creation of the player and player item unless we already know that the asset is playable. If we aren't
    // there yet, or if we already know it is not playable, creating them now won't help.
    if (m_preload == MediaPlayer::Auto && m_assetIsPlayable) {
        createAVPlayerItem();
        createAVPlayer();
    }

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundation::setDelayCallbacks(bool delay) const
{
    LockHolder lock(m_queueMutex);
    if (delay)
        ++m_delayCallbacks;
    else {
        ASSERT(m_delayCallbacks);
        --m_delayCallbacks;
    }
}

void MediaPlayerPrivateAVFoundation::mainThreadCallback()
{
    clearMainThreadPendingFlag();
    dispatchNotification();
}

void MediaPlayerPrivateAVFoundation::clearMainThreadPendingFlag()
{
    LockHolder lock(m_queueMutex);
    m_mainThreadCallPending = false;
}

void MediaPlayerPrivateAVFoundation::scheduleMainThreadNotification(Notification::Type type, const MediaTime& time)
{
    scheduleMainThreadNotification(Notification(type, time));
}

void MediaPlayerPrivateAVFoundation::scheduleMainThreadNotification(Notification::Type type, bool finished)
{
    scheduleMainThreadNotification(Notification(type, finished));
}

void MediaPlayerPrivateAVFoundation::scheduleMainThreadNotification(Notification&& notification)
{
    m_queueMutex.lock();

    // It is important to always process the properties in the order that we are notified,
    // so always go through the queue because notifications happen on different threads.
    m_queuedNotifications.append(WTFMove(notification));

#if OS(WINDOWS)
    bool delayDispatch = true;
#else
    bool delayDispatch = m_delayCallbacks || !isMainThread();
#endif
    if (delayDispatch && !m_mainThreadCallPending) {
        m_mainThreadCallPending = true;

        callOnMainThread([weakThis = makeWeakPtr(*this)] {
            if (!weakThis)
                return;

            weakThis->mainThreadCallback();
        });
    }

    m_queueMutex.unlock();

    if (delayDispatch)
        return;

    dispatchNotification();
}

void MediaPlayerPrivateAVFoundation::dispatchNotification()
{
    ASSERT(isMainThread());

    Notification notification;
    {
        LockHolder lock(m_queueMutex);
        
        if (m_queuedNotifications.isEmpty())
            return;
        
        if (!m_delayCallbacks) {
            // Only dispatch one notification callback per invocation because they can cause recursion.
            notification = m_queuedNotifications.takeFirst();
        }
        
        if (!m_queuedNotifications.isEmpty() && !m_mainThreadCallPending) {
            callOnMainThread([weakThis = makeWeakPtr(*this)] {
                if (!weakThis)
                    return;

                weakThis->mainThreadCallback();
            });
        }

        if (!notification.isValid())
            return;
    }

    switch (notification.type()) {
    case Notification::ItemDidPlayToEndTime:
        didEnd();
        break;
    case Notification::ItemTracksChanged:
        tracksChanged();
        updateStates();
        break;
    case Notification::ItemStatusChanged:
        updateStates();
        break;
    case Notification::ItemSeekableTimeRangesChanged:
        seekableTimeRangesChanged();
        updateStates();
        break;
    case Notification::ItemLoadedTimeRangesChanged:
        loadedTimeRangesChanged();
        updateStates();
        break;
    case Notification::ItemPresentationSizeChanged:
        sizeChanged();
        updateStates();
        break;
    case Notification::ItemIsPlaybackLikelyToKeepUpChanged:
        updateStates();
        break;
    case Notification::ItemIsPlaybackBufferEmptyChanged:
        updateStates();
        break;
    case Notification::ItemIsPlaybackBufferFullChanged:
        updateStates();
        break;
    case Notification::PlayerRateChanged:
        updateStates();
        rateChanged();
        break;
    case Notification::PlayerTimeChanged:
        timeChanged(notification.time());
        break;
    case Notification::SeekCompleted:
        seekCompleted(notification.finished());
        break;
    case Notification::AssetMetadataLoaded:
        metadataLoaded();
        updateStates();
        break;
    case Notification::AssetPlayabilityKnown:
        updateStates();
        playabilityKnown();
        break;
    case Notification::DurationChanged:
        invalidateCachedDuration();
        break;
    case Notification::ContentsNeedsDisplay:
        contentsNeedsDisplay();
        break;
    case Notification::InbandTracksNeedConfiguration:
        m_inbandTrackConfigurationPending = false;
        configureInbandTracks();
        break;
    case Notification::FunctionType:
        notification.function()();
        break;
    case Notification::TargetIsWirelessChanged:
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        playbackTargetIsWirelessChanged();
#endif
        break;

    case Notification::None:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaPlayerPrivateAVFoundation::configureInbandTracks()
{
    RefPtr<InbandTextTrackPrivateAVF> trackToEnable;
    
#if ENABLE(AVF_CAPTIONS)
    synchronizeTextTrackState();
#endif

    // AVFoundation can only emit cues for one track at a time, so enable the first track that is showing, or the first that
    // is hidden if none are showing. Otherwise disable all tracks.
    for (unsigned i = 0; i < m_textTracks.size(); ++i) {
        RefPtr<InbandTextTrackPrivateAVF> track = m_textTracks[i];
        if (track->mode() == InbandTextTrackPrivate::Showing) {
            trackToEnable = track;
            break;
        }
        if (track->mode() == InbandTextTrackPrivate::Hidden)
            trackToEnable = track;
    }

    setCurrentTextTrack(trackToEnable.get());
}

void MediaPlayerPrivateAVFoundation::trackModeChanged()
{
    if (m_inbandTrackConfigurationPending)
        return;
    m_inbandTrackConfigurationPending = true;
    scheduleMainThreadNotification(Notification::InbandTracksNeedConfiguration);
}

void MediaPlayerPrivateAVFoundation::clearTextTracks()
{
    for (auto& track : m_textTracks) {
        player()->removeTextTrack(*track);
        track->disconnect();
    }
    m_textTracks.clear();
}

void MediaPlayerPrivateAVFoundation::processNewAndRemovedTextTracks(const Vector<RefPtr<InbandTextTrackPrivateAVF>>& removedTextTracks)
{
    if (removedTextTracks.size()) {
        for (unsigned i = 0; i < m_textTracks.size(); ) {
            if (!removedTextTracks.contains(m_textTracks[i])) {
                ++i;
                continue;
            }
            player()->removeTextTrack(*m_textTracks[i]);
            m_textTracks.remove(i);
        }
    }

    unsigned trackCount = m_textTracks.size();
    unsigned inBandCount = 0;
    for (unsigned i = 0; i < trackCount; ++i) {
        RefPtr<InbandTextTrackPrivateAVF> track = m_textTracks[i];

#if ENABLE(AVF_CAPTIONS)
        if (track->textTrackCategory() == InbandTextTrackPrivateAVF::OutOfBand)
            continue;
#endif

        track->setTextTrackIndex(inBandCount);
        ++inBandCount;
        if (track->hasBeenReported())
            continue;
        
        track->setHasBeenReported(true);
        player()->addTextTrack(*track);
    }

    if (trackCount != m_textTracks.size())
        INFO_LOG(LOGIDENTIFIER, "found ", m_textTracks.size(), " text tracks");
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaPlayerPrivateAVFoundation::playbackTargetIsWirelessChanged()
{
    if (m_player)
        m_player->currentPlaybackTargetIsWirelessChanged();
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

    keyURI = String(reinterpret_cast<UChar*>(keyURIArray->data()), keyURILength / sizeof(unsigned short));
    offset += keyURILength;

    uint32_t keyIDLength = initDataView->get<uint32_t>(offset, true, &status);
    offset += 4;
    if (!status || offset + keyIDLength > initData->length())
        return false;

    auto keyIDArray = Uint8Array::tryCreate(initDataBuffer.copyRef(), offset, keyIDLength);
    if (!keyIDArray)
        return false;

    keyID = String(reinterpret_cast<UChar*>(keyIDArray->data()), keyIDLength / sizeof(unsigned short));
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

    if (url.isLocalFile())
        return true;

    if (!url.protocolIsInHTTPFamily())
        return false;

    if (isLiveStream())
        return false;

    return true;
}

bool MediaPlayerPrivateAVFoundation::isUnsupportedMIMEType(const String& type)
{
    String lowerCaseType = type.convertToASCIILowercase();

    // AVFoundation will return non-video MIME types which it claims to support, but which we
    // do not support in the <video> element. Reject all non video/, audio/, and application/ types.
    if (!lowerCaseType.startsWith("video/") && !lowerCaseType.startsWith("audio/") && !lowerCaseType.startsWith("application/"))
        return true;

    // Reject types we know AVFoundation does not support that sites commonly ask about.
    if (lowerCaseType == "video/webm" || lowerCaseType == "audio/webm" || lowerCaseType == "video/x-webm")
        return true;

    if (lowerCaseType == "video/x-flv")
        return true;

    if (lowerCaseType == "audio/ogg" || lowerCaseType == "video/ogg" || lowerCaseType == "application/ogg")
        return true;

    if (lowerCaseType == "video/h264")
        return true;

    return false;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaPlayerPrivateAVFoundation::logChannel() const
{
    return LogMedia;
}
#endif

const HashSet<String, ASCIICaseInsensitiveHash>& MediaPlayerPrivateAVFoundation::staticMIMETypeList()
{
    static const auto cache = makeNeverDestroyed(HashSet<String, ASCIICaseInsensitiveHash> {
        "application/vnd.apple.mpegurl",
        "application/x-mpegurl",
        "audio/3gpp",
        "audio/aac",
        "audio/aacp",
        "audio/aiff",
        "audio/basic",
        "audio/mp3",
        "audio/mp4",
        "audio/mpeg",
        "audio/mpeg3",
        "audio/mpegurl",
        "audio/mpg",
        "audio/vnd.wave",
        "audio/wav",
        "audio/wave",
        "audio/x-aac",
        "audio/x-aiff",
        "audio/x-m4a",
        "audio/x-mpegurl",
        "audio/x-wav",
        "video/3gpp",
        "video/3gpp2",
        "video/mp4",
        "video/mpeg",
        "video/mpeg2",
        "video/mpg",
        "video/quicktime",
        "video/x-m4v",
        "video/x-mpeg",
        "video/x-mpg",
    });
    return cache;
}

} // namespace WebCore

#endif
