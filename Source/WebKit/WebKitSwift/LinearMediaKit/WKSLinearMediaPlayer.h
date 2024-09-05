/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>

#if defined(TARGET_OS_VISION) && TARGET_OS_VISION

#import <UIKit/UIKit.h>

#if __has_include(<xpc/xpc.h>)
#import <xpc/xpc.h>
#else
// Avoid importing <wtf/spi/darwin/XPCSPI.h> since this header needs to be parsed as a module, and
// XPCSPI.h has some non-modular includes.
OS_OBJECT_DECL(xpc_object);
#endif

NS_ASSUME_NONNULL_BEGIN

@class LMPlayableViewController;
@class WKSLinearMediaContentMetadata;
@class WKSLinearMediaPlayer;
@class WKSLinearMediaTimeRange;
@class WKSLinearMediaTrack;
@class WKSLinearMediaSpatialVideoMetadata;

typedef NS_ENUM(NSInteger, WKSLinearMediaContentMode);
typedef NS_ENUM(NSInteger, WKSLinearMediaContentType);
typedef NS_ENUM(NSInteger, WKSLinearMediaPresentationState);
typedef NS_ENUM(NSInteger, WKSLinearMediaViewingMode);

typedef NS_OPTIONS(NSInteger, WKSLinearMediaFullscreenBehaviors) {
    WKSLinearMediaFullscreenBehaviorsSceneResize = 1 << 0,
    WKSLinearMediaFullscreenBehaviorsSceneSizeRestrictions = 1 << 1,
    WKSLinearMediaFullscreenBehaviorsSceneChromeOptions = 1 << 2,
    WKSLinearMediaFullscreenBehaviorsHostContentInline = 1 << 3,
};

API_AVAILABLE(visionos(1.0))
@protocol WKSLinearMediaPlayerDelegate <NSObject>
@optional
- (void)linearMediaPlayerPlay:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerPause:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerTogglePlayback:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setPlaybackRate:(double)playbackRate;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekToTime:(NSTimeInterval)time;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekByDelta:(NSTimeInterval)delta;
- (NSTimeInterval)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekToDestination:(NSTimeInterval)destination fromSource:(NSTimeInterval)source;
- (void)linearMediaPlayerBeginScrubbing:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerEndScrubbing:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerBeginScanningForward:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerEndScanningForward:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerBeginScanningBackward:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerEndScanningBackward:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setVolume:(double)volume;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setMuted:(BOOL)muted;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player completeTrimming:(BOOL)commitChanges;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player updateStartTime:(NSTimeInterval)startTime;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player updateEndTime:(NSTimeInterval)endTime;
- (void)linearMediaPlayerBeginEditingVolume:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerEndEditingVolume:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setAudioTrack:(WKSLinearMediaTrack * _Nullable)audioTrack;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setLegibleTrack:(WKSLinearMediaTrack * _Nullable)legibleTrack;
- (void)linearMediaPlayerSkipActiveInterstitial:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setCaptionContentInsets:(UIEdgeInsets)insets;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player updateVideoBounds:(CGRect)videoBounds;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player updateViewingMode:(WKSLinearMediaViewingMode)viewingMode;
- (void)linearMediaPlayerTogglePip:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerEnterFullscreen:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayerExitFullscreen:(WKSLinearMediaPlayer *)player;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setTimeResolverInterval:(NSTimeInterval)interval;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setTimeResolverResolution:(NSTimeInterval)resolution;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setThumbnailSize:(CGSize)size;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekThumbnailToTime:(NSTimeInterval)time;
- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setVideoReceiverEndpoint:(xpc_object_t)videoReceiverEndpoint;
@end

API_AVAILABLE(visionos(1.0))
@interface WKSLinearMediaPlayer : NSObject
@property (nonatomic, weak, nullable) id <WKSLinearMediaPlayerDelegate> delegate;
@property (nonatomic) double selectedPlaybackRate;
@property (nonatomic, readonly) WKSLinearMediaPresentationState presentationState;
@property (nonatomic, strong, nullable) NSError *error;
@property (nonatomic) BOOL canTogglePlayback;
@property (nonatomic) BOOL requiresLinearPlayback;
@property (nonatomic, copy) NSArray<WKSLinearMediaTimeRange *> *interstitialRanges;
@property (nonatomic) BOOL isInterstitialActive;
@property (nonatomic) NSTimeInterval duration;
@property (nonatomic) NSTimeInterval currentTime;
@property (nonatomic) NSTimeInterval remainingTime;
@property (nonatomic) double playbackRate;
@property (nonatomic, copy) NSArray<NSNumber *> *playbackRates;
@property (nonatomic) BOOL isLoading;
@property (nonatomic) BOOL isTrimming;
@property (nonatomic, strong, nullable) UIView *trimView;
@property (nonatomic, strong, nullable) CALayer *thumbnailLayer;
@property (nonatomic, strong, nullable) CALayer *captionLayer;
@property (nonatomic) UIEdgeInsets captionContentInsets;
@property (nonatomic) BOOL showsPlaybackControls;
@property (nonatomic) BOOL canSeek;
@property (nonatomic, copy) NSArray<WKSLinearMediaTimeRange *> *seekableTimeRanges;
@property (nonatomic) BOOL isSeeking;
@property (nonatomic) BOOL canScanBackward;
@property (nonatomic) BOOL canScanForward;
@property (nonatomic, copy) NSArray<UIViewController *> *contentInfoViewControllers;
@property (nonatomic, copy) NSArray<UIAction *> *contextualActions;
@property (nonatomic, strong, nullable) UIView *contextualActionsInfoView;
@property (nonatomic) CGSize contentDimensions;
@property (nonatomic) WKSLinearMediaContentMode contentMode;
@property (nonatomic, strong, nullable) CALayer *videoLayer;
@property (nonatomic) WKSLinearMediaViewingMode anticipatedViewingMode;
@property (nonatomic, strong, nullable) UIView *contentOverlay;
@property (nonatomic, strong, nullable) UIViewController *contentOverlayViewController;
@property (nonatomic) double volume;
@property (nonatomic) BOOL isMuted;
@property (nonatomic, copy, nullable) NSString *sessionDisplayTitle;
@property (nonatomic, strong, nullable) UIImage *sessionThumbnail;
@property (nonatomic) BOOL isSessionExtended;
@property (nonatomic) BOOL hasAudioContent;
@property (nonatomic, strong, nullable) WKSLinearMediaTrack *currentAudioTrack;
@property (nonatomic, copy) NSArray<WKSLinearMediaTrack *> *audioTracks;
@property (nonatomic, strong, nullable) WKSLinearMediaTrack *currentLegibleTrack;
@property (nonatomic, copy) NSArray<WKSLinearMediaTrack *> *legibleTracks;
@property (nonatomic) WKSLinearMediaContentType contentType;
@property (nonatomic, strong) WKSLinearMediaContentMetadata *contentMetadata;
@property (nonatomic) BOOL transportBarIncludesTitleView;
@property (nonatomic, copy, nullable) NSData *artwork;
@property (nonatomic) BOOL isPlayableOffline;
@property (nonatomic) BOOL allowPip;
@property (nonatomic) BOOL allowFullScreenFromInline;
@property (nonatomic) BOOL isLiveStream;
@property (nonatomic, strong, nullable) NSNumber *recommendedViewingRatio;
@property (nonatomic) WKSLinearMediaFullscreenBehaviors fullscreenSceneBehaviors;
@property (nonatomic) double startTime;
@property (nonatomic) double endTime;
@property (nonatomic, strong, nullable) NSDate *startDate;
@property (nonatomic, strong, nullable) NSDate *endDate;
@property (nonatomic) BOOL spatialImmersive;
@property (nonatomic, strong, nullable) WKSLinearMediaSpatialVideoMetadata *spatialVideoMetadata;

- (LMPlayableViewController *)makeViewController;
- (void)enterFullscreenWithCompletionHandler:(void (^)(BOOL, NSError * _Nullable))completionHandler;
- (void)exitFullscreenWithCompletionHandler:(void (^)(BOOL, NSError * _Nullable))completionHandler;
@end

NS_ASSUME_NONNULL_END

#endif /* defined(TARGET_OS_VISION) && TARGET_OS_VISION */
