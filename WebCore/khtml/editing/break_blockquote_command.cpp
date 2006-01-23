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
#include "break_blockquote_command.h"

#include "dom_elementimpl.h"
#include "dom_textimpl.h"
#include "htmlediting.h"
#include "htmlnames.h"
#include "visible_position.h"
#include <kxmlcore/Assertions.h>

namespace WebCore {

using namespace HTMLNames;

BreakBlockquoteCommand::BreakBlockquoteCommand(DocumentImpl *document)
    : CompositeEditCommand(document)
{
}

void BreakBlockquoteCommand::doApply()
{
    DOM::ElementImpl *breakNode;
    QPtrList<DOM::NodeImpl> ancestors;
    
    Selection selection = endingSelection();
    if (selection.isNone())
        return;
    
    // Delete the current selection.
    Position pos = selection.start();
    EAffinity affinity = selection.affinity();
    if (selection.isRange()) {
        deleteSelection(false, false);
        pos = endingSelection().start().upstream();
        affinity = endingSelection().affinity();
    }
    
    // Find the top-most blockquote from the start.
    NodeImpl *startNode = pos.node();
    NodeImpl *topBlockquote = 0;
    for (NodeImpl *node = startNode->parentNode(); node; node = node->parentNode()) {
        if (isMailBlockquote(node))
            topBlockquote = node;
    }
    if (!topBlockquote || !topBlockquote->parentNode())
        return;
    
    // Insert a break after the top blockquote.
    breakNode = createBreakElement(document());
    insertNodeAfter(breakNode, topBlockquote);
    
    if (!isLastVisiblePositionInNode(VisiblePosition(pos, affinity), topBlockquote)) {
        
        NodeImpl *newStartNode = 0;
        // Split at pos if in the middle of a text node.
        if (startNode->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(startNode);
            if ((unsigned)pos.offset() >= textNode->length()) {
                newStartNode = startNode->traverseNextNode();
                ASSERT(newStartNode);
            } else if (pos.offset() > 0)
                splitTextNode(textNode, pos.offset());
        } else if (startNode->hasTagName(brTag)) {
            newStartNode = startNode->traverseNextNode();
            ASSERT(newStartNode);
        } else if (pos.offset() > 0) {
            newStartNode = startNode->traverseNextNode();
            ASSERT(newStartNode);
        }
        
        // If a new start node was determined, find a new top block quote.
        if (newStartNode) {
            startNode = newStartNode;
            for (NodeImpl *node = startNode->parentNode(); node; node = node->parentNode()) {
                if (isMailBlockquote(node))
                    topBlockquote = node;
            }
            if (!topBlockquote || !topBlockquote->parentNode())
                return;
        }
        
        // Build up list of ancestors in between the start node and the top blockquote.
        for (NodeImpl *node = startNode->parentNode(); node != topBlockquote; node = node->parentNode())
            ancestors.prepend(node);
        
        // Insert a clone of the top blockquote after the break.
        RefPtr<NodeImpl> clonedBlockquote = topBlockquote->cloneNode(false);
        insertNodeAfter(clonedBlockquote.get(), breakNode);
        
        // Clone startNode's ancestors into the cloned blockquote.
        // On exiting this loop, clonedAncestor is the lowest ancestor
        // that was cloned (i.e. the clone of either ancestors.last()
        // or clonedBlockquote if ancestors is empty).
        RefPtr<NodeImpl> clonedAncestor = clonedBlockquote;
        for (QPtrListIterator<NodeImpl> it(ancestors); it.current(); ++it) {
            RefPtr<NodeImpl> clonedChild = it.current()->cloneNode(false); // shallow clone
            appendNode(clonedChild.get(), clonedAncestor.get());
            clonedAncestor = clonedChild;
        }
        
        // Move the startNode and its siblings.
        NodeImpl *moveNode = startNode;
        while (moveNode) {
            NodeImpl *next = moveNode->nextSibling();
            removeNode(moveNode);
            appendNode(moveNode, clonedAncestor.get());
            moveNode = next;
        }

        // Hold open startNode's original parent if we emptied it
        addBlockPlaceholderIfNeeded(ancestors.last());

        // Split the tree up the ancestor chain until the topBlockquote
        // Throughout this loop, clonedParent is the clone of ancestor's parent.
        // This is so we can clone ancestor's siblings and place the clones
        // into the clone corresponding to the ancestor's parent.
        NodeImpl *ancestor, *clonedParent;
        for (ancestor = ancestors.last(), clonedParent = clonedAncestor->parentNode();
             ancestor && ancestor != topBlockquote;
             ancestor = ancestor->parentNode(), clonedParent = clonedParent->parentNode()) {
            moveNode = ancestor->nextSibling();
            while (moveNode) {
                NodeImpl *next = moveNode->nextSibling();
                removeNode(moveNode);
                appendNode(moveNode, clonedParent);
                moveNode = next;
            }
        }
        
        // Make sure the cloned block quote renders.
        addBlockPlaceholderIfNeeded(clonedBlockquote.get());
    }
    
    // Put the selection right before the break.
    setEndingSelection(Position(breakNode, 0), DOWNSTREAM);
    rebalanceWhitespace();
}

} // namespace khtml
