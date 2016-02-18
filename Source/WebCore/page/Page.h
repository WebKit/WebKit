/*
 * Copyright (C) 2006-2010, 2013, 2015 Apple Inc. All rights reserved.
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

#ifndef Page_h
#define Page_h

#include "FindOptions.h"
#include "FrameLoaderTypes.h"
#include "LayoutMilestones.h"
#include "LayoutRect.h"
#include "MediaProducer.h"
#include "PageThrottler.h"
#include "PageVisibilityState.h"
#include "Pagination.h"
#include "PlatformScreen.h"
#include "Region.h"
#include "ScrollTypes.h"
#include "SessionID.h"
#include "Supplementable.h"
#include "ViewState.h"
#include "ViewportArguments.h"
#include "WheelEventTestTrigger.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if OS(SOLARIS)
#include <sys/time.h> // For time_t structure.
#endif

#if PLATFORM(COCOA)
#include <wtf/SchedulePair.h>
#endif

#if ENABLE(MEDIA_SESSION)
#include "MediaSessionEvents.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetContext.h"
#endif

namespace JSC {
class Debugger;
}

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
}

class AlternativeTextClient;
class ApplicationCacheStorage;
class BackForwardController;
class BackForwardClient;
class Chrome;
class ChromeClient;
class ClientRectList;
class Color;
class ContextMenuClient;
class ContextMenuController;
class DatabaseProvider;
class DragCaretController;
class DragClient;
class DragController;
class EditorClient;
class FocusController;
class Frame;
class FrameLoaderClient;
class HistoryItem;
class HTMLMediaElement;
class UserInputBridge;
class InspectorClient;
class InspectorController;
class MainFrame;
class MediaCanStartListener;
class MediaPlaybackTarget;
class PageConfiguration;
class PageConsoleClient;
class PageDebuggable;
class PageGroup;
class PageThrottler;
class PlugInClient;
class PluginData;
class PluginViewBase;
class PointerLockController;
class ProgressTracker;
class ProgressTrackerClient;
class Range;
class RenderObject;
class RenderTheme;
class ReplayController;
class ResourceUsageOverlay;
class VisibleSelection;
class ScrollableArea;
class ScrollingCoordinator;
class Settings;
class StorageNamespace;
class StorageNamespaceProvider;
class UserContentController;
class ValidationMessageClient;
class ViewStateChangeObserver;
class VisitedLinkStore;

typedef uint64_t LinkHash;

enum FindDirection { FindDirectionForward, FindDirectionBackward };

class Page : public Supplementable<Page> {
    WTF_MAKE_NONCOPYABLE(Page);
    WTF_MAKE_FAST_ALLOCATED;
    friend class Settings;
    friend class PageThrottler;

public:
    WEBCORE_EXPORT static void updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    WEBCORE_EXPORT static void clearPreviousItemFromAllPages(HistoryItem*);
    WEBCORE_EXPORT static void setTabSuspensionEnabled(bool);

    WEBCORE_EXPORT explicit Page(PageConfiguration&);
    WEBCORE_EXPORT ~Page();

    WEBCORE_EXPORT uint64_t renderTreeSize() const;

    void setNeedsRecalcStyleInAllFrames();

    RenderTheme& theme() const { return *m_theme; }

    WEBCORE_EXPORT ViewportArguments viewportArguments() const;

    static void refreshPlugins(bool reload);
    WEBCORE_EXPORT PluginData& pluginData() const;

    WEBCORE_EXPORT void setCanStartMedia(bool);
    bool canStartMedia() const { return m_canStartMedia; }

    EditorClient& editorClient() { return m_editorClient; }
    PlugInClient* plugInClient() const { return m_plugInClient; }

    MainFrame& mainFrame() { ASSERT(m_mainFrame); return *m_mainFrame; }
    const MainFrame& mainFrame() const { ASSERT(m_mainFrame); return *m_mainFrame; }

    bool inPageCache() const;

    bool openedByDOM() const;
    void setOpenedByDOM();

    WEBCORE_EXPORT void goToItem(HistoryItem&, FrameLoadType);

    WEBCORE_EXPORT void setGroupName(const String&);
    WEBCORE_EXPORT const String& groupName() const;

    PageGroup& group();
    PageGroup* groupPtr() { return m_group; } // can return 0

    static void forEachPage(std::function<void(Page&)>);

    void incrementSubframeCount() { ++m_subframeCount; }
    void decrementSubframeCount() { ASSERT(m_subframeCount); --m_subframeCount; }
    int subframeCount() const { checkSubframeCountConsistency(); return m_subframeCount; }

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
#if ENABLE(WEB_REPLAY)
    ReplayController& replayController() const { return *m_replayController; }
#endif
    InspectorController& inspectorController() const { return *m_inspectorController; }
#if ENABLE(POINTER_LOCK)
    PointerLockController& pointerLockController() const { return *m_pointerLockController; }
#endif
    ValidationMessageClient* validationMessageClient() const { return m_validationMessageClient; }

    WEBCORE_EXPORT ScrollingCoordinator* scrollingCoordinator();

    WEBCORE_EXPORT String scrollingStateTreeAsText();
    WEBCORE_EXPORT String synchronousScrollingReasonsAsText();
    WEBCORE_EXPORT Ref<ClientRectList> nonFastScrollableRects();

    Settings& settings() const { return *m_settings; }
    ProgressTracker& progress() const { return *m_progress; }
    BackForwardController& backForward() const { return *m_backForwardController; }

#if ENABLE(VIEW_MODE_CSS_MEDIA)
    enum ViewMode {
        ViewModeInvalid,
        ViewModeWindowed,
        ViewModeFloating,
        ViewModeFullscreen,
        ViewModeMaximized,
        ViewModeMinimized
    };
    static ViewMode stringToViewMode(const String&);

    ViewMode viewMode() const { return m_viewMode; }
    WEBCORE_EXPORT void setViewMode(ViewMode);
#endif // ENABLE(VIEW_MODE_CSS_MEDIA)

    void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
    bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

    WEBCORE_EXPORT bool findString(const String&, FindOptions);

    WEBCORE_EXPORT RefPtr<Range> rangeOfString(const String&, Range*, FindOptions);

    WEBCORE_EXPORT unsigned countFindMatches(const String&, FindOptions, unsigned maxMatchCount);
    WEBCORE_EXPORT unsigned markAllMatchesForText(const String&, FindOptions, bool shouldHighlight, unsigned maxMatchCount);

    WEBCORE_EXPORT void unmarkAllTextMatches();

    // find all the Ranges for the matching text.
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

    // The view scale factor is multiplied into the page scale factor by all
    // callers of setPageScaleFactor.
    WEBCORE_EXPORT void setViewScaleFactor(float);
    float viewScaleFactor() const { return m_viewScaleFactor; }

    WEBCORE_EXPORT void setZoomedOutPageScaleFactor(float);
    float zoomedOutPageScaleFactor() const { return m_zoomedOutPageScaleFactor; }

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    WEBCORE_EXPORT void setDeviceScaleFactor(float);

    float topContentInset() const { return m_topContentInset; }
    WEBCORE_EXPORT void setTopContentInset(float);
    
#if ENABLE(IOS_TEXT_AUTOSIZING)
    float textAutosizingWidth() const { return m_textAutosizingWidth; }
    void setTextAutosizingWidth(float textAutosizingWidth) { m_textAutosizingWidth = textAutosizingWidth; }
#endif
    
    bool shouldSuppressScrollbarAnimations() const { return m_suppressScrollbarAnimations; }
    WEBCORE_EXPORT void setShouldSuppressScrollbarAnimations(bool suppressAnimations);
    void lockAllOverlayScrollbarsToHidden(bool lockOverlayScrollbars);
    
    WEBCORE_EXPORT void setVerticalScrollElasticity(ScrollElasticity);
    ScrollElasticity verticalScrollElasticity() const { return static_cast<ScrollElasticity>(m_verticalScrollElasticity); }

    WEBCORE_EXPORT void setHorizontalScrollElasticity(ScrollElasticity);
    ScrollElasticity horizontalScrollElasticity() const { return static_cast<ScrollElasticity>(m_horizontalScrollElasticity); }
    
    // Page and FrameView both store a Pagination value. Page::pagination() is set only by API,
    // and FrameView::pagination() is set only by CSS. Page::pagination() will affect all
    // FrameViews in the page cache, but FrameView::pagination() only affects the current
    // FrameView.
    const Pagination& pagination() const { return m_pagination; }
    WEBCORE_EXPORT void setPagination(const Pagination&);
    bool paginationLineGridEnabled() const { return m_paginationLineGridEnabled; }
    WEBCORE_EXPORT void setPaginationLineGridEnabled(bool flag);

    WEBCORE_EXPORT unsigned pageCount() const;

    // Notifications when the Page starts and stops being presented via a native window.
    WEBCORE_EXPORT void setViewState(ViewState::Flags);
    void setPageActivityState(PageActivityState::Flags);
    WEBCORE_EXPORT void setIsVisible(bool);
    WEBCORE_EXPORT void setIsPrerender();
    bool isVisible() const { return m_viewState & ViewState::IsVisible; }

    // Notification that this Page was moved into or out of a native window.
    WEBCORE_EXPORT void setIsInWindow(bool);
    bool isInWindow() const { return m_viewState & ViewState::IsInWindow; }

    void setIsClosing() { m_isClosing = true; }
    bool isClosing() const { return m_isClosing; }

    void addViewStateChangeObserver(ViewStateChangeObserver&);
    void removeViewStateChangeObserver(ViewStateChangeObserver&);

    WEBCORE_EXPORT void suspendScriptedAnimations();
    WEBCORE_EXPORT void resumeScriptedAnimations();
    bool scriptedAnimationsSuspended() const { return m_scriptedAnimationsSuspended; }

    void userStyleSheetLocationChanged();
    const String& userStyleSheet() const;

    void dnsPrefetchingStateChanged();
    void storageBlockingStateChanged();

#if ENABLE(RESOURCE_USAGE)
    void setResourceUsageOverlayVisible(bool);
#endif

    void setDebugger(JSC::Debugger*);
    JSC::Debugger* debugger() const { return m_debugger; }

    WEBCORE_EXPORT void invalidateStylesForAllLinks();
    WEBCORE_EXPORT void invalidateStylesForLink(LinkHash);

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

    static bool s_tabSuspensionIsEnabled;

    void setEditable(bool isEditable) { m_isEditable = isEditable; }
    bool isEditable() { return m_isEditable; }

    WEBCORE_EXPORT PageVisibilityState visibilityState() const;
    WEBCORE_EXPORT void resumeAnimatingImages();

    WEBCORE_EXPORT void addLayoutMilestones(LayoutMilestones);
    WEBCORE_EXPORT void removeLayoutMilestones(LayoutMilestones);
    LayoutMilestones requestedLayoutMilestones() const { return m_requestedLayoutMilestones; }

#if ENABLE(RUBBER_BANDING)
    WEBCORE_EXPORT void addHeaderWithHeight(int);
    WEBCORE_EXPORT void addFooterWithHeight(int);
#endif

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
    void suspendDeviceMotionAndOrientationUpdates();
    void resumeDeviceMotionAndOrientationUpdates();

#ifndef NDEBUG
    void setIsPainting(bool painting) { m_isPainting = painting; }
    bool isPainting() const { return m_isPainting; }
#endif

    AlternativeTextClient* alternativeTextClient() const { return m_alternativeTextClient; }

    bool hasSeenPlugin(const String& serviceType) const;
    WEBCORE_EXPORT bool hasSeenAnyPlugin() const;
    void sawPlugin(const String& serviceType);
    void resetSeenPlugins();

    bool hasSeenMediaEngine(const String& engineName) const;
    bool hasSeenAnyMediaEngine() const;
    void sawMediaEngine(const String& engineName);
    void resetSeenMediaEngines();

    PageThrottler& pageThrottler() { return m_pageThrottler; }

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

    void setLastSpatialNavigationCandidateCount(unsigned count) { m_lastSpatialNavigationCandidatesCount = count; }
    unsigned lastSpatialNavigationCandidateCount() const { return m_lastSpatialNavigationCandidatesCount; }

    ApplicationCacheStorage& applicationCacheStorage() { return m_applicationCacheStorage; }
    DatabaseProvider& databaseProvider() { return m_databaseProvider; }

    StorageNamespaceProvider& storageNamespaceProvider() { return m_storageNamespaceProvider.get(); }
    void setStorageNamespaceProvider(Ref<StorageNamespaceProvider>&&);

    UserContentController* userContentController() { return m_userContentController.get(); }
    WEBCORE_EXPORT void setUserContentController(UserContentController*);

    VisitedLinkStore& visitedLinkStore();
    WEBCORE_EXPORT void setVisitedLinkStore(Ref<VisitedLinkStore>&&);

    WEBCORE_EXPORT SessionID sessionID() const;
    WEBCORE_EXPORT void setSessionID(SessionID);
    WEBCORE_EXPORT void enableLegacyPrivateBrowsing(bool privateBrowsingEnabled);
    bool usesEphemeralSession() const { return m_sessionID.isEphemeral(); }

    MediaProducer::MediaStateFlags mediaState() const { return m_mediaState; }
    void updateIsPlayingMedia(uint64_t);
    bool isMuted() const { return m_muted; }
    WEBCORE_EXPORT void setMuted(bool);

#if ENABLE(MEDIA_SESSION)
    WEBCORE_EXPORT void handleMediaEvent(MediaEventType);
    WEBCORE_EXPORT void setVolumeOfMediaElement(double, uint64_t);
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void addPlaybackTargetPickerClient(uint64_t);
    void removePlaybackTargetPickerClient(uint64_t);
    void showPlaybackTargetPicker(uint64_t, const IntPoint&, bool);
    void playbackTargetPickerClientStateDidChange(uint64_t, MediaProducer::MediaStateFlags);
    WEBCORE_EXPORT void setMockMediaPlaybackTargetPickerEnabled(bool);
    WEBCORE_EXPORT void setMockMediaPlaybackTargetPickerState(const String&, MediaPlaybackTargetContext::State);

    WEBCORE_EXPORT void setPlaybackTarget(uint64_t, Ref<MediaPlaybackTarget>&&);
    WEBCORE_EXPORT void playbackTargetAvailabilityDidChange(uint64_t, bool);
    WEBCORE_EXPORT void setShouldPlayToPlaybackTarget(uint64_t, bool);
#endif

    RefPtr<WheelEventTestTrigger> testTrigger() const { return m_testTrigger; }
    WEBCORE_EXPORT WheelEventTestTrigger& ensureTestTrigger();
    void clearTrigger() { m_testTrigger = nullptr; }
    bool expectsWheelEventTriggers() const { return !!m_testTrigger; }

#if ENABLE(VIDEO)
    bool allowsMediaDocumentInlinePlayback() const { return m_allowsMediaDocumentInlinePlayback; }
    WEBCORE_EXPORT void setAllowsMediaDocumentInlinePlayback(bool);
#endif

#if ENABLE(INDEXED_DATABASE)
    IDBClient::IDBConnectionToServer& idbConnection();
#endif

    void setShowAllPlugins(bool showAll) { m_showAllPlugins = showAll; }
    bool showAllPlugins() const;
    void setIsTabSuspended(bool);

private:
    WEBCORE_EXPORT void initGroup();

    void setIsInWindowInternal(bool);
    void setIsVisibleInternal(bool);
    void setIsVisuallyIdleInternal(bool);

#if ASSERT_DISABLED
    void checkSubframeCountConsistency() const { }
#else
    void checkSubframeCountConsistency() const;
#endif

    enum ShouldHighlightMatches { DoNotHighlightMatches, HighlightMatches };
    enum ShouldMarkMatches { DoNotMarkMatches, MarkMatches };

    unsigned findMatchesForText(const String&, FindOptions, unsigned maxMatchCount, ShouldHighlightMatches, ShouldMarkMatches);

    MediaCanStartListener* takeAnyMediaCanStartListener();

    Vector<Ref<PluginViewBase>> pluginViews();

    void hiddenPageDOMTimerThrottlingStateChanged();
    void setTimerThrottlingEnabled(bool);
    bool canTabSuspend();
    void updateTabSuspensionState();
    void tabSuspensionTimerFired();

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
#if ENABLE(WEB_REPLAY)
    const std::unique_ptr<ReplayController> m_replayController;
#endif
    const std::unique_ptr<InspectorController> m_inspectorController;
#if ENABLE(POINTER_LOCK)
    const std::unique_ptr<PointerLockController> m_pointerLockController;
#endif
    RefPtr<ScrollingCoordinator> m_scrollingCoordinator;

    const RefPtr<Settings> m_settings;
    const std::unique_ptr<ProgressTracker> m_progress;

    const std::unique_ptr<BackForwardController> m_backForwardController;
    const RefPtr<MainFrame> m_mainFrame;

    mutable RefPtr<PluginData> m_pluginData;

    RefPtr<RenderTheme> m_theme;

    EditorClient& m_editorClient;
    PlugInClient* m_plugInClient;
    ValidationMessageClient* m_validationMessageClient;

    int m_subframeCount;
    String m_groupName;
    bool m_openedByDOM;

    bool m_tabKeyCyclesThroughElements;
    bool m_defersLoading;
    unsigned m_defersLoadingCallCount;

    bool m_inLowQualityInterpolationMode;
    bool m_areMemoryCacheClientCallsEnabled;
    float m_mediaVolume;
    bool m_muted;

    float m_pageScaleFactor;
    float m_zoomedOutPageScaleFactor;
    float m_deviceScaleFactor { 1 };
    float m_viewScaleFactor { 1 };

    float m_topContentInset;
    
#if ENABLE(IOS_TEXT_AUTOSIZING)
    float m_textAutosizingWidth;
#endif
    
    bool m_suppressScrollbarAnimations;
    
    unsigned m_verticalScrollElasticity : 2; // ScrollElasticity
    unsigned m_horizontalScrollElasticity : 2; // ScrollElasticity    

    Pagination m_pagination;
    bool m_paginationLineGridEnabled { false };

    String m_userStyleSheetPath;
    mutable String m_userStyleSheet;
    mutable bool m_didLoadUserStyleSheet;
    mutable time_t m_userStyleSheetModificationTime;

    std::unique_ptr<PageGroup> m_singlePageGroup;
    PageGroup* m_group;

    JSC::Debugger* m_debugger;

    bool m_canStartMedia;

    RefPtr<StorageNamespace> m_sessionStorage;

#if ENABLE(VIEW_MODE_CSS_MEDIA)
    ViewMode m_viewMode;
#endif // ENABLE(VIEW_MODE_CSS_MEDIA)

    bool m_timerThrottlingEnabled;

    bool m_isEditable;
    bool m_isPrerender;
    ViewState::Flags m_viewState;
    PageActivityState::Flags m_pageActivityState;

    LayoutMilestones m_requestedLayoutMilestones;

    int m_headerHeight;
    int m_footerHeight;

    HashSet<RenderObject*> m_relevantUnpaintedRenderObjects;
    Region m_topRelevantPaintedRegion;
    Region m_bottomRelevantPaintedRegion;
    Region m_relevantUnpaintedRegion;
    bool m_isCountingRelevantRepaintedObjects;
#ifndef NDEBUG
    bool m_isPainting;
#endif
    AlternativeTextClient* m_alternativeTextClient;

    bool m_scriptedAnimationsSuspended;
    PageThrottler m_pageThrottler;
    const std::unique_ptr<PageConsoleClient> m_consoleClient;

#if ENABLE(REMOTE_INSPECTOR)
    const std::unique_ptr<PageDebuggable> m_inspectorDebuggable;
#endif

#if ENABLE(INDEXED_DATABASE)
    RefPtr<IDBClient::IDBConnectionToServer> m_idbIDBConnectionToServer;
#endif

    HashSet<String> m_seenPlugins;
    HashSet<String> m_seenMediaEngines;

    unsigned m_lastSpatialNavigationCandidatesCount;
    unsigned m_forbidPromptsDepth;

    Ref<ApplicationCacheStorage> m_applicationCacheStorage;
    Ref<DatabaseProvider> m_databaseProvider;
    Ref<StorageNamespaceProvider> m_storageNamespaceProvider;
    RefPtr<UserContentController> m_userContentController;
    Ref<VisitedLinkStore> m_visitedLinkStore;
    RefPtr<WheelEventTestTrigger> m_testTrigger;

    HashSet<ViewStateChangeObserver*> m_viewStateChangeObservers;

#if ENABLE(RESOURCE_USAGE)
    std::unique_ptr<ResourceUsageOverlay> m_resourceUsageOverlay;
#endif

    SessionID m_sessionID;

    bool m_isClosing;
    bool m_isTabSuspended { false };
    Timer m_tabSuspensionTimer;

    MediaProducer::MediaStateFlags m_mediaState { MediaProducer::IsNotPlaying };
    
    bool m_allowsMediaDocumentInlinePlayback { false };
    bool m_showAllPlugins { false };
};

inline PageGroup& Page::group()
{
    if (!m_group)
        initGroup();
    return *m_group;
}

} // namespace WebCore
    
#endif // Page_h
