/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#include "BreakBlockquoteCommand.h"

#include "Element.h"
#include "HTMLNames.h"
#include "Text.h"
#include "VisiblePosition.h"
#include "htmlediting.h"
#include "RenderListItem.h"

namespace WebCore {

using namespace HTMLNames;

BreakBlockquoteCommand::BreakBlockquoteCommand(Document *document)
    : CompositeEditCommand(document)
{
}

void BreakBlockquoteCommand::doApply()
{
    if (endingSelection().isNone())
        return;
    
    // Delete the current selection.
    if (endingSelection().isRange())
        deleteSelection(false, false);
    
    VisiblePosition visiblePos = endingSelection().visibleStart();
    // pos is a position equivalent to the caret.  We use downstream() so that pos will 
    // be in the first node that we need to move (there are a few exceptions to this, see below).
    Position pos = endingSelection().start().downstream();
    
    // startNode is the first node that we need to move to the new blockquote.
    Node* startNode = pos.node();
    // Find the top-most blockquote from the start.
    Element* topBlockquote = 0;
    for (Node *node = startNode->parentNode(); node; node = node->parentNode()) {
        if (isMailBlockquote(node))
            topBlockquote = static_cast<Element*>(node);
    }
    if (!topBlockquote || !topBlockquote->parentNode())
        return;
    
    // Insert a break after the top blockquote.
    RefPtr<Element> breakNode = createBreakElement(document());
    insertNodeAfter(breakNode.get(), topBlockquote);
    
    if (isLastVisiblePositionInNode(visiblePos, topBlockquote)) {
        setEndingSelection(Selection(Position(breakNode.get(), 0), DOWNSTREAM));
        rebalanceWhitespace();   
        return;
    }
    
    // Don't move a line break just after the caret.  Doing so would create an extra, empty paragraph
    // in the new blockquote.
    if (lineBreakExistsAtPosition(visiblePos))
        pos = pos.next();
        
    // Split at pos if in the middle of a text node.
    if (startNode->isTextNode()) {
        Text* textNode = static_cast<Text*>(startNode);
        if ((unsigned)pos.offset() >= textNode->length()) {
            startNode = startNode->traverseNextNode();
            ASSERT(startNode);
        } else if (pos.offset() > 0)
            splitTextNode(textNode, pos.offset());
    } else if (pos.offset() > 0) {
        startNode = startNode->traverseNextNode();
        ASSERT(startNode);
    }
    
    // If there's nothing inside topBlockquote to move, we're finished.
    if (!startNode->isDescendantOf(topBlockquote)) {
        setEndingSelection(Selection(VisiblePosition(Position(startNode, 0))));
        return;
    }
    
    // Build up list of ancestors in between the start node and the top blockquote.
    Vector<Element*> ancestors;    
    for (Element* node = startNode->parentElement(); node && node != topBlockquote; node = node->parentElement())
        ancestors.append(node);
    
    // Insert a clone of the top blockquote after the break.
    RefPtr<Element> clonedBlockquote = topBlockquote->cloneElement();
    insertNodeAfter(clonedBlockquote.get(), breakNode.get());
    
    // Clone startNode's ancestors into the cloned blockquote.
    // On exiting this loop, clonedAncestor is the lowest ancestor
    // that was cloned (i.e. the clone of either ancestors.last()
    // or clonedBlockquote if ancestors is empty).
    RefPtr<Element> clonedAncestor = clonedBlockquote;
    for (size_t i = ancestors.size(); i != 0; --i) {
        RefPtr<Element> clonedChild = ancestors[i - 1]->cloneElement(); // shallow clone
        // Preserve list item numbering in cloned lists.
        if (clonedChild->isElementNode() && clonedChild->hasTagName(olTag)) {
            Node* listChildNode = i > 1 ? ancestors[i - 2] : startNode;
            // The first child of the cloned list might not be a list item element, 
            // find the first one so that we know where to start numbering.
            while (listChildNode && !listChildNode->hasTagName(liTag))
                listChildNode = listChildNode->nextSibling();
            if (listChildNode && listChildNode->renderer())
                setNodeAttribute(static_cast<Element*>(clonedChild.get()), startAttr, String::number(static_cast<RenderListItem*>(listChildNode->renderer())->value()));
        }
            
        appendNode(clonedChild.get(), clonedAncestor.get());
        clonedAncestor = clonedChild;
    }
    
    // Move the startNode and its siblings.
    Node *moveNode = startNode;
    while (moveNode) {
        Node *next = moveNode->nextSibling();
        removeNode(moveNode);
        appendNode(moveNode, clonedAncestor.get());
        moveNode = next;
    }

    // Hold open startNode's original parent if we emptied it
    if (!ancestors.isEmpty()) {
        addBlockPlaceholderIfNeeded(ancestors.first());

        // Split the tree up the ancestor chain until the topBlockquote
        // Throughout this loop, clonedParent is the clone of ancestor's parent.
        // This is so we can clone ancestor's siblings and place the clones
        // into the clone corresponding to the ancestor's parent.
        Element* ancestor;
        Element* clonedParent;
        for (ancestor = ancestors.first(), clonedParent = clonedAncestor->parentElement();
             ancestor && ancestor != topBlockquote;
             ancestor = ancestor->parentElement(), clonedParent = clonedParent->parentElement()) {
            moveNode = ancestor->nextSibling();
            while (moveNode) {
                Node *next = moveNode->nextSibling();
                removeNode(moveNode);
                appendNode(moveNode, clonedParent);
                moveNode = next;
            }
        }
    }
    
    // Make sure the cloned block quote renders.
    addBlockPlaceholderIfNeeded(clonedBlockquote.get());
    
    // Put the selection right before the break.
    setEndingSelection(Selection(Position(breakNode.get(), 0), DOWNSTREAM));
    rebalanceWhitespace();
}

} // namespace WebCore
