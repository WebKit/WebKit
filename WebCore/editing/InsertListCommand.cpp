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
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

Node* InsertListCommand::fixOrphanedListChild(Node* node)
{
    RefPtr<Element> listElement = createUnorderedListElement(document());
    insertNodeBefore(listElement.get(), node);
    removeNode(node);
    appendNode(node, listElement.get());
    return listElement.get();
}

InsertListCommand::InsertListCommand(Document* document, EListType type, const String& id) 
    : CompositeEditCommand(document), m_type(type), m_id(id), m_forceCreateList(false)
{}

bool InsertListCommand::modifyRange()
{
    ASSERT(endingSelection().isRange());
    VisiblePosition visibleStart = endingSelection().visibleStart();
    VisiblePosition visibleEnd = endingSelection().visibleEnd();
    VisiblePosition startOfLastParagraph = startOfParagraph(visibleEnd);
    
    if (startOfParagraph(visibleStart) == startOfLastParagraph)
        return false;
    
    Node* startList = enclosingList(visibleStart.deepEquivalent().node());
    Node* endList = enclosingList(visibleEnd.deepEquivalent().node());
    if (!startList || startList != endList)
        m_forceCreateList = true;

    setEndingSelection(visibleStart);
    doApply();
    visibleStart = endingSelection().visibleStart();
    VisiblePosition nextParagraph = endOfParagraph(visibleStart).next();
    while (nextParagraph.isNotNull() && nextParagraph != startOfLastParagraph) {
        setEndingSelection(nextParagraph);
        doApply();
        nextParagraph = endOfParagraph(endingSelection().visibleStart()).next();
    }
    setEndingSelection(visibleEnd);
    doApply();
    visibleEnd = endingSelection().visibleEnd();
    setEndingSelection(Selection(visibleStart.deepEquivalent(), visibleEnd.deepEquivalent(), DOWNSTREAM));
    m_forceCreateList = false;
    
    return true;
}

void InsertListCommand::doApply()
{
    if (endingSelection().isNone())
        return;

    if (endingSelection().isRange() && modifyRange())
        return;
    
    if (!endingSelection().rootEditableElement())
        return;
    
    Node* selectionNode = endingSelection().start().node();
    const QualifiedName listTag = (m_type == OrderedListType) ? olTag : ulTag;
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
        if (nextListChild && previousListChild) {
            splitElement(static_cast<Element *>(listNode), nextListChild);
            insertNodeBefore(placeholder.get(), listNode);
        } else if (nextListChild)
            insertNodeBefore(placeholder.get(), listNode);
        else
            insertNodeAfter(placeholder.get(), listNode);
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
        Node* previousList = outermostEnclosingList(previousPosition.deepEquivalent().node());
        Node* nextList = outermostEnclosingList(nextPosition.deepEquivalent().node());
        if (previousList && !previousList->hasTagName(listTag))
            previousList = 0;
        if (nextList && !nextList->hasTagName(listTag))
            nextList = 0;
        // Stitch matching adjoining lists together.
        if (previousList)
            appendNode(listItemElement.get(), previousList);
        else if (nextList)
            appendNode(listItemElement.get(), nextList);
        else {
            // Create the list.
            RefPtr<Element> listElement = m_type == OrderedListType ? createOrderedListElement(document()) : createUnorderedListElement(document());
            static_cast<HTMLElement*>(listElement.get())->setId(m_id);
            appendNode(listItemElement.get(), listElement.get());
            insertNodeAt(listElement.get(), start.deepEquivalent().node(), start.deepEquivalent().offset());
        }
        moveParagraph(start, end, VisiblePosition(Position(listItemElement.get(), 0)), true);
        if (nextList && previousList)
            mergeIdenticalElements(static_cast<Element*>(previousList), static_cast<Element*>(nextList));
    }
}

}
