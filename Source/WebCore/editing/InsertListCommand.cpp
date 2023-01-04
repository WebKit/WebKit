/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2015 Google Inc. All rights reserved.
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
#include "InsertListCommand.h"

#include "Document.h"
#include "Editing.h"
#include "ElementTraversal.h"
#include "HTMLBRElement.h"
#include "HTMLLIElement.h"
#include "HTMLNames.h"
#include "HTMLUListElement.h"
#include "Range.h"
#include "VisibleUnits.h"

namespace WebCore {

using namespace HTMLNames;

static Node* enclosingListChild(Node* node, Node* listNode)
{
    Node* listChild = enclosingListChild(node);
    while (listChild && enclosingList(listChild) != listNode)
        listChild = enclosingListChild(listChild->parentNode());
    return listChild;
}

RefPtr<HTMLElement> InsertListCommand::insertList(Document& document, Type type)
{
    RefPtr<InsertListCommand> insertCommand = create(document, type);
    insertCommand->apply();
    return insertCommand->m_listElement;
}

HTMLElement* InsertListCommand::fixOrphanedListChild(Node& node)
{
    RefPtr parentNode { node.parentNode() };
    if (parentNode && !parentNode->hasRichlyEditableStyle())
        return nullptr;

    auto listElement = HTMLUListElement::create(document());
    insertNodeBefore(listElement.copyRef(), node);
    if (!listElement->hasEditableStyle())
        return nullptr;

    removeNode(node);
    appendNode(node, listElement.copyRef());
    m_listElement = WTFMove(listElement);
    return m_listElement.get();
}

Ref<HTMLElement> InsertListCommand::mergeWithNeighboringLists(HTMLElement& list)
{
    Ref<HTMLElement> protectedList = list;
    RefPtr previousList = list.previousElementSibling();
    if (canMergeLists(previousList.get(), &list))
        mergeIdenticalElements(*previousList, list);

    RefPtr sibling = ElementTraversal::nextSibling(list);
    if (!is<HTMLElement>(sibling))
        return protectedList;

    Ref<HTMLElement> nextList = downcast<HTMLElement>(*sibling);
    if (canMergeLists(&list, nextList.ptr())) {
        mergeIdenticalElements(list, nextList);
        return nextList;
    }
    return protectedList;
}

bool InsertListCommand::selectionHasListOfType(const VisibleSelection& selection, const QualifiedName& listTag)
{
    VisiblePosition start = selection.visibleStart();

    if (!enclosingList(start.deepEquivalent().deprecatedNode()))
        return false;

    VisiblePosition end = startOfParagraph(selection.visibleEnd());
    while (start.isNotNull() && start != end) {
        Element* listNode = enclosingList(start.deepEquivalent().deprecatedNode());
        if (!listNode || !listNode->hasTagName(listTag))
            return false;
        start = startOfNextParagraph(start);
    }

    return true;
}

InsertListCommand::InsertListCommand(Document& document, Type type)
    : CompositeEditCommand(document)
    , m_type(type)
{
}

void InsertListCommand::doApply()
{
    VisiblePosition visibleEnd = endingSelection().visibleEnd();
    VisiblePosition visibleStart = endingSelection().visibleStart();

    if (visibleEnd.isNull() || visibleStart.isNull() || !endingSelection().isContentRichlyEditable())
        return;

    // When a selection ends at the start of a paragraph, we rarely paint
    // the selection gap before that paragraph, because there often is no gap.  
    // In a case like this, it's not obvious to the user that the selection 
    // ends "inside" that paragraph, so it would be confusing if InsertUn{Ordered}List 
    // operated on that paragraph.
    // FIXME: We paint the gap before some paragraphs that are indented with left 
    // margin/padding, but not others.  We should make the gap painting more consistent and 
    // then use a left margin/padding rule here.
    if (visibleEnd != visibleStart && isStartOfParagraph(visibleEnd, CanSkipOverEditingBoundary)) {
        setEndingSelection(VisibleSelection(visibleStart, visibleEnd.previous(CannotCrossEditingBoundary), endingSelection().isDirectional()));
        if (!endingSelection().rootEditableElement())
            return;
    }

    auto& listTag = (m_type == Type::OrderedList) ? olTag : ulTag;
    if (endingSelection().isRange()) {
        VisibleSelection selection = selectionForParagraphIteration(endingSelection());
        if (selection.isRange()) {
            VisiblePosition startOfSelection = selection.visibleStart();
            VisiblePosition endOfSelection = selection.visibleEnd();
            VisiblePosition startOfLastParagraph = startOfParagraph(endOfSelection, CanSkipOverEditingBoundary);

            RefPtr<ContainerNode> startScope;
            int startIndex = indexForVisiblePosition(startOfSelection, startScope);

            if (startOfLastParagraph.isNotNull() && startOfParagraph(startOfSelection, CanSkipOverEditingBoundary) != startOfLastParagraph) {
                bool forceCreateList = !selectionHasListOfType(selection, listTag);

                auto currentSelection = *endingSelection().firstRange();
                VisiblePosition startOfCurrentParagraph = startOfSelection;
                while (startOfCurrentParagraph.isNotNull() && !inSameParagraph(startOfCurrentParagraph, startOfLastParagraph, CanCrossEditingBoundary)) {
                    // doApply() may operate on and remove the last paragraph of the selection from the document
                    // if it's in the same list item as startOfCurrentParagraph. Return early to avoid an
                    // infinite loop and because there is no more work to be done.
                    // FIXME(<rdar://problem/5983974>): The endingSelection() may be incorrect here. Compute
                    // the new location of endOfSelection and use it as the end of the new selection.
                    if (startOfLastParagraph.isOrphan())
                        return;
                    setEndingSelection(startOfCurrentParagraph);

                    // Save and restore endOfSelection and startOfLastParagraph when necessary
                    // since moveParagraph and movePragraphWithClones can remove nodes.
                    // FIXME: This is an inefficient way to keep selection alive because indexForVisiblePosition walks from
                    // the beginning of the document to the endOfSelection everytime this code is executed.
                    // But not using index is hard because there are so many ways we can lose selection inside doApplyForSingleParagraph.
                    RefPtr<ContainerNode> scope;
                    if (endOfSelection.isOrphan())
                        return;
                    int indexForEndOfSelection = indexForVisiblePosition(endOfSelection, scope);
                    doApplyForSingleParagraph(forceCreateList, listTag, currentSelection);
                    if (endOfSelection.isNull() || endOfSelection.isOrphan() || startOfLastParagraph.isNull() || startOfLastParagraph.isOrphan()) {
                        endOfSelection = visiblePositionForIndex(indexForEndOfSelection, scope.get());
                        // If endOfSelection is null, then some contents have been deleted from the document.
                        // This should never happen and if it did, exit early immediately because we've lost the loop invariant.
                        ASSERT(endOfSelection.isNotNull());
                        if (endOfSelection.isNull() || !endOfSelection.rootEditableElement())
                            return;
                        startOfLastParagraph = startOfParagraph(endOfSelection, CanSkipOverEditingBoundary);
                    }

                    // Fetch the start of the selection after moving the first paragraph,
                    // because moving the paragraph will invalidate the original start.
                    // We'll use the new start to restore the original selection after
                    // we modified all selected paragraphs.
                    if (startOfCurrentParagraph == startOfSelection)
                        startOfSelection = endingSelection().visibleStart();

                    startOfCurrentParagraph = startOfNextParagraph(endingSelection().visibleStart());
                }
                setEndingSelection(endOfSelection);
                doApplyForSingleParagraph(forceCreateList, listTag, currentSelection);
                // Fetch the end of the selection, for the reason mentioned above.
                endOfSelection = endingSelection().visibleEnd();
                if (startOfSelection.isOrphan())
                    startOfSelection = visiblePositionForIndex(startIndex, startScope.get());
                setEndingSelection(VisibleSelection(startOfSelection, endOfSelection, endingSelection().isDirectional()));
                return;
            }
        }
    }

    auto range = endingSelection().firstRange();
    doApplyForSingleParagraph(false, listTag, *range);
}

EditAction InsertListCommand::editingAction() const
{
    return m_type == Type::OrderedList ? EditAction::InsertOrderedList : EditAction::InsertUnorderedList;
}

void InsertListCommand::doApplyForSingleParagraph(bool forceCreateList, const HTMLQualifiedName& listTag, SimpleRange& currentSelection)
{
    // FIXME: This will produce unexpected results for a selection that starts just before a
    // table and ends inside the first cell, selectionForParagraphIteration should probably
    // be renamed and deployed inside setEndingSelection().
    Node* selectionNode = endingSelection().start().deprecatedNode();
    Node* listChildNode = enclosingListChild(selectionNode);
    bool switchListType = false;
    if (listChildNode) {
        // Remove the list child.
        RefPtr<HTMLElement> listNode = enclosingList(listChildNode);
        if (!listNode) {
            RefPtr<HTMLElement> listElement = fixOrphanedListChild(*listChildNode);
            if (!listElement || !listElement->isConnected())
                return;

            listNode = mergeWithNeighboringLists(*listElement);
            if (!listNode || !listNode->isConnected())
                return;
        }
        
        if (!listNode->hasTagName(listTag)) {
            // listChildNode will be removed from the list and a list of type m_type will be created.
            switchListType = true;
        }

        // If the list is of the desired type, and we are not removing the list, then exit early.
        if (!switchListType && forceCreateList)
            return;

        // If the entire list is selected, then convert the whole list.
        if (switchListType && isNodeVisiblyContainedWithin(*listNode, currentSelection)) {
            bool rangeStartIsInList = visiblePositionBeforeNode(*listNode) == makeDeprecatedLegacyPosition(currentSelection.start);
            bool rangeEndIsInList = visiblePositionAfterNode(*listNode) == makeDeprecatedLegacyPosition(currentSelection.end);

            RefPtr<HTMLElement> newList = createHTMLElement(document(), listTag);
            insertNodeBefore(*newList, *listNode);
            if (!newList->hasEditableStyle())
                return;

            auto* firstChildInList = enclosingListChild(VisiblePosition(firstPositionInNode(listNode.get())).deepEquivalent().deprecatedNode(), listNode.get());
            Node* outerBlock = firstChildInList && isBlockFlowElement(*firstChildInList) ? firstChildInList : listNode.get();
            
            moveParagraphWithClones(firstPositionInNode(listNode.get()), lastPositionInNode(listNode.get()), newList.get(), outerBlock);

            // Manually remove listNode because moveParagraphWithClones sometimes leaves it behind in the document.
            // See the bug 33668 and editing/execCommand/insert-list-orphaned-item-with-nested-lists.html.
            // FIXME: This might be a bug in moveParagraphWithClones or deleteSelection.
            if (listNode && listNode->isConnected())
                removeNode(*listNode);

            newList = mergeWithNeighboringLists(*newList);

            // Restore the start and the end of current selection if they started inside listNode
            // because moveParagraphWithClones could have removed them.
            if (rangeStartIsInList && newList)
                currentSelection.start = makeBoundaryPointBeforeNodeContents(*newList);
            if (rangeEndIsInList && newList)
                currentSelection.end = makeBoundaryPointAfterNodeContents(*newList);

            setEndingSelection(VisibleSelection(makeContainerOffsetPosition(currentSelection.start),
                makeContainerOffsetPosition(currentSelection.end)));

            return;
        }
        
        unlistifyParagraph(endingSelection().visibleStart(), *listNode, listChildNode);
    }

    if (!listChildNode || switchListType || forceCreateList)
        m_listElement = listifyParagraph(endingSelection().visibleStart(), listTag);
}

void InsertListCommand::unlistifyParagraph(const VisiblePosition& originalStart, HTMLElement& listNode, Node* listChildNode)
{
    RefPtr<Node> nextListChild;
    RefPtr<Node> previousListChild;
    VisiblePosition start;
    VisiblePosition end;

    if (!listNode.parentNode() || !listNode.parentNode()->hasEditableStyle())
        return;

    if (listChildNode->hasTagName(liTag)) {
        start = firstPositionInNode(listChildNode);
        end = lastPositionInNode(listChildNode);
        nextListChild = listChildNode->nextSibling();
        previousListChild = listChildNode->previousSibling();
    } else {
        // A paragraph is visually a list item minus a list marker.  The paragraph will be moved.
        start = startOfParagraph(originalStart, CanSkipOverEditingBoundary);
        end = endOfParagraph(start, CanSkipOverEditingBoundary);
        nextListChild = enclosingListChild(end.next().deepEquivalent().deprecatedNode(), &listNode);
        ASSERT(nextListChild != listChildNode);
        previousListChild = enclosingListChild(start.previous().deepEquivalent().deprecatedNode(), &listNode);
        ASSERT(previousListChild != listChildNode);
    }

    if (start.isNull() || end.isNull())
        return;

    // When removing a list, we must always create a placeholder to act as a point of insertion
    // for the list content being removed.
    auto placeholder = HTMLBRElement::create(document());
    RefPtr<Element> nodeToInsert = placeholder.copyRef();
    // If the content of the list item will be moved into another list, put it in a list item
    // so that we don't create an orphaned list child.
    if (enclosingList(&listNode)) {
        nodeToInsert = HTMLLIElement::create(document());
        appendNode(placeholder.copyRef(), *nodeToInsert);
    }

    if (nextListChild && previousListChild) {
        // We want to pull listChildNode out of listNode, and place it before nextListChild 
        // and after previousListChild, so we split listNode and insert it between the two lists.  
        // But to split listNode, we must first split ancestors of listChildNode between it and listNode,
        // if any exist.
        // FIXME: We appear to split at nextListChild as opposed to listChildNode so that when we remove
        // listChildNode below in moveParagraphs, previousListChild will be removed along with it if it is 
        // unrendered. But we ought to remove nextListChild too, if it is unrendered.
        splitElement(listNode, *splitTreeToNode(*nextListChild, listNode));
        insertNodeBefore(nodeToInsert.releaseNonNull(), listNode);
    } else if (nextListChild || listChildNode->parentNode() != &listNode) {
        // Just because listChildNode has no previousListChild doesn't mean there isn't any content
        // in listNode that comes before listChildNode, as listChildNode could have ancestors
        // between it and listNode. So, we split up to listNode before inserting the placeholder
        // where we're about to move listChildNode to.
        if (RefPtr listChildNodeParentNode { listChildNode->parentNode() }; listChildNodeParentNode && listChildNodeParentNode != &listNode)
            splitElement(listNode, *splitTreeToNode(*listChildNode, listNode).get());
        insertNodeBefore(nodeToInsert.releaseNonNull(), listNode);
    } else
        insertNodeAfter(nodeToInsert.releaseNonNull(), listNode);

    VisiblePosition insertionPoint = VisiblePosition(positionBeforeNode(placeholder.ptr()));
    moveParagraphs(start, end, insertionPoint, true);
}

static RefPtr<Element> adjacentEnclosingList(const VisiblePosition& pos, const VisiblePosition& adjacentPos, const QualifiedName& listTag)
{
    RefPtr<Element> listNode = outermostEnclosingList(adjacentPos.deepEquivalent().deprecatedNode());

    if (!listNode)
        return nullptr;

    RefPtr previousCell = enclosingTableCell(pos.deepEquivalent());
    RefPtr currentCell = enclosingTableCell(adjacentPos.deepEquivalent());

    if (!listNode->hasTagName(listTag)
        || listNode->contains(pos.deepEquivalent().deprecatedNode())
        || previousCell != currentCell
        || enclosingList(listNode.get()) != enclosingList(pos.deepEquivalent().deprecatedNode()))
        return nullptr;

    return listNode;
}

RefPtr<HTMLElement> InsertListCommand::listifyParagraph(const VisiblePosition& originalStart, const QualifiedName& listTag)
{
    VisiblePosition start = startOfParagraph(originalStart, CanSkipOverEditingBoundary);
    VisiblePosition end = endOfParagraph(start, CanSkipOverEditingBoundary);
    
    if (start.isNull() || end.isNull() || !start.deepEquivalent().containerNode()->hasEditableStyle() || !end.deepEquivalent().containerNode()->hasEditableStyle())
        return nullptr;

    // Check for adjoining lists.
    auto listItemElement = HTMLLIElement::create(document());
    auto placeholder = HTMLBRElement::create(document());
    appendNode(placeholder.copyRef(), listItemElement.copyRef());

    // Place list item into adjoining lists.
    auto previousList = adjacentEnclosingList(start.deepEquivalent(), start.previous(CannotCrossEditingBoundary), listTag);
    auto nextList = adjacentEnclosingList(start.deepEquivalent(), end.next(CannotCrossEditingBoundary), listTag);
    RefPtr<HTMLElement> listElement;
    if (previousList)
        appendNode(WTFMove(listItemElement), *previousList);
    else if (nextList)
        insertNodeAt(WTFMove(listItemElement), positionBeforeNode(nextList.get()));
    else {
        // Create the list.
        listElement = createHTMLElement(document(), listTag);
        appendNode(WTFMove(listItemElement), *listElement);

        if (start == end && isBlock(start.deepEquivalent().deprecatedNode())) {
            // Inserting the list into an empty paragraph that isn't held open 
            // by a br or a '\n', will invalidate start and end.  Insert 
            // a placeholder and then recompute start and end.
            auto blockPlaceholder = insertBlockPlaceholder(start.deepEquivalent());
            start = positionBeforeNode(blockPlaceholder.get());
            end = start;
        }

        // Insert the list at a position visually equivalent to start of the
        // paragraph that is being moved into the list. 
        // Try to avoid inserting it somewhere where it will be surrounded by 
        // inline ancestors of start, since it is easier for editing to produce 
        // clean markup when inline elements are pushed down as far as possible.
        Position insertionPos(start.deepEquivalent().upstream());
        // Also avoid the containing list item.
        RefPtr listChild = enclosingListChild(insertionPos.deprecatedNode());
        if (listChild && listChild->hasTagName(liTag))
            insertionPos = positionInParentBeforeNode(listChild.get());

        if (!isEditablePosition(insertionPos))
            return nullptr;

        insertNodeAt(*listElement, insertionPos);

        // We inserted the list at the start of the content we're about to move
        // Update the start of content, so we don't try to move the list into itself.  bug 19066
        // Layout is necessary since start's node's inline renderers may have been destroyed by the insertion
        // The end of the content may have changed after the insertion and layout so update it as well.
        if (insertionPos == start.deepEquivalent())
            start = originalStart;
    }

    // Inserting list element and list item list may change start of pargraph to move. We calculate start of paragraph again.
    document().updateLayoutIgnorePendingStylesheets();
    start = startOfParagraph(start, CanSkipOverEditingBoundary);
    end = endOfParagraph(start, CanSkipOverEditingBoundary);
    moveParagraph(start, end, positionBeforeNode(placeholder.ptr()), true);

    if (listElement)
        return mergeWithNeighboringLists(*listElement);

    if (canMergeLists(previousList.get(), nextList.get()))
        mergeIdenticalElements(*previousList, *nextList);

    return listElement;
}

}
