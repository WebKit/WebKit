/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#import "MediaPlayerPrivateAVFoundationObjC.h"

#import "BlockExceptions.h"
#import "DataView.h"
#import "FloatConversion.h"
#import "FrameView.h"
#import "FloatConversion.h"
#import "GraphicsContext.h"
#import "InbandTextTrackPrivateAVFObjC.h"
#import "KURL.h"
#import "Logging.h"
#import "SecurityOrigin.h"
#import "SoftLinking.h"
#import "TimeRanges.h"
#import "UUID.h"
#import "WebCoreAVFResourceLoader.h"
#import "WebCoreSystemInterface.h"
#import <objc/runtime.h>
#import <wtf/UnusedParam.h>
#import <wtf/Uint8Array.h>
#import <wtf/Uint16Array.h>
#import <wtf/Uint32Array.h>

#import <CoreMedia/CoreMedia.h>
#import <AVFoundation/AVFoundation.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreMedia)

SOFT_LINK(CoreMedia, CMTimeCompare, int32_t, (CMTime time1, CMTime time2), (time1, time2))
SOFT_LINK(CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))
SOFT_LINK(CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))
SOFT_LINK(CoreMedia, CMTimeRangeGetEnd, CMTime, (CMTimeRange range), (range))

SOFT_LINK_CLASS(AVFoundation, AVPlayer)
SOFT_LINK_CLASS(AVFoundation, AVPlayerItem)
SOFT_LINK_CLASS(AVFoundation, AVPlayerItemVideoOutput)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)
SOFT_LINK_CLASS(AVFoundation, AVURLAsset)
SOFT_LINK_CLASS(AVFoundation, AVAssetImageGenerator)

SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicVisual, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicAudible, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeClosedCaption, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVPlayerItemDidPlayToEndTimeNotification, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVAssetImageGeneratorApertureModeCleanAperture, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVURLAssetReferenceRestrictionsKey, NSString *)

SOFT_LINK_CONSTANT(CoreMedia, kCMTimeZero, CMTime)

#define AVPlayer getAVPlayerClass()
#define AVPlayerItem getAVPlayerItemClass()
#define AVPlayerItemVideoOutput getAVPlayerItemVideoOutputClass()
#define AVPlayerLayer getAVPlayerLayerClass()
#define AVURLAsset getAVURLAssetClass()
#define AVAssetImageGenerator getAVAssetImageGeneratorClass()

#define AVMediaCharacteristicVisual getAVMediaCharacteristicVisual()
#define AVMediaCharacteristicAudible getAVMediaCharacteristicAudible()
#define AVMediaTypeClosedCaption getAVMediaTypeClosedCaption()
#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVMediaTypeAudio getAVMediaTypeAudio()
#define AVPlayerItemDidPlayToEndTimeNotification getAVPlayerItemDidPlayToEndTimeNotification()
#define AVAssetImageGeneratorApertureModeCleanAperture getAVAssetImageGeneratorApertureModeCleanAperture()
#define AVURLAssetReferenceRestrictionsKey getAVURLAssetReferenceRestrictionsKey()

#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
typedef AVMediaSelectionGroup AVMediaSelectionGroupType;
typedef AVMediaSelectionOption AVMediaSelectionOptionType;

SOFT_LINK_CLASS(AVFoundation, AVPlayerItemLegibleOutput)
SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionGroup)
SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionOption)

SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicLegible, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeSubtitle, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicContainsOnlyForcedSubtitles, NSString *)

#define AVPlayerItemLegibleOutput getAVPlayerItemLegibleOutputClass()
#define AVMediaSelectionGroup getAVMediaSelectionGroupClass()
#define AVMediaSelectionOption getAVMediaSelectionOptionClass()
#define AVMediaCharacteristicLegible getAVMediaCharacteristicLegible()
#define AVMediaTypeSubtitle getAVMediaTypeSubtitle()
#define AVMediaCharacteristicContainsOnlyForcedSubtitles getAVMediaCharacteristicContainsOnlyForcedSubtitles()
#endif

#define kCMTimeZero getkCMTimeZero()

using namespace WebCore;
using namespace std;

enum MediaPlayerAVFoundationObservationContext {
    MediaPlayerAVFoundationObservationContextPlayerItem,
    MediaPlayerAVFoundationObservationContextPlayer
};

#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
@interface WebCoreAVFMovieObserver : NSObject <AVPlayerItemLegibleOutputPushDelegate>
#else
@interface WebCoreAVFMovieObserver : NSObject
#endif
{
    MediaPlayerPrivateAVFoundationObjC* m_callback;
    int m_delayCallbacks;
}
-(id)initWithCallback:(MediaPlayerPrivateAVFoundationObjC*)callback;
-(void)disconnect;
-(void)playableKnown;
-(void)metadataLoaded;
-(void)seekCompleted:(BOOL)finished;
-(void)didEnd:(NSNotification *)notification;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(MediaPlayerAVFoundationObservationContext)context;
#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
- (void)legibleOutput:(id)output didOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime;
#endif
@end

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
@interface WebCoreAVFLoaderDelegate : NSObject<AVAssetResourceLoaderDelegate> {
    MediaPlayerPrivateAVFoundationObjC* m_callback;
}
- (id)initWithCallback:(MediaPlayerPrivateAVFoundationObjC*)callback;
- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest;
@end
#endif

namespace WebCore {

static NSArray *assetMetadataKeyNames();
static NSArray *itemKVOProperties();

#if !LOG_DISABLED
static const char *boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivateAVFoundationObjC::create(MediaPlayer* player)
{ 
    return adoptPtr(new MediaPlayerPrivateAVFoundationObjC(player));
}

void MediaPlayerPrivateAVFoundationObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
#if ENABLE(ENCRYPTED_MEDIA)
        registrar(create, getSupportedTypes, extendedSupportsType, 0, 0, 0);
#else
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
#endif
}

MediaPlayerPrivateAVFoundationObjC::MediaPlayerPrivateAVFoundationObjC(MediaPlayer* player)
    : MediaPlayerPrivateAVFoundation(player)
    , m_objcObserver(AdoptNS, [[WebCoreAVFMovieObserver alloc] initWithCallback:this])
    , m_videoFrameHasDrawn(false)
    , m_haveCheckedPlayability(false)
#if ENABLE(ENCRYPTED_MEDIA)
    , m_loaderDelegate(AdoptNS, [[WebCoreAVFLoaderDelegate alloc] initWithCallback:this])
#endif
#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
    , m_currentTrack(0)
#endif
{
}

MediaPlayerPrivateAVFoundationObjC::~MediaPlayerPrivateAVFoundationObjC()
{
    cancelLoad();
    [m_objcObserver.get() disconnect];
}

void MediaPlayerPrivateAVFoundationObjC::cancelLoad()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::cancelLoad(%p)", this);
    tearDownVideoRendering();

    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];

    // Tell our observer to do nothing when our cancellation of pending loading calls its completion handler.
    setIgnoreLoadStateChanges(true);
    if (m_avAsset) {
        [m_avAsset.get() cancelLoading];
        m_avAsset = nil;
    }

#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
    if (m_legibleOutput) {
        if (m_avPlayerItem)
            [m_avPlayerItem.get() removeOutput:m_legibleOutput.get()];
        m_legibleOutput = nil;
    }
#endif
    if (m_avPlayerItem) {
        for (NSString *keyName in itemKVOProperties())
            [m_avPlayerItem.get() removeObserver:m_objcObserver.get() forKeyPath:keyName];
        
        m_avPlayerItem = nil;
    }
    if (m_avPlayer) {
        if (m_timeObserver)
            [m_avPlayer.get() removeTimeObserver:m_timeObserver.get()];
        m_timeObserver = nil;
        [m_avPlayer.get() removeObserver:m_objcObserver.get() forKeyPath:@"rate"];
        m_avPlayer = nil;
    }
    setIgnoreLoadStateChanges(false);
}

bool MediaPlayerPrivateAVFoundationObjC::hasLayerRenderer() const
{
    return m_videoLayer;
}

bool MediaPlayerPrivateAVFoundationObjC::hasContextRenderer() const
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    return m_videoOutput;
#else
    return m_imageGenerator;
#endif
}

void MediaPlayerPrivateAVFoundationObjC::createContextVideoRenderer()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    createVideoOutput();
#else
    createImageGenerator();
#endif
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 1080
void MediaPlayerPrivateAVFoundationObjC::createImageGenerator()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createImageGenerator(%p)", this);

    if (!m_avAsset || m_imageGenerator)
        return;

    m_imageGenerator = [AVAssetImageGenerator assetImageGeneratorWithAsset:m_avAsset.get()];

    [m_imageGenerator.get() setApertureMode:AVAssetImageGeneratorApertureModeCleanAperture];
    [m_imageGenerator.get() setAppliesPreferredTrackTransform:YES];
    [m_imageGenerator.get() setRequestedTimeToleranceBefore:kCMTimeZero];
    [m_imageGenerator.get() setRequestedTimeToleranceAfter:kCMTimeZero];

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createImageGenerator(%p) - returning %p", this, m_imageGenerator.get());
}
#endif

void MediaPlayerPrivateAVFoundationObjC::destroyContextVideoRenderer()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    destroyVideoOutput();
#else
    destroyImageGenerator();
#endif
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 1080
void MediaPlayerPrivateAVFoundationObjC::destroyImageGenerator()
{
    if (!m_imageGenerator)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::destroyImageGenerator(%p) - destroying  %p", this, m_imageGenerator.get());

    m_imageGenerator = 0;
}
#endif

void MediaPlayerPrivateAVFoundationObjC::createVideoLayer()
{
    if (!m_avPlayer)
        return;

    if (!m_videoLayer) {
        m_videoLayer.adoptNS([[AVPlayerLayer alloc] init]);
        [m_videoLayer.get() setPlayer:m_avPlayer.get()];
#ifndef NDEBUG
        [m_videoLayer.get() setName:@"Video layer"];
#endif
        LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createVideoLayer(%p) - returning %p", this, m_videoLayer.get());
    }
}

void MediaPlayerPrivateAVFoundationObjC::destroyVideoLayer()
{
    if (!m_videoLayer)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::destroyVideoLayer(%p) - destroying", this, m_videoLayer.get());

    [m_videoLayer.get() setPlayer:nil];

    m_videoLayer = 0;
}

bool MediaPlayerPrivateAVFoundationObjC::hasAvailableVideoFrame() const
{
    return (m_videoFrameHasDrawn || (m_videoLayer && [m_videoLayer.get() isReadyForDisplay]));
}

void MediaPlayerPrivateAVFoundationObjC::createAVAssetForURL(const String& url)
{
    if (m_avAsset)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createAVAssetForURL(%p)", this);

    setDelayCallbacks(true);

    RetainPtr<NSMutableDictionary> options(AdoptNS, [[NSMutableDictionary alloc] init]);    

    [options.get() setObject:[NSNumber numberWithInt:AVAssetReferenceRestrictionForbidRemoteReferenceToLocal | AVAssetReferenceRestrictionForbidLocalReferenceToRemote] forKey:AVURLAssetReferenceRestrictionsKey];

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    RetainPtr<NSMutableDictionary> headerFields(AdoptNS, [[NSMutableDictionary alloc] init]);

    String referrer = player()->referrer();
    if (!referrer.isEmpty())
        [headerFields.get() setObject:referrer forKey:@"Referer"];

    String userAgent = player()->userAgent();
    if (!userAgent.isEmpty())
        [headerFields.get() setObject:userAgent forKey:@"User-Agent"];

    if ([headerFields.get() count])
        [options.get() setObject:headerFields.get() forKey:@"AVURLAssetHTTPHeaderFieldsKey"];
#endif

    NSURL *cocoaURL = KURL(ParsedURLString, url);
    m_avAsset.adoptNS([[AVURLAsset alloc] initWithURL:cocoaURL options:options.get()]);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    [[m_avAsset.get() resourceLoader] setDelegate:m_loaderDelegate.get() queue:dispatch_get_main_queue()];
#endif

    m_haveCheckedPlayability = false;

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::createAVPlayer()
{
    if (m_avPlayer)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createAVPlayer(%p)", this);

    setDelayCallbacks(true);

    m_avPlayer.adoptNS([[AVPlayer alloc] init]);
    [m_avPlayer.get() addObserver:m_objcObserver.get() forKeyPath:@"rate" options:nil context:(void *)MediaPlayerAVFoundationObservationContextPlayer];

#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
    [m_avPlayer.get() setAppliesMediaSelectionCriteriaAutomatically:YES];
#endif
    

    if (m_avPlayerItem)
        [m_avPlayer.get() replaceCurrentItemWithPlayerItem:m_avPlayerItem.get()];

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::createAVPlayerItem()
{
    if (m_avPlayerItem)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createAVPlayerItem(%p)", this);

    setDelayCallbacks(true);

    // Create the player item so we can load media data. 
    m_avPlayerItem.adoptNS([[AVPlayerItem alloc] initWithAsset:m_avAsset.get()]);
    
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(didEnd:) name:AVPlayerItemDidPlayToEndTimeNotification object:m_avPlayerItem.get()];

    for (NSString *keyName in itemKVOProperties())
        [m_avPlayerItem.get() addObserver:m_objcObserver.get() forKeyPath:keyName options:nil context:(void *)MediaPlayerAVFoundationObservationContextPlayerItem];

    if (m_avPlayer)
        [m_avPlayer.get() replaceCurrentItemWithPlayerItem:m_avPlayerItem.get()];

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::checkPlayability()
{
    if (m_haveCheckedPlayability)
        return;
    m_haveCheckedPlayability = true;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::checkPlayability(%p)", this);

    [m_avAsset.get() loadValuesAsynchronouslyForKeys:[NSArray arrayWithObject:@"playable"] completionHandler:^{
        [m_objcObserver.get() playableKnown];
    }];
}

void MediaPlayerPrivateAVFoundationObjC::beginLoadingMetadata()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::beginLoadingMetadata(%p) - requesting metadata loading", this);
    [m_avAsset.get() loadValuesAsynchronouslyForKeys:[assetMetadataKeyNames() retain] completionHandler:^{
        [m_objcObserver.get() metadataLoaded];
    }];
}

MediaPlayerPrivateAVFoundation::ItemStatus MediaPlayerPrivateAVFoundationObjC::playerItemStatus() const
{
    if (!m_avPlayerItem)
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusDoesNotExist;

    AVPlayerItemStatus status = [m_avPlayerItem.get() status];
    if (status == AVPlayerItemStatusUnknown)
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusUnknown;
    if (status == AVPlayerItemStatusFailed)
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusFailed;
    if ([m_avPlayerItem.get() isPlaybackLikelyToKeepUp])
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusPlaybackLikelyToKeepUp;
    if ([m_avPlayerItem.get() isPlaybackBufferFull])
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusPlaybackBufferFull;
    if ([m_avPlayerItem.get() isPlaybackBufferEmpty])
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusPlaybackBufferEmpty;

    return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusReadyToPlay;
}

PlatformMedia MediaPlayerPrivateAVFoundationObjC::platformMedia() const
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::platformMedia(%p)", this);
    PlatformMedia pm;
    pm.type = PlatformMedia::AVFoundationMediaPlayerType;
    pm.media.avfMediaPlayer = m_avPlayer.get();
    return pm;
}

PlatformLayer* MediaPlayerPrivateAVFoundationObjC::platformLayer() const
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::platformLayer(%p)", this);
    return m_videoLayer.get();
}

void MediaPlayerPrivateAVFoundationObjC::platformSetVisible(bool isVisible)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];    
    if (m_videoLayer)
        [m_videoLayer.get() setHidden:!isVisible];
    [CATransaction commit];
}
    
void MediaPlayerPrivateAVFoundationObjC::platformPlay()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::platformPlay(%p)", this);
    if (!metaDataAvailable())
        return;

    setDelayCallbacks(true);
    [m_avPlayer.get() setRate:requestedRate()];
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::platformPause()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::platformPause(%p)", this);
    if (!metaDataAvailable())
        return;

    setDelayCallbacks(true);
    [m_avPlayer.get() setRate:nil];
    setDelayCallbacks(false);
}

float MediaPlayerPrivateAVFoundationObjC::platformDuration() const
{
    // Do not ask the asset for duration before it has been loaded or it will fetch the
    // answer synchronously.
    if (!m_avAsset || assetStatus() < MediaPlayerAVAssetStatusLoaded)
         return MediaPlayer::invalidTime();
    
    CMTime cmDuration;
    
    // Check the AVItem if we have one and it has loaded duration, some assets never report duration.
    if (m_avPlayerItem && playerItemStatus() >= MediaPlayerAVPlayerItemStatusReadyToPlay)
        cmDuration = [m_avPlayerItem.get() duration];
    else
        cmDuration= [m_avAsset.get() duration];

    if (CMTIME_IS_NUMERIC(cmDuration))
        return narrowPrecisionToFloat(CMTimeGetSeconds(cmDuration));

    if (CMTIME_IS_INDEFINITE(cmDuration)) {
        if (![[m_avAsset.get() tracks] count])
            return 0;
        else
            return numeric_limits<float>::infinity();
    }

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::platformDuration(%p) - invalid duration, returning %.0f", this, MediaPlayer::invalidTime());
    return MediaPlayer::invalidTime();
}

float MediaPlayerPrivateAVFoundationObjC::currentTime() const
{
    if (!metaDataAvailable() || !m_avPlayerItem)
        return 0;

    CMTime itemTime = [m_avPlayerItem.get() currentTime];
    if (CMTIME_IS_NUMERIC(itemTime)) {
        return max(narrowPrecisionToFloat(CMTimeGetSeconds(itemTime)), 0.0f);
    }

    return 0;
}

void MediaPlayerPrivateAVFoundationObjC::seekToTime(float time)
{
    // setCurrentTime generates several event callbacks, update afterwards.
    setDelayCallbacks(true);

    WebCoreAVFMovieObserver *observer = m_objcObserver.get();
    [m_avPlayerItem.get() seekToTime:CMTimeMakeWithSeconds(time, 600) toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero completionHandler:^(BOOL finished) {
        [observer seekCompleted:finished];
    }];

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::setVolume(float volume)
{
    if (!metaDataAvailable())
        return;

    [m_avPlayer.get() setVolume:volume];
}

void MediaPlayerPrivateAVFoundationObjC::setClosedCaptionsVisible(bool closedCaptionsVisible)
{
    if (!metaDataAvailable())
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::setClosedCaptionsVisible(%p) - setting to %s", this, boolString(closedCaptionsVisible));
    [m_avPlayer.get() setClosedCaptionDisplayEnabled:closedCaptionsVisible];
}

void MediaPlayerPrivateAVFoundationObjC::updateRate()
{
    setDelayCallbacks(true);
    [m_avPlayer.get() setRate:requestedRate()];
    setDelayCallbacks(false);
}

float MediaPlayerPrivateAVFoundationObjC::rate() const
{
    if (!metaDataAvailable())
        return 0;

    return [m_avPlayer.get() rate];
}

PassRefPtr<TimeRanges> MediaPlayerPrivateAVFoundationObjC::platformBufferedTimeRanges() const
{
    RefPtr<TimeRanges> timeRanges = TimeRanges::create();

    if (!m_avPlayerItem)
        return timeRanges.release();

    NSArray *loadedRanges = [m_avPlayerItem.get() loadedTimeRanges];
    for (NSValue *thisRangeValue in loadedRanges) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (CMTIMERANGE_IS_VALID(timeRange) && !CMTIMERANGE_IS_EMPTY(timeRange)) {
            float rangeStart = narrowPrecisionToFloat(CMTimeGetSeconds(timeRange.start));
            float rangeEnd = narrowPrecisionToFloat(CMTimeGetSeconds(CMTimeRangeGetEnd(timeRange)));
            timeRanges->add(rangeStart, rangeEnd);
        }
    }
    return timeRanges.release();
}

float MediaPlayerPrivateAVFoundationObjC::platformMaxTimeSeekable() const
{
    NSArray *seekableRanges = [m_avPlayerItem.get() seekableTimeRanges];
    if (!seekableRanges)
        return 0;

    float maxTimeSeekable = 0;
    for (NSValue *thisRangeValue in seekableRanges) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;
        
        float endOfRange = narrowPrecisionToFloat(CMTimeGetSeconds(CMTimeRangeGetEnd(timeRange)));
        if (maxTimeSeekable < endOfRange)
            maxTimeSeekable = endOfRange;
    }
    return maxTimeSeekable;   
}

float MediaPlayerPrivateAVFoundationObjC::platformMaxTimeLoaded() const
{
    NSArray *loadedRanges = [m_avPlayerItem.get() loadedTimeRanges];
    if (!loadedRanges)
        return 0;

    float maxTimeLoaded = 0;
    for (NSValue *thisRangeValue in loadedRanges) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;
        
        float endOfRange = narrowPrecisionToFloat(CMTimeGetSeconds(CMTimeRangeGetEnd(timeRange)));
        if (maxTimeLoaded < endOfRange)
            maxTimeLoaded = endOfRange;
    }

    return maxTimeLoaded;   
}

unsigned MediaPlayerPrivateAVFoundationObjC::totalBytes() const
{
    if (!metaDataAvailable())
        return 0;

    long long totalMediaSize = 0;
    NSArray *tracks = [m_avAsset.get() tracks];
    for (AVAssetTrack *thisTrack in tracks)
        totalMediaSize += [thisTrack totalSampleDataLength];

    return static_cast<unsigned>(totalMediaSize);
}

void MediaPlayerPrivateAVFoundationObjC::setAsset(id asset)
{
    m_avAsset = asset;
}

MediaPlayerPrivateAVFoundation::AssetStatus MediaPlayerPrivateAVFoundationObjC::assetStatus() const
{
    if (!m_avAsset)
        return MediaPlayerAVAssetStatusDoesNotExist;

    for (NSString *keyName in assetMetadataKeyNames()) {
        AVKeyValueStatus keyStatus = [m_avAsset.get() statusOfValueForKey:keyName error:nil];
        if (keyStatus < AVKeyValueStatusLoaded)
            return MediaPlayerAVAssetStatusLoading;// At least one key is not loaded yet.
        
        if (keyStatus == AVKeyValueStatusFailed)
            return MediaPlayerAVAssetStatusFailed; // At least one key could not be loaded.

        if (keyStatus == AVKeyValueStatusCancelled)
            return MediaPlayerAVAssetStatusCancelled; // Loading of at least one key was cancelled.
    }

    if ([[m_avAsset.get() valueForKey:@"playable"] boolValue])
        return MediaPlayerAVAssetStatusPlayable;

    return MediaPlayerAVAssetStatusLoaded;
}

void MediaPlayerPrivateAVFoundationObjC::paintCurrentFrameInContext(GraphicsContext* context, const IntRect& rect)
{
    if (!metaDataAvailable() || context->paintingDisabled())
        return;

    paint(context, rect);
}

void MediaPlayerPrivateAVFoundationObjC::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!metaDataAvailable() || context->paintingDisabled())
        return;

    setDelayCallbacks(true);
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    paintWithVideoOutput(context, rect);
#else
    paintWithImageGenerator(context, rect);
#endif

    END_BLOCK_OBJC_EXCEPTIONS;
    setDelayCallbacks(false);

    m_videoFrameHasDrawn = true;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 1080
void MediaPlayerPrivateAVFoundationObjC::paintWithImageGenerator(GraphicsContext* context, const IntRect& rect)
{
    RetainPtr<CGImageRef> image = createImageForTimeInRect(currentTime(), rect);
    if (image) {
        GraphicsContextStateSaver stateSaver(*context);
        context->translate(rect.x(), rect.y() + rect.height());
        context->scale(FloatSize(1.0f, -1.0f));
        context->setImageInterpolationQuality(InterpolationLow);
        IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
        CGContextDrawImage(context->platformContext(), CGRectMake(0, 0, paintRect.width(), paintRect.height()), image.get());
        image = 0;
    }
}
#endif

static HashSet<String> mimeTypeCache()
{
    DEFINE_STATIC_LOCAL(HashSet<String>, cache, ());
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return cache;
    typeListInitialized = true;

    NSArray *types = [AVURLAsset audiovisualMIMETypes];
    for (NSString *mimeType in types)
        cache.add(mimeType);

    return cache;
} 

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 1080
RetainPtr<CGImageRef> MediaPlayerPrivateAVFoundationObjC::createImageForTimeInRect(float time, const IntRect& rect)
{
    if (!m_imageGenerator)
        createImageGenerator();
    ASSERT(m_imageGenerator);

#if !LOG_DISABLED
    double start = WTF::currentTime();
#endif

    [m_imageGenerator.get() setMaximumSize:CGSize(rect.size())];
    RetainPtr<CGImageRef> image = adoptCF([m_imageGenerator.get() copyCGImageAtTime:CMTimeMakeWithSeconds(time, 600) actualTime:nil error:nil]);

#if !LOG_DISABLED
    double duration = WTF::currentTime() - start;
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createImageForTimeInRect(%p) - creating image took %.4f", this, narrowPrecisionToFloat(duration));
#endif

    return image;
}
#endif

void MediaPlayerPrivateAVFoundationObjC::getSupportedTypes(HashSet<String>& supportedTypes)
{
    supportedTypes = mimeTypeCache();
} 

MediaPlayer::SupportsType MediaPlayerPrivateAVFoundationObjC::supportsType(const String& type, const String& codecs, const KURL&)
{
    if (!mimeTypeCache().contains(type))
        return MediaPlayer::IsNotSupported;

    // The spec says:
    // "Implementors are encouraged to return "maybe" unless the type can be confidently established as being supported or not."
    if (codecs.isEmpty())
        return MediaPlayer::MayBeSupported;

    NSString *typeString = [NSString stringWithFormat:@"%@; codecs=\"%@\"", (NSString *)type, (NSString *)codecs];
    return [AVURLAsset isPlayableExtendedMIMEType:typeString] ? MediaPlayer::IsSupported : MediaPlayer::MayBeSupported;;
}

#if ENABLE(ENCRYPTED_MEDIA)
static bool keySystemIsSupported(const String& keySystem)
{
    if (equalIgnoringCase(keySystem, "com.apple.lskd") || equalIgnoringCase(keySystem, "com.apple.lskd.1_0"))
        return true;

    return false;
}

MediaPlayer::SupportsType MediaPlayerPrivateAVFoundationObjC::extendedSupportsType(const String& type, const String& codecs, const String& keySystem, const KURL& url)
{
    // From: <http://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#dom-canplaytype>
    // In addition to the steps in the current specification, this method must run the following steps:

    // 1. Check whether the Key System is supported with the specified container and codec type(s) by following the steps for the first matching condition from the following list:
    //    If keySystem is null, continue to the next step.
    if (keySystem.isNull() || keySystem.isEmpty())
        return supportsType(type, codecs, url);

    // If keySystem contains an unrecognized or unsupported Key System, return the empty string
    if (!keySystemIsSupported(keySystem))
        return MediaPlayer::IsNotSupported;

    // If the Key System specified by keySystem does not support decrypting the container and/or codec specified in the rest of the type string.
    // (AVFoundation does not provide an API which would allow us to determine this, so this is a no-op)

    // 2. Return "maybe" or "probably" as appropriate per the existing specification of canPlayType().
    return supportsType(type, codecs, url);
}

bool MediaPlayerPrivateAVFoundationObjC::shouldWaitForLoadingOfResource(AVAssetResourceLoadingRequest* avRequest)
{
    String scheme = [[[avRequest request] URL] scheme];
    String keyURI = [[[avRequest request] URL] absoluteString];

#if ENABLE(ENCRYPTED_MEDIA)
    if (scheme == "skd") {
        // Create an initData with the following layout:
        // [4 bytes: keyURI size], [keyURI size bytes: keyURI]
        unsigned keyURISize = keyURI.length() * sizeof(UChar);
        RefPtr<ArrayBuffer> initDataBuffer = ArrayBuffer::create(4 + keyURISize, 1);
        RefPtr<DataView> initDataView = DataView::create(initDataBuffer, 0, initDataBuffer->byteLength());
        ExceptionCode ec = 0;
        initDataView->setUint32(0, keyURISize, true, ec);

        RefPtr<Uint16Array> keyURIArray = Uint16Array::create(initDataBuffer, 4, keyURI.length());
        keyURIArray->setRange(keyURI.characters(), keyURI.length() / sizeof(unsigned char), 0);

        if (!player()->keyNeeded("com.apple.lskd", emptyString(), static_cast<const unsigned char*>(initDataBuffer->data()), initDataBuffer->byteLength()))
            return false;

        m_keyURIToRequestMap.set(keyURI, avRequest);
        return true;
    }
#endif

    m_resourceLoader = WebCoreAVFResourceLoader::create(this, avRequest);
    m_resourceLoader->startLoading();
    return true;
}

void MediaPlayerPrivateAVFoundationObjC::didCancelLoadingRequest(AVAssetResourceLoadingRequest* avRequest)
{
    String scheme = [[[avRequest request] URL] scheme];

    if (m_resourceLoader)
        m_resourceLoader->stopLoading();
}
#endif

bool MediaPlayerPrivateAVFoundationObjC::isAvailable()
{
    return AVFoundationLibrary() && CoreMediaLibrary();
}

float MediaPlayerPrivateAVFoundationObjC::mediaTimeForTimeValue(float timeValue) const
{
    if (!metaDataAvailable())
        return timeValue;

    // FIXME - impossible to implement until rdar://8721510 is fixed.
    return timeValue;
}

void MediaPlayerPrivateAVFoundationObjC::tracksChanged()
{
    if (!m_avAsset)
        return;

#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
    if (m_avPlayerItem && !m_legibleOutput) {
        m_legibleOutput = adoptNS([[AVPlayerItemLegibleOutput alloc] initWithMediaSubtypesForNativeRepresentation:nil]);
        [m_legibleOutput.get() setSuppressesPlayerRendering:YES];

        // We enabled automatic media selection because we want alternate audio tracks to be enabled/disabled automatically,
        // but set the selected legible track to nil so text tracks will not be automatically configured.
        [m_avPlayerItem.get() selectMediaOption:nil inMediaSelectionGroup:[m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicLegible]];

        [m_legibleOutput.get() setDelegate:m_objcObserver.get() queue:dispatch_get_main_queue()];
        [m_legibleOutput.get() setAdvanceIntervalForDelegateInvocation:NSTimeIntervalSince1970];
        [m_avPlayerItem.get() addOutput:m_legibleOutput.get()];
    }
#endif

    bool hasCaptions = false;

    // This is called whenever the tracks collection changes so cache hasVideo and hasAudio since we are
    // asked about those fairly fequently.
    if (!m_avPlayerItem) {
        // We don't have a player item yet, so check with the asset because some assets support inspection
        // prior to becoming ready to play.
        setHasVideo([[m_avAsset.get() tracksWithMediaCharacteristic:AVMediaCharacteristicVisual] count]);
        setHasAudio([[m_avAsset.get() tracksWithMediaCharacteristic:AVMediaCharacteristicAudible] count]);
        hasCaptions = [[m_avAsset.get() tracksWithMediaType:AVMediaTypeClosedCaption] count];
    } else {
        bool hasVideo = false;
        bool hasAudio = false;
        NSArray *tracks = [m_avPlayerItem.get() tracks];
        for (AVPlayerItemTrack *track in tracks) {
            if ([track isEnabled]) {
                AVAssetTrack *assetTrack = [track assetTrack];
                if ([[assetTrack mediaType] isEqualToString:AVMediaTypeVideo])
                    hasVideo = true;
                else if ([[assetTrack mediaType] isEqualToString:AVMediaTypeAudio])
                    hasAudio = true;
                else if ([[assetTrack mediaType] isEqualToString:AVMediaTypeClosedCaption])
                    hasCaptions = true;
            }
        }
        setHasVideo(hasVideo);
        setHasAudio(hasAudio);
    }

#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
    if (!hasCaptions) {
        AVMediaSelectionGroupType *legibleGroup = [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicLegible];
        hasCaptions = [[AVMediaSelectionGroup playableMediaSelectionOptionsFromArray:[legibleGroup options]] count];
    }
    if (hasCaptions)
        processTextTracks();
#endif

    setHasClosedCaptions(hasCaptions);

    LOG(Media, "WebCoreAVFMovieObserver:tracksChanged(%p) - hasVideo = %s, hasAudio = %s, hasCaptions = %s",
        this, boolString(hasVideo()), boolString(hasAudio()), boolString(hasClosedCaptions()));

    sizeChanged();
}

void MediaPlayerPrivateAVFoundationObjC::sizeChanged()
{
    if (!m_avAsset)
        return;

    NSArray *tracks = [m_avAsset.get() tracks];

    // Some assets don't report track properties until they are completely ready to play, but we
    // want to report a size as early as possible so use presentationSize when an asset has no tracks.
    if (m_avPlayerItem && ![tracks count]) {
        setNaturalSize(IntSize([m_avPlayerItem.get() presentationSize]));
        return;
    }

    // AVAsset's 'naturalSize' property only considers the movie's first video track, so we need to compute
    // the union of all visual track rects.
    CGRect trackUnionRect = CGRectZero;
    for (AVAssetTrack *track in tracks) {
        CGSize trackSize = [track naturalSize];
        CGRect trackRect = CGRectMake(0, 0, trackSize.width, trackSize.height);
        trackUnionRect = CGRectUnion(trackUnionRect, CGRectApplyAffineTransform(trackRect, [track preferredTransform]));
    }

    // The movie is always displayed at 0,0 so move the track rect to the origin before using width and height.
    trackUnionRect = CGRectOffset(trackUnionRect, trackUnionRect.origin.x, trackUnionRect.origin.y);
    
    // Also look at the asset's preferred transform so we account for a movie matrix.
    CGSize naturalSize = CGSizeApplyAffineTransform(trackUnionRect.size, [m_avAsset.get() preferredTransform]);

    // Cache the natural size (setNaturalSize will notify the player if it has changed).
    setNaturalSize(IntSize(naturalSize));
}

bool MediaPlayerPrivateAVFoundationObjC::hasSingleSecurityOrigin() const 
{
    if (!m_avAsset)
        return false;
    
    RefPtr<SecurityOrigin> resolvedOrigin = SecurityOrigin::create(KURL(wkAVAssetResolvedURL(m_avAsset.get())));
    RefPtr<SecurityOrigin> requestedOrigin = SecurityOrigin::createFromString(assetURL());
    return resolvedOrigin->isSameSchemeHostPort(requestedOrigin.get());
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
void MediaPlayerPrivateAVFoundationObjC::createVideoOutput()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createVideoOutput(%p)", this);

    if (!m_avPlayerItem || m_videoOutput)
        return;

    NSDictionary* attributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithUnsignedInt:k32BGRAPixelFormat], kCVPixelBufferPixelFormatTypeKey,
                                nil];
    m_videoOutput.adoptNS([[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:attributes]);
    ASSERT(m_videoOutput);

    [m_avPlayerItem.get() addOutput:m_videoOutput.get()];

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createVideoOutput(%p) - returning %p", this, m_videoOutput.get());
}

void MediaPlayerPrivateAVFoundationObjC::destroyVideoOutput()
{
    if (!m_videoOutput)
        return;

    if (m_avPlayerItem)
        [m_avPlayerItem.get() removeOutput:m_videoOutput.get()];
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::destroyVideoOutput(%p) - destroying  %p", this, m_videoOutput.get());

    m_videoOutput = 0;
}

RetainPtr<CVPixelBufferRef> MediaPlayerPrivateAVFoundationObjC::createPixelBuffer()
{
    if (!m_videoOutput)
        createVideoOutput();
    ASSERT(m_videoOutput);

#if !LOG_DISABLED
    double start = WTF::currentTime();
#endif

    RetainPtr<CVPixelBufferRef> buffer = adoptCF([m_videoOutput.get() copyPixelBufferForItemTime:[m_avPlayerItem.get() currentTime] itemTimeForDisplay:nil]);

#if !LOG_DISABLED
    double duration = WTF::currentTime() - start;
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createPixelBuffer() - creating buffer took %.4f", this, narrowPrecisionToFloat(duration));
#endif

    return buffer;
}

void MediaPlayerPrivateAVFoundationObjC::paintWithVideoOutput(GraphicsContext* context, const IntRect& rect)
{
    RetainPtr<CVPixelBufferRef> pixelBuffer = createPixelBuffer();

    // Calls to copyPixelBufferForItemTime:itemTimeForDisplay: may return nil if the pixel buffer
    // for the requested time has already been retrieved. In this case, the last valid image (if any)
    // should be displayed.
    if (pixelBuffer)
        m_lastImage = pixelBuffer;

    if (m_lastImage) {
        GraphicsContextStateSaver stateSaver(*context);
        context->translate(rect.x(), rect.y() + rect.height());
        context->scale(FloatSize(1.0f, -1.0f));
        RetainPtr<CIImage> image(AdoptNS, [[CIImage alloc] initWithCVImageBuffer:m_lastImage.get()]);

        // ciContext does not use a RetainPtr for results of contextWithCGContext:, as the returned value
        // is autoreleased, and there is no non-autoreleased version of that function.
        CIContext* ciContext = [CIContext contextWithCGContext:context->platformContext() options:nil];
        [ciContext drawImage:image.get() inRect:rect fromRect:rect];
    }
}

#endif

#if ENABLE(ENCRYPTED_MEDIA)

static bool extractKeyURIKeyIDAndCertificateFromInitData(Uint8Array* initData, String& keyURI, String& keyID, RefPtr<Uint8Array>& certificate)
{
    // initData should have the following layout:
    // [4 bytes: keyURI length][N bytes: keyURI][4 bytes: contentID length], [N bytes: contentID], [4 bytes: certificate length][N bytes: certificate]
    if (initData->byteLength() < 4)
        return false;

    RefPtr<ArrayBuffer> initDataBuffer = initData->buffer();

    // Use a DataView to read uint32 values from the buffer, as Uint32Array requires the reads be aligned on 4-byte boundaries. 
    RefPtr<DataView> initDataView = DataView::create(initDataBuffer, 0, initDataBuffer->byteLength());
    uint32_t offset = 0;
    ExceptionCode ec = 0;

    uint32_t keyURILength = initDataView->getUint32(offset, true, ec);
    offset += 4;
    if (ec || offset + keyURILength > initData->length())
        return false;

    RefPtr<Uint16Array> keyURIArray = Uint16Array::create(initDataBuffer, offset, keyURILength);
    if (!keyURIArray)
        return false;

    keyURI = String(keyURIArray->data(), keyURILength / sizeof(unsigned short));
    offset += keyURILength;

    uint32_t keyIDLength = initDataView->getUint32(offset, true, ec);
    offset += 4;
    if (ec || offset + keyIDLength > initData->length())
        return false;

    RefPtr<Uint16Array> keyIDArray = Uint16Array::create(initDataBuffer, offset, keyIDLength);
    if (!keyIDArray)
        return false;

    keyID = String(keyIDArray->data(), keyIDLength / sizeof(unsigned short));
    offset += keyIDLength;

    uint32_t certificateLength = initDataView->getUint32(offset, true, ec);
    offset += 4;
    if (ec || offset + certificateLength > initData->length())
        return false;

    certificate = Uint8Array::create(initDataBuffer, offset, certificateLength);
    if (!certificate)
        return false;

    return true;
}

MediaPlayer::MediaKeyException MediaPlayerPrivateAVFoundationObjC::generateKeyRequest(const String& keySystem, const unsigned char* initDataPtr, unsigned initDataLength)
{
    if (!keySystemIsSupported(keySystem))
        return MediaPlayer::KeySystemNotSupported;

    RefPtr<Uint8Array> initData = Uint8Array::create(initDataPtr, initDataLength);
    String keyURI;
    String keyID;
    RefPtr<Uint8Array> certificate;
    if (!extractKeyURIKeyIDAndCertificateFromInitData(initData.get(), keyURI, keyID, certificate))
        return MediaPlayer::InvalidPlayerState;

    if (!m_keyURIToRequestMap.contains(keyURI))
        return MediaPlayer::InvalidPlayerState;

    String sessionID = createCanonicalUUIDString();

    RetainPtr<AVAssetResourceLoadingRequest> avRequest = m_keyURIToRequestMap.get(keyURI);

    RetainPtr<NSData> certificateData = adoptNS([[NSData alloc] initWithBytes:certificate->baseAddress() length:certificate->byteLength()]);
    NSString* assetStr = keyID;
    RetainPtr<NSData> assetID = [NSData dataWithBytes: [assetStr cStringUsingEncoding:NSUTF8StringEncoding] length:[assetStr lengthOfBytesUsingEncoding:NSUTF8StringEncoding]];
    NSError* error = 0;
    RetainPtr<NSData> keyRequest = [avRequest.get() streamingContentKeyRequestDataForApp:certificateData.get() contentIdentifier:assetID.get() options:nil error:&error];

    if (!keyRequest) {
        NSError* underlyingError = [[error userInfo] objectForKey:NSUnderlyingErrorKey];
        player()->keyError(keySystem, sessionID, MediaPlayerClient::DomainError, [underlyingError code]);
        return MediaPlayer::NoError;
    }

    RefPtr<ArrayBuffer> keyRequestBuffer = ArrayBuffer::create([keyRequest.get() bytes], [keyRequest.get() length]);
    RefPtr<Uint8Array> keyRequestArray = Uint8Array::create(keyRequestBuffer, 0, keyRequestBuffer->byteLength());
    player()->keyMessage(keySystem, sessionID, keyRequestArray->data(), keyRequestArray->byteLength(), KURL());

    // Move ownership of the AVAssetResourceLoadingRequestfrom the keyIDToRequestMap to the sessionIDToRequestMap:
    m_sessionIDToRequestMap.set(sessionID, avRequest);
    m_keyURIToRequestMap.remove(keyURI);

    return MediaPlayer::NoError;
}

MediaPlayer::MediaKeyException MediaPlayerPrivateAVFoundationObjC::addKey(const String& keySystem, const unsigned char* keyPtr, unsigned keyLength, const unsigned char* initDataPtr, unsigned initDataLength, const String& sessionID)
{
    if (!keySystemIsSupported(keySystem))
        return MediaPlayer::KeySystemNotSupported;

    if (!m_sessionIDToRequestMap.contains(sessionID))
        return MediaPlayer::InvalidPlayerState;

    RetainPtr<AVAssetResourceLoadingRequest> avRequest = m_sessionIDToRequestMap.get(sessionID);
    RetainPtr<NSData> keyData = adoptNS([[NSData alloc] initWithBytes:keyPtr length:keyLength]);
    [[avRequest.get() dataRequest] respondWithData:keyData.get()];
    [avRequest.get() finishLoading];
    m_sessionIDToRequestMap.remove(sessionID);

    player()->keyAdded(keySystem, sessionID);

    UNUSED_PARAM(initDataPtr);
    UNUSED_PARAM(initDataLength);
    return MediaPlayer::NoError;
}

MediaPlayer::MediaKeyException MediaPlayerPrivateAVFoundationObjC::cancelKeyRequest(const String& keySystem, const String& sessionID)
{
    if (!keySystemIsSupported(keySystem))
        return MediaPlayer::KeySystemNotSupported;

    if (!m_sessionIDToRequestMap.contains(sessionID))
        return MediaPlayer::InvalidPlayerState;

    m_sessionIDToRequestMap.remove(sessionID);
    return MediaPlayer::NoError;
}
#endif

#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
    
void MediaPlayerPrivateAVFoundationObjC::processTextTracks()
{
    AVMediaSelectionGroupType *legibleGroup = [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicLegible];
    if (!legibleGroup) {
        LOG(Media, "MediaPlayerPrivateAVFoundationObjC::processTextTracks(%p) - nil mediaSelectionGroup", this);
        return;
    }

    Vector<RefPtr<InbandTextTrackPrivateAVF> > removedTextTracks = m_textTracks;
    NSArray *legibleOptions = [AVMediaSelectionGroup playableMediaSelectionOptionsFromArray:[legibleGroup options]];
    for (AVMediaSelectionOptionType *option in legibleOptions) {
        bool newTrack = true;
        for (unsigned i = removedTextTracks.size(); i > 0; --i) {
            RefPtr<InbandTextTrackPrivateAVFObjC> track = static_cast<InbandTextTrackPrivateAVFObjC*>(removedTextTracks[i - 1].get());
            if ([track->mediaSelectionOption() isEqual:option]) {
                removedTextTracks.remove(i - 1);
                newTrack = false;
                break;
            }
        }
        if (!newTrack)
            continue;

        if ([[option mediaType] isEqualToString:AVMediaTypeSubtitle]) {
            if (![option hasMediaCharacteristic:AVMediaCharacteristicContainsOnlyForcedSubtitles]) {
                AVMediaSelectionOptionType *forcedOnlyOption = [option associatedMediaSelectionOptionInMediaSelectionGroup:legibleGroup];
                if (forcedOnlyOption)
                    continue;
            }
        }

        m_textTracks.append(InbandTextTrackPrivateAVFObjC::create(this, option));
    }

    if (removedTextTracks.size()) {
        for (unsigned i = 0; i < m_textTracks.size(); ++i) {
            RefPtr<InbandTextTrackPrivateAVF> track = static_cast<InbandTextTrackPrivateAVF*>(m_textTracks[i].get());
            
            if (!removedTextTracks.contains(track))
                continue;
    
            player()->removeTextTrack(removedTextTracks[i].get());
            m_textTracks.remove(i);
        }
    }

    for (unsigned i = 0; i < m_textTracks.size(); ++i) {
        RefPtr<InbandTextTrackPrivateAVF> track = static_cast<InbandTextTrackPrivateAVF*>(m_textTracks[i].get());

        track->setTextTrackIndex(i);
        if (track->hasBeenReported())
            continue;

        track->setHasBeenReported(true);
        player()->addTextTrack(track.get());
    }
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::processTextTracks(%p) - found %i media selection options", this, m_textTracks.size());
}

void MediaPlayerPrivateAVFoundationObjC::processCue(NSArray *attributedStrings, double time)
{
    if (!m_currentTrack)
        return;

    m_currentTrack->processCue(reinterpret_cast<CFArrayRef>(attributedStrings), time);
}

void MediaPlayerPrivateAVFoundationObjC::setCurrentTrack(InbandTextTrackPrivateAVF *track)
{
    InbandTextTrackPrivateAVFObjC* trackPrivate = static_cast<InbandTextTrackPrivateAVFObjC*>(track);
    AVMediaSelectionOptionType *mediaSelectionOption = trackPrivate ? trackPrivate->mediaSelectionOption() : 0;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::setCurrentTrack(%p) - selecting media option %p", this, mediaSelectionOption);

    [m_avPlayerItem.get() selectMediaOption:mediaSelectionOption inMediaSelectionGroup:[m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicLegible]];
    m_currentTrack = trackPrivate;
}

InbandTextTrackPrivateAVF* MediaPlayerPrivateAVFoundationObjC::currentTrack()
{
    return m_currentTrack;
}
#endif // HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)

NSArray* assetMetadataKeyNames()
{
    static NSArray* keys;
    if (!keys) {
        keys = [[NSArray alloc] initWithObjects:@"duration",
                    @"naturalSize",
                    @"preferredTransform",
                    @"preferredVolume",
                    @"preferredRate",
                    @"playable",
                    @"tracks",
                   nil];
    }
    return keys;
}

NSArray* itemKVOProperties()
{
    static NSArray* keys;
    if (!keys) {
        keys = [[NSArray alloc] initWithObjects:@"presentationSize",
                @"status",
                @"asset",
                @"tracks",
                @"seekableTimeRanges",
                @"loadedTimeRanges",
                @"playbackLikelyToKeepUp",
                @"playbackBufferFull",
                @"playbackBufferEmpty",
                @"duration",
                nil];
    }
    return keys;
}

} // namespace WebCore

@implementation WebCoreAVFMovieObserver

- (id)initWithCallback:(MediaPlayerPrivateAVFoundationObjC*)callback
{
    m_callback = callback;
    return [super init];
}

- (void)disconnect
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    m_callback = 0;
}

- (void)metadataLoaded
{
    if (!m_callback)
        return;

    m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::AssetMetadataLoaded);
}

- (void)playableKnown
{
    if (!m_callback)
        return;

    m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::AssetPlayabilityKnown);
}

- (void)seekCompleted:(BOOL)finished
{
    if (!m_callback)
        return;
    
    m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::SeekCompleted, static_cast<bool>(finished));
}

- (void)didEnd:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (!m_callback)
        return;
    m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemDidPlayToEndTime);
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(MediaPlayerAVFoundationObservationContext)context
{
    UNUSED_PARAM(change);

    LOG(Media, "WebCoreAVFMovieObserver:observeValueForKeyPath(%p) - keyPath = %s", self, [keyPath UTF8String]);

    if (!m_callback)
        return;

    if (context == MediaPlayerAVFoundationObservationContextPlayerItem) {
        // A value changed for an AVPlayerItem
        if ([keyPath isEqualToString:@"status"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemStatusChanged);
        else if ([keyPath isEqualToString:@"playbackLikelyToKeepUp"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemIsPlaybackLikelyToKeepUpChanged);
        else if ([keyPath isEqualToString:@"playbackBufferEmpty"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemIsPlaybackBufferEmptyChanged);
        else if ([keyPath isEqualToString:@"playbackBufferFull"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemIsPlaybackBufferFullChanged);
        else if ([keyPath isEqualToString:@"asset"])
            m_callback->setAsset([object asset]);
        else if ([keyPath isEqualToString:@"loadedTimeRanges"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemLoadedTimeRangesChanged);
        else if ([keyPath isEqualToString:@"seekableTimeRanges"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemSeekableTimeRangesChanged);
        else if ([keyPath isEqualToString:@"tracks"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemTracksChanged);
        else if ([keyPath isEqualToString:@"presentationSize"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemPresentationSizeChanged);
        else if ([keyPath isEqualToString:@"duration"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::DurationChanged);

        return;
    }

    if (context == MediaPlayerAVFoundationObservationContextPlayer) {
        // A value changed for an AVPlayer.
        if ([keyPath isEqualToString:@"rate"])
            m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::PlayerRateChanged);
    }
}

#if HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
- (void)legibleOutput:(id)output didOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime
{
    UNUSED_PARAM(output);
    UNUSED_PARAM(nativeSamples);

    if (!m_callback)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        m_callback->processCue(strings, CMTimeGetSeconds(itemTime));
    });
}
#endif

@end

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
@implementation WebCoreAVFLoaderDelegate

- (id)initWithCallback:(MediaPlayerPrivateAVFoundationObjC*)callback
{
    m_callback = callback;
    return [super init];
}

- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest
{
    UNUSED_PARAM(resourceLoader);
    return m_callback->shouldWaitForLoadingOfResource(loadingRequest);
}

- (void)resourceLoader:(AVAssetResourceLoader *)resourceLoader didCancelLoadingRequest:(AVAssetResourceLoadingRequest *)loadingRequest
{
    UNUSED_PARAM(resourceLoader);
    return m_callback->didCancelLoadingRequest(loadingRequest);
}
@end
#endif

#endif
