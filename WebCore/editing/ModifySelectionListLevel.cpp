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
#include "ModifySelectionListLevel.h"

#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "RenderObject.h"
#include "SelectionController.h"
#include "htmlediting.h"

namespace WebCore {

ModifySelectionListLevelCommand::ModifySelectionListLevelCommand(Document* document) 
    : CompositeEditCommand(document)
{
}

bool ModifySelectionListLevelCommand::preservesTypingStyle() const
{
    return true;
}

// This needs to be static so it can be called by canIncreaseSelectionListLevel and canDecreaseSelectionListLevel
static bool getStartEndListChildren(const Selection& selection, Node** start, Node** end)
{
    if (selection.isNone())
        return false;

    // start must be in a list child
    Node* startListChild = enclosingListChild(selection.start().node());
    if (!startListChild)
        return false;
        
    // end must be in a list child
    Node* endListChild = selection.isRange() ? enclosingListChild(selection.end().node()) : startListChild;
    if (!endListChild)
        return false;
    
    // For a range selection we want the following behavior:
    //      - the start and end must be within the same overall list
    //      - the start must be at or above the level of the rest of the range
    //      - if the end is anywhere in a sublist lower than start, the whole sublist gets moved
    // In terms of this function, this means:
    //      - endListChild must start out being be a sibling of startListChild, or be in a
    //         sublist of startListChild or a sibling
    //      - if endListChild is in a sublist of startListChild or a sibling, it must be adjusted
    //         to be the ancestor that is startListChild or its sibling
    while (startListChild->parentNode() != endListChild->parentNode()) {
        endListChild = endListChild->parentNode();
        if (!endListChild)
            return false;
    }
    
    // if the selection ends on a list item with a sublist, include the entire sublist
    if (endListChild->renderer()->isListItem()) {
        RenderObject* r = endListChild->renderer()->nextSibling();
        if (r && isListElement(r->element()))
            endListChild = r->element();
    }

    *start = startListChild;
    *end = endListChild;
    return true;
}

void ModifySelectionListLevelCommand::insertSiblingNodeRangeBefore(Node* startNode, Node* endNode, Node* refNode)
{
    Node* node = startNode;
    while (1) {
        Node* next = node->nextSibling();
        removeNode(node);
        insertNodeBefore(node, refNode);

        if (node == endNode)
            break;

        node = next;
    }
}

void ModifySelectionListLevelCommand::insertSiblingNodeRangeAfter(Node* startNode, Node* endNode, Node* refNode)
{
    Node* node = startNode;
    while (1) {
        Node* next = node->nextSibling();
        removeNode(node);
        insertNodeAfter(node, refNode);

        if (node == endNode)
            break;

        refNode = node;
        node = next;
    }
}

void ModifySelectionListLevelCommand::appendSiblingNodeRange(Node* startNode, Node* endNode, Node* newParent)
{
    Node* node = startNode;
    while (1) {
        Node* next = node->nextSibling();
        removeNode(node);
        appendNode(node, newParent);

        if (node == endNode)
            break;

        node = next;
    }
}

IncreaseSelectionListLevelCommand::IncreaseSelectionListLevelCommand(Document* document, EListType listType)
    : ModifySelectionListLevelCommand(document)
{
    m_listType = listType;
    m_listElement = 0;
}

Node* IncreaseSelectionListLevelCommand::listElement()
{
    return m_listElement;
}

// This needs to be static so it can be called by canIncreaseSelectionListLevel
static bool canIncreaseListLevel(const Selection& selection, Node** start, Node** end)
{
    if (!getStartEndListChildren(selection, start, end))
        return false;
        
    // start must not be the first child (because you need a prior one
    // to increase relative to)
    if (!(*start)->renderer()->previousSibling())
        return false;
    
    return true;
}

// For the moment, this is SPI and the only client (Mail.app) is satisfied.
// Here are two things to re-evaluate when making into API.
// 1. Currently, InheritedListType uses clones whereas OrderedList and
// UnorderedList create a new list node of the specified type.  That is
// inconsistent wrt style.  If that is not OK, here are some alternatives:
//  - new nodes always inherit style (probably the best choice)
//  - new nodes have always have no style
//  - new nodes of the same type inherit style
// 2. Currently, the node we return may be either a pre-existing one or
// a new one. Is it confusing to return the pre-existing one without
// somehow indicating that it is not new?  If so, here are some alternatives:
//  - only return the list node if we created it
//  - indicate whether the list node is new or pre-existing
//  - (silly) client specifies whether to return pre-existing list nodes
void IncreaseSelectionListLevelCommand::doApply()
{
    Node* startListChild;
    Node* endListChild;
    if (!canIncreaseListLevel(endingSelection(), &startListChild, &endListChild))
        return;

    Node* previousItem = startListChild->renderer()->previousSibling()->element();
    if (isListElement(previousItem)) {
        // move nodes up into preceding list
        appendSiblingNodeRange(startListChild, endListChild, previousItem);
        m_listElement = previousItem;
    } else {
        // create a sublist for the preceding element and move nodes there
        RefPtr<Node> newParent;
        switch (m_listType) {
            case InheritedListType:
                newParent = startListChild->parentNode()->cloneNode(false);
                break;
            case OrderedList:
                newParent = createOrderedListElement(document());
                break;
            case UnorderedList:
                newParent = createUnorderedListElement(document());
                break;
        }
        insertNodeBefore(newParent.get(), startListChild);
        appendSiblingNodeRange(startListChild, endListChild, newParent.get());
        m_listElement = newParent.get();
    }
}

bool IncreaseSelectionListLevelCommand::canIncreaseSelectionListLevel(Document* document)
{
    Node* startListChild;
    Node* endListChild;
    
    return canIncreaseListLevel(document->frame()->selection().selection(), &startListChild, &endListChild);
}

static Node* increaseSelectionListLevelWithType(Document* document, EListType listType)
{
    ASSERT(document);
    ASSERT(document->frame());
    IncreaseSelectionListLevelCommand* modCommand = new IncreaseSelectionListLevelCommand(document, listType);
    EditCommandPtr cmd(modCommand);
    cmd.apply();
    return modCommand->listElement();
}

Node* IncreaseSelectionListLevelCommand::increaseSelectionListLevel(Document* document) {
    return increaseSelectionListLevelWithType(document, InheritedListType);
}

Node* IncreaseSelectionListLevelCommand::increaseSelectionListLevelOrdered(Document* document) {
    return increaseSelectionListLevelWithType(document, OrderedList);
}

Node* IncreaseSelectionListLevelCommand::increaseSelectionListLevelUnordered(Document* document) {
    return increaseSelectionListLevelWithType(document, UnorderedList);
}

DecreaseSelectionListLevelCommand::DecreaseSelectionListLevelCommand(Document* document) 
    : ModifySelectionListLevelCommand(document)
{
}

// This needs to be static so it can be called by canDecreaseSelectionListLevel
static bool canDecreaseListLevel(const Selection& selection, Node** start, Node** end)
{
    if (!getStartEndListChildren(selection, start, end))
        return false;
    
    // there must be a destination list to move the items to
    if (!isListElement((*start)->parentNode()->parentNode()))
        return false;
        
    return true;
}

void DecreaseSelectionListLevelCommand::doApply()
{
    Node* startListChild;
    Node* endListChild;
    if (!canDecreaseListLevel(endingSelection(), &startListChild, &endListChild))
        return;

    Node* previousItem = startListChild->renderer()->previousSibling() ? startListChild->renderer()->previousSibling()->element() : 0;
    Node* nextItem = endListChild->renderer()->nextSibling() ? endListChild->renderer()->nextSibling()->element() : 0;
    Node* listNode = startListChild->parentNode();

    if (!previousItem) {
        // at start of sublist, move the child(ren) to before the sublist
        insertSiblingNodeRangeBefore(startListChild, endListChild, listNode);
        // if that was the whole sublist we moved, remove the sublist node
        if (!nextItem)
            removeNode(listNode);
    } else if (!nextItem) {
        // at end of list, move the child(ren) to after the sublist
        insertSiblingNodeRangeAfter(startListChild, endListChild, listNode);    
    } else {
        // in the middle of list, split the list and move the children to the divide
        splitElement(static_cast<Element*>(listNode), startListChild);
        insertSiblingNodeRangeBefore(startListChild, endListChild, listNode);
    }
}

bool DecreaseSelectionListLevelCommand::canDecreaseSelectionListLevel(Document* document)
{
    Node* startListChild;
    Node* endListChild;
    
    return canDecreaseListLevel(document->frame()->selection().selection(), &startListChild, &endListChild);
}

void DecreaseSelectionListLevelCommand::decreaseSelectionListLevel(Document* document)
{
    ASSERT(document);
    ASSERT(document->frame());
    DecreaseSelectionListLevelCommand* modCommand = new DecreaseSelectionListLevelCommand(document);
    EditCommandPtr cmd(modCommand);
    cmd.apply();
}

}
