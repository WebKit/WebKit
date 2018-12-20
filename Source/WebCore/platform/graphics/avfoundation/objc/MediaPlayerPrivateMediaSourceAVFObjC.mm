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

#import "AVAssetTrackUtilities.h"
#import "AVFoundationMIMETypeCache.h"
#import "CDMSessionAVStreamSession.h"
#import "FileSystem.h"
#import "GraphicsContextCG.h"
#import "Logging.h"
#import "MediaSourcePrivateAVFObjC.h"
#import "MediaSourcePrivateClient.h"
#import "PixelBufferConformerCV.h"
#import "TextTrackRepresentation.h"
#import "TextureCacheCV.h"
#import "VideoFullscreenLayerManagerObjC.h"
#import "VideoTextureCopierCV.h"
#import "WebCoreDecompressionSession.h"
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVTime.h>
#import <QuartzCore/CALayer.h>
#import <objc_runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/mac/AVFoundationSPI.h>
#import <wtf/Deque.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

#pragma mark - Soft Linking

#import <pal/cf/CoreMediaSoftLink.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVAsset)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVURLAsset)
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferAudioRenderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferDisplayLayer)
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVSampleBufferRenderSynchronizer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVStreamDataParser)
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVStreamSession);
SOFT_LINK_CLASS_OPTIONAL(AVFoundation, AVVideoPerformanceMetrics)

SOFT_LINK_CONSTANT(AVFoundation, AVAudioTimePitchAlgorithmSpectral, NSString*)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioTimePitchAlgorithmVarispeed, NSString*)

#define AVAudioTimePitchAlgorithmSpectral getAVAudioTimePitchAlgorithmSpectral()
#define AVAudioTimePitchAlgorithmVarispeed getAVAudioTimePitchAlgorithmVarispeed()

#pragma mark -
#pragma mark AVStreamSession

@interface AVStreamSession : NSObject
- (instancetype)initWithStorageDirectoryAtURL:(NSURL *)storageDirectory;
@end

namespace WebCore {
using namespace PAL;

#pragma mark -
#pragma mark MediaPlayerPrivateMediaSourceAVFObjC

static void CMTimebaseEffectiveRateChangedCallback(CMNotificationCenterRef, const void *listener, CFStringRef, const void *, CFTypeRef)
{
    MediaPlayerPrivateMediaSourceAVFObjC* player = (MediaPlayerPrivateMediaSourceAVFObjC*)const_cast<void*>(listener);
    callOnMainThread([weakThis = player->createWeakPtr()] {
        if (!weakThis)
            return;
        weakThis.get()->effectiveRateChanged();
    });
}

MediaPlayerPrivateMediaSourceAVFObjC::MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer* player)
    : m_player(player)
    , m_synchronizer(adoptNS([allocAVSampleBufferRenderSynchronizerInstance() init]))
    , m_seekTimer(*this, &MediaPlayerPrivateMediaSourceAVFObjC::seekInternal)
    , m_session(nullptr)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_rate(1)
    , m_playing(0)
    , m_seeking(false)
    , m_loadingProgressed(false)
    , m_videoFullscreenLayerManager(std::make_unique<VideoFullscreenLayerManagerObjC>())
{
    CMTimebaseRef timebase = [m_synchronizer timebase];
    CMNotificationCenterRef nc = CMNotificationCenterGetDefaultLocalCenter();
    CMNotificationCenterAddListener(nc, this, CMTimebaseEffectiveRateChangedCallback, kCMTimebaseNotification_EffectiveRateChanged, timebase, 0);

    // addPeriodicTimeObserverForInterval: throws an exception if you pass a non-numeric CMTime, so just use
    // an arbitrarily large time value of once an hour:
    __block auto weakThis = createWeakPtr();
    m_timeJumpedObserver = [m_synchronizer addPeriodicTimeObserverForInterval:PAL::toCMTime(MediaTime::createWithDouble(3600)) queue:dispatch_get_main_queue() usingBlock:^(CMTime time) {
#if LOG_DISABLED
        UNUSED_PARAM(time);
#endif
        // FIXME: Remove the below once <rdar://problem/15798050> is fixed.
        if (!weakThis)
            return;

        if (m_seeking && !m_pendingSeek) {
            LOG(MediaSource, "MediaPlayerPrivateMediaSourceAVFObjC::m_timeJumpedObserver(%p) - time(%s)", weakThis.get(), toString(PAL::toMediaTime(time)).utf8().data());
            m_seeking = false;

            if (shouldBePlaying())
                [m_synchronizer setRate:m_rate];
            if (!seeking() && m_seekCompleted == SeekCompleted)
                m_player->timeChanged();
        }

        if (m_pendingSeek)
            seekInternal();
    }];
}

MediaPlayerPrivateMediaSourceAVFObjC::~MediaPlayerPrivateMediaSourceAVFObjC()
{
    CMTimebaseRef timebase = [m_synchronizer timebase];
    CMNotificationCenterRef nc = CMNotificationCenterGetDefaultLocalCenter();
    CMNotificationCenterRemoveListener(nc, this, CMTimebaseEffectiveRateChangedCallback, kCMTimebaseNotification_EffectiveRateChanged, timebase);

    if (m_timeJumpedObserver)
        [m_synchronizer removeTimeObserver:m_timeJumpedObserver.get()];
    if (m_durationObserver)
        [m_synchronizer removeTimeObserver:m_durationObserver.get()];
    flushPendingSizeChanges();

    destroyLayer();
    destroyDecompressionSession();

    m_seekTimer.stop();
}

#pragma mark -
#pragma mark MediaPlayer Factory Methods

void MediaPlayerPrivateMediaSourceAVFObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (!isAvailable())
        return;

    registrar([](MediaPlayer* player) { return std::make_unique<MediaPlayerPrivateMediaSourceAVFObjC>(player); },
        getSupportedTypes, supportsType, 0, 0, 0, 0);
    ASSERT(AVFoundationMIMETypeCache::singleton().isAvailable());
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isAvailable()
{
    return AVFoundationLibrary()
        && isCoreMediaFrameworkAvailable()
        && getAVStreamDataParserClass()
        && getAVSampleBufferAudioRendererClass()
        && getAVSampleBufferRenderSynchronizerClass()
        && class_getInstanceMethod(getAVSampleBufferAudioRendererClass(), @selector(setMuted:));
}

void MediaPlayerPrivateMediaSourceAVFObjC::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    types = AVFoundationMIMETypeCache::singleton().types();
}

MediaPlayer::SupportsType MediaPlayerPrivateMediaSourceAVFObjC::supportsType(const MediaEngineSupportParameters& parameters)
{
    // This engine does not support non-media-source sources.
    if (!parameters.isMediaSource)
        return MediaPlayer::IsNotSupported;
#if ENABLE(MEDIA_STREAM)
    if (parameters.isMediaStream)
        return MediaPlayer::IsNotSupported;
#endif

    if (parameters.type.isEmpty() || !AVFoundationMIMETypeCache::singleton().canDecodeType(parameters.type.containerType()))
        return MediaPlayer::IsNotSupported;

    // The spec says:
    // "Implementors are encouraged to return "maybe" unless the type can be confidently established as being supported or not."
    auto codecs = parameters.type.parameter(ContentType::codecsParameter());
    if (codecs.isEmpty())
        return MediaPlayer::MayBeSupported;

    NSString *outputCodecs = codecs;
    if ([getAVStreamDataParserClass() respondsToSelector:@selector(outputMIMECodecParameterForInputMIMECodecParameter:)])
        outputCodecs = [getAVStreamDataParserClass() outputMIMECodecParameterForInputMIMECodecParameter:outputCodecs];

    if (!contentTypeMeetsHardwareDecodeRequirements(parameters.type, parameters.contentTypesRequiringHardwareSupport))
        return MediaPlayer::IsNotSupported;

    NSString *typeString = [NSString stringWithFormat:@"%@; codecs=\"%@\"", (NSString *)parameters.type.containerType(), (NSString *)outputCodecs];
    return [getAVURLAssetClass() isPlayableExtendedMIMEType:typeString] ? MediaPlayer::IsSupported : MediaPlayer::MayBeSupported;;
}

#pragma mark -
#pragma mark MediaPlayerPrivateInterface Overrides

void MediaPlayerPrivateMediaSourceAVFObjC::load(const String&)
{
    // This media engine only supports MediaSource URLs.
    m_networkState = MediaPlayer::FormatError;
    m_player->networkStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::load(const String& url, MediaSourcePrivateClient* client)
{
    UNUSED_PARAM(url);

    m_mediaSourcePrivate = MediaSourcePrivateAVFObjC::create(this, client);
    m_mediaSourcePrivate->setVideoLayer(m_sampleBufferDisplayLayer.get());
    m_mediaSourcePrivate->setDecompressionSession(m_decompressionSession.get());

    acceleratedRenderingStateChanged();
}

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateMediaSourceAVFObjC::load(MediaStreamPrivate&)
{
    setNetworkState(MediaPlayer::FormatError);
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
    return m_videoFullscreenLayerManager->videoInlineLayer();
}

void MediaPlayerPrivateMediaSourceAVFObjC::play()
{
    callOnMainThread([weakThis = createWeakPtr()] {
        if (!weakThis)
            return;
        weakThis.get()->playInternal();
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::playInternal()
{
    if (currentMediaTime() >= m_mediaSourcePrivate->duration())
        return;

    m_playing = true;
    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];
}

void MediaPlayerPrivateMediaSourceAVFObjC::pause()
{
    callOnMainThread([weakThis = createWeakPtr()] {
        if (!weakThis)
            return;
        weakThis.get()->pauseInternal();
    });
}

void MediaPlayerPrivateMediaSourceAVFObjC::pauseInternal()
{
    m_playing = false;
    [m_synchronizer setRate:0];
}

bool MediaPlayerPrivateMediaSourceAVFObjC::paused() const
{
    return ![m_synchronizer rate];
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVolume(float volume)
{
    for (const auto& key : m_sampleBufferAudioRendererMap.keys())
        [(__bridge AVSampleBufferAudioRenderer *)key.get() setVolume:volume];
}

bool MediaPlayerPrivateMediaSourceAVFObjC::supportsScanning() const
{
    return true;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setMuted(bool muted)
{
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

void MediaPlayerPrivateMediaSourceAVFObjC::setVisible(bool)
{
    acceleratedRenderingStateChanged();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::durationMediaTime() const
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->duration() : MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateMediaSourceAVFObjC::currentMediaTime() const
{
    MediaTime synchronizerTime = PAL::toMediaTime(CMTimebaseGetTime([m_synchronizer timebase]));
    if (synchronizerTime < MediaTime::zeroTime())
        return MediaTime::zeroTime();
    if (synchronizerTime < m_lastSeekTime)
        return m_lastSeekTime;
    return synchronizerTime;
}

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
    LOG(MediaSource, "MediaPlayerPrivateMediaSourceAVFObjC::seekWithTolerance(%p) - time(%s), negativeThreshold(%s), positiveThreshold(%s)", this, toString(time).utf8().data(), toString(negativeThreshold).utf8().data(), toString(positiveThreshold).utf8().data());
    m_seeking = true;
    auto weakThis = createWeakPtr();
    m_pendingSeek = std::make_unique<PendingSeek>(time, negativeThreshold, positiveThreshold);

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

    MediaTime synchronizerTime = PAL::toMediaTime(CMTimebaseGetTime([m_synchronizer timebase]));
    LOG(MediaSource, "MediaPlayerPrivateMediaSourceAVFObjC::seekInternal(%p) - seekTime(%s), synchronizerTime(%s)", this, toString(m_lastSeekTime).utf8().data(), toString(synchronizerTime).utf8().data());

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
    LOG(MediaSource, "MediaPlayerPrivateMediaSourceAVFObjC::waitForSeekCompleted(%p)", this);
    m_seekCompleted = Seeking;
}

void MediaPlayerPrivateMediaSourceAVFObjC::seekCompleted()
{
    if (m_seekCompleted == SeekCompleted)
        return;
    if (hasVideo() && !m_hasAvailableVideoFrame) {
        m_seekCompleted = WaitingForAvailableFame;
        return;
    }
    LOG(MediaSource, "MediaPlayerPrivateMediaSourceAVFObjC::seekCompleted(%p)", this);
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

void MediaPlayerPrivateMediaSourceAVFObjC::setPreservesPitch(bool preservesPitch)
{
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
    return std::make_unique<PlatformTimeRanges>(minMediaTimeSeekable(), maxMediaTimeSeekable());
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
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->buffered() : std::make_unique<PlatformTimeRanges>();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::didLoadingProgress() const
{
    bool loadingProgressed = m_loadingProgressed;
    m_loadingProgressed = false;
    return loadingProgressed;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setSize(const IntSize&)
{
    // No-op.
}

NativeImagePtr MediaPlayerPrivateMediaSourceAVFObjC::nativeImageForCurrentTime()
{
    updateLastImage();
    return m_lastImage.get();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::updateLastPixelBuffer()
{
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
        m_rgbConformer = std::make_unique<PixelBufferConformerCV>((__bridge CFDictionaryRef)attributes);
    }

    m_lastImage = m_rgbConformer->createImageFromPixelBuffer(m_lastPixelBuffer.get());
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
    FloatRect imageRect(0, 0, CGImageGetWidth(image.get()), CGImageGetHeight(image.get()));
    context.drawNativeImage(image, imageRect.size(), outputRect, imageRect);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::copyVideoTextureToPlatformTexture(GraphicsContext3D* context, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY)
{
    // We have been asked to paint into a WebGL canvas, so take that as a signal to create
    // a decompression session, even if that means the native video can't also be displayed
    // in page.
    if (!m_hasBeenAskedToPaintGL) {
        m_hasBeenAskedToPaintGL = true;
        acceleratedRenderingStateChanged();
    }

    ASSERT(context);

    if (updateLastPixelBuffer()) {
        if (!m_lastPixelBuffer)
            return false;
    }

    size_t width = CVPixelBufferGetWidth(m_lastPixelBuffer.get());
    size_t height = CVPixelBufferGetHeight(m_lastPixelBuffer.get());

    if (!m_videoTextureCopier)
        m_videoTextureCopier = std::make_unique<VideoTextureCopierCV>(*context);

    return m_videoTextureCopier->copyImageToPlatformTexture(m_lastPixelBuffer.get(), width, height, outputTexture, outputTarget, level, internalFormat, format, type, premultiplyAlpha, flipY);
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
    if (!m_hasBeenAskedToPaintGL) {
        destroyDecompressionSession();
        ensureLayer();
    } else {
        destroyLayer();
        ensureDecompressionSession();
    }
}

void MediaPlayerPrivateMediaSourceAVFObjC::notifyActiveSourceBuffersChanged()
{
    m_player->client().mediaPlayerActiveSourceBuffersChanged(m_player);
}

MediaPlayer::MovieLoadType MediaPlayerPrivateMediaSourceAVFObjC::movieLoadType() const
{
    return MediaPlayer::StoredStream;
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

Optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateMediaSourceAVFObjC::videoPlaybackQualityMetrics()
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
        return WTF::nullopt;

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

    m_sampleBufferDisplayLayer = adoptNS([allocAVSampleBufferDisplayLayerInstance() init]);
#ifndef NDEBUG
    [m_sampleBufferDisplayLayer setName:@"MediaPlayerPrivateMediaSource AVSampleBufferDisplayLayer"];
#endif

    [m_synchronizer addRenderer:m_sampleBufferDisplayLayer.get()];
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->setVideoLayer(m_sampleBufferDisplayLayer.get());
    m_videoFullscreenLayerManager->setVideoLayer(m_sampleBufferDisplayLayer.get(), snappedIntRect(m_player->client().mediaPlayerContentBoxRect()).size());
    m_player->client().mediaPlayerRenderingModeChanged(m_player);
}

void MediaPlayerPrivateMediaSourceAVFObjC::destroyLayer()
{
    if (!m_sampleBufferDisplayLayer)
        return;

    CMTime currentTime = CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:m_sampleBufferDisplayLayer.get() atTime:currentTime withCompletionHandler:^(BOOL){
        // No-op.
    }];

    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->setVideoLayer(nullptr);
    m_videoFullscreenLayerManager->didDestroyVideoLayer();
    m_sampleBufferDisplayLayer = nullptr;
    setHasAvailableVideoFrame(false);
    m_player->client().mediaPlayerRenderingModeChanged(m_player);
}

void MediaPlayerPrivateMediaSourceAVFObjC::ensureDecompressionSession()
{
    if (m_decompressionSession)
        return;

    m_decompressionSession = WebCoreDecompressionSession::createOpenGL();
    m_decompressionSession->setTimebase([m_synchronizer timebase]);

    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->setDecompressionSession(m_decompressionSession.get());

    m_player->client().mediaPlayerRenderingModeChanged(m_player);
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
    return m_playing && !seeking() && allRenderersHaveAvailableSamples() && m_readyState >= MediaPlayer::HaveFutureData;
}

void MediaPlayerPrivateMediaSourceAVFObjC::setHasAvailableVideoFrame(bool flag)
{
    if (m_hasAvailableVideoFrame == flag)
        return;
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
    auto weakThis = createWeakPtr();
    NSArray* times = @[[NSValue valueWithCMTime:PAL::toCMTime(duration)]];

    LOG(MediaSource, "MediaPlayerPrivateMediaSourceAVFObjC::durationChanged(%p) - duration = %s", this, toString(duration).utf8().data());

    m_durationObserver = [m_synchronizer addBoundaryTimeObserverForTimes:times queue:dispatch_get_main_queue() usingBlock:[weakThis, duration] {
        if (!weakThis)
            return;

        MediaTime now = weakThis->currentMediaTime();
        LOG(MediaSource, "MediaPlayerPrivateMediaSourceAVFObjC::durationChanged(%p) - boundary time observer called, now = %s", weakThis.get(), toString(now).utf8().data());

        weakThis->pauseInternal();
        if (now < duration) {
            LOG(MediaSource, "   ERROR: boundary time observer called before duration!", weakThis.get());
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
AVStreamSession* MediaPlayerPrivateMediaSourceAVFObjC::streamSession()
{
    if (!getAVStreamSessionClass() || ![getAVStreamSessionClass() instancesRespondToSelector:@selector(initWithStorageDirectoryAtURL:)])
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
        m_streamSession = adoptNS([allocAVStreamSessionInstance() initWithStorageDirectoryAtURL:[NSURL fileURLWithPath:storagePath]]);
    }
    return m_streamSession.get();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setCDMSession(LegacyCDMSession* session)
{
    if (session == m_session)
        return;

    m_session = toCDMSessionMediaSourceAVFObjC(session);

    if (CDMSessionAVStreamSession* cdmStreamSession = toCDMSessionAVStreamSession(m_session))
        cdmStreamSession->setStreamSession(streamSession());
    for (auto& sourceBuffer : m_mediaSourcePrivate->sourceBuffers())
        sourceBuffer->setCDMSession(m_session);
}

void MediaPlayerPrivateMediaSourceAVFObjC::keyNeeded(Uint8Array* initData)
{
    m_player->keyNeeded(initData);
}

void MediaPlayerPrivateMediaSourceAVFObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
#if ENABLE(ENCRYPTED_MEDIA)
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->outputObscuredDueToInsufficientExternalProtectionChanged(obscured);
#else
    UNUSED_PARAM(obscured);
#endif
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateMediaSourceAVFObjC::cdmInstanceAttached(CDMInstance& instance)
{
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->cdmInstanceAttached(instance);
}

void MediaPlayerPrivateMediaSourceAVFObjC::cdmInstanceDetached(CDMInstance& instance)
{
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->cdmInstanceDetached(instance);
}

void MediaPlayerPrivateMediaSourceAVFObjC::attemptToDecryptWithInstance(CDMInstance& instance)
{
    if (m_mediaSourcePrivate)
        m_mediaSourcePrivate->attemptToDecryptWithInstance(instance);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::waitingForKey() const
{
    return m_mediaSourcePrivate ? m_mediaSourcePrivate->waitingForKey() : false;
}

void MediaPlayerPrivateMediaSourceAVFObjC::waitingForKeyChanged()
{
    m_player->waitingForKeyChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::initializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
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

    m_readyState = readyState;

    if (shouldBePlaying())
        [m_synchronizer setRate:m_rate];
    else
        [m_synchronizer setRate:0];

    if (m_readyState >= MediaPlayerEnums::HaveCurrentData && hasVideo() && !m_hasAvailableVideoFrame) {
        m_readyStateIsWaitingForAvailableFrame = true;
        return;
    }

    m_player->readyStateChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (m_networkState == networkState)
        return;

    m_networkState = networkState;
    m_player->networkStateChanged();
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::addAudioRenderer(AVSampleBufferAudioRenderer* audioRenderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    if (!m_sampleBufferAudioRendererMap.add((__bridge CFTypeRef)audioRenderer, AudioRendererProperties()).isNewEntry)
        return;

    [audioRenderer setMuted:m_player->muted()];
    [audioRenderer setVolume:m_player->volume()];
    [audioRenderer setAudioTimePitchAlgorithm:(m_player->preservesPitch() ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed)];

    [m_synchronizer addRenderer:audioRenderer];
    m_player->client().mediaPlayerRenderingModeChanged(m_player);
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
void MediaPlayerPrivateMediaSourceAVFObjC::removeAudioRenderer(AVSampleBufferAudioRenderer* audioRenderer)
ALLOW_NEW_API_WITHOUT_GUARDS_END
{
    auto iter = m_sampleBufferAudioRendererMap.find((__bridge CFTypeRef)audioRenderer);
    if (iter == m_sampleBufferAudioRendererMap.end())
        return;

    CMTime currentTime = CMTimebaseGetTime([m_synchronizer timebase]);
    [m_synchronizer removeRenderer:audioRenderer atTime:currentTime withCompletionHandler:^(BOOL){
        // No-op.
    }];

    m_sampleBufferAudioRendererMap.remove(iter);
    m_player->client().mediaPlayerRenderingModeChanged(m_player);
}

void MediaPlayerPrivateMediaSourceAVFObjC::characteristicsChanged()
{
    updateAllRenderersHaveAvailableSamples();
    m_player->characteristicChanged();
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
{
    updateLastImage();
    m_videoFullscreenLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), m_lastImage);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoFullscreenLayerManager->setVideoFullscreenFrame(frame);
}

bool MediaPlayerPrivateMediaSourceAVFObjC::requiresTextTrackRepresentation() const
{
    return m_videoFullscreenLayerManager->videoFullscreenLayer();
}
    
void MediaPlayerPrivateMediaSourceAVFObjC::syncTextTrackBounds()
{
    m_videoFullscreenLayerManager->syncTextTrackBounds();
}
    
void MediaPlayerPrivateMediaSourceAVFObjC::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    m_videoFullscreenLayerManager->setTextTrackRepresentation(representation);
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void MediaPlayerPrivateMediaSourceAVFObjC::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    m_playbackTarget = WTFMove(target);
}

void MediaPlayerPrivateMediaSourceAVFObjC::setShouldPlayToPlaybackTarget(bool shouldPlayToTarget)
{
    if (shouldPlayToTarget == m_shouldPlayToTarget)
        return;

    m_shouldPlayToTarget = shouldPlayToTarget;

    if (m_player)
        m_player->currentPlaybackTargetIsWirelessChanged();
}

bool MediaPlayerPrivateMediaSourceAVFObjC::isCurrentPlaybackTargetWireless() const
{
    if (!m_playbackTarget)
        return false;

    return m_shouldPlayToTarget && m_playbackTarget->hasActiveRoute();
}
#endif

bool MediaPlayerPrivateMediaSourceAVFObjC::performTaskAtMediaTime(WTF::Function<void()>&& task, MediaTime time)
{
    __block WTF::Function<void()> taskIn = WTFMove(task);

    [m_synchronizer addBoundaryTimeObserverForTimes:@[[NSValue valueWithCMTime:toCMTime(time)]] queue:dispatch_get_main_queue() usingBlock:^{
        taskIn();
    }];
    return true;
}

}

#endif
