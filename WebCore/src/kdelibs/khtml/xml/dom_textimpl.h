/**
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
 * $Id$
 */
#ifndef _DOM_CharacterDataImpl_h_
#define _DOM_CharacterDataImpl_h_

#include "dom_nodeimpl.h"
#include "dom_string.h"

namespace khtml {
    class RenderStyle;
}

namespace DOM {

    class DocumentImpl;

class CharacterDataImpl : public NodeWParentImpl
{
public:
    CharacterDataImpl(DocumentPtr *doc, const DOMString &_text);
    CharacterDataImpl(DocumentPtr *doc);
    virtual ~CharacterDataImpl();

    DOMString data() const;

    void setData( const DOMString & );

    unsigned long length() const;

    DOMString substringData ( const unsigned long offset, const unsigned long count, int &exceptioncode );

    void appendData ( const DOMString &arg );

    void insertData ( const unsigned long offset, const DOMString &arg, int &exceptioncode );

    void deleteData ( const unsigned long offset, const unsigned long count, int &exceptioncode );

    void replaceData ( const unsigned long offset, const unsigned long count, const DOMString &arg, int &exceptioncode );

    DOMStringImpl *string() { return str; }
protected:
    // note: since DOMStrings are shared, str should always be copied when making
    // a change or returning a string
    DOMStringImpl *str;
    void dispatchModifiedEvent(DOMStringImpl *prevValue);
};

// ----------------------------------------------------------------------------

class CommentImpl : public CharacterDataImpl
{
public:
    CommentImpl(DocumentPtr *doc, const DOMString &_text);
    CommentImpl(DocumentPtr *doc);
    virtual ~CommentImpl();

    virtual const DOMString nodeName() const;
    virtual DOMString nodeValue() const;
    virtual unsigned short nodeType() const;
    virtual ushort id() const;
    virtual NodeImpl *cloneNode(bool deep, int &exceptioncode);
    virtual bool childTypeAllowed( unsigned short type );
};

// ----------------------------------------------------------------------------

class TextImpl : public CharacterDataImpl
{
public:
    TextImpl(DocumentPtr *impl, const DOMString &_text);
    TextImpl(DocumentPtr *impl);
    virtual ~TextImpl();

    virtual const DOMString nodeName() const;
    virtual DOMString nodeValue() const;
    virtual unsigned short nodeType() const;
    virtual bool isTextNode() const { return true; }

    TextImpl *splitText ( const unsigned long offset, int &exceptioncode );

    virtual ushort id() const;

    virtual khtml::RenderStyle *style() const;

    virtual void attach();
    virtual void detach();
    virtual void applyChanges(bool top=true, bool force=true);

    virtual bool prepareMouseEvent( int _x, int _y,
                                    int _tx, int _ty,
                                    MouseEvent *ev);

    virtual khtml::FindSelectionResult findSelectionNode( int _x, int _y, int _tx, int _ty,
                                                   DOM::Node & node, int & offset );

    virtual NodeImpl *cloneNode(bool deep, int &exceptioncode);
    virtual void recalcStyle();
    virtual bool childTypeAllowed( unsigned short type );

protected:
    virtual TextImpl *createNew(DOMStringImpl *_str);
};

// ----------------------------------------------------------------------------

class CDATASectionImpl : public TextImpl
{
// ### should these have id==ID_TEXT
public:
    CDATASectionImpl(DocumentPtr *impl, const DOMString &_text);
    CDATASectionImpl(DocumentPtr *impl);
    virtual ~CDATASectionImpl();
    virtual const DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual NodeImpl *cloneNode(bool deep, int &exceptioncode);
    virtual bool childTypeAllowed( unsigned short type );

protected:
    virtual TextImpl *createNew(DOMStringImpl *_str);
};



}; //namespace
#endif
