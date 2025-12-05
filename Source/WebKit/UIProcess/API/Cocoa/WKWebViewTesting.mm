/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#import "EditingRange.h"
#import "GPUProcessProxy.h"
#import "LogStream.h"
#import "MediaSessionCoordinatorProxyPrivate.h"
#import "NetworkProcessProxy.h"
#import "PlaybackSessionManagerProxy.h"
#import "PrintInfo.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "UserMediaProcessManager.h"
#import "ViewGestureController.h"
#import "ViewSnapshotStore.h"
#import "WKColorExtensionView.h"
#import "WKContentViewInteraction.h"
#import "WKPreferencesInternal.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import "WebPageProxyTesting.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "WebViewImpl.h"
#import "WebsiteDataStore.h"
#import "_WKFrameHandleInternal.h"
#import "_WKInspectorInternal.h"
#import <WebCore/BoxSides.h>
#import <WebCore/NowPlayingInfo.h>
#import <WebCore/ScrollingNodeID.h>
#import <WebCore/ValidationBubble.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/TZoneMallocInlines.h>
#if USE(APPLE_INTERNAL_SDK)
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/darwin/XPCSPI.h>
#endif
#import <wtf/text/MakeString.h>

#if PLATFORM(MAC)
#import "WKWebViewMac.h"
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
#import "WindowServerConnection.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import "WKWebViewIOS.h"
#endif

#if ENABLE(MODEL_PROCESS)
#import "ModelProcessProxy.h"
#endif

#if ENABLE(THREADED_ANIMATIONS)
#import "RemoteAnimationStack.h"
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
@interface WKMediaSessionCoordinatorHelper : NSObject <_WKMediaSessionCoordinatorDelegate>
- (id)initWithCoordinator:(WebCore::MediaSessionCoordinatorClient*)coordinator;
- (void)seekSessionToTime:(double)time withCompletion:(void(^)(BOOL))completionHandler;
- (void)playSessionWithCompletion:(void(^)(BOOL))completionHandler;
- (void)pauseSessionWithCompletion:(void(^)(BOOL))completionHandler;
- (void)setSessionTrack:(NSString*)trackIdentifier withCompletion:(void(^)(BOOL))completionHandler;
- (void)coordinatorStateChanged:(_WKMediaSessionCoordinatorState)state;
@end
#endif

@implementation WKWebView (WKTesting)

- (_WKRectEdge)_fixedContainerEdges
{
    // FIXME: Remove once it's no longer required to maintain binary compatibility with internal clients.
    _WKRectEdge edges = _WKRectEdgeNone;
    if (_fixedContainerEdges.hasFixedEdge(WebCore::BoxSide::Bottom))
        edges |= _WKRectEdgeBottom;
    if (_fixedContainerEdges.hasFixedEdge(WebCore::BoxSide::Left))
        edges |= _WKRectEdgeLeft;
    if (_fixedContainerEdges.hasFixedEdge(WebCore::BoxSide::Right))
        edges |= _WKRectEdgeRight;
    if (_fixedContainerEdges.hasFixedEdge(WebCore::BoxSide::Top))
        edges |= _WKRectEdgeTop;
    return edges;
}

- (WebCore::CocoaColor *)_sampledBottomFixedPositionContentColor
{
    return sampledFixedPositionContentColor(_fixedContainerEdges, WebCore::BoxSide::Bottom);
}

- (WebCore::CocoaColor *)_sampledLeftFixedPositionContentColor
{
    return sampledFixedPositionContentColor(_fixedContainerEdges, WebCore::BoxSide::Left);
}

- (WebCore::CocoaColor *)_sampledRightFixedPositionContentColor
{
    return sampledFixedPositionContentColor(_fixedContainerEdges, WebCore::BoxSide::Right);
}

- (NSString *)_caLayerTreeAsTextForLayerWithID:(unsigned long long)layerID
{
    if (!layerID)
        return nil;
    RetainPtr layer = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(_page->protectedDrawingArea())->layerWithIDForTesting({ ObjectIdentifier<WebCore::PlatformLayerIdentifierType>(layerID), _page->legacyMainFrameProcess().coreProcessIdentifier() });
    if (!layer)
        return nil;

    return [self _caLayerTreeAsTextForLayer:layer.get()];
}

- (NSString *)_caLayerTreeAsTextForLayer:(CALayer *)layer
{
    TextStream ts(TextStream::LineMode::MultipleLine);

    {
        TextStream::GroupScope scope(ts);
        ts << "CALayer tree root "_s;
        dumpCALayer(ts, layer, true);
    }

    return ts.release().createNSString().autorelease();
}

- (NSString *)_caLayerTreeAsText
{
#if PLATFORM(IOS_FAMILY)
        return [self _caLayerTreeAsTextForLayer:[_contentView layer]];
#else
        return [self _caLayerTreeAsTextForLayer:retainPtr(self.layer).get()];
#endif
}

static void dumpCALayer(TextStream& ts, CALayer *layer, bool traverse)
{
    auto rectToString = [] (auto rect) {
        return makeString("[x: "_s, rect.origin.x, " y: "_s, rect.origin.x, " width: "_s, rect.size.width, " height: "_s, rect.size.height, ']');
    };

    auto pointToString = [] (auto point) {
        return makeString("[x: "_s, point.x, " y: "_s, point.x, ']');
    };

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    NSNumber *interactionRegionLayerType = [layer valueForKey:@"WKInteractionRegionType"];
    if (interactionRegionLayerType) {
        traverse = false;

        ts.dumpProperty("type"_s, interactionRegionLayerType);

        if (layer.mask) {
            TextStream::GroupScope scope(ts);
            ts << "mask"_s;
            ts.dumpProperty("frame"_s, rectToString(layer.mask.frame));
        }
    }
#endif

    ts.dumpProperty("layer bounds"_s, rectToString(layer.bounds));

    if (layer.position.x || layer.position.y)
        ts.dumpProperty("layer position"_s, pointToString(layer.position));

    if (layer.zPosition)
        ts.dumpProperty("layer zPosition"_s, makeString(layer.zPosition));

    if (layer.anchorPoint.x != 0.5 || layer.anchorPoint.y != 0.5)
        ts.dumpProperty("layer anchorPoint"_s, pointToString(layer.anchorPoint));

    if (layer.anchorPointZ)
        ts.dumpProperty("layer anchorPointZ"_s, makeString(layer.anchorPointZ));

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (layer.separated)
        ts.dumpProperty("separated"_s, true);
#endif

    if (layer.opacity != 1.0)
        ts.dumpProperty("layer opacity"_s, makeString(layer.opacity));

    if (layer.cornerRadius != 0.0)
        ts.dumpProperty("layer cornerRadius"_s, makeString(layer.cornerRadius));

    constexpr CACornerMask allCorners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner | kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
    if (layer.maskedCorners != allCorners)
        ts.dumpProperty("layer masked corners"_s, makeString(layer.maskedCorners));
    
    if (traverse && layer.sublayers.count > 0) {
        TextStream::GroupScope scope(ts);
        ts << "sublayers"_s;
        for (CALayer *sublayer in layer.sublayers) {
            TextStream::GroupScope scope(ts);
            dumpCALayer(ts, sublayer, true);
        }
    }
}

- (void)_addEventAttributionWithSourceID:(uint8_t)sourceID destinationURL:(NSURL *)destination sourceDescription:(NSString *)sourceDescription purchaser:(NSString *)purchaser reportEndpoint:(NSURL *)reportEndpoint optionalNonce:(NSString *)nonce applicationBundleID:(NSString *)bundleID ephemeral:(BOOL)ephemeral
{
    WebCore::PrivateClickMeasurement measurement(
        WebCore::PrivateClickMeasurement::SourceID(sourceID),
        WebCore::PCM::SourceSite(reportEndpoint),
        WebCore::PCM::AttributionDestinationSite(destination),
        bundleID,
        WallTime::now(),
        ephemeral ? WebCore::PCM::AttributionEphemeral::Yes : WebCore::PCM::AttributionEphemeral::No
    );
    if (nonce)
        measurement.setEphemeralSourceNonce({ nonce });

    _page->setPrivateClickMeasurement(WTFMove(measurement));
}

- (void)_setPageScale:(CGFloat)scale withOrigin:(CGPoint)origin
{
    _page->scalePage(scale, WebCore::roundedIntPoint(origin), [] { });
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

- (void)_setGrammarCheckingEnabledForTesting:(BOOL)enabled
{
#if PLATFORM(IOS_FAMILY)
    [_contentView setGrammarCheckingEnabled:enabled];
#else
    _impl->setGrammarCheckingEnabled(enabled);
#endif
}

- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem
{
    if ([userInterfaceItem isEqualToString:@"validationBubble"]) {
        RefPtr validationBubble = _page->validationBubble();
        String message = validationBubble ? validationBubble->message() : emptyString();
        double fontSize = validationBubble ? validationBubble->fontSize() : 0;
        return @{ userInterfaceItem: @{ @"message": message.createNSString().get(), @"fontSize": @(fontSize) } };
    }

    if (RetainPtr contents = _page->contentsOfUserInterfaceItem(userInterfaceItem))
        return contents.autorelease();

#if PLATFORM(IOS_FAMILY)
    return [_contentView _contentsOfUserInterfaceItem:userInterfaceItem];
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

    _page->requestActiveNowPlayingSessionInfo([handler = makeBlockPtr(callback)] (bool registeredAsNowPlayingApplication, WebCore::NowPlayingInfo&& nowPlayingInfo) {
        handler(nowPlayingInfo.allowsNowPlayingControlsVisibility, registeredAsNowPlayingApplication, nowPlayingInfo.metadata.title.createNSString().get(), nowPlayingInfo.duration, nowPlayingInfo.currentTime, nowPlayingInfo.uniqueIdentifier ? nowPlayingInfo.uniqueIdentifier->toUInt64() : 0);
    });
}

- (void)_setNowPlayingMetadataObserver:(void(^)(_WKNowPlayingMetadata *))observer
{
    RefPtr page = _page;
    if (!page)
        return;

    if (!observer) {
        page->setNowPlayingMetadataObserverForTesting(nullptr);
        return;
    }

    auto nowPlayingMetadataObserver = WebCore::NowPlayingMetadataObserver::create([observer = makeBlockPtr(observer)](auto& metadata) {
        RetainPtr nowPlayingMetadata = adoptNS([[_WKNowPlayingMetadata alloc] init]);
        [nowPlayingMetadata setTitle:metadata.title.createNSString().get()];
        [nowPlayingMetadata setArtist:metadata.artist.createNSString().get()];
        [nowPlayingMetadata setAlbum:metadata.album.createNSString().get()];
        [nowPlayingMetadata setSourceApplicationIdentifier:metadata.sourceApplicationIdentifier.createNSString().get()];
        observer(nowPlayingMetadata.get());
    });

    page->setNowPlayingMetadataObserverForTesting(WTFMove(nowPlayingMetadataObserver));
}

- (BOOL)_scrollingUpdatesDisabledForTesting
{
    // For subclasses to override;
    return NO;
}

- (NSString *)_scrollingTreeAsText
{
    CheckedPtr coordinator = _page->scrollingCoordinatorProxy();
    if (!coordinator)
        return @"";

    return coordinator->scrollingTreeAsText().createNSString().autorelease();
}

- (pid_t)_networkProcessIdentifier
{
    RefPtr networkProcess = _page->websiteDataStore().networkProcessIfExists();
    RELEASE_ASSERT(networkProcess);
    return networkProcess->processID();
}

- (void)_setScrollingUpdatesDisabledForTesting:(BOOL)disabled
{
}

- (unsigned long)_countOfUpdatesWithLayerChanges
{
    if (RefPtr drawingAreaProxy = dynamicDowncast<WebKit::RemoteLayerTreeDrawingAreaProxy>(_page->drawingArea()))
        return drawingAreaProxy->countOfTransactionsWithNonEmptyLayerChanges();

    return 0;
}

- (void)_doAfterNextPresentationUpdateWithoutWaitingForAnimatedResizeForTesting:(void (^)(void))updateBlock
{
    [self _internalDoAfterNextPresentationUpdate:updateBlock withoutWaitingForPainting:NO withoutWaitingForAnimatedResize:YES];
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

- (BOOL)_didCallEndSwipeGestureForTesting
{
#if PLATFORM(MAC)
    return _impl->didCallEndSwipeGestureForTesting();
#else
    return _gestureController && _gestureController->didCallEndSwipeGesture();
#endif
}

- (void)_resetNavigationGestureStateForTesting
{
#if PLATFORM(MAC)
    if (RefPtr gestureController = _impl->gestureController())
        gestureController->reset();
#else
    if (_gestureController)
        _gestureController->reset();
#endif
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
    _page->protectedLegacyMainFrameProcess()->sendPrepareToSuspend(WebKit::IsSuspensionImminent::No, 0.0, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_processWillSuspendImminentlyForTesting
{
    if (_page)
        _page->protectedLegacyMainFrameProcess()->sendPrepareToSuspend(WebKit::IsSuspensionImminent::Yes, 0.0, [] { });
}

- (void)_processDidResumeForTesting
{
    if (_page)
        _page->protectedLegacyMainFrameProcess()->sendProcessDidResume(WebKit::AuxiliaryProcessProxy::ResumeReason::ForegroundActivity);
}

- (void)_setThrottleStateForTesting:(int)value
{
    if (!_page)
        return;

    _page->protectedLegacyMainFrameProcess()->setThrottleStateForTesting(static_cast<WebKit::ProcessThrottleState>(value));
}

- (BOOL)_hasServiceWorkerBackgroundActivityForTesting
{
    return _page ? _page->configuration().protectedProcessPool()->hasServiceWorkerBackgroundActivityForTesting() : false;
}

- (BOOL)_hasServiceWorkerForegroundActivityForTesting
{
    return _page ? _page->configuration().protectedProcessPool()->hasServiceWorkerForegroundActivityForTesting() : false;
}

- (void)_denyNextUserMediaRequest
{
#if ENABLE(MEDIA_STREAM)
    WebKit::UserMediaProcessManager::singleton().denyNextUserMediaRequest();
#endif
}

- (void)_setIndexOfGetDisplayMediaDeviceSelectedForTesting:(NSNumber *)nsIndex
{
#if ENABLE(MEDIA_STREAM)
    if (!_page)
        return;

    std::optional<unsigned> index;
    if (nsIndex)
        index = nsIndex.unsignedIntValue;

    if (RefPtr pageForTesting = _page->pageForTesting())
        pageForTesting->setIndexOfGetDisplayMediaDeviceSelectedForTesting(index);
#endif
}

- (void)_setSystemCanPromptForGetDisplayMediaForTesting:(BOOL)canPrompt
{
#if ENABLE(MEDIA_STREAM)
    if (!_page)
        return;

    if (RefPtr pageForTesting = _page->pageForTesting())
        pageForTesting->setSystemCanPromptForGetDisplayMediaForTesting(!!canPrompt);
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
    if (RefPtr playbackSessionManager = _page->playbackSessionManager())
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
    setApplicationBundleIdentifierOverride(String(bundleIdentifier));
}

+ (void)_clearApplicationBundleIdentifierTestingOverride
{
    clearApplicationBundleIdentifierTestingOverride();
}

- (BOOL)_hasSleepDisabler
{
    return _page && _page->hasSleepDisabler();
}

- (NSString*)_scrollbarStateForScrollingNodeID:(uint64_t)scrollingNodeID processID:(uint64_t)processID isVertical:(bool)isVertical
{
    if (!_page || !ObjectIdentifier<WebCore::ProcessIdentifierType>::isValidIdentifier(processID) || !ObjectIdentifier<WebCore::ScrollingNodeIDType>::isValidIdentifier(scrollingNodeID))
        return @"";
    return _page->scrollbarStateForScrollingNodeID(WebCore::ScrollingNodeID(ObjectIdentifier<WebCore::ScrollingNodeIDType>(scrollingNodeID), ObjectIdentifier<WebCore::ProcessIdentifierType>(processID)), isVertical).createNSString().autorelease();
}

- (WKWebViewAudioRoutingArbitrationStatus)_audioRoutingArbitrationStatus
{
#if ENABLE(ROUTING_ARBITRATION)
    WeakPtr arbitrator = _page->legacyMainFrameProcess().audioSessionRoutingArbitrator();
    if (!arbitrator)
        return WKWebViewAudioRoutingArbitrationStatusNone;

    switch (arbitrator->arbitrationStatus()) {
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
    WeakPtr arbitrator = _page->legacyMainFrameProcess().audioSessionRoutingArbitrator();
    if (!arbitrator)
        return 0;

    return arbitrator->arbitrationUpdateTime().secondsSinceEpoch().seconds();
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
    RefPtr pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementOverrideTimer(overrideTimer, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAttributionReportURLsForTesting:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL completionHandler:(void(^)(void))completionHandler
{
    RefPtr pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementAttributionReportURLs(sourceURL, destinationURL, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAttributionTokenPublicKeyURLForTesting:(NSURL *)url completionHandler:(void(^)(void))completionHandler
{
    RefPtr pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementTokenPublicKeyURL(url, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAttributionTokenSignatureURLForTesting:(NSURL *)url completionHandler:(void(^)(void))completionHandler
{
    RefPtr pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementTokenSignatureURL(url, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAppBundleIDForTesting:(NSString *)appBundleID completionHandler:(void(^)(void))completionHandler
{
    RefPtr pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementAppBundleID(appBundleID, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_dumpPrivateClickMeasurement:(void(^)(NSString *))completionHandler
{
    RefPtr pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler({ });

    pageForTesting->dumpPrivateClickMeasurement([completionHandler = makeBlockPtr(completionHandler)](const String& privateClickMeasurement) {
        completionHandler(privateClickMeasurement.createNSString().get());
    });
}

- (BOOL)_shouldBypassGeolocationPromptForTesting
{
    // For subclasses to override.
    return NO;
}

- (void)_didShowContextMenu
{
    // For subclasses to override.
}

- (void)_didDismissContextMenu
{
    // For subclasses to override.
}

- (void)_resetInteraction
{
#if PLATFORM(IOS_FAMILY)
    [_contentView cleanUpInteraction];
    [_contentView setUpInteraction];
#endif
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

- (void)_getRenderTreeAsStringWithCompletionHandler:(void(^)(NSString *, NSError *))callback
{
    _page->getRenderTreeExternalRepresentation([callback = makeBlockPtr(callback)] (const String& result) {
        callback(result.createNSString().get(), nil);
    });
}

- (void)_lastNavigationWasAppInitiated:(void(^)(BOOL))completionHandler
{
    _page->lastNavigationWasAppInitiated([completionHandler = makeBlockPtr(completionHandler)] (bool isAppInitiated) {
        completionHandler(isAppInitiated);
    });
}

- (void)_appPrivacyReportTestingData:(void(^)(struct WKAppPrivacyReportTestingData data))completionHandler
{
    _page->appPrivacyReportTestingData([completionHandler = makeBlockPtr(completionHandler)] (auto&& appPrivacyReportTestingData) {
        completionHandler({ appPrivacyReportTestingData.hasLoadedAppInitiatedRequestTesting, appPrivacyReportTestingData.hasLoadedNonAppInitiatedRequestTesting, appPrivacyReportTestingData.didPerformSoftUpdate });
    });
}

- (void)_clearAppPrivacyReportTestingData:(void(^)(void))completionHandler
{
    _page->clearAppPrivacyReportTestingData([completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_isLayerTreeFrozenForTesting:(void (^)(BOOL frozen))completionHandler
{
    RefPtr pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler(false);

    pageForTesting->isLayerTreeFrozen([completionHandler = makeBlockPtr(completionHandler)](bool isFrozen) {
        completionHandler(isFrozen);
    });
}

- (void)_computePagesForPrinting:(_WKFrameHandle *)handle completionHandler:(void(^)(void))completionHandler
{
    WebKit::PrintInfo printInfo;
    _page->computePagesForPrinting(*handle->_frameHandle->frameID(), printInfo, [completionHandler = makeBlockPtr(completionHandler)] (const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&) {
        completionHandler();
    });
}

- (void)_gpuToWebProcessConnectionCountForTesting:(void(^)(NSUInteger))completionHandler
{
    RefPtr gpuProcess = _page->configuration().processPool().gpuProcess();
    if (!gpuProcess) {
        completionHandler(0);
        return;
    }

    gpuProcess->webProcessConnectionCountForTesting([completionHandler = makeBlockPtr(completionHandler)](uint64_t count) {
        completionHandler(count);
    });
}

- (void)_setConnectedToHardwareConsoleForTesting:(BOOL)connected
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    WebKit::WindowServerConnection::singleton().hardwareConsoleStateChanged(connected ? WebKit::WindowServerConnection::HardwareConsoleState::Connected : WebKit::WindowServerConnection::HardwareConsoleState::Disconnected);
#endif
}

- (void)_setSystemPreviewCompletionHandlerForLoadTesting:(void(^)(bool))completionHandler
{
#if USE(SYSTEM_PREVIEW)
    _page->setSystemPreviewCompletionHandlerForLoadTesting(makeBlockPtr(completionHandler));
#endif
}

- (void)_createMediaSessionCoordinatorForTesting:(id <_WKMediaSessionCoordinator>)privateCoordinator completionHandler:(void(^)(BOOL))completionHandler
{
#if ENABLE(MEDIA_SESSION_COORDINATOR)
    class WKMediaSessionCoordinatorForTesting
        : public WebKit::MediaSessionCoordinatorProxyPrivate
        , public WebCore::MediaSessionCoordinatorClient {
        WTF_MAKE_TZONE_ALLOCATED_INLINE(WKMediaSessionCoordinatorForTesting);
    public:

        static Ref<WKMediaSessionCoordinatorForTesting> create(id <_WKMediaSessionCoordinator> privateCoordinator)
        {
            return adoptRef(*new WKMediaSessionCoordinatorForTesting(privateCoordinator));
        }

        USING_CAN_MAKE_WEAKPTR(WebCore::MediaSessionCoordinatorClient);

        void ref() const final { WebKit::MediaSessionCoordinatorProxyPrivate::ref(); }
        void deref() const final { WebKit::MediaSessionCoordinatorProxyPrivate::deref(); }

    private:
        explicit WKMediaSessionCoordinatorForTesting(id <_WKMediaSessionCoordinator> clientCoordinator)
            : WebKit::MediaSessionCoordinatorProxyPrivate()
            , m_clientCoordinator(clientCoordinator)
        {
            m_coordinatorDelegate = adoptNS([[WKMediaSessionCoordinatorHelper alloc] initWithCoordinator:this]);
            [m_clientCoordinator setDelegate:m_coordinatorDelegate.get()];
        }

        void seekSessionToTime(double time, CompletionHandler<void(bool)>&& callback) final
        {
            if (RefPtr coordinatorClient = client())
                coordinatorClient->seekSessionToTime(time, WTFMove(callback));
            else
                callback(false);
        }

        void playSession(std::optional<double> atTime, std::optional<MonotonicTime> hostTime, CompletionHandler<void(bool)>&& callback) final
        {
            if (RefPtr coordinatorClient = client())
                coordinatorClient->playSession(WTFMove(atTime), WTFMove(hostTime), WTFMove(callback));
            else
                callback(false);
        }

        void pauseSession(CompletionHandler<void(bool)>&& callback) final
        {
            if (RefPtr coordinatorClient = client())
                coordinatorClient->pauseSession(WTFMove(callback));
            else
                callback(false);
        }

        void setSessionTrack(const String& trackIdentifier, CompletionHandler<void(bool)>&& callback) final
        {
            if (RefPtr coordinatorClient = client())
                coordinatorClient->setSessionTrack(trackIdentifier, WTFMove(callback));
            else
                callback(false);
        }

        void coordinatorStateChanged(WebCore::MediaSessionCoordinatorState state) final
        {
            if (RefPtr coordinatorClient = client())
                coordinatorClient->coordinatorStateChanged(state);
        }

        std::optional<WebCore::ExceptionData> result(bool success) const
        {
            if (!success)
                return { WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, String() } };

            return { };
        }

        String identifier() const
        {
            return [m_clientCoordinator identifier];
        }

        void join(WebKit::MediaSessionCommandCompletionHandler&& callback) final
        {
            [m_clientCoordinator joinWithCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (BOOL success) mutable {
                if (!weakThis) {
                    callback(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, String() });
                    return;
                }

                callback(weakThis->result(success));
            }).get()];
        }

        void leave() final
        {
            [m_clientCoordinator leave];
        }

        void seekTo(double time, WebKit::MediaSessionCommandCompletionHandler&& callback) final
        {
            [m_clientCoordinator seekTo:time withCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (BOOL success) mutable {
                if (!weakThis) {
                    callback(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, String() });
                    return;
                }

                callback(weakThis->result(success));
            }).get()];
        }

        void play(WebKit::MediaSessionCommandCompletionHandler&& callback) final
        {
            [m_clientCoordinator playWithCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (BOOL success) mutable {
                if (!weakThis) {
                    callback(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, String() });
                    return;
                }

                callback(weakThis->result(success));
            }).get()];
        }

        void pause(WebKit::MediaSessionCommandCompletionHandler&& callback) final
        {
            [m_clientCoordinator pauseWithCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (BOOL success) mutable {
                if (!weakThis) {
                    callback(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, String() });
                    return;
                }

                callback(weakThis->result(success));
            }).get()];
        }

        void setTrack(const String& track, WebKit::MediaSessionCommandCompletionHandler&& callback) final
        {
            [m_clientCoordinator setTrack:track.createNSString().get() withCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (BOOL success) mutable {
                if (!weakThis) {
                    callback(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, String() });
                    return;
                }

                callback(weakThis->result(success));
            }).get()];
        }

        void positionStateChanged(const std::optional<WebCore::MediaPositionState>& state) final
        {
            if (!state) {
                [m_clientCoordinator positionStateChanged:nil];
                return;
            }

            _WKMediaPositionState position = {
                .duration = state->duration,
                .playbackRate = state->playbackRate,
                .position = state->position
            };
            [m_clientCoordinator positionStateChanged:&position];
        }

        void readyStateChanged(WebCore::MediaSessionReadyState state) final
        {
            static_assert(static_cast<size_t>(WebCore::MediaSessionReadyState::Havenothing) == static_cast<size_t>(WKMediaSessionReadyStateHaveNothing), "WKMediaSessionReadyStateHaveNothing does not match WebKit value");
            static_assert(static_cast<size_t>(WebCore::MediaSessionReadyState::Havemetadata) == static_cast<size_t>(WKMediaSessionReadyStateHaveMetadata), "WKMediaSessionReadyStateHaveMetadata does not match WebKit value");
            static_assert(static_cast<size_t>(WebCore::MediaSessionReadyState::Havecurrentdata) == static_cast<size_t>(WKMediaSessionReadyStateHaveCurrentData), "WKMediaSessionReadyStateHaveCurrentData does not match WebKit value");
            static_assert(static_cast<size_t>(WebCore::MediaSessionReadyState::Havefuturedata) == static_cast<size_t>(WKMediaSessionReadyStateHaveFutureData), "WKMediaSessionReadyStateHaveFutureData does not match WebKit value");
            static_assert(static_cast<size_t>(WebCore::MediaSessionReadyState::Haveenoughdata) == static_cast<size_t>(WKMediaSessionReadyStateHaveEnoughData), "WKMediaSessionReadyStateHaveEnoughData does not match WebKit value");

            [m_clientCoordinator readyStateChanged:static_cast<_WKMediaSessionReadyState>(state)];
        }

        void playbackStateChanged(WebCore::MediaSessionPlaybackState state) final
        {
            static_assert(static_cast<size_t>(WebCore::MediaSessionPlaybackState::None) == static_cast<size_t>(WKMediaSessionPlaybackStateNone), "WKMediaSessionPlaybackStateNone does not match WebKit value");
            static_assert(static_cast<size_t>(WebCore::MediaSessionPlaybackState::Paused) == static_cast<size_t>(WKMediaSessionPlaybackStatePaused), "WKMediaSessionPlaybackStatePaused does not match WebKit value");
            static_assert(static_cast<size_t>(WebCore::MediaSessionPlaybackState::Playing) == static_cast<size_t>(WKMediaSessionPlaybackStatePlaying), "WKMediaSessionPlaybackStatePlaying does not match WebKit value");

            [m_clientCoordinator playbackStateChanged:static_cast<_WKMediaSessionPlaybackState>(state)];
        }

        void trackIdentifierChanged(const String& identifier) final
        {
            [m_clientCoordinator trackIdentifierChanged:identifier.createNSString().get()];
        }

    private:
        RetainPtr<id <_WKMediaSessionCoordinator>> m_clientCoordinator;
        RetainPtr<WKMediaSessionCoordinatorHelper> m_coordinatorDelegate;
    };

    ASSERT(!_impl->mediaSessionCoordinatorForTesting());

    _impl->setMediaSessionCoordinatorForTesting(WKMediaSessionCoordinatorForTesting::create(privateCoordinator).ptr());
    _impl->page().createMediaSessionCoordinator(*_impl->mediaSessionCoordinatorForTesting(), [completionHandler = makeBlockPtr(completionHandler)] (bool success) {
        completionHandler(success);
    });
#else

    completionHandler(NO);

#endif
}

- (BOOL)_isLoggerEnabledForTesting
{
    return _page->logger().enabled();
}

- (void)_terminateIdleServiceWorkersForTesting
{
    Ref protectedProcessProxy = _page->legacyMainFrameProcess();
    RefPtr store = protectedProcessProxy->websiteDataStore();
    RefPtr networkProcess = store ? store->networkProcessIfExists() : nullptr;
    if (networkProcess)
        networkProcess->terminateIdleServiceWorkers(protectedProcessProxy->coreProcessIdentifier(), [] { });
}

- (void)_getNotifyStateForTesting:(NSString *)notificationName completionHandler:(void(^)(NSNumber *))completionHandler
{
#if ENABLE(NOTIFY_BLOCKING)
    _page->protectedLegacyMainFrameProcess()->getNotifyStateForTesting(notificationName, [completionHandler = WTFMove(completionHandler)](std::optional<uint64_t> result) mutable {
        if (!result) {
            completionHandler(nil);
            return;
        }
        completionHandler(@(result.value()));
    });
#else
    completionHandler(nil);
#endif
}

- (BOOL)_hasAccessibilityActivityForTesting
{
#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    return _page->hasAccessibilityActivityForTesting();
#else
    return NO;
#endif
}

- (void)_setMediaVolumeForTesting:(float)mediaVolume
{
    _page->setMediaVolume(mediaVolume);
}

- (NSDictionary<NSString *, id> *)_propertiesOfLayerWithID:(unsigned long long)layerID
{
    if (!layerID)
        return nil;
    RetainPtr layer = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(_page->protectedDrawingArea())->layerWithIDForTesting({ ObjectIdentifier<WebCore::PlatformLayerIdentifierType>(layerID), _page->legacyMainFrameProcess().coreProcessIdentifier() });
    if (!layer)
        return nil;

    return @{
        @"bounds" : @{
            @"x" : @(layer.get().bounds.origin.x),
            @"y" : @(layer.get().bounds.origin.x),
            @"width" : @(layer.get().bounds.size.width),
            @"height" : @(layer.get().bounds.size.height),

        },
        @"position" : @{
            @"x" : @(layer.get().position.x),
            @"y" : @(layer.get().position.y),
        },
        @"zPosition" : @(layer.get().zPosition),
        @"anchorPoint" : @{
            @"x" : @(layer.get().anchorPoint.x),
            @"y" : @(layer.get().anchorPoint.y),
        },
        @"anchorPointZ" : @(layer.get().anchorPointZ),
        @"transform" : @{
            @"m11" : @(layer.get().transform.m11),
            @"m12" : @(layer.get().transform.m12),
            @"m13" : @(layer.get().transform.m13),
            @"m14" : @(layer.get().transform.m14),

            @"m21" : @(layer.get().transform.m21),
            @"m22" : @(layer.get().transform.m22),
            @"m23" : @(layer.get().transform.m23),
            @"m24" : @(layer.get().transform.m24),

            @"m31" : @(layer.get().transform.m31),
            @"m32" : @(layer.get().transform.m32),
            @"m33" : @(layer.get().transform.m33),
            @"m34" : @(layer.get().transform.m34),

            @"m41" : @(layer.get().transform.m41),
            @"m42" : @(layer.get().transform.m42),
            @"m43" : @(layer.get().transform.m43),
            @"m44" : @(layer.get().transform.m44),
        },
        @"sublayerTransform" : @{
            @"m11" : @(layer.get().sublayerTransform.m11),
            @"m12" : @(layer.get().sublayerTransform.m12),
            @"m13" : @(layer.get().sublayerTransform.m13),
            @"m14" : @(layer.get().sublayerTransform.m14),

            @"m21" : @(layer.get().sublayerTransform.m21),
            @"m22" : @(layer.get().sublayerTransform.m22),
            @"m23" : @(layer.get().sublayerTransform.m23),
            @"m24" : @(layer.get().sublayerTransform.m24),

            @"m31" : @(layer.get().sublayerTransform.m31),
            @"m32" : @(layer.get().sublayerTransform.m32),
            @"m33" : @(layer.get().sublayerTransform.m33),
            @"m34" : @(layer.get().sublayerTransform.m34),

            @"m41" : @(layer.get().sublayerTransform.m41),
            @"m42" : @(layer.get().sublayerTransform.m42),
            @"m43" : @(layer.get().sublayerTransform.m43),
            @"m44" : @(layer.get().sublayerTransform.m44),
        },

        @"hidden" : @(layer.get().hidden),
        @"doubleSided" : @(layer.get().doubleSided),
        @"masksToBounds" : @(layer.get().masksToBounds),
        @"contentsScale" : @(layer.get().contentsScale),
        @"rasterizationScale" : @(layer.get().rasterizationScale),
        @"opaque" : @(layer.get().opaque),
        @"opacity" : @(layer.get().opacity),
    };
}

- (void)_textFragmentRangesWithCompletionHandlerForTesting:(void(^)(NSArray<NSValue *> *fragmentRanges))completionHandler
{
    _page->getTextFragmentRanges([completion = makeBlockPtr(completionHandler)](const Vector<WebKit::EditingRange> editingRanges) {
        RetainPtr<NSMutableArray<NSValue *>> resultRanges = [NSMutableArray array];
        for (auto editingRange : editingRanges) {
            NSRange resultRange = editingRange;
            if (resultRange.location != NSNotFound)
                [resultRanges addObject:[NSValue valueWithRange:resultRange]];
        }
        completion(resultRanges.get());
    });
}

- (void)_cancelFixedColorExtensionFadeAnimationsForTesting
{
#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    for (auto side : WebCore::allBoxSides)
        [_fixedColorExtensionViews.at(side) cancelFadeAnimation];
#endif
}

- (unsigned)_forwardedLogsCountForTesting
{
#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    return WebKit::LogStream::logCountForTesting();
#else
    return 0;
#endif
}

- (bool)_receivedLogsDuringLaunchForTesting
{
#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    if (RefPtr mainFrame = _page->mainFrame())
        return mainFrame->process().receivedLogsDuringLaunchForTesting();
#endif
    return 0;
}

- (void)_modelProcessModelPlayerCountForTesting:(void(^)(NSUInteger))completionHandler
{
#if ENABLE(MODEL_PROCESS)
    RefPtr modelProcess = _page->configuration().processPool().modelProcess();
    if (!modelProcess) {
        completionHandler(0);
        return;
    }

    modelProcess->modelPlayerCountForTesting([completionHandler = makeBlockPtr(completionHandler)](uint64_t count) {
        completionHandler(count);
    });
#else
    completionHandler(0);
#endif
}

- (NSString *)_webContentProcessVariantForFrame:(_WKFrameHandle *)frameHandle
{
#if USE(APPLE_INTERNAL_SDK)
    if (!_page)
        return @"standard";

    RefPtr<WebKit::WebProcessProxy> process;

    if (frameHandle) {
        auto frameID = frameHandle->_frameHandle.get() ? frameHandle->_frameHandle->frameID() : std::nullopt;
        if (!frameID)
            return @"standard";

        process = _page->processContainingFrame(frameID);
        if (!process)
            process = _page->legacyMainFrameProcess();
    } else {
        // No frameHandle provided, check main frame process
        process = _page->legacyMainFrameProcess();
    }

    if (!process || !process->hasConnection())
        return @"standard";

    Ref connection = process->connection();

#if !PLATFORM(IOS_FAMILY)
    bool hasAllowJIT = hasEntitlement(connection->xpcConnection(), "com.apple.security.cs.allow-jit"_s);
    bool hasVerifiedJIT = hasEntitlement(connection->xpcConnection(), "com.apple.private.verified-jit"_s);
    bool hasSingleJIT = hasEntitlement(connection->xpcConnection(), "com.apple.security.cs.single-jit"_s);
    bool hasJIT = hasAllowJIT || hasVerifiedJIT || hasSingleJIT;

    bool hasEnhancedSecurityEntitlement = hasEntitlement(connection->xpcConnection(), "com.apple.private.webkit.enhanced-security"_s);

    bool hasEnhancedSecurity = hasEnhancedSecurityEntitlement || !hasJIT;
    bool hasLockdownMode = !hasAllowJIT && !hasEnhancedSecurity;

#else
    bool hasEnhancedSecurity = hasEntitlement(connection->xpcConnection(), "com.apple.private.webkit.enhanced-security"_s);
    bool hasLockdownMode = hasEntitlement(connection->xpcConnection(), "com.apple.private.webkit.lockdown-mode"_s);

#endif

    if (hasLockdownMode)
        return @"lockdown";
    if (hasEnhancedSecurity)
        return @"security";

#endif
    return @"standard";
}

#if ENABLE(THREADED_ANIMATIONS)
- (NSString *)_animationStackForLayerWithID:(unsigned long long)layerID
{
    auto animationStack = [&] -> RefPtr<const WebKit::RemoteAnimationStack> {
        if (!layerID)
            return nullptr;
        WebCore::PlatformLayerIdentifier platformLayerID { ObjectIdentifier<WebCore::PlatformLayerIdentifierType>(layerID), _page->legacyMainFrameProcess().coreProcessIdentifier() };
        if (RefPtr nodeStack = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(_page->protectedDrawingArea())->animationStackForNodeWithIDForTesting(platformLayerID))
            return nodeStack;
        if (CheckedPtr scrollingCoordinator = _page->scrollingCoordinatorProxy())
            return scrollingCoordinator->animationStackForNodeWithIDForTesting(platformLayerID);
        return nullptr;
    }();
    if (animationStack)
        return animationStack->toJSONForTesting()->toJSONString().createNSString().autorelease();
    return @"";
}
#endif

@end

#if ENABLE(MEDIA_SESSION_COORDINATOR)
@implementation WKMediaSessionCoordinatorHelper {
    WeakPtr<WebCore::MediaSessionCoordinatorClient> m_coordinatorClient;
}

- (id)initWithCoordinator:(WebCore::MediaSessionCoordinatorClient*)coordinator
{
    self = [super init];
    if (!self)
        return nil;
    m_coordinatorClient = coordinator;
    return self;
}

- (void)seekSessionToTime:(double)time withCompletion:(void(^)(BOOL))completionHandler
{
    Ref { *m_coordinatorClient }->seekSessionToTime(time, makeBlockPtr(completionHandler));
}

- (void)playSessionWithCompletion:(void(^)(BOOL))completionHandler
{
    Ref { *m_coordinatorClient }->playSession({ }, std::optional<MonotonicTime>(), makeBlockPtr(completionHandler));
}

- (void)pauseSessionWithCompletion:(void(^)(BOOL))completionHandler
{
    Ref { *m_coordinatorClient }->pauseSession(makeBlockPtr(completionHandler));
}

- (void)setSessionTrack:(NSString*)trackIdentifier withCompletion:(void(^)(BOOL))completionHandler
{
    Ref { *m_coordinatorClient }->setSessionTrack(trackIdentifier, makeBlockPtr(completionHandler));
}

- (void)coordinatorStateChanged:(_WKMediaSessionCoordinatorState)state
{
    static_assert(static_cast<size_t>(WebCore::MediaSessionCoordinatorState::Waiting) == static_cast<size_t>(WKMediaSessionCoordinatorStateWaiting), "WKMediaSessionCoordinatorStateWaiting does not match WebKit value");
    static_assert(static_cast<size_t>(WebCore::MediaSessionCoordinatorState::Joined) == static_cast<size_t>(WKMediaSessionCoordinatorStateJoined), "WKMediaSessionCoordinatorStateJoined does not match WebKit value");
    static_assert(static_cast<size_t>(WebCore::MediaSessionCoordinatorState::Closed) == static_cast<size_t>(WKMediaSessionCoordinatorStateClosed), "WKMediaSessionCoordinatorStateClosed does not match WebKit value");

    Ref { *m_coordinatorClient }->coordinatorStateChanged(static_cast<WebCore::MediaSessionCoordinatorState>(state));
}

@end
#endif

@implementation _WKNowPlayingMetadata : NSObject
- (void)dealloc
{
IGNORE_NULL_CHECK_WARNINGS_BEGIN
    self.title = nil;
    self.artist = nil;
    self.album = nil;
    self.sourceApplicationIdentifier = nil;
IGNORE_NULL_CHECK_WARNINGS_END

    [super dealloc];
}
@end
