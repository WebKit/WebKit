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
#ifndef HTML_ELEMENTIMPL_H
#define HTML_ELEMENTIMPL_H

#include "xml/dom_elementimpl.h"

namespace DOM {

class DOMString;
class CSSStyleDeclarationImpl;
class HTMLFormElementImpl;
 class DocumentFragmentImpl;

class HTMLElementImpl : public ElementImpl
{
public:
    HTMLElementImpl(DocumentPtr *doc);

    virtual ~HTMLElementImpl();

    virtual bool isHTMLElement() const { return true; }

    virtual bool isInline() const;
     
    virtual Id id() const = 0;

    virtual void parseAttribute(AttributeImpl *token);

    void addCSSLength(int id, const DOMString &value);
    void addCSSProperty(int id, const DOMString &value);
    void addCSSProperty(int id, int value);
    void addHTMLColor( int id, const DOMString &c );
    void removeCSSProperty(int id);

    DOMString innerHTML() const;
    DOMString innerText() const;
    DocumentFragmentImpl *createContextualFragment( const DOMString &html );
    bool setInnerHTML( const DOMString &html );
    bool setInnerText( const DOMString &text );

    virtual DOMString namespaceURI() const;

#if APPLE_CHANGES
    virtual bool isGenericFormElement() const { return false; }
#endif

protected:
    // for IMG, OBJECT and APPLET
    void addHTMLAlignment( DOMString alignment );
};

class HTMLGenericElementImpl : public HTMLElementImpl
{
public:
    HTMLGenericElementImpl(DocumentPtr *doc, ushort i);

    virtual ~HTMLGenericElementImpl();

    virtual Id id() const { return _id; };

protected:
    ushort _id;
};

}; //namespace

#endif
