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

#include "visible_position.h"

#include "misc/htmltags.h"
#include "rendering/render_line.h"
#include "rendering/render_object.h"
#include "rendering/render_text.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#else
#define ASSERT(assertion) assert(assertion)
#define LOG(channel, formatAndArgs...) ((void)0)
#endif

using DOM::CharacterDataImpl;
using DOM::offsetInCharacters;
using DOM::Position;
using DOM::Range;
using DOM::RangeImpl;
using DOM::StayInBlock;
using DOM::TextImpl;

namespace khtml {

VisiblePosition::VisiblePosition(NodeImpl *node, long offset, EAffinity affinity)
{
    Position pos = Position(node, offset);

    // A first step toward eliminating the affinity parameter.
    // For <br> 0, it's important not to move past the <br>.
    if (node && node->id() == ID_BR && offset == 0)
        affinity = UPSTREAM;

    switch (affinity) {
        case UPSTREAM:
            initUpstream(pos);
            break;
        case DOWNSTREAM:
            initDownstream(pos);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

VisiblePosition::VisiblePosition(const Position &pos, EAffinity affinity)
{
    // A first step toward eliminating the affinity parameter.
    // For <br> 0, it's important not to move past the <br>.
    if (pos.node() && pos.node()->id() == ID_BR && pos.offset() == 0)
        affinity = UPSTREAM;

    switch (affinity) {
        case UPSTREAM:
            initUpstream(pos);
            break;
        case DOWNSTREAM:
            initDownstream(pos);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

void VisiblePosition::initUpstream(const Position &pos)
{
    Position deepPos = deepEquivalent(pos);

    if (isCandidate(deepPos)) {
        m_deepPosition = deepPos;
        Position next = nextVisiblePosition(deepPos);
        if (next.isNotNull()) {
            Position previous = previousVisiblePosition(next);
            if (previous.isNotNull())
                m_deepPosition = previous;
        }
    }
    else {
        Position previous = previousVisiblePosition(deepPos);
        if (previous.isNotNull()) {
            m_deepPosition = previous;
        }
        else {
            Position next = nextVisiblePosition(deepPos);
            if (next.isNotNull())
                m_deepPosition = next;
        }
    }
}

void VisiblePosition::initDownstream(const Position &pos)
{
    Position deepPos = deepEquivalent(pos);

    if (isCandidate(deepPos)) {
        m_deepPosition = deepPos;
        Position previous = previousVisiblePosition(deepPos);
        if (previous.isNotNull()) {
            Position next = nextVisiblePosition(previous);
            if (next.isNotNull())
                m_deepPosition = next;
        }
    }
    else {
        Position next = nextVisiblePosition(deepPos);
        if (next.isNotNull()) {
            m_deepPosition = next;
        }
        else {
            Position previous = previousVisiblePosition(deepPos);
            if (previous.isNotNull())
                m_deepPosition = previous;
        }
    }
}

bool VisiblePosition::isLastInBlock() const
{
    if (isNull())
        return false;
        
    VisiblePosition n = next();
    return n.isNull() || (n.deepEquivalent().node()->enclosingBlockFlowElement() != m_deepPosition.node()->enclosingBlockFlowElement());
}

VisiblePosition VisiblePosition::next() const
{
    VisiblePosition result;
    result.m_deepPosition = nextVisiblePosition(m_deepPosition);
    return result;
}

VisiblePosition VisiblePosition::previous() const
{
    VisiblePosition result;
    result.m_deepPosition = previousVisiblePosition(m_deepPosition);
    return result;
}

Position VisiblePosition::previousVisiblePosition(const Position &pos)
{
    if (pos.isNull() || atStart(pos))
        return Position();

    Position test = deepEquivalent(pos);
    Position downstreamTest = test.downstream(StayInBlock);
    bool acceptAnyVisiblePosition = !isCandidate(test);

    Position current = test;
    while (!atStart(current)) {
        current = previousPosition(current);
        if (isCandidate(current) && (acceptAnyVisiblePosition || (downstreamTest != current.downstream(StayInBlock)))) {
            return current;
        }
    }
    
    return Position();
}

Position VisiblePosition::nextVisiblePosition(const Position &pos)
{
    if (pos.isNull() || atEnd(pos))
        return Position();

    Position test = deepEquivalent(pos);
    bool acceptAnyVisiblePosition = !isCandidate(test);

    Position current = test;
    Position downstreamTest = test.downstream(StayInBlock);
    while (!atEnd(current)) {
        current = nextPosition(current);
        if (isCandidate(current) && (acceptAnyVisiblePosition || (downstreamTest != current.downstream(StayInBlock)))) {
            return current;
        }
    }
    
    return Position();
}

Position VisiblePosition::previousPosition(const Position &pos)
{
    if (pos.isNull())
        return pos;
    
    Position result;

    if (pos.offset() <= 0) {
        NodeImpl *prevNode = pos.node()->traversePreviousNode();
        if (prevNode)
            result = Position(prevNode, prevNode->maxOffset());
    }
    else {
        result = Position(pos.node(), pos.offset() - 1);
    }
    
    return result;
}

Position VisiblePosition::nextPosition(const Position &pos)
{
    if (pos.isNull())
        return pos;
    
    Position result;

    if (pos.offset() >= pos.node()->maxOffset()) {
        NodeImpl *nextNode = pos.node()->traverseNextNode();
        if (nextNode)
            result = Position(nextNode, 0);
    }
    else {
        result = Position(pos.node(), pos.offset() + 1);
    }
    
    return result;
}

bool VisiblePosition::atStart(const Position &pos)
{
    if (pos.isNull())
        return true;

    return pos.offset() <= 0 && pos.node()->previousLeafNode() == 0;
}

bool VisiblePosition::atEnd(const Position &pos)
{
    if (pos.isNull())
        return true;

    return pos.offset() >= pos.node()->maxOffset() && pos.node()->nextLeafNode() == 0;
}

bool VisiblePosition::isCandidate(const Position &pos)
{
    if (pos.isNull())
        return false;
        
    RenderObject *renderer = pos.node()->renderer();
    if (!renderer)
        return false;
    
    if (renderer->style()->visibility() != VISIBLE)
        return false;

    if (renderer->isReplaced())
        // return true for replaced elements
        return pos.offset() == 0 || pos.offset() == 1;

    if (renderer->isBR()) {
        if (pos.offset() == 0) {
            InlineBox* box = static_cast<RenderText*>(renderer)->firstTextBox();
            if (box) {
                // return true for offset 0 into BR element on a line by itself
                RootInlineBox* root = box->root();
                if (root)
                    return root->firstLeafChild() == box && root->lastLeafChild() == box;
            }
        }   
        return false;
    }
    
    if (renderer->isText()) {
        RenderText *textRenderer = static_cast<RenderText *>(renderer);
        for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
            if (pos.offset() >= box->m_start && pos.offset() <= box->m_start + box->m_len) {
                // return true if in a text node
                return true;
            }
            else if (pos.offset() < box->m_start) {
                // The offset we're looking for is before this node
                // this means the offset must be in content that is
                // not rendered. Return false.
                return false;
            }
        }
    }
    
    if (renderer->isBlockFlow() && !renderer->firstChild() && (renderer->height() || pos.node()->id() == ID_BODY))
        // return true for offset 0 into rendered blocks that are empty of rendered kids, but have a height
        return pos.offset() == 0;
    
    return false;
}

Position VisiblePosition::deepEquivalent(const Position &pos)
{
    NodeImpl *node = pos.node();
    long offset = pos.offset();

    if (!node)
        return Position();
    
    if (isAtomicNode(node))
        return pos;

    if (offset >= (long)node->childNodeCount()) {
        do {
            NodeImpl *child = node->lastChild();
            if (!child)
                break;
            node = child;
        } while (!isAtomicNode(node));
        return Position(node, node->caretMaxOffset());
    }
    
    node = node->childNode(offset);
    ASSERT(node);
    while (!isAtomicNode(node)) {
        NodeImpl *child = node->firstChild();
        if (!child)
            break;
        node = child;
    }
    return Position(node, 0);
}

Position VisiblePosition::downstreamDeepEquivalent() const
{
    Position pos = m_deepPosition;
    
    if (pos.isNull() || atEnd(pos))
        return pos;

    Position downstreamTest = pos.downstream(StayInBlock);

    Position current = pos;
    while (!atEnd(current)) {
        current = nextPosition(current);
        if (isCandidate(current)) {
            if (downstreamTest != current.downstream(StayInBlock))
                break;
            pos = current;
        }
    }
    
    return pos;
}

Position VisiblePosition::rangeCompliantEquivalent(const Position &pos)
{
    NodeImpl *node = pos.node();
    if (!node)
        return Position();
    
    // FIXME: This clamps out-of-range values.
    // Instead we should probably assert, and not use such values.

    long offset = pos.offset();
    if (!offsetInCharacters(node->nodeType()) && isAtomicNode(node) && offset > 0)
        return Position(node->parentNode(), node->nodeIndex() + 1);

    return Position(node, kMax(0L, kMin(offset, maxOffset(node))));
}

long VisiblePosition::maxOffset(const NodeImpl *node)
{
    return offsetInCharacters(node->nodeType()) ? (long)static_cast<const CharacterDataImpl *>(node)->length() : (long)node->childNodeCount();
}

bool VisiblePosition::isAtomicNode(const NodeImpl *node)
{
    return node && (!node->hasChildNodes() || (node->id() == ID_OBJECT && node->renderer() && node->renderer()->isReplaced()));
}

QChar VisiblePosition::character() const
{
    Position pos = position();
    NodeImpl *node = pos.node();
    if (!node || !node->isTextNode()) {
        return QChar();
    }
    TextImpl *textNode = static_cast<TextImpl *>(pos.node());
    long offset = pos.offset();
    if ((unsigned)offset >= textNode->length()) {
        return QChar();
    }
    return textNode->data()[offset];
}

void VisiblePosition::debugPosition(const char *msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, getTagName(m_deepPosition.node()->id()).string().latin1(), m_deepPosition.node(), m_deepPosition.offset());
}

#ifndef NDEBUG
void VisiblePosition::formatForDebugger(char *buffer, unsigned length) const
{
    m_deepPosition.formatForDebugger(buffer, length);
}
#endif

Range makeRange(const VisiblePosition &start, const VisiblePosition &end)
{
    Position s = start.position();
    Position e = end.position();
    return Range(s.node(), s.offset(), e.node(), e.offset());
}

VisiblePosition startVisiblePosition(const Range &r)
{
    return VisiblePosition(r.startContainer().handle(), r.startOffset());
}

VisiblePosition endVisiblePosition(const Range &r)
{
    return VisiblePosition(r.endContainer().handle(), r.endOffset());
}

bool setStart(Range &r, const VisiblePosition &c)
{
    RangeImpl *ri = r.handle();
    if (!ri)
        return false;
    Position p = c.position();
    int code = 0;
    ri->setStart(p.node(), p.offset(), code);
    return code == 0;
}

bool setEnd(Range &r, const VisiblePosition &c)
{
    RangeImpl *ri = r.handle();
    if (!ri)
        return false;
    Position p = c.position();
    int code = 0;
    ri->setEnd(p.node(), p.offset(), code);
    return code == 0;
}

bool visiblePositionsOnDifferentLines(const VisiblePosition &pos1, const VisiblePosition &pos2)
{
    if (pos1.isNull() || pos2.isNull())
        return false;
    if (pos1 == pos2)
        return false;
    
    Position p1 = pos1.deepEquivalent();
    Position p2 = pos2.deepEquivalent();
    RenderObject *r1 = p1.node()->renderer();
    RenderObject *r2 = p2.node()->renderer();
    InlineBox *b1 = r1 ? r1->inlineBox(p1.offset()) : 0;
    InlineBox *b2 = r2 ? r2->inlineBox(p2.offset()) : 0;
    return (b1 && b2 && b1->root() != b2->root());
}

bool isFirstVisiblePositionOnLine(const VisiblePosition &pos)
{
    if (pos.isNull())
        return false;
        
    VisiblePosition previous = pos.previous();
    return previous.isNull() || visiblePositionsOnDifferentLines(pos, previous);
}

bool isLastVisiblePositionOnLine(const VisiblePosition &pos)
{
    if (pos.isNull())
        return false;
        
    VisiblePosition next = pos.next();
    return next.isNull() || visiblePositionsOnDifferentLines(pos, next);
}


}  // namespace DOM
