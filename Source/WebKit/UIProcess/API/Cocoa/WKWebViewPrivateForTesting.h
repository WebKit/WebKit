/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "WKWebViewPrivateForTestingIOS.h"
#import "WKWebViewPrivateForTestingMac.h"

NS_ASSUME_NONNULL_BEGIN

typedef enum {
    WKWebViewAudioRoutingArbitrationStatusNone,
    WKWebViewAudioRoutingArbitrationStatusPending,
    WKWebViewAudioRoutingArbitrationStatusActive,
} WKWebViewAudioRoutingArbitrationStatus;

struct WKAppBoundNavigationTestingData {
    BOOL hasLoadedAppBoundRequestTesting;
    BOOL hasLoadedNonAppBoundRequestTesting;
    BOOL didPerformSoftUpdate;
};

@protocol _WKMediaSessionCoordinator;

@interface WKWebView (WKTesting)

- (void)_setPageScale:(CGFloat)scale withOrigin:(CGPoint)origin;
- (CGFloat)_pageScale;

- (void)_setContinuousSpellCheckingEnabledForTesting:(BOOL)enabled;
- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem;

- (void)_requestActiveNowPlayingSessionInfo:(void(^)(BOOL, BOOL, NSString*, double, double, NSInteger))callback;

- (void)_doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:(void (^)(void))updateBlock;
- (void)_doAfterNextVisibleContentRectUpdate:(void (^)(void))updateBlock;

- (void)_disableBackForwardSnapshotVolatilityForTesting;

- (void)_denyNextUserMediaRequest;
@property (nonatomic, setter=_setMediaCaptureReportingDelayForTesting:) double _mediaCaptureReportingDelayForTesting WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, readonly) BOOL _wirelessVideoPlaybackDisabled;

- (BOOL)_beginBackSwipeForTesting;
- (BOOL)_completeBackSwipeForTesting;
- (void)_resetNavigationGestureStateForTesting;
- (void)_setDefersLoadingForTesting:(BOOL)defersLoading;

- (void)_setShareSheetCompletesImmediatelyWithResolutionForTesting:(BOOL)resolved;

- (void)_didShowContextMenu;
- (void)_didDismissContextMenu;

- (void)_didPresentContactPicker;
- (void)_didDismissContactPicker;
- (void)_dismissContactPickerWithContacts:(NSArray *)contacts;

@property (nonatomic, setter=_setScrollingUpdatesDisabledForTesting:) BOOL _scrollingUpdatesDisabledForTesting;

- (void)_processWillSuspendForTesting:(void (^)(void))completionHandler;
- (void)_processWillSuspendImminentlyForTesting;
- (void)_processDidResumeForTesting;
@property (nonatomic, readonly) BOOL _hasServiceWorkerBackgroundActivityForTesting;
@property (nonatomic, readonly) BOOL _hasServiceWorkerForegroundActivityForTesting;
- (void)_setAssertionTypeForTesting:(int)type;

- (void)_doAfterProcessingAllPendingMouseEvents:(dispatch_block_t)action;

+ (void)_setApplicationBundleIdentifier:(NSString *)bundleIdentifier;
+ (void)_clearApplicationBundleIdentifierTestingOverride;

- (BOOL)_hasSleepDisabler;
- (WKWebViewAudioRoutingArbitrationStatus)_audioRoutingArbitrationStatus;
- (double)_audioRoutingArbitrationUpdateTime;

- (void)_doAfterActivityStateUpdate:(void (^)(void))completionHandler;

- (NSNumber *)_suspendMediaPlaybackCounter WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

- (void)_setPrivateClickMeasurementOverrideTimerForTesting:(BOOL)overrideTimer completionHandler:(void(^)(void))completionHandler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
- (void)_setPrivateClickMeasurementAttributionReportURLsForTesting:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL completionHandler:(void(^)(void))completionHandler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
- (void)_setPrivateClickMeasurementAttributionTokenPublicKeyURLForTesting:(NSURL *)url completionHandler:(void(^)(void))completionHandler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
- (void)_setPrivateClickMeasurementAttributionTokenSignatureURLForTesting:(NSURL *)url completionHandler:(void(^)(void))completionHandler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

- (void)_lastNavigationWasAppBound:(void(^)(BOOL))completionHandler;
- (void)_appBoundNavigationData:(void(^)(struct WKAppBoundNavigationTestingData data))completionHandler;
- (void)_clearAppBoundNavigationData:(void(^)(void))completionHandler;

- (void)_createMediaSessionCoordinatorForTesting:(id <_WKMediaSessionCoordinator>)privateCoordinator completionHandler:(void(^)(BOOL))completionHandler;

@end

typedef NS_ENUM(NSInteger, _WKMediaSessionReadyState) {
    WKMediaSessionReadyStateHaveNothing,
    WKMediaSessionReadyStateHaveMetadata,
    WKMediaSessionReadyStateHaveCurrentData,
    WKMediaSessionReadyStateHaveFutureData,
    WKMediaSessionReadyStateHaveEnoughData
};

typedef NS_ENUM(NSInteger, _WKMediaSessionPlaybackState) {
    WKMediaSessionPlaybackStateNone,
    WKMediaSessionPlaybackStatePaused,
    WKMediaSessionPlaybackStatePlaying
};

typedef NS_ENUM(NSInteger, _WKMediaSessionCoordinatorState) {
    WKMediaSessionCoordinatorStateWaiting,
    WKMediaSessionCoordinatorStateJoined,
    WKMediaSessionCoordinatorStateClosed
};

struct _WKMediaPositionState {
    double duration;
    double playbackRate;
    double position;
};

@protocol _WKMediaSessionCoordinatorDelegate <NSObject>
- (void)seekSessionToTime:(double)time withCompletion:(void(^)(BOOL))completionHandler;
- (void)playSessionWithCompletion:(void(^)(BOOL))completionHandler;
- (void)pauseSessionWithCompletion:(void(^)(BOOL))completionHandler;
- (void)setSessionTrack:(NSString*)trackIdentifier withCompletion:(void(^)(BOOL))completionHandler;
@end

@protocol _WKMediaSessionCoordinator <NSObject>
@property (nullable, weak) id <_WKMediaSessionCoordinatorDelegate> delegate;
@property (nonatomic, readonly) NSString * _Nonnull identifier;
- (void)joinWithCompletion:(void(^ _Nonnull)(BOOL))completionHandler;
- (void)leave;
- (void)seekTo:(double)time withCompletion:(void(^ _Nonnull)(BOOL))completionHandler;
- (void)playWithCompletion:(void(^ _Nonnull)(BOOL))completionHandler;
- (void)pauseWithCompletion:(void(^ _Nonnull)(BOOL))completionHandler;
- (void)setTrack:(NSString *_Nonnull)trackIdentifier withCompletion:(void(^ _Nonnull)(BOOL))completionHandler;
- (void)positionStateChanged:(struct _WKMediaPositionState * _Nullable)state;
- (void)readyStateChanged:(_WKMediaSessionReadyState)state;
- (void)playbackStateChanged:(_WKMediaSessionPlaybackState)state;
- (void)coordinatorStateChanged:(_WKMediaSessionCoordinatorState)state;
@end

NS_ASSUME_NONNULL_END
