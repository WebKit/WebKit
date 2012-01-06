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
#include "InlineTextBox.h"
#include "Position.h"
#include "RenderBlock.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "RenderedPosition.h"
#include "Text.h"
#include "TextBoundaries.h"
#include "TextBreakIterator.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include "VisibleSelection.h"
#include "htmlediting.h"
#include <wtf/unicode/Unicode.h>

namespace WebCore {

using namespace HTMLNames;
using namespace WTF::Unicode;

enum BoundarySearchContextAvailability { DontHaveMoreContext, MayHaveMoreContext };

typedef unsigned (*BoundarySearchFunction)(const UChar*, unsigned length, unsigned offset, BoundarySearchContextAvailability, bool& needMoreContext);

static VisiblePosition previousBoundary(const VisiblePosition& c, BoundarySearchFunction searchFunction)
{
    Position pos = c.deepEquivalent();
    Node* boundary = pos.parentEditingBoundary();
    if (!boundary)
        return VisiblePosition();

    Document* d = boundary->document();
    Position start = createLegacyEditingPosition(boundary, 0).parentAnchoredEquivalent();
    Position end = pos.parentAnchoredEquivalent();
    RefPtr<Range> searchRange = Range::create(d);
    
    Vector<UChar, 1024> string;
    unsigned suffixLength = 0;

    ExceptionCode ec = 0;
    if (requiresContextForWordBoundary(c.characterBefore())) {
        RefPtr<Range> forwardsScanRange(d->createRange());
        forwardsScanRange->setEndAfter(boundary, ec);
        forwardsScanRange->setStart(end.deprecatedNode(), end.deprecatedEditingOffset(), ec);
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

    searchRange->setStart(start.deprecatedNode(), start.deprecatedEditingOffset(), ec);
    searchRange->setEnd(end.deprecatedNode(), end.deprecatedEditingOffset(), ec);

    ASSERT(!ec);
    if (ec)
        return VisiblePosition();

    SimplifiedBackwardsTextIterator it(searchRange.get());
    unsigned next = 0;
    bool inTextSecurityMode = start.deprecatedNode() && start.deprecatedNode()->renderer() && start.deprecatedNode()->renderer()->style()->textSecurity() != TSNONE;
    bool needMoreContext = false;
    while (!it.atEnd()) {
        // iterate to get chunks until the searchFunction returns a non-zero value.
        if (!inTextSecurityMode) 
            string.prepend(it.characters(), it.length());
        else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            String iteratorString(it.characters(), it.length());
            iteratorString.fill('x');
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

    if (!next)
        return VisiblePosition(it.atEnd() ? it.range()->startPosition() : pos, DOWNSTREAM);

    Node* node = it.range()->startContainer(ec);
    if ((node->isTextNode() && static_cast<int>(next) <= node->maxCharacterOffset()) || (node->renderer() && node->renderer()->isBR() && !next))
        // The next variable contains a usable index into a text node
        return VisiblePosition(createLegacyEditingPosition(node, next), DOWNSTREAM);

    // Use the character iterator to translate the next value into a DOM position.
    BackwardsCharacterIterator charIt(searchRange.get());
    charIt.advance(string.size() - suffixLength - next);
    // FIXME: charIt can get out of shadow host.
    return VisiblePosition(charIt.range()->endPosition(), DOWNSTREAM);
}

static VisiblePosition nextBoundary(const VisiblePosition& c, BoundarySearchFunction searchFunction)
{
    Position pos = c.deepEquivalent();
    Node* boundary = pos.parentEditingBoundary();
    if (!boundary)
        return VisiblePosition();

    Document* d = boundary->document();
    RefPtr<Range> searchRange(d->createRange());
    Position start(pos.parentAnchoredEquivalent());

    Vector<UChar, 1024> string;
    unsigned prefixLength = 0;

    ExceptionCode ec = 0;
    if (requiresContextForWordBoundary(c.characterAfter())) {
        RefPtr<Range> backwardsScanRange(d->createRange());
        backwardsScanRange->setEnd(start.deprecatedNode(), start.deprecatedEditingOffset(), ec);
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
    searchRange->setStart(start.deprecatedNode(), start.deprecatedEditingOffset(), ec);
    TextIterator it(searchRange.get(), TextIteratorEmitsCharactersBetweenAllVisiblePositions);
    unsigned next = 0;
    bool inTextSecurityMode = start.deprecatedNode() && start.deprecatedNode()->renderer() && start.deprecatedNode()->renderer()->style()->textSecurity() != TSNONE;
    bool needMoreContext = false;
    while (!it.atEnd()) {
        // Keep asking the iterator for chunks until the search function
        // returns an end value not equal to the length of the string passed to it.
        if (!inTextSecurityMode)
            string.append(it.characters(), it.length());
        else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            String iteratorString(it.characters(), it.length());
            iteratorString.fill('x');
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
        CharacterIterator charIt(searchRange.get(), TextIteratorEmitsCharactersBetweenAllVisiblePositions);
        charIt.advance(next - prefixLength - 1);
        RefPtr<Range> characterRange = charIt.range();
        pos = characterRange->endPosition();
        
        if (*charIt.characters() == '\n') {
            // FIXME: workaround for collapsed range (where only start position is correct) emitted for some emitted newlines (see rdar://5192593)
            VisiblePosition visPos = VisiblePosition(pos);
            if (visPos == VisiblePosition(characterRange->startPosition())) {
                charIt.advance(1);
                pos = charIt.range()->startPosition();
            }
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
    U16_BACK_1(characters, 0, offset);
    findWordBoundary(characters, length, offset, &start, &end);
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
    return c.honorEditingBoundaryAtOrBefore(prev);
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
    return c.honorEditingBoundaryAtOrAfter(next);
}

bool isStartOfWord(const VisiblePosition& p)
{
    return p.isNotNull() && p == startOfWord(p, RightWordIfOnBoundary);
}

// ---------

enum LineEndpointComputationMode { UseLogicalOrdering, UseInlineBoxOrdering };
static VisiblePosition startPositionForLine(const VisiblePosition& c, LineEndpointComputationMode mode)
{
    if (c.isNull())
        return VisiblePosition();

    RootInlineBox* rootBox = RenderedPosition(c).rootBox();
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        Position p = c.deepEquivalent();
        if (p.deprecatedNode()->renderer() && p.deprecatedNode()->renderer()->isRenderBlock() && !p.deprecatedEditingOffset())
            return c;

        return VisiblePosition();
    }

    Node* startNode;
    InlineBox* startBox;
    if (mode == UseLogicalOrdering) {
        startNode = rootBox->getLogicalStartBoxWithNode(startBox);
        if (!startNode)
            return VisiblePosition();
    } else {
        // Generated content (e.g. list markers and CSS :before and :after pseudoelements) have no corresponding DOM element,
        // and so cannot be represented by a VisiblePosition. Use whatever follows instead.
        startBox = rootBox->firstLeafChild();
        while (true) {
            if (!startBox)
                return VisiblePosition();

            RenderObject* startRenderer = startBox->renderer();
            if (!startRenderer)
                return VisiblePosition();

            startNode = startRenderer->node();
            if (startNode)
                break;

            startBox = startBox->nextLeafChild();
        }
    }

    return startNode->isTextNode() ? Position(static_cast<Text*>(startNode), toInlineTextBox(startBox)->start())
        : positionBeforeNode(startNode);
}

static VisiblePosition startOfLine(const VisiblePosition& c, LineEndpointComputationMode mode)
{
    // TODO: this is the current behavior that might need to be fixed.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
    VisiblePosition visPos = startPositionForLine(c, mode);

    if (mode == UseLogicalOrdering) {
        if (Node* editableRoot = highestEditableRoot(c.deepEquivalent())) {
            if (!editableRoot->contains(visPos.deepEquivalent().containerNode()))
                return firstPositionInNode(editableRoot);
        }
    }

    return c.honorEditingBoundaryAtOrBefore(visPos);
}

// FIXME: Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition startOfLine(const VisiblePosition& currentPosition)
{
    return startOfLine(currentPosition, UseInlineBoxOrdering);
}

VisiblePosition logicalStartOfLine(const VisiblePosition& currentPosition)
{
    return startOfLine(currentPosition, UseLogicalOrdering);
}

static VisiblePosition endPositionForLine(const VisiblePosition& c, LineEndpointComputationMode mode)
{
    if (c.isNull())
        return VisiblePosition();

    RootInlineBox* rootBox = RenderedPosition(c).rootBox();
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        Position p = c.deepEquivalent();
        if (p.deprecatedNode()->renderer() && p.deprecatedNode()->renderer()->isRenderBlock() && !p.deprecatedEditingOffset())
            return c;
        return VisiblePosition();
    }

    Node* endNode;
    InlineBox* endBox;
    if (mode == UseLogicalOrdering) {
        endNode = rootBox->getLogicalEndBoxWithNode(endBox);
        if (!endNode)
            return VisiblePosition();
    } else {
        // Generated content (e.g. list markers and CSS :before and :after pseudoelements) have no corresponding DOM element,
        // and so cannot be represented by a VisiblePosition. Use whatever precedes instead.
        endBox = rootBox->lastLeafChild();
        while (true) {
            if (!endBox)
                return VisiblePosition();

            RenderObject* endRenderer = endBox->renderer();
            if (!endRenderer)
                return VisiblePosition();

            endNode = endRenderer->node();
            if (endNode)
                break;
            
            endBox = endBox->prevLeafChild();
        }
    }

    Position pos;
    if (endNode->hasTagName(brTag))
        pos = positionBeforeNode(endNode);
    else if (endBox->isInlineTextBox() && endNode->isTextNode()) {
        InlineTextBox* endTextBox = toInlineTextBox(endBox);
        int endOffset = endTextBox->start();
        if (!endTextBox->isLineBreak())
            endOffset += endTextBox->len();
        pos = Position(static_cast<Text*>(endNode), endOffset);
    } else
        pos = positionAfterNode(endNode);
    
    return VisiblePosition(pos, VP_UPSTREAM_IF_POSSIBLE);
}

static bool inSameLogicalLine(const VisiblePosition& a, const VisiblePosition& b)
{
    return a.isNotNull() && logicalStartOfLine(a) == logicalStartOfLine(b);
}

static VisiblePosition endOfLine(const VisiblePosition& c, LineEndpointComputationMode mode)
{
    // TODO: this is the current behavior that might need to be fixed.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
    VisiblePosition visPos = endPositionForLine(c, mode);

    if (mode == UseLogicalOrdering) {
        // Make sure the end of line is at the same line as the given input position. For a wrapping line, the logical end
        // position for the not-last-2-lines might incorrectly hand back the logical beginning of the next line. 
        // For example, <div contenteditable dir="rtl" style="line-break:before-white-space">abcdefg abcdefg abcdefg
        // a abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg </div>
        // In this case, use the previous position of the computed logical end position.
        if (!inSameLogicalLine(c, visPos))
            visPos = visPos.previous();

        if (Node* editableRoot = highestEditableRoot(c.deepEquivalent())) {
            if (!editableRoot->contains(visPos.deepEquivalent().containerNode()))
                return lastPositionInNode(editableRoot);
        }

        return c.honorEditingBoundaryAtOrAfter(visPos);
    }

    // Make sure the end of line is at the same line as the given input position.  Else use the previous position to 
    // obtain end of line.  This condition happens when the input position is before the space character at the end 
    // of a soft-wrapped non-editable line. In this scenario, endPositionForLine would incorrectly hand back a position
    // in the next line instead. This fix is to account for the discrepancy between lines with webkit-line-break:after-white-space style
    // versus lines without that style, which would break before a space by default. 
    if (!inSameLine(c, visPos)) {
        visPos = c.previous();
        if (visPos.isNull())
            return VisiblePosition();
        visPos = endPositionForLine(visPos, UseInlineBoxOrdering);
    }
    
    return c.honorEditingBoundaryAtOrAfter(visPos);
}

// FIXME: Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition endOfLine(const VisiblePosition& currentPosition)
{
    return endOfLine(currentPosition, UseInlineBoxOrdering);
}

VisiblePosition logicalEndOfLine(const VisiblePosition& currentPosition)
{
    return endOfLine(currentPosition, UseLogicalOrdering);
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
static Node* previousLeafWithSameEditability(Node* node, EditableType editableType)
{
    bool editable = node->rendererIsEditable(editableType);
    Node* n = node->previousLeafNode();
    while (n) {
        if (editable == n->rendererIsEditable(editableType))
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

static inline IntPoint absoluteLineDirectionPointToLocalPointInBlock(RootInlineBox* root, int lineDirectionPoint)
{
    ASSERT(root);
    RenderBlock* containingBlock = root->block();
    FloatPoint absoluteBlockPoint = containingBlock->localToAbsolute(FloatPoint());
    if (containingBlock->hasOverflowClip())
        absoluteBlockPoint -= containingBlock->layer()->scrolledContentOffset();

    if (root->block()->isHorizontalWritingMode())
        return IntPoint(lineDirectionPoint - absoluteBlockPoint.x(), root->blockDirectionPointInLine());

    return IntPoint(root->selectionTop(), lineDirectionPoint - absoluteBlockPoint.y());
}

VisiblePosition previousLinePosition(const VisiblePosition &visiblePosition, int lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.deprecatedNode();
    Node* highestRoot = highestEditableRoot(p, editableType);

    if (!node)
        return VisiblePosition();
    
    node->document()->updateLayoutIgnorePendingStylesheets();
    
    RenderObject *renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RootInlineBox *root = 0;
    InlineBox* box;
    int ignoredCaretOffset;
    visiblePosition.getInlineBoxAndOffset(box, ignoredCaretOffset);
    if (box) {
        root = box->root()->prevRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight())
            root = 0;
    }

    if (!root) {
        // This containing editable block does not have a previous line.
        // Need to move back to previous containing editable block in this root editable
        // block and find the last root line box in that block.
        Node* startBlock = enclosingNodeWithNonInlineRenderer(node);
        Node* n = previousLeafWithSameEditability(node, editableType);
        while (n && startBlock == enclosingNodeWithNonInlineRenderer(n))
            n = previousLeafWithSameEditability(n, editableType);
        while (n) {
            if (highestEditableRoot(firstPositionInOrBeforeNode(n), editableType) != highestRoot)
                break;
            Position pos = n->hasTagName(brTag) ? positionBeforeNode(n) : createLegacyEditingPosition(n, caretMaxOffset(n));
            if (pos.isCandidate()) {
                pos.getInlineBoxAndOffset(DOWNSTREAM, box, ignoredCaretOffset);
                if (box) {
                    // previous root line box found
                    root = box->root();
                    break;
                }

                return VisiblePosition(pos, DOWNSTREAM);
            }
            n = previousLeafWithSameEditability(n, editableType);
        }
    }
    
    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        IntPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(root, lineDirectionPoint);
        RenderObject* renderer = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p))->renderer();
        Node* node = renderer->node();
        if (node && editingIgnoresContent(node))
            return positionInParentBeforeNode(node);
        return renderer->positionForPoint(pointInLine);
    }
    
    // Could not find a previous line. This means we must already be on the first line.
    // Move to the start of the content in this block, which effectively moves us
    // to the start of the line we're on.
    Element* rootElement = node->rendererIsEditable(editableType) ? node->rootEditableElement(editableType) : node->document()->documentElement();
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition(firstPositionInNode(rootElement), DOWNSTREAM);
}

static Node* nextLeafWithSameEditability(Node* node, int offset)
{
    bool editable = node->rendererIsEditable();
    ASSERT(offset >= 0);
    Node* child = node->childNode(offset);
    Node* n = child ? child->nextLeafNode() : node->lastDescendant()->nextLeafNode();
    while (n) {
        if (editable == n->rendererIsEditable())
            return n;
        n = n->nextLeafNode();
    }
    return 0;
}

static Node* nextLeafWithSameEditability(Node* node, EditableType editableType = ContentIsEditable)
{
    if (!node)
        return 0;
    
    bool editable = node->rendererIsEditable(editableType);
    Node* n = node->nextLeafNode();
    while (n) {
        if (editable == n->rendererIsEditable(editableType))
            return n;
        n = n->nextLeafNode();
    }
    return 0;
}

VisiblePosition nextLinePosition(const VisiblePosition &visiblePosition, int lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.deprecatedNode();
    Node* highestRoot = highestEditableRoot(p, editableType);

    if (!node)
        return VisiblePosition();
    
    node->document()->updateLayoutIgnorePendingStylesheets();

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RootInlineBox *root = 0;
    InlineBox* box;
    int ignoredCaretOffset;
    visiblePosition.getInlineBoxAndOffset(box, ignoredCaretOffset);
    if (box) {
        root = box->root()->nextRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight())
            root = 0;
    }

    if (!root) {
        // This containing editable block does not have a next line.
        // Need to move forward to next containing editable block in this root editable
        // block and find the first root line box in that block.
        Node* startBlock = enclosingNodeWithNonInlineRenderer(node);
        Node* n = nextLeafWithSameEditability(node, p.deprecatedEditingOffset());
        while (n && startBlock == enclosingNodeWithNonInlineRenderer(n))
            n = nextLeafWithSameEditability(n, editableType);
        while (n) {
            if (highestEditableRoot(firstPositionInOrBeforeNode(n), editableType) != highestRoot)
                break;
            Position pos = createLegacyEditingPosition(n, caretMinOffset(n));
            if (pos.isCandidate()) {
                ASSERT(n->renderer());
                pos.getInlineBoxAndOffset(DOWNSTREAM, box, ignoredCaretOffset);
                if (box) {
                    // next root line box found
                    root = box->root();
                    break;
                }

                return VisiblePosition(pos, DOWNSTREAM);
            }
            n = nextLeafWithSameEditability(n, editableType);
        }
    }
    
    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        IntPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(root, lineDirectionPoint);
        RenderObject* renderer = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p))->renderer();
        Node* node = renderer->node();
        if (node && editingIgnoresContent(node))
            return positionInParentBeforeNode(node);
        return renderer->positionForPoint(pointInLine);
    }

    // Could not find a next line. This means we must already be on the last line.
    // Move to the end of the content in this block, which effectively moves us
    // to the end of the line we're on.
    Element* rootElement = node->rendererIsEditable(editableType) ? node->rootEditableElement(editableType) : node->document()->documentElement();
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition(lastPositionInNode(rootElement), DOWNSTREAM);
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
    return c.honorEditingBoundaryAtOrBefore(prev);
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
    return c.honorEditingBoundaryAtOrAfter(next);
}

VisiblePosition startOfParagraph(const VisiblePosition& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    Position p = c.deepEquivalent();
    Node* startNode = p.deprecatedNode();

    if (!startNode)
        return VisiblePosition();
    
    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return positionBeforeNode(startNode);

    Node* startBlock = enclosingBlock(startNode);

    Node* node = startNode;
    Node* highestRoot = highestEditableRoot(p);
    int offset = p.deprecatedEditingOffset();
    Position::AnchorType type = p.anchorType();

    Node* n = startNode;
    while (n) {
        if (boundaryCrossingRule == CannotCrossEditingBoundary && n->rendererIsEditable() != startNode->rendererIsEditable())
            break;
        if (boundaryCrossingRule == CanSkipOverEditingBoundary) {
            while (n && n->rendererIsEditable() != startNode->rendererIsEditable())
                n = n->traversePreviousNodePostOrder(startBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }
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

        if (r->isText() && toRenderText(r)->renderedTextLength()) {
            ASSERT(n->isTextNode());
            type = Position::PositionIsOffsetInAnchor;
            if (style->preserveNewline()) {
                const UChar* chars = toRenderText(r)->characters();
                int i = toRenderText(r)->textLength();
                int o = offset;
                if (n == startNode && o < i)
                    i = max(0, o);
                while (--i >= 0) {
                    if (chars[i] == '\n')
                        return VisiblePosition(Position(static_cast<Text*>(n), i + 1), DOWNSTREAM);
                }
            }
            node = n;
            offset = 0;
            n = n->traversePreviousNodePostOrder(startBlock);
        } else if (editingIgnoresContent(n) || isTableElement(n)) {
            node = n;
            type = Position::PositionIsBeforeAnchor;
            n = n->previousSibling() ? n->previousSibling() : n->traversePreviousNodePostOrder(startBlock);
        } else
            n = n->traversePreviousNodePostOrder(startBlock);
    }

    if (type == Position::PositionIsOffsetInAnchor) {
        ASSERT(type == Position::PositionIsOffsetInAnchor || !offset);
        return VisiblePosition(Position(node, offset, type), DOWNSTREAM);
    }

    return VisiblePosition(Position(node, type), DOWNSTREAM);
}

VisiblePosition endOfParagraph(const VisiblePosition &c, EditingBoundaryCrossingRule boundaryCrossingRule)
{    
    if (c.isNull())
        return VisiblePosition();

    Position p = c.deepEquivalent();
    Node* startNode = p.deprecatedNode();

    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return positionAfterNode(startNode);
    
    Node* startBlock = enclosingBlock(startNode);
    Node* stayInsideBlock = startBlock;
    
    Node* node = startNode;
    Node* highestRoot = highestEditableRoot(p);
    int offset = p.deprecatedEditingOffset();
    Position::AnchorType type = p.anchorType();

    Node* n = startNode;
    while (n) {
        if (boundaryCrossingRule == CannotCrossEditingBoundary && n->rendererIsEditable() != startNode->rendererIsEditable())
            break;
        if (boundaryCrossingRule == CanSkipOverEditingBoundary) {
            while (n && n->rendererIsEditable() != startNode->rendererIsEditable())
                n = n->traverseNextNode(stayInsideBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }

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
        if (r->isText() && toRenderText(r)->renderedTextLength()) {
            ASSERT(n->isTextNode());
            int length = toRenderText(r)->textLength();
            type = Position::PositionIsOffsetInAnchor;
            if (style->preserveNewline()) {
                const UChar* chars = toRenderText(r)->characters();
                int o = n == startNode ? offset : 0;
                for (int i = o; i < length; ++i) {
                    if (chars[i] == '\n')
                        return VisiblePosition(Position(static_cast<Text*>(n), i), DOWNSTREAM);
                }
            }
            node = n;
            offset = r->caretMaxOffset();
            n = n->traverseNextNode(stayInsideBlock);
        } else if (editingIgnoresContent(n) || isTableElement(n)) {
            node = n;
            type = Position::PositionIsAfterAnchor;
            n = n->traverseNextSibling(stayInsideBlock);
        } else
            n = n->traverseNextNode(stayInsideBlock);
    }

    if (type == Position::PositionIsOffsetInAnchor)
        return VisiblePosition(Position(node, offset, type), DOWNSTREAM);

    return VisiblePosition(Position(node, type), DOWNSTREAM);
}

// FIXME: isStartOfParagraph(startOfNextParagraph(pos)) is not always true
VisiblePosition startOfNextParagraph(const VisiblePosition& visiblePosition)
{
    VisiblePosition paragraphEnd(endOfParagraph(visiblePosition, CanSkipOverEditingBoundary));
    VisiblePosition afterParagraphEnd(paragraphEnd.next(CannotCrossEditingBoundary));
    // The position after the last position in the last cell of a table
    // is not the start of the next paragraph.
    if (isFirstPositionAfterTable(afterParagraphEnd))
        return afterParagraphEnd.next(CannotCrossEditingBoundary);
    return afterParagraphEnd;
}

bool inSameParagraph(const VisiblePosition &a, const VisiblePosition &b, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return a.isNotNull() && startOfParagraph(a, boundaryCrossingRule) == startOfParagraph(b, boundaryCrossingRule);
}

bool isStartOfParagraph(const VisiblePosition &pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return pos.isNotNull() && pos == startOfParagraph(pos, boundaryCrossingRule);
}

bool isEndOfParagraph(const VisiblePosition &pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return pos.isNotNull() && pos == endOfParagraph(pos, boundaryCrossingRule);
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

VisiblePosition startOfBlock(const VisiblePosition& visiblePosition, EditingBoundaryCrossingRule rule)
{
    Position position = visiblePosition.deepEquivalent();
    Node* startBlock;
    if (!position.containerNode() || !(startBlock = enclosingBlock(position.containerNode(), rule)))
        return VisiblePosition();
    return firstPositionInNode(startBlock);
}

VisiblePosition endOfBlock(const VisiblePosition& visiblePosition, EditingBoundaryCrossingRule rule)
{
    Position position = visiblePosition.deepEquivalent();
    Node* endBlock;
    if (!position.containerNode() || !(endBlock = enclosingBlock(position.containerNode(), rule)))
        return VisiblePosition();
    return lastPositionInNode(endBlock);
}

bool inSameBlock(const VisiblePosition &a, const VisiblePosition &b)
{
    return !a.isNull() && enclosingBlock(a.deepEquivalent().containerNode()) == enclosingBlock(b.deepEquivalent().containerNode());
}

bool isStartOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos == startOfBlock(pos, CanCrossEditingBoundary);
}

bool isEndOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos == endOfBlock(pos, CanCrossEditingBoundary);
}

// ---------

VisiblePosition startOfDocument(const Node* node)
{
    if (!node || !node->document() || !node->document()->documentElement())
        return VisiblePosition();
    
    return VisiblePosition(firstPositionInNode(node->document()->documentElement()), DOWNSTREAM);
}

VisiblePosition startOfDocument(const VisiblePosition &c)
{
    return startOfDocument(c.deepEquivalent().deprecatedNode());
}

VisiblePosition endOfDocument(const Node* node)
{
    if (!node || !node->document() || !node->document()->documentElement())
        return VisiblePosition();
    
    Element* doc = node->document()->documentElement();
    return VisiblePosition(lastPositionInNode(doc), DOWNSTREAM);
}

VisiblePosition endOfDocument(const VisiblePosition &c)
{
    return endOfDocument(c.deepEquivalent().deprecatedNode());
}

bool inSameDocument(const VisiblePosition &a, const VisiblePosition &b)
{
    Position ap = a.deepEquivalent();
    Node* an = ap.deprecatedNode();
    if (!an)
        return false;
    Position bp = b.deepEquivalent();
    Node* bn = bp.deprecatedNode();
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

    return firstPositionInNode(highestRoot);
}

VisiblePosition endOfEditableContent(const VisiblePosition& visiblePosition)
{
    Node* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return VisiblePosition();

    return lastPositionInNode(highestRoot);
}

VisiblePosition leftBoundaryOfLine(const VisiblePosition& c, TextDirection direction)
{
    return direction == LTR ? logicalStartOfLine(c) : logicalEndOfLine(c);
}

VisiblePosition rightBoundaryOfLine(const VisiblePosition& c, TextDirection direction)
{
    return direction == LTR ? logicalEndOfLine(c) : logicalStartOfLine(c);
}

static const int invalidOffset = -1;
static const int offsetNotFound = -1;

static bool positionIsInBox(const VisiblePosition& wordBreak, const InlineBox* box, int& offsetOfWordBreak)
{
    if (wordBreak.isNull())
        return false;

    InlineBox* boxOfWordBreak;
    wordBreak.getInlineBoxAndOffset(boxOfWordBreak, offsetOfWordBreak);
    return box == boxOfWordBreak;
}

static VisiblePosition previousWordBreakInBoxInsideBlockWithSameDirectionality(const InlineBox* box, const VisiblePosition& previousWordBreak, int& offsetOfWordBreak)
{
    // In a LTR block, the word break should be on the left boundary of a word.
    // In a RTL block, the word break should be on the right boundary of a word.
    // Because nextWordPosition() returns the word break on the right boundary of the word for LTR text,
    // we need to use previousWordPosition() to traverse words within the inline boxes from right to left
    // to find the previous word break (i.e. the first word break on the left). The same applies to RTL text.
    
    bool hasSeenWordBreakInThisBox = previousWordBreak.isNotNull();

    VisiblePosition wordBreak;

    if (hasSeenWordBreakInThisBox)
        wordBreak = previousWordBreak;
    else {
        wordBreak = createLegacyEditingPosition(box->renderer()->node(), box->caretMaxOffset());

        // Return the rightmost word boundary of LTR box or leftmost word boundary of RTL box if
        // it is not in the previously visited boxes. For example, given a logical text 
        // "abc def     hij opq", there are 2 boxes: the "abc def " (starts at 0 and length is 8) 
        // and the "hij opq" (starts at 12 and length is 7). The word breaks are 
        // "abc |def |    hij |opq". We normally catch the word break between "def" and "hij" when
        // we visit the box that contains "hij opq", but this word break doesn't exist in the box
        // that contains "hij opq" when there are multiple spaces. So we detect it when we're
        // traversing the box that contains "abc def " instead.

        if ((box->isLeftToRightDirection() && box->nextLeafChild())
            || (!box->isLeftToRightDirection() && box->prevLeafChild())) {

            VisiblePosition positionAfterWord = nextBoundary(wordBreak, nextWordPositionBoundary);
            if (positionAfterWord.isNotNull()) {
                VisiblePosition positionBeforeWord = previousBoundary(positionAfterWord, previousWordPositionBoundary);
            
                if (positionIsInBox(positionBeforeWord, box, offsetOfWordBreak))
                    return positionBeforeWord;
            }
        }
    }
  
    wordBreak = previousBoundary(wordBreak, previousWordPositionBoundary);
    if (previousWordBreak == wordBreak)
        return VisiblePosition();

    return positionIsInBox(wordBreak, box, offsetOfWordBreak) ? wordBreak : VisiblePosition();
}

static VisiblePosition leftmostPositionInRTLBoxInLTRBlock(const InlineBox* box)
{
    // FIXME: Probably need to take care of bidi level too.
    Node* node = box->renderer()->node();
    InlineBox* previousLeaf = box->prevLeafChild();
    InlineBox* nextLeaf = box->nextLeafChild();   
    
    if (previousLeaf && !previousLeaf->isLeftToRightDirection())
        return createLegacyEditingPosition(node, box->caretMaxOffset());

    if (nextLeaf && !nextLeaf->isLeftToRightDirection()) {
        if (previousLeaf)
            return createLegacyEditingPosition(previousLeaf->renderer()->node(), previousLeaf->caretMaxOffset());

        InlineBox* lastRTLLeaf;
        do {
            lastRTLLeaf = nextLeaf;
            nextLeaf = nextLeaf->nextLeafChild();
        } while (nextLeaf && !nextLeaf->isLeftToRightDirection());
        return createLegacyEditingPosition(lastRTLLeaf->renderer()->node(), lastRTLLeaf->caretMinOffset());
    }

    return createLegacyEditingPosition(node, box->caretMinOffset());
}

static VisiblePosition rightmostPositionInLTRBoxInRTLBlock(const InlineBox* box)
{
    // FIXME: Probably need to take care of bidi level too.
    Node* node = box->renderer()->node();
    InlineBox* previousLeaf = box->prevLeafChild();
    InlineBox* nextLeaf = box->nextLeafChild();   
    
    if (nextLeaf && nextLeaf->isLeftToRightDirection())    
        return createLegacyEditingPosition(node, box->caretMaxOffset());

    if (previousLeaf && previousLeaf->isLeftToRightDirection()) {
        if (nextLeaf)
            return createLegacyEditingPosition(nextLeaf->renderer()->node(), nextLeaf->caretMaxOffset());

        InlineBox* firstLTRLeaf;
        do {
            firstLTRLeaf = previousLeaf;
            previousLeaf = previousLeaf->prevLeafChild();
        } while (previousLeaf && previousLeaf->isLeftToRightDirection());
        return createLegacyEditingPosition(firstLTRLeaf->renderer()->node(), firstLTRLeaf->caretMinOffset());
    }

    return createLegacyEditingPosition(node, box->caretMinOffset());
}
    
static VisiblePosition lastWordBreakInBox(const InlineBox* box, int& offsetOfWordBreak)
{
    // Add the leftmost word break for RTL box or rightmost word break for LTR box.
    InlineBox* previousLeaf = box->prevLeafChild();
    InlineBox* nextLeaf = box->nextLeafChild();
    VisiblePosition boundaryPosition;
    if (box->direction() == RTL && (!previousLeaf || previousLeaf->isLeftToRightDirection()))
        boundaryPosition = leftmostPositionInRTLBoxInLTRBlock(box);
    else if (box->direction() == LTR && (!nextLeaf || !nextLeaf->isLeftToRightDirection()))
        boundaryPosition = rightmostPositionInLTRBoxInRTLBlock(box);

    if (boundaryPosition.isNull())
        return VisiblePosition();            

    VisiblePosition wordBreak = nextBoundary(boundaryPosition, nextWordPositionBoundary);
    if (wordBreak.isNull())
        wordBreak = boundaryPosition;
    else if (wordBreak != boundaryPosition)
        wordBreak = previousBoundary(wordBreak, previousWordPositionBoundary);

    return positionIsInBox(wordBreak, box, offsetOfWordBreak) ? wordBreak : VisiblePosition();    
}

static bool positionIsVisuallyOrderedInBoxInBlockWithDifferentDirectionality(const VisiblePosition& wordBreak, const InlineBox* box, int& offsetOfWordBreak)
{
    int previousOffset = offsetOfWordBreak;
    return positionIsInBox(wordBreak, box, offsetOfWordBreak)
        && (previousOffset == invalidOffset || previousOffset < offsetOfWordBreak);
}
    
static VisiblePosition nextWordBreakInBoxInsideBlockWithDifferentDirectionality(
    const InlineBox* box, const VisiblePosition& previousWordBreak, int& offsetOfWordBreak, bool& isLastWordBreakInBox)
{
    // FIXME: Probably need to take care of bidi level too.
    
    // In a LTR block, the word break should be on the left boundary of a word.
    // In a RTL block, the word break should be on the right boundary of a word.
    // Because previousWordPosition() returns the word break on the right boundary of the word for RTL text,
    // we need to use nextWordPosition() to traverse words within the inline boxes from right to left to find the next word break.
    // The same applies to LTR text, in which words are traversed within the inline boxes from left to right.
    
    bool hasSeenWordBreakInThisBox = previousWordBreak.isNotNull();
    VisiblePosition wordBreak = hasSeenWordBreakInThisBox ? previousWordBreak : 
        createLegacyEditingPosition(box->renderer()->node(), box->caretMinOffset());

    wordBreak = nextBoundary(wordBreak, nextWordPositionBoundary);
  
    // Given RTL box "ABC DEF" either follows a LTR box or is the first visual box in an LTR block as an example,
    // the visual display of the RTL box is: "(0)J(10)I(9)H(8) (7)F(6)E(5)D(4) (3)C(2)B(1)A(11)",
    // where the number in parenthesis represents offset in visiblePosition. 
    // Start at offset 0, the first word break is at offset 3, the 2nd word break is at offset 7, and the 3rd word break should be at offset 0.
    // But nextWordPosition() of offset 7 is offset 11, which should be ignored, 
    // and the position at offset 0 should be manually added as the last word break within the box.
    if (wordBreak != previousWordBreak && positionIsVisuallyOrderedInBoxInBlockWithDifferentDirectionality(wordBreak, box, offsetOfWordBreak)) {
        isLastWordBreakInBox = false;
        return wordBreak;
    }
    
    isLastWordBreakInBox = true;
    return lastWordBreakInBox(box, offsetOfWordBreak);
}

struct WordBoundaryEntry {
    WordBoundaryEntry()
        : offsetInInlineBox(invalidOffset) 
    { 
    }

    WordBoundaryEntry(const VisiblePosition& position, int offset)
        : visiblePosition(position)
        , offsetInInlineBox(offset) 
    { 
    }

    VisiblePosition visiblePosition;
    int offsetInInlineBox;
};
    
typedef Vector<WordBoundaryEntry, 50> WordBoundaryVector;
    
static void collectWordBreaksInBoxInsideBlockWithSameDirectionality(const InlineBox* box, WordBoundaryVector& orderedWordBoundaries)
{
    orderedWordBoundaries.clear();

    VisiblePosition wordBreak;
    int offsetOfWordBreak = invalidOffset;
    while (1) {
        wordBreak = previousWordBreakInBoxInsideBlockWithSameDirectionality(box, wordBreak, offsetOfWordBreak);
        if (wordBreak.isNull())
            break;
        WordBoundaryEntry wordBoundaryEntry(wordBreak, offsetOfWordBreak);
        orderedWordBoundaries.append(wordBoundaryEntry);
    }
}

static void collectWordBreaksInBoxInsideBlockWithDifferntDirectionality(const InlineBox* box, WordBoundaryVector& orderedWordBoundaries)
{
    orderedWordBoundaries.clear();
    
    VisiblePosition wordBreak;
    int offsetOfWordBreak = invalidOffset;
    bool isLastWordBreakInBox = false;
    while (1) {
        wordBreak = nextWordBreakInBoxInsideBlockWithDifferentDirectionality(box, wordBreak, offsetOfWordBreak, isLastWordBreakInBox);
        if (wordBreak.isNotNull()) {
            WordBoundaryEntry wordBoundaryEntry(wordBreak, offsetOfWordBreak);
            orderedWordBoundaries.append(wordBoundaryEntry);
        }
        if (isLastWordBreakInBox)
            break;
    }
}

static void collectWordBreaksInBox(const InlineBox* box, WordBoundaryVector& orderedWordBoundaries, TextDirection blockDirection)
{
    if (box->direction() == blockDirection)
        collectWordBreaksInBoxInsideBlockWithSameDirectionality(box, orderedWordBoundaries);
    else
        collectWordBreaksInBoxInsideBlockWithDifferntDirectionality(box, orderedWordBoundaries);        
}
    
static VisiblePosition previousWordBoundaryInBox(const InlineBox* box, int offset)
{
    int offsetOfWordBreak = 0;
    VisiblePosition wordBreak;
    while (true) {
        wordBreak = previousWordBreakInBoxInsideBlockWithSameDirectionality(box, wordBreak, offsetOfWordBreak);
        if (wordBreak.isNull())
            break;
        if (offset == invalidOffset || offsetOfWordBreak != offset)
            return wordBreak;
    }        
    return VisiblePosition();
}

static VisiblePosition nextWordBoundaryInBox(const InlineBox* box, int offset)
{
    int offsetOfWordBreak = 0;
    VisiblePosition wordBreak;
    bool isLastWordBreakInBox = false;
    do {
        wordBreak = nextWordBreakInBoxInsideBlockWithDifferentDirectionality(box, wordBreak, offsetOfWordBreak, isLastWordBreakInBox);
        if (wordBreak.isNotNull() && (offset == invalidOffset || offsetOfWordBreak != offset))
            return wordBreak;
    } while (!isLastWordBreakInBox);       
    return VisiblePosition();
}
    
static VisiblePosition visuallyLastWordBoundaryInBox(const InlineBox* box, int offset, TextDirection blockDirection)
{
    WordBoundaryVector orderedWordBoundaries;
    collectWordBreaksInBox(box, orderedWordBoundaries, blockDirection);
    if (!orderedWordBoundaries.size()) 
        return VisiblePosition();
    if (offset == invalidOffset || orderedWordBoundaries[orderedWordBoundaries.size() - 1].offsetInInlineBox != offset)
        return orderedWordBoundaries[orderedWordBoundaries.size() - 1].visiblePosition;
    if (orderedWordBoundaries.size() > 1)
        return orderedWordBoundaries[orderedWordBoundaries.size() - 2].visiblePosition;
    return VisiblePosition();
}
        
static int greatestOffsetUnder(int offset, bool boxAndBlockAreInSameDirection, const WordBoundaryVector& orderedWordBoundaries)
{
    if (!orderedWordBoundaries.size())
        return offsetNotFound;
    // FIXME: binary search.
    if (boxAndBlockAreInSameDirection) {
        for (unsigned i = 0; i < orderedWordBoundaries.size(); ++i) {
            if (orderedWordBoundaries[i].offsetInInlineBox < offset)
                return i;
        }
        return offsetNotFound;
    }
    for (int i = orderedWordBoundaries.size() - 1; i >= 0; --i) {
        if (orderedWordBoundaries[i].offsetInInlineBox < offset)
            return i;
    }
    return offsetNotFound;
}

static int smallestOffsetAbove(int offset, bool boxAndBlockAreInSameDirection, const WordBoundaryVector& orderedWordBoundaries)
{
    if (!orderedWordBoundaries.size())
        return offsetNotFound;
    // FIXME: binary search.
    if (boxAndBlockAreInSameDirection) {
        for (int i = orderedWordBoundaries.size() - 1; i >= 0; --i) {
            if (orderedWordBoundaries[i].offsetInInlineBox > offset)
                return i;
        }
        return offsetNotFound;
    }
    for (unsigned i = 0; i < orderedWordBoundaries.size(); ++i) {
        if (orderedWordBoundaries[i].offsetInInlineBox > offset)
            return i;
    }
    return offsetNotFound;
}

static const RootInlineBox* previousRootInlineBox(const InlineBox* box)
{
    Node* node = box->renderer()->node();
    Node* enclosingBlockNode = enclosingNodeWithNonInlineRenderer(node);
    Node* previousNode = node->previousLeafNode();
    while (previousNode && enclosingBlockNode == enclosingNodeWithNonInlineRenderer(previousNode))
        previousNode = previousNode->previousLeafNode();
  
    while (previousNode && !previousNode->isShadowRoot()) {
        Position pos = createLegacyEditingPosition(previousNode, caretMaxOffset(previousNode));
        
        if (pos.isCandidate()) {
            RenderedPosition renderedPos(pos, DOWNSTREAM);
            RootInlineBox* root = renderedPos.rootBox();
            if (root)
                return root;
        }

        previousNode = previousNode->previousLeafNode();
    }
    return 0;
}

static const RootInlineBox* nextRootInlineBox(const InlineBox* box)
{
    Node* node = box->renderer()->node();
    Node* enclosingBlockNode = enclosingNodeWithNonInlineRenderer(node);
    Node* nextNode = node->nextLeafNode();
    while (nextNode && enclosingBlockNode == enclosingNodeWithNonInlineRenderer(nextNode))
        nextNode = nextNode->nextLeafNode();
  
    while (nextNode && !nextNode->isShadowRoot()) {
        Position pos;
        pos = createLegacyEditingPosition(nextNode, caretMinOffset(nextNode));
        
        if (pos.isCandidate()) {
            RenderedPosition renderedPos(pos, DOWNSTREAM);
            RootInlineBox* root = renderedPos.rootBox();
            if (root)
                return root;
        }

        nextNode = nextNode->nextLeafNode();
    }
    return 0;
}

static const InlineBox* leftInlineBox(const InlineBox* box, TextDirection blockDirection)
{
    if (box->prevLeafChild())
        return box->prevLeafChild();
    
    const RootInlineBox* rootBox = box->root();
    const bool isBlockLTR = blockDirection == LTR;
    const InlineFlowBox* leftLineBox = isBlockLTR ? rootBox->prevLineBox() : rootBox->nextLineBox();
    if (leftLineBox)
        return leftLineBox->lastLeafChild();

    const RootInlineBox* leftRootInlineBox = isBlockLTR ? previousRootInlineBox(box) :
        nextRootInlineBox(box); 
    return leftRootInlineBox ? leftRootInlineBox->lastLeafChild() : 0; 
}

static const InlineBox* rightInlineBox(const InlineBox* box, TextDirection blockDirection)
{
    if (box->nextLeafChild())
        return box->nextLeafChild();
    
    const RootInlineBox* rootBox = box->root();
    const bool isBlockLTR = blockDirection == LTR;
    const InlineFlowBox* rightLineBox = isBlockLTR ? rootBox->nextLineBox() : rootBox->prevLineBox();
    if (rightLineBox)
        return rightLineBox->firstLeafChild();

    const RootInlineBox* rightRootInlineBox = isBlockLTR ? nextRootInlineBox(box) :
        previousRootInlineBox(box); 
    return rightRootInlineBox ? rightRootInlineBox->firstLeafChild() : 0; 
}

static VisiblePosition leftWordBoundary(const InlineBox* box, int offset, TextDirection blockDirection)
{
    VisiblePosition wordBreak;
    for  (const InlineBox* adjacentBox = box; adjacentBox; adjacentBox = leftInlineBox(adjacentBox, blockDirection)) {
        if (blockDirection == LTR) {
            if (adjacentBox->isLeftToRightDirection()) 
                wordBreak = previousWordBoundaryInBox(adjacentBox, adjacentBox == box ? offset : invalidOffset);
            else
                wordBreak = nextWordBoundaryInBox(adjacentBox, adjacentBox == box ? offset : invalidOffset);
        } else 
            wordBreak = visuallyLastWordBoundaryInBox(adjacentBox, adjacentBox == box ? offset : invalidOffset, blockDirection);            
        if (wordBreak.isNotNull())
            return wordBreak;
    }
    return VisiblePosition();
}
 
static VisiblePosition rightWordBoundary(const InlineBox* box, int offset, TextDirection blockDirection)
{
    
    VisiblePosition wordBreak;
    for (const InlineBox* adjacentBox = box; adjacentBox; adjacentBox = rightInlineBox(adjacentBox, blockDirection)) {
        if (blockDirection == RTL) {
            if (adjacentBox->isLeftToRightDirection())
                wordBreak = nextWordBoundaryInBox(adjacentBox, adjacentBox == box ? offset : invalidOffset);
            else
                wordBreak = previousWordBoundaryInBox(adjacentBox, adjacentBox == box ? offset : invalidOffset);
        } else 
            wordBreak = visuallyLastWordBoundaryInBox(adjacentBox, adjacentBox == box ? offset : invalidOffset, blockDirection);            
        if (!wordBreak.isNull())
            return wordBreak;
    }
    return VisiblePosition();
}
    
static bool positionIsInBoxButNotOnBoundary(const VisiblePosition& wordBreak, const InlineBox* box)
{
    int offsetOfWordBreak;
    return positionIsInBox(wordBreak, box, offsetOfWordBreak)
        && offsetOfWordBreak != box->caretMaxOffset() && offsetOfWordBreak != box->caretMinOffset();
}

static VisiblePosition leftWordPositionIgnoringEditingBoundary(const VisiblePosition& visiblePosition)
{
    InlineBox* box;
    int offset;
    visiblePosition.getInlineBoxAndOffset(box, offset);

    if (!box)
        return VisiblePosition();

    TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());

    // FIXME: If the box's directionality is the same as that of the enclosing block, when the offset is at the box boundary
    // and the direction is towards inside the box, do I still need to make it a special case? For example, a LTR box inside a LTR block,
    // when offset is at box's caretMinOffset and the direction is DirectionRight, should it be taken care as a general case?
    if (offset == box->caretLeftmostOffset())
        return leftWordBoundary(leftInlineBox(box, blockDirection), invalidOffset, blockDirection);
    if (offset == box->caretRightmostOffset())
        return leftWordBoundary(box, offset, blockDirection);
    
    
    VisiblePosition wordBreak;
    if (blockDirection == LTR) {
        if (box->direction() == blockDirection)
            wordBreak = previousBoundary(visiblePosition, previousWordPositionBoundary);
        else
            wordBreak = nextBoundary(visiblePosition, nextWordPositionBoundary);
    }
    if (wordBreak.isNotNull() && positionIsInBoxButNotOnBoundary(wordBreak, box))
        return wordBreak;
    
    WordBoundaryVector orderedWordBoundaries;
    collectWordBreaksInBox(box, orderedWordBoundaries, blockDirection);

    int index = box->isLeftToRightDirection() ? greatestOffsetUnder(offset, blockDirection == LTR, orderedWordBoundaries)
        : smallestOffsetAbove(offset, blockDirection == RTL, orderedWordBoundaries);
    if (index >= 0)
        return orderedWordBoundaries[index].visiblePosition;
    
    return leftWordBoundary(leftInlineBox(box, blockDirection), invalidOffset, blockDirection);
}

static VisiblePosition rightWordPositionIgnoringEditingBoundary(const VisiblePosition& visiblePosition)
{
    InlineBox* box;
    int offset;
    visiblePosition.getInlineBoxAndOffset(box, offset);

    if (!box)
        return VisiblePosition();

    TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
    
    if (offset == box->caretLeftmostOffset())
        return rightWordBoundary(box, offset, blockDirection);
    if (offset == box->caretRightmostOffset())
        return rightWordBoundary(rightInlineBox(box, blockDirection), invalidOffset, blockDirection);
 
    VisiblePosition wordBreak;
    if (blockDirection == RTL) {
        if (box->direction() == blockDirection)
            wordBreak = previousBoundary(visiblePosition, previousWordPositionBoundary);
        else
            wordBreak = nextBoundary(visiblePosition, nextWordPositionBoundary);
    }
    if (wordBreak.isNotNull() && positionIsInBoxButNotOnBoundary(wordBreak, box))
        return wordBreak;
    
    WordBoundaryVector orderedWordBoundaries;
    collectWordBreaksInBox(box, orderedWordBoundaries, blockDirection);
    
    int index = box->isLeftToRightDirection() ? smallestOffsetAbove(offset, blockDirection == LTR, orderedWordBoundaries)
        : greatestOffsetUnder(offset, blockDirection == RTL, orderedWordBoundaries);
    if (index >= 0)
        return orderedWordBoundaries[index].visiblePosition;
    
    return rightWordBoundary(rightInlineBox(box, blockDirection), invalidOffset, blockDirection);
}

VisiblePosition leftWordPosition(const VisiblePosition& visiblePosition)
{
    if (visiblePosition.isNull())
        return VisiblePosition();

    VisiblePosition leftWordBreak = leftWordPositionIgnoringEditingBoundary(visiblePosition);
    leftWordBreak = visiblePosition.honorEditingBoundaryAtOrBefore(leftWordBreak);
    
    // FIXME: How should we handle a non-editable position?
    if (leftWordBreak.isNull() && isEditablePosition(visiblePosition.deepEquivalent())) {
        TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
        leftWordBreak = blockDirection == LTR ? startOfEditableContent(visiblePosition) : endOfEditableContent(visiblePosition);
    }
    return leftWordBreak;
}

VisiblePosition rightWordPosition(const VisiblePosition& visiblePosition)
{
    if (visiblePosition.isNull())
        return VisiblePosition();

    VisiblePosition rightWordBreak = rightWordPositionIgnoringEditingBoundary(visiblePosition);
    rightWordBreak = visiblePosition.honorEditingBoundaryAtOrBefore(rightWordBreak);

    // FIXME: How should we handle a non-editable position?
    if (rightWordBreak.isNull() && isEditablePosition(visiblePosition.deepEquivalent())) {
        TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
        rightWordBreak = blockDirection == LTR ? endOfEditableContent(visiblePosition) : startOfEditableContent(visiblePosition);
    }
    return rightWordBreak;
}

}
