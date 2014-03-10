/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
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
#include "PageThrottler.h"
#include "PageVisibilityState.h"
#include "Pagination.h"
#include "PlatformScreen.h"
#include "Region.h"
#include "SessionID.h"
#include "Supplementable.h"
#include "ViewState.h"
#include "ViewportArguments.h"
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

namespace JSC {
class Debugger;
}

namespace WebCore {

class AlternativeTextClient;
class BackForwardController;
class BackForwardClient;
class Chrome;
class ChromeClient;
class ClientRectList;
class Color;
class ContextMenuClient;
class ContextMenuController;
class Document;
class DragCaretController;
class DragClient;
class DragController;
class EditorClient;
class FocusController;
class Frame;
class FrameLoaderClient;
class FrameSelection;
class HaltablePlugin;
class HistoryItem;
class UserInputBridge;
class InspectorClient;
class InspectorController;
class MainFrame;
class MediaCanStartListener;
class Node;
class PageActivityAssertionToken;
class PageConsole;
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
class VisibleSelection;
class ScrollableArea;
class ScrollingCoordinator;
class Settings;
class StorageNamespace;
class UserContentController;
class ValidationMessageClient;
class VisitedLinkStore;

typedef uint64_t LinkHash;

enum FindDirection { FindDirectionForward, FindDirectionBackward };

class Page : public Supplementable<Page> {
    WTF_MAKE_NONCOPYABLE(Page);
    friend class Settings;
    friend class PageThrottler;

public:
    static void updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    static void jettisonStyleResolversInAllDocuments();

    // It is up to the platform to ensure that non-null clients are provided where required.
    struct PageClients {
        WTF_MAKE_NONCOPYABLE(PageClients); WTF_MAKE_FAST_ALLOCATED;
    public:
        PageClients();
        ~PageClients();

        AlternativeTextClient* alternativeTextClient;
        ChromeClient* chromeClient;
#if ENABLE(CONTEXT_MENUS)
        ContextMenuClient* contextMenuClient;
#endif
        EditorClient* editorClient;
        DragClient* dragClient;
        InspectorClient* inspectorClient;
        PlugInClient* plugInClient;
        ProgressTrackerClient* progressTrackerClient;
        RefPtr<BackForwardClient> backForwardClient;
        ValidationMessageClient* validationMessageClient;
        FrameLoaderClient* loaderClientForMainFrame;

        RefPtr<VisitedLinkStore> visitedLinkStore;
    };

    explicit Page(PageClients&);
    ~Page();

    uint64_t renderTreeSize() const;

    void setNeedsRecalcStyleInAllFrames();

    RenderTheme& theme() const { return *m_theme; }

    ViewportArguments viewportArguments() const;

    static void refreshPlugins(bool reload);
    PluginData& pluginData() const;

    void setCanStartMedia(bool);
    bool canStartMedia() const { return m_canStartMedia; }

    EditorClient* editorClient() const { return m_editorClient; }
    PlugInClient* plugInClient() const { return m_plugInClient; }

    MainFrame& mainFrame() { ASSERT(m_mainFrame); return *m_mainFrame; }
    const MainFrame& mainFrame() const { ASSERT(m_mainFrame); return *m_mainFrame; }

    bool openedByDOM() const;
    void setOpenedByDOM();

    void goToItem(HistoryItem*, FrameLoadType);

    void setGroupName(const String&);
    const String& groupName() const;

    PageGroup& group();
    PageGroup* groupPtr() { return m_group; } // can return 0

    void incrementSubframeCount() { ++m_subframeCount; }
    void decrementSubframeCount() { ASSERT(m_subframeCount); --m_subframeCount; }
    int subframeCount() const { checkSubframeCountConsistency(); return m_subframeCount; }

#if ENABLE(REMOTE_INSPECTOR)
    bool remoteInspectionAllowed() const;
    void setRemoteInspectionAllowed(bool);
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
#if ENABLE(INSPECTOR)
    InspectorController& inspectorController() const { return *m_inspectorController; }
#endif
#if ENABLE(POINTER_LOCK)
    PointerLockController* pointerLockController() const { return m_pointerLockController.get(); }
#endif
    ValidationMessageClient* validationMessageClient() const { return m_validationMessageClient; }

    ScrollingCoordinator* scrollingCoordinator();

    String scrollingStateTreeAsText();
    String synchronousScrollingReasonsAsText();
    PassRefPtr<ClientRectList> nonFastScrollableRects(const Frame*);

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
    void setViewMode(ViewMode);
#endif // ENABLE(VIEW_MODE_CSS_MEDIA)

    void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
    bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

    bool findString(const String&, FindOptions);

    PassRefPtr<Range> rangeOfString(const String&, Range*, FindOptions);

    unsigned countFindMatches(const String&, FindOptions, unsigned maxMatchCount);
    unsigned markAllMatchesForText(const String&, FindOptions, bool shouldHighlight, unsigned maxMatchCount);

    void unmarkAllTextMatches();

    // find all the Ranges for the matching text.
    // Upon return, indexForSelection will be one of the following:
    // 0 if there is no user selection
    // the index of the first range after the user selection
    // NoMatchAfterUserSelection if there is no matching text after the user selection.
    enum { NoMatchAfterUserSelection = -1 };
    void findStringMatchingRanges(const String&, FindOptions, int maxCount, Vector<RefPtr<Range>>*, int& indexForSelection);
#if PLATFORM(COCOA)
    void addSchedulePair(PassRefPtr<SchedulePair>);
    void removeSchedulePair(PassRefPtr<SchedulePair>);
    SchedulePairHashSet* scheduledRunLoopPairs() { return m_scheduledRunLoopPairs.get(); }

    OwnPtr<SchedulePairHashSet> m_scheduledRunLoopPairs;
#endif

    const VisibleSelection& selection() const;

    void setDefersLoading(bool);
    bool defersLoading() const { return m_defersLoading; }

    void clearUndoRedoOperations();

    bool inLowQualityImageInterpolationMode() const;
    void setInLowQualityImageInterpolationMode(bool = true);

    float mediaVolume() const { return m_mediaVolume; }
    void setMediaVolume(float);

    void setPageScaleFactor(float scale, const IntPoint& origin);
    float pageScaleFactor() const { return m_pageScaleFactor; }

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    void setDeviceScaleFactor(float);

    bool shouldSuppressScrollbarAnimations() const { return m_suppressScrollbarAnimations; }
    void setShouldSuppressScrollbarAnimations(bool suppressAnimations);
    void lockAllOverlayScrollbarsToHidden(bool lockOverlayScrollbars);

    // Page and FrameView both store a Pagination value. Page::pagination() is set only by API,
    // and FrameView::pagination() is set only by CSS. Page::pagination() will affect all
    // FrameViews in the page cache, but FrameView::pagination() only affects the current
    // FrameView.
    const Pagination& pagination() const { return m_pagination; }
    void setPagination(const Pagination&);

    unsigned pageCount() const;

    // Notifications when the Page starts and stops being presented via a native window.
    void setViewState(ViewState::Flags);
    void setIsVisible(bool);
    void setIsPrerender();
    bool isVisible() const { return m_viewState & ViewState::IsVisible; }

    // Notification that this Page was moved into or out of a native window.
    void setIsInWindow(bool);
    bool isInWindow() const { return m_viewState & ViewState::IsInWindow; }

    void suspendScriptedAnimations();
    void resumeScriptedAnimations();
    bool scriptedAnimationsSuspended() const { return m_scriptedAnimationsSuspended; }

    void userStyleSheetLocationChanged();
    const String& userStyleSheet() const;

    void dnsPrefetchingStateChanged();
    void storageBlockingStateChanged();
    void privateBrowsingStateChanged();

    void setDebugger(JSC::Debugger*);
    JSC::Debugger* debugger() const { return m_debugger; }

    static void removeAllVisitedLinks();

    void invalidateStylesForAllLinks();
    void invalidateStylesForLink(LinkHash);

    StorageNamespace* sessionStorage(bool optionalCreate = true);
    void setSessionStorage(PassRefPtr<StorageNamespace>);

    bool hasCustomHTMLTokenizerTimeDelay() const;
    double customHTMLTokenizerTimeDelay() const;

    void setMemoryCacheClientCallsEnabled(bool);
    bool areMemoryCacheClientCallsEnabled() const { return m_areMemoryCacheClientCallsEnabled; }

    // Don't allow more than a certain number of frames in a page.
    // This seems like a reasonable upper bound, and otherwise mutually
    // recursive frameset pages can quickly bring the program to its knees
    // with exponential growth in the number of frames.
    static const int maxNumberOfFrames = 1000;

    void setEditable(bool isEditable) { m_isEditable = isEditable; }
    bool isEditable() { return m_isEditable; }

#if ENABLE(PAGE_VISIBILITY_API)
    PageVisibilityState visibilityState() const;
#endif
    void resumeAnimatingImages();

    void addLayoutMilestones(LayoutMilestones);
    void removeLayoutMilestones(LayoutMilestones);
    LayoutMilestones requestedLayoutMilestones() const { return m_requestedLayoutMilestones; }

#if ENABLE(RUBBER_BANDING)
    void addHeaderWithHeight(int);
    void addFooterWithHeight(int);
#endif

    int headerHeight() const { return m_headerHeight; }
    int footerHeight() const { return m_footerHeight; }

    Color pageExtendedBackgroundColor() const;

    bool isCountingRelevantRepaintedObjects() const;
    void startCountingRelevantRepaintedObjects();
    void resetRelevantPaintedObjectCounter();
    void addRelevantRepaintedObject(RenderObject*, const LayoutRect& objectPaintRect);
    void addRelevantUnpaintedObject(RenderObject*, const LayoutRect& objectPaintRect);

    void suspendActiveDOMObjectsAndAnimations();
    void resumeActiveDOMObjectsAndAnimations();
#ifndef NDEBUG
    void setIsPainting(bool painting) { m_isPainting = painting; }
    bool isPainting() const { return m_isPainting; }
#endif

    AlternativeTextClient* alternativeTextClient() const { return m_alternativeTextClient; }

    bool hasSeenPlugin(const String& serviceType) const;
    bool hasSeenAnyPlugin() const;
    void sawPlugin(const String& serviceType);
    void resetSeenPlugins();

    bool hasSeenMediaEngine(const String& engineName) const;
    bool hasSeenAnyMediaEngine() const;
    void sawMediaEngine(const String& engineName);
    void resetSeenMediaEngines();

    PageThrottler& pageThrottler() { return m_pageThrottler; }

    PageConsole& console() { return *m_console; }

#if ENABLE(PAGE_VISIBILITY_API)
    void hiddenPageCSSAnimationSuspensionStateChanged();
#endif

#if ENABLE(VIDEO_TRACK)
    void captionPreferencesChanged();
#endif

    void incrementFrameHandlingBeforeUnloadEventCount();
    void decrementFrameHandlingBeforeUnloadEventCount();
    bool isAnyFrameHandlingBeforeUnloadEvent();
    void setLastSpatialNavigationCandidateCount(unsigned count) { m_lastSpatialNavigationCandidatesCount = count; }
    unsigned lastSpatialNavigationCandidateCount() const { return m_lastSpatialNavigationCandidatesCount; }

    void setUserContentController(UserContentController*);
    UserContentController* userContentController() { return m_userContentController.get(); }

    VisitedLinkStore& visitedLinkStore();

    SessionID sessionID() const;
    void setSessionID(SessionID);
    bool isSessionIDSet() const { return m_sessionID.isValid(); }

private:
    void initGroup();

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

    void setMinimumTimerInterval(double);
    double minimumTimerInterval() const;

    double timerAlignmentInterval() const { return m_timerAlignmentInterval; }

    Vector<Ref<PluginViewBase>> pluginViews();

    void setTimerThrottlingEnabled(bool);

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
#if ENABLE(INSPECTOR)
    const std::unique_ptr<InspectorController> m_inspectorController;
#endif
#if ENABLE(POINTER_LOCK)
    OwnPtr<PointerLockController> m_pointerLockController;
#endif
    RefPtr<ScrollingCoordinator> m_scrollingCoordinator;

    const RefPtr<Settings> m_settings;
    const std::unique_ptr<ProgressTracker> m_progress;

    const std::unique_ptr<BackForwardController> m_backForwardController;
    const RefPtr<MainFrame> m_mainFrame;

    mutable RefPtr<PluginData> m_pluginData;

    RefPtr<RenderTheme> m_theme;

    EditorClient* m_editorClient;
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

    float m_pageScaleFactor;
    float m_deviceScaleFactor;

    bool m_suppressScrollbarAnimations;

    Pagination m_pagination;

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

    double m_minimumTimerInterval;

    bool m_timerThrottlingEnabled;
    double m_timerAlignmentInterval;

    bool m_isEditable;
    bool m_isPrerender;
    ViewState::Flags m_viewState;

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
    const std::unique_ptr<PageConsole> m_console;

#if ENABLE(REMOTE_INSPECTOR)
    const std::unique_ptr<PageDebuggable> m_inspectorDebuggable;
#endif

    HashSet<String> m_seenPlugins;
    HashSet<String> m_seenMediaEngines;

    unsigned m_lastSpatialNavigationCandidatesCount;
    unsigned m_framesHandlingBeforeUnloadEvent;

    RefPtr<UserContentController> m_userContentController;
    RefPtr<VisitedLinkStore> m_visitedLinkStore;

    SessionID m_sessionID;
};

inline PageGroup& Page::group()
{
    if (!m_group)
        initGroup();
    return *m_group;
}

} // namespace WebCore
    
#endif // Page_h
