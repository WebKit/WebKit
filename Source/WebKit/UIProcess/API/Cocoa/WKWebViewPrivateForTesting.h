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

typedef enum {
    WKWebViewAudioRoutingArbitrationStatusNone,
    WKWebViewAudioRoutingArbitrationStatusPending,
    WKWebViewAudioRoutingArbitrationStatusActive,
} WKWebViewAudioRoutingArbitrationStatus;

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

- (BOOL)_beginBackSwipeForTesting;
- (BOOL)_completeBackSwipeForTesting;
- (void)_resetNavigationGestureStateForTesting;
- (void)_setDefersLoadingForTesting:(BOOL)defersLoading;

- (void)_setShareSheetCompletesImmediatelyWithResolutionForTesting:(BOOL)resolved;

@property (nonatomic, readonly) BOOL _hasInspectorFrontend;

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
@end
