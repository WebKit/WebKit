/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "dom_position.h"

#include "helper.h"
#include "htmltags.h"
#include "qstring.h"
#include "rendering/render_block.h"
#include "rendering/render_line.h"
#include "rendering/render_object.h"
#include "rendering/render_style.h"
#include "rendering/render_text.h"
#include "xml/dom_positioniterator.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_nodeimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#else
#define ASSERT(assertion) assert(assertion)
#define LOG(channel, formatAndArgs...) ((void)0)
#endif

using khtml::InlineBox;
using khtml::InlineFlowBox;
using khtml::InlineTextBox;
using khtml::RenderBlock;
using khtml::RenderObject;
using khtml::RenderText;
using khtml::RootInlineBox;

namespace DOM {

static bool renderersOnDifferentLine(RenderObject *r1, long o1, RenderObject *r2, long o2)
{
    InlineBox *b1 = r1 ? r1->inlineBox(o1) : 0;
    InlineBox *b2 = r2 ? r2->inlineBox(o2) : 0;
    return (b1 && b2 && b1->root() != b2->root());
}

static NodeImpl *nextRenderedEditable(NodeImpl *node)
{
    while (1) {
        node = node->nextEditable();
        if (!node)
            return 0;
        if (!node->renderer())
            continue;
        if (node->renderer()->inlineBox(0))
            return node;
    }
    return 0;
}

static NodeImpl *previousRenderedEditable(NodeImpl *node)
{
    while (1) {
        node = node->previousEditable();
        if (!node)
            return 0;
        if (!node->renderer())
            continue;
        if (node->renderer()->inlineBox(0))
            return node;
    }
    return 0;
}


Position::Position(NodeImpl *node, long offset) 
    : m_node(0), m_offset(offset) 
{ 
    if (node) {
        m_node = node;
        m_node->ref();
    }
};

Position::Position(const Position &o)
    : m_node(0), m_offset(o.offset()) 
{
    if (o.node()) {
        m_node = o.node();
        m_node->ref();
    }
}

Position::~Position() {
    if (m_node) {
        m_node->deref();
    }
}

Position &Position::operator=(const Position &o)
{
    if (m_node) {
        m_node->deref();
    }
    m_node = o.node();
    if (m_node) {
        m_node->ref();
    }

    m_offset = o.offset();
    
    return *this;
}

ElementImpl *Position::element() const
{
    if (isEmpty())
        return 0;
        
    NodeImpl *n = node();
    for (; n && !n->isElementNode(); n = n->parentNode()); //loop
        
    return static_cast<ElementImpl *>(n);
}

long Position::renderedOffset() const
{
    if (!node()->isTextNode())
        return offset();
   
    if (!node()->renderer())
        return offset();
                    
    long result = 0;
    RenderText *textRenderer = static_cast<RenderText *>(node()->renderer());
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        long start = box->m_start;
        long end = box->m_start + box->m_len;
        if (offset() < start)
            return result;
        if (offset() <= end) {
            result += offset() - start;
            return result;
        }
        result += box->m_len;
    }
    return result;
}

Position Position::equivalentLeafPosition() const
{
    if (isEmpty())
        return Position();

    if (!node()->renderer() || !node()->renderer()->firstChild())
        return *this;
    
    NodeImpl *n = node();
    int count = 0;
    while (1) {
        n = n->nextLeafNode();
        if (!n || !n->inSameContainingBlockFlowElement(node()))
            return *this;
        if (count + n->maxOffset() >= offset()) {
            count = offset() - count;
            break;
        }
        count += n->maxOffset();
    }
    return Position(n, count);
}

Position Position::previousRenderedEditablePosition() const
{
    if (isEmpty())
        return Position();

    if (node()->isContentEditable() && node()->hasChildNodes() == false && inRenderedContent())
        return *this;

    NodeImpl *n = node();
    while (1) {
        n = n->previousEditable();
        if (!n)
            return Position();
        if (n->renderer() && n->renderer()->style()->visibility() == khtml::VISIBLE)
            break;
    }
    
    return Position(n, 0);
}

Position Position::nextRenderedEditablePosition() const
{
    if (isEmpty())
        return Position();

    if (node()->isContentEditable() && node()->hasChildNodes() == false && inRenderedContent())
        return *this;

    NodeImpl *n = node();
    while (1) {
        n = n->nextEditable();
        if (!n)
            return Position();
        if (n->renderer() && n->renderer()->style()->visibility() == khtml::VISIBLE)
            break;
    }
    
    return Position(n, 0);
}

Position Position::previousCharacterPosition() const
{
    if (isEmpty())
        return Position();

    NodeImpl *fromRootEditableElement = node()->rootEditableElement();
    PositionIterator it(*this);

    bool atStartOfLine = isFirstRenderedPositionOnLine();
    
    while (!it.atStart()) {
        Position pos = it.previous();

        if (pos.node()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atStartOfLine) {
            if (pos.inRenderedContent())
                return pos;
        }
        else if (rendersInDifferentPosition(pos))
            return pos;
    }
    
    return *this;
}

Position Position::nextCharacterPosition() const
{
    if (isEmpty())
        return Position();

    NodeImpl *fromRootEditableElement = node()->rootEditableElement();
    PositionIterator it(*this);

    bool atEndOfLine = isLastRenderedPositionOnLine();
    
    while (!it.atEnd()) {
        Position pos = it.next();

        if (pos.node()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atEndOfLine) {
            if (pos.inRenderedContent())
                return pos;
        }
        else if (rendersInDifferentPosition(pos))
            return pos;
    }
    
    return *this;
}

Position Position::previousWordPosition() const
{
    if (isEmpty())
        return Position();

    Position pos = *this;
    for (PositionIterator it(*this); !it.atStart(); it.previous()) {
        if (it.current().node()->nodeType() == Node::TEXT_NODE || it.current().node()->nodeType() == Node::CDATA_SECTION_NODE) {
            DOMString t = it.current().node()->nodeValue();
            QChar *chars = t.unicode();
            uint len = t.length();
            int start, end;
            khtml::findWordBoundary(chars, len, it.current().offset(), &start, &end);
            pos = Position(it.current().node(), start);
        }
        else {
            pos = Position(it.current().node(), it.current().node()->caretMinOffset());
        }
        if (pos != *this)
            return pos;
        it.setPosition(pos);
    }
    
    return *this;
}

Position Position::nextWordPosition() const
{
    if (isEmpty())
        return Position();

    Position pos = *this;
    for (PositionIterator it(*this); !it.atEnd(); it.next()) {
        if (it.current().node()->nodeType() == Node::TEXT_NODE || it.current().node()->nodeType() == Node::CDATA_SECTION_NODE) {
            DOMString t = it.current().node()->nodeValue();
            QChar *chars = t.unicode();
            uint len = t.length();
            int start, end;
            khtml::findWordBoundary(chars, len, it.current().offset(), &start, &end);
            pos = Position(it.current().node(), end);
        }
        else {
            pos = Position(it.current().node(), it.current().node()->caretMaxOffset());
        }
        if (pos != *this)
            return pos;
        it.setPosition(pos);
    }
    
    return *this;
}

Position Position::previousLinePosition(int x) const
{
    if (!node())
        return Position();

    if (!node()->renderer())
        return *this;

    InlineBox *box = node()->renderer()->inlineBox(offset());
    if (!box)
        return *this;

    RenderBlock *containingBlock = 0;
    RootInlineBox *root = box->root()->prevRootBox();
    if (root) {
        containingBlock = node()->renderer()->containingBlock();
    }
    else {
        // This containing editable block does not have a previous line.
        // Need to move back to previous containing editable block in this root editable
        // block and find the last root line box in that block.
        NodeImpl *startBlock = node()->enclosingBlockFlowElement();
        NodeImpl *n = node()->previousEditable();
        while (n && startBlock == n->enclosingBlockFlowElement())
            n = n->previousEditable();
        if (n) {
            while (n && !Position(n, n->caretMaxOffset()).inRenderedContent())
                n = n->previousEditable();
            if (n && n->inSameRootEditableElement(node())) {
                ASSERT(n->renderer());
                box = n->renderer()->inlineBox(n->caretMaxOffset());
                ASSERT(box);
                // previous root line box found
                root = box->root();
                containingBlock = n->renderer()->containingBlock();
            }
        }
    }
    
    if (root) {
        int absx, absy;
        containingBlock->absolutePosition(absx, absy);
        RenderObject *renderer = root->closestLeafChildForXPos(x, absx)->object();
        return renderer->positionForCoordinates(x, absy + root->topOverflow());
    }
    
    return *this;
}

Position Position::nextLinePosition(int x) const
{
    if (!node())
        return Position();

    if (!node()->renderer())
        return *this;

    InlineBox *box = node()->renderer()->inlineBox(offset());
    if (!box)
        return *this;

    RenderBlock *containingBlock = 0;
    RootInlineBox *root = box->root()->nextRootBox();
    if (root) {
        containingBlock = node()->renderer()->containingBlock();
    }
    else {
        // This containing editable block does not have a next line.
        // Need to move forward to next containing editable block in this root editable
        // block and find the first root line box in that block.
        NodeImpl *startBlock = node()->enclosingBlockFlowElement();
        NodeImpl *n = node()->nextEditable();
        while (n && startBlock == n->enclosingBlockFlowElement())
            n = n->nextEditable();
        if (n) {
            while (n && !Position(n, n->caretMinOffset()).inRenderedContent())
                n = n->nextEditable();
            if (n && n->inSameRootEditableElement(node())) {
                ASSERT(n->renderer());
                box = n->renderer()->inlineBox(n->caretMinOffset());
                ASSERT(box);
                // previous root line box found
                root = box->root();
                containingBlock = n->renderer()->containingBlock();
            }
        }
    }
    
    if (root) {
        int absx, absy;
        containingBlock->absolutePosition(absx, absy);
        RenderObject *renderer = root->closestLeafChildForXPos(x, absx)->object();
        return renderer->positionForCoordinates(x, absy + root->topOverflow());
    }

    return *this;
}

Position Position::equivalentUpstreamPosition() const
{
    if (!node())
        return Position();

    NodeImpl *block = node()->enclosingBlockFlowElement();
    
    PositionIterator it(*this);            
    for (; !it.atStart(); it.previous()) {   
        NodeImpl *currentBlock = it.current().node()->enclosingBlockFlowElement();
        if (block != currentBlock)
            return it.next();

        RenderObject *renderer = it.current().node()->renderer();
        if (!renderer)
            continue;

        if (renderer->style()->visibility() != khtml::VISIBLE)
            continue;

        if (renderer->isBlockFlow() || renderer->isReplaced() || renderer->isBR()) {
            if (it.current().offset() >= renderer->caretMaxOffset())
                return Position(it.current().node(), renderer->caretMaxOffset());
            else
                continue;
        }

        if (renderer->isText() && static_cast<RenderText *>(renderer)->firstTextBox()) {
            if (it.current().node() != node())
                return Position(it.current().node(), renderer->caretMaxOffset());

            if (it.current().offset() < 0)
                continue;
            uint textOffset = it.current().offset();

            RenderText *textRenderer = static_cast<RenderText *>(renderer);
            for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset > box->start() && textOffset <= box->start() + box->len())
                    return it.current();
            }
        }
    }
    
    return it.current();
}

Position Position::equivalentDownstreamPosition() const
{
    if (!node())
        return Position();

    NodeImpl *block = node()->enclosingBlockFlowElement();
    
    PositionIterator it(*this);            
    for (; !it.atEnd(); it.next()) {   
        NodeImpl *currentBlock = it.current().node()->enclosingBlockFlowElement();
        if (block != currentBlock)
            return it.previous();

        RenderObject *renderer = it.current().node()->renderer();
        if (!renderer)
            continue;

        if (renderer->style()->visibility() != khtml::VISIBLE)
            continue;

        if (renderer->isBlockFlow() || renderer->isReplaced() || renderer->isBR()) {
            if (it.current().offset() <= renderer->caretMinOffset())
                return Position(it.current().node(), renderer->caretMinOffset());
            else
                continue;
        }

        if (renderer->isText() && static_cast<RenderText *>(renderer)->firstTextBox()) {
            if (it.current().node() != node())
                return Position(it.current().node(), renderer->caretMinOffset());

            if (it.current().offset() < 0)
                continue;
            uint textOffset = it.current().offset();

            RenderText *textRenderer = static_cast<RenderText *>(renderer);
            for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset >= box->start() && textOffset <= box->end())
                    return it.current();
            }
        }
    }
    
    return it.current();
}

Position Position::equivalentRangeCompliantPosition() const
{
    if (isEmpty())
        return *this;

    if (!node()->parentNode())
        return *this;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return *this;
        
    if (!renderer->isReplaced() && !renderer->isBR())
        return *this;
    
    int o = 0;
    const NodeImpl *n = node();
    while ((n = n->previousSibling()))
        o++;
    
    return Position(node()->parentNode(), o + offset());
}

Position Position::equivalentShallowPosition() const
{
    if (isEmpty())
        return *this;

    Position pos(*this);
    while (pos.offset() == pos.node()->caretMinOffset() && pos.node()->parentNode() && pos.node() == pos.node()->parentNode()->firstChild())
        pos = Position(pos.node()->parentNode(), 0);
    return pos;
}

bool Position::atStartOfContainingEditableBlock() const
{
    return renderedOffset() == 0 && inFirstEditableInContainingEditableBlock();
}

bool Position::atStartOfRootEditableElement() const
{
    return renderedOffset() == 0 && inFirstEditableInRootEditableElement();
}

bool Position::inRenderedContent() const
{
    if (isEmpty())
        return false;
        
    RenderObject *renderer = node()->renderer();
    if (!renderer || !renderer->isEditable())
        return false;
    
    if (renderer->style()->visibility() != khtml::VISIBLE)
        return false;

    if (renderer->isBR() && static_cast<RenderText *>(renderer)->firstTextBox()) {
        return offset() == 0;
    }
    else if (renderer->isText()) {
        RenderText *textRenderer = static_cast<RenderText *>(renderer);
        for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
            if (offset() >= box->m_start && offset() <= box->m_start + box->m_len) {
                return true;
            }
            else if (offset() < box->m_start) {
                // The offset we're looking for is before this node
                // this means the offset must be in content that is
                // not rendered. Return false.
                return false;
            }
        }
    }
    else if (offset() >= renderer->caretMinOffset() && offset() <= renderer->caretMaxOffset()) {
        // don't return containing editable blocks unless they are empty
        if (node()->enclosingBlockFlowElement() == node() && node()->firstChild())
            return false;
        return true;
    }
    
    return false;
}

bool Position::inRenderedText() const
{
    if (!node()->isTextNode())
        return false;
        
    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderText *textRenderer = static_cast<RenderText *>(renderer);
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        if (offset() < box->m_start) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not rendered. Return false.
            return false;
        }
        if (offset() >= box->m_start && offset() <= box->m_start + box->m_len)
            return true;
    }
    
    return false;
}

bool Position::rendersOnSameLine(const Position &pos) const
{
    if (isEmpty() || pos.isEmpty())
        return false;

    if (node() == pos.node() && offset() == pos.offset())
        return true;

    if (node()->enclosingBlockFlowElement() != pos.node()->enclosingBlockFlowElement())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderObject *posRenderer = pos.node()->renderer();
    if (!posRenderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE ||
        posRenderer->style()->visibility() != khtml::VISIBLE)
        return false;

    return renderersOnDifferentLine(renderer, offset(), posRenderer, pos.offset());
}

bool Position::rendersInDifferentPosition(const Position &pos) const
{
    if (isEmpty() || pos.isEmpty())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderObject *posRenderer = pos.node()->renderer();
    if (!posRenderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE ||
        posRenderer->style()->visibility() != khtml::VISIBLE)
        return false;
    
    if (node() == pos.node()) {
        if (node()->id() == ID_BR)
            return false;

        if (offset() == pos.offset())
            return false;
            
        if (!node()->isTextNode() && !pos.node()->isTextNode()) {
            if (offset() != pos.offset())
                return true;
        }
    }
    
    if (node()->id() == ID_BR && pos.inRenderedContent())
        return true;
                
    if (pos.node()->id() == ID_BR && inRenderedContent())
        return true;
                
    if (node()->enclosingBlockFlowElement() != pos.node()->enclosingBlockFlowElement())
        return true;

    if (node()->isTextNode() && !inRenderedText())
        return false;

    if (pos.node()->isTextNode() && !pos.inRenderedText())
        return false;

    long thisRenderedOffset = renderedOffset();
    long posRenderedOffset = pos.renderedOffset();

    if (renderer == posRenderer && thisRenderedOffset == posRenderedOffset)
        return false;

    LOG(Editing, "onDifferentLine:        %s\n", renderersOnDifferentLine(renderer, offset(), posRenderer, pos.offset()) ? "YES" : "NO");
    LOG(Editing, "renderer:               %p [%p]\n", renderer, renderer ? renderer->inlineBox(offset()) : 0);
    LOG(Editing, "thisRenderedOffset:         %d\n", thisRenderedOffset);
    LOG(Editing, "posRenderer:            %p [%p]\n", posRenderer, posRenderer ? posRenderer->inlineBox(offset()) : 0);
    LOG(Editing, "posRenderedOffset:      %d\n", posRenderedOffset);
    LOG(Editing, "node min/max:           %d:%d\n", node()->caretMinOffset(), node()->caretMaxRenderedOffset());
    LOG(Editing, "pos node min/max:       %d:%d\n", pos.node()->caretMinOffset(), pos.node()->caretMaxRenderedOffset());
    LOG(Editing, "----------------------------------------------------------------------\n");

    InlineBox *b1 = renderer ? renderer->inlineBox(offset()) : 0;
    InlineBox *b2 = posRenderer ? posRenderer->inlineBox(pos.offset()) : 0;

    if (!b1 || !b2) {
        return false;
    }

    if (b1->root() != b2->root()) {
        return true;
    }

    if (nextRenderedEditable(node()) == pos.node() && 
        thisRenderedOffset == (long)node()->caretMaxRenderedOffset() && posRenderedOffset == 0) {
        return false;
    }
    
    if (previousRenderedEditable(node()) == pos.node() && 
        thisRenderedOffset == 0 && posRenderedOffset == (long)pos.node()->caretMaxRenderedOffset()) {
        return false;
    }

    return true;
}

bool Position::isFirstRenderedPositionOnLine() const
{
    if (isEmpty())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE)
        return false;
    
    Position pos(node(), offset());
    PositionIterator it(pos);
    while (!it.atStart()) {
        it.previous();
        if (it.current().inRenderedContent())
            return renderersOnDifferentLine(renderer, offset(), it.current().node()->renderer(), it.current().offset());
    }
    
    return true;
}

bool Position::isLastRenderedPositionOnLine() const
{
    if (isEmpty())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE)
        return false;
    
    if (node()->id() == ID_BR)
        return true;
    
    Position pos(node(), offset());
    PositionIterator it(pos);
    while (!it.atEnd()) {
        it.next();
        if (it.current().inRenderedContent())
            return renderersOnDifferentLine(renderer, offset(), it.current().node()->renderer(), it.current().offset());
    }
    
    return true;
}

bool Position::isLastRenderedPositionInEditableBlock() const
{
    if (isEmpty())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;

    if (renderer->style()->visibility() != khtml::VISIBLE)
        return false;

    if (renderedOffset() != (long)node()->caretMaxRenderedOffset())
        return false;

    Position pos(node(), offset());
    PositionIterator it(pos);
    while (!it.atEnd()) {
        it.next();
        if (!it.current().node()->inSameContainingBlockFlowElement(node()))
            return true;
        if (it.current().inRenderedContent())
            return false;
    }
    return true;
}

bool Position::inFirstEditableInRootEditableElement() const
{
    if (isEmpty() || !inRenderedContent())
        return false;

    PositionIterator it(*this);
    while (!it.atStart()) {
        if (it.previous().inRenderedContent())
            return false;
    }

    return true;
}

bool Position::inLastEditableInRootEditableElement() const
{
    if (isEmpty() || !inRenderedContent())
        return false;

    PositionIterator it(*this);
    while (!it.atEnd()) {
        if (it.next().inRenderedContent())
            return false;
    }

    return true;
}

bool Position::inFirstEditableInContainingEditableBlock() const
{
    if (isEmpty() || !inRenderedContent())
        return false;
    
    NodeImpl *block = node()->enclosingBlockFlowElement();

    PositionIterator it(*this);
    while (!it.atStart()) {
        it.previous();
        if (!it.current().inRenderedContent())
            continue;
        return block != it.current().node()->enclosingBlockFlowElement();
    }

    return true;
}

bool Position::inLastEditableInContainingEditableBlock() const
{
    if (isEmpty() || !inRenderedContent())
        return false;
    
    NodeImpl *block = node()->enclosingBlockFlowElement();

    PositionIterator it(*this);
    while (!it.atEnd()) {
        it.next();
        if (!it.current().inRenderedContent())
            continue;
        return block != it.current().node()->enclosingBlockFlowElement();
    }

    return true;
}

void Position::debugPosition(const char *msg) const
{
    if (isEmpty())
        fprintf(stderr, "Position [%s]: empty\n");
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, getTagName(node()->id()).string().latin1(), node(), offset());
}

} // namespace DOM