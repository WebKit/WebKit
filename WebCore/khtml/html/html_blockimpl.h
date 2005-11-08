/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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

// -------------------------------------------------------------------------
#ifndef HTML_BLOCKIMPL_H
#define HTML_BLOCKIMPL_H

#include "html_elementimpl.h"

namespace DOM {

class DOMString;

// -------------------------------------------------------------------------

class HTMLBlockquoteElementImpl : public HTMLElementImpl
{
public:
    HTMLBlockquoteElementImpl(DocumentImpl *doc);
    ~HTMLBlockquoteElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }
    
    DOMString cite() const;
    void setCite(const DOMString &);
};

// -------------------------------------------------------------------------

class DOMString;

class HTMLDivElementImpl : public HTMLElementImpl
{
public:
    HTMLDivElementImpl(DocumentImpl *doc);
    ~HTMLDivElementImpl();
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    DOMString align() const;
    void setAlign(const DOMString &);
};

// -------------------------------------------------------------------------

class HTMLHRElementImpl : public HTMLElementImpl
{
public:
    HTMLHRElementImpl(DocumentImpl *doc);
    ~HTMLHRElementImpl();
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl* attr);

    DOMString align() const;
    void setAlign(const DOMString &);

    bool noShade() const;
    void setNoShade(bool);

    DOMString size() const;
    void setSize(const DOMString &);

    DOMString width() const;
    void setWidth(const DOMString &);
};

// -------------------------------------------------------------------------

class HTMLHeadingElementImpl : public HTMLElementImpl
{
public:
    HTMLHeadingElementImpl(const QualifiedName& tagName, DocumentImpl *doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }
    virtual bool checkDTD(const NodeImpl* newChild);

    DOMString align() const;
    void setAlign(const DOMString &);
};

// -------------------------------------------------------------------------

class HTMLParagraphElementImpl : public HTMLElementImpl
{
public:
    HTMLParagraphElementImpl(DocumentImpl *doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 3; }
    virtual bool checkDTD(const NodeImpl* newChild);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    DOMString align() const;
    void setAlign(const DOMString &);
};

// -------------------------------------------------------------------------

class HTMLPreElementImpl : public HTMLElementImpl
{
public:
    HTMLPreElementImpl(const QualifiedName& tagName, DocumentImpl *doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    void parseMappedAttribute(MappedAttributeImpl *attr);

    int width() const;
    void setWidth(int w);
    
    bool wrap() const;
    void setWrap(bool b);
};

// -------------------------------------------------------------------------

class HTMLMarqueeElementImpl : public HTMLElementImpl
{
public:
    HTMLMarqueeElementImpl(DocumentImpl *doc);
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 3; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    int minimumDelay() const { return m_minimumDelay; }
    
private:
    int m_minimumDelay;
};

}; //namespace
#endif
