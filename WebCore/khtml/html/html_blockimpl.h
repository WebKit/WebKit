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

#include "HTMLElement.h"

namespace WebCore {

class String;

// -------------------------------------------------------------------------

class HTMLBlockquoteElement : public HTMLElement
{
public:
    HTMLBlockquoteElement(Document *doc);
    ~HTMLBlockquoteElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }
    
    String cite() const;
    void setCite(const String &);
};

// -------------------------------------------------------------------------

class String;

class HTMLDivElement : public HTMLElement
{
public:
    HTMLDivElement(Document *doc);
    ~HTMLDivElement();
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *attr);

    String align() const;
    void setAlign(const String &);
};

// -------------------------------------------------------------------------

class HTMLHRElement : public HTMLElement
{
public:
    HTMLHRElement(Document *doc);
    ~HTMLHRElement();
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute* attr);

    String align() const;
    void setAlign(const String &);

    bool noShade() const;
    void setNoShade(bool);

    String size() const;
    void setSize(const String &);

    String width() const;
    void setWidth(const String &);
};

// -------------------------------------------------------------------------

class HTMLHeadingElement : public HTMLElement
{
public:
    HTMLHeadingElement(const QualifiedName& tagName, Document *doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }
    virtual bool checkDTD(const Node* newChild);

    String align() const;
    void setAlign(const String &);
};

// -------------------------------------------------------------------------

class HTMLParagraphElement : public HTMLElement
{
public:
    HTMLParagraphElement(Document *doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 3; }
    virtual bool checkDTD(const Node* newChild);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *attr);

    String align() const;
    void setAlign(const String &);
};

// -------------------------------------------------------------------------

class HTMLPreElement : public HTMLElement
{
public:
    HTMLPreElement(const QualifiedName& tagName, Document *doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    void parseMappedAttribute(MappedAttribute *attr);

    int width() const;
    void setWidth(int w);
    
    bool wrap() const;
    void setWrap(bool b);
};

// -------------------------------------------------------------------------

class HTMLMarqueeElement : public HTMLElement
{
public:
    HTMLMarqueeElement(Document *doc);
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 3; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *attr);

    int minimumDelay() const { return m_minimumDelay; }
    
private:
    int m_minimumDelay;
};

}; //namespace
#endif
