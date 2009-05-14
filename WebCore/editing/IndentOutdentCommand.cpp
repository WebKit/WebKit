/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (IndentOutdentCommandINCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "IndentOutdentCommand.h"

#include "Document.h"
#include "Element.h"
#include "HTMLBlockquoteElement.h"
#include "HTMLNames.h"
#include "InsertLineBreakCommand.h"
#include "InsertListCommand.h"
#include "Range.h"
#include "SplitElementCommand.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

static String indentBlockquoteString()
{
    DEFINE_STATIC_LOCAL(String, string, ("webkit-indent-blockquote"));
    return string;
}

static PassRefPtr<HTMLBlockquoteElement> createIndentBlockquoteElement(Document* document)
{
    RefPtr<HTMLBlockquoteElement> element = new HTMLBlockquoteElement(blockquoteTag, document);
    element->setAttribute(classAttr, indentBlockquoteString());
    element->setAttribute(styleAttr, "margin: 0 0 0 40px; border: none; padding: 0px;");
    return element.release();
}

static bool isIndentBlockquote(const Node* node)
{
    if (!node || !node->hasTagName(blockquoteTag) || !node->isElementNode())
        return false;

    const Element* elem = static_cast<const Element*>(node);
    return elem->getAttribute(classAttr) == indentBlockquoteString();
}

static bool isListOrIndentBlockquote(const Node* node)
{
    return node && (node->hasTagName(ulTag) || node->hasTagName(olTag) || isIndentBlockquote(node));
}

IndentOutdentCommand::IndentOutdentCommand(Document* document, EIndentType typeOfAction, int marginInPixels)
    : CompositeEditCommand(document), m_typeOfAction(typeOfAction), m_marginInPixels(marginInPixels)
{
}

// This function is a workaround for moveParagraph's tendency to strip blockquotes. It updates lastBlockquote to point to the
// correct level for the current paragraph, and returns a pointer to a placeholder br where the insertion should be performed.
PassRefPtr<Element> IndentOutdentCommand::prepareBlockquoteLevelForInsertion(VisiblePosition& currentParagraph, RefPtr<Element>& lastBlockquote)
{
    int currentBlockquoteLevel = 0;
    int lastBlockquoteLevel = 0;
    Node* node = currentParagraph.deepEquivalent().node();
    while ((node = enclosingNodeOfType(Position(node->parentNode(), 0), &isIndentBlockquote)))
        currentBlockquoteLevel++;
    node = lastBlockquote.get();
    while ((node = enclosingNodeOfType(Position(node->parentNode(), 0), &isIndentBlockquote)))
        lastBlockquoteLevel++;
    while (currentBlockquoteLevel > lastBlockquoteLevel) {
        RefPtr<Element> newBlockquote = createIndentBlockquoteElement(document());
        appendNode(newBlockquote, lastBlockquote);
        lastBlockquote = newBlockquote;
        lastBlockquoteLevel++;
    }
    while (currentBlockquoteLevel < lastBlockquoteLevel) {
        lastBlockquote = static_cast<Element*>(enclosingNodeOfType(Position(lastBlockquote->parentNode(), 0), isIndentBlockquote));
        lastBlockquoteLevel--;
    }
    RefPtr<Element> placeholder = createBreakElement(document());
    appendNode(placeholder, lastBlockquote);
    // Add another br before the placeholder if it collapsed.
    VisiblePosition visiblePos(Position(placeholder.get(), 0));
    if (!isStartOfParagraph(visiblePos))
        insertNodeBefore(createBreakElement(document()), placeholder);
    return placeholder.release();
}

void IndentOutdentCommand::indentRegion()
{
    VisibleSelection selection = selectionForParagraphIteration(endingSelection());
    VisiblePosition startOfSelection = selection.visibleStart();
    VisiblePosition endOfSelection = selection.visibleEnd();
    int startIndex = indexForVisiblePosition(startOfSelection);
    int endIndex = indexForVisiblePosition(endOfSelection);

    ASSERT(!startOfSelection.isNull());
    ASSERT(!endOfSelection.isNull());
    
    // Special case empty root editable elements because there's nothing to split
    // and there's nothing to move.
    Position start = startOfSelection.deepEquivalent().downstream();
    if (start.node() == editableRootForPosition(start)) {
        RefPtr<Element> blockquote = createIndentBlockquoteElement(document());
        insertNodeAt(blockquote, start);
        RefPtr<Element> placeholder = createBreakElement(document());
        appendNode(placeholder, blockquote);
        setEndingSelection(VisibleSelection(Position(placeholder.get(), 0), DOWNSTREAM));
        return;
    }
    
    RefPtr<Element> previousListNode;
    RefPtr<Element> newListNode;
    RefPtr<Element> newBlockquote;
    VisiblePosition endOfCurrentParagraph = endOfParagraph(startOfSelection);
    VisiblePosition endAfterSelection = endOfParagraph(endOfParagraph(endOfSelection).next());
    while (endOfCurrentParagraph != endAfterSelection) {
        // Iterate across the selected paragraphs...
        VisiblePosition endOfNextParagraph = endOfParagraph(endOfCurrentParagraph.next());
        RefPtr<Element> listNode = enclosingList(endOfCurrentParagraph.deepEquivalent().node());
        RefPtr<Element> insertionPoint;
        if (listNode) {
            RefPtr<Element> placeholder = createBreakElement(document());
            insertionPoint = placeholder;
            newBlockquote = 0;
            RefPtr<Element> listItem = createListItemElement(document());
            if (listNode == previousListNode) {
                // The previous paragraph was inside the same list, so add this list item to the list we already created
                appendNode(listItem, newListNode);
                appendNode(placeholder, listItem);
            } else {
                // Clone the list element, insert it before the current paragraph, and move the paragraph into it.
                RefPtr<Element> clonedList = listNode->cloneElementWithoutChildren();
                insertNodeBefore(clonedList, enclosingListChild(endOfCurrentParagraph.deepEquivalent().node()));
                appendNode(listItem, clonedList);
                appendNode(placeholder, listItem);
                newListNode = clonedList;
                previousListNode = listNode;
            }
        } else if (newBlockquote)
            // The previous paragraph was put into a new blockquote, so move this paragraph there as well
            insertionPoint = prepareBlockquoteLevelForInsertion(endOfCurrentParagraph, newBlockquote);
        else {
            // Create a new blockquote and insert it as a child of the root editable element. We accomplish
            // this by splitting all parents of the current paragraph up to that point.
            RefPtr<Element> blockquote = createIndentBlockquoteElement(document());
            Position start = startOfParagraph(endOfCurrentParagraph).deepEquivalent();
            
            Node* enclosingCell = enclosingNodeOfType(start, &isTableCell);
            Node* nodeToSplitTo = enclosingCell ? enclosingCell : editableRootForPosition(start);
            RefPtr<Node> startOfNewBlock = splitTreeToNode(start.node(), nodeToSplitTo);
            insertNodeBefore(blockquote, startOfNewBlock);
            newBlockquote = blockquote;
            insertionPoint = prepareBlockquoteLevelForInsertion(endOfCurrentParagraph, newBlockquote);
            // Don't put the next paragraph in the blockquote we just created for this paragraph unless 
            // the next paragraph is in the same cell.
            if (enclosingCell && enclosingCell != enclosingNodeOfType(endOfNextParagraph.deepEquivalent(), &isTableCell))
                newBlockquote = 0;
        }
        moveParagraph(startOfParagraph(endOfCurrentParagraph), endOfCurrentParagraph, VisiblePosition(Position(insertionPoint, 0)), true);
        // moveParagraph should not destroy content that contains endOfNextParagraph, but if it does, return here
        // to avoid a crash.
        if (endOfNextParagraph.isNotNull() && !endOfNextParagraph.deepEquivalent().node()->inDocument()) {
            ASSERT_NOT_REACHED();
            return;
        }
        endOfCurrentParagraph = endOfNextParagraph;
    }
    
    RefPtr<Range> startRange = TextIterator::rangeFromLocationAndLength(document()->documentElement(), startIndex, 0, true);
    RefPtr<Range> endRange = TextIterator::rangeFromLocationAndLength(document()->documentElement(), endIndex, 0, true);
    if (startRange && endRange)
        setEndingSelection(VisibleSelection(startRange->startPosition(), endRange->startPosition(), DOWNSTREAM));
}

void IndentOutdentCommand::outdentParagraph()
{
    VisiblePosition visibleStartOfParagraph = startOfParagraph(endingSelection().visibleStart());
    VisiblePosition visibleEndOfParagraph = endOfParagraph(visibleStartOfParagraph);

    Node* enclosingNode = enclosingNodeOfType(visibleStartOfParagraph.deepEquivalent(), &isListOrIndentBlockquote);
    if (!enclosingNode || !isContentEditable(enclosingNode->parentNode()))  // We can't outdent if there is no place to go!
        return;

    // Use InsertListCommand to remove the selection from the list
    if (enclosingNode->hasTagName(olTag)) {
        applyCommandToComposite(InsertListCommand::create(document(), InsertListCommand::OrderedList));
        return;        
    }
    if (enclosingNode->hasTagName(ulTag)) {
        applyCommandToComposite(InsertListCommand::create(document(), InsertListCommand::UnorderedList));
        return;
    }
    
    // The selection is inside a blockquote
    VisiblePosition positionInEnclosingBlock = VisiblePosition(Position(enclosingNode, 0));
    VisiblePosition startOfEnclosingBlock = startOfBlock(positionInEnclosingBlock);
    VisiblePosition lastPositionInEnclosingBlock = VisiblePosition(Position(enclosingNode, enclosingNode->childNodeCount()));
    VisiblePosition endOfEnclosingBlock = endOfBlock(lastPositionInEnclosingBlock);
    if (visibleStartOfParagraph == startOfEnclosingBlock &&
        visibleEndOfParagraph == endOfEnclosingBlock) {
        // The blockquote doesn't contain anything outside the paragraph, so it can be totally removed.
        Node* splitPoint = enclosingNode->nextSibling();
        removeNodePreservingChildren(enclosingNode);
        // outdentRegion() assumes it is operating on the first paragraph of an enclosing blockquote, but if there are multiply nested blockquotes and we've
        // just removed one, then this assumption isn't true. By splitting the next containing blockquote after this node, we keep this assumption true
        if (splitPoint) {
            if (Node* splitPointParent = splitPoint->parentNode()) {
                if (isIndentBlockquote(splitPointParent)
                    && !isIndentBlockquote(splitPoint)
                    && isContentEditable(splitPointParent->parentNode())) // We can't outdent if there is no place to go!
                    splitElement(static_cast<Element*>(splitPointParent), splitPoint);
            }
        }
        
        updateLayout();
        visibleStartOfParagraph = VisiblePosition(visibleStartOfParagraph.deepEquivalent());
        visibleEndOfParagraph = VisiblePosition(visibleEndOfParagraph.deepEquivalent());
        if (visibleStartOfParagraph.isNotNull() && !isStartOfParagraph(visibleStartOfParagraph))
            insertNodeAt(createBreakElement(document()), visibleStartOfParagraph.deepEquivalent());
        if (visibleEndOfParagraph.isNotNull() && !isEndOfParagraph(visibleEndOfParagraph))
            insertNodeAt(createBreakElement(document()), visibleEndOfParagraph.deepEquivalent());

        return;
    }
    Node* enclosingBlockFlow = enclosingBlockFlowElement(visibleStartOfParagraph);
    RefPtr<Node> splitBlockquoteNode = enclosingNode;
    if (enclosingBlockFlow != enclosingNode)
        splitBlockquoteNode = splitTreeToNode(enclosingBlockFlowElement(visibleStartOfParagraph), enclosingNode, true);
    RefPtr<Node> placeholder = createBreakElement(document());
    insertNodeBefore(placeholder, splitBlockquoteNode);
    moveParagraph(startOfParagraph(visibleStartOfParagraph), endOfParagraph(visibleEndOfParagraph), VisiblePosition(Position(placeholder.get(), 0)), true);
}

void IndentOutdentCommand::outdentRegion()
{
    VisiblePosition startOfSelection = endingSelection().visibleStart();
    VisiblePosition endOfSelection = endingSelection().visibleEnd();
    VisiblePosition endOfLastParagraph = endOfParagraph(endOfSelection);

    ASSERT(!startOfSelection.isNull());
    ASSERT(!endOfSelection.isNull());

    if (endOfParagraph(startOfSelection) == endOfLastParagraph) {
        outdentParagraph();
        return;
    }

    Position originalSelectionEnd = endingSelection().end();
    setEndingSelection(endingSelection().visibleStart());
    outdentParagraph();
    Position originalSelectionStart = endingSelection().start();
    VisiblePosition endOfCurrentParagraph = endOfParagraph(endOfParagraph(endingSelection().visibleStart()).next(true));
    VisiblePosition endAfterSelection = endOfParagraph(endOfParagraph(endOfSelection).next());
    while (endOfCurrentParagraph != endAfterSelection) {
        VisiblePosition endOfNextParagraph = endOfParagraph(endOfCurrentParagraph.next());
        if (endOfCurrentParagraph == endOfLastParagraph)
            setEndingSelection(VisibleSelection(originalSelectionEnd, DOWNSTREAM));
        else
            setEndingSelection(endOfCurrentParagraph);
        outdentParagraph();
        endOfCurrentParagraph = endOfNextParagraph;
    }
    setEndingSelection(VisibleSelection(originalSelectionStart, endingSelection().end(), DOWNSTREAM));
}

void IndentOutdentCommand::doApply()
{
    if (endingSelection().isNone())
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

    if (m_typeOfAction == Indent)
        indentRegion();
    else
        outdentRegion();
}

}
