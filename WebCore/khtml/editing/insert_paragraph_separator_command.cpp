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
#include "insert_paragraph_separator_command.h"

#include "DocumentImpl.h"
#include "KWQLogging.h"
#include "css_computedstyle.h"
#include "dom_elementimpl.h"
#include "TextImpl.h"
#include "htmlediting.h"
#include "htmlnames.h"
#include "visible_position.h"
#include "visible_units.h"
#include <kxmlcore/Assertions.h>

namespace WebCore {

using namespace HTMLNames;

InsertParagraphSeparatorCommand::InsertParagraphSeparatorCommand(DocumentImpl *document) 
    : CompositeEditCommand(document)
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
    
    m_style = styleAtPosition(pos);
}

void InsertParagraphSeparatorCommand::applyStyleAfterInsertion()
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (!m_style)
        return;

    CSSComputedStyleDeclarationImpl endingStyle(endingSelection().start().node());
    endingStyle.diff(m_style.get());
    if (m_style->length() > 0)
        applyStyle(m_style.get());
}

void InsertParagraphSeparatorCommand::doApply()
{
    bool splitText = false;
    Selection selection = endingSelection();
    if (selection.isNone())
        return;
    
    Position pos = selection.start();
    EAffinity affinity = selection.affinity();
        
    // Delete the current selection.
    if (selection.isRange()) {
        calculateStyleBeforeInsertion(pos);
        deleteSelection(false, false);
        pos = endingSelection().start();
        affinity = endingSelection().affinity();
    }

    pos = positionAvoidingSpecialElementBoundary(pos);

    calculateStyleBeforeInsertion(pos);

    // Find the start block.
    NodeImpl *startNode = pos.node();
    NodeImpl *startBlock = startNode->enclosingBlockFlowElement();
    if (!startBlock || !startBlock->parentNode())
        return;

    VisiblePosition visiblePos(pos, affinity);
    bool isFirstInBlock = isStartOfBlock(visiblePos);
    bool isLastInBlock = isEndOfBlock(visiblePos);
    bool startBlockIsRoot = startBlock == startBlock->rootEditableElement();

    // This is the block that is going to be inserted.
    RefPtr<NodeImpl> blockToInsert = startBlockIsRoot
        ? static_pointer_cast<NodeImpl>(createDefaultParagraphElement(document()))
        : startBlock->cloneNode(false);

    //---------------------------------------------------------------------
    // Handle empty block case.
    if (isFirstInBlock && isLastInBlock) {
        LOG(Editing, "insert paragraph separator: empty block case");
        if (startBlockIsRoot) {
            RefPtr<NodeImpl> extraBlock = createDefaultParagraphElement(document());
            appendNode(extraBlock.get(), startBlock);
            appendBlockPlaceholder(extraBlock.get());
            appendNode(blockToInsert.get(), startBlock);
        }
        else {
            insertNodeAfter(blockToInsert.get(), startBlock);
        }
        appendBlockPlaceholder(blockToInsert.get());
        setEndingSelection(Position(blockToInsert.get(), 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        return;
    }

    //---------------------------------------------------------------------
    // Handle case when position is in the last visible position in its block. 
    if (isLastInBlock) {
        LOG(Editing, "insert paragraph separator: last in block case");
        if (startBlockIsRoot)
            appendNode(blockToInsert.get(), startBlock);
        else
            insertNodeAfter(blockToInsert.get(), startBlock);
        appendBlockPlaceholder(blockToInsert.get());
        setEndingSelection(Position(blockToInsert.get(), 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        return;
    }

    //---------------------------------------------------------------------
    // Handle case when position is in the first visible position in its block.
    // and similar case where upstream position is in another block.
    bool prevInDifferentBlock = !inSameBlock(visiblePos, visiblePos.previous());

    if (prevInDifferentBlock || isFirstInBlock) {
        LOG(Editing, "insert paragraph separator: first in block case");
        pos = pos.downstream();
        pos = positionOutsideContainingSpecialElement(pos);
        Position refPos;
        NodeImpl *refNode;
        if (isFirstInBlock && !startBlockIsRoot) {
            refNode = startBlock;
        } else if (pos.node() == startBlock && startBlockIsRoot) {
            ASSERT(startBlock->childNode(pos.offset())); // must be true or we'd be in the end of block case
            refNode = startBlock->childNode(pos.offset());
        } else {
            refNode = pos.node();
        }

        insertNodeBefore(blockToInsert.get(), refNode);
        appendBlockPlaceholder(blockToInsert.get());
        setEndingSelection(Position(blockToInsert.get(), 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        setEndingSelection(pos, DOWNSTREAM);
        return;
    }

    //---------------------------------------------------------------------
    // Handle the (more complicated) general case,

    LOG(Editing, "insert paragraph separator: general case");

    // If pos.node() is a <br> and the document is in quirks mode, this <br>
    // will collapse away when we add a block after it. Add an extra <br>.
    if (!document()->inStrictMode()) {
        Position upstreamPos = pos.upstream();
        if (upstreamPos.node()->hasTagName(brTag))
            insertNodeAfter(createBreakElement(document()).get(), upstreamPos.node());
    }
    
    // Move downstream. Typing style code will take care of carrying along the 
    // style of the upstream position.
    pos = pos.downstream();
    startNode = pos.node();

    // Build up list of ancestors in between the start node and the start block.
    QPtrList<NodeImpl> ancestors;
    if (startNode != startBlock) {
        for (NodeImpl *n = startNode->parentNode(); n && n != startBlock; n = n->parentNode())
            ancestors.prepend(n);
    }

    // Make sure we do not cause a rendered space to become unrendered.
    // FIXME: We need the affinity for pos, but pos.downstream() does not give it
    Position leadingWhitespace = pos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY);
    if (leadingWhitespace.isNotNull()) {
        TextImpl *textNode = static_cast<TextImpl *>(leadingWhitespace.node());
        replaceTextInNode(textNode, leadingWhitespace.offset(), 1, nonBreakingSpaceString());
    }
    
    // Split at pos if in the middle of a text node.
    if (startNode->isTextNode()) {
        TextImpl *textNode = static_cast<TextImpl *>(startNode);
        bool atEnd = (unsigned)pos.offset() >= textNode->length();
        if (pos.offset() > 0 && !atEnd) {
            splitTextNode(textNode, pos.offset());
            pos = Position(startNode, 0);
            splitText = true;
        }
    }

    // Put the added block in the tree.
    if (startBlockIsRoot) {
        appendNode(blockToInsert.get(), startBlock);
    } else {
        insertNodeAfter(blockToInsert.get(), startBlock);
    }

    // Make clones of ancestors in between the start node and the start block.
    RefPtr<NodeImpl> parent = blockToInsert;
    for (QPtrListIterator<NodeImpl> it(ancestors); it.current(); ++it) {
        RefPtr<NodeImpl> child = it.current()->cloneNode(false); // shallow clone
        appendNode(child.get(), parent.get());
        parent = child.release();
    }

    // Insert a block placeholder if the next visible position is in a different paragraph,
    // because we know that there will be no content on the first line of the new block 
    // before the first block child. So, we need the placeholder to "hold the first line open".
    VisiblePosition next = visiblePos.next();
    if (!next.isNull() && !inSameBlock(visiblePos, next))
        appendBlockPlaceholder(blockToInsert.get());

    // Move the start node and the siblings of the start node.
    if (startNode != startBlock) {
        NodeImpl *n = startNode;
        if (pos.offset() >= startNode->caretMaxOffset()) {
            n = startNode->nextSibling();
        }
        while (n && n != blockToInsert) {
            NodeImpl *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent.get());
            n = next;
        }
    }            

    // Move everything after the start node.
    NodeImpl *leftParent = ancestors.last();
    while (leftParent && leftParent != startBlock) {
        parent = parent->parentNode();
        NodeImpl *n = leftParent->nextSibling();
        while (n && n != blockToInsert) {
            NodeImpl *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent.get());
            n = next;
        }
        leftParent = leftParent->parentNode();
    }

    // Handle whitespace that occurs after the split
    if (splitText) {
        updateLayout();
        pos = Position(startNode, 0);
        if (!pos.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            ASSERT(startNode && startNode->isTextNode());
            deleteInsignificantTextDownstream(pos);
            insertTextIntoNode(static_cast<TextImpl *>(startNode), 0, nonBreakingSpaceString());
        }
    }

    setEndingSelection(Position(blockToInsert.get(), 0), DOWNSTREAM);
    rebalanceWhitespace();
    applyStyleAfterInsertion();
}

} // namespace WebCore
