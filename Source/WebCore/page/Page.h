/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "ActivityState.h"
#include "AnimationFrameRate.h"
#include "BadgeClient.h"
#include "Color.h"
#include "ContentSecurityPolicy.h"
#include "DisabledAdaptations.h"
#include "Document.h"
#include "EventTrackingRegions.h"
#include "FilterRenderingMode.h"
#include "FindOptions.h"
#include "FrameLoaderTypes.h"
#include "IntRectHash.h"
#include "KeyboardScrollingAnimator.h"
#include "LayoutMilestone.h"
#include "LayoutRect.h"
#include "LengthBox.h"
#include "LoadSchedulingMode.h"
#include "MediaProducer.h"
#include "MediaSessionGroupIdentifier.h"
#include "Pagination.h"
#include "PlaybackTargetClientContextIdentifier.h"
#include "RTCController.h"
#include "Region.h"
#include "RegistrableDomain.h"
#include "ScrollTypes.h"
#include "ShouldRelaxThirdPartyCookieBlocking.h"
#include "SpeechRecognitionConnection.h"
#include "Supplementable.h"
#include "Timer.h"
#include "UserInterfaceLayoutDirection.h"
#include "ViewportArguments.h"
#include "VisibilityState.h"
#include <memory>
#include <pal/SessionID.h>
#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/SchedulePair.h>
#endif

#if ENABLE(APPLICATION_MANIFEST)
#include "ApplicationManifest.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetContext.h"
#endif

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
#include "DeviceOrientationUpdateProvider.h"
#endif

namespace JSC {
class Debugger;
}

namespace PAL {
class HysteresisActivity;
}

namespace WTF {
class TextStream;
}

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
}

class AccessibilityRootAtspi;
class ApplePayAMSUIPaymentHandler;
class ActivityStateChangeObserver;
class AlternativeTextClient;
class ApplicationCacheStorage;
class AttachmentElementClient;
class AuthenticatorCoordinator;
class BackForwardController;
class BroadcastChannelRegistry;
class CacheStorageProvider;
class Chrome;
class ContextMenuController;
class CookieJar;
class DOMRectList;
class DatabaseProvider;
class DiagnosticLoggingClient;
class DragCaretController;
class DragController;
class EditorClient;
class Element;
class FocusController;
class FormData;
class Frame;
class HTMLElement;
class HTMLMediaElement;
class HistoryItem;
class ImageAnalysisQueue;
class ImageOverlayController;
class InspectorClient;
class InspectorController;
class IntSize;
class WebRTCProvider;
class LowPowerModeNotifier;
class MediaCanStartListener;
class MediaPlaybackTarget;
class MediaRecorderProvider;
class MediaSessionCoordinatorPrivate;
class ModelPlayerProvider;
class PageConfiguration;
class PageConsoleClient;
class PageDebuggable;
class PageGroup;
class PageOverlayController;
class PaymentCoordinator;
class PerformanceLogging;
class PerformanceLoggingClient;
class PerformanceMonitor;
class PluginData;
class PluginInfoProvider;
class PointerCaptureController;
class PointerLockController;
class ProgressTracker;
class RenderObject;
class ResourceUsageOverlay;
class RenderingUpdateScheduler;
class ScreenOrientationManager;
class ScrollLatchingController;
class ScrollingCoordinator;
class ServicesOverlayController;
class ServiceWorkerGlobalScope;
class Settings;
class SocketProvider;
class SpeechRecognitionProvider;
class SpeechSynthesisClient;
class StorageNamespace;
class StorageNamespaceProvider;
class StorageProvider;
class UserContentProvider;
class UserContentURLPattern;
class UserInputBridge;
class UserScript;
class UserStyleSheet;
class ValidationMessageClient;
class VisibleSelection;
class VisitedLinkStore;
class WebGLStateTracker;
class WheelEventDeltaFilter;
class WheelEventTestMonitor;

struct AXTreeData;
struct ApplePayAMSUIRequest;
struct SimpleRange;
struct TextRecognitionResult;

using PlatformDisplayID = uint32_t;
using SharedStringHash = uint32_t;

enum class CanWrap : bool;
enum class DidWrap : bool;
enum class RouteSharingPolicy : uint8_t;
enum class ShouldTreatAsContinuingLoad : uint8_t;
enum class MediaProducerMediaState : uint32_t;
enum class MediaProducerMediaCaptureKind : uint8_t;
enum class MediaProducerMutedState : uint8_t;

using MediaProducerMediaStateFlags = OptionSet<MediaProducerMediaState>;
using MediaProducerMutedStateFlags = OptionSet<MediaProducerMutedState>;

enum class EventThrottlingBehavior : bool { Responsive, Unresponsive };
enum class MainFrameMainResource : bool { No, Yes };

enum class PageIsEditable : bool { No, Yes };

enum class CompositingPolicy : bool {
    Normal,
    Conservative, // Used in low memory situations.
};

enum class FinalizeRenderingUpdateFlags : uint8_t {
    ApplyScrollingTreeLayerPositions    = 1 << 0,
    InvalidateImagesWithAsyncDecodes    = 1 << 1,
};

enum class RenderingUpdateStep : uint32_t {
    Resize                          = 1 << 0,
    Scroll                          = 1 << 1,
    MediaQueryEvaluation            = 1 << 2,
    Animations                      = 1 << 3,
    Fullscreen                      = 1 << 4,
    AnimationFrameCallbacks         = 1 << 5,
    IntersectionObservations        = 1 << 6,
    ResizeObservations              = 1 << 7,
    Images                          = 1 << 8,
    WheelEventMonitorCallbacks      = 1 << 9,
    CursorUpdate                    = 1 << 10,
    EventRegionUpdate               = 1 << 11,
    LayerFlush                      = 1 << 12,
#if ENABLE(ASYNC_SCROLLING)
    ScrollingTreeUpdate             = 1 << 13,
#endif
    FlushAutofocusCandidates        = 1 << 14,
    VideoFrameCallbacks             = 1 << 15,
    PrepareCanvasesForDisplay       = 1 << 16,
    CaretAnimation                  = 1 << 17,
};

constexpr OptionSet<RenderingUpdateStep> updateRenderingSteps = {
    RenderingUpdateStep::FlushAutofocusCandidates,
    RenderingUpdateStep::Resize,
    RenderingUpdateStep::Scroll,
    RenderingUpdateStep::MediaQueryEvaluation,
    RenderingUpdateStep::Animations,
    RenderingUpdateStep::Fullscreen,
    RenderingUpdateStep::AnimationFrameCallbacks,
    RenderingUpdateStep::IntersectionObservations,
    RenderingUpdateStep::ResizeObservations,
    RenderingUpdateStep::Images,
    RenderingUpdateStep::WheelEventMonitorCallbacks,
    RenderingUpdateStep::CursorUpdate,
    RenderingUpdateStep::EventRegionUpdate,
    RenderingUpdateStep::PrepareCanvasesForDisplay,
    RenderingUpdateStep::CaretAnimation,
};

constexpr auto allRenderingUpdateSteps = updateRenderingSteps | OptionSet<RenderingUpdateStep> {
    RenderingUpdateStep::LayerFlush,
#if ENABLE(ASYNC_SCROLLING)
    RenderingUpdateStep::ScrollingTreeUpdate,
#endif
};


class Page : public Supplementable<Page>, public CanMakeWeakPtr<Page> {
    WTF_MAKE_NONCOPYABLE(Page);
    WTF_MAKE_FAST_ALLOCATED;
    friend class SettingsBase;

public:
    WEBCORE_EXPORT static void updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    WEBCORE_EXPORT static void clearPreviousItemFromAllPages(HistoryItem*);

    WEBCORE_EXPORT void setupForRemoteWorker(const URL& scriptURL, const SecurityOriginData& topOrigin, const String& referrerPolicy);

    void updateStyleAfterChangeInEnvironment();

    WEBCORE_EXPORT explicit Page(PageConfiguration&&);
    WEBCORE_EXPORT ~Page();

    WEBCORE_EXPORT uint64_t renderTreeSize() const;

    WEBCORE_EXPORT void setNeedsRecalcStyleInAllFrames();

    WEBCORE_EXPORT OptionSet<DisabledAdaptations> disabledAdaptations() const;
    WEBCORE_EXPORT ViewportArguments viewportArguments() const;

    const std::optional<ViewportArguments>& overrideViewportArguments() const { return m_overrideViewportArguments; }
    WEBCORE_EXPORT void setOverrideViewportArguments(const std::optional<ViewportArguments>&);

    static void refreshPlugins(bool reload);
    WEBCORE_EXPORT PluginData& pluginData();
    void clearPluginData();

    WEBCORE_EXPORT void setCanStartMedia(bool);
    bool canStartMedia() const { return m_canStartMedia; }

    EditorClient& editorClient() { return m_editorClient.get(); }

    Frame& mainFrame() { return m_mainFrame.get(); }
    const Frame& mainFrame() const { return m_mainFrame.get(); }

    bool openedByDOM() const;
    WEBCORE_EXPORT void setOpenedByDOM();

    bool openedByDOMWithOpener() const { return m_openedByDOMWithOpener; }
    void setOpenedByDOMWithOpener(bool value) { m_openedByDOMWithOpener = value; }

    WEBCORE_EXPORT void goToItem(HistoryItem&, FrameLoadType, ShouldTreatAsContinuingLoad);

    WEBCORE_EXPORT void setGroupName(const String&);
    WEBCORE_EXPORT const String& groupName() const;

    PageGroup& group();

    BroadcastChannelRegistry& broadcastChannelRegistry() { return m_broadcastChannelRegistry; }
    WEBCORE_EXPORT void setBroadcastChannelRegistry(Ref<BroadcastChannelRegistry>&&); // Only used by WebKitLegacy.

    WEBCORE_EXPORT static void forEachPage(const Function<void(Page&)>&);
    WEBCORE_EXPORT static unsigned nonUtilityPageCount();

    unsigned subframeCount() const;

    void incrementNestedRunLoopCount();
    void decrementNestedRunLoopCount();
    bool insideNestedRunLoop() const { return m_nestedRunLoopCount > 0; }
    WEBCORE_EXPORT void whenUnnested(Function<void()>&&);

    void setCurrentKeyboardScrollingAnimator(KeyboardScrollingAnimator*);
    KeyboardScrollingAnimator* currentKeyboardScrollingAnimator() const { return m_currentKeyboardScrollingAnimator.get(); }


#if ENABLE(REMOTE_INSPECTOR)
    WEBCORE_EXPORT bool inspectable() const;
    WEBCORE_EXPORT void setInspectable(bool);
    WEBCORE_EXPORT String remoteInspectionNameOverride() const;
    WEBCORE_EXPORT void setRemoteInspectionNameOverride(const String&);
    void remoteInspectorInformationDidChange() const;
#endif

    Chrome& chrome() const { return *m_chrome; }
    DragCaretController& dragCaretController() const { return *m_dragCaretController; }
#if ENABLE(DRAG_SUPPORT)
    DragController& dragController() const { return *m_dragController; }
#endif
    FocusController& focusController() const { return *m_focusController; }
#if ENABLE(CONTEXT_MENUS)
    ContextMenuController& contextMenuController() const { return *m_contextMenuController; }
#endif
    UserInputBridge& userInputBridge() const { return *m_userInputBridge; }
    InspectorController& inspectorController() const { return *m_inspectorController; }
    PointerCaptureController& pointerCaptureController() const { return *m_pointerCaptureController; }
#if ENABLE(POINTER_LOCK)
    PointerLockController& pointerLockController() const { return *m_pointerLockController; }
#endif
    WebRTCProvider& webRTCProvider() { return m_webRTCProvider.get(); }
    RTCController& rtcController() { return m_rtcController; }
    WEBCORE_EXPORT void disableICECandidateFiltering();
    WEBCORE_EXPORT void enableICECandidateFiltering();
    bool shouldEnableICECandidateFilteringByDefault() const { return m_shouldEnableICECandidateFilteringByDefault; }

    void didChangeMainDocument();
    void mainFrameDidChangeToNonInitialEmptyDocument();

    PerformanceMonitor* performanceMonitor() { return m_performanceMonitor.get(); }

    ValidationMessageClient* validationMessageClient() const { return m_validationMessageClient.get(); }
    void updateValidationBubbleStateIfNeeded();

    WEBCORE_EXPORT ScrollingCoordinator* scrollingCoordinator();

    WEBCORE_EXPORT String scrollingStateTreeAsText();
    WEBCORE_EXPORT String synchronousScrollingReasonsAsText();
    WEBCORE_EXPORT Ref<DOMRectList> nonFastScrollableRectsForTesting();

    WEBCORE_EXPORT Ref<DOMRectList> touchEventRectsForEventForTesting(EventTrackingRegions::EventType);
    WEBCORE_EXPORT Ref<DOMRectList> passiveTouchEventListenerRectsForTesting();

    WEBCORE_EXPORT void settingsDidChange();

    Settings& settings() const { return *m_settings; }
    ProgressTracker& progress() const { return *m_progress; }
    void progressEstimateChanged(Frame&) const;
    void progressFinished(Frame&) const;
    BackForwardController& backForward() const { return *m_backForwardController; }

    Seconds domTimerAlignmentInterval() const { return m_domTimerAlignmentInterval; }

    void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
    bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

    WEBCORE_EXPORT bool findString(const String&, FindOptions, DidWrap* = nullptr);
    WEBCORE_EXPORT uint32_t replaceRangesWithText(const Vector<SimpleRange>& rangesToReplace, const String& replacementText, bool selectionOnly);
    WEBCORE_EXPORT uint32_t replaceSelectionWithText(const String& replacementText);

    WEBCORE_EXPORT void revealCurrentSelection();

    WEBCORE_EXPORT std::optional<SimpleRange> rangeOfString(const String&, const std::optional<SimpleRange>& searchRange, FindOptions);

    WEBCORE_EXPORT unsigned countFindMatches(const String&, FindOptions, unsigned maxMatchCount);
    WEBCORE_EXPORT unsigned markAllMatchesForText(const String&, FindOptions, bool shouldHighlight, unsigned maxMatchCount);

    WEBCORE_EXPORT void unmarkAllTextMatches();

    WEBCORE_EXPORT void dispatchBeforePrintEvent();
    WEBCORE_EXPORT void dispatchAfterPrintEvent();

    // Find all the ranges for the matching text.
    // Upon return, indexForSelection will be one of the following:
    // 0 if there is no user selection
    // the index of the first range after the user selection
    // NoMatchAfterUserSelection if there is no matching text after the user selection.
    struct MatchingRanges {
        Vector<SimpleRange> ranges;
        int indexForSelection { 0 }; // FIXME: Consider std::optional<unsigned> or unsigned for this instead.
    };
    static constexpr int NoMatchAfterUserSelection = -1;
    WEBCORE_EXPORT MatchingRanges findTextMatches(const String&, FindOptions, unsigned maxCount, bool markMatches = true);

#if PLATFORM(COCOA)
    void platformInitialize();
    WEBCORE_EXPORT void addSchedulePair(Ref<SchedulePair>&&);
    WEBCORE_EXPORT void removeSchedulePair(Ref<SchedulePair>&&);
    SchedulePairHashSet* scheduledRunLoopPairs() { return m_scheduledRunLoopPairs.get(); }

    std::unique_ptr<SchedulePairHashSet> m_scheduledRunLoopPairs;
#endif

    WEBCORE_EXPORT const VisibleSelection& selection() const;

    WEBCORE_EXPORT void setDefersLoading(bool);
    bool defersLoading() const { return m_defersLoading; }

    WEBCORE_EXPORT void clearUndoRedoOperations();

    WEBCORE_EXPORT bool inLowQualityImageInterpolationMode() const;
    WEBCORE_EXPORT void setInLowQualityImageInterpolationMode(bool = true);

    float mediaVolume() const { return m_mediaVolume; }
    WEBCORE_EXPORT void setMediaVolume(float);

    WEBCORE_EXPORT void setPageScaleFactor(float scale, const IntPoint& origin, bool inStableState = true);
    float pageScaleFactor() const { return m_pageScaleFactor; }

    UserInterfaceLayoutDirection userInterfaceLayoutDirection() const { return m_userInterfaceLayoutDirection; }
    WEBCORE_EXPORT void setUserInterfaceLayoutDirection(UserInterfaceLayoutDirection);

    WEBCORE_EXPORT void updateMediaElementRateChangeRestrictions();

    void didStartProvisionalLoad();
    void didCommitLoad();
    void didFinishLoad();

    bool delegatesScaling() const { return m_delegatesScaling; }
    WEBCORE_EXPORT void setDelegatesScaling(bool);

    // The view scale factor is multiplied into the page scale factor by all
    // callers of setPageScaleFactor.
    WEBCORE_EXPORT void setViewScaleFactor(float);
    float viewScaleFactor() const { return m_viewScaleFactor; }

    WEBCORE_EXPORT void setZoomedOutPageScaleFactor(float);
    float zoomedOutPageScaleFactor() const { return m_zoomedOutPageScaleFactor; }

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    WEBCORE_EXPORT void setDeviceScaleFactor(float);

    float initialScaleIgnoringContentSize() const { return m_initialScaleIgnoringContentSize; }
    WEBCORE_EXPORT void setInitialScaleIgnoringContentSize(float);

    WEBCORE_EXPORT void screenPropertiesDidChange();
    void windowScreenDidChange(PlatformDisplayID, std::optional<FramesPerSecond> nominalFramesPerSecond);
    PlatformDisplayID displayID() const { return m_displayID; }
    std::optional<FramesPerSecond> displayNominalFramesPerSecond() const { return m_displayNominalFramesPerSecond; }

    // This can return nullopt if throttling reasons result in a frequency less than one, in which case
    // preferredRenderingUpdateInterval provides the frequency.
    // FIXME: Have a single function that returns a std::variant<>.
    enum class PreferredRenderingUpdateOption : uint8_t {
        IncludeThrottlingReasons    = 1 << 0,
        IncludeAnimationsFrameRate  = 1 << 1
    };
    static constexpr OptionSet<PreferredRenderingUpdateOption> allPreferredRenderingUpdateOptions = { PreferredRenderingUpdateOption::IncludeThrottlingReasons, PreferredRenderingUpdateOption::IncludeAnimationsFrameRate };
    std::optional<FramesPerSecond> preferredRenderingUpdateFramesPerSecond(OptionSet<PreferredRenderingUpdateOption> = allPreferredRenderingUpdateOptions) const;
    Seconds preferredRenderingUpdateInterval() const;

    float topContentInset() const { return m_topContentInset; }
    WEBCORE_EXPORT void setTopContentInset(float);

    const FloatBoxExtent& obscuredInsets() const { return m_obscuredInsets; }
    void setObscuredInsets(const FloatBoxExtent& obscuredInsets) { m_obscuredInsets = obscuredInsets; }

    const FloatBoxExtent& contentInsets() const { return m_contentInsets; }
    void setContentInsets(const FloatBoxExtent& insets) { m_contentInsets = insets; }

    const FloatBoxExtent& unobscuredSafeAreaInsets() const { return m_unobscuredSafeAreaInsets; }
    WEBCORE_EXPORT void setUnobscuredSafeAreaInsets(const FloatBoxExtent&);

#if PLATFORM(IOS_FAMILY)
    bool enclosedInScrollableAncestorView() const { return m_enclosedInScrollableAncestorView; }
    void setEnclosedInScrollableAncestorView(bool f) { m_enclosedInScrollableAncestorView = f; }
#endif

    bool useSystemAppearance() const { return m_useSystemAppearance; }
    WEBCORE_EXPORT void setUseSystemAppearance(bool);

    WEBCORE_EXPORT bool useDarkAppearance() const;
    bool useElevatedUserInterfaceLevel() const { return m_useElevatedUserInterfaceLevel; }
    WEBCORE_EXPORT void effectiveAppearanceDidChange(bool useDarkAppearance, bool useElevatedUserInterfaceLevel);
    bool defaultUseDarkAppearance() const { return m_useDarkAppearance; }
    void setUseDarkAppearanceOverride(std::optional<bool>);

#if ENABLE(TEXT_AUTOSIZING)
    float textAutosizingWidth() const { return m_textAutosizingWidth; }
    void setTextAutosizingWidth(float textAutosizingWidth) { m_textAutosizingWidth = textAutosizingWidth; }
    WEBCORE_EXPORT void recomputeTextAutoSizingInAllFrames();
#endif

    OptionSet<FilterRenderingMode> preferredFilterRenderingModes() const;

    const FloatBoxExtent& fullscreenInsets() const { return m_fullscreenInsets; }
    WEBCORE_EXPORT void setFullscreenInsets(const FloatBoxExtent&);

    const Seconds fullscreenAutoHideDuration() const { return m_fullscreenAutoHideDuration; }
    WEBCORE_EXPORT void setFullscreenAutoHideDuration(Seconds);
    WEBCORE_EXPORT void setFullscreenControlsHidden(bool);

    bool shouldSuppressScrollbarAnimations() const { return m_suppressScrollbarAnimations; }
    WEBCORE_EXPORT void setShouldSuppressScrollbarAnimations(bool suppressAnimations);
    void lockAllOverlayScrollbarsToHidden(bool lockOverlayScrollbars);
    
    WEBCORE_EXPORT void setVerticalScrollElasticity(ScrollElasticity);
    ScrollElasticity verticalScrollElasticity() const { return static_cast<ScrollElasticity>(m_verticalScrollElasticity); }

    WEBCORE_EXPORT void setHorizontalScrollElasticity(ScrollElasticity);
    ScrollElasticity horizontalScrollElasticity() const { return static_cast<ScrollElasticity>(m_horizontalScrollElasticity); }

    WEBCORE_EXPORT void accessibilitySettingsDidChange();
    WEBCORE_EXPORT void appearanceDidChange();

    // Page and FrameView both store a Pagination value. Page::pagination() is set only by API,
    // and FrameView::pagination() is set only by CSS. Page::pagination() will affect all
    // FrameViews in the back/forward cache, but FrameView::pagination() only affects the current
    // FrameView.
    const Pagination& pagination() const { return m_pagination; }
    WEBCORE_EXPORT void setPagination(const Pagination&);

    WEBCORE_EXPORT unsigned pageCount() const;

    WEBCORE_EXPORT DiagnosticLoggingClient& diagnosticLoggingClient() const;

    WEBCORE_EXPORT void logMediaDiagnosticMessage(const FormData*) const;

    PerformanceLoggingClient* performanceLoggingClient() const { return m_performanceLoggingClient.get(); }

    WheelEventDeltaFilter* wheelEventDeltaFilter() { return m_recentWheelEventDeltaFilter.get(); }
    PageOverlayController& pageOverlayController() { return *m_pageOverlayController; }

#if PLATFORM(MAC) && (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION))
    ServicesOverlayController& servicesOverlayController() { return *m_servicesOverlayController; }
#endif
    ImageOverlayController& imageOverlayController();
    ImageOverlayController* imageOverlayControllerIfExists() { return m_imageOverlayController.get(); }

#if ENABLE(IMAGE_ANALYSIS)
    WEBCORE_EXPORT ImageAnalysisQueue& imageAnalysisQueue();
    ImageAnalysisQueue* imageAnalysisQueueIfExists() { return m_imageAnalysisQueue.get(); }
#endif

#if ENABLE(WHEEL_EVENT_LATCHING)
    ScrollLatchingController& scrollLatchingController();
    ScrollLatchingController* scrollLatchingControllerIfExists();
#endif // ENABLE(WHEEL_EVENT_LATCHING)

#if ENABLE(APPLE_PAY)
    PaymentCoordinator& paymentCoordinator() const { return *m_paymentCoordinator; }
    WEBCORE_EXPORT void setPaymentCoordinator(std::unique_ptr<PaymentCoordinator>&&);
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    bool hasActiveApplePayAMSUISession() const { return m_activeApplePayAMSUIPaymentHandler; }
    bool startApplePayAMSUISession(Document&, ApplePayAMSUIPaymentHandler&, const ApplePayAMSUIRequest&);
    void abortApplePayAMSUISession(ApplePayAMSUIPaymentHandler&);
#endif

#if ENABLE(WEB_AUTHN)
    AuthenticatorCoordinator& authenticatorCoordinator() { return m_authenticatorCoordinator.get(); }
#endif

#if ENABLE(APPLICATION_MANIFEST)
    const std::optional<ApplicationManifest>& applicationManifest() const { return m_applicationManifest; }
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    MediaSessionCoordinatorPrivate* mediaSessionCoordinator() { return m_mediaSessionCoordinator.get(); }
    WEBCORE_EXPORT void setMediaSessionCoordinator(Ref<MediaSessionCoordinatorPrivate>&&);
    WEBCORE_EXPORT void invalidateMediaSessionCoordinator();
#endif

    bool isServiceWorkerPage() const { return m_isServiceWorkerPage; }
    void markAsServiceWorkerPage() { m_isServiceWorkerPage = true; }

    WEBCORE_EXPORT static Page* serviceWorkerPage(ScriptExecutionContextIdentifier);

#if ENABLE(SERVICE_WORKER)
    // Service worker pages have an associated ServiceWorkerGlobalScope on the main thread.
    void setServiceWorkerGlobalScope(ServiceWorkerGlobalScope&);
    WEBCORE_EXPORT JSC::JSGlobalObject* serviceWorkerGlobalObject(DOMWrapperWorld&);
#endif

    // Notifications when the Page starts and stops being presented via a native window.
    WEBCORE_EXPORT void setActivityState(OptionSet<ActivityState::Flag>);
    OptionSet<ActivityState::Flag> activityState() const { return m_activityState; }

    bool isWindowActive() const;
    bool isVisibleAndActive() const;
    WEBCORE_EXPORT void setIsVisible(bool);
    WEBCORE_EXPORT void setIsPrerender();
    bool isVisible() const { return m_activityState.contains(ActivityState::IsVisible); }

    // Notification that this Page was moved into or out of a native window.
    WEBCORE_EXPORT void setIsInWindow(bool);
    bool isInWindow() const { return m_activityState.contains(ActivityState::IsInWindow); }

    void setIsClosing() { m_isClosing = true; }
    bool isClosing() const { return m_isClosing; }

    void setIsRestoringCachedPage(bool value) { m_isRestoringCachedPage = value; }
    bool isRestoringCachedPage() const { return m_isRestoringCachedPage; }

    WEBCORE_EXPORT void addActivityStateChangeObserver(ActivityStateChangeObserver&);
    WEBCORE_EXPORT void removeActivityStateChangeObserver(ActivityStateChangeObserver&);

    WEBCORE_EXPORT void layoutIfNeeded();
    WEBCORE_EXPORT void updateRendering();
    // A call to updateRendering() that is not followed by a call to finalizeRenderingUpdate().
    WEBCORE_EXPORT void isolatedUpdateRendering();
    // Called when the rendering update steps are complete, but before painting.
    WEBCORE_EXPORT void finalizeRenderingUpdate(OptionSet<FinalizeRenderingUpdateFlags>);

    // Called before and after the "display" steps of the rendering update: painting, and when we push
    // layers to the platform compositor.
    WEBCORE_EXPORT void willStartRenderingUpdateDisplay();
    WEBCORE_EXPORT void didCompleteRenderingUpdateDisplay();
    // Called after didCompleteRenderingUpdateDisplay, but in the same run loop iteration (i.e. before zero-delay timers triggered from the rendering update).
    WEBCORE_EXPORT void didCompleteRenderingFrame();

    // Schedule a rendering update that coordinates with display refresh.
    WEBCORE_EXPORT void scheduleRenderingUpdate(OptionSet<RenderingUpdateStep> requestedSteps);
    void didScheduleRenderingUpdate();
    // Trigger a rendering update in the current runloop. Only used for testing.
    void triggerRenderingUpdateForTesting();

    WEBCORE_EXPORT void startTrackingRenderingUpdates();
    WEBCORE_EXPORT unsigned renderingUpdateCount() const;

    WEBCORE_EXPORT void suspendScriptedAnimations();
    WEBCORE_EXPORT void resumeScriptedAnimations();
    bool scriptedAnimationsSuspended() const { return m_scriptedAnimationsSuspended; }

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    WEBCORE_EXPORT void setImageAnimationEnabled(bool);
    void addIndividuallyPlayingAnimationElement(HTMLImageElement&);
    void removeIndividuallyPlayingAnimationElement(HTMLImageElement&);
#endif
    bool imageAnimationEnabled() const { return m_imageAnimationEnabled; }

    void userStyleSheetLocationChanged();
    const String& userStyleSheet() const;

    WEBCORE_EXPORT void userAgentChanged();

    void dnsPrefetchingStateChanged();
    void storageBlockingStateChanged();

#if ENABLE(RESOURCE_USAGE)
    void setResourceUsageOverlayVisible(bool);
#endif

    void setDebugger(JSC::Debugger*);
    JSC::Debugger* debugger() const { return m_debugger; }

    WEBCORE_EXPORT void invalidateStylesForAllLinks();
    WEBCORE_EXPORT void invalidateStylesForLink(SharedStringHash);

    void invalidateInjectedStyleSheetCacheInAllFrames();

    bool hasCustomHTMLTokenizerTimeDelay() const;
    double customHTMLTokenizerTimeDelay() const;

    WEBCORE_EXPORT void setCORSDisablingPatterns(Vector<UserContentURLPattern>&&);
    const Vector<UserContentURLPattern>& corsDisablingPatterns() const { return m_corsDisablingPatterns; }

    WEBCORE_EXPORT void setMemoryCacheClientCallsEnabled(bool);
    bool areMemoryCacheClientCallsEnabled() const { return m_areMemoryCacheClientCallsEnabled; }

    // Don't allow more than a certain number of frames in a page.
    // This seems like a reasonable upper bound, and otherwise mutually
    // recursive frameset pages can quickly bring the program to its knees
    // with exponential growth in the number of frames.
    static constexpr int maxNumberOfFrames = 1000;

    // Don't allow more than a certain frame depth to avoid stack exhaustion.
    static constexpr int maxFrameDepth = 32;

    void setEditable(bool isEditable) { m_isEditable = isEditable; }
    bool isEditable() const { return m_isEditable; }

    WEBCORE_EXPORT VisibilityState visibilityState() const;
    WEBCORE_EXPORT void resumeAnimatingImages();
    void updatePlayStateForAllAnimations();

    void didFinishLoadingImageForElement(HTMLImageElement&);

    WEBCORE_EXPORT void addLayoutMilestones(OptionSet<LayoutMilestone>);
    WEBCORE_EXPORT void removeLayoutMilestones(OptionSet<LayoutMilestone>);
    OptionSet<LayoutMilestone> requestedLayoutMilestones() const { return m_requestedLayoutMilestones; }

    WEBCORE_EXPORT void setHeaderHeight(int);
    WEBCORE_EXPORT void setFooterHeight(int);

    int headerHeight() const { return m_headerHeight; }
    int footerHeight() const { return m_footerHeight; }

    WEBCORE_EXPORT Color themeColor() const;
    WEBCORE_EXPORT Color pageExtendedBackgroundColor() const;
    WEBCORE_EXPORT Color sampledPageTopColor() const;

    Color underPageBackgroundColorOverride() const { return m_underPageBackgroundColorOverride; }
    WEBCORE_EXPORT void setUnderPageBackgroundColorOverride(Color&&);

    bool isCountingRelevantRepaintedObjects() const;
    void setIsCountingRelevantRepaintedObjects(bool isCounting) { m_isCountingRelevantRepaintedObjects = isCounting; }
    void startCountingRelevantRepaintedObjects();
    void resetRelevantPaintedObjectCounter();
    void addRelevantRepaintedObject(RenderObject*, const LayoutRect& objectPaintRect);
    void addRelevantUnpaintedObject(RenderObject*, const LayoutRect& objectPaintRect);

    WEBCORE_EXPORT void suspendActiveDOMObjectsAndAnimations();
    WEBCORE_EXPORT void resumeActiveDOMObjectsAndAnimations();

#ifndef NDEBUG
    void setIsPainting(bool painting) { m_isPainting = painting; }
    bool isPainting() const { return m_isPainting; }
#endif

    AlternativeTextClient* alternativeTextClient() const { return m_alternativeTextClient.get(); }

    bool hasSeenPlugin(const String& serviceType) const;
    WEBCORE_EXPORT bool hasSeenAnyPlugin() const;
    void sawPlugin(const String& serviceType);
    void resetSeenPlugins();

    bool hasSeenMediaEngine(const String& engineName) const;
    bool hasSeenAnyMediaEngine() const;
    void sawMediaEngine(const String& engineName);
    void resetSeenMediaEngines();

    PageConsoleClient& console() { return *m_consoleClient; }

#if ENABLE(REMOTE_INSPECTOR)
    PageDebuggable& inspectorDebuggable() const { return *m_inspectorDebuggable.get(); }
#endif

    void hiddenPageCSSAnimationSuspensionStateChanged();

#if ENABLE(VIDEO)
    void captionPreferencesChanged();
#endif

    void forbidPrompts();
    void allowPrompts();
    bool arePromptsAllowed();

    void forbidSynchronousLoads();
    void allowSynchronousLoads();
    bool areSynchronousLoadsAllowed();

    void mainFrameLoadStarted(const URL&, FrameLoadType);

    void setLastSpatialNavigationCandidateCount(unsigned count) { m_lastSpatialNavigationCandidatesCount = count; }
    unsigned lastSpatialNavigationCandidateCount() const { return m_lastSpatialNavigationCandidatesCount; }

    ApplicationCacheStorage& applicationCacheStorage() { return m_applicationCacheStorage; }
    DatabaseProvider& databaseProvider() { return m_databaseProvider; }
    CacheStorageProvider& cacheStorageProvider() { return m_cacheStorageProvider; }
    SocketProvider& socketProvider() { return m_socketProvider; }
    MediaRecorderProvider& mediaRecorderProvider() { return m_mediaRecorderProvider; }
    CookieJar& cookieJar() { return m_cookieJar.get(); }

    StorageNamespaceProvider& storageNamespaceProvider() { return m_storageNamespaceProvider.get(); }

    PluginInfoProvider& pluginInfoProvider();

    WEBCORE_EXPORT UserContentProvider& userContentProvider();
    WEBCORE_EXPORT void setUserContentProvider(Ref<UserContentProvider>&&);

    ScreenOrientationManager* screenOrientationManager() const;

    VisitedLinkStore& visitedLinkStore();
    WEBCORE_EXPORT void setVisitedLinkStore(Ref<VisitedLinkStore>&&);

    WEBCORE_EXPORT PAL::SessionID sessionID() const;
    WEBCORE_EXPORT void setSessionID(PAL::SessionID);
    bool usesEphemeralSession() const { return m_sessionID.isEphemeral(); }

    MediaProducerMediaStateFlags mediaState() const { return m_mediaState; }
    void updateIsPlayingMedia();
    MediaProducerMutedStateFlags mutedState() const { return m_mutedState; }
    inline bool isAudioMuted() const;
    inline bool isMediaCaptureMuted() const;
    void schedulePlaybackControlsManagerUpdate();
#if ENABLE(VIDEO)
    void playbackControlsMediaEngineChanged();
#endif
    WEBCORE_EXPORT void setMuted(MediaProducerMutedStateFlags);

    WEBCORE_EXPORT void stopMediaCapture(MediaProducerMediaCaptureKind);

    MediaSessionGroupIdentifier mediaSessionGroupIdentifier() const;
    WEBCORE_EXPORT bool mediaPlaybackExists();
    WEBCORE_EXPORT bool mediaPlaybackIsPaused();
    WEBCORE_EXPORT void pauseAllMediaPlayback();
    WEBCORE_EXPORT void suspendAllMediaPlayback();
    WEBCORE_EXPORT void resumeAllMediaPlayback();
    bool mediaPlaybackIsSuspended() const { return m_mediaPlaybackIsSuspended; }
    WEBCORE_EXPORT void suspendAllMediaBuffering();
    WEBCORE_EXPORT void resumeAllMediaBuffering();
    bool mediaBufferingIsSuspended() const { return m_mediaBufferingIsSuspended; }

    void setHasResourceLoadClient(bool has) { m_hasResourceLoadClient = has; }
    bool hasResourceLoadClient() const { return m_hasResourceLoadClient; }

    void setCanUseCredentialStorage(bool canUse) { m_canUseCredentialStorage = canUse; }
    bool canUseCredentialStorage() const { return m_canUseCredentialStorage; }

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier);
    void removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier);
    void showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier, const IntPoint&, bool, RouteSharingPolicy, const String&);
    void playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier, MediaProducerMediaStateFlags);
    WEBCORE_EXPORT void setMockMediaPlaybackTargetPickerEnabled(bool);
    WEBCORE_EXPORT void setMockMediaPlaybackTargetPickerState(const String&, MediaPlaybackTargetContext::MockState);
    WEBCORE_EXPORT void mockMediaPlaybackTargetPickerDismissPopup();

    WEBCORE_EXPORT void setPlaybackTarget(PlaybackTargetClientContextIdentifier, Ref<MediaPlaybackTarget>&&);
    WEBCORE_EXPORT void playbackTargetAvailabilityDidChange(PlaybackTargetClientContextIdentifier, bool);
    WEBCORE_EXPORT void setShouldPlayToPlaybackTarget(PlaybackTargetClientContextIdentifier, bool);
    WEBCORE_EXPORT void playbackTargetPickerWasDismissed(PlaybackTargetClientContextIdentifier);
#endif

    WEBCORE_EXPORT RefPtr<WheelEventTestMonitor> wheelEventTestMonitor() const;
    WEBCORE_EXPORT void clearWheelEventTestMonitor();
    WEBCORE_EXPORT void startMonitoringWheelEvents(bool clearLatchingState);
    WEBCORE_EXPORT bool isMonitoringWheelEvents() const;

#if ENABLE(VIDEO)
    bool allowsMediaDocumentInlinePlayback() const { return m_allowsMediaDocumentInlinePlayback; }
    WEBCORE_EXPORT void setAllowsMediaDocumentInlinePlayback(bool);
#endif

    bool allowsPlaybackControlsForAutoplayingAudio() const { return m_allowsPlaybackControlsForAutoplayingAudio; }
    void setAllowsPlaybackControlsForAutoplayingAudio(bool allowsPlaybackControlsForAutoplayingAudio) { m_allowsPlaybackControlsForAutoplayingAudio = allowsPlaybackControlsForAutoplayingAudio; }

    IDBClient::IDBConnectionToServer& idbConnection();
    WEBCORE_EXPORT IDBClient::IDBConnectionToServer* optionalIDBConnection();
    WEBCORE_EXPORT void clearIDBConnection();

    void setShowAllPlugins(bool showAll) { m_showAllPlugins = showAll; }
    bool showAllPlugins() const;

    WEBCORE_EXPORT void setDOMTimerAlignmentIntervalIncreaseLimit(Seconds);

    bool isControlledByAutomation() const { return m_controlledByAutomation; }
    void setControlledByAutomation(bool controlled) { m_controlledByAutomation = controlled; }

    String captionUserPreferencesStyleSheet();
    void setCaptionUserPreferencesStyleSheet(const String&);

    bool isResourceCachingDisabledByWebInspector() const { return m_resourceCachingDisabledByWebInspector; }
    void setResourceCachingDisabledByWebInspector(bool disabled) { m_resourceCachingDisabledByWebInspector = disabled; }

    std::optional<EventThrottlingBehavior> eventThrottlingBehaviorOverride() const { return m_eventThrottlingBehaviorOverride; }
    void setEventThrottlingBehaviorOverride(std::optional<EventThrottlingBehavior> throttling) { m_eventThrottlingBehaviorOverride = throttling; }

    std::optional<CompositingPolicy> compositingPolicyOverride() const { return m_compositingPolicyOverride; }
    void setCompositingPolicyOverride(std::optional<CompositingPolicy> policy) { m_compositingPolicyOverride = policy; }

#if ENABLE(WEBGL)
    WebGLStateTracker* webGLStateTracker() const { return m_webGLStateTracker.get(); }
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    SpeechSynthesisClient* speechSynthesisClient() const { return m_speechSynthesisClient.get(); }
#endif

    WEBCORE_EXPORT SpeechRecognitionConnection& speechRecognitionConnection();

    bool isOnlyNonUtilityPage() const;
    bool isUtilityPage() const { return m_isUtilityPage; }

    WEBCORE_EXPORT bool allowsLoadFromURL(const URL&, MainFrameMainResource) const;
    ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking() const { return m_shouldRelaxThirdPartyCookieBlocking; }

    bool isLowPowerModeEnabled() const { return m_throttlingReasons.contains(ThrottlingReason::LowPowerMode); }
    bool canUpdateThrottlingReason(ThrottlingReason reason) const { return !m_throttlingReasonsOverridenForTesting.contains(reason); }
    WEBCORE_EXPORT void setLowPowerModeEnabledOverrideForTesting(std::optional<bool>);
    WEBCORE_EXPORT void setOutsideViewportThrottlingEnabledForTesting(bool);

    OptionSet<ThrottlingReason> throttlingReasons() const { return m_throttlingReasons; }

    WEBCORE_EXPORT void applicationWillResignActive();
    WEBCORE_EXPORT void applicationDidEnterBackground();
    WEBCORE_EXPORT void applicationWillEnterForeground();
    WEBCORE_EXPORT void applicationDidBecomeActive();

    PerformanceLogging& performanceLogging() const { return *m_performanceLogging; }

    void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel);

#if ENABLE(EDITABLE_REGION)
    bool shouldBuildEditableRegion() const;
    bool isEditableRegionEnabled() const { return m_isEditableRegionEnabled; }
    WEBCORE_EXPORT void setEditableRegionEnabled(bool = true);
#endif

    WEBCORE_EXPORT Vector<Ref<Element>> editableElementsInRect(const FloatRect&) const;

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    bool shouldBuildInteractionRegions() const;
    WEBCORE_EXPORT void setInteractionRegionsEnabled(bool);
#endif

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    DeviceOrientationUpdateProvider* deviceOrientationUpdateProvider() const { return m_deviceOrientationUpdateProvider.get(); }
#endif

    WEBCORE_EXPORT void forEachDocument(const Function<void(Document&)>&) const;
    void forEachMediaElement(const Function<void(HTMLMediaElement&)>&);
    static void forEachDocumentFromMainFrame(const Frame&, const Function<void(Document&)>&);
    void forEachFrame(const Function<void(Frame&)>&);

    bool shouldDisableCorsForRequestTo(const URL&) const;
    const HashSet<String>& maskedURLSchemes() const { return m_maskedURLSchemes; }

    WEBCORE_EXPORT void injectUserStyleSheet(UserStyleSheet&);
    WEBCORE_EXPORT void removeInjectedUserStyleSheet(UserStyleSheet&);

    bool isTakingSnapshotsForApplicationSuspension() const { return m_isTakingSnapshotsForApplicationSuspension; }
    void setIsTakingSnapshotsForApplicationSuspension(bool isTakingSnapshotsForApplicationSuspension) { m_isTakingSnapshotsForApplicationSuspension = isTakingSnapshotsForApplicationSuspension; }

    bool hasBeenNotifiedToInjectUserScripts() const { return m_hasBeenNotifiedToInjectUserScripts; }
    WEBCORE_EXPORT void notifyToInjectUserScripts();

    MonotonicTime lastRenderingUpdateTimestamp() const { return m_lastRenderingUpdateTimestamp; }

    bool httpsUpgradeEnabled() const { return m_httpsUpgradeEnabled; }

    URL sanitizeLookalikeCharacters(const URL&) const;
    String sanitizeLookalikeCharacters(const String&) const;

    LoadSchedulingMode loadSchedulingMode() const { return m_loadSchedulingMode; }
    void setLoadSchedulingMode(LoadSchedulingMode);

#if ENABLE(IMAGE_ANALYSIS)
    std::optional<TextRecognitionResult> cachedTextRecognitionResult(const HTMLElement&) const;
    WEBCORE_EXPORT bool hasCachedTextRecognitionResult(const HTMLElement&) const;
    void cacheTextRecognitionResult(const HTMLElement&, const IntRect& containerRect, const TextRecognitionResult&);
    void resetTextRecognitionResult(const HTMLElement&);
    void resetImageAnalysisQueue();
#endif

    WEBCORE_EXPORT StorageConnection& storageConnection();

    ModelPlayerProvider& modelPlayerProvider();

#if ENABLE(ATTACHMENT_ELEMENT)
    AttachmentElementClient* attachmentElementClient() { return m_attachmentElementClient.get(); }
#endif

    WEBCORE_EXPORT std::optional<AXTreeData> accessibilityTreeData() const;
#if USE(ATSPI)
    AccessibilityRootAtspi* accessibilityRootObject() const { return m_accessibilityRootObject; }
    void setAccessibilityRootObject(AccessibilityRootAtspi* rootObject) { m_accessibilityRootObject = rootObject; }
#endif

#if PLATFORM(COCOA)
    void setIsAwaitingLayerTreeTransactionFlush(bool isAwaiting) { m_isAwaitingLayerTreeTransactionFlush = isAwaiting; }
    bool isAwaitingLayerTreeTransactionFlush() const { return m_isAwaitingLayerTreeTransactionFlush; }
#endif

    void timelineControllerMaximumAnimationFrameRateDidChange(DocumentTimelinesController&);

    ContentSecurityPolicyModeForExtension contentSecurityPolicyModeForExtension() const { return m_contentSecurityPolicyModeForExtension; }

    WEBCORE_EXPORT void forceRepaintAllFrames();

#if ENABLE(IMAGE_ANALYSIS)
    WEBCORE_EXPORT void analyzeImagesForFindInPage(Function<void()>&& callback);
#endif

    BadgeClient& badgeClient() { return m_badgeClient.get(); }

    void willBeginScrolling();
    void didFinishScrolling();

private:
    struct Navigation {
        RegistrableDomain domain;
        FrameLoadType type;
    };
    void logNavigation(const Navigation&);

    static void firstTimeInitialization();

    WEBCORE_EXPORT void initGroup();

    void setIsInWindowInternal(bool);
    void setIsVisibleInternal(bool);
    void setIsVisuallyIdleInternal(bool);

    void stopKeyboardScrollAnimation();

    enum ShouldHighlightMatches { DoNotHighlightMatches, HighlightMatches };
    enum ShouldMarkMatches { DoNotMarkMatches, MarkMatches };

    unsigned findMatchesForText(const String&, FindOptions, unsigned maxMatchCount, ShouldHighlightMatches, ShouldMarkMatches);

    std::optional<std::pair<MediaCanStartListener&, Document&>> takeAnyMediaCanStartListener();

#if ENABLE(VIDEO)
    void playbackControlsManagerUpdateTimerFired();
#endif

    void handleLowModePowerChange(bool);

    enum class TimerThrottlingState { Disabled, Enabled, EnabledIncreasing };
    void hiddenPageDOMTimerThrottlingStateChanged();
    void setTimerThrottlingState(TimerThrottlingState);
    void updateTimerThrottlingState();
    void updateDOMTimerAlignmentInterval();
    void domTimerAlignmentIntervalIncreaseTimerFired();

    void doAfterUpdateRendering();
    void renderingUpdateCompleted();
    void computeUnfulfilledRenderingSteps(OptionSet<RenderingUpdateStep>);
    void scheduleRenderingUpdateInternal();
    void prioritizeVisibleResources();

    RenderingUpdateScheduler& renderingUpdateScheduler();

    WheelEventTestMonitor& ensureWheelEventTestMonitor();

#if ENABLE(IMAGE_ANALYSIS)
    void resetTextRecognitionResults();
    void updateElementsWithTextRecognitionResults();
#endif

    const std::unique_ptr<Chrome> m_chrome;
    const std::unique_ptr<DragCaretController> m_dragCaretController;

#if ENABLE(DRAG_SUPPORT)
    const std::unique_ptr<DragController> m_dragController;
#endif
    const std::unique_ptr<FocusController> m_focusController;
#if ENABLE(CONTEXT_MENUS)
    const std::unique_ptr<ContextMenuController> m_contextMenuController;
#endif
    const std::unique_ptr<UserInputBridge> m_userInputBridge;
    const std::unique_ptr<InspectorController> m_inspectorController;
    const std::unique_ptr<PointerCaptureController> m_pointerCaptureController;
#if ENABLE(POINTER_LOCK)
    const std::unique_ptr<PointerLockController> m_pointerLockController;
#endif
    RefPtr<ScrollingCoordinator> m_scrollingCoordinator;

    const RefPtr<Settings> m_settings;
    const std::unique_ptr<ProgressTracker> m_progress;

    const std::unique_ptr<BackForwardController> m_backForwardController;
    Ref<Frame> m_mainFrame;

    RefPtr<PluginData> m_pluginData;

    UniqueRef<EditorClient> m_editorClient;
    std::unique_ptr<ValidationMessageClient> m_validationMessageClient;
    std::unique_ptr<DiagnosticLoggingClient> m_diagnosticLoggingClient;
    std::unique_ptr<PerformanceLoggingClient> m_performanceLoggingClient;

#if ENABLE(WEBGL)
    std::unique_ptr<WebGLStateTracker> m_webGLStateTracker;
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    std::unique_ptr<SpeechSynthesisClient> m_speechSynthesisClient;
#endif

    UniqueRef<SpeechRecognitionProvider> m_speechRecognitionProvider;

    UniqueRef<MediaRecorderProvider> m_mediaRecorderProvider;
    UniqueRef<WebRTCProvider> m_webRTCProvider;
    RTCController m_rtcController;

    PlatformDisplayID m_displayID { 0 };
    std::optional<FramesPerSecond> m_displayNominalFramesPerSecond;

    int m_nestedRunLoopCount { 0 };
    Function<void()> m_unnestCallback;

    String m_groupName;
    bool m_openedByDOM { false };
    bool m_openedByDOMWithOpener { false };

    bool m_tabKeyCyclesThroughElements { true };
    bool m_defersLoading { false };
    unsigned m_defersLoadingCallCount { 0 };

    bool m_inLowQualityInterpolationMode { false };
    bool m_areMemoryCacheClientCallsEnabled { true };
    float m_mediaVolume { 1 };
    MediaProducerMutedStateFlags m_mutedState;

    float m_pageScaleFactor { 1 };
    float m_zoomedOutPageScaleFactor { 0 };
    float m_deviceScaleFactor { 1 };
    float m_viewScaleFactor { 1 };

    float m_topContentInset { 0 };
    FloatBoxExtent m_obscuredInsets;
    FloatBoxExtent m_contentInsets;
    FloatBoxExtent m_unobscuredSafeAreaInsets;
    FloatBoxExtent m_fullscreenInsets;
    Seconds m_fullscreenAutoHideDuration { 0_s };

#if PLATFORM(IOS_FAMILY)
    bool m_enclosedInScrollableAncestorView { false };
#endif
    
    bool m_useSystemAppearance { false };
    bool m_useElevatedUserInterfaceLevel { false };
    bool m_useDarkAppearance { false };
    std::optional<bool> m_useDarkAppearanceOverride;

#if ENABLE(TEXT_AUTOSIZING)
    float m_textAutosizingWidth { 0 };
#endif
    float m_initialScaleIgnoringContentSize { 1.0f };
    
    bool m_suppressScrollbarAnimations { false };
    
    ScrollElasticity m_verticalScrollElasticity { ScrollElasticity::Allowed };
    ScrollElasticity m_horizontalScrollElasticity { ScrollElasticity::Allowed };

    Pagination m_pagination;

    String m_userStyleSheetPath;
    mutable String m_userStyleSheet;
    mutable bool m_didLoadUserStyleSheet { false };
    mutable std::optional<WallTime> m_userStyleSheetModificationTime;

    String m_captionUserPreferencesStyleSheet;

    std::unique_ptr<PageGroup> m_singlePageGroup;
    PageGroup* m_group { nullptr };

    JSC::Debugger* m_debugger { nullptr };

    bool m_canStartMedia { true };
    bool m_imageAnimationEnabled { true };
    // Elements containing animations that are individually playing (potentially overriding the page-wide m_imageAnimationEnabled state).
    WeakHashSet<HTMLImageElement, WeakPtrImplWithEventTargetData> m_individuallyPlayingAnimationElements;

    TimerThrottlingState m_timerThrottlingState { TimerThrottlingState::Disabled };
    MonotonicTime m_timerThrottlingStateLastChangedTime;
    Seconds m_domTimerAlignmentInterval;
    Timer m_domTimerAlignmentIntervalIncreaseTimer;
    Seconds m_domTimerAlignmentIntervalIncreaseLimit;

    bool m_isEditable { false };
    bool m_isPrerender { false };
    OptionSet<ActivityState::Flag> m_activityState;

    OptionSet<LayoutMilestone> m_requestedLayoutMilestones;

    int m_headerHeight { 0 };
    int m_footerHeight { 0 };

    std::unique_ptr<RenderingUpdateScheduler> m_renderingUpdateScheduler;

    HashSet<RenderObject*> m_relevantUnpaintedRenderObjects;
    Region m_topRelevantPaintedRegion;
    Region m_bottomRelevantPaintedRegion;
    Region m_relevantUnpaintedRegion;
    bool m_isCountingRelevantRepaintedObjects { false };
#ifndef NDEBUG
    bool m_isPainting { false };
#endif
    std::unique_ptr<AlternativeTextClient> m_alternativeTextClient;

    bool m_scriptedAnimationsSuspended { false };
    const std::unique_ptr<PageConsoleClient> m_consoleClient;

#if ENABLE(REMOTE_INSPECTOR)
    const std::unique_ptr<PageDebuggable> m_inspectorDebuggable;
#endif

    RefPtr<IDBClient::IDBConnectionToServer> m_idbConnectionToServer;

    MemoryCompactRobinHoodHashSet<String> m_seenPlugins;
    MemoryCompactRobinHoodHashSet<String> m_seenMediaEngines;

    unsigned m_lastSpatialNavigationCandidatesCount { 0 };
    unsigned m_forbidPromptsDepth { 0 };
    unsigned m_forbidSynchronousLoadsDepth { 0 };

    Ref<SocketProvider> m_socketProvider;
    Ref<CookieJar> m_cookieJar;
    Ref<ApplicationCacheStorage> m_applicationCacheStorage;
    Ref<CacheStorageProvider> m_cacheStorageProvider;
    Ref<DatabaseProvider> m_databaseProvider;
    Ref<PluginInfoProvider> m_pluginInfoProvider;
    Ref<StorageNamespaceProvider> m_storageNamespaceProvider;
    Ref<UserContentProvider> m_userContentProvider;
    WeakPtr<ScreenOrientationManager> m_screenOrientationManager;
    Ref<VisitedLinkStore> m_visitedLinkStore;
    Ref<BroadcastChannelRegistry> m_broadcastChannelRegistry;
    RefPtr<WheelEventTestMonitor> m_wheelEventTestMonitor;
    WeakHashSet<ActivityStateChangeObserver> m_activityStateChangeObservers;

#if ENABLE(SERVICE_WORKER)
    WeakPtr<ServiceWorkerGlobalScope, WeakPtrImplWithEventTargetData> m_serviceWorkerGlobalScope;
#endif

#if ENABLE(RESOURCE_USAGE)
    std::unique_ptr<ResourceUsageOverlay> m_resourceUsageOverlay;
#endif

    PAL::SessionID m_sessionID;

    unsigned m_renderingUpdateCount { 0 };
    bool m_isTrackingRenderingUpdates { false };

    bool m_isClosing { false };
    bool m_isRestoringCachedPage { false };

    MediaProducerMediaStateFlags m_mediaState;

#if ENABLE(VIDEO)
    Timer m_playbackControlsManagerUpdateTimer;
#endif

    bool m_allowsMediaDocumentInlinePlayback { false };
    bool m_allowsPlaybackControlsForAutoplayingAudio { false };
    bool m_showAllPlugins { false };
    bool m_controlledByAutomation { false };
    bool m_resourceCachingDisabledByWebInspector { false };
    bool m_isUtilityPage;
    bool m_shouldEnableICECandidateFilteringByDefault { true };
    bool m_mediaPlaybackIsSuspended { false };
    bool m_mediaBufferingIsSuspended { false };
    bool m_hasResourceLoadClient { false };
    bool m_delegatesScaling { false };

#if ENABLE(EDITABLE_REGION)
    bool m_isEditableRegionEnabled { false };
#endif

#if PLATFORM(COCOA)
    bool m_isAwaitingLayerTreeTransactionFlush { false };
#endif

    Vector<OptionSet<RenderingUpdateStep>, 2> m_renderingUpdateRemainingSteps;
    OptionSet<RenderingUpdateStep> m_unfulfilledRequestedSteps;
    
    UserInterfaceLayoutDirection m_userInterfaceLayoutDirection { UserInterfaceLayoutDirection::LTR };
    
    // For testing.
    std::optional<EventThrottlingBehavior> m_eventThrottlingBehaviorOverride;
    std::optional<CompositingPolicy> m_compositingPolicyOverride;

    std::unique_ptr<PerformanceMonitor> m_performanceMonitor;
    std::unique_ptr<LowPowerModeNotifier> m_lowPowerModeNotifier;
    OptionSet<ThrottlingReason> m_throttlingReasons;
    OptionSet<ThrottlingReason> m_throttlingReasonsOverridenForTesting;

    std::optional<Navigation> m_navigationToLogWhenVisible;

    std::unique_ptr<PerformanceLogging> m_performanceLogging;
#if ENABLE(WHEEL_EVENT_LATCHING)
    std::unique_ptr<ScrollLatchingController> m_scrollLatchingController;
#endif
#if PLATFORM(MAC) && (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION))
    std::unique_ptr<ServicesOverlayController> m_servicesOverlayController;
#endif
    std::unique_ptr<ImageOverlayController> m_imageOverlayController;

#if ENABLE(IMAGE_ANALYSIS)
    std::unique_ptr<ImageAnalysisQueue> m_imageAnalysisQueue;
#endif

    std::unique_ptr<WheelEventDeltaFilter> m_recentWheelEventDeltaFilter;
    std::unique_ptr<PageOverlayController> m_pageOverlayController;

#if ENABLE(APPLE_PAY)
    std::unique_ptr<PaymentCoordinator> m_paymentCoordinator;
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    RefPtr<ApplePayAMSUIPaymentHandler> m_activeApplePayAMSUIPaymentHandler;
#endif

#if ENABLE(WEB_AUTHN)
    UniqueRef<AuthenticatorCoordinator> m_authenticatorCoordinator;
#endif

#if ENABLE(APPLICATION_MANIFEST)
    std::optional<ApplicationManifest> m_applicationManifest;
#endif

    std::optional<ViewportArguments> m_overrideViewportArguments;

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    RefPtr<DeviceOrientationUpdateProvider> m_deviceOrientationUpdateProvider;
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    RefPtr<MediaSessionCoordinatorPrivate> m_mediaSessionCoordinator;
#endif

    Vector<UserContentURLPattern> m_corsDisablingPatterns;
    HashSet<String> m_maskedURLSchemes;
    Vector<UserStyleSheet> m_userStyleSheetsPendingInjection;
    std::optional<MemoryCompactLookupOnlyRobinHoodHashSet<String>> m_allowedNetworkHosts;
    bool m_isTakingSnapshotsForApplicationSuspension { false };
    bool m_loadsSubresources { true };
    bool m_canUseCredentialStorage { true };
    ShouldRelaxThirdPartyCookieBlocking m_shouldRelaxThirdPartyCookieBlocking { ShouldRelaxThirdPartyCookieBlocking::No };
    LoadSchedulingMode m_loadSchedulingMode { LoadSchedulingMode::Direct };
    bool m_hasBeenNotifiedToInjectUserScripts { false };
    bool m_isServiceWorkerPage { false };

    MonotonicTime m_lastRenderingUpdateTimestamp;

    Color m_underPageBackgroundColorOverride;
    std::optional<Color> m_sampledPageTopColor;

    const bool m_httpsUpgradeEnabled { true };
    mutable MediaSessionGroupIdentifier m_mediaSessionGroupIdentifier;

    UniqueRef<StorageProvider> m_storageProvider;
    UniqueRef<ModelPlayerProvider> m_modelPlayerProvider;

    WeakPtr<KeyboardScrollingAnimator> m_currentKeyboardScrollingAnimator;

#if ENABLE(ATTACHMENT_ELEMENT)
    std::unique_ptr<AttachmentElementClient> m_attachmentElementClient;
#endif

#if ENABLE(IMAGE_ANALYSIS)
    using CachedTextRecognitionResult = std::pair<TextRecognitionResult, IntRect>;
    WeakHashMap<HTMLElement, CachedTextRecognitionResult, WeakPtrImplWithEventTargetData> m_textRecognitionResults;
#endif

#if USE(ATSPI)
    AccessibilityRootAtspi* m_accessibilityRootObject { nullptr };
#endif

    ContentSecurityPolicyModeForExtension m_contentSecurityPolicyModeForExtension { ContentSecurityPolicyModeForExtension::None };

    Ref<BadgeClient> m_badgeClient;
};

inline PageGroup& Page::group()
{
    if (!m_group)
        initGroup();
    return *m_group;
}

WTF::TextStream& operator<<(WTF::TextStream&, RenderingUpdateStep);

} // namespace WebCore
