/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#ifndef HTML_HTMLOptionElementImpl_H
#define HTML_HTMLOptionElementImpl_H

#include "HTMLGenericFormElementImpl.h"
#include "htmlnames.h"

namespace khtml {
    class RenderSelect;
}

namespace DOM {

class HTMLSelectElementImpl;
class HTMLFormElementImpl;
class MappedAttributeImpl;

class HTMLOptionElementImpl : public HTMLGenericFormElementImpl
{
    friend class khtml::RenderSelect;
    friend class DOM::HTMLSelectElementImpl;

public:
    HTMLOptionElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f = 0);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 2; }
    virtual bool checkDTD(const NodeImpl* newChild) { return newChild->isTextNode() || newChild->hasTagName(HTMLNames::scriptTag); }

    virtual bool isFocusable() const;

    DOMString type() const;

    DOMString text() const;
    void setText(const DOMString &, int &exception);

    int index() const;
    void setIndex(int, int &exception);
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    DOMString value() const;
    void setValue(const DOMString &);

    bool selected() const { return m_selected; }
    void setSelected(bool _selected);

    HTMLSelectElementImpl *getSelect() const;

    virtual void childrenChanged();

    bool defaultSelected() const;
    void setDefaultSelected( bool );

    DOMString label() const;
    void setLabel( const DOMString & );

protected:
    DOMString m_value;
    bool m_selected;
};

} //namespace

#endif
