/*
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MediaPlayerPrivateMediaSourceAVFObjC.h"

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#import "AVAssetMIMETypeCache.h"
#import "AVAssetTrackUtilities.h"
#import "AVStreamDataParserMIMETypeCache.h"
#import "AudioMediaStreamTrackRenderer.h"
#import "AudioVideoRendererAVFObjC.h"
#import "CDMInstance.h"
#import "CDMSessionAVContentKeySession.h"
#import "ContentTypeUtilities.h"
#import "GraphicsContext.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaPlayerIdentifier.h"
#import "MediaSessionManagerCocoa.h"
#import "MediaSourcePrivate.h"
#import "MediaSourcePrivateAVFObjC.h"
#import "MediaSourcePrivateClient.h"
#import "MediaStrategy.h"
#import "MessageClientForTesting.h"
#import "MessageForTesting.h"
#import "PixelBufferConformerCV.h"
#import "PlatformDynamicRangeLimitCocoa.h"
#import "PlatformScreen.h"
#import "PlatformStrategies.h"
#import "SourceBufferPrivateAVFObjC.h"
#import "SpatialAudioExperienceHelper.h"
#import "TextTrackRepresentation.h"
#import "VideoFrameCV.h"
#import "VideoLayerManagerObjC.h"
#import "VideoMediaSampleRenderer.h"
#import "WebSampleBufferVideoRendering.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CMTime.h>
#import <QuartzCore/CALayer.h>
#import <objc_runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cf/CFNotificationCenterSPI.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Deque.h>
#import <wtf/FileSystem.h>
#import <wtf/MachSendRightAnnotated.h>
#import <wtf/MainThread.h>
#import <wtf/NativePromise.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>

#import "CoreVideoSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/MediaToolboxSoftLink.h>

@interface AVSampleBufferDisplayLayer (Staging_100128644)
@property (assign, nonatomic) BOOL preventsAutomaticBackgroundingDuringVideoPlayback;
@end

namespace WebCore {

#pragma mark -
#pragma mark MediaPlayerPrivateMediaSourceAVFObjC

Ref<AudioVideoRenderer> MediaPlayerPrivateMediaSourceAVFObjC::createRenderer(LoggerHelper& loggerHelper, HTMLMediaElementIdentifier mediaElementIdentifier, MediaPlayerIdentifier playerIdentifier)
{
    if (hasPlatformStrategies()) {
        if (RefPtr renderer = platformStrategies()->mediaStrategy()->createAudioVideoRenderer(&loggerHelper, mediaElementIdentifier, playerIdentifier))
            return renderer.releaseNonNull();
    }
    return AudioVideoRendererAVFObjC::create(Ref { loggerHelper.logger() }, loggerHelper.logIdentifier());
}

MediaPlayerPrivateMediaSourceAVFObjC::MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer& player)
    : m_player(player)
    , m_seekTimer(*this, &MediaPlayerPrivateMediaSourceAVFObjC::seekInternal)
    , m_rendererSeekRequest(NativePromiseRequest::create())
    , m_networkState(MediaPlayer::NetworkState::Empty)
    , m_logger(player.mediaPlayerLogger())
    , m_logIdentifier(player.mediaPlayerLogIdentifier())
#if HAVE(SPATIAL_TRACKING_LABEL)
    , m_defaultSpatialTrackingLabel(player.defaultSpatialTrackingLabel())
    , m_spatialTrackingLabel(player.spatialTrackingLabel())
#endif
    , m_playerIdentifier(MediaPlayerIdentifier::generate())
    , m_renderer(createRenderer(*this, player.clientIdentifier(), m_playerIdentifier))
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

MediaPlayerPrivateMediaSourceAVFObjC::~MediaPlayerPrivateMediaSourceAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    cancelPendingSeek();
    m_seekTimer.stop();
    m_renderer->pause();
    m_renderer->flush();
}

#pragma mark -
#pragma mark MediaPlayer Factory Methods

class MediaPlayerFactoryMediaSourceAVFObjC final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryMediaSourceAVFObjC);
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final
    {
        return adoptRef(*new MediaPlayerPrivateMediaSourceAVFObjC(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        return MediaPlayerPrivateMediaSourceAVFObjC::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(parameters);
    }
};

void MediaPlayerPrivateMediaSourceAVFObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (!isAvailable())
        return;

    ASSERT(AVAssetMIMETypeCache::singleton().isAvailable());

    registrar(makeUnique<MediaPlayerFactoryMediaSourceAVFObjC>());
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isAvailable()
{
    return PAL::isAVFoundationFrameworkAvailable()
        && PAL::isCoreMediaFrameworkAvailable()
        && PAL::getAVStreamDataParserClassSingleton()
        && PAL::getAVSampleBufferAudioRendererClassSingleton()
        && PAL::getAVSampleBufferRenderSynchronizerClassSingleton()
        && class_getInstanceMethod(PAL::getAVSampleBufferAudioRendererClassSingleton(), @selector(setMuted:));
}

void MediaPlayerPrivateMediaSourceAVFObjC::getSupportedTypes(HashSet<String>& types)
{
    types = AVStreamDataParserMIMETypeCache::singleton().supportedTypes();
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters)
{
    // This engine does not support non-media-source sources.
    if (!parameters.isMediaSource)
        return MediaPlayer::SupportsType::IsNotSupported;

    if (!contentTypeMeetsContainerAndCodecTypeRequirements(parameters.type, parameters.allowedMediaContainerTypes, parameters.allowedMediaCodecTypes))
        return MediaPlayer::SupportsType::IsNotSupported;

    auto supported = SourceBufferParser::isContentTypeSupported(parameters.type);

    if (supported != MediaPlayer::SupportsType::IsSupported)
        return supported;

    if (!contentTypeMeetsHardwareDecodeRequirements(parameters.type, parameters.contentTypesRequiringHardwareSupport))
        return MediaPlayer::SupportsType::IsNotSupported;

    return MediaPlayer::SupportsType::IsSupported;
}

#pragma mark -
#pragma mark MediaPlayerPrivateInterface Overrides

void MediaPlayerPrivateMediaSourceAVFObjC::load(const String&)
{
    assertIsMainThread();
    // This media engine only supports MediaSource URLs.
    m_networkState = MediaPlayer::NetworkState::FormatError;
    if (RefPtr player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::load(const URL&, const LoadOptions& options, MediaSourcePrivateClient& client)
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER);

    m_renderer->notifyWhenErrorOccurs([weakThis = WeakPtr { *this }](PlatformMediaError) {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get()) {
                protectedThis->setNetworkState(MediaPlayer::NetworkState::DecodeError);
                protectedThis->setReadyState(MediaPlayer::ReadyState::HaveNothing);
            }
        });
    });

    m_renderer->notifyFirstFrameAvailable([weakThis = WeakPtr { *this }] {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get(); protectedThis && !protectedThis->seeking())
                protectedThis->setHasAvailableVideoFrame(true);
        });
    });

    m_renderer->notifyWhenRequiresFlushToResume([weakThis = WeakPtr { *this }] {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->setLayerRequiresFlush();
        });
    });

    m_renderer->notifyRenderingModeChanged([weakThis = WeakPtr { *this }] {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get()) {
                if (RefPtr player = protectedThis->m_player.get())
                    player->renderingModeChanged();
            }
        });
    });

    m_renderer->notifySizeChanged([weakThis = WeakPtr { *this }](const MediaTime&, FloatSize size) {
        ensureOnMainThread([weakThis, size] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->setNaturalSize(size);
        });
    });

    m_renderer->notifyEffectiveRateChanged([weakThis = WeakPtr { *this }](double) {
        ensureOnMainThread([weakThis] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->effectiveRateChanged();
        });
    });

    m_renderer->notifyVideoLayerSizeChanged([weakThis = WeakPtr { *this }](const MediaTime&, FloatSize size) {
        ensureOnMainThread([weakThis, size] {
            if (RefPtr protectedThis = weakThis.get()) {
                if (RefPtr player = protectedThis->m_player.get())
                    player->videoLayerSizeDidChange(size);
            }
        });
    });

    m_loadOptions = options;
    m_renderer->setPreferences(options.videoRendererPreferences);
    if (RefPtr player = m_player.get()) {
        m_renderer->setPresentationSize(player->presentationSize());
        m_renderer->renderingCanBeAcceleratedChanged(player->renderingCanBeAccelerated());
        m_renderer->setVolume(player->volume());
        m_renderer->setMuted(player->muted());
        m_renderer->setPreservesPitchAndCorrectionAlgorithm(player->preservesPitch(), player->pitchCorrectionAlgorithm());
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
        m_renderer->setOutputDeviceId(player->audioOutputDeviceIdOverride());
#endif
#if ENABLE(LINEAR_MEDIA_PLAYER)
        if (RetainPtr videoTarget = player->videoTarget())
            m_renderer->setVideoTarget(videoTarget.get());
#endif
    }

    if (RefPtr mediaSourcePrivate = downcast<MediaSourcePrivateAVFObjC>(client.mediaSourcePrivate())) {
        mediaSourcePrivate->setPlayer(this);
        m_mediaSourcePrivate = WTFMove(mediaSourcePrivate);
        client.reOpen();
    } else
        m_mediaSourcePrivate = MediaSourcePrivateAVFObjC::create(*this, client);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setResourceOwner(const ProcessIdentity& resourceOwner)
{
    m_renderer->setResourceOwner(resourceOwner);
}

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateMediaSourceAVFObjC::load(MediaStreamPrivate&)
{
    setNetworkState(MediaPlayer::NetworkState::FormatError);
}
#endif

void MediaPlayerPrivateMediaSourceAVFObjC::cancelLoad()
{
}

void MediaPlayerPrivateMediaSourceAVFObjC::prepareToPlay()
{
}

PlatformLayer* MediaPlayerPrivateMediaSourceAVFObjC::platformLayer() const
{
    return m_renderer->platformVideoLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::play()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    playInternal();
}

void MediaPlayerPrivateMediaSourceAVFObjC::playInternal(std::optional<MonotonicTime>&& hostTime)
{
    assertIsMainThread();
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    if (!mediaSourcePrivate)
        return;

    if (currentTime() >= mediaSourcePrivate->duration()) {
        ALWAYS_LOG(LOGIDENTIFIER, "bailing, current time: ", currentTime(), " greater than duration ", mediaSourcePrivate->duration());
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER);
    flushVideoIfNeeded();

    m_renderer->play(hostTime);
}

void MediaPlayerPrivateMediaSourceAVFObjC::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    pauseInternal();
}

void MediaPlayerPrivateMediaSourceAVFObjC::pauseInternal(std::optional<MonotonicTime>&& hostTime)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_renderer->pause(hostTime);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::paused() const
{
    return m_renderer->paused();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVolume(float volume)
{
    ALWAYS_LOG(LOGIDENTIFIER, volume);
    m_renderer->setVolume(volume);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsScanning() const
{
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setMuted(bool muted)
{
    ALWAYS_LOG(LOGIDENTIFIER, muted);
    m_renderer->setMuted(muted);
}

FloatSize MediaPlayerPrivateMediaSourceAVFObjC::naturalSize() const
{
    return m_naturalSize;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasVideo() const
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate && mediaSourcePrivate->hasVideo();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasAudio() const
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate && mediaSourcePrivate->hasAudio();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPageIsVisible(bool visible)
{
    assertIsMainThread();
    if (m_visible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);
    m_visible = visible;
    m_renderer->setIsVisible(visible);
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::duration() const
{
    assertIsMainThread();
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate ? mediaSourcePrivate->duration() : MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::currentTime() const
{
    return m_renderer->currentTime();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::timeIsProgressing() const
{
    return m_renderer->timeIsProgressing();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::clampTimeToSensicalValue(const MediaTime& time) const
{
    assertIsMainThread();
    if (m_lastSeekTime.isFinite() && time < m_lastSeekTime)
        return m_lastSeekTime;

    if (time < MediaTime::zeroTime())
        return MediaTime::zeroTime();
    if (time > duration())
        return duration();
    return time;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::setCurrentTimeDidChangeCallback(MediaPlayer::CurrentTimeDidChangeCallback&& callback)
{
    assertIsMainThread();
    m_renderer->setTimeObserver(10_ms, [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](const MediaTime& currentTime) mutable {
        // This method is only used with the RemoteMediaPlayerProxy and RemoteAudioVideoRendererProxyManager where m_renderer is an AudioVideoRendererAVFObjC that runs on the main thread only (for now).
        assertIsMainThread();
        if (RefPtr protectedThis = weakThis.get())
            callback(protectedThis->clampTimeToSensicalValue(currentTime));
    });

    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::timeChanged()
{
    assertIsMainThread();
    if (RefPtr player = m_player.get())
        player->timeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::stall()
{
    assertIsMainThread();
    m_renderer->stall();
    if (shouldBePlaying())
        timeChanged();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::playAtHostTime(const MonotonicTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    playInternal(time);
    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::pauseAtHostTime(const MonotonicTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    pauseInternal(time);
    return true;
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::startTime() const
{
    return MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::initialTime() const
{
    return MediaTime::zeroTime();
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekToTarget(const SeekTarget& target)
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER, "time = ", target.time, ", negativeThreshold = ", target.negativeThreshold, ", positiveThreshold = ", target.positiveThreshold);

    m_pendingSeek = target;

    if (m_seekTimer.isActive())
        m_seekTimer.stop();
    m_seekTimer.startOneShot(0_s);
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekInternal()
{
    assertIsMainThread();
    if (!m_pendingSeek)
        return;

    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    if (!mediaSourcePrivate)
        return;

    auto pendingSeek = std::exchange(m_pendingSeek, { }).value();
    m_lastSeekTime = pendingSeek.time;

    m_seeking = true;

    cancelPendingSeek();
    m_renderer->prepareToSeek();

    mediaSourcePrivate->waitForTarget(pendingSeek)->whenSettled(RunLoop::currentSingleton(), [weakThis = WeakPtr { *this }, seekTime = m_lastSeekTime] (auto&& result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return; // seek cancelled;

        protectedThis->startSeek(seekTime);
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::startSeek(const MediaTime& seekTime)
{
    assertIsMainThread();
    if (m_rendererSeekRequest->hasCallback()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Seeking pending, cancel earlier seek");
        cancelPendingSeek();
    }
    m_renderer->seekTo(seekTime)->whenSettled(RunLoop::mainSingleton(), [weakThis = WeakPtr { *this }, seekTime](auto&& result) {
        assertIsMainThread();
        if (!result && result.error() != PlatformMediaError::RequiresFlushToResume)
            return; // cancelled.

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_rendererSeekRequest->complete();

        if (!result) {
            ASSERT(result.error() == PlatformMediaError::RequiresFlushToResume);
            protectedThis->flush();
            protectedThis->reenqueueMediaForTime(seekTime);
            protectedThis->startSeek(seekTime);
            return;
        }
        protectedThis->completeSeek(*result);
    })->track(m_rendererSeekRequest.get());
}

void MediaPlayerPrivateMediaSourceAVFObjC::cancelPendingSeek()
{
    assertIsMainThread();

    if (m_rendererSeekRequest->hasCallback())
        m_rendererSeekRequest->disconnect();
}

void MediaPlayerPrivateMediaSourceAVFObjC::completeSeek(const MediaTime& seekedTime)
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER, "");

    m_seeking = false;

    if (auto player = m_player.get()) {
        player->seeked(seekedTime);
        player->timeChanged();
    }

    if (hasVideo())
        setHasAvailableVideoFrame(true);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::seeking() const
{
    assertIsMainThread();
    return m_pendingSeek || m_seeking;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setLoadingProgresssed(bool flag)
{
    assertIsMainThread();
    m_loadingProgressed = flag;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setRateDouble(double rate)
{
    assertIsMainThread();
    // AVSampleBufferRenderSynchronizer does not support negative rate yet.
    m_rate = std::max<double>(rate, 0);
    m_renderer->setRate(m_rate);
}

double MediaPlayerPrivateMediaSourceAVFObjC::rate() const
{
    assertIsMainThread();
    return m_rate;
}

double MediaPlayerPrivateMediaSourceAVFObjC::effectiveRate() const
{
    assertIsMainThread();
    return m_renderer->effectiveRate();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPreservesPitch(bool preservesPitch)
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER, preservesPitch);
    if (auto player = m_player.get())
        m_renderer->setPreservesPitchAndCorrectionAlgorithm(preservesPitch, player->pitchCorrectionAlgorithm());
}

MediaPlayer::NetworkState MediaPlayerPrivateMediaSourceAVFObjC::networkState() const
{
    assertIsMainThread();
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateMediaSourceAVFObjC::readyState() const
{
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        return mediaSourcePrivate->mediaPlayerReadyState();
    assertIsMainThread();
    return m_readyState;
}

void MediaPlayerPrivateMediaSourceAVFObjC::readyStateFromMediaSourceChanged()
{
    assertIsMainThread();
    updateStateFromReadyState();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::maxTimeSeekable() const
{
    return duration();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::minTimeSeekable() const
{
    return startTime();
}

const PlatformTimeRanges& MediaPlayerPrivateMediaSourceAVFObjC::buffered() const
{
    return PlatformTimeRanges::emptyRanges();
}

RefPtr<MediaSourcePrivateAVFObjC> MediaPlayerPrivateMediaSourceAVFObjC::protectedMediaSourcePrivate() const
{
    return m_mediaSourcePrivate;
}

void MediaPlayerPrivateMediaSourceAVFObjC::bufferedChanged()
{
    assertIsMainThread();
    m_renderer->cancelTimeReachedAction();

    auto ranges = protectedMediaSourcePrivate()->buffered();
    auto currentTime = this->currentTime();
    if (!protectedMediaSourcePrivate()->hasFutureTime(currentTime) && shouldBePlaying()) {
        ALWAYS_LOG(LOGIDENTIFIER, "Not having data to play at currentTime: ", currentTime, " stalling");
        stall();
    }

    auto stallAtTime = duration();
    size_t index = ranges.find(currentTime);
    if (index != notFound) {
        // Find the next gap (or end of media)
        for (; index < ranges.length(); index++) {
            if ((index < ranges.length() - 1 && ranges.start(index + 1) - ranges.end(index) > m_mediaSourcePrivate->timeFudgeFactor())
                || (index == ranges.length() - 1 && ranges.end(index) > currentTime)) {
                stallAtTime = ranges.end(index);
                break;
            }
        }
    }

    ALWAYS_LOG(LOGIDENTIFIER, "will stall playback at time: ", stallAtTime);
    auto logSiteIdentifier = LOGIDENTIFIER;
    UNUSED_PARAM(logSiteIdentifier);
    m_renderer->notifyTimeReachedAndStall(stallAtTime, [weakThis = WeakPtr { *this }, logSiteIdentifier](const MediaTime& stallTime) {
        ensureOnMainThread([weakThis, logSiteIdentifier, stallTime] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (protectedThis->protectedMediaSourcePrivate()->hasFutureTime(stallTime) && protectedThis->shouldBePlaying()) {
                ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "Data now available at ", stallTime, " resuming");
                protectedThis->m_renderer->play(); // New data was added, resume. Can't happen in practice, action would have been cancelled once buffered changed.
                return;
            }
            MediaTime now = protectedThis->currentTime();
            ALWAYS_LOG_WITH_THIS(protectedThis, logSiteIdentifier, "boundary time observer called, now = ", now);

            if (stallTime == protectedThis->duration())
                protectedThis->pause();
            protectedThis->timeChanged();
        });
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::setLayerRequiresFlush()
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER);
    m_layerRequiresFlush = true;
#if PLATFORM(IOS_FAMILY)
    if (m_applicationIsActive)
        flushVideoIfNeeded();
#else
    flushVideoIfNeeded();
#endif
}

void MediaPlayerPrivateMediaSourceAVFObjC::flushVideoIfNeeded()
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER, "layerRequiresFlush: ", m_layerRequiresFlush);
    if (!m_layerRequiresFlush)
        return;

    m_layerRequiresFlush = false;

    // Rendering may have been interrupted while the page was in a non-visible
    // state, which would require a flush to resume decoding.
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate) {
        setHasAvailableVideoFrame(false);
        mediaSourcePrivate->flushAndReenqueueActiveVideoSourceBuffers();
    }
}

void MediaPlayerPrivateMediaSourceAVFObjC::flush()
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER);
    m_renderer->flush();
    setHasAvailableVideoFrame(false);
}

void MediaPlayerPrivateMediaSourceAVFObjC::reenqueueMediaForTime(const MediaTime& seekTime)
{
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->seekToTime(seekTime);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::didLoadingProgress() const
{
    assertIsMainThread();
    bool loadingProgressed = m_loadingProgressed;
    m_loadingProgressed = false;
    return loadingProgressed;
}

RefPtr<NativeImage> MediaPlayerPrivateMediaSourceAVFObjC::nativeImageForCurrentTime()
{
    assertIsMainThread();
    updateLastImage();
    return m_lastImage;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::updateLastVideoFrame()
{
    assertIsMainThread();
    RefPtr videoFrame = m_renderer->currentVideoFrame();
    if (!videoFrame)
        return false;

    INFO_LOG(LOGIDENTIFIER, "displayed pixelbuffer copied for time ", videoFrame->presentationTime());
    m_lastVideoFrame = WTFMove(videoFrame);
    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::updateLastImage()
{
    assertIsMainThread();
    if (m_isGatheringVideoFrameMetadata) {
        if (!m_videoFrameMetadata)
            return false;
        if (m_videoFrameMetadata->presentedFrames == m_lastConvertedSampleCount)
            return false;
        m_lastConvertedSampleCount = m_videoFrameMetadata->presentedFrames;
    }

    m_lastImage = m_renderer->currentNativeImage();
    return !!m_lastImage;
}

void MediaPlayerPrivateMediaSourceAVFObjC::maybePurgeLastImage()
{
    assertIsMainThread();
    // If we are in the middle of a rVFC operation, do not purge anything:
    if (m_isGatheringVideoFrameMetadata)
        return;

    m_lastImage = nullptr;
    m_lastVideoFrame = nullptr;
}

void MediaPlayerPrivateMediaSourceAVFObjC::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateMediaSourceAVFObjC::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& outputRect)
{
    m_renderer->paintCurrentVideoFrameInContext(context, outputRect);
}

RefPtr<VideoFrame> MediaPlayerPrivateMediaSourceAVFObjC::videoFrameForCurrentTime()
{
    assertIsMainThread();
    if (!m_isGatheringVideoFrameMetadata)
        updateLastVideoFrame();
    return m_lastVideoFrame;
}

DestinationColorSpace MediaPlayerPrivateMediaSourceAVFObjC::colorSpace()
{
    assertIsMainThread();
    updateLastImage();
    RefPtr lastImage = m_lastImage;
    return lastImage ? lastImage->colorSpace() : DestinationColorSpace::SRGB();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasAvailableVideoFrame() const
{
    assertIsMainThread();
    return m_hasAvailableVideoFrame;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsAcceleratedRendering() const
{
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPresentationSize(const IntSize& newSize)
{
    m_renderer->setPresentationSize(newSize);
}

Ref<AudioVideoRenderer> MediaPlayerPrivateMediaSourceAVFObjC::audioVideoRenderer() const
{
    return m_renderer;
}

void MediaPlayerPrivateMediaSourceAVFObjC::acceleratedRenderingStateChanged()
{
    RefPtr player = m_player.get();
    m_renderer->renderingCanBeAcceleratedChanged(player ? player->renderingCanBeAccelerated() : false);
}

void MediaPlayerPrivateMediaSourceAVFObjC::notifyActiveSourceBuffersChanged()
{
    assertIsMainThread();
    if (auto player = m_player.get())
        player->activeSourceBuffersChanged();
}

MediaPlayer::MovieLoadType MediaPlayerPrivateMediaSourceAVFObjC::movieLoadType() const
{
    return MediaPlayer::MovieLoadType::StoredStream;
}

String MediaPlayerPrivateMediaSourceAVFObjC::engineDescription() const
{
    static NeverDestroyed<String> description(MAKE_STATIC_STRING_IMPL("AVFoundation MediaSource Engine"));
    return description;
}

String MediaPlayerPrivateMediaSourceAVFObjC::languageOfPrimaryAudioTrack() const
{
    // FIXME(125158): implement languageOfPrimaryAudioTrack()
    return emptyString();
}

size_t MediaPlayerPrivateMediaSourceAVFObjC::extraMemoryCost() const
{
    return 0;
}

std::optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateMediaSourceAVFObjC::videoPlaybackQualityMetrics()
{
    return m_renderer->videoPlaybackQualityMetrics();
}

#pragma mark -
#pragma mark Utility Methods

bool MediaPlayerPrivateMediaSourceAVFObjC::shouldBePlaying() const
{
    assertIsMainThread();
    return !m_renderer->paused() && !seeking() && readyState() >= MediaPlayer::ReadyState::HaveFutureData;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setHasAvailableVideoFrame(bool flag)
{
    assertIsMainThread();
    if (m_hasAvailableVideoFrame == flag)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, flag);
    m_hasAvailableVideoFrame = flag;

    if (!m_hasAvailableVideoFrame)
        return;

    auto player = m_player.get();
    if (player)
        player->firstVideoFrameAvailable();

    if (m_readyStateIsWaitingForAvailableFrame) {
        m_readyStateIsWaitingForAvailableFrame = false;
        if (player)
            player->readyStateChanged();
    }
}

void MediaPlayerPrivateMediaSourceAVFObjC::durationChanged()
{
    assertIsMainThread();
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    if (!mediaSourcePrivate)
        return;

    MediaTime duration = mediaSourcePrivate->duration();
    // Avoid emiting durationchanged in the case where the previous duration was unknown as that case is already handled
    // by the HTMLMediaElement.
    if (m_duration != duration && m_duration.isValid()) {
        if (auto player = m_player.get())
            player->durationChanged();
    }
    m_duration = duration;
    bufferedChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::effectiveRateChanged()
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER, effectiveRate());
    if (auto player = m_player.get())
        player->rateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNaturalSize(const FloatSize& size)
{
    assertIsMainThread();
    if (size == m_naturalSize)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, size);

    m_naturalSize = size;
    if (auto player = m_player.get())
        player->sizeChanged();
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RefPtr<CDMSessionAVContentKeySession> MediaPlayerPrivateMediaSourceAVFObjC::cdmSession() const
{
    assertIsMainThread();
    return m_session.get();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setCDMSession(LegacyCDMSession* session)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_renderer->setCDMSession(session);
}

void MediaPlayerPrivateMediaSourceAVFObjC::keyAdded()
{
    m_renderer->attemptToDecrypt();
}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateMediaSourceAVFObjC::keyNeeded(const SharedBuffer& initData)
{
    if (auto player = m_player.get())
        player->keyNeeded(initData);
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateMediaSourceAVFObjC::cdmInstanceAttached(CDMInstance& instance)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_renderer->setCDMInstance(&instance);

    needsVideoLayerChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::cdmInstanceDetached(CDMInstance&)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_renderer->setCDMInstance(nullptr);

    needsVideoLayerChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::attemptToDecryptWithInstance(CDMInstance&)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_renderer->attemptToDecrypt();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::waitingForKey() const
{
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    return mediaSourcePrivate && mediaSourcePrivate->waitingForKey();
}

void MediaPlayerPrivateMediaSourceAVFObjC::waitingForKeyChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto player = m_player.get())
        player->waitingForKeyChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::initializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    ALWAYS_LOG(LOGIDENTIFIER, initDataType);
    if (auto player = m_player.get())
        player->initializationDataEncountered(initDataType, WTFMove(initData));
}
#endif

const Vector<ContentType>& MediaPlayerPrivateMediaSourceAVFObjC::mediaContentTypesRequiringHardwareSupport() const
{
    return m_player.get()->mediaContentTypesRequiringHardwareSupport();
}

void MediaPlayerPrivateMediaSourceAVFObjC::needsVideoLayerChanged()
{
    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        m_renderer->setHasProtectedVideoContent(mediaSourcePrivate->needsVideoLayer());
}

void MediaPlayerPrivateMediaSourceAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    assertIsMainThread();
    if (m_readyState == readyState)
        return;

    if (m_readyState > MediaPlayer::ReadyState::HaveCurrentData && readyState == MediaPlayer::ReadyState::HaveCurrentData)
        ALWAYS_LOG(LOGIDENTIFIER, "stall detected currentTime:", currentTime());

    m_readyState = readyState;
    updateStateFromReadyState();
}

void MediaPlayerPrivateMediaSourceAVFObjC::updateStateFromReadyState()
{
    assertIsMainThread();
    if (shouldBePlaying()) {
        m_renderer->play();
        timeChanged();
    } else
        stall();

    if (readyState() >= MediaPlayer::ReadyState::HaveCurrentData && hasVideo() && !m_hasAvailableVideoFrame) {
        m_readyStateIsWaitingForAvailableFrame = true;
        return;
    }

    if (auto player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNetworkState(MediaPlayer::NetworkState networkState)
{
    assertIsMainThread();
    if (m_networkState == networkState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, networkState);
    m_networkState = networkState;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::mediaSourceHasRetrievedAllData()
{
    assertIsMainThread();
    setNetworkState(MediaPlayer::NetworkState::Loaded);
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::addAudioTrack(TrackIdentifier audioTrack)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    assertIsMainThread();
    if (!m_audioTracksMap.add(audioTrack, AudioTrackProperties()).isNewEntry)
        return;

    if (RefPtr player = m_player.get())
        player->characteristicChanged();
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::removeAudioTrack(TrackIdentifier audioTrack)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    assertIsMainThread();
    if (auto iter = m_audioTracksMap.find(audioTrack); iter != m_audioTracksMap.end())
        m_audioTracksMap.remove(iter);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeAudioTrack(AudioTrackPrivate& track)
{
    assertIsMainThread();
    if (auto player = m_player.get())
        player->removeAudioTrack(track);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeVideoTrack(VideoTrackPrivate& track)
{
    assertIsMainThread();
    if (auto player = m_player.get())
        player->removeVideoTrack(track);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeTextTrack(InbandTextTrackPrivate& track)
{
    assertIsMainThread();
    if (auto player = m_player.get())
        player->removeTextTrack(track);
}

void MediaPlayerPrivateMediaSourceAVFObjC::characteristicsChanged()
{
    assertIsMainThread();
    if (auto player = m_player.get())
        player->characteristicChanged();
}

RetainPtr<PlatformLayer> MediaPlayerPrivateMediaSourceAVFObjC::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    m_renderer->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler));
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoFullscreenFrame(const FloatRect& frame)
{
    m_renderer->setVideoFullscreenFrame(frame);
}

void MediaPlayerPrivateMediaSourceAVFObjC::syncTextTrackBounds()
{
    m_renderer->syncTextTrackBounds();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    m_renderer->setTextTrackRepresentation(representation);
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaPlayerPrivateMediaSourceAVFObjC::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER);
    m_playbackTarget = WTFMove(target);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldPlayToPlaybackTarget(bool shouldPlayToTarget)
{
    assertIsMainThread();
    if (shouldPlayToTarget == m_shouldPlayToTarget)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldPlayToTarget);
    m_shouldPlayToTarget = shouldPlayToTarget;

    if (auto player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless());
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isCurrentPlaybackTargetWireless() const
{
    assertIsMainThread();
    RefPtr playbackTarget = m_playbackTarget;
    if (!playbackTarget)
        return false;

    auto hasTarget = m_shouldPlayToTarget && playbackTarget->hasActiveRoute();
    INFO_LOG(LOGIDENTIFIER, hasTarget);
    return hasTarget;
}
#endif

bool MediaPlayerPrivateMediaSourceAVFObjC::performTaskAtTime(WTF::Function<void(const MediaTime&)>&& task, const MediaTime& time)
{
    m_renderer->performTaskAtTime(time, [task = WTFMove(task)](const MediaTime& time) mutable {
        ensureOnMainThread([time, task = WTFMove(task)] {
            task(time);
        });
    });
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::audioOutputDeviceChanged()
{
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto player = m_player.get();
    if (!player)
        return;
    auto deviceId = player->audioOutputDeviceId();
    m_renderer->setOutputDeviceId(deviceId);
#endif
}

void MediaPlayerPrivateMediaSourceAVFObjC::startVideoFrameMetadataGathering()
{
    assertIsMainThread();
    if (m_isGatheringVideoFrameMetadata)
        return;

    m_isGatheringVideoFrameMetadata = true;
    m_renderer->notifyWhenHasAvailableVideoFrame([weakThis = WeakPtr { *this }](const MediaTime& presentationTime, double displayTime) {
        ensureOnMainThread([weakThis, presentationTime, displayTime] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->checkNewVideoFrameMetadata(presentationTime, displayTime);
        });
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::checkNewVideoFrameMetadata(MediaTime presentationTime, double displayTime)
{
    assertIsMainThread();
    auto player = m_player.get();
    if (!player)
        return;

    if (!updateLastVideoFrame())
        return;

    RefPtr lastVideoFrame = m_lastVideoFrame;
#ifndef NDEBUG
    if (m_lastVideoFrame->presentationTime() != presentationTime)
        ALWAYS_LOG(LOGIDENTIFIER, "notification of new frame delayed retrieved:", lastVideoFrame->presentationTime(), " expected:", presentationTime);
#else
    UNUSED_PARAM(presentationTime);
#endif
    VideoFrameMetadata metadata;
    metadata.width = m_naturalSize.width();
    metadata.height = m_naturalSize.height();
    auto metrics = m_renderer->videoPlaybackQualityMetrics();
    ASSERT(metrics);
    metadata.presentedFrames = metrics ? metrics->displayCompositedVideoFrames : 0;
    metadata.presentationTime = displayTime;
    metadata.expectedDisplayTime = displayTime;
    metadata.mediaTime = lastVideoFrame->presentationTime().toDouble();

    m_videoFrameMetadata = metadata;
    player->onNewVideoFrameMetadata(WTFMove(metadata), lastVideoFrame->pixelBuffer());
}

void MediaPlayerPrivateMediaSourceAVFObjC::stopVideoFrameMetadataGathering()
{
    assertIsMainThread();
    m_isGatheringVideoFrameMetadata = false;
    m_videoFrameMetadata = { };
    m_renderer->notifyWhenHasAvailableVideoFrame(nullptr);
}

std::optional<VideoFrameMetadata> MediaPlayerPrivateMediaSourceAVFObjC::videoFrameMetadata()
{
    assertIsMainThread();
    return std::exchange(m_videoFrameMetadata, { });
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldDisableHDR(bool shouldDisable)
{
    m_renderer->setShouldDisableHDR(shouldDisable);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPlatformDynamicRangeLimit(PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    m_renderer->setPlatformDynamicRangeLimit(platformDynamicRangeLimit);
}

void MediaPlayerPrivateMediaSourceAVFObjC::playerContentBoxRectChanged(const LayoutRect& newRect)
{
    m_renderer->contentBoxRectChanged(newRect);
}

WTFLogChannel& MediaPlayerPrivateMediaSourceAVFObjC::logChannel() const
{
    return LogMediaSource;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    m_renderer->setShouldMaintainAspectRatio(shouldMaintainAspectRatio);
}

#if HAVE(SPATIAL_TRACKING_LABEL)
String MediaPlayerPrivateMediaSourceAVFObjC::defaultSpatialTrackingLabel() const
{
    assertIsMainThread();
    return m_defaultSpatialTrackingLabel;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    assertIsMainThread();
    if (m_defaultSpatialTrackingLabel == defaultSpatialTrackingLabel)
        return;
    m_defaultSpatialTrackingLabel = defaultSpatialTrackingLabel;
    updateSpatialTrackingLabel();
}

String MediaPlayerPrivateMediaSourceAVFObjC::spatialTrackingLabel() const
{
    assertIsMainThread();
    return m_spatialTrackingLabel;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    assertIsMainThread();
    if (m_spatialTrackingLabel == spatialTrackingLabel)
        return;
    m_spatialTrackingLabel = spatialTrackingLabel;
    updateSpatialTrackingLabel();
}

void MediaPlayerPrivateMediaSourceAVFObjC::updateSpatialTrackingLabel()
{
    assertIsMainThread();
#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    RefPtr player = m_player.get();
    m_renderer->setSpatialTrackingInfo(player && player->prefersSpatialAudioExperience(), player ? player->soundStageSize() : MediaPlayer::SoundStageSize::Auto, player ? player->sceneIdentifier() : emptyString(), m_defaultSpatialTrackingLabel, m_spatialTrackingLabel);
#else
    m_renderer->setSpatialTrackingInfo(false, MediaPlayer::SoundStageSize::Auto, { }, m_defaultSpatialTrackingLabel, m_spatialTrackingLabel);
#endif
}
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
void MediaPlayerPrivateMediaSourceAVFObjC::setVideoTarget(const PlatformVideoTarget& videoTarget)
{
    ALWAYS_LOG(LOGIDENTIFIER, !!videoTarget);
    m_renderer->setVideoTarget(videoTarget);
}
#endif

#if PLATFORM(IOS_FAMILY)
void MediaPlayerPrivateMediaSourceAVFObjC::sceneIdentifierDidChange()
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

void MediaPlayerPrivateMediaSourceAVFObjC::applicationWillResignActive()
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER);
    m_renderer->applicationWillResignActive();
    m_applicationIsActive = false;
}

void MediaPlayerPrivateMediaSourceAVFObjC::applicationDidBecomeActive()
{
    assertIsMainThread();
    ALWAYS_LOG(LOGIDENTIFIER);
    m_applicationIsActive = true;
    flushVideoIfNeeded();
}
#endif

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsLimitedMatroska() const
{
    assertIsMainThread();
    return m_loadOptions.supportsLimitedMatroska;
}

void MediaPlayerPrivateMediaSourceAVFObjC::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
    m_renderer->isInFullscreenOrPictureInPictureChanged(isInFullscreenOrPictureInPicture);
}

WebCore::HostingContext MediaPlayerPrivateMediaSourceAVFObjC::hostingContext() const
{
    return m_renderer->hostingContext();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoLayerSizeFenced(const WebCore::FloatSize& size, WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    m_renderer->setVideoLayerSizeFenced(size, WTFMove(sendRightAnnotated));
}

} // namespace WebCore

#endif
