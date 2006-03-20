/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#include "HTMLElement.h"
#include "ScrollBarMode.h"

namespace WebCore {

class CSSStyleSheet;
class String;
class Frame;
class HTMLFrameElement;
class RenderFrameSet;
class RenderFrame;
class RenderPartObject;

struct Length;

// -------------------------------------------------------------------------

class HTMLBodyElement : public HTMLElement
{
public:
    HTMLBodyElement(Document *doc);
    ~HTMLBodyElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 10; }
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *);

    virtual void insertedIntoDocument();

    void createLinkDecl();
    
    virtual bool isURLAttribute(Attribute *attr) const;

    String aLink() const;
    void setALink(const String &);
    String background() const;
    void setBackground(const String &);
    String bgColor() const;
    void setBgColor(const String &);
    String link() const;
    void setLink(const String &);
    String text() const;
    void setText(const String &);
    String vLink() const;
    void setVLink(const String &);

protected:
    RefPtr<CSSMutableStyleDeclaration> m_linkDecl;
};

// -------------------------------------------------------------------------

class HTMLFrameElement : public HTMLElement
{
    friend class RenderFrame;
    friend class RenderPartObject;

public:
    HTMLFrameElement(Document *doc);
    HTMLFrameElement(const QualifiedName& tagName, Document* doc);
    ~HTMLFrameElement();

    void init();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }
  
    virtual void parseMappedAttribute(MappedAttribute *);
    virtual void attach();
    void close();
    virtual void willRemove();
    virtual void detach();
    virtual bool rendererIsNeeded(RenderStyle *);
    virtual RenderObject *createRenderer(RenderArena *, RenderStyle *);

    bool noResize() { return m_noResize; }

    void setLocation(const String&);

    virtual bool isFocusable() const;
    virtual void setFocus(bool);

    Frame* contentFrame() const;
    Document* contentDocument() const;
    
    virtual bool isURLAttribute(Attribute *attr) const;

    ScrollBarMode scrollingMode() const { return m_scrolling; }
    int getMarginWidth() const { return m_marginWidth; }
    int getMarginHeight() const { return m_marginHeight; }

    String frameBorder() const;
    void setFrameBorder(const String &);

    String longDesc() const;
    void setLongDesc(const String &);

    String marginHeight() const;
    void setMarginHeight(const String &);

    String marginWidth() const;
    void setMarginWidth(const String &);

    String name() const;
    void setName(const String &);

    void setNoResize(bool);

    String scrolling() const;
    void setScrolling(const String &);

    virtual String src() const;
    void setSrc(const String &);

    int frameWidth() const;
    int frameHeight() const;

protected:
    bool isURLAllowed(const AtomicString &) const;
    virtual void openURL();

    AtomicString m_URL;
    AtomicString m_name;

    int m_marginWidth;
    int m_marginHeight;
    ScrollBarMode m_scrolling;

    bool m_frameBorder : 1;
    bool m_frameBorderSet : 1;
    bool m_noResize : 1;
};

// -------------------------------------------------------------------------

class HTMLFrameSetElement : public HTMLElement
{
    friend class RenderFrameSet;
public:
    HTMLFrameSetElement(Document *doc);
    ~HTMLFrameSetElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 10; }
    virtual bool checkDTD(const Node* newChild);

    virtual void parseMappedAttribute(MappedAttribute *);
    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle *);
    virtual RenderObject *createRenderer(RenderArena *, RenderStyle *);

    virtual void defaultEventHandler(Event *evt);

    bool frameBorder() { return frameborder; }
    bool noResize() { return noresize; }

    int totalRows() const { return m_totalRows; }
    int totalCols() const { return m_totalCols; }
    int border() const { return m_border; }

    virtual void recalcStyle( StyleChange ch );
    
    String cols() const;
    void setCols(const String &);

    String rows() const;
    void setRows(const String &);

protected:
    Length* m_rows;
    Length* m_cols;

    int m_totalRows;
    int m_totalCols;
    int m_border;

    bool frameborder : 1;
    bool frameBorderSet : 1;
    bool noresize : 1;
};

// -------------------------------------------------------------------------

class HTMLHeadElement : public HTMLElement
{
public:
    HTMLHeadElement(Document *doc);
    ~HTMLHeadElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 10; }
    virtual bool checkDTD(const Node* newChild);

    String profile() const;
    void setProfile(const String &);
};

// -------------------------------------------------------------------------

class HTMLHtmlElement : public HTMLElement
{
public:
    HTMLHtmlElement(Document *doc);
    ~HTMLHtmlElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 11; }
    virtual bool checkDTD(const Node* newChild);

    String version() const;
    void setVersion(const String &);
};


// -------------------------------------------------------------------------

class HTMLIFrameElement : public HTMLFrameElement
{
public:
    HTMLIFrameElement(Document *doc);
    ~HTMLIFrameElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *attr);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle *);
    virtual RenderObject *createRenderer(RenderArena *, RenderStyle *);
    virtual void recalcStyle( StyleChange ch );
    
    virtual bool isURLAttribute(Attribute *attr) const;

    String align() const;
    void setAlign(const String &);

    String height() const;
    void setHeight(const String &);

    String width() const;
    void setWidth(const String &);

    virtual String src() const;

protected:
    virtual void openURL();

    bool needWidgetUpdate;

 private:
    String oldNameAttr;
};

} //namespace

#endif
