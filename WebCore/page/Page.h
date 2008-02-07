// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "PlatformString.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>

#if PLATFORM(WIN) || (PLATFORM(WX) && PLATFORM(WIN_OS)) 
typedef struct HINSTANCE__* HINSTANCE;
#endif

typedef enum TextCaseSensitivity {
    TextCaseSensitive,
    TextCaseInsensitive
};

typedef enum FindDirection {
    FindDirectionForward,
    FindDirectionBackward
};

namespace WebCore {

    class Chrome;
    class ChromeClient;
    class ContextMenuClient;
    class ContextMenuController;
    class DragClient;
    class DragController;
    class EditorClient;
    class FocusController;
    class Frame;
    class InspectorClient;
    class InspectorController;
    class Node;
    class ProgressTracker;
    class Selection;
    class SelectionController;
    class Settings;

    class Page : Noncopyable {
    public:
        static void setNeedsReapplyStyles();
        static const HashSet<Page*>* frameNamespace(const String&);

        Page(ChromeClient*, ContextMenuClient*, EditorClient*, DragClient*, InspectorClient*);
        ~Page();
        
        EditorClient* editorClient() const { return m_editorClient; }

        void setMainFrame(PassRefPtr<Frame>);
        Frame* mainFrame() const { return m_mainFrame.get(); }

        BackForwardList* backForwardList();

        // FIXME: The following three methods don't fall under the responsibilities of the Page object
        // They seem to fit a hypothetical Page-controller object that would be akin to the 
        // Frame-FrameLoader relationship.  They have to live here now, but should move somewhere that
        // makes more sense when that class exists.
        bool goBack();
        bool goForward();
        void goToItem(HistoryItem*, FrameLoadType);
        
        void setGroupName(const String&);
        String groupName() const { return m_groupName; }

        const HashSet<Page*>* frameNamespace() const;

        void incrementFrameCount() { ++m_frameCount; }
        void decrementFrameCount() { --m_frameCount; }
        int frameCount() const { return m_frameCount; }

        Chrome* chrome() const { return m_chrome.get(); }
        SelectionController* dragCaretController() const { return m_dragCaretController.get(); }
        DragController* dragController() const { return m_dragController.get(); }
        FocusController* focusController() const { return m_focusController.get(); }
        ContextMenuController* contextMenuController() const { return m_contextMenuController.get(); }
        InspectorController* inspectorController() const { return m_inspectorController.get(); }
        Settings* settings() const { return m_settings.get(); }
        ProgressTracker* progress() const { return m_progress.get(); }

        void setParentInspectorController(InspectorController* controller) { m_parentInspectorController = controller; }
        InspectorController* parentInspectorController() const { return m_parentInspectorController; }
        
        void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
        bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

        bool findString(const String&, TextCaseSensitivity, FindDirection, bool shouldWrap);
        unsigned int markAllMatchesForText(const String&, TextCaseSensitivity, bool shouldHighlight, unsigned);
        void unmarkAllTextMatches();

        const Selection& selection() const;

        void setDefersLoading(bool);
        bool defersLoading() const { return m_defersLoading; }
        
        void clearUndoRedoOperations();

        bool inLowQualityImageInterpolationMode() const;
        void setInLowQualityImageInterpolationMode(bool = true);

        void userStyleSheetLocationChanged();
        const String& userStyleSheet() const;

#if PLATFORM(WIN) || (PLATFORM(WX) && PLATFORM(WIN_OS))
        // The global DLL or application instance used for all windows.
        static void setInstanceHandle(HINSTANCE instanceHandle) { s_instanceHandle = instanceHandle; }
        static HINSTANCE instanceHandle() { return s_instanceHandle; }
#endif

    private:
        OwnPtr<Chrome> m_chrome;
        OwnPtr<SelectionController> m_dragCaretController;
        OwnPtr<DragController> m_dragController;
        OwnPtr<FocusController> m_focusController;
        OwnPtr<ContextMenuController> m_contextMenuController;
        OwnPtr<InspectorController> m_inspectorController;
        OwnPtr<Settings> m_settings;
        OwnPtr<ProgressTracker> m_progress;
        
        RefPtr<BackForwardList> m_backForwardList;
        RefPtr<Frame> m_mainFrame;
        RefPtr<Node> m_focusedNode;

        EditorClient* m_editorClient;

        int m_frameCount;
        String m_groupName;

        bool m_tabKeyCyclesThroughElements;
        bool m_defersLoading;

        bool m_inLowQualityInterpolationMode;
    
        InspectorController* m_parentInspectorController;

        String m_userStyleSheetPath;
        mutable String m_userStyleSheet;
        mutable bool m_didLoadUserStyleSheet;
        mutable time_t m_userStyleSheetModificationTime;

#if PLATFORM(WIN) || (PLATFORM(WX) && defined(__WXMSW__))
        static HINSTANCE s_instanceHandle;
#endif
    };

} // namespace WebCore
    
#endif // Page_h
