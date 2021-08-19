/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#import "CDMSessionAVStreamSession.h"
#import "GraphicsContext.h"
#import "Logging.h"
#import "MediaSessionManagerCocoa.h"
#import "MediaSourcePrivateAVFObjC.h"
#import "MediaSourcePrivateClient.h"
#import "PixelBufferConformerCV.h"
#import "TextTrackRepresentation.h"
#import "VideoLayerManagerObjC.h"
#import "WebCoreDecompressionSession.h"
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVTime.h>
#import <CoreMedia/CMTime.h>
#import <QuartzCore/CALayer.h>
#import <objc_runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/Deque.h>
#import <wtf/FileSystem.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/WeakPtr.h>

#import "CoreVideoSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#pragma mark -
#pragma mark AVStreamSession

@interface AVStreamSession : NSObject
- (instancetype)initWithStorageDirectoryAtURL:(NSURL *)storageDirectory;
@end

namespace WebCore {

String convertEnumerationToString(MediaPlayerPrivateMediaSourceAVFObjC::SeekState enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("Seeking"),
        MAKE_STATIC_STRING_IMPL("WaitingForAvailableFame"),
        MAKE_STATIC_STRING_IMPL("SeekCompleted"),
    };
    static_assert(static_cast<size_t>(MediaPlayerPrivateMediaSourceAVFObjC::SeekState::Seeking) == 0, "MediaPlayerPrivateMediaSourceAVFObjC::SeekState::Seeking is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayerPrivateMediaSourceAVFObjC::SeekState::WaitingForAvailableFame) == 1, "MediaPlayerPrivateMediaSourceAVFObjC::SeekState::WaitingForAvailableFame is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayerPrivateMediaSourceAVFObjC::SeekState::SeekCompleted) == 2, "MediaPlayerPrivateMediaSourceAVFObjC::SeekState::SeekCompleted is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}
    
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
        callOnMainThread([this, protectedThis = makeRef(*this)] {
            if (m_client)
                m_client->effectiveRateChanged();
        });
    }

    void stop(CMTimebaseRef);

private:
    EffectiveRateChangedListener(MediaPlayerPrivateMediaSourceAVFObjC&, CMTimebaseRef);

    WeakPtr<MediaPlayerPrivateMediaSourceAVFObjC> m_client;
};

static void CMTimebaseEffectiveRateChangedCallback(CMNotificationCenterRef, const void *listener, CFStringRef, const void *, CFTypeRef)
{
    auto* effectiveRateChangedListener = (EffectiveRateChangedListener*)const_cast<void*>(listener);
    effectiveRateChangedListener->effectiveRateChanged();
}

void EffectiveRateChangedListener::stop(CMTimebaseRef timebase)
{
    CMNotificationCenterRef nc = PAL::CMNotificationCenterGetDefaultLocalCenter();
    PAL::CMNotificationCenterRemoveListener(nc, this, CMTimebaseEffectiveRateChangedCallback, PAL::kCMTimebaseNotification_EffectiveRateChanged, timebase);
}

EffectiveRateChangedListener::EffectiveRateChangedListener(MediaPlayerPrivateMediaSourceAVFObjC& client, CMTimebaseRef timebase)
    : m_client(makeWeakPtr(client))
{
    CMNotificationCenterRef nc = PAL::CMNotificationCenterGetDefaultLocalCenter();
    PAL::CMNotificationCenterAddListener(nc, this, CMTimebaseEffectiveRateChangedCallback, PAL::kCMTimebaseNotification_EffectiveRateChanged, timebase, 0);
}

MediaPlayerPrivateMediaSourceAVFObjC::MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_synchronizer(adoptNS([PAL::allocAVSampleBufferRenderSynchronizerInstance() init]))
    , m_seekTimer(*this, &MediaPlayerPrivateMediaSourceAVFObjC::seekInternal)
    , m_networkState(MediaPlayer::NetworkState::Empty)
    , m_readyState(MediaPlayer::ReadyState::HaveNothing)
    , m_rate(1)
    , m_playing(0)
    , m_seeking(false)
    , m_loadingProgressed(false)
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
    , m_videoLayerManager(makeUnique<VideoLayerManagerObjC>(m_logger, m_logIdentifier))
    , m_effectiveRateChangedListener(EffectiveRateChangedListener::create(*this, [m_synchronizer timebase]))
{
    auto logSiteIdentifier = LOGIDENTIFIER;
    ALWAYS_LOG(logSiteIdentifier);
    UNUSED_PARAM(logSiteIdentifier);

    // addPeriodicTimeObserverForInterval: throws an exception if you pass a non-numeric CMTime, so just use
    // an arbitrarily large time value of once an hour:
    __block auto weakThis = makeWeakPtr(*this);
    m_timeJumpedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::toCMTime(MediaTime::createWithDouble(3600)) queue:dispatch_get_main_queue() usingBlock:^(CMTime time) {
#if LOG_DISABLED
        UNUSED_PARAM(time);
#endif
        // FIXME: Remove the below once <rdar://problem/15798050> is fixed.
        if (!weakThis)
            return;

        DEBUG_LOG(logSiteIdentifier, "synchronizer fired for ", PAL::toMediaTime(time), ", seeking = ", m_seeking, ", pending = ", !!m_pendingSeek);

        if (m_seeking && !m_pendingSeek) {
            m_seeking = false;

            if (shouldBePlaying())
                [m_synchronizer setRate:m_rate];
            if (!seeking() && m_seekCompleted == SeekCompleted)
                m_player->timeChanged();
        }

        if (m_pendingSeek)
            seekInternal();

        if (m_currentTimeDidChangeCallback)
            m_currentTimeDidChangeCallback(CMTIME_IS_NUMERIC(time) ? PAL::toMediaTime(time) : MediaTime::zeroTime());
    }];
}

MediaPlayerPrivateMediaSourceAVFObjC::~MediaPlayerPrivateMediaSourceAVFObjC()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_effectiveRateChangedListener->stop([m_synchronizer timebase]);

    if (m_timeJumpedObserver)
        [m_synchronizer removeTimeObserver:m_timeJumpedObserver.get()];
    if (m_timeChangedObserver)
        [m_synchronizer removeTimeObserver:m_timeChangedObserver.get()];
    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];
    flushPendingSizeChanges();

    destroyLayer();
    destroyDecompressionSession();

    m_seekTimer.stop();
}

#pragma mark -
#pragma mark MediaPlayer Factory Methods

class MediaPlayerFactoryMediaSourceAVFObjC final : public MediaPlayerFactory {
public:
    MediaPlayerFactoryMediaSourceAVFObjC()
    {
        MediaSessionManagerCocoa::ensureCodecsRegistered();
    }

private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE; };

    std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return makeUnique<MediaPlayerPrivateMediaSourceAVFObjC>(player);        
    }

    void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types) const final
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

void MediaPlayerPrivateMediaSourceAVFObjC::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    types = AVStreamDataParserMIMETypeCache::singleton().supportedTypes();
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaSourceAVFObjC::supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters)
{
    // This engine does not support non-media-source sources.
    if (!parameters.isMediaSource)
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
    m_player->networkStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::load(const URL&, const ContentType&, MediaSourcePrivateClient* client)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_mediaSourcePrivate = MediaSourcePrivateAVFObjC::create(*this, client);
    m_mediaSourcePrivate->setVideoLayer(m_sampleBufferDisplayLayer.get());
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
    callOnMainThread([weakThis = makeWeakPtr(*this)] {
        if (!weakThis)
            return;
        weakThis.get()->playInternal();
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::playInternal(std::optional<MonotonicTime>&& hostTime)
{
    if (!m_mediaSourcePrivate)
        return;

    if (currentMediaTime() >= m_mediaSourcePrivate->duration()) {
        ALWAYS_LOG(LOGIDENTIFIER, "bailing, current time: ", currentMediaTime(), " greater than duration ", m_mediaSourcePrivate->duration());
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER);
#if PLATFORM(IOS_FAMILY)
    m_mediaSourcePrivate->flushActiveSourceBuffersIfNeeded();
#endif
    m_playing = true;
    if (!shouldBePlaying())
        return;

#if HAVE(AVSAMPLEBUFFERRENDERSYNCHRONIZER_RATEATHOSTTIME)
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if (hostTime && [m_synchronizer respondsToSelector:@selector(setRate:time:atHostTime:)]) {
        auto cmHostTime = PAL::CMClockMakeHostTimeFromSystemUnits(hostTime->toMachAbsoluteTime());
        INFO_LOG(LOGIDENTIFIER, "setting rate to ", m_rate, " at host time ", PAL::CMTimeGetSeconds(cmHostTime));
        [m_synchronizer setRate:m_rate time:PAL::kCMTimeInvalid atHostTime:cmHostTime];
    } else
        [m_synchronizer setRate:m_rate];
    ALLOW_NEW_API_WITHOUT_GUARDS_END
#else
    UNUSED_PARAM(hostTime);
    [m_synchronizer setRate:m_rate];
#endif
}

void MediaPlayerPrivateMediaSourceAVFObjC::pause()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    callOnMainThread([weakThis = makeWeakPtr(*this)] {
        if (!weakThis)
            return;
        weakThis.get()->pauseInternal();
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::pauseInternal(std::optional<MonotonicTime>&& hostTime)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_playing = false;

#if HAVE(AVSAMPLEBUFFERRENDERSYNCHRONIZER_RATEATHOSTTIME)
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if (hostTime && [m_synchronizer respondsToSelector:@selector(setRate:time:atHostTime:)]) {
        auto cmHostTime = PAL::CMClockMakeHostTimeFromSystemUnits(hostTime->toMachAbsoluteTime());
        INFO_LOG(LOGIDENTIFIER, "setting rate to 0 at host time ", PAL::CMTimeGetSeconds(cmHostTime));
        [m_synchronizer setRate:0 time:PAL::kCMTimeInvalid atHostTime:cmHostTime];
    } else
        [m_synchronizer setRate:0];
    ALLOW_NEW_API_WITHOUT_GUARDS_END
#else
    UNUSED_PARAM(hostTime);
    [m_synchronizer setRate:0];
#endif
}

bool MediaPlayerPrivateMediaSourceAVFObjC::paused() const
{
    return ![m_synchronizer rate];
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
    if (m_visible)
        acceleratedRenderingStateChanged();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::durationMediaTime() const
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->duration() : MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::currentMediaTime() const
{
    MediaTime synchronizerTime = PAL::toMediaTime(PAL::CMTimebaseGetTime([m_synchronizer timebase]));
    if (synchronizerTime < MediaTime::zeroTime())
        return MediaTime::zeroTime();
    if (synchronizerTime < m_lastSeekTime)
        return m_lastSeekTime;
    return synchronizerTime;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::setCurrentTimeDidChangeCallback(MediaPlayer::CurrentTimeDidChangeCallback&& callback)
{
    m_currentTimeDidChangeCallback = WTFMove(callback);

    if (m_currentTimeDidChangeCallback) {
        m_timeChangedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::CMTimeMake(1, 10) queue:dispatch_get_main_queue() usingBlock:^(CMTime time) {
            if (m_currentTimeDidChangeCallback)
                m_currentTimeDidChangeCallback(CMTIME_IS_NUMERIC(time) ? PAL::toMediaTime(time) : MediaTime::zeroTime());
        }];

    } else
        m_timeChangedObserver = nullptr;

    return true;
}

#if HAVE(AVSAMPLEBUFFERRENDERSYNCHRONIZER_RATEATHOSTTIME)
bool MediaPlayerPrivateMediaSourceAVFObjC::playAtHostTime(const MonotonicTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    callOnMainThread([weakThis = makeWeakPtr(*this), time = time] {
        if (!weakThis)
            return;
        weakThis.get()->playInternal(time);
    });
    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::pauseAtHostTime(const MonotonicTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    callOnMainThread([weakThis = makeWeakPtr(*this), time = time] {
        if (!weakThis)
            return;
        weakThis.get()->pauseInternal(time);
    });
    return true;
}
#endif

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::startTime() const
{
    return MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::initialTime() const
{
    return MediaTime::zeroTime();
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekWithTolerance(const MediaTime& time, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    INFO_LOG(LOGIDENTIFIER, "time = ", time, ", negativeThreshold = ", negativeThreshold, ", positiveThreshold = ", positiveThreshold);

    m_seeking = true;
    m_pendingSeek = makeUnique<PendingSeek>(time, negativeThreshold, positiveThreshold);

    if (m_seekTimer.isActive())
        m_seekTimer.stop();
    m_seekTimer.startOneShot(0_s);
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekInternal()
{
    std::unique_ptr<PendingSeek> pendingSeek;
    pendingSeek.swap(m_pendingSeek);

    if (!pendingSeek)
        return;

    if (!m_mediaSourcePrivate)
        return;

    if (!pendingSeek->negativeThreshold && !pendingSeek->positiveThreshold)
        m_lastSeekTime = pendingSeek->targetTime;
    else
        m_lastSeekTime = m_mediaSourcePrivate->fastSeekTimeForMediaTime(pendingSeek->targetTime, pendingSeek->positiveThreshold, pendingSeek->negativeThreshold);

    if (m_lastSeekTime.hasDoubleValue())
        m_lastSeekTime = MediaTime::createWithDouble(m_lastSeekTime.toDouble(), MediaTime::DefaultTimeScale);

    MediaTime synchronizerTime = PAL::toMediaTime(PAL::CMTimebaseGetTime([m_synchronizer timebase]));
    INFO_LOG(LOGIDENTIFIER, "seekTime = ", m_lastSeekTime, ", synchronizerTime = ", synchronizerTime);

    bool doesNotRequireSeek = synchronizerTime == m_lastSeekTime;

    m_mediaSourcePrivate->willSeek();
    [m_synchronizer setRate:0 time:PAL::toCMTime(m_lastSeekTime)];
    m_mediaSourcePrivate->seekToTime(m_lastSeekTime);

    // In cases where the destination seek time precisely matches the synchronizer's existing time
    // no time jumped notification will be issued. In this case, just notify the MediaPlayer that
    // the seek completed successfully.
    if (doesNotRequireSeek) {
        m_seeking = false;

        if (shouldBePlaying())
            [m_synchronizer setRate:m_rate];
        if (!seeking() && m_seekCompleted)
            m_player->timeChanged();
    }
}

void MediaPlayerPrivateMediaSourceAVFObjC::waitForSeekCompleted()
{
    if (!m_seeking)
        return;
    ALWAYS_LOG(LOGIDENTIFIER);
    m_seekCompleted = Seeking;
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekCompleted()
{
    if (m_seekCompleted == SeekCompleted)
        return;
    if (hasVideo() && !m_hasAvailableVideoFrame) {
        ALWAYS_LOG(LOGIDENTIFIER, "waiting for video frame");
        m_seekCompleted = WaitingForAvailableFame;
        return;
    }
    ALWAYS_LOG(LOGIDENTIFIER);
    m_seekCompleted = SeekCompleted;
    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];
    if (!m_seeking)
        m_player->timeChanged();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::seeking() const
{
    return m_seeking || m_seekCompleted != SeekCompleted;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setRateDouble(double rate)
{
    // AVSampleBufferRenderSynchronizer does not support negative rate yet.
    m_rate = std::max<double>(rate, 0);
    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];
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
    NSString *algorithm = preservesPitch ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed;
    for (const auto& key : m_sampleBufferAudioRendererMap.keys())
        [(__bridge AVSampleBufferAudioRenderer *)key.get() setAudioTimePitchAlgorithm:algorithm];
}

MediaPlayer::NetworkState MediaPlayerPrivateMediaSourceAVFObjC::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateMediaSourceAVFObjC::readyState() const
{
    return m_readyState;
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateMediaSourceAVFObjC::seekable() const
{
    return makeUnique<PlatformTimeRanges>(minMediaTimeSeekable(), maxMediaTimeSeekable());
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::maxMediaTimeSeekable() const
{
    return durationMediaTime();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::minMediaTimeSeekable() const
{
    return startTime();
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateMediaSourceAVFObjC::buffered() const
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->buffered() : makeUnique<PlatformTimeRanges>();
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
#if HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    if (m_videoOutput) {
        CMTime outputTime;
        if (auto pixelBuffer = adoptCF([m_videoOutput copyPixelBufferForSourceTime:toCMTime(currentMediaTime()) sourceTimeForDisplay:&outputTime])) {
            INFO_LOG(LOGIDENTIFIER, "new pixelbuffer found for time ", PAL::toMediaTime(outputTime));
            m_lastPixelBuffer = WTFMove(pixelBuffer);
            return true;
        }
    }
#endif

    if (m_sampleBufferDisplayLayer || !m_decompressionSession)
        return false;

    auto flags = !m_lastPixelBuffer ? WebCoreDecompressionSession::AllowLater : WebCoreDecompressionSession::ExactTime;
    auto newPixelBuffer = m_decompressionSession->imageForTime(currentMediaTime(), flags);
    if (!newPixelBuffer)
        return false;

    m_lastPixelBuffer = newPixelBuffer;
    return true;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::updateLastImage()
{
    if (!updateLastPixelBuffer())
        return false;

    ASSERT(m_lastPixelBuffer);

    if (!m_rgbConformer) {
        auto attributes = @{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
        m_rgbConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)attributes);
    }

    m_lastImage = NativeImage::create(m_rgbConformer->createImageFromPixelBuffer(m_lastPixelBuffer.get()));
    return true;
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
    context.drawNativeImage(*image, imageRect.size(), outputRect, imageRect);
}

RetainPtr<CVPixelBufferRef> MediaPlayerPrivateMediaSourceAVFObjC::pixelBufferForCurrentTime()
{
    // We have been asked to paint into a WebGL canvas, so take that as a signal to create
    // a decompression session, even if that means the native video can't also be displayed
    // in page.
    if (!m_hasBeenAskedToPaintGL) {
        m_hasBeenAskedToPaintGL = true;
        acceleratedRenderingStateChanged();
    }

    if (updateLastPixelBuffer()) {
        if (!m_lastPixelBuffer)
            return nullptr;
    }

    return m_lastPixelBuffer;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::hasAvailableVideoFrame() const
{
    return m_hasAvailableVideoFrame;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsAcceleratedRendering() const
{
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::acceleratedRenderingStateChanged()
{
    if (!m_hasBeenAskedToPaintGL || isVideoOutputAvailable()) {
        destroyDecompressionSession();
        ensureLayer();
    } else {
        destroyLayer();
        ensureDecompressionSession();
    }
}

void MediaPlayerPrivateMediaSourceAVFObjC::notifyActiveSourceBuffersChanged()
{
    m_player->activeSourceBuffersChanged();
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

    auto metrics = [m_sampleBufferDisplayLayer videoPerformanceMetrics];
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

    m_sampleBufferDisplayLayer = adoptNS([PAL::allocAVSampleBufferDisplayLayerInstance() init]);
#ifndef NDEBUG
    [m_sampleBufferDisplayLayer setName:@"MediaPlayerPrivateMediaSource AVSampleBufferDisplayLayer"];
#endif

    if (!m_sampleBufferDisplayLayer) {
        ERROR_LOG(LOGIDENTIFIER, "Failed to create AVSampleBufferDisplayLayer");
        if (m_mediaSourcePrivate)
            m_mediaSourcePrivate->failedToCreateRenderer(MediaSourcePrivateAVFObjC::RendererType::Video);
        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

#if HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    ASSERT(!m_videoOutput);
    if (isVideoOutputAvailable()) {
        m_videoOutput = adoptNS([PAL::allocAVSampleBufferVideoOutputInstance() init]);
        ASSERT(m_videoOutput);
        [m_sampleBufferDisplayLayer setOutput:m_videoOutput.get()];
    }
#endif

    if ([m_sampleBufferDisplayLayer respondsToSelector:@selector(setPreventsDisplaySleepDuringVideoPlayback:)])
        m_sampleBufferDisplayLayer.get().preventsDisplaySleepDuringVideoPlayback = NO;

    @try {
        [m_synchronizer addRenderer:m_sampleBufferDisplayLayer.get()];
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->setVideoLayer(m_sampleBufferDisplayLayer.get());
    m_videoLayerManager->setVideoLayer(m_sampleBufferDisplayLayer.get(), snappedIntRect(m_player->playerContentBoxRect()).size());
    m_player->renderingModeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::destroyLayer()
{
    if (!m_sampleBufferDisplayLayer)
        return;

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime withCompletionHandler:^(BOOL){
        // No-op.
    }];

    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->setVideoLayer(nullptr);
    m_videoLayerManager->didDestroyVideoLayer();
    m_sampleBufferDisplayLayer = nullptr;
    setHasAvailableVideoFrame(false);
    m_player->renderingModeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::ensureDecompressionSession()
{
    if (m_decompressionSession)
        return;

    m_decompressionSession = WebCoreDecompressionSession::createOpenGL();
    m_decompressionSession->setTimebase([m_synchronizer timebase]);

    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->setDecompressionSession(m_decompressionSession.get());

    m_player->renderingModeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::destroyDecompressionSession()
{
    if (!m_decompressionSession)
        return;

    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->setDecompressionSession(nullptr);

    m_decompressionSession->invalidate();
    m_decompressionSession = nullptr;
    setHasAvailableVideoFrame(false);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::shouldBePlaying() const
{
    return m_playing && !seeking() && allRenderersHaveAvailableSamples() && m_readyState >= MediaPlayer::ReadyState::HaveFutureData;
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isVideoOutputAvailable() const
{
#if HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    return PAL::getAVSampleBufferVideoOutputClass;
#else
    return false;
#endif
}

void MediaPlayerPrivateMediaSourceAVFObjC::setHasAvailableVideoFrame(bool flag)
{
    if (m_hasAvailableVideoFrame == flag)
        return;

    DEBUG_LOG(LOGIDENTIFIER, flag);
    m_hasAvailableVideoFrame = flag;
    updateAllRenderersHaveAvailableSamples();

    if (!m_hasAvailableVideoFrame)
        return;

    m_player->firstVideoFrameAvailable();
    if (m_seekCompleted == WaitingForAvailableFame)
        seekCompleted();

    if (m_readyStateIsWaitingForAvailableFrame) {
        m_readyStateIsWaitingForAvailableFrame = false;
        m_player->readyStateChanged();
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
    DEBUG_LOG(LOGIDENTIFIER, flag);
    properties.hasAudibleSample = flag;
    updateAllRenderersHaveAvailableSamples();
}

void MediaPlayerPrivateMediaSourceAVFObjC::updateAllRenderersHaveAvailableSamples()
{
    bool allRenderersHaveAvailableSamples = true;

    do {
        if (hasVideo() && !m_hasAvailableVideoFrame) {
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
        [m_synchronizer setRate:m_rate];
    else if (!shouldBePlaying() && [m_synchronizer rate])
        [m_synchronizer setRate:0];
}

void MediaPlayerPrivateMediaSourceAVFObjC::durationChanged()
{
    m_player->durationChanged();

    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];

    if (!m_mediaSourcePrivate)
        return;

    MediaTime duration = m_mediaSourcePrivate->duration();
    NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(duration)]];

    auto logSiteIdentifier = LOGIDENTIFIER;
    DEBUG_LOG(logSiteIdentifier, duration);
    UNUSED_PARAM(logSiteIdentifier);

    m_durationObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:dispatch_get_main_queue() usingBlock:[weakThis = makeWeakPtr(*this), duration, logSiteIdentifier, this] {
        if (!weakThis)
            return;

        MediaTime now = weakThis->currentMediaTime();
        DEBUG_LOG(logSiteIdentifier, "boundary time observer called, now = ", now);

        weakThis->pauseInternal();
        if (now < duration) {
            ERROR_LOG(logSiteIdentifier, "ERROR: boundary time observer called before duration");
            [weakThis->m_synchronizer setRate:0 time:PAL::toCMTime(duration)];
        }
        weakThis->m_player->timeChanged();

    }];

    if (m_playing && duration <= currentMediaTime())
        pauseInternal();
}

void MediaPlayerPrivateMediaSourceAVFObjC::effectiveRateChanged()
{
    m_player->rateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::sizeWillChangeAtTime(const MediaTime& time, const FloatSize& size)
{
    auto weakThis = m_sizeChangeObserverWeakPtrFactory.createWeakPtr(*this);
    NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(time)]];
    RetainPtr<id> observer = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:dispatch_get_main_queue() usingBlock:[this, weakThis, size] {
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

    if (currentMediaTime() >= time)
        setNaturalSize(size);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNaturalSize(const FloatSize& size)
{
    if (size == m_naturalSize)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, size);

    m_naturalSize = size;
    m_player->sizeChanged();
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
#if HAVE(AVSTREAMSESSION)
AVStreamSession* MediaPlayerPrivateMediaSourceAVFObjC::streamSession()
{
    if (!PAL::getAVStreamSessionClass() || ![PAL::getAVStreamSessionClass() instancesRespondToSelector:@selector(initWithStorageDirectoryAtURL:)])
        return nil;

    if (!m_streamSession) {
        String storageDirectory = m_player->mediaKeysStorageDirectory();
        if (storageDirectory.isEmpty())
            return nil;

        if (!FileSystem::fileExists(storageDirectory)) {
            if (!FileSystem::makeAllDirectories(storageDirectory))
                return nil;
        }

        String storagePath = FileSystem::pathByAppendingComponent(storageDirectory, "SecureStop.plist");
        m_streamSession = adoptNS([PAL::allocAVStreamSessionInstance() initWithStorageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]]);
    }
    return m_streamSession.get();
}
#endif

CDMSessionMediaSourceAVFObjC* MediaPlayerPrivateMediaSourceAVFObjC::cdmSession() const
{
    return m_session.get();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setCDMSession(LegacyCDMSession* session)
{
    if (session == m_session)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_session = makeWeakPtr(toCDMSessionMediaSourceAVFObjC(session));

#if HAVE(AVSTREAMSESSION)
    if (CDMSessionAVStreamSession* cdmStreamSession = toCDMSessionAVStreamSession(m_session.get()))
        cdmStreamSession->setStreamSession(streamSession());
#endif

    if (!m_mediaSourcePrivate)
        return;

    for (auto& sourceBuffer : m_mediaSourcePrivate->sourceBuffers())
        sourceBuffer->setCDMSession(m_session.get());
}
#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateMediaSourceAVFObjC::keyNeeded(Uint8Array* initData)
{
    m_player->keyNeeded(initData);
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
}

void MediaPlayerPrivateMediaSourceAVFObjC::cdmInstanceDetached(CDMInstance& instance)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->cdmInstanceDetached(instance);
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
    m_player->waitingForKeyChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::initializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    ALWAYS_LOG(LOGIDENTIFIER, initDataType);
    m_player->initializationDataEncountered(initDataType, WTFMove(initData));
}
#endif

const Vector<ContentType>& MediaPlayerPrivateMediaSourceAVFObjC::mediaContentTypesRequiringHardwareSupport() const
{
    return m_player->mediaContentTypesRequiringHardwareSupport();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::shouldCheckHardwareSupport() const
{
    return m_player->shouldCheckHardwareSupport();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (m_readyState == readyState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, readyState);
    m_readyState = readyState;

    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];
    else
        [m_synchronizer setRate:0];

    if (m_readyState >= MediaPlayer::ReadyState::HaveCurrentData && hasVideo() && !m_hasAvailableVideoFrame) {
        m_readyStateIsWaitingForAvailableFrame = true;
        return;
    }

    m_player->readyStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (m_networkState == networkState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, networkState);
    m_networkState = networkState;
    m_player->networkStateChanged();
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

    [audioRenderer setMuted:m_player->muted()];
    [audioRenderer setVolume:m_player->volume()];
    [audioRenderer setAudioTimePitchAlgorithm:(m_player->preservesPitch() ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed)];
#if PLATFORM(MAC)
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if ([audioRenderer respondsToSelector:@selector(setIsUnaccompaniedByVisuals:)])
        [audioRenderer setIsUnaccompaniedByVisuals:!m_player->isVideoPlayer()];
    ALLOW_NEW_API_WITHOUT_GUARDS_END
#endif

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto deviceId = m_player->audioOutputDeviceIdOverride();
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
        ERROR_LOG(LOGIDENTIFIER, "-[AVSampleBufferRenderSynchronizer addRenderer:] threw an exception: ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
        ASSERT_NOT_REACHED();

        setNetworkState(MediaPlayer::NetworkState::DecodeError);
        return;
    }

    m_player->characteristicChanged();
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::removeAudioRenderer(AVSampleBufferAudioRenderer* audioRenderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    auto iter = m_sampleBufferAudioRendererMap.find((__bridge CFTypeRef)audioRenderer);
    if (iter == m_sampleBufferAudioRendererMap.end())
        return;

    CMTime currentTime = PAL::CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:audioRenderer atTime:currentTime withCompletionHandler:^(BOOL){
        // No-op.
    }];

    m_sampleBufferAudioRendererMap.remove(iter);
    m_player->renderingModeChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::characteristicsChanged()
{
    updateAllRenderersHaveAvailableSamples();
    m_player->characteristicChanged();
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

bool MediaPlayerPrivateMediaSourceAVFObjC::requiresTextTrackRepresentation() const
{
    return m_videoLayerManager->videoFullscreenLayer();
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

    if (m_player)
        m_player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless());
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

bool MediaPlayerPrivateMediaSourceAVFObjC::performTaskAtMediaTime(WTF::Function<void()>&& task, const MediaTime& time)
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
    if (!m_player)
        return;
    auto deviceId = m_player->audioOutputDeviceId();
    for (auto& key : m_sampleBufferAudioRendererMap.keys()) {
        auto renderer = ((__bridge AVSampleBufferAudioRenderer *)key.get());
        if (deviceId.isEmpty())
            renderer.audioOutputDeviceUniqueID = nil;
        else
            renderer.audioOutputDeviceUniqueID = deviceId;
    }
#endif
}

WTFLogChannel& MediaPlayerPrivateMediaSourceAVFObjC::logChannel() const
{
    return LogMediaSource;
}

}

#endif
