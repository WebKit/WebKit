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

#include "dom_caretposition.h"

#include "dom2_rangeimpl.h"
#include "dom_elementimpl.h"
#include "dom_nodeimpl.h"
#include "dom_string.h"
#include "htmltags.h"
#include "khtml_text_operations.h"
#include "render_object.h"
#include "render_text.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#else
#define ASSERT(assertion) assert(assertion)
#define LOG(channel, formatAndArgs...) ((void)0)
#endif

using khtml::CharacterIterator;
using khtml::findWordBoundary;
using khtml::InlineTextBox;
using khtml::nextWordFromIndex;
using khtml::PRE;
using khtml::RenderObject;
using khtml::RenderStyle;
using khtml::RenderText;
using khtml::SimplifiedBackwardsTextIterator;
using khtml::TextIterator;
using khtml::VISIBLE;

namespace DOM {

CaretPosition::CaretPosition(NodeImpl *node, long offset)
{
    init(Position(node, offset));
}

CaretPosition::CaretPosition(const Position &pos)
{
    init(pos);
}

void CaretPosition::init(const Position &pos)
{
    Position deepPos = deepEquivalent(pos);
    
    if (isCandidate(deepPos)) {
        m_deepPosition = deepPos;
        Position previous = previousCaretPosition(deepPos);
        if (previous.isNotNull()) {
            Position next = nextCaretPosition(previous);
            if (next.isNotNull())
                m_deepPosition = next;
        }
    }
    else {
        Position next = nextCaretPosition(deepPos);
        if (next.isNotNull()) {
            m_deepPosition = next;
        }
        else {
            Position previous = previousCaretPosition(deepPos);
            if (previous.isNotNull())
                m_deepPosition = previous;
        }
    }
}

bool CaretPosition::isLastInBlock() const
{
    if (isNull())
        return false;
        
    CaretPosition n = next();
    return n.isNull() || (n.deepEquivalent().node()->enclosingBlockFlowElement() != m_deepPosition.node()->enclosingBlockFlowElement());
}

CaretPosition CaretPosition::next() const
{
    CaretPosition result;
    result.m_deepPosition = nextCaretPosition(m_deepPosition);
    return result;
}

CaretPosition CaretPosition::previous() const
{
    CaretPosition result;
    result.m_deepPosition = previousCaretPosition(m_deepPosition);
    return result;
}

Position CaretPosition::previousCaretPosition(const Position &pos)
{
    if (pos.isNull() || atStart(pos))
        return Position();

    Position test = deepEquivalent(pos);
    bool acceptAnyCaretPosition = !isCandidate(test) || pos.isFirstRenderedPositionOnLine();

    Position current = test;
    while (!atStart(current)) {
        current = previousPosition(current);
        if (isCandidate(current) && (acceptAnyCaretPosition || current.rendersInDifferentPosition(test))) {
            return current;
        }
    }
    
    return Position();
}

Position CaretPosition::nextCaretPosition(const Position &pos)
{
    if (pos.isNull() || atEnd(pos))
        return Position();

    Position test = deepEquivalent(pos);
    bool acceptAnyCaretPosition = !isCandidate(test) || pos.isLastRenderedPositionOnLine();

    Position current = test;
    while (!atEnd(current)) {
        current = nextPosition(current);
        if (isCandidate(current) && (acceptAnyCaretPosition || current.rendersInDifferentPosition(test))) {
            return current;
        }
    }
    
    return Position();
}

Position CaretPosition::previousPosition(const Position &pos)
{
    if (pos.isNull())
        return pos;
    
    Position result;

    if (pos.offset() <= 0) {
        NodeImpl *prevNode = pos.node()->previousLeafNode();
        if (prevNode)
            result = Position(prevNode, prevNode->maxOffset());
    }
    else {
        result = Position(pos.node(), pos.offset() - 1);
    }
    
    return result;
}

Position CaretPosition::nextPosition(const Position &pos)
{
    if (pos.isNull())
        return pos;
    
    Position result;

    if (pos.offset() >= pos.node()->maxOffset()) {
        NodeImpl *nextNode = pos.node()->nextLeafNode();
        if (nextNode)
            result = Position(nextNode, 0);
    }
    else {
        result = Position(pos.node(), pos.offset() + 1);
    }
    
    return result;
}

bool CaretPosition::atStart(const Position &pos)
{
    if (pos.isNull())
        return true;

    return pos.offset() <= 0 && pos.node()->previousLeafNode() == 0;
}

bool CaretPosition::atEnd(const Position &pos)
{
    if (pos.isNull())
        return true;

    return pos.offset() >= pos.node()->maxOffset() && pos.node()->nextLeafNode() == 0;
}

bool CaretPosition::isCandidate(const Position &pos)
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

    if (renderer->isBR() && static_cast<RenderText *>(renderer)->firstTextBox())
        // return true for offset 0 into BR element on a line by itself
        return pos.offset() == 0;
    
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
    
    if (renderer->isBlockFlow() && !renderer->firstChild() && renderer->height())
        // return true for offset 0 into rendered blocks that are empty of rendered kids, but have a height
        return pos.offset() == 0;
    
    return false;
}

Position CaretPosition::deepEquivalent(const Position &pos)
{
    NodeImpl *node = pos.node();
    long offset = pos.offset();

    if (!node)
        return Position();
    
    if (isAtomicNode(node)) {
        // This is part of the strategy to wean the code off Positions with BRs and replaced elements
        // as the nodes and offsets > 0.
        if (offset > 0 && (node->id() == ID_BR || node->renderer() && node->renderer()->isReplaced())) {
            NodeImpl *next = node->traverseNextNode();
            if (next && node->enclosingBlockFlowElement() == next->enclosingBlockFlowElement())
                return deepEquivalent(Position(next, 0));
        }
        return pos;
    }

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

Position CaretPosition::rangeCompliantEquivalent(const Position &pos)
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

long CaretPosition::maxOffset(const NodeImpl *node)
{
    return offsetInCharacters(node->nodeType()) ? (long)static_cast<const CharacterDataImpl *>(node)->length() : (long)node->childNodeCount();
}

bool CaretPosition::isAtomicNode(const NodeImpl *node)
{
    return node && (!node->hasChildNodes() || (node->id() == ID_OBJECT && node->renderer() && node->renderer()->isReplaced()));
}

void CaretPosition::debugPosition(const char *msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, getTagName(m_deepPosition.node()->id()).string().latin1(), m_deepPosition.node(), m_deepPosition.offset());
}

#ifndef NDEBUG
void CaretPosition::formatForDebugger(char *buffer, unsigned length) const
{
    m_deepPosition.formatForDebugger(buffer, length);
}
#endif

Range makeRange(const CaretPosition &start, const CaretPosition &end)
{
    Position s = start.position();
    Position e = end.position();
    return Range(s.node(), s.offset(), e.node(), e.offset());
}

CaretPosition start(const Range &r)
{
    return CaretPosition(r.startContainer().handle(), r.startOffset());
}

CaretPosition end(const Range &r)
{
    return CaretPosition(r.endContainer().handle(), r.endOffset());
}

bool setStart(Range &r, const CaretPosition &c)
{
    RangeImpl *ri = r.handle();
    if (!ri)
        return false;
    Position p = c.position();
    int code = 0;
    ri->setStart(p.node(), p.offset(), code);
    return code == 0;
}

bool setEnd(Range &r, const CaretPosition &c)
{
    RangeImpl *ri = r.handle();
    if (!ri)
        return false;
    Position p = c.position();
    int code = 0;
    ri->setEnd(p.node(), p.offset(), code);
    return code == 0;
}

static CaretPosition previousWordBoundary(const CaretPosition &c, unsigned (*searchFunction)(const QChar *, unsigned))
{
    Position pos = c.deepEquivalent();
    NodeImpl *n = pos.node();
    if (!n)
        return CaretPosition();
    DocumentImpl *d = n->getDocument();
    if (!d)
        return CaretPosition();
    NodeImpl *de = d->documentElement();
    if (!de)
        return CaretPosition();

    Range searchRange(d);
    searchRange.setStartBefore(de);
    Position end(pos.equivalentRangeCompliantPosition());
    searchRange.setEnd(end.node(), end.offset());
    SimplifiedBackwardsTextIterator it(searchRange);
    QString string;
    unsigned next = 0;
    while (!it.atEnd() && it.length() > 0) {
        // Keep asking the iterator for chunks until the nextWordFromIndex() function
        // returns a non-zero value.
        string.prepend(it.characters(), it.length());
        next = searchFunction(string.unicode(), string.length());
        if (next != 0)
            break;
        it.advance();
    }
    
    if (it.atEnd() && next == 0) {
        Range range(it.range());
        pos = Position(range.startContainer().handle(), range.startOffset());
    }
    else if (!it.atEnd() && it.length() == 0) {
        // Got a zero-length chunk.
        // This means we have hit a replaced element.
        // Make a check to see if the position should be before or after the replaced element
        // by performing an additional check with a modified string which uses an "X" 
        // character to stand in for the replaced element.
        QChar chars[2];
        chars[0] = 'X';
        chars[1] = ' ';
        string.prepend(chars, 2);
        unsigned pastImage = searchFunction(string.unicode(), string.length());
        Range range(it.range());
        if (pastImage == 0)
            pos = Position(range.startContainer().handle(), range.startOffset());
        else
            pos = Position(range.endContainer().handle(), range.endOffset());
    }
    else if (next != 0) {
        // The simpler iterator used in this function, as compared to the one used in 
        // nextWordPosition(), gives us results we can use directly without having to 
        // iterate again to translate the next value into a DOM position. 
        NodeImpl *node = it.range().startContainer().handle();
        if (node->isTextNode()) {
            // The next variable contains a usable index into a text node
            pos = Position(node, next);
        }
        else {
            // If we are not in a text node, we ended on a node boundary, so the
            // range start offset should be used.
            pos = Position(node, it.range().startOffset());
        }
    }
    // Use DOWNSTREAM here so that we don't jump past words at the start of lines.
    // <rdar://problem/3765519> REGRESSION (Mail): word movement goes too far upstream at start of line
    return pos.equivalentDeepPosition().closestRenderedPosition(DOWNSTREAM);
}

static CaretPosition nextWordBoundary(const CaretPosition &c, unsigned (*searchFunction)(const QChar *, unsigned))
{
    Position pos = c.deepEquivalent();
    NodeImpl *n = pos.node();
    if (!n)
        return CaretPosition();
    DocumentImpl *d = n->getDocument();
    if (!d)
        return CaretPosition();
    NodeImpl *de = d->documentElement();
    if (!de)
        return CaretPosition();

    Range searchRange(d);
    Position start(pos.equivalentRangeCompliantPosition());
    searchRange.setStart(start.node(), start.offset());
    searchRange.setEndAfter(de);
    TextIterator it(searchRange);
    QString string;
    unsigned next = 0;
    while (!it.atEnd() && it.length() > 0) {
        // Keep asking the iterator for chunks until the findWordBoundary() function
        // returns an end value not equal to the length of the string passed to it.
        string.append(it.characters(), it.length());
        next = searchFunction(string.unicode(), string.length());
        if (next != string.length())
            break;
        it.advance();
    }
    
    if (it.atEnd() && next == string.length()) {
        Range range(it.range());
        pos = Position(range.startContainer().handle(), range.startOffset());
    }
    else if (!it.atEnd() && it.length() == 0) {
        // Got a zero-length chunk.
        // This means we have hit a replaced element.
        // Make a check to see if the position should be before or after the replaced element
        // by performing an additional check with a modified string which uses an "X" 
        // character to stand in for the replaced element.
        QChar chars[2];
        chars[0] = ' ';
        chars[1] = 'X';
        string.append(chars, 2);
        unsigned pastImage = searchFunction(string.unicode(), string.length());
        Range range(it.range());
        if (next != pastImage)
            pos = Position(range.endContainer().handle(), range.endOffset());
        else
            pos = Position(range.startContainer().handle(), range.startOffset());
    }
    else if (next != 0) {
        // Use the character iterator to translate the next value into a DOM position.
        CharacterIterator charIt(searchRange);
        charIt.advance(next - 1);
        pos = Position(charIt.range().endContainer().handle(), charIt.range().endOffset());
    }
    return pos.equivalentDeepPosition().closestRenderedPosition(UPSTREAM);
}

static unsigned startWordBoundary(const QChar *characters, unsigned length)
{
    int start, end;
    findWordBoundary(characters, length, length, &start, &end);
    return start;
}

CaretPosition startOfWord(const CaretPosition &c, EWordSide side)
{
    CaretPosition p = c;
    if (side == RightWordIfOnBoundary) {
        p = c.next();
        if (p.isNull())
            return c;
    }
    return previousWordBoundary(p, startWordBoundary);
}

static unsigned endWordBoundary(const QChar *characters, unsigned length)
{
    int start, end;
    findWordBoundary(characters, length, 0, &start, &end);
    return end;
}

CaretPosition endOfWord(const CaretPosition &c, EWordSide side)
{
    CaretPosition p = c;
    if (side == LeftWordIfOnBoundary) {
        p = c.previous();
        if (p.isNull())
            return c;
    }
    return nextWordBoundary(p, endWordBoundary);
}

static unsigned previousWordPositionBoundary(const QChar *characters, unsigned length)
{
    return nextWordFromIndex(characters, length, length, false);
}

CaretPosition previousWordPosition(const CaretPosition &c)
{
    return previousWordBoundary(c, previousWordPositionBoundary);
}

static unsigned nextWordPositionBoundary(const QChar *characters, unsigned length)
{
    return nextWordFromIndex(characters, length, 0, true);
}

CaretPosition nextWordPosition(const CaretPosition &c)
{
    return nextWordBoundary(c, nextWordPositionBoundary);
}

CaretPosition previousLinePosition(const CaretPosition &c, int x)
{
    return c.deepEquivalent().previousLinePosition(x);
}

CaretPosition nextLinePosition(const CaretPosition &c, int x)
{
    return c.deepEquivalent().nextLinePosition(x);
}

CaretPosition startOfParagraph(const CaretPosition &c)
{
    Position p = c.deepEquivalent();
    NodeImpl *startNode = p.node();
    if (!startNode)
        return CaretPosition();

    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();

    NodeImpl *node = startNode;
    long offset = p.offset();

    for (NodeImpl *n = startNode; n; n = n->traversePreviousNodePostOrder(startBlock)) {
        RenderObject *r = n->renderer();
        if (!r)
            continue;
        RenderStyle *style = r->style();
        if (style->visibility() != VISIBLE)
            continue;
        if (r->isBR() || r->isBlockFlow())
            break;
        if (r->isText()) {
            if (style->whiteSpace() == PRE) {
                QChar *text = static_cast<RenderText *>(r)->text();
                long i = static_cast<RenderText *>(r)->length();
                long o = offset;
                if (n == startNode && o < i)
                    i = kMax(0L, o);
                while (--i >= 0)
                    if (text[i] == '\n')
                        return CaretPosition(n, i + 1);
            }
            node = n;
            offset = 0;
        } else if (r->isReplaced()) {
            node = n;
            offset = 0;
        }
    }

    return CaretPosition(node, offset);
}

CaretPosition endOfParagraph(const CaretPosition &c, EIncludeLineBreak includeLineBreak)
{
    Position p = c.deepEquivalent();

    NodeImpl *startNode = p.node();
    if (!startNode)
        return CaretPosition();

    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();

    NodeImpl *node = startNode;
    long offset = p.offset();

    for (NodeImpl *n = startNode; n; n = n->traverseNextNode(startBlock)) {
        RenderObject *r = n->renderer();
        if (!r)
            continue;
        RenderStyle *style = r->style();
        if (style->visibility() != VISIBLE)
            continue;
        if (r->isBR()) {
            if (includeLineBreak)
                return CaretPosition(n, 1);
            break;
        }
        if (r->isBlockFlow()) {
            if (includeLineBreak)
                return CaretPosition(n, 0);
            break;
        }
        if (r->isText()) {
            long length = static_cast<RenderText *>(r)->length();
            if (style->whiteSpace() == PRE) {
                QChar *text = static_cast<RenderText *>(r)->text();
                long o = 0;
                if (n == startNode && offset < length)
                    o = offset;
                for (long i = o; i < length; ++i)
                    if (text[i] == '\n')
                        return CaretPosition(n, i + includeLineBreak);
            }
            node = n;
            offset = length;
        } else if (r->isReplaced()) {
            node = n;
            offset = 1;
        }
    }

    return CaretPosition(node, offset);
}

bool inSameParagraph(const CaretPosition &a, const CaretPosition &b)
{
    return a == b || startOfParagraph(a) == startOfParagraph(b);
}

CaretPosition previousParagraphPosition(const CaretPosition &p, int x)
{
    CaretPosition pos = p;
    do {
        CaretPosition n = previousLinePosition(pos, x);
        if (n.isNull() || n == pos)
            return p;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

CaretPosition nextParagraphPosition(const CaretPosition &p, int x)
{
    CaretPosition pos = p;
    do {
        CaretPosition n = nextLinePosition(pos, x);
        if (n.isNull() || n == pos)
            return p;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

}  // namespace DOM
