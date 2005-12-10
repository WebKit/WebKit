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
#include "visible_units.h"

#include <qstring.h>

#include "htmlnames.h"
#include "misc/helper.h"
#include "rendering/render_text.h"
#include "rendering/render_block.h"
#include "visible_position.h"
#include "visible_text.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_elementimpl.h"

using namespace DOM::HTMLNames;

using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::NodeImpl;
using DOM::Position;
using DOM::RangeImpl;

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
    NodeImpl *boundary = n->enclosingBlockFlowElement();
    if (!boundary)
        return VisiblePosition();
    bool isContentEditable = boundary->isContentEditable();
    while (boundary && boundary != de && boundary->parentNode() && isContentEditable == boundary->parentNode()->isContentEditable()) {
        boundary = boundary->parentNode();
    }

    RefPtr<RangeImpl> searchRange(d->createRange());
    int exception = 0;
    searchRange->setStartBefore(boundary, exception);
    Position end(pos.equivalentRangeCompliantPosition());
    searchRange->setEnd(end.node(), end.offset(), exception);
    SimplifiedBackwardsTextIterator it(searchRange.get());
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
        RefPtr<RangeImpl> range(it.range());
        pos = Position(range->startContainer(exception), range->startOffset(exception));
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
        RefPtr<RangeImpl> range(it.range());
        if (pastImage == 0)
            pos = Position(range->startContainer(exception), range->startOffset(exception));
        else
            pos = Position(range->endContainer(exception), range->endOffset(exception));
    }
    else if (next != 0) {
        // The simpler iterator used in this function, as compared to the one used in 
        // nextWordPosition(), gives us results we can use directly without having to 
        // iterate again to translate the next value into a DOM position. 
        NodeImpl *node = it.range()->startContainer(exception);
        if (node->isTextNode() || (node->renderer() && node->renderer()->isBR())) {
            // The next variable contains a usable index into a text node
            pos = Position(node, next);
        }
        else {
            // If we are not in a text node, we ended on a node boundary, so the
            // range start offset should be used.
            pos = Position(node, it.range()->startOffset(exception));
        }
    }

    return VisiblePosition(pos, DOWNSTREAM);
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
    NodeImpl *boundary = n->enclosingBlockFlowElement();
    if (!boundary)
        return VisiblePosition();
    bool isContentEditable = boundary->isContentEditable();
    while (boundary && boundary != de && boundary->parentNode() && isContentEditable == boundary->parentNode()->isContentEditable()) {
        boundary = boundary->parentNode();
    }

    RefPtr<RangeImpl> searchRange(d->createRange());
    Position start(pos.equivalentRangeCompliantPosition());
    int exception = 0;
    searchRange->setStart(start.node(), start.offset(), exception);
    searchRange->setEndAfter(boundary, exception);
    TextIterator it(searchRange.get(), RUNFINDER);
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
        RefPtr<RangeImpl> range(it.range());
        int exception = 0;
        pos = Position(range->startContainer(exception), range->startOffset(exception));
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
        RefPtr<RangeImpl> range(it.range());
        int exception = 0;
        if (next != pastImage)
            pos = Position(range->endContainer(exception), range->endOffset(exception));
        else
            pos = Position(range->startContainer(exception), range->startOffset(exception));
    }
    else if (next != 0) {
        // Use the character iterator to translate the next value into a DOM position.
        CharacterIterator charIt(searchRange.get());
        charIt.advance(next - 1);
        pos = Position(charIt.range()->endContainer(exception), charIt.range()->endOffset(exception));
    }

    // generate VisiblePosition, use UPSTREAM affinity if possible
    return VisiblePosition(pos, VP_UPSTREAM_IF_POSSIBLE);
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
        // at paragraph end, the startofWord is the current position
        if (isEndOfParagraph(c))
            return c;
        
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
        if (isStartOfParagraph(c))
            return c;
            
        p = c.previous();
        if (p.isNull())
            return c;
    } else {
        // at paragraph end, the endOfWord is the start of next paragraph
        if (isEndOfParagraph(c)) {
            p = c.next();
            return p.isNotNull() ? p : c;
        }
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

static RootInlineBox *rootBoxForLine(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    NodeImpl *node = p.node();
    if (!node)
        return 0;

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return 0;
    
    InlineBox *box = renderer->inlineBox(p.offset(), c.affinity());
    if (!box)
        return 0;
    
    return box->root();
}

VisiblePosition startOfLine(const VisiblePosition &c)
{
    RootInlineBox *rootBox = rootBoxForLine(c);
    if (!rootBox)
        return VisiblePosition();
    
    InlineBox *startBox = rootBox->firstLeafChild();
    if (!startBox)
        return VisiblePosition();

    RenderObject *startRenderer = startBox->object();
    if (!startRenderer)
        return VisiblePosition();

    NodeImpl *startNode = startRenderer->element();
    if (!startNode)
        return VisiblePosition();

    int startOffset = 0;
    if (startBox->isInlineTextBox()) {
        InlineTextBox *startTextBox = static_cast<InlineTextBox *>(startBox);
        startOffset = startTextBox->m_start;
    }
    
    return VisiblePosition(startNode, startOffset, DOWNSTREAM);
}

VisiblePosition endOfLine(const VisiblePosition &c)
{
    RootInlineBox *rootBox = rootBoxForLine(c);
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

    int endOffset = 1;
    if (endNode->hasTagName(brTag)) {
        endOffset = 0;
    } else if (endBox->isInlineTextBox()) {
        InlineTextBox *endTextBox = static_cast<InlineTextBox *>(endBox);
        endOffset = endTextBox->m_start + endTextBox->m_len;
    }
    
    return VisiblePosition(endNode, endOffset, VP_UPSTREAM_IF_POSSIBLE);
}

bool inSameLine(const VisiblePosition &a, const VisiblePosition &b)
{
    return a.isNotNull() && startOfLine(a) == startOfLine(b);
}

bool isStartOfLine(const VisiblePosition &p)
{
    return p.isNotNull() && p == startOfLine(p);
}

bool isEndOfLine(const VisiblePosition &p)
{
    return p.isNotNull() && p == endOfLine(p);
}

VisiblePosition previousLinePosition(const VisiblePosition &c, int x)
{
    Position p = c.affinity() == UPSTREAM ? c.deepEquivalent() : c.downstreamDeepEquivalent();
    NodeImpl *node = p.node();
    if (!node || !node->getDocument())
        return VisiblePosition();
    
    node->getDocument()->updateLayout();
    
    RenderObject *renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RenderBlock *containingBlock = 0;
    RootInlineBox *root = 0;
    InlineBox *box = renderer->inlineBox(p.offset(), c.affinity());
    if (box) {
        root = box->root()->prevRootBox();
        if (root)
            containingBlock = renderer->containingBlock();
    }

    if (!root) {
        // This containing editable block does not have a previous line.
        // Need to move back to previous containing editable block in this root editable
        // block and find the last root line box in that block.
        NodeImpl *startBlock = node->enclosingBlockFlowElement();
        NodeImpl *n = node->previousEditable();
        while (n && startBlock == n->enclosingBlockFlowElement())
            n = n->previousEditable();
        while (n) {
            if (!n->inSameRootEditableElement(node))
                break;
            Position pos(n, n->caretMinOffset());
            if (pos.inRenderedContent()) {
                assert(n->renderer());
                box = n->renderer()->inlineBox(n->caretMaxOffset());
                if (box) {
                    // previous root line box found
                    root = box->root();
                    containingBlock = n->renderer()->containingBlock();
                    break;
                }

                // Work around <rdar://problem/4040763>.  Need to
                // doublecheck that pos is really on a different line,
                // because it might not be a viable VisiblePosition
                // (whereupon the next VP is returned, which could be
                // back on c's line)
                VisiblePosition c2 = VisiblePosition(pos, DOWNSTREAM);
                if (!inSameLine(c, c2))
                    return c2;
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
    return VisiblePosition(node->rootEditableElement(), 0, DOWNSTREAM);
}

VisiblePosition nextLinePosition(const VisiblePosition &c, int x)
{
    Position p = c.affinity() == UPSTREAM ? c.deepEquivalent() : c.downstreamDeepEquivalent();
    NodeImpl *node = p.node();
    if (!node || !node->getDocument())
        return VisiblePosition();
    
    node->getDocument()->updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RenderBlock *containingBlock = 0;
    RootInlineBox *root = 0;
    InlineBox *box = renderer->inlineBox(p.offset(), c.affinity());
    if (box) {
        root = box->root()->nextRootBox();
        if (root)
            containingBlock = renderer->containingBlock();
    }

    if (!root) {
        // This containing editable block does not have a next line.
        // Need to move forward to next containing editable block in this root editable
        // block and find the first root line box in that block.
        NodeImpl *startBlock = node->enclosingBlockFlowElement();
        NodeImpl *n = node->nextEditable();
        while (n && startBlock == n->enclosingBlockFlowElement())
            n = n->nextEditable();
        while (n) {
            if (!n->inSameRootEditableElement(node))
                break;
            Position pos(n, n->caretMinOffset());
            if (pos.inRenderedContent()) {
                assert(n->renderer());
                box = n->renderer()->inlineBox(n->caretMinOffset());
                if (box) {
                    // next root line box found
                    root = box->root();
                    containingBlock = n->renderer()->containingBlock();
                    break;
                }

                return VisiblePosition(pos, DOWNSTREAM);
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
    ElementImpl *rootElement = node->rootEditableElement();
    return VisiblePosition(rootElement, rootElement ? rootElement->childNodeCount() : 0, DOWNSTREAM);
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

VisiblePosition previousSentencePosition(const VisiblePosition &c, int x)
{
    return previousBoundary(c, previousSentencePositionBoundary);
}

static unsigned nextSentencePositionBoundary(const QChar *characters, unsigned length)
{
    return nextSentenceFromIndex(characters, length, 0, true);
}

VisiblePosition nextSentencePosition(const VisiblePosition &c, int x)
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
    int offset = p.offset();

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
            // FIXME: Not clear what to do with pre-wrap or pre-line here.
            if (style->whiteSpace() == PRE) {
                QChar *text = static_cast<RenderText *>(r)->text();
                int i = static_cast<RenderText *>(r)->length();
                int o = offset;
                if (n == startNode && o < i)
                    i = kMax(0, o);
                while (--i >= 0)
                    if (text[i] == '\n')
                        return VisiblePosition(n, i + 1, DOWNSTREAM);
            }
            node = n;
            offset = 0;
        } else if (r->isReplaced()) {
            node = n;
            offset = 0;
        }
    }

    return VisiblePosition(node, offset, DOWNSTREAM);
}

VisiblePosition endOfParagraph(const VisiblePosition &c)
{
    if (c.isNull())
        return VisiblePosition();

    Position p = c.deepEquivalent();
    NodeImpl *startNode = p.node();
    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    NodeImpl *stayInsideBlock = startBlock;
    
    NodeImpl *node = startNode;
    int offset = p.offset();

    for (NodeImpl *n = startNode; n; n = n->traverseNextNode(stayInsideBlock)) {
        if (n->isContentEditable() != startNode->isContentEditable())
            break;
        RenderObject *r = n->renderer();
        if (!r)
            continue;
        RenderStyle *style = r->style();
        if (style->visibility() != VISIBLE)
            continue;
            
        if (r->isBR() || r->isBlockFlow())
            break;
            
        // FIXME: We avoid returning a position where the renderer can't accept the caret.
        // We should probably do this in other cases such as startOfParagraph.
        if (r->isText() && r->caretMaxRenderedOffset() > 0) {
            int length = static_cast<RenderText *>(r)->length();
            // FIXME: Not clear what to do with pre-wrap or pre-line here.
            if (style->whiteSpace() == PRE) {
                QChar *text = static_cast<RenderText *>(r)->text();
                int o = n == startNode ? offset : 0;
                for (int i = o; i < length; ++i)
                    if (text[i] == '\n')
                        return VisiblePosition(n, i, DOWNSTREAM);
            }
            node = n;
            offset = r->caretMaxOffset();
        } else if (r->isReplaced()) {
            node = n;
            offset = 1;
        }
    }

    return VisiblePosition(node, offset, DOWNSTREAM);
}

bool inSameParagraph(const VisiblePosition &a, const VisiblePosition &b)
{
    return a.isNotNull() && startOfParagraph(a) == startOfParagraph(b);
}

bool isStartOfParagraph(const VisiblePosition &pos)
{
    return pos.isNotNull() && isEqualIgnoringAffinity(pos, startOfParagraph(pos));
}

bool isEndOfParagraph(const VisiblePosition &pos)
{
    return pos.isNotNull() && isEqualIgnoringAffinity(pos, endOfParagraph(pos));
}

VisiblePosition previousParagraphPosition(const VisiblePosition &p, int x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = previousLinePosition(pos, x);
        if (n.isNull() || n == pos) {
            return p;
        }
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

VisiblePosition nextParagraphPosition(const VisiblePosition &p, int x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = nextLinePosition(pos, x);
        if (n.isNull() || n == pos) {
            return p;
        }
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

// ---------

VisiblePosition startOfBlock(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    NodeImpl *startNode = p.node();
    if (!startNode)
        return VisiblePosition();
    return VisiblePosition(Position(startNode->enclosingBlockFlowElement(), 0), DOWNSTREAM);
}

VisiblePosition endOfBlock(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();

    NodeImpl *startNode = p.node();
    if (!startNode)
        return VisiblePosition();

    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    
    return VisiblePosition(startBlock, startBlock->childNodeCount(), VP_DEFAULT_AFFINITY);   
}

bool inSameBlock(const VisiblePosition &a, const VisiblePosition &b)
{
    return !a.isNull() && enclosingBlockFlowElement(a) == enclosingBlockFlowElement(b);
}

bool isStartOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && isEqualIgnoringAffinity(pos, startOfBlock(pos));
}

bool isEndOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && isEqualIgnoringAffinity(pos, endOfBlock(pos));
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

    return VisiblePosition(doc->documentElement(), 0, DOWNSTREAM);
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

    return VisiblePosition(docElem, docElem->childNodeCount(), DOWNSTREAM);
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

// ---------

VisiblePosition startOfEditableContent(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    NodeImpl *node = p.node();
    if (!node)
        return VisiblePosition();

    return VisiblePosition(node->rootEditableElement(), 0, DOWNSTREAM);
}

VisiblePosition endOfEditableContent(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    NodeImpl *node = p.node();
    if (!node)
        return VisiblePosition();

    node = node->rootEditableElement();
    if (!node)
        return VisiblePosition();

    return VisiblePosition(node, node->childNodeCount(), DOWNSTREAM);
}

bool inSameEditableContent(const VisiblePosition &a, const VisiblePosition &b)
{
    Position ap = a.deepEquivalent();
    NodeImpl *an = ap.node();
    if (!an)
        return false;
        
    Position bp = b.deepEquivalent();
    NodeImpl *bn = bp.node();
    if (!bn)
        return false;
    
    if (!an->isContentEditable() || !bn->isContentEditable())
        return false;

    return an->rootEditableElement() == bn->rootEditableElement();
}

bool isStartOfEditableContent(const VisiblePosition &p)
{
    return !inSameEditableContent(p, p.previous());
}

bool isEndOfEditableContent(const VisiblePosition &p)
{
    return !inSameEditableContent(p, p.next());
}

} // namespace khtml
