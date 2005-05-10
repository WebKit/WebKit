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
#include "visible_units.h"

#include "misc/htmltags.h"
#include "rendering/render_line.h"
#include "rendering/render_object.h"
#include "rendering/render_text.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#else
#define ASSERT(assertion) assert(assertion)
#define LOG(channel, formatAndArgs...) ((void)0)
#endif

using DOM::CharacterDataImpl;
using DOM::NodeImpl;
using DOM::offsetInCharacters;
using DOM::UsingComposedCharacters;
using DOM::Position;
using DOM::Range;
using DOM::RangeImpl;
using DOM::TextImpl;

namespace khtml {

VisiblePosition::VisiblePosition(const Position &pos, EAffinity affinity, EInitHint initHint)
{
    init(pos, initHint, affinity);
}

VisiblePosition::VisiblePosition(NodeImpl *node, long offset, EAffinity affinity, EInitHint initHint)
{
    init(Position(node, offset), initHint, affinity);
}

VisiblePosition::VisiblePosition(const VisiblePosition &other)
{
    m_deepPosition = other.m_deepPosition;
    m_affinity = other.m_affinity;
}

void VisiblePosition::init(const Position &pos, EInitHint initHint, EAffinity affinity)
{
    m_affinity = affinity;

    // A first step toward eliminating the initHint parameter.
    // For <br> 0, it's important not to move past the <br>.
    if (pos.node() && pos.node()->id() == ID_BR && pos.offset() == 0)
        initHint = INIT_UP;

    switch (initHint) {
        case INIT_UP:
            initUpstream(pos);
            break;
        case INIT_DOWN:
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
        } else {
            Position next = nextVisiblePosition(deepPos);
            if (next.isNotNull())
                m_deepPosition = next;
        }
    }
}

static bool isRenderedBR(NodeImpl *node)
{
    if (!node)
        return false;

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return false;
    
    if (renderer->style()->visibility() != VISIBLE)
        return false;

    if (renderer->isBR())
        return true;

    return false;
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
    } else {
        Position next = nextVisiblePosition(deepPos);
        if (next.isNotNull()) {
            m_deepPosition = next;
        } else if (isRenderedBR(deepPos.node()) && deepPos.offset() == 1) {
            m_deepPosition = deepPos;
        } else {
            Position previous = previousVisiblePosition(deepPos);
            if (previous.isNotNull())
                m_deepPosition = previous;
        }
    }
}

VisiblePosition VisiblePosition::next() const
{
    VisiblePosition result = VisiblePosition(nextVisiblePosition(m_deepPosition), m_affinity);
    setAffinityUsingLinePosition(result);
    return result;
}

VisiblePosition VisiblePosition::previous() const
{
    VisiblePosition result =  VisiblePosition(previousVisiblePosition(m_deepPosition), DOWNSTREAM);

#ifndef NDEBUG
    // we should always be able to make the affinity DOWNSTREAM, because going previous from an
    // UPSTREAM position can never yield another UPSTREAM position (unless line wrap length is 0!).
    if (result.isNotNull() && m_affinity == UPSTREAM) {
        VisiblePosition temp = result;
        temp.setAffinity(UPSTREAM);
        ASSERT(!visiblePositionsOnDifferentLines(temp, result));
    }
#endif
    return result;
}

Position VisiblePosition::previousVisiblePosition(const Position &pos)
{
    if (pos.isNull() || pos.atStart())
        return Position();

    Position test = deepEquivalent(pos);
    Position downstreamTest = test.downstream();
    bool acceptAnyVisiblePosition = !isCandidate(test);

    Position current = test;
    while (!current.atStart()) {
        current = current.previous(UsingComposedCharacters);
        if (isCandidate(current) && (acceptAnyVisiblePosition || (downstreamTest != current.downstream()))) {
            return current;
        }
    }
    
    return Position();
}

Position VisiblePosition::nextVisiblePosition(const Position &pos)
{
    if (pos.isNull() || pos.atEnd())
        return Position();

    Position test = deepEquivalent(pos);
    bool acceptAnyVisiblePosition = !isCandidate(test);

    Position current = test;
    Position downstreamTest = test.downstream();
    while (!current.atEnd()) {
        current = current.next(UsingComposedCharacters);
        if (isCandidate(current) && (acceptAnyVisiblePosition || (downstreamTest != current.downstream()))) {
            return current;
        }
    }
    
    return Position();
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
        }
    }
    
    if (renderer->isBlockFlow() && (!renderer->firstChild() || !pos.node()->firstChild()) && 
       (renderer->height() || pos.node()->id() == ID_BODY)) {
        // return true for offset 0 into rendered blocks that are empty of rendered kids, but have a height
        return pos.offset() == 0;
    }
    
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
    
    if (pos.isNull() || pos.atEnd())
        return pos;

    Position downstreamTest = pos.downstream();

    Position current = pos;
    while (!current.atEnd()) {
        current = current.next(UsingComposedCharacters);
        if (isCandidate(current)) {
            if (downstreamTest != current.downstream())
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

bool isEqualIgnoringAffinity(const VisiblePosition &a, const VisiblePosition &b)
{
    bool result = a.m_deepPosition == b.m_deepPosition;
    if (result) {
        // We want to catch cases where positions are equal, but affinities are not, since
        // this is very likely a bug, given the places where this call is used. The difference
        // is very likely due to code that set the affinity on a VisiblePosition "by hand" and 
        // did so incorrectly.
        ASSERT(a.m_affinity == b.m_affinity);
    }
    return result;
}

bool isNotEqualIgnoringAffinity(const VisiblePosition &a, const VisiblePosition &b)
{
    return !isEqualIgnoringAffinity(a, b);
}

void VisiblePosition::debugPosition(const char *msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %ld\n", msg, m_deepPosition.node()->nodeName().string().latin1(), m_deepPosition.node(), m_deepPosition.offset());
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

VisiblePosition startVisiblePosition(const Range &r, EAffinity affinity)
{
    return VisiblePosition(r.startContainer().handle(), r.startOffset(), affinity);
}

VisiblePosition endVisiblePosition(const Range &r, EAffinity affinity)
{
    return VisiblePosition(r.endContainer().handle(), r.endOffset(), affinity);
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

void setAffinityUsingLinePosition(VisiblePosition &pos)
{
    // When not moving across line wrap, make sure to end up with DOWNSTREAM affinity.  
    if (pos.isNotNull() && pos.affinity() == UPSTREAM) {
        VisiblePosition temp(pos);
        temp.setAffinity(DOWNSTREAM);
        if (!visiblePositionsOnDifferentLines(temp, pos))
            pos.setAffinity(DOWNSTREAM);
    }
}

DOM::NodeImpl *enclosingBlockFlowElement(const VisiblePosition &vp)
{
    if (vp.isNull())
        return NULL;

    return vp.position().node()->enclosingBlockFlowElement();
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
    if (r1->isBlockFlow() || r2->isBlockFlow())
        return r1 == r2 ? false : true;
    InlineBox *b1 = r1 ? r1->inlineBox(p1.offset(), pos1.affinity()) : 0;
    InlineBox *b2 = r2 ? r2->inlineBox(p2.offset(), pos2.affinity()) : 0;
    return (b1 && b2 && b1->root() != b2->root());
}

enum EBlockRelationship { 
    NoBlockRelationship, 
    SameBlockRelationship, 
    PeerBlockRelationship, 
    AncestorBlockRelationship, 
    DescendantBlockRelationship, 
    OtherBlockRelationship 
};

static EBlockRelationship blockRelationship(const VisiblePosition &pos1, const VisiblePosition &pos2)
{
    if (pos1.isNull() || pos2.isNull())
        return NoBlockRelationship;
    if (pos1 == pos2)
        return SameBlockRelationship;

    Position p1 = pos1.deepEquivalent();
    Position p2 = pos2.deepEquivalent();
    NodeImpl *b1 = p1.node()->enclosingBlockFlowElement();
    NodeImpl *b2 = p2.node()->enclosingBlockFlowElement();
    if (!b1 || !b2)
        return NoBlockRelationship;
    if (b1 == b2) 
        return SameBlockRelationship;
    if (b1->parentNode() == b2->parentNode())
        return PeerBlockRelationship;
    if (b2->isAncestor(b1))
        return AncestorBlockRelationship;
    if (b1->isAncestor(b2))
        return DescendantBlockRelationship;
    return OtherBlockRelationship;
}

bool visiblePositionsInDifferentBlocks(const VisiblePosition &pos1, const VisiblePosition &pos2)
{
    switch (blockRelationship(pos1, pos2)) {
        case NoBlockRelationship:
        case SameBlockRelationship:
            return false;
        case PeerBlockRelationship:
        case AncestorBlockRelationship:
        case DescendantBlockRelationship:
        case OtherBlockRelationship:
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool isFirstVisiblePositionOnLine(const VisiblePosition &pos)
{
    if (pos.isNull())
        return false;
        
    VisiblePosition previous = pos.previous();
    return previous.isNull() || visiblePositionsOnDifferentLines(pos, previous);
}

bool isFirstVisiblePositionInParagraph(const VisiblePosition &pos)
{
    if (pos.isNull())
        return false;

    return pos.deepEquivalent().upstream().node()->id() == ID_BR || isStartOfBlock(pos);
}

bool isFirstVisiblePositionInNode(const VisiblePosition &pos, const NodeImpl *node)
{
    if (pos.isNull())
        return false;
        
    VisiblePosition previous = pos.previous();
    return previous.isNull() || !previous.deepEquivalent().node()->isAncestor(node);
}

bool isLastVisiblePositionOnLine(const VisiblePosition &pos)
{
    if (pos.isNull())
        return false;
        
    VisiblePosition next = pos.next();
    return next.isNull() || visiblePositionsOnDifferentLines(pos, next);
}

bool isLastVisiblePositionInParagraph(const VisiblePosition &pos)
{
    if (pos.isNull())
        return false;

    return pos.deepEquivalent().downstream().node()->id() == ID_BR || isEndOfBlock(pos);
}

bool isLastVisiblePositionInNode(const VisiblePosition &pos, const NodeImpl *node)
{
    if (pos.isNull())
        return false;
        
    VisiblePosition next = pos.next();
    return next.isNull() || !next.deepEquivalent().node()->isAncestor(node);
}

}  // namespace DOM
