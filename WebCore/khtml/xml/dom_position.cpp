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

#include <qstring.h>

#include "css_computedstyle.h"
#include "css_valueimpl.h"
#include "dom_elementimpl.h"
#include "dom_nodeimpl.h"
#include "dom_positioniterator.h"
#include "dom2_range.h"
#include "dom2_viewsimpl.h"
#include "helper.h"
#include "htmltags.h"
#include "text_affinity.h"
#include "visible_position.h"
#include "rendering/render_block.h"
#include "rendering/render_flow.h"
#include "rendering/render_line.h"
#include "rendering/render_style.h"
#include "rendering/render_text.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#else
#define ASSERT(assertion) assert(assertion)
#define LOG(channel, formatAndArgs...) ((void)0)
#endif

using khtml::EAffinity;
using khtml::InlineBox;
using khtml::InlineTextBox;
using khtml::RenderBlock;
using khtml::RenderFlow;
using khtml::RenderObject;
using khtml::RenderText;
using khtml::RootInlineBox;
using khtml::VISIBLE;
using khtml::VisiblePosition;

namespace DOM {

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
    : m_node(node), m_offset(offset) 
{ 
    if (node) {
        node->ref();
    }
}

Position::Position(const Position &o)
    : m_node(o.m_node), m_offset(o.m_offset) 
{
    if (m_node) {
        m_node->ref();
    }
}

Position::~Position()
{
    if (m_node) {
        m_node->deref();
    }
}

Position &Position::operator=(const Position &o)
{
    if (m_node) {
        m_node->deref();
    }
    m_node = o.m_node;
    if (m_node) {
        m_node->ref();
    }

    m_offset = o.m_offset;
    
    return *this;
}

void Position::clear()
{
    if (m_node) {
        m_node->deref();
        m_node = 0;
    }
    m_offset = 0;
}

ElementImpl *Position::element() const
{
    NodeImpl *n;
    for (n = node(); n && !n->isElementNode(); n = n->parentNode())
        ; // empty loop body
    return static_cast<ElementImpl *>(n);
}

CSSComputedStyleDeclarationImpl *Position::computedStyle() const
{
    ElementImpl *elem = element();
    if (!elem)
        return 0;
    return new CSSComputedStyleDeclarationImpl(elem);
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

Position Position::previousCharacterPosition() const
{
    if (isNull())
        return Position();

    NodeImpl *fromRootEditableElement = node()->rootEditableElement();
    PositionIterator it(*this);

    bool atStartOfLine = isFirstVisiblePositionOnLine(VisiblePosition(*this));
    bool rendered = inRenderedContent();
    
    while (!it.atStart()) {
        Position pos = it.previous();

        if (pos.node()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atStartOfLine || !rendered) {
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
    if (isNull())
        return Position();

    NodeImpl *fromRootEditableElement = node()->rootEditableElement();
    PositionIterator it(*this);

    bool atEndOfLine = isLastVisiblePositionOnLine(VisiblePosition(*this));
    bool rendered = inRenderedContent();
    
    while (!it.atEnd()) {
        Position pos = it.next();

        if (pos.node()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atEndOfLine || !rendered) {
            if (pos.inRenderedContent())
                return pos;
        }
        else if (rendersInDifferentPosition(pos))
            return pos;
    }
    
    return *this;
}

Position Position::previousLinePosition(int x, EAffinity affinity) const
{
    if (!node())
        return Position();

    if (!node()->renderer())
        return *this;

    RenderBlock *containingBlock = 0;
    RootInlineBox *root = 0;
    InlineBox *box = node()->renderer()->inlineBox(offset(), affinity);
    if (box) {
        root = box->root()->prevRootBox();
        if (root)
            containingBlock = node()->renderer()->containingBlock();
    }

    if (!root) {
        // This containing editable block does not have a previous line.
        // Need to move back to previous containing editable block in this root editable
        // block and find the last root line box in that block.
        NodeImpl *startBlock = node()->enclosingBlockFlowElement();
        NodeImpl *n = node()->previousEditable();
        while (n && startBlock == n->enclosingBlockFlowElement())
            n = n->previousEditable();
        while (n) {
            if (!n->inSameRootEditableElement(node()))
                break;
            Position pos(n, n->caretMinOffset());
            if (pos.inRenderedContent()) {
                ASSERT(n->renderer());
                box = n->renderer()->inlineBox(n->caretMaxOffset());
                if (box) {
                    // previous root line box found
                    root = box->root();
                    containingBlock = n->renderer()->containingBlock();
                    break;
                }
                else {
                    return pos;
                }
            }
            n = n->previousEditable();
        }
    }
    
    if (root) {
        int absx, absy;
        containingBlock->absolutePosition(absx, absy);
        RenderObject *renderer = root->closestLeafChildForXPos(x, absx)->object();
        return renderer->positionForCoordinates(x, absy + root->topOverflow());
    }
    
    // Could not find a previous line. This means we must already be on the first line.
    // Move to the start of the content in this block, which effectively moves us
    // to the start of the line we're on.
    return Position(node()->rootEditableElement(), 0);
}

Position Position::nextLinePosition(int x, EAffinity affinity) const
{
    if (!node())
        return Position();

    if (!node()->renderer())
        return *this;

    RenderBlock *containingBlock = 0;
    RootInlineBox *root = 0;
    InlineBox *box = node()->renderer()->inlineBox(offset(), affinity);
    if (box) {
        root = box->root()->nextRootBox();
        if (root)
            containingBlock = node()->renderer()->containingBlock();
    }

    if (!root) {
        // This containing editable block does not have a next line.
        // Need to move forward to next containing editable block in this root editable
        // block and find the first root line box in that block.
        NodeImpl *startBlock = node()->enclosingBlockFlowElement();
        NodeImpl *n = node()->nextEditable();
        while (n && startBlock == n->enclosingBlockFlowElement())
            n = n->nextEditable();
        while (n) {
            if (!n->inSameRootEditableElement(node()))
                break;
            Position pos(n, n->caretMinOffset());
            if (pos.inRenderedContent()) {
                ASSERT(n->renderer());
                box = n->renderer()->inlineBox(n->caretMinOffset());
                if (box) {
                    // next root line box found
                    root = box->root();
                    containingBlock = n->renderer()->containingBlock();
                    break;
                }
                else {
                    return pos;
                }
            }
            n = n->nextEditable();
        }
    }
    
    if (root) {
        int absx, absy;
        containingBlock->absolutePosition(absx, absy);
        RenderObject *renderer = root->closestLeafChildForXPos(x, absx)->object();
        return renderer->positionForCoordinates(x, absy + root->topOverflow());
    }    

    // Could not find a next line. This means we must already be on the last line.
    // Move to the end of the content in this block, which effectively moves us
    // to the end of the line we're on.
    ElementImpl *rootElement = node()->rootEditableElement();
    return Position(rootElement, rootElement ? rootElement->childNodeCount() : 0);
}

Position Position::upstream(EStayInBlock stayInBlock) const
{
    Position start = equivalentDeepPosition();
    NodeImpl *startNode = start.node();
    if (!startNode)
        return Position();

    NodeImpl *block = startNode->enclosingBlockFlowElement();
    
    PositionIterator it(start);
    for (; !it.atStart(); it.previous()) {
        NodeImpl *currentNode = it.current().node();

        if (stayInBlock) {
            NodeImpl *currentBlock = currentNode->enclosingBlockFlowElement();
            if (block != currentBlock)
                return it.next();
        }

        RenderObject *renderer = currentNode->renderer();
        if (!renderer)
            continue;

        if (renderer->style()->visibility() != VISIBLE)
            continue;

        if (renderer->isReplaced() || renderer->isBR()) {
            if (it.current().offset() >= renderer->caretMaxOffset())
                return Position(currentNode, renderer->caretMaxOffset());
            else
                continue;
        }

        if (renderer->isText() && static_cast<RenderText *>(renderer)->firstTextBox()) {
            if (currentNode != startNode)
                return Position(currentNode, renderer->caretMaxOffset());

            if (it.current().offset() < 0)
                continue;
            uint textOffset = it.current().offset();

            RenderText *textRenderer = static_cast<RenderText *>(renderer);
            for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset > box->start() && textOffset <= box->start() + box->len())
                    return it.current();
                else if (box != textRenderer->lastTextBox() && 
                         !box->nextOnLine() && 
                         textOffset == box->start() + box->len() + 1)
                    return it.current();
            }
        }
    }
    
    return it.current();
}

Position Position::downstream(EStayInBlock stayInBlock) const
{
    Position start = equivalentDeepPosition();
    NodeImpl *startNode = start.node();
    if (!startNode)
        return Position();

    NodeImpl *block = startNode->enclosingBlockFlowElement();
    
    PositionIterator it(start);            
    for (; !it.atEnd(); it.next()) {   
        NodeImpl *currentNode = it.current().node();

        if (stayInBlock) {
            NodeImpl *currentBlock = currentNode->enclosingBlockFlowElement();
            if (block != currentBlock)
                return it.previous();
        }

        RenderObject *renderer = currentNode->renderer();
        if (!renderer)
            continue;

        if (renderer->style()->visibility() != VISIBLE)
            continue;

        if (currentNode != startNode && renderer->isBlockFlow()) {
            if (it.current().offset() == 0) {
                // If no first child, or first visible child is a not a block, return; otherwise continue.
                if (!currentNode->firstChild())
                    return Position(currentNode, 0);
                for (NodeImpl *child = currentNode->firstChild(); child; child = child->nextSibling()) {
                    RenderObject *r = child->renderer();
                    if (r && r->style()->visibility() == VISIBLE) {
                         if (r->isBlockFlow())
                            break; // break causes continue code below to run.
                         else
                            return Position(child, 0);
                    }
                }
                continue;
            }
        }

        if (renderer->isReplaced() || renderer->isBR()) {
            if (it.current().offset() <= renderer->caretMinOffset())
                return Position(currentNode, renderer->caretMinOffset());
            else
                continue;
        }

        if (renderer->isText() && static_cast<RenderText *>(renderer)->firstTextBox()) {
            if (currentNode != node())
                return Position(currentNode, renderer->caretMinOffset());

            if (it.current().offset() < 0)
                continue;
            uint textOffset = it.current().offset();

            RenderText *textRenderer = static_cast<RenderText *>(renderer);
            for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset >= box->start() && textOffset <= box->end())
                    return it.current();
                else if (box != textRenderer->lastTextBox() && 
                         !box->nextOnLine() && 
                         textOffset == box->start() + box->len())
                    return it.current();
            }
        }
    }
    
    return it.current();
}

Position Position::equivalentRangeCompliantPosition() const
{
    if (isNull())
        return Position();

    // Make sure that 0 <= constrainedOffset <= num kids, otherwise using this Position
    // in DOM calls can result in exceptions.
    long maxOffset = node()->isTextNode() ? static_cast<TextImpl *>(node())->length(): node()->childNodeCount();
    long constrainedOffset = offset() <= 0 ? 0 : kMin(maxOffset, offset());

    if (!node()->parentNode())
        return Position(node(), constrainedOffset);

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return Position(node(), constrainedOffset);
        
    if (!renderer->isReplaced() && !renderer->isBR())
        return Position(node(), constrainedOffset);
    
    long o = offset();
    const NodeImpl *n = node();
    while ((n = n->previousSibling()))
        o++;
    
    // Make sure that 0 <= constrainedOffset <= num kids, as above.
    NodeImpl *parent = node()->parentNode();
    maxOffset = parent->isTextNode() ? static_cast<TextImpl *>(parent)->length(): parent->childNodeCount();
    constrainedOffset = o <= 0 ? 0 : kMin(maxOffset, o);
    return Position(parent, constrainedOffset);
}

Position Position::equivalentDeepPosition() const
{
    if (isNull() || node()->isAtomicNode())
        return *this;

    NodeImpl *child = 0;
    Position pos(*this);
    if (offset() >= (int)node()->childNodeCount()) {
        child = node()->lastChild();
        pos = Position(child, child->caretMaxOffset());
        ASSERT(child);
        while (!child->isAtomicNode() && pos.node()->hasChildNodes()) {
            child = pos.node()->lastChild();
            ASSERT(child);
            pos = Position(child, child->caretMaxOffset());
        }
    }
    else {
        child = node()->childNode(offset());
        ASSERT(child);
        pos = Position(child, 0);
        while (!child->isAtomicNode() && pos.node()->hasChildNodes()) {
            child = pos.node()->firstChild();
            ASSERT(child);
            pos = Position(child, 0);
        }
    }
    return pos;
}

bool Position::inRenderedContent() const
{
    if (isNull())
        return false;
        
    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    if (renderer->style()->visibility() != VISIBLE)
        return false;

    // FIXME: This check returns false for a <br> at the end of a line!
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
        // return true for replaced elements, for inline flows if they have a line box
        // and for blocks if they are empty
        if (renderer->isReplaced() ||
            (renderer->isInlineFlow() && static_cast<RenderFlow *>(renderer)->firstLineBox()) ||
            (renderer->isBlockFlow() && !renderer->firstChild() && renderer->height()))
            return true;
    }
    
    return false;
}

bool Position::inRenderedText() const
{
    if (isNull() || !node()->isTextNode())
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

bool Position::isRenderedCharacter() const
{
    if (isNull() || !node()->isTextNode())
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
        if (offset() >= box->m_start && offset() < box->m_start + box->m_len)
            return true;
    }
    
    return false;
}

bool Position::rendersInDifferentPosition(const Position &pos) const
{
    if (isNull() || pos.isNull())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderObject *posRenderer = pos.node()->renderer();
    if (!posRenderer)
        return false;

    if (renderer->style()->visibility() != VISIBLE ||
        posRenderer->style()->visibility() != VISIBLE)
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

static inline bool isWS(const QChar &c)
{
    const char nonBreakingSpace = 0xA0;
    return c.isSpace() && c != nonBreakingSpace;
}

Position Position::leadingWhitespacePosition() const
{
    if (isNull())
        return Position();
    
    if (upstream(StayInBlock).node()->id() == ID_BR)
        return Position();
    
    Position prev = previousCharacterPosition();
    if (prev != *this && prev.node()->inSameContainingBlockFlowElement(node()) && prev.node()->isTextNode()) {
        DOMString string = static_cast<TextImpl *>(prev.node())->data();
        if (isWS(string[prev.offset()]))
            return prev;
    }

    return Position();
}

Position Position::trailingWhitespacePosition() const
{
    if (isNull())
        return Position();

    if (node()->isTextNode()) {
        TextImpl *textNode = static_cast<TextImpl *>(node());
        if (offset() < (long)textNode->length()) {
            DOMString string = static_cast<TextImpl *>(node())->data();
            if (isWS(string[offset()]))
                return *this;
            return Position();
        }
    }

    if (downstream(StayInBlock).node()->id() == ID_BR)
        return Position();

    Position next = nextCharacterPosition();
    if (next != *this && next.node()->inSameContainingBlockFlowElement(node()) && next.node()->isTextNode()) {
        DOMString string = static_cast<TextImpl *>(next.node())->data();
        if (isWS(string[0]))
            return next;
    }

    return Position();
}

void Position::debugPosition(const char *msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, getTagName(node()->id()).string().latin1(), node(), offset());
}

#ifndef NDEBUG
#define FormatBufferSize 1024
void Position::formatForDebugger(char *buffer, unsigned length) const
{
    DOMString result;
    DOMString s;
    
    if (isNull()) {
        result = "<null>";
    }
    else {
        char s[FormatBufferSize];
        result += "offset ";
        result += QString::number(m_offset);
        result += " of ";
        m_node->formatForDebugger(s, FormatBufferSize);
        result += s;
    }
          
    strncpy(buffer, result.string().latin1(), length - 1);
}
#undef FormatBufferSize
#endif


Position startPosition(const Range &r)
{
    return Position(r.startContainer().handle(), r.startOffset());
}

Position endPosition(const Range &r)
{
    return Position(r.endContainer().handle(), r.endOffset());
}

} // namespace DOM
