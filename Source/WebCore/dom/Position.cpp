/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include "Position.h"

#include "BoundaryPoint.h"
#include "CSSComputedStyleDeclaration.h"
#include "Editing.h"
#include "HTMLBRElement.h"
#include "HTMLBodyElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableElement.h"
#include "InlineElementBox.h"
#include "InlineIterator.h"
#include "InlineTextBox.h"
#include "LineLayoutTraversal.h"
#include "Logging.h"
#include "NodeTraversal.h"
#include "PositionIterator.h"
#include "Range.h"
#include "RenderBlock.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLineBreak.h"
#include "RenderText.h"
#include "Text.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include "VisibleUnits.h"
#include <stdio.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>
#include <wtf/unicode/CharacterNames.h>

#if ENABLE(TREE_DEBUGGING)
#include <wtf/text/StringBuilder.h>
#endif

namespace WebCore {

using namespace HTMLNames;

static bool hasInlineBoxWrapper(RenderObject& renderer)
{
    if (is<RenderBox>(renderer) && downcast<RenderBox>(renderer).inlineBoxWrapper())
        return true;
    if (is<RenderText>(renderer) && downcast<RenderText>(renderer).firstTextBox())
        return true;
    if (is<RenderLineBreak>(renderer) && downcast<RenderLineBreak>(renderer).inlineBoxWrapper())
        return true;
    return false;
}

static Node* nextRenderedEditable(Node* node)
{
    while ((node = nextLeafNode(node))) {
        RenderObject* renderer = node->renderer();
        if (!renderer || !node->hasEditableStyle())
            continue;
        if (hasInlineBoxWrapper(*renderer))
            return node;
    }
    return nullptr;
}

static Node* previousRenderedEditable(Node* node)
{
    while ((node = previousLeafNode(node))) {
        RenderObject* renderer = node->renderer();
        if (!renderer || !node->hasEditableStyle())
            continue;
        if (hasInlineBoxWrapper(*renderer))
            return node;
    }
    return nullptr;
}

Position::Position(Node* anchorNode, unsigned offset, LegacyEditingPositionFlag)
    : m_anchorNode(anchorNode)
    , m_offset(offset)
    , m_anchorType(anchorTypeForLegacyEditingPosition(m_anchorNode.get(), m_offset))
    , m_isLegacyEditingPosition(true)
{
    ASSERT(!m_anchorNode || !m_anchorNode->isShadowRoot() || m_anchorNode == containerNode());
    ASSERT(!m_anchorNode || !m_anchorNode->isPseudoElement());
}

Position::Position(Node* anchorNode, AnchorType anchorType)
    : m_anchorNode(anchorNode)
    , m_offset(0)
    , m_anchorType(anchorType)
    , m_isLegacyEditingPosition(false)
{
    ASSERT(!m_anchorNode || !m_anchorNode->isShadowRoot() || m_anchorNode == containerNode());
    ASSERT(!m_anchorNode || !m_anchorNode->isPseudoElement());
    ASSERT(anchorType != PositionIsOffsetInAnchor);
    ASSERT(!((anchorType == PositionIsBeforeChildren || anchorType == PositionIsAfterChildren)
        && (is<Text>(*m_anchorNode) || editingIgnoresContent(*m_anchorNode))));
}

Position::Position(Node* anchorNode, unsigned offset, AnchorType anchorType)
    : m_anchorNode(anchorNode)
    , m_offset(offset)
    , m_anchorType(anchorType)
    , m_isLegacyEditingPosition(false)
{
    ASSERT(!m_anchorNode || !editingIgnoresContent(*m_anchorNode));
    ASSERT(!m_anchorNode || !m_anchorNode->isPseudoElement());
    ASSERT(anchorType == PositionIsOffsetInAnchor);
}

Position::Position(Text* textNode, unsigned offset)
    : m_anchorNode(textNode)
    , m_offset(offset)
    , m_anchorType(PositionIsOffsetInAnchor)
    , m_isLegacyEditingPosition(false)
{
    ASSERT(m_anchorNode);
}

void Position::moveToPosition(Node* node, unsigned offset)
{
    ASSERT(!editingIgnoresContent(*node));
    ASSERT(anchorType() == PositionIsOffsetInAnchor || m_isLegacyEditingPosition);
    m_anchorNode = node;
    m_offset = offset;
    if (m_isLegacyEditingPosition)
        m_anchorType = anchorTypeForLegacyEditingPosition(m_anchorNode.get(), m_offset);
}

void Position::moveToOffset(unsigned offset)
{
    ASSERT(anchorType() == PositionIsOffsetInAnchor || m_isLegacyEditingPosition);
    m_offset = offset;
    if (m_isLegacyEditingPosition)
        m_anchorType = anchorTypeForLegacyEditingPosition(m_anchorNode.get(), m_offset);
}

Node* Position::containerNode() const
{
    if (!m_anchorNode)
        return nullptr;

    switch (anchorType()) {
    case PositionIsBeforeChildren:
    case PositionIsAfterChildren:
    case PositionIsOffsetInAnchor:
        return m_anchorNode.get();
    case PositionIsBeforeAnchor:
    case PositionIsAfterAnchor:
        return m_anchorNode->parentNode();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

Text* Position::containerText() const
{
    switch (anchorType()) {
    case PositionIsOffsetInAnchor:
        return m_anchorNode && is<Text>(*m_anchorNode) ? downcast<Text>(m_anchorNode.get()) : nullptr;
    case PositionIsBeforeAnchor:
    case PositionIsAfterAnchor:
        return nullptr;
    case PositionIsBeforeChildren:
    case PositionIsAfterChildren:
        ASSERT(!m_anchorNode || !is<Text>(*m_anchorNode));
        return nullptr;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

int Position::computeOffsetInContainerNode() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionIsBeforeChildren:
        return 0;
    case PositionIsAfterChildren:
        return m_anchorNode->length();
    case PositionIsOffsetInAnchor:
        return minOffsetForNode(m_anchorNode.get(), m_offset);
    case PositionIsBeforeAnchor:
        return m_anchorNode->computeNodeIndex();
    case PositionIsAfterAnchor:
        return m_anchorNode->computeNodeIndex() + 1;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

int Position::offsetForPositionAfterAnchor() const
{
    ASSERT(m_anchorType == PositionIsAfterAnchor || m_anchorType == PositionIsAfterChildren);
    ASSERT(!m_isLegacyEditingPosition);
    ASSERT(m_anchorNode);
    return m_anchorNode ? lastOffsetForEditing(*m_anchorNode) : 0;
}

// Neighbor-anchored positions are invalid DOM positions, so they need to be
// fixed up before handing them off to the Range object.
Position Position::parentAnchoredEquivalent() const
{
    if (!m_anchorNode)
        return { };
    
    // FIXME: This should only be necessary for legacy positions, but is also needed for positions before and after Tables
    if (m_offset <= 0 && (m_anchorType != PositionIsAfterAnchor && m_anchorType != PositionIsAfterChildren)) {
        if (m_anchorNode->parentNode() && (editingIgnoresContent(*m_anchorNode) || isRenderedTable(m_anchorNode.get())))
            return positionInParentBeforeNode(m_anchorNode.get());
        return Position(m_anchorNode.get(), 0, PositionIsOffsetInAnchor);
    }

    if (!m_anchorNode->isCharacterDataNode()
        && (m_anchorType == PositionIsAfterAnchor || m_anchorType == PositionIsAfterChildren || static_cast<unsigned>(m_offset) == m_anchorNode->countChildNodes())
        && (editingIgnoresContent(*m_anchorNode) || isRenderedTable(m_anchorNode.get()))
        && containerNode()) {
        return positionInParentAfterNode(m_anchorNode.get());
    }

    return { containerNode(), static_cast<unsigned>(computeOffsetInContainerNode()), PositionIsOffsetInAnchor };
}

RefPtr<Node> Position::firstNode() const
{
    auto container = makeRefPtr(containerNode());
    if (!container)
        return nullptr;
    if (is<CharacterData>(*container))
        return container;
    if (auto* node = computeNodeAfterPosition())
        return node;
    if (!computeOffsetInContainerNode())
        return container;
    return NodeTraversal::nextSkippingChildren(*container);
}

Node* Position::computeNodeBeforePosition() const
{
    if (!m_anchorNode)
        return nullptr;

    switch (anchorType()) {
    case PositionIsBeforeChildren:
        return nullptr;
    case PositionIsAfterChildren:
        return m_anchorNode->lastChild();
    case PositionIsOffsetInAnchor:
        return m_offset ? m_anchorNode->traverseToChildAt(m_offset - 1) : nullptr;
    case PositionIsBeforeAnchor:
        return m_anchorNode->previousSibling();
    case PositionIsAfterAnchor:
        return m_anchorNode.get();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

Node* Position::computeNodeAfterPosition() const
{
    if (!m_anchorNode)
        return nullptr;

    switch (anchorType()) {
    case PositionIsBeforeChildren:
        return m_anchorNode->firstChild();
    case PositionIsAfterChildren:
        return nullptr;
    case PositionIsOffsetInAnchor:
        return m_anchorNode->traverseToChildAt(m_offset);
    case PositionIsBeforeAnchor:
        return m_anchorNode.get();
    case PositionIsAfterAnchor:
        return m_anchorNode->nextSibling();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

Position::AnchorType Position::anchorTypeForLegacyEditingPosition(Node* anchorNode, unsigned offset)
{
    if (anchorNode && editingIgnoresContent(*anchorNode)) {
        if (offset == 0)
            return Position::PositionIsBeforeAnchor;
        return Position::PositionIsAfterAnchor;
    }
    return Position::PositionIsOffsetInAnchor;
}

// FIXME: This method is confusing (does it return anchorNode() or containerNode()?) and should be renamed or removed
Element* Position::element() const
{
    Node* node = anchorNode();
    while (node && !is<Element>(*node))
        node = node->parentNode();
    return downcast<Element>(node);
}

Position Position::previous(PositionMoveType moveType) const
{
    Node* node = deprecatedNode();
    if (!node)
        return *this;

    // FIXME: Negative offsets shouldn't be allowed. We should catch this earlier.
    ASSERT(deprecatedEditingOffset() >= 0);

    unsigned offset = deprecatedEditingOffset();

    if (anchorType() == PositionIsBeforeAnchor) {
        node = containerNode();
        if (!node)
            return *this;

        offset = computeOffsetInContainerNode();
    }

    if (offset > 0) {
        if (Node* child = node->traverseToChildAt(offset - 1))
            return lastPositionInOrAfterNode(child);

        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going backward one character at a time is correct.
        //   2) The old offset was a bogus offset like (<br>, 1), and there is no child.
        //      Going from 1 to 0 is correct.
        switch (moveType) {
        case CodePoint:
            return createLegacyEditingPosition(node, offset - 1);
        case Character:
            return createLegacyEditingPosition(node, uncheckedPreviousOffset(node, offset));
        case BackwardDeletion:
            return createLegacyEditingPosition(node, uncheckedPreviousOffsetForBackwardDeletion(node, offset));
        }
    }

    ContainerNode* parent = node->parentNode();
    if (!parent)
        return *this;

    if (positionBeforeOrAfterNodeIsCandidate(*node))
        return positionBeforeNode(node);

    Node* previousSibling = node->previousSibling();
    if (previousSibling && positionBeforeOrAfterNodeIsCandidate(*previousSibling))
        return positionAfterNode(previousSibling);

    return createLegacyEditingPosition(parent, node->computeNodeIndex());
}

Position Position::next(PositionMoveType moveType) const
{
    ASSERT(moveType != BackwardDeletion);

    Node* node = deprecatedNode();
    if (!node)
        return *this;

    // FIXME: Negative offsets shouldn't be allowed. We should catch this earlier.
    ASSERT(deprecatedEditingOffset() >= 0);

    unsigned offset = deprecatedEditingOffset();

    if (anchorType() == PositionIsAfterAnchor) {
        node = containerNode();
        if (!node)
            return *this;

        offset = computeOffsetInContainerNode();
    }

    Node* child = node->traverseToChildAt(offset);
    if (child || (!node->hasChildNodes() && offset < static_cast<unsigned>(lastOffsetForEditing(*node)))) {
        if (child)
            return firstPositionInOrBeforeNode(child);

        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going forward one character at a time is correct.
        //   2) The new offset is a bogus offset like (<br>, 1), and there is no child.
        //      Going from 0 to 1 is correct.
        return createLegacyEditingPosition(node, (moveType == Character) ? uncheckedNextOffset(node, offset) : offset + 1);
    }

    ContainerNode* parent = node->parentNode();
    if (!parent)
        return *this;

    if (isRenderedTable(node) || editingIgnoresContent(*node))
        return positionAfterNode(node);

    Node* nextSibling = node->nextSibling();
    if (nextSibling && positionBeforeOrAfterNodeIsCandidate(*nextSibling))
        return positionBeforeNode(nextSibling);

    return createLegacyEditingPosition(parent, node->computeNodeIndex() + 1);
}

int Position::uncheckedPreviousOffset(const Node* n, unsigned current)
{
    return n->renderer() ? n->renderer()->previousOffset(current) : current - 1;
}

int Position::uncheckedPreviousOffsetForBackwardDeletion(const Node* n, unsigned current)
{
    return n->renderer() ? n->renderer()->previousOffsetForBackwardDeletion(current) : current - 1;
}

int Position::uncheckedNextOffset(const Node* n, unsigned current)
{
    return n->renderer() ? n->renderer()->nextOffset(current) : current + 1;
}

bool Position::atFirstEditingPositionForNode() const
{
    if (isNull())
        return true;
    // FIXME: Position before anchor shouldn't be considered as at the first editing position for node
    // since that position resides outside of the node.
    switch (m_anchorType) {
    case PositionIsOffsetInAnchor:
        return m_offset <= 0;
    case PositionIsBeforeChildren:
    case PositionIsBeforeAnchor:
        return true;
    case PositionIsAfterChildren:
    case PositionIsAfterAnchor:
        return !lastOffsetForEditing(*deprecatedNode());
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool Position::atLastEditingPositionForNode() const
{
    if (isNull())
        return true;
    // FIXME: Position after anchor shouldn't be considered as at the first editing position for node since that position resides outside of the node.
    return m_anchorType == PositionIsAfterAnchor || m_anchorType == PositionIsAfterChildren || m_offset >= static_cast<unsigned>(lastOffsetForEditing(*deprecatedNode()));
}

// A position is considered at editing boundary if one of the following is true:
// 1. It is the first position in the node and the next visually equivalent position
//    is non editable.
// 2. It is the last position in the node and the previous visually equivalent position
//    is non editable.
// 3. It is an editable position and both the next and previous visually equivalent
//    positions are both non editable.
bool Position::atEditingBoundary() const
{
    Position nextPosition = downstream(CanCrossEditingBoundary);
    if (atFirstEditingPositionForNode() && nextPosition.isNotNull() && !nextPosition.deprecatedNode()->hasEditableStyle())
        return true;
        
    Position prevPosition = upstream(CanCrossEditingBoundary);
    if (atLastEditingPositionForNode() && prevPosition.isNotNull() && !prevPosition.deprecatedNode()->hasEditableStyle())
        return true;
        
    return nextPosition.isNotNull() && !nextPosition.deprecatedNode()->hasEditableStyle()
        && prevPosition.isNotNull() && !prevPosition.deprecatedNode()->hasEditableStyle();
}

Node* Position::parentEditingBoundary() const
{
    if (!m_anchorNode)
        return nullptr;

    Node* documentElement = m_anchorNode->document().documentElement();
    if (!documentElement)
        return nullptr;

    Node* boundary = m_anchorNode.get();
    while (boundary != documentElement && boundary->nonShadowBoundaryParentNode() && m_anchorNode->hasEditableStyle() == boundary->parentNode()->hasEditableStyle())
        boundary = boundary->nonShadowBoundaryParentNode();
    
    return boundary;
}


bool Position::atStartOfTree() const
{
    if (isNull())
        return true;

    Node* container = containerNode();
    if (container && container->parentNode())
        return false;

    switch (m_anchorType) {
    case PositionIsOffsetInAnchor:
        return m_offset <= 0;
    case PositionIsBeforeAnchor:
        return !m_anchorNode->previousSibling();
    case PositionIsAfterAnchor:
        return false;
    case PositionIsBeforeChildren:
        return true;
    case PositionIsAfterChildren:
        return !lastOffsetForEditing(*m_anchorNode);
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool Position::atEndOfTree() const
{
    if (isNull())
        return true;

    Node* container = containerNode();
    if (container && container->parentNode())
        return false;

    switch (m_anchorType) {
    case PositionIsOffsetInAnchor:
        return m_offset >= static_cast<unsigned>(lastOffsetForEditing(*m_anchorNode));
    case PositionIsBeforeAnchor:
        return false;
    case PositionIsAfterAnchor:
        return !m_anchorNode->nextSibling();
    case PositionIsBeforeChildren:
        return !lastOffsetForEditing(*m_anchorNode);
    case PositionIsAfterChildren:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// return first preceding DOM position rendered at a different location, or "this"
Position Position::previousCharacterPosition(EAffinity affinity) const
{
    if (isNull())
        return { };

    Node* fromRootEditableElement = deprecatedNode()->rootEditableElement();

    bool atStartOfLine = isStartOfLine(VisiblePosition(*this, affinity));
    bool rendered = isCandidate();
    
    Position currentPosition = *this;
    while (!currentPosition.atStartOfTree()) {
        currentPosition = currentPosition.previous();

        if (currentPosition.deprecatedNode()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atStartOfLine || !rendered) {
            if (currentPosition.isCandidate())
                return currentPosition;
        } else if (rendersInDifferentPosition(currentPosition))
            return currentPosition;
    }
    
    return *this;
}

// return first following position rendered at a different location, or "this"
Position Position::nextCharacterPosition(EAffinity affinity) const
{
    if (isNull())
        return { };

    Node* fromRootEditableElement = deprecatedNode()->rootEditableElement();

    bool atEndOfLine = isEndOfLine({ *this, affinity });
    bool rendered = isCandidate();
    
    Position currentPosition = *this;
    while (!currentPosition.atEndOfTree()) {
        currentPosition = currentPosition.next();

        if (currentPosition.deprecatedNode()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atEndOfLine || !rendered) {
            if (currentPosition.isCandidate())
                return currentPosition;
        } else if (rendersInDifferentPosition(currentPosition))
            return currentPosition;
    }
    
    return *this;
}

// Whether or not [node, 0] and [node, lastOffsetForEditing(node)] are their own VisiblePositions.
// If true, adjacent candidates are visually distinct.
// FIXME: Disregard nodes with renderers that have no height, as we do in isCandidate.
// FIXME: Share code with isCandidate, if possible.
static bool endsOfNodeAreVisuallyDistinctPositions(Node* node)
{
    if (!node || !node->renderer())
        return false;
        
    if (!node->renderer()->isInline())
        return true;
        
    // Don't include inline tables.
    if (is<HTMLTableElement>(*node))
        return false;
    
    if (!node->renderer()->isReplaced() || !canHaveChildrenForEditing(*node) || !downcast<RenderBox>(*node->renderer()).height())
        return false;

    // There is a VisiblePosition inside an empty inline-block container.
    if (!node->hasChildNodes())
        return true;

    return !Position::hasRenderedNonAnonymousDescendantsWithHeight(downcast<RenderElement>(*node->renderer()));
}

static Node* enclosingVisualBoundary(Node* node)
{
    while (node && !endsOfNodeAreVisuallyDistinctPositions(node))
        node = node->parentNode();
        
    return node;
}

// upstream() and downstream() want to return positions that are either in a
// text node or at just before a non-text node.  This method checks for that.
static bool isStreamer(const PositionIterator& pos)
{
    if (!pos.node())
        return true;
        
    if (isAtomicNode(pos.node()))
        return true;
        
    return pos.atStartOfNode();
}

// This function and downstream() are used for moving back and forth between visually equivalent candidates.
// For example, for the text node "foo     bar" where whitespace is collapsible, there are two candidates 
// that map to the VisiblePosition between 'b' and the space.  This function will return the left candidate 
// and downstream() will return the right one.
// Also, upstream() will return [boundary, 0] for any of the positions from [boundary, 0] to the first candidate
// in boundary, where endsOfNodeAreVisuallyDistinctPositions(boundary) is true.
Position Position::upstream(EditingBoundaryCrossingRule rule) const
{
    Node* startNode = deprecatedNode();
    if (!startNode)
        return { };
    
    // iterate backward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary(startNode);
    // FIXME: PositionIterator should respect Before and After positions.
    PositionIterator lastVisible = m_anchorType == PositionIsAfterAnchor ? createLegacyEditingPosition(m_anchorNode.get(), caretMaxOffset(*m_anchorNode)) : *this;
    PositionIterator currentPosition = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    Node* lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPosition.atStart(); currentPosition.decrement()) {
        auto& currentNode = *currentPosition.node();
        
        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (&currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode.hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }
            lastNode = &currentNode;
        }

        // If we've moved to a position that is visually distinct, return the last saved position. There 
        // is code below that terminates early if we're *about* to move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(&currentNode) && &currentNode != boundary)
            return lastVisible;

        // skip position in unrendered or invisible node
        RenderObject* renderer = currentNode.renderer();
        if (!renderer || renderer->style().visibility() != Visibility::Visible)
            continue;

        if (rule == CanCrossEditingBoundary && boundaryCrossed) {
            lastVisible = currentPosition;
            break;
        }
        
        // track last visible streamer position
        if (isStreamer(currentPosition))
            lastVisible = currentPosition;
        
        // Don't move past a position that is visually distinct.  We could rely on code above to terminate and 
        // return lastVisible on the next iteration, but we terminate early to avoid doing a computeNodeIndex() call.
        if (endsOfNodeAreVisuallyDistinctPositions(&currentNode) && currentPosition.atStartOfNode())
            return lastVisible;

        // Return position after tables and nodes which have content that can be ignored.
        if (editingIgnoresContent(currentNode) || isRenderedTable(&currentNode)) {
            if (currentPosition.atEndOfNode())
                return positionAfterNode(&currentNode);
            continue;
        }

        // return current position if it is in rendered text
        if (is<RenderText>(*renderer)) {
            auto& textRenderer = downcast<RenderText>(*renderer);

            auto firstTextBox = LineLayoutTraversal::firstTextBoxInTextOrderFor(textRenderer);
            if (!firstTextBox)
                continue;

            if (&currentNode != startNode) {
                // This assertion fires in layout tests in the case-transform.html test because
                // of a mix-up between offsets in the text in the DOM tree with text in the
                // render tree which can have a different length due to case transformation.
                // Until we resolve that, disable this so we can run the layout tests!
                //ASSERT(currentOffset >= renderer->caretMaxOffset());
                return createLegacyEditingPosition(&currentNode, renderer->caretMaxOffset());
            }

            unsigned textOffset = currentPosition.offsetInLeafNode();
            for (auto box = firstTextBox; box; box.traverseNextInTextOrder()) {
                if (textOffset <= box->localEndOffset()) {
                    if (textOffset > box->localStartOffset())
                        return currentPosition;
                    continue;
                }

                if (textOffset == box->localEndOffset() + 1 && box->isLastOnLine() && !box->isLast())
                    return currentPosition;
            }
        }
    }

    return lastVisible;
}

// This function and upstream() are used for moving back and forth between visually equivalent candidates.
// For example, for the text node "foo     bar" where whitespace is collapsible, there are two candidates 
// that map to the VisiblePosition between 'b' and the space.  This function will return the right candidate 
// and upstream() will return the left one.
// Also, downstream() will return the last position in the last atomic node in boundary for all of the positions
// in boundary after the last candidate, where endsOfNodeAreVisuallyDistinctPositions(boundary).
// FIXME: This function should never be called when the line box tree is dirty. See https://bugs.webkit.org/show_bug.cgi?id=97264
Position Position::downstream(EditingBoundaryCrossingRule rule) const
{
    Node* startNode = deprecatedNode();
    if (!startNode)
        return { };

    // iterate forward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary(startNode);
    // FIXME: PositionIterator should respect Before and After positions.
    PositionIterator lastVisible = m_anchorType == PositionIsAfterAnchor ? createLegacyEditingPosition(m_anchorNode.get(), caretMaxOffset(*m_anchorNode)) : *this;
    PositionIterator currentPosition = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    Node* lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPosition.atEnd(); currentPosition.increment()) {
        auto& currentNode = *currentPosition.node();

        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (&currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode.hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }

            lastNode = &currentNode;
        }

        // stop before going above the body, up into the head
        // return the last visible streamer position
        if (is<HTMLBodyElement>(currentNode) && currentPosition.atEndOfNode())
            break;

        // Do not move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(&currentNode) && &currentNode != boundary)
            return lastVisible;
        // Do not move past a visually disinct position.
        // Note: The first position after the last in a node whose ends are visually distinct
        // positions will be [boundary->parentNode(), originalBlock->computeNodeIndex() + 1].
        if (boundary && boundary->parentNode() == &currentNode)
            return lastVisible;

        // skip position in unrendered or invisible node
        auto* renderer = currentNode.renderer();
        if (!renderer || renderer->style().visibility() != Visibility::Visible)
            continue;

        if (rule == CanCrossEditingBoundary && boundaryCrossed) {
            lastVisible = currentPosition;
            break;
        }

        // track last visible streamer position
        if (isStreamer(currentPosition))
            lastVisible = currentPosition;

        // Return position before tables and nodes which have content that can be ignored.
        if (editingIgnoresContent(currentNode) || isRenderedTable(&currentNode)) {
            if (currentPosition.atStartOfNode())
                return positionBeforeNode(&currentNode);
            continue;
        }

        // return current position if it is in rendered text
        if (is<RenderText>(*renderer)) {
            auto& textRenderer = downcast<RenderText>(*renderer);

            auto firstTextBox = LineLayoutTraversal::firstTextBoxInTextOrderFor(textRenderer);
            if (!firstTextBox)
                continue;

            if (&currentNode != startNode) {
                ASSERT(currentPosition.atStartOfNode());
                return createLegacyEditingPosition(&currentNode, textRenderer.caretMinOffset());
            }

            unsigned textOffset = currentPosition.offsetInLeafNode();
            for (auto box = firstTextBox; box; box.traverseNextInTextOrder()) {
                if (!box->length() && textOffset == box->localStartOffset())
                    return currentPosition;

                if (textOffset < box->localEndOffset()) {
                    if (textOffset >= box->localStartOffset())
                        return currentPosition;
                    continue;
                }

                if (textOffset == box->localEndOffset() && box->isLastOnLine() && !box->isLast())
                    return currentPosition;
            }
        }
    }

    return lastVisible;
}

unsigned Position::positionCountBetweenPositions(const Position& a, const Position& b)
{
    if (a.isNull() || b.isNull())
        return UINT_MAX;
    
    Position endPos;
    Position pos;
    if (a > b) {
        endPos = a;
        pos = b;
    } else if (a < b) {
        endPos = b;
        pos = a;
    } else
        return 0;
    
    unsigned posCount = 0;
    while (!pos.atEndOfTree() && pos != endPos) {
        pos = pos.next();
        ++posCount;
    }
    return posCount;
}

static int boundingBoxLogicalHeight(RenderObject *o, const IntRect &rect)
{
    return o->style().isHorizontalWritingMode() ? rect.height() : rect.width();
}

bool Position::hasRenderedNonAnonymousDescendantsWithHeight(const RenderElement& renderer)
{
    RenderObject* stop = renderer.nextInPreOrderAfterChildren();
    for (RenderObject* o = renderer.firstChild(); o && o != stop; o = o->nextInPreOrder()) {
        if (!o->nonPseudoNode())
            continue;
        if (is<RenderText>(*o)) {
            if (boundingBoxLogicalHeight(o, downcast<RenderText>(*o).linesBoundingBox()))
                return true;
            continue;
        }
        if (is<RenderLineBreak>(*o)) {
            if (boundingBoxLogicalHeight(o, downcast<RenderLineBreak>(*o).linesBoundingBox()))
                return true;
            continue;
        }
        if (is<RenderBox>(*o)) {
            if (roundToInt(downcast<RenderBox>(*o).logicalHeight()))
                return true;
            continue;
        }
        if (is<RenderInline>(*o)) {
            const RenderInline& renderInline = downcast<RenderInline>(*o);
            if (isEmptyInline(renderInline) && boundingBoxLogicalHeight(o, renderInline.linesBoundingBox()))
                return true;
            continue;
        }
    }
    return false;
}

bool Position::nodeIsUserSelectNone(Node* node)
{
    return node && node->renderer() && node->renderer()->style().userSelect() == UserSelect::None;
}

#if ENABLE(USERSELECT_ALL)
bool Position::nodeIsUserSelectAll(const Node* node)
{
    return node && node->renderer() && node->renderer()->style().userSelect() == UserSelect::All;
}

Node* Position::rootUserSelectAllForNode(Node* node)
{
    if (!node || !nodeIsUserSelectAll(node))
        return nullptr;
    Node* parent = node->parentNode();
    if (!parent)
        return node;

    Node* candidateRoot = node;
    while (parent) {
        if (!parent->renderer()) {
            parent = parent->parentNode();
            continue;
        }
        if (!nodeIsUserSelectAll(parent))
            break;
        candidateRoot = parent;
        parent = candidateRoot->parentNode();
    }
    return candidateRoot;
}
#endif

bool Position::isCandidate() const
{
    if (isNull())
        return false;

    auto* renderer = deprecatedNode()->renderer();
    if (!renderer)
        return false;

    if (renderer->style().visibility() != Visibility::Visible)
        return false;

    if (renderer->isBR()) {
        // FIXME: The condition should be m_anchorType == PositionIsBeforeAnchor, but for now we still need to support legacy positions.
        return !m_offset && m_anchorType != PositionIsAfterAnchor && !nodeIsUserSelectNone(deprecatedNode()->parentNode());
    }

    if (is<RenderText>(*renderer))
        return !nodeIsUserSelectNone(deprecatedNode()) && downcast<RenderText>(*renderer).containsCaretOffset(m_offset);

    if (positionBeforeOrAfterNodeIsCandidate(*deprecatedNode())) {
        return ((atFirstEditingPositionForNode() && m_anchorType == PositionIsBeforeAnchor)
            || (atLastEditingPositionForNode() && m_anchorType == PositionIsAfterAnchor))
            && !nodeIsUserSelectNone(deprecatedNode()->parentNode());
    }

    if (is<HTMLHtmlElement>(*m_anchorNode))
        return false;

    if (is<RenderBlockFlow>(*renderer) || is<RenderGrid>(*renderer) || is<RenderFlexibleBox>(*renderer)) {
        RenderBlock& block = downcast<RenderBlock>(*renderer);
        if (block.logicalHeight() || is<HTMLBodyElement>(*m_anchorNode) || m_anchorNode->isRootEditableElement()) {
            if (!Position::hasRenderedNonAnonymousDescendantsWithHeight(block))
                return atFirstEditingPositionForNode() && !Position::nodeIsUserSelectNone(deprecatedNode());
            return m_anchorNode->hasEditableStyle() && !Position::nodeIsUserSelectNone(deprecatedNode()) && atEditingBoundary();
        }
        return false;
    }

    return m_anchorNode->hasEditableStyle() && !Position::nodeIsUserSelectNone(deprecatedNode()) && atEditingBoundary();
}

bool Position::isRenderedCharacter() const
{
    if (!is<Text>(deprecatedNode()))
        return false;

    RenderText* renderer = downcast<Text>(*deprecatedNode()).renderer();
    if (!renderer)
        return false;

    return renderer->containsRenderedCharacterOffset(m_offset);
}

static bool inSameEnclosingBlockFlowElement(Node* a, Node* b)
{
    return a && b && deprecatedEnclosingBlockFlowElement(a) == deprecatedEnclosingBlockFlowElement(b);
}

bool Position::rendersInDifferentPosition(const Position& position) const
{
    if (isNull() || position.isNull())
        return false;

    auto* renderer = deprecatedNode()->renderer();
    if (!renderer)
        return false;
    
    auto* positionRenderer = position.deprecatedNode()->renderer();
    if (!positionRenderer)
        return false;

    if (renderer->style().visibility() != Visibility::Visible || positionRenderer->style().visibility() != Visibility::Visible)
        return false;
    
    if (deprecatedNode() == position.deprecatedNode()) {
        if (is<HTMLBRElement>(*deprecatedNode()))
            return false;

        if (m_offset == static_cast<unsigned>(position.deprecatedEditingOffset()))
            return false;

        if (!is<Text>(*deprecatedNode()))
            return true;
    }

    if (is<HTMLBRElement>(*deprecatedNode()) && position.isCandidate())
        return true;

    if (is<HTMLBRElement>(*position.deprecatedNode()) && isCandidate())
        return true;

    if (!inSameEnclosingBlockFlowElement(deprecatedNode(), position.deprecatedNode()))
        return true;

    if (is<RenderText>(*renderer) && !downcast<RenderText>(*renderer).containsCaretOffset(m_offset))
        return false;

    if (is<RenderText>(*positionRenderer) && !downcast<RenderText>(*positionRenderer).containsCaretOffset(position.m_offset))
        return false;

    unsigned thisRenderedOffset = is<RenderText>(*renderer) ? downcast<RenderText>(*renderer).countRenderedCharacterOffsetsUntil(m_offset) : m_offset;
    unsigned positionRenderedOffset = is<RenderText>(*positionRenderer) ? downcast<RenderText>(*positionRenderer).countRenderedCharacterOffsetsUntil(position.m_offset) : position.m_offset;

    if (renderer == positionRenderer && thisRenderedOffset == positionRenderedOffset)
        return false;

    int ignoredCaretOffset;
    InlineBox* b1;
    getInlineBoxAndOffset(DOWNSTREAM, b1, ignoredCaretOffset);
    InlineBox* b2;
    position.getInlineBoxAndOffset(DOWNSTREAM, b2, ignoredCaretOffset);

    LOG(Editing, "renderer:               %p [%p]\n", renderer, b1);
    LOG(Editing, "thisRenderedOffset:         %d\n", thisRenderedOffset);
    LOG(Editing, "posRenderer:            %p [%p]\n", positionRenderer, b2);
    LOG(Editing, "posRenderedOffset:      %d\n", positionRenderedOffset);
    LOG(Editing, "node min/max:           %d:%d\n", caretMinOffset(*deprecatedNode()), caretMaxOffset(*deprecatedNode()));
    LOG(Editing, "pos node min/max:       %d:%d\n", caretMinOffset(*position.deprecatedNode()), caretMaxOffset(*position.deprecatedNode()));
    LOG(Editing, "----------------------------------------------------------------------\n");

    if (!b1 || !b2) {
        return false;
    }

    if (&b1->root() != &b2->root()) {
        return true;
    }

    if (nextRenderedEditable(deprecatedNode()) == position.deprecatedNode()
        && thisRenderedOffset == static_cast<unsigned>(caretMaxOffset(*deprecatedNode())) && !positionRenderedOffset) {
        return false;
    }
    
    if (previousRenderedEditable(deprecatedNode()) == position.deprecatedNode()
        && !thisRenderedOffset && positionRenderedOffset == static_cast<unsigned>(caretMaxOffset(*position.deprecatedNode()))) {
        return false;
    }

    return true;
}

// This assumes that it starts in editable content.
Position Position::leadingWhitespacePosition(EAffinity affinity, bool considerNonCollapsibleWhitespace) const
{
    ASSERT(isEditablePosition(*this));
    if (isNull())
        return { };
    
    if (is<HTMLBRElement>(*upstream().deprecatedNode()))
        return { };

    Position prev = previousCharacterPosition(affinity);
    if (prev != *this && inSameEnclosingBlockFlowElement(deprecatedNode(), prev.deprecatedNode()) && is<Text>(*prev.deprecatedNode())) {
        UChar c = downcast<Text>(*prev.deprecatedNode()).data()[prev.deprecatedEditingOffset()];
        if (considerNonCollapsibleWhitespace ? (isHTMLSpace(c) || c == noBreakSpace) : deprecatedIsCollapsibleWhitespace(c)) {
            if (isEditablePosition(prev))
                return prev;
        }
    }

    return { };
}

// This assumes that it starts in editable content.
Position Position::trailingWhitespacePosition(EAffinity, bool considerNonCollapsibleWhitespace) const
{
    ASSERT(isEditablePosition(*this));
    if (isNull())
        return { };
    
    VisiblePosition v(*this);
    UChar c = v.characterAfter();
    // The space must not be in another paragraph and it must be editable.
    if (!isEndOfParagraph(v) && v.next(CannotCrossEditingBoundary).isNotNull())
        if (considerNonCollapsibleWhitespace ? (isHTMLSpace(c) || c == noBreakSpace) : deprecatedIsCollapsibleWhitespace(c))
            return *this;
    
    return { };
}

void Position::getInlineBoxAndOffset(EAffinity affinity, InlineBox*& inlineBox, int& caretOffset) const
{
    getInlineBoxAndOffset(affinity, primaryDirection(), inlineBox, caretOffset);
}

static bool isNonTextLeafChild(RenderObject& object)
{
    if (is<RenderText>(object))
        return false;
    return !downcast<RenderElement>(object).firstChild();
}

static InlineTextBox* searchAheadForBetterMatch(RenderObject* renderer)
{
    RenderBlock* container = renderer->containingBlock();
    RenderObject* next = renderer;
    while ((next = next->nextInPreOrder(container))) {
        if (is<RenderBlock>(*next))
            return nullptr;
        if (next->isBR())
            return nullptr;
        if (isNonTextLeafChild(*next))
            return nullptr;
        if (is<RenderText>(*next)) {
            InlineTextBox* match = nullptr;
            unsigned minOffset = std::numeric_limits<unsigned>::max();
            for (InlineTextBox* box = downcast<RenderText>(*next).firstTextBox(); box; box = box->nextTextBox()) {
                unsigned caretMinOffset = box->caretMinOffset();
                if (caretMinOffset < minOffset) {
                    match = box;
                    minOffset = caretMinOffset;
                }
            }
            if (match)
                return match;
        }
    }
    return nullptr;
}

static Position downstreamIgnoringEditingBoundaries(Position position)
{
    Position lastPosition;
    while (position != lastPosition) {
        lastPosition = position;
        position = position.downstream(CanCrossEditingBoundary);
    }
    return position;
}

static Position upstreamIgnoringEditingBoundaries(Position position)
{
    Position lastPosition;
    while (position != lastPosition) {
        lastPosition = position;
        position = position.upstream(CanCrossEditingBoundary);
    }
    return position;
}

void Position::getInlineBoxAndOffset(EAffinity affinity, TextDirection primaryDirection, InlineBox*& inlineBox, int& caretOffset) const
{
    caretOffset = deprecatedEditingOffset();
    RenderObject* renderer = deprecatedNode()->renderer();
    if (!renderer)
        return;
    
    if (renderer->isBR()) {
        auto& lineBreakRenderer = downcast<RenderLineBreak>(*renderer);
        lineBreakRenderer.ensureLineBoxes();
        inlineBox = !caretOffset ? lineBreakRenderer.inlineBoxWrapper() : nullptr;
    } else if (is<RenderText>(*renderer)) {
        auto& textRenderer = downcast<RenderText>(*renderer);
        textRenderer.ensureLineBoxes();

        InlineTextBox* box;
        InlineTextBox* candidate = nullptr;

        for (box = textRenderer.firstTextBox(); box; box = box->nextTextBox()) {
            unsigned caretMinOffset = box->caretMinOffset();
            unsigned caretMaxOffset = box->caretMaxOffset();

            if (static_cast<unsigned>(caretOffset) < caretMinOffset || static_cast<unsigned>(caretOffset) > caretMaxOffset || (static_cast<unsigned>(caretOffset) == caretMaxOffset && box->isLineBreak()))
                continue;

            if (static_cast<unsigned>(caretOffset) > caretMinOffset && static_cast<unsigned>(caretOffset) < caretMaxOffset) {
                inlineBox = box;
                return;
            }

            if (((static_cast<unsigned>(caretOffset) == caretMaxOffset) ^ (affinity == DOWNSTREAM))
                || ((static_cast<unsigned>(caretOffset) == caretMinOffset) ^ (affinity == UPSTREAM))
                || (static_cast<unsigned>(caretOffset) == caretMaxOffset && box->nextLeafOnLine() && box->nextLeafOnLine()->isLineBreak()))
                break;

            candidate = box;
        }
        if (candidate && candidate == textRenderer.lastTextBox() && affinity == DOWNSTREAM) {
            box = searchAheadForBetterMatch(&textRenderer);
            if (box)
                caretOffset = box->caretMinOffset();
        }
        inlineBox = box ? box : candidate;
    } else {
        inlineBox = nullptr;
        if (canHaveChildrenForEditing(*deprecatedNode()) && is<RenderBlockFlow>(*renderer) && hasRenderedNonAnonymousDescendantsWithHeight(downcast<RenderBlockFlow>(*renderer))) {
            // Try a visually equivalent position with possibly opposite editability. This helps in case |this| is in
            // an editable block but surrounded by non-editable positions. It acts to negate the logic at the beginning
            // of RenderObject::createVisiblePosition().
            Position equivalent = downstreamIgnoringEditingBoundaries(*this);
            if (equivalent == *this) {
                equivalent = upstreamIgnoringEditingBoundaries(*this);
                if (equivalent == *this || downstreamIgnoringEditingBoundaries(equivalent) == *this)
                    return;
            }

            equivalent.getInlineBoxAndOffset(UPSTREAM, primaryDirection, inlineBox, caretOffset);
            return;
        }
        if (is<RenderBox>(*renderer)) {
            inlineBox = downcast<RenderBox>(*renderer).inlineBoxWrapper();
            if (!inlineBox || (caretOffset > inlineBox->caretMinOffset() && caretOffset < inlineBox->caretMaxOffset()))
                return;
        }
    }

    if (!inlineBox)
        return;

    unsigned char level = inlineBox->bidiLevel();

    if (inlineBox->direction() == primaryDirection) {
        if (caretOffset == inlineBox->caretRightmostOffset()) {
            InlineBox* nextBox = inlineBox->nextLeafOnLine();
            if (!nextBox || nextBox->bidiLevel() >= level)
                return;

            level = nextBox->bidiLevel();
            InlineBox* prevBox = inlineBox;
            do {
                prevBox = prevBox->previousLeafOnLine();
            } while (prevBox && prevBox->bidiLevel() > level);

            if (prevBox && prevBox->bidiLevel() == level)   // For example, abc FED 123 ^ CBA
                return;

            // For example, abc 123 ^ CBA
            while (InlineBox* nextBox = inlineBox->nextLeafOnLine()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            caretOffset = inlineBox->caretRightmostOffset();
        } else {
            InlineBox* prevBox = inlineBox->previousLeafOnLine();
            if (!prevBox || prevBox->bidiLevel() >= level)
                return;

            level = prevBox->bidiLevel();
            InlineBox* nextBox = inlineBox;
            do {
                nextBox = nextBox->nextLeafOnLine();
            } while (nextBox && nextBox->bidiLevel() > level);

            if (nextBox && nextBox->bidiLevel() == level)
                return;

            while (InlineBox* prevBox = inlineBox->previousLeafOnLine()) {
                if (prevBox->bidiLevel() < level)
                    break;
                inlineBox = prevBox;
            }
            caretOffset = inlineBox->caretLeftmostOffset();
        }
        return;
    }

    if (caretOffset == inlineBox->caretLeftmostOffset()) {
        InlineBox* prevBox = inlineBox->previousLeafOnLineIgnoringLineBreak();
        if (!prevBox || prevBox->bidiLevel() < level) {
            // Left edge of a secondary run. Set to the right edge of the entire run.
            while (InlineBox* nextBox = inlineBox->nextLeafOnLineIgnoringLineBreak()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            caretOffset = inlineBox->caretRightmostOffset();
        } else if (prevBox->bidiLevel() > level) {
            // Right edge of a "tertiary" run. Set to the left edge of that run.
            while (InlineBox* tertiaryBox = inlineBox->previousLeafOnLineIgnoringLineBreak()) {
                if (tertiaryBox->bidiLevel() <= level)
                    break;
                inlineBox = tertiaryBox;
            }
            caretOffset = inlineBox->caretLeftmostOffset();
        }
    } else {
        InlineBox* nextBox = inlineBox->nextLeafOnLineIgnoringLineBreak();
        if (!nextBox || nextBox->bidiLevel() < level) {
            // Right edge of a secondary run. Set to the left edge of the entire run.
            while (InlineBox* prevBox = inlineBox->previousLeafOnLineIgnoringLineBreak()) {
                if (prevBox->bidiLevel() < level)
                    break;
                inlineBox = prevBox;
            }
            caretOffset = inlineBox->caretLeftmostOffset();
        } else if (nextBox->bidiLevel() > level) {
            // Left edge of a "tertiary" run. Set to the right edge of that run.
            while (InlineBox* tertiaryBox = inlineBox->nextLeafOnLineIgnoringLineBreak()) {
                if (tertiaryBox->bidiLevel() <= level)
                    break;
                inlineBox = tertiaryBox;
            }
            caretOffset = inlineBox->caretRightmostOffset();
        }
    }
}

TextDirection Position::primaryDirection() const
{
    if (!m_anchorNode->renderer())
        return TextDirection::LTR;
    if (auto* blockFlow = lineageOfType<RenderBlockFlow>(*m_anchorNode->renderer()).first())
        return blockFlow->style().direction();
    return TextDirection::LTR;
}

#if ENABLE(TREE_DEBUGGING)

void Position::debugPosition(const char* msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, deprecatedNode()->nodeName().utf8().data(), deprecatedNode(), m_offset);
}

void Position::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;

    if (isNull())
        result.appendLiteral("<null>");
    else {
        char s[1024];
        result.appendLiteral("offset ");
        result.appendNumber(m_offset);
        result.appendLiteral(" of ");
        deprecatedNode()->formatForDebugger(s, sizeof(s));
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}

void Position::showAnchorTypeAndOffset() const
{
    if (m_isLegacyEditingPosition)
        fputs("legacy, ", stderr);
    switch (anchorType()) {
    case PositionIsOffsetInAnchor:
        fputs("offset", stderr);
        break;
    case PositionIsBeforeChildren:
        fputs("beforeChildren", stderr);
        break;
    case PositionIsAfterChildren:
        fputs("afterChildren", stderr);
        break;
    case PositionIsBeforeAnchor:
        fputs("before", stderr);
        break;
    case PositionIsAfterAnchor:
        fputs("after", stderr);
        break;
    }
    fprintf(stderr, ", offset:%d\n", m_offset);
}

void Position::showTreeForThis() const
{
    if (anchorNode()) {
        anchorNode()->showTreeForThis();
        showAnchorTypeAndOffset();
    }
}

#endif

bool Position::equals(const Position& other) const
{
    if (!m_anchorNode)
        return !m_anchorNode == !other.m_anchorNode;
    if (!other.m_anchorNode)
        return false;

    switch (anchorType()) {
    case PositionIsBeforeChildren:
        ASSERT(!is<Text>(*m_anchorNode));
        switch (other.anchorType()) {
        case PositionIsBeforeChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return m_anchorNode == other.m_anchorNode;
        case PositionIsAfterChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return m_anchorNode == other.m_anchorNode && !m_anchorNode->hasChildNodes();
        case PositionIsOffsetInAnchor:
            return m_anchorNode == other.m_anchorNode && !other.m_offset;
        case PositionIsBeforeAnchor:
            return m_anchorNode->firstChild() == other.m_anchorNode;
        case PositionIsAfterAnchor:
            return false;
        }
        break;
    case PositionIsAfterChildren:
        ASSERT(!is<Text>(*m_anchorNode));
        switch (other.anchorType()) {
        case PositionIsBeforeChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return m_anchorNode == other.m_anchorNode && !m_anchorNode->hasChildNodes();
        case PositionIsAfterChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return m_anchorNode == other.m_anchorNode;
        case PositionIsOffsetInAnchor:
            return m_anchorNode == other.m_anchorNode && m_anchorNode->countChildNodes() == static_cast<unsigned>(m_offset);
        case PositionIsBeforeAnchor:
            return false;
        case PositionIsAfterAnchor:
            return m_anchorNode->lastChild() == other.m_anchorNode;
        }
        break;
    case PositionIsOffsetInAnchor:
        switch (other.anchorType()) {
        case PositionIsBeforeChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return m_anchorNode == other.m_anchorNode && !m_offset;
        case PositionIsAfterChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return m_anchorNode == other.m_anchorNode && m_offset == other.m_anchorNode->countChildNodes();
        case PositionIsOffsetInAnchor:
            return m_anchorNode == other.m_anchorNode && m_offset == other.m_offset;
        case PositionIsBeforeAnchor:
            return m_anchorNode->traverseToChildAt(m_offset) == other.m_anchorNode;
        case PositionIsAfterAnchor:
            return m_offset && m_anchorNode->traverseToChildAt(m_offset - 1) == other.m_anchorNode;
        }
        break;
    case PositionIsBeforeAnchor:
        switch (other.anchorType()) {
        case PositionIsBeforeChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return m_anchorNode == other.m_anchorNode->firstChild();
        case PositionIsAfterChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return false;
        case PositionIsOffsetInAnchor:
            return m_anchorNode == other.m_anchorNode->traverseToChildAt(other.m_offset);
        case PositionIsBeforeAnchor:
            return m_anchorNode == other.m_anchorNode;
        case PositionIsAfterAnchor:
            return m_anchorNode->previousSibling() == other.m_anchorNode;
        }
        break;
    case PositionIsAfterAnchor:
        switch (other.anchorType()) {
        case PositionIsBeforeChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return false;
        case PositionIsAfterChildren:
            ASSERT(!is<Text>(*other.m_anchorNode));
            return m_anchorNode == other.m_anchorNode->lastChild();
        case PositionIsOffsetInAnchor:
            return other.m_offset && m_anchorNode == other.m_anchorNode->traverseToChildAt(other.m_offset - 1);
        case PositionIsBeforeAnchor:
            return m_anchorNode->nextSibling() == other.m_anchorNode;
        case PositionIsAfterAnchor:
            return m_anchorNode == other.m_anchorNode;
        }
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static TextStream& operator<<(TextStream& stream, Position::AnchorType anchorType)
{
    switch (anchorType) {
    case Position::PositionIsOffsetInAnchor:
        stream << "offset in anchor";
        break;
    case Position::PositionIsBeforeAnchor:
        stream << "before anchor";
        break;
    case Position::PositionIsAfterAnchor:
        stream << "after anchor";
        break;
    case Position::PositionIsBeforeChildren:
        stream << "before children";
        break;
    case Position::PositionIsAfterChildren:
        stream << "after children";
        break;
    }
    return stream;
}

TextStream& operator<<(TextStream& stream, const Position& position)
{
    TextStream::GroupScope scope(stream);
    stream << "Position " << &position;

    stream.dumpProperty("anchor node", position.anchorNode());
    stream.dumpProperty("offset", position.offsetInContainerNode());
    stream.dumpProperty("anchor type", position.anchorType());

    return stream;
}

RefPtr<Node> commonShadowIncludingAncestor(const Position& a, const Position& b)
{
    auto* commonScope = commonTreeScope(a.containerNode(), b.containerNode());
    if (!commonScope)
        return nullptr;
    auto* nodeA = commonScope->ancestorNodeInThisScope(a.containerNode());
    ASSERT(nodeA);
    auto* nodeB = commonScope->ancestorNodeInThisScope(b.containerNode());
    ASSERT(nodeB);
    return commonInclusiveAncestor(*nodeA, *nodeB);
}

Position positionInParentBeforeNode(Node* node)
{
    auto* ancestor = node->parentNode();
    while (ancestor && editingIgnoresContent(*ancestor)) {
        node = ancestor;
        ancestor = ancestor->parentNode();
    }
    ASSERT(ancestor);
    return Position(ancestor, node->computeNodeIndex(), Position::PositionIsOffsetInAnchor);
}

Position positionInParentAfterNode(Node* node)
{
    auto* ancestor = node->parentNode();
    while (ancestor && editingIgnoresContent(*ancestor)) {
        node = ancestor;
        ancestor = ancestor->parentNode();
    }
    ASSERT(ancestor);
    return Position(ancestor, node->computeNodeIndex() + 1, Position::PositionIsOffsetInAnchor);
}

// FIXME: Rename to use make for the prefix verb instead of create.
Position createLegacyEditingPosition(const BoundaryPoint& point)
{
    return createLegacyEditingPosition(point.container.ptr(), point.offset);
}

Optional<BoundaryPoint> makeBoundaryPoint(const Position& position)
{
    if (position.isNull())
        return WTF::nullopt;
    return BoundaryPoint { *position.containerNode(), static_cast<unsigned>(position.computeOffsetInContainerNode()) };
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showTree(const WebCore::Position& pos)
{
    pos.showTreeForThis();
}

void showTree(const WebCore::Position* pos)
{
    if (pos)
        pos->showTreeForThis();
}

#endif
