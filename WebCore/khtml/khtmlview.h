/* This file is part of the KDE project

   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)

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

#ifndef KHTML_H
#define KHTML_H

// qt includes and classes
#include <qscrollview.h>

template<class C> class QList;

class QPainter;
class QRect;

namespace DOM {
    class HTMLDocumentImpl;
    class ElementImpl;
    class HTMLElementImpl;
    class HTMLTitleElementImpl;
    class HTMLGenericFormElementImpl;
    class HTMLFormElementImpl;
    class Range;
    class NodeImpl;
    class CSSProperty;
};

namespace khtml {
    class RenderObject;
    class RenderRoot;
    class RenderStyle;
    class RenderLineEdit;
    void applyRule(RenderStyle *style, DOM::CSSProperty *prop, DOM::ElementImpl *e);
};

#include "khtml_part.h"

class KHTMLPart;
class KHTMLViewPrivate;

/**
 * Render and display HTML in a @ref QScrollView.
 *
 * Suitable for use as an application's main view.
 **/
class KHTMLView : public QScrollView
{
    Q_OBJECT

    friend class DOM::HTMLDocumentImpl;
    friend class DOM::HTMLElementImpl;
    friend class DOM::HTMLTitleElementImpl;
    friend class KHTMLPart;
    friend class khtml::RenderRoot;
    friend class DOM::HTMLGenericFormElementImpl;
    friend class DOM::HTMLFormElementImpl;
    friend class khtml::RenderLineEdit;
    friend void khtml::applyRule(khtml::RenderStyle *style, DOM::CSSProperty *prop, DOM::ElementImpl *e);

public:
    /**
     * Construct a @ref KHTMLView.
     */
    KHTMLView( KHTMLPart *part, QWidget *parent, const char *name=0 );
    virtual ~KHTMLView();

    /**
     * Retrieve a pointer to the @ref KHTMLPart that is
     * rendering the page.
     **/
    KHTMLPart *part() const { return m_part; }

    int frameWidth() const { return _width; }

    /**
     * BCI: This function is provided for compatibility reasons only and 
     * will be removed in KDE3.0.
     * use sendEvent(view, Key_Tab) instead.
     * Move the view towards the next link and
     * draw a cursor around it
     **/
    bool gotoNextLink();

    /**
     * BCI: This function is provided for compatibility reasons only and 
     * will be removed in KDE3.0
     * use sendEvent(view, Key_BackTab) instead.
     * Move the view towards the next link and
     * draw a cursor around it
     **/
    bool gotoPrevLink();

    /**
     * BCI: This function is provided for compatibility reasons only and 
     * will be removed in KDE3.0
     * use sendEvent(view, Key_Return) instead.
     * visualize that the item under the cursor
     * has been pressed (true) or released(false)
     */
    void toggleActLink(bool);

    /**
     * Set a margin in x direction.
     */
    void setMarginWidth(int x) { _marginWidth = x; }

    /**
     * Retrieve the margin width.
     *
     * A return value of -1 means the default value will be used.
     */
    int marginWidth() const { return _marginWidth; }

    /*
     * Set a margin in y direction.
     */
    void setMarginHeight(int y) { _marginHeight = y; }

    /*
     * Retrieve the margin height.
     *
     * A return value of -1 means the default value will be used.
     */
    int marginHeight() { return _marginHeight; }

    /*
     * Set vertical scrollbar mode. Reimplemented for internal reasons.
     */
    virtual void setVScrollBarMode ( ScrollBarMode mode );

    /*
     * Set horizontal scrollbar mode. Reimplemented for internal reasons.
     */
    virtual void setHScrollBarMode ( ScrollBarMode mode );

    /**
     * Print the HTML document.
     **/
    void print();

    /**
     * Paint the HTML document to a QPainter.
     * The document will be scaled to match the width of
     * rc and clipped to fit in the height.
     * yOff determines the vertical offset in the document to start with.
     * more, if nonzero will be set to true if the documents extends
     * beyond the rc or false if everything below yOff was painted.
     **/
    void paint(QPainter *p, const QRect &rc, int yOff = 0, bool *more = 0);

    void layout(bool force = false);

    static const QList<KHTMLView> *viewList() { return lstViews; }

signals:
    void cleared();

protected:
    void clear();

    virtual void resizeEvent ( QResizeEvent * event );
    virtual void showEvent ( QShowEvent * );
    virtual void hideEvent ( QHideEvent *);
    virtual bool focusNextPrevChild( bool next );
    virtual void drawContents ( QPainter * p, int clipx, int clipy, int clipw, int cliph );

    virtual void viewportMousePressEvent( QMouseEvent * );
    virtual void focusOutEvent( QFocusEvent * );

    /**
     * This function emits the @ref doubleClick() signal when the user
     * double clicks a <a href=...> tag.
     */
    virtual void viewportMouseDoubleClickEvent( QMouseEvent * );

    /**
     * This function is called when the user moves the mouse.
     */
    virtual void viewportMouseMoveEvent(QMouseEvent *);

    /**
     * this function is called when the user releases a mouse button.
     */
    virtual void viewportMouseReleaseEvent(QMouseEvent *);

    virtual void viewportWheelEvent(QWheelEvent*);

    void keyPressEvent( QKeyEvent *_ke );
    void keyReleaseEvent( QKeyEvent *_ke );

    /**
     * Scroll the view
     */
    void doAutoScroll();

protected slots:
    void slotPaletteChanged();

private:
    /**
     * move the view towards the given rectangle be up to one page.
     * return true if reached.
     */
    bool scrollTo(const QRect &);

    /**
     * move the view towards the next node
     * or the last node from this one.
     */
    bool gotoLink(bool);

    void useSlowRepaints();

    void init();

    DOM::NodeImpl *nodeUnderMouse() const;

    void restoreScrollBar();

    QStringList formCompletionItems(const QString &name) const;
    void addFormCompletionItem(const QString &name, const QString &value);

    void dispatchMouseEvent(int eventId, DOM::NodeImpl *targetNode, bool cancelable,
			    int detail,QMouseEvent *_mouse, bool setUnder,
			    int mouseEventType);

    // ------------------------------------- member variables ------------------------------------
 private:
    /**
     * List of all open browsers.
     */
    static QList<KHTMLView> *lstViews;

    int _width;
    int _height;

    int _marginWidth;
    int _marginHeight;

    KHTMLPart *m_part;
    KHTMLViewPrivate *d;
};

#endif

