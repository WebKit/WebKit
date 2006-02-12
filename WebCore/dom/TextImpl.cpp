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
#include "TextImpl.h"

#include "DocumentImpl.h"
#include "dom_exception.h"
#include "dom_node.h"
#include "RenderText.h"

namespace WebCore {

// DOM Section 1.1.1

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

} // namespace WebCore
