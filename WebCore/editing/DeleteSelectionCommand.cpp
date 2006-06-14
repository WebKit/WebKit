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
#include "DeleteSelectionCommand.h"

#include "Document.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "Frame.h"
#include "Logging.h"
#include "CSSComputedStyleDeclaration.h"
#include "htmlediting.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "markup.h"
#include "ReplaceSelectionCommand.h"
#include "TextIterator.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

DeleteSelectionCommand::DeleteSelectionCommand(Document *document, bool smartDelete, bool mergeBlocksAfterDelete)
    : CompositeEditCommand(document), 
      m_hasSelectionToDelete(false), 
      m_smartDelete(smartDelete), 
      m_mergeBlocksAfterDelete(mergeBlocksAfterDelete),
      m_startBlock(0),
      m_endBlock(0),
      m_typingStyle(0),
      m_deleteIntoBlockquoteStyle(0)
{
}

DeleteSelectionCommand::DeleteSelectionCommand(Document *document, const Selection &selection, bool smartDelete, bool mergeBlocksAfterDelete)
    : CompositeEditCommand(document), 
      m_hasSelectionToDelete(true), 
      m_smartDelete(smartDelete), 
      m_mergeBlocksAfterDelete(mergeBlocksAfterDelete),
      m_selectionToDelete(selection),
      m_startBlock(0),
      m_endBlock(0),
      m_typingStyle(0),
      m_deleteIntoBlockquoteStyle(0)
{
}

void DeleteSelectionCommand::initializeStartEnd()
 {
    Node* startSpecialContainer = 0;
    Node* endSpecialContainer = 0;
 
    Position start = m_selectionToDelete.start();
    Position end = m_selectionToDelete.end();
 
    while (1) {
        startSpecialContainer = 0;
        endSpecialContainer = 0;
    
        Position s = positionOutsideContainingSpecialElement(start, &startSpecialContainer);
        Position e = positionOutsideContainingSpecialElement(end, &endSpecialContainer);
        
        if (!startSpecialContainer || !endSpecialContainer)
            break;
        
        start = s;
        end = e;
    }
 
    m_upstreamStart = start.upstream();
    m_downstreamStart = start.downstream();
    m_upstreamEnd = end.upstream();
    m_downstreamEnd = end.downstream();
}

void DeleteSelectionCommand::initializePositionData()
{
    initializeStartEnd();
    
    // Usually the start and the end of the selection to delete are pulled together as a result of the deletion.
    // Sometimes they aren't (like when no merge is requested), so we must choose one position to hold the caret 
    // and receive the placeholder after deletion.
    VisiblePosition visibleEnd(m_downstreamEnd);
    if (m_mergeBlocksAfterDelete && !isEndOfParagraph(visibleEnd))
        m_endingPosition = m_downstreamEnd;
    else
        m_endingPosition = m_downstreamStart;
        
    // Handle leading and trailing whitespace, as well as smart delete adjustments to the selection
    m_leadingWhitespace = m_upstreamStart.leadingWhitespacePosition(m_selectionToDelete.affinity());
    m_trailingWhitespace = m_downstreamEnd.trailingWhitespacePosition(VP_DEFAULT_AFFINITY);

    if (m_smartDelete) {
    
        // skip smart delete if the selection to delete already starts or ends with whitespace
        Position pos = VisiblePosition(m_upstreamStart, m_selectionToDelete.affinity()).deepEquivalent();
        bool skipSmartDelete = pos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNotNull();
        if (!skipSmartDelete)
            skipSmartDelete = m_downstreamEnd.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNotNull();

        // extend selection upstream if there is whitespace there
        bool hasLeadingWhitespaceBeforeAdjustment = m_upstreamStart.leadingWhitespacePosition(m_selectionToDelete.affinity(), true).isNotNull();
        if (!skipSmartDelete && hasLeadingWhitespaceBeforeAdjustment) {
            VisiblePosition visiblePos = VisiblePosition(m_upstreamStart, VP_DEFAULT_AFFINITY).previous();
            pos = visiblePos.deepEquivalent();
            // Expand out one character upstream for smart delete and recalculate
            // positions based on this change.
            m_upstreamStart = pos.upstream();
            m_downstreamStart = pos.downstream();
            m_leadingWhitespace = m_upstreamStart.leadingWhitespacePosition(visiblePos.affinity());
        }
        
        // trailing whitespace is only considered for smart delete if there is no leading
        // whitespace, as in the case where you double-click the first word of a paragraph.
        if (!skipSmartDelete && !hasLeadingWhitespaceBeforeAdjustment && m_downstreamEnd.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNotNull()) {
            // Expand out one character downstream for smart delete and recalculate
            // positions based on this change.
            pos = VisiblePosition(m_downstreamEnd, VP_DEFAULT_AFFINITY).next().deepEquivalent();
            m_upstreamEnd = pos.upstream();
            m_downstreamEnd = pos.downstream();
            m_trailingWhitespace = m_downstreamEnd.trailingWhitespacePosition(VP_DEFAULT_AFFINITY);
        }
    }
    
    //
    // Handle setting start and end blocks and the start node.
    //
    m_startBlock = m_downstreamStart.node()->enclosingBlockFlowElement();
    m_endBlock = m_upstreamEnd.node()->enclosingBlockFlowElement();
}

void DeleteSelectionCommand::saveTypingStyleState()
{
    // Figure out the typing style in effect before the delete is done.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    RefPtr<CSSComputedStyleDeclaration> computedStyle = positionBeforeTabSpan(m_selectionToDelete.start()).computedStyle();
    m_typingStyle = computedStyle->copyInheritableProperties();
    
    // If we're deleting into a Mail blockquote, save the style at end() instead of start()
    // We'll use this later in computeTypingStyleAfterDelete if we end up outside of a Mail blockquote
    if (nearestMailBlockquote(m_selectionToDelete.start().node())) {
        computedStyle = m_selectionToDelete.end().computedStyle();
        m_deleteIntoBlockquoteStyle = computedStyle->copyInheritableProperties();
    } else
        m_deleteIntoBlockquoteStyle = 0;
}

bool DeleteSelectionCommand::handleSpecialCaseBRDelete()
{
    // Check for special-case where the selection contains only a BR on a line by itself after another BR.
    bool upstreamStartIsBR = m_upstreamStart.node()->hasTagName(brTag);
    bool downstreamStartIsBR = m_downstreamStart.node()->hasTagName(brTag);
    bool isBROnLineByItself = upstreamStartIsBR && downstreamStartIsBR && m_downstreamStart.node() == m_upstreamEnd.node();
    if (isBROnLineByItself) {
        removeNode(m_downstreamStart.node());
        m_mergeBlocksAfterDelete = false;
        return true;
    }

    // Not a special-case delete per se, but we can detect that the merging of content between blocks
    // should not be done.
    if (upstreamStartIsBR && downstreamStartIsBR)
        m_mergeBlocksAfterDelete = false;

    return false;
}

static void updatePositionForNodeRemoval(Node* node, Position& position)
{
    if (position.isNull())
        return;
    if (node->parent() == position.node() && node->nodeIndex() < (unsigned)position.offset())
        position = Position(position.node(), position.offset() - 1);
    if (position.node() == node || position.node()->isAncestor(node))
        position = positionBeforeNode(node);
}

void DeleteSelectionCommand::removeNode(Node *node)
{
    if (isTableStructureNode(node) || node == node->rootEditableElement()) {
        // Do not remove an element of table structure; remove its contents.
        // Likewise for the root editable element.
        Node *child = node->firstChild();
        while (child) {
            Node *remove = child;
            child = child->nextSibling();
            removeNode(remove);
        }
        
        // make sure empty cell has some height
        updateLayout();
        RenderObject *r = node->renderer();
        if (r && r->isTableCell() && r->contentHeight() <= 0)
            insertBlockPlaceholder(Position(node,0));
        return;
    }
    
    // FIXME: Update the endpoints of the range being deleted.
    updatePositionForNodeRemoval(node, m_endingPosition);
    updatePositionForNodeRemoval(node, m_leadingWhitespace);
    updatePositionForNodeRemoval(node, m_trailingWhitespace);
    
    CompositeEditCommand::removeNode(node);
}


void updatePositionForTextRemoval(Node* node, int offset, int count, Position& position)
{
    if (position.node() == node) {
        if (position.offset() > offset + count)
            position = Position(position.node(), position.offset() - count);
        else if (position.offset() > offset)
            position = Position(position.node(), offset);
    }
}

void DeleteSelectionCommand::deleteTextFromNode(Text *node, int offset, int count)
{
    // FIXME: Update the endpoints of the range being deleted.
    updatePositionForTextRemoval(node, offset, count, m_endingPosition);
    updatePositionForTextRemoval(node, offset, count, m_leadingWhitespace);
    updatePositionForTextRemoval(node, offset, count, m_trailingWhitespace);
    
    CompositeEditCommand::deleteTextFromNode(node, offset, count);
}

void DeleteSelectionCommand::handleGeneralDelete()
{
    int startOffset = m_upstreamStart.offset();
    Node* startNode = m_upstreamStart.node();
    
    // Never remove the start block.
    if (startNode == m_startBlock && startOffset == 0) {
        startOffset = 0;
        startNode = startNode->traverseNextNode();
    }

    if (startOffset >= startNode->caretMaxOffset() && startNode->isTextNode()) {
        Text *text = static_cast<Text *>(startNode);
        if (text->length() > (unsigned)startNode->caretMaxOffset())
            deleteTextFromNode(text, startNode->caretMaxOffset(), text->length() - startNode->caretMaxOffset());
    }

    if (startOffset >= maxDeepOffset(startNode)) {
        startNode = startNode->traverseNextSibling();
        startOffset = 0;
    }

    // Done adjusting the start.  See if we're all done.
    if (!startNode)
        return;

    if (startNode == m_downstreamEnd.node()) {
        // The selection to delete is all in one node.
        if (!startNode->renderer() || 
            (startOffset == 0 && m_downstreamEnd.offset() >= maxDeepOffset(startNode))) {
            // just delete
            removeNode(startNode);
        } else if (m_downstreamEnd.offset() - startOffset > 0) {
            if (startNode->isTextNode()) {
                // in a text node that needs to be trimmed
                Text *text = static_cast<Text *>(startNode);
                deleteTextFromNode(text, startOffset, m_downstreamEnd.offset() - startOffset);
            } else {
                removeChildrenInRange(startNode, startOffset, m_downstreamEnd.offset());
                m_endingPosition = m_upstreamStart;
            }
        }
    }
    else {
        // The selection to delete spans more than one node.
        Node *node = startNode;
        
        if (startOffset > 0) {
            if (startNode->isTextNode()) {
                // in a text node that needs to be trimmed
                Text *text = static_cast<Text *>(node);
                deleteTextFromNode(text, startOffset, text->length() - startOffset);
                node = node->traverseNextNode();
            } else {
                node = startNode->childNode(startOffset);
            }
        }
        
        // handle deleting all nodes that are completely selected
        while (node && node != m_downstreamEnd.node()) {
            if (Range::compareBoundaryPoints(Position(node, 0), m_downstreamEnd) >= 0) {
                // traverseNextSibling just blew past the end position, so stop deleting
                node = 0;
            } else if (!m_downstreamEnd.node()->isAncestor(node)) {
                Node *nextNode = node->traverseNextSibling();
                // if we just removed a node from the end container, update end position so the
                // check above will work
                if (node->parentNode() == m_downstreamEnd.node()) {
                    ASSERT(node->nodeIndex() < (unsigned)m_downstreamEnd.offset());
                    m_downstreamEnd = Position(m_downstreamEnd.node(), m_downstreamEnd.offset() - 1);
                }
                removeNode(node);
                node = nextNode;
            } else {
                Node* n = node->lastDescendant();
                if (m_downstreamEnd.node() == n && m_downstreamEnd.offset() >= n->caretMaxOffset()) {
                    removeNode(node); 
                    node = 0;
                } else {
                    node = node->traverseNextNode();
                }
            }
        }
        
        if (m_downstreamEnd.node() != startNode && !m_upstreamStart.node()->isAncestor(m_downstreamEnd.node()) && m_downstreamEnd.node()->inDocument() && m_downstreamEnd.offset() >= m_downstreamEnd.node()->caretMinOffset()) {
            if (m_downstreamEnd.offset() >= maxDeepOffset(m_downstreamEnd.node())) {
                // need to delete whole node
                // we can get here if this is the last node in the block
                // remove an ancestor of m_downstreamEnd.node(), and thus m_downstreamEnd.node() itself
                if (!m_upstreamStart.node()->inDocument() ||
                    m_upstreamStart.node() == m_downstreamEnd.node() ||
                    m_upstreamStart.node()->isAncestor(m_downstreamEnd.node())) {
                    m_upstreamStart = Position(m_downstreamEnd.node()->parentNode(), m_downstreamEnd.node()->nodeIndex());
                }
                
                removeNode(m_downstreamEnd.node());
            } else {
                if (m_downstreamEnd.node()->isTextNode()) {
                    // in a text node that needs to be trimmed
                    Text *text = static_cast<Text *>(m_downstreamEnd.node());
                    if (m_downstreamEnd.offset() > 0) {
                        deleteTextFromNode(text, 0, m_downstreamEnd.offset());
                        m_downstreamEnd = Position(text, 0);
                    }
                } else {
                    int offset = 0;
                    if (m_upstreamStart.node()->isAncestor(m_downstreamEnd.node())) {
                        Node *n = m_upstreamStart.node();
                        while (n && n->parentNode() != m_downstreamEnd.node())
                            n = n->parentNode();
                        if (n)
                            offset = n->nodeIndex() + 1;
                    }
                    removeChildrenInRange(m_downstreamEnd.node(), offset, m_downstreamEnd.offset());
                    m_downstreamEnd = Position(m_downstreamEnd.node(), offset);
                }
            }
        }
    }
}

void DeleteSelectionCommand::fixupWhitespace()
{
    updateLayout();
    // FIXME: isRenderedCharacter should be removed, and we should use VisiblePosition::characterAfter and VisiblePosition::characterBefore
    if (m_leadingWhitespace.isNotNull() && !m_leadingWhitespace.isRenderedCharacter()) {
        Text *textNode = static_cast<Text *>(m_leadingWhitespace.node());
        replaceTextInNode(textNode, m_leadingWhitespace.offset(), 1, nonBreakingSpaceString());
    }
    if (m_trailingWhitespace.isNotNull() && !m_trailingWhitespace.isRenderedCharacter()) {
        Text *textNode = static_cast<Text *>(m_trailingWhitespace.node());
        replaceTextInNode(textNode, m_trailingWhitespace.offset(), 1, nonBreakingSpaceString());
    }
}

// If a selection starts in one block and ends in another, we have to merge to bring content before the
// start together with content after the end.
void DeleteSelectionCommand::mergeParagraphs()
{
    if (!m_mergeBlocksAfterDelete)
        return;

    // FIXME: Deletion should adjust selection endpoints as it removes nodes so that we never get into this state (4099839).
    if (!m_downstreamEnd.node()->inDocument() || !m_upstreamStart.node()->inDocument())
         return;
         
    // FIXME: The deletion algorithm shouldn't let this happen.
    if (Range::compareBoundaryPoints(m_upstreamStart, m_downstreamEnd) > 0)
        return;
        
    // FIXME: Merging will always be unnecessary in this case, but we really bail here because this is a case where
    // deletion commonly fails to adjust its endpoints, which would cause the visible position comparison below to false negative.
    if (m_endBlock == m_startBlock)
        return;
        
    // Don't move content between parts of a table or between table and non-table content.
    // FIXME: This isn't right.  A table with two rows and a single column appears as two paragraphs.
    if (isTableStructureNode(m_downstreamEnd.node()->enclosingBlockFlowElement()) || isTableStructureNode(m_upstreamStart.node()->enclosingBlockFlowElement()))
        return;
        
    VisiblePosition startOfParagraphToMove(m_downstreamEnd);
    VisiblePosition mergeDestination(m_upstreamStart);
    
    // We need to merge into m_upstreamStart's block, but it's been emptied out and collapsed by deletion.
    if (!mergeDestination.deepEquivalent().node()->isAncestor(m_upstreamStart.node()->enclosingBlockFlowElement())) {
        insertNodeAt(createBreakElement(document()).get(), m_upstreamStart.node(), m_upstreamStart.offset());
        mergeDestination = VisiblePosition(m_upstreamStart);
    }
    
    if (mergeDestination == startOfParagraphToMove)
        return;
        
    VisiblePosition endOfParagraphToMove = endOfParagraph(startOfParagraphToMove);
    
    if (mergeDestination == endOfParagraphToMove)
        return;
    
    moveParagraph(startOfParagraphToMove, endOfParagraphToMove, mergeDestination);
    // The endingPosition was likely clobbered by the move, so recompute it (moveParagraph selects the moved paragraph).
    m_endingPosition = endingSelection().start();
}

void DeleteSelectionCommand::calculateTypingStyleAfterDelete(Node *insertedPlaceholder)
{
    // Compute the difference between the style before the delete and the style now
    // after the delete has been done. Set this style on the frame, so other editing
    // commands being composed with this one will work, and also cache it on the command,
    // so the Frame::appliedEditing can set it after the whole composite command 
    // has completed.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    
    // If we deleted into a blockquote, but are now no longer in a blockquote, use the alternate typing style
    if (m_deleteIntoBlockquoteStyle && !nearestMailBlockquote(m_endingPosition.node()))
        m_typingStyle = m_deleteIntoBlockquoteStyle;
    m_deleteIntoBlockquoteStyle = 0;
    
    RefPtr<CSSComputedStyleDeclaration> endingStyle = new CSSComputedStyleDeclaration(m_endingPosition.node());
    endingStyle->diff(m_typingStyle.get());
    if (!m_typingStyle->length())
        m_typingStyle = 0;
    if (insertedPlaceholder && m_typingStyle) {
        // Apply style to the placeholder. This makes sure that the single line in the
        // paragraph has the right height, and that the paragraph takes on the style
        // of the preceding line and retains it even if you click away, click back, and
        // then start typing. In this case, the typing style is applied right now, and
        // is not retained until the next typing action.

        setEndingSelection(Selection(Position(insertedPlaceholder, 0), DOWNSTREAM));
        applyStyle(m_typingStyle.get(), EditActionUnspecified);
        m_typingStyle = 0;
    }
    // Set m_typingStyle as the typing style.
    // It's perfectly OK for m_typingStyle to be null.
    document()->frame()->setTypingStyle(m_typingStyle.get());
    setTypingStyle(m_typingStyle.get());
}

void DeleteSelectionCommand::clearTransientState()
{
    m_selectionToDelete = Selection();
    m_upstreamStart.clear();
    m_downstreamStart.clear();
    m_upstreamEnd.clear();
    m_downstreamEnd.clear();
    m_endingPosition.clear();
    m_leadingWhitespace.clear();
    m_trailingWhitespace.clear();
}

void DeleteSelectionCommand::doApply()
{
    // If selection has not been set to a custom selection when the command was created,
    // use the current ending selection.
    if (!m_hasSelectionToDelete)
        m_selectionToDelete = endingSelection();
        
    if (!m_selectionToDelete.isRange())
        return;

    // If the deletion is occurring in a text field, let the frame call across the bridge to notify the form delegate. 
    Node* startNode = m_selectionToDelete.start().node();
    Node* ancestorNode = startNode ? startNode->shadowAncestorNode() : 0;
    if (ancestorNode && ancestorNode->hasTagName(inputTag) && static_cast<HTMLInputElement*>(ancestorNode)->isNonWidgetTextField())
        document()->frame()->textWillBeDeletedInTextField(static_cast<Element*>(ancestorNode));

    // save this to later make the selection with
    EAffinity affinity = m_selectionToDelete.affinity();
    
    Position downstreamEnd = m_selectionToDelete.end().downstream();
    bool needPlaceholder = isStartOfParagraph(m_selectionToDelete.visibleStart()) &&
                           isEndOfParagraph(m_selectionToDelete.visibleEnd()) &&
                           !(downstreamEnd.node()->hasTagName(brTag) && downstreamEnd.offset() == 0);
    
    // set up our state
    initializePositionData();
    if (!m_startBlock || !m_endBlock) {
        // Can't figure out what blocks we're in. This can happen if
        // the document structure is not what we are expecting, like if
        // the document has no body element, or if the editable block
        // has been changed to display: inline. Some day it might
        // be nice to be able to deal with this, but for now, bail.
        clearTransientState();
        return;
    }

    // Delete any text that may hinder our ability to fixup whitespace after the delete
    deleteInsignificantTextDownstream(m_trailingWhitespace);    

    saveTypingStyleState();
    
    // deleting just a BR is handled specially, at least because we do not
    // want to replace it with a placeholder BR!
    if (handleSpecialCaseBRDelete()) {
        calculateTypingStyleAfterDelete(false);
        setEndingSelection(Selection(m_endingPosition, affinity));
        clearTransientState();
        rebalanceWhitespace();
        return;
    }
    
    handleGeneralDelete();
    
    fixupWhitespace();
    
    mergeParagraphs();
    
    RefPtr<Node> placeholder = needPlaceholder ? createBreakElement(document()) : 0;
    if (placeholder)
        insertNodeAt(placeholder.get(), m_endingPosition.node(), m_endingPosition.offset());

    calculateTypingStyleAfterDelete(placeholder.get());
    
    rebalanceWhitespaceAt(m_endingPosition);
    setEndingSelection(Selection(m_endingPosition, affinity));
    clearTransientState();
}

EditAction DeleteSelectionCommand::editingAction() const
{
    // Note that DeleteSelectionCommand is also used when the user presses the Delete key,
    // but in that case there's a TypingCommand that supplies the editingAction(), so
    // the Undo menu correctly shows "Undo Typing"
    return EditActionCut;
}

bool DeleteSelectionCommand::preservesTypingStyle() const
{
    return true;
}

} // namespace WebCore
