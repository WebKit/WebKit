/**
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
 */

#include "config.h"
#include "CharacterDataImpl.h"

#include "DocumentImpl.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "RenderText.h"
#include "dom2_eventsimpl.h"
#include <qtextstream.h>

namespace WebCore {

using namespace EventNames;

CharacterDataImpl::CharacterDataImpl(DocumentImpl *doc)
    : NodeImpl(doc)
{
    str = 0;
}

CharacterDataImpl::CharacterDataImpl(DocumentImpl *doc, const DOMString &_text)
    : NodeImpl(doc)
{
    str = _text.impl() ? _text.impl() : new DOMStringImpl((QChar*)0, 0);
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

void CharacterDataImpl::setData( const DOMString &_data, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    if(str == _data.impl()) return; // ### fire DOMCharacterDataModified if modified?
    DOMStringImpl *oldStr = str;
    str = _data.impl();
    if(str) str->ref();
    if (renderer())
        static_cast<RenderText*>(renderer())->setText(str);
    
    dispatchModifiedEvent(oldStr);
    if(oldStr) oldStr->deref();
    
    getDocument()->removeMarkers(this);
}

unsigned CharacterDataImpl::length() const
{
    return str->l;
}

DOMString CharacterDataImpl::substringData( const unsigned offset, const unsigned count, ExceptionCode& ec)
{
    ec = 0;
    checkCharDataOperation(offset, ec);
    if (ec)
        return DOMString();

    return str->substring(offset,count);
}

void CharacterDataImpl::appendData( const DOMString &arg, ExceptionCode& ec)
{
    ec = 0;

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    DOMStringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->append(arg.impl());
    if (renderer())
        static_cast<RenderText*>(renderer())->setTextWithOffset(str, oldStr->l, 0);
    
    dispatchModifiedEvent(oldStr);
    oldStr->deref();
}

void CharacterDataImpl::insertData( const unsigned offset, const DOMString &arg, ExceptionCode& ec)
{
    ec = 0;
    checkCharDataOperation(offset, ec);
    if (ec)
        return;

    DOMStringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->insert(arg.impl(), offset);
    if (renderer())
        static_cast<RenderText*>(renderer())->setTextWithOffset(str, offset, 0);
    
    dispatchModifiedEvent(oldStr);
    oldStr->deref();
    
    // update the markers for spell checking and grammar checking
    uint length = arg.length();
    getDocument()->shiftMarkers(this, offset, length);
}

void CharacterDataImpl::deleteData( const unsigned offset, const unsigned count, ExceptionCode& ec)
{
    ec = 0;
    checkCharDataOperation(offset, ec);
    if (ec)
        return;

    DOMStringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->remove(offset,count);
    if (renderer())
        static_cast<RenderText*>(renderer())->setTextWithOffset(str, offset, count);
    
    dispatchModifiedEvent(oldStr);
    oldStr->deref();

    // update the markers for spell checking and grammar checking
    getDocument()->removeMarkers(this, offset, count);
    getDocument()->shiftMarkers(this, offset + count, -count);
}

void CharacterDataImpl::replaceData( const unsigned offset, const unsigned count, const DOMString &arg, ExceptionCode& ec)
{
    ec = 0;
    checkCharDataOperation(offset, ec);
    if (ec)
        return;

    unsigned realCount;
    if (offset + count > str->l)
        realCount = str->l-offset;
    else
        realCount = count;

    DOMStringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->remove(offset,realCount);
    str->insert(arg.impl(), offset);
    if (renderer())
        static_cast<RenderText*>(renderer())->setTextWithOffset(str, offset, count);
    
    dispatchModifiedEvent(oldStr);
    oldStr->deref();
    
    // update the markers for spell checking and grammar checking
    int diff = arg.length() - count;
    getDocument()->removeMarkers(this, offset, count);
    getDocument()->shiftMarkers(this, offset + count, diff);
}

DOMString CharacterDataImpl::nodeValue() const
{
    return str;
}

bool CharacterDataImpl::containsOnlyWhitespace(unsigned int from, unsigned int len) const
{
    if (str)
        return str->containsOnlyWhitespace(from, len);
    return true;
}

bool CharacterDataImpl::containsOnlyWhitespace() const
{
    if (str)
        return str->containsOnlyWhitespace();
    return true;
}

void CharacterDataImpl::setNodeValue( const DOMString &_nodeValue, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setData()
    setData(_nodeValue, ec);
}

void CharacterDataImpl::dispatchModifiedEvent(DOMStringImpl *prevValue)
{
    if (parentNode())
        parentNode()->childrenChanged();
    if (!getDocument()->hasListenerType(DocumentImpl::DOMCHARACTERDATAMODIFIED_LISTENER))
        return;

    DOMStringImpl *newValue = str->copy();
    newValue->ref();
    ExceptionCode ec = 0;
    dispatchEvent(new MutationEventImpl(DOMCharacterDataModifiedEvent,
                  true,false,0,prevValue,newValue,DOMString(),0),ec);
    newValue->deref();
    dispatchSubtreeModifiedEvent();
}

void CharacterDataImpl::checkCharDataOperation( const unsigned offset, ExceptionCode& ec)
{
    ec = 0;

    // INDEX_SIZE_ERR: Raised if the specified offset is negative or greater than the number of 16-bit
    // units in data.
    if (offset > str->l) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
}

int CharacterDataImpl::maxOffset() const 
{
    return (int)length();
}

int CharacterDataImpl::caretMinOffset() const 
{
    RenderText *r = static_cast<RenderText *>(renderer());
    return r && r->isText() ? r->caretMinOffset() : 0;
}

int CharacterDataImpl::caretMaxOffset() const 
{
    RenderText *r = static_cast<RenderText *>(renderer());
    return r && r->isText() ? r->caretMaxOffset() : (int)length();
}

unsigned CharacterDataImpl::caretMaxRenderedOffset() const 
{
    RenderText *r = static_cast<RenderText *>(renderer());
    return r ? r->caretMaxRenderedOffset() : length();
}

bool CharacterDataImpl::rendererIsNeeded(RenderStyle *style)
{
    if (!str || str->l == 0)
        return false;
    return NodeImpl::rendererIsNeeded(style);
}

bool CharacterDataImpl::offsetInCharacters() const
{
    return true;
}

#ifndef NDEBUG
void CharacterDataImpl::dump(QTextStream *stream, QString ind) const
{
    *stream << " str=\"" << DOMString(str).qstring().ascii() << "\"";

    NodeImpl::dump(stream,ind);
}
#endif

} // namespace WebCore
