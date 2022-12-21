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
#import "PlaybackSessionManagerProxy.h"
#import "PrintInfo.h"
#import "UserMediaProcessManager.h"
#import "ViewGestureController.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "WebViewImpl.h"
#import "_WKFrameHandleInternal.h"
#import "_WKInspectorInternal.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/ValidationBubble.h>
#import <wtf/RetainPtr.h>

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

    _page->setPrivateClickMeasurement({{ WTFMove(measurement), { }, { }}});
}

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
    _page->process().sendPrepareToSuspend(WebKit::IsSuspensionImminent::No, 0.0, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_processWillSuspendImminentlyForTesting
{
    if (_page)
        _page->process().sendPrepareToSuspend(WebKit::IsSuspensionImminent::Yes, 0.0, [] { });
}

- (void)_processDidResumeForTesting
{
    if (_page)
        _page->process().sendProcessDidResume(WebKit::ProcessThrottlerClient::ResumeReason::ForegroundActivity);
}

- (void)_setThrottleStateForTesting:(int)value
{
    if (!_page)
        return;

    _page->process().setThrottleStateForTesting(static_cast<WebKit::ProcessThrottleState>(value));
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

- (void)_setIndexOfGetDisplayMediaDeviceSelectedForTesting:(NSNumber *)nsIndex
{
#if HAVE(SCREEN_CAPTURE_KIT)
    if (!_page)
        return;

    std::optional<unsigned> index;
    if (nsIndex)
        index = nsIndex.unsignedIntValue;

    _page->setIndexOfGetDisplayMediaDeviceSelectedForTesting(index);
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

- (void)_startMonitoringWheelEvents
{
    _page->startMonitoringWheelEventsForTesting();
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

- (void)_setPrivateClickMeasurementAttributionReportURLsForTesting:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL completionHandler:(void(^)(void))completionHandler
{
    _page->setPrivateClickMeasurementAttributionReportURLsForTesting(sourceURL, destinationURL, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAttributionTokenPublicKeyURLForTesting:(NSURL *)url completionHandler:(void(^)(void))completionHandler
{
    _page->setPrivateClickMeasurementTokenPublicKeyURLForTesting(url, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAttributionTokenSignatureURLForTesting:(NSURL *)url completionHandler:(void(^)(void))completionHandler
{
    _page->setPrivateClickMeasurementTokenSignatureURLForTesting(url, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateClickMeasurementAppBundleIDForTesting:(NSString *)appBundleID completionHandler:(void(^)(void))completionHandler
{
    _page->setPrivateClickMeasurementAppBundleIDForTesting(appBundleID, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_dumpPrivateClickMeasurement:(void(^)(NSString *))completionHandler
{
    _page->dumpPrivateClickMeasurement([completionHandler = makeBlockPtr(completionHandler)](const String& privateClickMeasurement) {
        completionHandler(privateClickMeasurement);
    });
}

- (void)_didShowContextMenu
{
    // For subclasses to override.
}

- (void)_didDismissContextMenu
{
    // For subclasses to override.
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
    _page->isLayerTreeFrozen([completionHandler = makeBlockPtr(completionHandler)](bool isFrozen) {
        completionHandler(isFrozen);
    });
}

- (void)_computePagesForPrinting:(_WKFrameHandle *)handle completionHandler:(void(^)(void))completionHandler
{
    WebKit::PrintInfo printInfo;
    _page->computePagesForPrinting(handle->_frameHandle->frameID(), printInfo, [completionHandler = makeBlockPtr(completionHandler)] (const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&) {
        completionHandler();
    });
}

- (void)_gpuToWebProcessConnectionCountForTesting:(void(^)(NSUInteger))completionHandler
{
    RefPtr gpuProcess = _page->process().processPool().gpuProcess();
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

+ (void)_setLookalikeCharacterStringsForTesting:(NSArray<NSString *> *)strings
{
    // FIXME: Remove this method and simply swizzle out the appropriate platform API, once we're able to
    // call into the API from within the application process.
#if ENABLE(NETWORK_CONNECTION_INTEGRITY)
    WebKit::WebPageProxy::cachedLookalikeStrings() = makeVector<String>(strings);
#else
    UNUSED_PARAM(strings);
#endif
}

- (void)_createMediaSessionCoordinatorForTesting:(id <_WKMediaSessionCoordinator>)privateCoordinator completionHandler:(void(^)(BOOL))completionHandler
{
#if ENABLE(MEDIA_SESSION_COORDINATOR)
    class WKMediaSessionCoordinatorForTesting
        : public WebKit::MediaSessionCoordinatorProxyPrivate
        , public WebCore::MediaSessionCoordinatorClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:

        static Ref<WKMediaSessionCoordinatorForTesting> create(id <_WKMediaSessionCoordinator> privateCoordinator)
        {
            return adoptRef(*new WKMediaSessionCoordinatorForTesting(privateCoordinator));
        }

        using WebCore::MediaSessionCoordinatorClient::weakPtrFactory;
        using WebCore::MediaSessionCoordinatorClient::WeakValueType;
        using WebCore::MediaSessionCoordinatorClient::WeakPtrImplType;

    private:
        explicit WKMediaSessionCoordinatorForTesting(id <_WKMediaSessionCoordinator> clientCoordinator)
            : WebKit::MediaSessionCoordinatorProxyPrivate()
            , m_clientCoordinator(clientCoordinator)
        {
            auto* delegateHelper = [[WKMediaSessionCoordinatorHelper alloc] initWithCoordinator:this];
            [m_clientCoordinator setDelegate:delegateHelper];
            m_coordinatorDelegate = adoptNS(delegateHelper);
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
                return { WebCore::ExceptionData { WebCore::InvalidStateError, String() } };

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
                    callback(WebCore::ExceptionData { WebCore::InvalidStateError, String() });
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
                    callback(WebCore::ExceptionData { WebCore::InvalidStateError, String() });
                    return;
                }

                callback(weakThis->result(success));
            }).get()];
        }

        void play(WebKit::MediaSessionCommandCompletionHandler&& callback) final
        {
            [m_clientCoordinator playWithCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (BOOL success) mutable {
                if (!weakThis) {
                    callback(WebCore::ExceptionData { WebCore::InvalidStateError, String() });
                    return;
                }

                callback(weakThis->result(success));
            }).get()];
        }

        void pause(WebKit::MediaSessionCommandCompletionHandler&& callback) final
        {
            [m_clientCoordinator pauseWithCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (BOOL success) mutable {
                if (!weakThis) {
                    callback(WebCore::ExceptionData { WebCore::InvalidStateError, String() });
                    return;
                }

                callback(weakThis->result(success));
            }).get()];
        }

        void setTrack(const String& track, WebKit::MediaSessionCommandCompletionHandler&& callback) final
        {
            [m_clientCoordinator setTrack:track withCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (BOOL success) mutable {
                if (!weakThis) {
                    callback(WebCore::ExceptionData { WebCore::InvalidStateError, String() });
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
