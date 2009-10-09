/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#include "BackForwardList.h"
#include "Chrome.h"
#include "ContextMenuController.h"
#include "FrameLoaderTypes.h"
#include "LinkHash.h"
#include "PlatformString.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>

#if PLATFORM(MAC)
#include "SchedulePair.h"
#endif

#if PLATFORM(WIN) || (PLATFORM(WX) && PLATFORM(WIN_OS)) || (PLATFORM(QT) && defined(Q_WS_WIN))
typedef struct HINSTANCE__* HINSTANCE;
#endif

namespace JSC {
    class Debugger;
}

namespace WebCore {

    class Chrome;
    class ChromeClient;
    class ContextMenuClient;
    class ContextMenuController;
    class Document;
    class DragClient;
    class DragController;
    class EditorClient;
    class FocusController;
    class Frame;
    class HaltablePlugin;
    class InspectorClient;
    class InspectorController;
    class InspectorTimelineAgent;
    class Node;
    class PageGroup;
    class PluginData;
    class PluginHalter;
    class PluginHalterClient;
    class PluginView;
    class ProgressTracker;
    class RenderTheme;
    class VisibleSelection;
    class SelectionController;
    class Settings;
#if ENABLE(DOM_STORAGE)
    class StorageNamespace;
#endif
#if ENABLE(WML)
    class WMLPageState;
#endif
#if ENABLE(NOTIFICATIONS)
    class NotificationPresenter;
#endif

    enum FindDirection { FindDirectionForward, FindDirectionBackward };

    class Page : public Noncopyable {
    public:
        static void setNeedsReapplyStyles();

        Page(ChromeClient*, ContextMenuClient*, EditorClient*, DragClient*, InspectorClient*, PluginHalterClient*);
        ~Page();

        RenderTheme* theme() const { return m_theme.get(); };

        static void refreshPlugins(bool reload);
        PluginData* pluginData() const;

        void setCanStartPlugins(bool);
        bool canStartPlugins() const { return m_canStartPlugins; }
        void addUnstartedPlugin(PluginView*);
        void removeUnstartedPlugin(PluginView*);

        EditorClient* editorClient() const { return m_editorClient; }

        void setMainFrame(PassRefPtr<Frame>);
        Frame* mainFrame() const { return m_mainFrame.get(); }

        bool openedByDOM() const;
        void setOpenedByDOM();

        BackForwardList* backForwardList();

        // FIXME: The following three methods don't fall under the responsibilities of the Page object
        // They seem to fit a hypothetical Page-controller object that would be akin to the 
        // Frame-FrameLoader relationship.  They have to live here now, but should move somewhere that
        // makes more sense when that class exists.
        bool goBack();
        bool goForward();
        bool canGoBackOrForward(int distance) const;
        void goBackOrForward(int distance);
        void goToItem(HistoryItem*, FrameLoadType);
        int getHistoryLength();

        HistoryItem* globalHistoryItem() const { return m_globalHistoryItem.get(); }
        void setGlobalHistoryItem(HistoryItem*);

        void setGroupName(const String&);
        const String& groupName() const;

        PageGroup& group() { if (!m_group) initGroup(); return *m_group; }
        PageGroup* groupPtr() { return m_group; } // can return 0

        void incrementFrameCount() { ++m_frameCount; }
        void decrementFrameCount() { --m_frameCount; }
        int frameCount() const { return m_frameCount; }

        Chrome* chrome() const { return m_chrome.get(); }
        SelectionController* dragCaretController() const { return m_dragCaretController.get(); }
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
        Settings* settings() const { return m_settings.get(); }
        ProgressTracker* progress() const { return m_progress.get(); }

#if ENABLE(INSPECTOR)
        void setParentInspectorController(InspectorController* controller) { m_parentInspectorController = controller; }
        InspectorController* parentInspectorController() const { return m_parentInspectorController; }
#endif
        
        void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
        bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

        bool findString(const String&, TextCaseSensitivity, FindDirection, bool shouldWrap);
        unsigned int markAllMatchesForText(const String&, TextCaseSensitivity, bool shouldHighlight, unsigned);
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

        bool cookieEnabled() const { return m_cookieEnabled; }
        void setCookieEnabled(bool enabled) { m_cookieEnabled = enabled; }

        float mediaVolume() const { return m_mediaVolume; }
        void setMediaVolume(float volume);

        // Notifications when the Page starts and stops being presented via a native window.
        void didMoveOnscreen();
        void willMoveOffscreen();

        void userStyleSheetLocationChanged();
        const String& userStyleSheet() const;

        void didStartPlugin(HaltablePlugin*);
        void didStopPlugin(HaltablePlugin*);
        void pluginAllowedRunTimeChanged();

        static void setDebuggerForAllPages(JSC::Debugger*);
        void setDebugger(JSC::Debugger*);
        JSC::Debugger* debugger() const { return m_debugger; }

#if PLATFORM(WIN) || (PLATFORM(WX) && PLATFORM(WIN_OS)) || (PLATFORM(QT) && defined(Q_WS_WIN))
        // The global DLL or application instance used for all windows.
        static void setInstanceHandle(HINSTANCE instanceHandle) { s_instanceHandle = instanceHandle; }
        static HINSTANCE instanceHandle() { return s_instanceHandle; }
#endif

        static void removeAllVisitedLinks();

        static void allVisitedStateChanged(PageGroup*);
        static void visitedStateChanged(PageGroup*, LinkHash visitedHash);

#if ENABLE(DOM_STORAGE)
        StorageNamespace* sessionStorage(bool optionalCreate = true);
        void setSessionStorage(PassRefPtr<StorageNamespace>);
#endif

#if ENABLE(WML)
        WMLPageState* wmlPageState();
#endif

        void setCustomHTMLTokenizerTimeDelay(double);
        bool hasCustomHTMLTokenizerTimeDelay() const { return m_customHTMLTokenizerTimeDelay != -1; }
        double customHTMLTokenizerTimeDelay() const { ASSERT(m_customHTMLTokenizerTimeDelay != -1); return m_customHTMLTokenizerTimeDelay; }

        void setCustomHTMLTokenizerChunkSize(int);
        bool hasCustomHTMLTokenizerChunkSize() const { return m_customHTMLTokenizerChunkSize != -1; }
        int customHTMLTokenizerChunkSize() const { ASSERT(m_customHTMLTokenizerChunkSize != -1); return m_customHTMLTokenizerChunkSize; }

        void setMemoryCacheClientCallsEnabled(bool);
        bool areMemoryCacheClientCallsEnabled() const { return m_areMemoryCacheClientCallsEnabled; }

        void setJavaScriptURLsAreAllowed(bool);
        bool javaScriptURLsAreAllowed() const;

#if ENABLE(INSPECTOR)
        InspectorTimelineAgent* inspectorTimelineAgent() const;
#endif
    private:
        void initGroup();

        OwnPtr<Chrome> m_chrome;
        OwnPtr<SelectionController> m_dragCaretController;
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
        OwnPtr<Settings> m_settings;
        OwnPtr<ProgressTracker> m_progress;
        
        RefPtr<BackForwardList> m_backForwardList;
        RefPtr<Frame> m_mainFrame;

        RefPtr<HistoryItem> m_globalHistoryItem;

        mutable RefPtr<PluginData> m_pluginData;

        RefPtr<RenderTheme> m_theme;

        EditorClient* m_editorClient;

        int m_frameCount;
        String m_groupName;
        bool m_openedByDOM;

        bool m_tabKeyCyclesThroughElements;
        bool m_defersLoading;

        bool m_inLowQualityInterpolationMode;
        bool m_cookieEnabled;
        bool m_areMemoryCacheClientCallsEnabled;
        float m_mediaVolume;

        bool m_javaScriptURLsAreAllowed;

#if ENABLE(INSPECTOR)
        InspectorController* m_parentInspectorController;
#endif

        String m_userStyleSheetPath;
        mutable String m_userStyleSheet;
        mutable bool m_didLoadUserStyleSheet;
        mutable time_t m_userStyleSheetModificationTime;

        OwnPtr<PageGroup> m_singlePageGroup;
        PageGroup* m_group;

        JSC::Debugger* m_debugger;

        double m_customHTMLTokenizerTimeDelay;
        int m_customHTMLTokenizerChunkSize;

        bool m_canStartPlugins;
        HashSet<PluginView*> m_unstartedPlugins;

        OwnPtr<PluginHalter> m_pluginHalter;

#if ENABLE(DOM_STORAGE)
        RefPtr<StorageNamespace> m_sessionStorage;
#endif

#if PLATFORM(WIN) || (PLATFORM(WX) && defined(__WXMSW__)) || (PLATFORM(QT) && defined(Q_WS_WIN))
        static HINSTANCE s_instanceHandle;
#endif

#if ENABLE(WML)
        OwnPtr<WMLPageState> m_wmlPageState;
#endif

#if ENABLE(NOTIFICATIONS)
        NotificationPresenter* m_notificationPresenter;
#endif
    };

} // namespace WebCore
    
#endif // Page_h
