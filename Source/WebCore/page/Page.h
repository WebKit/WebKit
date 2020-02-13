/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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
#include "DisabledAdaptations.h"
#include "Document.h"
#include "FindOptions.h"
#include "FrameLoaderTypes.h"
#include "LayoutMilestone.h"
#include "LayoutRect.h"
#include "LengthBox.h"
#include "MediaProducer.h"
#include "Pagination.h"
#include "RTCController.h"
#include "Region.h"
#include "RegistrableDomain.h"
#include "RenderingUpdateScheduler.h"
#include "ScrollTypes.h"
#include "Supplementable.h"
#include "Timer.h"
#include "UserInterfaceLayoutDirection.h"
#include "ViewportArguments.h"
#include "VisibilityState.h"
#include "WheelEventTestMonitor.h"
#include <memory>
#include <pal/SessionID.h>
#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/SchedulePair.h>
#endif

#if ENABLE(APPLICATION_MANIFEST)
#include "ApplicationManifest.h"
#endif

#if ENABLE(MEDIA_SESSION)
#include "MediaSessionEvents.h"
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

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
}

class ActivityStateChangeObserver;
class AlternativeTextClient;
class ApplicationCacheStorage;
class AuthenticatorCoordinator;
class BackForwardController;
class CacheStorageProvider;
class Chrome;
class Color;
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
class Frame;
class HTMLMediaElement;
class HistoryItem;
class InspectorClient;
class InspectorController;
class LibWebRTCProvider;
class LowPowerModeNotifier;
class MediaCanStartListener;
class MediaPlaybackTarget;
class MediaRecorderProvider;
class PageConfiguration;
class PageConsoleClient;
class PageDebuggable;
class PageGroup;
class PageOverlayController;
class PaymentCoordinator;
class PerformanceLogging;
class PerformanceLoggingClient;
class PerformanceMonitor;
class PlugInClient;
class PluginData;
class PluginInfoProvider;
class PluginViewBase;
class PointerCaptureController;
class PointerLockController;
class ProgressTracker;
class Range;
class RenderObject;
class ResourceUsageOverlay;
class ScrollLatchingState;
class ScrollingCoordinator;
class ServicesOverlayController;
class Settings;
class SocketProvider;
class SpeechSynthesisClient;
class StorageNamespace;
class StorageNamespaceProvider;
class UserContentProvider;
class UserContentURLPattern;
class UserInputBridge;
class ValidationMessageClient;
class VisibleSelection;
class VisitedLinkStore;
class WebGLStateTracker;
class WheelEventDeltaFilter;

using SharedStringHash = uint32_t;

enum class CanWrap : bool;
enum class DidWrap : bool;
enum class RouteSharingPolicy : uint8_t;
enum class ShouldTreatAsContinuingLoad : bool;

enum class EventThrottlingBehavior : bool { Responsive, Unresponsive };

enum class CompositingPolicy : bool {
    Normal,
    Conservative, // Used in low memory situations.
};

class Page : public Supplementable<Page>, public CanMakeWeakPtr<Page> {
    WTF_MAKE_NONCOPYABLE(Page);
    WTF_MAKE_FAST_ALLOCATED;
    friend class SettingsBase;

public:
    WEBCORE_EXPORT static void updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    WEBCORE_EXPORT static void clearPreviousItemFromAllPages(HistoryItem*);

    void updateStyleAfterChangeInEnvironment();

    WEBCORE_EXPORT explicit Page(PageConfiguration&&);
    WEBCORE_EXPORT ~Page();

    WEBCORE_EXPORT uint64_t renderTreeSize() const;

    WEBCORE_EXPORT void setNeedsRecalcStyleInAllFrames();

    WEBCORE_EXPORT OptionSet<DisabledAdaptations> disabledAdaptations() const;
    WEBCORE_EXPORT ViewportArguments viewportArguments() const;

    const Optional<ViewportArguments>& overrideViewportArguments() const { return m_overrideViewportArguments; }
    WEBCORE_EXPORT void setOverrideViewportArguments(const Optional<ViewportArguments>&);

    static void refreshPlugins(bool reload);
    WEBCORE_EXPORT PluginData& pluginData();
    void clearPluginData();

    WEBCORE_EXPORT void setCanStartMedia(bool);
    bool canStartMedia() const { return m_canStartMedia; }

    EditorClient& editorClient() { return m_editorClient.get(); }
    PlugInClient* plugInClient() const { return m_plugInClient.get(); }

    Frame& mainFrame() { return m_mainFrame.get(); }
    const Frame& mainFrame() const { return m_mainFrame.get(); }

    bool openedByDOM() const;
    void setOpenedByDOM();

    bool openedByDOMWithOpener() const { return m_openedByDOMWithOpener; }
    void setOpenedByDOMWithOpener() { m_openedByDOMWithOpener = true; }

    WEBCORE_EXPORT void goToItem(HistoryItem&, FrameLoadType, ShouldTreatAsContinuingLoad);

    WEBCORE_EXPORT void setGroupName(const String&);
    WEBCORE_EXPORT const String& groupName() const;

    PageGroup& group();

    WEBCORE_EXPORT static void forEachPage(const WTF::Function<void(Page&)>&);

    void incrementSubframeCount() { ++m_subframeCount; }
    void decrementSubframeCount() { ASSERT(m_subframeCount); --m_subframeCount; }
    int subframeCount() const { checkSubframeCountConsistency(); return m_subframeCount; }

    void incrementNestedRunLoopCount();
    void decrementNestedRunLoopCount();
    bool insideNestedRunLoop() const { return m_nestedRunLoopCount > 0; }
    WEBCORE_EXPORT void whenUnnested(WTF::Function<void()>&&);

#if ENABLE(REMOTE_INSPECTOR)
    WEBCORE_EXPORT bool remoteInspectionAllowed() const;
    WEBCORE_EXPORT void setRemoteInspectionAllowed(bool);
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
#if ENABLE(POINTER_EVENTS)
    PointerCaptureController& pointerCaptureController() const { return *m_pointerCaptureController; }
#endif
#if ENABLE(POINTER_LOCK)
    PointerLockController& pointerLockController() const { return *m_pointerLockController; }
#endif
    LibWebRTCProvider& libWebRTCProvider() { return m_libWebRTCProvider.get(); }
    RTCController& rtcController() { return m_rtcController; }
    WEBCORE_EXPORT void disableICECandidateFiltering();
    WEBCORE_EXPORT void enableICECandidateFiltering();
    bool shouldEnableICECandidateFilteringByDefault() const { return m_shouldEnableICECandidateFilteringByDefault; }

    void didChangeMainDocument();

    PerformanceMonitor* performanceMonitor() { return m_performanceMonitor.get(); }

    RenderingUpdateScheduler& renderingUpdateScheduler();

    ValidationMessageClient* validationMessageClient() const { return m_validationMessageClient.get(); }
    void updateValidationBubbleStateIfNeeded();

    WEBCORE_EXPORT ScrollingCoordinator* scrollingCoordinator();

    WEBCORE_EXPORT String scrollingStateTreeAsText();
    WEBCORE_EXPORT String synchronousScrollingReasonsAsText();
    WEBCORE_EXPORT Ref<DOMRectList> nonFastScrollableRects();

    WEBCORE_EXPORT Ref<DOMRectList> touchEventRectsForEvent(const String& eventName);
    WEBCORE_EXPORT Ref<DOMRectList> passiveTouchEventListenerRects();

    Settings& settings() const { return *m_settings; }
    ProgressTracker& progress() const { return *m_progress; }
    BackForwardController& backForward() const { return *m_backForwardController; }

    Seconds domTimerAlignmentInterval() const { return m_domTimerAlignmentInterval; }

    void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
    bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

    WEBCORE_EXPORT bool findString(const String&, FindOptions, DidWrap* = nullptr);
    WEBCORE_EXPORT uint32_t replaceRangesWithText(const Vector<Ref<Range>>& rangesToReplace, const String& replacementText, bool selectionOnly);
    WEBCORE_EXPORT uint32_t replaceSelectionWithText(const String& replacementText);

    WEBCORE_EXPORT RefPtr<Range> rangeOfString(const String&, Range*, FindOptions);

    WEBCORE_EXPORT unsigned countFindMatches(const String&, FindOptions, unsigned maxMatchCount);
    WEBCORE_EXPORT unsigned markAllMatchesForText(const String&, FindOptions, bool shouldHighlight, unsigned maxMatchCount);

    WEBCORE_EXPORT void unmarkAllTextMatches();

    WEBCORE_EXPORT void dispatchBeforePrintEvent();
    WEBCORE_EXPORT void dispatchAfterPrintEvent();

    // Find all the Ranges for the matching text.
    // Upon return, indexForSelection will be one of the following:
    // 0 if there is no user selection
    // the index of the first range after the user selection
    // NoMatchAfterUserSelection if there is no matching text after the user selection.
    enum { NoMatchAfterUserSelection = -1 };
    WEBCORE_EXPORT void findStringMatchingRanges(const String&, FindOptions, int maxCount, Vector<RefPtr<Range>>&, int& indexForSelection);

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
    void didFinishLoad(); // Called when the load has been committed in the main frame.

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

    float initialScale() const { return m_initialScale; }
    WEBCORE_EXPORT void setInitialScale(float);

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
    void setUseDarkAppearanceOverride(Optional<bool>);

#if ENABLE(TEXT_AUTOSIZING)
    float textAutosizingWidth() const { return m_textAutosizingWidth; }
    void setTextAutosizingWidth(float textAutosizingWidth) { m_textAutosizingWidth = textAutosizingWidth; }
    WEBCORE_EXPORT void recomputeTextAutoSizingInAllFrames();
#endif

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
    bool paginationLineGridEnabled() const { return m_paginationLineGridEnabled; }
    WEBCORE_EXPORT void setPaginationLineGridEnabled(bool flag);

    WEBCORE_EXPORT unsigned pageCount() const;

    WEBCORE_EXPORT DiagnosticLoggingClient& diagnosticLoggingClient() const;

    PerformanceLoggingClient* performanceLoggingClient() const { return m_performanceLoggingClient.get(); }

    WheelEventDeltaFilter* wheelEventDeltaFilter() { return m_recentWheelEventDeltaFilter.get(); }
    PageOverlayController& pageOverlayController() { return *m_pageOverlayController; }

#if PLATFORM(MAC) && (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION))
    ServicesOverlayController& servicesOverlayController() { return *m_servicesOverlayController; }
#endif

#if PLATFORM(MAC)
    ScrollLatchingState* latchingState();
    void pushNewLatchingState();
    void popLatchingState();
    void resetLatchingState();
    void removeLatchingStateForTarget(Element&);
#endif // PLATFORM(MAC)

#if ENABLE(APPLE_PAY)
    PaymentCoordinator& paymentCoordinator() const { return *m_paymentCoordinator; }
    WEBCORE_EXPORT void setPaymentCoordinator(std::unique_ptr<PaymentCoordinator>&&);
#endif

#if ENABLE(WEB_AUTHN)
    AuthenticatorCoordinator& authenticatorCoordinator() { return m_authenticatorCoordinator.get(); }
#endif

#if ENABLE(APPLICATION_MANIFEST)
    const Optional<ApplicationManifest>& applicationManifest() const { return m_applicationManifest; }
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

    WEBCORE_EXPORT void suspendScriptedAnimations();
    WEBCORE_EXPORT void resumeScriptedAnimations();
    bool scriptedAnimationsSuspended() const { return m_scriptedAnimationsSuspended; }

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

    StorageNamespace* sessionStorage(bool optionalCreate = true);
    void setSessionStorage(RefPtr<StorageNamespace>&&);

    bool hasCustomHTMLTokenizerTimeDelay() const;
    double customHTMLTokenizerTimeDelay() const;

    WEBCORE_EXPORT void setMemoryCacheClientCallsEnabled(bool);
    bool areMemoryCacheClientCallsEnabled() const { return m_areMemoryCacheClientCallsEnabled; }

    // Don't allow more than a certain number of frames in a page.
    // This seems like a reasonable upper bound, and otherwise mutually
    // recursive frameset pages can quickly bring the program to its knees
    // with exponential growth in the number of frames.
    static const int maxNumberOfFrames = 1000;

    void setEditable(bool isEditable) { m_isEditable = isEditable; }
    bool isEditable() { return m_isEditable; }

    WEBCORE_EXPORT VisibilityState visibilityState() const;
    WEBCORE_EXPORT void resumeAnimatingImages();

    void didFinishLoadingImageForElement(HTMLImageElement&);

    WEBCORE_EXPORT void addLayoutMilestones(OptionSet<LayoutMilestone>);
    WEBCORE_EXPORT void removeLayoutMilestones(OptionSet<LayoutMilestone>);
    OptionSet<LayoutMilestone> requestedLayoutMilestones() const { return m_requestedLayoutMilestones; }

    WEBCORE_EXPORT void setHeaderHeight(int);
    WEBCORE_EXPORT void setFooterHeight(int);

    int headerHeight() const { return m_headerHeight; }
    int footerHeight() const { return m_footerHeight; }

    WEBCORE_EXPORT Color pageExtendedBackgroundColor() const;

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

#if ENABLE(VIDEO_TRACK)
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

    VisitedLinkStore& visitedLinkStore();
    WEBCORE_EXPORT void setVisitedLinkStore(Ref<VisitedLinkStore>&&);

    WEBCORE_EXPORT PAL::SessionID sessionID() const;
    WEBCORE_EXPORT void setSessionID(PAL::SessionID);
    bool usesEphemeralSession() const { return m_sessionID.isEphemeral(); }

    MediaProducer::MediaStateFlags mediaState() const { return m_mediaState; }
    void updateIsPlayingMedia(uint64_t);
    MediaProducer::MutedStateFlags mutedState() const { return m_mutedState; }
    bool isAudioMuted() const { return m_mutedState & MediaProducer::AudioIsMuted; }
    bool isMediaCaptureMuted() const { return m_mutedState & MediaProducer::MediaStreamCaptureIsMuted; };
    void schedulePlaybackControlsManagerUpdate();
    WEBCORE_EXPORT void setMuted(MediaProducer::MutedStateFlags);
    WEBCORE_EXPORT void stopMediaCapture();

    WEBCORE_EXPORT void stopAllMediaPlayback();
    WEBCORE_EXPORT void suspendAllMediaPlayback();
    WEBCORE_EXPORT void resumeAllMediaPlayback();
    bool mediaPlaybackIsSuspended() const { return m_mediaPlaybackIsSuspended; }
    WEBCORE_EXPORT void suspendAllMediaBuffering();
    WEBCORE_EXPORT void resumeAllMediaBuffering();
    bool mediaBufferingIsSuspended() const { return m_mediaBufferingIsSuspended; }

    void setHasResourceLoadClient(bool has) { m_hasResourceLoadClient = has; }
    bool hasResourceLoadClient() const { return m_hasResourceLoadClient; }

#if ENABLE(MEDIA_SESSION)
    WEBCORE_EXPORT void handleMediaEvent(MediaEventType);
    WEBCORE_EXPORT void setVolumeOfMediaElement(double, uint64_t);
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void addPlaybackTargetPickerClient(uint64_t);
    void removePlaybackTargetPickerClient(uint64_t);
    void showPlaybackTargetPicker(uint64_t, const IntPoint&, bool, RouteSharingPolicy, const String&);
    void playbackTargetPickerClientStateDidChange(uint64_t, MediaProducer::MediaStateFlags);
    WEBCORE_EXPORT void setMockMediaPlaybackTargetPickerEnabled(bool);
    WEBCORE_EXPORT void setMockMediaPlaybackTargetPickerState(const String&, MediaPlaybackTargetContext::State);
    WEBCORE_EXPORT void mockMediaPlaybackTargetPickerDismissPopup();

    WEBCORE_EXPORT void setPlaybackTarget(uint64_t, Ref<MediaPlaybackTarget>&&);
    WEBCORE_EXPORT void playbackTargetAvailabilityDidChange(uint64_t, bool);
    WEBCORE_EXPORT void setShouldPlayToPlaybackTarget(uint64_t, bool);
    WEBCORE_EXPORT void playbackTargetPickerWasDismissed(uint64_t);
#endif

    RefPtr<WheelEventTestMonitor> wheelEventTestMonitor() const { return m_wheelEventTestMonitor; }
    WEBCORE_EXPORT WheelEventTestMonitor& ensureWheelEventTestMonitor();
    void clearWheelEventTestMonitor() { m_wheelEventTestMonitor = nullptr; }
    bool isMonitoringWheelEvents() const { return !!m_wheelEventTestMonitor; }

#if ENABLE(VIDEO)
    bool allowsMediaDocumentInlinePlayback() const { return m_allowsMediaDocumentInlinePlayback; }
    WEBCORE_EXPORT void setAllowsMediaDocumentInlinePlayback(bool);
#endif

    bool allowsPlaybackControlsForAutoplayingAudio() const { return m_allowsPlaybackControlsForAutoplayingAudio; }
    void setAllowsPlaybackControlsForAutoplayingAudio(bool allowsPlaybackControlsForAutoplayingAudio) { m_allowsPlaybackControlsForAutoplayingAudio = allowsPlaybackControlsForAutoplayingAudio; }

#if ENABLE(INDEXED_DATABASE)
    IDBClient::IDBConnectionToServer& idbConnection();
    WEBCORE_EXPORT IDBClient::IDBConnectionToServer* optionalIDBConnection();
    WEBCORE_EXPORT void clearIDBConnection();
#endif

    void setShowAllPlugins(bool showAll) { m_showAllPlugins = showAll; }
    bool showAllPlugins() const;

    WEBCORE_EXPORT void setDOMTimerAlignmentIntervalIncreaseLimit(Seconds);

    bool isControlledByAutomation() const { return m_controlledByAutomation; }
    void setControlledByAutomation(bool controlled) { m_controlledByAutomation = controlled; }

    WEBCORE_EXPORT bool isAlwaysOnLoggingAllowed() const;

    String captionUserPreferencesStyleSheet();
    void setCaptionUserPreferencesStyleSheet(const String&);

    bool isResourceCachingDisabledByWebInspector() const { return m_resourceCachingDisabledByWebInspector; }
    void setResourceCachingDisabledByWebInspector(bool disabled) { m_resourceCachingDisabledByWebInspector = disabled; }

    Optional<EventThrottlingBehavior> eventThrottlingBehaviorOverride() const { return m_eventThrottlingBehaviorOverride; }
    void setEventThrottlingBehaviorOverride(Optional<EventThrottlingBehavior> throttling) { m_eventThrottlingBehaviorOverride = throttling; }

    Optional<CompositingPolicy> compositingPolicyOverride() const { return m_compositingPolicyOverride; }
    void setCompositingPolicyOverride(Optional<CompositingPolicy> policy) { m_compositingPolicyOverride = policy; }

#if ENABLE(WEBGL)
    WebGLStateTracker* webGLStateTracker() const { return m_webGLStateTracker.get(); }
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    SpeechSynthesisClient* speechSynthesisClient() const { return m_speechSynthesisClient.get(); }
#endif

    bool isOnlyNonUtilityPage() const;
    bool isUtilityPage() const { return m_isUtilityPage; }

    bool isLowPowerModeEnabled() const;
    WEBCORE_EXPORT void setLowPowerModeEnabledOverrideForTesting(Optional<bool>);

    WEBCORE_EXPORT void applicationWillResignActive();
    WEBCORE_EXPORT void applicationDidEnterBackground();
    WEBCORE_EXPORT void applicationWillEnterForeground();
    WEBCORE_EXPORT void applicationDidBecomeActive();

    PerformanceLogging& performanceLogging() const { return *m_performanceLogging; }

    void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel);

    WEBCORE_EXPORT Vector<Ref<Element>> editableElementsInRect(const FloatRect&) const;

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    DeviceOrientationUpdateProvider* deviceOrientationUpdateProvider() const { return m_deviceOrientationUpdateProvider.get(); }
#endif

    WEBCORE_EXPORT void forEachDocument(const WTF::Function<void(Document&)>&) const;
    void forEachMediaElement(const WTF::Function<void(HTMLMediaElement&)>&);

    bool shouldDisableCorsForRequestTo(const URL&) const;

private:
    struct Navigation {
        RegistrableDomain domain;
        FrameLoadType type;
    };
    void logNavigation(const Navigation&);

    WEBCORE_EXPORT void initGroup();

    void setIsInWindowInternal(bool);
    void setIsVisibleInternal(bool);
    void setIsVisuallyIdleInternal(bool);

    void checkSubframeCountConsistency() const;

    enum ShouldHighlightMatches { DoNotHighlightMatches, HighlightMatches };
    enum ShouldMarkMatches { DoNotMarkMatches, MarkMatches };

    unsigned findMatchesForText(const String&, FindOptions, unsigned maxMatchCount, ShouldHighlightMatches, ShouldMarkMatches);

    Optional<std::pair<MediaCanStartListener&, Document&>> takeAnyMediaCanStartListener();

#if ENABLE(VIDEO)
    void playbackControlsManagerUpdateTimerFired();
#endif

    Vector<Ref<PluginViewBase>> pluginViews();

    void handleLowModePowerChange(bool);

    enum class TimerThrottlingState { Disabled, Enabled, EnabledIncreasing };
    void hiddenPageDOMTimerThrottlingStateChanged();
    void setTimerThrottlingState(TimerThrottlingState);
    void updateTimerThrottlingState();
    void updateDOMTimerAlignmentInterval();
    void domTimerAlignmentIntervalIncreaseTimerFired();

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
#if ENABLE(POINTER_EVENTS)
    const std::unique_ptr<PointerCaptureController> m_pointerCaptureController;
#endif
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
    std::unique_ptr<PlugInClient> m_plugInClient;
    std::unique_ptr<ValidationMessageClient> m_validationMessageClient;
    std::unique_ptr<DiagnosticLoggingClient> m_diagnosticLoggingClient;
    std::unique_ptr<PerformanceLoggingClient> m_performanceLoggingClient;

#if ENABLE(WEBGL)
    std::unique_ptr<WebGLStateTracker> m_webGLStateTracker;
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    std::unique_ptr<SpeechSynthesisClient> m_speechSynthesisClient;
#endif

    UniqueRef<MediaRecorderProvider> m_mediaRecorderProvider;
    UniqueRef<LibWebRTCProvider> m_libWebRTCProvider;
    RTCController m_rtcController;

    int m_nestedRunLoopCount { 0 };
    WTF::Function<void()> m_unnestCallback;

    int m_subframeCount { 0 };
    String m_groupName;
    bool m_openedByDOM { false };
    bool m_openedByDOMWithOpener { false };

    bool m_tabKeyCyclesThroughElements { true };
    bool m_defersLoading { false };
    unsigned m_defersLoadingCallCount { 0 };

    bool m_inLowQualityInterpolationMode { false };
    bool m_areMemoryCacheClientCallsEnabled { true };
    float m_mediaVolume { 1 };
    MediaProducer::MutedStateFlags m_mutedState { MediaProducer::NoneMuted };

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
    Optional<bool> m_useDarkAppearanceOverride;

#if ENABLE(TEXT_AUTOSIZING)
    float m_textAutosizingWidth { 0 };
#endif
    float m_initialScale { 1.0f };
    
    bool m_suppressScrollbarAnimations { false };
    
    unsigned m_verticalScrollElasticity : 2; // ScrollElasticity
    unsigned m_horizontalScrollElasticity : 2; // ScrollElasticity    

    Pagination m_pagination;
    bool m_paginationLineGridEnabled { false };

    String m_userStyleSheetPath;
    mutable String m_userStyleSheet;
    mutable bool m_didLoadUserStyleSheet { false };
    mutable Optional<WallTime> m_userStyleSheetModificationTime;

    String m_captionUserPreferencesStyleSheet;

    std::unique_ptr<PageGroup> m_singlePageGroup;
    PageGroup* m_group { nullptr };

    JSC::Debugger* m_debugger { nullptr };

    bool m_canStartMedia { true };

    RefPtr<StorageNamespace> m_sessionStorage;

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

#if ENABLE(INDEXED_DATABASE)
    RefPtr<IDBClient::IDBConnectionToServer> m_idbConnectionToServer;
#endif

    HashSet<String> m_seenPlugins;
    HashSet<String> m_seenMediaEngines;

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
    Ref<VisitedLinkStore> m_visitedLinkStore;
    RefPtr<WheelEventTestMonitor> m_wheelEventTestMonitor;
    HashSet<ActivityStateChangeObserver*> m_activityStateChangeObservers;

#if ENABLE(RESOURCE_USAGE)
    std::unique_ptr<ResourceUsageOverlay> m_resourceUsageOverlay;
#endif

    PAL::SessionID m_sessionID;

    bool m_isClosing { false };
    bool m_isRestoringCachedPage { false };

    MediaProducer::MediaStateFlags m_mediaState { MediaProducer::IsNotPlaying };

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
    bool m_inUpdateRendering { false };
    bool m_hasResourceLoadClient { false };
    bool m_delegatesScaling { false };

    UserInterfaceLayoutDirection m_userInterfaceLayoutDirection { UserInterfaceLayoutDirection::LTR };
    
    // For testing.
    Optional<EventThrottlingBehavior> m_eventThrottlingBehaviorOverride;
    Optional<CompositingPolicy> m_compositingPolicyOverride;

    std::unique_ptr<PerformanceMonitor> m_performanceMonitor;
    std::unique_ptr<LowPowerModeNotifier> m_lowPowerModeNotifier;
    Optional<bool> m_lowPowerModeEnabledOverrideForTesting;

    Optional<Navigation> m_navigationToLogWhenVisible;

    std::unique_ptr<PerformanceLogging> m_performanceLogging;
#if PLATFORM(MAC)
    Vector<ScrollLatchingState> m_latchingState;
#endif
#if PLATFORM(MAC) && (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION))
    std::unique_ptr<ServicesOverlayController> m_servicesOverlayController;
#endif

    std::unique_ptr<WheelEventDeltaFilter> m_recentWheelEventDeltaFilter;
    std::unique_ptr<PageOverlayController> m_pageOverlayController;

#if ENABLE(APPLE_PAY)
    std::unique_ptr<PaymentCoordinator> m_paymentCoordinator;
#endif

#if ENABLE(WEB_AUTHN)
    UniqueRef<AuthenticatorCoordinator> m_authenticatorCoordinator;
#endif

#if ENABLE(APPLICATION_MANIFEST)
    Optional<ApplicationManifest> m_applicationManifest;
#endif

    Optional<ViewportArguments> m_overrideViewportArguments;

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    RefPtr<DeviceOrientationUpdateProvider> m_deviceOrientationUpdateProvider;
#endif

    Vector<UserContentURLPattern> m_corsDisablingPatterns;
};

inline PageGroup& Page::group()
{
    if (!m_group)
        initGroup();
    return *m_group;
}

#if !ASSERT_ENABLED

inline void Page::checkSubframeCountConsistency() const
{
}

#endif // !ASSERT_ENABLED

} // namespace WebCore
