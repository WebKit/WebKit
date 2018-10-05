/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#pragma once

#import <objc/runtime.h>
#import <wtf/SoftLinking.h>

#if USE(APPLE_INTERNAL_SDK)

#import <AVFoundation/AVAssetCache_Private.h>
#import <AVFoundation/AVOutputContext_Private.h>
#import <AVFoundation/AVPlayerItem_Private.h>
#import <AVFoundation/AVPlayerLayer_Private.h>
#import <AVFoundation/AVPlayer_Private.h>

#if PLATFORM(IOS) && HAVE(AVKIT)
#import <AVKit/AVPlayerViewController_WebKitOnly.h>
#endif

#if !PLATFORM(IOS)
#import <AVFoundation/AVStreamDataParser.h>
#endif

#if PLATFORM(IOS)
#import <AVFoundation/AVAudioSession_Private.h>
#endif

#else

#import <AVFoundation/AVPlayer.h>
#import <AVFoundation/AVPlayerItem.h>
#import <AVFoundation/AVPlayerLayer.h>

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000)
NS_ASSUME_NONNULL_BEGIN
@interface AVPlayerItem ()
@property (nonatomic, readonly) NSTimeInterval seekableTimeRangesLastModifiedTime NS_AVAILABLE(10_13, 11_0);
@property (nonatomic, readonly) NSTimeInterval liveUpdateInterval;
@end
NS_ASSUME_NONNULL_END
#endif // (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000)

#if ENABLE(WIRELESS_PLAYBACK_TARGET) || PLATFORM(IOS)

NS_ASSUME_NONNULL_BEGIN

@class AVOutputContext;
@class AVOutputDevice;
@interface AVOutputContext : NSObject <NSSecureCoding>
@property (nonatomic, readonly) NSString *deviceName;
+ (instancetype)outputContext;
+ (nullable AVOutputContext *)sharedAudioPresentationOutputContext;
@property (readonly) BOOL supportsMultipleOutputDevices;
@property (readonly) NSArray<AVOutputDevice *> *outputDevices;
@end

#if !PLATFORM(IOS)
@interface AVPlayer (AVPlayerExternalPlaybackSupportPrivate)
@property (nonatomic, retain) AVOutputContext *outputContext;
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

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) || PLATFORM(IOS)

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

#if PLATFORM(IOS) && !PLATFORM(IOS_SIMULATOR)
@interface AVPlayer (AVPlayerVideoSleepPrevention)
@property (nonatomic, getter=_preventsSleepDuringVideoPlayback, setter=_setPreventsSleepDuringVideoPlayback:) BOOL preventsSleepDuringVideoPlayback;
@end

#endif // PLATFORM(IOS) && !PLATFORM(IOS_SIMULATOR)

#if !PLATFORM(IOS)

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
#import <AVFoundation/AVContentKeySession.h>
@interface AVStreamDataParser () <AVContentKeyRecipient>
@end
#endif

#endif // !PLATFORM(IOS)

#endif // USE(APPLE_INTERNAL_SDK)

#if !USE(APPLE_INTERNAL_SDK) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED < 101300)
@interface AVVideoPerformanceMetrics : NSObject
@property (nonatomic, readonly) unsigned long totalNumberOfVideoFrames;
@property (nonatomic, readonly) unsigned long numberOfDroppedVideoFrames;
@property (nonatomic, readonly) unsigned long numberOfCorruptedVideoFrames;
@property (nonatomic, readonly) unsigned long numberOfDisplayCompositedVideoFrames;
@property (nonatomic, readonly) double totalFrameDelay;
@end
#endif

#if PLATFORM(MAC) && !USE(APPLE_INTERNAL_SDK)
NS_ASSUME_NONNULL_BEGIN
@interface AVStreamDataParser (AVStreamDataParserPrivate)
+ (NSString *)outputMIMECodecParameterForInputMIMECodecParameter:(NSString *)inputMIMECodecParameter;
@end
NS_ASSUME_NONNULL_END
#endif

#if PLATFORM(IOS) && (!HAVE(AVKIT) || !USE(APPLE_INTERNAL_SDK))
#import <AVFoundation/AVPlayerLayer.h>
@interface AVPlayerLayer (AVPlayerLayerPictureInPictureModeSupportPrivate)
- (void)setPIPModeEnabled:(BOOL)flag;
@end
#endif // !HAVE(AVKIT)

#if !USE(APPLE_INTERNAL_SDK) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED < 101404) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MAX_ALLOWED < 120200)
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

#if ((PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED < 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MAX_ALLOWED < 110000)) && __has_include(<AVFoundation/AVQueuedSampleBufferRendering.h>)
#import <AVFoundation/AVQueuedSampleBufferRendering.h>
@class AVVideoPerformanceMetrics;
NS_ASSUME_NONNULL_BEGIN
@interface AVSampleBufferDisplayLayer (VideoPerformanceMetrics)
- (AVVideoPerformanceMetrics *)videoPerformanceMetrics;
@end
NS_ASSUME_NONNULL_END
#elif __has_include(<AVFoundation/AVSampleBufferDisplayLayer_Private.h>)
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

#if ((PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED < 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MAX_ALLOWED < 110000)) && __has_include(<AVFoundation/AVQueuedSampleBufferRendering.h>)
// Nothing to do, AVfoundation/AVQueuedSampleBufferRendering.h was imported above.
#elif __has_include(<AVFoundation/AVSampleBufferAudioRenderer.h>)
#import <AVFoundation/AVSampleBufferAudioRenderer.h>
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
@property (nonatomic, copy) NSString *audioTimePitchAlgorithm;
@end

NS_ASSUME_NONNULL_END

#endif // __has_include(<AVFoundation/AVSampleBufferAudioRenderer.h>)

#if !USE(APPLE_INTERNAL_SDK) && PLATFORM(IOS) && !PLATFORM(IOS_SIMULATOR) && !PLATFORM(IOSMAC)
#import <AVFoundation/AVAudioSession.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVAudioSession (AVAudioSessionPrivate)
@property (readonly) NSString* routingContextUID;
@end

NS_ASSUME_NONNULL_END
#endif

