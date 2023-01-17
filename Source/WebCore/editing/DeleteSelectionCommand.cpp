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
#include "DeleteSelectionCommand.h"

#include "Document.h"
#include "DocumentMarkerController.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "ElementIterator.h"
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "HTMLTableElement.h"
#include "NodeTraversal.h"
#include "Range.h"
#include "RenderTableCell.h"
#include "RenderText.h"
#include "RenderedDocumentMarker.h"
#include "ScriptDisallowedScope.h"
#include "Text.h"
#include "VisibleUnits.h"

namespace WebCore {

using namespace HTMLNames;

static bool isTableRow(const Node* node)
{
    return node && node->hasTagName(trTag);
}

static bool isTableCellEmpty(Node* cell)
{
    ASSERT(isTableCell(cell));
    return VisiblePosition(firstPositionInNode(cell)) == VisiblePosition(lastPositionInNode(cell));
}

static bool isTableRowEmpty(Node* row)
{
    if (!isTableRow(row))
        return false;

    for (RefPtr child = row->firstChild(); child; child = child->nextSibling()) {
        if (isTableCell(child.get()) && !isTableCellEmpty(child.get()))
            return false;
    }

    return true;
}

static bool isSpecialHTMLElement(const Node& node)
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (!is<HTMLElement>(node))
        return false;

    if (downcast<HTMLElement>(node).isLink())
        return true;

    auto* renderer = downcast<HTMLElement>(node).renderer();
    if (!renderer)
        return false;

    if (renderer->style().display() == DisplayType::Table || renderer->style().display() == DisplayType::InlineTable)
        return true;

    if (renderer->style().isFloating())
        return true;

    if (renderer->style().position() != PositionType::Static)
        return true;

    return false;
}

static RefPtr<HTMLElement> firstInSpecialElement(const Position& position)
{
    RefPtr rootEditableElement { position.rootEditableElement() };
    for (RefPtr node = position.deprecatedNode(); node && node->rootEditableElement() == rootEditableElement; node = node->parentNode()) {
        if (!isSpecialHTMLElement(*node))
            continue;
        VisiblePosition visiblePosition = position;
        VisiblePosition firstInElement = firstPositionInOrBeforeNode(node.get());
        if ((isRenderedTable(node.get()) && visiblePosition == firstInElement.next()) || visiblePosition == firstInElement) {
            RELEASE_ASSERT(is<HTMLElement>(node));
            return static_pointer_cast<HTMLElement>(WTFMove(node));
        }
    }
    return nullptr;
}

static RefPtr<HTMLElement> lastInSpecialElement(const Position& position)
{
    RefPtr rootEditableElement { position.rootEditableElement() };
    for (RefPtr node = position.deprecatedNode(); node && node->rootEditableElement() == rootEditableElement; node = node->parentNode()) {
        if (!isSpecialHTMLElement(*node))
            continue;
        VisiblePosition visiblePosition = position;
        VisiblePosition lastInElement = lastPositionInOrAfterNode(node.get());
        if ((isRenderedTable(node.get()) && visiblePosition == lastInElement.previous()) || visiblePosition == lastInElement) {
            RELEASE_ASSERT(is<HTMLElement>(node));
            return static_pointer_cast<HTMLElement>(WTFMove(node));
        }
    }
    return nullptr;
}

static std::pair<Position, RefPtr<HTMLElement>> positionBeforeContainingSpecialElement(const Position& position)
{
    auto element = firstInSpecialElement(position);
    if (!element)
        return { position, nullptr };
    auto result = positionInParentBeforeNode(element.get());
    if (result.isNull() || result.containerNode()->rootEditableElement() != position.containerNode()->rootEditableElement())
        return { position, nullptr };
    return { result, WTFMove(element) };
}

static std::pair<Position, RefPtr<HTMLElement>> positionAfterContainingSpecialElement(const Position& position)
{
    auto element = lastInSpecialElement(position);
    if (!element)
        return { position, nullptr };
    auto result = positionInParentAfterNode(element.get());
    if (result.isNull() || result.deprecatedNode()->rootEditableElement() != position.containerNode()->rootEditableElement())
        return { position, nullptr };
    return { result, WTFMove(element) };
}

DeleteSelectionCommand::DeleteSelectionCommand(Document& document, bool smartDelete, bool mergeBlocksAfterDelete, bool replace, bool expandForSpecialElements, bool sanitizeMarkup, EditAction editingAction)
    : CompositeEditCommand(document, editingAction)
    , m_hasSelectionToDelete(false)
    , m_smartDelete(smartDelete)
    , m_mergeBlocksAfterDelete(mergeBlocksAfterDelete)
    , m_needPlaceholder(false)
    , m_replace(replace)
    , m_expandForSpecialElements(expandForSpecialElements)
    , m_pruneStartBlockIfNecessary(false)
    , m_startsAtEmptyLine(false)
    , m_sanitizeMarkup(sanitizeMarkup)
{
}

DeleteSelectionCommand::DeleteSelectionCommand(const VisibleSelection& selection, bool smartDelete, bool mergeBlocksAfterDelete, bool replace, bool expandForSpecialElements, bool sanitizeMarkup, EditAction editingAction)
    : CompositeEditCommand(selection.start().anchorNode()->document(), editingAction)
    , m_hasSelectionToDelete(true)
    , m_smartDelete(smartDelete)
    , m_mergeBlocksAfterDelete(mergeBlocksAfterDelete)
    , m_needPlaceholder(false)
    , m_replace(replace)
    , m_expandForSpecialElements(expandForSpecialElements)
    , m_pruneStartBlockIfNecessary(false)
    , m_startsAtEmptyLine(false)
    , m_sanitizeMarkup(sanitizeMarkup)
    , m_selectionToDelete(selection)
{
}

void DeleteSelectionCommand::initializeStartEnd(Position& start, Position& end)
{ 
    start = m_selectionToDelete.start();
    end = m_selectionToDelete.end();
 
    // For HRs, we'll get a position at (HR,1) when hitting delete from the beginning of the previous line, or (HR,0) when forward deleting,
    // but in these cases, we want to delete it, so manually expand the selection
    if (start.deprecatedNode()->hasTagName(hrTag))
        start = positionBeforeNode(start.deprecatedNode());
    else if (end.deprecatedNode()->hasTagName(hrTag))
        end = positionAfterNode(end.deprecatedNode());
    
    // FIXME: This is only used so that moveParagraphs can avoid the bugs in special element expansion.
    if (!m_expandForSpecialElements)
        return;
    
    while (1) {
        auto [startBeforeSpecialElement, startSpecialContainer] = positionBeforeContainingSpecialElement(start);
        auto [endAfterSpecialElement, endSpecialContainer] = positionAfterContainingSpecialElement(end);

        if (!startSpecialContainer && !endSpecialContainer)
            break;

        m_mergeBlocksAfterDelete = false;

        if (VisiblePosition(start) != m_selectionToDelete.visibleStart() || VisiblePosition(end) != m_selectionToDelete.visibleEnd())
            break;

        // If we're going to expand to include the startSpecialContainer, it must be fully selected.
        if (startSpecialContainer && !endSpecialContainer && positionInParentAfterNode(startSpecialContainer.get()) >= end)
            break;

        // If we're going to expand to include the endSpecialContainer, it must be fully selected.
        if (endSpecialContainer && !startSpecialContainer && start >= positionInParentBeforeNode(endSpecialContainer.get()))
            break;

        if (startSpecialContainer && startSpecialContainer->isDescendantOf(endSpecialContainer.get())) {
            // Don't adjust the end yet, it is the end of a special element that contains the start
            // special element (which may or may not be fully selected).
            start = startBeforeSpecialElement;
        } else if (endSpecialContainer && endSpecialContainer->isDescendantOf(startSpecialContainer.get())) {
            // Don't adjust the start yet, it is the start of a special element that contains the end
            // special element (which may or may not be fully selected).
            end = endAfterSpecialElement;
        } else {
            start = startBeforeSpecialElement;
            end = endAfterSpecialElement;
        }
    }
}

void DeleteSelectionCommand::setStartingSelectionOnSmartDelete(const Position& start, const Position& end)
{
    VisiblePosition newBase;
    VisiblePosition newExtent;
    if (startingSelection().isBaseFirst()) {
        newBase = start;
        newExtent = end;
    } else {
        newBase = end;
        newExtent = start;        
    }
    setStartingSelection(VisibleSelection(newBase, newExtent, startingSelection().isDirectional())); 
}
    
bool DeleteSelectionCommand::shouldSmartDeleteParagraphSpacers()
{
    return document().editingBehavior().shouldSmartInsertDeleteParagraphs();
}
    
void DeleteSelectionCommand::smartDeleteParagraphSpacers()
{
    VisiblePosition visibleStart { m_upstreamStart };
    VisiblePosition visibleEnd { m_downstreamEnd };
    bool selectionEndsInParagraphSeperator = isEndOfParagraph(visibleEnd);
    bool selectionEndIsEndOfContent = endOfEditableContent(visibleEnd) == visibleEnd;
    bool startAndEndInSameUnsplittableElement = unsplittableElementForPosition(visibleStart.deepEquivalent()) == unsplittableElementForPosition(visibleEnd.deepEquivalent());
    visibleStart = visibleStart.previous(CannotCrossEditingBoundary);
    visibleEnd = visibleEnd.next(CannotCrossEditingBoundary);
    bool previousPositionIsStartOfContent = startOfEditableContent(visibleStart) == visibleStart;
    bool previousPositionIsBlankParagraph = isBlankParagraph(visibleStart);
    bool endPositionIsBlankParagraph = isBlankParagraph(visibleEnd);
    bool hasBlankParagraphAfterEndOrIsEndOfContent = !selectionEndIsEndOfContent && (endPositionIsBlankParagraph || selectionEndsInParagraphSeperator);
    if (startAndEndInSameUnsplittableElement && previousPositionIsBlankParagraph && hasBlankParagraphAfterEndOrIsEndOfContent) {
        m_needPlaceholder = false;
        Position position;
        if (endPositionIsBlankParagraph)
            position = startOfNextParagraph(startOfNextParagraph(m_downstreamEnd)).deepEquivalent();
        else
            position = VisiblePosition(m_downstreamEnd).next().deepEquivalent();
        m_upstreamEnd = position.upstream();
        m_downstreamEnd = position.downstream();
        m_trailingWhitespace = m_downstreamEnd.trailingWhitespacePosition(VisiblePosition::defaultAffinity);
        setStartingSelectionOnSmartDelete(m_upstreamStart, m_downstreamEnd);
    }
    if (startAndEndInSameUnsplittableElement && selectionEndIsEndOfContent && previousPositionIsBlankParagraph && selectionEndsInParagraphSeperator) {
        m_needPlaceholder = false;
        VisiblePosition endOfParagraphBeforeStart;
        if (previousPositionIsStartOfContent)
            endOfParagraphBeforeStart = endOfParagraph(VisiblePosition { m_upstreamStart }.previous());
        else
            endOfParagraphBeforeStart = endOfParagraph(VisiblePosition { m_upstreamStart }.previous().previous());
        auto position = endOfParagraphBeforeStart.deepEquivalent();
        m_upstreamStart = position.upstream();
        m_downstreamStart = position.downstream();
        m_leadingWhitespace = m_upstreamStart.leadingWhitespacePosition(Affinity::Downstream);
        setStartingSelectionOnSmartDelete(m_upstreamStart, m_upstreamEnd);
    }
}
    
bool DeleteSelectionCommand::initializePositionData()
{
    Position start, end;
    initializeStartEnd(start, end);
    
    if (!isEditablePosition(start, ContentIsEditable))
        start = firstEditablePositionAfterPositionInRoot(start, highestEditableRoot(start));
    if (!isEditablePosition(end, ContentIsEditable))
        end = lastEditablePositionBeforePositionInRoot(end, highestEditableRoot(start));

    if (start.isNull() || end.isNull())
        return false;

    m_upstreamStart = start.upstream();
    m_downstreamStart = start.downstream();
    m_upstreamEnd = end.upstream();
    m_downstreamEnd = end.downstream();
    
    m_startRoot = editableRootForPosition(start);
    m_endRoot = editableRootForPosition(end);
    
    m_startTableRow = enclosingNodeOfType(start, &isTableRow);
    m_endTableRow = enclosingNodeOfType(end, &isTableRow);
    
    // Don't move content out of a table cell.
    // If the cell is non-editable, enclosingNodeOfType won't return it by default, so
    // tell that function that we don't care if it returns non-editable nodes.
    RefPtr startCell { enclosingNodeOfType(m_upstreamStart, &isTableCell, CanCrossEditingBoundary) };
    RefPtr endCell { enclosingNodeOfType(m_downstreamEnd, &isTableCell, CanCrossEditingBoundary) };
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
    
    // We don't want to merge into a block if it will mean changing the quote level of content after deleting 
    // selections that contain a whole number paragraphs plus a line break, since it is unclear to most users 
    // that such a selection actually ends at the start of the next paragraph. This matches TextEdit behavior 
    // for indented paragraphs.
    // Only apply this rule if the endingSelection is a range selection.  If it is a caret, then other operations have created
    // the selection we're deleting (like the process of creating a selection to delete during a backspace), and the user isn't in the situation described above.
    if (numEnclosingMailBlockquotes(start) != numEnclosingMailBlockquotes(end) 
            && isStartOfParagraph(visibleEnd) && isStartOfParagraph(VisiblePosition(start)) 
            && endingSelection().isRange()) {
        m_mergeBlocksAfterDelete = false;
        m_pruneStartBlockIfNecessary = true;
    }

    // Handle leading and trailing whitespace, as well as smart delete adjustments to the selection
    m_leadingWhitespace = m_upstreamStart.leadingWhitespacePosition(m_selectionToDelete.affinity());
    m_trailingWhitespace = m_downstreamEnd.trailingWhitespacePosition(VisiblePosition::defaultAffinity);

    if (m_smartDelete) {
    
        // skip smart delete if the selection to delete already starts or ends with whitespace
        Position pos = VisiblePosition(m_upstreamStart, m_selectionToDelete.affinity()).deepEquivalent();
        bool skipSmartDelete = pos.trailingWhitespacePosition(VisiblePosition::defaultAffinity, true).isNotNull();
        if (!skipSmartDelete)
            skipSmartDelete = m_downstreamEnd.leadingWhitespacePosition(VisiblePosition::defaultAffinity, true).isNotNull();

        // extend selection upstream if there is whitespace there
        bool hasLeadingWhitespaceBeforeAdjustment = m_upstreamStart.leadingWhitespacePosition(m_selectionToDelete.affinity(), true).isNotNull();
        if (!skipSmartDelete && hasLeadingWhitespaceBeforeAdjustment) {
            auto visiblePos = VisiblePosition(m_upstreamStart).previous();
            pos = visiblePos.deepEquivalent();
            // Expand out one character upstream for smart delete and recalculate
            // positions based on this change.
            m_upstreamStart = pos.upstream();
            m_downstreamStart = pos.downstream();
            m_leadingWhitespace = m_upstreamStart.leadingWhitespacePosition(visiblePos.affinity());

            setStartingSelectionOnSmartDelete(m_upstreamStart, m_upstreamEnd);
        }
        
        // trailing whitespace is only considered for smart delete if there is no leading
        // whitespace, as in the case where you double-click the first word of a paragraph.
        if (!skipSmartDelete && !hasLeadingWhitespaceBeforeAdjustment && m_downstreamEnd.trailingWhitespacePosition(VisiblePosition::defaultAffinity, true).isNotNull()) {
            // Expand out one character downstream for smart delete and recalculate
            // positions based on this change.
            pos = VisiblePosition(m_downstreamEnd).next().deepEquivalent();
            m_upstreamEnd = pos.upstream();
            m_downstreamEnd = pos.downstream();
            m_trailingWhitespace = m_downstreamEnd.trailingWhitespacePosition(VisiblePosition::defaultAffinity);

            setStartingSelectionOnSmartDelete(m_downstreamStart, m_downstreamEnd);
        }
    
        if (shouldSmartDeleteParagraphSpacers())
            smartDeleteParagraphSpacers();
    }
    
    // We must pass call parentAnchoredEquivalent on the positions since some editing positions
    // that appear inside their nodes aren't really inside them.  [hr, 0] is one example.
    // FIXME: parentAnchoredEquivalent should eventually be moved into enclosing element getters
    // like the one below, since editing functions should obviously accept editing positions.
    // FIXME: Passing false to enclosingNodeOfType tells it that it's OK to return a non-editable
    // node.  This was done to match existing behavior, but it seems wrong.
    m_startBlock = enclosingNodeOfType(m_downstreamStart.parentAnchoredEquivalent(), &isBlock, CanCrossEditingBoundary);
    m_endBlock = enclosingNodeOfType(m_upstreamEnd.parentAnchoredEquivalent(), &isBlock, CanCrossEditingBoundary);

    return true;
}

void DeleteSelectionCommand::saveTypingStyleState()
{
    // A common case is deleting characters that are all from the same text node. In 
    // that case, the style at the start of the selection before deletion will be the 
    // same as the style at the start of the selection after deletion (since those
    // two positions will be identical). Therefore there is no need to save the
    // typing style at the start of the selection, nor is there a reason to 
    // compute the style at the start of the selection after deletion (see the 
    // early return in calculateTypingStyleAfterDelete).
    // However, if typing style was previously set from another text node at the previous
    // position (now deleted), we need to clear that style as well.
    if (m_upstreamStart.deprecatedNode() == m_downstreamEnd.deprecatedNode() && m_upstreamStart.deprecatedNode()->isTextNode()) {
        document().selection().clearTypingStyle();
        return;
    }

    RefPtr<Node> startNode = m_selectionToDelete.start().deprecatedNode();
    if (!startNode->isTextNode() && !startNode->hasTagName(imgTag) && !startNode->hasTagName(brTag))
        return;

    // Figure out the typing style in effect before the delete is done.
    m_typingStyle = EditingStyle::create(m_selectionToDelete.start(), EditingStyle::EditingPropertiesInEffect);
    m_typingStyle->removeStyleAddedByNode(enclosingAnchorElement(m_selectionToDelete.start()));

    // If we're deleting into a Mail blockquote, save the style at end() instead of start()
    // We'll use this later in computeTypingStyleAfterDelete if we end up outside of a Mail blockquote
    if (enclosingNodeOfType(m_selectionToDelete.start(), isMailBlockquote))
        m_deleteIntoBlockquoteStyle = EditingStyle::create(m_selectionToDelete.end());
    else
        m_deleteIntoBlockquoteStyle = nullptr;
}

bool DeleteSelectionCommand::handleSpecialCaseBRDelete()
{
    RefPtr nodeAfterUpstreamStart { m_upstreamStart.computeNodeAfterPosition() };
    RefPtr nodeAfterDownstreamStart { m_downstreamStart.computeNodeAfterPosition() };
    // Upstream end will appear before BR due to canonicalization
    RefPtr nodeAfterUpstreamEnd { m_upstreamEnd.computeNodeAfterPosition() };

    if (!nodeAfterUpstreamStart || !nodeAfterDownstreamStart)
        return false;

    // Check for special-case where the selection contains only a BR on a line by itself after another BR.
    bool upstreamStartIsBR = nodeAfterUpstreamStart->hasTagName(brTag);
    bool downstreamStartIsBR = nodeAfterDownstreamStart->hasTagName(brTag);
    // We should consider that the BR is on a line by itself also when we have <br><br>. This test should be true only
    // when the two elements are siblings and should be false in a case like <div><br></div><br>.
    bool isBROnLineByItself = upstreamStartIsBR && downstreamStartIsBR && ((nodeAfterDownstreamStart == nodeAfterUpstreamEnd) || (nodeAfterUpstreamEnd && nodeAfterUpstreamEnd->hasTagName(brTag) && nodeAfterUpstreamStart->nextSibling() == nodeAfterUpstreamEnd));

    if (isBROnLineByItself) {
        removeNode(*nodeAfterDownstreamStart);
        return true;
    }

    // FIXME: This code doesn't belong in here.
    // We detect the case where the start is an empty line consisting of BR not wrapped in a block element.
    if (upstreamStartIsBR && downstreamStartIsBR
        && !(isStartOfBlock(positionBeforeNode(nodeAfterUpstreamStart.get())) && isEndOfBlock(positionAfterNode(nodeAfterDownstreamStart.get())))
        && (!nodeAfterUpstreamEnd || nodeAfterUpstreamEnd->hasTagName(brTag) || nodeAfterUpstreamEnd->previousSibling() != nodeAfterUpstreamStart)) {
        m_startsAtEmptyLine = true;
        m_endingPosition = m_downstreamEnd;
    }
    
    return false;
}

static Position firstEditablePositionInNode(Node* node)
{
    ASSERT(node);
    RefPtr next { node };
    while (next && !next->hasEditableStyle())
        next = NodeTraversal::next(*next, node);
    return next ? firstPositionInOrBeforeNode(next.get()) : Position();
}

void DeleteSelectionCommand::insertBlockPlaceholderForTableCellIfNeeded(Element& element)
{
    // Make sure empty cell has some height.
    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        auto* renderer = element.renderer();
        if (!is<RenderTableCell>(renderer))
            return;
        if (downcast<RenderTableCell>(*renderer).contentHeight() > 0)
            return;
    }
    insertBlockPlaceholder(firstEditablePositionInNode(&element));
}
    
void DeleteSelectionCommand::removeNodeUpdatingStates(Node& node, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
{
    if (&node == m_startBlock) {
        auto prev = VisiblePosition(firstPositionInNode(m_startBlock.get())).previous();
        if (!prev.isNull() && !isEndOfBlock(prev))
            m_needPlaceholder = true;
    } else if (&node == m_endBlock) {
        auto next = VisiblePosition(lastPositionInNode(m_endBlock.get())).next();
        if (!next.isNull() && !isStartOfBlock(next))
            m_needPlaceholder = true;
    }
    
    // FIXME: Update the endpoints of the range being deleted.
    updatePositionForNodeRemoval(m_endingPosition, node);
    updatePositionForNodeRemoval(m_leadingWhitespace, node);
    updatePositionForNodeRemoval(m_trailingWhitespace, node);
    
    CompositeEditCommand::removeNode(node, shouldAssumeContentIsAlwaysEditable);
}
    
static inline bool shouldRemoveContentOnly(const Node& node)
{
    return isTableStructureNode(&node) || node.isRootEditableElement();
}

void DeleteSelectionCommand::removeNode(Node& node, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
{
    if (!node.parentNode())
        return;

    Ref<Node> protectedNode = node;
    if (m_startRoot != m_endRoot && !(node.isDescendantOf(m_startRoot.get()) && node.isDescendantOf(m_endRoot.get()))) {
        // If a node is not in both the start and end editable roots, remove it only if its inside an editable region.
        if (!node.parentNode()->hasEditableStyle()) {
            // Don't remove non-editable atomic nodes.
            if (!node.firstChild())
                return;
            // Search this non-editable region for editable regions to empty.
            RefPtr child { node.firstChild() };
            while (child) {
                RefPtr nextChild { child->nextSibling() };
                removeNode(*child, shouldAssumeContentIsAlwaysEditable);
                // Bail if nextChild is no longer node's child.
                if (nextChild && nextChild->parentNode() != &node)
                    return;
                child = WTFMove(nextChild);
            }
            
            // Don't remove editable regions that are inside non-editable ones, just clear them.
            return;
        }
    }
    
    if (shouldRemoveContentOnly(node)) {
        // Do not remove an element of table structure; remove its contents.
        // Likewise for the root editable element.
        RefPtr child { NodeTraversal::next(node, &node) };
        while (child) {
            if (shouldRemoveContentOnly(*child)) {
                child = NodeTraversal::next(*child, &node);
                continue;
            }
            RefPtr nextChild { NodeTraversal::nextSkippingChildren(*child, &node) };
            removeNodeUpdatingStates(*child, shouldAssumeContentIsAlwaysEditable);
            child = WTFMove(nextChild);
        }
        
        ASSERT(is<Element>(node));
        auto& element = downcast<Element>(node);
        document().updateLayoutIgnorePendingStylesheets();
        // Check if we need to insert a placeholder for descendant table cells.
        RefPtr descendant { ElementTraversal::next(element, &element) };
        while (descendant) {
            RefPtr nextDescendant { ElementTraversal::next(*descendant, &element) };
            insertBlockPlaceholderForTableCellIfNeeded(*descendant);
            descendant = WTFMove(nextDescendant);
        }
        insertBlockPlaceholderForTableCellIfNeeded(element);
        return;
    }
    removeNodeUpdatingStates(node, shouldAssumeContentIsAlwaysEditable);
}

static void updatePositionForTextRemoval(Node* node, int offset, int count, Position& position)
{
    if (position.anchorType() != Position::PositionIsOffsetInAnchor || position.containerNode() != node)
        return;

    if (position.offsetInContainerNode() > offset + count)
        position.moveToOffset(position.offsetInContainerNode() - count);
    else if (position.offsetInContainerNode() > offset)
        position.moveToOffset(offset);
}

void DeleteSelectionCommand::deleteTextFromNode(Text& node, unsigned offset, unsigned count)
{
    // FIXME: Update the endpoints of the range being deleted.
    updatePositionForTextRemoval(&node, offset, count, m_endingPosition);
    updatePositionForTextRemoval(&node, offset, count, m_leadingWhitespace);
    updatePositionForTextRemoval(&node, offset, count, m_trailingWhitespace);
    updatePositionForTextRemoval(&node, offset, count, m_downstreamEnd);
    
    CompositeEditCommand::deleteTextFromNode(node, offset, count);
}

void DeleteSelectionCommand::makeStylingElementsDirectChildrenOfEditableRootToPreventStyleLoss()
{
    auto range = m_selectionToDelete.toNormalizedRange();
    if (!range)
        return;
    auto nodes = intersectingNodes(*range).begin();
    while (nodes) {
        Ref node = *nodes;
        auto shouldMove = is<HTMLLinkElement>(node)
            || (is<HTMLStyleElement>(node) && !downcast<HTMLStyleElement>(node.get()).hasAttributeWithoutSynchronization(scopedAttr));
        if (!shouldMove)
            nodes.advance();
        else {
            nodes.advanceSkippingChildren();
            if (RefPtr rootEditableElement = node->rootEditableElement()) {
                removeNode(node.get());
                appendNode(node.get(), *rootEditableElement);
            }
        }
    }
}

void DeleteSelectionCommand::handleGeneralDelete()
{
    if (m_upstreamStart.isNull())
        return;

    int startOffset = m_upstreamStart.deprecatedEditingOffset();
    RefPtr startNode { m_upstreamStart.deprecatedNode() };

    makeStylingElementsDirectChildrenOfEditableRootToPreventStyleLoss();

    // Never remove the start block unless it's a table, in which case we won't merge content in.
    if (startNode == m_startBlock && !startOffset && canHaveChildrenForEditing(*startNode) && !is<HTMLTableElement>(*startNode)) {
        startOffset = 0;
        startNode = NodeTraversal::next(*startNode);
        if (!startNode)
            return;
    }

    int startNodeCaretMaxOffset = caretMaxOffset(*startNode);
    if (startOffset >= startNodeCaretMaxOffset && is<Text>(*startNode)) {
        Text& text = downcast<Text>(*startNode);
        if (text.length() > static_cast<unsigned>(startNodeCaretMaxOffset))
            deleteTextFromNode(text, startNodeCaretMaxOffset, text.length() - startNodeCaretMaxOffset);
    }

    if (startOffset >= lastOffsetForEditing(*startNode)) {
        startNode = NodeTraversal::nextSkippingChildren(*startNode);
        startOffset = 0;
    }

    // Done adjusting the start.  See if we're all done.
    if (!startNode)
        return;

    if (startNode == m_downstreamEnd.deprecatedNode()) {
        if (m_downstreamEnd.deprecatedEditingOffset() - startOffset > 0) {
            if (is<Text>(*startNode)) {
                // in a text node that needs to be trimmed
                Text& text = downcast<Text>(*startNode);
                deleteTextFromNode(text, startOffset, m_downstreamEnd.deprecatedEditingOffset() - startOffset);
            } else {
                removeChildrenInRange(*startNode, startOffset, m_downstreamEnd.deprecatedEditingOffset());
                m_endingPosition = m_upstreamStart;
            }
        }

        // The selection to delete is all in one node.
        if (!startNode->renderer() || (!startOffset && m_downstreamEnd.atLastEditingPositionForNode()))
            removeNode(*startNode);
    }
    else {
        bool startNodeWasDescendantOfEndNode = m_upstreamStart.deprecatedNode()->isDescendantOf(m_downstreamEnd.deprecatedNode());
        // The selection to delete spans more than one node.
        auto node = startNode.copyRef();
        
        if (startOffset > 0) {
            if (is<Text>(*node)) {
                // in a text node that needs to be trimmed
                Text& text = downcast<Text>(*startNode);
                deleteTextFromNode(text, startOffset, text.length() - startOffset);
                node = NodeTraversal::next(*startNode);
            } else {
                node = startNode->traverseToChildAt(startOffset);
            }
        } else if (startNode == m_upstreamEnd.deprecatedNode() && is<Text>(*startNode)) {
            Text& text = downcast<Text>(*startNode);
            deleteTextFromNode(text, 0, m_upstreamEnd.deprecatedEditingOffset());
        }
        
        // handle deleting all nodes that are completely selected
        while (node && node != m_downstreamEnd.deprecatedNode() && !m_downstreamEnd.isNull() && !m_downstreamEnd.isOrphan()) {
            if (firstPositionInOrBeforeNode(node.get()) >= m_downstreamEnd) {
                // NodeTraversal::nextSkippingChildren just blew past the end position, so stop deleting
                node = nullptr;
            } else if (!m_downstreamEnd.deprecatedNode()->isDescendantOf(*node)) {
                RefPtr<Node> nextNode = NodeTraversal::nextSkippingChildren(*node);
                // if we just removed a node from the end container, update end position so the
                // check above will work
                updatePositionForNodeRemoval(m_downstreamEnd, *node);
                removeNode(*node);
                node = nextNode.get();
            } else {
                RefPtr lastDescendant { node->lastDescendant() };
                if (m_downstreamEnd.deprecatedNode() == lastDescendant && m_downstreamEnd.deprecatedEditingOffset() >= caretMaxOffset(*lastDescendant)) {
                    removeNode(*node);
                    node = nullptr;
                } else
                    node = NodeTraversal::next(*node);
            }
        }
        
        if (!m_downstreamEnd.isNull() && !m_downstreamEnd.isOrphan() && m_downstreamEnd.deprecatedNode() != startNode
            && !m_upstreamStart.deprecatedNode()->isDescendantOf(m_downstreamEnd.deprecatedNode())
            && m_downstreamEnd.deprecatedEditingOffset() >= caretMinOffset(*m_downstreamEnd.deprecatedNode())) {
            if (m_downstreamEnd.atLastEditingPositionForNode() && !canHaveChildrenForEditing(*m_downstreamEnd.deprecatedNode())) {
                // The node itself is fully selected, not just its contents.  Delete it.
                removeNode(*m_downstreamEnd.deprecatedNode());
            } else {
                if (is<Text>(*m_downstreamEnd.deprecatedNode())) {
                    // in a text node that needs to be trimmed
                    Text& text = downcast<Text>(*m_downstreamEnd.deprecatedNode());
                    if (m_downstreamEnd.deprecatedEditingOffset() > 0)
                        deleteTextFromNode(text, 0, m_downstreamEnd.deprecatedEditingOffset());
                // Remove children of m_downstreamEnd.deprecatedNode() that come after m_upstreamStart.
                // Don't try to remove children if m_upstreamStart was inside m_downstreamEnd.deprecatedNode()
                // and m_upstreamStart has been removed from the document, because then we don't 
                // know how many children to remove.
                // FIXME: Make m_upstreamStart a position we update as we remove content, then we can
                // always know which children to remove.
                } else if (!(startNodeWasDescendantOfEndNode && !m_upstreamStart.anchorNode()->isConnected())) {
                    unsigned offset = 0;
                    if (m_upstreamStart.deprecatedNode()->isDescendantOf(m_downstreamEnd.deprecatedNode())) {
                        RefPtr n { m_upstreamStart.deprecatedNode() };
                        while (n && n->parentNode() != m_downstreamEnd.deprecatedNode())
                            n = n->parentNode();
                        if (n)
                            offset = n->computeNodeIndex() + 1;
                    }
                    removeChildrenInRange(*m_downstreamEnd.deprecatedNode(), offset, m_downstreamEnd.deprecatedEditingOffset());
                    m_downstreamEnd = makeDeprecatedLegacyPosition(m_downstreamEnd.deprecatedNode(), offset);
                }
            }
        }
    }
}

void DeleteSelectionCommand::fixupWhitespace()
{
    document().updateLayoutIgnorePendingStylesheets();
    // FIXME: isRenderedCharacter should be removed, and we should use VisiblePosition::characterAfter and VisiblePosition::characterBefore
    if (m_leadingWhitespace.isNotNull() && !m_leadingWhitespace.isRenderedCharacter() && is<Text>(*m_leadingWhitespace.deprecatedNode())) {
        Text& textNode = downcast<Text>(*m_leadingWhitespace.deprecatedNode());
        ASSERT(!textNode.renderer() || textNode.renderer()->style().collapseWhiteSpace());
        replaceTextInNodePreservingMarkers(textNode, m_leadingWhitespace.deprecatedEditingOffset(), 1, nonBreakingSpaceString());
    }
    if (m_trailingWhitespace.isNotNull() && !m_trailingWhitespace.isRenderedCharacter() && is<Text>(*m_trailingWhitespace.deprecatedNode())) {
        Text& textNode = downcast<Text>(*m_trailingWhitespace.deprecatedNode());
        ASSERT(!textNode.renderer() || textNode.renderer()->style().collapseWhiteSpace());
        replaceTextInNodePreservingMarkers(textNode, m_trailingWhitespace.deprecatedEditingOffset(), 1, nonBreakingSpaceString());
    }
}

// If a selection starts in one block and ends in another, we have to merge to bring content before the
// start together with content after the end.
void DeleteSelectionCommand::mergeParagraphs()
{
    if (!m_mergeBlocksAfterDelete) {
        if (m_pruneStartBlockIfNecessary) {
            // We aren't going to merge into the start block, so remove it if it's empty.
            prune(m_startBlock.get());
            // Removing the start block during a deletion is usually an indication that we need
            // a placeholder, but not in this case.
            m_needPlaceholder = false;
        }
        return;
    }
    
    // It shouldn't have been asked to both try and merge content into the start block and prune it.
    ASSERT(!m_pruneStartBlockIfNecessary);

    // FIXME: Deletion should adjust selection endpoints as it removes nodes so that we never get into this state (4099839).
    if (m_downstreamEnd.isNull() || m_upstreamStart.isNull() || m_downstreamEnd.isOrphan() || m_upstreamStart.isOrphan())
        return;

    if (m_upstreamStart >= m_downstreamEnd)
        return;

    VisiblePosition startOfParagraphToMove(m_downstreamEnd);
    VisiblePosition mergeDestination(m_upstreamStart);
    
    // m_downstreamEnd's block has been emptied out by deletion.  There is no content inside of it to
    // move, so just remove it.
    RefPtr endBlock { enclosingBlock(m_downstreamEnd.deprecatedNode()) };
    if (!endBlock)
        return;

    if (!endBlock->contains(startOfParagraphToMove.deepEquivalent().deprecatedNode()) || !startOfParagraphToMove.deepEquivalent().deprecatedNode()) {
        removeNode(*endBlock);
        return;
    }
    
    // We need to merge into m_upstreamStart's block, but it's been emptied out and collapsed by deletion.
    if (!mergeDestination.deepEquivalent().deprecatedNode() || !mergeDestination.deepEquivalent().deprecatedNode()->isDescendantOf(enclosingBlock(m_upstreamStart.containerNode())) || m_startsAtEmptyLine) {
        insertNodeAt(HTMLBRElement::create(document()), m_upstreamStart);
        mergeDestination = VisiblePosition(m_upstreamStart);
    }
    
    if (mergeDestination == startOfParagraphToMove)
        return;
        
    VisiblePosition endOfParagraphToMove = endOfParagraph(startOfParagraphToMove, CanSkipOverEditingBoundary);
    
    if (mergeDestination == endOfParagraphToMove)
        return;
    
    // The rule for merging into an empty block is: only do so if its farther to the right.
    // FIXME: Consider RTL.
    if (!m_startsAtEmptyLine && isStartOfParagraph(mergeDestination) && startOfParagraphToMove.absoluteCaretBounds().x() > mergeDestination.absoluteCaretBounds().x()) {
        if (mergeDestination.deepEquivalent().downstream().deprecatedNode()->hasTagName(brTag)) {
            RefPtr nodeToRemove { mergeDestination.deepEquivalent().downstream().deprecatedNode() };
            removeNodeAndPruneAncestors(*nodeToRemove);
            m_endingPosition = startOfParagraphToMove.deepEquivalent();
            return;
        }
    }
    
    // Block images, tables and horizontal rules cannot be made inline with content at mergeDestination.  If there is 
    // any (!isStartOfParagraph(mergeDestination)), don't merge, just move the caret to just before the selection we deleted.
    // See https://bugs.webkit.org/show_bug.cgi?id=25439
    if (isRenderedAsNonInlineTableImageOrHR(startOfParagraphToMove.deepEquivalent().deprecatedNode()) && !isStartOfParagraph(mergeDestination)) {
        m_endingPosition = m_upstreamStart;
        return;
    }
    
    auto range = makeSimpleRange(startOfParagraphToMove, endOfParagraphToMove);
    if (!range)
        return;
    auto rangeToBeReplaced = makeSimpleRange(mergeDestination);
    if (!rangeToBeReplaced)
        return;
    if (!document().editor().client()->shouldMoveRangeAfterDelete(*range, *rangeToBeReplaced))
        return;

    // moveParagraphs will insert placeholders if it removes blocks that would require their use, don't let block
    // removals that it does cause the insertion of *another* placeholder.
    bool needPlaceholder = m_needPlaceholder;
    bool paragraphToMergeIsEmpty = (startOfParagraphToMove == endOfParagraphToMove);
    moveParagraph(startOfParagraphToMove, endOfParagraphToMove, mergeDestination, false, !paragraphToMergeIsEmpty);
    m_needPlaceholder = needPlaceholder;
    // The endingPosition was likely clobbered by the move, so recompute it (moveParagraph selects the moved paragraph).

    // FIXME (Bug 211793): endingSelection() becomes disconnected in moveParagraph
    if (auto* anchorNode = endingSelection().start().anchorNode(); anchorNode && anchorNode->isConnected())
        m_endingPosition = endingSelection().start();
}

void DeleteSelectionCommand::removePreviouslySelectedEmptyTableRows()
{
    // DeleteSelectionCommand::removeNode does not remove rows but only empties them in preparation for this function.
    // Instead, DeleteSelectionCommand::removeNodeUpdatingStates is used below, which calls a raw CompositeEditCommand::removeNode and adjusts selection.
    if (m_endTableRow && m_endTableRow->isConnected() && m_endTableRow != m_startTableRow) {
        RefPtr row { m_endTableRow->previousSibling() };
        while (row && row != m_startTableRow) {
            RefPtr previousRow { row->previousSibling() };
            if (isTableRowEmpty(row.get()))
                removeNodeUpdatingStates(*row, DoNotAssumeContentIsAlwaysEditable);
            row = WTFMove(previousRow);
        }
    }
    
    // Remove empty rows after the start row.
    if (m_startTableRow && m_startTableRow->isConnected() && m_startTableRow != m_endTableRow) {
        RefPtr row { m_startTableRow->nextSibling() };
        while (row && row != m_endTableRow) {
            RefPtr nextRow { row->nextSibling() };
            if (isTableRowEmpty(row.get()))
                removeNodeUpdatingStates(*row, DoNotAssumeContentIsAlwaysEditable);
            row = WTFMove(nextRow);
        }
    }

    if (m_endTableRow && m_endTableRow->isConnected() && m_endTableRow != m_startTableRow) {
        if (isTableRowEmpty(m_endTableRow.get())) {
            // Don't remove m_endTableRow if it's where we're putting the ending selection.
            if (!m_endingPosition.deprecatedNode()->isDescendantOf(*m_endTableRow)) {
                // FIXME: We probably shouldn't remove m_endTableRow unless it's fully selected, even if it is empty.
                // We'll need to start adjusting the selection endpoints during deletion to know whether or not m_endTableRow
                // was fully selected here.
                removeNodeUpdatingStates(*m_endTableRow, DoNotAssumeContentIsAlwaysEditable);
            }
        }
    }
}

void DeleteSelectionCommand::calculateTypingStyleAfterDelete()
{
    if (!m_typingStyle)
        return;
        
    // Compute the difference between the style before the delete and the style now
    // after the delete has been done. Set this style on the frame, so other editing
    // commands being composed with this one will work, and also cache it on the command,
    // so the Frame::appliedEditing can set it after the whole composite command 
    // has completed.
    
    // If we deleted into a blockquote, but are now no longer in a blockquote, use the alternate typing style
    if (m_deleteIntoBlockquoteStyle && !enclosingNodeOfType(m_endingPosition, isMailBlockquote, CanCrossEditingBoundary))
        m_typingStyle = m_deleteIntoBlockquoteStyle;
    m_deleteIntoBlockquoteStyle = nullptr;

    m_typingStyle->prepareToApplyAt(m_endingPosition);
    if (m_typingStyle->isEmpty())
        m_typingStyle = nullptr;
    // This is where we've deleted all traces of a style but not a whole paragraph (that's handled above).
    // In this case if we start typing, the new characters should have the same style as the just deleted ones,
    // but, if we change the selection, come back and start typing that style should be lost.  Also see 
    // preserveTypingStyle() below.
    document().selection().setTypingStyle(m_typingStyle.copyRef());
}

void DeleteSelectionCommand::clearTransientState()
{
    m_selectionToDelete = VisibleSelection();
    m_upstreamStart.clear();
    m_downstreamStart.clear();
    m_upstreamEnd.clear();
    m_downstreamEnd.clear();
    m_endingPosition.clear();
    m_leadingWhitespace.clear();
    m_trailingWhitespace.clear();
}
    
String DeleteSelectionCommand::originalStringForAutocorrectionAtBeginningOfSelection()
{
    if (!m_selectionToDelete.isRange())
        return String();

    VisiblePosition startOfSelection = m_selectionToDelete.start();
    if (!isStartOfWord(startOfSelection))
        return String();

    auto rangeOfFirstCharacter = makeSimpleRange(startOfSelection, startOfSelection.next());
    if (!rangeOfFirstCharacter)
        return String();

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    for (auto* marker : document().markers().markersInRange(*rangeOfFirstCharacter, DocumentMarker::Autocorrected)) {
        int startOffset = marker->startOffset();
        if (startOffset == startOfSelection.deepEquivalent().offsetInContainerNode())
            return marker->description();
    }
    return String();
}

// This method removes div elements with no attributes that have only one child or no children at all.
void DeleteSelectionCommand::removeRedundantBlocks()
{
    RefPtr node { m_endingPosition.containerNode() };
    if (!node)
        return;
    RefPtr rootNode { node->rootEditableElement() };
   
    while (node && node != rootNode) {
        if (isRemovableBlock(node.get())) {
            if (node == m_endingPosition.anchorNode())
                updatePositionForNodeRemovalPreservingChildren(m_endingPosition, *node);
            
            CompositeEditCommand::removeNodePreservingChildren(*node);
            node = m_endingPosition.anchorNode();
        } else
            node = node->parentNode();
    }
}

void DeleteSelectionCommand::doApply()
{
    // If selection has not been set to a custom selection when the command was created,
    // use the current ending selection.
    if (!m_hasSelectionToDelete)
        m_selectionToDelete = endingSelection();

    if (!m_selectionToDelete.isNonOrphanedRange() || !m_selectionToDelete.isContentEditable())
        return;

    String originalString = originalStringForAutocorrectionAtBeginningOfSelection();

    // If the deletion is occurring in a text field, and we're not deleting to replace the selection, then let the frame call across the bridge to notify the form delegate. 
    if (!m_replace) {
        if (RefPtr textControl = enclosingTextFormControl(m_selectionToDelete.start()); textControl && textControl->focused())
            document().editor().textWillBeDeletedInTextField(*textControl);
    }

    // save this to later make the selection with
    auto affinity = m_selectionToDelete.affinity();

    Position downstreamEnd = m_selectionToDelete.end().downstream();
    m_needPlaceholder = isStartOfParagraph(m_selectionToDelete.visibleStart(), CanCrossEditingBoundary)
            && isEndOfParagraph(m_selectionToDelete.visibleEnd(), CanCrossEditingBoundary)
            && !lineBreakExistsAtVisiblePosition(m_selectionToDelete.visibleEnd());
    if (m_needPlaceholder) {
        // Don't need a placeholder when deleting a selection that starts just before a table
        // and ends inside it (we do need placeholders to hold open empty cells, but that's
        // handled elsewhere).
        if (RefPtr table = isLastPositionBeforeTable(m_selectionToDelete.visibleStart())) {
            if (m_selectionToDelete.end().deprecatedNode()->isDescendantOf(*table))
                m_needPlaceholder = false;
        }
    }
        
    
    // set up our state
    if (!initializePositionData())
        return;

    // Delete any text that may hinder our ability to fixup whitespace after the delete
    deleteInsignificantTextDownstream(m_trailingWhitespace);    

    saveTypingStyleState();
    
    // deleting just a BR is handled specially, at least because we do not
    // want to replace it with a placeholder BR!
    if (handleSpecialCaseBRDelete()) {
        calculateTypingStyleAfterDelete();
        setEndingSelection(VisibleSelection(m_endingPosition, affinity, endingSelection().isDirectional()));
        clearTransientState();
        rebalanceWhitespace();
        return;
    }
    
    handleGeneralDelete();
    
    fixupWhitespace();
    
    mergeParagraphs();
    
    removePreviouslySelectedEmptyTableRows();
    
    if (m_needPlaceholder) {
        if (m_sanitizeMarkup)
            removeRedundantBlocks();

        // FIXME (Bug 212723): m_endingPosition becomes disconnected in moveParagraph()
        // because it is ancestor of the deleted element and gets pruned in removeNodeAndPruneAncestors().
        // Either removeNodeAndPruneAncestors() should not remove ending position or we should find
        // a different ending position.
        if (!m_endingPosition.containerNode() || !m_endingPosition.containerNode()->isConnected())
            return;
        insertNodeAt(HTMLBRElement::create(document()), m_endingPosition);
    }

    bool shouldRebalaceWhiteSpace = true;
    if (!document().editor().behavior().shouldRebalanceWhiteSpacesInSecureField()) {
        if (RefPtr endNode = m_endingPosition.deprecatedNode(); is<Text>(endNode)) {
            auto& textNode = downcast<Text>(*endNode);
            ScriptDisallowedScope::InMainThread scriptDisallowedScope;
            if (textNode.length() && textNode.renderer())
                shouldRebalaceWhiteSpace = textNode.renderer()->style().textSecurity() == TextSecurity::None;
        }        
    }
    if (shouldRebalaceWhiteSpace)
        rebalanceWhitespaceAt(m_endingPosition);

    calculateTypingStyleAfterDelete();

    if (!originalString.isEmpty())
        document().editor().deletedAutocorrectionAtPosition(m_endingPosition, originalString);

    setEndingSelection(VisibleSelection(VisiblePosition(m_endingPosition, affinity), endingSelection().isDirectional()));
    clearTransientState();
}

// Normally deletion doesn't preserve the typing style that was present before it.  For example,
// type a character, Bold, then delete the character and start typing.  The Bold typing style shouldn't
// stick around.  Deletion should preserve a typing style that *it* sets, however.
bool DeleteSelectionCommand::preservesTypingStyle() const
{
    return m_typingStyle;
}

} // namespace WebCore
