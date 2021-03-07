/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKWebViewPrivateForTesting.h"

#import "AudioSessionRoutingArbitratorProxy.h"
#import "PlaybackSessionManagerProxy.h"
#import "UserMediaProcessManager.h"
#import "ViewGestureController.h"
#import "WKWebViewIOS.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "WebViewImpl.h"
#import "_WKFrameHandleInternal.h"
#import "_WKInspectorInternal.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/ValidationBubble.h>


@implementation WKWebView (WKTesting)

- (void)_setPageScale:(CGFloat)scale withOrigin:(CGPoint)origin
{
    _page->scalePage(scale, WebCore::roundedIntPoint(origin));
}

- (CGFloat)_pageScale
{
    return _page->pageScaleFactor();
}

- (void)_setContinuousSpellCheckingEnabledForTesting:(BOOL)enabled
{
#if PLATFORM(IOS_FAMILY)
    [_contentView setContinuousSpellCheckingEnabled:enabled];
#else
    _impl->setContinuousSpellCheckingEnabled(enabled);
#endif
}

- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem
{
    if ([userInterfaceItem isEqualToString:@"validationBubble"]) {
        auto* validationBubble = _page->validationBubble();
        String message = validationBubble ? validationBubble->message() : emptyString();
        double fontSize = validationBubble ? validationBubble->fontSize() : 0;
        return @{ userInterfaceItem: @{ @"message": (NSString *)message, @"fontSize": @(fontSize) } };
    }

#if PLATFORM(IOS_FAMILY)
    return [_contentView _contentsOfUserInterfaceItem:(NSString *)userInterfaceItem];
#else
    return nil;
#endif
}

- (void)_requestActiveNowPlayingSessionInfo:(void(^)(BOOL, BOOL, NSString*, double, double, NSInteger))callback
{
    if (!_page) {
        callback(NO, NO, @"", std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), 0);
        return;
    }

    _page->requestActiveNowPlayingSessionInfo([handler = makeBlockPtr(callback)] (bool active, bool registeredAsNowPlayingApplication, String title, double duration, double elapsedTime, uint64_t uniqueIdentifier) {
        handler(active, registeredAsNowPlayingApplication, title, duration, elapsedTime, uniqueIdentifier);
    });
}

- (BOOL)_scrollingUpdatesDisabledForTesting
{
    // For subclasses to override;
    return NO;
}

- (void)_setScrollingUpdatesDisabledForTesting:(BOOL)disabled
{
}

- (void)_doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:(void (^)(void))updateBlock
{
    [self _internalDoAfterNextPresentationUpdate:updateBlock withoutWaitingForPainting:NO withoutWaitingForAnimatedResize:YES];
}

- (void)_doAfterNextVisibleContentRectUpdate:(void (^)(void))updateBlock
{
#if PLATFORM(IOS_FAMILY)
    _visibleContentRectUpdateCallbacks.append(makeBlockPtr(updateBlock));
    [self _scheduleVisibleContentRectUpdate];
#else
    RunLoop::main().dispatch([updateBlock = makeBlockPtr(updateBlock)] {
        updateBlock();
    });
#endif
}

- (void)_disableBackForwardSnapshotVolatilityForTesting
{
    WebKit::ViewSnapshotStore::singleton().setDisableSnapshotVolatilityForTesting(true);
}

- (BOOL)_beginBackSwipeForTesting
{
#if PLATFORM(MAC)
    return _impl->beginBackSwipeForTesting();
#else
    if (!_gestureController)
        return NO;
    return _gestureController->beginSimulatedSwipeInDirectionForTesting(WebKit::ViewGestureController::SwipeDirection::Back);
#endif
}

- (BOOL)_completeBackSwipeForTesting
{
#if PLATFORM(MAC)
    return _impl->completeBackSwipeForTesting();
#else
    if (!_gestureController)
        return NO;
    return _gestureController->completeSimulatedSwipeInDirectionForTesting(WebKit::ViewGestureController::SwipeDirection::Back);
#endif
}

- (void)_resetNavigationGestureStateForTesting
{
#if PLATFORM(MAC)
    if (auto gestureController = _impl->gestureController())
        gestureController->reset();
#else
    if (_gestureController)
        _gestureController->reset();
#endif
}

- (void)_setDefersLoadingForTesting:(BOOL)defersLoading
{
    _page->setDefersLoadingForTesting(defersLoading);
}

- (void)_setShareSheetCompletesImmediatelyWithResolutionForTesting:(BOOL)resolved
{
    _resolutionForShareSheetImmediateCompletionForTesting = resolved;
}

- (void)_processWillSuspendForTesting:(void (^)(void))completionHandler
{
    if (!_page) {
        completionHandler();
        return;
    }
    _page->process().sendPrepareToSuspend(WebKit::IsSuspensionImminent::No, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_processWillSuspendImminentlyForTesting
{
    if (_page)
        _page->process().sendPrepareToSuspend(WebKit::IsSuspensionImminent::Yes, [] { });
}

- (void)_processDidResumeForTesting
{
    if (_page)
        _page->process().sendProcessDidResume();
}

- (void)_setAssertionTypeForTesting:(int)value
{
    if (!_page)
        return;

    _page->process().setAssertionTypeForTesting(static_cast<WebKit::ProcessAssertionType>(value));
}

- (BOOL)_hasServiceWorkerBackgroundActivityForTesting
{
#if ENABLE(SERVICE_WORKER)
    return _page ? _page->process().processPool().hasServiceWorkerBackgroundActivityForTesting() : false;
#else
    return false;
#endif
}

- (BOOL)_hasServiceWorkerForegroundActivityForTesting
{
#if ENABLE(SERVICE_WORKER)
    return _page ? _page->process().processPool().hasServiceWorkerForegroundActivityForTesting() : false;
#else
    return false;
#endif
}

- (void)_denyNextUserMediaRequest
{
#if ENABLE(MEDIA_STREAM)
    WebKit::UserMediaProcessManager::singleton().denyNextUserMediaRequest();
#endif
}

- (double)_mediaCaptureReportingDelayForTesting
{
    return _page->mediaCaptureReportingDelay().value();
}

- (void)_setMediaCaptureReportingDelayForTesting:(double)captureReportingDelay
{
    _page->setMediaCaptureReportingDelay(Seconds(captureReportingDelay));
}

- (BOOL)_wirelessVideoPlaybackDisabled
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (auto* playbackSessionManager = _page->playbackSessionManager())
        return playbackSessionManager->wirelessVideoPlaybackDisabled();
#endif
    return false;
}

- (void)_doAfterProcessingAllPendingMouseEvents:(dispatch_block_t)action
{
    _page->doAfterProcessingAllPendingMouseEvents([action = makeBlockPtr(action)] {
        action();
    });
}

+ (void)_setApplicationBundleIdentifier:(NSString *)bundleIdentifier
{
    WebCore::setApplicationBundleIdentifierOverride(String(bundleIdentifier));
}

+ (void)_clearApplicationBundleIdentifierTestingOverride
{
    WebCore::clearApplicationBundleIdentifierTestingOverride();
}

- (BOOL)_hasSleepDisabler
{
    return _page && _page->process().hasSleepDisabler();
}

- (WKWebViewAudioRoutingArbitrationStatus)_audioRoutingArbitrationStatus
{
#if ENABLE(ROUTING_ARBITRATION)
    switch (_page->process().audioSessionRoutingArbitrator().arbitrationStatus()) {
    case WebKit::AudioSessionRoutingArbitratorProxy::ArbitrationStatus::None: return WKWebViewAudioRoutingArbitrationStatusNone;
    case WebKit::AudioSessionRoutingArbitratorProxy::ArbitrationStatus::Pending: return WKWebViewAudioRoutingArbitrationStatusPending;
    case WebKit::AudioSessionRoutingArbitratorProxy::ArbitrationStatus::Active: return WKWebViewAudioRoutingArbitrationStatusActive;
    default: ASSERT_NOT_REACHED();
    }
#else
    return WKWebViewAudioRoutingArbitrationStatusNone;
#endif
}

- (double)_audioRoutingArbitrationUpdateTime
{
#if ENABLE(ROUTING_ARBITRATION)
    return _page->process().audioSessionRoutingArbitrator().arbitrationUpdateTime().secondsSinceEpoch().seconds();
#else
    return 0;
#endif
}

- (void)_doAfterActivityStateUpdate:(void (^)(void))completionHandler
{
    _page->addActivityStateUpdateCompletionHandler(makeBlockPtr(completionHandler));
}

- (NSNumber *)_suspendMediaPlaybackCounter
{
    return @(_page->suspendMediaPlaybackCounter());
}

- (void)_setPrivateClickMeasurementOverrideTimerForTesting:(BOOL)overrideTimer completionHandler:(void(^)(void))completionHandler
{
    _page->setPrivateClickMeasurementOverrideTimerForTesting(overrideTimer, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAttributionReportURLForTesting:(NSURL *)url completionHandler:(void(^)(void))completionHandler
{
    _page->setPrivateClickMeasurementAttributionReportURLForTesting(url, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_didPresentContactPicker
{
    // For subclasses to override.
}

- (void)_didDismissContactPicker
{
    // For subclasses to override.
}

- (void)_dismissContactPickerWithContacts:(NSArray *)contacts
{
#if PLATFORM(IOS_FAMILY)
    [_contentView _dismissContactPickerWithContacts:contacts];
#endif
}

- (void)_lastNavigationWasAppBound:(void(^)(BOOL))completionHandler
{
    _page->lastNavigationWasAppBound([completionHandler = makeBlockPtr(completionHandler)] (bool isAppBound) {
        completionHandler(isAppBound);
    });
}

- (void)_appBoundNavigationData:(void(^)(struct WKAppBoundNavigationTestingData data))completionHandler
{
    _page->appBoundNavigationData([completionHandler = makeBlockPtr(completionHandler)] (auto&& appBoundData) {
        completionHandler({ appBoundData.hasLoadedAppBoundRequestTesting, appBoundData.hasLoadedNonAppBoundRequestTesting });
    });
}

- (void)_clearAppBoundNavigationData:(void(^)(void))completionHandler
{
    _page->clearAppBoundNavigationData([completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

@end
