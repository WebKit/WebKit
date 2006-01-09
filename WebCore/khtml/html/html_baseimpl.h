/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004 Apple Computer, Inc.
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
 */

#ifndef HTML_BASEIMPL_H
#define HTML_BASEIMPL_H

#include "html/html_elementimpl.h"
#include "misc/khtmllayout.h"

#include <qscrollview.h>

class KHTMLPart;
class KHTMLView;

namespace khtml {
    class RenderFrameSet;
    class RenderFrame;
    class RenderPartObject;
}

namespace DOM {

class DOMString;
class CSSStyleSheetImpl;
class HTMLFrameElement;

// -------------------------------------------------------------------------

class HTMLBodyElementImpl : public HTMLElementImpl
{
public:
    HTMLBodyElementImpl(DocumentImpl *doc);
    ~HTMLBodyElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 10; }
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *);

    virtual void insertedIntoDocument();

    void createLinkDecl();
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    DOMString aLink() const;
    void setALink(const DOMString &);
    DOMString background() const;
    void setBackground(const DOMString &);
    DOMString bgColor() const;
    void setBgColor(const DOMString &);
    DOMString link() const;
    void setLink(const DOMString &);
    DOMString text() const;
    void setText(const DOMString &);
    DOMString vLink() const;
    void setVLink(const DOMString &);

protected:
    CSSMutableStyleDeclarationImpl* m_linkDecl;
};

// -------------------------------------------------------------------------

class HTMLFrameElementImpl : public HTMLElementImpl
{
    friend class khtml::RenderFrame;
    friend class khtml::RenderPartObject;

public:
    HTMLFrameElementImpl(DocumentImpl *doc);
    HTMLFrameElementImpl(const QualifiedName& tagName, DocumentImpl* doc);
    ~HTMLFrameElementImpl();

    void init();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }
  
    virtual void parseMappedAttribute(MappedAttributeImpl *);
    virtual void attach();
    void close();
    virtual void willRemove();
    virtual void detach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

    bool noResize() { return m_noResize; }

    void setLocation(const DOMString&);

    virtual bool isFocusable() const;
    virtual void setFocus(bool);

    KHTMLPart* contentPart() const;
    DocumentImpl* contentDocument() const;
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    QScrollView::ScrollBarMode scrollingMode() const { return m_scrolling; }
    int getMarginWidth() const { return m_marginWidth; }
    int getMarginHeight() const { return m_marginHeight; }

    DOMString frameBorder() const;
    void setFrameBorder(const DOMString &);

    DOMString longDesc() const;
    void setLongDesc(const DOMString &);

    DOMString marginHeight() const;
    void setMarginHeight(const DOMString &);

    DOMString marginWidth() const;
    void setMarginWidth(const DOMString &);

    DOMString name() const;
    void setName(const DOMString &);

    void setNoResize(bool);

    DOMString scrolling() const;
    void setScrolling(const DOMString &);

    virtual DOMString src() const;
    void setSrc(const DOMString &);

    int frameWidth() const;
    int frameHeight() const;

protected:
    bool isURLAllowed(const AtomicString &) const;
    virtual void openURL();

    AtomicString m_URL;
    AtomicString m_name;

    int m_marginWidth;
    int m_marginHeight;
    QScrollView::ScrollBarMode m_scrolling;

    bool m_frameBorder : 1;
    bool m_frameBorderSet : 1;
    bool m_noResize : 1;

private:
    void updateForNewURL();
};

// -------------------------------------------------------------------------

class HTMLFrameSetElementImpl : public HTMLElementImpl
{
    friend class khtml::RenderFrameSet;
public:
    HTMLFrameSetElementImpl(DocumentImpl *doc);
    ~HTMLFrameSetElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 10; }
    virtual bool checkDTD(const NodeImpl* newChild);

    virtual void parseMappedAttribute(MappedAttributeImpl *);
    virtual void attach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

    virtual void defaultEventHandler(EventImpl *evt);

    bool frameBorder() { return frameborder; }
    bool noResize() { return noresize; }

    int totalRows() const { return m_totalRows; }
    int totalCols() const { return m_totalCols; }
    int border() const { return m_border; }

    virtual void recalcStyle( StyleChange ch );
    
    DOMString cols() const;
    void setCols(const DOMString &);

    DOMString rows() const;
    void setRows(const DOMString &);

protected:
    khtml::Length* m_rows;
    khtml::Length* m_cols;

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
    HTMLHeadElementImpl(DocumentImpl *doc);
    ~HTMLHeadElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 10; }
    virtual bool checkDTD(const NodeImpl* newChild);

    DOMString profile() const;
    void setProfile(const DOMString &);
};

// -------------------------------------------------------------------------

class HTMLHtmlElementImpl : public HTMLElementImpl
{
public:
    HTMLHtmlElementImpl(DocumentImpl *doc);
    ~HTMLHtmlElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 11; }
    virtual bool checkDTD(const NodeImpl* newChild);

    DOMString version() const;
    void setVersion(const DOMString &);
};


// -------------------------------------------------------------------------

class HTMLIFrameElementImpl : public HTMLFrameElementImpl
{
public:
    HTMLIFrameElementImpl(DocumentImpl *doc);
    ~HTMLIFrameElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    virtual void attach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void recalcStyle( StyleChange ch );
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    DOMString align() const;
    void setAlign(const DOMString &);

    DOMString height() const;
    void setHeight(const DOMString &);

    DOMString width() const;
    void setWidth(const DOMString &);

    virtual DOMString src() const;

protected:
    virtual void openURL();

    bool needWidgetUpdate;

 private:
    DOMString oldNameAttr;
};

} //namespace

#endif
