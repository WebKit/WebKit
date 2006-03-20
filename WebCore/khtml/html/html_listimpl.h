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

#ifndef HTML_LISTIMPL_H
#define HTML_LISTIMPL_H

/*
 * we ignore the deprecated compact attribute. Netscape does so too...
 */

#include "HTMLElement.h"
#include "htmlnames.h"

namespace WebCore
{

class HTMLUListElement : public HTMLElement
{
public:
    HTMLUListElement(Document *doc) : HTMLElement(HTMLNames::ulTag, doc) {}
    virtual ~HTMLUListElement() {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *);

    virtual int start() const { return 1; }

    bool compact() const;
    void setCompact(bool);

    String type() const;
    void setType(const String &);
};

// -------------------------------------------------------------------------

class HTMLDirectoryElement : public HTMLElement
{
public:
    HTMLDirectoryElement(Document *doc) : HTMLElement(HTMLNames::dirTag, doc) {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    bool compact() const;
    void setCompact(bool);
};

// -------------------------------------------------------------------------

class HTMLMenuElement : public HTMLElement
{
public:
    HTMLMenuElement(Document *doc) : HTMLElement(HTMLNames::menuTag, doc) {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    bool compact() const;
    void setCompact(bool);
};

// -------------------------------------------------------------------------

class HTMLOListElement : public HTMLElement
{
public:
    HTMLOListElement(Document *doc)
        : HTMLElement(HTMLNames::olTag, doc) { _start = 1; }
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *);

    bool compact() const;
    void setCompact(bool);

    int start() const { return _start; }
    void setStart(int);

    String type() const;
    void setType(const String &);

private:
    int _start;
};

// -------------------------------------------------------------------------

class HTMLLIElement : public HTMLElement
{
public:
    HTMLLIElement(Document *doc)
        : HTMLElement(HTMLNames::liTag, doc) { isValued = false; }
    virtual ~HTMLLIElement() {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 3; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *attr);

    virtual void attach();

    String type() const;
    void setType(const String &);

    int value() const;
    void setValue(int);

private:
    bool isValued;
    int requestedValue;
};

// -------------------------------------------------------------------------

class HTMLDListElement : public HTMLElement
{
public:
    HTMLDListElement(Document *doc) : HTMLElement(HTMLNames::dlTag, doc) {}
    virtual ~HTMLDListElement() {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    bool compact() const;
    void setCompact(bool);
};

} //namespace

#endif
