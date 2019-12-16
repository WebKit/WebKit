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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "Editing.h"
#include "HTMLBRElement.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "NodeTraversal.h"
#include "RenderBlockFlow.h"
#include "RenderObject.h"
#include "RenderedPosition.h"
#include "Text.h"
#include "TextBoundaries.h"
#include "TextIterator.h"
#include "VisibleSelection.h"
#include <unicode/ubrk.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

using namespace HTMLNames;
using namespace WTF::Unicode;

static Node* previousLeafWithSameEditability(Node* node, EditableType editableType)
{
    bool editable = hasEditableStyle(*node, editableType);
    node = previousLeafNode(node);
    while (node) {
        if (editable == hasEditableStyle(*node, editableType))
            return node;
        node = previousLeafNode(node);
    }
    return nullptr;
}

static Node* nextLeafWithSameEditability(Node* node, EditableType editableType)
{
    if (!node)
        return nullptr;
    
    bool editable = hasEditableStyle(*node, editableType);
    node = nextLeafNode(node);
    while (node) {
        if (editable == hasEditableStyle(*node, editableType))
            return node;
        node = nextLeafNode(node);
    }
    return nullptr;
}

// FIXME: consolidate with code in previousLinePosition.
static Position previousRootInlineBoxCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    auto* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* previousNode = previousLeafWithSameEditability(node, editableType);

    while (previousNode && (!previousNode->renderer() || inSameLine(firstPositionInOrBeforeNode(previousNode), visiblePosition)))
        previousNode = previousLeafWithSameEditability(previousNode, editableType);

    while (previousNode && !previousNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(previousNode), editableType) != highestRoot)
            break;

        Position pos = previousNode->hasTagName(brTag) ? positionBeforeNode(previousNode) :
            createLegacyEditingPosition(previousNode, caretMaxOffset(*previousNode));
        
        if (pos.isCandidate())
            return pos;

        previousNode = previousLeafWithSameEditability(previousNode, editableType);
    }
    return Position();
}

static Position nextRootInlineBoxCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    auto* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* nextNode = nextLeafWithSameEditability(node, editableType);
    while (nextNode && (!nextNode->renderer() || inSameLine(firstPositionInOrBeforeNode(nextNode), visiblePosition)))
        nextNode = nextLeafWithSameEditability(nextNode, ContentIsEditable);

    while (nextNode && !nextNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(nextNode), editableType) != highestRoot)
            break;

        Position pos;
        pos = createLegacyEditingPosition(nextNode, caretMinOffset(*nextNode));
        
        if (pos.isCandidate())
            return pos;

        nextNode = nextLeafWithSameEditability(nextNode, editableType);
    }
    return Position();
}

class CachedLogicallyOrderedLeafBoxes {
public:
    CachedLogicallyOrderedLeafBoxes();

    const InlineBox* previousTextOrLineBreakBox(const RootInlineBox*, const InlineBox*);
    const InlineBox* nextTextOrLineBreakBox(const RootInlineBox*, const InlineBox*);

    size_t size() const { return m_leafBoxes.size(); }
    const InlineBox* firstBox() const { return m_leafBoxes[0]; }

private:
    const Vector<InlineBox*>& collectBoxes(const RootInlineBox*);
    int boxIndexInLeaves(const InlineBox*) const;

    const RootInlineBox* m_rootInlineBox { nullptr };
    Vector<InlineBox*> m_leafBoxes;
};

CachedLogicallyOrderedLeafBoxes::CachedLogicallyOrderedLeafBoxes()
{
}

const InlineBox* CachedLogicallyOrderedLeafBoxes::previousTextOrLineBreakBox(const RootInlineBox* root, const InlineBox* box)
{
    if (!root)
        return nullptr;

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

    return nullptr;
}

const InlineBox* CachedLogicallyOrderedLeafBoxes::nextTextOrLineBreakBox(const RootInlineBox* root, const InlineBox* box)
{
    if (!root)
        return nullptr;

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

    return nullptr;
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

int CachedLogicallyOrderedLeafBoxes::boxIndexInLeaves(const InlineBox* box) const
{
    for (size_t i = 0; i < m_leafBoxes.size(); ++i) {
        if (box == m_leafBoxes[i])
            return i;
    }
    return 0;
}

static const InlineBox* logicallyPreviousBox(const VisiblePosition& visiblePosition, const InlineBox* textBox,
    bool& previousBoxInDifferentLine, CachedLogicallyOrderedLeafBoxes& leafBoxes)
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

        previousBox = leafBoxes.previousTextOrLineBreakBox(previousRoot, &startBox->root() == previousRoot ? startBox : nullptr);
        if (previousBox) {
            previousBoxInDifferentLine = true;
            return previousBox;
        }

        if (!leafBoxes.size())
            break;
        startBox = leafBoxes.firstBox();
    }
    return 0;
}


static const InlineBox* logicallyNextBox(const VisiblePosition& visiblePosition, const InlineBox* textBox,
    bool& nextBoxInDifferentLine, CachedLogicallyOrderedLeafBoxes& leafBoxes)
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

        nextBox = leafBoxes.nextTextOrLineBreakBox(nextRoot, &startBox->root() == nextRoot ? startBox : nullptr);
        if (nextBox) {
            nextBoxInDifferentLine = true;
            return nextBox;
        }

        if (!leafBoxes.size())
            break;
        startBox = leafBoxes.firstBox();
    }
    return 0;
}

static UBreakIterator* wordBreakIteratorForMinOffsetBoundary(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    int& previousBoxLength, bool& previousBoxInDifferentLine, Vector<UChar, 1024>& string, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    previousBoxInDifferentLine = false;

    const InlineBox* previousBox = logicallyPreviousBox(visiblePosition, textBox, previousBoxInDifferentLine, leafBoxes);
    while (previousBox && !is<InlineTextBox>(previousBox)) {
        ASSERT(previousBox->renderer().isBR());
        previousBoxInDifferentLine = true;
        previousBox = logicallyPreviousBox(visiblePosition, previousBox, previousBoxInDifferentLine, leafBoxes);
    }

    string.clear();

    if (is<InlineTextBox>(previousBox)) {
        const auto& previousTextBox = downcast<InlineTextBox>(*previousBox);
        previousBoxLength = previousTextBox.len();
        append(string, StringView(previousTextBox.renderer().text()).substring(previousTextBox.start(), previousBoxLength));
    }
    append(string, StringView(textBox->renderer().text()).substring(textBox->start(), textBox->len()));

    return wordBreakIterator(StringView(string.data(), string.size()));
}

static UBreakIterator* wordBreakIteratorForMaxOffsetBoundary(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& nextBoxInDifferentLine, Vector<UChar, 1024>& string, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    nextBoxInDifferentLine = false;

    const InlineBox* nextBox = logicallyNextBox(visiblePosition, textBox, nextBoxInDifferentLine, leafBoxes);
    while (nextBox && !is<InlineTextBox>(nextBox)) {
        ASSERT(nextBox->renderer().isBR());
        nextBoxInDifferentLine = true;
        nextBox = logicallyNextBox(visiblePosition, nextBox, nextBoxInDifferentLine, leafBoxes);
    }

    string.clear();
    append(string, StringView(textBox->renderer().text()).substring(textBox->start(), textBox->len()));
    if (is<InlineTextBox>(nextBox)) {
        const auto& nextTextBox = downcast<InlineTextBox>(*nextBox);
        append(string, StringView(nextTextBox.renderer().text()).substring(nextTextBox.start(), nextTextBox.len()));
    }

    return wordBreakIterator(StringView(string.data(), string.size()));
}

static bool isLogicalStartOfWord(UBreakIterator* iter, int position, bool hardLineBreak)
{
    bool boundary = hardLineBreak ? true : ubrk_isBoundary(iter, position);
    if (!boundary)
        return false;

    ubrk_following(iter, position);
    // isWordTextBreak returns true after moving across a word and false after moving across a punctuation/space.
    return isWordTextBreak(iter);
}

static bool islogicalEndOfWord(UBreakIterator* iter, int position, bool hardLineBreak)
{
    bool boundary = ubrk_isBoundary(iter, position);
    return (hardLineBreak || boundary) && isWordTextBreak(iter);
}

enum CursorMovementDirection { MoveLeft, MoveRight };

static VisiblePosition visualWordPosition(const VisiblePosition& visiblePosition, CursorMovementDirection direction, 
    bool skipsSpaceWhenMovingRight)
{
    if (visiblePosition.isNull())
        return VisiblePosition();

    TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
    InlineBox* previouslyVisitedBox = nullptr;
    VisiblePosition current = visiblePosition;
    Optional<VisiblePosition> previousPosition;
    UBreakIterator* iter = nullptr;

    CachedLogicallyOrderedLeafBoxes leafBoxes;
    Vector<UChar, 1024> string;

    while (1) {
        VisiblePosition adjacentCharacterPosition = direction == MoveRight ? current.right(true) : current.left(true); 
        if (adjacentCharacterPosition == current || adjacentCharacterPosition.isNull())
            return VisiblePosition();
        // FIXME: This is a workaround for webkit.org/b/167138.
        if (previousPosition && adjacentCharacterPosition == previousPosition.value())
            return VisiblePosition();
    
        InlineBox* box;
        int offsetInBox;
        adjacentCharacterPosition.deepEquivalent().getInlineBoxAndOffset(UPSTREAM, box, offsetInBox);
    
        if (!box)
            break;
        if (!is<InlineTextBox>(*box)) {
            current = adjacentCharacterPosition;
            continue;
        }

        InlineTextBox& textBox = downcast<InlineTextBox>(*box);
        int previousBoxLength = 0;
        bool previousBoxInDifferentLine = false;
        bool nextBoxInDifferentLine = false;
        bool movingIntoNewBox = previouslyVisitedBox != box;

        if (offsetInBox == box->caretMinOffset())
            iter = wordBreakIteratorForMinOffsetBoundary(adjacentCharacterPosition, &textBox, previousBoxLength, previousBoxInDifferentLine, string, leafBoxes);
        else if (offsetInBox == box->caretMaxOffset())
            iter = wordBreakIteratorForMaxOffsetBoundary(adjacentCharacterPosition, &textBox, nextBoxInDifferentLine, string, leafBoxes);
        else if (movingIntoNewBox) {
            iter = wordBreakIterator(StringView(textBox.renderer().text()).substring(textBox.start(), textBox.len()));
            previouslyVisitedBox = box;
        }

        if (!iter)
            break;

        ubrk_first(iter);
        int offsetInIterator = offsetInBox - textBox.start() + previousBoxLength;

        bool isWordBreak;
        bool boxHasSameDirectionalityAsBlock = box->direction() == blockDirection;
        bool movingBackward = (direction == MoveLeft && box->direction() == TextDirection::LTR) || (direction == MoveRight && box->direction() == TextDirection::RTL);
        if ((skipsSpaceWhenMovingRight && boxHasSameDirectionalityAsBlock)
            || (!skipsSpaceWhenMovingRight && movingBackward)) {
            bool logicalStartInRenderer = offsetInBox == static_cast<int>(textBox.start()) && previousBoxInDifferentLine;
            isWordBreak = isLogicalStartOfWord(iter, offsetInIterator, logicalStartInRenderer);
            if (isWordBreak && offsetInBox == box->caretMaxOffset() && nextBoxInDifferentLine)
                isWordBreak = false;
        } else {
            bool logicalEndInRenderer = offsetInBox == static_cast<int>(textBox.start() + textBox.len()) && nextBoxInDifferentLine;
            isWordBreak = islogicalEndOfWord(iter, offsetInIterator, logicalEndInRenderer);
            if (isWordBreak && offsetInBox == box->caretMinOffset() && previousBoxInDifferentLine)
                isWordBreak = false;
        }      

        if (isWordBreak)
            return adjacentCharacterPosition;
    
        previousPosition = current;
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
        leftWordBreak = blockDirection == TextDirection::LTR ? startOfEditableContent(visiblePosition) : endOfEditableContent(visiblePosition);
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
        rightWordBreak = blockDirection == TextDirection::LTR ? endOfEditableContent(visiblePosition) : startOfEditableContent(visiblePosition);
    }
    return rightWordBreak;
}


static void prepend(Vector<UChar, 1024>& buffer, StringView string)
{
    unsigned oldSize = buffer.size();
    unsigned length = string.length();
    buffer.grow(oldSize + length);
    memmove(buffer.data() + length, buffer.data(), oldSize * sizeof(UChar));
    for (unsigned i = 0; i < length; ++i)
        buffer[i] = string[i];
}

static void prependRepeatedCharacter(Vector<UChar, 1024>& buffer, UChar character, unsigned count)
{
    unsigned oldSize = buffer.size();
    buffer.grow(oldSize + count);
    memmove(buffer.data() + count, buffer.data(), oldSize * sizeof(UChar));
    for (unsigned i = 0; i < count; ++i)
        buffer[i] = character;
}

static void appendRepeatedCharacter(Vector<UChar, 1024>& buffer, UChar character, unsigned count)
{
    unsigned oldSize = buffer.size();
    buffer.grow(oldSize + count);
    for (unsigned i = 0; i < count; ++i)
        buffer[oldSize + i] = character;
}

unsigned suffixLengthForRange(const Range& forwardsScanRange, Vector<UChar, 1024>& string)
{
    unsigned suffixLength = 0;
    TextIterator forwardsIterator(&forwardsScanRange);
    while (!forwardsIterator.atEnd()) {
        StringView text = forwardsIterator.text();
        unsigned i = endOfFirstWordBoundaryContext(text);
        append(string, text.substring(0, i));
        suffixLength += i;
        if (i < text.length())
            break;
        forwardsIterator.advance();
    }
    return suffixLength;
}

unsigned prefixLengthForRange(const Range& backwardsScanRange, Vector<UChar, 1024>& string)
{
    unsigned prefixLength = 0;
    SimplifiedBackwardsTextIterator backwardsIterator(backwardsScanRange);
    while (!backwardsIterator.atEnd()) {
        StringView text = backwardsIterator.text();
        int i = startOfLastWordBoundaryContext(text);
        prepend(string, text.substring(i));
        prefixLength += text.length() - i;
        if (i > 0)
            break;
        backwardsIterator.advance();
    }
    return prefixLength;
}

unsigned backwardSearchForBoundaryWithTextIterator(SimplifiedBackwardsTextIterator& it, Vector<UChar, 1024>& string, unsigned suffixLength, BoundarySearchFunction searchFunction)
{
    unsigned next = 0;
    bool needMoreContext = false;
    while (!it.atEnd()) {
        bool inTextSecurityMode = it.node() && it.node()->renderer() && it.node()->renderer()->style().textSecurity() != TextSecurity::None;
        // iterate to get chunks until the searchFunction returns a non-zero value.
        if (!inTextSecurityMode)
            prepend(string, it.text());
        else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            prependRepeatedCharacter(string, 'x', it.text().length());
        }
        if (string.size() > suffixLength) {
            next = searchFunction(StringView(string.data(), string.size()), string.size() - suffixLength, MayHaveMoreContext, needMoreContext);
            if (next > 1) // FIXME: This is a work around for https://webkit.org/b/115070. We need to provide more contexts in general case.
                break;
        }
        it.advance();
    }
    if (needMoreContext && string.size() > suffixLength) {
        // The last search returned the beginning of the buffer and asked for more context,
        // but there is no earlier text. Force a search with what's available.
        next = searchFunction(StringView(string.data(), string.size()), string.size() - suffixLength, DontHaveMoreContext, needMoreContext);
        ASSERT(!needMoreContext);
    }
    
    return next;
}

unsigned forwardSearchForBoundaryWithTextIterator(TextIterator& it, Vector<UChar, 1024>& string, unsigned prefixLength, BoundarySearchFunction searchFunction)
{
    unsigned next = 0;
    bool needMoreContext = false;
    while (!it.atEnd()) {
        bool inTextSecurityMode = it.node() && it.node()->renderer() && it.node()->renderer()->style().textSecurity() != TextSecurity::None;
        // Keep asking the iterator for chunks until the search function
        // returns an end value not equal to the length of the string passed to it.
        if (!inTextSecurityMode)
            append(string, it.text());
        else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            appendRepeatedCharacter(string, 'x', it.text().length());
        }
        if (string.size() > prefixLength) {
            next = searchFunction(StringView(string.data(), string.size()), prefixLength, MayHaveMoreContext, needMoreContext);
            if (next != string.size())
                break;
        }
        it.advance();
    }
    if (needMoreContext && string.size() > prefixLength) {
        // The last search returned the end of the buffer and asked for more context,
        // but there is no further text. Force a search with what's available.
        next = searchFunction(StringView(string.data(), string.size()), prefixLength, DontHaveMoreContext, needMoreContext);
        ASSERT(!needMoreContext);
    }
    
    return next;
}

enum class NeedsContextAtParagraphStart { Yes, No };
static VisiblePosition previousBoundary(const VisiblePosition& c, BoundarySearchFunction searchFunction,
    NeedsContextAtParagraphStart needsContextAtParagraphStart = NeedsContextAtParagraphStart::No)
{
    Position pos = c.deepEquivalent();
    Node* boundary = pos.parentEditingBoundary();
    if (!boundary)
        return VisiblePosition();

    Document& boundaryDocument = boundary->document();
    Position start = createLegacyEditingPosition(boundary, 0).parentAnchoredEquivalent();
    Position end = pos.parentAnchoredEquivalent();

    if (start.isNull() || end.isNull())
        return VisiblePosition();

    Ref<Range> searchRange = Range::create(boundaryDocument);
    
    Vector<UChar, 1024> string;
    unsigned suffixLength = 0;

    if (needsContextAtParagraphStart == NeedsContextAtParagraphStart::Yes && isStartOfParagraph(c)) {
        auto forwardsScanRange = boundaryDocument.createRange();
        auto endOfCurrentParagraph = endOfParagraph(c);
        auto result = forwardsScanRange->setEnd(endOfCurrentParagraph.deepEquivalent());
        if (result.hasException())
            return { };
        result = forwardsScanRange->setStart(start);
        if (result.hasException())
            return { };
        for (TextIterator forwardsIterator(forwardsScanRange.ptr()); !forwardsIterator.atEnd(); forwardsIterator.advance())
            append(string, forwardsIterator.text());
        suffixLength = string.size();
    } else if (requiresContextForWordBoundary(c.characterBefore())) {
        auto forwardsScanRange = boundaryDocument.createRange();
        auto result = forwardsScanRange->setEndAfter(*boundary);
        if (result.hasException())
            return { };
        result = forwardsScanRange->setStart(*end.deprecatedNode(), end.deprecatedEditingOffset());
        if (result.hasException())
            return { };
        suffixLength = suffixLengthForRange(forwardsScanRange, string);
    }

    auto result = searchRange->setStart(*start.deprecatedNode(), start.deprecatedEditingOffset());
    if (result.hasException())
        return { };
    result = searchRange->setEnd(*end.deprecatedNode(), end.deprecatedEditingOffset());
    if (result.hasException())
        return { };

    SimplifiedBackwardsTextIterator it(searchRange);
    unsigned next = backwardSearchForBoundaryWithTextIterator(it, string, suffixLength, searchFunction);

    if (!next)
        return VisiblePosition(it.atEnd() ? searchRange->startPosition() : pos, DOWNSTREAM);

    Node& node = it.atEnd() ? searchRange->startContainer() : it.range()->startContainer();
    if ((!suffixLength && node.isTextNode() && static_cast<int>(next) <= node.maxCharacterOffset()) || (node.renderer() && node.renderer()->isBR() && !next)) {
        // The next variable contains a usable index into a text node
        return VisiblePosition(createLegacyEditingPosition(&node, next), DOWNSTREAM);
    }

    // Use the character iterator to translate the next value into a DOM position.
    BackwardsCharacterIterator charIt(searchRange);
    if (next < string.size() - suffixLength)
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
    Ref<Range> searchRange = boundaryDocument.createRange();
    Position start(pos.parentAnchoredEquivalent());

    Vector<UChar, 1024> string;
    unsigned prefixLength = 0;

    if (requiresContextForWordBoundary(c.characterAfter())) {
        auto backwardsScanRange = boundaryDocument.createRange();
        if (start.deprecatedNode())
            backwardsScanRange->setEnd(*start.deprecatedNode(), start.deprecatedEditingOffset());
        prefixLength = prefixLengthForRange(backwardsScanRange, string);
    }

    searchRange->selectNodeContents(*boundary);
    if (start.deprecatedNode())
        searchRange->setStart(*start.deprecatedNode(), start.deprecatedEditingOffset());
    TextIterator it(searchRange.ptr(), TextIteratorEmitsCharactersBetweenAllVisiblePositions);
    unsigned next = forwardSearchForBoundaryWithTextIterator(it, string, prefixLength, searchFunction);
    
    if (it.atEnd() && next == string.size())
        pos = searchRange->endPosition();
    else if (next > prefixLength) {
        // Use the character iterator to translate the next value into a DOM position.
        CharacterIterator charIt(searchRange, TextIteratorEmitsCharactersBetweenAllVisiblePositions);
        charIt.advance(next - prefixLength - 1);
        RefPtr<Range> characterRange = charIt.range();
        pos = characterRange->endPosition();
        
        if (charIt.text()[0] == '\n') {
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

unsigned startWordBoundary(StringView text, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    ASSERT(offset);
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(text.substring(0, offset))) {
        needMoreContext = true;
        return 0;
    }
    needMoreContext = false;
    int start, end;
    U16_BACK_1(text, 0, offset);
    findWordBoundary(text, offset, &start, &end);
    return start;
}

VisiblePosition startOfWord(const VisiblePosition& c, EWordSide side)
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

unsigned endWordBoundary(StringView text, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    ASSERT(offset <= text.length());
    if (mayHaveMoreContext && endOfFirstWordBoundaryContext(text.substring(offset)) == text.length() - offset) {
        needMoreContext = true;
        return text.length();
    }
    needMoreContext = false;
    int end;
    findEndWordBoundary(text, offset, &end);
    return end;
}

VisiblePosition endOfWord(const VisiblePosition& c, EWordSide side)
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

static unsigned previousWordPositionBoundary(StringView text, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(text.substring(0, offset))) {
        needMoreContext = true;
        return 0;
    }
    needMoreContext = false;
    return findNextWordFromIndex(text, offset, false);
}

VisiblePosition previousWordPosition(const VisiblePosition& position)
{
    return position.honorEditingBoundaryAtOrBefore(previousBoundary(position, previousWordPositionBoundary));
}

static unsigned nextWordPositionBoundary(StringView text, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    if (mayHaveMoreContext && endOfFirstWordBoundaryContext(text.substring(offset)) == text.length() - offset) {
        needMoreContext = true;
        return text.length();
    }
    needMoreContext = false;
    return findNextWordFromIndex(text, offset, true);
}

VisiblePosition nextWordPosition(const VisiblePosition& position)
{
    return position.honorEditingBoundaryAtOrAfter(nextBoundary(position, nextWordPositionBoundary));
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
        startBox = rootBox->firstLeafDescendant();
        while (true) {
            if (!startBox)
                return VisiblePosition();

            startNode = startBox->renderer().nonPseudoNode();
            if (startNode)
                break;

            startBox = startBox->nextLeafOnLine();
        }
    }

    return is<Text>(*startNode) ? Position(downcast<Text>(startNode), downcast<InlineTextBox>(*startBox).start())
        : positionBeforeNode(startNode);
}

static VisiblePosition startOfLine(const VisiblePosition& c, LineEndpointComputationMode mode, bool* reachedBoundary)
{
    if (reachedBoundary)
        *reachedBoundary = false;
    // TODO: this is the current behavior that might need to be fixed.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
    VisiblePosition visPos = startPositionForLine(c, mode);

    if (mode == UseLogicalOrdering) {
        if (Node* editableRoot = highestEditableRoot(c.deepEquivalent())) {
            if (!editableRoot->contains(visPos.deepEquivalent().containerNode())) {
                VisiblePosition newPosition = firstPositionInNode(editableRoot);
                if (reachedBoundary)
                    *reachedBoundary = c == newPosition;
                return newPosition;
            }
        }
    }

    return c.honorEditingBoundaryAtOrBefore(visPos, reachedBoundary);
}

// FIXME: Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition startOfLine(const VisiblePosition& currentPosition)
{
    return startOfLine(currentPosition, UseInlineBoxOrdering, nullptr);
}

VisiblePosition logicalStartOfLine(const VisiblePosition& currentPosition, bool* reachedBoundary)
{
    return startOfLine(currentPosition, UseLogicalOrdering, reachedBoundary);
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
        endBox = rootBox->lastLeafDescendant();
        while (true) {
            if (!endBox)
                return VisiblePosition();

            endNode = endBox->renderer().nonPseudoNode();
            if (endNode)
                break;
            
            endBox = endBox->previousLeafOnLine();
        }
    }

    Position pos;
    if (is<HTMLBRElement>(*endNode))
        pos = positionBeforeNode(endNode);
    else if (is<InlineTextBox>(*endBox) && is<Text>(*endNode)) {
        auto& endTextBox = downcast<InlineTextBox>(*endBox);
        int endOffset = endTextBox.start();
        if (!endTextBox.isLineBreak())
            endOffset += endTextBox.len();
        pos = Position(downcast<Text>(endNode), endOffset);
    } else
        pos = positionAfterNode(endNode);
    
    return VisiblePosition(pos, VP_UPSTREAM_IF_POSSIBLE);
}

static bool inSameLogicalLine(const VisiblePosition& a, const VisiblePosition& b)
{
    return a.isNotNull() && logicalStartOfLine(a) == logicalStartOfLine(b);
}

static VisiblePosition endOfLine(const VisiblePosition& c, LineEndpointComputationMode mode, bool* reachedBoundary)
{
    if (reachedBoundary)
        *reachedBoundary = false;
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
            if (!editableRoot->contains(visPos.deepEquivalent().containerNode())) {
                VisiblePosition newPosition = lastPositionInNode(editableRoot);
                if (reachedBoundary)
                    *reachedBoundary = c == newPosition;
                return newPosition;
            }
        }

        return c.honorEditingBoundaryAtOrAfter(visPos, reachedBoundary);
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
    
    return c.honorEditingBoundaryAtOrAfter(visPos, reachedBoundary);
}

// FIXME: Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition endOfLine(const VisiblePosition& currentPosition)
{
    return endOfLine(currentPosition, UseInlineBoxOrdering, nullptr);
}

VisiblePosition logicalEndOfLine(const VisiblePosition& currentPosition, bool* reachedBoundary)
{
    return endOfLine(currentPosition, UseLogicalOrdering, reachedBoundary);
}

bool inSameLine(const VisiblePosition& a, const VisiblePosition& b)
{
    return a.isNotNull() && startOfLine(a) == startOfLine(b);
}

bool isStartOfLine(const VisiblePosition& p)
{
    return p.isNotNull() && p == startOfLine(p);
}

bool isEndOfLine(const VisiblePosition& p)
{
    return p.isNotNull() && p == endOfLine(p);
}

bool isLogicalEndOfLine(const VisiblePosition &p)
{
    return p.isNotNull() && p == logicalEndOfLine(p);
}

static inline IntPoint absoluteLineDirectionPointToLocalPointInBlock(RootInlineBox& root, int lineDirectionPoint)
{
    RenderBlockFlow& containingBlock = root.blockFlow();
    FloatPoint absoluteBlockPoint = containingBlock.localToAbsolute(FloatPoint()) - toFloatSize(containingBlock.scrollPosition());

    if (containingBlock.isHorizontalWritingMode())
        return IntPoint(lineDirectionPoint - absoluteBlockPoint.x(), root.blockDirectionPointInLine());

    return IntPoint(root.blockDirectionPointInLine(), lineDirectionPoint - absoluteBlockPoint.y());
}

static Element* rootEditableOrDocumentElement(Node& node, EditableType editableType)
{
    if (hasEditableStyle(node, editableType))
        return editableRootForPosition(firstPositionInOrBeforeNode(&node), editableType);
    return node.document().documentElement();
}

VisiblePosition previousLinePosition(const VisiblePosition& visiblePosition, int lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.deprecatedNode();

    if (!node)
        return VisiblePosition();
    
    node->document().updateLayoutIgnorePendingStylesheets();
    
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RootInlineBox* root = nullptr;
    InlineBox* box;
    int ignoredCaretOffset;
    visiblePosition.getInlineBoxAndOffset(box, ignoredCaretOffset);
    if (box) {
        root = box->root().prevRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight() || !root->firstLeafDescendant())
            root = nullptr;
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
        IntPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(*root, lineDirectionPoint);
        RenderObject& renderer = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p))->renderer();
        Node* node = renderer.node();
        if (node && editingIgnoresContent(*node))
            return positionInParentBeforeNode(node);
        return renderer.positionForPoint(pointInLine, nullptr);
    }
    
    // Could not find a previous line. This means we must already be on the first line.
    // Move to the start of the content in this block, which effectively moves us
    // to the start of the line we're on.
    Element* rootElement = rootEditableOrDocumentElement(*node, editableType);
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition(firstPositionInNode(rootElement), DOWNSTREAM);
}

VisiblePosition nextLinePosition(const VisiblePosition& visiblePosition, int lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.deprecatedNode();

    if (!node)
        return VisiblePosition();
    
    node->document().updateLayoutIgnorePendingStylesheets();

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();

    RootInlineBox* root = nullptr;
    InlineBox* box;
    int ignoredCaretOffset;
    visiblePosition.getInlineBoxAndOffset(box, ignoredCaretOffset);
    if (box) {
        root = box->root().nextRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight() || !root->firstLeafDescendant())
            root = nullptr;
    }

    if (!root) {
        // FIXME: We need do the same in previousLinePosition.
        Node* child = node->traverseToChildAt(p.deprecatedEditingOffset());
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
        IntPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(*root, lineDirectionPoint);
        RenderObject& renderer = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p))->renderer();
        Node* node = renderer.node();
        if (node && editingIgnoresContent(*node))
            return positionInParentBeforeNode(node);
        return renderer.positionForPoint(pointInLine, nullptr);
    }

    // Could not find a next line. This means we must already be on the last line.
    // Move to the end of the content in this block, which effectively moves us
    // to the end of the line we're on.
    Element* rootElement = rootEditableOrDocumentElement(*node, editableType);
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition(lastPositionInNode(rootElement), DOWNSTREAM);
}

// ---------

unsigned startSentenceBoundary(StringView text, unsigned, BoundarySearchContextAvailability, bool&)
{
    // FIXME: The following function can return -1; we don't handle that.
    return ubrk_preceding(sentenceBreakIterator(text), text.length());
}

VisiblePosition startOfSentence(const VisiblePosition& position)
{
    return previousBoundary(position, startSentenceBoundary, NeedsContextAtParagraphStart::Yes);
}

unsigned endSentenceBoundary(StringView text, unsigned, BoundarySearchContextAvailability, bool&)
{
    return ubrk_next(sentenceBreakIterator(text));
}

VisiblePosition endOfSentence(const VisiblePosition& position)
{
    // FIXME: This includes the space after the punctuation that marks the end of the sentence.
    return nextBoundary(position, endSentenceBoundary);
}

static unsigned previousSentencePositionBoundary(StringView text, unsigned, BoundarySearchContextAvailability, bool&)
{
    // FIXME: This is identical to startSentenceBoundary. I'm pretty sure that's not right.
    // FIXME: The following function can return -1; we don't handle that.
    return ubrk_preceding(sentenceBreakIterator(text), text.length());
}

VisiblePosition previousSentencePosition(const VisiblePosition& position)
{
    return position.honorEditingBoundaryAtOrBefore(previousBoundary(position, previousSentencePositionBoundary));
}

static unsigned nextSentencePositionBoundary(StringView text, unsigned, BoundarySearchContextAvailability, bool&)
{
    // FIXME: This is identical to endSentenceBoundary.
    // That isn't right. This function needs to move to the equivalent position in the following sentence.
    return ubrk_following(sentenceBreakIterator(text), 0);
}

VisiblePosition nextSentencePosition(const VisiblePosition& position)
{
    return position.honorEditingBoundaryAtOrAfter(nextBoundary(position, nextSentencePositionBoundary));
}

Node* findStartOfParagraph(Node* startNode, Node* highestRoot, Node* startBlock, int& offset, Position::AnchorType& type, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    Node* node = startNode;
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
                n = NodeTraversal::previousPostOrder(*n, startBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }
        RenderObject* r = n->renderer();
        if (!r) {
            n = NodeTraversal::previousPostOrder(*n, startBlock);
            continue;
        }
        const RenderStyle& style = r->style();
        if (style.visibility() != Visibility::Visible) {
            n = NodeTraversal::previousPostOrder(*n, startBlock);
            continue;
        }
        
        if (r->isBR() || isBlock(n))
            break;

        if (is<RenderText>(*r) && downcast<RenderText>(*r).hasRenderedText()) {
            ASSERT_WITH_SECURITY_IMPLICATION(is<Text>(*n));
            type = Position::PositionIsOffsetInAnchor;
            if (style.preserveNewline()) {
                StringImpl& text = downcast<RenderText>(*r).text();
                int i = text.length();
                int o = offset;
                if (n == startNode && o < i)
                    i = std::max(0, o);
                while (--i >= 0) {
                    if (text[i] == '\n') {
                        offset = i + 1;
                        return n;
                    }
                }
            }
            node = n;
            offset = 0;
            n = NodeTraversal::previousPostOrder(*n, startBlock);
        } else if (editingIgnoresContent(*n) || isRenderedTable(n)) {
            node = n;
            type = Position::PositionIsBeforeAnchor;
            n = n->previousSibling() ? n->previousSibling() : NodeTraversal::previousPostOrder(*n, startBlock);
        } else
            n = NodeTraversal::previousPostOrder(*n, startBlock);
    }

    return node;
}

Node* findEndOfParagraph(Node* startNode, Node* highestRoot, Node* stayInsideBlock, int& offset, Position::AnchorType& type, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    Node* node = startNode;
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
                n = NodeTraversal::next(*n, stayInsideBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }

        RenderObject* r = n->renderer();
        if (!r) {
            n = NodeTraversal::next(*n, stayInsideBlock);
            continue;
        }
        const RenderStyle& style = r->style();
        if (style.visibility() != Visibility::Visible) {
            n = NodeTraversal::next(*n, stayInsideBlock);
            continue;
        }
        
        // FIXME: This is wrong when startNode is a block. We should return a position after the block.
        if (r->isBR() || isBlock(n))
            break;

        // FIXME: We avoid returning a position where the renderer can't accept the caret.
        if (is<RenderText>(*r) && downcast<RenderText>(*r).hasRenderedText()) {
            ASSERT_WITH_SECURITY_IMPLICATION(is<Text>(*n));
            type = Position::PositionIsOffsetInAnchor;
            if (style.preserveNewline()) {
                StringImpl& text = downcast<RenderText>(*r).text();
                int o = n == startNode ? offset : 0;
                int length = text.length();
                for (int i = o; i < length; ++i) {
                    if (text[i] == '\n') {
                        offset = i;
                        return n;
                    }
                }
            }
            node = n;
            offset = r->caretMaxOffset();
            n = NodeTraversal::next(*n, stayInsideBlock);
        } else if (editingIgnoresContent(*n) || isRenderedTable(n)) {
            node = n;
            type = Position::PositionIsAfterAnchor;
            n = NodeTraversal::nextSkippingChildren(*n, stayInsideBlock);
        } else
            n = NodeTraversal::next(*n, stayInsideBlock);
    }
    return node;
}

VisiblePosition startOfParagraph(const VisiblePosition& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    Position p = c.deepEquivalent();
    auto* startNode = p.deprecatedNode();
    
    if (!startNode)
        return VisiblePosition();
    
    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return positionBeforeNode(startNode);
    
    Node* startBlock = enclosingBlock(startNode);
    
    auto* highestRoot = highestEditableRoot(p);
    int offset = p.deprecatedEditingOffset();
    Position::AnchorType type = p.anchorType();
    
    auto* node = findStartOfParagraph(startNode, highestRoot, startBlock, offset, type, boundaryCrossingRule);
    
    if (is<Text>(node))
        return VisiblePosition(Position(downcast<Text>(node), offset), DOWNSTREAM);
    
    if (type == Position::PositionIsOffsetInAnchor) {
        ASSERT(type == Position::PositionIsOffsetInAnchor || !offset);
        return VisiblePosition(Position(node, offset, type), DOWNSTREAM);
    }
    
    return VisiblePosition(Position(node, type), DOWNSTREAM);
}

VisiblePosition endOfParagraph(const VisiblePosition& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{    
    if (c.isNull())
        return VisiblePosition();
    
    Position p = c.deepEquivalent();
    auto* startNode = p.deprecatedNode();
    
    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return positionAfterNode(startNode);
    
    auto* startBlock = enclosingBlock(startNode);
    auto* stayInsideBlock = startBlock;
    
    auto* highestRoot = highestEditableRoot(p);
    int offset = p.deprecatedEditingOffset();
    Position::AnchorType type = p.anchorType();
    
    auto* node = findEndOfParagraph(startNode, highestRoot, stayInsideBlock, offset, type, boundaryCrossingRule);
    
    if (is<Text>(node))
        return VisiblePosition(Position(downcast<Text>(node), offset), DOWNSTREAM);
    
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

bool inSameParagraph(const VisiblePosition& a, const VisiblePosition& b, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return a.isNotNull() && startOfParagraph(a, boundaryCrossingRule) == startOfParagraph(b, boundaryCrossingRule);
}

bool isStartOfParagraph(const VisiblePosition& pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return pos.isNotNull() && pos == startOfParagraph(pos, boundaryCrossingRule);
}

bool isEndOfParagraph(const VisiblePosition& pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return pos.isNotNull() && pos == endOfParagraph(pos, boundaryCrossingRule);
}
    
bool isBlankParagraph(const VisiblePosition& position)
{
    return isStartOfParagraph(position) && startOfParagraph(position.next()) != startOfParagraph(position);
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

bool inSameBlock(const VisiblePosition& a, const VisiblePosition& b)
{
    return !a.isNull() && enclosingBlock(a.deepEquivalent().containerNode()) == enclosingBlock(b.deepEquivalent().containerNode());
}

bool isStartOfBlock(const VisiblePosition& pos)
{
    return pos.isNotNull() && pos == startOfBlock(pos, CanCrossEditingBoundary);
}

bool isEndOfBlock(const VisiblePosition& pos)
{
    return pos.isNotNull() && pos == endOfBlock(pos, CanCrossEditingBoundary);
}

// ---------

VisiblePosition startOfDocument(const Node* node)
{
    if (!node || !node->document().documentElement())
        return VisiblePosition();
    
    // The canonicalization of the position at (documentElement, 0) can turn the visible
    // position to null, even when there's a valid candidate to be had, because the root HTML element
    // is not content editable.  So we construct directly from the valid candidate.
    Position firstCandidate = nextCandidate(createLegacyEditingPosition(node->document().documentElement(), 0));
    if (firstCandidate.isNull())
        return VisiblePosition();
    return VisiblePosition(firstCandidate);
}

VisiblePosition startOfDocument(const VisiblePosition& c)
{
    return startOfDocument(c.deepEquivalent().deprecatedNode());
}

VisiblePosition endOfDocument(const Node* node)
{
    if (!node || !node->document().documentElement())
        return VisiblePosition();
    
    // (As above, in startOfDocument.)  The canonicalization can reject valid visible positions
    // when descending from the root element, so we construct the visible position directly from a
    // valid candidate.
    Position lastPosition = createLegacyEditingPosition(node->document().documentElement(), node->document().documentElement()->countChildNodes());
    Position lastCandidate = previousCandidate(lastPosition);
    if (lastCandidate.isNull())
        return VisiblePosition();
    return VisiblePosition(lastCandidate);
}

VisiblePosition endOfDocument(const VisiblePosition& c)
{
    return endOfDocument(c.deepEquivalent().deprecatedNode());
}

bool inSameDocument(const VisiblePosition& a, const VisiblePosition& b)
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

bool isStartOfDocument(const VisiblePosition& p)
{
    return p.isNotNull() && p.previous(CanCrossEditingBoundary).isNull();
}

bool isEndOfDocument(const VisiblePosition& p)
{
    return p.isNotNull() && p.next(CanCrossEditingBoundary).isNull();
}

// ---------

VisiblePosition startOfEditableContent(const VisiblePosition& visiblePosition)
{
    auto* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return { };

    return firstPositionInNode(highestRoot);
}

VisiblePosition endOfEditableContent(const VisiblePosition& visiblePosition)
{
    auto* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return { };

    return lastPositionInNode(highestRoot);
}

bool isEndOfEditableOrNonEditableContent(const VisiblePosition& p)
{
    return p.isNotNull() && p.next().isNull();
}

VisiblePosition leftBoundaryOfLine(const VisiblePosition& c, TextDirection direction, bool* reachedBoundary)
{
    return direction == TextDirection::LTR ? logicalStartOfLine(c, reachedBoundary) : logicalEndOfLine(c, reachedBoundary);
}

VisiblePosition rightBoundaryOfLine(const VisiblePosition& c, TextDirection direction, bool* reachedBoundary)
{
    return direction == TextDirection::LTR ? logicalEndOfLine(c, reachedBoundary) : logicalStartOfLine(c, reachedBoundary);
}

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
        boundary = useDownstream ? endOfSentence(previousSentencePosition(vp)) : startOfSentence(nextSentencePosition(vp));
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

static VisiblePosition nextCharacterBoundaryInDirection(const VisiblePosition& vp, SelectionDirection direction, EditingBoundaryCrossingRule rule)
{
    return directionIsDownstream(direction) ? vp.next(rule) : vp.previous(rule);
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

    // Positions can only be compared if they are in the same tree scope.
    ASSERT_IMPLIES(areVisiblePositionsInSameTreeScope(result, vp), useDownstream ? (result > vp) : (result < vp));

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

static VisiblePosition nextParagraphBoundaryInDirection(const VisiblePosition& position, SelectionDirection direction)
{
    auto useDownstream = directionIsDownstream(direction);
    auto lineDirection = position.lineDirectionPointForBlockDirectionNavigation();
    if (atBoundaryOfGranularity(position, ParagraphGranularity, direction))
        return useDownstream ? startOfParagraph(nextParagraphPosition(position, lineDirection)) : endOfParagraph(previousParagraphPosition(position, lineDirection));
    ASSERT(withinTextUnitOfGranularity(position, ParagraphGranularity, direction));
    return useDownstream ? endOfParagraph(position) : startOfParagraph(position);
}

static VisiblePosition nextDocumentBoundaryInDirection(const VisiblePosition& vp, SelectionDirection direction)
{
    return directionIsDownstream(direction) ? endOfDocument(vp) : startOfDocument(vp);
}

VisiblePosition positionOfNextBoundaryOfGranularity(const VisiblePosition& vp, TextGranularity granularity, SelectionDirection direction)
{
    switch (granularity) {
    case CharacterGranularity:
        return nextCharacterBoundaryInDirection(vp, direction, CanCrossEditingBoundary);
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

RefPtr<Range> enclosingTextUnitOfGranularity(const VisiblePosition& vp, TextGranularity granularity, SelectionDirection direction)
{
    // This is particularly inefficient.  We could easily obtain the answer with the boundaries computed below.
    if (!withinTextUnitOfGranularity(vp, granularity, direction))
        return nullptr;

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
            return nullptr;
    }

    if (prevBoundary.isNull() || nextBoundary.isNull())
        return nullptr;

    if (vp < prevBoundary || vp > nextBoundary)
        return nullptr;

    return Range::create(prevBoundary.deepEquivalent().deprecatedNode()->document(), prevBoundary, nextBoundary);
}

int distanceBetweenPositions(const VisiblePosition& vp, const VisiblePosition& other)
{
    if (vp.isNull() || other.isNull())
        return 0;

    bool thisIsStart = (vp < other);

    // Start must come first in the Range constructor.
    auto range = Range::create(vp.deepEquivalent().deprecatedNode()->document(),
                                        (thisIsStart ? vp : other),
                                        (thisIsStart ? other : vp));
    int distance = TextIterator::rangeLength(range.ptr());

    return (thisIsStart ? -distance : distance);
}

void charactersAroundPosition(const VisiblePosition& position, UChar32& oneAfter, UChar32& oneBefore, UChar32& twoBefore)
{
    const int maxCharacters = 3;
    UChar32 characters[maxCharacters] = { 0 };

    if (position.isNull() || isStartOfDocument(position))
        return;

    VisiblePosition startPosition = position;
    VisiblePosition endPosition = position;

    VisiblePosition nextPosition = nextCharacterBoundaryInDirection(position, DirectionForward, CannotCrossEditingBoundary);
    if (nextPosition.isNotNull())
        endPosition = nextPosition;

    VisiblePosition previousPosition = nextCharacterBoundaryInDirection(position, DirectionBackward, CannotCrossEditingBoundary);
    if (previousPosition.isNotNull()) {
        startPosition = previousPosition;
        previousPosition = nextCharacterBoundaryInDirection(previousPosition, DirectionBackward, CannotCrossEditingBoundary);
        if (previousPosition.isNotNull())
            startPosition = previousPosition;
    }

    if (startPosition != endPosition) {
        String characterString = plainText(Range::create(position.deepEquivalent().anchorNode()->document(), startPosition, endPosition).ptr()).replace(noBreakSpace, ' ');
        for (int i = characterString.length() - 1, index = 0; i >= 0 && index < maxCharacters; --i) {
            if (!index && nextPosition.isNull())
                index++;
            characters[index++] = characterString[i];
        }
    }
    oneAfter = characters[0];
    oneBefore = characters[1];
    twoBefore = characters[2];
}

RefPtr<Range> wordRangeFromPosition(const VisiblePosition& position)
{
    // The selection could be in a non visible element and we don't have a VisiblePosition.
    if (position.isNull())
        return nullptr;

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

    // move the position at the end of the word
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

RefPtr<Range> rangeExpandedByCharactersInDirectionAtWordBoundary(const VisiblePosition& position, int numberOfCharactersToExpand, SelectionDirection direction)
{
    Position start = position.deepEquivalent();
    Position end = position.deepEquivalent();
    for (int i = 0; i < numberOfCharactersToExpand; ++i) {
        if (direction == DirectionBackward)
            start = start.previous(Character);
        else
            end = end.next(Character);
    }
    
    if (direction == DirectionBackward && !atBoundaryOfGranularity(start, WordGranularity, DirectionBackward))
        start = startOfWord(start).deepEquivalent();
    if (direction == DirectionForward && !atBoundaryOfGranularity(end, WordGranularity, DirectionForward))
        end = endOfWord(end).deepEquivalent();

    return makeRange(start, end);
}    

RefPtr<Range> rangeExpandedAroundPositionByCharacters(const VisiblePosition& position, int numberOfCharactersToExpand)
{
    Position start = position.deepEquivalent();
    Position end = position.deepEquivalent();
    for (int i = 0; i < numberOfCharactersToExpand; ++i) {
        start = start.previous(Character);
        end = end.next(Character);
    }
    
    return makeRange(start, end);
}    

}
