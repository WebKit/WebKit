/*
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
 */
#ifndef HTML_INLINEIMPL_H
#define HTML_INLINEIMPL_H

#include "html_elementimpl.h"

namespace DOM {

class DOMString;

class HTMLAnchorElementImpl : public HTMLElementImpl
{
public:
    HTMLAnchorElementImpl(DocumentPtr *doc);
    HTMLAnchorElementImpl(const QualifiedName& tagName, DocumentPtr* doc);
    ~HTMLAnchorElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    virtual bool isMouseFocusable() const;
    virtual bool isKeyboardFocusable() const;
    virtual bool isFocusable() const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    virtual void defaultEventHandler(EventImpl *evt);
    virtual void accessKeyAction(bool fullAction);
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    DOMString accessKey() const;
    void setAccessKey(const DOMString &);

    DOMString charset() const;
    void setCharset(const DOMString &);

    DOMString coords() const;
    void setCoords(const DOMString &);

    DOMString href() const;
    void setHref(const DOMString &);

    DOMString hreflang() const;
    void setHreflang(const DOMString &);

    DOMString name() const;
    void setName(const DOMString &);

    DOMString rel() const;
    void setRel(const DOMString &);

    DOMString rev() const;
    void setRev(const DOMString &);

    DOMString shape() const;
    void setShape(const DOMString &);

    long tabIndex() const;
    void setTabIndex(long);

    DOMString target() const;
    void setTarget(const DOMString &);

    DOMString type() const;
    void setType(const DOMString &);

    void blur();
    void focus();

protected:
    bool m_hasTarget : 1;
};

// -------------------------------------------------------------------------

//typedef enum { BRNone=0, BRLeft, BRRight, BRAll} BRClear;

class HTMLBRElementImpl : public HTMLElementImpl
{
public:
    HTMLBRElementImpl(DocumentPtr *doc);
    ~HTMLBRElementImpl();
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool mapToEntry(Id attr, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

    DOMString clear() const;
    void setClear(const DOMString &);
};

// -------------------------------------------------------------------------

class HTMLFontElementImpl : public HTMLElementImpl
{
public:
    HTMLFontElementImpl(DocumentPtr *doc);
    ~HTMLFontElementImpl();
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    virtual bool mapToEntry(Id attr, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    DOMString color() const;
    void setColor(const DOMString &);

    DOMString face() const;
    void setFace(const DOMString &);

    DOMString size() const;
    void setSize(const DOMString &);
};

// -------------------------------------------------------------------------

class HTMLModElementImpl : public HTMLElementImpl
{
public:
    HTMLModElementImpl(const QualifiedName& tagName, DocumentPtr *doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    DOMString cite() const;
    void setCite(const DOMString &);

    DOMString dateTime() const;
    void setDateTime(const DOMString &);
};

// -------------------------------------------------------------------------

class HTMLQuoteElementImpl : public HTMLElementImpl
{
public:
    HTMLQuoteElementImpl(DocumentPtr *doc);
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    DOMString cite() const;
    void setCite(const DOMString &);
};

} //namespace

#endif
