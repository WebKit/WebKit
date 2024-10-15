/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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
#import "GPUProcessProxy.h"
#import "MediaSessionCoordinatorProxyPrivate.h"
#import "NetworkProcessProxy.h"
#import "PlaybackSessionManagerProxy.h"
#import "PrintInfo.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "UserMediaProcessManager.h"
#import "ViewGestureController.h"
#import "ViewSnapshotStore.h"
#import "WKContentViewInteraction.h"
#import "WKPreferencesInternal.h"
#import "WebPageProxy.h"
#import "WebPageProxyTesting.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "WebViewImpl.h"
#import "WebsiteDataStore.h"
#import "_WKFrameHandleInternal.h"
#import "_WKInspectorInternal.h"
#import <WebCore/NowPlayingInfo.h>
#import <WebCore/ScrollingNodeID.h>
#import <WebCore/ValidationBubble.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/TZoneMallocInlines.h>
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

- (NSString *)_caLayerTreeAsText
{
    TextStream ts(TextStream::LineMode::MultipleLine);

    {
        TextStream::GroupScope scope(ts);
        ts << "CALayer tree root ";
#if PLATFORM(IOS_FAMILY)
        dumpCALayer(ts, [_contentView layer], true);
#else
        dumpCALayer(ts, self.layer, true);
#endif
    }

    return ts.release();
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

        ts.dumpProperty("type", interactionRegionLayerType);

        if (layer.mask) {
            TextStream::GroupScope scope(ts);
            ts << "mask";
            ts.dumpProperty("frame", rectToString(layer.mask.frame));
        }
    }
#endif

    ts.dumpProperty("layer bounds", rectToString(layer.bounds));

    if (layer.position.x || layer.position.y)
        ts.dumpProperty("layer position", pointToString(layer.position));

    if (layer.zPosition)
        ts.dumpProperty("layer zPosition", makeString(layer.zPosition));

    if (layer.anchorPoint.x != 0.5 || layer.anchorPoint.y != 0.5)
        ts.dumpProperty("layer anchorPoint", pointToString(layer.anchorPoint));

    if (layer.anchorPointZ)
        ts.dumpProperty("layer anchorPointZ", makeString(layer.anchorPointZ));

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (layer.separated)
        ts.dumpProperty("separated", true);
#endif

    if (layer.opacity != 1.0)
        ts.dumpProperty("layer opacity", makeString(layer.opacity));

    if (layer.cornerRadius != 0.0)
        ts.dumpProperty("layer cornerRadius", makeString(layer.cornerRadius));

    constexpr CACornerMask allCorners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner | kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
    if (layer.maskedCorners != allCorners)
        ts.dumpProperty("layer masked corners", makeString(layer.maskedCorners));
    
    if (traverse && layer.sublayers.count > 0) {
        TextStream::GroupScope scope(ts);
        ts << "sublayers";
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
        auto* validationBubble = _page->validationBubble();
        String message = validationBubble ? validationBubble->message() : emptyString();
        double fontSize = validationBubble ? validationBubble->fontSize() : 0;
        return @{ userInterfaceItem: @{ @"message": (NSString *)message, @"fontSize": @(fontSize) } };
    }

    if (NSDictionary *contents = _page->contentsOfUserInterfaceItem(userInterfaceItem))
        return contents;

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

    _page->requestActiveNowPlayingSessionInfo([handler = makeBlockPtr(callback)] (bool registeredAsNowPlayingApplication, WebCore::NowPlayingInfo&& nowPlayingInfo) {
        handler(nowPlayingInfo.allowsNowPlayingControlsVisibility, registeredAsNowPlayingApplication, nowPlayingInfo.metadata.title, nowPlayingInfo.duration, nowPlayingInfo.currentTime, nowPlayingInfo.uniqueIdentifier ? nowPlayingInfo.uniqueIdentifier->toUInt64() : 0);
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

    auto nowPlayingMetadataObserver = makeUnique<WebCore::NowPlayingMetadataObserver>([observer = makeBlockPtr(observer)](auto& metadata) {
        RetainPtr nowPlayingMetadata = adoptNS([[_WKNowPlayingMetadata alloc] init]);
        [nowPlayingMetadata setTitle:metadata.title];
        [nowPlayingMetadata setArtist:metadata.artist];
        [nowPlayingMetadata setAlbum:metadata.album];
        [nowPlayingMetadata setSourceApplicationIdentifier:metadata.sourceApplicationIdentifier];
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
    WebKit::RemoteScrollingCoordinatorProxy* coordinator = _page->scrollingCoordinatorProxy();
    if (!coordinator)
        return @"";

    return coordinator->scrollingTreeAsText();
}

- (pid_t)_networkProcessIdentifier
{
    auto* networkProcess = _page->websiteDataStore().networkProcessIfExists();
    RELEASE_ASSERT(networkProcess);
    return networkProcess->processID();
}

- (void)_setScrollingUpdatesDisabledForTesting:(BOOL)disabled
{
}

- (unsigned long)_countOfUpdatesWithLayerChanges
{
    if (auto* drawingAreaProxy = dynamicDowncast<WebKit::RemoteLayerTreeDrawingAreaProxy>(_page->drawingArea()))
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
    if (auto* pageForTesting = _page->pageForTesting())
        pageForTesting->setDefersLoading(defersLoading);
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
    _page->legacyMainFrameProcess().sendPrepareToSuspend(WebKit::IsSuspensionImminent::No, 0.0, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_processWillSuspendImminentlyForTesting
{
    if (_page)
        _page->legacyMainFrameProcess().sendPrepareToSuspend(WebKit::IsSuspensionImminent::Yes, 0.0, [] { });
}

- (void)_processDidResumeForTesting
{
    if (_page)
        _page->legacyMainFrameProcess().sendProcessDidResume(WebKit::AuxiliaryProcessProxy::ResumeReason::ForegroundActivity);
}

- (void)_setThrottleStateForTesting:(int)value
{
    if (!_page)
        return;

    _page->legacyMainFrameProcess().setThrottleStateForTesting(static_cast<WebKit::ProcessThrottleState>(value));
}

- (BOOL)_hasServiceWorkerBackgroundActivityForTesting
{
    return _page ? _page->configuration().processPool().hasServiceWorkerBackgroundActivityForTesting() : false;
}

- (BOOL)_hasServiceWorkerForegroundActivityForTesting
{
    return _page ? _page->configuration().processPool().hasServiceWorkerForegroundActivityForTesting() : false;
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

    if (auto* pageForTesting = _page->pageForTesting())
        pageForTesting->setIndexOfGetDisplayMediaDeviceSelectedForTesting(index);
#endif
}

- (void)_setSystemCanPromptForGetDisplayMediaForTesting:(BOOL)canPrompt
{
#if ENABLE(MEDIA_STREAM)
    if (!_page)
        return;

    if (auto* pageForTesting = _page->pageForTesting())
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
    return _page->scrollbarStateForScrollingNodeID(WebCore::ScrollingNodeID(ObjectIdentifier<WebCore::ScrollingNodeIDType>(scrollingNodeID), ObjectIdentifier<WebCore::ProcessIdentifierType>(processID)), isVertical);
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
    auto* pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementOverrideTimer(overrideTimer, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAttributionReportURLsForTesting:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL completionHandler:(void(^)(void))completionHandler
{
    auto* pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementAttributionReportURLs(sourceURL, destinationURL, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAttributionTokenPublicKeyURLForTesting:(NSURL *)url completionHandler:(void(^)(void))completionHandler
{
    auto* pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementTokenPublicKeyURL(url, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAttributionTokenSignatureURLForTesting:(NSURL *)url completionHandler:(void(^)(void))completionHandler
{
    auto* pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementTokenSignatureURL(url, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAppBundleIDForTesting:(NSString *)appBundleID completionHandler:(void(^)(void))completionHandler
{
    auto* pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler();

    pageForTesting->setPrivateClickMeasurementAppBundleID(appBundleID, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_dumpPrivateClickMeasurement:(void(^)(NSString *))completionHandler
{
    auto* pageForTesting = _page->pageForTesting();
    if (!pageForTesting)
        return completionHandler({ });

    pageForTesting->dumpPrivateClickMeasurement([completionHandler = makeBlockPtr(completionHandler)](const String& privateClickMeasurement) {
        completionHandler(privateClickMeasurement);
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
    auto* pageForTesting = _page->pageForTesting();
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
            if (auto coordinatorClient = client())
                coordinatorClient->seekSessionToTime(time, WTFMove(callback));
            else
                callback(false);
        }

        void playSession(std::optional<double> atTime, std::optional<MonotonicTime> hostTime, CompletionHandler<void(bool)>&& callback) final
        {
            if (auto coordinatorClient = client())
                coordinatorClient->playSession(WTFMove(atTime), WTFMove(hostTime), WTFMove(callback));
            else
                callback(false);
        }

        void pauseSession(CompletionHandler<void(bool)>&& callback) final
        {
            if (auto coordinatorClient = client())
                coordinatorClient->pauseSession(WTFMove(callback));
            else
                callback(false);
        }

        void setSessionTrack(const String& trackIdentifier, CompletionHandler<void(bool)>&& callback) final
        {
            if (auto coordinatorClient = client())
                coordinatorClient->setSessionTrack(trackIdentifier, WTFMove(callback));
            else
                callback(false);
        }

        void coordinatorStateChanged(WebCore::MediaSessionCoordinatorState state) final
        {
            if (auto coordinatorClient = client())
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
            [m_clientCoordinator setTrack:track withCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (BOOL success) mutable {
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
            [m_clientCoordinator trackIdentifierChanged:identifier];
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
    m_coordinatorClient->seekSessionToTime(time, makeBlockPtr(completionHandler));
}

- (void)playSessionWithCompletion:(void(^)(BOOL))completionHandler
{
    m_coordinatorClient->playSession({ }, std::optional<MonotonicTime>(), makeBlockPtr(completionHandler));
}

- (void)pauseSessionWithCompletion:(void(^)(BOOL))completionHandler
{
    m_coordinatorClient->pauseSession(makeBlockPtr(completionHandler));
}

- (void)setSessionTrack:(NSString*)trackIdentifier withCompletion:(void(^)(BOOL))completionHandler
{
    m_coordinatorClient->setSessionTrack(trackIdentifier, makeBlockPtr(completionHandler));
}

- (void)coordinatorStateChanged:(_WKMediaSessionCoordinatorState)state
{
    static_assert(static_cast<size_t>(WebCore::MediaSessionCoordinatorState::Waiting) == static_cast<size_t>(WKMediaSessionCoordinatorStateWaiting), "WKMediaSessionCoordinatorStateWaiting does not match WebKit value");
    static_assert(static_cast<size_t>(WebCore::MediaSessionCoordinatorState::Joined) == static_cast<size_t>(WKMediaSessionCoordinatorStateJoined), "WKMediaSessionCoordinatorStateJoined does not match WebKit value");
    static_assert(static_cast<size_t>(WebCore::MediaSessionCoordinatorState::Closed) == static_cast<size_t>(WKMediaSessionCoordinatorStateClosed), "WKMediaSessionCoordinatorStateClosed does not match WebKit value");

    m_coordinatorClient->coordinatorStateChanged(static_cast<WebCore::MediaSessionCoordinatorState>(state));
}

@end
#endif

@implementation _WKNowPlayingMetadata : NSObject
@end
