/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
#import "MediaPlayerPrivateAVFoundationObjC.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#import "AVAssetTrackUtilities.h"
#import "AVFoundationMIMETypeCache.h"
#import "AVTrackPrivateAVFObjCImpl.h"
#import "AudioSourceProviderAVFObjC.h"
#import "AudioTrackPrivateAVFObjC.h"
#import "AuthenticationChallenge.h"
#import "CDMInstanceFairPlayStreamingAVFObjC.h"
#import "CDMSessionAVFoundationObjC.h"
#import "Cookie.h"
#import "DeprecatedGlobalSettings.h"
#import "Extensions3D.h"
#import "FloatConversion.h"
#import "GraphicsContext.h"
#import "GraphicsContext3D.h"
#import "GraphicsContextCG.h"
#import "InbandMetadataTextTrackPrivateAVF.h"
#import "InbandTextTrackPrivateAVFObjC.h"
#import "InbandTextTrackPrivateLegacyAVFObjC.h"
#import "Logging.h"
#import "MediaPlaybackTargetMac.h"
#import "MediaPlaybackTargetMock.h"
#import "MediaSelectionGroupAVFObjC.h"
#import "OutOfBandTextTrackPrivateAVF.h"
#import "PixelBufferConformerCV.h"
#import "PlatformTimeRanges.h"
#import "SecurityOrigin.h"
#import "SerializedPlatformRepresentationMac.h"
#import "SharedBuffer.h"
#import "TextEncoding.h"
#import "TextTrackRepresentation.h"
#import "TextureCacheCV.h"
#import "VideoFullscreenLayerManagerObjC.h"
#import "VideoTextureCopierCV.h"
#import "VideoTrackPrivateAVFObjC.h"
#import "WebCoreAVFResourceLoader.h"
#import "WebCoreCALayerExtras.h"
#import "WebCoreNSURLSession.h"
#import <JavaScriptCore/DataView.h>
#import <JavaScriptCore/JSCInlines.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <JavaScriptCore/Uint16Array.h>
#import <JavaScriptCore/Uint32Array.h>
#import <JavaScriptCore/Uint8Array.h>
#import <functional>
#import <objc/runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/mac/AVFoundationSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/ListHashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/URL.h>
#import <wtf/text/CString.h>

#if ENABLE(AVF_CAPTIONS)
#include "TextTrack.h"
#endif

#import <AVFoundation/AVAssetImageGenerator.h>
#import <AVFoundation/AVAssetTrack.h>
#import <AVFoundation/AVMediaSelectionGroup.h>
#import <AVFoundation/AVMetadataItem.h>
#import <AVFoundation/AVPlayer.h>
#import <AVFoundation/AVPlayerItem.h>
#import <AVFoundation/AVPlayerItemOutput.h>
#import <AVFoundation/AVPlayerItemTrack.h>
#import <AVFoundation/AVPlayerLayer.h>
#import <AVFoundation/AVTime.h>

#if PLATFORM(IOS_FAMILY)
#import "WAKAppKitStubs.h"
#import <CoreImage/CoreImage.h>
#import <UIKit/UIDevice.h>
#import <mach/mach_port.h>
#else
#import <Foundation/NSGeometry.h>
#import <QuartzCore/CoreImage.h>
#endif

#if USE(VIDEOTOOLBOX)
#import <CoreVideo/CoreVideo.h>
#import <VideoToolbox/VideoToolbox.h>
#endif

#import "CoreVideoSoftLink.h"
#import "MediaRemoteSoftLink.h"

namespace std {
template <> struct iterator_traits<HashSet<RefPtr<WebCore::MediaSelectionOptionAVFObjC>>::iterator> {
    typedef RefPtr<WebCore::MediaSelectionOptionAVFObjC> value_type;
};
}

#if ENABLE(AVF_CAPTIONS)
// Note: This must be defined before our SOFT_LINK macros:
@class AVMediaSelectionOption;
@interface AVMediaSelectionOption (OutOfBandExtensions)
@property (nonatomic, readonly) NSString* outOfBandSource;
@property (nonatomic, readonly) NSString* outOfBandIdentifier;
@end
#endif

@interface AVURLAsset (WebKitExtensions)
@property (nonatomic, readonly) NSURL *resolvedURL;
@end

typedef AVPlayer AVPlayerType;
typedef AVPlayerItem AVPlayerItemType;
typedef AVPlayerItemLegibleOutput AVPlayerItemLegibleOutputType;
typedef AVPlayerItemVideoOutput AVPlayerItemVideoOutputType;
typedef AVMetadataItem AVMetadataItemType;
typedef AVMediaSelectionGroup AVMediaSelectionGroupType;
typedef AVMediaSelectionOption AVMediaSelectionOptionType;
typedef AVAssetCache AVAssetCacheType;

#pragma mark - Soft Linking

// Soft-linking headers must be included last since they #define functions, constants, etc.
#import <pal/cf/CoreMediaSoftLink.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_FRAMEWORK_OPTIONAL(CoreImage)

SOFT_LINK_CLASS_FOR_SOURCE(WebCore, AVFoundation, AVPlayer)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, AVFoundation, AVPlayerItem)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, AVFoundation, AVPlayerItemVideoOutput)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, AVFoundation, AVPlayerLayer)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, AVFoundation, AVURLAsset)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, AVFoundation, AVAssetImageGenerator)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, AVFoundation, AVMetadataItem)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, AVFoundation, AVAssetCache)

SOFT_LINK_CLASS(CoreImage, CIContext)
SOFT_LINK_CLASS(CoreImage, CIImage)

SOFT_LINK_CONSTANT(AVFoundation, AVAudioTimePitchAlgorithmSpectral, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioTimePitchAlgorithmVarispeed, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicVisual, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicAudible, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeClosedCaption, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeMetadata, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVPlayerItemDidPlayToEndTimeNotification, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVURLAssetInheritURIQueryComponentFromReferencingURIKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAssetImageGeneratorApertureModeCleanAperture, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVURLAssetReferenceRestrictionsKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVLayerVideoGravityResizeAspect, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVLayerVideoGravityResizeAspectFill, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVLayerVideoGravityResize, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVStreamingKeyDeliveryContentKeyType, NSString *)

SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVURLAssetOutOfBandMIMETypeKey, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVURLAssetUseClientURLLoadingExclusively, NSString *)

#define AVPlayer initAVPlayer()
#define AVPlayerItem initAVPlayerItem()
#define AVPlayerLayer initAVPlayerLayer()
#define AVURLAsset initAVURLAsset()
#define AVAssetImageGenerator initAVAssetImageGenerator()
#define AVPlayerItemVideoOutput initAVPlayerItemVideoOutput()
#define AVMetadataItem initAVMetadataItem()
#define AVAssetCache initAVAssetCache()

#define AVAudioTimePitchAlgorithmSpectral getAVAudioTimePitchAlgorithmSpectral()
#define AVAudioTimePitchAlgorithmVarispeed getAVAudioTimePitchAlgorithmVarispeed()
#define AVMediaCharacteristicVisual getAVMediaCharacteristicVisual()
#define AVMediaCharacteristicAudible getAVMediaCharacteristicAudible()
#define AVMediaTypeClosedCaption getAVMediaTypeClosedCaption()
#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVMediaTypeAudio getAVMediaTypeAudio()
#define AVMediaTypeMetadata getAVMediaTypeMetadata()
#define AVPlayerItemDidPlayToEndTimeNotification getAVPlayerItemDidPlayToEndTimeNotification()
#define AVURLAssetInheritURIQueryComponentFromReferencingURIKey getAVURLAssetInheritURIQueryComponentFromReferencingURIKey()
#define AVURLAssetOutOfBandMIMETypeKey getAVURLAssetOutOfBandMIMETypeKey()
#define AVURLAssetUseClientURLLoadingExclusively getAVURLAssetUseClientURLLoadingExclusively()
#define AVAssetImageGeneratorApertureModeCleanAperture getAVAssetImageGeneratorApertureModeCleanAperture()
#define AVURLAssetReferenceRestrictionsKey getAVURLAssetReferenceRestrictionsKey()
#define AVLayerVideoGravityResizeAspect getAVLayerVideoGravityResizeAspect()
#define AVLayerVideoGravityResizeAspectFill getAVLayerVideoGravityResizeAspectFill()
#define AVLayerVideoGravityResize getAVLayerVideoGravityResize()
#define AVStreamingKeyDeliveryContentKeyType getAVStreamingKeyDeliveryContentKeyType()

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

typedef AVMediaSelectionGroup AVMediaSelectionGroupType;
typedef AVMediaSelectionOption AVMediaSelectionOptionType;

SOFT_LINK_CLASS(AVFoundation, AVPlayerItemLegibleOutput)
SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionGroup)
SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionOption)
SOFT_LINK_CLASS(AVFoundation, AVOutputContext)

SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicLegible, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeSubtitle, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicContainsOnlyForcedSubtitles, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly, NSString *)

#define AVPlayerItemLegibleOutput getAVPlayerItemLegibleOutputClass()
#define AVMediaSelectionGroup getAVMediaSelectionGroupClass()
#define AVMediaSelectionOption getAVMediaSelectionOptionClass()
#define AVMediaCharacteristicLegible getAVMediaCharacteristicLegible()
#define AVMediaTypeSubtitle getAVMediaTypeSubtitle()
#define AVMediaCharacteristicContainsOnlyForcedSubtitles getAVMediaCharacteristicContainsOnlyForcedSubtitles()
#define AVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly getAVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly()

#endif

#if ENABLE(AVF_CAPTIONS)

SOFT_LINK_CONSTANT(AVFoundation, AVURLAssetCacheKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVURLAssetOutOfBandAlternateTracksKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVURLAssetUsesNoPersistentCacheKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVOutOfBandAlternateTrackDisplayNameKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVOutOfBandAlternateTrackExtendedLanguageTagKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVOutOfBandAlternateTrackIsDefaultKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVOutOfBandAlternateTrackMediaCharactersticsKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVOutOfBandAlternateTrackIdentifierKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVOutOfBandAlternateTrackSourceKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicDescribesMusicAndSoundForAccessibility, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaCharacteristicIsAuxiliaryContent, NSString *)

#define AVURLAssetOutOfBandAlternateTracksKey getAVURLAssetOutOfBandAlternateTracksKey()
#define AVURLAssetCacheKey getAVURLAssetCacheKey()
#define AVURLAssetUsesNoPersistentCacheKey getAVURLAssetUsesNoPersistentCacheKey()
#define AVOutOfBandAlternateTrackDisplayNameKey getAVOutOfBandAlternateTrackDisplayNameKey()
#define AVOutOfBandAlternateTrackExtendedLanguageTagKey getAVOutOfBandAlternateTrackExtendedLanguageTagKey()
#define AVOutOfBandAlternateTrackIsDefaultKey getAVOutOfBandAlternateTrackIsDefaultKey()
#define AVOutOfBandAlternateTrackMediaCharactersticsKey getAVOutOfBandAlternateTrackMediaCharactersticsKey()
#define AVOutOfBandAlternateTrackIdentifierKey getAVOutOfBandAlternateTrackIdentifierKey()
#define AVOutOfBandAlternateTrackSourceKey getAVOutOfBandAlternateTrackSourceKey()
#define AVMediaCharacteristicDescribesMusicAndSoundForAccessibility getAVMediaCharacteristicDescribesMusicAndSoundForAccessibility()
#define AVMediaCharacteristicTranscribesSpokenDialogForAccessibility getAVMediaCharacteristicTranscribesSpokenDialogForAccessibility()
#define AVMediaCharacteristicIsAuxiliaryContent getAVMediaCharacteristicIsAuxiliaryContent()

#endif

#if ENABLE(DATACUE_VALUE)

SOFT_LINK_CONSTANT(AVFoundation, AVMetadataKeySpaceQuickTimeUserData, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVMetadataKeySpaceISOUserData, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMetadataKeySpaceQuickTimeMetadata, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMetadataKeySpaceiTunes, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMetadataKeySpaceID3, NSString *)

#define AVMetadataKeySpaceQuickTimeUserData getAVMetadataKeySpaceQuickTimeUserData()
#define AVMetadataKeySpaceISOUserData getAVMetadataKeySpaceISOUserData()
#define AVMetadataKeySpaceQuickTimeMetadata getAVMetadataKeySpaceQuickTimeMetadata()
#define AVMetadataKeySpaceiTunes getAVMetadataKeySpaceiTunes()
#define AVMetadataKeySpaceID3 getAVMetadataKeySpaceID3()

#endif

#if PLATFORM(IOS_FAMILY)

SOFT_LINK_CONSTANT(AVFoundation, AVURLAssetBoundNetworkInterfaceName, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVURLAssetClientBundleIdentifierKey, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVURLAssetHTTPCookiesKey, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVURLAssetRequiresCustomURLLoadingKey, NSString *)

#define AVURLAssetBoundNetworkInterfaceName getAVURLAssetBoundNetworkInterfaceName()
#define AVURLAssetClientBundleIdentifierKey getAVURLAssetClientBundleIdentifierKey()
#define AVURLAssetHTTPCookiesKey getAVURLAssetHTTPCookiesKey()
#define AVURLAssetRequiresCustomURLLoadingKey getAVURLAssetRequiresCustomURLLoadingKey()

#endif

SOFT_LINK_FRAMEWORK(MediaToolbox)
SOFT_LINK_OPTIONAL(MediaToolbox, MTEnableCaption2015Behavior, Boolean, (), ())

#if PLATFORM(IOS_FAMILY)

#if HAVE(CELESTIAL)

SOFT_LINK_PRIVATE_FRAMEWORK(Celestial)
SOFT_LINK_CONSTANT(Celestial, AVController_RouteDescriptionKey_RouteCurrentlyPicked, NSString *)
SOFT_LINK_CONSTANT(Celestial, AVController_RouteDescriptionKey_RouteName, NSString *)
SOFT_LINK_CONSTANT(Celestial, AVController_RouteDescriptionKey_AVAudioRouteName, NSString *)
#define AVController_RouteDescriptionKey_RouteCurrentlyPicked getAVController_RouteDescriptionKey_RouteCurrentlyPicked()
#define AVController_RouteDescriptionKey_RouteName getAVController_RouteDescriptionKey_RouteName()
#define AVController_RouteDescriptionKey_AVAudioRouteName getAVController_RouteDescriptionKey_AVAudioRouteName()

#endif // HAVE(CELESTIAL)

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIDevice)
#define UIDevice getUIDeviceClass()

#endif // PLATFORM(IOS_FAMILY)

using namespace WebCore;

enum MediaPlayerAVFoundationObservationContext {
    MediaPlayerAVFoundationObservationContextPlayerItem,
    MediaPlayerAVFoundationObservationContextPlayerItemTrack,
    MediaPlayerAVFoundationObservationContextPlayer,
    MediaPlayerAVFoundationObservationContextAVPlayerLayer,
};

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
@interface WebCoreAVFMovieObserver : NSObject <AVPlayerItemLegibleOutputPushDelegate>
#else
@interface WebCoreAVFMovieObserver : NSObject
#endif
{
    WeakPtr<MediaPlayerPrivateAVFoundationObjC> m_player;
    GenericTaskQueue<Timer, std::atomic<unsigned>> m_taskQueue;
    int m_delayCallbacks;
}
-(id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)callback;
-(void)disconnect;
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
    WeakPtr<MediaPlayerPrivateAVFoundationObjC> m_player;
    GenericTaskQueue<Timer, std::atomic<unsigned>> m_taskQueue;
}
- (id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)player;
- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest;
@end
#endif

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
@interface WebCoreAVFPullDelegate : NSObject<AVPlayerItemOutputPullDelegate> {
    WeakPtr<MediaPlayerPrivateAVFoundationObjC> m_player;
}
- (id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)player;
- (void)outputMediaDataWillChange:(AVPlayerItemOutput *)sender;
- (void)outputSequenceWasFlushed:(AVPlayerItemOutput *)output;
@end
#endif

namespace WebCore {
using namespace PAL;

static NSArray *assetMetadataKeyNames();
static NSArray *itemKVOProperties();
static NSArray *assetTrackMetadataKeyNames();
static NSArray *playerKVOProperties();
static AVAssetTrack* firstEnabledTrack(NSArray* tracks);

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

void MediaPlayerPrivateAVFoundationObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (!isAvailable())
        return;

    registrar([](MediaPlayer* player) { return std::make_unique<MediaPlayerPrivateAVFoundationObjC>(player); },
            getSupportedTypes, supportsType, originsInMediaCache, clearMediaCache, clearMediaCacheForOrigins, supportsKeySystem);
    ASSERT(AVFoundationMIMETypeCache::singleton().isAvailable());
}

static AVAssetCacheType *assetCacheForPath(const String& path)
{
    NSURL *assetCacheURL;
    
    if (path.isEmpty())
        assetCacheURL = [[NSURL fileURLWithPath:NSTemporaryDirectory()] URLByAppendingPathComponent:@"MediaCache" isDirectory:YES];
    else
        assetCacheURL = [NSURL fileURLWithPath:path isDirectory:YES];

    return [initAVAssetCache() assetCacheWithURL:assetCacheURL];
}

HashSet<RefPtr<SecurityOrigin>> MediaPlayerPrivateAVFoundationObjC::originsInMediaCache(const String& path)
{
    HashSet<RefPtr<SecurityOrigin>> origins;
    for (NSString *key in [assetCacheForPath(path) allKeys]) {
        URL keyAsURL = URL(URL(), key);
        if (keyAsURL.isValid())
            origins.add(SecurityOrigin::create(keyAsURL));
    }
    return origins;
}

static WallTime toSystemClockTime(NSDate *date)
{
    ASSERT(date);
    return WallTime::fromRawSeconds(date.timeIntervalSince1970);
}

void MediaPlayerPrivateAVFoundationObjC::clearMediaCache(const String& path, WallTime modifiedSince)
{
    AVAssetCacheType* assetCache = assetCacheForPath(path);
    
    for (NSString *key in [assetCache allKeys]) {
        if (toSystemClockTime([assetCache lastModifiedDateOfEntryForKey:key]) > modifiedSince)
            [assetCache removeEntryForKey:key];
    }

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *baseURL = [assetCache URL];

    if (modifiedSince <= WallTime::fromRawSeconds(0)) {
        [fileManager removeItemAtURL:baseURL error:nil];
        return;
    }
    
    NSArray *propertyKeys = @[NSURLNameKey, NSURLContentModificationDateKey, NSURLIsRegularFileKey];
    NSDirectoryEnumerator *enumerator = [fileManager enumeratorAtURL:baseURL includingPropertiesForKeys:
        propertyKeys options:NSDirectoryEnumerationSkipsSubdirectoryDescendants
        errorHandler:nil];
    
    RetainPtr<NSMutableArray> urlsToDelete = adoptNS([[NSMutableArray alloc] init]);
    for (NSURL *fileURL : enumerator) {
        NSDictionary *fileAttributes = [fileURL resourceValuesForKeys:propertyKeys error:nil];
    
        if (![fileAttributes[NSURLNameKey] hasPrefix:@"CachedMedia-"])
            continue;
        
        if (![fileAttributes[NSURLIsRegularFileKey] boolValue])
            continue;
        
        if (toSystemClockTime(fileAttributes[NSURLContentModificationDateKey]) <= modifiedSince)
            continue;
        
        [urlsToDelete addObject:fileURL];
    }
    
    for (NSURL *fileURL in urlsToDelete.get())
        [fileManager removeItemAtURL:fileURL error:nil];
}

void MediaPlayerPrivateAVFoundationObjC::clearMediaCacheForOrigins(const String& path, const HashSet<RefPtr<SecurityOrigin>>& origins)
{
    AVAssetCacheType* assetCache = assetCacheForPath(path);
    for (NSString *key in [assetCache allKeys]) {
        URL keyAsURL = URL(URL(), key);
        if (keyAsURL.isValid()) {
            if (origins.contains(SecurityOrigin::create(keyAsURL)))
                [assetCache removeEntryForKey:key];
        }
    }
}

MediaPlayerPrivateAVFoundationObjC::MediaPlayerPrivateAVFoundationObjC(MediaPlayer* player)
    : MediaPlayerPrivateAVFoundation(player)
    , m_videoFullscreenLayerManager(std::make_unique<VideoFullscreenLayerManagerObjC>())
    , m_videoFullscreenGravity(MediaPlayer::VideoGravityResizeAspect)
    , m_objcObserver(adoptNS([[WebCoreAVFMovieObserver alloc] initWithPlayer:m_weakPtrFactory.createWeakPtr(*this)]))
    , m_videoFrameHasDrawn(false)
    , m_haveCheckedPlayability(false)
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    , m_videoOutputDelegate(adoptNS([[WebCoreAVFPullDelegate alloc] initWithPlayer:m_weakPtrFactory.createWeakPtr(*this)]))
#endif
#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    , m_loaderDelegate(adoptNS([[WebCoreAVFLoaderDelegate alloc] initWithPlayer:m_weakPtrFactory.createWeakPtr(*this)]))
#endif
    , m_currentTextTrack(0)
    , m_cachedRate(0)
    , m_cachedTotalBytes(0)
    , m_pendingStatusChanges(0)
    , m_cachedItemStatus(MediaPlayerAVPlayerItemStatusDoesNotExist)
    , m_cachedLikelyToKeepUp(false)
    , m_cachedBufferEmpty(false)
    , m_cachedBufferFull(false)
    , m_cachedHasEnabledAudio(false)
    , m_shouldBufferData(true)
    , m_cachedIsReadyForDisplay(false)
    , m_haveBeenAskedToCreateLayer(false)
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    , m_allowsWirelessVideoPlayback(true)
#endif
{
}

MediaPlayerPrivateAVFoundationObjC::~MediaPlayerPrivateAVFoundationObjC()
{
    m_weakPtrFactory.revokeAll();

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    [[m_avAsset.get() resourceLoader] setDelegate:nil queue:0];

    for (auto& pair : m_resourceLoaderMap)
        pair.value->invalidate();
#endif
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    [m_videoOutput setDelegate:nil queue:0];
#endif

    if (m_videoLayer)
        destroyVideoLayer();

    cancelLoad();
}

void MediaPlayerPrivateAVFoundationObjC::cancelLoad()
{
    INFO_LOG(LOGIDENTIFIER);
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

        for (NSString *keyName in playerKVOProperties())
            [m_avPlayer.get() removeObserver:m_objcObserver.get() forKeyPath:keyName];

        setShouldObserveTimeControlStatus(false);

        [m_avPlayer replaceCurrentItemWithPlayerItem:nil];
        m_avPlayer = nil;
    }

    // Reset cached properties
    m_pendingStatusChanges = 0;
    m_cachedItemStatus = MediaPlayerAVPlayerItemStatusDoesNotExist;
    m_cachedSeekableRanges = nullptr;
    m_cachedLoadedRanges = nullptr;
    m_cachedHasEnabledAudio = false;
    m_cachedPresentationSize = FloatSize();
    m_cachedDuration = MediaTime::zeroTime();

    for (AVPlayerItemTrack *track in m_cachedTracks.get())
        [track removeObserver:m_objcObserver.get() forKeyPath:@"enabled"];
    m_cachedTracks = nullptr;

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    if (m_provider) {
        m_provider->setPlayerItem(nullptr);
        m_provider->setAudioTrack(nullptr);
    }
#endif

    setIgnoreLoadStateChanges(false);
}

bool MediaPlayerPrivateAVFoundationObjC::hasLayerRenderer() const
{
    return m_haveBeenAskedToCreateLayer;
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
    using namespace PAL;
    INFO_LOG(LOGIDENTIFIER);

    if (!m_avAsset || m_imageGenerator)
        return;

    m_imageGenerator = [AVAssetImageGenerator assetImageGeneratorWithAsset:m_avAsset.get()];

    [m_imageGenerator.get() setApertureMode:AVAssetImageGeneratorApertureModeCleanAperture];
    [m_imageGenerator.get() setAppliesPreferredTrackTransform:YES];
    [m_imageGenerator.get() setRequestedTimeToleranceBefore:kCMTimeZero];
    [m_imageGenerator.get() setRequestedTimeToleranceAfter:kCMTimeZero];
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

    INFO_LOG(LOGIDENTIFIER);

    m_imageGenerator = 0;
}

void MediaPlayerPrivateAVFoundationObjC::createVideoLayer()
{
    if (!m_avPlayer || m_haveBeenAskedToCreateLayer)
        return;

    callOnMainThread([this, weakThis = makeWeakPtr(*this)] {
        if (!weakThis)
            return;

        if (!m_avPlayer || m_haveBeenAskedToCreateLayer)
            return;
        m_haveBeenAskedToCreateLayer = true;

        if (!m_videoLayer)
            createAVPlayerLayer();

#if USE(VIDEOTOOLBOX) && HAVE(AVFOUNDATION_VIDEO_OUTPUT)
        if (!m_videoOutput)
            createVideoOutput();
#endif

        player()->client().mediaPlayerRenderingModeChanged(player());
    });
}

void MediaPlayerPrivateAVFoundationObjC::createAVPlayerLayer()
{
    if (!m_avPlayer)
        return;

    m_videoLayer = adoptNS([[AVPlayerLayer alloc] init]);
    [m_videoLayer setPlayer:m_avPlayer.get()];

#ifndef NDEBUG
    [m_videoLayer setName:@"MediaPlayerPrivate AVPlayerLayer"];
#endif
    [m_videoLayer addObserver:m_objcObserver.get() forKeyPath:@"readyForDisplay" options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextAVPlayerLayer];
    updateVideoLayerGravity();
    [m_videoLayer setContentsScale:player()->client().mediaPlayerContentsScale()];
    IntSize defaultSize = snappedIntRect(player()->client().mediaPlayerContentBoxRect()).size();
    INFO_LOG(LOGIDENTIFIER);

    m_videoFullscreenLayerManager->setVideoLayer(m_videoLayer.get(), defaultSize);

#if PLATFORM(IOS_FAMILY) && !PLATFORM(WATCHOS)
    if ([m_videoLayer respondsToSelector:@selector(setPIPModeEnabled:)])
        [m_videoLayer setPIPModeEnabled:(player()->fullscreenMode() & MediaPlayer::VideoFullscreenModePictureInPicture)];
#endif
}

void MediaPlayerPrivateAVFoundationObjC::destroyVideoLayer()
{
    if (!m_videoLayer)
        return;

    INFO_LOG(LOGIDENTIFIER);

    [m_videoLayer removeObserver:m_objcObserver.get() forKeyPath:@"readyForDisplay"];
    [m_videoLayer setPlayer:nil];
    m_videoFullscreenLayerManager->didDestroyVideoLayer();

    m_videoLayer = nil;
}

MediaTime MediaPlayerPrivateAVFoundationObjC::getStartDate() const
{
    // Date changes as the track's playback position changes. Must subtract currentTime (offset in seconds) from date offset to get date beginning
    double date = [[m_avPlayerItem currentDate] timeIntervalSince1970] * 1000;

    // No live streams were made during the epoch (1970). AVFoundation returns 0 if the media file doesn't have a start date
    if (!date)
        return MediaTime::invalidTime();

    double currentTime = CMTimeGetSeconds([m_avPlayerItem currentTime]) * 1000;

    // Rounding due to second offset error when subtracting.
    return MediaTime::createWithDouble(round(date - currentTime));
}

bool MediaPlayerPrivateAVFoundationObjC::hasAvailableVideoFrame() const
{
    if (currentRenderingMode() == MediaRenderingToLayer)
        return m_cachedIsReadyForDisplay;

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    if (m_videoOutput && (m_lastPixelBuffer || [m_videoOutput hasNewPixelBufferForItemTime:[m_avPlayerItem currentTime]]))
        return true;
#endif

    return m_videoFrameHasDrawn;
}

#if ENABLE(AVF_CAPTIONS)
static const NSArray* mediaDescriptionForKind(PlatformTextTrack::TrackKind kind)
{
    static bool manualSelectionMode = MTEnableCaption2015BehaviorPtr() && MTEnableCaption2015BehaviorPtr()();
    if (manualSelectionMode)
        return @[ AVMediaCharacteristicIsAuxiliaryContent ];

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
    
void MediaPlayerPrivateAVFoundationObjC::notifyTrackModeChanged()
{
    trackModeChanged();
}
    
void MediaPlayerPrivateAVFoundationObjC::synchronizeTextTrackState()
{
    const Vector<RefPtr<PlatformTextTrack>>& outOfBandTrackSources = player()->outOfBandTrackSources();
    
    for (auto& textTrack : m_textTracks) {
        if (textTrack->textTrackCategory() != InbandTextTrackPrivateAVF::OutOfBand)
            continue;
        
        RefPtr<OutOfBandTextTrackPrivateAVF> trackPrivate = static_cast<OutOfBandTextTrackPrivateAVF*>(textTrack.get());
        RetainPtr<AVMediaSelectionOptionType> currentOption = trackPrivate->mediaSelectionOption();
        
        for (auto& track : outOfBandTrackSources) {
            RetainPtr<CFStringRef> uniqueID = String::number(track->uniqueId()).createCFString();
            
            if (![[currentOption.get() outOfBandIdentifier] isEqual:(__bridge NSString *)uniqueID.get()])
                continue;
            
            InbandTextTrackPrivate::Mode mode = InbandTextTrackPrivate::Hidden;
            if (track->mode() == PlatformTextTrack::Hidden)
                mode = InbandTextTrackPrivate::Hidden;
            else if (track->mode() == PlatformTextTrack::Disabled)
                mode = InbandTextTrackPrivate::Disabled;
            else if (track->mode() == PlatformTextTrack::Showing)
                mode = InbandTextTrackPrivate::Showing;
            
            textTrack->setMode(mode);
            break;
        }
    }
}
#endif


static NSURL *canonicalURL(const URL& url)
{
    NSURL *cocoaURL = url;
    if (url.isEmpty())
        return cocoaURL;

    RetainPtr<NSURLRequest> request = adoptNS([[NSURLRequest alloc] initWithURL:cocoaURL]);
    if (!request)
        return cocoaURL;

    NSURLRequest *canonicalRequest = [NSURLProtocol canonicalRequestForRequest:request.get()];
    if (!canonicalRequest)
        return cocoaURL;

    return [canonicalRequest URL];
}

#if PLATFORM(IOS_FAMILY)
static NSHTTPCookie* toNSHTTPCookie(const Cookie& cookie)
{
    RetainPtr<NSMutableDictionary> properties = adoptNS([[NSMutableDictionary alloc] init]);
    [properties setDictionary:@{
        NSHTTPCookieName: cookie.name,
        NSHTTPCookieValue: cookie.value,
        NSHTTPCookieDomain: cookie.domain,
        NSHTTPCookiePath: cookie.path,
        NSHTTPCookieExpires: [NSDate dateWithTimeIntervalSince1970:(cookie.expires / 1000)],
    }];
    if (cookie.secure)
        [properties setObject:@YES forKey:NSHTTPCookieSecure];
    if (cookie.session)
        [properties setObject:@YES forKey:NSHTTPCookieDiscard];

    return [NSHTTPCookie cookieWithProperties:properties.get()];
}
#endif

void MediaPlayerPrivateAVFoundationObjC::createAVAssetForURL(const URL& url)
{
    if (m_avAsset)
        return;

    INFO_LOG(LOGIDENTIFIER);

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

    if (player()->doesHaveAttribute("x-itunes-inherit-uri-query-component"))
        [options.get() setObject:@YES forKey: AVURLAssetInheritURIQueryComponentFromReferencingURIKey];

    if (canLoadAVURLAssetUseClientURLLoadingExclusively())
        [options setObject:@YES forKey:AVURLAssetUseClientURLLoadingExclusively];
#if PLATFORM(IOS_FAMILY)
    else if (canLoadAVURLAssetRequiresCustomURLLoadingKey())
        [options setObject:@YES forKey:AVURLAssetRequiresCustomURLLoadingKey];
    // FIXME: rdar://problem/20354688
    String identifier = player()->sourceApplicationIdentifier();
    if (!identifier.isEmpty() && canLoadAVURLAssetClientBundleIdentifierKey())
        [options setObject:identifier forKey:AVURLAssetClientBundleIdentifierKey];
#endif

    auto type = player()->contentMIMEType();
    if (canLoadAVURLAssetOutOfBandMIMETypeKey() && !type.isEmpty() && !player()->contentMIMETypeWasInferredFromExtension()) {
        auto codecs = player()->contentTypeCodecs();
        if (!codecs.isEmpty()) {
            NSString *typeString = [NSString stringWithFormat:@"%@; codecs=\"%@\"", (NSString *)type, (NSString *)codecs];
            [options setObject:typeString forKey:AVURLAssetOutOfBandMIMETypeKey];
        } else
            [options setObject:(NSString *)type forKey:AVURLAssetOutOfBandMIMETypeKey];
    }

#if ENABLE(AVF_CAPTIONS)
    const Vector<RefPtr<PlatformTextTrack>>& outOfBandTrackSources = player()->outOfBandTrackSources();
    if (!outOfBandTrackSources.isEmpty()) {
        RetainPtr<NSMutableArray> outOfBandTracks = adoptNS([[NSMutableArray alloc] init]);
        for (auto& trackSource : outOfBandTrackSources) {
            RetainPtr<CFStringRef> label = trackSource->label().createCFString();
            RetainPtr<CFStringRef> language = trackSource->language().createCFString();
            RetainPtr<CFStringRef> uniqueID = String::number(trackSource->uniqueId()).createCFString();
            RetainPtr<CFStringRef> url = trackSource->url().createCFString();
            [outOfBandTracks.get() addObject:@{
                AVOutOfBandAlternateTrackDisplayNameKey: (__bridge NSString *)label.get(),
                AVOutOfBandAlternateTrackExtendedLanguageTagKey: (__bridge NSString *)language.get(),
                AVOutOfBandAlternateTrackIsDefaultKey: trackSource->isDefault() ? @YES : @NO,
                AVOutOfBandAlternateTrackIdentifierKey: (__bridge NSString *)uniqueID.get(),
                AVOutOfBandAlternateTrackSourceKey: (__bridge NSString *)url.get(),
                AVOutOfBandAlternateTrackMediaCharactersticsKey: mediaDescriptionForKind(trackSource->kind()),
            }];
        }

        [options.get() setObject:outOfBandTracks.get() forKey:AVURLAssetOutOfBandAlternateTracksKey];
    }
#endif

#if PLATFORM(IOS_FAMILY)
    String networkInterfaceName = player()->mediaPlayerNetworkInterfaceName();
    if (!networkInterfaceName.isEmpty())
        [options setObject:networkInterfaceName forKey:AVURLAssetBoundNetworkInterfaceName];
#endif

#if PLATFORM(IOS_FAMILY)
    Vector<Cookie> cookies;
    if (player()->getRawCookies(url, cookies)) {
        RetainPtr<NSMutableArray> nsCookies = adoptNS([[NSMutableArray alloc] initWithCapacity:cookies.size()]);
        for (auto& cookie : cookies)
            [nsCookies addObject:toNSHTTPCookie(cookie)];

        if (canLoadAVURLAssetHTTPCookiesKey())
            [options setObject:nsCookies.get() forKey:AVURLAssetHTTPCookiesKey];
    }
#endif

    bool usePersistentCache = player()->client().mediaPlayerShouldUsePersistentCache();
    [options setObject:@(!usePersistentCache) forKey:AVURLAssetUsesNoPersistentCacheKey];
    
    if (usePersistentCache)
        [options setObject:assetCacheForPath(player()->client().mediaPlayerMediaCacheDirectory()) forKey:AVURLAssetCacheKey];

    NSURL *cocoaURL = canonicalURL(url);
    m_avAsset = adoptNS([[AVURLAsset alloc] initWithURL:cocoaURL options:options.get()]);

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    AVAssetResourceLoader *resourceLoader = m_avAsset.get().resourceLoader;
    [resourceLoader setDelegate:m_loaderDelegate.get() queue:globalLoaderDelegateQueue()];

    if (DeprecatedGlobalSettings::isAVFoundationNSURLSessionEnabled()
        && [resourceLoader respondsToSelector:@selector(setURLSession:)]
        && [resourceLoader respondsToSelector:@selector(URLSessionDataDelegate)]
        && [resourceLoader respondsToSelector:@selector(URLSessionDataDelegateQueue)]) {
        RefPtr<PlatformMediaResourceLoader> mediaResourceLoader = player()->createResourceLoader();
        if (mediaResourceLoader)
            resourceLoader.URLSession = (NSURLSession *)[[[WebCoreNSURLSession alloc] initWithResourceLoader:*mediaResourceLoader delegate:resourceLoader.URLSessionDataDelegate delegateQueue:resourceLoader.URLSessionDataDelegateQueue] autorelease];
    }

#endif

    m_haveCheckedPlayability = false;

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::setAVPlayerItem(AVPlayerItemType *item)
{
    if (!m_avPlayer)
        return;

    if (pthread_main_np()) {
        [m_avPlayer replaceCurrentItemWithPlayerItem:item];
        return;
    }

    RetainPtr<AVPlayerType> strongPlayer = m_avPlayer.get();
    RetainPtr<AVPlayerItemType> strongItem = item;
    dispatch_async(dispatch_get_main_queue(), [strongPlayer, strongItem] {
        [strongPlayer replaceCurrentItemWithPlayerItem:strongItem.get()];
    });
}

void MediaPlayerPrivateAVFoundationObjC::createAVPlayer()
{
    if (m_avPlayer)
        return;

    INFO_LOG(LOGIDENTIFIER);

    setDelayCallbacks(true);

    m_avPlayer = adoptNS([[AVPlayer alloc] init]);
    for (NSString *keyName in playerKVOProperties())
        [m_avPlayer.get() addObserver:m_objcObserver.get() forKeyPath:keyName options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextPlayer];

    setShouldObserveTimeControlStatus(true);

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    [m_avPlayer.get() setAppliesMediaSelectionCriteriaAutomatically:NO];
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    updateDisableExternalPlayback();
    [m_avPlayer.get() setAllowsExternalPlayback:m_allowsWirelessVideoPlayback];
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    if (m_shouldPlayToPlaybackTarget) {
        // Clear m_shouldPlayToPlaybackTarget so doesn't return without doing anything.
        m_shouldPlayToPlaybackTarget = false;
        setShouldPlayToPlaybackTarget(true);
    }
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(IOSMAC)
    setShouldDisableSleep(player()->shouldDisableSleep());
#endif

    if (m_muted) {
        // Clear m_muted so setMuted doesn't return without doing anything.
        m_muted = false;
        [m_avPlayer.get() setMuted:m_muted];
    }

    if (player()->client().mediaPlayerIsVideo())
        createAVPlayerLayer();

    if (m_avPlayerItem)
        setAVPlayerItem(m_avPlayerItem.get());

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::createAVPlayerItem()
{
    if (m_avPlayerItem)
        return;

    INFO_LOG(LOGIDENTIFIER);

    setDelayCallbacks(true);

    // Create the player item so we can load media data. 
    m_avPlayerItem = adoptNS([[AVPlayerItem alloc] initWithAsset:m_avAsset.get()]);

    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(didEnd:) name:AVPlayerItemDidPlayToEndTimeNotification object:m_avPlayerItem.get()];

    NSKeyValueObservingOptions options = NSKeyValueObservingOptionNew | NSKeyValueObservingOptionPrior;
    for (NSString *keyName in itemKVOProperties())
        [m_avPlayerItem.get() addObserver:m_objcObserver.get() forKeyPath:keyName options:options context:(void *)MediaPlayerAVFoundationObservationContextPlayerItem];

    [m_avPlayerItem setAudioTimePitchAlgorithm:(player()->preservesPitch() ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed)];

    if (m_avPlayer)
        setAVPlayerItem(m_avPlayerItem.get());

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    const NSTimeInterval legibleOutputAdvanceInterval = 2;

    RetainPtr<NSArray> subtypes = adoptNS([[NSArray alloc] initWithObjects:[NSNumber numberWithUnsignedInt:kCMSubtitleFormatType_WebVTT], nil]);
    m_legibleOutput = adoptNS([[AVPlayerItemLegibleOutput alloc] initWithMediaSubtypesForNativeRepresentation:subtypes.get()]);
    [m_legibleOutput.get() setSuppressesPlayerRendering:YES];

    [m_legibleOutput.get() setDelegate:m_objcObserver.get() queue:dispatch_get_main_queue()];
    [m_legibleOutput.get() setAdvanceIntervalForDelegateInvocation:legibleOutputAdvanceInterval];
    [m_legibleOutput.get() setTextStylingResolution:AVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly];
    [m_avPlayerItem.get() addOutput:m_legibleOutput.get()];
#endif

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    if (m_provider) {
        m_provider->setPlayerItem(m_avPlayerItem.get());
        m_provider->setAudioTrack(firstEnabledTrack(safeAVAssetTracksForAudibleMedia()));
    }
#endif

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    createVideoOutput();
#endif

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::checkPlayability()
{
    if (m_haveCheckedPlayability)
        return;
    m_haveCheckedPlayability = true;

    INFO_LOG(LOGIDENTIFIER);
    auto weakThis = makeWeakPtr(*this);

    [m_avAsset.get() loadValuesAsynchronouslyForKeys:[NSArray arrayWithObjects:@"playable", @"tracks", nil] completionHandler:^{
        callOnMainThread([weakThis] {
            if (weakThis)
                weakThis->scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification::AssetPlayabilityKnown);
        });
    }];
}

void MediaPlayerPrivateAVFoundationObjC::beginLoadingMetadata()
{
    INFO_LOG(LOGIDENTIFIER);

    OSObjectPtr<dispatch_group_t> metadataLoadingGroup = adoptOSObject(dispatch_group_create());
    dispatch_group_enter(metadataLoadingGroup.get());
    auto weakThis = makeWeakPtr(*this);
    [m_avAsset.get() loadValuesAsynchronouslyForKeys:assetMetadataKeyNames() completionHandler:^{

        callOnMainThread([weakThis, metadataLoadingGroup] {
            if (weakThis && [weakThis->m_avAsset.get() statusOfValueForKey:@"tracks" error:nil] == AVKeyValueStatusLoaded) {
                for (AVAssetTrack *track in [weakThis->m_avAsset.get() tracks]) {
                    dispatch_group_enter(metadataLoadingGroup.get());
                    [track loadValuesAsynchronouslyForKeys:assetTrackMetadataKeyNames() completionHandler:^{
                        dispatch_group_leave(metadataLoadingGroup.get());
                    }];
                }
            }
            dispatch_group_leave(metadataLoadingGroup.get());
        });
    }];

    dispatch_group_notify(metadataLoadingGroup.get(), dispatch_get_main_queue(), ^{
        callOnMainThread([weakThis] {
            if (weakThis)
                [weakThis->m_objcObserver.get() metadataLoaded];
        });
    });
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

PlatformLayer* MediaPlayerPrivateAVFoundationObjC::platformLayer() const
{
    return m_videoFullscreenLayerManager->videoInlineLayer();
}

void MediaPlayerPrivateAVFoundationObjC::updateVideoFullscreenInlineImage()
{
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    updateLastImage(UpdateType::UpdateSynchronously);
    m_videoFullscreenLayerManager->updateVideoFullscreenInlineImage(m_lastImage);
#endif
}

void MediaPlayerPrivateAVFoundationObjC::setVideoFullscreenLayer(PlatformLayer* videoFullscreenLayer, Function<void()>&& completionHandler)
{
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    updateLastImage(UpdateType::UpdateSynchronously);
    m_videoFullscreenLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), m_lastImage);
#else
    m_videoFullscreenLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), nil);
#endif
    updateDisableExternalPlayback();
}

void MediaPlayerPrivateAVFoundationObjC::setVideoFullscreenFrame(FloatRect frame)
{
    m_videoFullscreenLayerManager->setVideoFullscreenFrame(frame);
}

void MediaPlayerPrivateAVFoundationObjC::setVideoFullscreenGravity(MediaPlayer::VideoGravity gravity)
{
    m_videoFullscreenGravity = gravity;

    if (!m_videoLayer)
        return;

    NSString *videoGravity = AVLayerVideoGravityResizeAspect;
    if (gravity == MediaPlayer::VideoGravityResize)
        videoGravity = AVLayerVideoGravityResize;
    else if (gravity == MediaPlayer::VideoGravityResizeAspect)
        videoGravity = AVLayerVideoGravityResizeAspect;
    else if (gravity == MediaPlayer::VideoGravityResizeAspectFill)
        videoGravity = AVLayerVideoGravityResizeAspectFill;
    else
        ASSERT_NOT_REACHED();
    
    if ([m_videoLayer videoGravity] == videoGravity)
        return;

    [m_videoLayer setVideoGravity:videoGravity];
    syncTextTrackBounds();
}

void MediaPlayerPrivateAVFoundationObjC::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode mode)
{
#if PLATFORM(IOS_FAMILY) && !PLATFORM(WATCHOS)
    if ([m_videoLayer respondsToSelector:@selector(setPIPModeEnabled:)])
        [m_videoLayer setPIPModeEnabled:(mode & MediaPlayer::VideoFullscreenModePictureInPicture)];
    updateDisableExternalPlayback();
#else
    UNUSED_PARAM(mode);
#endif
}
    
void MediaPlayerPrivateAVFoundationObjC::videoFullscreenStandbyChanged()
{
#if PLATFORM(IOS_FAMILY) && !PLATFORM(WATCHOS)
    updateDisableExternalPlayback();
#endif
}

#if PLATFORM(IOS_FAMILY)
NSArray *MediaPlayerPrivateAVFoundationObjC::timedMetadata() const
{
    if (m_currentMetaData)
        return m_currentMetaData.get();
    return nil;
}

String MediaPlayerPrivateAVFoundationObjC::accessLog() const
{
    if (!m_avPlayerItem)
        return emptyString();
    
    AVPlayerItemAccessLog *log = [m_avPlayerItem.get() accessLog];
    RetainPtr<NSString> logString = adoptNS([[NSString alloc] initWithData:[log extendedLogData] encoding:[log extendedLogDataStringEncoding]]);

    return logString.get();
}

String MediaPlayerPrivateAVFoundationObjC::errorLog() const
{
    if (!m_avPlayerItem)
        return emptyString();

    AVPlayerItemErrorLog *log = [m_avPlayerItem.get() errorLog];
    RetainPtr<NSString> logString = adoptNS([[NSString alloc] initWithData:[log extendedLogData] encoding:[log extendedLogDataStringEncoding]]);

    return logString.get();
}
#endif

void MediaPlayerPrivateAVFoundationObjC::didEnd()
{
    m_requestedPlaying = false;
    MediaPlayerPrivateAVFoundation::didEnd();
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
    INFO_LOG(LOGIDENTIFIER);
    if (!metaDataAvailable())
        return;

    m_requestedPlaying = true;
    setPlayerRate(m_requestedRate);
}

void MediaPlayerPrivateAVFoundationObjC::platformPause()
{
    INFO_LOG(LOGIDENTIFIER);
    if (!metaDataAvailable())
        return;

    m_requestedPlaying = false;
    setPlayerRate(0);
}

MediaTime MediaPlayerPrivateAVFoundationObjC::platformDuration() const
{
    // Do not ask the asset for duration before it has been loaded or it will fetch the
    // answer synchronously.
    if (!m_avAsset || assetStatus() < MediaPlayerAVAssetStatusLoaded)
        return MediaTime::invalidTime();
    
    CMTime cmDuration;
    
    // Check the AVItem if we have one and it has loaded duration, some assets never report duration.
    if (m_avPlayerItem && playerItemStatus() >= MediaPlayerAVPlayerItemStatusReadyToPlay)
        cmDuration = [m_avPlayerItem.get() duration];
    else
        cmDuration = [m_avAsset.get() duration];

    if (CMTIME_IS_NUMERIC(cmDuration))
        return PAL::toMediaTime(cmDuration);

    if (CMTIME_IS_INDEFINITE(cmDuration))
        return MediaTime::positiveInfiniteTime();

    INFO_LOG(LOGIDENTIFIER, "returning invalid time");
    return MediaTime::invalidTime();
}

MediaTime MediaPlayerPrivateAVFoundationObjC::currentMediaTime() const
{
    if (!metaDataAvailable() || !m_avPlayerItem)
        return MediaTime::zeroTime();

    CMTime itemTime = [m_avPlayerItem.get() currentTime];
    if (CMTIME_IS_NUMERIC(itemTime))
        return std::max(PAL::toMediaTime(itemTime), MediaTime::zeroTime());

    return MediaTime::zeroTime();
}

void MediaPlayerPrivateAVFoundationObjC::seekToTime(const MediaTime& time, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
{
    // setCurrentTime generates several event callbacks, update afterwards.
    setDelayCallbacks(true);

    if (m_metadataTrack)
        m_metadataTrack->flushPartialCues();

    CMTime cmTime = PAL::toCMTime(time);
    CMTime cmBefore = PAL::toCMTime(negativeTolerance);
    CMTime cmAfter = PAL::toCMTime(positiveTolerance);

    // [AVPlayerItem seekToTime] will throw an exception if toleranceBefore is negative.
    if (CMTimeCompare(cmBefore, kCMTimeZero) < 0)
        cmBefore = kCMTimeZero;
    
    auto weakThis = makeWeakPtr(*this);

    setShouldObserveTimeControlStatus(false);
    [m_avPlayerItem.get() seekToTime:cmTime toleranceBefore:cmBefore toleranceAfter:cmAfter completionHandler:^(BOOL finished) {
        callOnMainThread([weakThis, finished] {
            auto _this = weakThis.get();
            if (!_this)
                return;

            _this->setShouldObserveTimeControlStatus(true);
            _this->seekCompleted(finished);
        });
    }];

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::setVolume(float volume)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(volume);
    return;
#else
    if (!m_avPlayer)
        return;

    [m_avPlayer.get() setVolume:volume];
#endif
}

void MediaPlayerPrivateAVFoundationObjC::setMuted(bool muted)
{
    if (m_muted == muted)
        return;

    INFO_LOG(LOGIDENTIFIER, "- ", muted);

    m_muted = muted;

    if (!m_avPlayer)
        return;

    [m_avPlayer.get() setMuted:m_muted];
}

void MediaPlayerPrivateAVFoundationObjC::setClosedCaptionsVisible(bool closedCaptionsVisible)
{
    UNUSED_PARAM(closedCaptionsVisible);

    if (!metaDataAvailable())
        return;

    INFO_LOG(LOGIDENTIFIER, "- ", closedCaptionsVisible);
}

void MediaPlayerPrivateAVFoundationObjC::setRateDouble(double rate)
{
    m_requestedRate = rate;
    if (m_requestedPlaying)
        setPlayerRate(rate);
}

void MediaPlayerPrivateAVFoundationObjC::setPlayerRate(double rate)
{
    setDelayCallbacks(true);
    m_cachedRate = rate;
    setShouldObserveTimeControlStatus(false);
    [m_avPlayer setRate:rate];
    m_cachedTimeControlStatus = [m_avPlayer timeControlStatus];
    setShouldObserveTimeControlStatus(true);
    setDelayCallbacks(false);
}

double MediaPlayerPrivateAVFoundationObjC::rate() const
{
    if (!metaDataAvailable())
        return 0;

    return m_cachedRate;
}

double MediaPlayerPrivateAVFoundationObjC::seekableTimeRangesLastModifiedTime() const
{
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
    return [m_avPlayerItem seekableTimeRangesLastModifiedTime];
#else
    return 0;
#endif
}

double MediaPlayerPrivateAVFoundationObjC::liveUpdateInterval() const
{
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
    return [m_avPlayerItem liveUpdateInterval];
#else
    return 0;
#endif
}

void MediaPlayerPrivateAVFoundationObjC::setPreservesPitch(bool preservesPitch)
{
    if (m_avPlayerItem)
        [m_avPlayerItem setAudioTimePitchAlgorithm:(preservesPitch ? AVAudioTimePitchAlgorithmSpectral : AVAudioTimePitchAlgorithmVarispeed)];
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateAVFoundationObjC::platformBufferedTimeRanges() const
{
    auto timeRanges = std::make_unique<PlatformTimeRanges>();

    if (!m_avPlayerItem)
        return timeRanges;

    for (NSValue *thisRangeValue in m_cachedLoadedRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (CMTIMERANGE_IS_VALID(timeRange) && !CMTIMERANGE_IS_EMPTY(timeRange))
            timeRanges->add(PAL::toMediaTime(timeRange.start), PAL::toMediaTime(CMTimeRangeGetEnd(timeRange)));
    }
    return timeRanges;
}

MediaTime MediaPlayerPrivateAVFoundationObjC::platformMinTimeSeekable() const
{
    if (!m_cachedSeekableRanges || ![m_cachedSeekableRanges count])
        return MediaTime::zeroTime();

    MediaTime minTimeSeekable = MediaTime::positiveInfiniteTime();
    bool hasValidRange = false;
    for (NSValue *thisRangeValue in m_cachedSeekableRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;

        hasValidRange = true;
        MediaTime startOfRange = PAL::toMediaTime(timeRange.start);
        if (minTimeSeekable > startOfRange)
            minTimeSeekable = startOfRange;
    }
    return hasValidRange ? minTimeSeekable : MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateAVFoundationObjC::platformMaxTimeSeekable() const
{
    if (!m_cachedSeekableRanges)
        m_cachedSeekableRanges = [m_avPlayerItem seekableTimeRanges];

    MediaTime maxTimeSeekable;
    for (NSValue *thisRangeValue in m_cachedSeekableRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;
        
        MediaTime endOfRange = PAL::toMediaTime(CMTimeRangeGetEnd(timeRange));
        if (maxTimeSeekable < endOfRange)
            maxTimeSeekable = endOfRange;
    }
    return maxTimeSeekable;
}

MediaTime MediaPlayerPrivateAVFoundationObjC::platformMaxTimeLoaded() const
{
    if (!m_cachedLoadedRanges)
        return MediaTime::zeroTime();

    MediaTime maxTimeLoaded;
    for (NSValue *thisRangeValue in m_cachedLoadedRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;
        
        MediaTime endOfRange = PAL::toMediaTime(CMTimeRangeGetEnd(timeRange));
        if (maxTimeLoaded < endOfRange)
            maxTimeLoaded = endOfRange;
    }

    return maxTimeLoaded;   
}

unsigned long long MediaPlayerPrivateAVFoundationObjC::totalBytes() const
{
    if (!metaDataAvailable())
        return 0;

    if (m_cachedTotalBytes)
        return m_cachedTotalBytes;

    for (AVPlayerItemTrack *thisTrack in m_cachedTracks.get())
        m_cachedTotalBytes += [[thisTrack assetTrack] totalSampleDataLength];

    return m_cachedTotalBytes;
}

void MediaPlayerPrivateAVFoundationObjC::setAsset(RetainPtr<id>&& asset)
{
    m_avAsset = WTFMove(asset);
}

MediaPlayerPrivateAVFoundation::AssetStatus MediaPlayerPrivateAVFoundationObjC::assetStatus() const
{
    if (!m_avAsset)
        return MediaPlayerAVAssetStatusDoesNotExist;

    for (NSString *keyName in assetMetadataKeyNames()) {
        NSError *error = nil;
        AVKeyValueStatus keyStatus = [m_avAsset.get() statusOfValueForKey:keyName error:&error];

        if (error)
            ERROR_LOG(LOGIDENTIFIER, "failed for ", [keyName UTF8String], ", error = ", [[error localizedDescription] UTF8String]);

        if (keyStatus < AVKeyValueStatusLoaded)
            return MediaPlayerAVAssetStatusLoading;// At least one key is not loaded yet.
        
        if (keyStatus == AVKeyValueStatusFailed)
            return MediaPlayerAVAssetStatusFailed; // At least one key could not be loaded.

        if (keyStatus == AVKeyValueStatusCancelled)
            return MediaPlayerAVAssetStatusCancelled; // Loading of at least one key was cancelled.
    }

    if (!player()->shouldCheckHardwareSupport())
        m_tracksArePlayable = true;

    if (!m_tracksArePlayable) {
        m_tracksArePlayable = true;
        for (AVAssetTrack *track in [m_avAsset tracks]) {
            if (!assetTrackMeetsHardwareDecodeRequirements(track, player()->mediaContentTypesRequiringHardwareSupport())) {
                m_tracksArePlayable = false;
                break;
            }
        }
    }

    if ([[m_avAsset.get() valueForKey:@"playable"] boolValue] && m_tracksArePlayable.value())
        return MediaPlayerAVAssetStatusPlayable;

    return MediaPlayerAVAssetStatusLoaded;
}

long MediaPlayerPrivateAVFoundationObjC::assetErrorCode() const
{
    if (!m_avAsset)
        return 0;

    NSError *error = nil;
    [m_avAsset statusOfValueForKey:@"playable" error:&error];
    return [error code];
}

void MediaPlayerPrivateAVFoundationObjC::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (!metaDataAvailable() || context.paintingDisabled())
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

void MediaPlayerPrivateAVFoundationObjC::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (!metaDataAvailable() || context.paintingDisabled())
        return;

    // We can ignore the request if we are already rendering to a layer.
    if (currentRenderingMode() == MediaRenderingToLayer)
        return;

    // paint() is best effort, so only paint if we already have an image generator or video output available.
    if (!hasContextRenderer())
        return;

    paintCurrentFrameInContext(context, rect);
}

void MediaPlayerPrivateAVFoundationObjC::paintWithImageGenerator(GraphicsContext& context, const FloatRect& rect)
{
    INFO_LOG(LOGIDENTIFIER);

    RetainPtr<CGImageRef> image = createImageForTimeInRect(currentTime(), rect);
    if (image) {
        GraphicsContextStateSaver stateSaver(context);
        context.translate(rect.x(), rect.y() + rect.height());
        context.scale(FloatSize(1.0f, -1.0f));
        context.setImageInterpolationQuality(InterpolationLow);
        IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
        CGContextDrawImage(context.platformContext(), CGRectMake(0, 0, paintRect.width(), paintRect.height()), image.get());
    }
}

RetainPtr<CGImageRef> MediaPlayerPrivateAVFoundationObjC::createImageForTimeInRect(float time, const FloatRect& rect)
{
    if (!m_imageGenerator)
        createImageGenerator();
    ASSERT(m_imageGenerator);

#if !RELEASE_LOG_DISABLED
    MonotonicTime start = MonotonicTime::now();
#endif

    [m_imageGenerator.get() setMaximumSize:CGSize(rect.size())];
    RetainPtr<CGImageRef> rawImage = adoptCF([m_imageGenerator.get() copyCGImageAtTime:CMTimeMakeWithSeconds(time, 600) actualTime:nil error:nil]);
    RetainPtr<CGImageRef> image = adoptCF(CGImageCreateCopyWithColorSpace(rawImage.get(), sRGBColorSpaceRef()));

#if !RELEASE_LOG_DISABLED
    DEBUG_LOG(LOGIDENTIFIER, "creating image took ", (MonotonicTime::now() - start).seconds());
#endif

    return image;
}

void MediaPlayerPrivateAVFoundationObjC::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& supportedTypes)
{
    supportedTypes = AVFoundationMIMETypeCache::singleton().types();
} 

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
static bool keySystemIsSupported(const String& keySystem)
{
    if (equalIgnoringASCIICase(keySystem, "com.apple.fps") || equalIgnoringASCIICase(keySystem, "com.apple.fps.1_0") || equalIgnoringASCIICase(keySystem, "org.w3c.clearkey"))
        return true;
    return false;
}
#endif

MediaPlayer::SupportsType MediaPlayerPrivateAVFoundationObjC::supportsType(const MediaEngineSupportParameters& parameters)
{
#if ENABLE(MEDIA_SOURCE)
    if (parameters.isMediaSource)
        return MediaPlayer::IsNotSupported;
#endif
#if ENABLE(MEDIA_STREAM)
    if (parameters.isMediaStream)
        return MediaPlayer::IsNotSupported;
#endif

    auto containerType = parameters.type.containerType();
    if (isUnsupportedMIMEType(containerType))
        return MediaPlayer::IsNotSupported;

    if (!staticMIMETypeList().contains(containerType) && !AVFoundationMIMETypeCache::singleton().canDecodeType(containerType))
        return MediaPlayer::IsNotSupported;

    // The spec says:
    // "Implementors are encouraged to return "maybe" unless the type can be confidently established as being supported or not."
    if (parameters.type.codecs().isEmpty())
        return MediaPlayer::MayBeSupported;

    if (!contentTypeMeetsHardwareDecodeRequirements(parameters.type, parameters.contentTypesRequiringHardwareSupport))
        return MediaPlayer::IsNotSupported;

    NSString *typeString = [NSString stringWithFormat:@"%@; codecs=\"%@\"", (NSString *)containerType, (NSString *)parameters.type.parameter(ContentType::codecsParameter())];
    return [AVURLAsset isPlayableExtendedMIMEType:typeString] ? MediaPlayer::IsSupported : MediaPlayer::MayBeSupported;
}

bool MediaPlayerPrivateAVFoundationObjC::supportsKeySystem(const String& keySystem, const String& mimeType)
{
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (!keySystem.isEmpty()) {
        // "Clear Key" is only supported with HLS:
        if (equalIgnoringASCIICase(keySystem, "org.w3c.clearkey") && !mimeType.isEmpty() && !equalIgnoringASCIICase(mimeType, "application/x-mpegurl"))
            return MediaPlayer::IsNotSupported;

        if (!keySystemIsSupported(keySystem))
            return false;

        if (!mimeType.isEmpty() && isUnsupportedMIMEType(mimeType))
            return false;

        if (!mimeType.isEmpty() && !staticMIMETypeList().contains(mimeType) && !AVFoundationMIMETypeCache::singleton().canDecodeType(mimeType))
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
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
static void fulfillRequestWithKeyData(AVAssetResourceLoadingRequest *request, ArrayBuffer* keyData)
{
    if (AVAssetResourceLoadingContentInformationRequest *infoRequest = [request contentInformationRequest]) {
        [infoRequest setContentLength:keyData->byteLength()];
        [infoRequest setByteRangeAccessSupported:YES];
    }

    if (AVAssetResourceLoadingDataRequest *dataRequest = [request dataRequest]) {
        long long start = [dataRequest currentOffset];
        long long end = std::min<long long>(keyData->byteLength(), [dataRequest currentOffset] + [dataRequest requestedLength]);

        if (start < 0 || end < 0 || start >= static_cast<long long>(keyData->byteLength())) {
            [request finishLoadingWithError:nil];
            return;
        }

        ASSERT(start <= std::numeric_limits<int>::max());
        ASSERT(end <= std::numeric_limits<int>::max());
        auto requestedKeyData = keyData->slice(static_cast<int>(start), static_cast<int>(end));
        RetainPtr<NSData> nsData = adoptNS([[NSData alloc] initWithBytes:requestedKeyData->data() length:requestedKeyData->byteLength()]);
        [dataRequest respondWithData:nsData.get()];
    }

    [request finishLoading];
}
#endif

bool MediaPlayerPrivateAVFoundationObjC::shouldWaitForLoadingOfResource(AVAssetResourceLoadingRequest* avRequest)
{
    String scheme = [[[avRequest request] URL] scheme];
    String keyURI = [[[avRequest request] URL] absoluteString];

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
    if (scheme == "skd") {
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
        // Create an initData with the following layout:
        // [4 bytes: keyURI size], [keyURI size bytes: keyURI]
        unsigned keyURISize = keyURI.length() * sizeof(UChar);
        auto initDataBuffer = ArrayBuffer::create(4 + keyURISize, 1);
        unsigned byteLength = initDataBuffer->byteLength();
        auto initDataView = JSC::DataView::create(initDataBuffer.copyRef(), 0, byteLength);
        initDataView->set<uint32_t>(0, keyURISize, true);

        auto keyURIArray = Uint16Array::create(initDataBuffer.copyRef(), 4, keyURI.length());
        keyURIArray->setRange(StringView(keyURI).upconvertedCharacters(), keyURI.length() / sizeof(unsigned char), 0);

        auto initData = Uint8Array::create(WTFMove(initDataBuffer), 0, byteLength);
        if (!player()->keyNeeded(initData.ptr()))
            return false;
#endif
        m_keyURIToRequestMap.set(keyURI, avRequest);
#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (m_cdmInstance)
            return false;

        RetainPtr<NSData> keyURIData = [keyURI dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES];
        m_keyID = SharedBuffer::create(keyURIData.get());
        player()->initializationDataEncountered("skd"_s, m_keyID->tryCreateArrayBuffer());
        setWaitingForKey(true);
#endif
        return true;
    }

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (scheme == "clearkey") {
        String keyID = [[[avRequest request] URL] resourceSpecifier];
        auto encodedKeyId = UTF8Encoding().encode(keyID, UnencodableHandling::URLEncodedEntities);

        auto initData = Uint8Array::create(encodedKeyId.size());
        initData->setRange(encodedKeyId.data(), encodedKeyId.size(), 0);

        auto keyData = player()->cachedKeyForKeyId(keyID);
        if (keyData) {
            fulfillRequestWithKeyData(avRequest, keyData.get());
            return false;
        }

        if (!player()->keyNeeded(initData.ptr()))
            return false;

        m_keyURIToRequestMap.set(keyID, avRequest);
        return true;
    }
#endif
#endif

    auto resourceLoader = WebCoreAVFResourceLoader::create(this, avRequest);
    m_resourceLoaderMap.add((__bridge CFTypeRef)avRequest, resourceLoader.copyRef());
    resourceLoader->startLoading();
    return true;
}

void MediaPlayerPrivateAVFoundationObjC::didCancelLoadingRequest(AVAssetResourceLoadingRequest* avRequest)
{
    String scheme = [[[avRequest request] URL] scheme];

    WebCoreAVFResourceLoader* resourceLoader = m_resourceLoaderMap.get((__bridge CFTypeRef)avRequest);

    if (resourceLoader)
        resourceLoader->stopLoading();
}

void MediaPlayerPrivateAVFoundationObjC::didStopLoadingRequest(AVAssetResourceLoadingRequest *avRequest)
{
    m_resourceLoaderMap.remove((__bridge CFTypeRef)avRequest);
}
#endif

bool MediaPlayerPrivateAVFoundationObjC::isAvailable()
{
    return AVFoundationLibrary() && isCoreMediaFrameworkAvailable();
}

MediaTime MediaPlayerPrivateAVFoundationObjC::mediaTimeForTimeValue(const MediaTime& timeValue) const
{
    if (!metaDataAvailable())
        return timeValue;

    // FIXME - impossible to implement until rdar://8721510 is fixed.
    return timeValue;
}

double MediaPlayerPrivateAVFoundationObjC::maximumDurationToCacheMediaTime() const
{
    return 0;
}

void MediaPlayerPrivateAVFoundationObjC::updateVideoLayerGravity()
{
    if (!m_videoLayer)
        return;

    // Do not attempt to change the video gravity while in full screen mode.
    // See setVideoFullscreenGravity().
    if (m_videoFullscreenLayerManager->videoFullscreenLayer())
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];    
    NSString* gravity = shouldMaintainAspectRatio() ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize;
    [m_videoLayer.get() setVideoGravity:gravity];
    [CATransaction commit];
}

static AVAssetTrack* firstEnabledTrack(NSArray* tracks)
{
    NSUInteger index = [tracks indexOfObjectPassingTest:^(id obj, NSUInteger, BOOL *) {
        return [static_cast<AVAssetTrack*>(obj) isEnabled];
    }];
    if (index == NSNotFound)
        return nil;
    return [tracks objectAtIndex:index];
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
        AVAssetTrack* firstEnabledVideoTrack = firstEnabledTrack([m_avAsset.get() tracksWithMediaCharacteristic:AVMediaCharacteristicVisual]);
        setHasVideo(firstEnabledVideoTrack);
        setHasAudio(firstEnabledTrack([m_avAsset.get() tracksWithMediaCharacteristic:AVMediaCharacteristicAudible]));
#if !HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        hasCaptions = [[m_avAsset.get() tracksWithMediaType:AVMediaTypeClosedCaption] count];
#endif
        auto size = firstEnabledVideoTrack ? FloatSize(CGSizeApplyAffineTransform([firstEnabledVideoTrack naturalSize], [firstEnabledVideoTrack preferredTransform])) : FloatSize();
        // For videos with rotation tag set, the transformation above might return a CGSize instance with negative width or height.
        // See https://bugs.webkit.org/show_bug.cgi?id=172648.
        if (size.width() < 0)
            size.setWidth(-size.width());
        if (size.height() < 0)
            size.setHeight(-size.height());
        presentationSizeDidChange(size);
    } else {
        bool hasVideo = false;
        bool hasAudio = false;
        bool hasMetaData = false;
        for (AVPlayerItemTrack *track in m_cachedTracks.get()) {
            if ([track isEnabled]) {
                AVAssetTrack *assetTrack = [track assetTrack];
                NSString *mediaType = [assetTrack mediaType];
                if ([mediaType isEqualToString:AVMediaTypeVideo])
                    hasVideo = true;
                else if ([mediaType isEqualToString:AVMediaTypeAudio])
                    hasAudio = true;
                else if ([mediaType isEqualToString:AVMediaTypeClosedCaption]) {
#if !HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
                    hasCaptions = true;
#endif
                    haveCCTrack = true;
                } else if ([mediaType isEqualToString:AVMediaTypeMetadata]) {
                    hasMetaData = true;
                }
            }
        }

#if ENABLE(VIDEO_TRACK)
        updateAudioTracks();
        updateVideoTracks();

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        hasAudio |= (m_audibleGroup && m_audibleGroup->selectedOption());
        hasVideo |= (m_visualGroup && m_visualGroup->selectedOption());
#endif
#endif

        // Always says we have video if the AVPlayerLayer is ready for diaplay to work around
        // an AVFoundation bug which causes it to sometimes claim a track is disabled even
        // when it is not.
        setHasVideo(hasVideo || m_cachedIsReadyForDisplay);

        setHasAudio(hasAudio);
#if ENABLE(DATACUE_VALUE)
        if (hasMetaData)
            processMetadataTrack();
#endif
    }

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    AVMediaSelectionGroupType *legibleGroup = safeMediaSelectionGroupForLegibleMedia();
    if (legibleGroup && m_cachedTracks) {
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

    INFO_LOG(LOGIDENTIFIER, "has video = ", hasVideo(), ", has audio = ", hasAudio(), ", has captions = ", hasClosedCaptions());

    sizeChanged();

    if (primaryAudioTrackLanguage != languageOfPrimaryAudioTrack())
        characteristicsChanged();

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    if (m_provider)
        m_provider->setAudioTrack(firstEnabledTrack(safeAVAssetTracksForAudibleMedia()));
#endif

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

    for (auto& oldItem : oldItems) {
        if (oldItem->playerItemTrack())
            [oldTracks addObject:oldItem->playerItemTrack()];
    }

    // Find the added & removed AVPlayerItemTracks:
    RetainPtr<NSMutableSet> removedTracks = adoptNS([oldTracks mutableCopy]);
    [removedTracks minusSet:newTracks.get()];

    RetainPtr<NSMutableSet> addedTracks = adoptNS([newTracks mutableCopy]);
    [addedTracks minusSet:oldTracks.get()];

    typedef Vector<RefT> ItemVector;
    ItemVector replacementItems;
    ItemVector addedItems;
    ItemVector removedItems;
    for (auto& oldItem : oldItems) {
        if (oldItem->playerItemTrack() && [removedTracks containsObject:oldItem->playerItemTrack()])
            removedItems.append(oldItem);
        else
            replacementItems.append(oldItem);
    }

    for (AVPlayerItemTrack* track in addedTracks.get())
        addedItems.append(itemFactory(track));

    replacementItems.appendVector(addedItems);
    oldItems.swap(replacementItems);

    for (auto& removedItem : removedItems)
        (player->*removedFunction)(*removedItem);

    for (auto& addedItem : addedItems)
        (player->*addedFunction)(*addedItem);
}

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

template <typename RefT, typename PassRefT>
void determineChangedTracksFromNewTracksAndOldItems(MediaSelectionGroupAVFObjC* group, Vector<RefT>& oldItems, const Vector<String>& characteristics, RefT (*itemFactory)(MediaSelectionOptionAVFObjC&), MediaPlayer* player, void (MediaPlayer::*removedFunction)(PassRefT), void (MediaPlayer::*addedFunction)(PassRefT))
{
    group->updateOptions(characteristics);

    ListHashSet<RefPtr<MediaSelectionOptionAVFObjC>> newSelectionOptions;
    for (auto& option : group->options()) {
        if (!option)
            continue;
        AVMediaSelectionOptionType* avOption = option->avMediaSelectionOption();
        if (!avOption)
            continue;
        newSelectionOptions.add(option);
    }

    ListHashSet<RefPtr<MediaSelectionOptionAVFObjC>> oldSelectionOptions;
    for (auto& oldItem : oldItems) {
        if (MediaSelectionOptionAVFObjC *option = oldItem->mediaSelectionOption())
            oldSelectionOptions.add(option);
    }

    // Find the added & removed AVMediaSelectionOptions:
    ListHashSet<RefPtr<MediaSelectionOptionAVFObjC>> removedSelectionOptions;
    for (auto& oldOption : oldSelectionOptions) {
        if (!newSelectionOptions.contains(oldOption))
            removedSelectionOptions.add(oldOption);
    }

    ListHashSet<RefPtr<MediaSelectionOptionAVFObjC>> addedSelectionOptions;
    for (auto& newOption : newSelectionOptions) {
        if (!oldSelectionOptions.contains(newOption))
            addedSelectionOptions.add(newOption);
    }

    typedef Vector<RefT> ItemVector;
    ItemVector replacementItems;
    ItemVector addedItems;
    ItemVector removedItems;
    for (auto& oldItem : oldItems) {
        if (!oldItem->mediaSelectionOption())
            removedItems.append(oldItem);
        else if (removedSelectionOptions.contains(oldItem->mediaSelectionOption()))
            removedItems.append(oldItem);
        else
            replacementItems.append(oldItem);
    }

    for (auto& option : addedSelectionOptions)
        addedItems.append(itemFactory(*option.get()));

    replacementItems.appendVector(addedItems);
    oldItems.swap(replacementItems);
    
    for (auto& removedItem : removedItems)
        (player->*removedFunction)(*removedItem);

    for (auto& addedItem : addedItems)
        (player->*addedFunction)(*addedItem);
}

#endif

void MediaPlayerPrivateAVFoundationObjC::updateAudioTracks()
{
#if !RELEASE_LOG_DISABLED
    size_t count = m_audioTracks.size();
#endif

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    Vector<String> characteristics = player()->preferredAudioCharacteristics();
    if (!m_audibleGroup) {
        if (AVMediaSelectionGroupType *group = safeMediaSelectionGroupForAudibleMedia())
            m_audibleGroup = MediaSelectionGroupAVFObjC::create(m_avPlayerItem.get(), group, characteristics);
    }

    if (m_audibleGroup)
        determineChangedTracksFromNewTracksAndOldItems(m_audibleGroup.get(), m_audioTracks, characteristics, &AudioTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeAudioTrack, &MediaPlayer::addAudioTrack);
    else
#endif
        determineChangedTracksFromNewTracksAndOldItems(m_cachedTracks.get(), AVMediaTypeAudio, m_audioTracks, &AudioTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeAudioTrack, &MediaPlayer::addAudioTrack);

    for (auto& track : m_audioTracks)
        track->resetPropertiesFromTrack();

#if !RELEASE_LOG_DISABLED
    INFO_LOG(LOGIDENTIFIER, "track count was ", count, ", is ", m_audioTracks.size());
#endif
}

void MediaPlayerPrivateAVFoundationObjC::updateVideoTracks()
{
#if !RELEASE_LOG_DISABLED
    size_t count = m_videoTracks.size();
#endif

    determineChangedTracksFromNewTracksAndOldItems(m_cachedTracks.get(), AVMediaTypeVideo, m_videoTracks, &VideoTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeVideoTrack, &MediaPlayer::addVideoTrack);

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    if (!m_visualGroup) {
        if (AVMediaSelectionGroupType *group = safeMediaSelectionGroupForVisualMedia())
            m_visualGroup = MediaSelectionGroupAVFObjC::create(m_avPlayerItem.get(), group, Vector<String>());
    }

    if (m_visualGroup)
        determineChangedTracksFromNewTracksAndOldItems(m_visualGroup.get(), m_videoTracks, Vector<String>(), &VideoTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeVideoTrack, &MediaPlayer::addVideoTrack);
#endif

    for (auto& track : m_audioTracks)
        track->resetPropertiesFromTrack();

#if !RELEASE_LOG_DISABLED
    INFO_LOG(LOGIDENTIFIER, "track count was ", count, ", is ", m_videoTracks.size());
#endif
}

bool MediaPlayerPrivateAVFoundationObjC::requiresTextTrackRepresentation() const
{
    return m_videoFullscreenLayerManager->requiresTextTrackRepresentation();
}

void MediaPlayerPrivateAVFoundationObjC::syncTextTrackBounds()
{
    m_videoFullscreenLayerManager->syncTextTrackBounds();
}

void MediaPlayerPrivateAVFoundationObjC::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    m_videoFullscreenLayerManager->setTextTrackRepresentation(representation);
}

#endif // ENABLE(VIDEO_TRACK)

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)

AudioSourceProvider* MediaPlayerPrivateAVFoundationObjC::audioSourceProvider()
{
    if (!m_provider) {
        m_provider = AudioSourceProviderAVFObjC::create(m_avPlayerItem.get());
        m_provider->setAudioTrack(firstEnabledTrack(safeAVAssetTracksForAudibleMedia()));
    }
    return m_provider.get();
}

#endif

void MediaPlayerPrivateAVFoundationObjC::sizeChanged()
{
    if (!m_avAsset)
        return;

    setNaturalSize(m_cachedPresentationSize);
}

void MediaPlayerPrivateAVFoundationObjC::resolvedURLChanged()
{
    setResolvedURL(m_avAsset ? URL([m_avAsset resolvedURL]) : URL());
}

bool MediaPlayerPrivateAVFoundationObjC::didPassCORSAccessCheck() const
{
    AVAssetResourceLoader *resourceLoader = m_avAsset.get().resourceLoader;
    if (!DeprecatedGlobalSettings::isAVFoundationNSURLSessionEnabled()
        || ![resourceLoader respondsToSelector:@selector(URLSession)])
        return false;

    WebCoreNSURLSession *session = (WebCoreNSURLSession *)resourceLoader.URLSession;
    if ([session isKindOfClass:[WebCoreNSURLSession class]])
        return session.didPassCORSAccessChecks;

    return false;
}

Optional<bool> MediaPlayerPrivateAVFoundationObjC::wouldTaintOrigin(const SecurityOrigin& origin) const
{
    AVAssetResourceLoader *resourceLoader = m_avAsset.get().resourceLoader;
    if (!DeprecatedGlobalSettings::isAVFoundationNSURLSessionEnabled()
        || ![resourceLoader respondsToSelector:@selector(URLSession)])
        return false;

    WebCoreNSURLSession *session = (WebCoreNSURLSession *)resourceLoader.URLSession;
    if ([session isKindOfClass:[WebCoreNSURLSession class]])
        return [session wouldTaintOrigin:origin];

    return WTF::nullopt;
}


#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)

void MediaPlayerPrivateAVFoundationObjC::createVideoOutput()
{
    INFO_LOG(LOGIDENTIFIER);

    if (!m_avPlayerItem || m_videoOutput)
        return;

#if USE(VIDEOTOOLBOX)
    NSDictionary* attributes = nil;
#else
    NSDictionary* attributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithUnsignedInt:kCVPixelFormatType_32BGRA], kCVPixelBufferPixelFormatTypeKey, nil];
#endif
    m_videoOutput = adoptNS([[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:attributes]);
    ASSERT(m_videoOutput);

    [m_videoOutput setDelegate:m_videoOutputDelegate.get() queue:globalPullDelegateQueue()];

    [m_avPlayerItem.get() addOutput:m_videoOutput.get()];
}

void MediaPlayerPrivateAVFoundationObjC::destroyVideoOutput()
{
    if (!m_videoOutput)
        return;

    if (m_avPlayerItem)
        [m_avPlayerItem.get() removeOutput:m_videoOutput.get()];

    INFO_LOG(LOGIDENTIFIER);

    m_videoOutput = 0;
}

bool MediaPlayerPrivateAVFoundationObjC::updateLastPixelBuffer()
{
    if (!m_avPlayerItem)
        return false;

    if (!m_videoOutput)
        createVideoOutput();
    ASSERT(m_videoOutput);

    CMTime currentTime = [m_avPlayerItem.get() currentTime];

    if (![m_videoOutput.get() hasNewPixelBufferForItemTime:currentTime])
        return false;

    m_lastPixelBuffer = adoptCF([m_videoOutput.get() copyPixelBufferForItemTime:currentTime itemTimeForDisplay:nil]);
    m_lastImage = nullptr;
    return true;
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

void MediaPlayerPrivateAVFoundationObjC::updateLastImage(UpdateType type)
{
#if HAVE(CORE_VIDEO)
    if (!m_avPlayerItem)
        return;

    if (type == UpdateType::UpdateSynchronously && !m_lastImage && !videoOutputHasAvailableFrame())
        waitForVideoOutputMediaDataWillChange();

    // Calls to copyPixelBufferForItemTime:itemTimeForDisplay: may return nil if the pixel buffer
    // for the requested time has already been retrieved. In this case, the last valid image (if any)
    // should be displayed.
    if (!updateLastPixelBuffer() && (m_lastImage || !m_lastPixelBuffer))
        return;

    if (!m_pixelBufferConformer) {
#if USE(VIDEOTOOLBOX)
        NSDictionary *attributes = @{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
#else
        NSDictionary *attributes = nil;
#endif
        m_pixelBufferConformer = std::make_unique<PixelBufferConformerCV>((__bridge CFDictionaryRef)attributes);
    }

#if !RELEASE_LOG_DISABLED
    MonotonicTime start = MonotonicTime::now();
#endif

    m_lastImage = m_pixelBufferConformer->createImageFromPixelBuffer(m_lastPixelBuffer.get());

#if !RELEASE_LOG_DISABLED
    DEBUG_LOG(LOGIDENTIFIER, "creating buffer took ", (MonotonicTime::now() - start).seconds());
#endif
#endif // HAVE(CORE_VIDEO)
}

void MediaPlayerPrivateAVFoundationObjC::paintWithVideoOutput(GraphicsContext& context, const FloatRect& outputRect)
{
    updateLastImage(UpdateType::UpdateSynchronously);
    if (!m_lastImage)
        return;

    AVAssetTrack* firstEnabledVideoTrack = firstEnabledTrack([m_avAsset.get() tracksWithMediaCharacteristic:AVMediaCharacteristicVisual]);
    if (!firstEnabledVideoTrack)
        return;

    INFO_LOG(LOGIDENTIFIER);

    GraphicsContextStateSaver stateSaver(context);
    FloatRect imageRect(0, 0, CGImageGetWidth(m_lastImage.get()), CGImageGetHeight(m_lastImage.get()));
    AffineTransform videoTransform = [firstEnabledVideoTrack preferredTransform];
    FloatRect transformedOutputRect = videoTransform.inverse().valueOr(AffineTransform()).mapRect(outputRect);

    context.concatCTM(videoTransform);
    context.drawNativeImage(m_lastImage.get(), imageRect.size(), transformedOutputRect, imageRect);

    // If we have created an AVAssetImageGenerator in the past due to m_videoOutput not having an available
    // video frame, destroy it now that it is no longer needed.
    if (m_imageGenerator)
        destroyImageGenerator();

}

bool MediaPlayerPrivateAVFoundationObjC::copyVideoTextureToPlatformTexture(GraphicsContext3D* context, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY)
{
    ASSERT(context);

    updateLastPixelBuffer();
    if (!m_lastPixelBuffer)
        return false;

    size_t width = CVPixelBufferGetWidth(m_lastPixelBuffer.get());
    size_t height = CVPixelBufferGetHeight(m_lastPixelBuffer.get());

    if (!m_videoTextureCopier)
        m_videoTextureCopier = std::make_unique<VideoTextureCopierCV>(*context);

    return m_videoTextureCopier->copyImageToPlatformTexture(m_lastPixelBuffer.get(), width, height, outputTexture, outputTarget, level, internalFormat, format, type, premultiplyAlpha, flipY);
}

NativeImagePtr MediaPlayerPrivateAVFoundationObjC::nativeImageForCurrentTime()
{
    updateLastImage();
    return m_lastImage;
}

void MediaPlayerPrivateAVFoundationObjC::waitForVideoOutputMediaDataWillChange()
{
    [m_videoOutput requestNotificationOfMediaDataChangeWithAdvanceInterval:0];

    // Wait for 1 second.
    bool satisfied = m_videoOutputSemaphore.waitFor(1_s);
    if (!satisfied)
        ERROR_LOG(LOGIDENTIFIER, "timed out");
}

void MediaPlayerPrivateAVFoundationObjC::outputMediaDataWillChange(AVPlayerItemVideoOutputType *)
{
    m_videoOutputSemaphore.signal();
}

#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

RetainPtr<AVAssetResourceLoadingRequest> MediaPlayerPrivateAVFoundationObjC::takeRequestForKeyURI(const String& keyURI)
{
    return m_keyURIToRequestMap.take(keyURI);
}

void MediaPlayerPrivateAVFoundationObjC::keyAdded()
{
    Vector<String> fulfilledKeyIds;

    for (auto& pair : m_keyURIToRequestMap) {
        const String& keyId = pair.key;
        const RetainPtr<AVAssetResourceLoadingRequest>& request = pair.value;

        auto keyData = player()->cachedKeyForKeyId(keyId);
        if (!keyData)
            continue;

        fulfillRequestWithKeyData(request.get(), keyData.get());
        fulfilledKeyIds.append(keyId);
    }

    for (auto& keyId : fulfilledKeyIds)
        m_keyURIToRequestMap.remove(keyId);
}

void MediaPlayerPrivateAVFoundationObjC::removeSession(LegacyCDMSession& session)
{
    ASSERT_UNUSED(session, &session == m_session);
    m_session = nullptr;
}

std::unique_ptr<LegacyCDMSession> MediaPlayerPrivateAVFoundationObjC::createSession(const String& keySystem, LegacyCDMSessionClient* client)
{
    if (!keySystemIsSupported(keySystem))
        return nullptr;
    auto session = std::make_unique<CDMSessionAVFoundationObjC>(this, client);
    m_session = makeWeakPtr(*session);
    return WTFMove(session);
}
#endif

#if ENABLE(ENCRYPTED_MEDIA) || ENABLE(LEGACY_ENCRYPTED_MEDIA)
void MediaPlayerPrivateAVFoundationObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool newValue)
{
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (m_session && newValue)
        m_session->playerDidReceiveError([NSError errorWithDomain:@"com.apple.WebKit" code:'HDCP' userInfo:nil]);
#endif

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (m_cdmInstance)
        m_cdmInstance->outputObscuredDueToInsufficientExternalProtectionChanged(newValue);
#elif !ENABLE(LEGACY_ENCRYPTED_MEDIA)
    UNUSED_PARAM(newValue);
#endif
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayerPrivateAVFoundationObjC::cdmInstanceAttached(CDMInstance& instance)
{
#if HAVE(AVCONTENTKEYSESSION)
    if (!is<CDMInstanceFairPlayStreamingAVFObjC>(instance))
        return;

    auto& fpsInstance = downcast<CDMInstanceFairPlayStreamingAVFObjC>(instance);
    if (&fpsInstance == m_cdmInstance)
        return;

    if (m_cdmInstance)
        cdmInstanceDetached(*m_cdmInstance);

    m_cdmInstance = &fpsInstance;
#else
    UNUSED_PARAM(instance);
#endif
}

void MediaPlayerPrivateAVFoundationObjC::cdmInstanceDetached(CDMInstance& instance)
{
#if HAVE(AVCONTENTKEYSESSION)
    ASSERT_UNUSED(instance, m_cdmInstance && m_cdmInstance == &instance);
    m_cdmInstance = nullptr;
#else
    UNUSED_PARAM(instance);
#endif
}

void MediaPlayerPrivateAVFoundationObjC::attemptToDecryptWithInstance(CDMInstance&)
{
#if HAVE(AVCONTENTKEYSESSION)
    if (!m_keyID || !m_cdmInstance)
        return;

    auto instanceSession = m_cdmInstance->sessionForKeyIDs(Vector<Ref<SharedBuffer>>::from(*m_keyID));
    if (!instanceSession)
        return;

    [instanceSession->contentKeySession() addContentKeyRecipient:m_avAsset.get()];

    auto keyURIToRequestMap = WTFMove(m_keyURIToRequestMap);
    for (auto& request : keyURIToRequestMap.values()) {
        if (auto *infoRequest = request.get().contentInformationRequest)
            infoRequest.contentType = AVStreamingKeyDeliveryContentKeyType;
        [request finishLoading];
    }
    setWaitingForKey(false);
#endif
}

void MediaPlayerPrivateAVFoundationObjC::setWaitingForKey(bool waitingForKey)
{
    if (m_waitingForKey == waitingForKey)
        return;

    m_waitingForKey = waitingForKey;
    player()->waitingForKeyChanged();
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
            if (removedTextTracks[i - 1]->textTrackCategory() != InbandTextTrackPrivateAVF::LegacyClosedCaption)
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

NSArray* MediaPlayerPrivateAVFoundationObjC::safeAVAssetTracksForAudibleMedia()
{
    if (!m_avAsset)
        return nil;

    if ([m_avAsset.get() statusOfValueForKey:@"tracks" error:NULL] != AVKeyValueStatusLoaded)
        return nil;

    return [m_avAsset tracksWithMediaCharacteristic:AVMediaCharacteristicAudible];
}

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

bool MediaPlayerPrivateAVFoundationObjC::hasLoadedMediaSelectionGroups()
{
    if (!m_avAsset)
        return false;

    if ([m_avAsset.get() statusOfValueForKey:@"availableMediaCharacteristicsWithMediaSelectionOptions" error:NULL] != AVKeyValueStatusLoaded)
        return false;

    return true;
}

AVMediaSelectionGroupType* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForLegibleMedia()
{
    if (!hasLoadedMediaSelectionGroups())
        return nil;

    return [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicLegible];
}

AVMediaSelectionGroupType* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForAudibleMedia()
{
    if (!hasLoadedMediaSelectionGroups())
        return nil;

    return [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicAudible];
}

AVMediaSelectionGroupType* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForVisualMedia()
{
    if (!hasLoadedMediaSelectionGroups())
        return nil;

    return [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicVisual];
}

void MediaPlayerPrivateAVFoundationObjC::processMediaSelectionOptions()
{
    AVMediaSelectionGroupType *legibleGroup = safeMediaSelectionGroupForLegibleMedia();
    if (!legibleGroup) {
        INFO_LOG(LOGIDENTIFIER, "no mediaSelectionGroup");
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
            if (removedTextTracks[i - 1]->textTrackCategory() == InbandTextTrackPrivateAVF::LegacyClosedCaption)
                continue;
            
            RetainPtr<AVMediaSelectionOptionType> currentOption;
#if ENABLE(AVF_CAPTIONS)
            if (removedTextTracks[i - 1]->textTrackCategory() == InbandTextTrackPrivateAVF::OutOfBand) {
                RefPtr<OutOfBandTextTrackPrivateAVF> track = static_cast<OutOfBandTextTrackPrivateAVF*>(removedTextTracks[i - 1].get());
                currentOption = track->mediaSelectionOption();
            } else
#endif
            {
                RefPtr<InbandTextTrackPrivateAVFObjC> track = static_cast<InbandTextTrackPrivateAVFObjC*>(removedTextTracks[i - 1].get());
                currentOption = track->mediaSelectionOption();
            }
            
            if ([currentOption.get() isEqual:option]) {
                removedTextTracks.remove(i - 1);
                newTrack = false;
                break;
            }
        }
        if (!newTrack)
            continue;

#if ENABLE(AVF_CAPTIONS)
        if ([option outOfBandSource]) {
            m_textTracks.append(OutOfBandTextTrackPrivateAVF::create(this, option));
            m_textTracks.last()->setHasBeenReported(true); // Ignore out-of-band tracks that we passed to AVFoundation so we do not double-count them
            continue;
        }
#endif

        m_textTracks.append(InbandTextTrackPrivateAVFObjC::create(this, option, InbandTextTrackPrivate::Generic));
    }

    processNewAndRemovedTextTracks(removedTextTracks);
}

void MediaPlayerPrivateAVFoundationObjC::processMetadataTrack()
{
    if (m_metadataTrack)
        return;

    m_metadataTrack = InbandMetadataTextTrackPrivateAVF::create(InbandTextTrackPrivate::Metadata, InbandTextTrackPrivate::Data);
    m_metadataTrack->setInBandMetadataTrackDispatchType("com.apple.streaming");
    player()->addTextTrack(*m_metadataTrack);
}

void MediaPlayerPrivateAVFoundationObjC::processCue(NSArray *attributedStrings, NSArray *nativeSamples, const MediaTime& time)
{
    ASSERT(time >= MediaTime::zeroTime());

    if (!m_currentTextTrack)
        return;

    m_currentTextTrack->processCue((__bridge CFArrayRef)attributedStrings, (__bridge CFArrayRef)nativeSamples, time);
}

void MediaPlayerPrivateAVFoundationObjC::flushCues()
{
    INFO_LOG(LOGIDENTIFIER);

    if (!m_currentTextTrack)
        return;
    
    m_currentTextTrack->resetCueValues();
}

#endif // HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

void MediaPlayerPrivateAVFoundationObjC::setCurrentTextTrack(InbandTextTrackPrivateAVF *track)
{
    if (m_currentTextTrack == track)
        return;

    INFO_LOG(LOGIDENTIFIER, "selecting track with language ", track ? track->language() : "");

    m_currentTextTrack = track;

    if (track) {
        if (track->textTrackCategory() == InbandTextTrackPrivateAVF::LegacyClosedCaption)
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [m_avPlayer.get() setClosedCaptionDisplayEnabled:YES];
            ALLOW_DEPRECATED_DECLARATIONS_END
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
#if ENABLE(AVF_CAPTIONS)
        else if (track->textTrackCategory() == InbandTextTrackPrivateAVF::OutOfBand)
            [m_avPlayerItem.get() selectMediaOption:static_cast<OutOfBandTextTrackPrivateAVF*>(track)->mediaSelectionOption() inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
#endif
        else
            [m_avPlayerItem.get() selectMediaOption:static_cast<InbandTextTrackPrivateAVFObjC*>(track)->mediaSelectionOption() inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
#endif
    } else {
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
        [m_avPlayerItem.get() selectMediaOption:0 inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
#endif
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [m_avPlayer.get() setClosedCaptionDisplayEnabled:NO];
        ALLOW_DEPRECATED_DECLARATIONS_END
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
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    AVMediaSelectionOptionType *currentlySelectedAudibleOption = [m_avPlayerItem.get() selectedMediaOptionInMediaSelectionGroup:audibleGroup];
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (currentlySelectedAudibleOption) {
        m_languageOfPrimaryAudioTrack = [[currentlySelectedAudibleOption locale] localeIdentifier];
        INFO_LOG(LOGIDENTIFIER, "language of selected audible option ", m_languageOfPrimaryAudioTrack);

        return m_languageOfPrimaryAudioTrack;
    }
#endif // HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

    // AVFoundation synthesizes an audible group when there is only one ungrouped audio track if there is also a legible group (one or
    // more in-band text tracks). It doesn't know about out-of-band tracks, so if there is a single audio track return its language.
    NSArray *tracks = [m_avAsset.get() tracksWithMediaType:AVMediaTypeAudio];
    if (!tracks || [tracks count] != 1) {
        m_languageOfPrimaryAudioTrack = emptyString();
        INFO_LOG(LOGIDENTIFIER, tracks ? [tracks count] : 0, " audio tracks, returning empty");
        return m_languageOfPrimaryAudioTrack;
    }

    AVAssetTrack *track = [tracks objectAtIndex:0];
    m_languageOfPrimaryAudioTrack = AVTrackPrivateAVFObjCImpl::languageForAVAssetTrack(track);

    INFO_LOG(LOGIDENTIFIER, "single audio track has language \"", m_languageOfPrimaryAudioTrack, "\"");

    return m_languageOfPrimaryAudioTrack;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
bool MediaPlayerPrivateAVFoundationObjC::isCurrentPlaybackTargetWireless() const
{
    bool wirelessTarget = false;

#if !PLATFORM(IOS_FAMILY)
    if (m_playbackTarget) {
        if (m_playbackTarget->targetType() == MediaPlaybackTarget::AVFoundation)
            wirelessTarget = m_avPlayer && m_avPlayer.get().externalPlaybackActive;
        else
            wirelessTarget = m_shouldPlayToPlaybackTarget && m_playbackTarget->hasActiveRoute();
    }
#else
    wirelessTarget = m_avPlayer && m_avPlayer.get().externalPlaybackActive;
#endif

    INFO_LOG(LOGIDENTIFIER, "- ", wirelessTarget);

    return wirelessTarget;
}

MediaPlayer::WirelessPlaybackTargetType MediaPlayerPrivateAVFoundationObjC::wirelessPlaybackTargetType() const
{
    if (!m_avPlayer)
        return MediaPlayer::TargetTypeNone;

#if PLATFORM(IOS_FAMILY)
    if (!AVFoundationLibrary())
        return MediaPlayer::TargetTypeNone;

    switch ([m_avPlayer externalPlaybackType]) {
    case AVPlayerExternalPlaybackTypeNone:
        return MediaPlayer::TargetTypeNone;
    case AVPlayerExternalPlaybackTypeAirPlay:
        return MediaPlayer::TargetTypeAirPlay;
    case AVPlayerExternalPlaybackTypeTVOut:
        return MediaPlayer::TargetTypeTVOut;
    }

    ASSERT_NOT_REACHED();
    return MediaPlayer::TargetTypeNone;

#else
    return MediaPlayer::TargetTypeAirPlay;
#endif
}
    
#if PLATFORM(IOS_FAMILY)
static NSString *exernalDeviceDisplayNameForPlayer(AVPlayerType *player)
{
#if HAVE(CELESTIAL)
    if (!AVFoundationLibrary())
        return nil;

    if ([getAVOutputContextClass() respondsToSelector:@selector(sharedAudioPresentationOutputContext)]) {
        AVOutputContext *outputContext = [getAVOutputContextClass() sharedAudioPresentationOutputContext];

        if (![outputContext respondsToSelector:@selector(supportsMultipleOutputDevices)]
            || ![outputContext supportsMultipleOutputDevices]
            || ![outputContext respondsToSelector:@selector(outputDevices)])
            return [outputContext deviceName];

        auto outputDeviceNames = adoptNS([[NSMutableArray alloc] init]);
        for (AVOutputDevice *outputDevice in [outputContext outputDevices]) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto outputDeviceName = adoptNS([[outputDevice name] copy]);
ALLOW_DEPRECATED_DECLARATIONS_END
            [outputDeviceNames addObject:outputDeviceName.get()];
        }

        return [outputDeviceNames componentsJoinedByString:@" + "];
    }

    if (player.externalPlaybackType != AVPlayerExternalPlaybackTypeAirPlay)
        return nil;

    NSArray *pickableRoutes = CFBridgingRelease(MRMediaRemoteCopyPickableRoutes());
    if (!pickableRoutes.count)
        return nil;

    NSString *displayName = nil;
    for (NSDictionary *pickableRoute in pickableRoutes) {
        if (![pickableRoute[AVController_RouteDescriptionKey_RouteCurrentlyPicked] boolValue])
            continue;

        displayName = pickableRoute[AVController_RouteDescriptionKey_RouteName];

        NSString *routeName = pickableRoute[AVController_RouteDescriptionKey_AVAudioRouteName];
        if (![routeName isEqualToString:@"Speaker"] && ![routeName isEqualToString:@"HDMIOutput"])
            break;

        // The route is a speaker or HDMI out, override the name to be the localized device model.
        NSString *localizedDeviceModel = [[UIDevice currentDevice] localizedModel];

        // In cases where a route with that name already exists, prefix the name with the model.
        BOOL includeLocalizedDeviceModelName = NO;
        for (NSDictionary *otherRoute in pickableRoutes) {
            if (otherRoute == pickableRoute)
                continue;

            if ([otherRoute[AVController_RouteDescriptionKey_RouteName] rangeOfString:displayName].location != NSNotFound) {
                includeLocalizedDeviceModelName = YES;
                break;
            }
        }

        if (includeLocalizedDeviceModelName)
            displayName =  [NSString stringWithFormat:@"%@ %@", localizedDeviceModel, displayName];
        else
            displayName = localizedDeviceModel;

        break;
    }

    return displayName;
#else
    UNUSED_PARAM(player);
    return nil;
#endif
}
#endif

String MediaPlayerPrivateAVFoundationObjC::wirelessPlaybackTargetName() const
{
    if (!m_avPlayer)
        return emptyString();

    String wirelessTargetName;
#if !PLATFORM(IOS_FAMILY)
    if (m_playbackTarget)
        wirelessTargetName = m_playbackTarget->deviceName();
#else
    wirelessTargetName = exernalDeviceDisplayNameForPlayer(m_avPlayer.get());
#endif

    return wirelessTargetName;
}

bool MediaPlayerPrivateAVFoundationObjC::wirelessVideoPlaybackDisabled() const
{
    if (!m_avPlayer)
        return !m_allowsWirelessVideoPlayback;

    m_allowsWirelessVideoPlayback = [m_avPlayer.get() allowsExternalPlayback];
    INFO_LOG(LOGIDENTIFIER, "- ", !m_allowsWirelessVideoPlayback);

    return !m_allowsWirelessVideoPlayback;
}

void MediaPlayerPrivateAVFoundationObjC::setWirelessVideoPlaybackDisabled(bool disabled)
{
    INFO_LOG(LOGIDENTIFIER, "- ", disabled);
    m_allowsWirelessVideoPlayback = !disabled;
    if (!m_avPlayer)
        return;

    setDelayCallbacks(true);
    [m_avPlayer.get() setAllowsExternalPlayback:!disabled];
    setDelayCallbacks(false);
}

#if !PLATFORM(IOS_FAMILY)

void MediaPlayerPrivateAVFoundationObjC::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& target)
{
    m_playbackTarget = WTFMove(target);

    m_outputContext = m_playbackTarget->targetType() == MediaPlaybackTarget::AVFoundation ? toMediaPlaybackTargetMac(m_playbackTarget.get())->outputContext() : nullptr;

    INFO_LOG(LOGIDENTIFIER);

    if (!m_playbackTarget->hasActiveRoute())
        setShouldPlayToPlaybackTarget(false);
}

void MediaPlayerPrivateAVFoundationObjC::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    if (m_shouldPlayToPlaybackTarget == shouldPlay)
        return;

    m_shouldPlayToPlaybackTarget = shouldPlay;

    if (!m_playbackTarget)
        return;

    INFO_LOG(LOGIDENTIFIER, "- ", shouldPlay);

    if (m_playbackTarget->targetType() == MediaPlaybackTarget::AVFoundation) {
        AVOutputContext *newContext = shouldPlay ? m_outputContext.get() : nil;

        if (!m_avPlayer)
            return;

        RetainPtr<AVOutputContext> currentContext = m_avPlayer.get().outputContext;
        if ((!newContext && !currentContext.get()) || [currentContext.get() isEqual:newContext])
            return;

        setDelayCallbacks(true);
        m_avPlayer.get().outputContext = newContext;
        setDelayCallbacks(false);

        return;
    }

    ASSERT(m_playbackTarget->targetType() == MediaPlaybackTarget::Mock);

    setDelayCallbacks(true);
    auto weakThis = makeWeakPtr(*this);
    scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification([weakThis] {
        if (!weakThis)
            return;
        weakThis->playbackTargetIsWirelessDidChange();
    }));
    setDelayCallbacks(false);
}

#endif // !PLATFORM(IOS_FAMILY)

void MediaPlayerPrivateAVFoundationObjC::updateDisableExternalPlayback()
{
#if PLATFORM(IOS_FAMILY)
    if (!m_avPlayer)
        return;

    if ([m_avPlayer respondsToSelector:@selector(setUsesExternalPlaybackWhileExternalScreenIsActive:)])
        [m_avPlayer setUsesExternalPlaybackWhileExternalScreenIsActive:(player()->fullscreenMode() == MediaPlayer::VideoFullscreenModeStandard) || player()->isVideoFullscreenStandby()];
#endif
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

void MediaPlayerPrivateAVFoundationObjC::seekableTimeRangesDidChange(RetainPtr<NSArray>&& seekableRanges)
{
    m_cachedSeekableRanges = WTFMove(seekableRanges);

    seekableTimeRangesChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::loadedTimeRangesDidChange(RetainPtr<NSArray>&& loadedRanges)
{
    m_cachedLoadedRanges = WTFMove(loadedRanges);

    loadedTimeRangesChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::firstFrameAvailableDidChange(bool isReady)
{
    m_cachedIsReadyForDisplay = isReady;
    if (!hasVideo() && isReady)
        tracksChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::trackEnabledDidChange(bool)
{
    tracksChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::setShouldBufferData(bool shouldBuffer)
{
    INFO_LOG(LOGIDENTIFIER, "- ", shouldBuffer);

    if (m_shouldBufferData == shouldBuffer)
        return;

    m_shouldBufferData = shouldBuffer;
    
    if (!m_avPlayer)
        return;

    setAVPlayerItem(shouldBuffer ? m_avPlayerItem.get() : nil);
    updateStates();
}

#if ENABLE(DATACUE_VALUE)

static const AtomicString& metadataType(NSString *avMetadataKeySpace)
{
    static NeverDestroyed<const AtomicString> quickTimeUserData("com.apple.quicktime.udta", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> isoUserData("org.mp4ra", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> quickTimeMetadata("com.apple.quicktime.mdta", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> iTunesMetadata("com.apple.itunes", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> id3Metadata("org.id3", AtomicString::ConstructFromLiteral);

    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceQuickTimeUserData])
        return quickTimeUserData;
    if (canLoadAVMetadataKeySpaceISOUserData() && [avMetadataKeySpace isEqualToString:AVMetadataKeySpaceISOUserData])
        return isoUserData;
    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceQuickTimeMetadata])
        return quickTimeMetadata;
    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceiTunes])
        return iTunesMetadata;
    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceID3])
        return id3Metadata;

    return emptyAtom();
}

#endif

void MediaPlayerPrivateAVFoundationObjC::metadataDidArrive(const RetainPtr<NSArray>& metadata, const MediaTime& mediaTime)
{
    m_currentMetaData = metadata && ![metadata isKindOfClass:[NSNull class]] ? metadata : nil;

    DEBUG_LOG(LOGIDENTIFIER, "adding ", m_currentMetaData ? [m_currentMetaData.get() count] : 0, " at time ", mediaTime);

#if ENABLE(DATACUE_VALUE)
    if (seeking())
        return;

    if (!m_metadataTrack)
        processMetadataTrack();

    if (!metadata || [metadata isKindOfClass:[NSNull class]]) {
        m_metadataTrack->updatePendingCueEndTimes(mediaTime);
        return;
    }

    // Set the duration of all incomplete cues before adding new ones.
    MediaTime earliestStartTime = MediaTime::positiveInfiniteTime();
    for (AVMetadataItemType *item in m_currentMetaData.get()) {
        MediaTime start = std::max(PAL::toMediaTime(item.time), MediaTime::zeroTime());
        if (start < earliestStartTime)
            earliestStartTime = start;
    }
    m_metadataTrack->updatePendingCueEndTimes(earliestStartTime);

    for (AVMetadataItemType *item in m_currentMetaData.get()) {
        MediaTime start = std::max(PAL::toMediaTime(item.time), MediaTime::zeroTime());
        MediaTime end = MediaTime::positiveInfiniteTime();
        if (CMTIME_IS_VALID(item.duration))
            end = start + PAL::toMediaTime(item.duration);

        AtomicString type = nullAtom();
        if (item.keySpace)
            type = metadataType(item.keySpace);

        m_metadataTrack->addDataCue(start, end, SerializedPlatformRepresentationMac::create(item), type);
    }
#endif
}

void MediaPlayerPrivateAVFoundationObjC::tracksDidChange(const RetainPtr<NSArray>& tracks)
{
    for (AVPlayerItemTrack *track in m_cachedTracks.get())
        [track removeObserver:m_objcObserver.get() forKeyPath:@"enabled"];

    NSArray *assetTracks = [m_avAsset tracks];

    m_cachedTracks = [tracks objectsAtIndexes:[tracks indexesOfObjectsPassingTest:^(id obj, NSUInteger, BOOL*) {
        AVAssetTrack* assetTrack = [obj assetTrack];

        if ([assetTracks containsObject:assetTrack])
            return YES;

        // Track is a streaming track. Omit if it belongs to a valid AVMediaSelectionGroup.
        if (!hasLoadedMediaSelectionGroups())
            return NO;

        if ([assetTrack hasMediaCharacteristic:AVMediaCharacteristicAudible] && safeMediaSelectionGroupForAudibleMedia())
            return NO;

        if ([assetTrack hasMediaCharacteristic:AVMediaCharacteristicVisual] && safeMediaSelectionGroupForVisualMedia())
            return NO;

        if ([assetTrack hasMediaCharacteristic:AVMediaCharacteristicLegible] && safeMediaSelectionGroupForLegibleMedia())
            return NO;

        return YES;
    }]];

    for (AVPlayerItemTrack *track in m_cachedTracks.get())
        [track addObserver:m_objcObserver.get() forKeyPath:@"enabled" options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextPlayerItemTrack];

    m_cachedTotalBytes = 0;

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

void MediaPlayerPrivateAVFoundationObjC::durationDidChange(const MediaTime& duration)
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

void MediaPlayerPrivateAVFoundationObjC::timeControlStatusDidChange(int timeControlStatus)
{
    if (m_cachedTimeControlStatus == timeControlStatus)
        return;

    if (!m_shouldObserveTimeControlStatus)
        return;

    m_cachedTimeControlStatus = timeControlStatus;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (!isCurrentPlaybackTargetWireless())
        return;

    bool playerIsPlaying = m_cachedTimeControlStatus != AVPlayerTimeControlStatusPaused;
    if (playerIsPlaying != m_requestedPlaying)
        player()->handlePlaybackCommand(playerIsPlaying ? PlatformMediaSession::PlayCommand : PlatformMediaSession::PauseCommand);
#endif
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

void MediaPlayerPrivateAVFoundationObjC::playbackTargetIsWirelessDidChange()
{
    playbackTargetIsWirelessChanged();
}

#endif

void MediaPlayerPrivateAVFoundationObjC::canPlayFastForwardDidChange(bool newValue)
{
    m_cachedCanPlayFastForward = newValue;
}

void MediaPlayerPrivateAVFoundationObjC::canPlayFastReverseDidChange(bool newValue)
{
    m_cachedCanPlayFastReverse = newValue;
}

void MediaPlayerPrivateAVFoundationObjC::setShouldDisableSleep(bool flag)
{
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(IOSMAC)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [m_avPlayer _setPreventsSleepDuringVideoPlayback:flag];
    ALLOW_DEPRECATED_DECLARATIONS_END
#else
    UNUSED_PARAM(flag);
#endif
}

Optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateAVFoundationObjC::videoPlaybackQualityMetrics()
{
    if (![m_videoLayer respondsToSelector:@selector(videoPerformanceMetrics)])
        return WTF::nullopt;

#if PLATFORM(WATCHOS)
    return WTF::nullopt;
#else
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN

    auto metrics = [m_videoLayer videoPerformanceMetrics];
    if (!metrics)
        return WTF::nullopt;

    uint32_t displayCompositedFrames = 0;
    if ([metrics respondsToSelector:@selector(numberOfDisplayCompositedVideoFrames)])
        displayCompositedFrames = [metrics numberOfDisplayCompositedVideoFrames];

    return VideoPlaybackQualityMetrics {
        static_cast<uint32_t>([metrics totalNumberOfVideoFrames]),
        static_cast<uint32_t>([metrics numberOfDroppedVideoFrames]),
        static_cast<uint32_t>([metrics numberOfCorruptedVideoFrames]),
        [metrics totalFrameDelay],
        displayCompositedFrames,
    };

    ALLOW_NEW_API_WITHOUT_GUARDS_END
#endif
}

bool MediaPlayerPrivateAVFoundationObjC::performTaskAtMediaTime(WTF::Function<void()>&& task, MediaTime time)
{
    if (!m_avPlayer)
        return false;

    __block WTF::Function<void()> taskIn = WTFMove(task);

    if (m_timeObserver)
        [m_avPlayer removeTimeObserver:m_timeObserver.get()];

    m_timeObserver = [m_avPlayer addBoundaryTimeObserverForTimes:@[[NSValue valueWithCMTime:toCMTime(time)]] queue:dispatch_get_main_queue() usingBlock:^{
        taskIn();
    }];
    return true;
}

void MediaPlayerPrivateAVFoundationObjC::setShouldObserveTimeControlStatus(bool shouldObserve)
{
    if (shouldObserve == m_shouldObserveTimeControlStatus)
        return;

    m_shouldObserveTimeControlStatus = shouldObserve;
    if (shouldObserve)
        [m_avPlayer addObserver:m_objcObserver.get() forKeyPath:@"timeControlStatus" options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextPlayer];
    else
        [m_avPlayer removeObserver:m_objcObserver.get() forKeyPath:@"timeControlStatus"];
}

NSArray* assetMetadataKeyNames()
{
    static NSArray* keys = [[NSArray alloc] initWithObjects:
        @"duration",
        @"naturalSize",
        @"preferredTransform",
        @"preferredVolume",
        @"preferredRate",
        @"playable",
        @"resolvedURL",
        @"tracks",
        @"availableMediaCharacteristicsWithMediaSelectionOptions",
    nil];
    return keys;
}

NSArray* itemKVOProperties()
{
    static NSArray* keys = [[NSArray alloc] initWithObjects:
        @"presentationSize",
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
        @"timedMetadata",
        @"canPlayFastForward",
        @"canPlayFastReverse",
    nil];
    return keys;
}

NSArray* assetTrackMetadataKeyNames()
{
    static NSArray* keys = [[NSArray alloc] initWithObjects:@"totalSampleDataLength", @"mediaType", @"enabled", @"preferredTransform", @"naturalSize", nil];
    return keys;
}

NSArray* playerKVOProperties()
{
    static NSArray* keys = [[NSArray alloc] initWithObjects:
        @"rate",
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        @"externalPlaybackActive",
        @"allowsExternalPlayback",
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
        @"outputObscuredDueToInsufficientExternalProtection",
#endif
    nil];
    return keys;
}
} // namespace WebCore

@implementation WebCoreAVFMovieObserver

- (id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)player
{
    self = [super init];
    if (!self)
        return nil;
    m_player = WTFMove(player);
    return self;
}

- (void)disconnect
{
    m_player = nullptr;
}

- (void)metadataLoaded
{
    m_taskQueue.enqueueTask([player = m_player] {
        if (player)
            player->metadataLoaded();
    });
}

- (void)didEnd:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    m_taskQueue.enqueueTask([player = m_player] {
        if (player)
            player->didEnd();
    });
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(MediaPlayerAVFoundationObservationContext)context
{
    m_taskQueue.enqueueTask([player = m_player, keyPath = retainPtr(keyPath), change = retainPtr(change), object = retainPtr(object), context] {
        if (!player)
            return;
        id newValue = [change valueForKey:NSKeyValueChangeNewKey];
        bool willChange = [[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue];
        bool shouldLogValue = !willChange;

        if (context == MediaPlayerAVFoundationObservationContextAVPlayerLayer) {
            if ([keyPath isEqualToString:@"readyForDisplay"])
                player->firstFrameAvailableDidChange([newValue boolValue]);
        }

        if (context == MediaPlayerAVFoundationObservationContextPlayerItemTrack) {
            if ([keyPath isEqualToString:@"enabled"])
                player->trackEnabledDidChange([newValue boolValue]);
        }

        if (context == MediaPlayerAVFoundationObservationContextPlayerItem && willChange) {
            if ([keyPath isEqualToString:@"playbackLikelyToKeepUp"])
                player->playbackLikelyToKeepUpWillChange();
            else if ([keyPath isEqualToString:@"playbackBufferEmpty"])
                player->playbackBufferEmptyWillChange();
            else if ([keyPath isEqualToString:@"playbackBufferFull"])
                player->playbackBufferFullWillChange();
        }

        if (context == MediaPlayerAVFoundationObservationContextPlayerItem && !willChange) {
            // A value changed for an AVPlayerItem
            if ([keyPath isEqualToString:@"status"])
                player->playerItemStatusDidChange([newValue intValue]);
            else if ([keyPath isEqualToString:@"playbackLikelyToKeepUp"])
                player->playbackLikelyToKeepUpDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"playbackBufferEmpty"])
                player->playbackBufferEmptyDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"playbackBufferFull"])
                player->playbackBufferFullDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"asset"]) {
                player->setAsset(RetainPtr<id>(newValue));
                shouldLogValue = false;
            } else if ([keyPath isEqualToString:@"loadedTimeRanges"])
                player->loadedTimeRangesDidChange(RetainPtr<NSArray>(newValue));
            else if ([keyPath isEqualToString:@"seekableTimeRanges"])
                player->seekableTimeRangesDidChange(RetainPtr<NSArray>(newValue));
            else if ([keyPath isEqualToString:@"tracks"]) {
                player->tracksDidChange(RetainPtr<NSArray>(newValue));
                shouldLogValue = false;
            } else if ([keyPath isEqualToString:@"hasEnabledAudio"])
                player->hasEnabledAudioDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"presentationSize"])
                player->presentationSizeDidChange(FloatSize([newValue sizeValue]));
            else if ([keyPath isEqualToString:@"duration"])
                player->durationDidChange(PAL::toMediaTime([newValue CMTimeValue]));
            else if ([keyPath isEqualToString:@"timedMetadata"] && newValue) {
                MediaTime now;
                CMTime itemTime = [(AVPlayerItemType *)object.get() currentTime];
                if (CMTIME_IS_NUMERIC(itemTime))
                    now = std::max(PAL::toMediaTime(itemTime), MediaTime::zeroTime());
                player->metadataDidArrive(RetainPtr<NSArray>(newValue), now);
                shouldLogValue = false;
            } else if ([keyPath isEqualToString:@"canPlayFastReverse"])
                player->canPlayFastReverseDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"canPlayFastForward"])
                player->canPlayFastForwardDidChange([newValue boolValue]);
        }

        if (context == MediaPlayerAVFoundationObservationContextPlayer && !willChange) {
            // A value changed for an AVPlayer.
            if ([keyPath isEqualToString:@"rate"])
                player->rateDidChange([newValue doubleValue]);
            else if ([keyPath isEqualToString:@"timeControlStatus"])
                player->timeControlStatusDidChange([newValue intValue]);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
            else if ([keyPath isEqualToString:@"externalPlaybackActive"] || [keyPath isEqualToString:@"allowsExternalPlayback"])
                player->playbackTargetIsWirelessDidChange();
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
            else if ([keyPath isEqualToString:@"outputObscuredDueToInsufficientExternalProtection"])
                player->outputObscuredDueToInsufficientExternalProtectionChanged([newValue boolValue]);
#endif
        }

#if !RELEASE_LOG_DISABLED
        if (player->logger().willLog(player->logChannel(), WTFLogLevelDebug) && !([keyPath isEqualToString:@"loadedTimeRanges"] || [keyPath isEqualToString:@"seekableTimeRanges"])) {
            auto identifier = Logger::LogSiteIdentifier("MediaPlayerPrivateAVFoundation", "observeValueForKeyPath", player->logIdentifier());

            if (shouldLogValue) {
                if ([keyPath isEqualToString:@"duration"])
                    player->logger().debug(player->logChannel(), identifier, "did change '", [keyPath UTF8String], "' to ", PAL::toMediaTime([newValue CMTimeValue]));
                else {
                    RetainPtr<NSString> valueString = adoptNS([[NSString alloc] initWithFormat:@"%@", newValue]);
                    player->logger().debug(player->logChannel(), identifier, "did change '", [keyPath UTF8String], "' to ", [valueString.get() UTF8String]);
                }
            } else
                player->logger().debug(player->logChannel(), identifier, willChange ? "will" : "did", " change '", [keyPath UTF8String], "'");
        }
#endif
    });
}

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

- (void)legibleOutput:(id)output didOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime
{
    UNUSED_PARAM(output);

    m_taskQueue.enqueueTask([player = m_player, strings = retainPtr(strings), nativeSamples = retainPtr(nativeSamples), itemTime] {
        if (!player)
            return;
        MediaTime time = std::max(PAL::toMediaTime(itemTime), MediaTime::zeroTime());
        player->processCue(strings.get(), nativeSamples.get(), time);
    });
}

- (void)outputSequenceWasFlushed:(id)output
{
    UNUSED_PARAM(output);

    m_taskQueue.enqueueTask([player = m_player] {
        if (player)
            player->flushCues();
    });
}

#endif

@end

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)

@implementation WebCoreAVFLoaderDelegate

- (id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)player
{
    self = [super init];
    if (!self)
        return nil;
    m_player = WTFMove(player);
    return self;
}

- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest
{
    UNUSED_PARAM(resourceLoader);
    if (!m_player)
        return NO;

    m_taskQueue.enqueueTask([player = m_player, loadingRequest = retainPtr(loadingRequest)] {
        if (!player) {
            [loadingRequest finishLoadingWithError:nil];
            return;
        }

        if (!player->shouldWaitForLoadingOfResource(loadingRequest.get()))
            [loadingRequest finishLoadingWithError:nil];
    });

    return YES;
}

- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForResponseToAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    UNUSED_PARAM(resourceLoader);
    UNUSED_PARAM(challenge);
    ASSERT_NOT_REACHED();
    return NO;
}

- (void)resourceLoader:(AVAssetResourceLoader *)resourceLoader didCancelLoadingRequest:(AVAssetResourceLoadingRequest *)loadingRequest
{
    UNUSED_PARAM(resourceLoader);
    m_taskQueue.enqueueTask([player = m_player, loadingRequest = retainPtr(loadingRequest)] {
        if (player)
            player->didCancelLoadingRequest(loadingRequest.get());
    });
}

@end

#endif

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)

@implementation WebCoreAVFPullDelegate

- (id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)player
{
    self = [super init];
    if (self)
        m_player = WTFMove(player);
    return self;
}

- (void)outputMediaDataWillChange:(AVPlayerItemVideoOutputType *)output
{
    if (m_player)
        m_player->outputMediaDataWillChange(output);
}

- (void)outputSequenceWasFlushed:(AVPlayerItemVideoOutputType *)output
{
    UNUSED_PARAM(output);
    // No-op.
}

@end

#endif

#endif
