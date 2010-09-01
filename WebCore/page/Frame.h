/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#ifndef Frame_h
#define Frame_h

#include "AnimationController.h"
#include "CSSMutableStyleDeclaration.h"
#include "DragImage.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "ScriptController.h"
#include "ScrollBehavior.h"
#include "SelectionController.h"
#include "UserScriptTypes.h"
#include "ZoomMode.h"

#if PLATFORM(WIN)
#include "FrameWin.h"
#endif

#if ENABLE(TILED_BACKING_STORE)
#include "TiledBackingStoreClient.h"
#endif

#if PLATFORM(MAC)
#ifndef __OBJC__
class NSArray;
class NSDictionary;
class NSMutableDictionary;
class NSString;
typedef int NSWritingDirection;
#endif
#endif

#if PLATFORM(WIN)
typedef struct HBITMAP__* HBITMAP;
#endif

namespace WebCore {

    class HTMLTableCellElement;
    class RegularExpression;
    class RenderPart;
    class TiledBackingStore;

    class Frame : public RefCounted<Frame>
#if ENABLE(TILED_BACKING_STORE)
        , public TiledBackingStoreClient
#endif
    {
    public:
        static PassRefPtr<Frame> create(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);
        void setView(PassRefPtr<FrameView>);
        ~Frame();

        void init();

        Page* page() const;
        void detachFromPage();
        void transferChildFrameToNewDocument();

        HTMLFrameOwnerElement* ownerElement() const;

        void pageDestroyed();
        void disconnectOwnerElement();

        Document* document() const;
        FrameView* view() const;

        void setDOMWindow(DOMWindow*);
        void clearFormerDOMWindow(DOMWindow*);

        // Unlike many of the accessors in Frame, domWindow() always creates a new DOMWindow if m_domWindow is null.
        // Callers that don't need a new DOMWindow to be created should use existingDOMWindow().
        DOMWindow* domWindow() const;
        DOMWindow* existingDOMWindow() { return m_domWindow.get(); }

        Editor* editor() const;
        EventHandler* eventHandler() const;
        FrameLoader* loader() const;
        RedirectScheduler* redirectScheduler() const;
        SelectionController* selection() const;
        FrameTree* tree() const;
        AnimationController* animation() const;
        ScriptController* script();

        RenderView* contentRenderer() const; // root renderer for the document contained in this frame
        RenderPart* ownerRenderer() const; // renderer for the element that contains this frame

        bool isDisconnected() const;
        void setIsDisconnected(bool);
        bool excludeFromTextSearch() const;
        void setExcludeFromTextSearch(bool);

        void createView(const IntSize&, const Color&, bool, const IntSize &, bool,
                        ScrollbarMode = ScrollbarAuto, bool horizontalLock = false,
                        ScrollbarMode = ScrollbarAuto, bool verticalLock = false);

        void injectUserScripts(UserScriptInjectionTime);
        
        String layerTreeAsText() const;

    private:
        void injectUserScriptsForWorld(DOMWrapperWorld*, const UserScriptVector&, UserScriptInjectionTime);

    private:
        Frame(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);

    // === undecided, would like to consider moving to another class

    public:
        static Frame* frameForWidget(const Widget*);

        Settings* settings() const; // can be NULL

        enum AdjustViewSizeOrNot { DoNotAdjustViewSize, AdjustViewSize };
        void setPrinting(bool printing, const FloatSize& pageSize, float maximumShrinkRatio, AdjustViewSizeOrNot);

        bool inViewSourceMode() const;
        void setInViewSourceMode(bool = true);

        void keepAlive(); // Used to keep the frame alive when running a script that might destroy it.
#ifndef NDEBUG
        static void cancelAllKeepAlive();
#endif

        void setDocument(PassRefPtr<Document>);

#if ENABLE(ORIENTATION_EVENTS)
        // Orientation is the interface orientation in degrees. Some examples are:
        //  0 is straight up; -90 is when the device is rotated 90 clockwise;
        //  90 is when rotated counter clockwise.
        void sendOrientationChangeEvent(int orientation);
        int orientation() const { return m_orientation; }
#endif

        void clearTimers();
        static void clearTimers(FrameView*, Document*);

        String documentTypeString() const;

        // This method -- and the corresponding list of former DOM windows --
        // should move onto ScriptController
        void clearDOMWindow();

        String displayStringModifiedByEncoding(const String& str) const
        {
            return document() ? document()->displayStringModifiedByEncoding(str) : str;
        }

#if ENABLE(TILED_BACKING_STORE)
        TiledBackingStore* tiledBackingStore() const { return m_tiledBackingStore.get(); }
        void setTiledBackingStoreEnabled(bool);
#endif

    private:
        void lifeSupportTimerFired(Timer<Frame>*);

    // === to be moved into Editor

    public:
        String selectedText() const;
        bool findString(const String&, bool forward, bool caseFlag, bool wrapFlag, bool startInSelection);

        const VisibleSelection& mark() const; // Mark, to be used as emacs uses it.
        void setMark(const VisibleSelection&);

        void computeAndSetTypingStyle(CSSStyleDeclaration* , EditAction = EditActionUnspecified);
        void applyEditingStyleToBodyElement() const;
        void applyEditingStyleToElement(Element*) const;

        IntRect firstRectForRange(Range*) const;

        void respondToChangedSelection(const VisibleSelection& oldSelection, bool closeTyping);
        bool shouldChangeSelection(const VisibleSelection& oldSelection, const VisibleSelection& newSelection, EAffinity, bool stillSelecting) const;

        RenderStyle* styleForSelectionStart(Node*& nodeToRemove) const;

        unsigned countMatchesForText(const String&, bool caseFlag, unsigned limit, bool markMatches);
        bool markedTextMatchesAreHighlighted() const;
        void setMarkedTextMatchesAreHighlighted(bool flag);

        PassRefPtr<CSSComputedStyleDeclaration> selectionComputedStyle(Node*& nodeToRemove) const;

        void textFieldDidBeginEditing(Element*);
        void textFieldDidEndEditing(Element*);
        void textDidChangeInTextField(Element*);
        bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
        void textWillBeDeletedInTextField(Element* input);
        void textDidChangeInTextArea(Element*);

        DragImageRef nodeImage(Node*);
        DragImageRef dragImageForSelection();

    // === to be moved into SelectionController

    public:
        TextGranularity selectionGranularity() const;

        bool shouldChangeSelection(const VisibleSelection&) const;
        bool shouldDeleteSelection(const VisibleSelection&) const;
        void setFocusedNodeIfNeeded();
        void notifyRendererOfSelectionChange(bool userTriggered);

        void paintDragCaret(GraphicsContext*, int tx, int ty, const IntRect& clipRect) const;

        bool isContentEditable() const; // if true, everything in frame is editable

        CSSMutableStyleDeclaration* typingStyle() const;
        void setTypingStyle(CSSMutableStyleDeclaration*);
        void clearTypingStyle();

        FloatRect selectionBounds(bool clipToVisibleContent = true) const;
        enum SelectionRectRespectTransforms { RespectTransforms = true, IgnoreTransforms = false };
        void selectionTextRects(Vector<FloatRect>&, SelectionRectRespectTransforms respectTransforms, bool clipToVisibleContent = true) const;

        HTMLFormElement* currentForm() const;

        void revealSelection(const ScrollAlignment& = ScrollAlignment::alignCenterIfNeeded, bool revealExtent = false);
        void setSelectionFromNone();

        SelectionController* dragCaretController() const;

        String searchForLabelsAboveCell(RegularExpression*, HTMLTableCellElement*, size_t* resultDistanceFromStartOfCell);
        String searchForLabelsBeforeElement(const Vector<String>& labels, Element*, size_t* resultDistance, bool* resultIsInCellAbove);
        String matchLabelsAgainstElement(const Vector<String>& labels, Element*);

        VisiblePosition visiblePositionForPoint(const IntPoint& framePoint);
        Document* documentAtPoint(const IntPoint& windowPoint);

#if ENABLE(TILED_BACKING_STORE)

    private:
        // TiledBackingStoreClient interface
        virtual void tiledBackingStorePaintBegin();
        virtual void tiledBackingStorePaint(GraphicsContext*, const IntRect&);
        virtual void tiledBackingStorePaintEnd(const Vector<IntRect>& paintedArea);
        virtual IntRect tiledBackingStoreContentsRect();
        virtual IntRect tiledBackingStoreVisibleRect();
#endif

#if PLATFORM(MAC)

    // === undecided, would like to consider moving to another class

    public:
        NSString* searchForNSLabelsAboveCell(RegularExpression*, HTMLTableCellElement*, size_t* resultDistanceFromStartOfCell);
        NSString* searchForLabelsBeforeElement(NSArray* labels, Element*, size_t* resultDistance, bool* resultIsInCellAbove);
        NSString* matchLabelsAgainstElement(NSArray* labels, Element*);

#if ENABLE(DASHBOARD_SUPPORT)
        NSMutableDictionary* dashboardRegionsDictionary();
#endif

        NSImage* selectionImage(bool forceBlackText = false) const;
        NSImage* snapshotDragImage(Node*, NSRect* imageRect, NSRect* elementRect) const;

    private:
        NSImage* imageFromRect(NSRect) const;

    // === to be moved into Editor

    public:
        NSDictionary* fontAttributesForSelectionStart() const;
        NSWritingDirection baseWritingDirectionForSelectionStart() const;

#endif

    private:
        Page* m_page;
        mutable FrameTree m_treeNode;
        mutable FrameLoader m_loader;
        mutable RedirectScheduler m_redirectScheduler;

        mutable RefPtr<DOMWindow> m_domWindow;
        HashSet<DOMWindow*> m_liveFormerWindows;

        HTMLFrameOwnerElement* m_ownerElement;
        RefPtr<FrameView> m_view;
        RefPtr<Document> m_doc;

        ScriptController m_script;

        mutable VisibleSelection m_mark;
        mutable Editor m_editor;
        mutable SelectionController m_selectionController;
        mutable EventHandler m_eventHandler;
        mutable AnimationController m_animationController;

        RefPtr<CSSMutableStyleDeclaration> m_typingStyle;

        Timer<Frame> m_lifeSupportTimer;

#if ENABLE(ORIENTATION_EVENTS)
        int m_orientation;
#endif

        bool m_highlightTextMatches;
        bool m_inViewSourceMode;
        bool m_isDisconnected;
        bool m_excludeFromTextSearch;

#if ENABLE(TILED_BACKING_STORE)        
        OwnPtr<TiledBackingStore> m_tiledBackingStore;
#endif
    };

    inline void Frame::init()
    {
        m_loader.init();
    }

    inline FrameLoader* Frame::loader() const
    {
        return &m_loader;
    }

    inline RedirectScheduler* Frame::redirectScheduler() const
    {
        return &m_redirectScheduler;
    }

    inline FrameView* Frame::view() const
    {
        return m_view.get();
    }

    inline ScriptController* Frame::script()
    {
        return &m_script;
    }

    inline Document* Frame::document() const
    {
        return m_doc.get();
    }

    inline SelectionController* Frame::selection() const
    {
        return &m_selectionController;
    }

    inline Editor* Frame::editor() const
    {
        return &m_editor;
    }

    inline AnimationController* Frame::animation() const
    {
        return &m_animationController;
    }

    inline const VisibleSelection& Frame::mark() const
    {
        return m_mark;
    }

    inline void Frame::setMark(const VisibleSelection& s)
    {
        ASSERT(!s.base().node() || s.base().node()->document() == document());
        ASSERT(!s.extent().node() || s.extent().node()->document() == document());
        ASSERT(!s.start().node() || s.start().node()->document() == document());
        ASSERT(!s.end().node() || s.end().node()->document() == document());

        m_mark = s;
    }

    inline CSSMutableStyleDeclaration* Frame::typingStyle() const
    {
        return m_typingStyle.get();
    }

    inline void Frame::clearTypingStyle()
    {
        m_typingStyle = 0;
    }

    inline HTMLFrameOwnerElement* Frame::ownerElement() const
    {
        return m_ownerElement;
    }

    inline bool Frame::isDisconnected() const
    {
        return m_isDisconnected;
    }

    inline void Frame::setIsDisconnected(bool isDisconnected)
    {
        m_isDisconnected = isDisconnected;
    }

    inline bool Frame::excludeFromTextSearch() const
    {
        return m_excludeFromTextSearch;
    }

    inline void Frame::setExcludeFromTextSearch(bool exclude)
    {
        m_excludeFromTextSearch = exclude;
    }

    inline bool Frame::inViewSourceMode() const
    {
        return m_inViewSourceMode;
    }

    inline void Frame::setInViewSourceMode(bool mode)
    {
        m_inViewSourceMode = mode;
    }

    inline bool Frame::markedTextMatchesAreHighlighted() const
    {
        return m_highlightTextMatches;
    }

    inline FrameTree* Frame::tree() const
    {
        return &m_treeNode;
    }

    inline Page* Frame::page() const
    {
        return m_page;
    }

    inline void Frame::detachFromPage()
    {
        m_page = 0;
    }

    inline EventHandler* Frame::eventHandler() const
    {
        return &m_eventHandler;
    }

} // namespace WebCore

#endif // Frame_h
