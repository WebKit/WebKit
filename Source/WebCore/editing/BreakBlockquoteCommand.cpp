/*
 * Copyright (C) 2005 Apple Inc.  All rights reserved.
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
#include "BreakBlockquoteCommand.h"

#include "CommonAtomStrings.h"
#include "Editing.h"
#include "ElementInlines.h"
#include "HTMLBRElement.h"
#include "HTMLDivElement.h"
#include "HTMLNames.h"
#include "NodeRenderStyle.h"
#include "NodeTraversal.h"
#include "RenderListItem.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

BreakBlockquoteCommand::BreakBlockquoteCommand(Document& document)
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
    
    // This is a scenario that should never happen, but we want to
    // make sure we don't dereference a null pointer below.    

    ASSERT(!endingSelection().isNone());
    
    if (endingSelection().isNone())
        return;
        
    VisiblePosition visiblePos = endingSelection().visibleStart();
    
    // pos is a position equivalent to the caret.  We use downstream() so that pos will 
    // be in the first node that we need to move (there are a few exceptions to this, see below).
    Position pos = endingSelection().start().downstream();
    
    // Find the top-most blockquote from the start.
    RefPtr topBlockquote = highestEnclosingNodeOfType(pos, isMailBlockquote);
    if (!topBlockquote || !topBlockquote->parentNode() || !topBlockquote->isElementNode())
        return;

    auto breakNode = [&]() -> Ref<HTMLElement> {
        auto lineBreak = HTMLBRElement::create(document());
        RefPtr containerNode = pos.containerNode();
        if (!containerNode || !containerNode->renderStyle())
            return lineBreak;

        auto* parentStyle = topBlockquote->parentNode()->renderStyle();
        if (!parentStyle)
            return lineBreak;

        if (parentStyle->direction() == containerNode->renderStyle()->direction())
            return lineBreak;

        auto container = HTMLDivElement::create(document());
        container->setDir(autoAtom());
        container->appendChild(lineBreak);
        return container;
    }();

    bool isLastVisPosInNode = isLastVisiblePositionInNode(visiblePos, topBlockquote.get());

    // If the position is at the beginning of the top quoted content, we don't need to break the quote.
    // Instead, insert the break before the blockquote, unless the position is as the end of the quoted content.
    if (isFirstVisiblePositionInNode(visiblePos, topBlockquote.get()) && !isLastVisPosInNode) {
        insertNodeBefore(breakNode.copyRef(), *topBlockquote);
        setEndingSelection(VisibleSelection(positionBeforeNode(breakNode.ptr()), Affinity::Downstream, endingSelection().isDirectional()));
        rebalanceWhitespace();   
        return;
    }
    
    // Insert a break after the top blockquote.
    insertNodeAfter(breakNode.copyRef(), *topBlockquote);

    // If we're inserting the break at the end of the quoted content, we don't need to break the quote.
    if (isLastVisPosInNode) {
        setEndingSelection(VisibleSelection(positionBeforeNode(breakNode.ptr()), Affinity::Downstream, endingSelection().isDirectional()));
        rebalanceWhitespace();
        return;
    }
    
    // Don't move a line break just after the caret.  Doing so would create an extra, empty paragraph
    // in the new blockquote.
    if (lineBreakExistsAtVisiblePosition(visiblePos))
        pos = pos.next();
        
    // Adjust the position so we don't split at the beginning of a quote.  
    while (isFirstVisiblePositionInNode(VisiblePosition(pos), enclosingNodeOfType(pos, isMailBlockquote)))
        pos = pos.previous();
    
    // startNode is the first node that we need to move to the new blockquote.
    Node* startNode = pos.deprecatedNode();
    ASSERT(startNode);
    // Split at pos if in the middle of a text node.
    if (is<Text>(*startNode)) {
        Text& textNode = downcast<Text>(*startNode);
        if ((unsigned)pos.deprecatedEditingOffset() >= textNode.length()) {
            if (auto* nextNode = NodeTraversal::next(*startNode))
                startNode = nextNode;
        } else if (pos.deprecatedEditingOffset() > 0)
            splitTextNode(textNode, pos.deprecatedEditingOffset());
    } else if (pos.deprecatedEditingOffset() > 0) {
        if (auto* child = startNode->traverseToChildAt(pos.deprecatedEditingOffset()))
            startNode = child;
        else if (auto* next = NodeTraversal::next(*startNode))
            startNode = next;
    }
    
    // If there's nothing inside topBlockquote to move, we're finished.
    if (!startNode->isDescendantOf(*topBlockquote)) {
        setEndingSelection(VisibleSelection(VisiblePosition(firstPositionInOrBeforeNode(startNode)), endingSelection().isDirectional()));
        return;
    }
    
    // Build up list of ancestors in between the start node and the top blockquote.
    Vector<RefPtr<Element>> ancestors;    
    for (Element* node = startNode->parentElement(); node && node != topBlockquote; node = node->parentElement())
        ancestors.append(node);
    
    // Insert a clone of the top blockquote after the break.
    auto clonedBlockquote = downcast<Element>(*topBlockquote).cloneElementWithoutChildren(document());
    insertNodeAfter(clonedBlockquote.copyRef(), breakNode);
    
    // Clone startNode's ancestors into the cloned blockquote.
    // On exiting this loop, clonedAncestor is the lowest ancestor
    // that was cloned (i.e. the clone of either ancestors.last()
    // or clonedBlockquote if ancestors is empty).
    RefPtr<Element> clonedAncestor = clonedBlockquote.copyRef();
    for (size_t i = ancestors.size(); i != 0; --i) {
        auto clonedChild = ancestors[i - 1]->cloneElementWithoutChildren(document());
        // Preserve list item numbering in cloned lists.
        if (clonedChild->isElementNode() && clonedChild->hasTagName(olTag)) {
            Node* listChildNode = i > 1 ? ancestors[i - 2].get() : startNode;
            // The first child of the cloned list might not be a list item element, 
            // find the first one so that we know where to start numbering.
            while (listChildNode && !listChildNode->hasTagName(liTag))
                listChildNode = listChildNode->nextSibling();
            if (listChildNode && is<RenderListItem>(listChildNode->renderer()))
                setNodeAttribute(clonedChild, startAttr, AtomString::number(downcast<RenderListItem>(*listChildNode->renderer()).value()));
        }
            
        appendNode(clonedChild.copyRef(), clonedAncestor.releaseNonNull());
        clonedAncestor = WTFMove(clonedChild);
    }

    moveRemainingSiblingsToNewParent(startNode, 0, *clonedAncestor);

    if (!ancestors.isEmpty()) {
        // Split the tree up the ancestor chain until the topBlockquote
        // Throughout this loop, clonedParent is the clone of ancestor's parent.
        // This is so we can clone ancestor's siblings and place the clones
        // into the clone corresponding to the ancestor's parent.
        RefPtr<Element> ancestor;
        RefPtr<Element> clonedParent;
        for (ancestor = ancestors.first(), clonedParent = clonedAncestor->parentElement();
            ancestor && ancestor != topBlockquote;
            ancestor = ancestor->parentElement(), clonedParent = clonedParent->parentElement()) {
            if (!clonedParent)
                break;
            moveRemainingSiblingsToNewParent(ancestor->nextSibling(), 0, *clonedParent);
        }

        // If the startNode's original parent is now empty, remove it
        Node* originalParent = ancestors.first().get();
        if (!originalParent->hasChildNodes())
            removeNode(*originalParent);
    }
    
    // Make sure the cloned block quote renders.
    addBlockPlaceholderIfNeeded(clonedBlockquote.ptr());

    // Put the selection right before br or at the first position in div.
    auto beforeBROrFirstPositionInDiv = isAtomicNode(breakNode.ptr()) ? positionBeforeNode(breakNode.ptr()) : firstPositionInNode(breakNode.ptr());
    setEndingSelection(VisibleSelection(beforeBROrFirstPositionInDiv, Affinity::Downstream, endingSelection().isDirectional()));
    rebalanceWhitespace();
}

} // namespace WebCore
