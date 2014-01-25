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
#include "VisibleUnits.h"

#include "Document.h"
#include "Element.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "NodeTraversal.h"
#include "RenderBlockFlow.h"
#include "RenderObject.h"
#include "RenderedPosition.h"
#include "Text.h"
#include "TextBoundaries.h"
#include "TextBreakIterator.h"
#include "TextIterator.h"
#include "VisibleSelection.h"
#include "htmlediting.h"

namespace WebCore {

using namespace HTMLNames;
using namespace WTF::Unicode;

static Node* previousLeafWithSameEditability(Node* node, EditableType editableType)
{
    bool editable = node->hasEditableStyle(editableType);
    node = previousLeafNode(node);
    while (node) {
        if (editable == node->hasEditableStyle(editableType))
            return node;
        node = previousLeafNode(node);
    }
    return 0;
}

static Node* nextLeafWithSameEditability(Node* node, EditableType editableType = ContentIsEditable)
{
    if (!node)
        return 0;
    
    bool editable = node->hasEditableStyle(editableType);
    node = nextLeafNode(node);
    while (node) {
        if (editable == node->hasEditableStyle(editableType))
            return node;
        node = nextLeafNode(node);
    }
    return 0;
}

// FIXME: consolidate with code in previousLinePosition.
static Position previousRootInlineBoxCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    Node* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* previousNode = previousLeafWithSameEditability(node, editableType);

    while (previousNode && (!previousNode->renderer() || inSameLine(firstPositionInOrBeforeNode(previousNode), visiblePosition)))
        previousNode = previousLeafWithSameEditability(previousNode, editableType);

    while (previousNode && !previousNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(previousNode), editableType) != highestRoot)
            break;

        Position pos = previousNode->hasTagName(brTag) ? positionBeforeNode(previousNode) :
            createLegacyEditingPosition(previousNode, caretMaxOffset(previousNode));
        
        if (pos.isCandidate())
            return pos;

        previousNode = previousLeafWithSameEditability(previousNode, editableType);
    }
    return Position();
}

static Position nextRootInlineBoxCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    Node* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* nextNode = nextLeafWithSameEditability(node, editableType);
    while (nextNode && (!nextNode->renderer() || inSameLine(firstPositionInOrBeforeNode(nextNode), visiblePosition)))
        nextNode = nextLeafWithSameEditability(nextNode, ContentIsEditable);

    while (nextNode && !nextNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(nextNode), editableType) != highestRoot)
            break;

        Position pos;
        pos = createLegacyEditingPosition(nextNode, caretMinOffset(nextNode));
        
        if (pos.isCandidate())
            return pos;

        nextNode = nextLeafWithSameEditability(nextNode, editableType);
    }
    return Position();
}

class CachedLogicallyOrderedLeafBoxes {
public:
    CachedLogicallyOrderedLeafBoxes();

    const InlineBox* previousTextOrLineBreakBox(const RootInlineBox*, const InlineTextBox*);
    const InlineBox* nextTextOrLineBreakBox(const RootInlineBox*, const InlineTextBox*);

    size_t size() const { return m_leafBoxes.size(); }
    const InlineBox* firstBox() const { return m_leafBoxes[0]; }

private:
    const Vector<InlineBox*>& collectBoxes(const RootInlineBox*);
    int boxIndexInLeaves(const InlineTextBox*) const;

    const RootInlineBox* m_rootInlineBox;
    Vector<InlineBox*> m_leafBoxes;
};

CachedLogicallyOrderedLeafBoxes::CachedLogicallyOrderedLeafBoxes() : m_rootInlineBox(0) { };

const InlineBox* CachedLogicallyOrderedLeafBoxes::previousTextOrLineBreakBox(const RootInlineBox* root, const InlineTextBox* box)
{
    if (!root)
        return 0;

    collectBoxes(root);

    // If box is null, root is box's previous RootInlineBox, and previousBox is the last logical box in root.
    int boxIndex = m_leafBoxes.size() - 1;
    if (box)
        boxIndex = boxIndexInLeaves(box) - 1;

    for (int i = boxIndex; i >= 0; --i) {
        InlineBox* box = m_leafBoxes[i];
        if (box->isInlineTextBox() || box->renderer().isBR())
            return box;
    }

    return 0;
}

const InlineBox* CachedLogicallyOrderedLeafBoxes::nextTextOrLineBreakBox(const RootInlineBox* root, const InlineTextBox* box)
{
    if (!root)
        return 0;

    collectBoxes(root);

    // If box is null, root is box's next RootInlineBox, and nextBox is the first logical box in root.
    // Otherwise, root is box's RootInlineBox, and nextBox is the next logical box in the same line.
    size_t nextBoxIndex = 0;
    if (box)
        nextBoxIndex = boxIndexInLeaves(box) + 1;

    for (size_t i = nextBoxIndex; i < m_leafBoxes.size(); ++i) {
        InlineBox* box = m_leafBoxes[i];
        if (box->isInlineTextBox() || box->renderer().isBR())
            return box;
    }

    return 0;
}

const Vector<InlineBox*>& CachedLogicallyOrderedLeafBoxes::collectBoxes(const RootInlineBox* root)
{
    if (m_rootInlineBox != root) {
        m_rootInlineBox = root;
        m_leafBoxes.clear();
        root->collectLeafBoxesInLogicalOrder(m_leafBoxes);
    }
    return m_leafBoxes;
}

int CachedLogicallyOrderedLeafBoxes::boxIndexInLeaves(const InlineTextBox* box) const
{
    for (size_t i = 0; i < m_leafBoxes.size(); ++i) {
        if (box == m_leafBoxes[i])
            return i;
    }
    return 0;
}

static const InlineBox* logicallyPreviousBox(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& previousBoxInDifferentBlock, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    const InlineBox* startBox = textBox;

    const InlineBox* previousBox = leafBoxes.previousTextOrLineBreakBox(&startBox->root(), textBox);
    if (previousBox)
        return previousBox;

    previousBox = leafBoxes.previousTextOrLineBreakBox(startBox->root().prevRootBox(), 0);
    if (previousBox)
        return previousBox;

    while (1) {
        Node* startNode = startBox->renderer().nonPseudoNode();
        if (!startNode)
            break;

        Position position = previousRootInlineBoxCandidatePosition(startNode, visiblePosition, ContentIsEditable);
        if (position.isNull())
            break;

        RenderedPosition renderedPosition(position, DOWNSTREAM);
        RootInlineBox* previousRoot = renderedPosition.rootBox();
        if (!previousRoot)
            break;

        previousBox = leafBoxes.previousTextOrLineBreakBox(previousRoot, 0);
        if (previousBox) {
            previousBoxInDifferentBlock = true;
            return previousBox;
        }

        if (!leafBoxes.size())
            break;
        startBox = leafBoxes.firstBox();
    }
    return 0;
}


static const InlineBox* logicallyNextBox(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& nextBoxInDifferentBlock, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    const InlineBox* startBox = textBox;

    const InlineBox* nextBox = leafBoxes.nextTextOrLineBreakBox(&startBox->root(), textBox);
    if (nextBox)
        return nextBox;

    nextBox = leafBoxes.nextTextOrLineBreakBox(startBox->root().nextRootBox(), 0);
    if (nextBox)
        return nextBox;

    while (1) {
        Node* startNode = startBox->renderer().nonPseudoNode();
        if (!startNode)
            break;

        Position position = nextRootInlineBoxCandidatePosition(startNode, visiblePosition, ContentIsEditable);
        if (position.isNull())
            break;

        RenderedPosition renderedPosition(position, DOWNSTREAM);
        RootInlineBox* nextRoot = renderedPosition.rootBox();
        if (!nextRoot)
            break;

        nextBox = leafBoxes.nextTextOrLineBreakBox(nextRoot, 0);
        if (nextBox) {
            nextBoxInDifferentBlock = true;
            return nextBox;
        }

        if (!leafBoxes.size())
            break;
        startBox = leafBoxes.firstBox();
    }
    return 0;
}

static TextBreakIterator* wordBreakIteratorForMinOffsetBoundary(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    int& previousBoxLength, bool& previousBoxInDifferentBlock, Vector<UChar, 1024>& string, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    previousBoxInDifferentBlock = false;

    // FIXME: Handle the case when we don't have an inline text box.
    const InlineBox* previousBox = logicallyPreviousBox(visiblePosition, textBox, previousBoxInDifferentBlock, leafBoxes);

    int len = 0;
    string.clear();
    if (previousBox && previousBox->isInlineTextBox()) {
        const InlineTextBox* previousTextBox = toInlineTextBox(previousBox);
        previousBoxLength = previousTextBox->len();
        string.append(previousTextBox->renderer().text()->deprecatedCharacters() + previousTextBox->start(), previousBoxLength);
        len += previousBoxLength;
    }
    string.append(textBox->renderer().text()->deprecatedCharacters() + textBox->start(), textBox->len());
    len += textBox->len();

    return wordBreakIterator(StringView(string.data(), len));
}

static TextBreakIterator* wordBreakIteratorForMaxOffsetBoundary(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& nextBoxInDifferentBlock, Vector<UChar, 1024>& string, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    nextBoxInDifferentBlock = false;

    // FIXME: Handle the case when we don't have an inline text box.
    const InlineBox* nextBox = logicallyNextBox(visiblePosition, textBox, nextBoxInDifferentBlock, leafBoxes);

    int len = 0;
    string.clear();
    string.append(textBox->renderer().text()->deprecatedCharacters() + textBox->start(), textBox->len());
    len += textBox->len();
    if (nextBox && nextBox->isInlineTextBox()) {
        const InlineTextBox* nextTextBox = toInlineTextBox(nextBox);
        string.append(nextTextBox->renderer().text()->deprecatedCharacters() + nextTextBox->start(), nextTextBox->len());
        len += nextTextBox->len();
    }

    return wordBreakIterator(StringView(string.data(), len));
}

static bool isLogicalStartOfWord(TextBreakIterator* iter, int position, bool hardLineBreak)
{
    bool boundary = hardLineBreak ? true : isTextBreak(iter, position);
    if (!boundary)
        return false;

    textBreakFollowing(iter, position);
    // isWordTextBreak returns true after moving across a word and false after moving across a punctuation/space.
    return isWordTextBreak(iter);
}

static bool islogicalEndOfWord(TextBreakIterator* iter, int position, bool hardLineBreak)
{
    bool boundary = isTextBreak(iter, position);
    return (hardLineBreak || boundary) && isWordTextBreak(iter);
}

enum CursorMovementDirection { MoveLeft, MoveRight };

static VisiblePosition visualWordPosition(const VisiblePosition& visiblePosition, CursorMovementDirection direction, 
    bool skipsSpaceWhenMovingRight)
{
    if (visiblePosition.isNull())
        return VisiblePosition();

    TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
    InlineBox* previouslyVisitedBox = 0;
    VisiblePosition current = visiblePosition;
    TextBreakIterator* iter = 0;

    CachedLogicallyOrderedLeafBoxes leafBoxes;
    Vector<UChar, 1024> string;

    while (1) {
        VisiblePosition adjacentCharacterPosition = direction == MoveRight ? current.right(true) : current.left(true); 
        if (adjacentCharacterPosition == current || adjacentCharacterPosition.isNull())
            return VisiblePosition();
    
        InlineBox* box;
        int offsetInBox;
        adjacentCharacterPosition.deepEquivalent().getInlineBoxAndOffset(UPSTREAM, box, offsetInBox);
    
        if (!box)
            break;
        if (!box->isInlineTextBox()) {
            current = adjacentCharacterPosition;
            continue;
        }

        InlineTextBox* textBox = toInlineTextBox(box);
        int previousBoxLength = 0;
        bool previousBoxInDifferentBlock = false;
        bool nextBoxInDifferentBlock = false;
        bool movingIntoNewBox = previouslyVisitedBox != box;

        if (offsetInBox == box->caretMinOffset())
            iter = wordBreakIteratorForMinOffsetBoundary(visiblePosition, textBox, previousBoxLength, previousBoxInDifferentBlock, string, leafBoxes);
        else if (offsetInBox == box->caretMaxOffset())
            iter = wordBreakIteratorForMaxOffsetBoundary(visiblePosition, textBox, nextBoxInDifferentBlock, string, leafBoxes);
        else if (movingIntoNewBox) {
            iter = wordBreakIterator(StringView(textBox->renderer().text()).substring(textBox->start(), textBox->len()));
            previouslyVisitedBox = box;
        }

        if (!iter)
            break;

        textBreakFirst(iter);
        int offsetInIterator = offsetInBox - textBox->start() + previousBoxLength;

        bool isWordBreak;
        bool boxHasSameDirectionalityAsBlock = box->direction() == blockDirection;
        bool movingBackward = (direction == MoveLeft && box->direction() == LTR) || (direction == MoveRight && box->direction() == RTL);
        if ((skipsSpaceWhenMovingRight && boxHasSameDirectionalityAsBlock)
            || (!skipsSpaceWhenMovingRight && movingBackward)) {
            bool logicalStartInRenderer = offsetInBox == static_cast<int>(textBox->start()) && previousBoxInDifferentBlock;
            isWordBreak = isLogicalStartOfWord(iter, offsetInIterator, logicalStartInRenderer);
        } else {
            bool logicalEndInRenderer = offsetInBox == static_cast<int>(textBox->start() + textBox->len()) && nextBoxInDifferentBlock;
            isWordBreak = islogicalEndOfWord(iter, offsetInIterator, logicalEndInRenderer);
        }      

        if (isWordBreak)
            return adjacentCharacterPosition;
    
        current = adjacentCharacterPosition;
    }
    return VisiblePosition();
}

VisiblePosition leftWordPosition(const VisiblePosition& visiblePosition, bool skipsSpaceWhenMovingRight)
{
    VisiblePosition leftWordBreak = visualWordPosition(visiblePosition, MoveLeft, skipsSpaceWhenMovingRight);
    leftWordBreak = visiblePosition.honorEditingBoundaryAtOrBefore(leftWordBreak);
    
    // FIXME: How should we handle a non-editable position?
    if (leftWordBreak.isNull() && isEditablePosition(visiblePosition.deepEquivalent())) {
        TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
        leftWordBreak = blockDirection == LTR ? startOfEditableContent(visiblePosition) : endOfEditableContent(visiblePosition);
    }
    return leftWordBreak;
}

VisiblePosition rightWordPosition(const VisiblePosition& visiblePosition, bool skipsSpaceWhenMovingRight)
{
    VisiblePosition rightWordBreak = visualWordPosition(visiblePosition, MoveRight, skipsSpaceWhenMovingRight);
    rightWordBreak = visiblePosition.honorEditingBoundaryAtOrBefore(rightWordBreak);

    // FIXME: How should we handle a non-editable position?
    if (rightWordBreak.isNull() && isEditablePosition(visiblePosition.deepEquivalent())) {
        TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
        rightWordBreak = blockDirection == LTR ? endOfEditableContent(visiblePosition) : startOfEditableContent(visiblePosition);
    }
    return rightWordBreak;
}


enum BoundarySearchContextAvailability { DontHaveMoreContext, MayHaveMoreContext };

typedef unsigned (*BoundarySearchFunction)(const UChar*, unsigned length, unsigned offset, BoundarySearchContextAvailability, bool& needMoreContext);

static VisiblePosition previousBoundary(const VisiblePosition& c, BoundarySearchFunction searchFunction)
{
    Position pos = c.deepEquivalent();
    Node* boundary = pos.parentEditingBoundary();
    if (!boundary)
        return VisiblePosition();

    Document& boundaryDocument = boundary->document();
    Position start = createLegacyEditingPosition(boundary, 0).parentAnchoredEquivalent();
    Position end = pos.parentAnchoredEquivalent();
    RefPtr<Range> searchRange = Range::create(boundaryDocument);
    
    Vector<UChar, 1024> string;
    unsigned suffixLength = 0;

    ExceptionCode ec = 0;
    if (requiresContextForWordBoundary(c.characterBefore())) {
        RefPtr<Range> forwardsScanRange(boundaryDocument.createRange());
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
    bool needMoreContext = false;
    while (!it.atEnd()) {
        bool inTextSecurityMode = it.node() && it.node()->renderer() && it.node()->renderer()->style().textSecurity() != TSNONE;
        // iterate to get chunks until the searchFunction returns a non-zero value.
        if (!inTextSecurityMode) 
            string.insert(0, it.characters(), it.length());
        else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            String iteratorString(it.characters(), it.length());
#if PLATFORM(IOS)
            iteratorString = iteratorString.impl()->fill('x');
#else
            iteratorString.fill('x');
#endif
            string.insert(0, iteratorString.deprecatedCharacters(), iteratorString.length());
        }
        next = searchFunction(string.data(), string.size(), string.size() - suffixLength, MayHaveMoreContext, needMoreContext);
        if (next > 1) // FIXME: This is a work around for https://webkit.org/b/115070. We need to provide more contexts in general case.
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

    Node* node = it.range()->startContainer();
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

    Document& boundaryDocument = boundary->document();
    RefPtr<Range> searchRange(boundaryDocument.createRange());
    Position start(pos.parentAnchoredEquivalent());

    Vector<UChar, 1024> string;
    unsigned prefixLength = 0;

    if (requiresContextForWordBoundary(c.characterAfter())) {
        RefPtr<Range> backwardsScanRange(boundaryDocument.createRange());
        backwardsScanRange->setEnd(start.deprecatedNode(), start.deprecatedEditingOffset(), IGNORE_EXCEPTION);
        SimplifiedBackwardsTextIterator backwardsIterator(backwardsScanRange.get());
        while (!backwardsIterator.atEnd()) {
            const UChar* characters = backwardsIterator.characters();
            int length = backwardsIterator.length();
            int i = startOfLastWordBoundaryContext(characters, length);
            string.insert(0, characters + i, length - i);
            prefixLength += length - i;
            if (i > 0)
                break;
            backwardsIterator.advance();
        }
    }

    searchRange->selectNodeContents(boundary, IGNORE_EXCEPTION);
    searchRange->setStart(start.deprecatedNode(), start.deprecatedEditingOffset(), IGNORE_EXCEPTION);
    TextIterator it(searchRange.get(), TextIteratorEmitsCharactersBetweenAllVisiblePositions);
    unsigned next = 0;
    bool needMoreContext = false;
    while (!it.atEnd()) {
        bool inTextSecurityMode = it.node() && it.node()->renderer() && it.node()->renderer()->style().textSecurity() != TSNONE;
        // Keep asking the iterator for chunks until the search function
        // returns an end value not equal to the length of the string passed to it.
        if (!inTextSecurityMode)
            string.append(it.characters(), it.length());
        else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            String iteratorString(it.characters(), it.length());
#if PLATFORM(IOS)
            iteratorString = iteratorString.impl()->fill('x');
#else
            iteratorString.fill('x');
#endif
            string.append(iteratorString.deprecatedCharacters(), iteratorString.length());
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
#if PLATFORM(IOS)
    // FIXME: Bug 126830: [iOS] Implement WebCore::findEndWordBoundary()
    int start, end;
    findWordBoundary(characters, length, offset, &start, &end);
#else
    int end;
    findEndWordBoundary(characters, length, offset, &end);
#endif
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

            startNode = startBox->renderer().nonPseudoNode();
            if (startNode)
                break;

            startBox = startBox->nextLeafChild();
        }
    }

    return startNode->isTextNode() ? Position(toText(startNode), toInlineTextBox(startBox)->start())
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

            endNode = endBox->renderer().nonPseudoNode();
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
        pos = Position(toText(endNode), endOffset);
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

    // Make sure the end of line is at the same line as the given input position. Else use the previous position to 
    // obtain end of line. This condition happens when the input position is before the space character at the end 
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

static inline IntPoint absoluteLineDirectionPointToLocalPointInBlock(RootInlineBox* root, int lineDirectionPoint)
{
    ASSERT(root);
    RenderBlockFlow& containingBlock = root->blockFlow();
    FloatPoint absoluteBlockPoint = containingBlock.localToAbsolute(FloatPoint());
    if (containingBlock.hasOverflowClip())
        absoluteBlockPoint -= containingBlock.scrolledContentOffset();

    if (containingBlock.isHorizontalWritingMode())
        return IntPoint(lineDirectionPoint - absoluteBlockPoint.x(), root->blockDirectionPointInLine());

    return IntPoint(root->blockDirectionPointInLine(), lineDirectionPoint - absoluteBlockPoint.y());
}

VisiblePosition previousLinePosition(const VisiblePosition &visiblePosition, int lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.deprecatedNode();

    if (!node)
        return VisiblePosition();
    
    node->document().updateLayoutIgnorePendingStylesheets();
    
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RootInlineBox* root = 0;
    InlineBox* box;
    int ignoredCaretOffset;
    visiblePosition.getInlineBoxAndOffset(box, ignoredCaretOffset);
    if (box) {
        root = box->root().prevRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight() || !root->firstLeafChild())
            root = 0;
    }

    if (!root) {
        Position position = previousRootInlineBoxCandidatePosition(node, visiblePosition, editableType);
        if (position.isNotNull()) {
            RenderedPosition renderedPosition(position);
            root = renderedPosition.rootBox();
            if (!root)
                return position;
        }
    }
    
    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        IntPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(root, lineDirectionPoint);
        RenderObject& renderer = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p))->renderer();
        Node* node = renderer.node();
        if (node && editingIgnoresContent(node))
            return positionInParentBeforeNode(node);
        return renderer.positionForPoint(pointInLine);
    }
    
    // Could not find a previous line. This means we must already be on the first line.
    // Move to the start of the content in this block, which effectively moves us
    // to the start of the line we're on.
    Element* rootElement = node->hasEditableStyle(editableType) ? node->rootEditableElement(editableType) : node->document().documentElement();
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition(firstPositionInNode(rootElement), DOWNSTREAM);
}

VisiblePosition nextLinePosition(const VisiblePosition &visiblePosition, int lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.deprecatedNode();

    if (!node)
        return VisiblePosition();
    
    node->document().updateLayoutIgnorePendingStylesheets();

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RootInlineBox* root = 0;
    InlineBox* box;
    int ignoredCaretOffset;
    visiblePosition.getInlineBoxAndOffset(box, ignoredCaretOffset);
    if (box) {
        root = box->root().nextRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight() || !root->firstLeafChild())
            root = 0;
    }

    if (!root) {
        // FIXME: We need do the same in previousLinePosition.
        Node* child = node->childNode(p.deprecatedEditingOffset());
        node = child ? child : node->lastDescendant();
        Position position = nextRootInlineBoxCandidatePosition(node, visiblePosition, editableType);
        if (position.isNotNull()) {
            RenderedPosition renderedPosition(position);
            root = renderedPosition.rootBox();
            if (!root)
                return position;
        }
    }
    
    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        IntPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(root, lineDirectionPoint);
        RenderObject& renderer = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p))->renderer();
        Node* node = renderer.node();
        if (node && editingIgnoresContent(node))
            return positionInParentBeforeNode(node);
        return renderer.positionForPoint(pointInLine);
    }

    // Could not find a next line. This means we must already be on the last line.
    // Move to the end of the content in this block, which effectively moves us
    // to the end of the line we're on.
    Element* rootElement = node->hasEditableStyle(editableType) ? node->rootEditableElement(editableType) : node->document().documentElement();
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition(lastPositionInNode(rootElement), DOWNSTREAM);
}

// ---------

static unsigned startSentenceBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    TextBreakIterator* iterator = sentenceBreakIterator(StringView(characters, length));
    // FIXME: The following function can return -1; we don't handle that.
    return textBreakPreceding(iterator, length);
}

VisiblePosition startOfSentence(const VisiblePosition &c)
{
    return previousBoundary(c, startSentenceBoundary);
}

static unsigned endSentenceBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    TextBreakIterator* iterator = sentenceBreakIterator(StringView(characters, length));
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
    TextBreakIterator* iterator = sentenceBreakIterator(StringView(characters, length));
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
    // FIXME: This is identical to endSentenceBoundary. This isn't right, it needs to 
    // move to the equivlant position in the following sentence.
    TextBreakIterator* iterator = sentenceBreakIterator(StringView(characters, length));
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
#if ENABLE(USERSELECT_ALL)
        if (boundaryCrossingRule == CannotCrossEditingBoundary && !Position::nodeIsUserSelectAll(n) && n->hasEditableStyle() != startNode->hasEditableStyle())
#else
        if (boundaryCrossingRule == CannotCrossEditingBoundary && n->hasEditableStyle() != startNode->hasEditableStyle())
#endif
            break;
        if (boundaryCrossingRule == CanSkipOverEditingBoundary) {
            while (n && n->hasEditableStyle() != startNode->hasEditableStyle())
                n = NodeTraversal::previousPostOrder(n, startBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }
        RenderObject* r = n->renderer();
        if (!r) {
            n = NodeTraversal::previousPostOrder(n, startBlock);
            continue;
        }
        const RenderStyle& style = r->style();
        if (style.visibility() != VISIBLE) {
            n = NodeTraversal::previousPostOrder(n, startBlock);
            continue;
        }
        
        if (r->isBR() || isBlock(n))
            break;

        if (r->isText() && toRenderText(r)->hasRenderedText()) {
            ASSERT_WITH_SECURITY_IMPLICATION(n->isTextNode());
            type = Position::PositionIsOffsetInAnchor;
            if (style.preserveNewline()) {
                const UChar* chars = toRenderText(r)->deprecatedCharacters();
                int i = toRenderText(r)->textLength();
                int o = offset;
                if (n == startNode && o < i)
                    i = std::max(0, o);
                while (--i >= 0) {
                    if (chars[i] == '\n')
                        return VisiblePosition(Position(toText(n), i + 1), DOWNSTREAM);
                }
            }
            node = n;
            offset = 0;
            n = NodeTraversal::previousPostOrder(n, startBlock);
        } else if (editingIgnoresContent(n) || isTableElement(n)) {
            node = n;
            type = Position::PositionIsBeforeAnchor;
            n = n->previousSibling() ? n->previousSibling() : NodeTraversal::previousPostOrder(n, startBlock);
        } else
            n = NodeTraversal::previousPostOrder(n, startBlock);
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
#if ENABLE(USERSELECT_ALL)
        if (boundaryCrossingRule == CannotCrossEditingBoundary && !Position::nodeIsUserSelectAll(n) && n->hasEditableStyle() != startNode->hasEditableStyle())
#else
        if (boundaryCrossingRule == CannotCrossEditingBoundary && n->hasEditableStyle() != startNode->hasEditableStyle())
#endif
            break;
        if (boundaryCrossingRule == CanSkipOverEditingBoundary) {
            while (n && n->hasEditableStyle() != startNode->hasEditableStyle())
                n = NodeTraversal::next(n, stayInsideBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }

        RenderObject* r = n->renderer();
        if (!r) {
            n = NodeTraversal::next(n, stayInsideBlock);
            continue;
        }
        const RenderStyle& style = r->style();
        if (style.visibility() != VISIBLE) {
            n = NodeTraversal::next(n, stayInsideBlock);
            continue;
        }
        
        if (r->isBR() || isBlock(n))
            break;

        // FIXME: We avoid returning a position where the renderer can't accept the caret.
        if (r->isText() && toRenderText(r)->hasRenderedText()) {
            ASSERT_WITH_SECURITY_IMPLICATION(n->isTextNode());
            int length = toRenderText(r)->textLength();
            type = Position::PositionIsOffsetInAnchor;
            if (style.preserveNewline()) {
                const UChar* chars = toRenderText(r)->deprecatedCharacters();
                int o = n == startNode ? offset : 0;
                for (int i = o; i < length; ++i) {
                    if (chars[i] == '\n')
                        return VisiblePosition(Position(toText(n), i), DOWNSTREAM);
                }
            }
            node = n;
            offset = r->caretMaxOffset();
            n = NodeTraversal::next(n, stayInsideBlock);
        } else if (editingIgnoresContent(n) || isTableElement(n)) {
            node = n;
            type = Position::PositionIsAfterAnchor;
            n = NodeTraversal::nextSkippingChildren(n, stayInsideBlock);
        } else
            n = NodeTraversal::next(n, stayInsideBlock);
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
    if (!node || !node->document().documentElement())
        return VisiblePosition();
    
#if PLATFORM(IOS)
    // The canonicalization of the position at (documentElement, 0) can turn the visible
    // position to null, even when there's a valid candidate to be had, because the root HTML element
    // is not content editable.  So we construct directly from the valid candidate.
    // FIXME: Merge this to Open Source. https://bugs.webkit.org/show_bug.cgi?id=56437
    Position firstCandidate = nextCandidate(createLegacyEditingPosition(node->document().documentElement(), 0));
    if (firstCandidate.isNull())
        return VisiblePosition();
    return VisiblePosition(firstCandidate);
#else
    return VisiblePosition(firstPositionInNode(node->document().documentElement()), DOWNSTREAM);
#endif
}

VisiblePosition startOfDocument(const VisiblePosition &c)
{
    return startOfDocument(c.deepEquivalent().deprecatedNode());
}

VisiblePosition endOfDocument(const Node* node)
{
    if (!node || !node->document().documentElement())
        return VisiblePosition();
    
#if PLATFORM(IOS)
    // (As above, in startOfDocument.)  The canonicalization can reject valid visible positions
    // when descending from the root element, so we construct the visible position directly from a
    // valid candidate.
    // FIXME: Merge this to Open Source. https://bugs.webkit.org/show_bug.cgi?id=56437
#endif
    Element* doc = node->document().documentElement();
#if PLATFORM(IOS)
    Position lastPosition = createLegacyEditingPosition(node->document().documentElement(), doc->childNodeCount());
    Position lastCandidate = previousCandidate(lastPosition);
    if (lastCandidate.isNull())
        return VisiblePosition();
    return VisiblePosition(lastCandidate);
#endif
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

    return &an->document() == &bn->document();
}

bool isStartOfDocument(const VisiblePosition &p)
{
    return p.isNotNull() && p.previous(CanCrossEditingBoundary).isNull();
}

bool isEndOfDocument(const VisiblePosition &p)
{
    return p.isNotNull() && p.next(CanCrossEditingBoundary).isNull();
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

bool isEndOfEditableOrNonEditableContent(const VisiblePosition &p)
{
    return p.isNotNull() && p.next().isNull();
}

VisiblePosition leftBoundaryOfLine(const VisiblePosition& c, TextDirection direction)
{
    return direction == LTR ? logicalStartOfLine(c) : logicalEndOfLine(c);
}

VisiblePosition rightBoundaryOfLine(const VisiblePosition& c, TextDirection direction)
{
    return direction == LTR ? logicalEndOfLine(c) : logicalStartOfLine(c);
}

#if PLATFORM(IOS)
static bool directionIsDownstream(SelectionDirection direction)
{
    if (direction == DirectionBackward)
        return false;
    else if (direction == DirectionForward)
        return true;

    // FIXME: this code doesn't take into account the original direction of the element.
    // I'm not fixing this now because I'm afraid there is some code in UIKit relying on
    // this wrong behavior.
    return direction == DirectionRight;
}

bool atBoundaryOfGranularity(const VisiblePosition& vp, TextGranularity granularity, SelectionDirection direction)
{
    if (granularity == CharacterGranularity)
        return true;

    VisiblePosition boundary;

    bool useDownstream = directionIsDownstream(direction);

    switch (granularity) {
    case WordGranularity:
        // visible_units claims erroneously that the start and the end
        // of a paragraph are the end and start of a word, respectively.
        if ((useDownstream && isStartOfParagraph(vp)) || (!useDownstream && isEndOfParagraph(vp)))
            return false;

        // Note that "Left" and "Right" in this context apparently mean "upstream/previous" and "downstream/next".
        boundary = useDownstream ? endOfWord(vp, LeftWordIfOnBoundary) : startOfWord(vp, RightWordIfOnBoundary);
        break;

    case SentenceGranularity:
        boundary = useDownstream ? endOfSentence(vp) : startOfSentence(vp);
        break;

    case LineGranularity:
        // Affinity has to be set to get right boundary of the line.
        boundary = vp;
        boundary.setAffinity(useDownstream ? VP_UPSTREAM_IF_POSSIBLE : DOWNSTREAM);
        boundary = useDownstream ? endOfLine(boundary) : startOfLine(boundary);
        break;

    case ParagraphGranularity:
        boundary = useDownstream ? endOfParagraph(vp) : startOfParagraph(vp);
        break;

    case DocumentGranularity:
        boundary = useDownstream ? endOfDocument(vp) : startOfDocument(vp);
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return vp == boundary;
}

bool withinTextUnitOfGranularity(const VisiblePosition& vp, TextGranularity granularity, SelectionDirection direction)
{
    if (granularity == CharacterGranularity || granularity == DocumentGranularity)
        return true;

    bool useDownstream = directionIsDownstream(direction);

    VisiblePosition prevBoundary;
    VisiblePosition nextBoundary;
    
    switch (granularity) {
    case WordGranularity:
        // Note that "Left" and "Right" in this context apparently mean "upstream/previous" and "downstream/next".
        prevBoundary = startOfWord(vp, (useDownstream ? RightWordIfOnBoundary : LeftWordIfOnBoundary));
        nextBoundary = endOfWord(vp, (useDownstream ? RightWordIfOnBoundary : LeftWordIfOnBoundary));
    
        // Workaround for <rdar://problem/7259611> Word boundary code on iPhone gives different results than desktop
        if (endOfWord(prevBoundary, RightWordIfOnBoundary) != nextBoundary)
            return false;

        break;

    case SentenceGranularity:
        prevBoundary = startOfSentence(vp);
        nextBoundary = endOfSentence(vp);
        break;

    case LineGranularity:
        prevBoundary = startOfLine(vp);
        nextBoundary = endOfLine(vp);

        if (prevBoundary == nextBoundary) {
            nextBoundary = nextLinePosition(nextBoundary, 0);
            nextBoundary.setAffinity(UPSTREAM);
            if (!inSameLine(prevBoundary, nextBoundary))
                nextBoundary = vp.next();
        }
        break;

    case ParagraphGranularity:
        prevBoundary = startOfParagraph(vp);
        nextBoundary = endOfParagraph(vp);
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (prevBoundary == nextBoundary)
        return false;

    if (vp == prevBoundary)
        return useDownstream;

    if (vp == nextBoundary)
        return !useDownstream;

    return (prevBoundary < vp && vp < nextBoundary);
}

static VisiblePosition nextCharacterBoundaryInDirection(const VisiblePosition& vp, SelectionDirection direction)
{
    return directionIsDownstream(direction) ? vp.next() : vp.previous();
}

static VisiblePosition nextWordBoundaryInDirection(const VisiblePosition& vp, SelectionDirection direction)
{
    bool useDownstream = directionIsDownstream(direction);
    bool withinUnitOfGranularity = withinTextUnitOfGranularity(vp, WordGranularity, direction);
    VisiblePosition result;
    
    if (useDownstream) {
        if (withinUnitOfGranularity)
            result = endOfWord(vp, RightWordIfOnBoundary);
        else {
            VisiblePosition start = startOfWord(vp, RightWordIfOnBoundary);
            if (start > vp && start != endOfWord(start))
                result = start;
            else {
                // Do same thing as backwards traveling below.
                start = vp;
                while (true) {
                    result = startOfWord(nextWordPosition(start), RightWordIfOnBoundary);

                    if (result == start)
                        break;

                    // We failed to find a word boundary.
                    if (result.isNull() || result < start)
                        return VisiblePosition();

                    // We consider successs also the case where start is before element and result is after.
                    // This covers moving past images like words.
                    if (result != endOfWord(result)
                        || (result.deepEquivalent().anchorNode() == start.deepEquivalent().anchorNode()
                            && result.deepEquivalent().anchorType() == Position::PositionIsAfterAnchor
                            && start.deepEquivalent().anchorType() == Position::PositionIsBeforeAnchor))
                        break;

                    start = result;
                }
            }
        }
    } else {
        if (withinUnitOfGranularity)
            result = startOfWord(vp, LeftWordIfOnBoundary);
        else {
            // This is complicated because:
            //   When given "Blah blah.|", endOfWord is "Blah blah|.", and previousWordPosition is "Blah| blah."
            //   When given "Blah blah. |", endOfWord is "Blah blah.| ", and previousWordPosition is "Blah |blah. ".
            VisiblePosition end = endOfWord(vp, LeftWordIfOnBoundary);
            if (end < vp && end != startOfWord(end))
                result = end;
            else {
                end = vp;
                while (true) {
                    result = endOfWord(previousWordPosition(end), RightWordIfOnBoundary);

                    if (result == end)
                        break;

                    if (result.isNull() || result > end)
                        return VisiblePosition();

                    if (result != startOfWord(result))
                        break;

                    end = result;
                }
            }
        }
    }
    
    if (result == vp)
        return VisiblePosition();
    
    return result;
}

static VisiblePosition nextSentenceBoundaryInDirection(const VisiblePosition& vp, SelectionDirection direction)
{
    bool useDownstream = directionIsDownstream(direction);
    bool withinUnitOfGranularity = withinTextUnitOfGranularity(vp, SentenceGranularity, direction);
    VisiblePosition result;

    if (withinUnitOfGranularity)
        result = useDownstream ? endOfSentence(vp) : startOfSentence(vp);
    else {
        result = useDownstream ? nextSentencePosition(vp) : previousSentencePosition(vp);
        if (result.isNull() || result == vp)
            return VisiblePosition();

        result = useDownstream ? startOfSentence(vp) : endOfSentence(vp);
    }

    if (result == vp)
        return VisiblePosition();

    ASSERT(useDownstream ? (result > vp) : (result < vp));

    return result;
}

static VisiblePosition nextLineBoundaryInDirection(const VisiblePosition& vp, SelectionDirection direction)
{
    bool useDownstream = directionIsDownstream(direction);
    VisiblePosition result = vp;

    if (useDownstream) {
        result.setAffinity(DOWNSTREAM);
        result = isEndOfLine(result) ? startOfLine(nextLinePosition(result, result.lineDirectionPointForBlockDirectionNavigation())) : endOfLine(result);
    } else {
        result.setAffinity(VP_UPSTREAM_IF_POSSIBLE);
        result = isStartOfLine(result) ? endOfLine(previousLinePosition(result, result.lineDirectionPointForBlockDirectionNavigation())) : startOfLine(result);
    }

    return result;
}

static VisiblePosition nextParagraphBoundaryInDirection(const VisiblePosition& vp, SelectionDirection direction)
{
    bool useDownstream = directionIsDownstream(direction);
    bool withinUnitOfGranularity = withinTextUnitOfGranularity(vp, ParagraphGranularity, direction);
    VisiblePosition result;

    if (!withinUnitOfGranularity)
        result =  useDownstream ? startOfParagraph(nextParagraphPosition(vp, vp.lineDirectionPointForBlockDirectionNavigation())) : endOfParagraph(previousParagraphPosition(vp, vp.lineDirectionPointForBlockDirectionNavigation()));
    else
        result = useDownstream ? endOfParagraph(vp) : startOfParagraph(vp);

    return result;
}

static VisiblePosition nextDocumentBoundaryInDirection(const VisiblePosition& vp, SelectionDirection direction)
{
    return directionIsDownstream(direction) ? endOfDocument(vp) : startOfDocument(vp);
}

VisiblePosition positionOfNextBoundaryOfGranularity(const VisiblePosition& vp, TextGranularity granularity, SelectionDirection direction)
{
    switch (granularity) {
    case CharacterGranularity:
        return nextCharacterBoundaryInDirection(vp, direction);
    case WordGranularity:
        return nextWordBoundaryInDirection(vp, direction);
    case SentenceGranularity:
        return nextSentenceBoundaryInDirection(vp, direction);
    case LineGranularity:
        return nextLineBoundaryInDirection(vp, direction);
    case ParagraphGranularity:
        return nextParagraphBoundaryInDirection(vp, direction);
    case DocumentGranularity:
        return nextDocumentBoundaryInDirection(vp, direction);
    default:
        ASSERT_NOT_REACHED();
        return VisiblePosition();
    }
}

PassRefPtr<Range> enclosingTextUnitOfGranularity(const VisiblePosition& vp, TextGranularity granularity, SelectionDirection direction)
{
    // This is particularly inefficient.  We could easily obtain the answer with the boundaries computed below.
    if (!withinTextUnitOfGranularity(vp, granularity, direction))
        return 0;

    VisiblePosition prevBoundary;
    VisiblePosition nextBoundary;
    bool useDownstream = directionIsDownstream(direction);

    switch (granularity) {
        case CharacterGranularity:
            prevBoundary = vp;
            nextBoundary = prevBoundary.next();
            break;

        case WordGranularity:
            // NB: "Left" and "Right" in this context apparently mean "upstream/previous" and "downstream/next".
            if (useDownstream) {
                prevBoundary = startOfWord(vp, RightWordIfOnBoundary);
                nextBoundary = endOfWord(vp, RightWordIfOnBoundary);
            } else {
                prevBoundary = startOfWord(vp, LeftWordIfOnBoundary);
                nextBoundary = endOfWord(vp, LeftWordIfOnBoundary);
            }
            break;

        case SentenceGranularity:
            prevBoundary = startOfSentence(vp);
            nextBoundary = endOfSentence(vp);
            break;

        case LineGranularity:
            prevBoundary = startOfLine(vp);
            nextBoundary = endOfLine(vp);

            if (prevBoundary == nextBoundary) {
                nextBoundary = nextLinePosition(nextBoundary, 0);
                nextBoundary.setAffinity(UPSTREAM);
                if (!inSameLine(prevBoundary, nextBoundary))
                    nextBoundary = vp.next();
            }
            break;

        case ParagraphGranularity:
            prevBoundary = startOfParagraph(vp);
            nextBoundary = endOfParagraph(vp);
            break;

        case DocumentGranularity:
            prevBoundary = startOfDocument(vp);
            nextBoundary = endOfDocument(vp);
            break;

        default:
            ASSERT_NOT_REACHED();
            return 0;
    }

    if (prevBoundary.isNull() || nextBoundary.isNull())
        return 0;

    if (vp < prevBoundary || vp > nextBoundary)
        return 0;

    RefPtr<Range> range = Range::create(prevBoundary.deepEquivalent().deprecatedNode()->document(), prevBoundary, nextBoundary);

    return range;
}

int distanceBetweenPositions(const VisiblePosition& vp, const VisiblePosition& other)
{
    if (vp.isNull() || other.isNull())
        return 0;

    bool thisIsStart = (vp < other);

    // Start must come first in the Range constructor.
    RefPtr<Range> range = Range::create(vp.deepEquivalent().deprecatedNode()->document(),
                                        (thisIsStart ? vp : other),
                                        (thisIsStart ? other : vp));
    int distance = TextIterator::rangeLength(range.get());

    return (thisIsStart ? -distance : distance);
}

PassRefPtr<Range> wordRangeFromPosition(const VisiblePosition& position)
{
    ASSERT(position.isNotNull());

    RefPtr<Range> range = enclosingTextUnitOfGranularity(position, WordGranularity, DirectionBackward);

    if (!range) {
        // We could be at the start of a word, try forward.
        range = enclosingTextUnitOfGranularity(position, WordGranularity, DirectionForward);
    }
    if (range)
        return range;

    VisiblePosition currentPosition = position;
    do {
        currentPosition = positionOfNextBoundaryOfGranularity(currentPosition, WordGranularity, DirectionBackward);
    } while (currentPosition.isNotNull() && !atBoundaryOfGranularity(currentPosition, WordGranularity, DirectionBackward));

    // If the position is an empty paragraph and at the end of the document
    // the word iterator could not pass the paragraph boundary, therefore iterating to
    // the previous line is required.
    if (currentPosition.isNull() && isEndOfDocument(position)) {
        VisiblePosition previousLinePosition = positionOfNextBoundaryOfGranularity(position, LineGranularity, DirectionBackward);
        if (previousLinePosition.isNotNull()) {
            currentPosition = positionOfNextBoundaryOfGranularity(previousLinePosition, WordGranularity, DirectionBackward);
            if (currentPosition.isNull())
                currentPosition = previousLinePosition;
        }
    }

    if (currentPosition.isNull())
        currentPosition = positionOfNextBoundaryOfGranularity(position, WordGranularity, DirectionForward);

    if (currentPosition.isNotNull()) {
        range = Range::create(position.deepEquivalent().deprecatedNode()->document(), currentPosition, position);
        ASSERT(range);
    }
    return range;
}

VisiblePosition closestWordBoundaryForPosition(const VisiblePosition& position)
{
    VisiblePosition result;

    // move the the position at the end of the word
    if (atBoundaryOfGranularity(position, LineGranularity, DirectionForward)) {
        // Don't cross line boundaries.
        result = position;
    } else if (withinTextUnitOfGranularity(position, WordGranularity, DirectionForward)) {
        // The position lies within a word.
        RefPtr<Range> wordRange = enclosingTextUnitOfGranularity(position, WordGranularity, DirectionForward);

        result = wordRange->startPosition();
        if (distanceBetweenPositions(position, result) > 1)
            result = wordRange->endPosition();
    } else if (atBoundaryOfGranularity(position, WordGranularity, DirectionBackward)) {
        // The position is at the end of a word.
        result = position;
    } else {
        // The position is not within a word.
        // Go to the next boundary.
        result = positionOfNextBoundaryOfGranularity(position, WordGranularity, DirectionForward);

        // If there is no such boundary we go to the end of the element.
        if (result.isNull())
            result = endOfEditableContent(position);
    }
    return result;
}

#endif

}
