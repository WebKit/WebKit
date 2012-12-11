/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "FeatureObserver.h"
#include "FrameLoaderTypes.h"
#include "FindOptions.h"
#include "LayoutMilestones.h"
#include "LayoutRect.h"
#include "PageVisibilityState.h"
#include "Pagination.h"
#include "PlatformScreen.h"
#include "Region.h"
#include "Supplementable.h"
#include "ViewportArguments.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

#if OS(SOLARIS)
#include <sys/time.h> // For time_t structure.
#endif

#if PLATFORM(MAC)
#include "SchedulePair.h"
#endif

namespace JSC {
    class Debugger;
}

namespace WebCore {

    class AlternativeTextClient;
    class BackForwardController;
    class BackForwardList;
    class Chrome;
    class ChromeClient;
#if ENABLE(CONTEXT_MENUS)
    class ContextMenuClient;
    class ContextMenuController;
#endif
    class Document;
    class DragCaretController;
    class DragClient;
    class DragController;
    class EditorClient;
    class FocusController;
    class Frame;
    class FrameSelection;
    class HaltablePlugin;
    class HistoryItem;
    class InspectorClient;
    class InspectorController;
    class MediaCanStartListener;
    class Node;
    class PageGroup;
    class PlugInClient;
    class PluginData;
    class PluginViewBase;
    class PointerLockController;
    class ProgressTracker;
    class Range;
    class RenderObject;
    class RenderTheme;
    class VisibleSelection;
    class ScrollableArea;
    class ScrollingCoordinator;
    class Settings;
    class StorageNamespace;
    class ValidationMessageClient;

    typedef uint64_t LinkHash;

    enum FindDirection { FindDirectionForward, FindDirectionBackward };

    float deviceScaleFactor(Frame*);

    struct ArenaSize {
        ArenaSize(size_t treeSize, size_t allocated)
            : treeSize(treeSize)
            , allocated(allocated)
        {
        }
        size_t treeSize;
        size_t allocated;
    };

    class Page : public Supplementable<Page> {
        WTF_MAKE_NONCOPYABLE(Page);
        friend class Settings;
    public:
        static void scheduleForcedStyleRecalcForAllPages();

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
            RefPtr<BackForwardList> backForwardClient;
            ValidationMessageClient* validationMessageClient;
        };

        explicit Page(PageClients&);
        ~Page();

        ArenaSize renderTreeSize() const;

        void setNeedsRecalcStyleInAllFrames();

        RenderTheme* theme() const { return m_theme.get(); };

        ViewportArguments viewportArguments() const;

        static void refreshPlugins(bool reload);
        PluginData* pluginData() const;

        void setCanStartMedia(bool);
        bool canStartMedia() const { return m_canStartMedia; }

        EditorClient* editorClient() const { return m_editorClient; }
        PlugInClient* plugInClient() const { return m_plugInClient; }

        void setMainFrame(PassRefPtr<Frame>);
        Frame* mainFrame() const { return m_mainFrame.get(); }

        bool openedByDOM() const;
        void setOpenedByDOM();

        // DEPRECATED. Use backForward() instead of the following 6 functions.
        BackForwardList* backForwardList() const;
        bool goBack();
        bool goForward();
        bool canGoBackOrForward(int distance) const;
        void goBackOrForward(int distance);
        int getHistoryLength();

        void goToItem(HistoryItem*, FrameLoadType);

        void setGroupName(const String&);
        const String& groupName() const;

        PageGroup& group() { if (!m_group) initGroup(); return *m_group; }
        PageGroup* groupPtr() { return m_group; } // can return 0

        void incrementSubframeCount() { ++m_subframeCount; }
        void decrementSubframeCount() { ASSERT(m_subframeCount); --m_subframeCount; }
        int subframeCount() const { checkSubframeCountConsistency(); return m_subframeCount; }

        Chrome* chrome() const { return m_chrome.get(); }
        DragCaretController* dragCaretController() const { return m_dragCaretController.get(); }
#if ENABLE(DRAG_SUPPORT)
        DragController* dragController() const { return m_dragController.get(); }
#endif
        FocusController* focusController() const { return m_focusController.get(); }
#if ENABLE(CONTEXT_MENUS)
        ContextMenuController* contextMenuController() const { return m_contextMenuController.get(); }
#endif
#if ENABLE(INSPECTOR)
        InspectorController* inspectorController() const { return m_inspectorController.get(); }
#endif
#if ENABLE(POINTER_LOCK)
        PointerLockController* pointerLockController() const { return m_pointerLockController.get(); }
#endif
        ValidationMessageClient* validationMessageClient() const { return m_validationMessageClient; }

        ScrollingCoordinator* scrollingCoordinator();

        String scrollingStateTreeAsText();

        Settings* settings() const { return m_settings.get(); }
        ProgressTracker* progress() const { return m_progress.get(); }
        BackForwardController* backForward() const { return m_backForwardController.get(); }

        FeatureObserver* featureObserver() { return &m_featureObserver; }

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
        
        void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
        bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

        bool findString(const String&, FindOptions);
        // FIXME: Switch callers over to the FindOptions version and retire this one.
        bool findString(const String&, TextCaseSensitivity, FindDirection, bool shouldWrap);

        PassRefPtr<Range> rangeOfString(const String&, Range*, FindOptions);

        unsigned markAllMatchesForText(const String&, FindOptions, bool shouldHighlight, unsigned);
        // FIXME: Switch callers over to the FindOptions version and retire this one.
        unsigned markAllMatchesForText(const String&, TextCaseSensitivity, bool shouldHighlight, unsigned);
        void unmarkAllTextMatches();

#if PLATFORM(MAC)
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
        void setMediaVolume(float volume);

        void setPageScaleFactor(float scale, const IntPoint& origin);
        float pageScaleFactor() const { return m_pageScaleFactor; }

        float deviceScaleFactor() const { return m_deviceScaleFactor; }
        void setDeviceScaleFactor(float);

        bool shouldSuppressScrollbarAnimations() const { return m_suppressScrollbarAnimations; }
        void setShouldSuppressScrollbarAnimations(bool suppressAnimations);

        // Page and FrameView both store a Pagination value. Page::pagination() is set only by API,
        // and FrameView::pagination() is set only by CSS. Page::pagination() will affect all
        // FrameViews in the page cache, but FrameView::pagination() only affects the current
        // FrameView. 
        const Pagination& pagination() const { return m_pagination; }
        void setPagination(const Pagination&);

        unsigned pageCount() const;

        // Notifications when the Page starts and stops being presented via a native window.
        void didMoveOnscreen();
        void willMoveOffscreen();
        bool isOnscreen() const { return m_isOnscreen; }

        void windowScreenDidChange(PlatformDisplayID);
        
        void suspendScriptedAnimations();
        void resumeScriptedAnimations();
        bool scriptedAnimationsSuspended() const { return m_scriptedAnimationsSuspended; }
        
        void userStyleSheetLocationChanged();
        const String& userStyleSheet() const;

        void dnsPrefetchingStateChanged();
        void storageBlockingStateChanged();
        void privateBrowsingStateChanged();

        static void setDebuggerForAllPages(JSC::Debugger*);
        void setDebugger(JSC::Debugger*);
        JSC::Debugger* debugger() const { return m_debugger; }

        static void removeAllVisitedLinks();

        static void allVisitedStateChanged(PageGroup*);
        static void visitedStateChanged(PageGroup*, LinkHash visitedHash);

        StorageNamespace* sessionStorage(bool optionalCreate = true);
        void setSessionStorage(PassRefPtr<StorageNamespace>);

        void setCustomHTMLTokenizerTimeDelay(double);
        bool hasCustomHTMLTokenizerTimeDelay() const { return m_customHTMLTokenizerTimeDelay != -1; }
        double customHTMLTokenizerTimeDelay() const { ASSERT(m_customHTMLTokenizerTimeDelay != -1); return m_customHTMLTokenizerTimeDelay; }

        void setCustomHTMLTokenizerChunkSize(int);
        bool hasCustomHTMLTokenizerChunkSize() const { return m_customHTMLTokenizerChunkSize != -1; }
        int customHTMLTokenizerChunkSize() const { ASSERT(m_customHTMLTokenizerChunkSize != -1); return m_customHTMLTokenizerChunkSize; }

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
#if ENABLE(PAGE_VISIBILITY_API) || ENABLE(HIDDEN_PAGE_DOM_TIMER_THROTTLING)
        void setVisibilityState(PageVisibilityState, bool);
#endif

        PlatformDisplayID displayID() const { return m_displayID; }

        void addLayoutMilestones(LayoutMilestones);
        LayoutMilestones layoutMilestones() const { return m_layoutMilestones; }

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

        void reportMemoryUsage(MemoryObjectInfo*) const;

    private:
        void initGroup();

#if ASSERT_DISABLED
        void checkSubframeCountConsistency() const { }
#else
        void checkSubframeCountConsistency() const;
#endif

        MediaCanStartListener* takeAnyMediaCanStartListener();

        void setMinimumTimerInterval(double);
        double minimumTimerInterval() const;

        void setTimerAlignmentInterval(double);
        double timerAlignmentInterval() const;

        void collectPluginViews(Vector<RefPtr<PluginViewBase>, 32>& pluginViewBases);

        OwnPtr<Chrome> m_chrome;
        OwnPtr<DragCaretController> m_dragCaretController;

#if ENABLE(DRAG_SUPPORT)
        OwnPtr<DragController> m_dragController;
#endif
        OwnPtr<FocusController> m_focusController;
#if ENABLE(CONTEXT_MENUS)
        OwnPtr<ContextMenuController> m_contextMenuController;
#endif
#if ENABLE(INSPECTOR)
        OwnPtr<InspectorController> m_inspectorController;
#endif
#if ENABLE(POINTER_LOCK)
        OwnPtr<PointerLockController> m_pointerLockController;
#endif
        RefPtr<ScrollingCoordinator> m_scrollingCoordinator;

        OwnPtr<Settings> m_settings;
        OwnPtr<ProgressTracker> m_progress;
        
        OwnPtr<BackForwardController> m_backForwardController;
        RefPtr<Frame> m_mainFrame;

        mutable RefPtr<PluginData> m_pluginData;

        RefPtr<RenderTheme> m_theme;

        EditorClient* m_editorClient;
        PlugInClient* m_plugInClient;
        ValidationMessageClient* m_validationMessageClient;

        FeatureObserver m_featureObserver;

        int m_subframeCount;
        String m_groupName;
        bool m_openedByDOM;

        bool m_tabKeyCyclesThroughElements;
        bool m_defersLoading;
        unsigned m_defersLoadingCallCount;

        bool m_inLowQualityInterpolationMode;
        bool m_cookieEnabled;
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

        OwnPtr<PageGroup> m_singlePageGroup;
        PageGroup* m_group;

        JSC::Debugger* m_debugger;

        double m_customHTMLTokenizerTimeDelay;
        int m_customHTMLTokenizerChunkSize;

        bool m_canStartMedia;

        RefPtr<StorageNamespace> m_sessionStorage;

        ViewMode m_viewMode;

        double m_minimumTimerInterval;

        double m_timerAlignmentInterval;

        bool m_isEditable;
        bool m_isOnscreen;

#if ENABLE(PAGE_VISIBILITY_API)
        PageVisibilityState m_visibilityState;
#endif
        PlatformDisplayID m_displayID;

        LayoutMilestones m_layoutMilestones;

        HashSet<RenderObject*> m_relevantUnpaintedRenderObjects;
        Region m_relevantPaintedRegion;
        Region m_relevantUnpaintedRegion;
        bool m_isCountingRelevantRepaintedObjects;
#ifndef NDEBUG
        bool m_isPainting;
#endif
        AlternativeTextClient* m_alternativeTextClient;

        bool m_scriptedAnimationsSuspended;

        HashSet<String> m_seenPlugins;
        HashSet<String> m_seenMediaEngines;
    };

} // namespace WebCore
    
#endif // Page_h
