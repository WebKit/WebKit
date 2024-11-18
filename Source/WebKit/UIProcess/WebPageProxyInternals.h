/**
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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

#include "ContextMenuContextData.h"
#include "EditorState.h"
#include "GeolocationPermissionRequestManagerProxy.h"
#include "HiddenPageThrottlingAutoIncreasesCounter.h"
#include "LayerTreeContext.h"
#include "PageLoadState.h"
#include "ProcessThrottler.h"
#include "ScrollingAccelerationCurve.h"
#include "VisibleWebPageCounter.h"
#include "WebColorPicker.h"
#include "WebFrameProxy.h"
#include "WebNotificationManagerMessageHandler.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessageReceiverRegistration.h"
#include "WebPopupMenuProxy.h"
#include "WebURLSchemeHandlerIdentifier.h"
#include "WindowKind.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceRequest.h>
#include <pal/HysteresisActivity.h>
#include <wtf/UUID.h>

#if ENABLE(APPLE_PAY)
#include "WebPaymentCoordinatorProxy.h"
#endif

#if ENABLE(DATALIST_ELEMENT)
#include "WebDataListSuggestionsDropdown.h"
#endif

#if ENABLE(DRAG_SUPPORT)
#include <WebCore/DragActions.h>
#endif

#if PLATFORM(MACCATALYST)
#include "EndowmentStateTracker.h"
#endif

#if ENABLE(META_VIEWPORT)
#include <WebCore/ViewportArguments.h>
#endif

#if ENABLE(SPEECH_SYNTHESIS)
#include <WebCore/PlatformSpeechSynthesisUtterance.h>
#include <WebCore/PlatformSpeechSynthesizer.h>
#endif

#if ENABLE(TOUCH_EVENTS)
#include "NativeWebTouchEvent.h"
#include <WebCore/EventTrackingRegions.h>
#endif

#if ENABLE(UI_SIDE_COMPOSITING)
#include "VisibleContentRectUpdateInfo.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
#include <WebCore/WebMediaSessionManagerClient.h>
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
#include "MediaCapability.h"
#endif

#if PLATFORM(COCOA)
#include "CocoaWindow.h"
#endif

namespace WebKit {

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)
class WebPageProxyFrameLoadStateObserver final : public FrameLoadStateObserver {
    WTF_MAKE_NONCOPYABLE(WebPageProxyFrameLoadStateObserver);
    WTF_MAKE_TZONE_ALLOCATED(WebPageProxyFrameLoadStateObserver);
public:
    static constexpr size_t maxVisitedDomainsSize = 6;

    explicit WebPageProxyFrameLoadStateObserver(const WebPageProxy&);
    virtual ~WebPageProxyFrameLoadStateObserver();

    void ref() const final;
    void deref() const final;

    void didReceiveProvisionalURL(const URL& url) override
    {
        m_provisionalURLs.append(url);
    }

    void didCancelProvisionalLoad() override
    {
        m_provisionalURLs.clear();
    }

    void didCommitProvisionalLoad() override
    {
        for (auto& url : m_provisionalURLs)
            didVisitDomain(WebCore::RegistrableDomain(url));
    }

    const ListHashSet<WebCore::RegistrableDomain>& visitedDomains() const
    {
        return m_visitedDomains;
    }

private:
    void didVisitDomain(WebCore::RegistrableDomain&& domain)
    {
        if (domain.isEmpty())
            return;

        m_visitedDomains.prependOrMoveToFirst(WTFMove(domain));

        if (m_visitedDomains.size() > maxVisitedDomainsSize)
            m_visitedDomains.removeLast();
    }

    WeakRef<WebPageProxy> m_page;
    Vector<URL> m_provisionalURLs;
    ListHashSet<WebCore::RegistrableDomain> m_visitedDomains;
};
#endif

class PageLoadTimingFrameLoadStateObserver final : public FrameLoadStateObserver {
public:
    explicit PageLoadTimingFrameLoadStateObserver(const WebPageProxy&page)
        : m_page(page)
    {
    }

    void ref() const final;
    void deref() const final;

    bool hasLoadingFrame() const { return !!m_loadingFrameCount; }

private:
    void didCommitProvisionalLoad(IsMainFrame isMainFrame)
    {
        if (isMainFrame == IsMainFrame::Yes) {
            // Teardown doesn't reliably inform the UI process of each iframe's provisional load failure.
            m_loadingFrameCount = 1;
        }
    }

    void didStartProvisionalLoad(const URL&) final
    {
        m_loadingFrameCount++;
    }

    void didFailProvisionalLoad(const URL&) final
    {
        ASSERT(m_loadingFrameCount);
        m_loadingFrameCount--;
    }

    void didFailLoad(const URL&) final
    {
        ASSERT(m_loadingFrameCount);
        m_loadingFrameCount--;
    }

    void didFinishLoad(IsMainFrame, const URL& url) final
    {
        ASSERT(m_loadingFrameCount);
        m_loadingFrameCount--;
        // FIXME: Assert that m_loadingFrameCount is zero if this is a main frame.
    }

    WeakRef<WebPageProxy> m_page;
    size_t m_loadingFrameCount { 0 };
};

struct PrivateClickMeasurementAndMetadata {
    WebCore::PrivateClickMeasurement pcm;
    String sourceDescription;
    String purchaser;
};

struct SpeechSynthesisData {
    Ref<WebCore::PlatformSpeechSynthesizer> synthesizer;
    RefPtr<WebCore::PlatformSpeechSynthesisUtterance> utterance;
    CompletionHandler<void()> speakingStartedCompletionHandler;
    CompletionHandler<void()> speakingFinishedCompletionHandler;
    CompletionHandler<void()> speakingPausedCompletionHandler;
    CompletionHandler<void()> speakingResumedCompletionHandler;
};

#if ENABLE(TOUCH_EVENTS)

struct QueuedTouchEvents {
    QueuedTouchEvents(const NativeWebTouchEvent& event)
        : forwardedEvent(event)
    {
    }
    NativeWebTouchEvent forwardedEvent;
    Vector<NativeWebTouchEvent> deferredTouchEvents;
};

struct TouchEventTracking {
    WebCore::TrackingType touchForceChangedTracking { WebCore::TrackingType::NotTracking };
    WebCore::TrackingType touchStartTracking { WebCore::TrackingType::NotTracking };
    WebCore::TrackingType touchMoveTracking { WebCore::TrackingType::NotTracking };
    WebCore::TrackingType touchEndTracking { WebCore::TrackingType::NotTracking };

    bool isTrackingAnything() const
    {
        return touchForceChangedTracking != WebCore::TrackingType::NotTracking
            || touchStartTracking != WebCore::TrackingType::NotTracking
            || touchMoveTracking != WebCore::TrackingType::NotTracking
            || touchEndTracking != WebCore::TrackingType::NotTracking;
    }

    void reset()
    {
        touchForceChangedTracking = WebCore::TrackingType::NotTracking;
        touchStartTracking = WebCore::TrackingType::NotTracking;
        touchMoveTracking = WebCore::TrackingType::NotTracking;
        touchEndTracking = WebCore::TrackingType::NotTracking;
    }
};

#endif

struct WebPageProxy::Internals final : WebPopupMenuProxy::Client
#if ENABLE(APPLE_PAY)
    , WebPaymentCoordinatorProxy::Client
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    , WebColorPickerClient
#endif
#if PLATFORM(MACCATALYST)
    , EndowmentStateTrackerClient
#endif
#if ENABLE(SPEECH_SYNTHESIS)
    , WebCore::PlatformSpeechSynthesisUtteranceClient
    , WebCore::PlatformSpeechSynthesizerClient
#endif
#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    , WebCore::WebMediaSessionManagerClient
#endif
{
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(Internals);

public:
    virtual ~Internals();

    uint32_t checkedPtrCount() const { return WebPopupMenuProxy::Client::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const { return WebPopupMenuProxy::Client::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const { WebPopupMenuProxy::Client::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const { WebPopupMenuProxy::Client::decrementCheckedPtrCount(); }

    WeakRef<WebPageProxy> page;
    OptionSet<WebCore::ActivityState> activityState;
    RunLoop::Timer audibleActivityTimer;
    std::optional<WebCore::Color> backgroundColor;
    WebCore::LayoutSize baseLayoutViewportSize;
    std::optional<WebCore::FontAttributes> cachedFontAttributesAtSelectionStart;
    Vector<Function<void()>> callbackHandlersAfterProcessingPendingMouseEvents;
    WebCore::FloatSize defaultUnobscuredSize;
    EditorState editorState;
    WebCore::IntSize fixedLayoutSize;
    GeolocationPermissionRequestManagerProxy geolocationPermissionRequestManager;
    HiddenPageThrottlingAutoIncreasesCounter::Token hiddenPageDOMTimerThrottlingAutoIncreasesCount;
    Deque<NativeWebKeyboardEvent> keyEventQueue;
    LayerHostingMode layerHostingMode { LayerHostingMode::InProcess };
    WebCore::RectEdges<bool> mainFramePinnedState { true, true, true, true };
    WebCore::LayoutPoint maxStableLayoutViewportOrigin;
    WebCore::FloatSize maximumUnobscuredSize;
    WebCore::MediaProducerMediaStateFlags mediaState;
    WebCore::LayoutPoint minStableLayoutViewportOrigin;
    WebCore::IntSize minimumSizeForAutoLayout;
    WebCore::FloatSize minimumUnobscuredSize;
    Deque<NativeWebMouseEvent> mouseEventQueue;
    Vector<WebMouseEvent> coalescedMouseEvents;
    WebCore::MediaProducerMutedStateFlags mutedState;
    WebNotificationManagerMessageHandler notificationManagerMessageHandler;
    OptionSet<WebCore::LayoutMilestone> observedLayoutMilestones;
    WebCore::Color pageExtendedBackgroundColor;
    UserObservablePageCounter::Token pageIsUserObservableCount;
    std::optional<MonotonicTime> pageLoadStart;
    PageLoadState pageLoadState;
    OptionSet<WebCore::ActivityState> potentiallyChangedActivityStateFlags;
    ProcessSuppressionDisabledToken preventProcessSuppressionCount;
    std::optional<PrivateClickMeasurementAndMetadata> privateClickMeasurement;
    WebCore::MediaProducerMediaStateFlags reportedMediaCaptureState;
    RunLoop::Timer resetRecentCrashCountTimer;
    WebCore::RectEdges<bool> rubberBandableEdges { true, true, true, true };
    WebCore::Color sampledPageTopColor;
    WebCore::ScrollPinningBehavior scrollPinningBehavior { WebCore::ScrollPinningBehavior::DoNotPin };
    WebCore::IntSize sizeToContentAutoSizeMaximumSize;
    WebCore::Color themeColor;
    RunLoop::Timer tryCloseTimeoutTimer;
    WebCore::Color underPageBackgroundColorOverride;
    WebCore::Color underlayColor;
    RunLoop::Timer updateReportedMediaCaptureStateTimer;
    HashMap<WebURLSchemeHandlerIdentifier, Ref<WebURLSchemeHandler>> urlSchemeHandlersByIdentifier;
    std::optional<WebCore::FloatRect> viewExposedRect;
    std::optional<WebCore::FloatSize> viewportSizeForCSSViewportUnits;
    VisibleWebPageToken visiblePageToken;
    WebCore::IntRect visibleScrollerThumbRect;
    WindowKind windowKind { WindowKind::Unparented };
    RefPtr<ProcessThrottlerActivity> pageAllowedToRunInTheBackgroundActivityDueToTitleChanges;
    RefPtr<ProcessThrottlerActivity> pageAllowedToRunInTheBackgroundActivityDueToNotifications;

    WebPageProxyMessageReceiverRegistration messageReceiverRegistration;

    WeakHashSet<WebPageProxy> m_openedPages;
    HashMap<WebCore::SleepDisablerIdentifier, std::unique_ptr<WebCore::SleepDisabler>> sleepDisablers;

#if ENABLE(APPLE_PAY)
    RefPtr<WebPaymentCoordinatorProxy> paymentCoordinator;
#endif

#if PLATFORM(COCOA)
    WeakObjCPtr<WKWebView> cocoaView;
    TransactionID firstLayerTreeTransactionIdAfterDidCommitLoad;
#endif

#if ENABLE(CONTEXT_MENUS)
    ContextMenuContextData activeContextMenuContextData;
#endif

#if HAVE(DISPLAY_LINK)
    PAL::HysteresisActivity wheelEventActivityHysteresis;
#endif

#if ENABLE(DATALIST_ELEMENT)
    RefPtr<WebDataListSuggestionsDropdown> dataListSuggestionsDropdown;
#endif

#if ENABLE(DRAG_SUPPORT)
    WebCore::IntRect currentDragCaretEditableElementRect;
    WebCore::IntRect currentDragCaretRect;
    WebCore::DragHandlingMethod currentDragHandlingMethod { WebCore::DragHandlingMethod::None };
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    RefPtr<WebColorPicker> colorPicker;
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
    Deque<NativeWebGestureEvent> gestureEventQueue;
#endif

#if ENABLE(META_VIEWPORT)
    std::optional<WebCore::ViewportArguments> overrideViewportArguments;
    WebCore::FloatSize viewportConfigurationViewLayoutSize;
#endif

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    std::optional<ScrollingAccelerationCurve> lastSentScrollingAccelerationCurve;
    std::optional<ScrollingAccelerationCurve> scrollingAccelerationCurve;
#endif

#if ENABLE(NOTIFICATIONS)
    HashSet<WebCore::SecurityOriginData> notificationPermissionRequesters;
#endif

    CompletionHandler<void(bool)> serviceWorkerLaunchCompletionHandler;

#if ENABLE(SPEECH_SYNTHESIS)
    std::optional<SpeechSynthesisData> optionalSpeechSynthesisData;
#endif

#if ENABLE(TOUCH_EVENTS)
    TouchEventTracking touchEventTracking;
#endif

#if ENABLE(TOUCH_EVENTS) && !ENABLE(IOS_TOUCH_EVENTS)
    Deque<QueuedTouchEvents> touchEventQueue;
#endif

#if ENABLE(WRITING_TOOLS)
    HashMap<WTF::UUID, WebCore::TextIndicatorData> textIndicatorDataForAnimationID;
    HashMap<WTF::UUID, CompletionHandler<void(WebCore::TextAnimationRunMode)>> completionHandlerForAnimationID;
    HashMap<WTF::UUID, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>)>> completionHandlerForDestinationTextIndicatorForSourceID;
    HashMap<WTF::UUID, WTF::UUID> sourceAnimationIDtoDestinationAnimationID;
#endif

    MonotonicTime didFinishDocumentLoadForMainFrameTimestamp;
    MonotonicTime lastActivationTimestamp;
    MonotonicTime didCommitLoadForMainFrameTimestamp;

#if ENABLE(UI_SIDE_COMPOSITING)
    VisibleContentRectUpdateInfo lastVisibleContentRectUpdate;
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    RunLoop::Timer fullscreenVideoTextRecognitionTimer;
    std::optional<PlaybackSessionContextIdentifier> currentFullscreenVideoSessionIdentifier;
#endif

#if ENABLE(WEBXR) && !USE(OPENXR)
    RefPtr<PlatformXRSystem> xrSystem;
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    RefPtr<MediaCapability> mediaCapability;
#endif

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)
    std::unique_ptr<WebPageProxyFrameLoadStateObserver> frameLoadStateObserver;
    HashMap<WebCore::RegistrableDomain, OptionSet<WebCore::WindowProxyProperty>> windowOpenerAccessedProperties;
#endif
    PageLoadTimingFrameLoadStateObserver pageLoadTimingFrameLoadStateObserver;

#if PLATFORM(GTK) || PLATFORM(WPE)
    RunLoop::Timer activityStateChangeTimer;
#endif

#if PLATFORM(MAC)
    WebCore::FloatPoint scrollPositionDuringLastEditorStateUpdate;
#endif

    bool allowsLayoutViewportHeightExpansion { true };

    explicit Internals(WebPageProxy&);

    Ref<WebPageProxy> protectedPage() const;

    SpeechSynthesisData& speechSynthesisData();

    // WebPopupMenuProxy::Client
    void valueChangedForPopupMenu(WebPopupMenuProxy*, int32_t newSelectedIndex) final;
    void setTextFromItemForPopupMenu(WebPopupMenuProxy*, int32_t index) final;
    NativeWebMouseEvent* currentlyProcessedMouseDownEvent() final;
#if PLATFORM(GTK)
    void failedToShowPopupMenu() final;
#endif

#if ENABLE(APPLE_PAY)
    // WebPaymentCoordinatorProxy::Client
    IPC::Connection* paymentCoordinatorConnection(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorBoundInterfaceIdentifier(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorSourceApplicationBundleIdentifier(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorSourceApplicationSecondaryIdentifier(const WebPaymentCoordinatorProxy&) final;
    void paymentCoordinatorAddMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName, IPC::MessageReceiver&) final;
    void paymentCoordinatorRemoveMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName) final;
    void getPaymentCoordinatorEmbeddingUserAgent(WebPageProxyIdentifier, CompletionHandler<void(const String&)>&&) final;
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebPaymentMessages() const final;
#endif
#if ENABLE(APPLE_PAY) && PLATFORM(IOS_FAMILY)
    UIViewController *paymentCoordinatorPresentingViewController(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorCTDataConnectionServiceType(const WebPaymentCoordinatorProxy&) final;
    Ref<PaymentAuthorizationPresenter> paymentCoordinatorAuthorizationPresenter(WebPaymentCoordinatorProxy&, PKPaymentRequest *) final;
#endif
#if ENABLE(APPLE_PAY) && PLATFORM(IOS_FAMILY) && ENABLE(APPLE_PAY_REMOTE_UI_USES_SCENE)
    void getWindowSceneAndBundleIdentifierForPaymentPresentation(WebPageProxyIdentifier, CompletionHandler<void(const String&, const String&)>&&) final;
#endif
#if ENABLE(APPLE_PAY)
    CocoaWindow *paymentCoordinatorPresentingWindow(const WebPaymentCoordinatorProxy&) const final;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    // WebColorPickerClient
    void didChooseColor(const WebCore::Color&) final;
    void didEndColorPicker() final;
#endif

#if PLATFORM(MACCATALYST)
    // EndowmentStateTrackerClient
    void isUserFacingChanged(bool) final;
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    // PlatformSpeechSynthesisUtteranceClient
    void didStartSpeaking(WebCore::PlatformSpeechSynthesisUtterance&) final;
    void didFinishSpeaking(WebCore::PlatformSpeechSynthesisUtterance&) final;
    void didPauseSpeaking(WebCore::PlatformSpeechSynthesisUtterance&) final;
    void didResumeSpeaking(WebCore::PlatformSpeechSynthesisUtterance&) final;
    void speakingErrorOccurred(WebCore::PlatformSpeechSynthesisUtterance&) final;
    void boundaryEventOccurred(WebCore::PlatformSpeechSynthesisUtterance&, WebCore::SpeechBoundary, unsigned characterIndex, unsigned characterLength) final;

    // PlatformSpeechSynthesizerClient
    void voicesDidChange() final;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    // WebMediaSessionManagerClient
    void setPlaybackTarget(WebCore::PlaybackTargetClientContextIdentifier, Ref<WebCore::MediaPlaybackTarget>&&) final;
    void externalOutputDeviceAvailableDidChange(WebCore::PlaybackTargetClientContextIdentifier, bool) final;
    void setShouldPlayToPlaybackTarget(WebCore::PlaybackTargetClientContextIdentifier, bool) final;
    void playbackTargetPickerWasDismissed(WebCore::PlaybackTargetClientContextIdentifier) final;
    bool alwaysOnLoggingAllowed() const final { return protectedPage()->isAlwaysOnLoggingAllowed(); }
    RetainPtr<PlatformView> platformView() const final;
#endif
};

} // namespace WebKit
