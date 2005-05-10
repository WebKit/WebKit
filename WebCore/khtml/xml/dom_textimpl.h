/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#ifndef _DOM_CharacterDataImpl_h_
#define _DOM_CharacterDataImpl_h_

#include "xml/dom_nodeimpl.h"
#include "dom/dom_string.h"

namespace DOM {

    class DocumentImpl;
#if APPLE_CHANGES
    class CharacterData;
    class Text;
#endif

class CharacterDataImpl : public NodeImpl
{
public:
    CharacterDataImpl(DocumentPtr *doc, const DOMString &_text);
    CharacterDataImpl(DocumentPtr *doc);
    virtual ~CharacterDataImpl();

    // DOM methods & attributes for CharacterData

    virtual DOMString data() const;
    virtual void setData( const DOMString &_data, int &exceptioncode );
    virtual unsigned long length (  ) const;
    virtual DOMString substringData ( const unsigned long offset, const unsigned long count, int &exceptioncode );
    virtual void appendData ( const DOMString &arg, int &exceptioncode );
    virtual void insertData ( const unsigned long offset, const DOMString &arg, int &exceptioncode );
    virtual void deleteData ( const unsigned long offset, const unsigned long count, int &exceptioncode );
    virtual void replaceData ( const unsigned long offset, const unsigned long count, const DOMString &arg, int &exceptioncode );

    virtual bool containsOnlyWhitespace() const;
    bool containsOnlyWhitespace(unsigned int from, unsigned int len) const;
    
    // DOM methods overridden from  parent classes

    virtual DOMString nodeValue() const;
    virtual void setNodeValue( const DOMString &_nodeValue, int &exceptioncode );

    // Other methods (not part of DOM)

    DOMStringImpl *string() { return str; }
    virtual void checkCharDataOperation( const unsigned long offset, int &exceptioncode );

    virtual long maxOffset() const;
    virtual long caretMinOffset() const;
    virtual long caretMaxOffset() const;
    virtual unsigned long caretMaxRenderedOffset() const;

    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    
#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

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

    // DOM methods overridden from  parent classes
    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual NodeImpl *cloneNode(bool deep);

    // Other methods (not part of DOM)

    virtual Id id() const;
    virtual bool childTypeAllowed( unsigned short type );

    virtual DOMString toString() const;
};

// ----------------------------------------------------------------------------

class TextImpl : public CharacterDataImpl
{
public:
    TextImpl(DocumentPtr *impl, const DOMString &_text);
    TextImpl(DocumentPtr *impl);
    virtual ~TextImpl();

    // DOM methods & attributes for CharacterData

    TextImpl *splitText ( const unsigned long offset, int &exceptioncode );

    // DOM methods overridden from  parent classes
    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual NodeImpl *cloneNode(bool deep);

    // Other methods (not part of DOM)

    virtual bool isTextNode() const { return true; }
    virtual Id id() const;
    virtual void attach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void recalcStyle( StyleChange = NoChange );
    virtual bool childTypeAllowed( unsigned short type );

    virtual DOMString toString() const;

#ifndef NDEBUG
    virtual void formatForDebugger(char *buffer, unsigned length) const;
#endif

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

    // DOM methods overridden from  parent classes
    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual NodeImpl *cloneNode(bool deep);

    // Other methods (not part of DOM)

    virtual bool childTypeAllowed( unsigned short type );

    virtual DOMString toString() const;

protected:
    virtual TextImpl *createNew(DOMStringImpl *_str);
};

// ----------------------------------------------------------------------------

class EditingTextImpl : public TextImpl
{
public:
    EditingTextImpl(DocumentPtr *impl, const DOMString &text);
    EditingTextImpl(DocumentPtr *impl);
    virtual ~EditingTextImpl();

    virtual bool rendererIsNeeded(khtml::RenderStyle *);
};

} //namespace
#endif
