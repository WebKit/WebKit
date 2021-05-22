/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#import "AVAssetMIMETypeCache.h"
#import "AVAssetTrackUtilities.h"
#import "AVTrackPrivateAVFObjCImpl.h"
#import "AudioSourceProviderAVFObjC.h"
#import "AudioTrackPrivateAVFObjC.h"
#import "AuthenticationChallenge.h"
#import "CDMInstanceFairPlayStreamingAVFObjC.h"
#import "CDMSessionAVFoundationObjC.h"
#import "ColorSpaceCG.h"
#import "Cookie.h"
#import "DeprecatedGlobalSettings.h"
#import "ExtensionsGL.h"
#import "FloatConversion.h"
#import "GraphicsContext.h"
#import "ImageRotationSessionVT.h"
#import "InbandMetadataTextTrackPrivateAVF.h"
#import "InbandTextTrackPrivateAVFObjC.h"
#import "Logging.h"
#import "MediaPlaybackTargetCocoa.h"
#import "MediaPlaybackTargetMock.h"
#import "MediaSelectionGroupAVFObjC.h"
#import "OutOfBandTextTrackPrivateAVF.h"
#import "PixelBufferConformerCV.h"
#import "PlatformTimeRanges.h"
#import "RuntimeApplicationChecks.h"
#import "SecurityOrigin.h"
#import "SerializedPlatformDataCueMac.h"
#import "SharedBuffer.h"
#import "SourceBufferParserWebM.h"
#import "TextEncoding.h"
#import "TextTrackRepresentation.h"
#import "VideoLayerManagerObjC.h"
#import "VideoTrackPrivateAVFObjC.h"
#import "WebCoreAVFResourceLoader.h"
#import "WebCoreCALayerExtras.h"
#import "WebCoreNSURLSession.h"
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
#import <CoreVideo/CoreVideo.h>
#import <JavaScriptCore/DataView.h>
#import <JavaScriptCore/JSCInlines.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <JavaScriptCore/Uint16Array.h>
#import <JavaScriptCore/Uint32Array.h>
#import <JavaScriptCore/Uint8Array.h>
#import <VideoToolbox/VideoToolbox.h>
#import <functional>
#import <objc/runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/avfoundation/OutputContext.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/FileSystem.h>
#import <wtf/ListHashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/CString.h>
#import <wtf/threads/BinarySemaphore.h>

#if ENABLE(AVF_CAPTIONS)
#import "TextTrack.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import "LocalizedDeviceModel.h"
#import "WAKAppKitStubs.h"
#import <CoreImage/CoreImage.h>
#import <UIKit/UIDevice.h>
#import <mach/mach_port.h>
#import <pal/ios/UIKitSoftLink.h>
#else
#import <Foundation/NSGeometry.h>
#import <QuartzCore/CoreImage.h>
#endif

#import "CoreVideoSoftLink.h"
#import "MediaRemoteSoftLink.h"

#import <pal/cocoa/MediaToolboxSoftLink.h>

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

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

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

#endif // PLATFORM(IOS_FAMILY)

using namespace WebCore;

enum MediaPlayerAVFoundationObservationContext {
    MediaPlayerAVFoundationObservationContextPlayerItem,
    MediaPlayerAVFoundationObservationContextPlayerItemTrack,
    MediaPlayerAVFoundationObservationContextPlayer,
    MediaPlayerAVFoundationObservationContextAVPlayerLayer,
};

@interface WebCoreAVFMovieObserver : NSObject <AVPlayerItemLegibleOutputPushDelegate, AVPlayerItemMetadataOutputPushDelegate, AVPlayerItemMetadataCollectorPushDelegate>
{
    WeakPtr<MediaPlayerPrivateAVFoundationObjC> m_player;
    GenericTaskQueue<Timer> m_taskQueue;
    int m_delayCallbacks;
}
-(id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)callback;
-(void)disconnect;
-(void)metadataLoaded;
-(void)didEnd:(NSNotification *)notification;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(MediaPlayerAVFoundationObservationContext)context;
- (void)legibleOutput:(id)output didOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime;
- (void)outputSequenceWasFlushed:(id)output;
- (void)metadataCollector:(AVPlayerItemMetadataCollector *)metadataCollector didCollectDateRangeMetadataGroups:(NSArray<AVDateRangeMetadataGroup *> *)metadataGroups indexesOfNewGroups:(NSIndexSet *)indexesOfNewGroups indexesOfModifiedGroups:(NSIndexSet *)indexesOfModifiedGroups;
- (void)metadataOutput:(AVPlayerItemMetadataOutput *)output didOutputTimedMetadataGroups:(NSArray<AVTimedMetadataGroup *> *)groups fromPlayerItemTrack:(AVPlayerItemTrack *)track;
@end

@interface WebCoreAVFLoaderDelegate : NSObject<AVAssetResourceLoaderDelegate> {
    WeakPtr<MediaPlayerPrivateAVFoundationObjC> m_player;
    GenericTaskQueue<Timer> m_taskQueue;
}
- (id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)player;
- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest;
@end

@interface WebCoreAVFPullDelegate : NSObject<AVPlayerItemOutputPullDelegate> {
    BinarySemaphore m_semaphore;
}
- (void)outputMediaDataWillChange:(AVPlayerItemOutput *)sender;
- (void)outputSequenceWasFlushed:(AVPlayerItemOutput *)output;

@property (nonatomic, readonly) BinarySemaphore& semaphore;
@end

namespace WebCore {
static String convertEnumerationToString(AVPlayerTimeControlStatus enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("AVPlayerTimeControlStatusPaused"),
        MAKE_STATIC_STRING_IMPL("AVPlayerTimeControlStatusWaitingToPlayAtSpecifiedRate"),
        MAKE_STATIC_STRING_IMPL("AVPlayerTimeControlStatusPlaying"),
    };
    static_assert(!static_cast<size_t>(AVPlayerTimeControlStatusPaused), "AVPlayerTimeControlStatusPaused is not 0 as expected");
    static_assert(static_cast<size_t>(AVPlayerTimeControlStatusWaitingToPlayAtSpecifiedRate) == 1, "AVPlayerTimeControlStatusWaitingToPlayAtSpecifiedRate is not 1 as expected");
    static_assert(static_cast<size_t>(AVPlayerTimeControlStatusPlaying) == 2, "AVPlayerTimeControlStatusPlaying is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}
}

namespace WTF {
template<typename Type>
struct LogArgument;

template <>
struct LogArgument<AVPlayerTimeControlStatus> {
    static String toString(const AVPlayerTimeControlStatus status)
    {
        return convertEnumerationToString(status);
    }
};
}; // namespace WTF

namespace WebCore {
using namespace PAL;

static NSArray *assetMetadataKeyNames();
static NSArray *itemKVOProperties();
static NSArray *assetTrackMetadataKeyNames();
static NSArray *playerKVOProperties();
static AVAssetTrack* firstEnabledTrack(NSArray* tracks);

static dispatch_queue_t globalLoaderDelegateQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create("WebCoreAVFLoaderDelegate queue", DISPATCH_QUEUE_SERIAL);
    });
    return globalQueue;
}

static dispatch_queue_t globalPullDelegateQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create("WebCoreAVFPullDelegate queue", DISPATCH_QUEUE_SERIAL);
    });
    return globalQueue;
}

static void registerFormatReaderIfNecessary()
{
#if ENABLE(WEBM_FORMAT_READER)
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // Like we do for other media formats, allow the format reader to run in the WebContent or GPU process
        // (which is already appropriately sandboxed) rather than in a separate MediaToolbox XPC service.
        RELEASE_ASSERT(isInGPUProcess() || isInWebProcess());
        MTRegisterPluginFormatReaderBundleDirectory((__bridge CFURLRef)NSBundle.mainBundle.builtInPlugInsURL);
        MTPluginFormatReaderDisableSandboxing();
    });
#endif
}

class MediaPlayerPrivateAVFoundationObjC::Factory final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::AVFoundation; };

    std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return makeUnique<MediaPlayerPrivateAVFoundationObjC>(player);
    }

    void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types) const final
    {
        return MediaPlayerPrivateAVFoundationObjC::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateAVFoundationObjC::supportsTypeAndCodecs(parameters);
    }

    HashSet<SecurityOriginData> originsInMediaCache(const String& path) const final
    {
        return MediaPlayerPrivateAVFoundationObjC::originsInMediaCache(path);
    }

    void clearMediaCache(const String& path, WallTime modifiedSince) const final
    {
        return MediaPlayerPrivateAVFoundationObjC::clearMediaCache(path, modifiedSince);
    }

    void clearMediaCacheForOrigins(const String& path, const HashSet<SecurityOriginData>& origins) const final
    {
        return MediaPlayerPrivateAVFoundationObjC::clearMediaCacheForOrigins(path, origins);
    }

    bool supportsKeySystem(const String& keySystem, const String& mimeType) const final
    {
        return MediaPlayerPrivateAVFoundationObjC::supportsKeySystem(keySystem, mimeType);
    }
};

void MediaPlayerPrivateAVFoundationObjC::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (!isAvailable())
        return;

    ASSERT(AVAssetMIMETypeCache::singleton().isAvailable());

    registrar(makeUnique<Factory>());
}

static AVAssetCache *assetCacheForPath(const String& path)
{
    if (path.isEmpty())
        return nil;

    return [PAL::getAVAssetCacheClass() assetCacheWithURL:[NSURL fileURLWithPath:path isDirectory:YES]];
}

static AVAssetCache *ensureAssetCacheExistsForPath(const String& path)
{
    if (path.isEmpty())
        return nil;

    auto fileExistsAtPath = FileSystem::fileExists(path);

    if (fileExistsAtPath && !FileSystem::fileIsDirectory(path, FileSystem::ShouldFollowSymbolicLinks::Yes)) {
        // Non-directory file already exists at the path location; bail.
        ASSERT_NOT_REACHED();
        return nil;
    }

    if (!fileExistsAtPath && !FileSystem::makeAllDirectories(path)) {
        // Could not create a directory at the specified location; bail.
        ASSERT_NOT_REACHED();
        return nil;
    }

    return assetCacheForPath(path);
}

HashSet<SecurityOriginData> MediaPlayerPrivateAVFoundationObjC::originsInMediaCache(const String& path)
{
    HashSet<SecurityOriginData> origins;
    AVAssetCache* assetCache = assetCacheForPath(path);
    if (!assetCache)
        return origins;

    for (NSString *key in [assetCache allKeys]) {
        URL keyAsURL = URL(URL(), key);
        if (keyAsURL.isValid())
            origins.add(SecurityOriginData::fromURL(keyAsURL));
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
    AVAssetCache* assetCache = assetCacheForPath(path);
    if (!assetCache)
        return;
    
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

void MediaPlayerPrivateAVFoundationObjC::clearMediaCacheForOrigins(const String& path, const HashSet<SecurityOriginData>& origins)
{
    AVAssetCache* assetCache = assetCacheForPath(path);
    if (!assetCache)
        return;

    for (NSString *key in [assetCache allKeys]) {
        URL keyAsURL = URL(URL(), key);
        if (keyAsURL.isValid()) {
            if (origins.contains(SecurityOriginData::fromURL(keyAsURL)))
                [assetCache removeEntryForKey:key];
        }
    }
}

MediaPlayerPrivateAVFoundationObjC::MediaPlayerPrivateAVFoundationObjC(MediaPlayer* player)
    : MediaPlayerPrivateAVFoundation(player)
    , m_videoLayerManager(makeUnique<VideoLayerManagerObjC>(logger(), logIdentifier()))
    , m_objcObserver(adoptNS([[WebCoreAVFMovieObserver alloc] initWithPlayer:makeWeakPtr(*this)]))
    , m_loaderDelegate(adoptNS([[WebCoreAVFLoaderDelegate alloc] initWithPlayer:makeWeakPtr(*this)]))
    , m_cachedItemStatus(MediaPlayerAVPlayerItemStatusDoesNotExist)
{
    m_muted = player->muted();
}

MediaPlayerPrivateAVFoundationObjC::~MediaPlayerPrivateAVFoundationObjC()
{
    weakPtrFactory().revokeAll();

    [[m_avAsset.get() resourceLoader] setDelegate:nil queue:0];

    for (auto& pair : m_resourceLoaderMap)
        pair.value->invalidate();

    [m_videoOutput setDelegate:nil queue:0];

    if (m_videoLayer)
        destroyVideoLayer();

    cancelLoad();
}

void MediaPlayerPrivateAVFoundationObjC::cancelLoad()
{
    ALWAYS_LOG(LOGIDENTIFIER);
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

    if (m_legibleOutput) {
        if (m_avPlayerItem)
            [m_avPlayerItem.get() removeOutput:m_legibleOutput.get()];
        m_legibleOutput = nil;
    }

    if (m_metadataCollector) {
        // FIXME - removeMediaDataCollector sometimes crashes, do not uncomment this until rdar://56617504 has been fixed.
        // if (m_avPlayerItem)
        //     [m_avPlayerItem.get() removeMediaDataCollector:m_metadataCollector.get()];
        m_metadataCollector = nil;
    }

    if (m_metadataOutput) {
        if (m_avPlayerItem)
            [m_avPlayerItem.get() removeOutput:m_metadataOutput.get()];
        m_metadataOutput = nil;
    }

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
#if !PLATFORM(IOS_FAMILY)
        [m_avPlayer setOutputContext:nil];
#endif

        if (m_currentTimeObserver)
            [m_avPlayer removeTimeObserver:m_currentTimeObserver.get()];
        m_currentTimeObserver = nil;

        m_avPlayer = nil;
    }

    // Reset cached properties
    m_createAssetPending = false;
    m_haveCheckedPlayability = false;
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
    return m_videoOutput || m_imageGenerator;
}

void MediaPlayerPrivateAVFoundationObjC::createContextVideoRenderer()
{
    createVideoOutput();
}

void MediaPlayerPrivateAVFoundationObjC::createImageGenerator()
{
    using namespace PAL;
    INFO_LOG(LOGIDENTIFIER);

    if (!m_avAsset || m_imageGenerator)
        return;

    m_imageGenerator = [PAL::getAVAssetImageGeneratorClass() assetImageGeneratorWithAsset:m_avAsset.get()];

    [m_imageGenerator.get() setApertureMode:AVAssetImageGeneratorApertureModeCleanAperture];
    [m_imageGenerator.get() setAppliesPreferredTrackTransform:YES];
    [m_imageGenerator.get() setRequestedTimeToleranceBefore:kCMTimeZero];
    [m_imageGenerator.get() setRequestedTimeToleranceAfter:kCMTimeZero];
}

void MediaPlayerPrivateAVFoundationObjC::destroyContextVideoRenderer()
{
    destroyVideoOutput();
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

        if (!m_videoOutput)
            createVideoOutput();

        player()->renderingModeChanged();
    });
}

void MediaPlayerPrivateAVFoundationObjC::createAVPlayerLayer()
{
    if (!m_avPlayer)
        return;

    m_videoLayer = adoptNS([PAL::allocAVPlayerLayerInstance() init]);
    [m_videoLayer setPlayer:m_avPlayer.get()];

    [m_videoLayer setName:@"MediaPlayerPrivate AVPlayerLayer"];
    [m_videoLayer addObserver:m_objcObserver.get() forKeyPath:@"readyForDisplay" options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextAVPlayerLayer];
    updateVideoLayerGravity();
    [m_videoLayer setContentsScale:player()->playerContentsScale()];
    IntSize defaultSize = snappedIntRect(player()->playerContentBoxRect()).size();
    ALWAYS_LOG(LOGIDENTIFIER);

    m_videoLayerManager->setVideoLayer(m_videoLayer.get(), defaultSize);

#if PLATFORM(IOS_FAMILY) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if ([m_videoLayer respondsToSelector:@selector(setPIPModeEnabled:)])
        [m_videoLayer setPIPModeEnabled:(player()->fullscreenMode() & MediaPlayer::VideoFullscreenModePictureInPicture)];
#endif
}

void MediaPlayerPrivateAVFoundationObjC::destroyVideoLayer()
{
    if (!m_videoLayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    [m_videoLayer removeObserver:m_objcObserver.get() forKeyPath:@"readyForDisplay"];
    [m_videoLayer setPlayer:nil];
    m_videoLayerManager->didDestroyVideoLayer();

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

    if (m_videoOutput && (m_lastPixelBuffer || [m_videoOutput hasNewPixelBufferForItemTime:[m_avPlayerItem currentTime]]))
        return true;

    return m_videoFrameHasDrawn;
}

#if ENABLE(AVF_CAPTIONS)

static const NSArray *mediaDescriptionForKind(PlatformTextTrackData::TrackKind kind)
{
    static bool manualSelectionMode = MTEnableCaption2015BehaviorPtr() && MTEnableCaption2015BehaviorPtr()();
    if (manualSelectionMode)
        return @[ AVMediaCharacteristicIsAuxiliaryContent ];

    // FIXME: Match these to correct types:
    if (kind == PlatformTextTrackData::TrackKind::Caption)
        return @[ AVMediaCharacteristicTranscribesSpokenDialogForAccessibility ];

    if (kind == PlatformTextTrackData::TrackKind::Subtitle)
        return @[ AVMediaCharacteristicTranscribesSpokenDialogForAccessibility ];

    if (kind == PlatformTextTrackData::TrackKind::Description)
        return @[ AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, AVMediaCharacteristicDescribesMusicAndSoundForAccessibility ];

    if (kind == PlatformTextTrackData::TrackKind::Forced)
        return @[ AVMediaCharacteristicContainsOnlyForcedSubtitles ];

    return @[ AVMediaCharacteristicTranscribesSpokenDialogForAccessibility ];
}
    
void MediaPlayerPrivateAVFoundationObjC::notifyTrackModeChanged()
{
    trackModeChanged();
}
    
void MediaPlayerPrivateAVFoundationObjC::synchronizeTextTrackState()
{
    const auto& outOfBandTrackSources = player()->outOfBandTrackSources();
    
    for (auto& textTrack : m_textTracks) {
        if (textTrack->textTrackCategory() != InbandTextTrackPrivateAVF::OutOfBand)
            continue;
        
        RefPtr<OutOfBandTextTrackPrivateAVF> trackPrivate = static_cast<OutOfBandTextTrackPrivateAVF*>(textTrack.get());
        RetainPtr<AVMediaSelectionOption> currentOption = trackPrivate->mediaSelectionOption();
        
        for (auto& track : outOfBandTrackSources) {
            RetainPtr<CFStringRef> uniqueID = String::number(track->uniqueId()).createCFString();
            
            if (![[currentOption.get() outOfBandIdentifier] isEqual:(__bridge NSString *)uniqueID.get()])
                continue;
            
            InbandTextTrackPrivate::Mode mode = InbandTextTrackPrivate::Mode::Hidden;
            switch (track->mode()) {
            case PlatformTextTrackData::TrackMode::Hidden:
                mode = InbandTextTrackPrivate::Mode::Hidden;
                break;
            case PlatformTextTrackData::TrackMode::Disabled:
                mode = InbandTextTrackPrivate::Mode::Disabled;
                break;
            case PlatformTextTrackData::TrackMode::Showing:
                mode = InbandTextTrackPrivate::Mode::Showing;
                break;
            }
            
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

void MediaPlayerPrivateAVFoundationObjC::createAVAssetForURL(const URL& url)
{
    if (m_avAsset || m_createAssetPending)
        return;

    m_createAssetPending = true;
    RetainPtr<NSMutableDictionary> options = adoptNS([[NSMutableDictionary alloc] init]);

#if PLATFORM(IOS_FAMILY)
    if (!PAL::canLoad_AVFoundation_AVURLAssetHTTPCookiesKey()) {
        createAVAssetForURL(url, WTFMove(options));
        return;
    }

    player()->getRawCookies(url, [this, weakThis = makeWeakPtr(this), options = WTFMove(options), url] (auto cookies) mutable {
        if (!weakThis)
            return;
        
        if (cookies.size()) {
            auto nsCookies = createNSArray(cookies, [] (auto& cookie) -> NSHTTPCookie * {
                return cookie;
            });

            [options setObject:nsCookies.get() forKey:AVURLAssetHTTPCookiesKey];
        }

        createAVAssetForURL(url, WTFMove(options));
    });
#else
    createAVAssetForURL(url, WTFMove(options));
#endif

}

static bool willUseWebMFormatReaderForType(const String& type)
{
#if ENABLE(WEBM_FORMAT_READER)
    if (!SourceBufferParserWebM::isWebMFormatReaderAvailable())
        return false;

    return equalIgnoringASCIICase(type, "video/webm") || equalIgnoringASCIICase(type, "audio/webm");
#else
    UNUSED_PARAM(type);
    return false;
#endif
}

static bool hasBrokenFragmentSupport()
{
#if PLATFORM(MAC)
    // On some versions of macOS, Photos.framework has overriden utility methods from AVFoundation that cause
    // a exception to be thrown when parsing fragment identifiers from a URL. Their implementation requires
    // an even number of components when splitting the fragment identifier with separator characters ['&','='].
    // Work around this broken implementation by pre-parsing the fragment and ensuring that it meets their
    // criteria. Problematic strings from the TC0051.html test include "t=3&", and this problem generally is
    // with subtrings between the '&' character not including an equal sign.
    static bool hasBrokenFragmentSupport = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        @try {
            auto selector = NSSelectorFromString(@"isURLForAssetInCollection:");
            auto theClass = PAL::getAVAssetCollectionClass();
            if (![theClass respondsToSelector:selector])
                return;
            [theClass performSelector:selector withObject:[NSURL URLWithString:@"file:///invalid-file.mp4#t=3&"]];
        } @catch (NSException *exception) {
            hasBrokenFragmentSupport = true;
        }
    });
    return hasBrokenFragmentSupport;
#else
    return false;
#endif
}

static URL conformFragmentIdentifierForURL(const URL& url)
{
#if PLATFORM(MAC)
    ASSERT(hasBrokenFragmentSupport());

    auto hasInvalidNumberOfEqualCharacters = [](const StringView& fragmentParameter) {
        auto results = fragmentParameter.splitAllowingEmptyEntries('=');
        auto iterator = results.begin();
        return iterator == results.end() || ++iterator == results.end() || ++iterator != results.end();
    };

    StringBuilder replacementFragmentIdentifierBuilder;
    bool firstParameter = true;
    bool hasInvalidFragmentIdentifier = false;

    for (auto fragmentParameter : url.fragmentIdentifier().splitAllowingEmptyEntries('&')) {
        if (hasInvalidNumberOfEqualCharacters(fragmentParameter)) {
            hasInvalidFragmentIdentifier = true;
            continue;
        }
        if (!firstParameter)
            replacementFragmentIdentifierBuilder.append('&');
        else
            firstParameter = false;
        replacementFragmentIdentifierBuilder.append(fragmentParameter);
    }

    if (!hasInvalidFragmentIdentifier)
        return url;

    URL validURL = url;
    if (replacementFragmentIdentifierBuilder.isEmpty())
        validURL.removeFragmentIdentifier();
    else
        validURL.setFragmentIdentifier(replacementFragmentIdentifierBuilder.toString());

    return validURL;
#else
    ASSERT_NOT_REACHED();
    return url;
#endif
}

void MediaPlayerPrivateAVFoundationObjC::createAVAssetForURL(const URL& url, RetainPtr<NSMutableDictionary> options)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_createAssetPending = false;

    if (m_avAsset)
        return;

    setDelayCallbacks(true);

    [options.get() setObject:@(AVAssetReferenceRestrictionForbidRemoteReferenceToLocal | AVAssetReferenceRestrictionForbidLocalReferenceToRemote) forKey:AVURLAssetReferenceRestrictionsKey];

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

    if (PAL::canLoad_AVFoundation_AVURLAssetUseClientURLLoadingExclusively())
        [options setObject:@YES forKey:AVURLAssetUseClientURLLoadingExclusively];
#if PLATFORM(IOS_FAMILY)
    else if (PAL::canLoad_AVFoundation_AVURLAssetRequiresCustomURLLoadingKey())
        [options setObject:@YES forKey:AVURLAssetRequiresCustomURLLoadingKey];
    // FIXME: rdar://problem/20354688
    String identifier = player()->sourceApplicationIdentifier();
    if (!identifier.isEmpty())
        [options setObject:identifier forKey:AVURLAssetClientBundleIdentifierKey];
#endif

    auto type = player()->contentMIMEType();

    // Don't advertise WebM MIME types or the format reader won't be loaded until rdar://72405127 is fixed.
    auto willUseWebMFormatReader = willUseWebMFormatReaderForType(type);

    if (PAL::canLoad_AVFoundation_AVURLAssetOutOfBandMIMETypeKey() && !type.isEmpty() && !player()->contentMIMETypeWasInferredFromExtension() && !willUseWebMFormatReader) {
        auto codecs = player()->contentTypeCodecs();
        if (!codecs.isEmpty()) {
            NSString *typeString = [NSString stringWithFormat:@"%@; codecs=\"%@\"", (NSString *)type, (NSString *)codecs];
            [options setObject:typeString forKey:AVURLAssetOutOfBandMIMETypeKey];
        } else
            [options setObject:(NSString *)type forKey:AVURLAssetOutOfBandMIMETypeKey];
    }

#if ENABLE(AVF_CAPTIONS)
    auto outOfBandTrackSources = player()->outOfBandTrackSources();
    if (!outOfBandTrackSources.isEmpty()) {
        auto outOfBandTracks = createNSArray(outOfBandTrackSources, [] (auto& trackSource) {
            return @{
                AVOutOfBandAlternateTrackDisplayNameKey: trackSource->label(),
                AVOutOfBandAlternateTrackExtendedLanguageTagKey: trackSource->language(),
                AVOutOfBandAlternateTrackIsDefaultKey: trackSource->isDefault() ? @YES : @NO,
                AVOutOfBandAlternateTrackIdentifierKey: String::number(trackSource->uniqueId()),
                AVOutOfBandAlternateTrackSourceKey: trackSource->url(),
                AVOutOfBandAlternateTrackMediaCharactersticsKey: mediaDescriptionForKind(trackSource->kind()),
            };
        });

        [options.get() setObject:outOfBandTracks.get() forKey:AVURLAssetOutOfBandAlternateTracksKey];
    }
#endif

#if PLATFORM(IOS_FAMILY)
    String networkInterfaceName = player()->mediaPlayerNetworkInterfaceName();
    if (!networkInterfaceName.isEmpty())
        [options setObject:networkInterfaceName forKey:AVURLAssetBoundNetworkInterfaceName];
#endif

    bool usePersistentCache = player()->shouldUsePersistentCache();
    [options setObject:@(!usePersistentCache) forKey:AVURLAssetUsesNoPersistentCacheKey];
    
    if (usePersistentCache) {
        if (auto* assetCache = ensureAssetCacheExistsForPath(player()->mediaCacheDirectory()))
            [options setObject:assetCache forKey:AVURLAssetCacheKey];
        else
            [options setObject:@NO forKey:AVURLAssetUsesNoPersistentCacheKey];
    }

    if (willUseWebMFormatReader)
        registerFormatReaderIfNecessary();

    NSURL *cocoaURL = nil;
    if (hasBrokenFragmentSupport() && url.hasFragmentIdentifier())
        cocoaURL = canonicalURL(conformFragmentIdentifierForURL(url));
    else
        cocoaURL = canonicalURL(url);

    m_avAsset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:cocoaURL options:options.get()]);

    AVAssetResourceLoader *resourceLoader = m_avAsset.get().resourceLoader;
    [resourceLoader setDelegate:m_loaderDelegate.get() queue:globalLoaderDelegateQueue()];

    if (DeprecatedGlobalSettings::isAVFoundationNSURLSessionEnabled()
        && [resourceLoader respondsToSelector:@selector(setURLSession:)]
        && [resourceLoader respondsToSelector:@selector(URLSessionDataDelegate)]
        && [resourceLoader respondsToSelector:@selector(URLSessionDataDelegateQueue)]) {
        RefPtr<PlatformMediaResourceLoader> mediaResourceLoader = player()->createResourceLoader();
        if (mediaResourceLoader)
            resourceLoader.URLSession = (NSURLSession *)adoptNS([[WebCoreNSURLSession alloc] initWithResourceLoader:*mediaResourceLoader delegate:resourceLoader.URLSessionDataDelegate delegateQueue:resourceLoader.URLSessionDataDelegateQueue]).get();
    }

    m_haveCheckedPlayability = false;

    setDelayCallbacks(false);

    checkPlayability();
}

void MediaPlayerPrivateAVFoundationObjC::setAVPlayerItem(AVPlayerItem *item)
{
    if (!m_avPlayer)
        return;

    if (pthread_main_np()) {
        [m_avPlayer replaceCurrentItemWithPlayerItem:item];
        return;
    }

    RetainPtr<AVPlayer> strongPlayer = m_avPlayer.get();
    RetainPtr<AVPlayerItem> strongItem = item;
    RunLoop::main().dispatch([strongPlayer, strongItem] {
        [strongPlayer replaceCurrentItemWithPlayerItem:strongItem.get()];
    });
}

#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
static NSString* convertDynamicRangeModeEnumToAVVideoRange(DynamicRangeMode mode)
{
    switch (mode) {
    case DynamicRangeMode::None:
        return nil;
    case DynamicRangeMode::Standard:
        return PAL::canLoad_AVFoundation_AVVideoRangeSDR() ? PAL::get_AVFoundation_AVVideoRangeSDR() : nil;
    case DynamicRangeMode::HLG:
        return PAL::canLoad_AVFoundation_AVVideoRangeHLG() ? PAL::get_AVFoundation_AVVideoRangeHLG() : nil;
    case DynamicRangeMode::HDR10:
        return PAL::canLoad_AVFoundation_AVVideoRangeHDR10() ? PAL::get_AVFoundation_AVVideoRangeHDR10() : nil;
    case DynamicRangeMode::DolbyVisionPQ:
        return PAL::canLoad_AVFoundation_AVVideoRangeDolbyVisionPQ() ? PAL::get_AVFoundation_AVVideoRangeDolbyVisionPQ() : nil;
    }

    ASSERT_NOT_REACHED();
    return nil;
}
#endif

void MediaPlayerPrivateAVFoundationObjC::createAVPlayer()
{
    if (m_avPlayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    setDelayCallbacks(true);

    m_avPlayer = adoptNS([PAL::allocAVPlayerInstance() init]);
    for (NSString *keyName in playerKVOProperties())
        [m_avPlayer.get() addObserver:m_objcObserver.get() forKeyPath:keyName options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextPlayer];

    setShouldObserveTimeControlStatus(true);

    m_avPlayer.get().appliesMediaSelectionCriteriaAutomatically = NO;
#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
    if ([m_avPlayer respondsToSelector:@selector(setVideoRangeOverride:)])
        m_avPlayer.get().videoRangeOverride = convertDynamicRangeModeEnumToAVVideoRange(player()->preferredDynamicRangeMode());
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

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
    setShouldDisableSleep(player()->shouldDisableSleep());
#endif

    if (m_muted) {
        [m_avPlayer.get() setMuted:m_muted];

#if HAVE(AVPLAYER_SUPRESSES_AUDIO_RENDERING)
        if (player()->isVideoPlayer())
            m_avPlayer.get().suppressesAudioRendering = YES;
#endif
    }

    if (player()->isVideoPlayer())
        createAVPlayerLayer();

    if (m_avPlayerItem)
        setAVPlayerItem(m_avPlayerItem.get());

#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    auto audioOutputDeviceId = player()->audioOutputDeviceIdOverride();
    if (!audioOutputDeviceId.isNull()) {
        if (audioOutputDeviceId.isEmpty())
            m_avPlayer.get().audioOutputDeviceUniqueID = nil;
        else
            m_avPlayer.get().audioOutputDeviceUniqueID = audioOutputDeviceId;
    }
#endif

    ASSERT(!m_currentTimeObserver);
    m_currentTimeObserver = [m_avPlayer addPeriodicTimeObserverForInterval:CMTimeMake(1, 10) queue:dispatch_get_main_queue() usingBlock:[this] (CMTime time) {
        currentMediaTimeDidChange(PAL::toMediaTime(time));
    }];

    setDelayCallbacks(false);
}

static NSString* audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(MediaPlayer::PitchCorrectionAlgorithm pitchCorrectionAlgorithm, bool preservesPitch, double rate)
{
    if (!preservesPitch || !rate || rate == 1.)
        return AVAudioTimePitchAlgorithmVarispeed;

    switch (pitchCorrectionAlgorithm) {
    case MediaPlayer::PitchCorrectionAlgorithm::BestAllAround:
    case MediaPlayer::PitchCorrectionAlgorithm::BestForMusic:
        return AVAudioTimePitchAlgorithmSpectral;
    case MediaPlayer::PitchCorrectionAlgorithm::BestForSpeech:
        return AVAudioTimePitchAlgorithmTimeDomain;
    }
}

void MediaPlayerPrivateAVFoundationObjC::createAVPlayerItem()
{
    if (m_avPlayerItem)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    setDelayCallbacks(true);

    // Create the player item so we can load media data. 
    m_avPlayerItem = adoptNS([PAL::allocAVPlayerItemInstance() initWithAsset:m_avAsset.get()]);

    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(didEnd:) name:AVPlayerItemDidPlayToEndTimeNotification object:m_avPlayerItem.get()];

    NSKeyValueObservingOptions options = NSKeyValueObservingOptionNew | NSKeyValueObservingOptionPrior;
    for (NSString *keyName in itemKVOProperties())
        [m_avPlayerItem.get() addObserver:m_objcObserver.get() forKeyPath:keyName options:options context:(void *)MediaPlayerAVFoundationObservationContextPlayerItem];

    [m_avPlayerItem setAudioTimePitchAlgorithm:audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player()->pitchCorrectionAlgorithm(), player()->preservesPitch(), m_requestedRate)];

#if HAVE(AVFOUNDATION_INTERSTITIAL_EVENTS)
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if ([m_avPlayerItem respondsToSelector:@selector(setAutomaticallyHandlesInterstitialEvents:)])
        [m_avPlayerItem setAutomaticallyHandlesInterstitialEvents:NO];
ALLOW_NEW_API_WITHOUT_GUARDS_END
#endif

    if (m_avPlayer)
        setAVPlayerItem(m_avPlayerItem.get());

    const NSTimeInterval avPlayerOutputAdvanceInterval = 2;

    RetainPtr<NSArray> subtypes = adoptNS([[NSArray alloc] initWithObjects:@(kCMSubtitleFormatType_WebVTT), nil]);
    m_legibleOutput = adoptNS([PAL::allocAVPlayerItemLegibleOutputInstance() initWithMediaSubtypesForNativeRepresentation:subtypes.get()]);
    [m_legibleOutput.get() setSuppressesPlayerRendering:YES];

    [m_legibleOutput.get() setDelegate:m_objcObserver.get() queue:dispatch_get_main_queue()];
    [m_legibleOutput.get() setAdvanceIntervalForDelegateInvocation:avPlayerOutputAdvanceInterval];
    [m_legibleOutput.get() setTextStylingResolution:AVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly];
    [m_avPlayerItem.get() addOutput:m_legibleOutput.get()];

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    if (m_provider) {
        m_provider->setPlayerItem(m_avPlayerItem.get());
        m_provider->setAudioTrack(firstEnabledTrack(safeAVAssetTracksForAudibleMedia()));
    }
#endif

    createVideoOutput();

    m_metadataCollector = adoptNS([PAL::allocAVPlayerItemMetadataCollectorInstance() initWithIdentifiers:nil classifyingLabels:nil]);
    [m_metadataCollector.get() setDelegate:m_objcObserver.get() queue:dispatch_get_main_queue()];
    [m_avPlayerItem.get() addMediaDataCollector:m_metadataCollector.get()];

    m_metadataOutput = adoptNS([PAL::allocAVPlayerItemMetadataOutputInstance() initWithIdentifiers:nil]);
    [m_metadataOutput.get() setDelegate:m_objcObserver.get() queue:dispatch_get_main_queue()];
    [m_metadataOutput.get() setAdvanceIntervalForDelegateInvocation:avPlayerOutputAdvanceInterval];
    [m_avPlayerItem.get() addOutput:m_metadataOutput.get()];

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::checkPlayability()
{
    if (m_haveCheckedPlayability)
        return;
    m_haveCheckedPlayability = true;

    INFO_LOG(LOGIDENTIFIER);
    auto weakThis = makeWeakPtr(*this);

    [m_avAsset.get() loadValuesAsynchronouslyForKeys:@[@"playable", @"tracks"] completionHandler:^{
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
    return m_videoLayerManager->videoInlineLayer();
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

void MediaPlayerPrivateAVFoundationObjC::updateVideoFullscreenInlineImage()
{
    updateLastImage(UpdateType::UpdateSynchronously);
    m_videoLayerManager->updateVideoFullscreenInlineImage(m_lastImage ? m_lastImage->platformImage() : nullptr);
}

RetainPtr<PlatformLayer> MediaPlayerPrivateAVFoundationObjC::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}

void MediaPlayerPrivateAVFoundationObjC::setVideoFullscreenLayer(PlatformLayer* videoFullscreenLayer, Function<void()>&& completionHandler)
{
    if (videoFullscreenLayer)
        updateLastImage(UpdateType::UpdateSynchronously);
    m_videoLayerManager->setVideoFullscreenLayer(videoFullscreenLayer, WTFMove(completionHandler), m_lastImage ? m_lastImage->platformImage() : nullptr);
    updateDisableExternalPlayback();
}

void MediaPlayerPrivateAVFoundationObjC::setVideoFullscreenFrame(FloatRect frame)
{
    ALWAYS_LOG(LOGIDENTIFIER, "width = ", frame.size().width(), ", height = ", frame.size().height());
    m_videoLayerManager->setVideoFullscreenFrame(frame);
}

void MediaPlayerPrivateAVFoundationObjC::setVideoFullscreenGravity(MediaPlayer::VideoGravity gravity)
{
    m_videoFullscreenGravity = gravity;

    if (!m_videoLayer)
        return;

    NSString *videoGravity = AVLayerVideoGravityResizeAspect;
    if (gravity == MediaPlayer::VideoGravity::Resize)
        videoGravity = AVLayerVideoGravityResize;
    else if (gravity == MediaPlayer::VideoGravity::ResizeAspect)
        videoGravity = AVLayerVideoGravityResizeAspect;
    else if (gravity == MediaPlayer::VideoGravity::ResizeAspectFill)
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

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

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
    if (!metaDataAvailable())
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_requestedPlaying = true;
    setPlayerRate(m_requestedRate);
}

void MediaPlayerPrivateAVFoundationObjC::platformPause()
{
    if (!metaDataAvailable())
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_requestedPlaying = false;
    setPlayerRate(0);
}

bool MediaPlayerPrivateAVFoundationObjC::platformPaused() const
{
    return m_cachedTimeControlStatus == AVPlayerTimeControlStatusPaused;
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

float MediaPlayerPrivateAVFoundationObjC::currentTime() const
{
    return currentMediaTime().toFloat();
}

MediaTime MediaPlayerPrivateAVFoundationObjC::currentMediaTime() const
{
    if (!metaDataAvailable() || !m_avPlayerItem)
        return MediaTime::zeroTime();

    if (!m_wallClockAtCachedCurrentTime)
        currentMediaTimeDidChange(toMediaTime([m_avPlayerItem.get() currentTime]));
    ASSERT(m_wallClockAtCachedCurrentTime);

    auto itemTime = m_cachedCurrentMediaTime;
    if (!itemTime.isFinite())
        return MediaTime::zeroTime();

    if (m_timeControlStatusAtCachedCurrentTime == AVPlayerTimeControlStatusPlaying) {
        auto elapsedMediaTime = (WallTime::now() - *m_wallClockAtCachedCurrentTime) * m_requestedRateAtCachedCurrentTime;
        itemTime += MediaTime::createWithDouble(elapsedMediaTime.seconds());
    }

    return std::min(std::max(itemTime, MediaTime::zeroTime()), m_cachedDuration);
}

bool MediaPlayerPrivateAVFoundationObjC::setCurrentTimeDidChangeCallback(MediaPlayer::CurrentTimeDidChangeCallback&& callback)
{
    m_currentTimeDidChangeCallback = WTFMove(callback);
    return true;
}

void MediaPlayerPrivateAVFoundationObjC::currentMediaTimeDidChange(MediaTime&& time) const
{
    m_cachedCurrentMediaTime = WTFMove(time);
    m_wallClockAtCachedCurrentTime = WallTime::now();
    m_timeControlStatusAtCachedCurrentTime = m_cachedTimeControlStatus;
    m_requestedRateAtCachedCurrentTime = m_requestedRate;

    if (m_currentTimeDidChangeCallback)
        m_currentTimeDidChangeCallback(time.isFinite() ? m_cachedCurrentMediaTime : MediaTime::zeroTime());
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

    ALWAYS_LOG(LOGIDENTIFIER, volume);

    [m_avPlayer.get() setVolume:volume];
#endif
}

void MediaPlayerPrivateAVFoundationObjC::setMuted(bool muted)
{
    if (m_muted == muted)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, muted);

    m_muted = muted;

    if (!m_avPlayer)
        return;

    [m_avPlayer.get() setMuted:m_muted];
#if HAVE(AVPLAYER_SUPRESSES_AUDIO_RENDERING)
    if (!m_muted)
        m_avPlayer.get().suppressesAudioRendering = NO;
#endif
}

void MediaPlayerPrivateAVFoundationObjC::setRateDouble(double rate)
{
    m_requestedRate = rate;
    if (m_requestedPlaying)
        setPlayerRate(rate);
    m_wallClockAtCachedCurrentTime = WTF::nullopt;
}

void MediaPlayerPrivateAVFoundationObjC::setPlayerRate(double rate)
{
    setDelayCallbacks(true);

    [m_avPlayerItem setAudioTimePitchAlgorithm:audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player()->pitchCorrectionAlgorithm(), player()->preservesPitch(), m_requestedRate)];

    setShouldObserveTimeControlStatus(false);
    [m_avPlayer setRate:rate];
    m_cachedTimeControlStatus = [m_avPlayer timeControlStatus];
    setShouldObserveTimeControlStatus(true);
    setDelayCallbacks(false);

    m_wallClockAtCachedCurrentTime = WTF::nullopt;
}

double MediaPlayerPrivateAVFoundationObjC::rate() const
{
    if (!metaDataAvailable())
        return 0;

    return m_cachedRate;
}

double MediaPlayerPrivateAVFoundationObjC::seekableTimeRangesLastModifiedTime() const
{
#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(MACCATALYST)
    return [m_avPlayerItem seekableTimeRangesLastModifiedTime];
#else
    return 0;
#endif
}

double MediaPlayerPrivateAVFoundationObjC::liveUpdateInterval() const
{
#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(MACCATALYST)
    return [m_avPlayerItem liveUpdateInterval];
#else
    return 0;
#endif
}

void MediaPlayerPrivateAVFoundationObjC::setPreservesPitch(bool preservesPitch)
{
    if (m_avPlayerItem)
        [m_avPlayerItem setAudioTimePitchAlgorithm:audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player()->pitchCorrectionAlgorithm(), preservesPitch, m_requestedRate)];
}

void MediaPlayerPrivateAVFoundationObjC::setPitchCorrectionAlgorithm(MediaPlayer::PitchCorrectionAlgorithm pitchCorrectionAlgorithm)
{
    if (m_avPlayerItem)
        [m_avPlayerItem setAudioTimePitchAlgorithm:audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(pitchCorrectionAlgorithm, player()->preservesPitch(), m_requestedRate)];
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateAVFoundationObjC::platformBufferedTimeRanges() const
{
    auto timeRanges = makeUnique<PlatformTimeRanges>();

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

    if (![m_cachedSeekableRanges count])
        return platformDuration();

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

    if (!m_cachedAssetIsLoaded) {
        for (NSString *keyName in assetMetadataKeyNames()) {
            NSError *error = nil;
            AVKeyValueStatus keyStatus = [m_avAsset.get() statusOfValueForKey:keyName error:&error];

            if (error)
                ERROR_LOG(LOGIDENTIFIER, "failed for ", [keyName UTF8String], ", error = ", [[error localizedDescription] UTF8String]);

            if (keyStatus < AVKeyValueStatusLoaded)
                return MediaPlayerAVAssetStatusLoading; // At least one key is not loaded yet.

            if (keyStatus == AVKeyValueStatusFailed)
                return MediaPlayerAVAssetStatusFailed; // At least one key could not be loaded.

            if (keyStatus == AVKeyValueStatusCancelled)
                return MediaPlayerAVAssetStatusCancelled; // Loading of at least one key was cancelled.
        }
        m_cachedAssetIsLoaded = true;
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

    if (!m_cachedAssetIsPlayable)
        m_cachedAssetIsPlayable = [[m_avAsset.get() valueForKey:@"playable"] boolValue];

    if (*m_cachedAssetIsPlayable && m_tracksArePlayable.value())
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
    if (!metaDataAvailable() || context.paintingDisabled() || isCurrentPlaybackTargetWireless())
        return;

    setDelayCallbacks(true);
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // Callers of this will often call copyVideoTextureToPlatformTexture first,
    // which calls updateLastPixelBuffer, which clears m_lastImage whenever the
    // video delivers a new frame. This breaks videoOutputHasAvailableFrame's
    // short-circuiting when m_lastImage is non-null, but the video often
    // doesn't have a new frame to deliver since the last time
    // hasNewPixelBufferForItemTime was called against m_videoOutput. To avoid
    // changing the semantics of videoOutputHasAvailableFrame in ways that might
    // break other callers, look for production of a recent pixel buffer from
    // the video output, too.
    if (videoOutputHasAvailableFrame() || (m_videoOutput && m_lastPixelBuffer))
        paintWithVideoOutput(context, rect);
    else
        paintWithImageGenerator(context, rect);

    END_BLOCK_OBJC_EXCEPTIONS
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
        context.setImageInterpolationQuality(InterpolationQuality::Low);
        IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
        CGContextDrawImage(context.platformContext(), CGRectMake(0, 0, paintRect.width(), paintRect.height()), image.get());
    }
}

RetainPtr<CGImageRef> MediaPlayerPrivateAVFoundationObjC::createImageForTimeInRect(float time, const FloatRect& rect)
{
    if (!m_imageGenerator)
        createImageGenerator();
    ASSERT(m_imageGenerator);

    MonotonicTime start = MonotonicTime::now();

    [m_imageGenerator.get() setMaximumSize:CGSize(rect.size())];
    RetainPtr<CGImageRef> rawImage = adoptCF([m_imageGenerator.get() copyCGImageAtTime:CMTimeMakeWithSeconds(time, 600) actualTime:nil error:nil]);
    RetainPtr<CGImageRef> image = adoptCF(CGImageCreateCopyWithColorSpace(rawImage.get(), sRGBColorSpaceRef()));

    INFO_LOG(LOGIDENTIFIER, "creating image took ", (MonotonicTime::now() - start).seconds());

    return image;
}

void MediaPlayerPrivateAVFoundationObjC::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& supportedTypes)
{
    supportedTypes = AVAssetMIMETypeCache::singleton().supportedTypes();
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
static bool keySystemIsSupported(const String& keySystem)
{
    if (equalIgnoringASCIICase(keySystem, "com.apple.fps") || equalIgnoringASCIICase(keySystem, "com.apple.fps.1_0") || equalIgnoringASCIICase(keySystem, "org.w3c.clearkey"))
        return true;
    return false;
}
#endif

MediaPlayer::SupportsType MediaPlayerPrivateAVFoundationObjC::supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters)
{
#if ENABLE(MEDIA_SOURCE)
    if (parameters.isMediaSource)
        return MediaPlayer::SupportsType::IsNotSupported;
#endif
#if ENABLE(MEDIA_STREAM)
    if (parameters.isMediaStream)
        return MediaPlayer::SupportsType::IsNotSupported;
#endif

    auto supported = AVAssetMIMETypeCache::singleton().canDecodeType(parameters.type.raw());
    if (supported != MediaPlayer::SupportsType::IsSupported)
        return supported;

    if (!contentTypeMeetsHardwareDecodeRequirements(parameters.type, parameters.contentTypesRequiringHardwareSupport))
        return MediaPlayer::SupportsType::IsNotSupported;

    return MediaPlayer::SupportsType::IsSupported;
}

bool MediaPlayerPrivateAVFoundationObjC::supportsKeySystem(const String& keySystem, const String& mimeType)
{
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (!keySystem.isEmpty()) {
        // "Clear Key" is only supported with HLS:
        if (equalIgnoringASCIICase(keySystem, "org.w3c.clearkey") && !mimeType.isEmpty() && !equalIgnoringASCIICase(mimeType, "application/x-mpegurl"))
            return false;

        if (!keySystemIsSupported(keySystem))
            return false;

        if (!mimeType.isEmpty() && AVAssetMIMETypeCache::singleton().canDecodeType(mimeType) == MediaPlayerEnums::SupportsType::IsNotSupported)
            return false;

        return true;
    }
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(mimeType);
#endif
    return false;
}

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
        keyURIArray->setRange(reinterpret_cast<const UniChar*>(StringView(keyURI).upconvertedCharacters().get()), keyURI.length() / sizeof(unsigned char), 0);

        auto initData = Uint8Array::create(WTFMove(initDataBuffer), 0, byteLength);
        player()->keyNeeded(initData.ptr());
#if ENABLE(ENCRYPTED_MEDIA)
        if (!player()->shouldContinueAfterKeyNeeded())
            return true;
#endif
#endif

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
        if (m_cdmInstance) {
            avRequest.contentInformationRequest.contentType = AVStreamingKeyDeliveryContentKeyType;
            [avRequest finishLoading];
            return true;
        }

        RetainPtr<NSData> keyURIData = [keyURI dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES];
        m_keyID = SharedBuffer::create(keyURIData.get());
        player()->initializationDataEncountered("skd"_s, m_keyID->tryCreateArrayBuffer());
        setWaitingForKey(true);
#endif
        m_keyURIToRequestMap.set(keyURI, avRequest);

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

        player()->keyNeeded(initData.ptr());

        if (!player()->shouldContinueAfterKeyNeeded())
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

bool MediaPlayerPrivateAVFoundationObjC::isAvailable()
{
    return PAL::isAVFoundationFrameworkAvailable() && isCoreMediaFrameworkAvailable();
}

MediaTime MediaPlayerPrivateAVFoundationObjC::mediaTimeForTimeValue(const MediaTime& timeValue) const
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

#if ENABLE(VIDEO_PRESENTATION_MODE)
    // Do not attempt to change the video gravity while in full screen mode.
    // See setVideoFullscreenGravity().
    if (m_videoLayerManager->videoFullscreenLayer())
        return;
#endif

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
                    haveCCTrack = true;
                } else if ([mediaType isEqualToString:AVMediaTypeMetadata]) {
                    hasMetaData = true;
                }
            }
        }

        updateAudioTracks();
        updateVideoTracks();

        hasAudio |= (m_audibleGroup && m_audibleGroup->selectedOption());
        hasVideo |= (m_visualGroup && m_visualGroup->selectedOption());

        // HLS streams will occasionally recreate all their tracks; during seek and after
        // buffering policy changes. "debounce" notifications which result in no enabled
        // audio tracks by also taking AVPlayerItem.hasEnabledAudio into account when determining
        // whethere there is any audio present.
        hasAudio |= m_cachedHasEnabledAudio;

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

    AVMediaSelectionGroup *legibleGroup = safeMediaSelectionGroupForLegibleMedia();
    if (legibleGroup && m_cachedTracks) {
        hasCaptions = [[PAL::getAVMediaSelectionGroupClass() playableMediaSelectionOptionsFromArray:[legibleGroup options]] count];
        if (hasCaptions)
            processMediaSelectionOptions();
    }

    setHasClosedCaptions(hasCaptions);

    ALWAYS_LOG(LOGIDENTIFIER, "has video = ", hasVideo(), ", has audio = ", hasAudio(), ", has captions = ", hasClosedCaptions());

    sizeChanged();

    if (primaryAudioTrackLanguage != languageOfPrimaryAudioTrack())
        characteristicsChanged();

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    if (m_provider)
        m_provider->setAudioTrack(firstEnabledTrack(safeAVAssetTracksForAudibleMedia()));
#endif

    setDelayCharacteristicsChangedNotification(false);
}

void MediaPlayerPrivateAVFoundationObjC::updateRotationSession()
{
    AffineTransform finalTransform = m_avAsset.get().preferredTransform;
    FloatSize naturalSize;
    if (auto* firstEnabledVideoTrack = firstEnabledTrack([m_avAsset.get() tracksWithMediaCharacteristic:AVMediaCharacteristicVisual])) {
        naturalSize = FloatSize(firstEnabledVideoTrack.naturalSize);
        finalTransform *= firstEnabledVideoTrack.preferredTransform;
    }

    if (finalTransform.isIdentity()) {
        m_imageRotationSession = nullptr;
        return;
    }

    if (m_imageRotationSession
        && m_imageRotationSession->transform()
        && m_imageRotationSession->transform().value() == finalTransform
        && m_imageRotationSession->size() == naturalSize)
        return;

    m_imageRotationSession = makeUnique<ImageRotationSessionVT>(WTFMove(finalTransform), naturalSize, ImageRotationSessionVT::IsCGImageCompatible::Yes);
}

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

template <typename RefT, typename PassRefT>
void determineChangedTracksFromNewTracksAndOldItems(MediaSelectionGroupAVFObjC* group, Vector<RefT>& oldItems, const Vector<String>& characteristics, RefT (*itemFactory)(MediaSelectionOptionAVFObjC&), MediaPlayer* player, void (MediaPlayer::*removedFunction)(PassRefT), void (MediaPlayer::*addedFunction)(PassRefT))
{
    group->updateOptions(characteristics);

    ListHashSet<RefPtr<MediaSelectionOptionAVFObjC>> newSelectionOptions;
    for (auto& option : group->options()) {
        if (!option)
            continue;
        AVMediaSelectionOption* avOption = option->avMediaSelectionOption();
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

void MediaPlayerPrivateAVFoundationObjC::updateAudioTracks()
{
    size_t count = m_audioTracks.size();

    Vector<String> characteristics = player()->preferredAudioCharacteristics();
    if (!m_audibleGroup) {
        if (AVMediaSelectionGroup *group = safeMediaSelectionGroupForAudibleMedia())
            m_audibleGroup = MediaSelectionGroupAVFObjC::create(m_avPlayerItem.get(), group, characteristics);
    }

    if (m_audibleGroup)
        determineChangedTracksFromNewTracksAndOldItems(m_audibleGroup.get(), m_audioTracks, characteristics, &AudioTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeAudioTrack, &MediaPlayer::addAudioTrack);
    else
        determineChangedTracksFromNewTracksAndOldItems(m_cachedTracks.get(), AVMediaTypeAudio, m_audioTracks, &AudioTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeAudioTrack, &MediaPlayer::addAudioTrack);

    for (auto& track : m_audioTracks)
        track->resetPropertiesFromTrack();

    ALWAYS_LOG(LOGIDENTIFIER, "track count was ", count, ", is ", m_audioTracks.size());
}

void MediaPlayerPrivateAVFoundationObjC::updateVideoTracks()
{
    size_t count = m_videoTracks.size();

    determineChangedTracksFromNewTracksAndOldItems(m_cachedTracks.get(), AVMediaTypeVideo, m_videoTracks, &VideoTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeVideoTrack, &MediaPlayer::addVideoTrack);

    if (!m_visualGroup) {
        if (AVMediaSelectionGroup *group = safeMediaSelectionGroupForVisualMedia())
            m_visualGroup = MediaSelectionGroupAVFObjC::create(m_avPlayerItem.get(), group, Vector<String>());
    }

    if (m_visualGroup)
        determineChangedTracksFromNewTracksAndOldItems(m_visualGroup.get(), m_videoTracks, Vector<String>(), &VideoTrackPrivateAVFObjC::create, player(), &MediaPlayer::removeVideoTrack, &MediaPlayer::addVideoTrack);

    for (auto& track : m_audioTracks)
        track->resetPropertiesFromTrack();

    ALWAYS_LOG(LOGIDENTIFIER, "track count was ", count, ", is ", m_videoTracks.size());
}

bool MediaPlayerPrivateAVFoundationObjC::requiresTextTrackRepresentation() const
{
    return m_videoLayerManager->requiresTextTrackRepresentation();
}

void MediaPlayerPrivateAVFoundationObjC::syncTextTrackBounds()
{
    m_videoLayerManager->syncTextTrackBounds();
}

void MediaPlayerPrivateAVFoundationObjC::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    auto* representationLayer = representation ? representation->platformLayer() : nil;
    m_videoLayerManager->setTextTrackRepresentationLayer(representationLayer);
}

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

    updateRotationSession();
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

void MediaPlayerPrivateAVFoundationObjC::createVideoOutput()
{
    INFO_LOG(LOGIDENTIFIER);

    if (!m_avPlayerItem || m_videoOutput)
        return;

    m_videoOutput = adoptNS([PAL::allocAVPlayerItemVideoOutputInstance() initWithPixelBufferAttributes:nil]);
    ASSERT(m_videoOutput);
    if (!m_videoOutput) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVPlayerItemVideoOutput initWithPixelBufferAttributes:] failed!");
        return;
    }

    m_videoOutputDelegate = adoptNS([[WebCoreAVFPullDelegate alloc] init]);
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

    if (m_imageRotationSession)
        m_lastPixelBuffer = m_imageRotationSession->rotate(m_lastPixelBuffer.get());

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
        NSDictionary *attributes = @{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };
        m_pixelBufferConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)attributes);
    }

    MonotonicTime start = MonotonicTime::now();

    m_lastImage = NativeImage::create(m_pixelBufferConformer->createImageFromPixelBuffer(m_lastPixelBuffer.get()));

    INFO_LOG(LOGIDENTIFIER, "creating buffer took ", (MonotonicTime::now() - start).seconds());
}

void MediaPlayerPrivateAVFoundationObjC::paintWithVideoOutput(GraphicsContext& context, const FloatRect& outputRect)
{
    // It's crucial to not wait synchronously for the next image. Videos that
    // come down this path are performing slow-case software uploads, and such
    // videos may not return metadata in a timely fashion. Use the most recently
    // available pixel buffer, if any.
    updateLastImage();
    if (!m_lastImage)
        return;

    AVAssetTrack* firstEnabledVideoTrack = firstEnabledTrack([m_avAsset.get() tracksWithMediaCharacteristic:AVMediaCharacteristicVisual]);
    if (!firstEnabledVideoTrack)
        return;

    INFO_LOG(LOGIDENTIFIER);

    FloatRect imageRect { FloatPoint::zero(), m_lastImage->size() };
    context.drawNativeImage(*m_lastImage, imageRect.size(), outputRect, imageRect);

    // If we have created an AVAssetImageGenerator in the past due to m_videoOutput not having an available
    // video frame, destroy it now that it is no longer needed.
    if (m_imageGenerator)
        destroyImageGenerator();

}

RetainPtr<CVPixelBufferRef> MediaPlayerPrivateAVFoundationObjC::pixelBufferForCurrentTime()
{
    updateLastPixelBuffer();
    if (!m_lastPixelBuffer)
        return nullptr;

    return m_lastPixelBuffer;
}

RefPtr<NativeImage> MediaPlayerPrivateAVFoundationObjC::nativeImageForCurrentTime()
{
    updateLastImage(UpdateType::UpdateSynchronously);
    return m_lastImage;
}

void MediaPlayerPrivateAVFoundationObjC::waitForVideoOutputMediaDataWillChange()
{
    [m_videoOutput requestNotificationOfMediaDataChangeWithAdvanceInterval:0];

    // Wait for 1 second.
    bool satisfied = [m_videoOutputDelegate semaphore].waitFor(1_s);
    if (!satisfied)
        ERROR_LOG(LOGIDENTIFIER, "timed out");
}

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

std::unique_ptr<LegacyCDMSession> MediaPlayerPrivateAVFoundationObjC::createSession(const String& keySystem, LegacyCDMSessionClient* client)
{
    if (!keySystemIsSupported(keySystem))
        return nullptr;
    auto session = makeUnique<CDMSessionAVFoundationObjC>(this, client);
    m_session = makeWeakPtr(*session);
    return WTFMove(session);
}

#endif

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

NSArray* MediaPlayerPrivateAVFoundationObjC::safeAVAssetTracksForAudibleMedia()
{
    if (!m_avAsset)
        return nil;

    if ([m_avAsset.get() statusOfValueForKey:@"tracks" error:NULL] != AVKeyValueStatusLoaded)
        return nil;

    return [m_avAsset tracksWithMediaCharacteristic:AVMediaCharacteristicAudible];
}

bool MediaPlayerPrivateAVFoundationObjC::hasLoadedMediaSelectionGroups()
{
    if (!m_avAsset)
        return false;

    if ([m_avAsset.get() statusOfValueForKey:@"availableMediaCharacteristicsWithMediaSelectionOptions" error:NULL] != AVKeyValueStatusLoaded)
        return false;

    return true;
}

AVMediaSelectionGroup* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForLegibleMedia()
{
    if (!hasLoadedMediaSelectionGroups())
        return nil;

    return [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicLegible];
}

AVMediaSelectionGroup* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForAudibleMedia()
{
    if (!hasLoadedMediaSelectionGroups())
        return nil;

    return [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicAudible];
}

AVMediaSelectionGroup* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForVisualMedia()
{
    if (!hasLoadedMediaSelectionGroups())
        return nil;

    return [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicVisual];
}

void MediaPlayerPrivateAVFoundationObjC::processMediaSelectionOptions()
{
    AVMediaSelectionGroup *legibleGroup = safeMediaSelectionGroupForLegibleMedia();
    if (!legibleGroup) {
        INFO_LOG(LOGIDENTIFIER, "no mediaSelectionGroup");
        return;
    }

    // We enabled automatic media selection because we want alternate audio tracks to be enabled/disabled automatically,
    // but set the selected legible track to nil so text tracks will not be automatically configured.
    if (!m_textTracks.size())
        [m_avPlayerItem.get() selectMediaOption:nil inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];

    Vector<RefPtr<InbandTextTrackPrivateAVF>> removedTextTracks = m_textTracks;
    NSArray *legibleOptions = [PAL::getAVMediaSelectionGroupClass() playableMediaSelectionOptionsFromArray:[legibleGroup options]];
    for (AVMediaSelectionOption *option in legibleOptions) {
        bool newTrack = true;
        for (unsigned i = removedTextTracks.size(); i > 0; --i) {
            if (removedTextTracks[i - 1]->textTrackCategory() == InbandTextTrackPrivateAVF::LegacyClosedCaption)
                continue;
            
            RetainPtr<AVMediaSelectionOption> currentOption;
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

        m_textTracks.append(InbandTextTrackPrivateAVFObjC::create(this, option, InbandTextTrackPrivate::CueFormat::Generic));
    }

    processNewAndRemovedTextTracks(removedTextTracks);
}

void MediaPlayerPrivateAVFoundationObjC::processMetadataTrack()
{
    if (m_metadataTrack)
        return;

    m_metadataTrack = InbandMetadataTextTrackPrivateAVF::create(InbandTextTrackPrivate::Kind::Metadata, InbandTextTrackPrivate::CueFormat::Data);
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
#if ENABLE(AVF_CAPTIONS)
        else if (track->textTrackCategory() == InbandTextTrackPrivateAVF::OutOfBand)
            [m_avPlayerItem.get() selectMediaOption:static_cast<OutOfBandTextTrackPrivateAVF*>(track)->mediaSelectionOption() inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
#endif
        else
            [m_avPlayerItem.get() selectMediaOption:static_cast<InbandTextTrackPrivateAVFObjC*>(track)->mediaSelectionOption() inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
    } else {
        [m_avPlayerItem.get() selectMediaOption:0 inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
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

    // If AVFoundation has an audible group, return the language of the currently selected audible option.
    AVMediaSelectionGroup *audibleGroup = [m_avAsset.get() mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicAudible];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    AVMediaSelectionOption *currentlySelectedAudibleOption = [m_avPlayerItem.get() selectedMediaOptionInMediaSelectionGroup:audibleGroup];
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (currentlySelectedAudibleOption) {
        m_languageOfPrimaryAudioTrack = [[currentlySelectedAudibleOption locale] localeIdentifier];
        INFO_LOG(LOGIDENTIFIER, "language of selected audible option ", m_languageOfPrimaryAudioTrack);

        return m_languageOfPrimaryAudioTrack;
    }

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
// AVPlayerItem.tracks is empty during AirPlay so if AirPlay is activated immediately
// after the item is created, we don't know if it has audio or video and state reported
// to the WebMediaSessionManager is incorrect. AirPlay can't actually be active if an item
// doesn't have audio or video, so lie during AirPlay.

bool MediaPlayerPrivateAVFoundationObjC::hasVideo() const
{
    if (isCurrentPlaybackTargetWireless())
        return true;

    return MediaPlayerPrivateAVFoundation::hasVideo();
}

bool MediaPlayerPrivateAVFoundationObjC::hasAudio() const
{
    if (isCurrentPlaybackTargetWireless())
        return true;

    return MediaPlayerPrivateAVFoundation::hasAudio();
}

bool MediaPlayerPrivateAVFoundationObjC::isCurrentPlaybackTargetWireless() const
{
    bool wirelessTarget = false;

#if !PLATFORM(IOS_FAMILY)
    if (m_playbackTarget) {
        if (m_playbackTarget->targetType() == MediaPlaybackTarget::TargetType::AVFoundation)
            wirelessTarget = m_avPlayer && m_avPlayer.get().externalPlaybackActive;
        else
            wirelessTarget = m_shouldPlayToPlaybackTarget && m_playbackTarget->hasActiveRoute();
    }
#else
    wirelessTarget = m_avPlayer && m_avPlayer.get().externalPlaybackActive;
#endif

    INFO_LOG(LOGIDENTIFIER, wirelessTarget);

    return wirelessTarget;
}

MediaPlayer::WirelessPlaybackTargetType MediaPlayerPrivateAVFoundationObjC::wirelessPlaybackTargetType() const
{
    if (!m_avPlayer)
        return MediaPlayer::WirelessPlaybackTargetType::TargetTypeNone;

#if PLATFORM(IOS_FAMILY)
    if (!PAL::isAVFoundationFrameworkAvailable())
        return MediaPlayer::WirelessPlaybackTargetType::TargetTypeNone;

    switch ([m_avPlayer externalPlaybackType]) {
    case AVPlayerExternalPlaybackTypeNone:
        return MediaPlayer::WirelessPlaybackTargetType::TargetTypeNone;
    case AVPlayerExternalPlaybackTypeAirPlay:
        return MediaPlayer::WirelessPlaybackTargetType::TargetTypeAirPlay;
    case AVPlayerExternalPlaybackTypeTVOut:
        return MediaPlayer::WirelessPlaybackTargetType::TargetTypeTVOut;
    }

    ASSERT_NOT_REACHED();
    return MediaPlayer::WirelessPlaybackTargetType::TargetTypeNone;

#else
    return MediaPlayer::WirelessPlaybackTargetType::TargetTypeAirPlay;
#endif
}
    
#if PLATFORM(IOS_FAMILY)
static NSString *exernalDeviceDisplayNameForPlayer(AVPlayer *player)
{
#if HAVE(CELESTIAL)
    if (!PAL::isAVFoundationFrameworkAvailable())
        return nil;

    if (auto context = OutputContext::sharedAudioPresentationOutputContext())
        return context->deviceName();

    if (player.externalPlaybackType != AVPlayerExternalPlaybackTypeAirPlay)
        return nil;

    auto pickableRoutes = adoptCF(MRMediaRemoteCopyPickableRoutes());
    if (!pickableRoutes || !CFArrayGetCount(pickableRoutes.get()))
        return nil;

    NSString *displayName = nil;
    for (NSDictionary *pickableRoute in (__bridge NSArray *)pickableRoutes.get()) {
        if (![pickableRoute[AVController_RouteDescriptionKey_RouteCurrentlyPicked] boolValue])
            continue;

        displayName = pickableRoute[AVController_RouteDescriptionKey_RouteName];

        NSString *routeName = pickableRoute[AVController_RouteDescriptionKey_AVAudioRouteName];
        if (![routeName isEqualToString:@"Speaker"] && ![routeName isEqualToString:@"HDMIOutput"])
            break;

        // The route is a speaker or HDMI out, override the name to be the localized device model.
        NSString *localizedDeviceModelName = localizedDeviceModel();

        // In cases where a route with that name already exists, prefix the name with the model.
        BOOL includeLocalizedDeviceModelName = NO;
        for (NSDictionary *otherRoute in (__bridge NSArray *)pickableRoutes.get()) {
            if (otherRoute == pickableRoute)
                continue;

            if ([otherRoute[AVController_RouteDescriptionKey_RouteName] rangeOfString:displayName].location != NSNotFound) {
                includeLocalizedDeviceModelName = YES;
                break;
            }
        }

        if (includeLocalizedDeviceModelName)
            displayName = [NSString stringWithFormat:@"%@ %@", localizedDeviceModelName, displayName];
        else
            displayName = localizedDeviceModelName;

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
    INFO_LOG(LOGIDENTIFIER, !m_allowsWirelessVideoPlayback);

    return !m_allowsWirelessVideoPlayback;
}

void MediaPlayerPrivateAVFoundationObjC::setWirelessVideoPlaybackDisabled(bool disabled)
{
    INFO_LOG(LOGIDENTIFIER, disabled);
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

    m_outputContext = m_playbackTarget->targetType() == MediaPlaybackTarget::TargetType::AVFoundation ? toMediaPlaybackTargetCocoa(m_playbackTarget.get())->targetContext().outputContext() : nullptr;

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

    INFO_LOG(LOGIDENTIFIER, shouldPlay);

    if (m_playbackTarget->targetType() == MediaPlaybackTarget::TargetType::AVFoundation) {
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

    ASSERT(m_playbackTarget->targetType() == MediaPlaybackTarget::TargetType::Mock);

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
#if PLATFORM(IOS_FAMILY) && !PLATFORM(APPLETV)
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

    // Setting the pitch algorithm here causes tests to fail on Mojave; revert this change
    // for <= Mojave builds.
#if !(PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 101400)
    // FIXME(rdar://72829354): Remove after AVFoundation radar is fixed.
    if (status == AVPlayerItemStatusReadyToPlay)
        [m_avPlayerItem setAudioTimePitchAlgorithm:audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player()->pitchCorrectionAlgorithm(), player()->preservesPitch(), m_requestedRate)];
#endif

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

void MediaPlayerPrivateAVFoundationObjC::setBufferingPolicy(MediaPlayer::BufferingPolicy policy)
{
    ALWAYS_LOG(LOGIDENTIFIER, policy);

    if (m_bufferingPolicy == policy)
        return;

    m_bufferingPolicy = policy;
    
    if (!m_avPlayer)
        return;

#if HAVE(AVPLAYER_RESOURCE_CONSERVATION_LEVEL)
    static_assert(static_cast<size_t>(MediaPlayer::BufferingPolicy::Default) == AVPlayerResourceConservationLevelNone, "MediaPlayer::BufferingPolicy::Default is not AVPlayerResourceConservationLevelNone as expected");
    static_assert(static_cast<size_t>(MediaPlayer::BufferingPolicy::LimitReadAhead) == AVPlayerResourceConservationLevelReduceReadAhead, "MediaPlayer::BufferingPolicy::LimitReadAhead is not AVPlayerResourceConservationLevelReduceReadAhead as expected");
    static_assert(static_cast<size_t>(MediaPlayer::BufferingPolicy::MakeResourcesPurgeable) == AVPlayerResourceConservationLevelReuseActivePlayerResources, "MediaPlayer::BufferingPolicy::MakeResourcesPurgeable is not AVPlayerResourceConservationLevelReuseActivePlayerResources as expected");
    static_assert(static_cast<size_t>(MediaPlayer::BufferingPolicy::PurgeResources) == AVPlayerResourceConservationLevelRecycleBuffer, "MediaPlayer::BufferingPolicy::PurgeResources is not AVPlayerResourceConservationLevelRecycleBuffer as expected");

    if ([m_avPlayer respondsToSelector:@selector(setResourceConservationLevelWhilePaused:)]) {
        m_avPlayer.get().resourceConservationLevelWhilePaused = static_cast<AVPlayerResourceConservationLevel>(policy);
        updateStates();
        return;
    }
#endif

    switch (policy) {
    case MediaPlayer::BufferingPolicy::Default:
        setAVPlayerItem(m_avPlayerItem.get());
        break;
    case MediaPlayer::BufferingPolicy::LimitReadAhead:
    case MediaPlayer::BufferingPolicy::MakeResourcesPurgeable:
        setAVPlayerItem(nil);
        break;
    case MediaPlayer::BufferingPolicy::PurgeResources:
        setAVPlayerItem(nil);
        setAVPlayerItem(m_avPlayerItem.get());
        break;
    }

    updateStates();
}

#if ENABLE(DATACUE_VALUE)
static const AtomString& metadataType(NSString *avMetadataKeySpace)
{
    static MainThreadNeverDestroyed<const AtomString> quickTimeUserData("com.apple.quicktime.udta", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> isoUserData("org.mp4ra", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> quickTimeMetadata("com.apple.quicktime.mdta", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> iTunesMetadata("com.apple.itunes", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> id3Metadata("org.id3", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> hlsDateRangeMetadata("com.apple.quicktime.HLS", AtomString::ConstructFromLiteral);

    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceQuickTimeUserData])
        return quickTimeUserData;
    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceISOUserData])
        return isoUserData;
    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceQuickTimeMetadata])
        return quickTimeMetadata;
    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceiTunes])
        return iTunesMetadata;
    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceID3])
        return id3Metadata;
    if ([avMetadataKeySpace isEqualToString:AVMetadataKeySpaceHLSDateRange])
        return hlsDateRangeMetadata;

    return emptyAtom();
}
#endif

void MediaPlayerPrivateAVFoundationObjC::metadataGroupDidArrive(const RetainPtr<NSArray>& groups, const MediaTime& currentTime)
{
    for (AVDateRangeMetadataGroup *group in groups.get()) {
        auto groupStartDate = group.startDate.timeIntervalSince1970;
        auto groupEndDate = group.endDate.timeIntervalSince1970;
        auto groupDuration = MediaTime::createWithDouble(groupEndDate - groupStartDate);

        // A manifest without a #EXT-X-PROGRAM-DATE-TIME won't have a start date, so if currentDate
        // is zero assume the metadata items start at the beginning of the stream.
        double groupStartTime = 0;
        if (auto currentDate = m_avPlayerItem.get().currentDate.timeIntervalSince1970)
            groupStartTime = groupStartDate - currentDate - currentTime.toDouble();

        auto groupCopy = adoptNS([[NSMutableArray alloc] init]);
        for (AVMetadataItem *item in group.items) {
            RetainPtr<AVMutableMetadataItem> itemCopy = adoptNS([item mutableCopy]);
            if (!CMTIME_IS_VALID(itemCopy.get().time))
                itemCopy.get().time = CMTimeMakeWithSeconds(groupStartTime, MediaTime::DefaultTimeScale);
            if (!CMTIME_IS_VALID(itemCopy.get().duration))
                itemCopy.get().duration = toCMTime(groupDuration);
            [groupCopy addObject:itemCopy.get()];
        }

        metadataDidArrive(groupCopy, currentTime);
    }
}

void MediaPlayerPrivateAVFoundationObjC::metadataDidArrive(const RetainPtr<NSArray>& metadata, const MediaTime& mediaTime)
{
    m_currentMetaData = metadata && ![metadata isKindOfClass:[NSNull class]] ? metadata : nil;

    INFO_LOG(LOGIDENTIFIER, "adding ", m_currentMetaData ? [m_currentMetaData.get() count] : 0, " at time ", mediaTime);

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
    for (AVMetadataItem *item in m_currentMetaData.get()) {
        MediaTime start = std::max(PAL::toMediaTime(item.time), MediaTime::zeroTime());
        if (start < earliestStartTime)
            earliestStartTime = start;
    }
    m_metadataTrack->updatePendingCueEndTimes(earliestStartTime);

    for (AVMetadataItem *item in m_currentMetaData.get()) {
        MediaTime start;
        if (CMTIME_IS_VALID(item.time))
            start = std::max(PAL::toMediaTime(item.time), MediaTime::zeroTime());

        MediaTime end = MediaTime::positiveInfiniteTime();
        auto duration = item.duration;
        if (CMTIME_IS_VALID(duration) && CMTimeGetSeconds(duration) > 0.001)
            end = start + PAL::toMediaTime(duration);

        AtomString type = nullAtom();
        if (item.keySpace)
            type = metadataType(item.keySpace);

        m_metadataTrack->addDataCue(start, end, SerializedPlatformDataCue::create({ WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC, item }), type);
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
    if (m_cachedRate == rate)
        return;

    m_cachedRate = rate;
    updateStates();
    rateChanged();
}

void MediaPlayerPrivateAVFoundationObjC::timeControlStatusDidChange(int timeControlStatus)
{
    if (m_cachedTimeControlStatus == timeControlStatus)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, static_cast<AVPlayerTimeControlStatus>(timeControlStatus), ", observing = ", m_shouldObserveTimeControlStatus);

    if (!m_shouldObserveTimeControlStatus)
        return;

    m_cachedTimeControlStatus = timeControlStatus;
    rateChanged();
    m_wallClockAtCachedCurrentTime = WTF::nullopt;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (!isCurrentPlaybackTargetWireless())
        return;

    bool playerIsPlaying = m_cachedTimeControlStatus != AVPlayerTimeControlStatusPaused;
    if (playerIsPlaying != m_requestedPlaying) {
        m_requestedPlaying = playerIsPlaying;
        player()->playbackStateChanged();
    }
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
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
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

bool MediaPlayerPrivateAVFoundationObjC::performTaskAtMediaTime(WTF::Function<void()>&& task, const MediaTime& time)
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
    if (shouldObserve) {
        [m_avPlayer addObserver:m_objcObserver.get() forKeyPath:@"timeControlStatus" options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextPlayer];
        timeControlStatusDidChange(m_avPlayer.get().timeControlStatus);
    } else {
BEGIN_BLOCK_OBJC_EXCEPTIONS
        [m_avPlayer removeObserver:m_objcObserver.get() forKeyPath:@"timeControlStatus"];
END_BLOCK_OBJC_EXCEPTIONS
    }
}

void MediaPlayerPrivateAVFoundationObjC::setPreferredDynamicRangeMode(DynamicRangeMode mode)
{
#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
    if (m_avPlayer && [m_avPlayer respondsToSelector:@selector(setVideoRangeOverride:)])
        m_avPlayer.get().videoRangeOverride = convertDynamicRangeModeEnumToAVVideoRange(mode);
#else
    UNUSED_PARAM(mode);
#endif
}

void MediaPlayerPrivateAVFoundationObjC::audioOutputDeviceChanged()
{
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    if (!m_avPlayer || !player())
        return;
    auto deviceId = player()->audioOutputDeviceId();
    if (deviceId.isEmpty())
        m_avPlayer.get().audioOutputDeviceUniqueID = nil;
    else
        m_avPlayer.get().audioOutputDeviceUniqueID = deviceId;
#endif
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
    ensureOnMainThread([self, strongSelf = retainPtr(self)] {
        m_taskQueue.enqueueTask([player = m_player] {
            if (player)
                player->metadataLoaded();
        });
    });
}

- (void)didEnd:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    ensureOnMainThread([self, strongSelf = retainPtr(self)] {
        m_taskQueue.enqueueTask([player = m_player] {
            if (player)
                player->didEnd();
        });
    });
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(MediaPlayerAVFoundationObservationContext)context
{
    ensureOnMainThread([self, strongSelf = retainPtr(self), keyPath = retainPtr(keyPath), change = retainPtr(change), object = retainPtr(object), context]() mutable {
        m_taskQueue.enqueueTask([player = m_player, keyPath = WTFMove(keyPath), change = WTFMove(change), object = WTFMove(object), context] {
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
                else if ([keyPath isEqualToString:@"canPlayFastReverse"])
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

            if (player->logger().willLog(player->logChannel(), WTFLogLevel::Debug) && !([keyPath isEqualToString:@"loadedTimeRanges"] || [keyPath isEqualToString:@"seekableTimeRanges"])) {
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
        });
    });
}

- (void)legibleOutput:(id)output didOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime
{
    UNUSED_PARAM(output);

    ensureOnMainThread([self, strongSelf = retainPtr(self), strings = retainPtr(strings), nativeSamples = retainPtr(nativeSamples), itemTime]() mutable {
        m_taskQueue.enqueueTask([player = m_player, strings = WTFMove(strings), nativeSamples = WTFMove(nativeSamples), itemTime] {
            if (!player)
                return;
            MediaTime time = std::max(PAL::toMediaTime(itemTime), MediaTime::zeroTime());
            player->processCue(strings.get(), nativeSamples.get(), time);
        });
    });
}

- (void)outputSequenceWasFlushed:(id)output
{
    UNUSED_PARAM(output);

    ensureOnMainThread([self, strongSelf = retainPtr(self)] {
        m_taskQueue.enqueueTask([player = m_player] {
            if (player)
                player->flushCues();
        });
    });
}

- (void)metadataOutput:(AVPlayerItemMetadataOutput *)output didOutputTimedMetadataGroups:(NSArray<AVTimedMetadataGroup *> *)metadataGroups fromPlayerItemTrack:(AVPlayerItemTrack *)track
{
    ASSERT(isMainThread());
    UNUSED_PARAM(output);
    UNUSED_PARAM(track);

    if (!m_player || !metadataGroups)
        return;

    m_taskQueue.enqueueTask([player = m_player, metadataGroups = retainPtr(metadataGroups), currentTime = m_player->currentMediaTime()] {
        if (!player)
            return;

        for (AVTimedMetadataGroup *group in metadataGroups.get())
            player->metadataDidArrive(retainPtr(group.items), currentTime);
    });
}

- (void)metadataCollector:(AVPlayerItemMetadataCollector *)metadataCollector didCollectDateRangeMetadataGroups:(NSArray<AVDateRangeMetadataGroup *> *)metadataGroups indexesOfNewGroups:(NSIndexSet *)indexesOfNewGroups indexesOfModifiedGroups:(NSIndexSet *)indexesOfModifiedGroups
{
    ASSERT(isMainThread());
    UNUSED_PARAM(metadataCollector);
    UNUSED_PARAM(indexesOfNewGroups);
    UNUSED_PARAM(indexesOfModifiedGroups);

    if (!m_player || !metadataGroups)
        return;

    m_taskQueue.enqueueTask([player = m_player, metadataGroups = retainPtr(metadataGroups), currentTime = m_player->currentMediaTime()] {
        if (!player)
            return;

        player->metadataGroupDidArrive(metadataGroups, currentTime);
    });
}
@end

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

    ensureOnMainThread([self, strongSelf = retainPtr(self), loadingRequest = retainPtr(loadingRequest)]() mutable {
        m_taskQueue.enqueueTask([player = m_player, loadingRequest = WTFMove(loadingRequest)] {
            if (!player) {
                [loadingRequest finishLoadingWithError:nil];
                return;
            }

            if (!player->shouldWaitForLoadingOfResource(loadingRequest.get()))
                [loadingRequest finishLoadingWithError:nil];
        });
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
    ensureOnMainThread([self, strongSelf = retainPtr(self), loadingRequest = retainPtr(loadingRequest)]() mutable {
        m_taskQueue.enqueueTask([player = m_player, loadingRequest = WTFMove(loadingRequest)] {
            if (player)
                player->didCancelLoadingRequest(loadingRequest.get());
        });
    });
}

@end

@implementation WebCoreAVFPullDelegate

@synthesize semaphore = m_semaphore;

- (void)outputMediaDataWillChange:(AVPlayerItemVideoOutput *)output
{
    UNUSED_PARAM(output);
    m_semaphore.signal();
}

- (void)outputSequenceWasFlushed:(AVPlayerItemVideoOutput *)output
{
    UNUSED_PARAM(output);
    // No-op.
}

@end

#endif
