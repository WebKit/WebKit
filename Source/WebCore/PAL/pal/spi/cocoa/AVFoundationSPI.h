/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <objc/runtime.h>
#import <wtf/Platform.h>
#import <wtf/SoftLinking.h>

#if HAVE(AVCONTENTKEYSESSION)
#import <AVFoundation/AVContentKeySession.h>
#endif

#import <pal/spi/cocoa/TCCSPI.h>

#if USE(APPLE_INTERNAL_SDK)

#import <AVFoundation/AVAssetCache_Private.h>
#import <AVFoundation/AVCaptureSession_Private.h>
#import <AVFoundation/AVContentKeySession_Private.h>
#import <AVFoundation/AVMediaSelectionGroup_Private.h>
#import <AVFoundation/AVOutputContext_Private.h>
#import <AVFoundation/AVOutputDevice.h>
#import <AVFoundation/AVPlayerItemOutput_Private.h>
#import <AVFoundation/AVPlayerItem_Private.h>
#import <AVFoundation/AVPlayerLayer_Private.h>
#import <AVFoundation/AVPlayer_Private.h>

#if ENABLE(MEDIA_SOURCE)
#if PLATFORM(IOS_FAMILY_SIMULATOR)
#import "AVStreamDataParserSPI.h"
#else
#import <AVFoundation/AVStreamDataParser.h>
#endif
#endif

#if PLATFORM(IOS_FAMILY)
#import <AVFoundation/AVAudioSession_Private.h>
NS_ASSUME_NONNULL_BEGIN
@interface AVAudioSession (AVAudioSessionWebKitPrivate)
- (BOOL)setAuditTokensForProcessAssertion:(NSArray<NSData *>*)inAuditTokens error:(NSError **)outError;
@end
NS_ASSUME_NONNULL_END
#endif

#else

#import <AVFoundation/AVCaptureSession.h>
#import <AVFoundation/AVPlayer.h>
#import <AVFoundation/AVPlayerItem.h>
#import <AVFoundation/AVPlayerItemOutput.h>

#if HAVE(AVFOUNDATION_INTERSTITIAL_EVENTS)
#import <AVFoundation/AVPlayerInterstitialEventController.h>
#endif

NS_ASSUME_NONNULL_BEGIN
@interface AVPlayerItem ()
@property (nonatomic, readonly) NSTimeInterval seekableTimeRangesLastModifiedTime NS_AVAILABLE(10_13, 11_0);
@property (nonatomic, readonly) NSTimeInterval liveUpdateInterval;
@end
NS_ASSUME_NONNULL_END

#if HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
typedef NSString * AVVideoRange NS_TYPED_ENUM;
@interface AVPlayer (AVPlayerVideoRangeOverride)
@property (nonatomic, copy, nullable) AVVideoRange videoRangeOverride;
+ (nullable AVVideoRange)preferredVideoRangeForDisplays:(nonnull NSArray <NSNumber *>*)displays;
@end
#endif

#if HAVE(AVPLAYER_SUPRESSES_AUDIO_RENDERING)
@interface AVPlayer (AVPlayerSupressesAudioRendering)
@property (nonatomic, getter=_suppressesAudioRendering, setter=_setSuppressesAudioRendering:) BOOL suppressesAudioRendering;
@end
#endif

@interface AVPlayerItemVideoOutput (AVPlayerItemVideoOutputEarliestTime)
@property (nonatomic, readonly) CMTime earliestAvailablePixelBufferItemTime;
- (void)requestNotificationOfMediaDataChangeAsSoonAsPossible;
@end

#if ENABLE(WIRELESS_PLAYBACK_TARGET) || PLATFORM(IOS_FAMILY)

NS_ASSUME_NONNULL_BEGIN

@class AVOutputContext;
@class AVOutputDevice;

@interface AVOutputContext : NSObject <NSSecureCoding>
@property (nonatomic, readonly) NSString *deviceName;
+ (instancetype)outputContext;
+ (instancetype)iTunesAudioContext;
+ (nullable AVOutputContext *)sharedAudioPresentationOutputContext;
+ (nullable AVOutputContext *)sharedSystemAudioContext;
+ (nullable AVOutputContext *)outputContextForID:(NSString *)ID;
@property (readonly) BOOL supportsMultipleOutputDevices;
@property (readonly) NSArray<AVOutputDevice *> *outputDevices;
@property (nonatomic, readonly, nullable) AVOutputDevice *outputDevice;
@end

typedef NS_OPTIONS(NSUInteger, AVOutputDeviceFeatures) {
    AVOutputDeviceFeatureAudio = (1UL << 0),
    AVOutputDeviceFeatureScreen = (1UL << 1),
    AVOutputDeviceFeatureVideo = (1UL << 2),
};

@interface AVOutputDevice : NSObject
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSString *deviceName;
@property (nonatomic, readonly) AVOutputDeviceFeatures deviceFeatures;
@property (nonatomic, readonly) BOOL supportsHeadTrackedSpatialAudio;
- (BOOL)allowsHeadTrackedSpatialAudio;
@end

#if !PLATFORM(IOS_FAMILY)
@interface AVPlayer (AVPlayerExternalPlaybackSupportPrivate)
@property (nonatomic, retain, nullable) AVOutputContext *outputContext;
@end
#else
typedef NS_ENUM(NSInteger, AVPlayerExternalPlaybackType) {
    AVPlayerExternalPlaybackTypeNone,
    AVPlayerExternalPlaybackTypeAirPlay,
    AVPlayerExternalPlaybackTypeTVOut,
};

@interface AVPlayer (AVPlayerExternalPlaybackSupportPrivate)
@property (nonatomic, readonly) AVPlayerExternalPlaybackType externalPlaybackType;
@end
#endif

NS_ASSUME_NONNULL_END

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) || PLATFORM(IOS_FAMILY)

#import <AVFoundation/AVAssetCache.h>
NS_ASSUME_NONNULL_BEGIN
@interface AVAssetCache ()
+ (AVAssetCache *)assetCacheWithURL:(NSURL *)URL;
- (id)initWithURL:(NSURL *)URL;
- (NSArray *)allKeys;
- (NSDate *)lastModifiedDateOfEntryForKey:(NSString *)key;
- (void)removeEntryForKey:(NSString *)key;
@property (nonatomic, readonly, copy) NSURL *URL;
@end
NS_ASSUME_NONNULL_END

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
@interface AVPlayer (AVPlayerVideoSleepPrevention)
@property (nonatomic, getter=_preventsSleepDuringVideoPlayback, setter=_setPreventsSleepDuringVideoPlayback:) BOOL preventsSleepDuringVideoPlayback;
@end

#endif // PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#if ENABLE(MEDIA_SOURCE)

#pragma mark -
#pragma mark AVStreamDataParser

@protocol AVStreamDataParserOutputHandling <NSObject>
@end

typedef int32_t CMPersistentTrackID;

NS_ASSUME_NONNULL_BEGIN
typedef NS_ENUM(NSUInteger, AVStreamDataParserOutputMediaDataFlags) {
    AVStreamDataParserOutputMediaDataReserved = 1 << 0
};

typedef NS_ENUM(NSUInteger, AVStreamDataParserStreamDataFlags) {
    AVStreamDataParserStreamDataDiscontinuity = 1 << 0,
};

@interface AVStreamDataParser : NSObject
- (void)setDelegate:(nullable id<AVStreamDataParserOutputHandling>)delegate;
- (void)appendStreamData:(NSData *)data;
- (void)appendStreamData:(NSData *)data withFlags:(AVStreamDataParserStreamDataFlags)flags;
- (void)setShouldProvideMediaData:(BOOL)shouldProvideMediaData forTrackID:(CMPersistentTrackID)trackID;
- (BOOL)shouldProvideMediaDataForTrackID:(CMPersistentTrackID)trackID;
- (void)providePendingMediaData;
- (void)processContentKeyResponseData:(NSData *)contentKeyResponseData forTrackID:(CMPersistentTrackID)trackID;
- (void)processContentKeyResponseError:(NSError *)error forTrackID:(CMPersistentTrackID)trackID;
- (void)renewExpiringContentKeyResponseDataForTrackID:(CMPersistentTrackID)trackID;
- (NSData *)streamingContentKeyRequestDataForApp:(NSData *)appIdentifier contentIdentifier:(NSData *)contentIdentifier trackID:(CMPersistentTrackID)trackID options:(NSDictionary *)options error:(NSError **)outError;
@end

NS_ASSUME_NONNULL_END

#if HAVE(AVCONTENTKEYSESSION)
@interface AVStreamDataParser () <AVContentKeyRecipient>
@end
#endif

#endif // ENABLE(MEDIA_SOURCE)

#import <AVFoundation/AVMediaSelectionGroup.h>
NS_ASSUME_NONNULL_BEGIN
@interface AVMediaSelectionOption (AVMediaSelectionOption_Private)
@property (nonatomic, readonly) AVAssetTrack *track;
@end
NS_ASSUME_NONNULL_END

#if HAVE(AVCONTENTKEYSESSION)

#if HAVE(AVCONTENTKEYREPORTGROUP)
@interface AVContentKeyReportGroup : NSObject
@property (readonly, nullable) NSData *contentProtectionSessionIdentifier;
- (void)expire;
- (void)processContentKeyRequestWithIdentifier:(nullable id)identifier initializationData:(nullable NSData *)initializationData options:(nullable NSDictionary<NSString *, id> *)options;
- (void)associateContentKeyRequest:(nonnull AVContentKeyRequest *)contentKeyRequest;
@end

@interface AVContentKeySession (AVContentKeyGroup_Support)
@property (readonly, nullable) AVContentKeyReportGroup *defaultContentKeyGroup;
- (nonnull AVContentKeyReportGroup *)makeContentKeyGroup;
@end
#endif

#if HAVE(AVCONTENTKEYSESSIONWILLOUTPUTBEOBSCURED)
@interface AVContentKeyRequest (OutputObscured)
NS_ASSUME_NONNULL_BEGIN
- (BOOL)willOutputBeObscuredDueToInsufficientExternalProtectionForDisplays:(NSArray<NSNumber *> *)displays;
NS_ASSUME_NONNULL_END
@end
#endif

#if HAVE(AVCONTENTKEYREQUEST_PENDING_PROTECTION_STATUS)
typedef NS_ENUM(NSInteger, AVExternalContentProtectionStatus) {
    AVExternalContentProtectionStatusPending      = 0,
    AVExternalContentProtectionStatusSufficient   = 1,
    AVExternalContentProtectionStatusInsufficient = 2,
};

@interface AVContentKeyRequest (AVContentKeyRequest_PendingProtectionStatus)
- (AVExternalContentProtectionStatus)externalContentProtectionStatus;
@end
#endif

#if HAVE(AVCONTENTKEYREQUEST_COMPATABILITIY_MODE)
NS_ASSUME_NONNULL_BEGIN
@interface AVContentKeyRequest (AVContentKeyRequest_WebKitCompatibilityMode)
+ (instancetype)contentKeySessionWithLegacyWebKitCompatibilityModeAndKeySystem:(AVContentKeySystem)keySystem storageDirectoryAtURL:(NSURL *)storageURL;
@end
NS_ASSUME_NONNULL_END
#endif

#endif // HAVE(AVCONTENTKEYSESSION)

#endif // USE(APPLE_INTERNAL_SDK)

#import <AVFoundation/AVPlayerLayer.h>

#if ENABLE(MEDIA_SOURCE) && !USE(APPLE_INTERNAL_SDK)
NS_ASSUME_NONNULL_BEGIN
@interface AVStreamDataParser (AVStreamDataParserPrivate)
+ (NSString *)outputMIMECodecParameterForInputMIMECodecParameter:(NSString *)inputMIMECodecParameter;
@end
NS_ASSUME_NONNULL_END
#endif

#if PLATFORM(IOS_FAMILY) && (!HAVE(AVKIT) || !USE(APPLE_INTERNAL_SDK))
@interface AVPlayerLayer (AVPlayerLayerPictureInPictureModeSupportPrivate)
- (void)setPIPModeEnabled:(BOOL)flag;
@end
#endif // !HAVE(AVKIT)

#if !USE(APPLE_INTERNAL_SDK) || HAVE(VIDEO_PERFORMANCE_METRICS)
@class AVVideoPerformanceMetrics;
NS_ASSUME_NONNULL_BEGIN
@interface AVPlayerLayer (AVPlayerLayerVideoPerformanceMetrics)
- (AVVideoPerformanceMetrics *)videoPerformanceMetrics;
@end
NS_ASSUME_NONNULL_END
#endif

// FIXME: Wrap in a #if USE(APPLE_INTERNAL_SDK) once these SPI land
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVAssetResourceLoader.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVAssetResourceLoader (AVAssetResourceLoaderPrivate)
@property (nonatomic, readonly) id<NSURLSessionDataDelegate> URLSessionDataDelegate;
@property (nonatomic, readonly) NSOperationQueue *URLSessionDataDelegateQueue;
@property (nonatomic, nullable, retain) NSURLSession *URLSession;
@end

@interface AVAsset (AVAssetFragmentsPrivate)
@property (nonatomic, readonly) CMTime overallDurationHint;
@end

NS_ASSUME_NONNULL_END

#import <CoreMedia/CMSampleBuffer.h>
#import <CoreMedia/CMSync.h>

#if __has_include(<AVFoundation/AVSampleBufferRenderSynchronizer.h>)
#import <AVFoundation/AVSampleBufferRenderSynchronizer.h>

NS_ASSUME_NONNULL_BEGIN
@interface AVSampleBufferRenderSynchronizer (AVSampleBufferRenderSynchronizerPrivate)
- (void)removeRenderer:(id)renderer atTime:(CMTime)time withCompletionHandler:(void (^)(BOOL didRemoveRenderer))completionHandler;
@end
NS_ASSUME_NONNULL_END

#else

NS_ASSUME_NONNULL_BEGIN

@interface AVSampleBufferRenderSynchronizer : NSObject
- (CMTimebaseRef)timebase;
- (float)rate;
- (void)setRate:(float)rate;
- (void)setRate:(float)rate time:(CMTime)time;
- (NSArray *)renderers;
- (void)addRenderer:(id)renderer;
- (void)removeRenderer:(id)renderer atTime:(CMTime)time withCompletionHandler:(void (^)(BOOL didRemoveRenderer))completionHandler;
- (id)addPeriodicTimeObserverForInterval:(CMTime)interval queue:(dispatch_queue_t)queue usingBlock:(void (^)(CMTime time))block;
- (id)addBoundaryTimeObserverForTimes:(NSArray *)times queue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block;
- (void)removeTimeObserver:(id)observer;
@end

NS_ASSUME_NONNULL_END

#endif // __has_include(<AVFoundation/AVSampleBufferRenderSynchronizer.h>)

#if __has_include(<AVFoundation/AVSampleBufferDisplayLayer_Private.h>)
#import <AVFoundation/AVSampleBufferDisplayLayer_Private.h>
#elif __has_include(<AVFoundation/AVSampleBufferDisplayLayer.h>)
#import <AVFoundation/AVSampleBufferDisplayLayer.h>
NS_ASSUME_NONNULL_BEGIN
@interface AVSampleBufferDisplayLayer (VideoPerformanceMetrics)
- (AVVideoPerformanceMetrics *)videoPerformanceMetrics;
@end
NS_ASSUME_NONNULL_END
#else

NS_ASSUME_NONNULL_BEGIN

#pragma mark -
#pragma mark AVSampleBufferDisplayLayer

@interface AVSampleBufferDisplayLayer : CALayer
- (NSInteger)status;
- (NSError*)error;
- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer;
- (void)flush;
- (void)flushAndRemoveImage;
- (BOOL)isReadyForMoreMediaData;
- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block;
- (void)stopRequestingMediaData;
- (AVVideoPerformanceMetrics *)videoPerformanceMetrics;
@end
NS_ASSUME_NONNULL_END
#endif // __has_include(<AVFoundation/AVSampleBufferDisplayLayer.h>)

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
@interface AVSampleBufferDisplayLayer (Staging_94324932)
- (nullable CVPixelBufferRef)copyDisplayedPixelBuffer;
@end
#endif

#if __has_include(<AVFoundation/AVSampleBufferAudioRenderer.h>)
#import <AVFoundation/AVSampleBufferAudioRenderer.h>
NS_ASSUME_NONNULL_BEGIN
@interface AVSampleBufferAudioRenderer (AVSampleBufferAudioRendererWebKitOnly)
- (void)setIsUnaccompaniedByVisuals:(BOOL)audioOnly SPI_AVAILABLE(macos(12.0)) API_UNAVAILABLE(ios, tvos, watchos);
@end
NS_ASSUME_NONNULL_END
#else

NS_ASSUME_NONNULL_BEGIN

@interface AVSampleBufferAudioRenderer : NSObject
- (NSInteger)status;
- (NSError*)error;
- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer;
- (void)flush;
- (BOOL)isReadyForMoreMediaData;
- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block;
- (void)stopRequestingMediaData;
- (void)setVolume:(float)volume;
- (void)setMuted:(BOOL)muted;
- (void)setIsUnaccompaniedByVisuals:(BOOL)audioOnly SPI_AVAILABLE(macos(12.0)) API_UNAVAILABLE(ios, tvos, watchos);
@property (nonatomic, copy) NSString *audioTimePitchAlgorithm;
@end

NS_ASSUME_NONNULL_END

#endif // __has_include(<AVFoundation/AVSampleBufferAudioRenderer.h>)

#if !USE(APPLE_INTERNAL_SDK)
@interface AVVideoPerformanceMetrics : NSObject
@property (nonatomic, readonly) unsigned long totalNumberOfVideoFrames;
@property (nonatomic, readonly) unsigned long numberOfDroppedVideoFrames;
@property (nonatomic, readonly) unsigned long numberOfCorruptedVideoFrames;
@property (nonatomic, readonly) unsigned long numberOfDisplayCompositedVideoFrames;
@property (nonatomic, readonly) unsigned long numberOfNonDisplayCompositedVideoFrames;
@property (nonatomic, readonly) double totalFrameDelay;
@end
#else
@interface AVVideoPerformanceMetrics (AVVideoPerformanceMetricsDisplayCompositedVideoFrames)
@property (nonatomic, readonly) unsigned long numberOfDisplayCompositedVideoFrames;
@property (nonatomic, readonly) unsigned long numberOfNonDisplayCompositedVideoFrames;
@end
#endif

#if !USE(APPLE_INTERNAL_SDK)
@interface AVSampleBufferDisplayLayer (WebCorePrivate)
@property (assign, nonatomic) BOOL preventsDisplaySleepDuringVideoPlayback;
@end
#endif

#if !USE(APPLE_INTERNAL_SDK) && PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
#import <AVFoundation/AVAudioSession.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVAudioSession (AVAudioSessionPrivate)
- (instancetype)initAuxiliarySession;
@property (readonly) NSString* routingContextUID;
@property (readonly) BOOL eligibleForBTSmartRoutingConsideration;
- (BOOL)setEligibleForBTSmartRoutingConsideration:(BOOL)inValue error:(NSError **)outError;
- (BOOL)setHostProcessAttribution:(NSArray<NSString *>*)inHostProcessInfo error:(NSError **)outError SPI_AVAILABLE(ios(15.0), watchos(8.0), tvos(15.0)) API_UNAVAILABLE(macCatalyst, macos);
- (BOOL)setAuditTokensForProcessAssertion:(NSArray<NSData *>*)inAuditTokens error:(NSError **)outError;
@end

NS_ASSUME_NONNULL_END
#endif

#if !USE(APPLE_INTERNAL_SDK) && HAVE(AVPLAYER_RESOURCE_CONSERVATION_LEVEL)
@interface AVPlayer (AVPlayerPrivate)

typedef NS_ENUM(NSInteger, AVPlayerResourceConservationLevel) {
    AVPlayerResourceConservationLevelNone                                 = 0,
    AVPlayerResourceConservationLevelReduceReadAhead                      = 1,
    AVPlayerResourceConservationLevelReuseActivePlayerResources           = 2,
    AVPlayerResourceConservationLevelRecycleBuffer                        = 3,
};

@property (nonatomic) AVPlayerResourceConservationLevel resourceConservationLevelWhilePaused;

@end
#endif

#if HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)

NS_ASSUME_NONNULL_BEGIN

// FIXME: Move this inside the #if USE(APPLE_INTERNAL_SDK) once rdar://81297776 has been in the build for awhile.
@interface AVCaptureSession (AVCaptureSessionPrivate)
- (instancetype)initWithAssumedIdentity:(tcc_identity_t)tccIdentity SPI_AVAILABLE(ios(15.0)) API_UNAVAILABLE(macos, macCatalyst, watchos, tvos);
@end

@interface AVCaptureDevice (AVCaptureServerConnection)
+ (void)ensureServerConnection;
@end

NS_ASSUME_NONNULL_END

#endif // HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
