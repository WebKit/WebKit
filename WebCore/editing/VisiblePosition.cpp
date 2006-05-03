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
#include "VisiblePosition.h"

#include "Document.h"
#include "Element.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "Logging.h"
#include "Range.h"
#include "htmlediting.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

VisiblePosition::VisiblePosition(const Position &pos, EAffinity affinity)
{
    init(pos, affinity);
}

VisiblePosition::VisiblePosition(Node *node, int offset, EAffinity affinity)
{
    init(Position(node, offset), affinity);
}

void VisiblePosition::init(const Position& position, EAffinity affinity)
{
    m_affinity = affinity;
    
    initDeepPosition(position, affinity);
    
    // When not at a line wrap, make sure to end up with DOWNSTREAM affinity.
    if (m_affinity == UPSTREAM && (isNull() || inSameLine(VisiblePosition(position, DOWNSTREAM), *this)))
        m_affinity = DOWNSTREAM;
}

VisiblePosition VisiblePosition::next() const
{
    return VisiblePosition(nextVisiblePosition(m_deepPosition), m_affinity);
}

VisiblePosition VisiblePosition::previous() const
{
    // find first previous DOM position that is visible
    Position pos = previousVisiblePosition(m_deepPosition);
    
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

Position VisiblePosition::previousVisiblePosition(const Position& pos)
{
    if (!pos.inRenderedContent()) {
        Position current = pos;
        while (!current.atStart()) {
            current = current.previous(UsingComposedCharacters);
            if (current.inRenderedContent())
                return current;
        }
        return Position();
    }

    Position downstreamStart = pos.downstream();
    Position current = pos;
    while (!current.atStart()) {
        current = current.previous(UsingComposedCharacters);
        if (current.inRenderedContent() && downstreamStart != current.downstream())
            return current;
    }
    return Position();
}

Position VisiblePosition::nextVisiblePosition(const Position& pos)
{
    if (!pos.inRenderedContent()) {
        Position current = pos;
        while (!current.atEnd()) {
            current = current.next(UsingComposedCharacters);
            if (current.inRenderedContent())
                return current;
        }
        return Position();
    }

    Position downstreamStart = pos.downstream();
    Position current = pos;
    while (!current.atEnd()) {
        current = current.next(UsingComposedCharacters);
        if (current.inRenderedContent() && downstreamStart != current.downstream())
            return current;
    }
    return Position();
}

void VisiblePosition::initDeepPosition(const Position& position, EAffinity affinity)
{
    // FIXME: No need for affinity parameter.
    // FIXME: Would read nicer if this was a function that returned a Position.

    Node* node = position.node();
    if (!node) {
        m_deepPosition = Position();
        return;
    }

    node->document()->updateLayoutIgnorePendingStylesheets();

    // If two visually equivalent positions are both candidates for being made the m_deepPosition,
    // (this can happen when two rendered positions have only collapsed whitespace between them),
    // we always choose the one that occurs first in the DOM to canonicalize VisiblePositions.
    m_deepPosition = position.upstream();
    if (m_deepPosition.inRenderedContent())
        return;
    m_deepPosition = position;
    if (m_deepPosition.inRenderedContent())
        return;
    m_deepPosition = position.downstream();
    if (m_deepPosition.inRenderedContent())
        return;

    // When neither upstream or downstream gets us to a visible position,
    // look at the next and previous visible position.
    Position next = nextVisiblePosition(position);
    Position prev = previousVisiblePosition(position);
    Node* nextNode = next.node();
    Node* prevNode = prev.node();

    // The new position must be in the same editable element. Enforce that first.
    Node* editingRoot = node->rootEditableElement();
    bool prevIsInSameEditableElement = prevNode && prevNode->rootEditableElement() == editingRoot;
    bool nextIsInSameEditableElement = nextNode && nextNode->rootEditableElement() == editingRoot;
    if (prevIsInSameEditableElement && !nextIsInSameEditableElement) {
        m_deepPosition = prev;
        return;
    }
    if (nextIsInSameEditableElement && !prevIsInSameEditableElement) {
        m_deepPosition = next;
        return;
    }
    if (!nextIsInSameEditableElement && !prevIsInSameEditableElement) {
        m_deepPosition = Position();
        return;
    }

    // The new position should be in the same block flow element. Favor that.
    Node *originalBlock = node->enclosingBlockFlowElement();
    bool nextIsOutsideOriginalBlock = !nextNode->isAncestor(originalBlock) && nextNode != originalBlock;
    bool prevIsOutsideOriginalBlock = !prevNode->isAncestor(originalBlock) && prevNode != originalBlock;
    if (nextIsOutsideOriginalBlock && !prevIsOutsideOriginalBlock)
        m_deepPosition = prev;
    else
        m_deepPosition = next;
}

int VisiblePosition::maxOffset(const Node *node)
{
    return node->offsetInCharacters() ? (int)static_cast<const CharacterData *>(node)->length() : (int)node->childNodeCount();
}

QChar VisiblePosition::characterAfter() const
{
    // We canonicalize to the first of two equivalent candidates, but the second of the two candidates
    // is the one that will be inside the text node containing the character after this visible position.
    Position pos = m_deepPosition.downstream();
    Node *node = pos.node();
    if (!node || !node->isTextNode()) {
        return QChar();
    }
    Text *textNode = static_cast<Text *>(pos.node());
    int offset = pos.offset();
    if ((unsigned)offset >= textNode->length()) {
        return QChar();
    }
    return textNode->data()[offset];
}

bool isEqualIgnoringAffinity(const VisiblePosition &a, const VisiblePosition &b)
{
    bool result = a.deepEquivalent() == b.deepEquivalent() || 
                  // FIXME (8622): This is a slow but temporary workaround. 
                  a.deepEquivalent().downstream() == b.deepEquivalent().downstream();
    if (result) {
        // We want to catch cases where positions are equal, but affinities are not, since
        // this is very likely a bug, given the places where this call is used. The difference
        // is very likely due to code that set the affinity on a VisiblePosition "by hand" and 
        // did so incorrectly.
        ASSERT(a.affinity() == b.affinity());
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
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, m_deepPosition.node()->nodeName().deprecatedString().latin1(), m_deepPosition.node(), m_deepPosition.offset());
}

#ifndef NDEBUG

void VisiblePosition::formatForDebugger(char* buffer, unsigned length) const
{
    m_deepPosition.formatForDebugger(buffer, length);
}

void VisiblePosition::showTreeForThis() const
{
    m_deepPosition.showTreeForThis();
}

#endif

PassRefPtr<Range> makeRange(const VisiblePosition &start, const VisiblePosition &end)
{
    Position s = rangeCompliantEquivalent(start);
    Position e = rangeCompliantEquivalent(end);
    return new Range(s.node()->document(), s.node(), s.offset(), e.node(), e.offset());
}

VisiblePosition startVisiblePosition(const Range *r, EAffinity affinity)
{
    int exception = 0;
    return VisiblePosition(r->startContainer(exception), r->startOffset(exception), affinity);
}

VisiblePosition endVisiblePosition(const Range *r, EAffinity affinity)
{
    int exception = 0;
    return VisiblePosition(r->endContainer(exception), r->endOffset(exception), affinity);
}

bool setStart(Range *r, const VisiblePosition &visiblePosition)
{
    if (!r)
        return false;
    Position p = rangeCompliantEquivalent(visiblePosition);
    int code = 0;
    r->setStart(p.node(), p.offset(), code);
    return code == 0;
}

bool setEnd(Range *r, const VisiblePosition &visiblePosition)
{
    if (!r)
        return false;
    Position p = rangeCompliantEquivalent(visiblePosition);
    int code = 0;
    r->setEnd(p.node(), p.offset(), code);
    return code == 0;
}

WebCore::Node *enclosingBlockFlowElement(const VisiblePosition &visiblePosition)
{
    if (visiblePosition.isNull())
        return NULL;

    return visiblePosition.deepEquivalent().node()->enclosingBlockFlowElement();
}

bool isFirstVisiblePositionInNode(const VisiblePosition &visiblePosition, const Node *node)
{
    if (visiblePosition.isNull())
        return false;
    
    if (!visiblePosition.deepEquivalent().node()->isAncestor(node))
        return false;
        
    VisiblePosition previous = visiblePosition.previous();
    return previous.isNull() || !previous.deepEquivalent().node()->isAncestor(node);
}

bool isLastVisiblePositionInNode(const VisiblePosition &visiblePosition, const Node *node)
{
    if (visiblePosition.isNull())
        return false;
    
    if (!visiblePosition.deepEquivalent().node()->isAncestor(node))
        return false;
                
    VisiblePosition next = visiblePosition.next();
    return next.isNull() || !next.deepEquivalent().node()->isAncestor(node);
}

}  // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::VisiblePosition* vpos)
{
    if (vpos)
        vpos->showTreeForThis();
}

void showTree(const WebCore::VisiblePosition& vpos)
{
    vpos.showTreeForThis();
}

#endif
