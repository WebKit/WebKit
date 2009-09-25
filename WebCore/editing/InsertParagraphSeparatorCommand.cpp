/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "InsertParagraphSeparatorCommand.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InsertLineBreakCommand.h"
#include "Logging.h"
#include "RenderObject.h"
#include "Text.h"
#include "htmlediting.h"
#include "visible_units.h"
#include "ApplyStyleCommand.h"

namespace WebCore {

using namespace HTMLNames;

InsertParagraphSeparatorCommand::InsertParagraphSeparatorCommand(Document *document, bool mustUseDefaultParagraphElement) 
    : CompositeEditCommand(document)
    , m_mustUseDefaultParagraphElement(mustUseDefaultParagraphElement)
{
}

bool InsertParagraphSeparatorCommand::preservesTypingStyle() const
{
    return true;
}

void InsertParagraphSeparatorCommand::calculateStyleBeforeInsertion(const Position &pos)
{
    // It is only important to set a style to apply later if we're at the boundaries of
    // a paragraph. Otherwise, content that is moved as part of the work of the command
    // will lend their styles to the new paragraph without any extra work needed.
    VisiblePosition visiblePos(pos, VP_DEFAULT_AFFINITY);
    if (!isStartOfParagraph(visiblePos) && !isEndOfParagraph(visiblePos))
        return;
    
    m_style = editingStyleAtPosition(pos, IncludeTypingStyle);
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
    
    prepareEditingStyleToApplyAt(m_style.get(), endingSelection().start());

    if (m_style->length() > 0)
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

void InsertParagraphSeparatorCommand::getAncestorsInsideBlock(const Node* insertionNode, Element* outerBlock, Vector<Element*>& ancestors)
{
    ancestors.clear();
    
    // Build up list of ancestors elements between the insertion node and the outer block.
    if (insertionNode != outerBlock) {
        for (Element* n = insertionNode->parentElement(); n && n != outerBlock; n = n->parentElement())
            ancestors.append(n);
    }
}

PassRefPtr<Element> InsertParagraphSeparatorCommand::cloneHierarchyUnderNewBlock(const Vector<Element*>& ancestors, PassRefPtr<Element> blockToInsert)
{
    // Make clones of ancestors in between the start node and the start block.
    RefPtr<Element> parent = blockToInsert;
    for (size_t i = ancestors.size(); i != 0; --i) {
        RefPtr<Element> child = ancestors[i - 1]->cloneElementWithoutChildren();
        appendNode(child, parent);
        parent = child.release();
    }
    
    return parent.release();
}

void InsertParagraphSeparatorCommand::doApply()
{
    bool splitText = false;
    if (endingSelection().isNone())
        return;
    
    Position insertionPosition = endingSelection().start();
        
    EAffinity affinity = endingSelection().affinity();
        
    // Delete the current selection.
    if (endingSelection().isRange()) {
        calculateStyleBeforeInsertion(insertionPosition);
        deleteSelection(false, true);
        insertionPosition = endingSelection().start();
        affinity = endingSelection().affinity();
    }
    
    // FIXME: The rangeCompliantEquivalent conversion needs to be moved into enclosingBlock.
    Node* startBlockNode = enclosingBlock(rangeCompliantEquivalent(insertionPosition).node());
    Position canonicalPos = VisiblePosition(insertionPosition).deepEquivalent();
    Element* startBlock = static_cast<Element*>(startBlockNode);
    if (!startBlockNode
            || !startBlockNode->isElementNode()
            || !startBlock->parentNode()
            || isTableCell(startBlock)
            || startBlock->hasTagName(formTag)
            || (canonicalPos.node()->renderer() && canonicalPos.node()->renderer()->isTable())
            || canonicalPos.node()->hasTagName(hrTag)) {
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

    // Create block to be inserted.
    RefPtr<Element> blockToInsert;
    if (startBlock == startBlock->rootEditableElement()) {
        blockToInsert = createDefaultParagraphElement(document());
        nestNewBlock = true;
    } else if (shouldUseDefaultParagraphElement(startBlock)) 
        blockToInsert = createDefaultParagraphElement(document());
    else
        blockToInsert = startBlock->cloneElementWithoutChildren();

    //---------------------------------------------------------------------
    // Handle case when position is in the last visible position in its block,
    // including when the block is empty. 
    if (isLastInBlock) {
        bool shouldApplyStyleAfterInsertion = true;
        if (nestNewBlock) {
            if (isFirstInBlock && !lineBreakExistsAtVisiblePosition(visiblePos)) {
                // The block is empty.  Create an empty block to
                // represent the paragraph that we're leaving.
                RefPtr<Element> extraBlock = createDefaultParagraphElement(document());
                appendNode(extraBlock, startBlock);
                appendBlockPlaceholder(extraBlock);
            }
            appendNode(blockToInsert, startBlock);
        } else {
            // We can get here if we pasted a copied portion of a blockquote with a newline at the end and are trying to paste it
            // into an unquoted area. We then don't want the newline within the blockquote or else it will also be quoted.
            if (Node* highestBlockquote = highestEnclosingNodeOfType(canonicalPos, &isMailBlockquote)) {
                startBlock = static_cast<Element*>(highestBlockquote);
                // When inserting the newline after the blockquote, we don't want to apply the original style after the insertion
                shouldApplyStyleAfterInsertion = false;
            }
            insertNodeAfter(blockToInsert, startBlock);
        }

        // Recreate the same structure in the new paragraph.
        
        Vector<Element*> ancestors;
        getAncestorsInsideBlock(insertionPosition.node(), startBlock, ancestors);      
        RefPtr<Element> parent = cloneHierarchyUnderNewBlock(ancestors, blockToInsert);
        
        appendBlockPlaceholder(parent);

        setEndingSelection(VisibleSelection(Position(parent.get(), 0), DOWNSTREAM));
        return;
    }
    

    //---------------------------------------------------------------------
    // Handle case when position is in the first visible position in its block, and
    // similar case where previous position is in another, presumeably nested, block.
    if (isFirstInBlock || !inSameBlock(visiblePos, visiblePos.previous())) {
        Node *refNode;
        if (isFirstInBlock && !nestNewBlock)
            refNode = startBlock;
        else if (insertionPosition.node() == startBlock && nestNewBlock) {
            refNode = startBlock->childNode(insertionPosition.deprecatedEditingOffset());
            ASSERT(refNode); // must be true or we'd be in the end of block case
        } else
            refNode = insertionPosition.node();

        // find ending selection position easily before inserting the paragraph
        insertionPosition = insertionPosition.downstream();
        
        insertNodeBefore(blockToInsert, refNode);

        // Recreate the same structure in the new paragraph.

        Vector<Element*> ancestors;
        getAncestorsInsideBlock(positionAvoidingSpecialElementBoundary(insertionPosition).node(), startBlock, ancestors);
        
        appendBlockPlaceholder(cloneHierarchyUnderNewBlock(ancestors, blockToInsert));
        
        // In this case, we need to set the new ending selection.
        setEndingSelection(VisibleSelection(insertionPosition, DOWNSTREAM));
        return;
    }

    //---------------------------------------------------------------------
    // Handle the (more complicated) general case,

    // All of the content in the current block after visiblePos is
    // about to be wrapped in a new paragraph element.  Add a br before 
    // it if visiblePos is at the start of a paragraph so that the 
    // content will move down a line.
    if (isStartOfParagraph(visiblePos)) {
        RefPtr<Element> br = createBreakElement(document());
        insertNodeAt(br.get(), insertionPosition);
        insertionPosition = positionInParentAfterNode(br.get());
    }
    
    // Move downstream. Typing style code will take care of carrying along the 
    // style of the upstream position.
    insertionPosition = insertionPosition.downstream();

    // At this point, the insertionPosition's node could be a container, and we want to make sure we include
    // all of the correct nodes when building the ancestor list.  So this needs to be the deepest representation of the position
    // before we walk the DOM tree.
    insertionPosition = VisiblePosition(insertionPosition).deepEquivalent();

    // Build up list of ancestors in between the start node and the start block.
    Vector<Element*> ancestors;
    getAncestorsInsideBlock(insertionPosition.node(), startBlock, ancestors);

    // Make sure we do not cause a rendered space to become unrendered.
    // FIXME: We need the affinity for pos, but pos.downstream() does not give it
    Position leadingWhitespace = insertionPosition.leadingWhitespacePosition(VP_DEFAULT_AFFINITY);
    // FIXME: leadingWhitespacePosition is returning the position before preserved newlines for positions
    // after the preserved newline, causing the newline to be turned into a nbsp.
    if (leadingWhitespace.isNotNull()) {
        Text* textNode = static_cast<Text*>(leadingWhitespace.node());
        ASSERT(!textNode->renderer() || textNode->renderer()->style()->collapseWhiteSpace());
        replaceTextInNode(textNode, leadingWhitespace.deprecatedEditingOffset(), 1, nonBreakingSpaceString());
    }
    
    // Split at pos if in the middle of a text node.
    if (insertionPosition.node()->isTextNode()) {
        Text* textNode = static_cast<Text*>(insertionPosition.node());
        bool atEnd = (unsigned)insertionPosition.deprecatedEditingOffset() >= textNode->length();
        if (insertionPosition.deprecatedEditingOffset() > 0 && !atEnd) {
            splitTextNode(textNode, insertionPosition.deprecatedEditingOffset());
            insertionPosition.moveToOffset(0);
            visiblePos = VisiblePosition(insertionPosition);
            splitText = true;
        }
    }

    // Put the added block in the tree.
    if (nestNewBlock)
        appendNode(blockToInsert.get(), startBlock);
    else
        insertNodeAfter(blockToInsert.get(), startBlock);

    updateLayout();
    
    // Make clones of ancestors in between the start node and the outer block.
    RefPtr<Element> parent = cloneHierarchyUnderNewBlock(ancestors, blockToInsert);

    // If the paragraph separator was inserted at the end of a paragraph, an empty line must be
    // created.  All of the nodes, starting at visiblePos, are about to be added to the new paragraph 
    // element.  If the first node to be inserted won't be one that will hold an empty line open, add a br.
    if (isEndOfParagraph(visiblePos) && !lineBreakExistsAtVisiblePosition(visiblePos))
        appendNode(createBreakElement(document()).get(), blockToInsert.get());
        
    // Move the start node and the siblings of the start node.
    if (insertionPosition.node() != startBlock) {
        Node* n = insertionPosition.node();
        if (insertionPosition.deprecatedEditingOffset() >= caretMaxOffset(n))
            n = n->nextSibling();

        while (n && n != blockToInsert) {
            Node *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent.get());
            n = next;
        }
    }            

    // Move everything after the start node.
    if (!ancestors.isEmpty()) {
        Element* leftParent = ancestors.first();
        while (leftParent && leftParent != startBlock) {
            parent = parent->parentElement();
            if (!parent)
                break;
            Node* n = leftParent->nextSibling();
            while (n && n != blockToInsert) {
                Node* next = n->nextSibling();
                removeNode(n);
                appendNode(n, parent.get());
                n = next;
            }
            leftParent = leftParent->parentElement();
        }
    }

    // Handle whitespace that occurs after the split
    if (splitText) {
        updateLayout();
        insertionPosition = Position(insertionPosition.node(), 0);
        if (!insertionPosition.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            ASSERT(insertionPosition.node()->isTextNode());
            ASSERT(!insertionPosition.node()->renderer() || insertionPosition.node()->renderer()->style()->collapseWhiteSpace());
            deleteInsignificantTextDownstream(insertionPosition);
            insertTextIntoNode(static_cast<Text*>(insertionPosition.node()), 0, nonBreakingSpaceString());
        }
    }

    setEndingSelection(VisibleSelection(Position(blockToInsert.get(), 0), DOWNSTREAM));
    applyStyleAfterInsertion(startBlock);
}

} // namespace WebCore
