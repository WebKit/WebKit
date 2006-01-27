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

// qt includes and classes
#include <qscrollview.h>

namespace WebCore {
    class IntRect;
    class QPainter;
}

namespace DOM {
    class HTMLDocumentImpl;
    class DocumentImpl;
    class ClipboardImpl;
    class ElementImpl;
    class HTMLElementImpl;
    class HTMLTitleElementImpl;
    class HTMLGenericFormElementImpl;
    class HTMLFormElementImpl;
    class HTMLAnchorElementImpl;
    class NodeImpl;
    class CSSProperty;
};

namespace khtml {
    class RenderObject;
    class RenderBox;
    class RenderCanvas;
    class RenderStyle;
    class RenderLineEdit;
    class RenderPart;
    class RenderPartObject;
    class RenderWidget;
    class CSSStyleSelector;
    class InlineBox;
    void applyRule(DOM::CSSProperty *prop);
};

class Frame;
class FrameViewPrivate;

/**
 * Renders and displays HTML in a @ref QScrollView.
 *
 * Suitable for use as an application's main view.
 **/
class FrameView : public QScrollView
{
    Q_OBJECT

    friend class DOM::HTMLDocumentImpl;
    friend class DOM::HTMLTitleElementImpl;
    friend class DOM::HTMLGenericFormElementImpl;
    friend class DOM::HTMLFormElementImpl;
    friend class DOM::HTMLAnchorElementImpl;
    friend class DOM::DocumentImpl;
    friend class Frame;
    friend class khtml::RenderCanvas;
    friend class khtml::RenderObject;
    friend class khtml::RenderBox;
    friend class khtml::RenderLineEdit;
    friend class khtml::RenderPart;
    friend class khtml::RenderPartObject;
    friend class khtml::RenderWidget;
    friend class khtml::CSSStyleSelector;
    friend void khtml::applyRule(DOM::CSSProperty *prop);
    friend class MacFrame;

public:
    /**
     * Constructs a FrameView.
     */
    FrameView( Frame *frame, QWidget *parent, const char *name=0 );
    virtual ~FrameView();

    /**
     * Returns a pointer to the Frame that is
     * rendering the page.
     **/
    Frame *frame() const { return m_frame; }

    int frameWidth() const { return _width; }

    /**
     * Sets a margin in x direction.
     */
    void setMarginWidth(int x);

    /**
     * Returns the margin width.
     *
     * A return value of -1 means the default value will be used.
     */
    int marginWidth() const { return _marginWidth; }

    /*
     * Sets a margin in y direction.
     */
    void setMarginHeight(int y);

    /**
     * Returns the margin height.
     *
     * A return value of -1 means the default value will be used.
     */
    int marginHeight() { return _marginHeight; }

    /**
     * Sets verticals scrollbar mode. Reimplemented for internal reasons.
     */
    virtual void setVScrollBarMode ( ScrollBarMode mode );

    /**
     * Sets horizontal scrollbar mode. Reimplemented for internal reasons.
     */
    virtual void setHScrollBarMode ( ScrollBarMode mode );

    // Sets both horizontal and vertical modes.
    virtual void setScrollBarsMode(ScrollBarMode mode);
    
    /**
     * Prints the HTML document.
     */
    void print();

    /**
     * ensure the display is up to date
     */
    void layout();

    bool inLayout() const;
    int layoutCount() const;

    bool needsFullRepaint() const;
    
    void addRepaintInfo(khtml::RenderObject* o, const IntRect& r);

    void resetScrollBars();

     void clear();

signals:
    void cleared();

public:
    void clearPart();
    virtual void resizeEvent ( QResizeEvent * event );
    virtual void viewportMousePressEvent( QMouseEvent * );
    virtual void focusInEvent( QFocusEvent * );
    virtual void focusOutEvent( QFocusEvent * );
    virtual void viewportMouseDoubleClickEvent( QMouseEvent * );
    virtual void viewportMouseMoveEvent(QMouseEvent *);
    virtual void viewportMouseReleaseEvent(QMouseEvent *);
#ifndef QT_NO_WHEELEVENT
    virtual void viewportWheelEvent(QWheelEvent*);
#endif

    void keyPressEvent( QKeyEvent *_ke );
    void doAutoScroll();

    bool updateDragAndDrop(const IntPoint &, DOM::ClipboardImpl *clipboard);
    void cancelDragAndDrop(const IntPoint &, DOM::ClipboardImpl *clipboard);
    bool performDragAndDrop(const IntPoint &, DOM::ClipboardImpl *clipboard);

    void timerEvent ( QTimerEvent * );

    void repaintRectangle(const IntRect& r, bool immediate);

    bool isTransparent() const;
    void setTransparent(bool isTransparent);
    
    void scheduleRelayout();
    void unscheduleRelayout();
    bool haveDelayedLayoutScheduled();
    bool layoutPending();

    QWidget *topLevelWidget() const;
    IntPoint mapToGlobal(const IntPoint &) const;
    // maps "viewport" (actually Cocoa window coords) to screen coords
    IntPoint viewportToGlobal(const IntPoint &) const;
    void adjustViewSize();
    void initScrollBars();
    
#if __APPLE__
    void updateDashboardRegions();
#endif

    void ref() { ++_refCount; }
    void deref() { if (!--_refCount) delete this; }
    
protected slots:
    void slotPaletteChanged();
    void slotScrollBarMoved();

private:

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
    void paint(WebCore::QPainter *p, const IntRect &rc, int yOff = 0, bool *more = 0);

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

    DOM::NodeImpl *nodeUnderMouse() const;

    void restoreScrollBar();

    QStringList formCompletionItems(const QString &name) const;
    void addFormCompletionItem(const QString &name, const QString &value);

    bool dispatchMouseEvent(const DOM::AtomicString &eventType, DOM::NodeImpl *targetNode, bool cancelable,
                            int detail,QMouseEvent *_mouse, bool setUnder,
                            int mouseEventType);
    bool dispatchDragEvent(const DOM::AtomicString &eventType, DOM::NodeImpl *dragTarget, const IntPoint &loc, DOM::ClipboardImpl *clipboard);

    void applyOverflowToViewport(khtml::RenderObject* o, ScrollBarMode& hMode, ScrollBarMode& vMode);

    virtual bool isFrameView() const;

    // ------------------------------------- member variables ------------------------------------
 private:
    unsigned _refCount;

    int _width;
    int _height;

    int _marginWidth;
    int _marginHeight;

    Frame *m_frame;
    FrameViewPrivate *d;

    QString m_medium;   // media type
};

#endif

