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

#include "html_elementimpl.h"

using namespace HTMLNames;

namespace DOM
{

class HTMLUListElementImpl : public HTMLElementImpl
{
public:
    HTMLUListElementImpl(DocumentPtr *doc) : HTMLElementImpl(HTMLNames::ulTag, doc) {}
    virtual ~HTMLUListElementImpl() {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *);

    virtual long start() const { return 1; }

    bool compact() const;
    void setCompact(bool);

    DOMString type() const;
    void setType(const DOMString &);
};

// -------------------------------------------------------------------------

class HTMLDirectoryElementImpl : public HTMLElementImpl
{
public:
    HTMLDirectoryElementImpl(DocumentPtr *doc) : HTMLElementImpl(HTMLNames::dirTag, doc) {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    bool compact() const;
    void setCompact(bool);
};

// -------------------------------------------------------------------------

class HTMLMenuElementImpl : public HTMLElementImpl
{
public:
    HTMLMenuElementImpl(DocumentPtr *doc) : HTMLElementImpl(HTMLNames::menuTag, doc) {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    bool compact() const;
    void setCompact(bool);
};

// -------------------------------------------------------------------------

class HTMLOListElementImpl : public HTMLElementImpl
{
public:
    HTMLOListElementImpl(DocumentPtr *doc)
        : HTMLElementImpl(HTMLNames::olTag, doc) { _start = 1; }
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *);

    bool compact() const;
    void setCompact(bool);

    long start() const { return _start; }
    void setStart(long);

    DOMString type() const;
    void setType(const DOMString &);

private:
    int _start;
};

// -------------------------------------------------------------------------

class HTMLLIElementImpl : public HTMLElementImpl
{
public:
    HTMLLIElementImpl(DocumentPtr *doc)
        : HTMLElementImpl(HTMLNames::liTag, doc) { isValued = false; }
    virtual ~HTMLLIElementImpl() {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 3; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    virtual void attach();

    DOMString type() const;
    void setType(const DOMString &);

    long value() const;
    void setValue(long);

private:
    bool isValued;
    long requestedValue;
};

// -------------------------------------------------------------------------

class HTMLDListElementImpl : public HTMLElementImpl
{
public:
    HTMLDListElementImpl(DocumentPtr *doc) : HTMLElementImpl(HTMLNames::dlTag, doc) {}
    virtual ~HTMLDListElementImpl() {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }

    bool compact() const;
    void setCompact(bool);
};

} //namespace

#endif
