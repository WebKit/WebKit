/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Text.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "RenderText.h"
#include "TextBreakIterator.h"

#if ENABLE(SVG)
#include "RenderSVGInlineText.h"
#endif // ENABLE(SVG)

namespace WebCore {

// DOM Section 1.1.1

// ### allow having children in text nodes for entities, comments etc.

Text::Text(Document *doc, const String &_text)
    : CharacterData(doc, _text)
{
}

Text::Text(Document *doc)
    : CharacterData(doc)
{
}

Text::~Text()
{
}

PassRefPtr<Text> Text::splitText(unsigned offset, ExceptionCode& ec)
{
    ec = 0;

    // FIXME: This does not copy markers
    
    // INDEX_SIZE_ERR: Raised if the specified offset is negative or greater than
    // the number of 16-bit units in data.
    if (offset > m_data->length()) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if (isReadOnlyNode()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    RefPtr<StringImpl> oldStr = m_data;
    RefPtr<Text> newText = createNew(oldStr->substring(offset));
    m_data = oldStr->substring(0, offset);

    dispatchModifiedEvent(oldStr.get());

    if (parentNode())
        parentNode()->insertBefore(newText.get(), nextSibling(), ec);
    if (ec)
        return 0;

    if (renderer())
        static_cast<RenderText*>(renderer())->setText(m_data);

    return newText.release();
}

String Text::nodeName() const
{
    return textAtom.domString();
}

Node::NodeType Text::nodeType() const
{
    return TEXT_NODE;
}

PassRefPtr<Node> Text::cloneNode(bool /*deep*/)
{
    return document()->createTextNode(m_data);
}

bool Text::rendererIsNeeded(RenderStyle *style)
{
    if (!CharacterData::rendererIsNeeded(style))
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
        
    if (par->isInlineFlow()) {
        // <span><div/> <div/></span>
        if (prev && !prev->isInline())
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

RenderObject *Text::createRenderer(RenderArena *arena, RenderStyle *style)
{
#if ENABLE(SVG)
    if (parentNode()->isSVGElement())
        return new (arena) RenderSVGInlineText(this, m_data);
#endif // ENABLE(SVG)
    
    return new (arena) RenderText(this, m_data);
}

void Text::attach()
{
    createRendererIfNeeded();
    CharacterData::attach();
}

void Text::recalcStyle( StyleChange change )
{
    if (change != NoChange && parentNode())
        if (renderer())
            renderer()->setStyle(parentNode()->renderer()->style());
    if (changed() && renderer() && renderer()->isText())
        static_cast<RenderText*>(renderer())->setText(m_data);
    setChanged(NoStyleChange);
}

// DOM Section 1.1.1
bool Text::childTypeAllowed(NodeType)
{
    return false;
}

PassRefPtr<Text> Text::createNew(PassRefPtr<StringImpl> string)
{
    return new Text(document(), string);
}

String Text::toString() const
{
    // FIXME: substitute entity references as needed!
    return nodeValue();
}

PassRefPtr<Text> Text::createWithLengthLimit(Document* doc, const UChar*& text, unsigned& charsLeft, unsigned maxChars)
{
    if (charsLeft <= maxChars) {
        String string = String(text, charsLeft);
        charsLeft = 0;
        return new Text(doc, string);
    }
    
    unsigned end = std::min(charsLeft, maxChars);
    
    // check we are not on an unbreakable boundary
    TextBreakIterator* it = characterBreakIterator(text, charsLeft);
    if (end < charsLeft && !isTextBreak(it, end))
        end = textBreakPreceding(it, end);
        
    // maxChars of unbreakable characters could lead to infinite loop
    if (end == 0)
        end = charsLeft;
    
    String nodeText = String(text, end);
    charsLeft -= end;
    text += end;
        
    return new Text(doc, nodeText);
}

#ifndef NDEBUG
void Text::formatForDebugger(char *buffer, unsigned length) const
{
    String result;
    String s;
    
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
          
    strncpy(buffer, result.deprecatedString().latin1(), length - 1);
}
#endif

} // namespace WebCore
