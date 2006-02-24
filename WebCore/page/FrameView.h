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

#include "QString.h"
#include "ScrollView.h"

class QStringList;

namespace WebCore {

class AtomicString;
class CSSProperty;
class CSSStyleSelector;
class ClipboardImpl;
class DocumentImpl;
class ElementImpl;
class Frame;
class FrameViewPrivate;
class HTMLAnchorElementImpl;
class HTMLDocumentImpl;
class HTMLElementImpl;
class HTMLFormElementImpl;
class HTMLGenericFormElementImpl;
class HTMLTitleElementImpl;
class InlineBox;
class IntRect;
class KeyEvent;
class MacFrame;
class MouseEvent;
class NodeImpl;
class QPainter;
class RenderBox;
class RenderCanvas;
class RenderLineEdit;
class RenderObject;
class RenderPart;
class RenderPartObject;
class RenderStyle;
class RenderWidget;
class WheelEvent;

template <typename T> class Timer;

void applyRule(CSSProperty*);

class FrameView : public ScrollView {
    friend class CSSStyleSelector;
    friend class DocumentImpl;
    friend class Frame;
    friend class HTMLAnchorElementImpl;
    friend class HTMLDocumentImpl;
    friend class HTMLFormElementImpl;
    friend class HTMLGenericFormElementImpl;
    friend class HTMLTitleElementImpl;
    friend class MacFrame;
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

    void viewportMousePressEvent(MouseEvent*);
    void viewportMouseDoubleClickEvent(MouseEvent*);
    void viewportMouseMoveEvent(MouseEvent*);
    void viewportMouseReleaseEvent(MouseEvent*);
    void viewportWheelEvent(WheelEvent*);
    void keyPressEvent(KeyEvent*);

    void doAutoScroll();

    bool updateDragAndDrop(const IntPoint &, ClipboardImpl *clipboard);
    void cancelDragAndDrop(const IntPoint &, ClipboardImpl *clipboard);
    bool performDragAndDrop(const IntPoint &, ClipboardImpl *clipboard);

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

    Widget *topLevelWidget() const;
    IntPoint mapToGlobal(const IntPoint &) const;
    // maps "viewport" (actually Cocoa window coords) to screen coords
    IntPoint viewportToGlobal(const IntPoint &) const;
    void adjustViewSize();
    void initScrollBars();
    
    void setHasBorder(bool);
    bool hasBorder() const;
    
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
     * Paints the HTML document to a QPainter.
     * The document will be scaled to match the width of
     * rc and clipped to fit in the height.
     * yOff determines the vertical offset in the document to start with.
     * more, if nonzero will be set to true if the documents extends
     * beyond the rc or false if everything below yOff was painted.
     **/
    void paint(QPainter *p, const IntRect &rc, int yOff = 0, bool *more = 0);

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
    void setMediaType( const QString &medium );
    QString mediaType() const;

    bool scrollTo(const IntRect &);

    void focusNextPrevNode(bool next);

    void useSlowRepaints();

    void setIgnoreWheelEvents(bool e);

    void init();

    NodeImpl *nodeUnderMouse() const;

    void restoreScrollBar();

    QStringList formCompletionItems(const QString &name) const;
    void addFormCompletionItem(const QString &name, const QString &value);

    bool dispatchMouseEvent(const AtomicString& eventType, NodeImpl* target,
        bool cancelable, int detail, MouseEvent*, bool setUnder);
    bool dispatchDragEvent(const AtomicString& eventType, NodeImpl* target,
        const IntPoint& loc, ClipboardImpl*);

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

    QString m_medium; // media type
};

}

#endif

