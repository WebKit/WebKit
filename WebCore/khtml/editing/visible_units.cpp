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

#include "visible_units.h"

#include <qstring.h>

#include "htmltags.h"
#include "misc/helper.h"
#include "rendering/render_text.h"
#include "visible_position.h"
#include "visible_text.h"
#include "xml/dom_docimpl.h"

using DOM::DocumentImpl;
using DOM::NodeImpl;
using DOM::Position;
using DOM::Range;

namespace khtml {

static VisiblePosition previousBoundary(const VisiblePosition &c, unsigned (*searchFunction)(const QChar *, unsigned))
{
    Position pos = c.deepEquivalent();
    NodeImpl *n = pos.node();
    if (!n)
        return VisiblePosition();
    DocumentImpl *d = n->getDocument();
    if (!d)
        return VisiblePosition();
    NodeImpl *de = d->documentElement();
    if (!de)
        return VisiblePosition();

    Range searchRange(d);
    searchRange.setStartBefore(de);
    Position end(pos.equivalentRangeCompliantPosition());
    searchRange.setEnd(end.node(), end.offset());
    SimplifiedBackwardsTextIterator it(searchRange);
    QString string;
    unsigned next = 0;
    while (!it.atEnd() && it.length() > 0) {
        // iterate to get chunks until the searchFunction returns a non-zero value.
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
        if (node->isTextNode() || (node->renderer() && node->renderer()->isBR())) {
            // The next variable contains a usable index into a text node
            pos = Position(node, next);
        }
        else {
            // If we are not in a text node, we ended on a node boundary, so the
            // range start offset should be used.
            pos = Position(node, it.range().startOffset());
        }
    }

    return VisiblePosition(pos, UPSTREAM);
}

static VisiblePosition nextBoundary(const VisiblePosition &c, unsigned (*searchFunction)(const QChar *, unsigned))
{
    Position pos = c.deepEquivalent();
    NodeImpl *n = pos.node();
    if (!n)
        return VisiblePosition();
    DocumentImpl *d = n->getDocument();
    if (!d)
        return VisiblePosition();
    NodeImpl *de = d->documentElement();
    if (!de)
        return VisiblePosition();

    Range searchRange(d);
    Position start(pos.equivalentRangeCompliantPosition());
    searchRange.setStart(start.node(), start.offset());
    searchRange.setEndAfter(de);
    TextIterator it(searchRange, RUNFINDER);
    QString string;
    unsigned next = 0;
    while (!it.atEnd() && it.length() > 0) {
        // Keep asking the iterator for chunks until the search function
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
    return VisiblePosition(pos, UPSTREAM);
}

// ---------

static unsigned startWordBoundary(const QChar *characters, unsigned length)
{
    int start, end;
    findWordBoundary(characters, length, length, &start, &end);
    return start;
}

VisiblePosition startOfWord(const VisiblePosition &c, EWordSide side)
{
    VisiblePosition p = c;
    if (side == RightWordIfOnBoundary) {
        p = c.next();
        if (p.isNull())
            return c;
    }
    return previousBoundary(p, startWordBoundary);
}

static unsigned endWordBoundary(const QChar *characters, unsigned length)
{
    int start, end;
    findWordBoundary(characters, length, 0, &start, &end);
    return end;
}

VisiblePosition endOfWord(const VisiblePosition &c, EWordSide side)
{
    VisiblePosition p = c;
    if (side == LeftWordIfOnBoundary) {
        p = c.previous();
        if (p.isNull())
            return c;
    }
    return nextBoundary(p, endWordBoundary);
}

static unsigned previousWordPositionBoundary(const QChar *characters, unsigned length)
{
    return nextWordFromIndex(characters, length, length, false);
}

VisiblePosition previousWordPosition(const VisiblePosition &c)
{
    return previousBoundary(c, previousWordPositionBoundary);
}

static unsigned nextWordPositionBoundary(const QChar *characters, unsigned length)
{
    return nextWordFromIndex(characters, length, 0, true);
}

VisiblePosition nextWordPosition(const VisiblePosition &c)
{
    return nextBoundary(c, nextWordPositionBoundary);
}

// ---------

static RootInlineBox *rootBoxForLine(const VisiblePosition &c, EAffinity affinity)
{
    Position p = c.deepEquivalent();
    NodeImpl *node = p.node();
    if (!node)
        return 0;

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return 0;
    
    InlineBox *box = renderer->inlineBox(p.offset(), affinity);
    if (!box)
        return 0;
    
    return box->root();
}

VisiblePosition startOfLine(const VisiblePosition &c, EAffinity affinity)
{
    RootInlineBox *rootBox = rootBoxForLine(c, affinity);
    if (!rootBox)
        return VisiblePosition();
    
    InlineBox *startBox = rootBox->firstChild();
    if (!startBox)
        return VisiblePosition();

    RenderObject *startRenderer = startBox->object();
    if (!startRenderer)
        return VisiblePosition();

    NodeImpl *startNode = startRenderer->element();
    if (!startNode)
        return VisiblePosition();

    long startOffset = 0;
    if (startBox->isInlineTextBox()) {
        InlineTextBox *startTextBox = static_cast<InlineTextBox *>(startBox);
        startOffset = startTextBox->m_start;
    }
    return VisiblePosition(startNode, startOffset);
}

VisiblePosition endOfLine(const VisiblePosition &c, EAffinity affinity, EIncludeLineBreak includeLineBreak)
{
    // FIXME: Need to implement the "include line break" version.
    assert(includeLineBreak == DoNotIncludeLineBreak);

    RootInlineBox *rootBox = rootBoxForLine(c, affinity);
    if (!rootBox)
        return VisiblePosition();
    
    InlineBox *endBox = rootBox->lastLeafChild();
    if (!endBox)
        return VisiblePosition();

    RenderObject *endRenderer = endBox->object();
    if (!endRenderer)
        return VisiblePosition();

    NodeImpl *endNode = endRenderer->element();
    if (!endNode)
        return VisiblePosition();

    long endOffset = 1;
    if (endNode->id() == ID_BR) {
        endOffset = 0;
    }
    else if (endBox->isInlineTextBox()) {
        InlineTextBox *endTextBox = static_cast<InlineTextBox *>(endBox);
        endOffset = endTextBox->m_start + endTextBox->m_len;
    }
    return VisiblePosition(endNode, endOffset);
}

bool inSameLine(const VisiblePosition &a, EAffinity aa, const VisiblePosition &b, EAffinity ab)
{
    return a.isNotNull() && startOfLine(a, aa) == startOfLine(b, ab);
}

bool isStartOfLine(const VisiblePosition &p, EAffinity affinity)
{
    return p.isNotNull() && p == startOfLine(p, affinity);
}

bool isEndOfLine(const VisiblePosition &p, EAffinity affinity)
{
    return p.isNotNull() && p == endOfLine(p, affinity, DoNotIncludeLineBreak);
}

VisiblePosition previousLinePosition(const VisiblePosition &c, EAffinity affinity, int x)
{
    Position p = affinity == UPSTREAM ? c.deepEquivalent() : c.downstreamDeepEquivalent();
    return VisiblePosition(p.previousLinePosition(x, affinity));
}

VisiblePosition nextLinePosition(const VisiblePosition &c, EAffinity affinity, int x)
{
    Position p = affinity == UPSTREAM ? c.deepEquivalent() : c.downstreamDeepEquivalent();
    return VisiblePosition(p.nextLinePosition(x, affinity));
}

// ---------

static unsigned startSentenceBoundary(const QChar *characters, unsigned length)
{
    int start, end;
    findSentenceBoundary(characters, length, length, &start, &end);
    return start;
}

VisiblePosition startOfSentence(const VisiblePosition &c)
{
    return previousBoundary(c, startSentenceBoundary);
}

static unsigned endSentenceBoundary(const QChar *characters, unsigned length)
{
    int start, end;
    findSentenceBoundary(characters, length, 0, &start, &end);
    return end;
}

VisiblePosition endOfSentence(const VisiblePosition &c)
{
    return nextBoundary(c, endSentenceBoundary);
}

static unsigned previousSentencePositionBoundary(const QChar *characters, unsigned length)
{
    return nextSentenceFromIndex(characters, length, length, false);
}

VisiblePosition previousSentencePosition(const VisiblePosition &c, EAffinity, int x)
{
    return previousBoundary(c, previousSentencePositionBoundary);
}

static unsigned nextSentencePositionBoundary(const QChar *characters, unsigned length)
{
    return nextSentenceFromIndex(characters, length, 0, true);
}

VisiblePosition nextSentencePosition(const VisiblePosition &c, EAffinity, int x)
{
    return nextBoundary(c, nextSentencePositionBoundary);
}

VisiblePosition startOfParagraph(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    NodeImpl *startNode = p.node();
    if (!startNode)
        return VisiblePosition();

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
                        return VisiblePosition(n, i + 1);
            }
            node = n;
            offset = 0;
        } else if (r->isReplaced()) {
            node = n;
            offset = 0;
        }
    }

    return VisiblePosition(node, offset);
}

VisiblePosition endOfParagraph(const VisiblePosition &c, EIncludeLineBreak includeLineBreak)
{
    Position p = c.deepEquivalent();

    NodeImpl *startNode = p.node();
    if (!startNode)
        return VisiblePosition();

    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    NodeImpl *stayInsideBlock = includeLineBreak ? 0 : startBlock;
    
    NodeImpl *node = startNode;
    long offset = p.offset();

    for (NodeImpl *n = startNode; n; n = n->traverseNextNode(stayInsideBlock)) {
        RenderObject *r = n->renderer();
        if (!r)
            continue;
        RenderStyle *style = r->style();
        if (style->visibility() != VISIBLE)
            continue;
        if (r->isBR()) {
            if (includeLineBreak)
                return VisiblePosition(n, 1);
            break;
        }
        if (r->isBlockFlow()) {
            if (includeLineBreak)
                return VisiblePosition(n, 0);
            break;
        }
        if (r->isText()) {
            if (includeLineBreak && !n->isAncestor(startBlock))
                return VisiblePosition(n, 0);
            long length = static_cast<RenderText *>(r)->length();
            if (style->whiteSpace() == PRE) {
                QChar *text = static_cast<RenderText *>(r)->text();
                long o = 0;
                if (n == startNode && offset < length)
                    o = offset;
                for (long i = o; i < length; ++i)
                    if (text[i] == '\n')
                        return VisiblePosition(n, i + includeLineBreak);
            }
            node = n;
            offset = length;
        } else if (r->isReplaced()) {
            node = n;
            offset = 1;
            if (includeLineBreak && !n->isAncestor(startBlock))
                break;
        }
    }

    return VisiblePosition(node, offset);
}

bool inSameParagraph(const VisiblePosition &a, const VisiblePosition &b)
{
    return a.isNotNull() && startOfParagraph(a) == startOfParagraph(b);
}

bool isStartOfParagraph(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos == startOfParagraph(pos);
}

bool isEndOfParagraph(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos == endOfParagraph(pos, DoNotIncludeLineBreak);
}

VisiblePosition previousParagraphPosition(const VisiblePosition &p, EAffinity affinity, int x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = previousLinePosition(pos, affinity, x);
        if (n.isNull() || n == pos)
            return p;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

VisiblePosition nextParagraphPosition(const VisiblePosition &p, EAffinity affinity, int x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = nextLinePosition(pos, affinity, x);
        if (n.isNull() || n == pos)
            return p;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

// ---------

// written, but not yet tested
VisiblePosition startOfBlock(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    NodeImpl *startNode = p.node();
    if (!startNode)
        return VisiblePosition();

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
        if (r->isBlockFlow())
            break;
        if (r->isText()) {
            node = n;
            offset = 0;
        } else if (r->isReplaced()) {
            node = n;
            offset = 0;
        }
    }

    return VisiblePosition(node, offset);
}

// written, but not yet tested
VisiblePosition endOfBlock(const VisiblePosition &c, EIncludeLineBreak includeLineBreak)
{
    Position p = c.deepEquivalent();

    NodeImpl *startNode = p.node();
    if (!startNode)
        return VisiblePosition();

    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    NodeImpl *stayInsideBlock = includeLineBreak ? 0 : startBlock;
    
    NodeImpl *node = startNode;
    long offset = p.offset();

    for (NodeImpl *n = startNode; n; n = n->traverseNextNode(stayInsideBlock)) {
        RenderObject *r = n->renderer();
        if (!r)
            continue;
        RenderStyle *style = r->style();
        if (style->visibility() != VISIBLE)
            continue;
        if (r->isBlockFlow()) {
            if (includeLineBreak)
                return VisiblePosition(n, 0);
            break;
        }
        if (r->isText()) {
            if (includeLineBreak && !n->isAncestor(startBlock))
                return VisiblePosition(n, 0);
            node = n;
            offset = static_cast<RenderText *>(r)->length();
        } else if (r->isReplaced()) {
            node = n;
            offset = 1;
            if (includeLineBreak && !n->isAncestor(startBlock))
                break;
        }
    }

    return VisiblePosition(node, offset);
}

bool inSameBlock(const VisiblePosition &a, const VisiblePosition &b)
{
    return a.isNotNull() && startOfBlock(a) == startOfBlock(b);
}

bool isStartOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos == startOfBlock(pos);
}

bool isEndOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos == endOfBlock(pos, DoNotIncludeLineBreak);
}

// ---------

VisiblePosition startOfDocument(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    NodeImpl *node = p.node();
    if (!node)
        return VisiblePosition();

    DocumentImpl *doc = node->getDocument();
    if (!doc)
        return VisiblePosition();

    return VisiblePosition(doc->documentElement(), 0);
}

VisiblePosition endOfDocument(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    NodeImpl *node = p.node();
    if (!node)
        return VisiblePosition();

    DocumentImpl *doc = node->getDocument();
    if (!doc)
        return VisiblePosition();

    NodeImpl *docElem = doc->documentElement();
    if (!node)
        return VisiblePosition();

    return VisiblePosition(docElem, docElem->childNodeCount());
}

bool inSameDocument(const VisiblePosition &a, const VisiblePosition &b)
{
    Position ap = a.deepEquivalent();
    NodeImpl *an = ap.node();
    if (!an)
        return false;
    Position bp = b.deepEquivalent();
    NodeImpl *bn = bp.node();
    if (an == bn)
        return true;

    return an->getDocument() == bn->getDocument();
}

bool isStartOfDocument(const VisiblePosition &p)
{
    return p.isNotNull() && p.previous().isNull();
}

bool isEndOfDocument(const VisiblePosition &p)
{
    return p.isNotNull() && p.next().isNull();
}

} // namespace khtml
