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
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "Frame.h"
#include "Logging.h"
#include "CSSComputedStyleDeclaration.h"
#include "htmlediting.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "markup.h"
#include "ReplaceSelectionCommand.h"
#include "Text.h"
#include "TextIterator.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

static bool isTableRow(const Node* node)
{
    return node && node->hasTagName(trTag);
}

static bool isTableCellEmpty(Node* cell)
{
    ASSERT(isTableCell(cell));
    VisiblePosition firstInCell(Position(cell, 0));
    VisiblePosition lastInCell(Position(cell, maxDeepOffset(cell)));
    return firstInCell == lastInCell;
}

static bool isTableRowEmpty(Node* row)
{
    if (!isTableRow(row))
        return false;
        
    for (Node* child = row->firstChild(); child; child = child->nextSibling())
        if (isTableCell(child) && !isTableCellEmpty(child))
            return false;
    
    return true;
}

DeleteSelectionCommand::DeleteSelectionCommand(Document *document, bool smartDelete, bool mergeBlocksAfterDelete, bool replace, bool expandForSpecialElements)
    : CompositeEditCommand(document), 
      m_hasSelectionToDelete(false), 
      m_smartDelete(smartDelete), 
      m_mergeBlocksAfterDelete(mergeBlocksAfterDelete),
      m_replace(replace),
      m_expandForSpecialElements(expandForSpecialElements),
      m_startBlock(0),
      m_endBlock(0),
      m_typingStyle(0),
      m_deleteIntoBlockquoteStyle(0)
{
}

DeleteSelectionCommand::DeleteSelectionCommand(const Selection& selection, bool smartDelete, bool mergeBlocksAfterDelete, bool replace, bool expandForSpecialElements)
    : CompositeEditCommand(selection.start().node()->document()), 
      m_hasSelectionToDelete(true), 
      m_smartDelete(smartDelete), 
      m_mergeBlocksAfterDelete(mergeBlocksAfterDelete),
      m_replace(replace),
      m_expandForSpecialElements(expandForSpecialElements),
      m_selectionToDelete(selection),
      m_startBlock(0),
      m_endBlock(0),
      m_typingStyle(0),
      m_deleteIntoBlockquoteStyle(0)
{
}

void DeleteSelectionCommand::initializeStartEnd(Position& start, Position& end)
{
    Node* startSpecialContainer = 0;
    Node* endSpecialContainer = 0;
 
    start = m_selectionToDelete.start();
    end = m_selectionToDelete.end();
 
    // For HRs, we'll get a position at (HR,1) when hitting delete from the beginning of the previous line, or (HR,0) when forward deleting,
    // but in these cases, we want to delete it, so manually expand the selection
    if (start.node()->hasTagName(hrTag))
        start = Position(start.node(), 0);
    else if (end.node()->hasTagName(hrTag))
        end = Position(end.node(), 1);
    
    // FIXME: This is only used so that moveParagraphs can avoid the bugs in special element expanion.
    if (!m_expandForSpecialElements)
        return;
    
    while (VisiblePosition(start) == m_selectionToDelete.visibleStart() && VisiblePosition(end) == m_selectionToDelete.visibleEnd()) {
        startSpecialContainer = 0;
        endSpecialContainer = 0;
    
        Position s = positionBeforeContainingSpecialElement(start, &startSpecialContainer);
        Position e = positionAfterContainingSpecialElement(end, &endSpecialContainer);
        
        if (!startSpecialContainer && !endSpecialContainer)
            break;
        
        // If we're going to expand to include the startSpecialContainer, it must be fully selected.
        if (startSpecialContainer && !endSpecialContainer && Range::compareBoundaryPoints(positionAfterNode(startSpecialContainer), end) > -1)
            break;

        // If we're going to expand to include the endSpecialContainer, it must be fully selected.         
        if (endSpecialContainer && !startSpecialContainer && Range::compareBoundaryPoints(start, positionBeforeNode(endSpecialContainer)) > -1)
            break;
        
        if (startSpecialContainer && startSpecialContainer->isDescendantOf(endSpecialContainer))
            // Don't adjust the end yet, it is the end of a special element that contains the start
            // special element (which may or may not be fully selected).
            start = s;
        else if (endSpecialContainer && endSpecialContainer->isDescendantOf(startSpecialContainer))
            // Don't adjust the start yet, it is the start of a special element that contains the end
            // special element (which may or may not be fully selected).
            end = e;
        else {
            start = s;
            end = e;
        }
    }
}

void DeleteSelectionCommand::initializePositionData()
{
    Position start, end;
    initializeStartEnd(start, end);
    
    m_upstreamStart = start.upstream();
    m_downstreamStart = start.downstream();
    m_upstreamEnd = end.upstream();
    m_downstreamEnd = end.downstream();
    
    m_startRoot = editableRootForPosition(start);
    m_endRoot = editableRootForPosition(end);
    
    m_startTableRow = enclosingNodeOfType(start, &isTableRow);
    m_endTableRow = enclosingNodeOfType(end, &isTableRow);
    
    Node* startCell = enclosingTableCell(m_upstreamStart);
    Node* endCell = enclosingTableCell(m_downstreamEnd);
    // Don't move content out of a table cell.
    // FIXME: This isn't right.  A borderless table with two rows and a single column would appear as two paragraphs.
    if (endCell && endCell != startCell)
        m_mergeBlocksAfterDelete = false;
    
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
    m_startBlock = m_downstreamStart.node()->enclosingBlockFlowOrTableElement();
    m_endBlock = m_upstreamEnd.node()->enclosingBlockFlowOrTableElement();
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
        return true;
    }

    // Not a special-case delete per se, but we can detect that the merging of content between blocks
    // should not be done.
    if (upstreamStartIsBR && downstreamStartIsBR) {
        m_mergeBlocksAfterDelete = false;
        m_endingPosition = m_downstreamEnd;
    }
    
    return false;
}

static void updatePositionForNodeRemoval(Node* node, Position& position)
{
    if (position.isNull())
        return;
    if (node->parent() == position.node() && node->nodeIndex() < (unsigned)position.offset())
        position = Position(position.node(), position.offset() - 1);
    if (position.node() == node || position.node()->isDescendantOf(node))
        position = positionBeforeNode(node);
}

void DeleteSelectionCommand::removeNode(Node *node)
{
    if (!node)
        return;
        
    if (m_startRoot != m_endRoot && !(node->isDescendantOf(m_startRoot.get()) && node->isDescendantOf(m_endRoot.get()))) {
        // If a node is not in both the start and end editable roots, remove it only if its inside an editable region.
        if (!node->parentNode()->isContentEditable()) {
            // Don't remove non-editable atomic nodes.
            if (!node->firstChild())
                return;
            // Search this non-editable region for editable regions to empty.
            RefPtr<Node> child = node->firstChild();
            while (child) {
                RefPtr<Node> nextChild = child->nextSibling();
                removeNode(child.get());
                // Bail if nextChild is no longer node's child.
                if (nextChild && nextChild->parentNode() != node)
                    return;
                child = nextChild;
            }
            
            // Don't remove editable regions that are inside non-editable ones, just clear them.
            return;
        }
    }
    
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
    
    if (node == m_startBlock && !isEndOfBlock(VisiblePosition(m_startBlock.get(), 0, DOWNSTREAM).previous()))
        m_needPlaceholder = true;
    else if (node == m_endBlock && !isStartOfBlock(VisiblePosition(m_endBlock.get(), maxDeepOffset(m_endBlock.get()), DOWNSTREAM).next()))
        m_needPlaceholder = true;
    
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
    
    // Never remove the start block unless it's a table, in which case we won't merge content in.
    if (startNode == m_startBlock && startOffset == 0 && canHaveChildrenForEditing(startNode) && !startNode->hasTagName(tableTag)) {
        startOffset = 0;
        startNode = startNode->traverseNextNode();
    }

    if (startOffset >= caretMaxOffset(startNode) && startNode->isTextNode()) {
        Text *text = static_cast<Text *>(startNode);
        if (text->length() > (unsigned)caretMaxOffset(startNode))
            deleteTextFromNode(text, caretMaxOffset(startNode), text->length() - caretMaxOffset(startNode));
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
        bool startNodeWasDescendantOfEndNode = m_upstreamStart.node()->isDescendantOf(m_downstreamEnd.node());
        // The selection to delete spans more than one node.
        RefPtr<Node> node(startNode);
        
        if (startOffset > 0) {
            if (startNode->isTextNode()) {
                // in a text node that needs to be trimmed
                Text *text = static_cast<Text *>(node.get());
                deleteTextFromNode(text, startOffset, text->length() - startOffset);
                node = node->traverseNextNode();
            } else {
                node = startNode->childNode(startOffset);
            }
        }
        
        // handle deleting all nodes that are completely selected
        while (node && node != m_downstreamEnd.node()) {
            if (Range::compareBoundaryPoints(Position(node.get(), 0), m_downstreamEnd) >= 0) {
                // traverseNextSibling just blew past the end position, so stop deleting
                node = 0;
            } else if (!m_downstreamEnd.node()->isDescendantOf(node.get())) {
                RefPtr<Node> nextNode = node->traverseNextSibling();
                // if we just removed a node from the end container, update end position so the
                // check above will work
                if (node->parentNode() == m_downstreamEnd.node()) {
                    ASSERT(node->nodeIndex() < (unsigned)m_downstreamEnd.offset());
                    m_downstreamEnd = Position(m_downstreamEnd.node(), m_downstreamEnd.offset() - 1);
                }
                removeNode(node.get());
                node = nextNode.get();
            } else {
                Node* n = node->lastDescendant();
                if (m_downstreamEnd.node() == n && m_downstreamEnd.offset() >= caretMaxOffset(n)) {
                    removeNode(node.get());
                    node = 0;
                } else
                    node = node->traverseNextNode();
            }
        }
        
        if (m_downstreamEnd.node() != startNode && !m_upstreamStart.node()->isDescendantOf(m_downstreamEnd.node()) && m_downstreamEnd.node()->inDocument() && m_downstreamEnd.offset() >= caretMinOffset(m_downstreamEnd.node())) {
            if (m_downstreamEnd.offset() >= maxDeepOffset(m_downstreamEnd.node()) && !canHaveChildrenForEditing(m_downstreamEnd.node())) {
                // The node itself is fully selected, not just its contents.  Delete it.
                removeNode(m_downstreamEnd.node());
            } else {
                if (m_downstreamEnd.node()->isTextNode()) {
                    // in a text node that needs to be trimmed
                    Text *text = static_cast<Text *>(m_downstreamEnd.node());
                    if (m_downstreamEnd.offset() > 0) {
                        deleteTextFromNode(text, 0, m_downstreamEnd.offset());
                        m_downstreamEnd = Position(text, 0);
                    }
                // Remove children of m_downstreamEnd.node() that come after m_upstreamStart.
                // Don't try to remove children if m_upstreamStart was inside m_downstreamEnd.node()
                // and m_upstreamStart has been removed from the document, because then we don't 
                // know how many children to remove.
                // FIXME: Make m_upstreamStart a position we update as we remove content, then we can
                // always know which children to remove.
                } else if (!(startNodeWasDescendantOfEndNode && !m_upstreamStart.node()->inDocument())) {
                    int offset = 0;
                    if (m_upstreamStart.node()->isDescendantOf(m_downstreamEnd.node())) {
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
        Text* textNode = static_cast<Text*>(m_leadingWhitespace.node());
        ASSERT(!textNode->renderer() || textNode->renderer()->style()->collapseWhiteSpace());
        replaceTextInNode(textNode, m_leadingWhitespace.offset(), 1, nonBreakingSpaceString());
    }
    if (m_trailingWhitespace.isNotNull() && !m_trailingWhitespace.isRenderedCharacter()) {
        Text* textNode = static_cast<Text*>(m_trailingWhitespace.node());
        ASSERT(!textNode->renderer() ||textNode->renderer()->style()->collapseWhiteSpace());
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
        
    VisiblePosition startOfParagraphToMove(m_downstreamEnd);
    VisiblePosition mergeDestination(m_upstreamStart);
    
    // m_downstreamEnd's block has been emptied out by deletion.  There is no content inside of it to
    // move, so just remove it.
    Element* endBlock = static_cast<Element*>(enclosingBlock(m_downstreamEnd.node()));
    if (!startOfParagraphToMove.deepEquivalent().node() || !endBlock->contains(startOfParagraphToMove.deepEquivalent().node())) {
        removeNode(enclosingBlock(m_downstreamEnd.node()));
        return;
    }
    
    // We need to merge into m_upstreamStart's block, but it's been emptied out and collapsed by deletion.
    if (!mergeDestination.deepEquivalent().node() || !mergeDestination.deepEquivalent().node()->isDescendantOf(m_upstreamStart.node()->enclosingBlockFlowElement())) {
        insertNodeAt(createBreakElement(document()).get(), m_upstreamStart);
        mergeDestination = VisiblePosition(m_upstreamStart);
    }
    
    if (mergeDestination == startOfParagraphToMove)
        return;
        
    VisiblePosition endOfParagraphToMove = endOfParagraph(startOfParagraphToMove);
    
    if (mergeDestination == endOfParagraphToMove)
        return;
    
    // The rule for merging into an empty block is: only do so if its farther to the right.
    // FIXME: Consider RTL.
    // FIXME: handleSpecialCaseBRDelete prevents us from getting here in a case like <ul><li>foo<br><br></li></ul>^foo
    if (isStartOfParagraph(mergeDestination) && 
        startOfParagraphToMove.deepEquivalent().node()->renderer()->caretRect(startOfParagraphToMove.deepEquivalent().offset()).location().x() >
        mergeDestination.deepEquivalent().node()->renderer()->caretRect(startOfParagraphToMove.deepEquivalent().offset()).location().x()) {
        ASSERT(mergeDestination.deepEquivalent().downstream().node()->hasTagName(brTag));
        removeNodeAndPruneAncestors(mergeDestination.deepEquivalent().downstream().node());
        m_endingPosition = startOfParagraphToMove.deepEquivalent();
        return;
    }
    
    RefPtr<Range> range = new Range(document(), rangeCompliantEquivalent(startOfParagraphToMove.deepEquivalent()), rangeCompliantEquivalent(endOfParagraphToMove.deepEquivalent()));
    RefPtr<Range> rangeToBeReplaced = new Range(document(), rangeCompliantEquivalent(mergeDestination.deepEquivalent()), rangeCompliantEquivalent(mergeDestination.deepEquivalent()));
    if (!document()->frame()->editor()->client()->shouldMoveRangeAfterDelete(range.get(), rangeToBeReplaced.get()))
        return;
    
    // moveParagraphs will insert placeholders if it removes blocks that would require their use, don't let block
    // removals that it does cause the insertion of *another* placeholder.
    bool needPlaceholder = m_needPlaceholder;
    moveParagraph(startOfParagraphToMove, endOfParagraphToMove, mergeDestination);
    m_needPlaceholder = needPlaceholder;
    // The endingPosition was likely clobbered by the move, so recompute it (moveParagraph selects the moved paragraph).
    m_endingPosition = endingSelection().start();
}

void DeleteSelectionCommand::removePreviouslySelectedEmptyTableRows()
{
    if (m_endTableRow && m_endTableRow->inDocument() && m_endTableRow != m_startTableRow) {
        Node* row = m_endTableRow->previousSibling();
        while (row && row != m_startTableRow) {
            RefPtr<Node> previousRow = row->previousSibling();
            if (isTableRowEmpty(row))
                // Use a raw removeNode, instead of DeleteSelectionCommand's, because
                // that won't remove rows, it only empties them in preparation for this function.
                CompositeEditCommand::removeNode(row);
            row = previousRow.get();
        }
    }
    
    // Remove empty rows after the start row.
    if (m_startTableRow && m_startTableRow->inDocument() && m_startTableRow != m_endTableRow) {
        Node* row = m_startTableRow->nextSibling();
        while (row && row != m_endTableRow) {
            RefPtr<Node> nextRow = row->nextSibling();
            if (isTableRowEmpty(row))
                CompositeEditCommand::removeNode(row);
            row = nextRow.get();
        }
    }
    
    if (m_endTableRow && m_endTableRow->inDocument() && m_endTableRow != m_startTableRow)
        if (isTableRowEmpty(m_endTableRow.get())) {
            // Don't remove m_endTableRow if it's where we're putting the ending selection.
            if (!m_endingPosition.node()->isDescendantOf(m_endTableRow.get())) {
                // FIXME: We probably shouldn't remove m_endTableRow unless it's fully selected, even if it is empty.
                // We'll need to start adjusting the selection endpoints during deletion to know whether or not m_endTableRow
                // was fully selected here.
                CompositeEditCommand::removeNode(m_endTableRow.get());
            }
        }
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

void DeleteSelectionCommand::saveFullySelectedAnchor()
{
    // If we're deleting an entire anchor element, save it away so that it can be restored
    // when the user begins entering text.
    VisiblePosition visibleStart = m_selectionToDelete.visibleStart();
    VisiblePosition visibleEnd = m_selectionToDelete.visibleEnd();
    Node* startAnchor = enclosingNodeWithTag(visibleStart.deepEquivalent().downstream(), aTag);
    Node* endAnchor = enclosingNodeWithTag(visibleEnd.deepEquivalent().upstream(), aTag);

    Node* beforeStartAnchor = enclosingNodeWithTag(visibleStart.previous().deepEquivalent().downstream(), aTag);
    Node* afterEndAnchor = enclosingNodeWithTag(visibleEnd.next().deepEquivalent().upstream(), aTag);

    if (startAnchor && startAnchor == endAnchor && startAnchor != beforeStartAnchor && endAnchor != afterEndAnchor)
        document()->frame()->editor()->setRemovedAnchor(startAnchor->cloneNode(false));
}

void DeleteSelectionCommand::doApply()
{
    // If selection has not been set to a custom selection when the command was created,
    // use the current ending selection.
    if (!m_hasSelectionToDelete)
        m_selectionToDelete = endingSelection();
    
    if (!m_selectionToDelete.isRange())
        return;

    // If the deletion is occurring in a text field, and we're not deleting to replace the selection, then let the frame call across the bridge to notify the form delegate. 
    if (!m_replace) {
        Node* startNode = m_selectionToDelete.start().node();
        Node* ancestorNode = startNode ? startNode->shadowAncestorNode() : 0;
        if (ancestorNode && ancestorNode->hasTagName(inputTag)
                && static_cast<HTMLInputElement*>(ancestorNode)->isTextField()
                && ancestorNode->focused())
            document()->frame()->textWillBeDeletedInTextField(static_cast<Element*>(ancestorNode));
    }

    // save this to later make the selection with
    EAffinity affinity = m_selectionToDelete.affinity();
    
    Position downstreamEnd = m_selectionToDelete.end().downstream();
    m_needPlaceholder = isStartOfParagraph(m_selectionToDelete.visibleStart()) &&
                        isEndOfParagraph(m_selectionToDelete.visibleEnd()) &&
                        !lineBreakExistsAtPosition(m_selectionToDelete.visibleEnd());
    if (m_needPlaceholder) {
        // Don't need a placeholder when deleting a selection that starts just before a table
        // and ends inside it (we do need placeholders to hold open empty cells, but that's
        // handled elsewhere).
        if (Node* table = isLastPositionBeforeTable(m_selectionToDelete.visibleStart()))
            if (m_selectionToDelete.end().node()->isDescendantOf(table))
                m_needPlaceholder = false;
    }
        
    
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
    
    saveFullySelectedAnchor();
    
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
    
    removePreviouslySelectedEmptyTableRows();
    
    RefPtr<Node> placeholder = m_needPlaceholder ? createBreakElement(document()).get() : 0;
    
    if (placeholder)
        insertNodeAt(placeholder.get(), m_endingPosition);

    rebalanceWhitespaceAt(m_endingPosition);

    calculateTypingStyleAfterDelete(placeholder.get());
    
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
