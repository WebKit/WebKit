/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Element.h"
#include "IndentOutdentCommand.h"
#include "InsertListCommand.h"
#include "Document.h"
#include "htmlediting.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InsertLineBreakCommand.h"
#include "Range.h"
#include "SplitElementCommand.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

IndentOutdentCommand::IndentOutdentCommand(Document* document, EIndentType typeOfAction, int marginInPixels)
    : CompositeEditCommand(document), m_typeOfAction(typeOfAction), m_marginInPixels(marginInPixels)
{}

static Node* enclosingListOrBlockquote(Node* node)
{
    if (!node)
        return 0;
    Node* root = (node->inDocument()) ? node->rootEditableElement() : highestAncestor(node);
    ASSERT(root);
    for (Node* n = node->parentNode(); n && (n == root || n->isDescendantOf(root)); n = n->parentNode())
        if (n->hasTagName(ulTag) || n->hasTagName(olTag) || n->hasTagName(blockquoteTag))
            return n;
            
    return 0;
}

// This function is a workaround for moveParagraph's tendency to strip blockquotes. It updates lastBlockquote to point to the
// correct level for the current paragraph, and returns a pointer to a placeholder br where the insertion should be performed.
Node* IndentOutdentCommand::prepareBlockquoteLevelForInsertion(VisiblePosition& currentParagraph, Node** lastBlockquote)
{
    int currentBlockquoteLevel = 0;
    int lastBlockquoteLevel = 0;
    Node* node = currentParagraph.deepEquivalent().node();
    while ((node = enclosingNodeWithTag(node, blockquoteTag)))
        currentBlockquoteLevel++;
    node = *lastBlockquote;
    while ((node = enclosingNodeWithTag(node, blockquoteTag)))
        lastBlockquoteLevel++;
    while (currentBlockquoteLevel > lastBlockquoteLevel) {
        RefPtr<Node> newBlockquote = createElement(document(), "blockquote");
        appendNode(newBlockquote.get(), *lastBlockquote);
        *lastBlockquote = newBlockquote.get();
        lastBlockquoteLevel++;
    }
    while (currentBlockquoteLevel < lastBlockquoteLevel) {
        *lastBlockquote = enclosingNodeWithTag(*lastBlockquote, blockquoteTag);
        lastBlockquoteLevel--;
    }
    RefPtr<Node> placeholder = createBreakElement(document());
    if ((*lastBlockquote)->firstChild() && !(*lastBlockquote)->lastChild()->hasTagName(brTag)) {
        RefPtr<Node> collapsedPlaceholder = createBreakElement(document());
        appendNode(collapsedPlaceholder.get(), (*lastBlockquote));
    }
    appendNode(placeholder.get(), *lastBlockquote);
    return placeholder.get();
}

// Splits the tree parent by parent until we reach the specified ancestor. We use VisiblePositions
// to determine if the split is necessary. Returns the last split node.
Node* IndentOutdentCommand::splitTreeToNode(Node* start, Node* end, bool splitAncestor)
{
    Node* node;
    for (node = start; node && node->parent() != end; node = node->parent()) {
        VisiblePosition positionInParent(Position(node->parent(), 0), DOWNSTREAM);
        VisiblePosition positionInNode(Position(node, 0), DOWNSTREAM);
        if (positionInParent != positionInNode)
            applyCommandToComposite(new SplitElementCommand(static_cast<Element*>(node->parent()), node));
    }
    if (splitAncestor)
        return splitTreeToNode(end, end->parent());
    return node;
}

void IndentOutdentCommand::indentRegion()
{
    VisiblePosition startOfSelection = endingSelection().visibleStart();
    VisiblePosition endOfSelection = endingSelection().visibleEnd();

    ASSERT(!startOfSelection.isNull());
    ASSERT(!endOfSelection.isNull());
    
    // Special case empty root editable elements because there's nothing to split
    // and there's nothing to move.
    Node* startNode = startOfSelection.deepEquivalent().downstream().node();
    if (startNode == startNode->rootEditableElement()) {
        RefPtr<Node> blockquote = createElement(document(), "blockquote");
        insertNodeAt(blockquote.get(), startNode, 0);
        RefPtr<Node> placeholder = createBreakElement(document());
        appendNode(placeholder.get(), blockquote.get());
        setEndingSelection(Selection(Position(placeholder.get(), 0), DOWNSTREAM));
        return;
    }
    
    Node* previousListNode = 0;
    Node* newListNode = 0;
    Node* newBlockquote = 0;
    VisiblePosition endOfCurrentParagraph = endOfParagraph(startOfSelection);
    VisiblePosition endAfterSelection = endOfParagraph(endOfParagraph(endOfSelection).next());
    while (endOfCurrentParagraph != endAfterSelection) {
        // Iterate across the selected paragraphs...
        VisiblePosition endOfNextParagraph = endOfParagraph(endOfCurrentParagraph.next());
        Node* listNode = enclosingList(endOfCurrentParagraph.deepEquivalent().node());
        Node* insertionPoint;
        if (listNode) {
            RefPtr<Node> placeholder = createBreakElement(document());
            insertionPoint = placeholder.get();
            newBlockquote = 0;
            RefPtr<Node> listItem = createListItemElement(document());
            if (listNode == previousListNode) {
                // The previous paragraph was inside the same list, so add this list item to the list we already created
                appendNode(listItem.get(), newListNode);
                appendNode(placeholder.get(), listItem.get());
            } else {
                // Clone the list element, insert it before the current paragraph, and move the paragraph into it.
                RefPtr<Node> clonedList = static_cast<Element*>(listNode)->cloneNode(false);
                insertNodeBefore(clonedList.get(), enclosingListChild(endOfCurrentParagraph.deepEquivalent().node()));
                appendNode(listItem.get(), clonedList.get());
                appendNode(placeholder.get(), listItem.get());
                newListNode = clonedList.get();
                previousListNode = listNode;
            }
        } else if (newBlockquote)
            // The previous paragraph was put into a new blockquote, so move this paragraph there as well
            insertionPoint = prepareBlockquoteLevelForInsertion(endOfCurrentParagraph, &newBlockquote);
        else {
            // Create a new blockquote and insert it as a child of the root editable element. We accomplish
            // this by splitting all parents of the current paragraph up to that point.
            RefPtr<Node> blockquote = createElement(document(), "blockquote");
            Node* startNode = startOfParagraph(endOfCurrentParagraph).deepEquivalent().node();
            Node* startOfNewBlock = splitTreeToNode(startNode, startNode->rootEditableElement());
            insertNodeBefore(blockquote.get(), startOfNewBlock);
            newBlockquote = blockquote.get();
            insertionPoint = prepareBlockquoteLevelForInsertion(endOfCurrentParagraph, &newBlockquote);
        }
        moveParagraph(startOfParagraph(endOfCurrentParagraph), endOfCurrentParagraph, VisiblePosition(Position(insertionPoint, 0)), true);
        endOfCurrentParagraph = endOfNextParagraph;
    }
}

void IndentOutdentCommand::outdentParagraph()
{
    VisiblePosition visibleStartOfParagraph = startOfParagraph(endingSelection().visibleStart());
    VisiblePosition visibleEndOfParagraph = endOfParagraph(visibleStartOfParagraph);

    Node* enclosingNode = enclosingListOrBlockquote(visibleStartOfParagraph.deepEquivalent().node());
    if (!enclosingNode)
        return;

    // Handle the list case
    bool inList = false;
    InsertListCommand::Type typeOfList;
    if (enclosingNode->hasTagName(olTag)) {
        inList = true;
        typeOfList = InsertListCommand::OrderedList;
    } else if (enclosingNode->hasTagName(ulTag)) {
        inList = true;
        typeOfList = InsertListCommand::UnorderedList;
    }
    if (inList) {
        // Use InsertListCommand to remove the selection from the list
        applyCommandToComposite(new InsertListCommand(document(), typeOfList, ""));
        return;
    }
    // The selection is inside a blockquote
    VisiblePosition positionInEnclosingBlock = VisiblePosition(Position(enclosingNode, 0));
    VisiblePosition startOfEnclosingBlock = startOfBlock(positionInEnclosingBlock);
    VisiblePosition endOfEnclosingBlock = endOfBlock(positionInEnclosingBlock);
    if (visibleStartOfParagraph == startOfEnclosingBlock &&
        visibleEndOfParagraph == endOfEnclosingBlock) {
        // The blockquote doesn't contain anything outside the paragraph, so it can be totally removed.
        removeNodePreservingChildren(enclosingNode);
        return;
    }
    Node* enclosingBlockFlow = enclosingBlockFlowElement(visibleStartOfParagraph);
    Node* splitBlockquoteNode = enclosingNode;
    if (enclosingBlockFlow != enclosingNode)
        splitBlockquoteNode = splitTreeToNode(enclosingBlockFlowElement(visibleStartOfParagraph), enclosingNode, true);
    RefPtr<Node> placeholder = createBreakElement(document());
    insertNodeBefore(placeholder.get(), splitBlockquoteNode);
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
            setEndingSelection(Selection(originalSelectionEnd, DOWNSTREAM));
        else
            setEndingSelection(endOfCurrentParagraph);
        outdentParagraph();
        endOfCurrentParagraph = endOfNextParagraph;
    }
    setEndingSelection(Selection(originalSelectionStart, endingSelection().end(), DOWNSTREAM));
}

void IndentOutdentCommand::doApply()
{
    if (endingSelection().isNone())
        return;

    if (!endingSelection().rootEditableElement())
        return;

    if (m_typeOfAction == Indent)
        indentRegion();
    else
        outdentRegion();
}

}
