/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include "InlineIteratorBox.h"
#include "InlineIteratorLineBox.h"
#include "InlineIteratorLogicalOrderTraversal.h"
#include "InlineRunAndOffset.h"
#include "NodeTraversal.h"
#include "Range.h"
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
static Position previousLineCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    auto* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* previousNode = previousLeafWithSameEditability(node, editableType);

    while (previousNode && (!previousNode->renderer() || inSameLine(firstPositionInOrBeforeNode(previousNode), visiblePosition)))
        previousNode = previousLeafWithSameEditability(previousNode, editableType);

    while (previousNode && !previousNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(previousNode), editableType) != highestRoot)
            break;

        Position pos = previousNode->hasTagName(brTag) ? positionBeforeNode(previousNode) :
            makeDeprecatedLegacyPosition(previousNode, caretMaxOffset(*previousNode));
        
        if (pos.isCandidate())
            return pos;

        previousNode = previousLeafWithSameEditability(previousNode, editableType);
    }
    return Position();
}

static Position nextLineCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    auto* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* nextNode = nextLeafWithSameEditability(node, editableType);
    while (nextNode && (!nextNode->renderer() || inSameLine(firstPositionInOrBeforeNode(nextNode), visiblePosition)))
        nextNode = nextLeafWithSameEditability(nextNode, ContentIsEditable);

    while (nextNode && !nextNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(nextNode), editableType) != highestRoot)
            break;

        Position pos;
        pos = makeDeprecatedLegacyPosition(nextNode, caretMinOffset(*nextNode));
        
        if (pos.isCandidate())
            return pos;

        nextNode = nextLeafWithSameEditability(nextNode, editableType);
    }
    return Position();
}

static bool isTextOrLineBreakBox(InlineIterator::LeafBoxIterator box)
{
    return box && (box->isText() || box->renderer().isBR());
}

static InlineIterator::LeafBoxIterator previousTextOrLineBreakBox(InlineIterator::LeafBoxIterator box, InlineIterator::LineLogicalOrderCache& orderCache)
{
    while (box) {
        box = InlineIterator::previousLeafOnLineInLogicalOrder(box, orderCache);
        if (isTextOrLineBreakBox(box))
            return box;
    }
    return { };
}

static InlineIterator::LeafBoxIterator nextTextOrLineBreakBox(InlineIterator::LeafBoxIterator box, InlineIterator::LineLogicalOrderCache& orderCache)
{
    while (box) {
        box = InlineIterator::nextLeafOnLineInLogicalOrder(box, orderCache);
        if (isTextOrLineBreakBox(box))
            return box;
    }
    return { };
}

static InlineIterator::LeafBoxIterator startTextOrLineBreakBox(InlineIterator::LineBoxIterator lineBox, InlineIterator::LineLogicalOrderCache& orderCache)
{
    auto box = InlineIterator::firstLeafOnLineInLogicalOrder(lineBox, orderCache);
    if (isTextOrLineBreakBox(box))
        return box;
    return nextTextOrLineBreakBox(box, orderCache);
}

static InlineIterator::LeafBoxIterator endTextOrLineBreakBox(InlineIterator::LineBoxIterator lineBox, InlineIterator::LineLogicalOrderCache& orderCache)
{
    auto box = InlineIterator::lastLeafOnLineInLogicalOrder(lineBox, orderCache);
    if (isTextOrLineBreakBox(box))
        return box;
    return previousTextOrLineBreakBox(box, orderCache);
}

static const InlineIterator::LeafBoxIterator logicallyPreviousBox(const VisiblePosition& visiblePosition, InlineIterator::LeafBoxIterator startBox, InlineIterator::LineLogicalOrderCache& orderCache, bool& previousBoxInDifferentLine)
{
    if (auto previousBox = previousTextOrLineBreakBox(startBox, orderCache))
        return previousBox;

    if (auto previousLineBox = startBox->lineBox()->previous()) {
        // FIXME: Why isn't previousBoxInDifferentLine set here?
        if (auto previousBox = endTextOrLineBreakBox(previousLineBox, orderCache))
            return previousBox;
    }

    while (1) {
        auto* startNode = startBox->renderer().nonPseudoNode();
        if (!startNode)
            break;

        Position position = previousLineCandidatePosition(startNode, visiblePosition, ContentIsEditable);
        if (position.isNull())
            break;

        RenderedPosition renderedPosition(position, Affinity::Downstream);
        auto previousLineBox = renderedPosition.lineBox();
        if (!previousLineBox)
            break;

        if (previousLineBox != startBox->lineBox()) {
            if (auto previousBox = endTextOrLineBreakBox(previousLineBox, orderCache)) {
                previousBoxInDifferentLine = true;
                return previousBox;
            }
        }

        startBox = InlineIterator::firstLeafOnLineInLogicalOrder(previousLineBox, orderCache);
    }
    return { };
}


static const InlineIterator::LeafBoxIterator logicallyNextBox(const VisiblePosition& visiblePosition, InlineIterator::LeafBoxIterator startBox, InlineIterator::LineLogicalOrderCache& orderCache, bool& nextBoxInDifferentLine)
{
    if (auto nextBox = nextTextOrLineBreakBox(startBox, orderCache))
        return nextBox;

    if (auto nextLineBox = startBox->lineBox()->next()) {
        // FIXME: Why isn't previousBoxInDifferentLine set here?
        if (auto nextBox = startTextOrLineBreakBox(nextLineBox, orderCache))
            return nextBox;
    }

    while (1) {
        auto* startNode = startBox->renderer().nonPseudoNode();
        if (!startNode)
            break;

        Position position = nextLineCandidatePosition(startNode, visiblePosition, ContentIsEditable);
        if (position.isNull())
            break;

        RenderedPosition renderedPosition(position, Affinity::Downstream);
        auto nextLineBox = renderedPosition.lineBox();
        if (!nextLineBox)
            break;

        if (nextLineBox != startBox->lineBox()) {
            if (auto nextBox = startTextOrLineBreakBox(nextLineBox, orderCache)) {
                nextBoxInDifferentLine = true;
                return nextBox;
            }
        }

        startBox = InlineIterator::lastLeafOnLineInLogicalOrderWithNode(nextLineBox, orderCache);
    }
    return { };
}

static UBreakIterator* wordBreakIteratorForMinOffsetBoundary(const VisiblePosition& visiblePosition, InlineIterator::TextBoxIterator textBox,
    unsigned& previousBoxLength, bool& previousBoxInDifferentLine, Vector<UChar, 1024>& string)
{
    previousBoxInDifferentLine = false;

    InlineIterator::LineLogicalOrderCache orderCache;
    auto previousBox = logicallyPreviousBox(visiblePosition, textBox, orderCache, previousBoxInDifferentLine);
    while (previousBox && !previousBox->isText()) {
        ASSERT(previousBox->renderer().isBR());
        previousBoxInDifferentLine = true;
        previousBox = logicallyPreviousBox(visiblePosition, previousBox, orderCache, previousBoxInDifferentLine);
    }

    string.clear();

    if (previousBox) {
        auto& previousTextBox = downcast<InlineIterator::TextBoxIterator>(previousBox);
        previousBoxLength = previousTextBox->length();
        append(string, previousTextBox->originalText());
    }
    append(string, textBox->originalText());

    return wordBreakIterator(StringView(string.data(), string.size()));
}

static UBreakIterator* wordBreakIteratorForMaxOffsetBoundary(const VisiblePosition& visiblePosition, InlineIterator::TextBoxIterator textBox,
    bool& nextBoxInDifferentLine, Vector<UChar, 1024>& string)
{
    nextBoxInDifferentLine = false;

    InlineIterator::LineLogicalOrderCache orderCache;
    auto nextBox = logicallyNextBox(visiblePosition, textBox, orderCache, nextBoxInDifferentLine);
    while (nextBox && !nextBox->isText()) {
        ASSERT(nextBox->renderer().isBR());
        nextBoxInDifferentLine = true;
        nextBox = logicallyNextBox(visiblePosition, nextBox, orderCache, nextBoxInDifferentLine);
    }

    string.clear();
    append(string, textBox->originalText());

    if (nextBox) {
        auto& nextTextBox = downcast<InlineIterator::TextBoxIterator>(nextBox);
        append(string, nextTextBox->originalText());
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
    if (visiblePosition.isNull() || !visiblePosition.deepEquivalent().document())
        return VisiblePosition();

    visiblePosition.deepEquivalent().document()->updateLayoutIgnorePendingStylesheets();

    TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
    InlineIterator::LeafBoxIterator previouslyVisitedBox;
    VisiblePosition current = visiblePosition;
    std::optional<VisiblePosition> previousPosition;
    UBreakIterator* iter = nullptr;

    Vector<UChar, 1024> string;

    while (1) {
        VisiblePosition adjacentCharacterPosition = direction == MoveRight ? current.right(true) : current.left(true); 
        if (adjacentCharacterPosition == current || adjacentCharacterPosition.isNull())
            return VisiblePosition();
        // FIXME: This is a workaround for webkit.org/b/167138.
        if (previousPosition && adjacentCharacterPosition == previousPosition.value())
            return VisiblePosition();
    
        // FIXME: Why force the use of upstream affinity here instead of VisiblePosition::inlineBoxAndOffset, which will get affinity from adjacentCharacterPosition?
        auto [box, offsetInBox] = adjacentCharacterPosition.deepEquivalent().inlineBoxAndOffset(Affinity::Upstream);
    
        if (!box)
            break;
        if (!box->isText()) {
            current = adjacentCharacterPosition;
            continue;
        }

        auto& textBox = downcast<InlineIterator::TextBoxIterator>(box);
        unsigned previousBoxLength = 0;
        bool previousBoxInDifferentLine = false;
        bool nextBoxInDifferentLine = false;
        bool movingIntoNewBox = previouslyVisitedBox != box;

        if (offsetInBox == textBox->minimumCaretOffset())
            iter = wordBreakIteratorForMinOffsetBoundary(adjacentCharacterPosition, textBox, previousBoxLength, previousBoxInDifferentLine, string);
        else if (offsetInBox == textBox->maximumCaretOffset())
            iter = wordBreakIteratorForMaxOffsetBoundary(adjacentCharacterPosition, textBox, nextBoxInDifferentLine, string);
        else if (movingIntoNewBox) {
            iter = wordBreakIterator(textBox->originalText());
            previouslyVisitedBox = box;
        }

        if (!iter)
            break;

        ubrk_first(iter);
        int offsetInIterator = offsetInBox - textBox->start() + previousBoxLength;

        bool isWordBreak;
        bool boxHasSameDirectionalityAsBlock = box->direction() == blockDirection;
        bool movingBackward = (direction == MoveLeft && box->direction() == TextDirection::LTR) || (direction == MoveRight && box->direction() == TextDirection::RTL);
        if ((skipsSpaceWhenMovingRight && boxHasSameDirectionalityAsBlock)
            || (!skipsSpaceWhenMovingRight && movingBackward)) {
            bool logicalStartInRenderer = offsetInBox == textBox->start() && previousBoxInDifferentLine;
            isWordBreak = isLogicalStartOfWord(iter, offsetInIterator, logicalStartInRenderer);
            if (isWordBreak && offsetInBox == box->maximumCaretOffset() && nextBoxInDifferentLine)
                isWordBreak = false;
        } else {
            bool logicalEndInRenderer = offsetInBox == textBox->end() && nextBoxInDifferentLine;
            isWordBreak = islogicalEndOfWord(iter, offsetInIterator, logicalEndInRenderer);
            if (isWordBreak && offsetInBox == box->minimumCaretOffset() && previousBoxInDifferentLine)
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

unsigned suffixLengthForRange(const SimpleRange& forwardsScanRange, Vector<UChar, 1024>& string)
{
    unsigned suffixLength = 0;
    TextIterator forwardsIterator(forwardsScanRange);
    while (!forwardsIterator.atEnd()) {
        StringView text = forwardsIterator.text();
        unsigned i = endOfFirstWordBoundaryContext(text);
        append(string, text.left(i));
        suffixLength += i;
        if (i < text.length())
            break;
        forwardsIterator.advance();
    }
    return suffixLength;
}

unsigned prefixLengthForRange(const SimpleRange& backwardsScanRange, Vector<UChar, 1024>& string)
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
static VisiblePosition previousBoundary(const VisiblePosition& position, BoundarySearchFunction searchFunction,
    NeedsContextAtParagraphStart needsContextAtParagraphStart = NeedsContextAtParagraphStart::No)
{
    auto boundary = position.deepEquivalent().parentEditingBoundary();
    if (!boundary)
        return { };

    Vector<UChar, 1024> string;
    unsigned suffixLength = 0;

    auto searchRange = makeSimpleRange(makeBoundaryPointBeforeNodeContents(*boundary), position);
    if (!searchRange)
        return { };

    if (needsContextAtParagraphStart == NeedsContextAtParagraphStart::Yes && isStartOfParagraph(position)) {
        auto forwardsScanRange = makeSimpleRange(searchRange->start, endOfParagraph(position));
        if (!forwardsScanRange)
            return { };
        for (TextIterator forwardsIterator(*forwardsScanRange); !forwardsIterator.atEnd(); forwardsIterator.advance())
            append(string, forwardsIterator.text());
        suffixLength = string.size();
    } else if (requiresContextForWordBoundary(position.characterBefore())) {
        auto forwardsScanRange = makeSimpleRange(searchRange->end, makeBoundaryPointAfterNodeContents(*boundary));
        suffixLength = suffixLengthForRange(forwardsScanRange, string);
    }

    SimplifiedBackwardsTextIterator it(*searchRange);
    unsigned next = backwardSearchForBoundaryWithTextIterator(it, string, suffixLength, searchFunction);

    if (!next)
        return it.atEnd() ? makeDeprecatedLegacyPosition(searchRange->start) : position;

    auto& node = (it.atEnd() ? *searchRange : it.range()).start.container.get();
    if ((!suffixLength && is<Text>(node) && next <= downcast<Text>(node).length()) || (node.renderer() && node.renderer()->isBR() && !next)) {
        // The next variable contains a usable index into a text node.
        return makeDeprecatedLegacyPosition(&node, next);
    }

    // Use the character iterator to translate the next value into a DOM position.
    BackwardsCharacterIterator charIt(*searchRange);
    if (next < string.size() - suffixLength)
        charIt.advance(string.size() - suffixLength - next);
    // FIXME: charIt can get out of shadow host.
    return makeDeprecatedLegacyPosition(charIt.range().end);
}

static VisiblePosition nextBoundary(const VisiblePosition& c, BoundarySearchFunction searchFunction)
{
    Position pos = c.deepEquivalent();
    Node* boundary = pos.parentEditingBoundary();
    if (!boundary)
        return VisiblePosition();

    Document& boundaryDocument = boundary->document();

    Vector<UChar, 1024> string;
    unsigned prefixLength = 0;

    if (requiresContextForWordBoundary(c.characterAfter())) {
        auto backwardsScanRange = makeSimpleRange(makeBoundaryPointBeforeNodeContents(boundaryDocument), c);
        if (!backwardsScanRange)
            return { };
        prefixLength = prefixLengthForRange(*backwardsScanRange, string);
    }

    auto searchRange = makeSimpleRange(c, makeBoundaryPointAfterNodeContents(*boundary));
    if (!searchRange)
        return { };

    TextIterator it(*searchRange, TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions);
    unsigned next = forwardSearchForBoundaryWithTextIterator(it, string, prefixLength, searchFunction);
    
    if (it.atEnd() && next == string.size())
        pos = makeDeprecatedLegacyPosition(searchRange->end);
    else if (next > prefixLength) {
        // Use the character iterator to translate the next value into a DOM position.
        CharacterIterator charIt(*searchRange, TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions);
        charIt.advance(next - prefixLength - 1);
        if (charIt.atEnd())
            return { };

        auto characterRange = charIt.range();
        pos = makeDeprecatedLegacyPosition(characterRange.end);
        
        if (charIt.text()[0] == '\n') {
            // FIXME: workaround for collapsed range (where only start position is correct) emitted for some emitted newlines (see rdar://5192593)
            if (VisiblePosition(pos) == VisiblePosition(makeDeprecatedLegacyPosition(characterRange.start))) {
                charIt.advance(1);
                pos = makeDeprecatedLegacyPosition(charIt.range().start);
            }
        }
    }

    return VisiblePosition(pos, Affinity::Upstream);
}

// ---------

unsigned startWordBoundary(StringView text, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    ASSERT(offset);
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(text.left(offset))) {
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
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(text.left(offset))) {
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

    auto lineBox = RenderedPosition(c).lineBox();
    if (!lineBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        Position p = c.deepEquivalent();
        if (p.deprecatedNode()->renderer() && p.deprecatedNode()->renderer()->isRenderBlock() && !p.deprecatedEditingOffset())
            return c;

        return VisiblePosition();
    }

    InlineIterator::LineLogicalOrderCache orderCache;

    Node* startNode = nullptr;
    auto startBox = mode == UseLogicalOrdering ? InlineIterator::firstLeafOnLineInLogicalOrderWithNode(lineBox, orderCache) : lineBox->firstLeafBox();
    // Generated content (e.g. list markers and CSS :before and :after pseudoelements) have no corresponding DOM element,
    // and so cannot be represented by a VisiblePosition. Use whatever follows instead.
    while (true) {
        if (!startBox)
            return VisiblePosition();

        startNode = startBox->renderer().nonPseudoNode();
        if (startNode)
            break;

        if (mode == UseLogicalOrdering)
            startBox = InlineIterator::nextLeafOnLineInLogicalOrder(startBox, orderCache);
        else
            startBox.traverseNextOnLine();
    }

    return is<Text>(*startNode) ? Position(downcast<Text>(startNode), downcast<InlineIterator::TextBox>(*startBox).start())
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

    auto lineBox = RenderedPosition(c).lineBox();
    if (!lineBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        Position p = c.deepEquivalent();
        if (p.deprecatedNode()->renderer() && p.deprecatedNode()->renderer()->isRenderBlock() && !p.deprecatedEditingOffset())
            return c;
        return VisiblePosition();
    }

    InlineIterator::LineLogicalOrderCache orderCache;

    Node* endNode = nullptr;
    auto endBox = mode == UseLogicalOrdering ? InlineIterator::lastLeafOnLineInLogicalOrder(lineBox, orderCache) : lineBox->lastLeafBox();
    // Generated content (e.g. list markers and CSS :before and :after pseudoelements) have no corresponding DOM element,
    // and so cannot be represented by a VisiblePosition. Use whatever precedes instead.
    while (true) {
        if (!endBox)
            return VisiblePosition();

        endNode = endBox->renderer().nonPseudoNode();
        if (endNode)
            break;

        if (mode == UseLogicalOrdering)
            endBox = InlineIterator::previousLeafOnLineInLogicalOrder(endBox, orderCache);
        else
            endBox.traversePreviousOnLine();
    }

    Position pos;
    if (is<HTMLBRElement>(*endNode))
        pos = positionBeforeNode(endNode);
    else if (is<InlineIterator::TextBox>(*endBox) && is<Text>(*endNode)) {
        auto& endTextBox = downcast<InlineIterator::TextBox>(*endBox);
        int endOffset = endTextBox.start();
        if (!endTextBox.isLineBreak())
            endOffset += endTextBox.length();
        pos = Position(downcast<Text>(endNode), endOffset);
    } else
        pos = positionAfterNode(endNode);
    
    return VisiblePosition(pos, Affinity::Upstream);
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

bool isLogicalEndOfLine(const VisiblePosition& p)
{
    return p.isNotNull() && p == logicalEndOfLine(p);
}

static inline IntPoint absoluteLineDirectionPointToLocalPointInBlock(InlineIterator::LineBoxIterator& lineBox, int lineDirectionPoint)
{
    auto& root = lineBox->formattingContextRoot();
    auto absoluteBlockPoint = root.localToAbsolute(FloatPoint()) - toFloatSize(root.scrollPosition());

    if (root.isHorizontalWritingMode())
        return IntPoint(lineDirectionPoint - absoluteBlockPoint.x(), contentStartInBlockDirection(*lineBox));

    return IntPoint(contentStartInBlockDirection(*lineBox), lineDirectionPoint - absoluteBlockPoint.y());
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

    InlineIterator::LineBoxIterator lineBox;
    if (auto box = visiblePosition.inlineBoxAndOffset().box) {
        lineBox = box->lineBox()->previous();
        // We want to skip zero height boxes.
        // This could happen in case it is a LegacyRootInlineBox with trailing floats.
        if (!lineBox || !lineBox->height() || !lineBox->firstLeafBox())
            lineBox = { };
    }

    if (!lineBox) {
        Position position = previousLineCandidatePosition(node, visiblePosition, editableType);
        if (position.isNotNull()) {
            RenderedPosition renderedPosition(position);
            lineBox = renderedPosition.lineBox();
            if (!lineBox)
                return position;
        }
    }
    
    if (lineBox) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        auto pointInLine = absoluteLineDirectionPointToLocalPointInBlock(lineBox, lineDirectionPoint);
        auto box = closestBoxForHorizontalPosition(*lineBox, lineBox->isHorizontal() ? pointInLine.x() : pointInLine.y(), isEditablePosition(p));
        if (!box)
            return VisiblePosition();
        auto& renderer = box->renderer();
        Node* node = renderer.node();
        if (node && editingIgnoresContent(*node))
            return positionInParentBeforeNode(node);
        return const_cast<RenderObject&>(renderer).positionForPoint(pointInLine, nullptr);
    }
    
    // Could not find a previous line. This means we must already be on the first line.
    // Move to the start of the content in this block, which effectively moves us
    // to the start of the line we're on.
    Element* rootElement = rootEditableOrDocumentElement(*node, editableType);
    if (!rootElement)
        return VisiblePosition();
    return firstPositionInNode(rootElement);
}

VisiblePosition nextLinePosition(const VisiblePosition& visiblePosition, int lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.deprecatedNode();
    if (!node)
        return VisiblePosition();
    
    node->document().updateLayoutIgnorePendingStylesheets();

    if (!node->renderer())
        return VisiblePosition();

    InlineIterator::LineBoxIterator lineBox;
    if (auto box = visiblePosition.inlineBoxAndOffset().box) {
        lineBox = box->lineBox()->next();
        // We want to skip zero height boxes.
        // This could happen in case it is a LegacyRootInlineBox with trailing floats.
        if (!lineBox || !lineBox->height() || !lineBox->firstLeafBox())
            lineBox = { };
    }

    if (!lineBox) {
        // FIXME: We need do the same in previousLinePosition.
        Node* child = node->traverseToChildAt(p.deprecatedEditingOffset());
        node = child ? child : node->lastDescendant();
        Position position = nextLineCandidatePosition(node, visiblePosition, editableType);
        if (position.isNotNull()) {
            RenderedPosition renderedPosition(position);
            lineBox = renderedPosition.lineBox();
            if (!lineBox)
                return position;
        }
    }
    
    if (lineBox) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        auto pointInLine = absoluteLineDirectionPointToLocalPointInBlock(lineBox, lineDirectionPoint);
        auto box = closestBoxForHorizontalPosition(*lineBox, lineBox->isHorizontal() ? pointInLine.x() : pointInLine.y(), isEditablePosition(p));
        if (!box)
            return VisiblePosition();
        auto& renderer = box->renderer();
        Node* node = renderer.node();
        if (node && editingIgnoresContent(*node))
            return positionInParentBeforeNode(node);
        return const_cast<RenderObject&>(renderer).positionForPoint(pointInLine, nullptr);
    }

    // Could not find a next line. This means we must already be on the last line.
    // Move to the end of the content in this block, which effectively moves us
    // to the end of the line we're on.
    Element* rootElement = rootEditableOrDocumentElement(*node, editableType);
    if (!rootElement)
        return VisiblePosition();
    return lastPositionInNode(rootElement);
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
        if (boundaryCrossingRule == CannotCrossEditingBoundary && !Position::nodeIsUserSelectAll(n) && n->hasEditableStyle() != startNode->hasEditableStyle())
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
        if (boundaryCrossingRule == CannotCrossEditingBoundary && !Position::nodeIsUserSelectAll(n) && n->hasEditableStyle() != startNode->hasEditableStyle())
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
        if (r->isBR() || is<HTMLBRElement>(n) || isBlock(n))
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
        return Position(downcast<Text>(node), offset);
    
    if (type == Position::PositionIsOffsetInAnchor) {
        ASSERT(type == Position::PositionIsOffsetInAnchor || !offset);
        return Position(node, offset, type);
    }
    
    return Position(node, type);
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
        return Position(downcast<Text>(node), offset);
    
    if (type == Position::PositionIsOffsetInAnchor)
        return Position(node, offset, type);

    return Position(node, type);
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
    Position firstCandidate = nextCandidate(makeDeprecatedLegacyPosition(node->document().documentElement(), 0));
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
    Position lastPosition = makeDeprecatedLegacyPosition(node->document().documentElement(), node->document().documentElement()->countChildNodes());
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
    if (direction == SelectionDirection::Backward)
        return false;
    if (direction == SelectionDirection::Forward)
        return true;

    // FIXME: this code doesn't take into account the original direction of the element.
    // I'm not fixing this now because I'm afraid there is some code in UIKit relying on
    // this wrong behavior.
    return direction == SelectionDirection::Right;
}

bool atBoundaryOfGranularity(const VisiblePosition& vp, TextGranularity granularity, SelectionDirection direction)
{
    if (granularity == TextGranularity::CharacterGranularity)
        return true;

    VisiblePosition boundary;

    bool useDownstream = directionIsDownstream(direction);

    switch (granularity) {
    case TextGranularity::WordGranularity:
        // visible_units claims erroneously that the start and the end
        // of a paragraph are the end and start of a word, respectively.
        if ((useDownstream && isStartOfParagraph(vp)) || (!useDownstream && isEndOfParagraph(vp)))
            return false;

        // Note that "Left" and "Right" in this context apparently mean "upstream/previous" and "downstream/next".
        boundary = useDownstream ? endOfWord(vp, LeftWordIfOnBoundary) : startOfWord(vp, RightWordIfOnBoundary);
        break;

    case TextGranularity::SentenceGranularity: {
        auto boundaryInDirection = useDownstream ? endOfSentence : startOfSentence;
        if (vp == boundaryInDirection(vp)) {
            boundary = vp;
            break;
        }

        auto position = useDownstream ? previousSentencePosition(vp) : nextSentencePosition(vp);
        boundary = boundaryInDirection(position);
        break;
    }

    case TextGranularity::LineGranularity:
        boundary = vp;
        boundary.setAffinity(useDownstream ? Affinity::Upstream : Affinity::Downstream);
        boundary = useDownstream ? endOfLine(boundary) : startOfLine(boundary);
        break;

    case TextGranularity::ParagraphGranularity:
        boundary = useDownstream ? endOfParagraph(vp) : startOfParagraph(vp);
        break;

    case TextGranularity::DocumentGranularity:
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
    if (granularity == TextGranularity::CharacterGranularity || granularity == TextGranularity::DocumentGranularity)
        return true;

    bool useDownstream = directionIsDownstream(direction);

    VisiblePosition prevBoundary;
    VisiblePosition nextBoundary;
    
    switch (granularity) {
    case TextGranularity::WordGranularity:
        // Note that "Left" and "Right" in this context apparently mean "upstream/previous" and "downstream/next".
        prevBoundary = startOfWord(vp, (useDownstream ? RightWordIfOnBoundary : LeftWordIfOnBoundary));
        nextBoundary = endOfWord(vp, (useDownstream ? RightWordIfOnBoundary : LeftWordIfOnBoundary));
    
        // Workaround for <rdar://problem/7259611> Word boundary code on iPhone gives different results than desktop
        if (endOfWord(prevBoundary, RightWordIfOnBoundary) != nextBoundary)
            return false;

        break;

    case TextGranularity::SentenceGranularity:
        prevBoundary = startOfSentence(vp);
        nextBoundary = endOfSentence(vp);
        break;

    case TextGranularity::LineGranularity:
        prevBoundary = startOfLine(vp);
        nextBoundary = endOfLine(vp);

        if (prevBoundary == nextBoundary) {
            nextBoundary = nextLinePosition(nextBoundary, 0);
            nextBoundary.setAffinity(Affinity::Upstream);
            if (!inSameLine(prevBoundary, nextBoundary))
                nextBoundary = vp.next();
        }
        break;

    case TextGranularity::ParagraphGranularity:
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
    bool withinUnitOfGranularity = withinTextUnitOfGranularity(vp, TextGranularity::WordGranularity, direction);
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
    bool withinUnitOfGranularity = withinTextUnitOfGranularity(vp, TextGranularity::SentenceGranularity, direction);
    VisiblePosition result;

    if (withinUnitOfGranularity)
        result = useDownstream ? endOfSentence(vp) : startOfSentence(vp);
    else {
        result = useDownstream ? nextSentencePosition(vp) : previousSentencePosition(vp);
        if (result.isNull() || result == vp)
            return VisiblePosition();

        result = useDownstream ? startOfSentence(result) : endOfSentence(result);
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
        result.setAffinity(Affinity::Downstream);
        result = isEndOfLine(result) ? startOfLine(nextLinePosition(result, result.lineDirectionPointForBlockDirectionNavigation())) : endOfLine(result);
    } else {
        result.setAffinity(Affinity::Upstream);
        result = isStartOfLine(result) ? endOfLine(previousLinePosition(result, result.lineDirectionPointForBlockDirectionNavigation())) : startOfLine(result);
    }

    return result;
}

static VisiblePosition nextParagraphBoundaryInDirection(const VisiblePosition& position, SelectionDirection direction)
{
    auto useDownstream = directionIsDownstream(direction);
    auto lineDirection = position.lineDirectionPointForBlockDirectionNavigation();
    if (atBoundaryOfGranularity(position, TextGranularity::ParagraphGranularity, direction))
        return useDownstream ? startOfParagraph(nextParagraphPosition(position, lineDirection)) : endOfParagraph(previousParagraphPosition(position, lineDirection));
    ASSERT(withinTextUnitOfGranularity(position, TextGranularity::ParagraphGranularity, direction));
    return useDownstream ? endOfParagraph(position) : startOfParagraph(position);
}

static VisiblePosition nextDocumentBoundaryInDirection(const VisiblePosition& vp, SelectionDirection direction)
{
    return directionIsDownstream(direction) ? endOfDocument(vp) : startOfDocument(vp);
}

VisiblePosition positionOfNextBoundaryOfGranularity(const VisiblePosition& vp, TextGranularity granularity, SelectionDirection direction)
{
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        return nextCharacterBoundaryInDirection(vp, direction, CanCrossEditingBoundary);
    case TextGranularity::WordGranularity:
        return nextWordBoundaryInDirection(vp, direction);
    case TextGranularity::SentenceGranularity:
        return nextSentenceBoundaryInDirection(vp, direction);
    case TextGranularity::LineGranularity:
        return nextLineBoundaryInDirection(vp, direction);
    case TextGranularity::ParagraphGranularity:
        return nextParagraphBoundaryInDirection(vp, direction);
    case TextGranularity::DocumentGranularity:
        return nextDocumentBoundaryInDirection(vp, direction);
    default:
        ASSERT_NOT_REACHED();
        return VisiblePosition();
    }
}

std::optional<SimpleRange> enclosingTextUnitOfGranularity(const VisiblePosition& vp, TextGranularity granularity, SelectionDirection direction)
{
    // This is particularly inefficient.  We could easily obtain the answer with the boundaries computed below.
    if (!withinTextUnitOfGranularity(vp, granularity, direction))
        return std::nullopt;

    VisiblePosition prevBoundary;
    VisiblePosition nextBoundary;
    bool useDownstream = directionIsDownstream(direction);

    switch (granularity) {
        case TextGranularity::CharacterGranularity:
            prevBoundary = vp;
            nextBoundary = prevBoundary.next();
            break;

        case TextGranularity::WordGranularity:
            // NB: "Left" and "Right" in this context apparently mean "upstream/previous" and "downstream/next".
            if (useDownstream) {
                prevBoundary = startOfWord(vp, RightWordIfOnBoundary);
                nextBoundary = endOfWord(vp, RightWordIfOnBoundary);
            } else {
                prevBoundary = startOfWord(vp, LeftWordIfOnBoundary);
                nextBoundary = endOfWord(vp, LeftWordIfOnBoundary);
            }
            break;

        case TextGranularity::SentenceGranularity:
            prevBoundary = startOfSentence(vp);
            nextBoundary = endOfSentence(vp);
            break;

        case TextGranularity::LineGranularity:
            prevBoundary = startOfLine(vp);
            nextBoundary = endOfLine(vp);

            if (prevBoundary == nextBoundary) {
                nextBoundary = nextLinePosition(nextBoundary, 0);
                nextBoundary.setAffinity(Affinity::Upstream);
                if (!inSameLine(prevBoundary, nextBoundary))
                    nextBoundary = vp.next();
            }
            break;

        case TextGranularity::ParagraphGranularity:
            prevBoundary = startOfParagraph(vp);
            nextBoundary = endOfParagraph(vp);
            break;

        case TextGranularity::DocumentGranularity:
            prevBoundary = startOfDocument(vp);
            nextBoundary = endOfDocument(vp);
            break;

        default:
            ASSERT_NOT_REACHED();
            return std::nullopt;
    }

    if (prevBoundary.isNull() || nextBoundary.isNull())
        return std::nullopt;

    if (vp < prevBoundary || vp > nextBoundary)
        return std::nullopt;

    return makeSimpleRange(prevBoundary, nextBoundary);
}

std::ptrdiff_t distanceBetweenPositions(const VisiblePosition& a, const VisiblePosition& b)
{
    if (a.isNull() || b.isNull())
        return 0;
    return a < b ? -characterCount(*makeSimpleRange(a, b)) : characterCount(*makeSimpleRange(b, a));
}

void charactersAroundPosition(const VisiblePosition& position, UChar32& oneAfter, UChar32& oneBefore, UChar32& twoBefore)
{
    const int maxCharacters = 3;
    UChar32 characters[maxCharacters] = { 0 };

    if (position.isNull() || isStartOfDocument(position))
        return;

    VisiblePosition startPosition = position;
    VisiblePosition endPosition = position;

    VisiblePosition nextPosition = nextCharacterBoundaryInDirection(position, SelectionDirection::Forward, CannotCrossEditingBoundary);
    if (nextPosition.isNotNull())
        endPosition = nextPosition;

    VisiblePosition previousPosition = nextCharacterBoundaryInDirection(position, SelectionDirection::Backward, CannotCrossEditingBoundary);
    if (previousPosition.isNotNull()) {
        startPosition = previousPosition;
        previousPosition = nextCharacterBoundaryInDirection(previousPosition, SelectionDirection::Backward, CannotCrossEditingBoundary);
        if (previousPosition.isNotNull())
            startPosition = previousPosition;
    }

    if (startPosition != endPosition) {
        String characterString = makeStringByReplacingAll(plainText(*makeSimpleRange(startPosition, endPosition)), noBreakSpace, ' ');
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

std::optional<SimpleRange> wordRangeFromPosition(const VisiblePosition& position)
{
    if (position.isNull())
        return std::nullopt;

    if (auto range = enclosingTextUnitOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Backward))
        return range;
    if (auto range = enclosingTextUnitOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Forward))
        return range;

    VisiblePosition currentPosition = position;
    do {
        currentPosition = positionOfNextBoundaryOfGranularity(currentPosition, TextGranularity::WordGranularity, SelectionDirection::Backward);
    } while (currentPosition.isNotNull() && !atBoundaryOfGranularity(currentPosition, TextGranularity::WordGranularity, SelectionDirection::Backward));
    if (currentPosition.isNull())
        currentPosition = positionOfNextBoundaryOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Forward);
    return makeSimpleRange(currentPosition, position);
}

VisiblePosition closestWordBoundaryForPosition(const VisiblePosition& position)
{
    VisiblePosition result;
    if (atBoundaryOfGranularity(position, TextGranularity::LineGranularity, SelectionDirection::Forward)) {
        // Don't cross line boundaries.
        result = position;
    } else if (withinTextUnitOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Forward)) {
        // The position lies within a word.
        if (auto wordRange = enclosingTextUnitOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Forward)) {
            result = makeDeprecatedLegacyPosition(wordRange->start);
            if (distanceBetweenPositions(position, result) > 1)
                result = makeDeprecatedLegacyPosition(wordRange->end);
        }
    } else if (atBoundaryOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Backward)) {
        // The position is at the end of a word.
        result = position;
    } else {
        // The position is not within a word.
        // Go to the next boundary.
        result = positionOfNextBoundaryOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Forward);

        // If there is no such boundary we go to the end of the element.
        if (result.isNull())
            result = endOfEditableContent(position);
    }
    return result;
}

std::optional<SimpleRange> rangeExpandedByCharactersInDirectionAtWordBoundary(const VisiblePosition& position, int numberOfCharactersToExpand, SelectionDirection direction)
{
    Position start = position.deepEquivalent();
    Position end = position.deepEquivalent();
    for (int i = 0; i < numberOfCharactersToExpand; ++i) {
        if (direction == SelectionDirection::Backward)
            start = start.previous(Character);
        else
            end = end.next(Character);
    }
    
    if (direction == SelectionDirection::Backward && !atBoundaryOfGranularity(start, TextGranularity::WordGranularity, SelectionDirection::Backward))
        start = startOfWord(start).deepEquivalent();
    if (direction == SelectionDirection::Forward && !atBoundaryOfGranularity(end, TextGranularity::WordGranularity, SelectionDirection::Forward))
        end = endOfWord(end).deepEquivalent();

    return makeSimpleRange(start, end);
}    

std::optional<SimpleRange> rangeExpandedAroundPositionByCharacters(const VisiblePosition& position, int numberOfCharactersToExpand)
{
    Position start = position.deepEquivalent();
    Position end = position.deepEquivalent();
    for (int i = 0; i < numberOfCharactersToExpand; ++i) {
        start = start.previous(Character);
        end = end.next(Character);
    }
    return makeSimpleRange(start, end);
}

std::pair<VisiblePosition, WithinWordBoundary> wordBoundaryForPositionWithoutCrossingLine(const VisiblePosition& position)
{
    if (atBoundaryOfGranularity(position, TextGranularity::LineGranularity, SelectionDirection::Forward))
        return { position, WithinWordBoundary::No };

    if (withinTextUnitOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Forward)) {
        auto adjustedPosition = position;
        if (auto wordRange = enclosingTextUnitOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Forward)) {
            adjustedPosition = makeDeprecatedLegacyPosition(wordRange->start);
            if (distanceBetweenPositions(position, adjustedPosition) > 1)
                adjustedPosition = makeDeprecatedLegacyPosition(wordRange->end);
        }
        return { adjustedPosition, WithinWordBoundary::Yes };
    }

    if (atBoundaryOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Backward))
        return { position, WithinWordBoundary::No };

    auto nextWordBoundary = positionOfNextBoundaryOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Forward);
    return { nextWordBoundary.isNotNull() ? nextWordBoundary : endOfEditableContent(position), WithinWordBoundary::No };
}

}
