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

#import "AVTrackPrivateAVFObjCImpl.h"
#import "AudioTrackPrivateAVFObjC.h"
#import "AuthenticationChallenge.h"
#import "BlockExceptions.h"
#import "CDMSessionAVFoundationObjC.h"
#import "ExceptionCodePlaceholder.h"
#import "FloatConversion.h"
#import "FloatConversion.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "GraphicsContextCG.h"
#import "InbandTextTrackPrivateAVFObjC.h"
#import "InbandTextTrackPrivateLegacyAVFObjC.h"
#import "URL.h"
#import "Logging.h"
#import "PlatformTimeRanges.h"
#import "SecurityOrigin.h"
#import "SoftLinking.h"
#import "UUID.h"
#import "VideoTrackPrivateAVFObjC.h"
#import "WebCoreAVFResourceLoader.h"
#import "WebCoreSystemInterface.h"
#import <objc/runtime.h>
#import <runtime/DataView.h>
#import <runtime/JSCInlines.h>
#import <runtime/TypedArrayInlines.h>
#import <runtime/Uint16Array.h>
#import <runtime/Uint32Array.h>
#import <runtime/Uint8Array.h>
#import <wtf/CurrentTime.h>
#import <wtf/Functional.h>
#import <wtf/text/CString.h>

#if ENABLE(AVF_CAPTIONS)
#include "TextTrack.h"
#endif

#import <AVFoundation/AVFoundation.h>
#if PLATFORM(IOS)
#import <CoreImage/CoreImage.h>
#else
#import <QuartzCore/CoreImage.h>
#endif
#import <CoreMedia/CoreMedia.h>

#if USE(VIDEOTOOLBOX)
#import <CoreVideo/CoreVideo.h>
#import <VideoToolbox/VideoToolbox.h>
#endif

#if ENABLE(AVF_CAPTIONS)
// Note: This must be defined before our SOFT_LINK macros:
@class AVMediaSelectionOption;
@interface AVMediaSelectionOption (OutOfBandExtensions)
@property (nonatomic, readonly) NSString *outOfBandSource /*NS_AVAILABLE(TBD, TBD)*/;
@end
#endif

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreMedia)
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreImage)
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreVideo)

#if USE(VIDEOTOOLBOX)
SOFT_LINK_FRAMEWORK_OPTIONAL(VideoToolbox)
#endif

SOFT_LINK(CoreMedia, CMTimeCompare, int32_t, (CMTime time1, CMTime time2), (time1, time2))
SOFT_LINK(CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))
SOFT_LINK(CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))
SOFT_LINK(CoreMedia, CMTimeRangeGetEnd, CMTime, (CMTimeRange range), (range))

SOFT_LINK(CoreVideo, CVPixelBufferGetWidth, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetHeight, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetBaseAddress, void*, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetBytesPerRow, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetDataSize, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetPixelFormatType, OSType, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferLockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
SOFT_LINK(CoreVideo, CVPixelBufferUnlockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))

#if USE(VIDEOTOOLBOX)
SOFT_LINK(VideoToolbox, VTPixelTransferSessionCreate, OSStatus, (CFAllocatorRef allocator, VTPixelTransferSessionRef *pixelTransferSessionOut), (allocator, pixelTransferSessionOut))
SOFT_LINK(VideoToolbox, VTPixelTransferSessionTransferImage, OSStatus, (VTPixelTransferSessionRef session, CVPixelBufferRef sourceBuffer, CVPixelBufferRef destinationBuffer), (session, sourceBuffer, destinationBuffer))
#endif

SOFT_LINK_CLASS(AVFoundation, AVPlayer)
SOFT_LINK_CLASS(AVFoundation, AVPlayerItem)
SOFT_LINK_CLASS(AVFoundation, AVPlayerItemVideoOutput)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)
SOFT_LINK_CLASS(AVFoundation, AVURLAsset)
SOFT_LINK_CLASS(AVFoundation, AVAssetImageGenerator)
SOFT_LINK_CLASS(CoreImage, CIContext)
SOFT_LINK_CLASS(CoreImage, CIImage)

SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicVisual, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicAudible, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeClosedCaption, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVPlayerItemDidPlayToEndTimeNotification, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVAssetImageGeneratorApertureModeCleanAperture, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVURLAssetReferenceRestrictionsKey, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVLayerVideoGravityResizeAspect, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVLayerVideoGravityResize, NSString *)
SOFT_LINK_POINTER(CoreVideo, kCVPixelBufferPixelFormatTypeKey, NSString *)

SOFT_LINK_CONSTANT(CoreMedia, kCMTimeZero, CMTime)

#define AVPlayer getAVPlayerClass()
#define AVPlayerItem getAVPlayerItemClass()
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
#define AVLayerVideoGravityResizeAspect getAVLayerVideoGravityResizeAspect()
#define AVLayerVideoGravityResize getAVLayerVideoGravityResize()
#define kCVPixelBufferPixelFormatTypeKey getkCVPixelBufferPixelFormatTypeKey()

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
typedef AVMediaSelectionGroup AVMediaSelectionGroupType;
typedef AVMediaSelectionOption AVMediaSelectionOptionType;

SOFT_LINK_CLASS(AVFoundation, AVPlayerItemLegibleOutput)
SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionGroup)
SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionOption)

SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicLegible, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeSubtitle, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicContainsOnlyForcedSubtitles, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly, NSString *)

#define AVPlayerItemLegibleOutput getAVPlayerItemLegibleOutputClass()
#define AVMediaSelectionGroup getAVMediaSelectionGroupClass()
#define AVMediaSelectionOption getAVMediaSelectionOptionClass()
#define AVMediaCharacteristicLegible getAVMediaCharacteristicLegible()
#define AVMediaTypeSubtitle getAVMediaTypeSubtitle()
#define AVMediaCharacteristicContainsOnlyForcedSubtitles getAVMediaCharacteristicContainsOnlyForcedSubtitles()
#define AVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly getAVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly()
#endif

#if ENABLE(AVF_CAPTIONS)
SOFT_LINK_POINTER(AVFoundation, AVURLAssetOutOfBandAlternateTracksKey, NSString*)
SOFT_LINK_POINTER(AVFoundation, AVOutOfBandAlternateTrackDisplayNameKey, NSString*)
SOFT_LINK_POINTER(AVFoundation, AVOutOfBandAlternateTrackExtendedLanguageTagKey, NSString*)
SOFT_LINK_POINTER(AVFoundation, AVOutOfBandAlternateTrackIsDefaultKey, NSString*)
SOFT_LINK_POINTER(AVFoundation, AVOutOfBandAlternateTrackMediaCharactersticsKey, NSString*)
SOFT_LINK_POINTER(AVFoundation, AVOutOfBandAlternateTrackIdentifierKey, NSString*)
SOFT_LINK_POINTER(AVFoundation, AVOutOfBandAlternateTrackSourceKey, NSString*)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicDescribesMusicAndSoundForAccessibility, NSString*)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, NSString*)

#define AVURLAssetOutOfBandAlternateTracksKey getAVURLAssetOutOfBandAlternateTracksKey()
#define AVOutOfBandAlternateTrackDisplayNameKey getAVOutOfBandAlternateTrackDisplayNameKey()
#define AVOutOfBandAlternateTrackExtendedLanguageTagKey getAVOutOfBandAlternateTrackExtendedLanguageTagKey()
#define AVOutOfBandAlternateTrackIsDefaultKey getAVOutOfBandAlternateTrackIsDefaultKey()
#define AVOutOfBandAlternateTrackMediaCharactersticsKey getAVOutOfBandAlternateTrackMediaCharactersticsKey()
#define AVOutOfBandAlternateTrackIdentifierKey getAVOutOfBandAlternateTrackIdentifierKey()
#define AVOutOfBandAlternateTrackSourceKey getAVOutOfBandAlternateTrackSourceKey()
#define AVMediaCharacteristicDescribesMusicAndSoundForAccessibility getAVMediaCharacteristicDescribesMusicAndSoundForAccessibility()
#define AVMediaCharacteristicTranscribesSpokenDialogForAccessibility getAVMediaCharacteristicTranscribesSpokenDialogForAccessibility()
#endif

#define kCMTimeZero getkCMTimeZero()

using namespace WebCore;

enum MediaPlayerAVFoundationObservationContext {
    MediaPlayerAVFoundationObservationContextPlayerItem,
    MediaPlayerAVFoundationObservationContextPlayer
};

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
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
-(void)didEnd:(NSNotification *)notification;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(MediaPlayerAVFoundationObservationContext)context;
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
- (void)legibleOutput:(id)output didOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime;
- (void)outputSequenceWasFlushed:(id)output;
#endif
@end

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
@interface WebCoreAVFLoaderDelegate : NSObject<AVAssetResourceLoaderDelegate> {
    MediaPlayerPrivateAVFoundationObjC* m_callback;
}
- (id)initWithCallback:(MediaPlayerPrivateAVFoundationObjC*)callback;
- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest;
- (void)setCallback:(MediaPlayerPrivateAVFoundationObjC*)callback;
@end
#endif

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
@interface WebCoreAVFPullDelegate : NSObject<AVPlayerItemOutputPullDelegate> {
    MediaPlayerPrivateAVFoundationObjC *m_callback;
    dispatch_semaphore_t m_semaphore;
}
- (id)initWithCallback:(MediaPlayerPrivateAVFoundationObjC *)callback;
- (void)setCallback:(MediaPlayerPrivateAVFoundationObjC*)callback;
- (void)outputMediaDataWillChange:(AVPlayerItemOutput *)sender;
- (void)outputSequenceWasFlushed:(AVPlayerItemOutput *)output;
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

#if ENABLE(ENCRYPTED_MEDIA_V2)
typedef HashMap<MediaPlayer*, MediaPlayerPrivateAVFoundationObjC*> PlayerToPrivateMapType;
static PlayerToPrivateMapType& playerToPrivateMap()
{
    DEFINE_STATIC_LOCAL(PlayerToPrivateMapType, map, ());
    return map;
};
#endif

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
static dispatch_queue_t globalLoaderDelegateQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create("WebCoreAVFLoaderDelegate queue", DISPATCH_QUEUE_SERIAL);
    });
    return globalQueue;
}
#endif

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
static dispatch_queue_t globalPullDelegateQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create("WebCoreAVFPullDelegate queue", DISPATCH_QUEUE_SERIAL);
    });
    return globalQueue;
}
#endif

PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivateAVFoundationObjC::create(MediaPlayer* player)
{ 
    return adoptPtr(new MediaPlayerPrivateAVFoundationObjC(player));
}

void MediaPlayerPrivateAVFoundationObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0, supportsKeySystem);
}

MediaPlayerPrivateAVFoundationObjC::MediaPlayerPrivateAVFoundationObjC(MediaPlayer* player)
    : MediaPlayerPrivateAVFoundation(player)
    , m_weakPtrFactory(this)
    , m_objcObserver(adoptNS([[WebCoreAVFMovieObserver alloc] initWithCallback:this]))
    , m_videoFrameHasDrawn(false)
    , m_haveCheckedPlayability(false)
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    , m_videoOutputDelegate(adoptNS([[WebCoreAVFPullDelegate alloc] initWithCallback:this]))
    , m_videoOutputSemaphore(nullptr)
#endif
#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    , m_loaderDelegate(adoptNS([[WebCoreAVFLoaderDelegate alloc] initWithCallback:this]))
#endif
    , m_currentTrack(0)
    , m_cachedDuration(MediaPlayer::invalidTime())
    , m_cachedRate(0)
    , m_pendingStatusChanges(0)
    , m_cachedItemStatus(MediaPlayerAVPlayerItemStatusDoesNotExist)
    , m_cachedLikelyToKeepUp(false)
    , m_cachedBufferEmpty(false)
    , m_cachedBufferFull(false)
    , m_cachedHasEnabledAudio(false)
#if ENABLE(IOS_AIRPLAY)
    , m_allowsWirelessVideoPlayback(true)
#endif
{
#if ENABLE(ENCRYPTED_MEDIA_V2)
    playerToPrivateMap().set(player, this);
#endif
}

MediaPlayerPrivateAVFoundationObjC::~MediaPlayerPrivateAVFoundationObjC()
{
#if ENABLE(ENCRYPTED_MEDIA_V2)
    playerToPrivateMap().remove(player());
#endif
#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    [m_loaderDelegate.get() setCallback:0];
    [[m_avAsset.get() resourceLoader] setDelegate:nil queue:0];
#endif
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    [m_videoOutputDelegate setCallback:0];
    [m_videoOutput setDelegate:nil queue:0];
    if (m_videoOutputSemaphore)
        dispatch_release(m_videoOutputSemaphore);
#endif
    cancelLoad();
}

void MediaPlayerPrivateAVFoundationObjC::cancelLoad()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::cancelLoad(%p)", this);
    tearDownVideoRendering();

    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver.get() disconnect];

    // Tell our observer to do nothing when our cancellation of pending loading calls its completion handler.
    setIgnoreLoadStateChanges(true);
    if (m_avAsset) {
        [m_avAsset.get() cancelLoading];
        m_avAsset = nil;
    }

    clearTextTracks();

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
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
#if ENABLE(IOS_AIRPLAY)
        [m_avPlayer.get() removeObserver:m_objcObserver.get() forKeyPath:@"externalPlaybackActive"];
#endif
        m_avPlayer = nil;
    }

    // Reset cached properties
    m_pendingStatusChanges = 0;
    m_cachedItemStatus = MediaPlayerAVPlayerItemStatusDoesNotExist;
    m_cachedSeekableRanges = nullptr;
    m_cachedLoadedRanges = nullptr;
    m_cachedTracks = nullptr;
    m_cachedHasEnabledAudio = false;
    m_cachedPresentationSize = FloatSize();
    m_cachedDuration = 0;

    setIgnoreLoadStateChanges(false);
}

bool MediaPlayerPrivateAVFoundationObjC::hasLayerRenderer() const
{
    return m_videoLayer;
}

bool MediaPlayerPrivateAVFoundationObjC::hasContextRenderer() const
{
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    if (m_videoOutput)
        return true;
#endif
    return m_imageGenerator;
}

void MediaPlayerPrivateAVFoundationObjC::createContextVideoRenderer()
{
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    createVideoOutput();
#else
    createImageGenerator();
#endif
}

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

void MediaPlayerPrivateAVFoundationObjC::destroyContextVideoRenderer()
{
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    destroyVideoOutput();
#endif
    destroyImageGenerator();
}

void MediaPlayerPrivateAVFoundationObjC::destroyImageGenerator()
{
    if (!m_imageGenerator)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::destroyImageGenerator(%p) - destroying  %p", this, m_imageGenerator.get());

    m_imageGenerator = 0;
}

void MediaPlayerPrivateAVFoundationObjC::createVideoLayer()
{
    if (!m_avPlayer || m_videoLayer)
        return;

    auto weakThis = createWeakPtr();
    callOnMainThread([this, weakThis] {
        if (!weakThis)
            return;

        if (!m_avPlayer || m_videoLayer)
            return;

        m_videoLayer = adoptNS([[AVPlayerLayer alloc] init]);
        [m_videoLayer.get() setPlayer:m_avPlayer.get()];
        [m_videoLayer.get() setBackgroundColor:cachedCGColor(Color::black, ColorSpaceDeviceRGB)];
#ifndef NDEBUG
        [m_videoLayer.get() setName:@"Video layer"];
#endif
        updateVideoLayerGravity();
        LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createVideoLayer(%p) - returning %p", this, m_videoLayer.get());

        player()->mediaPlayerClient()->mediaPlayerRenderingModeChanged(player());
    });
}

void MediaPlayerPrivateAVFoundationObjC::destroyVideoLayer()
{
    if (!m_videoLayer)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::destroyVideoLayer(%p) - destroying %p", this, m_videoLayer.get());

    [m_videoLayer.get() setPlayer:nil];

    m_videoLayer = 0;
}

bool MediaPlayerPrivateAVFoundationObjC::hasAvailableVideoFrame() const
{
    if (currentRenderingMode() == MediaRenderingToLayer)
        return m_videoLayer && [m_videoLayer.get() isReadyForDisplay];

    return m_videoFrameHasDrawn;
}

#if ENABLE(AVF_CAPTIONS)
static const NSArray* mediaDescriptionForKind(PlatformTextTrack::TrackKind kind)
{
    // FIXME: Match these to correct types:
    if (kind == PlatformTextTrack::Caption)
        return [NSArray arrayWithObjects: AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, nil];

    if (kind == PlatformTextTrack::Subtitle)
        return [NSArray arrayWithObjects: AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, nil];

    if (kind == PlatformTextTrack::Description)
        return [NSArray arrayWithObjects: AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, AVMediaCharacteristicDescribesMusicAndSoundForAccessibility, nil];

    if (kind == PlatformTextTrack::Forced)
        return [NSArray arrayWithObjects: AVMediaCharacteristicContainsOnlyForcedSubtitles, nil];

    return [NSArray arrayWithObjects: AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, nil];
}
#endif

void MediaPlayerPrivateAVFoundationObjC::createAVAssetForURL(const String& url)
{
    if (m_avAsset)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createAVAssetForURL(%p)", this);

    setDelayCallbacks(true);

    RetainPtr<NSMutableDictionary> options = adoptNS([[NSMutableDictionary alloc] init]);    

    [options.get() setObject:[NSNumber numberWithInt:AVAssetReferenceRestrictionForbidRemoteReferenceToLocal | AVAssetReferenceRestrictionForbidLocalReferenceToRemote] forKey:AVURLAssetReferenceRestrictionsKey];

    RetainPtr<NSMutableDictionary> headerFields = adoptNS([[NSMutableDictionary alloc] init]);

    String referrer = player()->referrer();
    if (!referrer.isEmpty())
        [headerFields.get() setObject:referrer forKey:@"Referer"];

    String userAgent = player()->userAgent();
    if (!userAgent.isEmpty())
        [headerFields.get() setObject:userAgent forKey:@"User-Agent"];

    if ([headerFields.get() count])
        [options.get() setObject:headerFields.get() forKey:@"AVURLAssetHTTPHeaderFieldsKey"];

#if ENABLE(AVF_CAPTIONS)
    const Vector<RefPtr<PlatformTextTrack>>& outOfBandTrackSources = player()->outOfBandTrackSources();
    if (!outOfBandTrackSources.isEmpty()) {
        NSMutableArray* outOfBandTracks = [[NSMutableArray alloc] init];
        for (auto& trackSource : outOfBandTrackSources) {
            RetainPtr<CFStringRef> label = trackSource->label().createCFString();
            RetainPtr<CFStringRef> language = trackSource->language().createCFString();
            RetainPtr<CFStringRef> uniqueID = String::number(trackSource->uniqueId()).createCFString();
            RetainPtr<CFStringRef> url = trackSource->url().createCFString();
            [outOfBandTracks addObject:
                [NSDictionary dictionaryWithObjectsAndKeys:
                    reinterpret_cast<const NSString*>(label.get()), AVOutOfBandAlternateTrackDisplayNameKey,
                    reinterpret_cast<const NSString*>(language.get()), AVOutOfBandAlternateTrackExtendedLanguageTagKey,
                    [NSNumber numberWithBool: (trackSource->isDefault() ? YES : NO)], AVOutOfBandAlternateTrackIsDefaultKey,
                    reinterpret_cast<const NSString*>(uniqueID.get()), AVOutOfBandAlternateTrackIdentifierKey,
                    reinterpret_cast<const NSString*>(url.get()), AVOutOfBandAlternateTrackSourceKey,
                    mediaDescriptionForKind(trackSource->kind()), AVOutOfBandAlternateTrackMediaCharactersticsKey,
                    nil]];
        }

        [options.get() setObject: outOfBandTracks forKey: AVURLAssetOutOfBandAlternateTracksKey];
    }
#endif
    
    NSURL *cocoaURL = URL(ParsedURLString, url);
    m_avAsset = adoptNS([[AVURLAsset alloc] initWithURL:cocoaURL options:options.get()]);

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    [[m_avAsset.get() resourceLoader] setDelegate:m_loaderDelegate.get() queue:globalLoaderDelegateQueue()];
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

    m_avPlayer = adoptNS([[AVPlayer alloc] init]);
    [m_avPlayer.get() addObserver:m_objcObserver.get() forKeyPath:@"rate" options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextPlayer];
#if ENABLE(IOS_AIRPLAY)
    [m_avPlayer.get() addObserver:m_objcObserver.get() forKeyPath:@"externalPlaybackActive" options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextPlayer];
#endif

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    [m_avPlayer.get() setAppliesMediaSelectionCriteriaAutomatically:YES];
#endif

#if ENABLE(IOS_AIRPLAY)
    [m_avPlayer.get() setAllowsExternalPlayback:m_allowsWirelessVideoPlayback];
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
    m_avPlayerItem = adoptNS([[AVPlayerItem alloc] initWithAsset:m_avAsset.get()]);

    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(didEnd:) name:AVPlayerItemDidPlayToEndTimeNotification object:m_avPlayerItem.get()];

    NSKeyValueObservingOptions options = NSKeyValueObservingOptionNew | NSKeyValueObservingOptionPrior;
    for (NSString *keyName in itemKVOProperties())
        [m_avPlayerItem.get() addObserver:m_objcObserver.get() forKeyPath:keyName options:options context:(void *)MediaPlayerAVFoundationObservationContextPlayerItem];

    if (m_avPlayer)
        [m_avPlayer.get() replaceCurrentItemWithPlayerItem:m_avPlayerItem.get()];

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    const NSTimeInterval legibleOutputAdvanceInterval = 2;

    m_legibleOutput = adoptNS([[AVPlayerItemLegibleOutput alloc] initWithMediaSubtypesForNativeRepresentation:[NSArray array]]);
    [m_legibleOutput.get() setSuppressesPlayerRendering:YES];

    [m_legibleOutput.get() setDelegate:m_objcObserver.get() queue:dispatch_get_main_queue()];
    [m_legibleOutput.get() setAdvanceIntervalForDelegateInvocation:legibleOutputAdvanceInterval];
    [m_legibleOutput.get() setTextStylingResolution:AVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly];
    [m_avPlayerItem.get() addOutput:m_legibleOutput.get()];
#endif

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

    if (m_cachedItemStatus == AVPlayerItemStatusUnknown)
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusUnknown;
    if (m_cachedItemStatus == AVPlayerItemStatusFailed)
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusFailed;
    if (m_cachedLikelyToKeepUp)
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusPlaybackLikelyToKeepUp;
    if (m_cachedBufferFull)
        return MediaPlayerPrivateAVFoundation::MediaPlayerAVPlayerItemStatusPlaybackBufferFull;
    if (m_cachedBufferEmpty)
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
    m_cachedRate = requestedRate();
    [m_avPlayer.get() setRate:requestedRate()];
    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::platformPause()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::platformPause(%p)", this);
    if (!metaDataAvailable())
        return;

    setDelayCallbacks(true);
    m_cachedRate = 0;
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
        return std::numeric_limits<float>::infinity();
    }

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::platformDuration(%p) - invalid duration, returning %.0f", this, MediaPlayer::invalidTime());
    return MediaPlayer::invalidTime();
}

float MediaPlayerPrivateAVFoundationObjC::currentTime() const
{
    if (!metaDataAvailable() || !m_avPlayerItem)
        return 0;

    CMTime itemTime = [m_avPlayerItem.get() currentTime];
    if (CMTIME_IS_NUMERIC(itemTime))
        return std::max(narrowPrecisionToFloat(CMTimeGetSeconds(itemTime)), 0.0f);

    return 0;
}

void MediaPlayerPrivateAVFoundationObjC::seekToTime(double time, double negativeTolerance, double positiveTolerance)
{
    // setCurrentTime generates several event callbacks, update afterwards.
    setDelayCallbacks(true);

    CMTime cmTime = CMTimeMakeWithSeconds(time, 600);
    CMTime cmBefore = CMTimeMakeWithSeconds(negativeTolerance, 600);
    CMTime cmAfter = CMTimeMakeWithSeconds(positiveTolerance, 600);

    auto weakThis = createWeakPtr();

    [m_avPlayerItem.get() seekToTime:cmTime toleranceBefore:cmBefore toleranceAfter:cmAfter completionHandler:^(BOOL finished) {
        callOnMainThread([weakThis, finished] {
            auto _this = weakThis.get();
            if (!_this)
                return;

            _this->seekCompleted(finished);
        });
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
    UNUSED_PARAM(closedCaptionsVisible);

    if (!metaDataAvailable())
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::setClosedCaptionsVisible(%p) - set to %s", this, boolString(closedCaptionsVisible));
}

void MediaPlayerPrivateAVFoundationObjC::updateRate()
{
    setDelayCallbacks(true);
    m_cachedRate = requestedRate();
    [m_avPlayer.get() setRate:requestedRate()];
    setDelayCallbacks(false);
}

float MediaPlayerPrivateAVFoundationObjC::rate() const
{
    if (!metaDataAvailable())
        return 0;

    return m_cachedRate;
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateAVFoundationObjC::platformBufferedTimeRanges() const
{
    auto timeRanges = PlatformTimeRanges::create();

    if (!m_avPlayerItem)
        return timeRanges;

    for (NSValue *thisRangeValue in m_cachedLoadedRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (CMTIMERANGE_IS_VALID(timeRange) && !CMTIMERANGE_IS_EMPTY(timeRange)) {
            float rangeStart = narrowPrecisionToFloat(CMTimeGetSeconds(timeRange.start));
            float rangeEnd = narrowPrecisionToFloat(CMTimeGetSeconds(CMTimeRangeGetEnd(timeRange)));
            timeRanges->add(rangeStart, rangeEnd);
        }
    }
    return timeRanges;
}

double MediaPlayerPrivateAVFoundationObjC::platformMinTimeSeekable() const
{
    if (!m_cachedSeekableRanges || ![m_cachedSeekableRanges count])
        return 0;

    double minTimeSeekable = std::numeric_limits<double>::infinity();
    bool hasValidRange = false;
    for (NSValue *thisRangeValue in m_cachedSeekableRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;

        hasValidRange = true;
        double startOfRange = CMTimeGetSeconds(timeRange.start);
        if (minTimeSeekable > startOfRange)
            minTimeSeekable = startOfRange;
    }
    return hasValidRange ? minTimeSeekable : 0;
}

double MediaPlayerPrivateAVFoundationObjC::platformMaxTimeSeekable() const
{
    if (!m_cachedSeekableRanges)
        m_cachedSeekableRanges = [m_avPlayerItem seekableTimeRanges];

    double maxTimeSeekable = 0;
    for (NSValue *thisRangeValue in m_cachedSeekableRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;
        
        double endOfRange = CMTimeGetSeconds(CMTimeRangeGetEnd(timeRange));
        if (maxTimeSeekable < endOfRange)
            maxTimeSeekable = endOfRange;
    }
    return maxTimeSeekable;
}

float MediaPlayerPrivateAVFoundationObjC::platformMaxTimeLoaded() const
{
#if !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED <= 1080
    // AVFoundation on Mountain Lion will occasionally not send a KVO notification
    // when loadedTimeRanges changes when there is no video output. In that case
    // update the cached value explicitly.
    if (!hasLayerRenderer() && !hasContextRenderer())
        m_cachedLoadedRanges = [m_avPlayerItem loadedTimeRanges];
#endif

    if (!m_cachedLoadedRanges)
        return 0;

    float maxTimeLoaded = 0;
    for (NSValue *thisRangeValue in m_cachedLoadedRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;
        
        float endOfRange = narrowPrecisionToFloat(CMTimeGetSeconds(CMTimeRangeGetEnd(timeRange)));
        if (maxTimeLoaded < endOfRange)
            maxTimeLoaded = endOfRange;
    }

    return maxTimeLoaded;   
}

unsigned long long MediaPlayerPrivateAVFoundationObjC::totalBytes() const
{
    if (!metaDataAvailable())
        return 0;

    long long totalMediaSize = 0;
    for (AVPlayerItemTrack *thisTrack in m_cachedTracks.get())
        totalMediaSize += [[thisTrack assetTrack] totalSampleDataLength];

    return totalMediaSize;
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

    setDelayCallbacks(true);
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    if (videoOutputHasAvailableFrame())
        paintWithVideoOutput(context, rect);
    else
#endif
        paintWithImageGenerator(context, rect);

    END_BLOCK_OBJC_EXCEPTIONS;
    setDelayCallbacks(false);

    m_videoFrameHasDrawn = true;
}

void MediaPlayerPrivateAVFoundationObjC::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!metaDataAvailable() || context->paintingDisabled())
        return;

    // We can ignore the request if we are already rendering to a layer.
    if (currentRenderingMode() == MediaRenderingToLayer)
        return;

    paintCurrentFrameInContext(context, rect);
}

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

RetainPtr<CGImageRef> MediaPlayerPrivateAVFoundationObjC::createImageForTimeInRect(float time, const IntRect& rect)
{
    if (!m_imageGenerator)
        createImageGenerator();
    ASSERT(m_imageGenerator);

#if !LOG_DISABLED
    double start = monotonicallyIncreasingTime();
#endif

    [m_imageGenerator.get() setMaximumSize:CGSize(rect.size())];
    RetainPtr<CGImageRef> rawImage = adoptCF([m_imageGenerator.get() copyCGImageAtTime:CMTimeMakeWithSeconds(time, 600) actualTime:nil error:nil]);
    RetainPtr<CGImageRef> image = adoptCF(CGImageCreateCopyWithColorSpace(rawImage.get(), deviceRGBColorSpaceRef()));

#if !LOG_DISABLED
    double duration = monotonicallyIncreasingTime() - start;
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createImageForTimeInRect(%p) - creating image took %.4f", this, narrowPrecisionToFloat(duration));
#endif

    return image;
}

void MediaPlayerPrivateAVFoundationObjC::getSupportedTypes(HashSet<String>& supportedTypes)
{
    supportedTypes = mimeTypeCache();
} 

#if ENABLE(ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA_V2)
static bool keySystemIsSupported(const String& keySystem)
{
    if (equalIgnoringCase(keySystem, "com.apple.fps") || equalIgnoringCase(keySystem, "com.apple.fps.1_0"))
        return true;
    return false;
}
#endif

MediaPlayer::SupportsType MediaPlayerPrivateAVFoundationObjC::supportsType(const MediaEngineSupportParameters& parameters)
{
#if ENABLE(ENCRYPTED_MEDIA)
    // From: <http://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#dom-canplaytype>
    // In addition to the steps in the current specification, this method must run the following steps:

    // 1. Check whether the Key System is supported with the specified container and codec type(s) by following the steps for the first matching condition from the following list:
    //    If keySystem is null, continue to the next step.
    if (!parameters.keySystem.isNull() && !parameters.keySystem.isEmpty()) {
        // If keySystem contains an unrecognized or unsupported Key System, return the empty string
        if (!keySystemIsSupported(parameters.keySystem))
            return MediaPlayer::IsNotSupported;

        // If the Key System specified by keySystem does not support decrypting the container and/or codec specified in the rest of the type string.
        // (AVFoundation does not provide an API which would allow us to determine this, so this is a no-op)
    }

    // 2. Return "maybe" or "probably" as appropriate per the existing specification of canPlayType().
#endif

#if ENABLE(MEDIA_SOURCE)
    if (parameters.isMediaSource)
        return MediaPlayer::IsNotSupported;
#endif

    if (!mimeTypeCache().contains(parameters.type))
        return MediaPlayer::IsNotSupported;

    // The spec says:
    // "Implementors are encouraged to return "maybe" unless the type can be confidently established as being supported or not."
    if (parameters.codecs.isEmpty())
        return MediaPlayer::MayBeSupported;

    NSString *typeString = [NSString stringWithFormat:@"%@; codecs=\"%@\"", (NSString *)parameters.type, (NSString *)parameters.codecs];
    return [AVURLAsset isPlayableExtendedMIMEType:typeString] ? MediaPlayer::IsSupported : MediaPlayer::MayBeSupported;;
}

bool MediaPlayerPrivateAVFoundationObjC::supportsKeySystem(const String& keySystem, const String& mimeType)
{
#if ENABLE(ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA_V2)
    if (!keySystem.isEmpty()) {
        if (!keySystemIsSupported(keySystem))
            return false;

        if (!mimeType.isEmpty() && !mimeTypeCache().contains(mimeType))
            return false;

        return true;
    }
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(mimeType);
#endif
    return false;
}

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
bool MediaPlayerPrivateAVFoundationObjC::shouldWaitForLoadingOfResource(AVAssetResourceLoadingRequest* avRequest)
{
    String scheme = [[[avRequest request] URL] scheme];
    String keyURI = [[[avRequest request] URL] absoluteString];

#if ENABLE(ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA_V2)
    if (scheme == "skd") {
        // Create an initData with the following layout:
        // [4 bytes: keyURI size], [keyURI size bytes: keyURI]
        unsigned keyURISize = keyURI.length() * sizeof(UChar);
        RefPtr<ArrayBuffer> initDataBuffer = ArrayBuffer::create(4 + keyURISize, 1);
        RefPtr<JSC::DataView> initDataView = JSC::DataView::create(initDataBuffer, 0, initDataBuffer->byteLength());
        initDataView->set<uint32_t>(0, keyURISize, true);

        RefPtr<Uint16Array> keyURIArray = Uint16Array::create(initDataBuffer, 4, keyURI.length());
        keyURIArray->setRange(keyURI.deprecatedCharacters(), keyURI.length() / sizeof(unsigned char), 0);

#if ENABLE(ENCRYPTED_MEDIA)
        if (!player()->keyNeeded("com.apple.lskd", emptyString(), static_cast<const unsigned char*>(initDataBuffer->data()), initDataBuffer->byteLength()))
#elif ENABLE(ENCRYPTED_MEDIA_V2)
        RefPtr<Uint8Array> initData = Uint8Array::create(initDataBuffer, 0, initDataBuffer->byteLength());
        if (!player()->keyNeeded(initData.get()))
#endif
            return false;

        m_keyURIToRequestMap.set(keyURI, avRequest);
        return true;
    }
#endif

    RefPtr<WebCoreAVFResourceLoader> resourceLoader = WebCoreAVFResourceLoader::create(this, avRequest);
    m_resourceLoaderMap.add(avRequest, resourceLoader);
    resourceLoader->startLoading();
    return true;
}

bool MediaPlayerPrivateAVFoundationObjC::shouldWaitForResponseToAuthenticationChallenge(NSURLAuthenticationChallenge* nsChallenge)
{
#if USE(CFNETWORK)
    UNUSED_PARAM(nsChallenge);
    // FIXME: <rdar://problem/15799844>
    return false;
#else
    AuthenticationChallenge challenge(nsChallenge);

    return player()->shouldWaitForResponseToAuthenticationChallenge(challenge);
#endif
}

void MediaPlayerPrivateAVFoundationObjC::didCancelLoadingRequest(AVAssetResourceLoadingRequest* avRequest)
{
    String scheme = [[[avRequest request] URL] scheme];

    WebCoreAVFResourceLoader* resourceLoader = m_resourceLoaderMap.get(avRequest);

    if (resourceLoader)
        resourceLoader->stopLoading();
}

void MediaPlayerPrivateAVFoundationObjC::didStopLoadingRequest(AVAssetResourceLoadingRequest *avRequest)
{
    m_resourceLoaderMap.remove(avRequest);
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

void MediaPlayerPrivateAVFoundationObjC::updateVideoLayerGravity()
{
    if (!m_videoLayer)
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];    
    NSString* gravity = shouldMaintainAspectRatio() ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize;
    [m_videoLayer.get() setVideoGravity:gravity];
    [CATransaction commit];
}

void MediaPlayerPrivateAVFoundationObjC::tracksChanged()
{
    String primaryAudioTrackLanguage = m_languageOfPrimaryAudioTrack;
    m_languageOfPrimaryAudioTrack = String();

    if (!m_avAsset)
        return;

    setDelayCharacteristicsChangedNotification(true);

    bool haveCCTrack = false;
    bool hasCaptions = false;

    // This is called whenever the tracks collection changes so cache hasVideo and hasAudio since we are
    // asked about those fairly fequently.
    if (!m_avPlayerItem) {
        // We don't have a player item yet, so check with the asset because some assets support inspection
        // prior to becoming ready to play.
        setHasVideo([[m_avAsset.get() tracksWithMediaCharacteristic:AVMediaCharacteristicVisual] count]);
        setHasAudio([[m_avAsset.get() tracksWithMediaCharacteristic:AVMediaCharacteristicAudible] count]);
#if !HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        hasCaptions = [[m_avAsset.get() tracksWithMediaType:AVMediaTypeClosedCaption] count];
#endif
    } else {
        bool hasVideo = false;
        bool hasAudio = false;
        for (AVPlayerItemTrack *track in m_cachedTracks.get()) {
            if ([track isEnabled]) {
                AVAssetTrack *assetTrack = [track assetTrack];
                if ([[assetTrack mediaType] isEqualToString:AVMediaTypeVideo])
                    hasVideo = true;
                else if ([[assetTrack mediaType] isEqualToString:AVMediaTypeAudio])
                    hasAudio = true;
                else if ([[assetTrack mediaType] isEqualToString:AVMediaTypeClosedCaption]) {
#if !HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
                    hasCaptions = true;
#endif
                    haveCCTrack = true;
                }
            }
        }
        setHasVideo(hasVideo);
        setHasAudio(hasAudio);


#if ENABLE(VIDEO_TRACK)
        updateAudioTracks();
        updateVideoTracks();
#endif
    }

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    if (AVMediaSelectionGroupType *legibleGroup = safeMediaSelectionGroupForLegibleMedia()) {
        hasCaptions = [[AVMediaSelectionGroup playableMediaSelectionOptionsFromArray:[legibleGroup options]] count];
        if (hasCaptions)
            processMediaSelectionOptions();
    }
#endif

#if !HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT) && HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    if (!hasCaptions && haveCCTrack)
        processLegacyClosedCaptionsTracks();
#elif !HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    if (haveCCTrack)
        processLegacyClosedCaptionsTracks();
#endif

    setHasClosedCaptions(hasCaptions);

    LOG(Media, "WebCoreAVFMovieObserver:tracksChanged(%p) - hasVideo = %s, hasAudio = %s, hasCaptions = %s",
        this, boolString(hasVideo()), boolString(hasAudio()), boolString(hasClosedCaptions()));

    sizeChanged();

    if (primaryAudioTrackLanguage != languageOfPrimaryAudioTrack())
        characteristicsChanged();

    setDelayCharacteristicsChangedNotification(false);
}

#if ENABLE(VIDEO_TRACK)
template <typename RefT, typename PassRefT>
void determineChangedTracksFromNewTracksAndOldItems(NSArray* tracks, NSString* trackType, Vector<RefT>& oldItems, RefT (*itemFactory)(AVPlayerItemTrack*), MediaPlayer* player, void (MediaPlayer::*removedFunction)(PassRefT), void (MediaPlayer::*addedFunction)(PassRefT))
{
    RetainPtr<NSSet> newTracks = adoptNS([[NSSet alloc] initWithArray:[tracks objectsAtIndexes:[tracks indexesOfObjectsPassingTest:^(id track, NSUInteger, BOOL*){
        return [[[track assetTrack] mediaType] isEqualToString:trackType];
    }]]]);
    RetainPtr<NSMutableSet> oldTracks = adoptNS([[NSMutableSet alloc] initWithCapacity:oldItems.size()]);

    typedef Vector<RefT> ItemVector;
    for (auto i = oldItems.begin(); i != oldItems.end(); ++i)
        [oldTracks addObject:(*i)->playerItemTrack()];

    RetainPtr<NSMutableSet> removedTracks = adoptNS([oldTracks mutableCopy]);
    [removedTracks minusSet:newTracks.get()];

    RetainPtr<NSMutableSet> addedTracks = adoptNS([newTracks mutableCopy]);
    [addedTracks minusSet:oldTracks.get()];

    ItemVector replacementItems;
    ItemVector addedItems;
    ItemVector removedItems;
    for (auto i = oldItems.begin(); i != oldItems.end(); ++i) {
        if ([removedTracks containsObject:(*i)->playerItemTrack()])
            removedItems.append(*i);
        else
            replacementItems.append(*i);
    }

    for (AVPlayerItemTrack* track in addedTracks.get())
        addedItems.append(itemFactory(track));

    replacementItems.appendVector(addedItems);
    oldItems.swap(replacementItems);

    for (auto i = removedItems.begin(); i != removedItems.end(); ++i)
        (player->*removedFunction)(*i);

    for (auto i = addedItems.begin(); i != addedItems.end(); ++i)
        (player->*addedFunction)(*i);
}

void MediaPlayerPrivateAVFoundationObjC::updateAudioTracks()
{
    determineChangedTracksFromNewTracksAndOldItems(m_cachedTracks.get(), AVMediaTypeAudio, m_audioTracks, &AudioTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeAudioTrack, &MediaPlayer::addAudioTrack);
}

void MediaPlayerPrivateAVFoundationObjC::updateVideoTracks()
{
    determineChangedTracksFromNewTracksAndOldItems(m_cachedTracks.get(), AVMediaTypeVideo, m_videoTracks, &VideoTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeVideoTrack, &MediaPlayer::addVideoTrack);
}
#endif // ENABLE(VIDEO_TRACK)

void MediaPlayerPrivateAVFoundationObjC::sizeChanged()
{
    if (!m_avAsset)
        return;

    // Some assets don't report track properties until they are completely ready to play, but we
    // want to report a size as early as possible so use presentationSize when an asset has no tracks.
    if (m_avPlayerItem && ![m_cachedTracks count]) {
        setNaturalSize(roundedIntSize(m_cachedPresentationSize));
        return;
    }

    // AVAsset's 'naturalSize' property only considers the movie's first video track, so we need to compute
    // the union of all visual track rects.
    CGRect trackUnionRect = CGRectZero;
    for (AVPlayerItemTrack *track in m_cachedTracks.get()) {
        AVAssetTrack* assetTrack = [track assetTrack];
        CGSize trackSize = [assetTrack naturalSize];
        CGRect trackRect = CGRectMake(0, 0, trackSize.width, trackSize.height);
        trackUnionRect = CGRectUnion(trackUnionRect, CGRectApplyAffineTransform(trackRect, [assetTrack preferredTransform]));
    }

    // The movie is always displayed at 0,0 so move the track rect to the origin before using width and height.
    trackUnionRect = CGRectOffset(trackUnionRect, trackUnionRect.origin.x, trackUnionRect.origin.y);
    
    // Also look at the asset's preferred transform so we account for a movie matrix.
    CGSize naturalSize = CGSizeApplyAffineTransform(trackUnionRect.size, [m_avAsset.get() preferredTransform]);

    // Cache the natural size (setNaturalSize will notify the player if it has changed).
    setNaturalSize(IntSize(naturalSize));
}

#if PLATFORM(IOS)
// FIXME: Implement for iOS in WebKit System Interface.
static inline NSURL *wkAVAssetResolvedURL(AVAsset*)
{
    return nil;
}
#endif

bool MediaPlayerPrivateAVFoundationObjC::hasSingleSecurityOrigin() const 
{
    if (!m_avAsset)
        return false;
    
    RefPtr<SecurityOrigin> resolvedOrigin = SecurityOrigin::create(URL(wkAVAssetResolvedURL(m_avAsset.get())));
    RefPtr<SecurityOrigin> requestedOrigin = SecurityOrigin::createFromString(assetURL());
    return resolvedOrigin->isSameSchemeHostPort(requestedOrigin.get());
}

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
void MediaPlayerPrivateAVFoundationObjC::createVideoOutput()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createVideoOutput(%p)", this);

    if (!m_avPlayerItem || m_videoOutput)
        return;

#if USE(VIDEOTOOLBOX)
    NSDictionary* attributes = @{ (NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_422YpCbCr8) };
#else
    NSDictionary* attributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithUnsignedInt:kCVPixelFormatType_32BGRA], kCVPixelBufferPixelFormatTypeKey,
                                nil];
#endif
    m_videoOutput = adoptNS([[getAVPlayerItemVideoOutputClass() alloc] initWithPixelBufferAttributes:attributes]);
    ASSERT(m_videoOutput);

    [m_videoOutput setDelegate:m_videoOutputDelegate.get() queue:globalPullDelegateQueue()];

    [m_avPlayerItem.get() addOutput:m_videoOutput.get()];

    waitForVideoOutputMediaDataWillChange();

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
    double start = monotonicallyIncreasingTime();
#endif

    CMTime currentTime = [m_avPlayerItem.get() currentTime];

    if (![m_videoOutput.get() hasNewPixelBufferForItemTime:currentTime])
        return 0;

    RetainPtr<CVPixelBufferRef> buffer = adoptCF([m_videoOutput.get() copyPixelBufferForItemTime:currentTime itemTimeForDisplay:nil]);
    if (!buffer)
        return 0;

#if USE(VIDEOTOOLBOX)
    // Create a VTPixelTransferSession, if necessary, as we cannot guarantee timely delivery of ARGB pixels.
    if (!m_pixelTransferSession) {
        VTPixelTransferSessionRef session = 0;
        VTPixelTransferSessionCreate(kCFAllocatorDefault, &session);
        m_pixelTransferSession = adoptCF(session);
    }

    CVPixelBufferRef outputBuffer;
    CVPixelBufferCreate(kCFAllocatorDefault, CVPixelBufferGetWidth(buffer.get()), CVPixelBufferGetHeight(buffer.get()), kCVPixelFormatType_32BGRA, 0, &outputBuffer);
    VTPixelTransferSessionTransferImage(m_pixelTransferSession.get(), buffer.get(), outputBuffer);
    buffer = adoptCF(outputBuffer);
#endif

#if !LOG_DISABLED
    double duration = monotonicallyIncreasingTime() - start;
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::createPixelBuffer(%p) - creating buffer took %.4f", this, narrowPrecisionToFloat(duration));
#endif

    return buffer;
}

bool MediaPlayerPrivateAVFoundationObjC::videoOutputHasAvailableFrame()
{
    if (!m_avPlayerItem)
        return false;

    if (m_lastImage)
        return true;

    if (!m_videoOutput)
        createVideoOutput();

    return [m_videoOutput hasNewPixelBufferForItemTime:[m_avPlayerItem currentTime]];
}

static const void* CVPixelBufferGetBytePointerCallback(void* info)
{
    CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(info);
    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    return CVPixelBufferGetBaseAddress(pixelBuffer);
}

static void CVPixelBufferReleaseBytePointerCallback(void* info, const void*)
{
    CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(info);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
}

static void CVPixelBufferReleaseInfoCallback(void* info)
{
    CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(info);
    CFRelease(pixelBuffer);
}

static RetainPtr<CGImageRef> createImageFromPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    // pixelBuffer will be of type kCVPixelFormatType_32BGRA.
    ASSERT(CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_32BGRA);

    size_t width = CVPixelBufferGetWidth(pixelBuffer);
    size_t height = CVPixelBufferGetHeight(pixelBuffer);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
    size_t byteLength = CVPixelBufferGetDataSize(pixelBuffer);
    CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaFirst;

    CFRetain(pixelBuffer); // Balanced by CVPixelBufferReleaseInfoCallback in providerCallbacks.
    CGDataProviderDirectCallbacks providerCallbacks = { 0, CVPixelBufferGetBytePointerCallback, CVPixelBufferReleaseBytePointerCallback, 0, CVPixelBufferReleaseInfoCallback };
    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateDirect(pixelBuffer, byteLength, &providerCallbacks));

    return adoptCF(CGImageCreate(width, height, 8, 32, bytesPerRow, deviceRGBColorSpaceRef(), bitmapInfo, provider.get(), NULL, false, kCGRenderingIntentDefault));
}

void MediaPlayerPrivateAVFoundationObjC::updateLastImage()
{
    RetainPtr<CVPixelBufferRef> pixelBuffer = createPixelBuffer();

    // Calls to copyPixelBufferForItemTime:itemTimeForDisplay: may return nil if the pixel buffer
    // for the requested time has already been retrieved. In this case, the last valid image (if any)
    // should be displayed.
    if (pixelBuffer)
        m_lastImage = createImageFromPixelBuffer(pixelBuffer.get());
}

void MediaPlayerPrivateAVFoundationObjC::paintWithVideoOutput(GraphicsContext* context, const IntRect& outputRect)
{
    updateLastImage();

    if (m_lastImage) {
        GraphicsContextStateSaver stateSaver(*context);

        IntRect imageRect(0, 0, CGImageGetWidth(m_lastImage.get()), CGImageGetHeight(m_lastImage.get()));

        context->drawNativeImage(m_lastImage.get(), imageRect.size(), ColorSpaceDeviceRGB, outputRect, imageRect);

        // If we have created an AVAssetImageGenerator in the past due to m_videoOutput not having an available
        // video frame, destroy it now that it is no longer needed.
        if (m_imageGenerator)
            destroyImageGenerator();
    }
}

PassNativeImagePtr MediaPlayerPrivateAVFoundationObjC::nativeImageForCurrentTime()
{
    updateLastImage();
    return m_lastImage.get();
}

void MediaPlayerPrivateAVFoundationObjC::waitForVideoOutputMediaDataWillChange()
{
    if (!m_videoOutputSemaphore)
        m_videoOutputSemaphore = dispatch_semaphore_create(0);

    [m_videoOutput requestNotificationOfMediaDataChangeWithAdvanceInterval:0];

    // Wait for 1 second.
    long result = dispatch_semaphore_wait(m_videoOutputSemaphore, dispatch_time(0, 1 * NSEC_PER_SEC));

    if (result)
        LOG(Media, "MediaPlayerPrivateAVFoundationObjC::waitForVideoOutputMediaDataWillChange(%p) timed out", this);
}

void MediaPlayerPrivateAVFoundationObjC::outputMediaDataWillChange(AVPlayerItemVideoOutput*)
{
    dispatch_semaphore_signal(m_videoOutputSemaphore);
}
#endif

#if ENABLE(ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA_V2)
bool MediaPlayerPrivateAVFoundationObjC::extractKeyURIKeyIDAndCertificateFromInitData(Uint8Array* initData, String& keyURI, String& keyID, RefPtr<Uint8Array>& certificate)
{
    // initData should have the following layout:
    // [4 bytes: keyURI length][N bytes: keyURI][4 bytes: contentID length], [N bytes: contentID], [4 bytes: certificate length][N bytes: certificate]
    if (initData->byteLength() < 4)
        return false;

    RefPtr<ArrayBuffer> initDataBuffer = initData->buffer();

    // Use a DataView to read uint32 values from the buffer, as Uint32Array requires the reads be aligned on 4-byte boundaries. 
    RefPtr<JSC::DataView> initDataView = JSC::DataView::create(initDataBuffer, 0, initDataBuffer->byteLength());
    uint32_t offset = 0;
    bool status = true;

    uint32_t keyURILength = initDataView->get<uint32_t>(offset, true, &status);
    offset += 4;
    if (!status || offset + keyURILength > initData->length())
        return false;

    RefPtr<Uint16Array> keyURIArray = Uint16Array::create(initDataBuffer, offset, keyURILength);
    if (!keyURIArray)
        return false;

    keyURI = String(keyURIArray->data(), keyURILength / sizeof(unsigned short));
    offset += keyURILength;

    uint32_t keyIDLength = initDataView->get<uint32_t>(offset, true, &status);
    offset += 4;
    if (!status || offset + keyIDLength > initData->length())
        return false;

    RefPtr<Uint16Array> keyIDArray = Uint16Array::create(initDataBuffer, offset, keyIDLength);
    if (!keyIDArray)
        return false;

    keyID = String(keyIDArray->data(), keyIDLength / sizeof(unsigned short));
    offset += keyIDLength;

    uint32_t certificateLength = initDataView->get<uint32_t>(offset, true, &status);
    offset += 4;
    if (!status || offset + certificateLength > initData->length())
        return false;

    certificate = Uint8Array::create(initDataBuffer, offset, certificateLength);
    if (!certificate)
        return false;

    return true;
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
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
    player()->keyMessage(keySystem, sessionID, keyRequestArray->data(), keyRequestArray->byteLength(), URL());

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

#if ENABLE(ENCRYPTED_MEDIA_V2)
RetainPtr<AVAssetResourceLoadingRequest> MediaPlayerPrivateAVFoundationObjC::takeRequestForKeyURI(const String& keyURI)
{
    return m_keyURIToRequestMap.take(keyURI);
}

std::unique_ptr<CDMSession> MediaPlayerPrivateAVFoundationObjC::createSession(const String& keySystem)
{
    if (!keySystemIsSupported(keySystem))
        return nullptr;

    return std::make_unique<CDMSessionAVFoundationObjC>(this);
}
#endif

#if !HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
void MediaPlayerPrivateAVFoundationObjC::processLegacyClosedCaptionsTracks()
{
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    [m_avPlayerItem.get() selectMediaOption:nil inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
#endif

    Vector<RefPtr<InbandTextTrackPrivateAVF>> removedTextTracks = m_textTracks;
    for (AVPlayerItemTrack *playerItemTrack in m_cachedTracks.get()) {

        AVAssetTrack *assetTrack = [playerItemTrack assetTrack];
        if (![[assetTrack mediaType] isEqualToString:AVMediaTypeClosedCaption])
            continue;

        bool newCCTrack = true;
        for (unsigned i = removedTextTracks.size(); i > 0; --i) {
            if (!removedTextTracks[i - 1]->isLegacyClosedCaptionsTrack())
                continue;

            RefPtr<InbandTextTrackPrivateLegacyAVFObjC> track = static_cast<InbandTextTrackPrivateLegacyAVFObjC*>(m_textTracks[i - 1].get());
            if (track->avPlayerItemTrack() == playerItemTrack) {
                removedTextTracks.remove(i - 1);
                newCCTrack = false;
                break;
            }
        }

        if (!newCCTrack)
            continue;
        
        m_textTracks.append(InbandTextTrackPrivateLegacyAVFObjC::create(this, playerItemTrack));
    }

    processNewAndRemovedTextTracks(removedTextTracks);
}
#endif

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
AVMediaSelectionGroupType* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForLegibleMedia()
{
    if (!m_avAsset)
        return nil;
    
    if ([m_avAsset.get() statusOfValueForKey:@"availableMediaCharacteristicsWithMediaSelectionOptions" error:NULL] != AVKeyValueStatusLoaded)
        return nil;
    
    return [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicLegible];
}

void MediaPlayerPrivateAVFoundationObjC::processMediaSelectionOptions()
{
    AVMediaSelectionGroupType *legibleGroup = safeMediaSelectionGroupForLegibleMedia();
    if (!legibleGroup) {
        LOG(Media, "MediaPlayerPrivateAVFoundationObjC::processMediaSelectionOptions(%p) - nil mediaSelectionGroup", this);
        return;
    }

    // We enabled automatic media selection because we want alternate audio tracks to be enabled/disabled automatically,
    // but set the selected legible track to nil so text tracks will not be automatically configured.
    if (!m_textTracks.size())
        [m_avPlayerItem.get() selectMediaOption:nil inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];

    Vector<RefPtr<InbandTextTrackPrivateAVF>> removedTextTracks = m_textTracks;
    NSArray *legibleOptions = [AVMediaSelectionGroup playableMediaSelectionOptionsFromArray:[legibleGroup options]];
    for (AVMediaSelectionOptionType *option in legibleOptions) {
        bool newTrack = true;
        for (unsigned i = removedTextTracks.size(); i > 0; --i) {
             if (removedTextTracks[i - 1]->isLegacyClosedCaptionsTrack())
                 continue;

            RefPtr<InbandTextTrackPrivateAVFObjC> track = static_cast<InbandTextTrackPrivateAVFObjC*>(removedTextTracks[i - 1].get());
            if ([track->mediaSelectionOption() isEqual:option]) {
                removedTextTracks.remove(i - 1);
                newTrack = false;
                break;
            }
        }
        if (!newTrack)
            continue;

#if ENABLE(AVF_CAPTIONS)
        // Ignore out-of-band tracks that we passed to AVFoundation so we do not double-count them
        if ([option outOfBandSource])
            continue;
#endif

        m_textTracks.append(InbandTextTrackPrivateAVFObjC::create(this, option));
    }

    processNewAndRemovedTextTracks(removedTextTracks);
}

void MediaPlayerPrivateAVFoundationObjC::processCue(NSArray *attributedStrings, double time)
{
    if (!m_currentTrack)
        return;

    m_currentTrack->processCue(reinterpret_cast<CFArrayRef>(attributedStrings), time);
}

void MediaPlayerPrivateAVFoundationObjC::flushCues()
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::flushCues(%p)", this);

    if (!m_currentTrack)
        return;
    
    m_currentTrack->resetCueValues();
}
#endif // HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

void MediaPlayerPrivateAVFoundationObjC::setCurrentTrack(InbandTextTrackPrivateAVF *track)
{
    if (m_currentTrack == track)
        return;

    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::setCurrentTrack(%p) - selecting track %p, language = %s", this, track, track ? track->language().string().utf8().data() : "");
        
    m_currentTrack = track;

    if (track) {
        if (track->isLegacyClosedCaptionsTrack())
            [m_avPlayer.get() setClosedCaptionDisplayEnabled:YES];
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        else
            [m_avPlayerItem.get() selectMediaOption:static_cast<InbandTextTrackPrivateAVFObjC*>(track)->mediaSelectionOption() inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
#endif
    } else {
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        [m_avPlayerItem.get() selectMediaOption:0 inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
#endif
        [m_avPlayer.get() setClosedCaptionDisplayEnabled:NO];
    }

}

String MediaPlayerPrivateAVFoundationObjC::languageOfPrimaryAudioTrack() const
{
    if (!m_languageOfPrimaryAudioTrack.isNull())
        return m_languageOfPrimaryAudioTrack;

    if (!m_avPlayerItem.get())
        return emptyString();

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    // If AVFoundation has an audible group, return the language of the currently selected audible option.
    AVMediaSelectionGroupType *audibleGroup = [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicAudible];
    AVMediaSelectionOptionType *currentlySelectedAudibleOption = [m_avPlayerItem.get() selectedMediaOptionInMediaSelectionGroup:audibleGroup];
    if (currentlySelectedAudibleOption) {
        m_languageOfPrimaryAudioTrack = [[currentlySelectedAudibleOption locale] localeIdentifier];
        LOG(Media, "MediaPlayerPrivateAVFoundationObjC::languageOfPrimaryAudioTrack(%p) - returning language of selected audible option: %s", this, m_languageOfPrimaryAudioTrack.utf8().data());

        return m_languageOfPrimaryAudioTrack;
    }
#endif // HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

    // AVFoundation synthesizes an audible group when there is only one ungrouped audio track if there is also a legible group (one or
    // more in-band text tracks). It doesn't know about out-of-band tracks, so if there is a single audio track return its language.
    NSArray *tracks = [m_avAsset.get() tracksWithMediaType:AVMediaTypeAudio];
    if (!tracks || [tracks count] != 1) {
        m_languageOfPrimaryAudioTrack = emptyString();
        LOG(Media, "MediaPlayerPrivateAVFoundationObjC::languageOfPrimaryAudioTrack(%p) - %lu audio tracks, returning emptyString()", this, static_cast<unsigned long>(tracks ? [tracks count] : 0));
        return m_languageOfPrimaryAudioTrack;
    }

    AVAssetTrack *track = [tracks objectAtIndex:0];
    m_languageOfPrimaryAudioTrack = AVTrackPrivateAVFObjCImpl::languageForAVAssetTrack(track);

#if !LOG_DISABLED
    if (m_languageOfPrimaryAudioTrack == emptyString())
        LOG(Media, "MediaPlayerPrivateAVFoundationObjC::languageOfPrimaryAudioTrack(%p) - single audio track has no language, returning emptyString()", this);
    else
        LOG(Media, "MediaPlayerPrivateAVFoundationObjC::languageOfPrimaryAudioTrack(%p) - returning language of single audio track: %s", this, m_languageOfPrimaryAudioTrack.utf8().data());
#endif

    return m_languageOfPrimaryAudioTrack;
}

#if ENABLE(IOS_AIRPLAY)
bool MediaPlayerPrivateAVFoundationObjC::isCurrentPlaybackTargetWireless() const
{
    if (!m_avPlayer)
        return false;

    bool wirelessTarget = [m_avPlayer.get() isExternalPlaybackActive];
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::isCurrentPlaybackTargetWireless(%p) - returning %s", this, boolString(wirelessTarget));
    return wirelessTarget;
}

MediaPlayer::WirelessPlaybackTargetType MediaPlayerPrivateAVFoundationObjC::wirelessPlaybackTargetType() const
{
    if (!m_avPlayer)
        return MediaPlayer::TargetTypeNone;

    switch (wkExernalDeviceTypeForPlayer(m_avPlayer.get())) {
    case wkExternalPlaybackTypeNone:
        return MediaPlayer::TargetTypeNone;
    case wkExternalPlaybackTypeAirPlay:
        return MediaPlayer::TargetTypeAirPlay;
    case wkExternalPlaybackTypeTVOut:
        return MediaPlayer::TargetTypeTVOut;
    }

    ASSERT_NOT_REACHED();
    return MediaPlayer::TargetTypeNone;
}

String MediaPlayerPrivateAVFoundationObjC::wirelessPlaybackTargetName() const
{
    if (!m_avPlayer)
        return emptyString();
    
    String wirelessTargetName = wkExernalDeviceDisplayNameForPlayer(m_avPlayer.get());
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::wirelessPlaybackTargetName(%p) - returning %s", this, wirelessTargetName.utf8().data());

    return wirelessTargetName;
}

bool MediaPlayerPrivateAVFoundationObjC::wirelessVideoPlaybackDisabled() const
{
    if (!m_avPlayer)
        return !m_allowsWirelessVideoPlayback;
    
    m_allowsWirelessVideoPlayback = ![m_avPlayer.get() allowsExternalPlayback];
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::wirelessVideoPlaybackDisabled(%p) - returning %s", this, boolString(!m_allowsWirelessVideoPlayback));

    return !m_allowsWirelessVideoPlayback;
}

void MediaPlayerPrivateAVFoundationObjC::setWirelessVideoPlaybackDisabled(bool disabled)
{
    LOG(Media, "MediaPlayerPrivateAVFoundationObjC::setWirelessVideoPlaybackDisabled(%p) - %s", this, boolString(disabled));
    m_allowsWirelessVideoPlayback = !disabled;
    if (!m_avPlayer)
        return;
    
    [m_avPlayer.get() setAllowsExternalPlayback:disabled];
}
#endif

void MediaPlayerPrivateAVFoundationObjC::playerItemStatusDidChange(int status)
{
    m_cachedItemStatus = status;

    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::playbackLikelyToKeepUpWillChange()
{
    m_pendingStatusChanges++;
}

void MediaPlayerPrivateAVFoundationObjC::playbackLikelyToKeepUpDidChange(bool likelyToKeepUp)
{
    m_cachedLikelyToKeepUp = likelyToKeepUp;

    ASSERT(m_pendingStatusChanges);
    if (!--m_pendingStatusChanges)
        updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::playbackBufferEmptyWillChange()
{
    m_pendingStatusChanges++;
}

void MediaPlayerPrivateAVFoundationObjC::playbackBufferEmptyDidChange(bool bufferEmpty)
{
    m_cachedBufferEmpty = bufferEmpty;

    ASSERT(m_pendingStatusChanges);
    if (!--m_pendingStatusChanges)
        updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::playbackBufferFullWillChange()
{
    m_pendingStatusChanges++;
}

void MediaPlayerPrivateAVFoundationObjC::playbackBufferFullDidChange(bool bufferFull)
{
    m_cachedBufferFull = bufferFull;

    ASSERT(m_pendingStatusChanges);
    if (!--m_pendingStatusChanges)
        updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::seekableTimeRangesDidChange(RetainPtr<NSArray> seekableRanges)
{
    m_cachedSeekableRanges = seekableRanges;

    seekableTimeRangesChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::loadedTimeRangesDidChange(RetainPtr<NSArray> loadedRanges)
{
    m_cachedLoadedRanges = loadedRanges;

    loadedTimeRangesChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::tracksDidChange(RetainPtr<NSArray> tracks)
{
    m_cachedTracks = tracks;

    tracksChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::hasEnabledAudioDidChange(bool hasEnabledAudio)
{
    m_cachedHasEnabledAudio = hasEnabledAudio;

    tracksChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::presentationSizeDidChange(FloatSize size)
{
    m_cachedPresentationSize = size;

    sizeChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::durationDidChange(double duration)
{
    m_cachedDuration = duration;

    invalidateCachedDuration();
}

void MediaPlayerPrivateAVFoundationObjC::rateDidChange(double rate)
{
    m_cachedRate = rate;

    updateStates();
    rateChanged();
}
    
#if ENABLE(IOS_AIRPLAY)
void MediaPlayerPrivateAVFoundationObjC::playbackTargetIsWirelessDidChange()
{
    playbackTargetIsWirelessChanged();
}
#endif

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
                    @"availableMediaCharacteristicsWithMediaSelectionOptions",
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
                @"hasEnabledAudio",
                nil];
    }
    return keys;
}

} // namespace WebCore

@implementation WebCoreAVFMovieObserver

- (id)initWithCallback:(MediaPlayerPrivateAVFoundationObjC*)callback
{
    self = [super init];
    if (!self)
        return nil;
    m_callback = callback;
    return self;
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

- (void)didEnd:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (!m_callback)
        return;
    m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::ItemDidPlayToEndTime);
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(MediaPlayerAVFoundationObservationContext)context
{
    UNUSED_PARAM(object);
    id newValue = [change valueForKey:NSKeyValueChangeNewKey];

    LOG(Media, "WebCoreAVFMovieObserver::observeValueForKeyPath(%p) - keyPath = %s", self, [keyPath UTF8String]);

    if (!m_callback)
        return;

    bool willChange = [[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue];

    WTF::Function<void ()> function;

    if (context == MediaPlayerAVFoundationObservationContextPlayerItem && willChange) {
        if ([keyPath isEqualToString:@"playbackLikelyToKeepUp"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::playbackLikelyToKeepUpWillChange, m_callback);
        else if ([keyPath isEqualToString:@"playbackBufferEmpty"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::playbackBufferEmptyWillChange, m_callback);
        else if ([keyPath isEqualToString:@"playbackBufferFull"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::playbackBufferFullWillChange, m_callback);
    }

    if (context == MediaPlayerAVFoundationObservationContextPlayerItem && !willChange) {
        // A value changed for an AVPlayerItem
        if ([keyPath isEqualToString:@"status"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::playerItemStatusDidChange, m_callback, [newValue intValue]);
        else if ([keyPath isEqualToString:@"playbackLikelyToKeepUp"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::playbackLikelyToKeepUpDidChange, m_callback, [newValue boolValue]);
        else if ([keyPath isEqualToString:@"playbackBufferEmpty"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::playbackBufferEmptyDidChange, m_callback, [newValue boolValue]);
        else if ([keyPath isEqualToString:@"playbackBufferFull"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::playbackBufferFullDidChange, m_callback, [newValue boolValue]);
        else if ([keyPath isEqualToString:@"asset"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::setAsset, m_callback, RetainPtr<NSArray>(newValue));
        else if ([keyPath isEqualToString:@"loadedTimeRanges"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::loadedTimeRangesDidChange, m_callback, RetainPtr<NSArray>(newValue));
        else if ([keyPath isEqualToString:@"seekableTimeRanges"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::seekableTimeRangesDidChange, m_callback, RetainPtr<NSArray>(newValue));
        else if ([keyPath isEqualToString:@"tracks"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::tracksDidChange, m_callback, RetainPtr<NSArray>(newValue));
        else if ([keyPath isEqualToString:@"hasEnabledAudio"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::hasEnabledAudioDidChange, m_callback, [newValue boolValue]);
        else if ([keyPath isEqualToString:@"presentationSize"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::presentationSizeDidChange, m_callback, FloatSize([newValue sizeValue]));
        else if ([keyPath isEqualToString:@"duration"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::durationDidChange, m_callback, CMTimeGetSeconds([newValue CMTimeValue]));
    }

    if (context == MediaPlayerAVFoundationObservationContextPlayer && !willChange) {
        // A value changed for an AVPlayer.
        if ([keyPath isEqualToString:@"rate"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::rateDidChange, m_callback, [newValue doubleValue]);
#if ENABLE(IOS_AIRPLAY)
        else if ([keyPath isEqualToString:@"externalPlaybackActive"])
            function = WTF::bind(&MediaPlayerPrivateAVFoundationObjC::playbackTargetIsWirelessDidChange, m_callback);
#endif
    }
    
    if (function.isNull())
        return;

    auto weakThis = m_callback->createWeakPtr();
    m_callback->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification([weakThis, function]{
        // weakThis and function both refer to the same MediaPlayerPrivateAVFoundationObjC instance. If the WeakPtr has
        // been cleared, the underlying object has been destroyed, and it is unsafe to call function().
        if (!weakThis)
            return;
        function();
    }));
}

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
- (void)legibleOutput:(id)output didOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime
{
    UNUSED_PARAM(output);
    UNUSED_PARAM(nativeSamples);

    if (!m_callback)
        return;

    RetainPtr<WebCoreAVFMovieObserver> strongSelf = self;
    RetainPtr<NSArray> strongStrings = strings;
    callOnMainThread([strongSelf, strongStrings, itemTime] {
        MediaPlayerPrivateAVFoundationObjC* callback = strongSelf->m_callback;
        if (!callback)
            return;
        callback->processCue(strongStrings.get(), CMTimeGetSeconds(itemTime));
    });
}

- (void)outputSequenceWasFlushed:(id)output
{
    UNUSED_PARAM(output);

    if (!m_callback)
        return;
    
    RetainPtr<WebCoreAVFMovieObserver> strongSelf = self;
    callOnMainThread([strongSelf] {
        if (MediaPlayerPrivateAVFoundationObjC* callback = strongSelf->m_callback)
            callback->flushCues();
    });
}
#endif

@end

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
@implementation WebCoreAVFLoaderDelegate

- (id)initWithCallback:(MediaPlayerPrivateAVFoundationObjC*)callback
{
    self = [super init];
    if (!self)
        return nil;
    m_callback = callback;
    return self;
}

- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest
{
    UNUSED_PARAM(resourceLoader);
    if (!m_callback)
        return NO;

    RetainPtr<WebCoreAVFLoaderDelegate> strongSelf = self;
    RetainPtr<AVAssetResourceLoadingRequest> strongRequest = loadingRequest;
    callOnMainThread([strongSelf, strongRequest] {
        MediaPlayerPrivateAVFoundationObjC* callback = strongSelf->m_callback;
        if (!callback) {
            [strongRequest finishLoadingWithError:nil];
            return;
        }

        if (!callback->shouldWaitForLoadingOfResource(strongRequest.get()))
            [strongRequest finishLoadingWithError:nil];
    });

    return YES;
}

- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForResponseToAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    UNUSED_PARAM(resourceLoader);
    if (!m_callback)
        return NO;

    if ([[[challenge protectionSpace] authenticationMethod] isEqualToString:NSURLAuthenticationMethodServerTrust])
        return NO;

    RetainPtr<WebCoreAVFLoaderDelegate> strongSelf = self;
    RetainPtr<NSURLAuthenticationChallenge> strongChallenge = challenge;
    callOnMainThread([strongSelf, strongChallenge] {
        MediaPlayerPrivateAVFoundationObjC* callback = strongSelf->m_callback;
        if (!callback) {
            [[strongChallenge sender] cancelAuthenticationChallenge:strongChallenge.get()];
            return;
        }

        if (!callback->shouldWaitForResponseToAuthenticationChallenge(strongChallenge.get()))
            [[strongChallenge sender] cancelAuthenticationChallenge:strongChallenge.get()];
    });

    return YES;
}

- (void)resourceLoader:(AVAssetResourceLoader *)resourceLoader didCancelLoadingRequest:(AVAssetResourceLoadingRequest *)loadingRequest
{
    UNUSED_PARAM(resourceLoader);
    if (!m_callback)
        return;

    RetainPtr<WebCoreAVFLoaderDelegate> strongSelf = self;
    RetainPtr<AVAssetResourceLoadingRequest> strongRequest = loadingRequest;
    callOnMainThread([strongSelf, strongRequest] {
        MediaPlayerPrivateAVFoundationObjC* callback = strongSelf->m_callback;
        if (callback)
            callback->didCancelLoadingRequest(strongRequest.get());
    });
}

- (void)setCallback:(MediaPlayerPrivateAVFoundationObjC*)callback
{
    m_callback = callback;
}
@end
#endif

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
@implementation WebCoreAVFPullDelegate
- (id)initWithCallback:(MediaPlayerPrivateAVFoundationObjC *)callback
{
    self = [super init];
    if (self)
        m_callback = callback;
    return self;
}

- (void)setCallback:(MediaPlayerPrivateAVFoundationObjC *)callback
{
    m_callback = callback;
}

- (void)outputMediaDataWillChange:(AVPlayerItemVideoOutput *)output
{
    if (m_callback)
        m_callback->outputMediaDataWillChange(output);
}

- (void)outputSequenceWasFlushed:(AVPlayerItemVideoOutput *)output
{
    UNUSED_PARAM(output);
    // No-op.
}
@end
#endif

#endif
