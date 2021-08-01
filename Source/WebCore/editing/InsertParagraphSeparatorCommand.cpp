/*
 * Copyright (C) 2005, 2006 Apple Inc.  All rights reserved.
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
#include "InsertParagraphSeparatorCommand.h"

#include "Document.h"
#include "Editing.h"
#include "EditingStyle.h"
#include "HTMLBRElement.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "InsertLineBreakCommand.h"
#include "NodeTraversal.h"
#include "RenderText.h"
#include "Text.h"
#include "VisibleUnits.h"

namespace WebCore {

using namespace HTMLNames;

// When inserting a new line, we want to avoid nesting empty divs if we can.  Otherwise, when
// pasting, it's easy to have each new line be a div deeper than the previous.  E.g., in the case
// below, we want to insert at ^ instead of |.
// <div>foo<div>bar</div>|</div>^
static Element* highestVisuallyEquivalentDivBelowRoot(Element* startBlock)
{
    Element* curBlock = startBlock;
    // We don't want to return a root node (if it happens to be a div, e.g., in a document fragment) because there are no
    // siblings for us to append to.
    while (!curBlock->nextSibling() && curBlock->parentElement()->hasTagName(divTag) && curBlock->parentElement()->parentElement()) {
        if (curBlock->parentElement()->hasAttributes())
            break;
        curBlock = curBlock->parentElement();
    }
    return curBlock;
}

InsertParagraphSeparatorCommand::InsertParagraphSeparatorCommand(Document& document, bool mustUseDefaultParagraphElement, bool pasteBlockqutoeIntoUnquotedArea, EditAction editingAction)
    : CompositeEditCommand(document, editingAction)
    , m_mustUseDefaultParagraphElement(mustUseDefaultParagraphElement)
    , m_pasteBlockqutoeIntoUnquotedArea(pasteBlockqutoeIntoUnquotedArea)
{
}

bool InsertParagraphSeparatorCommand::preservesTypingStyle() const
{
    return true;
}

void InsertParagraphSeparatorCommand::calculateStyleBeforeInsertion(const Position& position)
{
    // It is only important to set a style to apply later if we're at the boundaries of
    // a paragraph. Otherwise, content that is moved as part of the work of the command
    // will lend their styles to the new paragraph without any extra work needed.
    auto visiblePosition = VisiblePosition { position };
    if (!isStartOfParagraph(visiblePosition) && !isEndOfParagraph(visiblePosition))
        return;

    m_style = EditingStyle::create(position, EditingStyle::EditingPropertiesInEffect);
    m_style->mergeTypingStyle(*position.document());
}

void InsertParagraphSeparatorCommand::applyStyleAfterInsertion(Node* originalEnclosingBlock)
{
    // Not only do we break out of header tags, but we also do not preserve the typing style,
    // in order to match other browsers.
    if (originalEnclosingBlock->hasTagName(h1Tag) ||
        originalEnclosingBlock->hasTagName(h2Tag) ||
        originalEnclosingBlock->hasTagName(h3Tag) ||
        originalEnclosingBlock->hasTagName(h4Tag) ||
        originalEnclosingBlock->hasTagName(h5Tag))
        return;

    if (!m_style)
        return;

    m_style->prepareToApplyAt(endingSelection().start());
    if (!m_style->isEmpty())
        applyStyle(m_style.get());
}

bool InsertParagraphSeparatorCommand::shouldUseDefaultParagraphElement(Node* enclosingBlock) const
{
    if (m_mustUseDefaultParagraphElement)
        return true;
    
    // Assumes that if there was a range selection, it was already deleted.
    if (!isEndOfBlock(endingSelection().visibleStart()))
        return false;

    return enclosingBlock->hasTagName(h1Tag) ||
           enclosingBlock->hasTagName(h2Tag) ||
           enclosingBlock->hasTagName(h3Tag) ||
           enclosingBlock->hasTagName(h4Tag) ||
           enclosingBlock->hasTagName(h5Tag);
}

void InsertParagraphSeparatorCommand::getAncestorsInsideBlock(const Node* insertionNode, Element* outerBlock, Vector<RefPtr<Element>>& ancestors)
{
    ancestors.clear();
    
    // Build up list of ancestors elements between the insertion node and the outer block.
    if (insertionNode != outerBlock) {
        for (Element* n = insertionNode->parentElement(); n && n != outerBlock; n = n->parentElement())
            ancestors.append(n);
    }
}

Ref<Element> InsertParagraphSeparatorCommand::cloneHierarchyUnderNewBlock(const Vector<RefPtr<Element>>& ancestors, Ref<Element>&& blockToInsert)
{
    // Make clones of ancestors in between the start node and the start block.
    RefPtr<Element> parent = WTFMove(blockToInsert);
    for (size_t i = ancestors.size(); i != 0; --i) {
        auto child = ancestors[i - 1]->cloneElementWithoutChildren(document());
        // It should always be okay to remove id from the cloned elements, since the originals are not deleted.
        child->removeAttribute(idAttr);
        appendNode(child.copyRef(), parent.releaseNonNull());
        parent = WTFMove(child);
    }
    
    return parent.releaseNonNull();
}

void InsertParagraphSeparatorCommand::doApply()
{
    if (endingSelection().isNoneOrOrphaned())
        return;
    
    auto insertionPosition = endingSelection().start();
    auto affinity = endingSelection().affinity();

    // Delete the current selection.
    if (endingSelection().isRange()) {
        calculateStyleBeforeInsertion(insertionPosition);
        deleteSelection(false, true);
        insertionPosition = endingSelection().start();
        affinity = endingSelection().affinity();
    }
    
    // FIXME: The parentAnchoredEquivalent conversion needs to be moved into enclosingBlock.
    RefPtr<Element> startBlock = enclosingBlock(insertionPosition.parentAnchoredEquivalent().containerNode());
    Position canonicalPos = VisiblePosition(insertionPosition).deepEquivalent();
    if (!startBlock
        || !startBlock->nonShadowBoundaryParentNode()
        || isRenderedTable(startBlock.get())
        || isTableCell(startBlock.get())
        || is<HTMLFormElement>(*startBlock)
        // FIXME: If the node is hidden, we don't have a canonical position so we will do the wrong thing for tables and <hr>. https://bugs.webkit.org/show_bug.cgi?id=40342
        || (!canonicalPos.isNull() && canonicalPos.deprecatedNode()->renderer() && canonicalPos.deprecatedNode()->renderer()->isTable())
        || (!canonicalPos.isNull() && canonicalPos.deprecatedNode()->hasTagName(hrTag))) {
        applyCommandToComposite(InsertLineBreakCommand::create(document()));
        return;
    }
    
    // Use the leftmost candidate.
    insertionPosition = insertionPosition.upstream();
    if (!insertionPosition.isCandidate())
        insertionPosition = insertionPosition.downstream();

    // Adjust the insertion position after the delete
    insertionPosition = positionAvoidingSpecialElementBoundary(insertionPosition);
    VisiblePosition visiblePos(insertionPosition, affinity);
    if (visiblePos.isNull())
        return;

    if (!startBlock->contains(visiblePos.deepEquivalent().containerNode()))
        return;

    calculateStyleBeforeInsertion(insertionPosition);

    //---------------------------------------------------------------------
    // Handle special case of typing return on an empty list item
    if (breakOutOfEmptyListItem())
        return;

    //---------------------------------------------------------------------
    // Prepare for more general cases.

    bool isFirstInBlock = isStartOfBlock(visiblePos);
    bool isLastInBlock = isEndOfBlock(visiblePos);
    bool nestNewBlock = false;

    // FIXME: <http://webkit.org/b/211864> If insertionPosition is not editable, we should compute a position that is.
    if (!isEditablePosition(insertionPosition))
        return;

    // Create block to be inserted.
    RefPtr<Element> blockToInsert;
    if (startBlock->isRootEditableElement()) {
        blockToInsert = createDefaultParagraphElement(document());
        nestNewBlock = true;
    } else if (shouldUseDefaultParagraphElement(startBlock.get())) 
        blockToInsert = createDefaultParagraphElement(document());
    else
        blockToInsert = startBlock->cloneElementWithoutChildren(document());

    //---------------------------------------------------------------------
    // Handle case when position is in the last visible position in its block,
    // including when the block is empty. 
    if (isLastInBlock) {
        if (nestNewBlock) {
            if (isFirstInBlock && !lineBreakExistsAtVisiblePosition(visiblePos)) {
                // The block is empty.  Create an empty block to
                // represent the paragraph that we're leaving.
                auto extraBlock = createDefaultParagraphElement(document());
                appendNode(extraBlock.copyRef(), *startBlock);
                if (!appendBlockPlaceholder(WTFMove(extraBlock)))
                    return;
            }
            appendNode(*blockToInsert, *startBlock);
        } else {
            // We can get here if we pasted a copied portion of a blockquote with a newline at the end and are trying to paste it
            // into an unquoted area. We then don't want the newline within the blockquote or else it will also be quoted.
            if (m_pasteBlockqutoeIntoUnquotedArea) {
                if (Node* highestBlockquote = highestEnclosingNodeOfType(canonicalPos, &isMailBlockquote))
                    startBlock = downcast<Element>(highestBlockquote);
            }

            // Most of the time we want to stay at the nesting level of the startBlock (e.g., when nesting within lists).  However,
            // for div nodes, this can result in nested div tags that are hard to break out of.
            Element* siblingNode = startBlock.get();
            if (blockToInsert->hasTagName(divTag))
                siblingNode = highestVisuallyEquivalentDivBelowRoot(startBlock.get());
            insertNodeAfter(*blockToInsert, *siblingNode);
        }

        // Recreate the same structure in the new paragraph.
        
        Vector<RefPtr<Element>> ancestors;
        getAncestorsInsideBlock(positionOutsideTabSpan(insertionPosition).deprecatedNode(), startBlock.get(), ancestors);      
        auto parent = cloneHierarchyUnderNewBlock(ancestors, *blockToInsert);
        auto* parentPtr = parent.ptr();
        
        if (!appendBlockPlaceholder(WTFMove(parent)))
            return;

        setEndingSelection(VisibleSelection(firstPositionInNode(parentPtr), Affinity::Downstream, endingSelection().isDirectional()));
        return;
    }
    

    //---------------------------------------------------------------------
    // Handle case when position is in the first visible position in its block, and
    // similar case where previous position is in another, presumeably nested, block.
    if (isFirstInBlock || !inSameBlock(visiblePos, visiblePos.previous())) {
        Node *refNode;
        
        insertionPosition = positionOutsideTabSpan(insertionPosition);

        if (isFirstInBlock && !nestNewBlock)
            refNode = startBlock.get();
        else if (isFirstInBlock && nestNewBlock) {
            // startBlock should always have children, otherwise isLastInBlock would be true and it's handled above.
            ASSERT(startBlock->firstChild());
            refNode = startBlock->firstChild();
        } else if (insertionPosition.containerNode() == startBlock && nestNewBlock) {
            refNode = startBlock->traverseToChildAt(insertionPosition.computeOffsetInContainerNode());
            ASSERT(refNode); // must be true or we'd be in the end of block case
        } else
            refNode = insertionPosition.deprecatedNode();

        // find ending selection position easily before inserting the paragraph
        insertionPosition = insertionPosition.downstream();
        
        insertNodeBefore(*blockToInsert, *refNode);

        // Recreate the same structure in the new paragraph.

        Vector<RefPtr<Element>> ancestors;
        getAncestorsInsideBlock(positionAvoidingSpecialElementBoundary(positionOutsideTabSpan(insertionPosition)).deprecatedNode(), startBlock.get(), ancestors);

        auto parent = cloneHierarchyUnderNewBlock(ancestors, *blockToInsert);
        if (!appendBlockPlaceholder(WTFMove(parent)))
            return;
        
        // In this case, we need to set the new ending selection.
        setEndingSelection(VisibleSelection(insertionPosition, Affinity::Downstream, endingSelection().isDirectional()));
        return;
    }

    //---------------------------------------------------------------------
    // Handle the (more complicated) general case,

    // All of the content in the current block after visiblePos is
    // about to be wrapped in a new paragraph element.  Add a br before 
    // it if visiblePos is at the start of a paragraph so that the 
    // content will move down a line.
    if (isStartOfParagraph(visiblePos)) {
        auto br = HTMLBRElement::create(document());
        insertNodeAt(br.copyRef(), insertionPosition);
        if (!br->parentNode())
            return;
        insertionPosition = positionInParentAfterNode(br.ptr());
        // If the insertion point is a break element, there is nothing else
        // we need to do.
        if (auto* renderer = visiblePos.deepEquivalent().anchorNode()->renderer(); renderer && renderer->isBR()) {
            setEndingSelection(VisibleSelection(insertionPosition, Affinity::Downstream, endingSelection().isDirectional()));
            return;
        }
    }
    
    // Move downstream. Typing style code will take care of carrying along the 
    // style of the upstream position.
    insertionPosition = insertionPosition.downstream();

    // At this point, the insertionPosition's node could be a container, and we want to make sure we include
    // all of the correct nodes when building the ancestor list.  So this needs to be the deepest representation of the position
    // before we walk the DOM tree.
    insertionPosition = positionOutsideTabSpan(VisiblePosition(insertionPosition).deepEquivalent());
    if (insertionPosition.isNull())
        return;

    // If the returned position lies either at the end or at the start of an element that is ignored by editing
    // we should move to its upstream or downstream position.
    if (editingIgnoresContent(*insertionPosition.deprecatedNode())) {
        if (insertionPosition.atLastEditingPositionForNode())
            insertionPosition = insertionPosition.downstream();
        else if (insertionPosition.atFirstEditingPositionForNode())
            insertionPosition = insertionPosition.upstream();
    }

    // Make sure we do not cause a rendered space to become unrendered.
    // FIXME: We need the affinity for pos, but pos.downstream() does not give it
    Position leadingWhitespace = insertionPosition.leadingWhitespacePosition(VisiblePosition::defaultAffinity);
    // FIXME: leadingWhitespacePosition is returning the position before preserved newlines for positions
    // after the preserved newline, causing the newline to be turned into a nbsp.
    if (is<Text>(leadingWhitespace.deprecatedNode())) {
        Text& textNode = downcast<Text>(*leadingWhitespace.deprecatedNode());
        ASSERT(!textNode.renderer() || textNode.renderer()->style().collapseWhiteSpace());
        replaceTextInNodePreservingMarkers(textNode, leadingWhitespace.deprecatedEditingOffset(), 1, nonBreakingSpaceString());
    }
    
    // Split at pos if in the middle of a text node.
    Position positionAfterSplit;
    if (insertionPosition.anchorType() == Position::PositionIsOffsetInAnchor && is<Text>(*insertionPosition.containerNode())) {
        Ref<Text> textNode = downcast<Text>(*insertionPosition.containerNode());
        bool atEnd = static_cast<unsigned>(insertionPosition.offsetInContainerNode()) >= textNode->length();
        if (insertionPosition.deprecatedEditingOffset() > 0 && !atEnd) {
            splitTextNode(textNode, insertionPosition.offsetInContainerNode());
            positionAfterSplit = firstPositionInNode(textNode.ptr());
            if (!textNode->previousSibling())
                return; // Bail out if mutation events detachd the split text node.
            insertionPosition.moveToPosition(textNode->previousSibling(), insertionPosition.offsetInContainerNode());
            visiblePos = VisiblePosition(insertionPosition);
        }
    }

    // If we got detached due to mutation events, just bail out.
    if (!startBlock->parentNode())
        return;

    // Put the added block in the tree.
    if (nestNewBlock)
        appendNode(*blockToInsert, *startBlock);
    else
        insertNodeAfter(*blockToInsert, *startBlock);

    document().updateLayoutIgnorePendingStylesheets();

    // If the paragraph separator was inserted at the end of a paragraph, an empty line must be
    // created.  All of the nodes, starting at visiblePos, are about to be added to the new paragraph 
    // element.  If the first node to be inserted won't be one that will hold an empty line open, add a br.
    if (isEndOfParagraph(visiblePos) && !lineBreakExistsAtVisiblePosition(visiblePos))
        appendNode(HTMLBRElement::create(document()), *blockToInsert);

    // Move the start node and the siblings of the start node.
    if (VisiblePosition(insertionPosition) != VisiblePosition(positionBeforeNode(blockToInsert.get()))) {
        Node* n;
        if (insertionPosition.containerNode() == startBlock)
            n = insertionPosition.computeNodeAfterPosition();
        else {
            Node* splitTo = insertionPosition.containerNode();
            if (is<Text>(*splitTo) && insertionPosition.offsetInContainerNode() >= caretMaxOffset(*splitTo))
                splitTo = NodeTraversal::next(*splitTo, startBlock.get());
            ASSERT(splitTo);
            splitTreeToNode(*splitTo, *startBlock);

            for (n = startBlock->firstChild(); n; n = n->nextSibling()) {
                if (VisiblePosition(insertionPosition) <= VisiblePosition(positionBeforeNode(n)))
                    break;
            }
        }

        moveRemainingSiblingsToNewParent(n, blockToInsert.get(), *blockToInsert);
    }            

    // Handle whitespace that occurs after the split
    if (positionAfterSplit.isNotNull()) {
        document().updateLayoutIgnorePendingStylesheets();
        if (!positionAfterSplit.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            ASSERT(!positionAfterSplit.containerNode()->renderer() || positionAfterSplit.containerNode()->renderer()->style().collapseWhiteSpace());
            deleteInsignificantTextDownstream(positionAfterSplit);
            if (is<Text>(*positionAfterSplit.deprecatedNode()))
                insertTextIntoNode(downcast<Text>(*positionAfterSplit.containerNode()), 0, nonBreakingSpaceString());
        }
    }

    setEndingSelection(VisibleSelection(firstPositionInNode(blockToInsert.get()), Affinity::Downstream, endingSelection().isDirectional()));
    applyStyleAfterInsertion(startBlock.get());
}

} // namespace WebCore
