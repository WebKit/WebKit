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

#include "Document.h"
#include "Element.h"
#include "HTMLNames.h"
#include "RenderBlock.h"
#include "RenderLayer.h"
#include "TextBoundaries.h"
#include "TextBreakIterator.h"
#include "TextIterator.h"
#include "htmlediting.h"

namespace WebCore {

using namespace HTMLNames;

static VisiblePosition previousBoundary(const VisiblePosition &c, unsigned (*searchFunction)(const UChar *, unsigned))
{
    Position pos = c.deepEquivalent();
    Node *n = pos.node();
    if (!n)
        return VisiblePosition();
    Document *d = n->document();
    Node *de = d->documentElement();
    if (!de)
        return VisiblePosition();
    Node *boundary = n->enclosingBlockFlowElement();
    if (!boundary)
        return VisiblePosition();
    bool isContentEditable = boundary->isContentEditable();
    while (boundary && boundary != de && boundary->parentNode() && isContentEditable == boundary->parentNode()->isContentEditable())
        boundary = boundary->parentNode();

    Position start = rangeCompliantEquivalent(Position(boundary, 0));
    Position end = rangeCompliantEquivalent(pos);
    RefPtr<Range> searchRange = new Range(d);
    
    int exception = 0;
    searchRange->setStart(start.node(), start.offset(), exception);
    searchRange->setEnd(end.node(), end.offset(), exception);
    
    ASSERT(!exception);
    if (exception)
        return VisiblePosition();
        
    SimplifiedBackwardsTextIterator it(searchRange.get());
    Vector<UChar, 1024> string;
    unsigned next = 0;
    bool inTextSecurityMode = start.node() && start.node()->renderer() && start.node()->renderer()->style()->textSecurity() != TSNONE;
    while (!it.atEnd()) {
        // iterate to get chunks until the searchFunction returns a non-zero value.
        if (!inTextSecurityMode) 
            string.prepend(it.characters(), it.length());
        else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            String iteratorString(it.characters(), it.length());
            iteratorString = iteratorString.impl()->secure('x');
            string.prepend(iteratorString.characters(), iteratorString.length());
        }
        
        next = searchFunction(string.data(), string.size());
        if (next != 0)
            break;
        it.advance();
    }
    
    if (it.atEnd() && next == 0) {
        pos = it.range()->startPosition();
    } else if (next != 0) {
        Node *node = it.range()->startContainer(exception);
        if (node->isTextNode() || (node->renderer() && node->renderer()->isBR()))
            // The next variable contains a usable index into a text node
            pos = Position(node, next);
        else {
            // Use the end of the found range, the start is not guaranteed to
            // be correct.
            Position end = it.range()->endPosition();
            VisiblePosition boundary(end);
            unsigned i = it.length() - next;
            while (i--)
                boundary = boundary.previous();
            return boundary;
        }
    }

    return VisiblePosition(pos, DOWNSTREAM);
}

static VisiblePosition nextBoundary(const VisiblePosition &c, unsigned (*searchFunction)(const UChar *, unsigned))
{
    Position pos = c.deepEquivalent();
    Node *n = pos.node();
    if (!n)
        return VisiblePosition();
    Document *d = n->document();
    Node *de = d->documentElement();
    if (!de)
        return VisiblePosition();
    Node *boundary = n->enclosingBlockFlowElement();
    if (!boundary)
        return VisiblePosition();
    bool isContentEditable = boundary->isContentEditable();
    while (boundary && boundary != de && boundary->parentNode() && isContentEditable == boundary->parentNode()->isContentEditable())
        boundary = boundary->parentNode();

    RefPtr<Range> searchRange(d->createRange());
    Position start(rangeCompliantEquivalent(pos));
    ExceptionCode ec = 0;
    searchRange->selectNodeContents(boundary, ec);
    searchRange->setStart(start.node(), start.offset(), ec);
    TextIterator it(searchRange.get(), true);
    Vector<UChar, 1024> string;
    unsigned next = 0;
    bool inTextSecurityMode = start.node() && start.node()->renderer() && start.node()->renderer()->style()->textSecurity() != TSNONE;
    while (!it.atEnd()) {
        // Keep asking the iterator for chunks until the search function
        // returns an end value not equal to the length of the string passed to it.
        if (!inTextSecurityMode)
            string.append(it.characters(), it.length());
        else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            String iteratorString(it.characters(), it.length());
            iteratorString = iteratorString.impl()->secure('x');
            string.append(iteratorString.characters(), iteratorString.length());
        }

        next = searchFunction(string.data(), string.size());
        if (next != string.size())
            break;
        it.advance();
    }
    
    if (it.atEnd() && next == string.size()) {
        pos = it.range()->startPosition();
    } else if (next != 0) {
        // Use the character iterator to translate the next value into a DOM position.
        CharacterIterator charIt(searchRange.get(), true);
        charIt.advance(next - 1);
        pos = charIt.range()->endPosition();
        
        // FIXME: workaround for collapsed range (where only start position is correct) emitted for some emitted newlines (see rdar://5192593)
        VisiblePosition visPos = VisiblePosition(pos);
        if (visPos == VisiblePosition(charIt.range()->startPosition()))
            pos = visPos.next(true).deepEquivalent();
    }

    // generate VisiblePosition, use UPSTREAM affinity if possible
    return VisiblePosition(pos, VP_UPSTREAM_IF_POSSIBLE);
}

// ---------

static unsigned startWordBoundary(const UChar* characters, unsigned length)
{
    int start, end;
    findWordBoundary(characters, length, length, &start, &end);
    return start;
}

VisiblePosition startOfWord(const VisiblePosition &c, EWordSide side)
{
    // FIXME: This returns a null VP for c at the start of the document
    // and side == LeftWordIfOnBoundary
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

static unsigned endWordBoundary(const UChar* characters, unsigned length)
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
    } else if (isEndOfParagraph(c))
        return c;
    
    return nextBoundary(p, endWordBoundary);
}

static unsigned previousWordPositionBoundary(const UChar* characters, unsigned length)
{
    return findNextWordFromIndex(characters, length, length, false);
}

VisiblePosition previousWordPosition(const VisiblePosition &c)
{
    VisiblePosition prev = previousBoundary(c, previousWordPositionBoundary);
    return c.honorEditableBoundaryAtOrAfter(prev);
}

static unsigned nextWordPositionBoundary(const UChar* characters, unsigned length)
{
    return findNextWordFromIndex(characters, length, 0, true);
}

VisiblePosition nextWordPosition(const VisiblePosition &c)
{
    VisiblePosition next = nextBoundary(c, nextWordPositionBoundary);    
    return c.honorEditableBoundaryAtOrBefore(next);
}

// ---------

static RootInlineBox *rootBoxForLine(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    Node *node = p.node();
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

static VisiblePosition positionAvoidingFirstPositionInTable(const VisiblePosition& c)
{
    // return table offset 0 instead of the first VisiblePosition inside the table
    VisiblePosition previous = c.previous();
    if (isLastPositionBeforeTable(previous))
        return previous;
    
    return c;
}

static VisiblePosition startPositionForLine(const VisiblePosition& c)
{
    if (c.isNull())
        return VisiblePosition();
        
    RootInlineBox *rootBox = rootBoxForLine(c);
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        Position p = c.deepEquivalent();
        if (p.node()->renderer() && p.node()->renderer()->isRenderBlock() && p.offset() == 0)
            return positionAvoidingFirstPositionInTable(c);
        
        return VisiblePosition();
    }
    
    // Generated content (e.g. list markers and CSS :before and :after
    // pseudoelements) have no corresponding DOM element, and so cannot be
    // represented by a VisiblePosition.  Use whatever follows instead.
    InlineBox *startBox = rootBox->firstLeafChild();
    Node *startNode;
    while (1) {
        if (!startBox)
            return VisiblePosition();

        RenderObject *startRenderer = startBox->object();
        if (!startRenderer)
            return VisiblePosition();

        startNode = startRenderer->element();
        if (startNode)
            break;
        
        startBox = startBox->nextLeafChild();
    }
    
    int startOffset = 0;
    if (startBox->isInlineTextBox()) {
        InlineTextBox *startTextBox = static_cast<InlineTextBox *>(startBox);
        startOffset = startTextBox->m_start;
    }
  
    VisiblePosition visPos = VisiblePosition(startNode, startOffset, DOWNSTREAM);
    return positionAvoidingFirstPositionInTable(visPos);
}

VisiblePosition startOfLine(const VisiblePosition& c)
{
    VisiblePosition visPos = startPositionForLine(c);
    
    if (visPos.isNotNull()) {
        // Make sure the start of line is not greater than the given input position.  Else use the previous position to 
        // obtain start of line.  This condition happens when the input position is before the space character at the end 
        // of a soft-wrapped non-editable line. In this scenario, startPositionForLine would incorrectly hand back a position
        // greater than the input position.  This fix is to account for the discrepancy between lines with webkit-line-break:after-white-space 
        // style versus lines without that style, which would break before a space by default. 
        Position p = visPos.deepEquivalent();
        if (p.offset() > c.deepEquivalent().offset() && p.node()->isSameNode(c.deepEquivalent().node())) {
            visPos = c.previous();
            if (visPos.isNull())
                return VisiblePosition();
            visPos = startPositionForLine(visPos);
        }
    }

    return c.honorEditableBoundaryAtOrAfter(visPos);
}

static VisiblePosition endPositionForLine(const VisiblePosition& c)
{
    if (c.isNull())
        return VisiblePosition();
        
    RootInlineBox *rootBox = rootBoxForLine(c);
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        Position p = c.deepEquivalent();
        if (p.node()->renderer() && p.node()->renderer()->isRenderBlock() && p.offset() == 0)
            return c;
        return VisiblePosition();
    }
    
    // Generated content (e.g. list markers and CSS :before and :after
    // pseudoelements) have no corresponding DOM element, and so cannot be
    // represented by a VisiblePosition.  Use whatever precedes instead.
    Node *endNode;
    InlineBox *endBox = rootBox->lastLeafChild();
    while (1) {
        if (!endBox)
            return VisiblePosition();

        RenderObject *endRenderer = endBox->object();
        if (!endRenderer)
            return VisiblePosition();

        endNode = endRenderer->element();
        if (endNode)
            break;
        
        endBox = endBox->prevLeafChild();
    }
    
    int endOffset = 1;
    if (endNode->hasTagName(brTag)) {
        endOffset = 0;
    } else if (endBox->isInlineTextBox()) {
        InlineTextBox *endTextBox = static_cast<InlineTextBox *>(endBox);
        endOffset = endTextBox->m_start;
        if (!endTextBox->isLineBreak())
            endOffset += endTextBox->m_len;
    }
    
    return VisiblePosition(endNode, endOffset, VP_UPSTREAM_IF_POSSIBLE);
}

VisiblePosition endOfLine(const VisiblePosition& c)
{
    VisiblePosition visPos = endPositionForLine(c);
    
    // Make sure the end of line is at the same line as the given input position.  Else use the previous position to 
    // obtain end of line.  This condition happens when the input position is before the space character at the end 
    // of a soft-wrapped non-editable line. In this scenario, endPositionForLine would incorrectly hand back a position
    // in the next line instead. This fix is to account for the discrepancy between lines with webkit-line-break:after-white-space style
    // versus lines without that style, which would break before a space by default. 
    if (!inSameLine(c, visPos)) {
        visPos = c.previous();
        if (visPos.isNull())
            return VisiblePosition();
        visPos = endPositionForLine(visPos);
    }
    
    return c.honorEditableBoundaryAtOrBefore(visPos);
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

VisiblePosition previousLinePosition(const VisiblePosition &visiblePosition, int x)
{
    Position p = visiblePosition.deepEquivalent();
    Node *node = p.node();
    Node* highestRoot = highestEditableRoot(p);
    if (!node)
        return VisiblePosition();
    
    node->document()->updateLayoutIgnorePendingStylesheets();
    
    RenderObject *renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RenderBlock *containingBlock = 0;
    RootInlineBox *root = 0;
    InlineBox *box = renderer->inlineBox(p.offset(), visiblePosition.affinity());
    if (box) {
        root = box->root()->prevRootBox();
        if (root)
            containingBlock = renderer->containingBlock();
    }

    if (!root) {
        // This containing editable block does not have a previous line.
        // Need to move back to previous containing editable block in this root editable
        // block and find the last root line box in that block.
        Node* startBlock = enclosingBlock(node);
        Node *n = node->previousEditable();
        while (n && startBlock == enclosingBlock(n))
            n = n->previousEditable();
        while (n) {
            if (highestEditableRoot(Position(n, 0)) != highestRoot)
                break;
            Position pos(n, caretMinOffset(n));
            if (pos.isCandidate()) {
                ASSERT(n->renderer());
                box = n->renderer()->inlineBox(caretMaxOffset(n));
                if (box) {
                    // previous root line box found
                    root = box->root();
                    containingBlock = n->renderer()->containingBlock();
                    break;
                }

                return VisiblePosition(pos, DOWNSTREAM);
            }
            n = n->previousEditable();
        }
    }
    
    if (root) {
        // FIXME: Can be wrong for multi-column layout.
        int absx, absy;
        containingBlock->absolutePositionForContent(absx, absy);
        if (containingBlock->hasOverflowClip())
            containingBlock->layer()->subtractScrollOffset(absx, absy);
        RenderObject *renderer = root->closestLeafChildForXPos(x - absx, isEditablePosition(p))->object();
        Node* node = renderer->element();
        if (editingIgnoresContent(node))
            return Position(node->parent(), node->nodeIndex());
        return renderer->positionForCoordinates(x - absx, root->topOverflow());
    }
    
    // Could not find a previous line. This means we must already be on the first line.
    // Move to the start of the content in this block, which effectively moves us
    // to the start of the line we're on.
    return VisiblePosition(node->rootEditableElement(), 0, DOWNSTREAM);
}

VisiblePosition nextLinePosition(const VisiblePosition &visiblePosition, int x)
{
    Position p = visiblePosition.deepEquivalent();
    Node *node = p.node();
    Node* highestRoot = highestEditableRoot(p);
    if (!node)
        return VisiblePosition();
    
    node->document()->updateLayoutIgnorePendingStylesheets();

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RenderBlock *containingBlock = 0;
    RootInlineBox *root = 0;
    InlineBox *box = renderer->inlineBox(p.offset(), visiblePosition.affinity());
    if (box) {
        root = box->root()->nextRootBox();
        if (root)
            containingBlock = renderer->containingBlock();
    }

    if (!root) {
        // This containing editable block does not have a next line.
        // Need to move forward to next containing editable block in this root editable
        // block and find the first root line box in that block.
        Node* startBlock = enclosingBlock(node);
        Node *n = node->nextEditable(p.offset());
        while (n && startBlock == enclosingBlock(n))
            n = n->nextEditable();
        while (n) {
            if (highestEditableRoot(Position(n, 0)) != highestRoot)
                break;
            Position pos(n, caretMinOffset(n));
            if (pos.isCandidate()) {
                ASSERT(n->renderer());
                box = n->renderer()->inlineBox(caretMinOffset(n));
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
        // FIXME: Can be wrong for multi-column layout.
        int absx, absy;
        containingBlock->absolutePositionForContent(absx, absy);
        if (containingBlock->hasOverflowClip())
            containingBlock->layer()->subtractScrollOffset(absx, absy);
        RenderObject *renderer = root->closestLeafChildForXPos(x - absx, isEditablePosition(p))->object();
        Node* node = renderer->element();
        if (editingIgnoresContent(node))
            return Position(node->parent(), node->nodeIndex());
        return renderer->positionForCoordinates(x - absx, root->topOverflow());
    }    

    // Could not find a next line. This means we must already be on the last line.
    // Move to the end of the content in this block, which effectively moves us
    // to the end of the line we're on.
    Element *rootElement = node->rootEditableElement();
    return VisiblePosition(rootElement, rootElement ? rootElement->childNodeCount() : 0, DOWNSTREAM);
}

// ---------

static unsigned startSentenceBoundary(const UChar* characters, unsigned length)
{
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    // FIXME: The following function can return -1; we don't handle that.
    return textBreakPreceding(iterator, length);
}

VisiblePosition startOfSentence(const VisiblePosition &c)
{
    return previousBoundary(c, startSentenceBoundary);
}

static unsigned endSentenceBoundary(const UChar* characters, unsigned length)
{
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    return textBreakNext(iterator);
}

// FIXME: This includes the space after the punctuation that marks the end of the sentence.
VisiblePosition endOfSentence(const VisiblePosition &c)
{
    return nextBoundary(c, endSentenceBoundary);
}

static unsigned previousSentencePositionBoundary(const UChar* characters, unsigned length)
{
    // FIXME: This is identical to startSentenceBoundary. I'm pretty sure that's not right.
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    // FIXME: The following function can return -1; we don't handle that.
    return textBreakPreceding(iterator, length);
}

VisiblePosition previousSentencePosition(const VisiblePosition &c)
{
    VisiblePosition prev = previousBoundary(c, previousSentencePositionBoundary);
    return c.honorEditableBoundaryAtOrAfter(prev);
}

static unsigned nextSentencePositionBoundary(const UChar* characters, unsigned length)
{
    // FIXME: This is identical to endSentenceBoundary.  This isn't right, it needs to 
    // move to the equivlant position in the following sentence.
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    return textBreakFollowing(iterator, 0);
}

VisiblePosition nextSentencePosition(const VisiblePosition &c)
{
    VisiblePosition next = nextBoundary(c, nextSentencePositionBoundary);    
    return c.honorEditableBoundaryAtOrBefore(next);
}

// FIXME: Broken for positions before/after images that aren't inline (5027702)
VisiblePosition startOfParagraph(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    Node *startNode = p.node();

    if (!startNode)
        return VisiblePosition();
    
    if (startNode->renderer()
        && ((startNode->renderer()->isTable() && !startNode->renderer()->isInline())
            || startNode->renderer()->isHR())
        && p.offset() == maxDeepOffset(startNode))
        return VisiblePosition(Position(startNode, 0));

    Node* startBlock = enclosingBlock(startNode);

    Node *node = startNode;
    int offset = p.offset();

    Node *n = startNode;
    while (n) {
        if (n->isContentEditable() != startNode->isContentEditable())
            break;
        RenderObject *r = n->renderer();
        if (!r) {
            n = n->traversePreviousNodePostOrder(startBlock);
            continue;
        }
        RenderStyle *style = r->style();
        if (style->visibility() != VISIBLE) {
            n = n->traversePreviousNodePostOrder(startBlock);
            continue;
        }
        
        if (r->isBR() || isBlock(n))
            break;
            
        if (r->isText()) {
            if (style->preserveNewline()) {
                const UChar* chars = static_cast<RenderText*>(r)->characters();
                int i = static_cast<RenderText*>(r)->textLength();
                int o = offset;
                if (n == startNode && o < i)
                    i = max(0, o);
                while (--i >= 0)
                    if (chars[i] == '\n')
                        return VisiblePosition(n, i + 1, DOWNSTREAM);
            }
            node = n;
            offset = 0;
            n = n->traversePreviousNodePostOrder(startBlock);
        } else if (editingIgnoresContent(n) || isTableElement(n)) {
            node = n;
            offset = 0;
            n = n->previousSibling() ? n->previousSibling() : n->traversePreviousNodePostOrder(startBlock);
        } else
            n = n->traversePreviousNodePostOrder(startBlock);
    }

    return VisiblePosition(node, offset, DOWNSTREAM);
}

// FIXME: Broken for positions before/after images that aren't inline (5027702)
VisiblePosition endOfParagraph(const VisiblePosition &c)
{    
    if (c.isNull())
        return VisiblePosition();

    Position p = c.deepEquivalent();
    Node* startNode = p.node();

    if (startNode->renderer()
        && ((startNode->renderer()->isTable() && !startNode->renderer()->isInline())
            || startNode->renderer()->isHR())
        && p.offset() == 0)
        return VisiblePosition(Position(startNode, maxDeepOffset(startNode)));
    
    Node* startBlock = enclosingBlock(startNode);
    Node *stayInsideBlock = startBlock;
    
    Node *node = startNode;
    int offset = p.offset();

    Node *n = startNode;
    while (n) {
        if (n->isContentEditable() != startNode->isContentEditable())
            break;
        RenderObject *r = n->renderer();
        if (!r) {
            n = n->traverseNextNode(stayInsideBlock);
            continue;
        }
        RenderStyle *style = r->style();
        if (style->visibility() != VISIBLE) {
            n = n->traverseNextNode(stayInsideBlock);
            continue;
        }
        
        if (r->isBR() || isBlock(n))
            break;
            
        // FIXME: We avoid returning a position where the renderer can't accept the caret.
        // We should probably do this in other cases such as startOfParagraph.
        if (r->isText() && r->caretMaxRenderedOffset() > 0) {
            int length = static_cast<RenderText*>(r)->textLength();
            if (style->preserveNewline()) {
                const UChar* chars = static_cast<RenderText*>(r)->characters();
                int o = n == startNode ? offset : 0;
                for (int i = o; i < length; ++i)
                    if (chars[i] == '\n')
                        return VisiblePosition(n, i, DOWNSTREAM);
            }
            node = n;
            offset = r->caretMaxOffset();
            n = n->traverseNextNode(stayInsideBlock);
        } else if (editingIgnoresContent(n) || isTableElement(n)) {
            node = n;
            offset = maxDeepOffset(n);
            n = n->traverseNextSibling(stayInsideBlock);
        } else
            n = n->traverseNextNode(stayInsideBlock);
    }

    return VisiblePosition(node, offset, DOWNSTREAM);
}

VisiblePosition startOfNextParagraph(const VisiblePosition& visiblePosition)
{
    VisiblePosition paragraphEnd(endOfParagraph(visiblePosition));
    VisiblePosition afterParagraphEnd(paragraphEnd.next(true));
    // The position after the last position in the last cell of a table
    // is not the start of the next paragraph.
    if (isFirstPositionAfterTable(afterParagraphEnd))
        return afterParagraphEnd.next(true);
    return afterParagraphEnd;
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
    return pos.isNotNull() && pos == endOfParagraph(pos);
}

VisiblePosition previousParagraphPosition(const VisiblePosition &p, int x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = previousLinePosition(pos, x);
        if (n.isNull() || n == pos)
            return p;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

VisiblePosition nextParagraphPosition(const VisiblePosition &p, int x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = nextLinePosition(pos, x);
        if (n.isNull() || n == pos)
            return p;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

// ---------

VisiblePosition startOfBlock(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();
    Node *startNode = p.node();
    if (!startNode)
        return VisiblePosition();
    return VisiblePosition(Position(startNode->enclosingBlockFlowElement(), 0), DOWNSTREAM);
}

VisiblePosition endOfBlock(const VisiblePosition &c)
{
    Position p = c.deepEquivalent();

    Node *startNode = p.node();
    if (!startNode)
        return VisiblePosition();

    Node *startBlock = startNode->enclosingBlockFlowElement();
    
    return VisiblePosition(startBlock, startBlock->childNodeCount(), VP_DEFAULT_AFFINITY);   
}

bool inSameBlock(const VisiblePosition &a, const VisiblePosition &b)
{
    return !a.isNull() && enclosingBlockFlowElement(a) == enclosingBlockFlowElement(b);
}

bool isStartOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos == startOfBlock(pos);
}

bool isEndOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos == endOfBlock(pos);
}

// ---------

VisiblePosition startOfDocument(const Node* node)
{
    if (!node)
        return VisiblePosition();
    
    return VisiblePosition(node->document()->documentElement(), 0, DOWNSTREAM);
}

VisiblePosition startOfDocument(const VisiblePosition &c)
{
    return startOfDocument(c.deepEquivalent().node());
}

VisiblePosition endOfDocument(const Node* node)
{
    if (!node || !node->document())
        return VisiblePosition();
    
    Element* doc = node->document()->documentElement();
    return VisiblePosition(doc, doc->childNodeCount(), DOWNSTREAM);
}

VisiblePosition endOfDocument(const VisiblePosition &c)
{
    return endOfDocument(c.deepEquivalent().node());
}

bool inSameDocument(const VisiblePosition &a, const VisiblePosition &b)
{
    Position ap = a.deepEquivalent();
    Node *an = ap.node();
    if (!an)
        return false;
    Position bp = b.deepEquivalent();
    Node *bn = bp.node();
    if (an == bn)
        return true;

    return an->document() == bn->document();
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

VisiblePosition startOfEditableContent(const VisiblePosition& visiblePosition)
{
    Node* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return VisiblePosition();

    return VisiblePosition(highestRoot, 0, DOWNSTREAM);
}

VisiblePosition endOfEditableContent(const VisiblePosition& visiblePosition)
{
    Node* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return VisiblePosition();

    return VisiblePosition(highestRoot, maxDeepOffset(highestRoot), DOWNSTREAM);
}

}
