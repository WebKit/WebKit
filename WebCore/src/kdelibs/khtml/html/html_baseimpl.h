/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id$
 */

#ifndef HTML_BASEIMPL_H
#define HTML_BASEIMPL_H

#include "dtd.h"
#include "html_elementimpl.h"
#include "khtmllayout.h"
#include <qscrollview.h>

class KHTMLView;

namespace khtml {
    class RenderFrameSet;
    class RenderFrame;
    class RenderPartObject;
}

namespace DOM {

class DOMString;
    class CSSStyleSheetImpl;

class HTMLBodyElementImpl : public HTMLElementImpl
{
public:
    HTMLBodyElementImpl(DocumentPtr *doc);
    ~HTMLBodyElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *);
    void attach();

    virtual bool prepareMouseEvent( int _x, int _y,
                                    int _tx, int _ty,
                                    MouseEvent *ev );

    CSSStyleSheetImpl *sheet() const { return m_styleSheet; }
    
protected:
    CSSStyleSheetImpl *m_styleSheet;
    DOMString bgImage;
    DOMString bgColor;
};

// -------------------------------------------------------------------------

class HTMLFrameElementImpl : public HTMLElementImpl
{
    friend class khtml::RenderFrame;
    friend class khtml::RenderPartObject;
public:
    HTMLFrameElementImpl(DocumentPtr *doc);

    ~HTMLFrameElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *);
    virtual void attach();
    virtual void detach();

    bool noResize() { return noresize; }

    virtual bool isSelectable() const;
    virtual void setFocus(bool);

protected:
    DOMString url;
    DOMString name;
    KHTMLView *view;
    KHTMLView *parentWidget;

    int marginWidth;
    int marginHeight;
    QScrollView::ScrollBarMode scrolling;
    
    bool frameBorder : 1;
    bool frameBorderSet : 1;
    bool noresize : 1;
};

// -------------------------------------------------------------------------

class HTMLFrameSetElementImpl : public HTMLElementImpl
{
    friend class khtml::RenderFrameSet;
public:
    HTMLFrameSetElementImpl(DocumentPtr *doc);

    ~HTMLFrameSetElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *);
    virtual void attach();

    virtual bool prepareMouseEvent( int _x, int _y,
                                    int _tx, int _ty,
                                    MouseEvent *ev );
    virtual void defaultEventHandler(EventImpl *evt);

    virtual khtml::FindSelectionResult findSelectionNode( int _x, int _y, int _tx, int _ty,
                                                   DOM::Node & node, int & offset );

    bool frameBorder() { return frameborder; }
    bool noResize() { return noresize; }

    int totalRows() const { return m_totalRows; }
    int totalCols() const { return m_totalCols; }
    int border() const { return m_border; }
    virtual void detach();

protected:

    // returns true if layout needs to be redone
    bool verifyLayout();

    QList<khtml::Length> *m_rows;
    QList<khtml::Length> *m_cols;
    KHTMLView *view;

    int m_totalRows;
    int m_totalCols;
    int m_border;
    
    bool frameborder : 1;
    bool frameBorderSet : 1;
    bool noresize : 1;
    bool m_resizing : 1;  // is the user resizing currently
};

// -------------------------------------------------------------------------

class HTMLHeadElementImpl : public HTMLElementImpl
{
public:
    HTMLHeadElementImpl(DocumentPtr *doc);

    ~HTMLHeadElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
};

// -------------------------------------------------------------------------

class HTMLHtmlElementImpl : public HTMLElementImpl
{
public:
    HTMLHtmlElementImpl(DocumentPtr *doc);

    ~HTMLHtmlElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void attach();

};


// -------------------------------------------------------------------------

class HTMLIFrameElementImpl : public HTMLFrameElementImpl
{
public:
    HTMLIFrameElementImpl(DocumentPtr *doc);

    ~HTMLIFrameElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *attr);
    virtual void attach();
    virtual void applyChanges(bool = true, bool = true);

    DOM::DocumentImpl* frameDocument() const;
protected:
    bool needWidgetUpdate;
};


}; //namespace

#endif

