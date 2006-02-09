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
#include "dom/dom_exception.h"
#include "css/cssstyleselector.h"
#include "dom2_eventsimpl.h"
#include "dom_textimpl.h"
#include "DocumentImpl.h"
#include "EventNames.h"

#include "RenderText.h"

#include <kdebug.h>

using namespace DOM;
using namespace DOM::EventNames;
using namespace khtml;

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

void CharacterDataImpl::setData( const DOMString &_data, int &exceptioncode )
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
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

DOMString CharacterDataImpl::substringData( const unsigned offset, const unsigned count, int &exceptioncode )
{
    exceptioncode = 0;
    checkCharDataOperation(offset, exceptioncode);
    if (exceptioncode)
        return DOMString();

    return str->substring(offset,count);
}

void CharacterDataImpl::appendData( const DOMString &arg, int &exceptioncode )
{
    exceptioncode = 0;

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
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

void CharacterDataImpl::insertData( const unsigned offset, const DOMString &arg, int &exceptioncode )
{
    exceptioncode = 0;
    checkCharDataOperation(offset, exceptioncode);
    if (exceptioncode)
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

void CharacterDataImpl::deleteData( const unsigned offset, const unsigned count, int &exceptioncode )
{
    exceptioncode = 0;
    checkCharDataOperation(offset, exceptioncode);
    if (exceptioncode)
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

void CharacterDataImpl::replaceData( const unsigned offset, const unsigned count, const DOMString &arg, int &exceptioncode )
{
    exceptioncode = 0;
    checkCharDataOperation(offset, exceptioncode);
    if (exceptioncode)
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

void CharacterDataImpl::setNodeValue( const DOMString &_nodeValue, int &exceptioncode )
{
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setData()
    setData(_nodeValue, exceptioncode);
}

void CharacterDataImpl::dispatchModifiedEvent(DOMStringImpl *prevValue)
{
    if (parentNode())
        parentNode()->childrenChanged();
    if (!getDocument()->hasListenerType(DocumentImpl::DOMCHARACTERDATAMODIFIED_LISTENER))
        return;

    DOMStringImpl *newValue = str->copy();
    newValue->ref();
    int exceptioncode = 0;
    dispatchEvent(new MutationEventImpl(DOMCharacterDataModifiedEvent,
                  true,false,0,prevValue,newValue,DOMString(),0),exceptioncode);
    newValue->deref();
    dispatchSubtreeModifiedEvent();
}

void CharacterDataImpl::checkCharDataOperation( const unsigned offset, int &exceptioncode )
{
    exceptioncode = 0;

    // INDEX_SIZE_ERR: Raised if the specified offset is negative or greater than the number of 16-bit
    // units in data.
    if (offset > str->l) {
        exceptioncode = DOMException::INDEX_SIZE_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
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

#ifndef NDEBUG
void CharacterDataImpl::dump(QTextStream *stream, QString ind) const
{
    *stream << " str=\"" << DOMString(str).qstring().ascii() << "\"";

    NodeImpl::dump(stream,ind);
}
#endif

// ---------------------------------------------------------------------------

CommentImpl::CommentImpl(DocumentImpl *doc, const DOMString &_text)
    : CharacterDataImpl(doc, _text)
{
}

CommentImpl::CommentImpl(DocumentImpl *doc)
    : CharacterDataImpl(doc)
{
}

CommentImpl::~CommentImpl()
{
}

const AtomicString& CommentImpl::localName() const
{
    return commentAtom;
}

DOMString CommentImpl::nodeName() const
{
    return commentAtom.domString();
}

unsigned short CommentImpl::nodeType() const
{
    return Node::COMMENT_NODE;
}

PassRefPtr<NodeImpl> CommentImpl::cloneNode(bool /*deep*/)
{
    return getDocument()->createComment( str );
}

// DOM Section 1.1.1
bool CommentImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

DOMString CommentImpl::toString() const
{
    // FIXME: substitute entity references as needed!
    return DOMString("<!--") + nodeValue() + "-->";
}

// ---------------------------------------------------------------------------

// ### allow having children in text nodes for entities, comments etc.

TextImpl::TextImpl(DocumentImpl *doc, const DOMString &_text)
    : CharacterDataImpl(doc, _text)
{
}

TextImpl::TextImpl(DocumentImpl *doc)
    : CharacterDataImpl(doc)
{
}

TextImpl::~TextImpl()
{
}

TextImpl *TextImpl::splitText( const unsigned offset, int &exceptioncode )
{
    exceptioncode = 0;

    // INDEX_SIZE_ERR: Raised if the specified offset is negative or greater than
    // the number of 16-bit units in data.

    // ### we explicitly check for a negative number that has been cast to an unsigned
    // ... this can happen if JS code passes in -1 - we need to catch this earlier! (in the
    // kjs bindings)
    if (offset > str->l || (int)offset < 0) {
        exceptioncode = DOMException::INDEX_SIZE_ERR;
        return 0;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    DOMStringImpl *oldStr = str;
    TextImpl *newText = createNew(str->substring(offset,str->l-offset));
    str = str->copy();
    str->ref();
    str->remove(offset,str->l-offset);

    dispatchModifiedEvent(oldStr);
    oldStr->deref();

    if (parentNode())
        parentNode()->insertBefore(newText,nextSibling(), exceptioncode );
    if ( exceptioncode )
        return 0;

    if (renderer())
        static_cast<RenderText*>(renderer())->setText(str);

    return newText;
}

const AtomicString& TextImpl::localName() const
{
    return textAtom;
}

DOMString TextImpl::nodeName() const
{
    return textAtom.domString();
}

unsigned short TextImpl::nodeType() const
{
    return Node::TEXT_NODE;
}

PassRefPtr<NodeImpl> TextImpl::cloneNode(bool /*deep*/)
{
    return getDocument()->createTextNode(str);
}

bool TextImpl::rendererIsNeeded(RenderStyle *style)
{
    if (!CharacterDataImpl::rendererIsNeeded(style))
        return false;

    bool onlyWS = containsOnlyWhitespace();
    if (!onlyWS)
        return true;

    RenderObject *par = parentNode()->renderer();
    
    if (par->isTable() || par->isTableRow() || par->isTableSection() || par->isTableCol() || par->isFrameSet())
        return false;
    
    if (style->preserveNewline()) // pre/pre-wrap/pre-line always make renderers.
        return true;
    
    RenderObject *prev = previousRenderer();
    if (prev && prev->isBR()) // <span><br/> <br/></span>
        return false;
        
    if (par->isInline()) {
        // <span><div/> <div/></span>
        if (prev && prev->isRenderBlock())
            return false;
    } else {
        if (par->isRenderBlock() && !par->childrenInline() && (!prev || !prev->isInline()))
            return false;
        
        RenderObject *first = par->firstChild();
        while (first && first->isFloatingOrPositioned())
            first = first->nextSibling();
        RenderObject *next = nextRenderer();
        if (!first || next == first)
            // Whitespace at the start of a block just goes away.  Don't even
            // make a render object for this text.
            return false;
    }
    
    return true;
}

RenderObject *TextImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderText(this, str);
}

void TextImpl::attach()
{
    createRendererIfNeeded();
    CharacterDataImpl::attach();
}

void TextImpl::recalcStyle( StyleChange change )
{
    if (change != NoChange && parentNode())
        if (renderer())
            renderer()->setStyle(parentNode()->renderer()->style());
    if (changed() && renderer() && renderer()->isText())
        static_cast<RenderText*>(renderer())->setText(str);
    setChanged(false);
}

// DOM Section 1.1.1
bool TextImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

TextImpl *TextImpl::createNew(DOMStringImpl *_str)
{
    return new TextImpl(getDocument(), _str);
}

DOMString TextImpl::toString() const
{
    // FIXME: substitute entity references as needed!
    return nodeValue();
}

#ifndef NDEBUG
void TextImpl::formatForDebugger(char *buffer, unsigned length) const
{
    DOMString result;
    DOMString s;
    
    s = nodeName();
    if (s.length() > 0) {
        result += s;
    }
          
    s = nodeValue();
    if (s.length() > 0) {
        if (result.length() > 0)
            result += "; ";
        result += "value=";
        result += s;
    }
          
    strncpy(buffer, result.qstring().latin1(), length - 1);
}
#endif

// ---------------------------------------------------------------------------

CDATASectionImpl::CDATASectionImpl(DocumentImpl *impl, const DOMString &_text) : TextImpl(impl,_text)
{
}

CDATASectionImpl::CDATASectionImpl(DocumentImpl *impl) : TextImpl(impl)
{
}

CDATASectionImpl::~CDATASectionImpl()
{
}

DOMString CDATASectionImpl::nodeName() const
{
  return "#cdata-section";
}

unsigned short CDATASectionImpl::nodeType() const
{
    return Node::CDATA_SECTION_NODE;
}

PassRefPtr<NodeImpl> CDATASectionImpl::cloneNode(bool /*deep*/)
{
    int ignoreException = 0;
    return getDocument()->createCDATASection(str, ignoreException);
}

// DOM Section 1.1.1
bool CDATASectionImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

TextImpl *CDATASectionImpl::createNew(DOMStringImpl *_str)
{
    return new CDATASectionImpl(getDocument(), _str);
}

DOMString CDATASectionImpl::toString() const
{
    // FIXME: substitute entity references as needed!
    return DOMString("<![CDATA[") + nodeValue() + "]]>";
}

// ---------------------------------------------------------------------------

EditingTextImpl::EditingTextImpl(DocumentImpl *impl, const DOMString &text)
    : TextImpl(impl, text)
{
}

EditingTextImpl::EditingTextImpl(DocumentImpl *impl)
    : TextImpl(impl)
{
}

EditingTextImpl::~EditingTextImpl()
{
}

bool EditingTextImpl::rendererIsNeeded(RenderStyle *style)
{
    return true;
}

