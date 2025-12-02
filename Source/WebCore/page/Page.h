/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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

#include <WebCore/ActivityState.h>
#include <WebCore/AnimationFrameRate.h>
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/BoxExtents.h>
#include <WebCore/Color.h>
#include <WebCore/DocumentEnums.h>
#include <WebCore/FindOptions.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/IntRectHash.h>
#include <WebCore/LoadSchedulingMode.h>
#include <WebCore/MediaSessionGroupIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/Pagination.h>
#include <WebCore/PlaybackTargetClientContextIdentifier.h>
#include <WebCore/ProcessSwapDisposition.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/ScriptTrackingPrivacyCategory.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/Supplementable.h>
#include <WebCore/Timer.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <memory>
#include <pal/SessionID.h>
#include <wtf/Assertions.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>
#include <wtf/Platform.h>
#include <wtf/ProcessID.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URLHash.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(APPLICATION_MANIFEST)
#include <WebCore/ApplicationManifest.h>
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
#include <WebCore/ShouldRequireExplicitConsentForGamepadAccess.h>
#endif

namespace JSC {
class Debugger;
class JSGlobalObject;
}

namespace PAL {
class HysteresisActivity;
}

namespace WTF {
class SchedulePair;
class TextStream;
struct SchedulePairHash;
using SchedulePairHashSet = HashSet<RefPtr<SchedulePair>, SchedulePairHash>;
}

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
}

class AXObjectCache;
class AccessibilityRootAtspi;
class ApplePayAMSUIPaymentHandler;
class ActivityStateChangeObserver;
class AlternativeTextClient;
class AnimationTimelinesController;
class AttachmentElementClient;
class AuthenticatorCoordinator;
class BackForwardController;
class BadgeClient;
class BroadcastChannelRegistry;
class CacheStorageProvider;
class CaptionDisplaySettingsClient;
class Chrome;
class CompositeEditCommand;
class ContextMenuController;
class CookieJar;
class CryptoClient;
class Document;
class DOMRectList;
class DOMWrapperWorld;
class DatabaseProvider;
class DeviceOrientationUpdateProvider;
class DiagnosticLoggingClient;
class DocumentSyncData;
class CredentialRequestCoordinator;
class DragCaretController;
class DragController;
class EditorClient;
class EditCommandComposition;
class ElementTargetingController;
class Element;
class FocusController;
class FormData;
class Frame;
class GraphicsContext;
class HTMLElement;
class HTMLImageElement;
class HTMLMediaElement;
class HistoryItem;
class HistoryItemClient;
class OpportunisticTaskScheduler;
class ImageAnalysisQueue;
class ImageOverlayController;
class InspectorBackendClient;
class IntSize;
class KeyboardScrollingAnimator;
class LayoutRect;
class LocalFrame;
class LoginStatus;
class LowPowerModeNotifier;
class MediaCanStartListener;
class MediaPlaybackTarget;
class MediaSessionCoordinatorPrivate;
class MediaSessionManagerInterface;
class ModelPlayerProvider;
class PageConfiguration;
class PageDebuggable;
class PageGroup;
class PageInspectorController;
class PageOverlayController;
class PaymentCoordinator;
class PerformanceLogging;
class PerformanceLoggingClient;
class PerformanceMonitor;
class PluginData;
class PluginInfoProvider;
class PointerCaptureController;
class PointerLockController;
class DocumentSyncClient;
class ProgressTracker;
class RTCController;
class RenderObject;
class ResourceUsageOverlay;
class RenderingUpdateScheduler;
class SVGImageElement;
class ScreenOrientationManager;
class ScrollLatchingController;
class ScrollingCoordinator;
class ServicesOverlayController;
class ServiceWorkerGlobalScope;
class Settings;
class SocketProvider;
class SpeechRecognitionProvider;
class SpeechSynthesisClient;
class SpeechRecognitionConnection;
class StorageConnection;
class StorageNamespace;
class StorageNamespaceProvider;
class StorageProvider;
class StringCallback;
class TextIndicator;
class ThermalMitigationNotifier;
class UserContentProvider;
class UserContentURLPattern;
class UserScript;
class UserStyleSheet;
class ValidatedFormListedElement;
class ValidationMessageClient;
class VisibleSelection;
class VisitedLinkStore;
class WeakPtrImplWithEventTargetData;
class WebRTCProvider;
class WheelEventDeltaFilter;
class WheelEventTestMonitor;
class WindowEventLoop;

#if ENABLE(WRITING_TOOLS)
class WritingToolsController;

namespace WritingTools {
enum class Action : uint8_t;
enum class TextSuggestionState : uint8_t;

struct Context;
struct TextSuggestion;
struct Session;

using TextSuggestionID = WTF::UUID;
using SessionID = WTF::UUID;
}
#endif

#if ENABLE(WEBXR)
class WebXRSession;
#endif

struct AXTreeData;
struct ApplePayAMSUIRequest;
struct AttributedString;
struct CharacterRange;
struct ClientOrigin;
struct DocumentSyncSerializationData;
struct FixedContainerEdges;
struct NavigationAPIMethodTracker;
struct ResolvedCaptionDisplaySettingsOptions;
struct SpatialBackdropSource;
struct SystemPreviewInfo;
struct TextRecognitionResult;
struct ViewportArguments;
struct WindowFeatures;

using PlatformDisplayID = uint32_t;
using SharedStringHash = uint32_t;

enum class ActivityState : uint16_t;
enum class AdvancedPrivacyProtections : uint16_t;
enum class BoxSide : uint8_t;
enum class CanWrap : bool;
enum class ContentSecurityPolicyModeForExtension : uint8_t;
enum class DidWrap : bool;
enum class DisabledAdaptations : uint8_t;
enum class DocumentClass : uint16_t;
enum class EventTrackingRegionsEventType : uint8_t;
enum class FindOption : uint16_t;
enum class FilterRenderingMode : uint8_t;
enum class LayoutMilestone : uint16_t;
enum class LoginStatusAuthenticationType : uint8_t;
enum class PlatformMediaSessionPlaybackControlsPurpose : uint8_t;
enum class MediaPlaybackTargetMockState : uint8_t;
enum class MediaProducerMediaState : uint32_t;
enum class MediaProducerMediaCaptureKind : uint8_t;
enum class MediaProducerMutedState : uint8_t;
enum class RouteSharingPolicy : uint8_t;
enum class ShouldRelaxThirdPartyCookieBlocking : bool;
enum class ShouldTreatAsContinuingLoad : uint8_t;
enum class VisibilityState : bool;

#if ENABLE(DOM_AUDIO_SESSION)
enum class DOMAudioSessionType : uint8_t;
#endif

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
    Reveal                              = 1 << 0,
    Resize                              = 1 << 1,
    Scroll                              = 1 << 2,
    MediaQueryEvaluation                = 1 << 3,
    Animations                          = 1 << 4,
    Fullscreen                          = 1 << 5,
    AnimationFrameCallbacks             = 1 << 6,
    UpdateContentRelevancy              = 1 << 7,
    PerformPendingViewTransitions       = 1 << 8,
    IntersectionObservations            = 1 << 9,
    ResizeObservations                  = 1 << 10,
    PaintTiming                         = 1 << 11,
    EventTiming                         = 1 << 12,
    Images                              = 1 << 13,
    WheelEventMonitorCallbacks          = 1 << 14,
    CursorUpdate                        = 1 << 15,
    EventRegionUpdate                   = 1 << 16,
    LayerFlush                          = 1 << 17,
#if ENABLE(ASYNC_SCROLLING)
    ScrollingTreeUpdate                 = 1 << 18,
#endif
    FlushAutofocusCandidates            = 1 << 19,
    VideoFrameCallbacks                 = 1 << 20,
    PrepareCanvasesForDisplayOrFlush    = 1 << 21,
    CaretAnimation                      = 1 << 22,
    FocusFixup                          = 1 << 23,
    UpdateValidationMessagePositions    = 1 << 25,
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    AccessibilityRegionUpdate           = 1 << 26,
#endif
    RestoreScrollPositionAndViewState   = 1 << 27,
    AdjustVisibility                    = 1 << 28,
    SnapshottedScrollOffsets            = 1 << 29,
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    Immersive                           = 1 << 30,
#endif

};

enum class LinkDecorationFilteringTrigger : uint8_t {
    Unspecified,
    Navigation,
    Copy,
    Paste,
};

// For accessibility tree debugging.
enum class IncludeDOMInfo : bool { No, Yes };

constexpr OptionSet<RenderingUpdateStep> updateRenderingSteps = {
    RenderingUpdateStep::Reveal,
    RenderingUpdateStep::FlushAutofocusCandidates,
    RenderingUpdateStep::Resize,
    RenderingUpdateStep::Scroll,
    RenderingUpdateStep::MediaQueryEvaluation,
    RenderingUpdateStep::Animations,
    RenderingUpdateStep::Fullscreen,
    RenderingUpdateStep::AnimationFrameCallbacks,
    RenderingUpdateStep::IntersectionObservations,
    RenderingUpdateStep::ResizeObservations,
    RenderingUpdateStep::PaintTiming,
    RenderingUpdateStep::EventTiming,
    RenderingUpdateStep::Images,
    RenderingUpdateStep::WheelEventMonitorCallbacks,
    RenderingUpdateStep::CursorUpdate,
    RenderingUpdateStep::EventRegionUpdate,
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    RenderingUpdateStep::AccessibilityRegionUpdate,
#endif
    RenderingUpdateStep::PrepareCanvasesForDisplayOrFlush,
    RenderingUpdateStep::CaretAnimation,
    RenderingUpdateStep::UpdateContentRelevancy,
    RenderingUpdateStep::PerformPendingViewTransitions,
    RenderingUpdateStep::AdjustVisibility,
};

constexpr auto allRenderingUpdateSteps = updateRenderingSteps | OptionSet<RenderingUpdateStep> {
    RenderingUpdateStep::LayerFlush,
#if ENABLE(ASYNC_SCROLLING)
    RenderingUpdateStep::ScrollingTreeUpdate,
#endif
};

using WeakElementEdges = RectEdges<WeakPtr<Element, WeakPtrImplWithEventTargetData>>;

class Page : public RefCountedAndCanMakeWeakPtr<Page>, public Supplementable<Page> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(Page, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(Page);
    friend class SettingsBase;

public:
    WEBCORE_EXPORT static Ref<Page> create(PageConfiguration&&);
    WEBCORE_EXPORT ~Page();

    static void updateControlTintsForAllPages();
    WEBCORE_EXPORT static void updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    WEBCORE_EXPORT static void clearPreviousItemFromAllPages(BackForwardItemIdentifier);

    WEBCORE_EXPORT void setupForRemoteWorker(const URL& scriptURL, const SecurityOriginData& topOrigin, const String& referrerPolicy, OptionSet<AdvancedPrivacyProtections>);

    WEBCORE_EXPORT void updateStyleAfterChangeInEnvironment();

    // Utility pages (e.g. SVG image pages) don't have an identifier currently.
    std::optional<PageIdentifier> identifier() const { return m_identifier; }

    WEBCORE_EXPORT uint64_t renderTreeSize() const;
    WEBCORE_EXPORT void destroyRenderTrees();

    WEBCORE_EXPORT void setNeedsRecalcStyleInAllFrames();

    WEBCORE_EXPORT OptionSet<DisabledAdaptations> disabledAdaptations() const;
    WEBCORE_EXPORT ViewportArguments viewportArguments() const;

    WEBCORE_EXPORT void reloadExecutionContextsForOrigin(const ClientOrigin&, std::optional<FrameIdentifier> triggeringFrame) const;

    const ViewportArguments* overrideViewportArguments() const { return m_overrideViewportArguments.get(); }
    WEBCORE_EXPORT void setOverrideViewportArguments(const std::optional<ViewportArguments>&);

    static void refreshPlugins(bool reload);
    WEBCORE_EXPORT PluginData& pluginData();
    WEBCORE_EXPORT Ref<PluginData> protectedPluginData();
    void clearPluginData();

    OpportunisticTaskScheduler& opportunisticTaskScheduler() const { return m_opportunisticTaskScheduler.get(); }
    Ref<OpportunisticTaskScheduler> protectedOpportunisticTaskScheduler() const;

    WEBCORE_EXPORT void setCanStartMedia(bool);
    bool canStartMedia() const { return m_canStartMedia; }

    EditorClient& editorClient() { return m_editorClient.get(); }

    WEBCORE_EXPORT RefPtr<LocalFrame> localMainFrame();
    WEBCORE_EXPORT RefPtr<const LocalFrame> localMainFrame() const;
    WEBCORE_EXPORT RefPtr<Document> localTopDocument();
    WEBCORE_EXPORT RefPtr<Document> localTopDocument() const;

    Frame& mainFrame() { return m_mainFrame.get(); }
    const Frame& mainFrame() const { return m_mainFrame.get(); }
    WEBCORE_EXPORT Ref<Frame> protectedMainFrame() const;
    WEBCORE_EXPORT void setMainFrame(Ref<Frame>&&);
    WEBCORE_EXPORT const URL& mainFrameURL() const;
    SecurityOrigin& mainFrameOrigin() const;
    Ref<SecurityOrigin> protectedMainFrameOrigin() const;

    WEBCORE_EXPORT void setMainFrameURLAndOrigin(const URL&, RefPtr<SecurityOrigin>&&);
#if ENABLE(DOM_AUDIO_SESSION)
    void setAudioSessionType(DOMAudioSessionType);
    DOMAudioSessionType audioSessionType() const;
#endif
    void setUserDidInteractWithPage(bool);
    bool userDidInteractWithPage() const;
    void setAutofocusProcessed();
    bool autofocusProcessed() const;
    bool topDocumentHasDocumentClass(DocumentClass) const;

    bool hasInjectedUserScript();
    WEBCORE_EXPORT void setHasInjectedUserScript();

    WEBCORE_EXPORT void updateTopDocumentSyncData(const DocumentSyncSerializationData&);
    WEBCORE_EXPORT void updateTopDocumentSyncData(Ref<DocumentSyncData>&&);

    WEBCORE_EXPORT void setMainFrameURLFragment(String&&);
    String mainFrameURLFragment() const { return m_mainFrameURLFragment; }

    bool openedByDOM() const;
    WEBCORE_EXPORT void setOpenedByDOM();

    bool openedByDOMWithOpener() const { return m_openedByDOMWithOpener; }
    void setOpenedByDOMWithOpener(bool value) { m_openedByDOMWithOpener = value; }

    const RegistrableDomain& openedByScriptDomain() const { return m_openedByScriptDomain; }
    void setOpenedByScriptDomain(RegistrableDomain&& domain) { m_openedByScriptDomain = WTFMove(domain); }

    WEBCORE_EXPORT void goToItem(LocalFrame& rootFrame, HistoryItem&, FrameLoadType, ShouldTreatAsContinuingLoad, ProcessSwapDisposition processSwapDisposition = ProcessSwapDisposition::None);
    void goToItemForNavigationAPI(LocalFrame& rootFrame, HistoryItem&, FrameLoadType, LocalFrame& triggeringFrame, NavigationAPIMethodTracker*);

    WEBCORE_EXPORT void setGroupName(const String&);
    WEBCORE_EXPORT const String& groupName() const;

    WEBCORE_EXPORT PageGroup& group();
    WEBCORE_EXPORT CheckedRef<PageGroup> checkedGroup();

    BroadcastChannelRegistry& broadcastChannelRegistry() { return m_broadcastChannelRegistry; }
    WEBCORE_EXPORT Ref<BroadcastChannelRegistry> protectedBroadcastChannelRegistry() const;
    WEBCORE_EXPORT void setBroadcastChannelRegistry(Ref<BroadcastChannelRegistry>&&); // Only used by WebKitLegacy.

    WEBCORE_EXPORT static void forEachPage(NOESCAPE const Function<void(Page&)>&);
    WEBCORE_EXPORT static unsigned nonUtilityPageCount();
    static Page* fromPageIdentifier(PageIdentifier);

    unsigned subframeCount() const;

    void setCurrentKeyboardScrollingAnimator(KeyboardScrollingAnimator*);
    KeyboardScrollingAnimator* currentKeyboardScrollingAnimator() const; // Deinfed in PageInlines.h

    bool shouldApplyScreenFingerprintingProtections(Document&) const;

    OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections() const;

#if ENABLE(REMOTE_INSPECTOR)
    WEBCORE_EXPORT bool inspectable() const;
    WEBCORE_EXPORT void setInspectable(bool);
    WEBCORE_EXPORT String remoteInspectionNameOverride() const;
    WEBCORE_EXPORT void setRemoteInspectionNameOverride(const String&);
    void remoteInspectorInformationDidChange();
#endif

    Chrome& chrome() { return m_chrome.get(); }
    const Chrome& chrome() const { return m_chrome.get(); }
    CryptoClient& cryptoClient() { return m_cryptoClient.get(); }
    const CryptoClient& cryptoClient() const { return m_cryptoClient.get(); }
    DocumentSyncClient& documentSyncClient() { return m_documentSyncClient.get(); }
    const DocumentSyncClient& documentSyncClient() const { return m_documentSyncClient.get(); }
    DragCaretController& dragCaretController() { return m_dragCaretController.get(); }
    const DragCaretController& dragCaretController() const { return m_dragCaretController.get(); }
#if ENABLE(DRAG_SUPPORT)
    DragController& dragController() { return m_dragController.get(); }
    const DragController& dragController() const { return m_dragController.get(); }
#endif
    FocusController& focusController() const { return m_focusController; }
#if ENABLE(CONTEXT_MENUS)
    ContextMenuController& contextMenuController() { return m_contextMenuController.get(); }
    const ContextMenuController& contextMenuController() const { return m_contextMenuController.get(); }
#endif
    PageInspectorController& inspectorController() { return m_inspectorController.get(); }
    WEBCORE_EXPORT Ref<PageInspectorController> protectedInspectorController();
    PointerCaptureController& pointerCaptureController() { return m_pointerCaptureController.get(); }
#if ENABLE(POINTER_LOCK)
    PointerLockController& pointerLockController() { return m_pointerLockController.get(); }
#endif
    WebRTCProvider& webRTCProvider() { return m_webRTCProvider.get(); }
    RTCController& rtcController() { return m_rtcController.get(); }
    WEBCORE_EXPORT void disableICECandidateFiltering();
    WEBCORE_EXPORT void enableICECandidateFiltering();
    bool shouldEnableICECandidateFilteringByDefault() const { return m_shouldEnableICECandidateFilteringByDefault; }

    WEBCORE_EXPORT CheckedRef<ElementTargetingController> checkedElementTargetingController();

    void didChangeMainDocument(Document* newDocument);
    void mainFrameDidChangeToNonInitialEmptyDocument();

    PerformanceMonitor* performanceMonitor() { return m_performanceMonitor.get(); }

    ValidationMessageClient* validationMessageClient() const { return m_validationMessageClient.get(); }
    void updateValidationBubbleStateIfNeeded();
    void scheduleValidationMessageUpdate(ValidatedFormListedElement&, HTMLElement&);

    WEBCORE_EXPORT ScrollingCoordinator* scrollingCoordinator();
    WEBCORE_EXPORT RefPtr<ScrollingCoordinator> protectedScrollingCoordinator();

    WEBCORE_EXPORT String scrollingStateTreeAsText();
    WEBCORE_EXPORT String synchronousScrollingReasonsAsText();
    WEBCORE_EXPORT Ref<DOMRectList> nonFastScrollableRectsForTesting();

    WEBCORE_EXPORT Ref<DOMRectList> touchEventRectsForEventForTesting(EventTrackingRegionsEventType);
    WEBCORE_EXPORT Ref<DOMRectList> passiveTouchEventListenerRectsForTesting();

    WEBCORE_EXPORT void setConsoleMessageListenerForTesting(RefPtr<StringCallback>&&);
    WEBCORE_EXPORT RefPtr<StringCallback> consoleMessageListenerForTesting() const;

    WEBCORE_EXPORT void settingsDidChange();

    Settings& settings() const { return *m_settings; }

    ProgressTracker& progress() { return m_progress.get(); }
    const ProgressTracker& progress() const { return m_progress.get(); }
    CheckedRef<ProgressTracker> checkedProgress();
    CheckedRef<const ProgressTracker> checkedProgress() const;

    WEBCORE_EXPORT void applyWindowFeatures(const WindowFeatures&);

    void progressEstimateChanged(LocalFrame&) const;
    void progressFinished(LocalFrame&) const;
    BackForwardController& backForward() { return m_backForwardController.get(); }
    WEBCORE_EXPORT CheckedRef<BackForwardController> checkedBackForward();

    Seconds domTimerAlignmentInterval() const { return m_domTimerAlignmentInterval; }

    void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
    bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

    struct FindStringData {
        std::optional<FrameIdentifier> frameIdentifier;
        std::optional<SimpleRange> range;
    };
    WEBCORE_EXPORT FindStringData findString(const String&, FindOptions, DidWrap* = nullptr);
    WEBCORE_EXPORT uint32_t replaceRangesWithText(const Vector<SimpleRange>& rangesToReplace, const String& replacementText, bool selectionOnly);
    WEBCORE_EXPORT uint32_t replaceSelectionWithText(const String& replacementText);

    WEBCORE_EXPORT void revealCurrentSelection();

    WEBCORE_EXPORT const URL fragmentDirectiveURLForSelectedText();

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
    WEBCORE_EXPORT void addSchedulePair(Ref<WTF::SchedulePair>&&);
    WEBCORE_EXPORT void removeSchedulePair(Ref<WTF::SchedulePair>&&);
    WTF::SchedulePairHashSet* scheduledRunLoopPairs() { return m_scheduledRunLoopPairs.get(); }

    std::unique_ptr<WTF::SchedulePairHashSet> m_scheduledRunLoopPairs;
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

    void willChangeLocationInCompletelyLoadedSubframe();

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

    WEBCORE_EXPORT void screenPropertiesDidChange(bool affectsStyle = true);
    void windowScreenDidChange(PlatformDisplayID, std::optional<FramesPerSecond> nominalFramesPerSecond);
    PlatformDisplayID displayID() const { return m_displayID; }
    std::optional<FramesPerSecond> displayNominalFramesPerSecond() const { return m_displayNominalFramesPerSecond; }

    // This can return nullopt if throttling reasons result in a frequency less than one, in which case
    // preferredRenderingUpdateInterval provides the frequency.
    // FIXME: Have a single function that returns a Variant<>.
    enum class PreferredRenderingUpdateOption : uint8_t {
        IncludeThrottlingReasons    = 1 << 0,
        IncludeAnimationsFrameRate  = 1 << 1
    };
    static constexpr OptionSet<PreferredRenderingUpdateOption> allPreferredRenderingUpdateOptions = { PreferredRenderingUpdateOption::IncludeThrottlingReasons, PreferredRenderingUpdateOption::IncludeAnimationsFrameRate };
    WEBCORE_EXPORT std::optional<FramesPerSecond> preferredRenderingUpdateFramesPerSecond(OptionSet<PreferredRenderingUpdateOption> = allPreferredRenderingUpdateOptions) const;
    WEBCORE_EXPORT Seconds preferredRenderingUpdateInterval() const;

    const FloatBoxExtent& contentInsets() const { return m_contentInsets; }
    void setContentInsets(const FloatBoxExtent& insets) { m_contentInsets = insets; }

    const FloatBoxExtent& unobscuredSafeAreaInsets() const { return m_unobscuredSafeAreaInsets; }
    WEBCORE_EXPORT void setUnobscuredSafeAreaInsets(const FloatBoxExtent&);

#if PLATFORM(IOS_FAMILY)
    bool enclosedInScrollableAncestorView() const { return m_enclosedInScrollableAncestorView; }
    void setEnclosedInScrollableAncestorView(bool f) { m_enclosedInScrollableAncestorView = f; }

    const FloatBoxExtent& obscuredInsets() const { return m_obscuredInsets; }
    WEBCORE_EXPORT void setObscuredInsets(const FloatBoxExtent&);
#endif

    const FloatBoxExtent& obscuredContentInsets() const { return m_obscuredContentInsets; }
    WEBCORE_EXPORT void setObscuredContentInsets(const FloatBoxExtent&);

    WEBCORE_EXPORT void useSystemAppearanceChanged();

    WEBCORE_EXPORT bool useDarkAppearance() const;
    bool useElevatedUserInterfaceLevel() const { return m_useElevatedUserInterfaceLevel; }
    WEBCORE_EXPORT void setUseColorAppearance(bool useDarkAppearance, bool useElevatedUserInterfaceLevel);
    bool defaultUseDarkAppearance() const { return m_useDarkAppearance; }
    void setUseDarkAppearanceOverride(std::optional<bool>);

#if ENABLE(TEXT_AUTOSIZING)
    float textAutosizingWidth() const { return m_textAutosizingWidth; }
    void setTextAutosizingWidth(float textAutosizingWidth) { m_textAutosizingWidth = textAutosizingWidth; }
    WEBCORE_EXPORT void recomputeTextAutoSizingInAllFrames();
#endif

    OptionSet<FilterRenderingMode> preferredFilterRenderingModes(const GraphicsContext&) const;

    const FloatBoxExtent& fullscreenInsets() const { return m_fullscreenInsets; }
    WEBCORE_EXPORT void setFullscreenInsets(const FloatBoxExtent&);

    const Seconds fullscreenAutoHideDuration() const { return m_fullscreenAutoHideDuration; }
    WEBCORE_EXPORT void setFullscreenAutoHideDuration(Seconds);

    Document* outermostFullscreenDocument() const;

    bool shouldSuppressScrollbarAnimations() const { return m_suppressScrollbarAnimations; }
    WEBCORE_EXPORT void setShouldSuppressScrollbarAnimations(bool suppressAnimations);
    void lockAllOverlayScrollbarsToHidden(bool lockOverlayScrollbars);

    WEBCORE_EXPORT void setVerticalScrollElasticity(ScrollElasticity);
    ScrollElasticity verticalScrollElasticity() const { return static_cast<ScrollElasticity>(m_verticalScrollElasticity); }

    WEBCORE_EXPORT void setHorizontalScrollElasticity(ScrollElasticity);
    ScrollElasticity horizontalScrollElasticity() const { return static_cast<ScrollElasticity>(m_horizontalScrollElasticity); }

    WEBCORE_EXPORT void accessibilitySettingsDidChange();
    WEBCORE_EXPORT void appearanceDidChange();

    void clearAXObjectCache();
    AXObjectCache* existingAXObjectCache();
    WEBCORE_EXPORT AXObjectCache* axObjectCache();

    // Page and FrameView both store a Pagination value. Page::pagination() is set only by API,
    // and FrameView::pagination() is set only by CSS. Page::pagination() will affect all
    // FrameViews in the back/forward cache, but FrameView::pagination() only affects the current
    // FrameView.
    const Pagination& pagination() const { return m_pagination; }
    WEBCORE_EXPORT void setPagination(const Pagination&);

    WEBCORE_EXPORT unsigned pageCount() const;
    WEBCORE_EXPORT unsigned pageCountAssumingLayoutIsUpToDate() const;

    WEBCORE_EXPORT DiagnosticLoggingClient& diagnosticLoggingClient() const;
    WEBCORE_EXPORT CheckedRef<DiagnosticLoggingClient> checkedDiagnosticLoggingClient() const;

    WEBCORE_EXPORT void logMediaDiagnosticMessage(const RefPtr<FormData>&) const;

    PerformanceLoggingClient* performanceLoggingClient() const { return m_performanceLoggingClient.get(); }

    WheelEventDeltaFilter* wheelEventDeltaFilter() { return m_recentWheelEventDeltaFilter.get(); }
    PageOverlayController& pageOverlayController() { return m_pageOverlayController; }

#if PLATFORM(MAC) && (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION))
    ServicesOverlayController& servicesOverlayController() { return m_servicesOverlayController.get(); }
    Ref<ServicesOverlayController> protectedServicesOverlayController();
#endif
    ImageOverlayController& imageOverlayController();
    Ref<ImageOverlayController> protectedImageOverlayController();
    ImageOverlayController* imageOverlayControllerIfExists() { return m_imageOverlayController.get(); }

#if ENABLE(IMAGE_ANALYSIS)
    WEBCORE_EXPORT ImageAnalysisQueue& imageAnalysisQueue();
    WEBCORE_EXPORT Ref<ImageAnalysisQueue> protectedImageAnalysisQueue();
    ImageAnalysisQueue* imageAnalysisQueueIfExists() { return m_imageAnalysisQueue.get(); }
#endif

#if ENABLE(WHEEL_EVENT_LATCHING)
    ScrollLatchingController& scrollLatchingController();
    Ref<ScrollLatchingController> protectedScrollLatchingController();
    ScrollLatchingController* scrollLatchingControllerIfExists() { return m_scrollLatchingController.get(); }
#endif // ENABLE(WHEEL_EVENT_LATCHING)

#if ENABLE(APPLE_PAY)
    PaymentCoordinator& paymentCoordinator() const { return *m_paymentCoordinator; }
    WEBCORE_EXPORT Ref<PaymentCoordinator> protectedPaymentCoordinator() const;
    WEBCORE_EXPORT void setPaymentCoordinator(Ref<PaymentCoordinator>&&);
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    bool hasActiveApplePayAMSUISession() const { return m_activeApplePayAMSUIPaymentHandler; }
    bool startApplePayAMSUISession(const URL&, ApplePayAMSUIPaymentHandler&, const ApplePayAMSUIRequest&);
    void abortApplePayAMSUISession(ApplePayAMSUIPaymentHandler&);
#endif

#if USE(SYSTEM_PREVIEW)
    void beginSystemPreview(const URL&, const SecurityOriginData& topOrigin, const SystemPreviewInfo&, CompletionHandler<void()>&&);
#endif

#if ENABLE(WEB_AUTHN)
    AuthenticatorCoordinator& authenticatorCoordinator() { return m_authenticatorCoordinator.get(); }
#if HAVE(DIGITAL_CREDENTIALS_UI)
    CredentialRequestCoordinator& credentialRequestCoordinator() { return m_credentialRequestCoordinator.get(); }
#endif
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

    // Service worker pages have an associated ServiceWorkerGlobalScope on the main thread.
    void setServiceWorkerGlobalScope(ServiceWorkerGlobalScope&);
    WEBCORE_EXPORT JSC::JSGlobalObject* serviceWorkerGlobalObject(DOMWrapperWorld&);

    // Notifications when the Page starts and stops being presented via a native window.
    WEBCORE_EXPORT void setActivityState(OptionSet<ActivityState>);
    OptionSet<ActivityState> activityState() const { return m_activityState; }

    bool isWindowActive() const;
    WEBCORE_EXPORT bool isVisibleAndActive() const;
    WEBCORE_EXPORT void setIsVisible(bool);
    WEBCORE_EXPORT void setIsPrerender();
    bool isVisible() const { return m_activityState.contains(ActivityState::IsVisible); }

    // Notification that this Page was moved into or out of a native window.
    WEBCORE_EXPORT void setIsInWindow(bool);
    bool isInWindow() const { return m_activityState.contains(ActivityState::IsInWindow); }

    void setIsClosing();
    bool isClosing() const;

    void setIsRestoringCachedPage(bool value) { m_isRestoringCachedPage = value; }
    bool isRestoringCachedPage() const { return m_isRestoringCachedPage; }

    WEBCORE_EXPORT void addActivityStateChangeObserver(ActivityStateChangeObserver&);
    WEBCORE_EXPORT void removeActivityStateChangeObserver(ActivityStateChangeObserver&);

    WEBCORE_EXPORT void layoutIfNeeded(OptionSet<LayoutOptions> = { });
    WEBCORE_EXPORT void updateRendering();
    // A call to updateRendering() that is not followed by a call to finalizeRenderingUpdate().
    WEBCORE_EXPORT void isolatedUpdateRendering();
    // Called when the rendering update steps are complete, but before painting.
    WEBCORE_EXPORT void finalizeRenderingUpdate(OptionSet<FinalizeRenderingUpdateFlags>);
    WEBCORE_EXPORT void finalizeRenderingUpdateForRootFrame(LocalFrame&, OptionSet<FinalizeRenderingUpdateFlags>);

    // Called before and after the "display" steps of the rendering update: painting, and when we push
    // layers to the platform compositor (including async painting).
    WEBCORE_EXPORT void willStartRenderingUpdateDisplay();
    WEBCORE_EXPORT void didCompleteRenderingUpdateDisplay();
    // Called after didCompleteRenderingUpdateDisplay, but in the same run loop iteration (i.e. before zero-delay timers triggered from the rendering update).
    WEBCORE_EXPORT void didCompleteRenderingFrame();
    // Called after the "display" steps of the rendering update, but before any async delays
    // waiting for async painting.
    WEBCORE_EXPORT void didUpdateRendering();

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
    void updatePlayStateForAllAnimations();
    WEBCORE_EXPORT void setImageAnimationEnabled(bool);
    void addIndividuallyPlayingAnimationElement(HTMLImageElement&);
    void removeIndividuallyPlayingAnimationElement(HTMLImageElement&);
#endif
    bool imageAnimationEnabled() const { return m_imageAnimationEnabled; }

#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
    WEBCORE_EXPORT void setPrefersNonBlinkingCursor(bool);
    bool prefersNonBlinkingCursor() const { return m_prefersNonBlinkingCursor; };
#endif

    void userStyleSheetLocationChanged();
    const String& userStyleSheet() const;

    WEBCORE_EXPORT void userAgentChanged();

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
    WEBCORE_EXPORT void addCORSDisablingPatternForTesting(UserContentURLPattern&&);

    WEBCORE_EXPORT void setMemoryCacheClientCallsEnabled(bool);
    bool areMemoryCacheClientCallsEnabled() const { return m_areMemoryCacheClientCallsEnabled; }
    void setHasPendingMemoryCacheLoadNotifications(bool hasPendingMemoryCacheLoadNotifications) { m_hasPendingMemoryCacheLoadNotifications = hasPendingMemoryCacheLoadNotifications; }

    // Don't allow more than a certain number of frames in a page.
    // This seems like a reasonable upper bound, and otherwise mutually
    // recursive frameset pages can quickly bring the program to its knees
    // with exponential growth in the number of frames.
    static constexpr int maxNumberOfFrames = 1000;

    // Don't allow more than a certain frame depth to avoid stack exhaustion.
    static constexpr int maxFrameDepth = 32;

    WEBCORE_EXPORT void setEditable(bool);
    bool isEditable() const { return m_isEditable; }

    WEBCORE_EXPORT VisibilityState visibilityState() const;
    WEBCORE_EXPORT void resumeAnimatingImages();

    void didFinishLoadingImageForElement(HTMLImageElement&);
    void didFinishLoadingImageForSVGImage(SVGImageElement&);

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

    WEBCORE_EXPORT void updateFixedContainerEdges(EnumSet<BoxSide>);
    const FixedContainerEdges& fixedContainerEdges() const { return m_fixedContainerEdgesAndElements.first; }
    Element* lastFixedContainer(BoxSide) const;

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
    WEBCORE_EXPORT std::optional<SpatialBackdropSource> spatialBackdropSource() const;
#endif

#if HAVE(APP_ACCENT_COLORS) && PLATFORM(MAC)
    WEBCORE_EXPORT void setAppUsesCustomAccentColor(bool);
    WEBCORE_EXPORT bool appUsesCustomAccentColor() const;
#endif

    Color underPageBackgroundColorOverride() const { return m_underPageBackgroundColorOverride; }
    WEBCORE_EXPORT void setUnderPageBackgroundColorOverride(Color&&);

    bool isCountingRelevantRepaintedObjects() const;
    void setIsCountingRelevantRepaintedObjects(bool isCounting) { m_isCountingRelevantRepaintedObjects = isCounting; }
    void startCountingRelevantRepaintedObjects();
    void resetRelevantPaintedObjectCounter();
    void addRelevantRepaintedObject(const RenderObject&, const LayoutRect& objectPaintRect);
    void addRelevantUnpaintedObject(const RenderObject&, const LayoutRect& objectPaintRect);

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

#if ENABLE(REMOTE_INSPECTOR)
    PageDebuggable& inspectorDebuggable() { return m_inspectorDebuggable.get(); }
    const PageDebuggable& inspectorDebuggable() const { return m_inspectorDebuggable.get(); }
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

    DatabaseProvider& databaseProvider() { return m_databaseProvider; }
    CacheStorageProvider& cacheStorageProvider() { return m_cacheStorageProvider; }
    SocketProvider& socketProvider() { return m_socketProvider; }
    CookieJar& cookieJar() { return m_cookieJar.get(); }
    WEBCORE_EXPORT Ref<CookieJar> protectedCookieJar() const;

    StorageNamespaceProvider& storageNamespaceProvider() { return m_storageNamespaceProvider.get(); }
    WEBCORE_EXPORT Ref<StorageNamespaceProvider> protectedStorageNamespaceProvider() const;

    PluginInfoProvider& pluginInfoProvider();
    Ref<PluginInfoProvider> protectedPluginInfoProvider() const;

    WEBCORE_EXPORT Ref<UserContentProvider> protectedUserContentProviderForFrame();
    WEBCORE_EXPORT void setUserContentProviderForWebKitLegacy(Ref<UserContentProvider>&&);

    ScreenOrientationManager* screenOrientationManager() const;

    VisitedLinkStore& visitedLinkStore();
    Ref<VisitedLinkStore> protectedVisitedLinkStore();
    WEBCORE_EXPORT void setVisitedLinkStore(Ref<VisitedLinkStore>&&);

    std::optional<uint64_t> noiseInjectionHashSaltForDomain(const RegistrableDomain&);

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
    void mediaEngineChanged(HTMLMediaElement&);
#endif
    WEBCORE_EXPORT void setMuted(MediaProducerMutedStateFlags);
    bool shouldSuppressHDR() const { return m_shouldSuppressHDR; }
    WEBCORE_EXPORT void setShouldSuppressHDR(bool);

    WEBCORE_EXPORT void stopMediaCapture(MediaProducerMediaCaptureKind);
#if ENABLE(MEDIA_STREAM)
    WEBCORE_EXPORT void updateCaptureState(bool isActive, MediaProducerMediaCaptureKind);
    WEBCORE_EXPORT void voiceActivityDetected();
#endif

#if ENABLE(MODEL_PROCESS)
    void incrementModelElementCount();
    void decrementModelElementCount(unsigned);
#endif

    std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const;
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
    WEBCORE_EXPORT void setMockMediaPlaybackTargetPickerState(const String&, MediaPlaybackTargetMockState);
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

#if ENABLE(SPEECH_SYNTHESIS)
    SpeechSynthesisClient* speechSynthesisClient() const { return m_speechSynthesisClient.get(); }
#endif

    WEBCORE_EXPORT SpeechRecognitionConnection& speechRecognitionConnection();

    bool isOnlyNonUtilityPage() const;
    bool isUtilityPage() const { return m_isUtilityPage; }

    WEBCORE_EXPORT bool allowsLoadFromURL(const URL&, MainFrameMainResource) const;
    WEBCORE_EXPORT bool hasLocalDataForURL(const URL&);

    ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking() const { return m_shouldRelaxThirdPartyCookieBlocking; }

    bool isLowPowerModeEnabled() const { return m_throttlingReasons.contains(ThrottlingReason::LowPowerMode); }
    bool isThermalMitigationEnabled() const { return m_throttlingReasons.contains(ThrottlingReason::ThermalMitigation); }
    bool isAggressiveThermalMitigationEnabled() const { return m_throttlingReasons.contains(ThrottlingReason::AggressiveThermalMitigation); }
    bool canUpdateThrottlingReason(ThrottlingReason reason) const { return !m_throttlingReasonsOverridenForTesting.contains(reason); }
    WEBCORE_EXPORT void setLowPowerModeEnabledOverrideForTesting(std::optional<bool>);
    WEBCORE_EXPORT void setAggressiveThermalMitigationEnabledForTesting(std::optional<bool>);
    WEBCORE_EXPORT void setOutsideViewportThrottlingEnabledForTesting(bool);

    OptionSet<ThrottlingReason> throttlingReasons() const { return m_throttlingReasons; }

    WEBCORE_EXPORT void applicationWillResignActive();
    WEBCORE_EXPORT void applicationDidEnterBackground();
    WEBCORE_EXPORT void applicationWillEnterForeground();
    WEBCORE_EXPORT void applicationDidBecomeActive();

    PerformanceLogging& performanceLogging() const { return m_performanceLogging; }

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

    WEBCORE_EXPORT void forEachDocument(NOESCAPE const Function<void(Document&)>&) const;
    bool findMatchingLocalDocument(NOESCAPE const Function<bool(Document&)>&) const;
    void forEachRenderableDocument(NOESCAPE const Function<void(Document&)>&) const;
    void forEachMediaElement(NOESCAPE const Function<void(HTMLMediaElement&)>&);
    static void forEachDocumentFromMainFrame(const Frame&, NOESCAPE const Function<void(Document&)>&);
    void forEachLocalFrame(NOESCAPE const Function<void(LocalFrame&)>&);
    void forEachWindowEventLoop(NOESCAPE const Function<void(WindowEventLoop&)>&);

    bool shouldDisableCorsForRequestTo(const URL&) const;
    bool shouldAssumeSameSiteForRequestTo(const URL& url) const { return shouldDisableCorsForRequestTo(url); }
    const HashSet<String>& maskedURLSchemes() const { return m_maskedURLSchemes; }

    WEBCORE_EXPORT void injectUserStyleSheet(UserStyleSheet&);
    WEBCORE_EXPORT void removeInjectedUserStyleSheet(UserStyleSheet&);

    bool isTakingSnapshotsForApplicationSuspension() const { return m_isTakingSnapshotsForApplicationSuspension; }
    void setIsTakingSnapshotsForApplicationSuspension(bool isTakingSnapshotsForApplicationSuspension) { m_isTakingSnapshotsForApplicationSuspension = isTakingSnapshotsForApplicationSuspension; }

    MonotonicTime lastRenderingUpdateTimestamp() const { return m_lastRenderingUpdateTimestamp; }
    std::optional<MonotonicTime> nextRenderingUpdateTimestamp() const;

    bool httpsUpgradeEnabled() const { return m_httpsUpgradeEnabled; }

    WEBCORE_EXPORT URL applyLinkDecorationFiltering(const URL&, LinkDecorationFilteringTrigger) const;
    String applyLinkDecorationFiltering(const String&, LinkDecorationFilteringTrigger) const;
    URL allowedQueryParametersForAdvancedPrivacyProtections(const URL&) const;

    LoadSchedulingMode loadSchedulingMode() const { return m_loadSchedulingMode; }
    void setLoadSchedulingMode(LoadSchedulingMode);

#if ENABLE(IMAGE_ANALYSIS)
    std::optional<TextRecognitionResult> cachedTextRecognitionResult(const HTMLElement&) const;
    WEBCORE_EXPORT bool hasCachedTextRecognitionResult(const HTMLElement&) const;
    void cacheTextRecognitionResult(const HTMLElement&, const IntRect& containerRect, const TextRecognitionResult&);
    void resetTextRecognitionResult(const HTMLElement&);
    void resetImageAnalysisQueue();
#endif

    bool hasEverSetVisibilityAdjustment() const { return m_hasEverSetVisibilityAdjustment; }
    void didSetVisibilityAdjustment() { m_hasEverSetVisibilityAdjustment = true; }

    WEBCORE_EXPORT StorageConnection& storageConnection();

    ModelPlayerProvider& modelPlayerProvider();

    void updateScreenSupportedContentsFormats();

#if ENABLE(ATTACHMENT_ELEMENT)
    AttachmentElementClient* attachmentElementClient() { return m_attachmentElementClient.get(); }
#endif

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    bool shouldUpdateAccessibilityRegions() const;
#endif
    WEBCORE_EXPORT std::optional<AXTreeData> accessibilityTreeData(IncludeDOMInfo) const;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    WEBCORE_EXPORT void clearAccessibilityIsolatedTree();
#endif
#if USE(ATSPI)
    AccessibilityRootAtspi* accessibilityRootObject() const;
    void setAccessibilityRootObject(AccessibilityRootAtspi*);
#endif

    void timelineControllerMaximumAnimationFrameRateDidChange(AnimationTimelinesController&);

    ContentSecurityPolicyModeForExtension contentSecurityPolicyModeForExtension() const { return m_contentSecurityPolicyModeForExtension; }

    WEBCORE_EXPORT void forceRepaintAllFrames();

#if ENABLE(IMAGE_ANALYSIS)
    WEBCORE_EXPORT void analyzeImagesForFindInPage(Function<void()>&& callback);
#endif

    BadgeClient& badgeClient() { return m_badgeClient.get(); }
    HistoryItemClient& historyItemClient() { return m_historyItemClient.get(); }

    void willBeginScrolling();
    void didFinishScrolling();

    const HashSet<WeakRef<LocalFrame>>& rootFrames() const { return m_rootFrames; }
    WEBCORE_EXPORT void addRootFrame(LocalFrame&);
    WEBCORE_EXPORT void removeRootFrame(LocalFrame&);

    void opportunisticallyRunIdleCallbacks(MonotonicTime deadline);
    WEBCORE_EXPORT void performOpportunisticallyScheduledTasks(MonotonicTime deadline);
    void deleteRemovedNodesAndDetachedRenderers();
    String ensureMediaKeysStorageDirectoryForOrigin(const SecurityOriginData&);
    WEBCORE_EXPORT void setMediaKeysStorageDirectory(const String&);

    bool isWaitingForLoadToFinish() const { return m_isWaitingForLoadToFinish; }

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT void setSceneIdentifier(String&&);
#endif
    WEBCORE_EXPORT const String& sceneIdentifier() const;

    std::optional<std::pair<uint16_t, uint16_t>> portsForUpgradingInsecureSchemeForTesting() const;
    WEBCORE_EXPORT void setPortsForUpgradingInsecureSchemeForTesting(uint16_t upgradeFromInsecurePort, uint16_t upgradeToSecurePort);

#if PLATFORM(IOS_FAMILY) && ENABLE(WEBXR)
    WEBCORE_EXPORT bool hasActiveImmersiveSession() const;
#endif

    void setIsInSwipeAnimation(bool inSwipeAnimation) { m_inSwipeAnimation = inSwipeAnimation; }
    bool isInSwipeAnimation() const { return m_inSwipeAnimation; }

#if HAVE(SPATIAL_TRACKING_LABEL)
    WEBCORE_EXPORT void setDefaultSpatialTrackingLabel(const String&);
    String defaultSpatialTrackingLabel() const { return m_defaultSpatialTrackingLabel; }
#endif

#if ENABLE(GAMEPAD)
    void gamepadsRecentlyAccessed();
#if PLATFORM(VISION)
    WEBCORE_EXPORT void allowGamepadAccess();
    bool gamepadAccessGranted() const { return m_gamepadAccessGranted; }
#endif
#endif

#if ENABLE(WRITING_TOOLS)
    WEBCORE_EXPORT void willBeginWritingToolsSession(const std::optional<WritingTools::Session>&, CompletionHandler<void(const Vector<WritingTools::Context>&)>&&);

    WEBCORE_EXPORT void didBeginWritingToolsSession(const WritingTools::Session&, const Vector<WritingTools::Context>&);

    WEBCORE_EXPORT void proofreadingSessionDidReceiveSuggestions(const WritingTools::Session&, const Vector<WritingTools::TextSuggestion>&, const CharacterRange&, const WritingTools::Context&, bool finished);

    WEBCORE_EXPORT void proofreadingSessionDidUpdateStateForSuggestion(const WritingTools::Session&, WritingTools::TextSuggestionState, const WritingTools::TextSuggestion&, const WritingTools::Context&);

    WEBCORE_EXPORT void willEndWritingToolsSession(const WritingTools::Session&, bool accepted);

    WEBCORE_EXPORT void didEndWritingToolsSession(const WritingTools::Session&, bool accepted);

    WEBCORE_EXPORT void compositionSessionDidReceiveTextWithReplacementRange(const WritingTools::Session&, const AttributedString&, const CharacterRange&, const WritingTools::Context&, bool finished);

    WEBCORE_EXPORT void writingToolsSessionDidReceiveAction(const WritingTools::Session&, WritingTools::Action);

    WEBCORE_EXPORT void updateStateForSelectedSuggestionIfNeeded();

    void respondToUnappliedWritingToolsEditing(EditCommandComposition*);
    void respondToReappliedWritingToolsEditing(EditCommandComposition*);

    WEBCORE_EXPORT Vector<FloatRect> proofreadingSessionSuggestionTextRectsInRootViewCoordinates(const CharacterRange&) const;
    WEBCORE_EXPORT void updateTextVisibilityForActiveWritingToolsSession(const CharacterRange&, bool, const WTF::UUID&);
    WEBCORE_EXPORT RefPtr<TextIndicator> textPreviewDataForActiveWritingToolsSession(const CharacterRange&);
    WEBCORE_EXPORT void decorateTextReplacementsForActiveWritingToolsSession(const CharacterRange&);
    WEBCORE_EXPORT void setSelectionForActiveWritingToolsSession(const CharacterRange&);

    WEBCORE_EXPORT std::optional<SimpleRange> contextRangeForActiveWritingToolsSession() const;
    WEBCORE_EXPORT void intelligenceTextAnimationsDidComplete();
#endif

    bool hasActiveNowPlayingSession() const { return m_hasActiveNowPlayingSession; }
    void hasActiveNowPlayingSessionChanged();
    void updateActiveNowPlayingSessionNow();

#if PLATFORM(IOS_FAMILY)
    bool canShowWhileLocked() const { return m_canShowWhileLocked; }
#endif

    void setLastAuthentication(LoginStatusAuthenticationType);
    const LoginStatus* lastAuthentication() const { return m_lastAuthentication.get(); }

#if ENABLE(FULLSCREEN_API)
    WEBCORE_EXPORT bool isDocumentFullscreenEnabled() const;
#endif

    bool shouldDeferResizeEvents() const { return m_shouldDeferResizeEvents; }
    WEBCORE_EXPORT void startDeferringResizeEvents();
    WEBCORE_EXPORT void flushDeferredResizeEvents();

    bool shouldDeferScrollEvents() const { return m_shouldDeferScrollEvents; }
    WEBCORE_EXPORT void startDeferringScrollEvents();
    WEBCORE_EXPORT void flushDeferredScrollEvents();

    bool shouldDeferIntersectionObservations() const { return m_shouldDeferIntersectionObservations; }
    WEBCORE_EXPORT void startDeferringIntersectionObservations();
    WEBCORE_EXPORT void flushDeferredIntersectionObservations();

    bool reportScriptTrackingPrivacy(const URL&, ScriptTrackingPrivacyCategory);
    bool shouldAllowScriptAccess(const URL&, const SecurityOrigin& topOrigin, ScriptTrackingPrivacyCategory) const;
    bool requiresScriptTrackingPrivacyProtections(const URL&) const;

    WEBCORE_EXPORT bool isAlwaysOnLoggingAllowed() const;

    ProcessID presentingApplicationPID() const;

#if HAVE(AUDIT_TOKEN)
    const std::optional<audit_token_t>& presentingApplicationAuditToken() const;
    WEBCORE_EXPORT void setPresentingApplicationAuditToken(std::optional<audit_token_t>);
#endif

#if PLATFORM(COCOA)
    const String& presentingApplicationBundleIdentifier() const;
    WEBCORE_EXPORT void setPresentingApplicationBundleIdentifier(String&&);
#endif

    WEBCORE_EXPORT RefPtr<HTMLMediaElement> bestMediaElementForRemoteControls(PlatformMediaSessionPlaybackControlsPurpose, Document*);

    WEBCORE_EXPORT RefPtr<MediaSessionManagerInterface> mediaSessionManager();
    WEBCORE_EXPORT MediaSessionManagerInterface* mediaSessionManagerIfExists() const;
    WEBCORE_EXPORT static RefPtr<MediaSessionManagerInterface> mediaSessionManagerForPageIdentifier(PageIdentifier);

#if ENABLE(MODEL_ELEMENT)
    bool shouldDisableModelLoadDelaysForTesting() const { return m_modelLoadDelaysDisabledForTesting; }
    void disableModelLoadDelaysForTesting() { m_modelLoadDelaysDisabledForTesting = true; }
#endif

    bool requiresUserGestureForAudioPlayback() const;
    bool requiresUserGestureForVideoPlayback() const;

#if HAVE(SUPPORT_HDR_DISPLAY)
    WEBCORE_EXPORT bool drawsHDRContent() const;
    Headroom displayEDRHeadroom() const { return m_displayEDRHeadroom; }
    bool hdrLayersRequireTonemapping() const { return m_hdrLayersRequireTonemapping; }
    void updateDisplayEDRHeadroom();
    void updateDisplayEDRSuppression();
#endif

#if PLATFORM(IOS_FAMILY)
    using HardwareKeyboardAttachmentObserver = Function<void(bool)>;
    void addHardwareKeyboardAttachmentObserver(HardwareKeyboardAttachmentObserver&&);

    WEBCORE_EXPORT void didUpdateHardwareKeyboardAttachment(bool);

    WEBCORE_EXPORT void clearIsShowingInputView();
#endif

#if ENABLE(VIDEO)
    WEBCORE_EXPORT void setCaptionDisplaySettingsClientForTesting(Ref<CaptionDisplaySettingsClient>&&);
    WEBCORE_EXPORT void clearCaptionDisplaySettingsClientForTesting();
    void showCaptionDisplaySettings(HTMLMediaElement&, const ResolvedCaptionDisplaySettingsOptions&, CompletionHandler<void(ExceptionOr<void>)>&&);
#endif

private:
    explicit Page(PageConfiguration&&);

    void updateValidationMessages();

    struct Navigation {
        RegistrableDomain domain;
        FrameLoadType type;
    };
    void logNavigation(const Navigation&);

    static void firstTimeInitialization();

    void initGroup();

    void setIsInWindowInternal(bool);
    void setIsVisibleInternal(bool);
    void setIsVisuallyIdleInternal(bool);

    void stopKeyboardScrollAnimation();

    Ref<DocumentSyncData> protectedTopDocumentSyncData() const;

    enum ShouldHighlightMatches { DoNotHighlightMatches, HighlightMatches };
    enum ShouldMarkMatches { DoNotMarkMatches, MarkMatches };

    unsigned findMatchesForText(const String&, FindOptions, unsigned maxMatchCount, ShouldHighlightMatches, ShouldMarkMatches);

    std::optional<std::pair<WeakRef<MediaCanStartListener>, WeakRef<Document, WeakPtrImplWithEventTargetData>>> takeAnyMediaCanStartListener();

#if ENABLE(VIDEO)
    void playbackControlsManagerUpdateTimerFired();
#endif

    void handleLowPowerModeChange(bool);
    void handleThermalMitigationChange(bool);

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
    CheckedRef<RenderingUpdateScheduler> checkedRenderingUpdateScheduler();
    RenderingUpdateScheduler* existingRenderingUpdateScheduler();

    WheelEventTestMonitor& ensureWheelEventTestMonitor();
    Ref<WheelEventTestMonitor> ensureProtectedWheelEventTestMonitor();

#if ENABLE(IMAGE_ANALYSIS)
    void resetTextRecognitionResults();
    void updateElementsWithTextRecognitionResults();
#endif

#if ENABLE(WEBXR)
    RefPtr<WebXRSession> activeImmersiveXRSession() const;
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
    void initializeGamepadAccessForPageLoad();
#endif

    void computeSampledPageTopColorIfNecessary();
    void clearSampledPageTopColor();

    bool hasLocalMainFrame();

    void updateControlTints();

    struct Internals;
    const UniqueRef<Internals> m_internals;

    std::optional<PageIdentifier> m_identifier;
    const UniqueRef<Chrome> m_chrome;
    const UniqueRef<DragCaretController> m_dragCaretController;

#if ENABLE(DRAG_SUPPORT)
    const UniqueRef<DragController> m_dragController;
#endif
    const UniqueRef<FocusController> m_focusController;
#if ENABLE(CONTEXT_MENUS)
    const UniqueRef<ContextMenuController> m_contextMenuController;
#endif
    const UniqueRef<PageInspectorController> m_inspectorController;
    const UniqueRef<PointerCaptureController> m_pointerCaptureController;
#if ENABLE(POINTER_LOCK)
    const UniqueRef<PointerLockController> m_pointerLockController;
#endif
    const UniqueRef<ElementTargetingController> m_elementTargetingController;
    RefPtr<ScrollingCoordinator> m_scrollingCoordinator;

    const RefPtr<Settings> m_settings;
    const UniqueRef<CryptoClient> m_cryptoClient;
    const UniqueRef<ProgressTracker> m_progress;
    const UniqueRef<DocumentSyncClient> m_documentSyncClient;

    const UniqueRef<BackForwardController> m_backForwardController;
    HashSet<WeakRef<LocalFrame>> m_rootFrames;
    const UniqueRef<EditorClient> m_editorClient;
    Ref<Frame> m_mainFrame;
    String m_mainFrameURLFragment;

    RefPtr<PluginData> m_pluginData;

    std::unique_ptr<ValidationMessageClient> m_validationMessageClient;
    Vector<std::pair<Ref<ValidatedFormListedElement>, WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>>> m_validationMessageUpdates;
    std::unique_ptr<DiagnosticLoggingClient> m_diagnosticLoggingClient;
    std::unique_ptr<PerformanceLoggingClient> m_performanceLoggingClient;

#if ENABLE(SPEECH_SYNTHESIS)
    const RefPtr<SpeechSynthesisClient> m_speechSynthesisClient;
#endif

    const UniqueRef<SpeechRecognitionProvider> m_speechRecognitionProvider;

    const UniqueRef<WebRTCProvider> m_webRTCProvider;
    const Ref<RTCController> m_rtcController;

    PlatformDisplayID m_displayID { 0 };
    std::optional<FramesPerSecond> m_displayNominalFramesPerSecond;

    String m_groupName;
    bool m_openedByDOM { false };
    bool m_openedByDOMWithOpener { false };

    bool m_tabKeyCyclesThroughElements { true };
    bool m_defersLoading { false };
    unsigned m_defersLoadingCallCount { 0 };

    bool m_inLowQualityInterpolationMode { false };
    bool m_areMemoryCacheClientCallsEnabled { true };
    bool m_hasPendingMemoryCacheLoadNotifications { false };
    float m_mediaVolume { 1 };
    MediaProducerMutedStateFlags m_mutedState;
    bool m_shouldSuppressHDR { false };

    float m_pageScaleFactor { 1 };
    float m_zoomedOutPageScaleFactor { 0 };
    float m_deviceScaleFactor { 1 };
    float m_viewScaleFactor { 1 };

    FloatBoxExtent m_obscuredContentInsets;
    FloatBoxExtent m_contentInsets;
    FloatBoxExtent m_unobscuredSafeAreaInsets;
    FloatBoxExtent m_fullscreenInsets;
    Seconds m_fullscreenAutoHideDuration { 0_s };

#if PLATFORM(IOS_FAMILY)
    FloatBoxExtent m_obscuredInsets;
    bool m_enclosedInScrollableAncestorView { false };
    bool m_canShowWhileLocked { false };
#endif

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
    mutable Markable<WallTime> m_userStyleSheetModificationTime;

    String m_captionUserPreferencesStyleSheet;

    std::unique_ptr<PageGroup> m_singlePageGroup;
    WeakPtr<PageGroup> m_group;

    JSC::Debugger* m_debugger { nullptr }; // FIXME: Use a smart pointer.

    bool m_canStartMedia { true };
    bool m_imageAnimationEnabled { true };
    // Elements containing animations that are individually playing (potentially overriding the page-wide m_imageAnimationEnabled state).
    WeakHashSet<HTMLImageElement, WeakPtrImplWithEventTargetData> m_individuallyPlayingAnimationElements;
#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
    bool m_prefersNonBlinkingCursor { false };
#endif

    TimerThrottlingState m_timerThrottlingState { TimerThrottlingState::Disabled };
    MonotonicTime m_timerThrottlingStateLastChangedTime;
    Seconds m_domTimerAlignmentInterval;
    Timer m_domTimerAlignmentIntervalIncreaseTimer;
    Seconds m_domTimerAlignmentIntervalIncreaseLimit;

    bool m_isEditable { false };
    bool m_isPrerender { false };
    OptionSet<ActivityState> m_activityState;

    OptionSet<LayoutMilestone> m_requestedLayoutMilestones;

    int m_headerHeight { 0 };
    int m_footerHeight { 0 };

    std::unique_ptr<RenderingUpdateScheduler> m_renderingUpdateScheduler;
    SingleThreadWeakHashSet<const RenderObject> m_relevantUnpaintedRenderObjects;

    bool m_isCountingRelevantRepaintedObjects { false };
#ifndef NDEBUG
    bool m_isPainting { false };
#endif
    std::unique_ptr<AlternativeTextClient> m_alternativeTextClient;

    bool m_scriptedAnimationsSuspended { false };

#if ENABLE(REMOTE_INSPECTOR)
    const Ref<PageDebuggable> m_inspectorDebuggable;
#endif

    RefPtr<IDBClient::IDBConnectionToServer> m_idbConnectionToServer;

    MemoryCompactRobinHoodHashSet<String> m_seenPlugins;
    MemoryCompactRobinHoodHashSet<String> m_seenMediaEngines;

    unsigned m_lastSpatialNavigationCandidatesCount { 0 };
    unsigned m_forbidPromptsDepth { 0 };
    unsigned m_forbidSynchronousLoadsDepth { 0 };

    const Ref<SocketProvider> m_socketProvider;
    const Ref<CookieJar> m_cookieJar;
    const Ref<CacheStorageProvider> m_cacheStorageProvider;
    const Ref<DatabaseProvider> m_databaseProvider;
    const Ref<PluginInfoProvider> m_pluginInfoProvider;
    const Ref<StorageNamespaceProvider> m_storageNamespaceProvider;
    Ref<UserContentProvider> m_userContentProvider;
    WeakPtr<ScreenOrientationManager> m_screenOrientationManager;
    Ref<VisitedLinkStore> m_visitedLinkStore;
    Ref<BroadcastChannelRegistry> m_broadcastChannelRegistry;
    RefPtr<WheelEventTestMonitor> m_wheelEventTestMonitor;
    WeakHashSet<ActivityStateChangeObserver> m_activityStateChangeObservers;
    WeakPtr<ServiceWorkerGlobalScope, WeakPtrImplWithEventTargetData> m_serviceWorkerGlobalScope;

#if ENABLE(RESOURCE_USAGE)
    RefPtr<ResourceUsageOverlay> m_resourceUsageOverlay;
#endif

    PAL::SessionID m_sessionID;

    unsigned m_renderingUpdateCount { 0 };
    bool m_isTrackingRenderingUpdates { false };

    bool m_isRestoringCachedPage { false };

    MediaProducerMediaStateFlags m_mediaState;

#if ENABLE(VIDEO)
    Timer m_playbackControlsManagerUpdateTimer;
#endif

#if ENABLE(MODEL_PROCESS)
    unsigned m_modelElementCount { 0 };
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

    bool m_hasEverSetVisibilityAdjustment { false };

#if ENABLE(EDITABLE_REGION)
    bool m_isEditableRegionEnabled { false };
#endif

    bool m_inSwipeAnimation { false };

    Vector<OptionSet<RenderingUpdateStep>, 2> m_renderingUpdateRemainingSteps;
    OptionSet<RenderingUpdateStep> m_unfulfilledRequestedSteps;
    
    UserInterfaceLayoutDirection m_userInterfaceLayoutDirection { UserInterfaceLayoutDirection::LTR };
    
    // For testing.
    std::optional<EventThrottlingBehavior> m_eventThrottlingBehaviorOverride;
    std::optional<CompositingPolicy> m_compositingPolicyOverride;

    const std::unique_ptr<PerformanceMonitor> m_performanceMonitor;
    const UniqueRef<LowPowerModeNotifier> m_lowPowerModeNotifier;
    const UniqueRef<ThermalMitigationNotifier> m_thermalMitigationNotifier;
    OptionSet<ThrottlingReason> m_throttlingReasons;
    OptionSet<ThrottlingReason> m_throttlingReasonsOverridenForTesting;

    std::optional<Navigation> m_navigationToLogWhenVisible;

    const UniqueRef<PerformanceLogging> m_performanceLogging;
#if ENABLE(WHEEL_EVENT_LATCHING)
    const std::unique_ptr<ScrollLatchingController> m_scrollLatchingController;
#endif
#if PLATFORM(MAC) && (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION))
    const UniqueRef<ServicesOverlayController> m_servicesOverlayController;
#endif
    std::unique_ptr<ImageOverlayController> m_imageOverlayController;

#if ENABLE(IMAGE_ANALYSIS)
    RefPtr<ImageAnalysisQueue> m_imageAnalysisQueue;
#endif

    std::unique_ptr<WheelEventDeltaFilter> m_recentWheelEventDeltaFilter;
    const UniqueRef<PageOverlayController> m_pageOverlayController;

#if ENABLE(APPLE_PAY)
    RefPtr<PaymentCoordinator> m_paymentCoordinator;
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    RefPtr<ApplePayAMSUIPaymentHandler> m_activeApplePayAMSUIPaymentHandler;
#endif

#if ENABLE(WEB_AUTHN)
    const UniqueRef<AuthenticatorCoordinator> m_authenticatorCoordinator;

#if HAVE(DIGITAL_CREDENTIALS_UI)
    const Ref<CredentialRequestCoordinator> m_credentialRequestCoordinator;
#endif

#endif // ENABLE(WEB_AUTHN)

#if ENABLE(APPLICATION_MANIFEST)
    std::optional<ApplicationManifest> m_applicationManifest;
#endif

    std::unique_ptr<ViewportArguments> m_overrideViewportArguments;

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    RefPtr<DeviceOrientationUpdateProvider> m_deviceOrientationUpdateProvider;
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    RefPtr<MediaSessionCoordinatorPrivate> m_mediaSessionCoordinator;
#endif

    Vector<UserContentURLPattern> m_corsDisablingPatterns;
    const HashSet<String> m_maskedURLSchemes;
    Vector<UserStyleSheet> m_userStyleSheetsPendingInjection;
    const std::optional<MemoryCompactLookupOnlyRobinHoodHashSet<String>> m_allowedNetworkHosts;
    bool m_isTakingSnapshotsForApplicationSuspension { false };
    bool m_loadsSubresources { true };
    bool m_canUseCredentialStorage { true };
    ShouldRelaxThirdPartyCookieBlocking m_shouldRelaxThirdPartyCookieBlocking;
    LoadSchedulingMode m_loadSchedulingMode { LoadSchedulingMode::Direct };
    bool m_isServiceWorkerPage { false };

    MonotonicTime m_lastRenderingUpdateTimestamp;
    bool m_renderingUpdateIsScheduled { false };
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    MonotonicTime m_lastAccessibilityObjectRegionsUpdate;
#endif

    Color m_underPageBackgroundColorOverride;
    std::optional<Color> m_sampledPageTopColor;
    std::pair<UniqueRef<FixedContainerEdges>, WeakElementEdges> m_fixedContainerEdgesAndElements;
    bool m_userHasInteractedSinceLastPageLoad { false };

    const bool m_httpsUpgradeEnabled { true };
    mutable Markable<MediaSessionGroupIdentifier> m_mediaSessionGroupIdentifier;

    std::optional<std::pair<uint16_t, uint16_t>> m_portsForUpgradingInsecureSchemeForTesting;

    RefPtr<StringCallback> m_consoleMessageListenerForTesting;

    const UniqueRef<StorageProvider> m_storageProvider;
    const Ref<ModelPlayerProvider> m_modelPlayerProvider;

    WeakPtr<KeyboardScrollingAnimator> m_currentKeyboardScrollingAnimator;

#if ENABLE(ATTACHMENT_ELEMENT)
    std::unique_ptr<AttachmentElementClient> m_attachmentElementClient;
#endif

    bool m_isWaitingForLoadToFinish { false };
    const Ref<OpportunisticTaskScheduler> m_opportunisticTaskScheduler;

#if ENABLE(IMAGE_ANALYSIS)
    using CachedTextRecognitionResult = std::pair<TextRecognitionResult, IntRect>;
    WeakHashMap<HTMLElement, CachedTextRecognitionResult, WeakPtrImplWithEventTargetData> m_textRecognitionResults;
#endif

#if USE(ATSPI)
    WeakPtr<AccessibilityRootAtspi> m_accessibilityRootObject;
#endif

    ContentSecurityPolicyModeForExtension m_contentSecurityPolicyModeForExtension;

    const Ref<BadgeClient> m_badgeClient;
    const Ref<HistoryItemClient> m_historyItemClient;

    HashMap<RegistrableDomain, uint64_t> m_noiseInjectionHashSalts;

#if PLATFORM(IOS_FAMILY)
    String m_sceneIdentifier;
#endif

#if HAVE(APP_ACCENT_COLORS) && PLATFORM(MAC)
    bool m_appUsesCustomAccentColor { false };
#endif

#if HAVE(SPATIAL_TRACKING_LABEL)
    String m_defaultSpatialTrackingLabel;
#endif

#if ENABLE(GAMEPAD)
    MonotonicTime m_lastAccessNotificationTime;
#if PLATFORM(VISION)
    bool m_gamepadAccessGranted { true };
    ShouldRequireExplicitConsentForGamepadAccess m_gamepadAccessRequiresExplicitConsent { ShouldRequireExplicitConsentForGamepadAccess::No };
#endif
#endif

#if ENABLE(WRITING_TOOLS)
    const UniqueRef<WritingToolsController> m_writingToolsController;
#endif

#if HAVE(SUPPORT_HDR_DISPLAY)
    Headroom m_displayEDRHeadroom { Headroom::None };
    bool m_screenSupportsHDR { false };
    bool m_hdrLayersRequireTonemapping { false };
#endif

    HashSet<std::pair<URL, ScriptTrackingPrivacyCategory>> m_scriptTrackingPrivacyReports;

    bool m_hasActiveNowPlayingSession { false };
    Timer m_activeNowPlayingSessionUpdateTimer;

    std::unique_ptr<LoginStatus> m_lastAuthentication;

    bool m_shouldDeferResizeEvents { false };
    bool m_shouldDeferScrollEvents { false };
    bool m_shouldDeferIntersectionObservations { false };

    Ref<DocumentSyncData> m_topDocumentSyncData;

    RegistrableDomain m_openedByScriptDomain;

#if HAVE(AUDIT_TOKEN)
    std::optional<audit_token_t> m_presentingApplicationAuditToken;
#endif

#if PLATFORM(COCOA)
    String m_presentingApplicationBundleIdentifier;
#endif

    using MediaSessionManagerFactory = Function<RefPtr<MediaSessionManagerInterface> (PageIdentifier)>;
    std::optional<MediaSessionManagerFactory> m_mediaSessionManagerFactory;
    RefPtr<MediaSessionManagerInterface> m_mediaSessionManager;

#if ENABLE(MODEL_ELEMENT)
    bool m_modelLoadDelaysDisabledForTesting { false };
#endif

    // Checking hardware keyboard attached
#if PLATFORM(IOS_FAMILY)
    bool m_hardwareKeyboardAttached { false };
    Vector<HardwareKeyboardAttachmentObserver> m_hardwareKeyboardAttachmentObservers;
    void flushHardwareKeyboardAttachmentObservers();
#endif

#if ENABLE(VIDEO)
    RefPtr<CaptionDisplaySettingsClient> m_captionDisplaySettingsClientForTesting;
#endif
}; // class Page

WTF::TextStream& operator<<(WTF::TextStream&, RenderingUpdateStep);

} // namespace WebCore
