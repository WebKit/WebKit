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

#include "config.h"
#include "visible_position.h"
#include "visible_units.h"
#include "htmlediting.h"
#include "htmlnames.h"
#include "rendering/render_line.h"
#include "rendering/render_object.h"
#include "InlineTextBox.h"
#include "DocumentImpl.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom_textimpl.h"

#include <kxmlcore/Assertions.h>
#include "KWQLogging.h"

using namespace DOM::HTMLNames;

using DOM::CharacterDataImpl;
using DOM::NodeImpl;
using DOM::offsetInCharacters;
using DOM::UsingComposedCharacters;
using DOM::Position;
using DOM::Range;
using DOM::RangeImpl;
using DOM::TextImpl;

namespace khtml {

VisiblePosition::VisiblePosition(const Position &pos, EAffinity affinity)
{
    init(pos, affinity);
}

VisiblePosition::VisiblePosition(NodeImpl *node, int offset, EAffinity affinity)
{
    init(Position(node, offset), affinity);
}

VisiblePosition::VisiblePosition(const VisiblePosition &other)
{
    m_deepPosition = other.m_deepPosition;
    m_affinity = other.m_affinity;
}

void VisiblePosition::init(const Position &pos, EAffinity affinity)
{
    m_affinity = affinity;
    
    if (pos.isNull()) {
        m_deepPosition = Position();
        return;
    }
    
    pos.node()->getDocument()->updateLayoutIgnorePendingStylesheets();
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
        Position prev = previousVisiblePosition(deepPos);
        
        if (next.isNull() && prev.isNull())
            m_deepPosition = Position();
        else if (next.isNull())
            m_deepPosition = prev;
        else if (prev.isNull())
            m_deepPosition = next;
        else {
            NodeImpl *originalBlock = pos.node() ? pos.node()->enclosingBlockFlowElement() : 0;
            bool nextIsOutsideOriginalBlock = !next.node()->isAncestor(originalBlock) && next.node() != originalBlock;
            bool prevIsOutsideOriginalBlock = !prev.node()->isAncestor(originalBlock) && prev.node() != originalBlock;
            
            if (nextIsOutsideOriginalBlock && !prevIsOutsideOriginalBlock || 
                (deepPos.node()->hasTagName(brTag) && deepPos.offset() == 0))
                m_deepPosition = prev;
            else
                m_deepPosition = next;
        }
    }

    // When not at a line wrap, make sure to end up with DOWNSTREAM affinity.
    if (m_affinity == UPSTREAM && (isNull() || inSameLine(VisiblePosition(pos, DOWNSTREAM), *this)))
        m_affinity = DOWNSTREAM;
}

VisiblePosition VisiblePosition::next() const
{
    return VisiblePosition(nextVisiblePosition(m_deepPosition), m_affinity);
}

VisiblePosition VisiblePosition::previous() const
{
    // find first previous DOM position that is visible
    Position pos = m_deepPosition;
    while (!pos.atStart()) {
        pos = pos.previous(UsingComposedCharacters);
        if (isCandidate(pos))
            break;
    }
    
    // return null visible position if there is no previous visible position
    if (pos.atStart())
        return VisiblePosition();
        
    VisiblePosition result = VisiblePosition(pos, DOWNSTREAM);
    ASSERT(result != *this);
    
#ifndef NDEBUG
    // we should always be able to make the affinity DOWNSTREAM, because going previous from an
    // UPSTREAM position can never yield another UPSTREAM position (unless line wrap length is 0!).
    if (result.isNotNull() && m_affinity == UPSTREAM) {
        VisiblePosition temp = result;
        temp.setAffinity(UPSTREAM);
        ASSERT(inSameLine(temp, result));
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

bool hasRenderedChildrenWithHeight(RenderObject *renderer)
{
    if (!renderer->firstChild())
        return false;

    for (RenderObject *child = renderer->firstChild(); child; child = child->nextSibling())
        if (child->height())
            return true;
    
    return false;
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
        return pos.offset() == 0 || pos.offset() == maxDeepOffset(pos.node());

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
    
    // True if at a rendered offset inside a text node
    if (renderer->isText()) {
        RenderText *textRenderer = static_cast<RenderText *>(renderer);
        for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
            if (pos.offset() >= box->m_start && pos.offset() <= box->m_start + box->m_len) {
                return true;
            }
        }
    }
    
    if (renderer->isBlockFlow() && !hasRenderedChildrenWithHeight(renderer) &&
       (renderer->height() || pos.node()->hasTagName(bodyTag)))
       return pos.offset() == 0;
    
    return false;
}

Position VisiblePosition::deepEquivalent(const Position &pos)
{
    NodeImpl *node = pos.node();
    int offset = pos.offset();

    if (!node)
        return Position();
    
    if (isCandidate(pos) || isAtomicNode(node))
        return pos;

    if (offset >= (int)node->childNodeCount()) {
        do {
            NodeImpl *child = node->lastChild();
            if (!child)
                break;
            node = child;
        } while (!isCandidate(Position(node, maxDeepOffset(node))) && !isAtomicNode(node));
        return Position(node, maxDeepOffset(node));
    }
    
    node = node->childNode(offset);
    ASSERT(node);
    while (!isCandidate(Position(node, 0)) && !isAtomicNode(node)) {
        NodeImpl *child = node->firstChild();
        if (!child)
            break;
        node = child;
    }
    return Position(node, 0);
}

int VisiblePosition::maxOffset(const NodeImpl *node)
{
    return offsetInCharacters(node->nodeType()) ? (int)static_cast<const CharacterDataImpl *>(node)->length() : (int)node->childNodeCount();
}

QChar VisiblePosition::character() const
{
    Position pos = m_deepPosition;
    NodeImpl *node = pos.node();
    if (!node || !node->isTextNode()) {
        return QChar();
    }
    TextImpl *textNode = static_cast<TextImpl *>(pos.node());
    int offset = pos.offset();
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
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, m_deepPosition.node()->nodeName().qstring().latin1(), m_deepPosition.node(), m_deepPosition.offset());
}

#ifndef NDEBUG
void VisiblePosition::formatForDebugger(char *buffer, unsigned length) const
{
    m_deepPosition.formatForDebugger(buffer, length);
}

void VisiblePosition::showTree() const
{
    if (m_deepPosition.node())
        m_deepPosition.node()->showTreeAndMark(m_deepPosition.node(), "*", NULL, NULL);
}
#endif

PassRefPtr<RangeImpl> makeRange(const VisiblePosition &start, const VisiblePosition &end)
{
    Position s = start.deepEquivalent().equivalentRangeCompliantPosition();
    Position e = end.deepEquivalent().equivalentRangeCompliantPosition();
    return new RangeImpl(s.node()->getDocument(), s.node(), s.offset(), e.node(), e.offset());
}

VisiblePosition startVisiblePosition(const RangeImpl *r, EAffinity affinity)
{
    int exception = 0;
    return VisiblePosition(r->startContainer(exception), r->startOffset(exception), affinity);
}

VisiblePosition endVisiblePosition(const RangeImpl *r, EAffinity affinity)
{
    int exception = 0;
    return VisiblePosition(r->endContainer(exception), r->endOffset(exception), affinity);
}

bool setStart(RangeImpl *r, const VisiblePosition &visiblePosition)
{
    if (!r)
        return false;
    Position p = visiblePosition.deepEquivalent().equivalentRangeCompliantPosition();
    int code = 0;
    r->setStart(p.node(), p.offset(), code);
    return code == 0;
}

bool setEnd(RangeImpl *r, const VisiblePosition &visiblePosition)
{
    if (!r)
        return false;
    Position p = visiblePosition.deepEquivalent().equivalentRangeCompliantPosition();
    int code = 0;
    r->setEnd(p.node(), p.offset(), code);
    return code == 0;
}

DOM::NodeImpl *enclosingBlockFlowElement(const VisiblePosition &visiblePosition)
{
    if (visiblePosition.isNull())
        return NULL;

    return visiblePosition.deepEquivalent().node()->enclosingBlockFlowElement();
}

bool isFirstVisiblePositionInNode(const VisiblePosition &visiblePosition, const NodeImpl *node)
{
    if (visiblePosition.isNull())
        return false;
    
    if (!visiblePosition.deepEquivalent().node()->isAncestor(node))
        return false;
        
    VisiblePosition previous = visiblePosition.previous();
    return previous.isNull() || !previous.deepEquivalent().node()->isAncestor(node);
}

bool isLastVisiblePositionInNode(const VisiblePosition &visiblePosition, const NodeImpl *node)
{
    if (visiblePosition.isNull())
        return false;
    
    if (!visiblePosition.deepEquivalent().node()->isAncestor(node))
        return false;
                
    VisiblePosition next = visiblePosition.next();
    return next.isNull() || !next.deepEquivalent().node()->isAncestor(node);
}

}  // namespace DOM
