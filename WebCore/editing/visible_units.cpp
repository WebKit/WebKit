/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "VisiblePosition.h"
#include "htmlediting.h"
#include <wtf/unicode/Unicode.h>

namespace WebCore {

using namespace HTMLNames;
using namespace WTF::Unicode;

static int endOfFirstWordBoundaryContext(const UChar* characters, int length)
{
    for (int i = 0; i < length; ) {
        int first = i;
        UChar32 ch;
        U16_NEXT(characters, i, length, ch);
        if (!requiresContextForWordBoundary(ch))
            return first;
    }
    return length;
}

static int startOfLastWordBoundaryContext(const UChar* characters, int length)
{
    for (int i = length; i > 0; ) {
        int last = i;
        UChar32 ch;
        U16_PREV(characters, 0, i, ch);
        if (!requiresContextForWordBoundary(ch))
            return last;
    }
    return 0;
}

enum BoundarySearchContextAvailability { DontHaveMoreContext, MayHaveMoreContext };

typedef unsigned (*BoundarySearchFunction)(const UChar*, unsigned length, unsigned offset, BoundarySearchContextAvailability, bool& needMoreContext);

static VisiblePosition previousBoundary(const VisiblePosition& c, BoundarySearchFunction searchFunction)
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
    RefPtr<Range> searchRange = Range::create(d);
    
    Vector<UChar, 1024> string;
    unsigned suffixLength = 0;

    ExceptionCode ec = 0;
    if (requiresContextForWordBoundary(c.characterBefore())) {
        RefPtr<Range> forwardsScanRange(d->createRange());
        forwardsScanRange->setEndAfter(boundary, ec);
        forwardsScanRange->setStart(end.node(), end.deprecatedEditingOffset(), ec);
        TextIterator forwardsIterator(forwardsScanRange.get());
        while (!forwardsIterator.atEnd()) {
            const UChar* characters = forwardsIterator.characters();
            int length = forwardsIterator.length();
            int i = endOfFirstWordBoundaryContext(characters, length);
            string.append(characters, i);
            suffixLength += i;
            if (i < length)
                break;
            forwardsIterator.advance();
        }
    }

    searchRange->setStart(start.node(), start.deprecatedEditingOffset(), ec);
    searchRange->setEnd(end.node(), end.deprecatedEditingOffset(), ec);
    
    ASSERT(!ec);
    if (ec)
        return VisiblePosition();

    SimplifiedBackwardsTextIterator it(searchRange.get());
    unsigned next = 0;
    bool inTextSecurityMode = start.node() && start.node()->renderer() && start.node()->renderer()->style()->textSecurity() != TSNONE;
    bool needMoreContext = false;
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
        next = searchFunction(string.data(), string.size(), string.size() - suffixLength, MayHaveMoreContext, needMoreContext);
        if (next != 0)
            break;
        it.advance();
    }
    if (needMoreContext) {
        // The last search returned the beginning of the buffer and asked for more context,
        // but there is no earlier text. Force a search with what's available.
        next = searchFunction(string.data(), string.size(), string.size() - suffixLength, DontHaveMoreContext, needMoreContext);
        ASSERT(!needMoreContext);
    }

    if (it.atEnd() && next == 0) {
        pos = it.range()->startPosition();
    } else if (next != 0) {
        Node *node = it.range()->startContainer(ec);
        if ((node->isTextNode() && static_cast<int>(next) <= node->maxCharacterOffset()) || (node->renderer() && node->renderer()->isBR() && !next))
            // The next variable contains a usable index into a text node
            pos = Position(node, next);
        else {
            // Use the character iterator to translate the next value into a DOM position.
            BackwardsCharacterIterator charIt(searchRange.get());
            charIt.advance(string.size() - suffixLength - next);
            pos = charIt.range()->endPosition();
        }
    }

    return VisiblePosition(pos, DOWNSTREAM);
}

static VisiblePosition nextBoundary(const VisiblePosition& c, BoundarySearchFunction searchFunction)
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

    Vector<UChar, 1024> string;
    unsigned prefixLength = 0;

    ExceptionCode ec = 0;
    if (requiresContextForWordBoundary(c.characterAfter())) {
        RefPtr<Range> backwardsScanRange(d->createRange());
        backwardsScanRange->setEnd(start.node(), start.deprecatedEditingOffset(), ec);
        SimplifiedBackwardsTextIterator backwardsIterator(backwardsScanRange.get());
        while (!backwardsIterator.atEnd()) {
            const UChar* characters = backwardsIterator.characters();
            int length = backwardsIterator.length();
            int i = startOfLastWordBoundaryContext(characters, length);
            string.prepend(characters + i, length - i);
            prefixLength += length - i;
            if (i > 0)
                break;
            backwardsIterator.advance();
        }
    }

    searchRange->selectNodeContents(boundary, ec);
    searchRange->setStart(start.node(), start.deprecatedEditingOffset(), ec);
    TextIterator it(searchRange.get(), true);
    unsigned next = 0;
    bool inTextSecurityMode = start.node() && start.node()->renderer() && start.node()->renderer()->style()->textSecurity() != TSNONE;
    bool needMoreContext = false;
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
        next = searchFunction(string.data(), string.size(), prefixLength, MayHaveMoreContext, needMoreContext);
        if (next != string.size())
            break;
        it.advance();
    }
    if (needMoreContext) {
        // The last search returned the end of the buffer and asked for more context,
        // but there is no further text. Force a search with what's available.
        next = searchFunction(string.data(), string.size(), prefixLength, DontHaveMoreContext, needMoreContext);
        ASSERT(!needMoreContext);
    }
    
    if (it.atEnd() && next == string.size()) {
        pos = it.range()->startPosition();
    } else if (next != prefixLength) {
        // Use the character iterator to translate the next value into a DOM position.
        CharacterIterator charIt(searchRange.get(), true);
        charIt.advance(next - prefixLength - 1);
        pos = charIt.range()->endPosition();
        
        if (*charIt.characters() == '\n') {
            // FIXME: workaround for collapsed range (where only start position is correct) emitted for some emitted newlines (see rdar://5192593)
            VisiblePosition visPos = VisiblePosition(pos);
            if (visPos == VisiblePosition(charIt.range()->startPosition()))
                pos = visPos.next(true).deepEquivalent();
        }
    }

    // generate VisiblePosition, use UPSTREAM affinity if possible
    return VisiblePosition(pos, VP_UPSTREAM_IF_POSSIBLE);
}

// ---------

static unsigned startWordBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    ASSERT(offset);
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(characters, offset)) {
        needMoreContext = true;
        return 0;
    }
    needMoreContext = false;
    int start, end;
    findWordBoundary(characters, length, offset - 1, &start, &end);
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

static unsigned endWordBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    ASSERT(offset <= length);
    if (mayHaveMoreContext && endOfFirstWordBoundaryContext(characters + offset, length - offset) == static_cast<int>(length - offset)) {
        needMoreContext = true;
        return length;
    }
    needMoreContext = false;
    int start, end;
    findWordBoundary(characters, length, offset, &start, &end);
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

static unsigned previousWordPositionBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(characters, offset)) {
        needMoreContext = true;
        return 0;
    }
    needMoreContext = false;
    return findNextWordFromIndex(characters, length, offset, false);
}

VisiblePosition previousWordPosition(const VisiblePosition &c)
{
    VisiblePosition prev = previousBoundary(c, previousWordPositionBoundary);
    return c.honorEditableBoundaryAtOrAfter(prev);
}

static unsigned nextWordPositionBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    if (mayHaveMoreContext && endOfFirstWordBoundaryContext(characters + offset, length - offset) == static_cast<int>(length - offset)) {
        needMoreContext = true;
        return length;
    }
    needMoreContext = false;
    return findNextWordFromIndex(characters, length, offset, true);
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

    InlineBox* box;
    int offset;
    c.getInlineBoxAndOffset(box, offset);
    
    return box ? box->root() : 0;
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
        if (p.node()->renderer() && p.node()->renderer()->isRenderBlock() && p.deprecatedEditingOffset() == 0)
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

        RenderObject *startRenderer = startBox->renderer();
        if (!startRenderer)
            return VisiblePosition();

        startNode = startRenderer->node();
        if (startNode)
            break;
        
        startBox = startBox->nextLeafChild();
    }
    
    int startOffset = 0;
    if (startBox->isInlineTextBox()) {
        InlineTextBox *startTextBox = static_cast<InlineTextBox *>(startBox);
        startOffset = startTextBox->start();
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
        if (p.deprecatedEditingOffset() > c.deepEquivalent().deprecatedEditingOffset() && p.node()->isSameNode(c.deepEquivalent().node())) {
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
        if (p.node()->renderer() && p.node()->renderer()->isRenderBlock() && p.deprecatedEditingOffset() == 0)
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

        RenderObject *endRenderer = endBox->renderer();
        if (!endRenderer)
            return VisiblePosition();

        endNode = endRenderer->node();
        if (endNode)
            break;
        
        endBox = endBox->prevLeafChild();
    }
    
    int endOffset = 1;
    if (endNode->hasTagName(brTag)) {
        endOffset = 0;
    } else if (endBox->isInlineTextBox()) {
        InlineTextBox *endTextBox = static_cast<InlineTextBox *>(endBox);
        endOffset = endTextBox->start();
        if (!endTextBox->isLineBreak())
            endOffset += endTextBox->len();
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

// The first leaf before node that has the same editability as node.
static Node* previousLeafWithSameEditability(Node* node)
{
    bool editable = node->isContentEditable();
    Node* n = node->previousLeafNode();
    while (n) {
        if (editable == n->isContentEditable())
            return n;
        n = n->previousLeafNode();
    }
    return 0;
}

static Node* enclosingNodeWithNonInlineRenderer(Node* n)
{
    for (Node* p = n; p; p = p->parentNode()) {
        if (p->renderer() && !p->renderer()->isInline())
            return p;
    }
    return 0;
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
    InlineBox* box;
    int ignoredCaretOffset;
    visiblePosition.getInlineBoxAndOffset(box, ignoredCaretOffset);
    if (box) {
        root = box->root()->prevRootBox();
        if (root)
            containingBlock = renderer->containingBlock();
    }

    if (!root) {
        // This containing editable block does not have a previous line.
        // Need to move back to previous containing editable block in this root editable
        // block and find the last root line box in that block.
        Node* startBlock = enclosingNodeWithNonInlineRenderer(node);
        Node* n = previousLeafWithSameEditability(node);
        while (n && startBlock == enclosingNodeWithNonInlineRenderer(n))
            n = previousLeafWithSameEditability(n);
        while (n) {
            if (highestEditableRoot(Position(n, 0)) != highestRoot)
                break;
            Position pos(n, caretMinOffset(n));
            if (pos.isCandidate()) {
                ASSERT(n->renderer());
                Position maxPos(n, caretMaxOffset(n));
                maxPos.getInlineBoxAndOffset(DOWNSTREAM, box, ignoredCaretOffset);
                if (box) {
                    // previous root line box found
                    root = box->root();
                    containingBlock = n->renderer()->containingBlock();
                    break;
                }

                return VisiblePosition(pos, DOWNSTREAM);
            }
            n = previousLeafWithSameEditability(n);
        }
    }
    
    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        FloatPoint absPos = containingBlock->localToAbsolute(FloatPoint());
        if (containingBlock->hasOverflowClip())
            absPos -= containingBlock->layer()->scrolledContentOffset();
        RenderObject* renderer = root->closestLeafChildForXPos(x - absPos.x(), isEditablePosition(p))->renderer();
        Node* node = renderer->node();
        if (node && editingIgnoresContent(node))
            return Position(node->parent(), node->nodeIndex());
        return renderer->positionForPoint(IntPoint(x - absPos.x(), root->topOverflow()));
    }
    
    // Could not find a previous line. This means we must already be on the first line.
    // Move to the start of the content in this block, which effectively moves us
    // to the start of the line we're on.
    Element* rootElement = node->isContentEditable() ? node->rootEditableElement() : node->document()->documentElement();
    return VisiblePosition(rootElement, 0, DOWNSTREAM);
}

static Node* nextLeafWithSameEditability(Node* node, int offset)
{
    bool editable = node->isContentEditable();
    ASSERT(offset >= 0);
    Node* child = node->childNode(offset);
    Node* n = child ? child->nextLeafNode() : node->nextLeafNode();
    while (n) {
        if (editable == n->isContentEditable())
            return n;
        n = n->nextLeafNode();
    }
    return 0;
}

static Node* nextLeafWithSameEditability(Node* node)
{
    if (!node)
        return 0;
    
    bool editable = node->isContentEditable();
    Node* n = node->nextLeafNode();
    while (n) {
        if (editable == n->isContentEditable())
            return n;
        n = n->nextLeafNode();
    }
    return 0;
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
    InlineBox* box;
    int ignoredCaretOffset;
    visiblePosition.getInlineBoxAndOffset(box, ignoredCaretOffset);
    if (box) {
        root = box->root()->nextRootBox();
        if (root)
            containingBlock = renderer->containingBlock();
    }

    if (!root) {
        // This containing editable block does not have a next line.
        // Need to move forward to next containing editable block in this root editable
        // block and find the first root line box in that block.
        Node* startBlock = enclosingNodeWithNonInlineRenderer(node);
        Node* n = nextLeafWithSameEditability(node, p.deprecatedEditingOffset());
        while (n && startBlock == enclosingNodeWithNonInlineRenderer(n))
            n = nextLeafWithSameEditability(n);
        while (n) {
            if (highestEditableRoot(Position(n, 0)) != highestRoot)
                break;
            Position pos(n, caretMinOffset(n));
            if (pos.isCandidate()) {
                ASSERT(n->renderer());
                pos.getInlineBoxAndOffset(DOWNSTREAM, box, ignoredCaretOffset);
                if (box) {
                    // next root line box found
                    root = box->root();
                    containingBlock = n->renderer()->containingBlock();
                    break;
                }

                return VisiblePosition(pos, DOWNSTREAM);
            }
            n = nextLeafWithSameEditability(n);
        }
    }
    
    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        FloatPoint absPos = containingBlock->localToAbsolute(FloatPoint());
        if (containingBlock->hasOverflowClip())
            absPos -= containingBlock->layer()->scrolledContentOffset();
        RenderObject* renderer = root->closestLeafChildForXPos(x - absPos.x(), isEditablePosition(p))->renderer();
        Node* node = renderer->node();
        if (node && editingIgnoresContent(node))
            return Position(node->parent(), node->nodeIndex());
        return renderer->positionForPoint(IntPoint(x - absPos.x(), root->topOverflow()));
    }    

    // Could not find a next line. This means we must already be on the last line.
    // Move to the end of the content in this block, which effectively moves us
    // to the end of the line we're on.
    Element* rootElement = node->isContentEditable() ? node->rootEditableElement() : node->document()->documentElement();
    return VisiblePosition(rootElement, rootElement ? rootElement->childNodeCount() : 0, DOWNSTREAM);
}

// ---------

static unsigned startSentenceBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    // FIXME: The following function can return -1; we don't handle that.
    return textBreakPreceding(iterator, length);
}

VisiblePosition startOfSentence(const VisiblePosition &c)
{
    return previousBoundary(c, startSentenceBoundary);
}

static unsigned endSentenceBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    return textBreakNext(iterator);
}

// FIXME: This includes the space after the punctuation that marks the end of the sentence.
VisiblePosition endOfSentence(const VisiblePosition &c)
{
    return nextBoundary(c, endSentenceBoundary);
}

static unsigned previousSentencePositionBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
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

static unsigned nextSentencePositionBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
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

static bool renderedAsNonInlineTableOrHR(RenderObject* renderer)
{
    return renderer && ((renderer->isTable() && !renderer->isInline()) || renderer->isHR());
}

// FIXME: Broken for positions before/after images that aren't inline (5027702)
VisiblePosition startOfParagraph(const VisiblePosition& c)
{
    Position p = c.deepEquivalent();
    Node *startNode = p.node();

    if (!startNode)
        return VisiblePosition();
    
    if (renderedAsNonInlineTableOrHR(startNode->renderer()) && p.atLastEditingPositionForNode())
        return firstDeepEditingPositionForNode(startNode);

    Node* startBlock = enclosingBlock(startNode);

    Node *node = startNode;
    int offset = p.deprecatedEditingOffset();

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
                const UChar* chars = toRenderText(r)->characters();
                int i = toRenderText(r)->textLength();
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

    if (renderedAsNonInlineTableOrHR(startNode->renderer()) && p.atFirstEditingPositionForNode())
        return lastDeepEditingPositionForNode(startNode);
    
    Node* startBlock = enclosingBlock(startNode);
    Node *stayInsideBlock = startBlock;
    
    Node *node = startNode;
    int offset = p.deprecatedEditingOffset();

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
            int length = toRenderText(r)->textLength();
            if (style->preserveNewline()) {
                const UChar* chars = toRenderText(r)->characters();
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
            offset = lastOffsetForEditing(n);
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

VisiblePosition previousParagraphPosition(const VisiblePosition& p, int x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = previousLinePosition(pos, x);
        if (n.isNull() || n == pos)
            break;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

VisiblePosition nextParagraphPosition(const VisiblePosition& p, int x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = nextLinePosition(pos, x);
        if (n.isNull() || n == pos)
            break;
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

    return firstDeepEditingPositionForNode(highestRoot);
}

VisiblePosition endOfEditableContent(const VisiblePosition& visiblePosition)
{
    Node* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return VisiblePosition();

    return lastDeepEditingPositionForNode(highestRoot);
}

static void getLeafBoxesInLogicalOrder(RootInlineBox* rootBox, Vector<InlineBox*>& leafBoxesInLogicalOrder)
{
    unsigned char minLevel = 128;
    unsigned char maxLevel = 0;
    unsigned count = 0;
    InlineBox* r = rootBox->firstLeafChild();
    // First find highest and lowest levels,
    // and initialize leafBoxesInLogicalOrder with the leaf boxes in visual order.
    while (r) {
        if (r->bidiLevel() > maxLevel)
            maxLevel = r->bidiLevel();
        if (r->bidiLevel() < minLevel)
            minLevel = r->bidiLevel();
        leafBoxesInLogicalOrder.append(r);
        r = r->nextLeafChild();
        ++count;
    }

    if (rootBox->renderer()->style()->visuallyOrdered())
        return;
    // Reverse of reordering of the line (L2 according to Bidi spec):
    // L2. From the highest level found in the text to the lowest odd level on each line,
    // reverse any contiguous sequence of characters that are at that level or higher.

    // Reversing the reordering of the line is only done up to the lowest odd level.
    if (!(minLevel % 2))
        minLevel++;
    
    InlineBox** end = leafBoxesInLogicalOrder.end();
    while (minLevel <= maxLevel) {
        InlineBox** iter = leafBoxesInLogicalOrder.begin();
        while (iter != end) {
            while (iter != end) {
                if ((*iter)->bidiLevel() >= minLevel)
                    break;
                ++iter;
            }
            InlineBox** first = iter;
            while (iter != end) {
                if ((*iter)->bidiLevel() < minLevel)
                    break;
                ++iter;
            }
            InlineBox** last = iter;
            std::reverse(first, last);
        }                
        ++minLevel;
    }
}

static void getLogicalStartBoxAndNode(RootInlineBox* rootBox, InlineBox*& startBox, Node*& startNode)
{
    Vector<InlineBox*> leafBoxesInLogicalOrder;
    getLeafBoxesInLogicalOrder(rootBox, leafBoxesInLogicalOrder);
    startBox = 0;
    startNode = 0;
    for (size_t i = 0; i < leafBoxesInLogicalOrder.size(); ++i) {
        startBox = leafBoxesInLogicalOrder[i];
        startNode = startBox->renderer()->node();
        if (startNode)
            return; 
    }
}

static void getLogicalEndBoxAndNode(RootInlineBox* rootBox, InlineBox*& endBox, Node*& endNode)
{
    Vector<InlineBox*> leafBoxesInLogicalOrder;
    getLeafBoxesInLogicalOrder(rootBox, leafBoxesInLogicalOrder);
    endBox = 0;
    endNode = 0;
    // Generated content (e.g. list markers and CSS :before and :after
    // pseudoelements) have no corresponding DOM element, and so cannot be
    // represented by a VisiblePosition.  Use whatever precedes instead.
    for (size_t i = leafBoxesInLogicalOrder.size(); i > 0; --i) { 
        endBox = leafBoxesInLogicalOrder[i - 1];
        endNode = endBox->renderer()->node();
        if (endNode)
            return;
    }
}

static VisiblePosition logicalStartPositionForLine(const VisiblePosition& c)
{
    if (c.isNull())
        return VisiblePosition();

    RootInlineBox* rootBox = rootBoxForLine(c);
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        Position p = c.deepEquivalent();
        if (p.node()->renderer() && p.node()->renderer()->isRenderBlock() && !p.deprecatedEditingOffset())
            return positionAvoidingFirstPositionInTable(c);
        
        return VisiblePosition();
    }
    
    InlineBox* logicalStartBox;
    Node* logicalStartNode;
    getLogicalStartBoxAndNode(rootBox, logicalStartBox, logicalStartNode);

    if (!logicalStartNode)
        return VisiblePosition();

    int startOffset = logicalStartBox->caretMinOffset();
  
    VisiblePosition visPos = VisiblePosition(logicalStartNode, startOffset, DOWNSTREAM);
    return positionAvoidingFirstPositionInTable(visPos);
}

VisiblePosition logicalStartOfLine(const VisiblePosition& c)
{
    VisiblePosition visPos = logicalStartPositionForLine(c);
    
    if (visPos.isNull())
        return c.honorEditableBoundaryAtOrAfter(visPos);

    return c.honorEditableBoundaryAtOrAfter(visPos);
}

static VisiblePosition logicalEndPositionForLine(const VisiblePosition& c)
{
    if (c.isNull())
        return VisiblePosition();

    RootInlineBox* rootBox = rootBoxForLine(c);
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        Position p = c.deepEquivalent();
        if (p.node()->renderer() && p.node()->renderer()->isRenderBlock() && !p.deprecatedEditingOffset())
            return c;
        return VisiblePosition();
    }
    
    InlineBox* logicalEndBox;
    Node* logicalEndNode;
    getLogicalEndBoxAndNode(rootBox, logicalEndBox, logicalEndNode);
    if (!logicalEndNode)
        return VisiblePosition();
    
    int endOffset = 1;
    if (logicalEndNode->hasTagName(brTag))
        endOffset = 0;
    else if (logicalEndBox->isInlineTextBox()) {
        InlineTextBox* endTextBox = static_cast<InlineTextBox*>(logicalEndBox);
        endOffset = endTextBox->start();
        if (!endTextBox->isLineBreak())
            endOffset += endTextBox->len();
    }
    
    return VisiblePosition(logicalEndNode, endOffset, VP_UPSTREAM_IF_POSSIBLE);
}

bool inSameLogicalLine(const VisiblePosition& a, const VisiblePosition& b)
{
    return a.isNotNull() && logicalStartOfLine(a) == logicalStartOfLine(b);
}

VisiblePosition logicalEndOfLine(const VisiblePosition& c)
{
    VisiblePosition visPos = logicalEndPositionForLine(c);
    
    // Make sure the end of line is at the same line as the given input position. For a wrapping line, the logical end
    // position for the not-last-2-lines might incorrectly hand back the logical beginning of the next line. 
    // For example, <div contenteditable dir="rtl" style="line-break:before-white-space">abcdefg abcdefg abcdefg
    // a abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg </div>
    // In this case, use the previous position of the computed logical end position.
    if (!inSameLogicalLine(c, visPos))
        visPos = visPos.previous();
    
    return c.honorEditableBoundaryAtOrBefore(visPos);
}

}
