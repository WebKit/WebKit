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
#import "CVUtilities.h"
#import "ColorSpaceCG.h"
#import "ContentTypeUtilities.h"
#import "Cookie.h"
#import "DeprecatedGlobalSettings.h"
#import "FloatConversion.h"
#import "FourCC.h"
#import "GraphicsContext.h"
#import "IOSurface.h"
#import "ImageRotationSessionVT.h"
#import "InbandChapterTrackPrivateAVFObjC.h"
#import "InbandMetadataTextTrackPrivateAVF.h"
#import "InbandTextTrackPrivateAVFObjC.h"
#import "Logging.h"
#import "MediaPlaybackTargetCocoa.h"
#import "MediaPlaybackTargetMock.h"
#import "MediaSelectionGroupAVFObjC.h"
#import "MediaSessionManagerCocoa.h"
#import "OutOfBandTextTrackPrivateAVF.h"
#import "PixelBufferConformerCV.h"
#import "PlatformMediaResourceLoader.h"
#import "PlatformScreen.h"
#import "PlatformTextTrack.h"
#import "PlatformTimeRanges.h"
#import "QueuedVideoOutput.h"
#import "RuntimeApplicationChecks.h"
#import "ScriptDisallowedScope.h"
#import "SecurityOrigin.h"
#import "SerializedPlatformDataCueMac.h"
#import "SharedBuffer.h"
#import "SourceBufferParserWebM.h"
#import "TextTrackRepresentation.h"
#import "UTIUtilities.h"
#import "VideoFrameCV.h"
#import "VideoLayerManagerObjC.h"
#import "VideoTrackPrivateAVFObjC.h"
#import "WebCoreAVFResourceLoader.h"
#import "WebCoreCALayerExtras.h"
#import "WebCoreNSURLExtras.h"
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
#import <pal/text/TextEncoding.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/FileSystem.h>
#import <wtf/ListHashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/URL.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/CString.h>
#import <wtf/threads/BinarySemaphore.h>

#if ENABLE(AVF_CAPTIONS)
#import "TextTrack.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import "MediaPlaybackTarget.h"
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
    int m_delayCallbacks;
    RefPtr<WorkQueue> m_backgroundQueue;
}
-(id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)callback;
-(void)disconnect;
-(void)metadataLoaded;
-(void)didEnd:(NSNotification *)notification;
-(void)chapterMetadataDidChange:(NSNotification *)notification;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(MediaPlayerAVFoundationObservationContext)context;
- (void)legibleOutput:(id)output didOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime;
- (void)outputSequenceWasFlushed:(id)output;
- (void)metadataCollector:(AVPlayerItemMetadataCollector *)metadataCollector didCollectDateRangeMetadataGroups:(NSArray<AVDateRangeMetadataGroup *> *)metadataGroups indexesOfNewGroups:(NSIndexSet *)indexesOfNewGroups indexesOfModifiedGroups:(NSIndexSet *)indexesOfModifiedGroups;
- (void)metadataOutput:(AVPlayerItemMetadataOutput *)output didOutputTimedMetadataGroups:(NSArray<AVTimedMetadataGroup *> *)groups fromPlayerItemTrack:(AVPlayerItemTrack *)track;
@end

@interface WebCoreAVFLoaderDelegate : NSObject<AVAssetResourceLoaderDelegate> {
    WeakPtr<MediaPlayerPrivateAVFoundationObjC> m_player;
}
- (id)initWithPlayer:(WeakPtr<MediaPlayerPrivateAVFoundationObjC>&&)player;
- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest;
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

static NSArray *assetMetadataKeyNames();
static NSArray *itemKVOProperties();
static NSArray *assetTrackMetadataKeyNames();
static NSArray *playerKVOProperties();

static dispatch_queue_t globalLoaderDelegateQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create("WebCoreAVFLoaderDelegate queue", DISPATCH_QUEUE_SERIAL);
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
public:
    Factory()
    {
        MediaSessionManagerCocoa::ensureCodecsRegistered();
    }

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

    auto fileType = FileSystem::fileTypeFollowingSymlinks(path);
    if (fileType && *fileType != FileSystem::FileType::Directory) {
        // Non-directory file already exists at the path location; bail.
        ASSERT_NOT_REACHED();
        return nil;
    }

    if (!fileType && !FileSystem::makeAllDirectories(path)) {
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
        URL keyAsURL { key };
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
        URL keyAsURL { key };
        if (keyAsURL.isValid()) {
            if (origins.contains(SecurityOriginData::fromURL(keyAsURL)))
                [assetCache removeEntryForKey:key];
        }
    }
}

MediaPlayerPrivateAVFoundationObjC::MediaPlayerPrivateAVFoundationObjC(MediaPlayer* player)
    : MediaPlayerPrivateAVFoundation(player)
    , m_videoLayerManager(makeUnique<VideoLayerManagerObjC>(logger(), logIdentifier()))
    , m_objcObserver(adoptNS([[WebCoreAVFMovieObserver alloc] initWithPlayer:*this]))
    , m_loaderDelegate(adoptNS([[WebCoreAVFLoaderDelegate alloc] initWithPlayer:*this]))
    , m_cachedItemStatus(MediaPlayerAVPlayerItemStatusDoesNotExist)
{
    m_muted = player->muted();
}

MediaPlayerPrivateAVFoundationObjC::~MediaPlayerPrivateAVFoundationObjC()
{
    weakPtrFactory().revokeAll();

    [[m_avAsset resourceLoader] setDelegate:nil queue:0];

    for (auto& pair : m_resourceLoaderMap) {
        m_targetQueue->dispatch([loader = pair.value] () mutable {
            loader->stopLoading();
        });
    }

    if (m_videoOutput)
        m_videoOutput->invalidate();

    if (m_videoLayer)
        destroyVideoLayer();

    cancelLoad();
}

void MediaPlayerPrivateAVFoundationObjC::cancelLoad()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    tearDownVideoRendering();

    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver disconnect];

    // Tell our observer to do nothing when our cancellation of pending loading calls its completion handler.
    setIgnoreLoadStateChanges(true);
    if (m_avAsset) {
        [m_avAsset cancelLoading];
        m_avAsset = nil;
    }

    clearTextTracks();

    if (m_legibleOutput) {
        if (m_avPlayerItem)
            [m_avPlayerItem removeOutput:m_legibleOutput.get()];
        m_legibleOutput = nil;
    }

    if (m_metadataCollector) {
        // FIXME - removeMediaDataCollector sometimes crashes, do not uncomment this until rdar://56617504 has been fixed.
        // if (m_avPlayerItem)
        //     [m_avPlayerItem removeMediaDataCollector:m_metadataCollector.get()];
        m_metadataCollector = nil;
    }

    if (m_metadataOutput) {
        if (m_avPlayerItem)
            [m_avPlayerItem removeOutput:m_metadataOutput.get()];
        m_metadataOutput = nil;
    }

    if (m_avPlayerItem) {
        for (NSString *keyName in itemKVOProperties())
            [m_avPlayerItem removeObserver:m_objcObserver.get() forKeyPath:keyName];
        
        m_avPlayerItem = nil;
    }
    if (m_avPlayer) {
        if (m_timeObserver)
            [m_avPlayer removeTimeObserver:m_timeObserver.get()];
        m_timeObserver = nil;

        for (NSString *keyName in playerKVOProperties())
            [m_avPlayer removeObserver:m_objcObserver.get() forKeyPath:keyName];

        setShouldObserveTimeControlStatus(false);

        [m_avPlayer replaceCurrentItemWithPlayerItem:nil];
#if !PLATFORM(IOS_FAMILY)
        [m_avPlayer setOutputContext:nil];
#endif

        if (m_currentTimeObserver)
            [m_avPlayer removeTimeObserver:m_currentTimeObserver.get()];
        m_currentTimeObserver = nil;

        if (m_videoFrameMetadataGatheringObserver) {
            [m_avPlayer removeTimeObserver:m_videoFrameMetadataGatheringObserver.get()];
            m_videoFrameMetadataGatheringObserver = nil;
        }

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
    m_cachedHasEnabledVideo = false;
    m_cachedPresentationSize = FloatSize();
    m_cachedDuration = MediaTime::zeroTime();

    for (AVPlayerItemTrack *track in m_cachedTracks.get())
        [track removeObserver:m_objcObserver.get() forKeyPath:@"enabled"];
    m_cachedTracks = nullptr;
    m_chapterTracks.clear();

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
    INFO_LOG(LOGIDENTIFIER);

    if (!m_avAsset || m_imageGenerator)
        return;

    m_imageGenerator = [PAL::getAVAssetImageGeneratorClass() assetImageGeneratorWithAsset:m_avAsset.get()];

    [m_imageGenerator setApertureMode:AVAssetImageGeneratorApertureModeCleanAperture];
    [m_imageGenerator setAppliesPreferredTrackTransform:YES];
    [m_imageGenerator setRequestedTimeToleranceBefore:PAL::kCMTimeZero];
    [m_imageGenerator setRequestedTimeToleranceAfter:PAL::kCMTimeZero];
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

    ensureOnMainThread([this, weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;

        if (!m_avPlayer || m_haveBeenAskedToCreateLayer)
            return;
        m_haveBeenAskedToCreateLayer = true;

        if (!m_videoLayer)
            createAVPlayerLayer();

        if (!m_videoOutput)
            createVideoOutput();
    });
}

void MediaPlayerPrivateAVFoundationObjC::createAVPlayerLayer()
{
    if (!m_avPlayer)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_videoLayer = adoptNS([PAL::allocAVPlayerLayerInstance() init]);
    [m_videoLayer setPlayer:m_avPlayer.get()];

    [m_videoLayer setName:@"MediaPlayerPrivate AVPlayerLayer"];
    [m_videoLayer addObserver:m_objcObserver.get() forKeyPath:@"readyForDisplay" options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextAVPlayerLayer];
    updateVideoLayerGravity();
    [m_videoLayer setContentsScale:player()->playerContentsScale()];
    m_videoLayerManager->setVideoLayer(m_videoLayer.get(), snappedIntRect(player()->playerContentBoxRect()).size());

#if PLATFORM(IOS_FAMILY) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if ([m_videoLayer respondsToSelector:@selector(setPIPModeEnabled:)])
        [m_videoLayer setPIPModeEnabled:(player()->fullscreenMode() & MediaPlayer::VideoFullscreenModePictureInPicture)];
#endif

    setNeedsRenderingModeChanged();
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

    setNeedsRenderingModeChanged();
}

MediaTime MediaPlayerPrivateAVFoundationObjC::getStartDate() const
{
    // Date changes as the track's playback position changes. Must subtract currentTime (offset in seconds) from date offset to get date beginning
    double date = [[m_avPlayerItem currentDate] timeIntervalSince1970];

    // No live streams were made during the epoch (1970). AVFoundation returns 0 if the media file doesn't have a start date
    if (!date)
        return MediaTime::invalidTime();

    double currentTime = PAL::CMTimeGetSeconds([m_avPlayerItem currentTime]);

    // Rounding due to second offset error when subtracting.
    return MediaTime::createWithDouble(round(date - currentTime));
}

bool MediaPlayerPrivateAVFoundationObjC::hasAvailableVideoFrame() const
{
    if (currentRenderingMode() == MediaRenderingMode::MediaRenderingToLayer)
        return m_cachedIsReadyForDisplay;

    if (m_videoOutput && (m_lastPixelBuffer || m_videoOutput->hasImageForTime(currentMediaTime())))
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
            
            if (![[currentOption outOfBandIdentifier] isEqual:(__bridge NSString *)uniqueID.get()])
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

    return URLByCanonicalizingURL(cocoaURL);
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

    player()->getRawCookies(url, [this, weakThis = WeakPtr { *this }, options = WTFMove(options), url] (auto cookies) mutable {
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

    return equalLettersIgnoringASCIICase(type, "video/webm"_s) || equalLettersIgnoringASCIICase(type, "audio/webm"_s);
#else
    UNUSED_PARAM(type);
    return false;
#endif
}

static URL conformFragmentIdentifierForURL(const URL& url)
{
#if PLATFORM(MAC)
    // On some versions of macOS, Photos.framework has overriden utility methods from AVFoundation that cause
    // a exception to be thrown when parsing fragment identifiers from a URL. Their implementation requires
    // an even number of components when splitting the fragment identifier with separator characters ['&','='].
    // Work around this broken implementation by pre-parsing the fragment and ensuring that it meets their
    // criteria. Problematic strings from the TC0051.html test include "t=3&", and this problem generally is
    // with subtrings between the '&' character not including an equal sign.
    auto hasInvalidNumberOfEqualCharacters = [](StringView fragmentParameter) {
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

    [options setObject:@(AVAssetReferenceRestrictionForbidRemoteReferenceToLocal | AVAssetReferenceRestrictionForbidLocalReferenceToRemote) forKey:AVURLAssetReferenceRestrictionsKey];

    RetainPtr<NSMutableDictionary> headerFields = adoptNS([[NSMutableDictionary alloc] init]);

    String referrer = player()->referrer();
    if (!referrer.isEmpty())
        [headerFields setObject:referrer forKey:@"Referer"];

    String userAgent = player()->userAgent();
    if (!userAgent.isEmpty())
        [headerFields setObject:userAgent forKey:@"User-Agent"];

    if ([headerFields count])
        [options setObject:headerFields.get() forKey:@"AVURLAssetHTTPHeaderFieldsKey"];

    if (shouldEnableInheritURIQueryComponent())
        [options setObject:@YES forKey:AVURLAssetInheritURIQueryComponentFromReferencingURIKey];

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
    if (player()->prefersSandboxedParsing() && PAL::canLoad_AVFoundation_AVAssetPrefersSandboxedParsingOptionKey())
        [options setObject:@YES forKey:AVAssetPrefersSandboxedParsingOptionKey];

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

        [options setObject:outOfBandTracks.get() forKey:AVURLAssetOutOfBandAlternateTracksKey];
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

    auto allowedMediaContainerTypes = player()->allowedMediaContainerTypes();
    if (allowedMediaContainerTypes && PAL::canLoad_AVFoundation_AVURLAssetAllowableTypeCategoriesKey()) {
        auto nsTypes = adoptNS([[NSMutableArray alloc] init]);
        for (auto type : *allowedMediaContainerTypes)
            [nsTypes addObject:(NSString *)UTIFromMIMEType(type)];
        [options setObject:nsTypes.get() forKey:AVURLAssetAllowableTypeCategoriesKey];
    }

    auto allowedMediaAudioCodecIDs = player()->allowedMediaAudioCodecIDs();
    if (allowedMediaAudioCodecIDs && PAL::canLoad_AVFoundation_AVURLAssetAllowableAudioCodecTypesKey()) {
        auto nsTypes = adoptNS([[NSMutableArray alloc] init]);
        for (auto type : *allowedMediaAudioCodecIDs)
            [nsTypes addObject:@(type.value)];
        [options setObject:nsTypes.get() forKey:AVURLAssetAllowableAudioCodecTypesKey];
    }

    auto allowedMediaVideoCodecIDs = player()->allowedMediaVideoCodecIDs();
    if (allowedMediaVideoCodecIDs && PAL::canLoad_AVFoundation_AVURLAssetAllowableVideoCodecTypesKey()) {
        auto nsTypes = adoptNS([[NSMutableArray alloc] init]);
        for (auto type : *allowedMediaVideoCodecIDs)
            [nsTypes addObject:@(type.value)];
        [options setObject:nsTypes.get() forKey:AVURLAssetAllowableVideoCodecTypesKey];
    }

    auto allowedMediaCaptionFormatTypes = player()->allowedMediaCaptionFormatTypes();
    if (allowedMediaCaptionFormatTypes && PAL::canLoad_AVFoundation_AVURLAssetAllowableCaptionFormatsKey()) {
        auto nsTypes = adoptNS([[NSMutableArray alloc] init]);
        for (auto type : *allowedMediaCaptionFormatTypes)
            [nsTypes addObject:@(type.value)];
        [options setObject:nsTypes.get() forKey:AVURLAssetAllowableCaptionFormatsKey];
    }

    if (willUseWebMFormatReader)
        registerFormatReaderIfNecessary();

    NSURL *cocoaURL = canonicalURL(url);

    @try {
        m_avAsset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:cocoaURL options:options.get()]);
    } @catch(NSException *exception) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVURLAssetInstance initWithURL:cocoaURL options:] threw an exception: ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
        cocoaURL = canonicalURL(conformFragmentIdentifierForURL(url));

        @try {
            m_avAsset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:cocoaURL options:options.get()]);
        } @catch(NSException *exception) {
            ASSERT_NOT_REACHED();
            ERROR_LOG(LOGIDENTIFIER, "-[AVURLAssetInstance initWithURL:cocoaURL options:] threw a second exception, bailing: ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
            setNetworkState(MediaPlayer::NetworkState::FormatError);
            return;
        }
    }

    AVAssetResourceLoader *resourceLoader = [m_avAsset resourceLoader];
    [resourceLoader setDelegate:m_loaderDelegate.get() queue:globalLoaderDelegateQueue()];

    if (auto mediaResourceLoader = player()->createResourceLoader()) {
        m_targetQueue = mediaResourceLoader->targetQueue();
        resourceLoader.URLSession = (NSURLSession *)adoptNS([[WebCoreNSURLSession alloc] initWithResourceLoader:*mediaResourceLoader delegate:resourceLoader.URLSessionDataDelegate delegateQueue:resourceLoader.URLSessionDataDelegateQueue]).get();
    }

    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(chapterMetadataDidChange:) name:AVAssetChapterMetadataGroupsDidChangeNotification object:m_avAsset.get()];

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
        [m_avPlayer addObserver:m_objcObserver.get() forKeyPath:keyName options:NSKeyValueObservingOptionNew context:(void *)MediaPlayerAVFoundationObservationContextPlayer];
    m_automaticallyWaitsToMinimizeStalling = [m_avPlayer automaticallyWaitsToMinimizeStalling];

    setShouldObserveTimeControlStatus(true);

    m_avPlayer.get().appliesMediaSelectionCriteriaAutomatically = NO;
#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
    if ([m_avPlayer respondsToSelector:@selector(setVideoRangeOverride:)])
        m_avPlayer.get().videoRangeOverride = convertDynamicRangeModeEnumToAVVideoRange(player()->preferredDynamicRangeMode());
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    updateDisableExternalPlayback();
    [m_avPlayer setAllowsExternalPlayback:m_allowsWirelessVideoPlayback];
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
        [m_avPlayer setMuted:m_muted];

#if HAVE(AVPLAYER_SUPRESSES_AUDIO_RENDERING)
        if (player()->isVideoPlayer())
            m_avPlayer.get().suppressesAudioRendering = YES;
#endif
    }

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
    m_currentTimeObserver = [m_avPlayer addPeriodicTimeObserverForInterval:PAL::CMTimeMake(1, 10) queue:dispatch_get_main_queue() usingBlock:[weakThis = WeakPtr { *this }, identifier = LOGIDENTIFIER, this] (CMTime cmTime) {
        ensureOnMainThread([weakThis, cmTime, this, identifier] {
            if (!weakThis)
                return;

            auto time = PAL::toMediaTime(cmTime);
            if (time == MediaTime::zeroTime() && m_lastPeriodicObserverMediaTime > MediaTime::zeroTime() && !seeking())
                ALWAYS_LOG(identifier, "PeriodicTimeObserver called with zero");
            if (time < MediaTime::zeroTime())
                ALWAYS_LOG(identifier, "PeriodicTimeObserver called with negative time ", time);
            if (time < m_lastPeriodicObserverMediaTime)
                ALWAYS_LOG(identifier, "PeriodicTimeObserver went backwards, was ", m_lastPeriodicObserverMediaTime, ", is now ", time);
            if (!time.isFinite())
                ALWAYS_LOG(identifier, "PeriodicTimeObserver called with called with infinite time");
            m_lastPeriodicObserverMediaTime = time;

            weakThis->currentMediaTimeDidChange(WTFMove(time));
        });
    }];

    if (m_isGatheringVideoFrameMetadata)
        startVideoFrameMetadataGathering();
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

    for (NSString *keyName in itemKVOProperties()) {
        NSKeyValueObservingOptions options = NSKeyValueObservingOptionNew | NSKeyValueObservingOptionPrior;
        if ([keyName isEqualToString:@"duration"])
            options |= NSKeyValueObservingOptionInitial;

        [m_avPlayerItem addObserver:m_objcObserver.get() forKeyPath:keyName options:options context:(void *)MediaPlayerAVFoundationObservationContextPlayerItem];
    }

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
    [m_legibleOutput setSuppressesPlayerRendering:YES];

    [m_legibleOutput setDelegate:m_objcObserver.get() queue:dispatch_get_main_queue()];
    [m_legibleOutput setAdvanceIntervalForDelegateInvocation:avPlayerOutputAdvanceInterval];
    [m_legibleOutput setTextStylingResolution:AVPlayerItemLegibleOutputTextStylingResolutionSourceAndRulesOnly];
    [m_avPlayerItem addOutput:m_legibleOutput.get()];

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    if (m_provider) {
        m_provider->setPlayerItem(m_avPlayerItem.get());
        m_provider->setAudioTrack(firstEnabledAudibleTrack());
    }
#endif

    m_metadataCollector = adoptNS([PAL::allocAVPlayerItemMetadataCollectorInstance() initWithIdentifiers:nil classifyingLabels:nil]);
    [m_metadataCollector setDelegate:m_objcObserver.get() queue:dispatch_get_main_queue()];
    [m_avPlayerItem addMediaDataCollector:m_metadataCollector.get()];

    m_metadataOutput = adoptNS([PAL::allocAVPlayerItemMetadataOutputInstance() initWithIdentifiers:nil]);
    [m_metadataOutput setDelegate:m_objcObserver.get() queue:dispatch_get_main_queue()];
    [m_metadataOutput setAdvanceIntervalForDelegateInvocation:avPlayerOutputAdvanceInterval];
    [m_avPlayerItem addOutput:m_metadataOutput.get()];

    setDelayCallbacks(false);
}

void MediaPlayerPrivateAVFoundationObjC::checkPlayability()
{
    if (m_haveCheckedPlayability)
        return;
    m_haveCheckedPlayability = true;

    INFO_LOG(LOGIDENTIFIER);
    WeakPtr weakThis { *this };

    [m_avAsset loadValuesAsynchronouslyForKeys:@[@"playable", @"tracks"] completionHandler:^{
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
    WeakPtr weakThis { *this };
    [m_avAsset loadValuesAsynchronouslyForKeys:assetMetadataKeyNames() completionHandler:^{
        callOnMainThread([weakThis, metadataLoadingGroup] {
            if (weakThis && [weakThis->m_avAsset statusOfValueForKey:@"tracks" error:nil] == AVKeyValueStatusLoaded) {
                for (AVAssetTrack *track in [weakThis->m_avAsset tracks]) {
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
                [weakThis->m_objcObserver metadataLoaded];
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
    updateVideoLayerGravity(ShouldAnimate::Yes);
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
    updateVideoLayerGravity(ShouldAnimate::Yes);
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
    
    AVPlayerItemAccessLog *log = [m_avPlayerItem accessLog];
    RetainPtr<NSString> logString = adoptNS([[NSString alloc] initWithData:[log extendedLogData] encoding:[log extendedLogDataStringEncoding]]);

    return logString.get();
}

String MediaPlayerPrivateAVFoundationObjC::errorLog() const
{
    if (!m_avPlayerItem)
        return emptyString();

    AVPlayerItemErrorLog *log = [m_avPlayerItem errorLog];
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
    if (!m_videoLayer)
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];    
    [m_videoLayer setHidden:!isVisible];
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

bool MediaPlayerPrivateAVFoundationObjC::playAtHostTime(const MonotonicTime& hostTime)
{
    if (!metaDataAvailable())
        return false;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_requestedPlaying = true;
    setPlayerRate(m_requestedRate, hostTime);

    return true;
}

bool MediaPlayerPrivateAVFoundationObjC::pauseAtHostTime(const MonotonicTime& hostTime)
{
    if (!metaDataAvailable())
        return false;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_requestedPlaying = false;
    setPlayerRate(0, hostTime);

    return true;
}

bool MediaPlayerPrivateAVFoundationObjC::platformPaused() const
{
    return m_cachedTimeControlStatus == AVPlayerTimeControlStatusPaused;
}

void MediaPlayerPrivateAVFoundationObjC::startVideoFrameMetadataGathering()
{
    // requestVideoFrameCallback() cares about the /next/ available frame. Pull the current frame from
    // the QueuedVideoOutput so paints of the current frame succeed;
    updateLastPixelBuffer();

    m_currentImageChangedObserver = WTF::makeUnique<Observer<void()>>([this] {
        m_currentImageChangedObserver = nullptr;
        checkNewVideoFrameMetadata();
    });

    if (m_videoOutput)
        m_videoOutput->addCurrentImageChangedObserver(*m_currentImageChangedObserver);

    m_isGatheringVideoFrameMetadata = true;
}

void MediaPlayerPrivateAVFoundationObjC::checkNewVideoFrameMetadata()
{
    if (!m_isGatheringVideoFrameMetadata)
        return;

    if (!updateLastPixelBuffer() && !m_videoFrameMetadata)
        return;

    player()->onNewVideoFrameMetadata(WTFMove(*m_videoFrameMetadata), m_lastPixelBuffer.get());
}

void MediaPlayerPrivateAVFoundationObjC::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
    m_videoFrameMetadata = { };

    if (m_videoFrameMetadataGatheringObserver) {
        [m_avPlayer removeTimeObserver:m_videoFrameMetadataGatheringObserver.get()];
        m_videoFrameMetadataGatheringObserver = nil;
    }
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
        cmDuration = [m_avPlayerItem duration];
    else
        cmDuration = [m_avAsset duration];

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
        currentMediaTimeDidChange(PAL::toMediaTime([m_avPlayerItem currentTime]));
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
        m_currentTimeDidChangeCallback(m_cachedCurrentMediaTime.isFinite() ? m_cachedCurrentMediaTime : MediaTime::zeroTime());
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
    if (PAL::CMTimeCompare(cmBefore, PAL::kCMTimeZero) < 0)
        cmBefore = PAL::kCMTimeZero;
    
    WeakPtr weakThis { *this };

    setShouldObserveTimeControlStatus(false);
    [m_avPlayerItem seekToTime:cmTime toleranceBefore:cmBefore toleranceAfter:cmAfter completionHandler:^(BOOL finished) {
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

    [m_avPlayer setVolume:volume];
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

    [m_avPlayer setMuted:m_muted];
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
    m_wallClockAtCachedCurrentTime = std::nullopt;
}

void MediaPlayerPrivateAVFoundationObjC::setPlayerRate(double rate, std::optional<MonotonicTime>&& hostTime)
{
    setDelayCallbacks(true);

    [m_avPlayerItem setAudioTimePitchAlgorithm:audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(player()->pitchCorrectionAlgorithm(), player()->preservesPitch(), m_requestedRate)];

    setShouldObserveTimeControlStatus(false);

    // -[AVPlayer setRate:time:atHostTime:] will throw if AVPlayer.automaticallyWaitsToMinimizeStalling
    // is set to YES. Disable when synchronized playback is indicated by the optional hostTime parameter
    // and enable otherwise.
    bool shouldAutomaticallyWait = !hostTime;
    if (m_automaticallyWaitsToMinimizeStalling != shouldAutomaticallyWait) {
        [m_avPlayer setAutomaticallyWaitsToMinimizeStalling:shouldAutomaticallyWait];
        m_automaticallyWaitsToMinimizeStalling = shouldAutomaticallyWait;
    }

    if (hostTime) {
        auto cmHostTime = PAL::CMClockMakeHostTimeFromSystemUnits(hostTime->toMachAbsoluteTime());
        INFO_LOG(LOGIDENTIFIER, "setting rate to ", rate, " at host time ", PAL::CMTimeGetSeconds(cmHostTime));
        [m_avPlayer setRate:rate time:PAL::kCMTimeInvalid atHostTime:cmHostTime];
    } else
        [m_avPlayer setRate:rate];

    m_cachedTimeControlStatus = [m_avPlayer timeControlStatus];
    setShouldObserveTimeControlStatus(true);
    setDelayCallbacks(false);

    m_wallClockAtCachedCurrentTime = std::nullopt;
}

double MediaPlayerPrivateAVFoundationObjC::rate() const
{
    if (!metaDataAvailable())
        return 0;

    return m_requestedRate;
}

double MediaPlayerPrivateAVFoundationObjC::effectiveRate() const
{
    if (!metaDataAvailable())
        return 0;

    return m_cachedRate;
}

double MediaPlayerPrivateAVFoundationObjC::seekableTimeRangesLastModifiedTime() const
{
#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(MACCATALYST)
    if (!m_cachedSeekableTimeRangesLastModifiedTime)
        m_cachedSeekableTimeRangesLastModifiedTime = [m_avPlayerItem seekableTimeRangesLastModifiedTime];
    return *m_cachedSeekableTimeRangesLastModifiedTime;
#else
    return 0;
#endif
}

double MediaPlayerPrivateAVFoundationObjC::liveUpdateInterval() const
{
#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(MACCATALYST)
    if (!m_cachedLiveUpdateInterval)
        m_cachedLiveUpdateInterval = [m_avPlayerItem liveUpdateInterval];
    return *m_cachedLiveUpdateInterval;
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
    using namespace PAL; // For CMTIMERANGE_IS_EMPTY.

    auto timeRanges = makeUnique<PlatformTimeRanges>();

    if (!m_avPlayerItem)
        return timeRanges;

    for (NSValue *thisRangeValue in m_cachedLoadedRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (CMTIMERANGE_IS_VALID(timeRange) && !CMTIMERANGE_IS_EMPTY(timeRange))
            timeRanges->add(PAL::toMediaTime(timeRange.start), PAL::toMediaTime(PAL::CMTimeRangeGetEnd(timeRange)));
    }
    return timeRanges;
}

MediaTime MediaPlayerPrivateAVFoundationObjC::platformMinTimeSeekable() const
{
    using namespace PAL; // For CMTIMERANGE_IS_EMPTY.

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
    using namespace PAL; // For CMTIMERANGE_IS_EMPTY.

    if (!m_cachedSeekableRanges)
        m_cachedSeekableRanges = [m_avPlayerItem seekableTimeRanges];

    if (![m_cachedSeekableRanges count])
        return platformDuration();

    MediaTime maxTimeSeekable;
    for (NSValue *thisRangeValue in m_cachedSeekableRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;
        
        MediaTime endOfRange = PAL::toMediaTime(PAL::CMTimeRangeGetEnd(timeRange));
        if (maxTimeSeekable < endOfRange)
            maxTimeSeekable = endOfRange;
    }
    return maxTimeSeekable;
}

MediaTime MediaPlayerPrivateAVFoundationObjC::platformMaxTimeLoaded() const
{
    using namespace PAL; // For CMTIMERANGE_IS_EMPTY.

    if (!m_cachedLoadedRanges)
        return MediaTime::zeroTime();

    MediaTime maxTimeLoaded;
    for (NSValue *thisRangeValue in m_cachedLoadedRanges.get()) {
        CMTimeRange timeRange = [thisRangeValue CMTimeRangeValue];
        if (!CMTIMERANGE_IS_VALID(timeRange) || CMTIMERANGE_IS_EMPTY(timeRange))
            continue;
        
        MediaTime endOfRange = PAL::toMediaTime(PAL::CMTimeRangeGetEnd(timeRange));
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
    processChapterTracks();
}

std::optional<bool> MediaPlayerPrivateAVFoundationObjC::allTracksArePlayable() const
{
    if (m_avPlayerItem) {
        for (AVPlayerItemTrack *track in [m_avPlayerItem tracks]) {
            if (!trackIsPlayable(track.assetTrack))
                return false;
        }

        return true;
    }

    if (!m_avAsset || [m_avAsset statusOfValueForKey:@"tracks" error:NULL] != AVKeyValueStatusLoaded)
        return std::nullopt;

    for (AVAssetTrack *assetTrack : [m_avAsset tracks]) {
        if (!trackIsPlayable(assetTrack))
            return false;
    }
    return true;
}

bool MediaPlayerPrivateAVFoundationObjC::trackIsPlayable(AVAssetTrack* track) const
{
    if (player()->shouldCheckHardwareSupport() && !assetTrackMeetsHardwareDecodeRequirements(track, player()->mediaContentTypesRequiringHardwareSupport()))
        return false;

    auto description = retainPtr((__bridge CMFormatDescriptionRef)track.formatDescriptions.firstObject);
    if (!description)
        return false;

    auto mediaType = PAL::CMFormatDescriptionGetMediaType(description.get());
    auto codecType = FourCC { PAL::CMFormatDescriptionGetMediaSubType(description.get()) };
    switch (PAL::CMFormatDescriptionGetMediaType(description.get())) {
    case kCMMediaType_Video: {
        auto& allowedMediaVideoCodecIDs = player()->allowedMediaVideoCodecIDs();
        if (allowedMediaVideoCodecIDs && !allowedMediaVideoCodecIDs->contains(codecType)) {
            ERROR_LOG(LOGIDENTIFIER, "Video track with codec type '", codecType, "' not contained in allowed codec list; blocking");
            return false;
        }
        return true;
    }
    case kCMMediaType_Audio: {
        auto& allowedMediaAudioCodecIDs = player()->allowedMediaAudioCodecIDs();
        if (allowedMediaAudioCodecIDs && !allowedMediaAudioCodecIDs->contains(codecType)) {
            ERROR_LOG(LOGIDENTIFIER, "Audio track with codec type '", codecType, "' not contained in allowed codec list; blocking");
            return false;
        }
        return true;
    }
    case kCMMediaType_Text:
    case kCMMediaType_ClosedCaption:
    case kCMMediaType_Subtitle: {
        auto& allowedMediaCaptionFormatTypes = player()->allowedMediaCaptionFormatTypes();
        if (allowedMediaCaptionFormatTypes && !allowedMediaCaptionFormatTypes->contains(codecType)) {
            ERROR_LOG(LOGIDENTIFIER, "Text track with codec type '", codecType, "' not contained in allowed codec list; blocking");
            return false;
        }
        return true;
    }
    case kCMMediaType_Muxed:
    case kCMMediaType_TimeCode:
    case kCMMediaType_Metadata:
        // No-op
        return true;
    }

    ERROR_LOG(LOGIDENTIFIER, "Track with unuexpected media type '", FourCC(mediaType), "' not contained in allowed codec list; ignoring");
    return true;
}

MediaPlayerPrivateAVFoundation::AssetStatus MediaPlayerPrivateAVFoundationObjC::assetStatus() const
{
    if (!m_avAsset)
        return MediaPlayerAVAssetStatusDoesNotExist;

    if (!m_cachedAssetIsLoaded) {
        for (NSString *keyName in assetMetadataKeyNames()) {
            NSError *error = nil;
            AVKeyValueStatus keyStatus = [m_avAsset statusOfValueForKey:keyName error:&error];

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

    // m_loadingMetadata will be false until all tracks' properties have finished loading.
    // See: beginLoadingMetadata().
    if (loadingMetadata())
        return MediaPlayerAVAssetStatusLoading;

    if (!m_cachedTracksArePlayable) {
        m_cachedTracksArePlayable = allTracksArePlayable();
        if (!m_cachedTracksArePlayable)
            return MediaPlayerAVAssetStatusLoading;
    }

    if (!m_cachedAssetIsPlayable)
        m_cachedAssetIsPlayable = [[m_avAsset valueForKey:@"playable"] boolValue];

    if (*m_cachedAssetIsPlayable && *m_cachedTracksArePlayable)
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

    paintWithVideoOutput(context, rect);

    END_BLOCK_OBJC_EXCEPTIONS
    setDelayCallbacks(false);

    m_videoFrameHasDrawn = true;
}

void MediaPlayerPrivateAVFoundationObjC::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (!metaDataAvailable() || context.paintingDisabled())
        return;

    // We can ignore the request if we are already rendering to a layer.
    if (currentRenderingMode() == MediaRenderingMode::MediaRenderingToLayer)
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

    [m_imageGenerator setMaximumSize:CGSize(rect.size())];
    RetainPtr<CGImageRef> rawImage = adoptCF([m_imageGenerator copyCGImageAtTime:PAL::CMTimeMakeWithSeconds(time, 600) actualTime:nil error:nil]);
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
    return equalLettersIgnoringASCIICase(keySystem, "com.apple.fps"_s)
        || equalIgnoringASCIICase(keySystem, "com.apple.fps.1_0"_s)
        || equalLettersIgnoringASCIICase(keySystem, "org.w3c.clearkey"_s);
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

    if (!contentTypeMeetsContainerAndCodecTypeRequirements(parameters.type, parameters.allowedMediaContainerTypes, parameters.allowedMediaCodecTypes))
        return MediaPlayer::SupportsType::IsNotSupported;

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
        if (equalLettersIgnoringASCIICase(keySystem, "org.w3c.clearkey"_s) && !mimeType.isEmpty() && !equalLettersIgnoringASCIICase(mimeType, "application/x-mpegurl"_s))
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
        INFO_LOG(LOGIDENTIFIER, "received skd:// URI");
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

        auto initData = SharedBuffer::create(Vector<uint8_t> { static_cast<uint8_t*>(initDataBuffer->data()), byteLength });
        player()->keyNeeded(initData);
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
        auto encodedKeyId = PAL::UTF8Encoding().encode(keyID, PAL::UnencodableHandling::URLEncodedEntities);
        auto initData = SharedBuffer::create(WTFMove(encodedKeyId));

        auto keyData = player()->cachedKeyForKeyId(keyID);
        if (keyData) {
            fulfillRequestWithKeyData(avRequest, keyData.get());
            return false;
        }

        player()->keyNeeded(initData);

        if (!player()->shouldContinueAfterKeyNeeded())
            return false;

        m_keyURIToRequestMap.set(keyID, avRequest);
        return true;
    }
#endif
#endif

    auto resourceLoader = WebCoreAVFResourceLoader::create(this, avRequest, m_targetQueue);
    m_resourceLoaderMap.add((__bridge CFTypeRef)avRequest, resourceLoader.copyRef());
    resourceLoader->startLoading();

    return true;
}

void MediaPlayerPrivateAVFoundationObjC::didCancelLoadingRequest(AVAssetResourceLoadingRequest* avRequest)
{
    ASSERT(isMainThread());
    if (RefPtr resourceLoader = m_resourceLoaderMap.get((__bridge CFTypeRef)avRequest)) {
        m_targetQueue->dispatch([resourceLoader = WTFMove(resourceLoader)] { resourceLoader->stopLoading();
        });
    }
}

void MediaPlayerPrivateAVFoundationObjC::didStopLoadingRequest(AVAssetResourceLoadingRequest *avRequest)
{
    ASSERT(isMainThread());
    if (RefPtr resourceLoader = m_resourceLoaderMap.take((__bridge CFTypeRef)avRequest))
        m_targetQueue->dispatch([resourceLoader = WTFMove(resourceLoader)] { });
}

bool MediaPlayerPrivateAVFoundationObjC::isAvailable()
{
    return PAL::isAVFoundationFrameworkAvailable() && PAL::isCoreMediaFrameworkAvailable();
}

MediaTime MediaPlayerPrivateAVFoundationObjC::mediaTimeForTimeValue(const MediaTime& timeValue) const
{
    if (!metaDataAvailable())
        return timeValue;

    // FIXME - impossible to implement until rdar://8721510 is fixed.
    return timeValue;
}

void MediaPlayerPrivateAVFoundationObjC::updateVideoLayerGravity(ShouldAnimate shouldAnimate)
{
    if (!m_videoLayer)
        return;

    NSString* videoGravity = shouldMaintainAspectRatio() ? AVLayerVideoGravityResizeAspect : AVLayerVideoGravityResize;
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (m_videoLayerManager->videoFullscreenLayer()) {
        switch (m_videoFullscreenGravity) {
        case MediaPlayer::VideoGravity::Resize:
            videoGravity = AVLayerVideoGravityResize;
            break;
        case MediaPlayer::VideoGravity::ResizeAspect:
            videoGravity = AVLayerVideoGravityResizeAspect;
            break;
        case MediaPlayer::VideoGravity::ResizeAspectFill:
            videoGravity = AVLayerVideoGravityResizeAspectFill;
            break;
        }
    }
#endif

    if ([m_videoLayer videoGravity] == videoGravity)
        return;

    bool shouldDisableActions = shouldAnimate == ShouldAnimate::No;
    ALWAYS_LOG(LOGIDENTIFIER, "Setting gravity to \"", String { videoGravity }, "\", animated: ", !shouldDisableActions);

    [CATransaction begin];
    [CATransaction setDisableActions:shouldDisableActions];
    [m_videoLayer setVideoGravity:videoGravity];
    [CATransaction commit];

    syncTextTrackBounds();
}

void MediaPlayerPrivateAVFoundationObjC::metadataLoaded()
{
    MediaPlayerPrivateAVFoundation::metadataLoaded();
    processChapterTracks();
}

void MediaPlayerPrivateAVFoundationObjC::processChapterTracks()
{
    ASSERT(m_avAsset);

    for (NSLocale *locale in [m_avAsset availableChapterLocales]) {

        auto chapters = [m_avAsset chapterMetadataGroupsWithTitleLocale:locale containingItemsWithCommonKeys:@[AVMetadataCommonKeyArtwork]];
        if (!chapters.count)
            continue;

        String language = [locale localeIdentifier];
        auto track = m_chapterTracks.ensure(language, [&]() {
            auto track = InbandChapterTrackPrivateAVFObjC::create(locale);
            player()->addTextTrack(track.get());
            return track;
        }).iterator->value;

        track->processChapters(chapters);
    }
}

void MediaPlayerPrivateAVFoundationObjC::tracksChanged()
{
    String primaryAudioTrackLanguage = m_languageOfPrimaryAudioTrack;
    m_languageOfPrimaryAudioTrack = String();

    if (!m_avAsset)
        return;

    setDelayCharacteristicsChangedNotification(true);

    bool hasCaptions = false;

    // This is called whenever the tracks collection changes so cache hasVideo and hasAudio since we are
    // asked about those fairly fequently.
    if (!m_avPlayerItem) {
        // We don't have a player item yet, so check with the asset because some assets support inspection
        // prior to becoming ready to play.
        auto* firstEnabledVideoTrack = firstEnabledVisibleTrack();
        setHasVideo(firstEnabledVideoTrack);
        setHasAudio(firstEnabledAudibleTrack());
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
                else if ([mediaType isEqualToString:AVMediaTypeMetadata]) {
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

        // Always says we have video if the AVPlayerLayer is ready for display to work around
        // an AVFoundation bug which causes it to sometimes claim a track is disabled even
        // when it is not. Also say we have video if AVPlayerItem's `hasEnabledVideo` is true,
        // as an AVAssetTrack will sometimes disappear briefly and reappear when `hasEnabledVideo`
        // doesn't change.
        setHasVideo(hasVideo || m_cachedIsReadyForDisplay || m_cachedHasEnabledVideo);

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
        m_provider->setAudioTrack(firstEnabledAudibleTrack());
#endif

    setDelayCharacteristicsChangedNotification(false);
}

void MediaPlayerPrivateAVFoundationObjC::updateRotationSession()
{
    if (!m_avAsset || assetStatus() < MediaPlayerAVAssetStatusLoaded)
        return;

    AffineTransform finalTransform = m_avAsset.get().preferredTransform;
    FloatSize naturalSize;
    if (auto* firstEnabledVideoTrack = firstEnabledVisibleTrack()) {
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

    for (auto& track : m_videoTracks)
        track->resetPropertiesFromTrack();

    // In case the video track content changed, we may be able to perform a readback again.
    if (count)
        m_waitForVideoOutputMediaDataWillChangeTimedOut = false;

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
        m_provider->setAudioTrack(firstEnabledAudibleTrack());
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
    AVAssetResourceLoader *resourceLoader = [m_avAsset resourceLoader];
    WebCoreNSURLSession *session = (WebCoreNSURLSession *)resourceLoader.URLSession;
    if ([session isKindOfClass:[WebCoreNSURLSession class]])
        return session.didPassCORSAccessChecks;

    return false;
}

std::optional<bool> MediaPlayerPrivateAVFoundationObjC::wouldTaintOrigin(const SecurityOrigin& origin) const
{
    AVAssetResourceLoader *resourceLoader = [m_avAsset resourceLoader];
    WebCoreNSURLSession *session = (WebCoreNSURLSession *)resourceLoader.URLSession;
    if ([session isKindOfClass:[WebCoreNSURLSession class]])
        return [session wouldTaintOrigin:origin];

    return std::nullopt;
}

void MediaPlayerPrivateAVFoundationObjC::createVideoOutput()
{
    INFO_LOG(LOGIDENTIFIER);

    if (!m_avPlayerItem || m_videoOutput)
        return;

    m_videoOutput = QueuedVideoOutput::create(m_avPlayerItem.get(), m_avPlayer.get());
    ASSERT(m_videoOutput);
    if (!m_videoOutput) {
        ERROR_LOG(LOGIDENTIFIER, "-[AVPlayerItemVideoOutput initWithPixelBufferAttributes:] failed!");
        return;
    }
    if (m_currentImageChangedObserver)
        m_videoOutput->addCurrentImageChangedObserver(*m_currentImageChangedObserver);

    if (m_waitForVideoOutputMediaDataWillChangeObserver)
        m_videoOutput->addCurrentImageChangedObserver(*m_waitForVideoOutputMediaDataWillChangeObserver);

    setNeedsRenderingModeChanged();
}

void MediaPlayerPrivateAVFoundationObjC::destroyVideoOutput()
{
    if (!m_videoOutput)
        return;

    INFO_LOG(LOGIDENTIFIER);

    m_videoOutput->invalidate();
    m_videoOutput = nullptr;

    m_videoFrameMetadata = { };

    setNeedsRenderingModeChanged();
}

bool MediaPlayerPrivateAVFoundationObjC::updateLastPixelBuffer()
{
    if (!m_avPlayerItem)
        return false;

    m_haveBeenAskedToPaint = true;

    if (!m_videoOutput)
        createVideoOutput();
    ASSERT(m_videoOutput);

    auto currentTime = currentMediaTime();

    if (!m_videoOutput->hasImageForTime(currentTime))
        return false;

    auto entry = m_videoOutput->takeVideoFrameEntryForTime(currentTime);
    m_lastPixelBuffer = WTFMove(entry.pixelBuffer);

    if (m_isGatheringVideoFrameMetadata) {
        auto presentationTime = MonotonicTime::now().secondsSinceEpoch().seconds() - (currentTime - entry.displayTime).toDouble();
        m_videoFrameMetadata = {
            .width = static_cast<unsigned>(CVPixelBufferGetWidth(m_lastPixelBuffer.get())),
            .height = static_cast<unsigned>(CVPixelBufferGetHeight(m_lastPixelBuffer.get())),
            .presentedFrames = static_cast<unsigned>(++m_sampleCount),
            .mediaTime = entry.displayTime.toDouble(),
            .presentationTime = presentationTime,
            .expectedDisplayTime = presentationTime,
        };
    }

    if (m_lastPixelBuffer && m_imageRotationSession)
        m_lastPixelBuffer = m_imageRotationSession->rotate(m_lastPixelBuffer.get());

    if (m_resourceOwner && m_lastPixelBuffer) {
        if (auto surface = CVPixelBufferGetIOSurface(m_lastPixelBuffer.get()))
            IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
    }

    m_lastImage = nullptr;
    return true;
}

bool MediaPlayerPrivateAVFoundationObjC::videoOutputHasAvailableFrame()
{
    if (!m_avPlayerItem || readyState() < MediaPlayer::ReadyState::HaveCurrentData)
        return false;

    if (m_lastImage)
        return true;

    if (!m_videoOutput)
        createVideoOutput();

    return m_videoOutput->hasImageForTime(PAL::toMediaTime([m_avPlayerItem currentTime]));
}

void MediaPlayerPrivateAVFoundationObjC::updateLastImage(UpdateType type)
{
    if (!m_avPlayerItem || readyState() < MediaPlayer::ReadyState::HaveCurrentData)
        return;

    auto* firstEnabledVideoTrack = firstEnabledVisibleTrack();
    if (!firstEnabledVideoTrack)
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
    updateLastImage(UpdateType::UpdateSynchronously);
    if (!m_lastImage)
        return;

    INFO_LOG(LOGIDENTIFIER);

    FloatRect imageRect { FloatPoint::zero(), m_lastImage->size() };
    context.drawNativeImage(*m_lastImage, imageRect.size(), outputRect, imageRect);

    // If we have created an AVAssetImageGenerator in the past due to m_videoOutput not having an available
    // video frame, destroy it now that it is no longer needed.
    if (m_imageGenerator)
        destroyImageGenerator();

}

RefPtr<VideoFrame> MediaPlayerPrivateAVFoundationObjC::videoFrameForCurrentTime()
{
    updateLastPixelBuffer();
    if (!m_lastPixelBuffer)
        return nullptr;
    return VideoFrameCV::create(currentMediaTime(), false, VideoFrame::Rotation::None, RetainPtr { m_lastPixelBuffer });
}

RefPtr<NativeImage> MediaPlayerPrivateAVFoundationObjC::nativeImageForCurrentTime()
{
    updateLastImage(UpdateType::UpdateSynchronously);
    return m_lastImage;
}

DestinationColorSpace MediaPlayerPrivateAVFoundationObjC::colorSpace()
{
    updateLastImage(UpdateType::UpdateSynchronously);
    if (!m_lastPixelBuffer)
        return DestinationColorSpace::SRGB();

    return DestinationColorSpace(createCGColorSpaceForCVPixelBuffer(m_lastPixelBuffer.get()));
}

void MediaPlayerPrivateAVFoundationObjC::waitForVideoOutputMediaDataWillChange()
{
    if (m_waitForVideoOutputMediaDataWillChangeTimedOut)
        return;

    // Wait for 1 second.
    MonotonicTime start = MonotonicTime::now();

    std::optional<RunLoop::Timer<MediaPlayerPrivateAVFoundationObjC>> timeoutTimer;

    if (!m_runLoopNestingLevel) {
        m_waitForVideoOutputMediaDataWillChangeObserver = WTF::makeUnique<Observer<void()>>([this, logIdentifier = LOGIDENTIFIER] () mutable {
            if (m_runLoopNestingLevel)
                RunLoop::main().stop();
        });
        m_videoOutput->addCurrentImageChangedObserver(*m_waitForVideoOutputMediaDataWillChangeObserver);

        timeoutTimer.emplace(RunLoop::main(), [&] {
            RunLoop::main().stop();
        });
        timeoutTimer->startOneShot(1_s);
    }

    ++m_runLoopNestingLevel;
    RunLoop::run();
    --m_runLoopNestingLevel;

    if (m_runLoopNestingLevel) {
        RunLoop::main().stop();
        return;
    }

    bool satisfied = timeoutTimer->isActive();
    if (!satisfied) {
        ERROR_LOG(LOGIDENTIFIER, "timed out");
        m_waitForVideoOutputMediaDataWillChangeTimedOut = true;
    } else
        INFO_LOG(LOGIDENTIFIER, "waiting for videoOutput took ", (MonotonicTime::now() - start).seconds());
}

void MediaPlayerPrivateAVFoundationObjC::outputMediaDataWillChange()
{
    checkNewVideoFrameMetadata();
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

std::unique_ptr<LegacyCDMSession> MediaPlayerPrivateAVFoundationObjC::createSession(const String& keySystem, LegacyCDMSessionClient& client)
{
    if (!keySystemIsSupported(keySystem))
        return nullptr;
    auto session = makeUnique<CDMSessionAVFoundationObjC>(this, client);
    m_session = *session;
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

AVAssetTrack* MediaPlayerPrivateAVFoundationObjC::firstEnabledTrack(AVMediaCharacteristic characteristic) const
{
    if (m_avPlayerItem) {
        for (AVPlayerItemTrack* track in [m_avPlayerItem tracks]) {
            if (!track.enabled)
                continue;
            if (!track.assetTrack)
                continue;
            if ([track.assetTrack hasMediaCharacteristic:characteristic])
                return track.assetTrack;
        }
    }
    if (!m_avAsset)
        return nil;

    if ([m_avAsset statusOfValueForKey:@"tracks" error:NULL] != AVKeyValueStatusLoaded)
        return nil;

    return [] (NSArray* tracks) -> AVAssetTrack* {
        NSUInteger index = [tracks indexOfObjectPassingTest:^(id obj, NSUInteger, BOOL *) {
            return [static_cast<AVAssetTrack*>(obj) isEnabled];
        }];
        if (index == NSNotFound)
            return nil;
        return [tracks objectAtIndex:index];
    }([m_avAsset tracksWithMediaCharacteristic:characteristic]);
}

AVAssetTrack* MediaPlayerPrivateAVFoundationObjC::firstEnabledAudibleTrack() const
{
    return firstEnabledTrack(AVMediaCharacteristicAudible);
}

AVAssetTrack* MediaPlayerPrivateAVFoundationObjC::firstEnabledVisibleTrack() const
{
    return firstEnabledTrack(AVMediaCharacteristicVisual);
}

bool MediaPlayerPrivateAVFoundationObjC::hasLoadedMediaSelectionGroups()
{
    if (!m_avAsset)
        return false;

    if ([m_avAsset statusOfValueForKey:@"availableMediaCharacteristicsWithMediaSelectionOptions" error:NULL] != AVKeyValueStatusLoaded)
        return false;

    return true;
}

AVMediaSelectionGroup* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForLegibleMedia()
{
    if (!hasLoadedMediaSelectionGroups())
        return nil;

    return [m_avAsset mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicLegible];
}

AVMediaSelectionGroup* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForAudibleMedia()
{
    if (!hasLoadedMediaSelectionGroups())
        return nil;

    return [m_avAsset mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicAudible];
}

AVMediaSelectionGroup* MediaPlayerPrivateAVFoundationObjC::safeMediaSelectionGroupForVisualMedia()
{
    if (!hasLoadedMediaSelectionGroups())
        return nil;

    return [m_avAsset mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicVisual];
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
        [m_avPlayerItem selectMediaOption:nil inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];

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
            
            if ([currentOption isEqual:option]) {
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
    m_metadataTrack->setInBandMetadataTrackDispatchType("com.apple.streaming"_s);
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

    INFO_LOG(LOGIDENTIFIER, "selecting track with language ", track ? track->language() : emptyAtom());

    m_currentTextTrack = track;

    if (track) {
        if (track->textTrackCategory() == InbandTextTrackPrivateAVF::LegacyClosedCaption)
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [m_avPlayer setClosedCaptionDisplayEnabled:YES];
            ALLOW_DEPRECATED_DECLARATIONS_END
#if ENABLE(AVF_CAPTIONS)
        else if (track->textTrackCategory() == InbandTextTrackPrivateAVF::OutOfBand)
            [m_avPlayerItem selectMediaOption:static_cast<OutOfBandTextTrackPrivateAVF*>(track)->mediaSelectionOption() inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
#endif
        else
            [m_avPlayerItem selectMediaOption:static_cast<InbandTextTrackPrivateAVFObjC*>(track)->mediaSelectionOption() inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
    } else {
        [m_avPlayerItem selectMediaOption:0 inMediaSelectionGroup:safeMediaSelectionGroupForLegibleMedia()];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [m_avPlayer setClosedCaptionDisplayEnabled:NO];
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
    AVMediaSelectionGroup *audibleGroup = [m_avAsset mediaSelectionGroupForMediaCharacteristic:AVMediaCharacteristicAudible];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    AVMediaSelectionOption *currentlySelectedAudibleOption = [m_avPlayerItem selectedMediaOptionInMediaSelectionGroup:audibleGroup];
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (currentlySelectedAudibleOption) {
        m_languageOfPrimaryAudioTrack = [[currentlySelectedAudibleOption locale] localeIdentifier];
        INFO_LOG(LOGIDENTIFIER, "language of selected audible option ", m_languageOfPrimaryAudioTrack);

        return m_languageOfPrimaryAudioTrack;
    }

    // AVFoundation synthesizes an audible group when there is only one ungrouped audio track if there is also a legible group (one or
    // more in-band text tracks). It doesn't know about out-of-band tracks, so if there is a single audio track return its language.
    NSArray *tracks = [m_avAsset tracksWithMediaType:AVMediaTypeAudio];
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

    if (auto context = PAL::OutputContext::sharedAudioPresentationOutputContext())
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

    m_allowsWirelessVideoPlayback = [m_avPlayer allowsExternalPlayback];
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
    [m_avPlayer setAllowsExternalPlayback:!disabled];
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
        if ((!newContext && !currentContext.get()) || [currentContext isEqual:newContext])
            return;

        setDelayCallbacks(true);
        m_avPlayer.get().outputContext = newContext;
        setDelayCallbacks(false);

        return;
    }

    ASSERT(m_playbackTarget->targetType() == MediaPlaybackTarget::TargetType::Mock);

    setDelayCallbacks(true);
    scheduleMainThreadNotification(MediaPlayerPrivateAVFoundation::Notification([weakThis = WeakPtr { *this }] {
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

void MediaPlayerPrivateAVFoundationObjC::seekableTimeRangesDidChange(RetainPtr<NSArray>&& seekableRanges, NSTimeInterval seekableTimeRangesLastModifiedTime, NSTimeInterval liveUpdateInterval)
{
    m_cachedSeekableRanges = WTFMove(seekableRanges);
    m_cachedSeekableTimeRangesLastModifiedTime = seekableTimeRangesLastModifiedTime;
    m_cachedLiveUpdateInterval = liveUpdateInterval;

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
    ALWAYS_LOG(LOGIDENTIFIER);

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
    static MainThreadNeverDestroyed<const AtomString> quickTimeUserData("com.apple.quicktime.udta"_s);
    static MainThreadNeverDestroyed<const AtomString> isoUserData("org.mp4ra"_s);
    static MainThreadNeverDestroyed<const AtomString> quickTimeMetadata("com.apple.quicktime.mdta"_s);
    static MainThreadNeverDestroyed<const AtomString> iTunesMetadata("com.apple.itunes"_s);
    static MainThreadNeverDestroyed<const AtomString> id3Metadata("org.id3"_s);
    static MainThreadNeverDestroyed<const AtomString> hlsDateRangeMetadata("com.apple.quicktime.HLS"_s);

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
                itemCopy.get().time = PAL::CMTimeMakeWithSeconds(groupStartTime, MediaTime::DefaultTimeScale);
            if (!CMTIME_IS_VALID(itemCopy.get().duration))
                itemCopy.get().duration = PAL::toCMTime(groupDuration);
            [groupCopy addObject:itemCopy.get()];
        }

        metadataDidArrive(groupCopy, currentTime);
    }
}

void MediaPlayerPrivateAVFoundationObjC::metadataDidArrive(const RetainPtr<NSArray>& metadata, const MediaTime& mediaTime)
{
    m_currentMetaData = metadata && ![metadata isKindOfClass:[NSNull class]] ? metadata : nil;

    INFO_LOG(LOGIDENTIFIER, "adding ", m_currentMetaData ? [m_currentMetaData count] : 0, " at time ", mediaTime);

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
        if (CMTIME_IS_VALID(duration) && PAL::CMTimeGetSeconds(duration) > 0.001)
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
    ALWAYS_LOG(LOGIDENTIFIER, hasEnabledAudio);
    m_cachedHasEnabledAudio = hasEnabledAudio;

    tracksChanged();
    updateStates();
}

void MediaPlayerPrivateAVFoundationObjC::hasEnabledVideoDidChange(bool hasEnabledVideo)
{
    ALWAYS_LOG(LOGIDENTIFIER, hasEnabledVideo);
    m_cachedHasEnabledVideo = hasEnabledVideo;

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
    m_wallClockAtCachedCurrentTime = std::nullopt;

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

std::optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateAVFoundationObjC::videoPlaybackQualityMetrics()
{
    if (![m_videoLayer respondsToSelector:@selector(videoPerformanceMetrics)])
        return std::nullopt;

#if PLATFORM(WATCHOS)
    return std::nullopt;
#else
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN

    auto metrics = [m_videoLayer videoPerformanceMetrics];
    if (!metrics)
        return std::nullopt;

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

    m_timeObserver = [m_avPlayer addBoundaryTimeObserverForTimes:@[[NSValue valueWithCMTime:PAL::toCMTime(time)]] queue:dispatch_get_main_queue() usingBlock:^{
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
        @"availableChapterLocales",
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
        @"hasEnabledVideo",
        @"canPlayFastForward",
        @"canPlayFastReverse",
    nil];
    return keys;
}

NSArray* assetTrackMetadataKeyNames()
{
    static NSArray* keys = [[NSArray alloc] initWithObjects:@"totalSampleDataLength", @"mediaType", @"enabled", @"preferredTransform", @"naturalSize", @"formatDescriptions", nil];
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
    m_backgroundQueue = WorkQueue::create("WebCoreAVFMovieObserver Background Queue");
    return self;
}

- (void)disconnect
{
    m_player = nullptr;
}

- (void)metadataLoaded
{
    ensureOnMainThread([self, strongSelf = retainPtr(self)] {
        if (!m_player)
            return;

        m_player->queueTaskOnEventLoop([player = m_player] {
            if (player)
                player->metadataLoaded();
        });
    });
}

- (void)didEnd:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    ensureOnMainThread([self, strongSelf = retainPtr(self)] {
        if (!m_player)
            return;

        m_player->queueTaskOnEventLoop([player = m_player] {
            if (player)
                player->didEnd();
        });
    });
}

- (void)chapterMetadataDidChange:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    ensureOnMainThread([self, strongSelf = retainPtr(self)] {
        if (!m_player)
            return;

        m_player->queueTaskOnEventLoop([player = m_player] {
            if (player)
                player->processChapterTracks();
        });
    });
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(MediaPlayerAVFoundationObservationContext)context
{
    auto queueTaskOnEventLoopWithPlayer = [self, strongSelf = retainPtr(self)] (Function<void(MediaPlayerPrivateAVFoundationObjC&)>&& function) mutable {
        ensureOnMainThread([self, strongSelf = WTFMove(strongSelf), function = WTFMove(function)] () mutable {
            if (!m_player)
                return;

            m_player->queueTaskOnEventLoop([player = m_player, function = WTFMove(function)] {
                if (!player)
                    return;

                ScriptDisallowedScope::InMainThread scriptDisallowedScope;
                function(*player);
            });
        });
    };

    if (context == MediaPlayerAVFoundationObservationContextPlayerItem && [keyPath isEqualToString:@"seekableTimeRanges"]) {
        // -liveUpdateInterval and -seekableTimeRangesLastModifiedTime are not KVO observable, but may also hang when queried.
        // Query their values here on a background thread, and pass to the main thread for caching.
        id newValue = [change valueForKey:NSKeyValueChangeNewKey];
        auto seekableTimeRanges = RetainPtr<NSArray> { newValue };

        m_backgroundQueue->dispatch([seekableTimeRanges = WTFMove(seekableTimeRanges), playerItem = RetainPtr<AVPlayerItem> { object }, queueTaskOnEventLoopWithPlayer] () mutable {
            auto seekableTimeRangesLastModifiedTime = [playerItem seekableTimeRangesLastModifiedTime];
            auto liveUpdateInterval = [playerItem liveUpdateInterval];
            queueTaskOnEventLoopWithPlayer([seekableTimeRanges = WTFMove(seekableTimeRanges), seekableTimeRangesLastModifiedTime, liveUpdateInterval] (auto& player) mutable {
                player.seekableTimeRangesDidChange(WTFMove(seekableTimeRanges), seekableTimeRangesLastModifiedTime, liveUpdateInterval);
            });
        });
    }

    queueTaskOnEventLoopWithPlayer([keyPath = RetainPtr { keyPath }, change = RetainPtr { change }, object = RetainPtr { object }, context] (auto& player) mutable {
        id newValue = [change valueForKey:NSKeyValueChangeNewKey];
        bool willChange = [[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue];
        bool shouldLogValue = !willChange;

        if (context == MediaPlayerAVFoundationObservationContextAVPlayerLayer) {
            if ([keyPath isEqualToString:@"readyForDisplay"])
                player.firstFrameAvailableDidChange([newValue boolValue]);
        }

        if (context == MediaPlayerAVFoundationObservationContextPlayerItemTrack) {
            if ([keyPath isEqualToString:@"enabled"])
                player.trackEnabledDidChange([newValue boolValue]);
        }

        if (context == MediaPlayerAVFoundationObservationContextPlayerItem && willChange) {
            if ([keyPath isEqualToString:@"playbackLikelyToKeepUp"])
                player.playbackLikelyToKeepUpWillChange();
            else if ([keyPath isEqualToString:@"playbackBufferEmpty"])
                player.playbackBufferEmptyWillChange();
            else if ([keyPath isEqualToString:@"playbackBufferFull"])
                player.playbackBufferFullWillChange();
        }

        if (context == MediaPlayerAVFoundationObservationContextPlayerItem && !willChange) {
            // A value changed for an AVPlayerItem
            if ([keyPath isEqualToString:@"status"])
                player.playerItemStatusDidChange([newValue intValue]);
            else if ([keyPath isEqualToString:@"playbackLikelyToKeepUp"])
                player.playbackLikelyToKeepUpDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"playbackBufferEmpty"])
                player.playbackBufferEmptyDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"playbackBufferFull"])
                player.playbackBufferFullDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"asset"]) {
                player.setAsset(RetainPtr<id>(newValue));
                shouldLogValue = false;
            } else if ([keyPath isEqualToString:@"loadedTimeRanges"])
                player.loadedTimeRangesDidChange(RetainPtr<NSArray>(newValue));
            else if ([keyPath isEqualToString:@"tracks"]) {
                player.tracksDidChange(RetainPtr<NSArray>(newValue));
                shouldLogValue = false;
            } else if ([keyPath isEqualToString:@"hasEnabledAudio"])
                player.hasEnabledAudioDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"hasEnabledVideo"])
                player.hasEnabledVideoDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"presentationSize"])
                player.presentationSizeDidChange(FloatSize([newValue sizeValue]));
            else if ([keyPath isEqualToString:@"duration"])
                player.durationDidChange(PAL::toMediaTime([newValue CMTimeValue]));
            else if ([keyPath isEqualToString:@"canPlayFastReverse"])
                player.canPlayFastReverseDidChange([newValue boolValue]);
            else if ([keyPath isEqualToString:@"canPlayFastForward"])
                player.canPlayFastForwardDidChange([newValue boolValue]);
        }

        if (context == MediaPlayerAVFoundationObservationContextPlayer && !willChange) {
            // A value changed for an AVPlayer.
            if ([keyPath isEqualToString:@"rate"])
                player.rateDidChange([newValue doubleValue]);
            else if ([keyPath isEqualToString:@"timeControlStatus"])
                player.timeControlStatusDidChange([newValue intValue]);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
            else if ([keyPath isEqualToString:@"externalPlaybackActive"] || [keyPath isEqualToString:@"allowsExternalPlayback"])
                player.playbackTargetIsWirelessDidChange();
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
            else if ([keyPath isEqualToString:@"outputObscuredDueToInsufficientExternalProtection"])
                player.outputObscuredDueToInsufficientExternalProtectionChanged([newValue boolValue]);
#endif
        }

        if (player.logger().willLog(player.logChannel(), WTFLogLevel::Debug) && !([keyPath isEqualToString:@"loadedTimeRanges"] || [keyPath isEqualToString:@"seekableTimeRanges"])) {
            auto identifier = Logger::LogSiteIdentifier("MediaPlayerPrivateAVFoundation", "observeValueForKeyPath", player.logIdentifier());

            if (shouldLogValue) {
                if ([keyPath isEqualToString:@"duration"])
                    player.logger().debug(player.logChannel(), identifier, "did change '", [keyPath UTF8String], "' to ", PAL::toMediaTime([newValue CMTimeValue]));
                else {
                    RetainPtr<NSString> valueString = adoptNS([[NSString alloc] initWithFormat:@"%@", [newValue description]]);
                    player.logger().debug(player.logChannel(), identifier, "did change '", [keyPath UTF8String], "' to ", [valueString UTF8String]);
                }
            } else
                player.logger().debug(player.logChannel(), identifier, willChange ? "will" : "did", " change '", [keyPath UTF8String], "'");
        }
    });
}

- (void)legibleOutput:(id)output didOutputAttributedStrings:(NSArray *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime
{
    UNUSED_PARAM(output);

    ensureOnMainThread([self, strongSelf = retainPtr(self), strings = retainPtr(strings), nativeSamples = retainPtr(nativeSamples), itemTime]() mutable {
        if (!m_player)
            return;

        m_player->queueTaskOnEventLoop([player = m_player, strings = WTFMove(strings), nativeSamples = WTFMove(nativeSamples), itemTime] {
            if (!player)
                return;

            ScriptDisallowedScope::InMainThread scriptDisallowedScope;

            MediaTime time = std::max(PAL::toMediaTime(itemTime), MediaTime::zeroTime());
            player->processCue(strings.get(), nativeSamples.get(), time);
        });
    });
}

- (void)outputSequenceWasFlushed:(id)output
{
    UNUSED_PARAM(output);

    ensureOnMainThread([self, strongSelf = retainPtr(self)] {
        if (!m_player)
            return;

        m_player->queueTaskOnEventLoop([player = m_player] {
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

    m_player->queueTaskOnEventLoop([player = m_player, metadataGroups = retainPtr(metadataGroups), currentTime = m_player->currentMediaTime()] {
        if (!player)
            return;

        ScriptDisallowedScope::InMainThread scriptDisallowedScope;

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

    m_player->queueTaskOnEventLoop([player = m_player, metadataGroups = retainPtr(metadataGroups), currentTime = m_player->currentMediaTime()] {
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
        if (!m_player)
            return;

        m_player->queueTaskOnEventLoop([player = m_player, loadingRequest = WTFMove(loadingRequest)] {
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
        if (!m_player)
            return;

        m_player->queueTaskOnEventLoop([player = m_player, loadingRequest = WTFMove(loadingRequest)] {

            ScriptDisallowedScope::InMainThread scriptDisallowedScope;

            if (player)
                player->didCancelLoadingRequest(loadingRequest.get());
        });
    });
}

@end

#endif
