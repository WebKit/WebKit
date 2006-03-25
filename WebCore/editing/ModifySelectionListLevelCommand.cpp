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
#include "ModifySelectionListLevelCommand.h"

#include "CompositeEditCommand.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "RenderObject.h"
#include "SelectionController.h"
#include "htmlediting.h"
#include <kxmlcore/Assertions.h>

namespace WebCore {

static bool canIncreaseListLevel(const Selection& selection, Node** start, Node** end);
static bool canDecreaseListLevel(const Selection& selection, Node** start, Node** end);
static void modifySelectionListLevel(Document* document, EListLevelModification mod);

// public functions
ModifySelectionListLevelCommand::ModifySelectionListLevelCommand(Document* document, EListLevelModification mod) 
    : CompositeEditCommand(document)
{
    m_modification = mod;
}

bool ModifySelectionListLevelCommand::preservesTypingStyle() const
{
    return true;
}

void ModifySelectionListLevelCommand::doApply()
{
    if (m_modification == IncreaseListLevel)
        increaseListLevel(endingSelection());
    else if (m_modification == DecreaseListLevel)
        decreaseListLevel(endingSelection());
}

bool ModifySelectionListLevelCommand::canIncreaseSelectionListLevel(Document* document)
{
    Node* startListChild;
    Node* endListChild;
    
    return canIncreaseListLevel(document->frame()->selection().selection(), &startListChild, &endListChild);
}

bool ModifySelectionListLevelCommand::canDecreaseSelectionListLevel(Document* document)
{
    Node* startListChild;
    Node* endListChild;
    
    return canDecreaseListLevel(document->frame()->selection().selection(), &startListChild, &endListChild);
}

void ModifySelectionListLevelCommand::increaseSelectionListLevel(Document* document)
{
    modifySelectionListLevel(document, IncreaseListLevel);
}

void ModifySelectionListLevelCommand::decreaseSelectionListLevel(Document* document)
{
    modifySelectionListLevel(document, DecreaseListLevel);
}

// private functions
static void modifySelectionListLevel(Document* document, EListLevelModification mod)
{
    ASSERT(document);
    ASSERT(document->frame());
    
    ModifySelectionListLevelCommand* modCommand = new ModifySelectionListLevelCommand(document, mod);
    EditCommandPtr cmd(modCommand);
    cmd.apply();
}

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
    //      - if the end is anywhere in a sublist lower than start, the whole sublist get moved
    // In terms of this function, this means:
    //      - endListChild must start out being be a sibling of startListChild, or be in a
    //         sublist of startListChild or a sibling
    //      - if endListChild is in a sublist of startListChild or a sibling, it must be adjusted
    //         to be the ancestor that is startListChild or its sibling
    // This loop satisfies both of these requirements.  Our utility functions that move sibling ranges
    // take care of the rest.
    while (startListChild->parentNode() != endListChild->parentNode()) {
        endListChild = endListChild->parentNode();
        if (!endListChild)
            return false;
    }
    
    // if the selection ends on a list item with a sublist, include the sublist
    if (endListChild->renderer()->isListItem()) {
        RenderObject* r = endListChild->renderer()->nextSibling();
        if (r && isListElement(r->element()))
            endListChild = r->element();
    }

    *start = startListChild;
    *end = endListChild;
    return true;
}

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

static bool canDecreaseListLevel(const Selection& selection, Node** start, Node** end)
{
    if (!getStartEndListChildren(selection, start, end))
        return false;
    
    // there must be a destination list to move the items to
    if (!isListElement((*start)->parentNode()->parentNode()))
        return false;
        
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

void ModifySelectionListLevelCommand::increaseListLevel(const Selection& selection)
{
    Node* startListChild;
    Node* endListChild;
    if (!canIncreaseListLevel(selection, &startListChild, &endListChild))
        return;

    Node* previousItem = startListChild->renderer()->previousSibling()->element();
    if (isListElement(previousItem)) {
        // move nodes up into preceding list
        appendSiblingNodeRange(startListChild, endListChild, previousItem);
    } else {
        // create a sublist for the preceding element and move nodes there
        RefPtr<Node> newParent = startListChild->parentNode()->cloneNode(false);
        insertNodeBefore(newParent.get(), startListChild);
        appendSiblingNodeRange(startListChild, endListChild, newParent.get());
    }
}

void ModifySelectionListLevelCommand::decreaseListLevel(const Selection& selection)
{    
    Node* startListChild;
    Node* endListChild;
    if (!canDecreaseListLevel(selection, &startListChild, &endListChild))
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

}
