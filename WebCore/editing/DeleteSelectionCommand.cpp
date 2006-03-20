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
#include "Frame.h"
#include "Logging.h"
#include "CSSComputedStyleDeclaration.h"
#include "Range.h"
#include "Position.h"
#include "htmlediting.h"
#include "htmlnames.h"
#include "render_line.h"
#include "RenderObject.h"
#include "VisiblePosition.h"
#include "TextIterator.h"
#include "visible_units.h"
#include <kxmlcore/Assertions.h>

namespace WebCore {

using namespace HTMLNames;

static void debugPosition(const char *prefix, const Position &pos)
{
    if (!prefix)
        prefix = "";
    if (pos.isNull())
        LOG(Editing, "%s <null>", prefix);
    else
        LOG(Editing, "%s%s %p : %d", prefix, pos.node()->nodeName().deprecatedString().latin1(), pos.node(), pos.offset());
}

static void debugNode(const char *prefix, const Node *node)
{
    if (!prefix)
        prefix = "";
    if (!node)
        LOG(Editing, "%s <null>", prefix);
    else
        LOG(Editing, "%s%s %p", prefix, node->nodeName().deprecatedString().latin1(), node);
}

#if 1
static Position positionBeforePossibleContainingSpecialElement(const Position &pos, Node **containingSpecialElement)
 {
    if (isFirstVisiblePositionInSpecialElement(pos))
        return positionBeforeContainingSpecialElement(pos, containingSpecialElement);
 
     return pos;
 }
 
static Position positionAfterPossibleContainingSpecialElement(const Position &pos, Node **containingSpecialElement)
 {
    if (isLastVisiblePositionInSpecialElement(pos))
        return positionAfterContainingSpecialElement(pos, containingSpecialElement);
 
     return pos;
 }
#endif

DeleteSelectionCommand::DeleteSelectionCommand(Document *document, bool smartDelete, bool mergeBlocksAfterDelete)
    : CompositeEditCommand(document), 
      m_hasSelectionToDelete(false), 
      m_smartDelete(smartDelete), 
      m_mergeBlocksAfterDelete(mergeBlocksAfterDelete),
      m_startBlock(0),
      m_endBlock(0),
      m_startNode(0),
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
      m_startNode(0),
      m_typingStyle(0),
      m_deleteIntoBlockquoteStyle(0)
{
}

void DeleteSelectionCommand::initializeStartEnd()
 {
#if 0
    Position start = m_selectionToDelete.start();
    Position end = m_selectionToDelete.end();
    m_upstreamStart = start.upstream();
    m_downstreamStart = start.downstream();
    m_upstreamEnd = end.upstream();
    m_downstreamEnd = end.downstream();
#else
    Node *startSpecialContainer = 0;
    Node *endSpecialContainer = 0;
    
    Position start = positionOutsideContainingSpecialElement(m_selectionToDelete.start(), &startSpecialContainer);
    Position end   = positionOutsideContainingSpecialElement(m_selectionToDelete.end(), &endSpecialContainer);
    
    m_upstreamStart = positionBeforePossibleContainingSpecialElement(start.upstream(), &startSpecialContainer);
    m_downstreamStart = positionBeforePossibleContainingSpecialElement(start.downstream(), 0);
    m_upstreamEnd = positionAfterPossibleContainingSpecialElement(end.upstream(), 0);
    m_downstreamEnd = positionAfterPossibleContainingSpecialElement(end.downstream(), &endSpecialContainer);
 
    // do not adjust start/end if one of them did not adjust, and the selection is entirely within the special element
    if (m_upstreamStart == m_selectionToDelete.start().upstream() || m_downstreamEnd == m_selectionToDelete.end().downstream()) {
        if (m_downstreamEnd.node()->isAncestor(startSpecialContainer) || m_upstreamStart.node()->isAncestor(endSpecialContainer)) {
            start = m_selectionToDelete.start();
            end = m_selectionToDelete.end();
            m_upstreamStart = start.upstream();
            m_downstreamStart = start.downstream();
            m_upstreamEnd = end.upstream();
            m_downstreamEnd = end.downstream();
        }
    }
#endif
}

void DeleteSelectionCommand::initializePositionData()
{
    //
    // Handle setting some basic positions
    //
    initializeStartEnd();
    
    //
    // Handle leading and trailing whitespace, as well as smart delete adjustments to the selection
    //
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
    
    m_trailingWhitespaceValid = true;
    
    //
    // Handle setting start and end blocks and the start node.
    //
    m_startBlock = m_downstreamStart.node()->enclosingBlockFlowElement();
    m_endBlock = m_upstreamEnd.node()->enclosingBlockFlowElement();
    m_startNode = m_upstreamStart.node();

    //
    // Handle detecting if the line containing the selection end is itself fully selected.
    // This is one of the tests that determines if block merging of content needs to be done.
    //
    VisiblePosition visibleEnd(m_downstreamEnd, VP_DEFAULT_AFFINITY);
    if (isEndOfParagraph(visibleEnd)) {
        Position previousLineStart = previousLinePosition(visibleEnd, 0).deepEquivalent();
        if (previousLineStart.isNull() || Range::compareBoundaryPoints(previousLineStart, m_downstreamStart) >= 0)
            m_mergeBlocksAfterDelete = false;
    }

    debugPosition("m_upstreamStart      ", m_upstreamStart);
    debugPosition("m_downstreamStart    ", m_downstreamStart);
    debugPosition("m_upstreamEnd        ", m_upstreamEnd);
    debugPosition("m_downstreamEnd      ", m_downstreamEnd);
    debugPosition("m_leadingWhitespace  ", m_leadingWhitespace);
    debugPosition("m_trailingWhitespace ", m_trailingWhitespace);
    debugNode(    "m_startBlock         ", m_startBlock.get());
    debugNode(    "m_endBlock           ", m_endBlock.get());    
    debugNode(    "m_startNode          ", m_startNode.get());    
}

void DeleteSelectionCommand::insertPlaceholderForAncestorBlockContent()
{
    // This code makes sure a line does not disappear when deleting in this case:
    // <p>foo</p>bar<p>baz</p>
    // Select "bar" and hit delete. If nothing is done, the line containing bar will disappear.
    // It needs to be held open by inserting a placeholder.
    // Also see:
    // <rdar://problem/3928305> selecting an entire line and typing over causes new inserted text at top of document
    //
    // The checks below detect the case where the selection contains content in an ancestor block 
    // surrounded by child blocks.
    //
    VisiblePosition visibleStart(m_upstreamStart, VP_DEFAULT_AFFINITY);
    VisiblePosition beforeStart = visibleStart.previous();
    Node *startBlock = enclosingBlockFlowElement(visibleStart);
    Node *beforeStartBlock = enclosingBlockFlowElement(beforeStart);
    
    if (!beforeStart.isNull() &&
        !inSameBlock(visibleStart, beforeStart) &&
        beforeStartBlock->isAncestor(startBlock) &&
        startBlock != m_upstreamStart.node()) {

        VisiblePosition visibleEnd(m_downstreamEnd, VP_DEFAULT_AFFINITY);
        VisiblePosition afterEnd = visibleEnd.next();
        
        if ((!afterEnd.isNull() && !inSameBlock(afterEnd, visibleEnd) && !inSameBlock(afterEnd, visibleStart)) ||
            (m_downstreamEnd == m_selectionToDelete.end() && isEndOfParagraph(visibleEnd) && !m_downstreamEnd.node()->hasTagName(brTag))) {
            RefPtr<Node> block = createDefaultParagraphElement(document());
            insertNodeBefore(block.get(), m_upstreamStart.node());
            addBlockPlaceholderIfNeeded(block.get());
            m_endingPosition = Position(block.get(), 0);
        }
    }
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
    bool upstreamStartIsBR = m_startNode->hasTagName(brTag);
    bool downstreamStartIsBR = m_downstreamStart.node()->hasTagName(brTag);
    bool isBROnLineByItself = upstreamStartIsBR && downstreamStartIsBR && m_downstreamStart.node() == m_upstreamEnd.node();
    if (isBROnLineByItself) {
        m_endingPosition = Position(m_downstreamStart.node()->parentNode(), m_downstreamStart.node()->nodeIndex());
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

void DeleteSelectionCommand::handleGeneralDelete()
{
    int startOffset = m_upstreamStart.offset();
    VisiblePosition visibleEnd = VisiblePosition(m_downstreamEnd, m_selectionToDelete.affinity());
    bool endAtEndOfBlock = isEndOfBlock(visibleEnd);

    // Handle some special cases where the selection begins and ends on specific visible units.
    // Sometimes a node that is actually selected needs to be retained in order to maintain
    // user expectations for the delete operation. Here is an example:
    //     1. Open a new Blot or Mail document
    //     2. hit Return ten times or so
    //     3. Type a letter (do not hit Return after it)
    //     4. Type shift-up-arrow to select the line containing the letter and the previous blank line
    //     5. Hit Delete
    // You expect the insertion point to wind up at the start of the line where your selection began.
    // Because of the nature of HTML, the editing code needs to perform a special check to get
    // this behavior. So:
    // If the entire start block is selected, and the selection does not extend to the end of the 
    // end of a block other than the block containing the selection start, then do not delete the 
    // start block, otherwise delete the start block.
    if (startOffset == 1 && m_startNode && m_startNode->hasTagName(brTag)) {
        m_startNode = m_startNode->traverseNextNode();
        startOffset = 0;
    }
    if (m_startBlock != m_endBlock && isStartOfBlock(VisiblePosition(m_upstreamStart, m_selectionToDelete.affinity()))) {
        if (!m_startBlock->isAncestor(m_endBlock.get()) && !isStartOfBlock(visibleEnd) && endAtEndOfBlock) {
            // Delete all the children of the block, but not the block itself.
            // Use traverseNextNode in case the block has no children (e.g. is an empty table cell)
            m_startNode = m_startBlock->traverseNextNode();
            startOffset = 0;
        }
    }  else if (startOffset >= m_startNode->caretMaxOffset() && (isAtomicNode(m_startNode.get()) || startOffset == 0)) {
        // Move the start node to the next node in the tree since the startOffset is equal to
        // or beyond the start node's caretMaxOffset This means there is nothing visible to delete. 
        // But don't do this if the node is not atomic - we don't want to move into the first child.

        // Also, before moving on, delete any insignificant text that may be present in a text node.
        if (m_startNode->isTextNode()) {
            // Delete any insignificant text from this node.
            Text *text = static_cast<Text *>(m_startNode.get());
            if (text->length() > (unsigned)m_startNode->caretMaxOffset())
                deleteTextFromNode(text, m_startNode->caretMaxOffset(), text->length() - m_startNode->caretMaxOffset());
        }
        
        // shift the start node to the next
        m_startNode = m_startNode->traverseNextNode();
        startOffset = 0;
    }

    // Done adjusting the start.  See if we're all done.
    if (!m_startNode)
        return;

    if (m_startNode == m_downstreamEnd.node()) {
        // The selection to delete is all in one node.
        if (!m_startNode->renderer() || 
            (startOffset == 0 && m_downstreamEnd.offset() >= maxDeepOffset(m_startNode.get()))) {
            // just delete
            removeFullySelectedNode(m_startNode.get());
        } else if (m_downstreamEnd.offset() - startOffset > 0) {
            if (m_startNode->isTextNode()) {
                // in a text node that needs to be trimmed
                Text *text = static_cast<Text *>(m_startNode.get());
                deleteTextFromNode(text, startOffset, m_downstreamEnd.offset() - startOffset);
                m_trailingWhitespaceValid = false;
            } else {
                removeChildrenInRange(m_startNode.get(), startOffset, m_downstreamEnd.offset());
                m_endingPosition = m_upstreamStart;
            }
        }
    }
    else {
        // The selection to delete spans more than one node.
        Node *node = m_startNode.get();
        
        if (startOffset > 0) {
            if (m_startNode->isTextNode()) {
                // in a text node that needs to be trimmed
                Text *text = static_cast<Text *>(node);
                deleteTextFromNode(text, startOffset, text->length() - startOffset);
                node = node->traverseNextNode();
            } else {
                node = m_startNode->childNode(startOffset);
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
                removeFullySelectedNode(node);
                node = nextNode;
            } else {
                Node *n = node->lastChild();
                while (n && n->lastChild())
                    n = n->lastChild();
                if (n == m_downstreamEnd.node() && m_downstreamEnd.offset() >= m_downstreamEnd.node()->caretMaxOffset()) {
                    removeFullySelectedNode(node);
                    m_trailingWhitespaceValid = false;
                    node = 0;
                } 
                else {
                    node = node->traverseNextNode();
                }
            }
        }

        
        if (m_downstreamEnd.node() != m_startNode && !m_upstreamStart.node()->isAncestor(m_downstreamEnd.node()) && m_downstreamEnd.node()->inDocument() && m_downstreamEnd.offset() >= m_downstreamEnd.node()->caretMinOffset()) {
            if (m_downstreamEnd.offset() >= maxDeepOffset(m_downstreamEnd.node())) {
                // need to delete whole node
                // we can get here if this is the last node in the block
                // remove an ancestor of m_downstreamEnd.node(), and thus m_downstreamEnd.node() itself
                if (!m_upstreamStart.node()->inDocument() ||
                    m_upstreamStart.node() == m_downstreamEnd.node() ||
                    m_upstreamStart.node()->isAncestor(m_downstreamEnd.node())) {
                    m_upstreamStart = Position(m_downstreamEnd.node()->parentNode(), m_downstreamEnd.node()->nodeIndex());
                }
                
                removeFullySelectedNode(m_downstreamEnd.node());
                m_trailingWhitespaceValid = false;
            } else {
                if (m_downstreamEnd.node()->isTextNode()) {
                    // in a text node that needs to be trimmed
                    Text *text = static_cast<Text *>(m_downstreamEnd.node());
                    if (m_downstreamEnd.offset() > 0) {
                        deleteTextFromNode(text, 0, m_downstreamEnd.offset());
                        m_downstreamEnd = Position(text, 0);
                        m_trailingWhitespaceValid = false;
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

// FIXME: Can't really determine this without taking white-space mode into account.
static inline bool nextCharacterIsCollapsibleWhitespace(const Position &pos)
{
    if (!pos.node())
        return false;
    if (!pos.node()->isTextNode())
        return false;
    return isCollapsibleWhitespace(static_cast<Text *>(pos.node())->data()[pos.offset()]);
}

void DeleteSelectionCommand::fixupWhitespace()
{
    updateLayout();
    if (m_leadingWhitespace.isNotNull() && (m_trailingWhitespace.isNotNull() || !m_leadingWhitespace.isRenderedCharacter())) {
        LOG(Editing, "replace leading");
        Text *textNode = static_cast<Text *>(m_leadingWhitespace.node());
        replaceTextInNode(textNode, m_leadingWhitespace.offset(), 1, nonBreakingSpaceString());
    }
    else if (m_trailingWhitespace.isNotNull()) {
        if (m_trailingWhitespaceValid) {
            if (!m_trailingWhitespace.isRenderedCharacter()) {
                LOG(Editing, "replace trailing [valid]");
                Text *textNode = static_cast<Text *>(m_trailingWhitespace.node());
                replaceTextInNode(textNode, m_trailingWhitespace.offset(), 1, nonBreakingSpaceString());
            }
        }
        else {
            Position pos = m_endingPosition.downstream();
            pos = Position(pos.node(), pos.offset() - 1);
            if (nextCharacterIsCollapsibleWhitespace(pos) && !pos.isRenderedCharacter()) {
                LOG(Editing, "replace trailing [invalid]");
                Text *textNode = static_cast<Text *>(pos.node());
                replaceTextInNode(textNode, pos.offset(), 1, nonBreakingSpaceString());
                // need to adjust ending position since the trailing position is not valid.
                m_endingPosition = pos;
            }
        }
    }
}

// This function moves nodes in the block containing startNode to dstBlock, starting
// from startNode and proceeding to the end of the paragraph. Nodes in the block containing
// startNode that appear in document order before startNode are not moved.
// This function is an important helper for deleting selections that cross paragraph
// boundaries.
void DeleteSelectionCommand::moveNodesAfterNode()
{
    if (!m_mergeBlocksAfterDelete)
        return;

    if (m_endBlock == m_startBlock)
        return;

    Node *startNode = m_downstreamEnd.node();
    Node *dstNode = m_upstreamStart.node();

    if (!startNode->inDocument() || !dstNode->inDocument())
        return;

    Node *startBlock = startNode->enclosingBlockFlowElement();
    if (isTableStructureNode(startBlock)) {
        // Do not move content between parts of a table.
        return;
    }
    
    // Now that we are about to add content, check to see if a placeholder element
    // can be removed.
    removeBlockPlaceholder(startBlock);

    // Move the subtree containing node
    Node *node = startNode->enclosingInlineElement();

    // Insert after the subtree containing dstNode
    Node *refNode = dstNode->enclosingInlineElement();

    // Nothing to do if start is already at the beginning of dstBlock
    Node *dstBlock = refNode->enclosingBlockFlowElement();
    if (startBlock == dstBlock->firstChild())
        return;

    // Do the move.
    Node *rootNode = refNode->rootEditableElement();
    while (node && node->isAncestor(startBlock)) {
        Node *moveNode = node;
        node = node->nextSibling();
        removeNode(moveNode);
        if (moveNode->hasTagName(brTag) && !moveNode->renderer()) {
            // Just remove this node, and don't put it back.
            // If the BR was not rendered (since it was at the end of a block, for instance), 
            // putting it back in the document might make it appear, and that is not desirable.
            break;
        }
        if (refNode == rootNode)
            insertNodeAt(moveNode, refNode, 0);
        else
            insertNodeAfter(moveNode, refNode);
        refNode = moveNode;
        if (moveNode->hasTagName(brTag))
            break;
    }

    // If the startBlock no longer has any kids, we may need to deal with adding a BR
    // to make the layout come out right. Consider this document:
    //
    // One
    // <div>Two</div>
    // Three
    // 
    // Placing the insertion before before the 'T' of 'Two' and hitting delete will
    // move the contents of the div to the block containing 'One' and delete the div.
    // This will have the side effect of moving 'Three' on to the same line as 'One'
    // and 'Two'. This is undesirable. We fix this up by adding a BR before the 'Three'.
    // This may not be ideal, but it is better than nothing.
    updateLayout();
    if (!startBlock->renderer() || startBlock->renderer()->isEmpty()) {
        removeNode(startBlock);
        updateLayout();
        if (refNode->renderer() && refNode->renderer()->inlineBox() && refNode->renderer()->inlineBox()->nextOnLineExists()) {
            insertNodeAfter(createBreakElement(document()).get(), refNode);
        }
    }
}

void DeleteSelectionCommand::calculateEndingPosition()
{
    if (m_endingPosition.isNotNull() && m_endingPosition.node()->inDocument())
        return;

    m_endingPosition = m_upstreamStart;
    if (m_endingPosition.node()->inDocument())
        return;
    
    m_endingPosition = m_downstreamEnd;
    if (m_endingPosition.node()->inDocument())
        return;

    m_endingPosition = Position(m_startBlock.get(), 0);
    if (m_endingPosition.node()->inDocument())
        return;

    m_endingPosition = Position(m_endBlock.get(), 0);
    if (m_endingPosition.node()->inDocument())
        return;

    m_endingPosition = Position(document()->documentElement(), 0);
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

    // save this to later make the selection with
    EAffinity affinity = m_selectionToDelete.affinity();
    
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

    // if all we are deleting is complete paragraph(s), we need to make
    // sure a blank paragraph remains when we are done
    bool forceBlankParagraph = isStartOfParagraph(VisiblePosition(m_upstreamStart, VP_DEFAULT_AFFINITY)) &&
                               isEndOfParagraph(VisiblePosition(m_downstreamEnd, VP_DEFAULT_AFFINITY));

    // Delete any text that may hinder our ability to fixup whitespace after the detele
    deleteInsignificantTextDownstream(m_trailingWhitespace);    

    saveTypingStyleState();
    
    // deleting just a BR is handled specially, at least because we do not
    // want to replace it with a placeholder BR!
    if (handleSpecialCaseBRDelete()) {
        calculateTypingStyleAfterDelete(false);
        debugPosition("endingPosition   ", m_endingPosition);
        setEndingSelection(Selection(m_endingPosition, affinity));
        clearTransientState();
        rebalanceWhitespace();
        return;
    }
    
    insertPlaceholderForAncestorBlockContent();
    handleGeneralDelete();
    
    // Do block merge if start and end of selection are in different blocks.
    moveNodesAfterNode();
    
    calculateEndingPosition();
    fixupWhitespace();

    // if the m_endingPosition is already a blank paragraph, there is
    // no need to force a new one
    if (forceBlankParagraph &&
        isStartOfParagraph(VisiblePosition(m_endingPosition, VP_DEFAULT_AFFINITY)) &&
        isEndOfParagraph(VisiblePosition(m_endingPosition, VP_DEFAULT_AFFINITY))) {
        forceBlankParagraph = false;
    }
    
    Node *addedPlaceholder = forceBlankParagraph ? insertBlockPlaceholder(m_endingPosition) :
        addBlockPlaceholderIfNeeded(m_endingPosition.node());

    calculateTypingStyleAfterDelete(addedPlaceholder);

    debugPosition("endingPosition   ", m_endingPosition);
    setEndingSelection(Selection(m_endingPosition, affinity));
    clearTransientState();
    rebalanceWhitespace();
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
