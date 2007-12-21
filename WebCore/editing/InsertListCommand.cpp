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
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Element.h"
#include "InsertListCommand.h"
#include "DocumentFragment.h"
#include "htmlediting.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "TextIterator.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<Node> InsertListCommand::insertList(Document* document, Type type)
{
    RefPtr<InsertListCommand> insertCommand = new InsertListCommand(document, type, "");
    insertCommand->apply();
    return insertCommand->m_listElement;
}

Node* InsertListCommand::fixOrphanedListChild(Node* node)
{
    RefPtr<Element> listElement = createUnorderedListElement(document());
    insertNodeBefore(listElement.get(), node);
    removeNode(node);
    appendNode(node, listElement.get());
    m_listElement = listElement;
    return listElement.get();
}

InsertListCommand::InsertListCommand(Document* document, Type type, const String& id) 
    : CompositeEditCommand(document), m_type(type), m_id(id), m_forceCreateList(false)
{
}

bool InsertListCommand::modifyRange()
{
    Selection selection = selectionForParagraphIteration(endingSelection());
    ASSERT(selection.isRange());
    VisiblePosition startOfSelection = selection.visibleStart();
    VisiblePosition endOfSelection = selection.visibleEnd();
    VisiblePosition startOfLastParagraph = startOfParagraph(endOfSelection);
    
    if (startOfParagraph(startOfSelection) == startOfLastParagraph)
        return false;

    Node* startList = enclosingList(startOfSelection.deepEquivalent().node());
    Node* endList = enclosingList(endOfSelection.deepEquivalent().node());
    if (!startList || startList != endList)
        m_forceCreateList = true;

    setEndingSelection(startOfSelection);
    doApply();
    // Fetch the start of the selection after moving the first paragraph,
    // because moving the paragraph will invalidate the original start.  
    // We'll use the new start to restore the original selection after 
    // we modified all selected paragraphs.
    startOfSelection = endingSelection().visibleStart();
    VisiblePosition startOfCurrentParagraph = startOfNextParagraph(startOfSelection);
    while (startOfCurrentParagraph != startOfLastParagraph) {
        setEndingSelection(startOfCurrentParagraph);
        doApply();
        startOfCurrentParagraph = startOfNextParagraph(endingSelection().visibleStart());
    }
    setEndingSelection(endOfSelection);
    doApply();
    // Fetch the end of the selection, for the reason mentioned above.
    endOfSelection = endingSelection().visibleEnd();
    setEndingSelection(Selection(startOfSelection, endOfSelection));
    m_forceCreateList = false;
    return true;
}

void InsertListCommand::doApply()
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
    // ends "inside" that paragraph, so it would be confusing if InsertUn{Ordered}List 
    // operated on that paragraph.
    // FIXME: We paint the gap before some paragraphs that are indented with left 
    // margin/padding, but not others.  We should make the gap painting more consistent and 
    // then use a left margin/padding rule here.
    if (visibleEnd != visibleStart && isStartOfParagraph(visibleEnd))
        setEndingSelection(Selection(visibleStart, visibleEnd.previous(true)));

    if (endingSelection().isRange() && modifyRange())
        return;
    
    // FIXME: This will produce unexpected results for a selection that starts just before a
    // table and ends inside the first cell, selectionForParagraphIteration should probably
    // be renamed and deployed inside setEndingSelection().
    Node* selectionNode = endingSelection().start().node();
    const QualifiedName listTag = (m_type == OrderedList) ? olTag : ulTag;
    Node* listChildNode = enclosingListChild(selectionNode);
    bool switchListType = false;
    if (listChildNode) {
        // Remove the list chlild.
        Node* listNode = enclosingList(listChildNode);
        if (!listNode)
            listNode = fixOrphanedListChild(listChildNode);
        if (!listNode->hasTagName(listTag))
            // listChildNode will be removed from the list and a list of type m_type will be created.
            switchListType = true;
        Node* nextListChild;
        Node* previousListChild;
        VisiblePosition start;
        VisiblePosition end;
        if (listChildNode->hasTagName(liTag)) {
            start = VisiblePosition(Position(listChildNode, 0));
            end = VisiblePosition(Position(listChildNode, maxDeepOffset(listChildNode)));
            nextListChild = listChildNode->nextSibling();
            previousListChild = listChildNode->previousSibling();
        } else {
            // A paragraph is visually a list item minus a list marker.  The paragraph will be moved.
            start = startOfParagraph(endingSelection().visibleStart());
            end = endOfParagraph(endingSelection().visibleEnd());
            nextListChild = enclosingListChild(end.next().deepEquivalent().node());
            ASSERT(nextListChild != listChildNode);
            if (enclosingList(nextListChild) != listNode)
                nextListChild = 0;
            previousListChild = enclosingListChild(start.previous().deepEquivalent().node());
            ASSERT(previousListChild != listChildNode);
            if (enclosingList(previousListChild) != listNode)
                previousListChild = 0;
        }
        // When removing a list, we must always create a placeholder to act as a point of insertion
        // for the list content being removed.
        RefPtr<Element> placeholder = createBreakElement(document());
        RefPtr<Node> nodeToInsert = placeholder;
        // If the content of the list item will be moved into another list, put it in a list item
        // so that we don't create an orphaned list child.
        if (enclosingList(listNode)) {
            nodeToInsert = createListItemElement(document());
            appendNode(placeholder.get(), nodeToInsert.get());
        }
        
        if (nextListChild && previousListChild) {
            // We want to pull listChildNode out of listNode, and place it before nextListChild 
            // and after previousListChild, so we split listNode and insert it between the two lists.  
            // But to split listNode, we must first split ancestors of listChildNode between it and listNode,
            // if any exist.
            // FIXME: We appear to split at nextListChild as opposed to listChildNode so that when we remove
            // listChildNode below in moveParagraphs, previousListChild will be removed along with it if it is 
            // unrendered. But we ought to remove nextListChild too, if it is unrendered.
            splitElement(static_cast<Element *>(listNode), splitTreeToNode(nextListChild, listNode));
            insertNodeBefore(nodeToInsert.get(), listNode);
        } else if (nextListChild || listChildNode->parentNode() != listNode) {
            // Just because listChildNode has no previousListChild doesn't mean there isn't any content
            // in listNode that comes before listChildNode, as listChildNode could have ancestors
            // between it and listNode. So, we split up to listNode before inserting the placeholder
            // where we're about to move listChildNode to.
            if (listChildNode->parentNode() != listNode)
                splitElement(static_cast<Element *>(listNode), splitTreeToNode(listChildNode, listNode));
            insertNodeBefore(nodeToInsert.get(), listNode);
        } else
            insertNodeAfter(nodeToInsert.get(), listNode);
        
        VisiblePosition insertionPoint = VisiblePosition(Position(placeholder.get(), 0));
        moveParagraphs(start, end, insertionPoint, true);
    }
    if (!listChildNode || switchListType || m_forceCreateList) {
        // Create list.
        VisiblePosition start = startOfParagraph(endingSelection().visibleStart());
        VisiblePosition end = endOfParagraph(endingSelection().visibleEnd());
        
        // Check for adjoining lists.
        VisiblePosition previousPosition = start.previous(true);
        VisiblePosition nextPosition = end.next(true);
        RefPtr<Element> listItemElement = createListItemElement(document());
        RefPtr<Element> placeholder = createBreakElement(document());
        appendNode(placeholder.get(), listItemElement.get());
        Node* previousList = outermostEnclosingList(previousPosition.deepEquivalent().node());
        Node* nextList = outermostEnclosingList(nextPosition.deepEquivalent().node());
        Node* startNode = start.deepEquivalent().node();
        Node* previousCell = enclosingTableCell(previousPosition.deepEquivalent());
        Node* nextCell = enclosingTableCell(nextPosition.deepEquivalent());
        Node* currentCell = enclosingTableCell(start.deepEquivalent());
        if (previousList && (!previousList->hasTagName(listTag) || startNode->isDescendantOf(previousList) || previousCell != currentCell))
            previousList = 0;
        if (nextList && (!nextList->hasTagName(listTag) || startNode->isDescendantOf(nextList) || nextCell != currentCell))
            nextList = 0;
        // Place list item into adjoining lists.
        if (previousList)
            appendNode(listItemElement.get(), previousList);
        else if (nextList)
            insertNodeAt(listItemElement.get(), Position(nextList, 0));
        else {
            // Create the list.
            RefPtr<Element> listElement = m_type == OrderedList ? createOrderedListElement(document()) : createUnorderedListElement(document());
            m_listElement = listElement;
            if (!m_id.isEmpty())
                static_cast<HTMLElement*>(listElement.get())->setId(m_id);
            appendNode(listItemElement.get(), listElement.get());
            
            if (start == end && isBlock(start.deepEquivalent().node())) {
                // Inserting the list into an empty paragraph that isn't held open 
                // by a br or a '\n', will invalidate start and end.  Insert 
                // a placeholder and then recompute start and end.
                Node* placeholder = insertBlockPlaceholder(start.deepEquivalent());
                start = VisiblePosition(Position(placeholder, 0));
                end = start;
            }
            
            // Insert the list at a position visually equivalent to start of the
            // paragraph that is being moved into the list. 
            // Try to avoid inserting it somewhere where it will be surrounded by 
            // inline ancestors of start, since it is easier for editing to produce 
            // clean markup when inline elements are pushed down as far as possible.
            Position insertionPos(start.deepEquivalent().upstream());
            // Also avoid the containing list item.
            Node* listChild = enclosingListChild(insertionPos.node());
            if (listChild && listChild->hasTagName(liTag))
                insertionPos = positionBeforeNode(listChild);
                
            insertNodeAt(listElement.get(), insertionPos);
        }
        moveParagraph(start, end, VisiblePosition(Position(placeholder.get(), 0)), true);
        if (nextList && previousList)
            mergeIdenticalElements(static_cast<Element*>(previousList), static_cast<Element*>(nextList));
    }
}

}
