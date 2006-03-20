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

#include "HTMLElement.h"

namespace WebCore {

class String;

class HTMLAnchorElement : public HTMLElement
{
public:
    HTMLAnchorElement(Document *doc);
    HTMLAnchorElement(const QualifiedName& tagName, Document* doc);
    ~HTMLAnchorElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    virtual bool isMouseFocusable() const;
    virtual bool isKeyboardFocusable() const;
    virtual bool isFocusable() const;
    virtual void parseMappedAttribute(MappedAttribute *attr);
    virtual void defaultEventHandler(Event *evt);
    virtual void accessKeyAction(bool fullAction);
    virtual bool isURLAttribute(Attribute *attr) const;

    String accessKey() const;
    void setAccessKey(const String &);

    String charset() const;
    void setCharset(const String &);

    String coords() const;
    void setCoords(const String &);

    String href() const;
    void setHref(const String &);

    String hreflang() const;
    void setHreflang(const String &);

    String name() const;
    void setName(const String &);

    String rel() const;
    void setRel(const String &);

    String rev() const;
    void setRev(const String &);

    String shape() const;
    void setShape(const String &);

    int tabIndex() const;
    void setTabIndex(int);

    String target() const;
    void setTarget(const String &);

    String type() const;
    void setType(const String &);

    void blur();
    void focus();

protected:
    bool m_hasTarget : 1;
};

// -------------------------------------------------------------------------

//typedef enum { BRNone=0, BRLeft, BRRight, BRAll} BRClear;

class HTMLBRElement : public HTMLElement
{
public:
    HTMLBRElement(Document *doc);
    ~HTMLBRElement();
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *attr);
    
    virtual WebCore::RenderObject *createRenderer(RenderArena *, WebCore::RenderStyle *);

    String clear() const;
    void setClear(const String &);
};

// -------------------------------------------------------------------------

class HTMLFontElement : public HTMLElement
{
public:
    HTMLFontElement(Document *doc);
    ~HTMLFontElement();
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *attr);

    String color() const;
    void setColor(const String &);

    String face() const;
    void setFace(const String &);

    String size() const;
    void setSize(const String &);
};

// -------------------------------------------------------------------------

class HTMLModElement : public HTMLElement
{
public:
    HTMLModElement(const QualifiedName& tagName, Document *doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    String cite() const;
    void setCite(const String &);

    String dateTime() const;
    void setDateTime(const String &);
};

// -------------------------------------------------------------------------

class HTMLQuoteElement : public HTMLElement
{
public:
    HTMLQuoteElement(Document *doc);
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    String cite() const;
    void setCite(const String &);
};

} //namespace

#endif
