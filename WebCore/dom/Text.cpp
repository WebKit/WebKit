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

#include "CString.h"
#include "ExceptionCode.h"
#include "RenderText.h"
#include "TextBreakIterator.h"

#if ENABLE(SVG)
#include "RenderSVGInlineText.h"
#include "SVGNames.h"
#endif

#if ENABLE(WML)
#include "WMLDocument.h"
#include "WMLVariables.h"
#endif

namespace WebCore {

// DOM Section 1.1.1

Text::Text(Document* document, const String& text)
    : CharacterData(document, text, true)
{
}

Text::Text(Document* document)
    : CharacterData(document, true)
{
}

Text::~Text()
{
}

PassRefPtr<Text> Text::splitText(unsigned offset, ExceptionCode& ec)
{
    ec = 0;

    // INDEX_SIZE_ERR: Raised if the specified offset is negative or greater than
    // the number of 16-bit units in data.
    if (offset > m_data->length()) {
        ec = INDEX_SIZE_ERR;
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

    if (parentNode())
        document()->textNodeSplit(this);

    if (renderer())
        toRenderText(renderer())->setText(m_data);

    return newText.release();
}

static const Text* earliestLogicallyAdjacentTextNode(const Text* t)
{
    const Node* n = t;
    while ((n = n->previousSibling())) {
        Node::NodeType type = n->nodeType();
        if (type == Node::TEXT_NODE || type == Node::CDATA_SECTION_NODE) {
            t = static_cast<const Text*>(n);
            continue;
        }

        // We would need to visit EntityReference child text nodes if they existed
        ASSERT(type != Node::ENTITY_REFERENCE_NODE || !n->hasChildNodes());
        break;
    }
    return t;
}

static const Text* latestLogicallyAdjacentTextNode(const Text* t)
{
    const Node* n = t;
    while ((n = n->nextSibling())) {
        Node::NodeType type = n->nodeType();
        if (type == Node::TEXT_NODE || type == Node::CDATA_SECTION_NODE) {
            t = static_cast<const Text*>(n);
            continue;
        }

        // We would need to visit EntityReference child text nodes if they existed
        ASSERT(type != Node::ENTITY_REFERENCE_NODE || !n->hasChildNodes());
        break;
    }
    return t;
}

String Text::wholeText() const
{
    const Text* startText = earliestLogicallyAdjacentTextNode(this);
    const Text* endText = latestLogicallyAdjacentTextNode(this);

    Node* onePastEndText = endText->nextSibling();
    unsigned resultLength = 0;
    for (const Node* n = startText; n != onePastEndText; n = n->nextSibling()) {
        if (!n->isTextNode())
            continue;
        const Text* t = static_cast<const Text*>(n);
        const String& data = t->data();
        resultLength += data.length();
    }
    UChar* resultData;
    String result = String::createUninitialized(resultLength, resultData);
    UChar* p = resultData;
    for (const Node* n = startText; n != onePastEndText; n = n->nextSibling()) {
        if (!n->isTextNode())
            continue;
        const Text* t = static_cast<const Text*>(n);
        const String& data = t->data();
        unsigned dataLength = data.length();
        memcpy(p, data.characters(), dataLength * sizeof(UChar));
        p += dataLength;
    }
    ASSERT(p == resultData + resultLength);

    return result;
}

PassRefPtr<Text> Text::replaceWholeText(const String& newText, ExceptionCode&)
{
    // Remove all adjacent text nodes, and replace the contents of this one.

    // Protect startText and endText against mutation event handlers removing the last ref
    RefPtr<Text> startText = const_cast<Text*>(earliestLogicallyAdjacentTextNode(this));
    RefPtr<Text> endText = const_cast<Text*>(latestLogicallyAdjacentTextNode(this));

    RefPtr<Text> protectedThis(this); // Mutation event handlers could cause our last ref to go away
    Node* parent = parentNode(); // Protect against mutation handlers moving this node during traversal
    ExceptionCode ignored = 0;
    for (RefPtr<Node> n = startText; n && n != this && n->isTextNode() && n->parentNode() == parent;) {
        RefPtr<Node> nodeToRemove(n.release());
        n = nodeToRemove->nextSibling();
        parent->removeChild(nodeToRemove.get(), ignored);
    }

    if (this != endText) {
        Node* onePastEndText = endText->nextSibling();
        for (RefPtr<Node> n = nextSibling(); n && n != onePastEndText && n->isTextNode() && n->parentNode() == parent;) {
            RefPtr<Node> nodeToRemove(n.release());
            n = nodeToRemove->nextSibling();
            parent->removeChild(nodeToRemove.get(), ignored);
        }
    }

    if (newText.isEmpty()) {
        if (parent && parentNode() == parent)
            parent->removeChild(this, ignored);
        return 0;
    }

    setData(newText, ignored);
    return protectedThis.release();
}

String Text::nodeName() const
{
    return textAtom.string();
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
        
    if (par->isRenderInline()) {
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

RenderObject *Text::createRenderer(RenderArena* arena, RenderStyle*)
{
#if ENABLE(SVG)
    if (parentNode()->isSVGElement()
#if ENABLE(SVG_FOREIGN_OBJECT)
        && !parentNode()->hasTagName(SVGNames::foreignObjectTag)
#endif
    )
        return new (arena) RenderSVGInlineText(this, m_data);
#endif
    
    return new (arena) RenderText(this, m_data);
}

void Text::attach()
{
    createRendererIfNeeded();
    CharacterData::attach();
}

void Text::recalcStyle(StyleChange change)
{
    if (change != NoChange && parentNode()) {
        if (renderer())
            renderer()->setStyle(parentNode()->renderer()->style());
    }
    if (needsStyleRecalc()) {
        if (renderer()) {
            if (renderer()->isText())
                toRenderText(renderer())->setText(m_data);
        } else {
            if (attached())
                detach();
            attach();
        }
    }
    setNeedsStyleRecalc(NoStyleChange);
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

PassRefPtr<Text> Text::createWithLengthLimit(Document* doc, const String& text, unsigned& charsLeft, unsigned maxChars)
{
    if (charsLeft == text.length() && charsLeft <= maxChars) {
        charsLeft = 0;
        return new Text(doc, text);
    }
    
    unsigned start = text.length() - charsLeft;
    unsigned end = start + std::min(charsLeft, maxChars);
    
    // check we are not on an unbreakable boundary
    TextBreakIterator* it = characterBreakIterator(text.characters(), text.length());
    if (end < text.length() && !isTextBreak(it, end))
        end = textBreakPreceding(it, end);
        
    // maxChars of unbreakable characters could lead to infinite loop
    if (end <= start)
        end = text.length();
    
    String nodeText = text.substring(start, end - start);
    charsLeft = text.length() - end;
        
    return new Text(doc, nodeText);
}

#if ENABLE(WML)
void Text::insertedIntoDocument()
{
    CharacterData::insertedIntoDocument();

    if (!parentNode()->isWMLElement() || !length())
        return;

    WMLPageState* pageState = wmlPageStateForDocument(document());
    if (!pageState->hasVariables())
        return;

    String text = data();
    if (!text.impl() || text.impl()->containsOnlyWhitespace())
        return;

    text = substituteVariableReferences(text, document());

    ExceptionCode ec;
    setData(text, ec);
}
#endif

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
          
    strncpy(buffer, result.utf8().data(), length - 1);
}
#endif

} // namespace WebCore
