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
#include "dom_textimpl.h"

#include "dom_stringimpl.h"
#include "dom_exception.h"

#include "dom/dom_string.h"
#include "dom_docimpl.h"
#include "dom2_eventsimpl.h"

#include "dom/dom_node.h"
#include "misc/htmlhashes.h"
#include "rendering/render_text.h"

#include <kdebug.h>

using namespace DOM;
using namespace khtml;


CharacterDataImpl::CharacterDataImpl(DocumentPtr *doc) : NodeWParentImpl(doc)
{
    str = 0;
}

CharacterDataImpl::CharacterDataImpl(DocumentPtr *doc, const DOMString &_text)
    : NodeWParentImpl(doc)
{
//    str = new DOMStringImpl(_text.impl->s,_text.impl->l);
    str = _text.impl;
    str->ref();
}

CharacterDataImpl::~CharacterDataImpl()
{
    if(str) str->deref();
}

DOMString CharacterDataImpl::data() const
{
    return str;
}

void CharacterDataImpl::setData( const DOMString &newStr )
{
    if(str == newStr.impl) return; // ### fire DOMCharacterDataModified if modified?
    DOMStringImpl *oldStr = str;
    str = newStr.impl;
    if(str) str->ref();
    if (m_render)
      (static_cast<RenderText*>(m_render))->setText(str);
    setChanged(true);

    dispatchModifiedEvent(oldStr);
    if(oldStr) oldStr->deref();
}

unsigned long CharacterDataImpl::length() const
{
    return str->l;
}

DOMString CharacterDataImpl::substringData( const unsigned long offset, const unsigned long count, int &exceptioncode )
{
    exceptioncode = 0;
    if (offset > str->l ) {
        exceptioncode = DOMException::INDEX_SIZE_ERR;
        return DOMString();
    }
    return str->substring(offset,count);
}

void CharacterDataImpl::appendData( const DOMString &arg )
{
    DOMStringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->append(arg.impl);
    if (m_render)
      (static_cast<RenderText*>(m_render))->setText(str);
    setChanged(true);

    dispatchModifiedEvent(oldStr);
    oldStr->deref();
}

void CharacterDataImpl::insertData( const unsigned long offset, const DOMString &arg, int &exceptioncode )
{
    exceptioncode = 0;
    if (offset > str->l) {
        exceptioncode = DOMException::INDEX_SIZE_ERR;
        return;
    }
    DOMStringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->insert(arg.impl, offset);
    if (m_render)
      (static_cast<RenderText*>(m_render))->setText(str);
    setChanged(true);

    dispatchModifiedEvent(oldStr);
    oldStr->deref();
}

void CharacterDataImpl::deleteData( const unsigned long offset, const unsigned long count, int &exceptioncode )
{
    exceptioncode = 0;
    if (offset > str->l) {
        exceptioncode = DOMException::INDEX_SIZE_ERR;
        return;
    }

    DOMStringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->remove(offset,count);
    if (m_render)
      (static_cast<RenderText*>(m_render))->setText(str);
    setChanged(true);

    dispatchModifiedEvent(oldStr);
    oldStr->deref();
}

void CharacterDataImpl::replaceData( const unsigned long offset, const unsigned long count, const DOMString &arg, int &exceptioncode )
{
    exceptioncode = 0;
    if (offset > str->l) {
        exceptioncode = DOMException::INDEX_SIZE_ERR;
        return;
    }

    unsigned long realCount;
    if (offset + count > str->l)
        realCount = str->l-offset;
    else
        realCount = count;

    DOMStringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->remove(offset,realCount);
    str->insert(arg.impl, offset);
    if (m_render)
      (static_cast<RenderText*>(m_render))->setText(str);
    setChanged(true);

    dispatchModifiedEvent(oldStr);
    oldStr->deref();
}

void CharacterDataImpl::dispatchModifiedEvent(DOMStringImpl *prevValue)
{
    // ### fixme (?) - hack so STYLE elements reparse their stylesheet when text changes
    if (_parent)
        _parent->setChanged(true);
    if (!getDocument()->hasListenerType(DocumentImpl::DOMCHARACTERDATAMODIFIED_LISTENER))
	return;

    DOMStringImpl *newValue = str->copy();
    newValue->ref();
    int exceptioncode;
    dispatchEvent(new MutationEventImpl(EventImpl::DOMCHARACTERDATAMODIFIED_EVENT,
		  true,false,0,prevValue,newValue,0,0),exceptioncode);
    newValue->deref();
    dispatchSubtreeModifiedEvent();
}

// ---------------------------------------------------------------------------

CommentImpl::CommentImpl(DocumentPtr *doc, const DOMString &_text)
    : CharacterDataImpl(doc, _text)
{
}

CommentImpl::CommentImpl(DocumentPtr *doc)
    : CharacterDataImpl(doc)
{
}

CommentImpl::~CommentImpl()
{
}

const DOMString CommentImpl::nodeName() const
{
    return "#comment";
}

DOMString CommentImpl::nodeValue() const
{
    return str;
}

unsigned short CommentImpl::nodeType() const
{
    return Node::COMMENT_NODE;
}

ushort CommentImpl::id() const
{
    return ID_COMMENT;
}

NodeImpl *CommentImpl::cloneNode(bool /*deep*/, int &/*exceptioncode*/)
{
    return ownerDocument()->createComment( str );
}

// DOM Section 1.1.1
bool CommentImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

// ---------------------------------------------------------------------------

// ### allow having children in text nodes for entities, comments etc.

TextImpl::TextImpl(DocumentPtr *doc, const DOMString &_text)
    : CharacterDataImpl(doc, _text)
{
}

TextImpl::TextImpl(DocumentPtr *doc)
    : CharacterDataImpl(doc)
{
}

TextImpl::~TextImpl()
{
}

TextImpl *TextImpl::splitText( const unsigned long offset, int &exceptioncode )
{
    exceptioncode = 0;
    if (offset > str->l || (long)offset < 0) {
        exceptioncode = DOMException::INDEX_SIZE_ERR;
        return 0;
    }

    DOMStringImpl *oldStr = str;
    TextImpl *newText = createNew(str->substring(offset,str->l-offset));
    str = str->copy();
    str->ref();
    str->remove(offset,str->l-offset);

    dispatchModifiedEvent(oldStr);
    oldStr->deref();

    if (_parent)
        _parent->insertBefore(newText,_next, exceptioncode );
    if ( exceptioncode )
        return 0;

    if (m_render)
        (static_cast<RenderText*>(m_render))->setText(str);
    setChanged(true);
    return newText;
}

const DOMString TextImpl::nodeName() const
{
  return "#text";
}
DOMString TextImpl::nodeValue() const
{
    return str;
}


unsigned short TextImpl::nodeType() const
{
    return Node::TEXT_NODE;
}

void TextImpl::attach()
{
    if (!m_render) {
        RenderObject *r = _parent->renderer();
        if( r && style() ) {
	    m_render = new RenderText(str);
	    m_render->setStyle( style() );
	    r->addChild(m_render, nextRenderer());
        }
    }

    CharacterDataImpl::attach();
}

void TextImpl::detach()
{
    CharacterDataImpl::detach();

    if ( m_render )
        m_render->detach();

    m_render = 0;
}

void TextImpl::applyChanges(bool,bool force)
{
    if (force || changed())
        recalcStyle();
    setChanged(false);
}

khtml::RenderStyle *TextImpl::style() const
{
    return _parent ? _parent->style() : 0;
}

bool TextImpl::prepareMouseEvent( int _x, int _y,
                                  int _tx, int _ty,
                                  MouseEvent *ev)
{
    //kdDebug( 6020 ) << "Text::prepareMouseEvent" << endl;

    if(!m_render) return false;

    if (m_render->style() && m_render->style()->visiblity() == HIDDEN)
        return false;

    int origTx = _tx;
    int origTy = _ty;

    if(m_render->parent() && m_render->parent()->isAnonymousBox())
    {
        // we need to add the offset of the anonymous box
        _tx += m_render->parent()->xPos();
        _ty += m_render->parent()->yPos();
    }

    if( static_cast<RenderText *>(m_render)->checkPoint(_x, _y, _tx, _ty) )
    {
        ev->innerNode = Node(this);
        ev->nodeAbsX = origTx;
        ev->nodeAbsY = origTy;
        return true;
    }
    return false;
}

khtml::FindSelectionResult TextImpl::findSelectionNode( int _x, int _y, int _tx, int _ty,
                                                 DOM::Node & node, int & offset )
{
    //kdDebug(6030) << "TextImpl::findSelectionNode " << this << " _x=" << _x << " _y=" << _y
    //           << " _tx=" << _tx << " _ty=" << _ty << endl;
    if(!m_render) return SelectionPointBefore;

    if(m_render->parent() && m_render->parent()->isAnonymousBox())
    {
        // we need to add the offset of the anonymous box
        _tx += m_render->parent()->xPos();
        _ty += m_render->parent()->yPos();
    }

    node = this;
    return static_cast<RenderText *>(m_render)->checkSelectionPoint(_x, _y, _tx, _ty, offset);
}

ushort TextImpl::id() const
{
    return ID_TEXT;
}

NodeImpl *TextImpl::cloneNode(bool /*deep*/, int &/*exceptioncode*/)
{
    return ownerDocument()->createTextNode(str);
}

void TextImpl::recalcStyle()
{
    if (!parentNode())
        return;
    if(m_render) m_render->setStyle(parentNode()->style());
}

// DOM Section 1.1.1
bool TextImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

TextImpl *TextImpl::createNew(DOMStringImpl *_str)
{
    return new TextImpl(docPtr(),_str);
}

// ---------------------------------------------------------------------------

CDATASectionImpl::CDATASectionImpl(DocumentPtr *impl, const DOMString &_text) : TextImpl(impl,_text)
{
}

CDATASectionImpl::CDATASectionImpl(DocumentPtr *impl) : TextImpl(impl)
{
}

CDATASectionImpl::~CDATASectionImpl()
{
}

const DOMString CDATASectionImpl::nodeName() const
{
  return "#cdata-section";
}

unsigned short CDATASectionImpl::nodeType() const
{
    return Node::CDATA_SECTION_NODE;
}

NodeImpl *CDATASectionImpl::cloneNode(bool /*deep*/, int &/*exceptioncode*/)
{
    return ownerDocument()->createCDATASection(str);
}

// DOM Section 1.1.1
bool CDATASectionImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

TextImpl *CDATASectionImpl::createNew(DOMStringImpl *_str)
{
    return new CDATASectionImpl(docPtr(),_str);
}




