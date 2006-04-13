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
#include "CharacterData.h"

#include "Document.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "RenderText.h"
#include "dom2_eventsimpl.h"
#include "KWQTextStream.h"

namespace WebCore {

using namespace EventNames;

CharacterData::CharacterData(Document *doc)
    : EventTargetNode(doc)
{
    str = 0;
}

CharacterData::CharacterData(Document *doc, const String &_text)
    : EventTargetNode(doc)
{
    str = _text.impl() ? _text.impl() : new StringImpl((QChar*)0, 0);
    str->ref();
}

CharacterData::~CharacterData()
{
    if(str) str->deref();
}

String CharacterData::data() const
{
    return str;
}

void CharacterData::setData( const String &_data, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    if(str == _data.impl()) return; // ### fire DOMCharacterDataModified if modified?
    StringImpl *oldStr = str;
    str = _data.impl();
    if (str)
        str->ref();
    
    if (!renderer() && attached()) {
        detach();
        attach();
    }
    
    if (renderer())
        static_cast<RenderText*>(renderer())->setText(str);
    
    dispatchModifiedEvent(oldStr);
    if(oldStr) oldStr->deref();
    
    document()->removeMarkers(this);
}

unsigned CharacterData::length() const
{
    return str->length();
}

String CharacterData::substringData( const unsigned offset, const unsigned count, ExceptionCode& ec)
{
    ec = 0;
    checkCharDataOperation(offset, ec);
    if (ec)
        return String();

    return str->substring(offset,count);
}

void CharacterData::appendData( const String &arg, ExceptionCode& ec)
{
    ec = 0;

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    StringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->append(arg.impl());

    if (!renderer() && attached()) {
        detach();
        attach();
    }

    if (renderer())
        static_cast<RenderText*>(renderer())->setTextWithOffset(str, oldStr->length(), 0);
    
    dispatchModifiedEvent(oldStr);
    oldStr->deref();
}

void CharacterData::insertData( const unsigned offset, const String &arg, ExceptionCode& ec)
{
    ec = 0;
    checkCharDataOperation(offset, ec);
    if (ec)
        return;

    StringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->insert(arg.impl(), offset);

    if (!renderer() && attached()) {
        detach();
        attach();
    }

    if (renderer())
        static_cast<RenderText*>(renderer())->setTextWithOffset(str, offset, 0);
    
    dispatchModifiedEvent(oldStr);
    oldStr->deref();
    
    // update the markers for spell checking and grammar checking
    unsigned length = arg.length();
    document()->shiftMarkers(this, offset, length);
}

void CharacterData::deleteData( const unsigned offset, const unsigned count, ExceptionCode& ec)
{
    ec = 0;
    checkCharDataOperation(offset, ec);
    if (ec)
        return;

    StringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->remove(offset,count);
    if (renderer())
        static_cast<RenderText*>(renderer())->setTextWithOffset(str, offset, count);
    
    dispatchModifiedEvent(oldStr);
    oldStr->deref();

    // update the markers for spell checking and grammar checking
    document()->removeMarkers(this, offset, count);
    document()->shiftMarkers(this, offset + count, -count);
}

void CharacterData::replaceData( const unsigned offset, const unsigned count, const String &arg, ExceptionCode& ec)
{
    ec = 0;
    checkCharDataOperation(offset, ec);
    if (ec)
        return;

    unsigned realCount;
    if (offset + count > str->length())
        realCount = str->length()-offset;
    else
        realCount = count;

    StringImpl *oldStr = str;
    str = str->copy();
    str->ref();
    str->remove(offset,realCount);
    str->insert(arg.impl(), offset);

    if (!renderer() && attached()) {
        detach();
        attach();
    }

    if (renderer())
        static_cast<RenderText*>(renderer())->setTextWithOffset(str, offset, count);
    
    dispatchModifiedEvent(oldStr);
    oldStr->deref();
    
    // update the markers for spell checking and grammar checking
    int diff = arg.length() - count;
    document()->removeMarkers(this, offset, count);
    document()->shiftMarkers(this, offset + count, diff);
}

String CharacterData::nodeValue() const
{
    return str;
}

bool CharacterData::containsOnlyWhitespace(unsigned int from, unsigned int len) const
{
    if (str)
        return str->containsOnlyWhitespace(from, len);
    return true;
}

bool CharacterData::containsOnlyWhitespace() const
{
    if (str)
        return str->containsOnlyWhitespace();
    return true;
}

void CharacterData::setNodeValue( const String &_nodeValue, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setData()
    setData(_nodeValue, ec);
}

void CharacterData::dispatchModifiedEvent(StringImpl *prevValue)
{
    if (parentNode())
        parentNode()->childrenChanged();
    if (!document()->hasListenerType(Document::DOMCHARACTERDATAMODIFIED_LISTENER))
        return;

    StringImpl *newValue = str->copy();
    newValue->ref();
    ExceptionCode ec = 0;
    dispatchEvent(new MutationEvent(DOMCharacterDataModifiedEvent,
                  true,false,0,prevValue,newValue,String(),0),ec);
    newValue->deref();
    dispatchSubtreeModifiedEvent();
}

void CharacterData::checkCharDataOperation( const unsigned offset, ExceptionCode& ec)
{
    ec = 0;

    // INDEX_SIZE_ERR: Raised if the specified offset is negative or greater than the number of 16-bit
    // units in data.
    if (offset > str->length()) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
}

int CharacterData::maxOffset() const 
{
    return (int)length();
}

int CharacterData::caretMinOffset() const 
{
    RenderText *r = static_cast<RenderText *>(renderer());
    return r && r->isText() ? r->caretMinOffset() : 0;
}

int CharacterData::caretMaxOffset() const 
{
    RenderText *r = static_cast<RenderText *>(renderer());
    return r && r->isText() ? r->caretMaxOffset() : (int)length();
}

unsigned CharacterData::caretMaxRenderedOffset() const 
{
    RenderText *r = static_cast<RenderText *>(renderer());
    return r ? r->caretMaxRenderedOffset() : length();
}

bool CharacterData::rendererIsNeeded(RenderStyle *style)
{
    if (!str || str->length() == 0)
        return false;
    return EventTargetNode::rendererIsNeeded(style);
}

bool CharacterData::offsetInCharacters() const
{
    return true;
}

#ifndef NDEBUG
void CharacterData::dump(QTextStream *stream, DeprecatedString ind) const
{
    *stream << " str=\"" << String(str).deprecatedString().ascii() << "\"";

    EventTargetNode::dump(stream,ind);
}
#endif

} // namespace WebCore
