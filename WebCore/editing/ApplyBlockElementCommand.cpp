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

#include "HTMLElement.h"
#include "HTMLNames.h"
#include "Text.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include "htmlediting.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;
    
// This function can return -1 if we are unable to count the paragraphs between |start| and |end|.
static int countParagraphs(const VisiblePosition& endOfFirstParagraph, const VisiblePosition& endOfLastParagraph)
{
    int count = 0;
    VisiblePosition cur = endOfFirstParagraph;
    while (cur != endOfLastParagraph) {
        ++count;
        cur = endOfParagraph(cur.next());
        // If start is before a table and end is inside a table, we will never hit end because the
        // whole table is considered a single paragraph.
        if (cur.isNull())
            return -1;
    }
    return count;
}

ApplyBlockElementCommand::ApplyBlockElementCommand(Document* document, const QualifiedName& tagName, const AtomicString& className, const AtomicString& inlineStyle)
    : CompositeEditCommand(document)
    , m_tagName(tagName)
    , m_className(className)
    , m_inlineStyle(inlineStyle)
{
}

ApplyBlockElementCommand::ApplyBlockElementCommand(Document* document, const QualifiedName& tagName)
    : CompositeEditCommand(document)
    , m_tagName(tagName)
{
}

void ApplyBlockElementCommand::doApply()
{
    if (!endingSelection().isNonOrphanedCaretOrRange())
        return;

    if (!endingSelection().rootEditableElement())
        return;

    VisiblePosition visibleEnd = endingSelection().visibleEnd();
    VisiblePosition visibleStart = endingSelection().visibleStart();
    // When a selection ends at the start of a paragraph, we rarely paint 
    // the selection gap before that paragraph, because there often is no gap.  
    // In a case like this, it's not obvious to the user that the selection 
    // ends "inside" that paragraph, so it would be confusing if Indent/Outdent 
    // operated on that paragraph.
    // FIXME: We paint the gap before some paragraphs that are indented with left 
    // margin/padding, but not others.  We should make the gap painting more consistent and 
    // then use a left margin/padding rule here.
    if (visibleEnd != visibleStart && isStartOfParagraph(visibleEnd))
        setEndingSelection(VisibleSelection(visibleStart, visibleEnd.previous(true)));

    VisibleSelection selection = selectionForParagraphIteration(endingSelection());
    VisiblePosition startOfSelection = selection.visibleStart();
    VisiblePosition endOfSelection = selection.visibleEnd();
    ASSERT(!startOfSelection.isNull());
    ASSERT(!endOfSelection.isNull());
    int startIndex = indexForVisiblePosition(startOfSelection);
    int endIndex = indexForVisiblePosition(endOfSelection);

    formatSelection(startOfSelection, endOfSelection);

    updateLayout();

    RefPtr<Range> startRange = TextIterator::rangeFromLocationAndLength(document()->documentElement(), startIndex, 0, true);
    RefPtr<Range> endRange = TextIterator::rangeFromLocationAndLength(document()->documentElement(), endIndex, 0, true);
    if (startRange && endRange)
        setEndingSelection(VisibleSelection(startRange->startPosition(), endRange->startPosition(), DOWNSTREAM));
}

void ApplyBlockElementCommand::formatSelection(const VisiblePosition& startOfSelection, const VisiblePosition& endOfSelection)
{
    // Special case empty unsplittable elements because there's nothing to split
    // and there's nothing to move.
    Position start = startOfSelection.deepEquivalent().downstream();
    if (isAtUnsplittableElement(start)) {
        RefPtr<Element> blockquote = createBlockElement();
        insertNodeAt(blockquote, start);
        RefPtr<Element> placeholder = createBreakElement(document());
        appendNode(placeholder, blockquote);
        setEndingSelection(VisibleSelection(Position(placeholder.get(), 0), DOWNSTREAM));
        return;
    }

    RefPtr<Element> blockquoteForNextIndent;
    VisiblePosition endOfCurrentParagraph = endOfParagraph(startOfSelection);
    VisiblePosition endAfterSelection = endOfParagraph(endOfParagraph(endOfSelection).next());
    int endOfCurrentParagraphIndex = indexForVisiblePosition(endOfCurrentParagraph);
    int endAfterSelectionIndex = indexForVisiblePosition(endAfterSelection);

    // When indenting within a <pre> tag, we need to split each paragraph into a separate node for moveParagraphWithClones to work.
    // However, splitting text nodes can cause endOfCurrentParagraph and endAfterSelection to point to an invalid position if we
    // changed the text node it was pointing at.  So we have to reset these positions.
    int numParagraphs = countParagraphs(endOfCurrentParagraph, endAfterSelection);
    if (splitTextNodes(startOfParagraph(startOfSelection), numParagraphs + 1)) {
        RefPtr<Range> endOfCurrentParagraphRange = TextIterator::rangeFromLocationAndLength(document()->documentElement(), endOfCurrentParagraphIndex, 0, true);
        RefPtr<Range> endAfterSelectionRange = TextIterator::rangeFromLocationAndLength(document()->documentElement(), endAfterSelectionIndex, 0, true);
        if (!endOfCurrentParagraphRange.get() || !endAfterSelectionRange.get()) {
            ASSERT_NOT_REACHED();
            return;
        }
        endOfCurrentParagraph = VisiblePosition(endOfCurrentParagraphRange->startPosition(), DOWNSTREAM);
        endAfterSelection = VisiblePosition(endAfterSelectionRange->startPosition(), DOWNSTREAM);
    }

    while (endOfCurrentParagraph != endAfterSelection) {
        // Iterate across the selected paragraphs...
        VisiblePosition endOfNextParagraph = endOfParagraph(endOfCurrentParagraph.next());
        Node* enclosingCell = enclosingNodeOfType(start, &isTableCell);

        formatParagraph(endOfCurrentParagraph, blockquoteForNextIndent);

        // Don't put the next paragraph in the blockquote we just created for this paragraph unless 
        // the next paragraph is in the same cell.
        if (enclosingCell && enclosingCell != enclosingNodeOfType(endOfNextParagraph.deepEquivalent(), &isTableCell))
            blockquoteForNextIndent = 0;

        // indentIntoBlockquote could move more than one paragraph if the paragraph
        // is in a list item or a table. As a result, endAfterSelection could refer to a position
        // no longer in the document.
        if (endAfterSelection.isNotNull() && !endAfterSelection.deepEquivalent().node()->inDocument())
            break;
        // Sanity check: Make sure our moveParagraph calls didn't remove endOfNextParagraph.deepEquivalent().node()
        // If somehow we did, return to prevent crashes.
        if (endOfNextParagraph.isNotNull() && !endOfNextParagraph.deepEquivalent().node()->inDocument()) {
            ASSERT_NOT_REACHED();
            return;
        }
        endOfCurrentParagraph = endOfNextParagraph;
    }
}

PassRefPtr<Element> ApplyBlockElementCommand::createBlockElement() const
{
    RefPtr<Element> element = createHTMLElement(document(), m_tagName);
    if (m_className.length())
        element->setAttribute(classAttr, m_className);
    if (m_inlineStyle.length())
        element->setAttribute(styleAttr, m_inlineStyle);
    return element.release();
}

// Returns true if at least one text node was split.
bool ApplyBlockElementCommand::splitTextNodes(const VisiblePosition& start, int numParagraphs)
{
    VisiblePosition currentParagraphStart = start;
    bool hasSplit = false;
    int paragraphCount;
    for (paragraphCount = 0; paragraphCount < numParagraphs; ++paragraphCount) {
        // If there are multiple paragraphs in a single text node, we split the text node into a separate node for each paragraph.
        if (currentParagraphStart.deepEquivalent().node()->isTextNode() && currentParagraphStart.deepEquivalent().node() == startOfParagraph(currentParagraphStart.previous()).deepEquivalent().node()) {
            Text* textNode = static_cast<Text *>(currentParagraphStart.deepEquivalent().node());
            int offset = currentParagraphStart.deepEquivalent().offsetInContainerNode();
            splitTextNode(textNode, offset);
            currentParagraphStart = VisiblePosition(textNode, 0, VP_DEFAULT_AFFINITY);
            hasSplit = true;
        }
        VisiblePosition nextParagraph = startOfParagraph(endOfParagraph(currentParagraphStart).next());
        if (nextParagraph.isNull())
            break;
        currentParagraphStart = nextParagraph;
    }
    return hasSplit;
}

}
