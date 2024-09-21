/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#import "CDMSessionMediaSourceAVFObjC.h"
#import "ContentTypeUtilities.h"
#import "GraphicsContext.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaSessionManagerCocoa.h"
#import "MediaSourcePrivate.h"
#import "MediaSourcePrivateAVFObjC.h"
#import "MediaSourcePrivateClient.h"
#import "PixelBufferConformerCV.h"
#import "PlatformScreen.h"
#import "SourceBufferPrivateAVFObjC.h"
#import "TextTrackRepresentation.h"
#import "VideoFrameCV.h"
#import "VideoLayerManagerObjC.h"
#import "WebCoreDecompressionSession.h"
#import "WebSampleBufferVideoRendering.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CMTime.h>
#import <QuartzCore/CALayer.h>
#import <objc_runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/Deque.h>
#import <wtf/FileSystem.h>
#import <wtf/MainThread.h>
#import <wtf/NativePromise.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>

#import "CoreVideoSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/MediaToolboxSoftLink.h>

@interface AVSampleBufferDisplayLayer (Staging_100128644)
@property (assign, nonatomic) BOOL preventsAutomaticBackgroundingDuringVideoPlayback;
@end

#if ENABLE(LINEAR_MEDIA_PLAYER)
@interface AVSampleBufferVideoRenderer (Staging_127455709)
- (void)removeAllVideoTargets;
@end
#endif

namespace WebCore {

String convertEnumerationToString(MediaPlayerPrivateMediaSourceAVFObjC::SeekState enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("Seeking"),
        MAKE_STATIC_STRING_IMPL("WaitingForAvailableFame"),
        MAKE_STATIC_STRING_IMPL("SeekCompleted"),
    };
    static_assert(!static_cast<size_t>(MediaPlayerPrivateMediaSourceAVFObjC::SeekState::Seeking), "MediaPlayerPrivateMediaSourceAVFObjC::SeekState::Seeking is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayerPrivateMediaSourceAVFObjC::SeekState::WaitingForAvailableFame) == 1, "MediaPlayerPrivateMediaSourceAVFObjC::SeekState::WaitingForAvailableFame is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayerPrivateMediaSourceAVFObjC::SeekState::SeekCompleted) == 2, "MediaPlayerPrivateMediaSourceAVFObjC::SeekState::SeekCompleted is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)

static bool isCopyDisplayedPixelBufferAvailable()
{
    static auto result = [] {
        return [PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(copyDisplayedPixelBuffer)];
    }();
    return MediaSessionManagerCocoa::mediaSourceInlinePaintingEnabled() && result;
}

#endif // HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)

#pragma mark -
#pragma mark MediaPlayerPrivateMediaSourceAVFObjC

class EffectiveRateChangedListener : public ThreadSafeRefCounted<EffectiveRateChangedListener> {
public:
    static Ref<EffectiveRateChangedListener> create(MediaPlayerPrivateMediaSourceAVFObjC& client, CMTimebaseRef timebase)
    {
        return adoptRef(*new EffectiveRateChangedListener(client, timebase));
    }

    void effectiveRateChanged()
    {
        callOnMainThread([this, protectedThis = Ref { *this }] {
            if (m_client)
                m_client->effectiveRateChanged();
        });
    }

    void stop(CMTimebaseRef);

private:
    EffectiveRateChangedListener(MediaPlayerPrivateMediaSourceAVFObjC&, CMTimebaseRef);

    WeakPtr<MediaPlayerPrivateMediaSourceAVFObjC> m_client;
};

static void timebaseEffectiveRateChangedCallback(CFNotificationCenterRef, void* observer, CFNotificationName, const void*, CFDictionaryRef)
{
    static_cast<EffectiveRateChangedListener*>(observer)->effectiveRateChanged();
}

void EffectiveRateChangedListener::stop(CMTimebaseRef timebase)
{
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), this, kCMTimebaseNotification_EffectiveRateChanged, timebase);
}

EffectiveRateChangedListener::EffectiveRateChangedListener(MediaPlayerPrivateMediaSourceAVFObjC& client, CMTimebaseRef timebase)
    : m_client(client)
{
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, timebaseEffectiveRateChangedCallback, kCMTimebaseNotification_EffectiveRateChanged, timebase, static_cast<CFNotificationSuspensionBehavior>(0));
}

MediaPlayerPrivateMediaSourceAVFObjC::MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_synchronizer(adoptNS([PAL::allocAVSampleBufferRenderSynchronizerInstance() init]))
    , m_seekTimer(*this, &MediaPlayerPrivateMediaSourceAVFObjC::seekInternal)
    , m_networkState(MediaPlayer::NetworkState::Empty)
    , m_readyState(MediaPlayer::ReadyState::HaveNothing)
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
    , m_videoLayerManager(makeUnique<VideoLayerManagerObjC>(m_logger, m_logIdentifier))
    , m_effectiveRateChangedListener(EffectiveRateChangedListener::create(*this, [m_synchronizer timebase]))
{
    auto logSiteIdentifier = LOGIDENTIFIER;
    ALWAYS_LOG(logSiteIdentifier);
    UNUSED_PARAM(logSiteIdentifier);

#if HAVE(SPATIAL_TRACKING_LABEL)
    m_defaultSpatialTrackingLabel = player->defaultSpatialTrackingLabel();
    m_spatialTrackingLabel = player->spatialTrackingLabel();
#endif

    // addPeriodicTimeObserverForInterval: throws an exception if you pass a non-numeric CMTime, so just use
    // an arbitrarily large time value of once an hour:
    __block WeakPtr weakThis { *this };
    m_timeJumpedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::toCMTime(MediaTime::createWithDouble(3600)) queue:dispatch_get_main_queue() usingBlock:^(CMTime time) {
#if LOG_DISABLED
        UNUSED_PARAM(time);
#endif
        if (!weakThis)
            return;

        auto clampedTime = CMTIME_IS_NUMERIC(time) ? clampTimeToSensicalValue(PAL::toMediaTime(time)) : MediaTime::zeroTime();
        ALWAYS_LOG(logSiteIdentifier, "synchronizer fired: time clamped = ", clampedTime, ", seeking = ", m_isSynchronizerSeeking, ", pending = ", !!m_pendingSeek);

        if (m_isSynchronizerSeeking && !m_pendingSeek) {
            m_isSynchronizerSeeking = false;
            maybeCompleteSeek();
        }

        if (m_pendingSeek)
            seekInternal();

        if (m_currentTimeDidChangeCallback)
            m_currentTimeDidChangeCallback(clampedTime);
    }];

#if ENABLE(LINEAR_MEDIA_PLAYER)
    setVideoTarget(player->videoTarget());
#endif
}

MediaPlayerPrivateMediaSourceAVFObjC::~MediaPlayerPrivateMediaSourceAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_effectiveRateChangedListener->stop([m_synchronizer timebase]);

    if (m_timeJumpedObserver)
        [m_synchronizer removeTimeObserver:m_timeJumpedObserver.get()];
    if (m_timeChangedObserver)
        [m_synchronizer removeTimeObserver:m_timeChangedObserver.get()];
    if (m_videoFrameMetadataGatheringObserver)
        [m_synchronizer removeTimeObserver:m_videoFrameMetadataGatheringObserver.get()];
    if (m_gapObserver)
        [m_synchronizer removeTimeObserver:m_gapObserver.get()];
    flushPendingSizeChanges();

    destroyLayer();
    destroyDecompressionSession();

    m_seekTimer.stop();
}

#pragma mark -
#pragma mark MediaPlayer Factory Methods

class MediaPlayerFactoryMediaSourceAVFObjC final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryMediaSourceAVFObjC);
public:
    MediaPlayerFactoryMediaSourceAVFObjC()
    {
        MediaSessionManagerCocoa::ensureCodecsRegistered();
    }

private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
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
        && PAL::getAVStreamDataParserClass()
        && PAL::getAVSampleBufferAudioRendererClass()
        && PAL::getAVSampleBufferRenderSynchronizerClass()
        && class_getInstanceMethod(PAL::getAVSampleBufferAudioRendererClass(), @selector(setMuted:));
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
    // This media engine only supports MediaSource URLs.
    m_networkState = MediaPlayer::NetworkState::FormatError;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::load(const URL&, const ContentType&, MediaSourcePrivateClient& client)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (RefPtr mediaSourcePrivate = downcast<MediaSourcePrivateAVFObjC>(client.mediaSourcePrivate())) {
        mediaSourcePrivate->setPlayer(this);
        m_mediaSourcePrivate = WTFMove(mediaSourcePrivate);
        client.reOpen();
    } else
        m_mediaSourcePrivate = MediaSourcePrivateAVFObjC::create(*this, client);
    m_mediaSourcePrivate->setResourceOwner(m_resourceOwner);
    m_mediaSourcePrivate->setVideoRenderer(layerOrVideoRenderer());
    m_mediaSourcePrivate->setDecompressionSession(m_decompressionSession.get());

    acceleratedRenderingStateChanged();
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
    return m_videoLayerManager->videoInlineLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::play()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    playInternal();
}

void MediaPlayerPrivateMediaSourceAVFObjC::playInternal(std::optional<MonotonicTime>&& hostTime)
{
    if (!m_mediaSourcePrivate)
        return;

    if (currentTime() >= m_mediaSourcePrivate->duration()) {
        ALWAYS_LOG(LOGIDENTIFIER, "bailing, current time: ", currentTime(), " greater than duration ", m_mediaSourcePrivate->duration());
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER);
    m_mediaSourcePrivate->flushActiveSourceBuffersIfNeeded();
    m_isPlaying = true;
    if (!shouldBePlaying())
        return;

    setSynchronizerRate(m_rate, WTFMove(hostTime));
}

void MediaPlayerPrivateMediaSourceAVFObjC::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    pauseInternal();
}

void MediaPlayerPrivateMediaSourceAVFObjC::pauseInternal(std::optional<MonotonicTime>&& hostTime)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_isPlaying = false;

    setSynchronizerRate(0, WTFMove(hostTime));
}

bool MediaPlayerPrivateMediaSourceAVFObjC::paused() const
{
    return !m_isPlaying;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVolume(float volume)
{
    ALWAYS_LOG(LOGIDENTIFIER, volume);
    for (const auto& key : m_sampleBufferAudioRendererMap.keys())
        [(__bridge AVSampleBufferAudioRenderer *)key.get() setVolume:volume];
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsScanning() const
{
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setMuted(bool muted)
{
    ALWAYS_LOG(LOGIDENTIFIER, muted);
    for (const auto& key : m_sampleBufferAudioRendererMap.keys())
        [(__bridge AVSampleBufferAudioRenderer *)key.get() setMuted:muted];
}

FloatSize MediaPlayerPrivateMediaSourceAVFObjC::naturalSize() const
{
    return m_naturalSize;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasVideo() const
{
    if (!m_mediaSourcePrivate)
        return false;

    return m_mediaSourcePrivate->hasVideo();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasAudio() const
{
    if (!m_mediaSourcePrivate)
        return false;

    return m_mediaSourcePrivate->hasAudio();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPageIsVisible(bool visible)
{
    if (m_visible == visible)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, visible);
    m_visible = visible;
    if (m_visible) {
        acceleratedRenderingStateChanged();

        // Rendering may have been interrupted while the page was in a non-visible
        // state, which would require a flush to resume decoding.
        if (m_mediaSourcePrivate) {
            SetForScope(m_flushingActiveSourceBuffersDueToVisibilityChange, true, false);
            m_mediaSourcePrivate->flushActiveSourceBuffersIfNeeded();
        }
    }


#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::duration() const
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->duration() : MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::currentTime() const
{
    if (seeking())
        return m_pendingSeek ? m_pendingSeek->time : m_lastSeekTime;
    return clampTimeToSensicalValue(PAL::toMediaTime(PAL::CMTimebaseGetTime([m_synchronizer timebase])));
}

bool MediaPlayerPrivateMediaSourceAVFObjC::timeIsProgressing() const
{
    return m_isPlaying && [m_synchronizer rate];
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::clampTimeToSensicalValue(const MediaTime& time) const
{
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
    m_currentTimeDidChangeCallback = WTFMove(callback);

    if (m_currentTimeDidChangeCallback) {
        m_timeChangedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::CMTimeMake(1, 10) queue:dispatch_get_main_queue() usingBlock:^(CMTime time) {
            if (!m_currentTimeDidChangeCallback)
                return;

            auto clampedTime = CMTIME_IS_NUMERIC(time) ? clampTimeToSensicalValue(PAL::toMediaTime(time)) : MediaTime::zeroTime();
            m_currentTimeDidChangeCallback(clampedTime);
        }];

    } else
        m_timeChangedObserver = nullptr;

    return true;
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
    ALWAYS_LOG(LOGIDENTIFIER, "time = ", target.time, ", negativeThreshold = ", target.negativeThreshold, ", positiveThreshold = ", target.positiveThreshold);

    m_pendingSeek = target;

    if (m_seekTimer.isActive())
        m_seekTimer.stop();
    m_seekTimer.startOneShot(0_s);
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekInternal()
{
    if (!m_pendingSeek)
        return;

    if (!m_mediaSourcePrivate)
        return;

    auto pendingSeek = std::exchange(m_pendingSeek, { }).value();
    m_lastSeekTime = pendingSeek.time;

    m_seekState = Seeking;
    m_mediaSourcePrivate->waitForTarget(pendingSeek)->whenSettled(RunLoop::current(), [this, weakThis = WeakPtr { *this }] (auto&& result) mutable {
        if (!weakThis)
            return;
        if (m_seekState != Seeking || !result) {
            ALWAYS_LOG(LOGIDENTIFIER, "seek Interrupted, aborting");
            return;
        }
        auto seekedTime = *result;
        m_lastSeekTime = seekedTime;

        ALWAYS_LOG(LOGIDENTIFIER);
        MediaTime synchronizerTime = PAL::toMediaTime([m_synchronizer currentTime]);

        m_isSynchronizerSeeking = synchronizerTime != seekedTime;
        ALWAYS_LOG(LOGIDENTIFIER, "seekedTime = ", seekedTime, ", synchronizerTime = ", synchronizerTime, "synchronizer seeking = ", m_isSynchronizerSeeking);

        if (!m_isSynchronizerSeeking) {
            // In cases where the destination seek time precisely matches the synchronizer's existing time
            // no time jumped notification will be issued. In this case, just notify the MediaPlayer that
            // the seek completed successfully.
            maybeCompleteSeek();
            return;
        }
        m_mediaSourcePrivate->willSeek();
        [m_synchronizer setRate:0 time:PAL::toCMTime(seekedTime)];

        m_mediaSourcePrivate->seekToTime(seekedTime)->whenSettled(RunLoop::current(), [this, weakThis = WTFMove(weakThis)]() mutable {
            if (weakThis)
                maybeCompleteSeek();
        });
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::maybeCompleteSeek()
{
    if (m_seekState == SeekCompleted)
        return;
    if (hasVideo() && hasVideoRenderer() && !m_hasAvailableVideoFrame) {
        ALWAYS_LOG(LOGIDENTIFIER, "waiting for video frame");
        m_seekState = WaitingForAvailableFame;
        return;
    }
    m_seekState = Seeking;
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_isSynchronizerSeeking) {
        ALWAYS_LOG(LOGIDENTIFIER, "Synchronizer still seeking, bailing out");
        return;
    }
    m_seekState = SeekCompleted;
    if (shouldBePlaying())
        setSynchronizerRate(m_rate);
    if (auto player = m_player.get()) {
        player->seeked(m_lastSeekTime);
        player->timeChanged();
    }
}

bool MediaPlayerPrivateMediaSourceAVFObjC::seeking() const
{
    return m_pendingSeek || m_seekState != SeekCompleted;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setRateDouble(double rate)
{
    // AVSampleBufferRenderSynchronizer does not support negative rate yet.
    m_rate = std::max<double>(rate, 0);

    if (auto player = m_player.get()) {
        auto algorithm = MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player->pitchCorrectionAlgorithm(), player->preservesPitch(), m_rate);
        for (const auto& key : m_sampleBufferAudioRendererMap.keys())
            [(__bridge AVSampleBufferAudioRenderer *)key.get() setAudioTimePitchAlgorithm:algorithm];
    }

    if (shouldBePlaying())
        setSynchronizerRate(m_rate);
}

double MediaPlayerPrivateMediaSourceAVFObjC::rate() const
{
    return m_rate;
}

double MediaPlayerPrivateMediaSourceAVFObjC::effectiveRate() const
{
    return PAL::CMTimebaseGetRate([m_synchronizer timebase]);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPreservesPitch(bool preservesPitch)
{
    ALWAYS_LOG(LOGIDENTIFIER, preservesPitch);
    if (auto player = m_player.get()) {
        auto algorithm = MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player->pitchCorrectionAlgorithm(), preservesPitch, m_rate);
        for (const auto& key : m_sampleBufferAudioRendererMap.keys())
            [(__bridge AVSampleBufferAudioRenderer *)key.get() setAudioTimePitchAlgorithm:algorithm];
    }
}

MediaPlayer::NetworkState MediaPlayerPrivateMediaSourceAVFObjC::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateMediaSourceAVFObjC::readyState() const
{
    return m_readyState;
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
    ASSERT_NOT_REACHED();
    return PlatformTimeRanges::emptyRanges();
}

void MediaPlayerPrivateMediaSourceAVFObjC::bufferedChanged()
{
    if (m_gapObserver) {
        [m_synchronizer removeTimeObserver:m_gapObserver.get()];
        m_gapObserver = nullptr;
    }

    auto ranges = m_mediaSourcePrivate->buffered();
    auto currentTime = this->currentTime();
    size_t index = ranges.find(currentTime);
    if (index == notFound)
        return;
    // Find the next gap (or end of media)
    for (; index < ranges.length(); index++) {
        if ((index < ranges.length() - 1 && ranges.start(index + 1) - ranges.end(index) > m_mediaSourcePrivate->timeFudgeFactor())
            || (index == ranges.length() - 1 && ranges.end(index) > currentTime)) {
            auto gapStart = ranges.end(index);
            NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(gapStart)]];

            auto logSiteIdentifier = LOGIDENTIFIER;
            UNUSED_PARAM(logSiteIdentifier);

            m_gapObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:dispatch_get_main_queue() usingBlock:[weakThis = WeakPtr { *this }, this, logSiteIdentifier, gapStart] {
                if (!weakThis)
                    return;
                if (m_mediaSourcePrivate->hasFutureTime(gapStart))
                    return; // New data was added, don't stall.
                MediaTime now = weakThis->currentTime();
                ALWAYS_LOG(logSiteIdentifier, "boundary time observer called, now = ", now);

                if (gapStart == duration())
                    weakThis->pauseInternal();
                // Experimentation shows that between the time the boundary time observer is called, the time have progressed by a few milliseconds. Re-adjust time. This seek doesn't require re-enqueuing/flushing.
                [m_synchronizer setRate:0 time:PAL::toCMTime(gapStart)];
                if (auto player = m_player.get())
                    player->timeChanged();
            }];
            return;
        }
    }
}

bool MediaPlayerPrivateMediaSourceAVFObjC::didLoadingProgress() const
{
    bool loadingProgressed = m_loadingProgressed;
    m_loadingProgressed = false;
    return loadingProgressed;
}

RefPtr<NativeImage> MediaPlayerPrivateMediaSourceAVFObjC::nativeImageForCurrentTime()
{
    updateLastImage();
    return m_lastImage;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::updateLastPixelBuffer()
{
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    if (isCopyDisplayedPixelBufferAvailable()) {
        if (auto pixelBuffer = adoptCF([layerOrVideoRenderer() copyDisplayedPixelBuffer])) {
            INFO_LOG(LOGIDENTIFIER, "displayed pixelbuffer copied for time ", currentTime());
            m_lastPixelBuffer = WTFMove(pixelBuffer);
            return true;
        }
    }
#endif

    if (layerOrVideoRenderer() || !m_decompressionSession)
        return false;

    auto flags = !m_lastPixelBuffer ? WebCoreDecompressionSession::AllowLater : WebCoreDecompressionSession::ExactTime;
    auto newPixelBuffer = m_decompressionSession->imageForTime(currentTime(), flags);
    if (!newPixelBuffer)
        return false;

    m_lastPixelBuffer = WTFMove(newPixelBuffer);

    if (m_resourceOwner) {
        if (auto surface = CVPixelBufferGetIOSurface(m_lastPixelBuffer.get()))
            IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
    }

    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::updateLastImage()
{
    if (m_isGatheringVideoFrameMetadata) {
        if (!m_lastPixelBuffer)
            return false;
        if (m_sampleCount == m_lastConvertedSampleCount)
            return false;
        m_lastConvertedSampleCount = m_sampleCount;
    } else if (!updateLastPixelBuffer())
        return false;

    ASSERT(m_lastPixelBuffer);

    if (!m_rgbConformer) {
        auto attributes = @{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
        m_rgbConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)attributes);
    }

    m_lastImage = NativeImage::create(m_rgbConformer->createImageFromPixelBuffer(m_lastPixelBuffer.get()));
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::maybePurgeLastImage()
{
    // If we are in the middle of a rVFC operation, do not purge anything:
    if (m_isGatheringVideoFrameMetadata)
        return;

    m_lastImage = nullptr;
    m_lastPixelBuffer = nullptr;
}

void MediaPlayerPrivateMediaSourceAVFObjC::paint(GraphicsContext& context, const FloatRect& rect)
{
    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateMediaSourceAVFObjC::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& outputRect)
{
    if (context.paintingDisabled())
        return;

    auto image = nativeImageForCurrentTime();
    if (!image)
        return;

    GraphicsContextStateSaver stateSaver(context);
    FloatRect imageRect { FloatPoint::zero(), image->size() };
    context.drawNativeImage(*image, outputRect, imageRect);
}

#if !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
void MediaPlayerPrivateMediaSourceAVFObjC::willBeAskedToPaintGL()
{
    // We have been asked to paint into a WebGL canvas, so take that as a signal to create
    // a decompression session, even if that means the native video can't also be displayed
    // in page.
    if (m_hasBeenAskedToPaintGL)
        return;

    m_hasBeenAskedToPaintGL = true;
    acceleratedRenderingStateChanged();
}
#endif

RefPtr<VideoFrame> MediaPlayerPrivateMediaSourceAVFObjC::videoFrameForCurrentTime()
{
    if (!m_isGatheringVideoFrameMetadata)
        updateLastPixelBuffer();
    if (!m_lastPixelBuffer)
        return nullptr;
    return VideoFrameCV::create(currentTime(), false, VideoFrame::Rotation::None, RetainPtr { m_lastPixelBuffer });
}

DestinationColorSpace MediaPlayerPrivateMediaSourceAVFObjC::colorSpace()
{
    updateLastImage();
    return m_lastImage ? m_lastImage->colorSpace() : DestinationColorSpace::SRGB();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasAvailableVideoFrame() const
{
    return m_hasAvailableVideoFrame;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsAcceleratedRendering() const
{
    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::shouldEnsureLayerOrVideoRenderer() const
{
    // Decompression sessions do not support encrypted content; force layer
    // creation.
    if (m_mediaSourcePrivate && m_mediaSourcePrivate->cdmInstance())
        return true;
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    return isCopyDisplayedPixelBufferAvailable() && [&] {
        if (m_mediaSourcePrivate && m_mediaSourcePrivate->needsVideoLayer())
            return true;
        auto player = m_player.get();
        return player && player->renderingCanBeAccelerated();
    }();
#else
    return !m_hasBeenAskedToPaintGL;
#endif
}

void MediaPlayerPrivateMediaSourceAVFObjC::setPresentationSize(const IntSize& newSize)
{
    if (!layerOrVideoRenderer() && !newSize.isEmpty())
        updateDisplayLayerAndDecompressionSession();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoLayerSizeFenced(const FloatSize& newSize, WTF::MachSendRight&&)
{
    if (!layerOrVideoRenderer() && !newSize.isEmpty())
        updateDisplayLayerAndDecompressionSession();
}

void MediaPlayerPrivateMediaSourceAVFObjC::acceleratedRenderingStateChanged()
{
    updateDisplayLayerAndDecompressionSession();
}

void MediaPlayerPrivateMediaSourceAVFObjC::updateDisplayLayerAndDecompressionSession()
{
    if (shouldEnsureLayerOrVideoRenderer()) {
        auto needsRenderingModeChanged = destroyDecompressionSession();
        ensureLayerOrVideoRenderer(needsRenderingModeChanged);
        return;
    }

    destroyLayerOrVideoRenderer();
    ensureDecompressionSession();
}

void MediaPlayerPrivateMediaSourceAVFObjC::notifyActiveSourceBuffersChanged()
{
    if (auto player = m_player.get())
        player->activeSourceBuffersChanged();
}

MediaPlayer::MovieLoadType MediaPlayerPrivateMediaSourceAVFObjC::movieLoadType() const
{
    return MediaPlayer::MovieLoadType::StoredStream;
}

void MediaPlayerPrivateMediaSourceAVFObjC::prepareForRendering()
{
    // No-op.
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
    if (m_decompressionSession) {
        return VideoPlaybackQualityMetrics {
            m_decompressionSession->totalVideoFrames(),
            m_decompressionSession->droppedVideoFrames(),
            m_decompressionSession->corruptedVideoFrames(),
            m_decompressionSession->totalFrameDelay().toDouble(),
            0,
        };
    }

    auto metrics = [layerOrVideoRenderer() videoPerformanceMetrics];
    if (!metrics)
        return std::nullopt;

    uint32_t displayCompositedFrames = 0;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if ([metrics respondsToSelector:@selector(numberOfDisplayCompositedVideoFrames)])
        displayCompositedFrames = [metrics numberOfDisplayCompositedVideoFrames];
ALLOW_NEW_API_WITHOUT_GUARDS_END

    return VideoPlaybackQualityMetrics {
        static_cast<uint32_t>([metrics totalNumberOfVideoFrames]),
        static_cast<uint32_t>([metrics numberOfDroppedVideoFrames]),
        static_cast<uint32_t>([metrics numberOfCorruptedVideoFrames]),
        [metrics totalFrameDelay],
        displayCompositedFrames,
    };
}

#pragma mark -
#pragma mark Utility Methods

void MediaPlayerPrivateMediaSourceAVFObjC::ensureLayer()
{
    if (m_sampleBufferDisplayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_sampleBufferDisplayLayer = adoptNS([PAL::allocAVSampleBufferDisplayLayerInstance() init]);
    if (!m_sampleBufferDisplayLayer)
        return;

#ifndef NDEBUG
    [m_sampleBufferDisplayLayer setName:@"MediaPlayerPrivateMediaSource AVSampleBufferDisplayLayer"];
#endif
    [m_sampleBufferDisplayLayer setVideoGravity: (m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    configureLayerOrVideoRenderer(m_sampleBufferDisplayLayer.get());

    if (RefPtr player = m_player.get()) {
        if ([m_sampleBufferDisplayLayer respondsToSelector:@selector(setToneMapToStandardDynamicRange:)])
            [m_sampleBufferDisplayLayer setToneMapToStandardDynamicRange:player->shouldDisableHDR()];

        m_videoLayerManager->setVideoLayer(m_sampleBufferDisplayLayer.get(), player->presentationSize());
    }
}

void MediaPlayerPrivateMediaSourceAVFObjC::destroyLayer()
{
    if (!m_sampleBufferDisplayLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime completionHandler:nil];

    m_videoLayerManager->didDestroyVideoLayer();
    m_sampleBufferDisplayLayer = nullptr;
}

void MediaPlayerPrivateMediaSourceAVFObjC::ensureVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_sampleBufferVideoRenderer = adoptNS([PAL::allocAVSampleBufferVideoRendererInstance() init]);
    if (!m_sampleBufferVideoRenderer)
        return;

    [m_sampleBufferVideoRenderer addVideoTarget:m_videoTarget.get()];

    configureLayerOrVideoRenderer(m_sampleBufferVideoRenderer.get());
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

void MediaPlayerPrivateMediaSourceAVFObjC::destroyVideoRenderer()
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (!m_sampleBufferVideoRenderer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferVideoRenderer.get() atTime:currentTime completionHandler:nil];

    if ([m_sampleBufferVideoRenderer respondsToSelector:@selector(removeAllVideoTargets)])
        [m_sampleBufferVideoRenderer removeAllVideoTargets];
    m_sampleBufferVideoRenderer = nullptr;
#endif // ENABLE(LINEAR_MEDIA_PLAYER)
}

void MediaPlayerPrivateMediaSourceAVFObjC::ensureDecompressionSession()
{
    if (m_decompressionSession)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_decompressionSession = WebCoreDecompressionSession::createOpenGL();
    m_decompressionSession->setTimebase([m_synchronizer timebase]);
    m_decompressionSession->setResourceOwner(m_resourceOwner);

    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->setDecompressionSession(m_decompressionSession.get());

    if (auto player = m_player.get())
        player->renderingModeChanged();
}

MediaPlayerEnums::NeedsRenderingModeChanged MediaPlayerPrivateMediaSourceAVFObjC::destroyDecompressionSession()
{
    if (!m_decompressionSession)
        return MediaPlayerEnums::NeedsRenderingModeChanged::No;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->setDecompressionSession(nullptr);

    m_decompressionSession->invalidate();
    m_decompressionSession = nullptr;
    setHasAvailableVideoFrame(false);
    return MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
}

void MediaPlayerPrivateMediaSourceAVFObjC::ensureLayerOrVideoRenderer(MediaPlayerEnums::NeedsRenderingModeChanged needsRenderingModeChanged)
{
    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
        destroyVideoRenderer();
        FALLTHROUGH;
    case AcceleratedVideoMode::StagedLayer:
        ensureLayer();
        break;
    case AcceleratedVideoMode::VideoRenderer:
        destroyLayer();
        FALLTHROUGH;
    case AcceleratedVideoMode::StagedVideoRenderer:
        ensureVideoRenderer();
        break;
    }

    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    RetainPtr renderer = layerOrVideoRenderer();

    if (!renderer) {
        ERROR_LOG(LOGIDENTIFIER, "Failed to create AVSampleBufferDisplayLayer or AVSampleBufferVideoRenderer");
        if (mediaSourcePrivate)
            mediaSourcePrivate->failedToCreateRenderer(MediaSourcePrivateAVFObjC::RendererType::Video);
        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, acceleratedVideoMode(), ", renderer=", !!renderer);

    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
#if ENABLE(LINEAR_MEDIA_PLAYER)
        if (!m_usingLinearMediaPlayer)
            needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
#else
        needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
#endif
        FALLTHROUGH;
    case AcceleratedVideoMode::VideoRenderer:
        if (mediaSourcePrivate)
            mediaSourcePrivate->setVideoRenderer(renderer.get());
        break;
    case AcceleratedVideoMode::StagedLayer:
        needsRenderingModeChanged = MediaPlayerEnums::NeedsRenderingModeChanged::Yes;
        FALLTHROUGH;
    case AcceleratedVideoMode::StagedVideoRenderer:
        if (mediaSourcePrivate)
            mediaSourcePrivate->stageVideoRenderer(renderer.get());
        break;
    }

    switch (needsRenderingModeChanged) {
    case MediaPlayerEnums::NeedsRenderingModeChanged::Yes:
        if (RefPtr player = m_player.get())
            player->renderingModeChanged();
        break;
    case MediaPlayerEnums::NeedsRenderingModeChanged::No:
        break;
    }
}

void MediaPlayerPrivateMediaSourceAVFObjC::destroyLayerOrVideoRenderer()
{
    destroyLayer();
    destroyVideoRenderer();

    if (RefPtr mediaSourcePrivate = m_mediaSourcePrivate)
        mediaSourcePrivate->setVideoRenderer(nil);

    setHasAvailableVideoFrame(false);

    if (RefPtr player = m_player.get())
        player->renderingModeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::configureLayerOrVideoRenderer(WebSampleBufferVideoRendering *renderer)
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    updateSpatialTrackingLabel();
#endif

    renderer.preventsDisplaySleepDuringVideoPlayback = NO;

    if ([renderer respondsToSelector:@selector(setPreventsAutomaticBackgroundingDuringVideoPlayback:)])
        renderer.preventsAutomaticBackgroundingDuringVideoPlayback = NO;

    @try {
        [m_synchronizer addRenderer:renderer];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }
}

MediaPlayerPrivateMediaSourceAVFObjC::AcceleratedVideoMode MediaPlayerPrivateMediaSourceAVFObjC::acceleratedVideoMode() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_usingLinearMediaPlayer) {
        RefPtr player = m_player.get();
        if (player && player->isInFullscreenOrPictureInPicture()) {
            if (m_videoTarget)
                return AcceleratedVideoMode::VideoRenderer;
            return AcceleratedVideoMode::StagedLayer;
        }

        if (m_videoTarget)
            return AcceleratedVideoMode::StagedVideoRenderer;
    }
#endif // ENABLE(LINEAR_MEDIA_PLAYER)

    return AcceleratedVideoMode::Layer;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::shouldBePlaying() const
{
    return m_isPlaying && !seeking() && (m_flushingActiveSourceBuffersDueToVisibilityChange || allRenderersHaveAvailableSamples()) && m_readyState >= MediaPlayer::ReadyState::HaveFutureData;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setSynchronizerRate(double rate, std::optional<MonotonicTime>&& hostTime)
{
    if (hostTime) {
        auto cmHostTime = PAL::CMClockMakeHostTimeFromSystemUnits(hostTime->toMachAbsoluteTime());
        ALWAYS_LOG(LOGIDENTIFIER, "setting rate to ", m_rate, " at host time ", PAL::CMTimeGetSeconds(cmHostTime));
        [m_synchronizer setRate:rate time:PAL::kCMTimeInvalid atHostTime:cmHostTime];
    } else
        [m_synchronizer setRate:rate];

    // If we are pausing the synchronizer, update the last image to ensure we have something
    // to display if and when the decoders are purged while in the background. And vice-versa,
    // purge our retained images and pixel buffers when playing the synchronizer, to release that
    // retained memory.
    if (!rate)
        updateLastPixelBuffer();
    else
        maybePurgeLastImage();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setHasAvailableVideoFrame(bool flag)
{
    if (m_hasAvailableVideoFrame == flag)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, flag);
    m_hasAvailableVideoFrame = flag;
    updateAllRenderersHaveAvailableSamples();

    if (!m_hasAvailableVideoFrame)
        return;

    setNeedsPlaceholderImage(false);

    auto player = m_player.get();
    if (player)
        player->firstVideoFrameAvailable();
    if (m_seekState == WaitingForAvailableFame)
        maybeCompleteSeek();

    if (m_readyStateIsWaitingForAvailableFrame) {
        m_readyStateIsWaitingForAvailableFrame = false;
        if (player)
            player->readyStateChanged();
    }
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::setHasAvailableAudioSample(AVSampleBufferAudioRenderer* renderer, bool flag)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    auto iter = m_sampleBufferAudioRendererMap.find((__bridge CFTypeRef)renderer);
    if (iter == m_sampleBufferAudioRendererMap.end())
        return;

    auto& properties = iter->value;
    if (properties.hasAudibleSample == flag)
        return;
    ALWAYS_LOG(LOGIDENTIFIER, flag);
    properties.hasAudibleSample = flag;
    updateAllRenderersHaveAvailableSamples();
}

void MediaPlayerPrivateMediaSourceAVFObjC::updateAllRenderersHaveAvailableSamples()
{
    bool allRenderersHaveAvailableSamples = true;

    do {
        if (hasVideo() && hasVideoRenderer() && !m_hasAvailableVideoFrame) {
            allRenderersHaveAvailableSamples = false;
            break;
        }

        for (auto& properties : m_sampleBufferAudioRendererMap.values()) {
            if (!properties.hasAudibleSample) {
                allRenderersHaveAvailableSamples = false;
                break;
            }
        }
    } while (0);

    if (m_allRenderersHaveAvailableSamples == allRenderersHaveAvailableSamples)
        return;

    DEBUG_LOG(LOGIDENTIFIER, allRenderersHaveAvailableSamples);
    m_allRenderersHaveAvailableSamples = allRenderersHaveAvailableSamples;

    if (shouldBePlaying() && [m_synchronizer rate] != m_rate)
        setSynchronizerRate(m_rate);
    else if (!shouldBePlaying() && [m_synchronizer rate])
        setSynchronizerRate(0);
}

void MediaPlayerPrivateMediaSourceAVFObjC::durationChanged()
{
    if (!m_mediaSourcePrivate)
        return;

    MediaTime duration = m_mediaSourcePrivate->duration();
    // Avoid emiting durationchanged in the case where the previous duration was unkniwn as that case is already handled
    // by the HTMLMediaElement.
    if (m_duration != duration && m_duration.isValid()) {
        if (auto player = m_player.get())
            player->durationChanged();
    }
    m_duration = duration;
}

void MediaPlayerPrivateMediaSourceAVFObjC::effectiveRateChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER, effectiveRate());
    if (auto player = m_player.get())
        player->rateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::sizeWillChangeAtTime(const MediaTime& time, const FloatSize& size)
{
    auto weakThis = m_sizeChangeObserverWeakPtrFactory.createWeakPtr(*this);
    NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(time)]];
    RetainPtr<id> observer = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:dispatch_get_main_queue() usingBlock:[this, weakThis = WTFMove(weakThis), size] {
        if (!weakThis)
            return;

        ASSERT(!m_sizeChangeObservers.isEmpty());
        if (!m_sizeChangeObservers.isEmpty()) {
            RetainPtr<id> observer = m_sizeChangeObservers.takeFirst();
            [m_synchronizer removeTimeObserver:observer.get()];
        }
        setNaturalSize(size);
    }];
    m_sizeChangeObservers.append(WTFMove(observer));

    if (currentTime() >= time)
        setNaturalSize(size);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNaturalSize(const FloatSize& size)
{
    if (size == m_naturalSize)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, size);

    m_naturalSize = size;
    if (auto player = m_player.get())
        player->sizeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::flushPendingSizeChanges()
{
    while (!m_sizeChangeObservers.isEmpty()) {
        RetainPtr<id> observer = m_sizeChangeObservers.takeFirst();
        [m_synchronizer removeTimeObserver:observer.get()];
    }
    m_sizeChangeObserverWeakPtrFactory.revokeAll();
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
CDMSessionMediaSourceAVFObjC* MediaPlayerPrivateMediaSourceAVFObjC::cdmSession() const
{
    return m_session.get();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setCDMSession(LegacyCDMSession* session)
{
    if (session == m_session)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_session = toCDMSessionMediaSourceAVFObjC(session);

    if (!m_mediaSourcePrivate)
        return;

    m_mediaSourcePrivate->setCDMSession(session);
}
#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateMediaSourceAVFObjC::keyNeeded(const SharedBuffer& initData)
{
    if (auto player = m_player.get())
        player->keyNeeded(initData);
}
#endif

void MediaPlayerPrivateMediaSourceAVFObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ALWAYS_LOG(LOGIDENTIFIER, obscured);
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->outputObscuredDueToInsufficientExternalProtectionChanged(obscured);
#else
    UNUSED_PARAM(obscured);
#endif
}

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateMediaSourceAVFObjC::cdmInstanceAttached(CDMInstance& instance)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->cdmInstanceAttached(instance);

    updateDisplayLayerAndDecompressionSession();
}

void MediaPlayerPrivateMediaSourceAVFObjC::cdmInstanceDetached(CDMInstance& instance)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->cdmInstanceDetached(instance);

    updateDisplayLayerAndDecompressionSession();
}

void MediaPlayerPrivateMediaSourceAVFObjC::attemptToDecryptWithInstance(CDMInstance& instance)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->attemptToDecryptWithInstance(instance);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::waitingForKey() const
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->waitingForKey() : false;
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
    updateDisplayLayerAndDecompressionSession();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNeedsPlaceholderImage(bool needsPlaceholder)
{
    if (m_needsPlaceholderImage == needsPlaceholder)
        return;

    m_needsPlaceholderImage = needsPlaceholder;

    if (m_needsPlaceholderImage)
        [m_sampleBufferDisplayLayer setContents:(id)m_lastPixelBuffer.get()];
    else
        [m_sampleBufferDisplayLayer setContents:nil];
}

void MediaPlayerPrivateMediaSourceAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;

    if (m_readyState > MediaPlayer::ReadyState::HaveCurrentData && readyState == MediaPlayer::ReadyState::HaveCurrentData)
        ALWAYS_LOG(LOGIDENTIFIER, "stall detected currentTime:", currentTime());

    m_readyState = readyState;

    if (shouldBePlaying())
        setSynchronizerRate(m_rate);
    else
        setSynchronizerRate(0);

    if (m_readyState >= MediaPlayer::ReadyState::HaveCurrentData && hasVideo() && hasVideoRenderer() && !m_hasAvailableVideoFrame) {
        m_readyStateIsWaitingForAvailableFrame = true;
        return;
    }

    if (auto player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (m_networkState == networkState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, networkState);
    m_networkState = networkState;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::addAudioRenderer(AVSampleBufferAudioRenderer* audioRenderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    if (!audioRenderer) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (!m_sampleBufferAudioRendererMap.add((__bridge CFTypeRef)audioRenderer, AudioRendererProperties()).isNewEntry)
        return;

    auto player = m_player.get();
    if (!player)
        return;

    [audioRenderer setMuted:player->muted()];
    [audioRenderer setVolume:player->volume()];
    auto algorithm = MediaSessionManagerCocoa::audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player->pitchCorrectionAlgorithm(), player->preservesPitch(), m_rate);
    [audioRenderer setAudioTimePitchAlgorithm:algorithm];
#if PLATFORM(MAC)
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if ([audioRenderer respondsToSelector:@selector(setIsUnaccompaniedByVisuals:)])
        [audioRenderer setIsUnaccompaniedByVisuals:!player->isVideoPlayer()];
ALLOW_NEW_API_WITHOUT_GUARDS_END
#endif

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto deviceId = player->audioOutputDeviceIdOverride();
    if (!deviceId.isNull()) {
        if (deviceId.isEmpty())
            audioRenderer.audioOutputDeviceUniqueID = nil;
        else
            audioRenderer.audioOutputDeviceUniqueID = deviceId;
    }
#endif

    @try {
        [m_synchronizer addRenderer:audioRenderer];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    player->characteristicChanged();
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::removeAudioRenderer(AVSampleBufferAudioRenderer* audioRenderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    auto iter = m_sampleBufferAudioRendererMap.find((__bridge CFTypeRef)audioRenderer);
    if (iter == m_sampleBufferAudioRendererMap.end())
        return;

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:audioRenderer atTime:currentTime completionHandler:nil];

    m_sampleBufferAudioRendererMap.remove(iter);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeAudioTrack(AudioTrackPrivate& track)
{
    if (auto player = m_player.get())
        player->removeAudioTrack(track);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeVideoTrack(VideoTrackPrivate& track)
{
    if (auto player = m_player.get())
        player->removeVideoTrack(track);
}

void MediaPlayerPrivateMediaSourceAVFObjC::removeTextTrack(InbandTextTrackPrivate& track)
{
    if (auto player = m_player.get())
        player->removeTextTrack(track);
}

void MediaPlayerPrivateMediaSourceAVFObjC::characteristicsChanged()
{
    updateAllRenderersHaveAvailableSamples();
    if (auto player = m_player.get())
        player->characteristicChanged();
}

RetainPtr<PlatformLayer> MediaPlayerPrivateMediaSourceAVFObjC::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    updateLastImage();
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), m_lastImage ? m_lastImage->platformImage() : nullptr);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoLayerManager->setVideoFullscreenFrame(frame);
}

void MediaPlayerPrivateMediaSourceAVFObjC::syncTextTrackBounds()
{
    m_videoLayerManager->syncTextTrackBounds();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    auto* representationLayer = representation ? representation->platformLayer() : nil;
    m_videoLayerManager->setTextTrackRepresentationLayer(representationLayer);
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaPlayerPrivateMediaSourceAVFObjC::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_playbackTarget = WTFMove(target);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldPlayToPlaybackTarget(bool shouldPlayToTarget)
{
    if (shouldPlayToTarget == m_shouldPlayToTarget)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldPlayToTarget);
    m_shouldPlayToTarget = shouldPlayToTarget;

    if (auto player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless());
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isCurrentPlaybackTargetWireless() const
{
    if (!m_playbackTarget)
        return false;

    auto hasTarget = m_shouldPlayToTarget && m_playbackTarget->hasActiveRoute();
    INFO_LOG(LOGIDENTIFIER, hasTarget);
    return hasTarget;
}
#endif

bool MediaPlayerPrivateMediaSourceAVFObjC::performTaskAtTime(WTF::Function<void()>&& task, const MediaTime& time)
{
    __block WTF::Function<void()> taskIn = WTFMove(task);

    if (m_performTaskObserver)
        [m_synchronizer removeTimeObserver:m_performTaskObserver.get()];

    m_performTaskObserver = [m_synchronizer addBoundaryTimeObserverForTimes:@[[NSValue valueWithCMTime:PAL::toCMTime(time)]] queue:dispatch_get_main_queue() usingBlock:^{
        taskIn();
    }];
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::audioOutputDeviceChanged()
{
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto player = m_player.get();
    if (!player)
        return;
    auto deviceId = player->audioOutputDeviceId();
    for (auto& key : m_sampleBufferAudioRendererMap.keys()) {
        auto renderer = ((__bridge AVSampleBufferAudioRenderer *)key.get());
        if (deviceId.isEmpty())
            renderer.audioOutputDeviceUniqueID = nil;
        else
            renderer.audioOutputDeviceUniqueID = deviceId;
    }
#endif
}

void MediaPlayerPrivateMediaSourceAVFObjC::startVideoFrameMetadataGathering()
{
    if (m_videoFrameMetadataGatheringObserver)
        return;
    ASSERT(m_synchronizer);
    m_isGatheringVideoFrameMetadata = true;
    acceleratedRenderingStateChanged();

    // FIXME: We should use a CADisplayLink to get updates on rendering, for now we emulate with addPeriodicTimeObserverForInterval.
    m_videoFrameMetadataGatheringObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::CMTimeMake(1, 60) queue:dispatch_get_main_queue() usingBlock:[weakThis = WeakPtr { *this }](CMTime currentTime) {
        ensureOnMainThread([weakThis, currentTime] {
            if (weakThis)
                weakThis->checkNewVideoFrameMetadata(currentTime);
        });
    }];
}

void MediaPlayerPrivateMediaSourceAVFObjC::checkNewVideoFrameMetadata(CMTime currentTime)
{
    auto player = m_player.get();
    if (!player)
        return;

    if (!updateLastPixelBuffer())
        return;

    VideoFrameMetadata metadata;
    metadata.width = m_naturalSize.width();
    metadata.height = m_naturalSize.height();
    metadata.presentedFrames = ++m_sampleCount;
    metadata.presentationTime = PAL::CMTimeGetSeconds(currentTime);

    m_videoFrameMetadata = metadata;
    player->onNewVideoFrameMetadata(WTFMove(metadata), m_lastPixelBuffer.get());
}

void MediaPlayerPrivateMediaSourceAVFObjC::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
    acceleratedRenderingStateChanged();
    m_videoFrameMetadata = { };

    ASSERT(m_videoFrameMetadataGatheringObserver);
    if (m_videoFrameMetadataGatheringObserver)
        [m_synchronizer removeTimeObserver:m_videoFrameMetadataGatheringObserver.get()];
    m_videoFrameMetadataGatheringObserver = nil;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldDisableHDR(bool shouldDisable)
{
    if (![m_sampleBufferDisplayLayer respondsToSelector:@selector(setToneMapToStandardDynamicRange:)])
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldDisable);
    [m_sampleBufferDisplayLayer setToneMapToStandardDynamicRange:shouldDisable];
}

void MediaPlayerPrivateMediaSourceAVFObjC::playerContentBoxRectChanged(const LayoutRect& newRect)
{
    if (!layerOrVideoRenderer() && !newRect.isEmpty())
        updateDisplayLayerAndDecompressionSession();
}

WTFLogChannel& MediaPlayerPrivateMediaSourceAVFObjC::logChannel() const
{
    return LogMediaSource;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    if (m_shouldMaintainAspectRatio == shouldMaintainAspectRatio)
        return;

    m_shouldMaintainAspectRatio = shouldMaintainAspectRatio;
    if (!m_sampleBufferDisplayLayer)
        return;

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];

    [m_sampleBufferDisplayLayer setVideoGravity: (m_shouldMaintainAspectRatio ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize)];

    [CATransaction commit];
}

#if HAVE(SPATIAL_TRACKING_LABEL)
const String& MediaPlayerPrivateMediaSourceAVFObjC::defaultSpatialTrackingLabel() const
{
    return m_defaultSpatialTrackingLabel;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    if (m_defaultSpatialTrackingLabel == defaultSpatialTrackingLabel)
        return;
    m_defaultSpatialTrackingLabel = defaultSpatialTrackingLabel;
    updateSpatialTrackingLabel();
}

const String& MediaPlayerPrivateMediaSourceAVFObjC::spatialTrackingLabel() const
{
    return m_spatialTrackingLabel;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (m_spatialTrackingLabel == spatialTrackingLabel)
        return;
    m_spatialTrackingLabel = spatialTrackingLabel;
    updateSpatialTrackingLabel();
}

void MediaPlayerPrivateMediaSourceAVFObjC::updateSpatialTrackingLabel()
{
    auto *renderer = m_sampleBufferVideoRenderer ? m_sampleBufferVideoRenderer.get() : [m_sampleBufferDisplayLayer sampleBufferRenderer];
    if (!m_spatialTrackingLabel.isNull()) {
        INFO_LOG(LOGIDENTIFIER, "Explicitly set STSLabel: ", m_spatialTrackingLabel);
        renderer.STSLabel = m_spatialTrackingLabel;
        return;
    }

    if (renderer && m_visible) {
        // If the media player has a renderer, and that renderer belongs to a page that is visible,
        // then let AVSBRS manage setting the spatial tracking label in its video renderer itself.
        INFO_LOG(LOGIDENTIFIER, "Has visible renderer, set STSLabel: nil");
        renderer.STSLabel = nil;
        return;
    }

    if (m_sampleBufferAudioRendererMap.isEmpty()) {
        INFO_LOG(LOGIDENTIFIER, "No audio renderers - no-op");
        // If there are no audio renderers, there's nothing to do.
        return;
    }

    // If there is no video renderer, use the default spatial tracking label if available, or
    // the session's spatial tracking label if not, and set the label directly on each audio
    // renderer.
    AVAudioSession *session = [PAL::getAVAudioSessionClass() sharedInstance];
    String defaultLabel;
    if (!m_defaultSpatialTrackingLabel.isNull()) {
        INFO_LOG(LOGIDENTIFIER, "Default STSLabel: ", m_defaultSpatialTrackingLabel);
        defaultLabel = m_defaultSpatialTrackingLabel;
    } else {
        INFO_LOG(LOGIDENTIFIER, "AVAudioSession label: ", session.spatialTrackingLabel);
        defaultLabel = session.spatialTrackingLabel;
    }
    for (const auto &key : m_sampleBufferAudioRendererMap.keys())
        [(__bridge AVSampleBufferAudioRenderer *)key.get() setSTSLabel:defaultLabel];
}
#endif

WebSampleBufferVideoRendering *MediaPlayerPrivateMediaSourceAVFObjC::layerOrVideoRenderer() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    switch (acceleratedVideoMode()) {
    case AcceleratedVideoMode::Layer:
    case AcceleratedVideoMode::StagedLayer:
        return m_sampleBufferDisplayLayer.get();
    case AcceleratedVideoMode::VideoRenderer:
    case AcceleratedVideoMode::StagedVideoRenderer:
        return m_sampleBufferVideoRenderer.get();
    }
#else
    return m_sampleBufferDisplayLayer.get();
#endif
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
void MediaPlayerPrivateMediaSourceAVFObjC::setVideoTarget(const PlatformVideoTarget& videoTarget)
{
    ALWAYS_LOG(LOGIDENTIFIER, !!videoTarget);
    if (!!videoTarget)
        m_usingLinearMediaPlayer = true;
    m_videoTarget = videoTarget;
    updateDisplayLayerAndDecompressionSession();
}
#endif

void MediaPlayerPrivateMediaSourceAVFObjC::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    ALWAYS_LOG(LOGIDENTIFIER, isInFullscreenOrPictureInPicture);
    if (!m_usingLinearMediaPlayer)
        return;
    updateDisplayLayerAndDecompressionSession();
#else
    UNUSED_PARAM(isInFullscreenOrPictureInPicture);
#endif
}

} // namespace WebCore

#endif
