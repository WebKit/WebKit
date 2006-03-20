/* This file is part of the KDE project

   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef FRAMEVIEW_H
#define FRAMEVIEW_H

#include "DeprecatedString.h"
#include "ScrollView.h"

class DeprecatedStringList;

namespace WebCore {

class AtomicString;
class CSSProperty;
class CSSStyleSelector;
class Clipboard;
class Document;
class Element;
class Frame;
class FrameViewPrivate;
class GraphicsContext;
class HTMLAnchorElement;
class HTMLDocument;
class HTMLElement;
class HTMLFormElement;
class HTMLFrameSetElement;
class HTMLGenericFormElement;
class HTMLTitleElement;
class InlineBox;
class IntRect;
class PlatformKeyboardEvent;
class FrameMac;
class PlatformMouseEvent;
class Node;
class RenderBox;
class RenderCanvas;
class RenderLineEdit;
class RenderObject;
class RenderPart;
class RenderPartObject;
class RenderStyle;
class RenderWidget;
class PlatformWheelEvent;

template <typename T> class Timer;

void applyRule(CSSProperty*);

class FrameView : public ScrollView {
    friend class CSSStyleSelector;
    friend class Document;
    friend class Frame;
    friend class HTMLAnchorElement;
    friend class HTMLDocument;
    friend class HTMLFormElement;
    friend class HTMLGenericFormElement;
    friend class HTMLTitleElement;
    friend class FrameMac;
    friend class RenderBox;
    friend class RenderCanvas;
    friend class RenderLineEdit;
    friend class RenderObject;
    friend class RenderPart;
    friend class RenderPartObject;
    friend class RenderWidget;
    friend void applyRule(CSSProperty *prop);

public:
    FrameView(Frame*);
    virtual ~FrameView();

    Frame* frame() const { return m_frame.get(); }

    int frameWidth() const { return _width; }

    void setMarginWidth(int x);

    /**
     * Returns the margin width.
     *
     * A return value of -1 means the default value will be used.
     */
    int marginWidth() const { return _marginWidth; }

    void setMarginHeight(int y);

    /**
     * Returns the margin height.
     *
     * A return value of -1 means the default value will be used.
     */
    int marginHeight() { return _marginHeight; }

    virtual void setVScrollBarMode(ScrollBarMode);
    virtual void setHScrollBarMode(ScrollBarMode);
    virtual void setScrollBarsMode(ScrollBarMode);
    
    void print();

    void layout();

    bool inLayout() const;
    int layoutCount() const;

    bool needsFullRepaint() const;
    
    void addRepaintInfo(RenderObject* o, const IntRect& r);

    void resetScrollBars();

     void clear();

public:
    void clearPart();

    void handleMousePressEvent(const PlatformMouseEvent&);
    void handleMouseDoubleClickEvent(const PlatformMouseEvent&);
    void handleMouseMoveEvent(const PlatformMouseEvent&);
    void handleMouseReleaseEvent(const PlatformMouseEvent&);
    void handleWheelEvent(PlatformWheelEvent&);

    void doAutoScroll();

    bool updateDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    void cancelDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    bool performDragAndDrop(const PlatformMouseEvent&, Clipboard*);

    void layoutTimerFired(Timer<FrameView>*);
    void hoverTimerFired(Timer<FrameView>*);

    void repaintRectangle(const IntRect& r, bool immediate);

    bool isTransparent() const;
    void setTransparent(bool isTransparent);
    
    void scheduleRelayout();
    void unscheduleRelayout();
    bool haveDelayedLayoutScheduled();
    bool layoutPending();

    void scheduleHoverStateUpdate();

    void adjustViewSize();
    void initScrollBars();
    
    void setHasBorder(bool);
    bool hasBorder() const;
    
    void setResizingFrameSet(HTMLFrameSetElement *);

#if __APPLE__
    void updateDashboardRegions();
#endif

    void ref() { ++_refCount; }
    void deref() { if (!--_refCount) delete this; }
    
private:
    void cleared();
    void scrollBarMoved();

    void resetCursor();
    void invalidateClick();

    /**
     * Get/set the CSS Media Type.
     *
     * Media type is set to "screen" for on-screen rendering and "print"
     * during printing. Other media types lack the proper support in the
     * renderer and are not activated. The DOM tree and the parser itself,
     * however, properly handle other media types. To make them actually work
     * you only need to enable the media type in the view and if necessary
     * add the media type dependent changes to the renderer.
     */
    void setMediaType(const DeprecatedString&);
    DeprecatedString mediaType() const;

    bool scrollTo(const IntRect&);

    void focusNextPrevNode(bool next);

    void useSlowRepaints();

    void setIgnoreWheelEvents(bool e);

    void init();

    Node *nodeUnderMouse() const;

    void restoreScrollBar();

    DeprecatedStringList formCompletionItems(const DeprecatedString &name) const;
    void addFormCompletionItem(const DeprecatedString &name, const DeprecatedString &value);

    bool dispatchMouseEvent(const AtomicString& eventType, Node* target,
        bool cancelable, int clickCount, const PlatformMouseEvent&, bool setUnder);
    bool dispatchDragEvent(const AtomicString& eventType, Node* target,
        const PlatformMouseEvent&, Clipboard*);

    void applyOverflowToViewport(RenderObject* o, ScrollBarMode& hMode, ScrollBarMode& vMode);

    virtual bool isFrameView() const;

    void updateBorder();

    unsigned _refCount;

    int _width;
    int _height;

    int _marginWidth;
    int _marginHeight;

    RefPtr<Frame> m_frame;
    FrameViewPrivate* d;

    DeprecatedString m_medium; // media type
};

}

#endif

