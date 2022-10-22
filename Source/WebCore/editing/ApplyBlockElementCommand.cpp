/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ApplyBlockElementCommand.h"

#include "Editing.h"
#include "HTMLBRElement.h"
#include "HTMLNames.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "Text.h"
#include "VisibleUnits.h"

namespace WebCore {

using namespace HTMLNames;

ApplyBlockElementCommand::ApplyBlockElementCommand(Document& document, const QualifiedName& tagName, const AtomString& inlineStyle)
    : CompositeEditCommand(document)
    , m_tagName(tagName)
    , m_inlineStyle(inlineStyle)
{
}

ApplyBlockElementCommand::ApplyBlockElementCommand(Document& document, const QualifiedName& tagName)
    : CompositeEditCommand(document)
    , m_tagName(tagName)
{
}

void ApplyBlockElementCommand::doApply()
{
    if (!endingSelection().rootEditableElement())
        return;

    VisiblePosition visibleEnd = endingSelection().visibleEnd();
    VisiblePosition visibleStart = endingSelection().visibleStart();
    if (visibleStart.isNull() || visibleStart.isOrphan() || visibleEnd.isNull() || visibleEnd.isOrphan())
        return;

    // When a selection ends at the start of a paragraph, we rarely paint 
    // the selection gap before that paragraph, because there often is no gap.  
    // In a case like this, it's not obvious to the user that the selection 
    // ends "inside" that paragraph, so it would be confusing if Indent/Outdent 
    // operated on that paragraph.
    // FIXME: We paint the gap before some paragraphs that are indented with left 
    // margin/padding, but not others.  We should make the gap painting more consistent and 
    // then use a left margin/padding rule here.
    if (visibleEnd != visibleStart && isStartOfParagraph(visibleEnd)) {
        VisibleSelection newSelection(visibleStart, visibleEnd.previous(CannotCrossEditingBoundary), endingSelection().isDirectional());
        if (newSelection.isNone())
            return;
        setEndingSelection(newSelection);
    }

    VisibleSelection selection = selectionForParagraphIteration(endingSelection());

    VisiblePosition startOfSelection = selection.visibleStart();
    if (startOfSelection.isNull())
        return;
    VisiblePosition endOfSelection = selection.visibleEnd();
    if (endOfSelection.isNull())
        return;
    RefPtr<ContainerNode> startScope;
    int startIndex = indexForVisiblePosition(startOfSelection, startScope);
    RefPtr<ContainerNode> endScope;
    int endIndex = indexForVisiblePosition(endOfSelection, endScope);

    formatSelection(startOfSelection, endOfSelection);

    document().updateLayoutIgnorePendingStylesheets();

    ASSERT(startScope == endScope);
    ASSERT(startIndex >= 0);
    ASSERT(startIndex <= endIndex);
    if (startScope == endScope && startIndex >= 0 && startIndex <= endIndex) {
        VisiblePosition start(visiblePositionForIndex(startIndex, startScope.get()));
        VisiblePosition end(visiblePositionForIndex(endIndex, endScope.get()));
        // Work around the fact indexForVisiblePosition can return a larger index due to TextIterator
        // using an extra newline to represent a large margin.
        // FIXME: Add a new TextIteratorBehavior to suppress it.
        if (start.isNotNull() && end.isNull())
            end = lastPositionInNode(endScope.get());
        if (start.isNotNull() && end.isNotNull()) {
            VisibleSelection selection { start, end, endingSelection().isDirectional() };
            // Use canonicalized positions for start & end.
            setEndingSelection(VisibleSelection(selection.start(), selection.end(), selection.isDirectional()));
        }
    }
}

void ApplyBlockElementCommand::formatSelection(const VisiblePosition& startOfSelection, const VisiblePosition& endOfSelection)
{
    // Special case empty unsplittable elements because there's nothing to split
    // and there's nothing to move.
    Position start = startOfSelection.deepEquivalent().downstream();
    if (isAtUnsplittableElement(start) && startOfParagraph(start) == endOfParagraph(endOfSelection)) {
        auto blockquote = createBlockElement();
        insertNodeAt(blockquote.copyRef(), start);
        auto placeholder = HTMLBRElement::create(document());
        appendNode(placeholder.copyRef(), WTFMove(blockquote));
        setEndingSelection(VisibleSelection(positionBeforeNode(placeholder.ptr()), Affinity::Downstream, endingSelection().isDirectional()));
        return;
    }

    RefPtr<Element> blockquoteForNextIndent;
    VisiblePosition endOfCurrentParagraph = endOfParagraph(startOfSelection);
    VisiblePosition endAfterSelection = endOfParagraph(endOfParagraph(endOfSelection).next());
    m_endOfLastParagraph = endOfParagraph(endOfSelection).deepEquivalent();

    bool atEnd = false;
    Position end;
    while (endOfCurrentParagraph != endAfterSelection && !atEnd) {
        if (endOfCurrentParagraph.deepEquivalent() == m_endOfLastParagraph)
            atEnd = true;

        rangeForParagraphSplittingTextNodesIfNeeded(endOfCurrentParagraph, start, end);
        if (start.isNull() || end.isNull())
            break;

        endOfCurrentParagraph = end;

        // FIXME: endOfParagraph can errornously return a position at the beginning of a block element
        // when the position passed into endOfParagraph is at the beginning of a block.
        // Work around this bug here because too much of the existing code depends on the current behavior of endOfParagraph.
        if (start == end && startOfBlock(start) != endOfBlock(start) && !isEndOfBlock(end) && start == startOfParagraph(endOfBlock(start))) {
            endOfCurrentParagraph = endOfBlock(end);
            end = endOfCurrentParagraph.deepEquivalent();
        }

        Position afterEnd = end.next();
        RefPtr enclosingCell = enclosingNodeOfType(start, &isTableCell);
        VisiblePosition endOfNextParagraph = endOfNextParagraphSplittingTextNodesIfNeeded(endOfCurrentParagraph, start, end);

        formatRange(start, end, m_endOfLastParagraph, blockquoteForNextIndent);

        // Don't put the next paragraph in the blockquote we just created for this paragraph unless 
        // the next paragraph is in the same cell.
        if (enclosingCell && enclosingCell != enclosingNodeOfType(endOfNextParagraph.deepEquivalent(), &isTableCell))
            blockquoteForNextIndent = nullptr;

        // indentIntoBlockquote could move more than one paragraph if the paragraph
        // is in a list item or a table. As a result, endAfterSelection could refer to a position
        // no longer in the document.
        if (endAfterSelection.isNotNull() && !endAfterSelection.deepEquivalent().anchorNode()->isConnected())
            break;
        // Sanity check: Make sure our moveParagraph calls didn't remove endOfNextParagraph.deepEquivalent().deprecatedNode()
        // If somehow we did, return to prevent crashes.
        if (endOfNextParagraph.isNotNull() && !endOfNextParagraph.deepEquivalent().anchorNode()->isConnected()) {
            ASSERT_NOT_REACHED();
            return;
        }
        endOfCurrentParagraph = endOfNextParagraph;
    }
}

static bool isNewLineAtPosition(const Position& position)
{
    RefPtr textNode = position.containerNode();
    unsigned offset = position.offsetInContainerNode();
    if (!is<Text>(textNode) || offset >= downcast<Text>(*textNode).length())
        return false;
    return downcast<Text>(*textNode).data()[offset] == '\n';
}

const RenderStyle* ApplyBlockElementCommand::renderStyleOfEnclosingTextNode(const Position& position)
{
    if (position.anchorType() != Position::PositionIsOffsetInAnchor
        || !position.containerNode()
        || !position.containerNode()->isTextNode())
        return nullptr;

    document().updateStyleIfNeeded();

    RenderObject* renderer = position.containerNode()->renderer();
    if (!renderer)
        return nullptr;

    return &renderer->style();
}

void ApplyBlockElementCommand::rangeForParagraphSplittingTextNodesIfNeeded(const VisiblePosition& endOfCurrentParagraph, Position& start, Position& end)
{
    start = startOfParagraph(endOfCurrentParagraph).deepEquivalent();
    end = endOfCurrentParagraph.deepEquivalent();

    bool isStartAndEndOnSameNode = false;
    if (auto* startStyle = renderStyleOfEnclosingTextNode(start)) {
        isStartAndEndOnSameNode = renderStyleOfEnclosingTextNode(end) && start.containerNode() == end.containerNode();
        bool isStartAndEndOfLastParagraphOnSameNode = renderStyleOfEnclosingTextNode(m_endOfLastParagraph) && start.containerNode() == m_endOfLastParagraph.containerNode();
        bool preservesNewLine = startStyle->preserveNewline();
        bool collapsesWhiteSpace = startStyle->collapseWhiteSpace();
        startStyle = nullptr;

        // Avoid obtanining the start of next paragraph for start
        if (preservesNewLine && isNewLineAtPosition(start) && !isNewLineAtPosition(start.previous()) && start.offsetInContainerNode() > 0)
            start = startOfParagraph(end.previous()).deepEquivalent();

        // If start is in the middle of a text node, split.
        if (!collapsesWhiteSpace && start.offsetInContainerNode() > 0) {
            int startOffset = start.offsetInContainerNode();
            RefPtr startText = start.containerText();
            ASSERT(startText);
            splitTextNode(*startText, startOffset);
            start = firstPositionInNode(startText.get());
            if (isStartAndEndOnSameNode) {
                ASSERT(end.offsetInContainerNode() >= startOffset);
                end = Position(startText.get(), end.offsetInContainerNode() - startOffset);
            }
            if (isStartAndEndOfLastParagraphOnSameNode) {
                ASSERT(m_endOfLastParagraph.offsetInContainerNode() >= startOffset);
                m_endOfLastParagraph = Position(startText.get(), m_endOfLastParagraph.offsetInContainerNode() - startOffset);
            }
        }
    }

    if (auto* endStyle = renderStyleOfEnclosingTextNode(end)) {
        bool isEndAndEndOfLastParagraphOnSameNode = renderStyleOfEnclosingTextNode(m_endOfLastParagraph) && end.deprecatedNode() == m_endOfLastParagraph.deprecatedNode();
        // Include \n at the end of line if we're at an empty paragraph
        unsigned endOffset = end.offsetInContainerNode();
        bool preservesNewLine = endStyle->preserveNewline();
        bool collapseWhiteSpace = endStyle->collapseWhiteSpace();
        auto userModify = endStyle->effectiveUserModify();
        endStyle = nullptr;

        if (preservesNewLine && start == end && endOffset < end.containerNode()->length()) {
            if (!isNewLineAtPosition(end.previous()) && isNewLineAtPosition(end))
                end = Position(end.containerText(), ++endOffset);
            if (isEndAndEndOfLastParagraphOnSameNode && end.offsetInContainerNode() >= m_endOfLastParagraph.offsetInContainerNode())
                m_endOfLastParagraph = end;
        }

        // If end is in the middle of a text node and the text node is editable, split.
        if (userModify != UserModify::ReadOnly && !collapseWhiteSpace && endOffset && endOffset < end.containerNode()->length()) {
            RefPtr<Text> endContainer = end.containerText();
            splitTextNode(*endContainer, endOffset);
            if (is<Text>(endContainer) && !endContainer->previousSibling()) {
                start = { };
                end = { };
                return;
            }
            if (isStartAndEndOnSameNode)
                start = firstPositionInOrBeforeNode(endContainer->previousSibling());
            if (isEndAndEndOfLastParagraphOnSameNode) {
                if (static_cast<unsigned>(m_endOfLastParagraph.offsetInContainerNode()) == endOffset)
                    m_endOfLastParagraph = lastPositionInOrAfterNode(endContainer->previousSibling());
                else
                    m_endOfLastParagraph = Position(endContainer.get(), m_endOfLastParagraph.offsetInContainerNode() - endOffset);
            }
            end = lastPositionInNode(endContainer->previousSibling());
        }
    }
}

VisiblePosition ApplyBlockElementCommand::endOfNextParagraphSplittingTextNodesIfNeeded(VisiblePosition& endOfCurrentParagraph, Position& start, Position& end)
{
    VisiblePosition endOfNextParagraph = endOfParagraph(endOfCurrentParagraph.next());
    Position position = endOfNextParagraph.deepEquivalent();

    auto* style = renderStyleOfEnclosingTextNode(position);
    if (!style)
        return endOfNextParagraph;
    bool preserveNewLine = style->preserveNewline();
    style = nullptr;

    RefPtr<Text> text = position.containerText();
    if (!preserveNewLine || !position.offsetInContainerNode() || !isNewLineAtPosition(firstPositionInNode(text.get())))
        return endOfNextParagraph;

    // \n at the beginning of the text node immediately following the current paragraph is trimmed by moveParagraphWithClones.
    // If endOfNextParagraph was pointing at this same text node, endOfNextParagraph will be shifted by one paragraph.
    // Avoid this by splitting "\n"
    splitTextNode(*text, 1);
    auto previousSiblingOfText = RefPtr { text->previousSibling() };

    if (text == start.containerNode() && previousSiblingOfText && is<Text>(previousSiblingOfText)) {
        ASSERT(start.offsetInContainerNode() < position.offsetInContainerNode());
        start = Position(downcast<Text>(text->previousSibling()), start.offsetInContainerNode());
    }
    if (text == end.containerNode() && previousSiblingOfText && is<Text>(previousSiblingOfText)) {
        ASSERT(end.offsetInContainerNode() < position.offsetInContainerNode());
        end = Position(downcast<Text>(text->previousSibling()), end.offsetInContainerNode());
    }
    if (text == m_endOfLastParagraph.containerNode()) {
        if (m_endOfLastParagraph.offsetInContainerNode() < position.offsetInContainerNode()) {
            // We can only fix endOfLastParagraph if the previous node was still text and hasn't been modified by script.
            if (previousSiblingOfText && is<Text>(previousSiblingOfText)
                && static_cast<unsigned>(m_endOfLastParagraph.offsetInContainerNode()) <= downcast<Text>(text->previousSibling())->length())
                m_endOfLastParagraph = Position(downcast<Text>(text->previousSibling()), m_endOfLastParagraph.offsetInContainerNode());
        } else
            m_endOfLastParagraph = Position(text.get(), m_endOfLastParagraph.offsetInContainerNode() - 1);
    }

    return Position(text.get(), position.offsetInContainerNode() - 1);
}

Ref<HTMLElement> ApplyBlockElementCommand::createBlockElement()
{
    auto element = createHTMLElement(document(), m_tagName);
    if (m_inlineStyle.length())
        element->setAttribute(styleAttr, m_inlineStyle);
    return element;
}

}
