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
#include "dom2_rangeimpl.h"
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
#include "editing/visible_text.h"

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
using khtml::isCollapsibleWhitespace;
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

Position Position::previousCharacterPosition(EAffinity affinity) const
{
    if (isNull())
        return Position();

    NodeImpl *fromRootEditableElement = node()->rootEditableElement();
    PositionIterator it(*this);

    bool atStartOfLine = isFirstVisiblePositionOnLine(VisiblePosition(*this, affinity));
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

Position Position::nextCharacterPosition(EAffinity affinity) const
{
    if (isNull())
        return Position();

    NodeImpl *fromRootEditableElement = node()->rootEditableElement();
    PositionIterator it(*this);

    bool atEndOfLine = isLastVisiblePositionOnLine(VisiblePosition(*this, affinity));
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

Position Position::upstream(EStayInBlock stayInBlock) const
{
    Position start = equivalentDeepPosition();
    NodeImpl *startNode = start.node();
    if (!startNode)
        return Position();

    NodeImpl *block = startNode->enclosingBlockFlowOrTableElement();
    Position lastVisible;
    
    PositionIterator it(start);
    for (; !it.atStart(); it.previous()) {
        NodeImpl *currentNode = it.current().node();

        if (stayInBlock) {
            NodeImpl *currentBlock = currentNode->enclosingBlockFlowOrTableElement();
            if (block != currentBlock)
                return it.next();
        }

        RenderObject *renderer = currentNode->renderer();
        if (!renderer)
            continue;

        if (renderer->style()->visibility() != VISIBLE)
            continue;

        lastVisible = it.current();

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
    
    return lastVisible.isNotNull() ? lastVisible : *this;
}

Position Position::downstream(EStayInBlock stayInBlock) const
{
    Position start = equivalentDeepPosition();
    NodeImpl *startNode = start.node();
    if (!startNode)
        return Position();

    NodeImpl *block = startNode->enclosingBlockFlowOrTableElement();
    Position lastVisible;
    
    PositionIterator it(start);            
    for (; !it.atEnd(); it.next()) {   
        NodeImpl *currentNode = it.current().node();

        if (stayInBlock) {
            NodeImpl *currentBlock = currentNode->enclosingBlockFlowOrTableElement();
            if (block != currentBlock)
                return it.previous();
        }

        RenderObject *renderer = currentNode->renderer();
        if (!renderer)
            continue;

        if (renderer->style()->visibility() != VISIBLE)
            continue;

        lastVisible = it.current();

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
            if (currentNode != start.node())
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
    
    return lastVisible.isNotNull() ? lastVisible : *this;
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

Position Position::leadingWhitespacePosition(EAffinity affinity, bool considerNonCollapsibleWhitespace) const
{
    if (isNull())
        return Position();
    
    if (upstream(StayInBlock).node()->id() == ID_BR)
        return Position();

    Position prev = previousCharacterPosition(affinity);
    if (prev != *this && prev.node()->inSameContainingBlockFlowElement(node()) && prev.node()->isTextNode()) {
        DOMString string = static_cast<TextImpl *>(prev.node())->data();
        const QChar &c = string[prev.offset()];
        if (considerNonCollapsibleWhitespace ? (c.isSpace() || c.unicode() == 0xa0) : isCollapsibleWhitespace(c))
            return prev;
    }

    return Position();
}

Position Position::trailingWhitespacePosition(EAffinity affinity, bool considerNonCollapsibleWhitespace) const
{
    if (isNull())
        return Position();

    if (node()->isTextNode()) {
        TextImpl *textNode = static_cast<TextImpl *>(node());
        if (offset() < (long)textNode->length()) {
            DOMString string = static_cast<TextImpl *>(node())->data();
            const QChar &c = string[offset()];
            if (considerNonCollapsibleWhitespace ? (c.isSpace() || c.unicode() == 0xa0) : isCollapsibleWhitespace(c))
                return *this;
            return Position();
        }
    }

    if (downstream(StayInBlock).node()->id() == ID_BR)
        return Position();

    Position next = nextCharacterPosition(affinity);
    if (next != *this && next.node()->inSameContainingBlockFlowElement(node()) && next.node()->isTextNode()) {
        DOMString string = static_cast<TextImpl *>(next.node())->data();
        const QChar &c = string[0];
        if (considerNonCollapsibleWhitespace ? (c.isSpace() || c.unicode() == 0xa0) : isCollapsibleWhitespace(c))
            return next;
    }

    return Position();
}

void Position::debugPosition(const char *msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %ld\n", msg, node()->nodeName().string().latin1(), node(), offset());
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
    if (r.isNull() || r.isDetached())
        return Position();
    return Position(r.startContainer().handle(), r.startOffset());
}

Position startPosition(const RangeImpl *r)
{
    if (!r || r->isDetached())
        return Position();
    int exceptionCode;
    return Position(r->startContainer(exceptionCode), r->startOffset(exceptionCode));
}

Position endPosition(const Range &r)
{
    if (r.isNull() || r.isDetached())
        return Position();
    return Position(r.endContainer().handle(), r.endOffset());
}

Position endPosition(const RangeImpl *r)
{
    if (!r || r->isDetached())
        return Position();
    int exceptionCode;
    return Position(r->endContainer(exceptionCode), r->endOffset(exceptionCode));
}

} // namespace DOM
