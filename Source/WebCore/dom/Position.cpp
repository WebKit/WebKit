/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2015-2018 Google Inc. All rights reserved.
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
#include "ElementInlines.h"
#include "HTMLBRElement.h"
#include "HTMLBodyElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLNames.h"
#include "HTMLTableElement.h"
#include "InlineIteratorLineBox.h"
#include "InlineIteratorLogicalOrderTraversal.h"
#include "InlineIteratorTextBox.h"
#include "InlineRunAndOffset.h"
#include "Logging.h"
#include "NodeTraversal.h"
#include "PositionIterator.h"
#include "RenderBlock.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLineBreak.h"
#include "RenderText.h"
#include "SVGElementTypeHelpers.h"
#include "SVGTextElement.h"
#include "Text.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include "VisibleUnits.h"
#include <stdio.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>
#include <wtf/unicode/CharacterNames.h>

#if ENABLE(TREE_DEBUGGING)
#include <wtf/text/StringBuilder.h>
#endif

namespace WebCore {

using namespace HTMLNames;

static bool hasInlineRun(RenderObject& renderer)
{
    if (auto* renderBox = dynamicDowncast<RenderBox>(renderer); renderBox && InlineIterator::boxFor(*renderBox))
        return true;
    if (auto* renderText = dynamicDowncast<RenderText>(renderer); renderText && InlineIterator::firstTextBoxFor(*renderText))
        return true;
    if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer); renderLineBreak && InlineIterator::boxFor(*renderLineBreak))
        return true;
    return false;
}

static Node* nextRenderedEditable(Node* node)
{
    while ((node = nextLeafNode(node))) {
        CheckedPtr renderer = node->renderer();
        if (!renderer || !node->hasEditableStyle())
            continue;
        if (hasInlineRun(*renderer))
            return node;
    }
    return nullptr;
}

static Node* previousRenderedEditable(Node* node)
{
    while ((node = previousLeafNode(node))) {
        CheckedPtr renderer = node->renderer();
        if (!renderer || !node->hasEditableStyle())
            continue;
        if (hasInlineRun(*renderer))
            return node;
    }
    return nullptr;
}

Position::Position(RefPtr<Node>&& anchorNode, unsigned offset, LegacyEditingPositionFlag)
    : m_anchorNode(WTFMove(anchorNode))
    , m_offset(offset)
    , m_anchorType(anchorTypeForLegacyEditingPosition(m_anchorNode.get(), m_offset))
    , m_isLegacyEditingPosition(true)
{
    ASSERT(!m_anchorNode || !m_anchorNode->isShadowRoot() || m_anchorNode == containerNode());
    ASSERT(!m_anchorNode || !m_anchorNode->isPseudoElement());
}

Position::Position(RefPtr<Node>&& anchorNode, AnchorType anchorType)
    : m_anchorNode(WTFMove(anchorNode))
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

Position::Position(RefPtr<Node>&& anchorNode, unsigned offset, AnchorType anchorType)
    : m_anchorNode(WTFMove(anchorNode))
    , m_offset(offset)
    , m_anchorType(anchorType)
    , m_isLegacyEditingPosition(false)
{
    ASSERT(anchorType == PositionIsOffsetInAnchor);
}

Position::Position(RefPtr<Text>&& textNode, unsigned offset)
    : m_anchorNode(WTFMove(textNode))
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
        return dynamicDowncast<Text>(m_anchorNode.get());
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

RefPtr<Text> Position::protectedContainerText() const
{
    return containerText();
}

Element* Position::containerOrParentElement() const
{
    auto* container = containerNode();
    if (!container)
        return nullptr;
    if (auto* element = dynamicDowncast<Element>(*container))
        return element;
    return container->parentElement();
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
        return m_offset;
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
    RefPtr anchorNode = this->anchorNode();
    ASSERT(anchorNode);
    return anchorNode ? lastOffsetForEditing(*anchorNode) : 0;
}

// Neighbor-anchored positions are invalid DOM positions, so they need to be
// fixed up before handing them off to the Range object.
Position Position::parentAnchoredEquivalent() const
{
    RefPtr anchorNode = this->anchorNode();
    if (!anchorNode)
        return { };
    
    // FIXME: This should only be necessary for legacy positions, but is also needed for positions before and after Tables
    if (!m_offset && (m_anchorType != PositionIsAfterAnchor && m_anchorType != PositionIsAfterChildren)) {
        if (anchorNode->parentNode() && (editingIgnoresContent(*anchorNode) || isRenderedTable(anchorNode.get())))
            return positionInParentBeforeNode(anchorNode.get());
        return Position(anchorNode.get(), 0, PositionIsOffsetInAnchor);
    }

    if (!anchorNode->isCharacterDataNode()
        && (m_anchorType == PositionIsAfterAnchor || m_anchorType == PositionIsAfterChildren || static_cast<unsigned>(m_offset) == anchorNode->countChildNodes())
        && (editingIgnoresContent(*anchorNode) || isRenderedTable(anchorNode.get()))
        && containerNode()) {
        return positionInParentAfterNode(anchorNode.get());
    }

    return { containerNode(), static_cast<unsigned>(computeOffsetInContainerNode()), PositionIsOffsetInAnchor };
}

RefPtr<Node> Position::firstNode() const
{
    RefPtr container { containerNode() };
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
RefPtr<Element> Position::anchorElementAncestor() const
{
    for (RefPtr node = anchorNode(); node; node = node->parentNode()) {
        if (auto* element = dynamicDowncast<Element>(*node))
            return element;
    }
    return nullptr;
}

Position Position::previous(PositionMoveType moveType) const
{
    auto node = protectedDeprecatedNode();
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
        if (RefPtr child = node->traverseToChildAt(offset - 1))
            return lastPositionInOrAfterNode(child.get());

        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going backward one character at a time is correct.
        //   2) The old offset was a bogus offset like (<br>, 1), and there is no child.
        //      Going from 1 to 0 is correct.
        switch (moveType) {
        case CodePoint:
            return makeDeprecatedLegacyPosition(WTFMove(node), offset - 1);
        case Character: {
            auto previousOffset = uncheckedPreviousOffset(node.get(), offset);
            return makeDeprecatedLegacyPosition(WTFMove(node), previousOffset);
        } case BackwardDeletion: {
            auto previousOffset = uncheckedPreviousOffsetForBackwardDeletion(node.get(), offset);
            return makeDeprecatedLegacyPosition(WTFMove(node), previousOffset);
        }
        }
    }

    RefPtr parent = node->parentNode();
    if (!parent)
        return *this;

    if (positionBeforeOrAfterNodeIsCandidate(*node))
        return positionBeforeNode(node.get());

    RefPtr previousSibling = node->previousSibling();
    if (previousSibling && positionBeforeOrAfterNodeIsCandidate(*previousSibling))
        return positionAfterNode(previousSibling.get());

    return makeContainerOffsetPosition(WTFMove(parent), node->computeNodeIndex());
}

Position Position::next(PositionMoveType moveType) const
{
    ASSERT(moveType != BackwardDeletion);

    auto node = protectedDeprecatedNode();
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

    RefPtr child = node->traverseToChildAt(offset);
    if (child || (!node->hasChildNodes() && offset < static_cast<unsigned>(lastOffsetForEditing(*node)))) {
        if (child)
            return firstPositionInOrBeforeNode(child.get());

        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going forward one character at a time is correct.
        //   2) The new offset is a bogus offset like (<br>, 1), and there is no child.
        //      Going from 0 to 1 is correct.
        auto nextOffset = moveType == Character ? uncheckedNextOffset(node.get(), offset) : offset + 1;
        return makeDeprecatedLegacyPosition(WTFMove(node), nextOffset);
    }

    RefPtr parent = node->parentNode();
    if (!parent)
        return *this;

    if (isRenderedTable(node.get()) || editingIgnoresContent(*node))
        return positionAfterNode(node.get());

    RefPtr nextSibling = node->nextSibling();
    if (nextSibling && positionBeforeOrAfterNodeIsCandidate(*nextSibling))
        return positionBeforeNode(nextSibling.get());

    return makeContainerOffsetPosition(WTFMove(parent), node->computeNodeIndex() + 1);
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
        return !m_offset;
    case PositionIsBeforeChildren:
    case PositionIsBeforeAnchor:
        return true;
    case PositionIsAfterChildren:
    case PositionIsAfterAnchor:
        return !lastOffsetForEditing(*protectedDeprecatedNode());
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool Position::atLastEditingPositionForNode() const
{
    if (isNull())
        return true;
    // FIXME: Position after anchor shouldn't be considered as at the first editing position for node since that position resides outside of the node.
    return m_anchorType == PositionIsAfterAnchor || m_anchorType == PositionIsAfterChildren || m_offset >= static_cast<unsigned>(lastOffsetForEditing(*protectedDeprecatedNode()));
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

RefPtr<Node> Position::parentEditingBoundary() const
{
    if (!m_anchorNode)
        return nullptr;

    RefPtr documentElement = m_anchorNode->document().documentElement();
    if (!documentElement)
        return nullptr;

    RefPtr boundary = m_anchorNode;
    while (boundary != documentElement && boundary->nonShadowBoundaryParentNode() && m_anchorNode->hasEditableStyle() == boundary->parentNode()->hasEditableStyle())
        boundary = boundary->nonShadowBoundaryParentNode();
    
    return boundary;
}


bool Position::atStartOfTree() const
{
    if (isNull())
        return true;

    RefPtr container = containerNode();
    if (container && container->parentNode())
        return false;

    switch (m_anchorType) {
    case PositionIsOffsetInAnchor:
        return !m_offset;
    case PositionIsBeforeAnchor:
        return !m_anchorNode->previousSibling();
    case PositionIsAfterAnchor:
        return false;
    case PositionIsBeforeChildren:
        return true;
    case PositionIsAfterChildren:
        return !lastOffsetForEditing(*protectedAnchorNode());
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool Position::atEndOfTree() const
{
    if (isNull())
        return true;

    RefPtr container = containerNode();
    if (container && container->parentNode())
        return false;

    switch (m_anchorType) {
    case PositionIsOffsetInAnchor:
        return m_offset >= static_cast<unsigned>(lastOffsetForEditing(*protectedAnchorNode()));
    case PositionIsBeforeAnchor:
        return false;
    case PositionIsAfterAnchor:
        return !m_anchorNode->nextSibling();
    case PositionIsBeforeChildren:
        return !lastOffsetForEditing(*protectedAnchorNode());
    case PositionIsAfterChildren:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// return first preceding DOM position rendered at a different location, or "this"
Position Position::previousCharacterPosition(Affinity affinity) const
{
    if (isNull())
        return { };

    RefPtr fromRootEditableElement = deprecatedNode()->rootEditableElement();

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
    
    if (!node->renderer()->isReplacedOrInlineBlock() || !canHaveChildrenForEditing(*node) || !downcast<RenderBox>(*node->renderer()).height())
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
    RefPtr node = pos.node();
    if (!node)
        return true;

    if (isAtomicNode(node.get()))
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
    RefPtr startNode = deprecatedNode();
    if (!startNode)
        return { };

    // iterate backward from there, looking for a qualified position
    RefPtr boundary = enclosingVisualBoundary(startNode.get());
    // FIXME: PositionIterator should respect Before and After positions.
    RefPtr anchorNode = m_anchorNode;
    PositionIterator lastVisible = m_anchorType == PositionIsAfterAnchor ? makeDeprecatedLegacyPosition(anchorNode.get(), caretMaxOffset(*anchorNode)) : *this;
    PositionIterator currentPosition = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    RefPtr lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPosition.atStart(); currentPosition.decrement()) {
        Ref currentNode = *currentPosition.node();
        
        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (currentNode.ptr() != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode->hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }
            lastNode = currentNode.ptr();
        }

        // There is no caret position in non-text svg elements.
        if (currentNode->isSVGElement() && !is<SVGTextElement>(currentNode))
            continue;

        // If we've moved to a position that is visually distinct, return the last saved position. There 
        // is code below that terminates early if we're *about* to move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode.ptr()) && currentNode.ptr() != boundary)
            return lastVisible;

        // skip position in unrendered or invisible node
        CheckedPtr renderer = currentNode->renderer();
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
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode.ptr()) && currentPosition.atStartOfNode())
            return lastVisible;

        // Return position after tables and nodes which have content that can be ignored.
        if (editingIgnoresContent(currentNode) || isRenderedTable(currentNode.ptr())) {
            if (currentPosition.atEndOfNode())
                return positionAfterNode(currentNode.ptr());
            continue;
        }

        // return current position if it is in rendered text
        if (auto* textRenderer = dynamicDowncast<RenderText>(*renderer)) {
            auto [firstTextBox, orderCache] = InlineIterator::firstTextBoxInLogicalOrderFor(*textRenderer);
            if (!firstTextBox)
                continue;

            if (currentNode.ptr() != startNode) {
                // This assertion fires in layout tests in the case-transform.html test because
                // of a mix-up between offsets in the text in the DOM tree with text in the
                // render tree which can have a different length due to case transformation.
                // Until we resolve that, disable this so we can run the layout tests!
                //ASSERT(currentOffset >= renderer->caretMaxOffset());
                return makeDeprecatedLegacyPosition(currentNode.ptr(), renderer->caretMaxOffset());
            }

            unsigned textOffset = currentPosition.offsetInLeafNode();
            for (auto box = firstTextBox; box;) {
                if (textOffset > box->start() && textOffset <= box->end())
                    return currentPosition;

                auto nextBox = InlineIterator::nextTextBoxInLogicalOrder(box, orderCache);
                if (textOffset == box->end() + 1 && nextBox && box->lineBox() != nextBox->lineBox())
                    return currentPosition;

                box = nextBox;
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
    RefPtr startNode = deprecatedNode();
    if (!startNode)
        return { };

    // iterate forward from there, looking for a qualified position
    RefPtr boundary = enclosingVisualBoundary(startNode.get());
    // FIXME: PositionIterator should respect Before and After positions.
    RefPtr anchorNode = m_anchorNode;
    PositionIterator lastVisible = m_anchorType == PositionIsAfterAnchor ? makeDeprecatedLegacyPosition(anchorNode.get(), caretMaxOffset(*anchorNode)) : *this;
    PositionIterator currentPosition = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    auto lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPosition.atEnd(); currentPosition.increment()) {
        Ref currentNode = *currentPosition.node();

        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (currentNode.ptr() != lastNode.get()) {
            // Don't change editability.
            bool currentEditable = currentNode->hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }

            lastNode = currentNode.ptr();
        }

        // stop before going above the body, up into the head
        // return the last visible streamer position
        if (is<HTMLBodyElement>(currentNode.get()) && currentPosition.atEndOfNode())
            break;

        // There is no caret position in non-text svg elements.
        if (currentNode->isSVGElement() && !is<SVGTextElement>(currentNode.get()))
            continue;

        // Do not move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode.ptr()) && currentNode.ptr() != boundary)
            return lastVisible;
        // Do not move past a visually disinct position.
        // Note: The first position after the last in a node whose ends are visually distinct
        // positions will be [boundary->parentNode(), originalBlock->computeNodeIndex() + 1].
        if (boundary && boundary->parentNode() == currentNode.ptr())
            return lastVisible;

        // skip position in unrendered or invisible node
        CheckedPtr renderer = currentNode->renderer();
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
        if (editingIgnoresContent(currentNode) || isRenderedTable(currentNode.ptr())) {
            if (currentPosition.atStartOfNode())
                return positionBeforeNode(currentNode.ptr());
            continue;
        }

        // return current position if it is in rendered text
        if (auto* textRenderer = dynamicDowncast<RenderText>(*renderer)) {
            auto [firstTextBox, orderCache] = InlineIterator::firstTextBoxInLogicalOrderFor(*textRenderer);
            if (!firstTextBox)
                continue;

            if (currentNode.ptr() != startNode) {
                ASSERT(currentPosition.atStartOfNode());
                return makeContainerOffsetPosition(currentNode.ptr(), textRenderer->caretMinOffset());
            }

            unsigned textOffset = currentPosition.offsetInLeafNode();
            for (auto box = firstTextBox; box;) {
                if (!box->length() && textOffset == box->start())
                    return currentPosition;

                if (textOffset >= box->start() && textOffset < box->end())
                    return currentPosition;

                auto nextBox = InlineIterator::nextTextBoxInLogicalOrder(box, orderCache);
                if (textOffset == box->end() && nextBox && box->lineBox() != nextBox->lineBox())
                    return currentPosition;

                box = nextBox;
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
    return o->writingMode().isHorizontal() ? rect.height() : rect.width();
}

bool Position::hasRenderedNonAnonymousDescendantsWithHeight(const RenderElement& renderer)
{
    RenderObject* stop = renderer.nextInPreOrderAfterChildren();
    for (RenderObject* o = renderer.firstChild(); o && o != stop; o = o->nextInPreOrder()) {
        if (!o->nonPseudoNode())
            continue;
        if (auto* renderText = dynamicDowncast<RenderText>(*o)) {
            if (boundingBoxLogicalHeight(o, renderText->linesBoundingBox()))
                return true;
            continue;
        }
        if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(*o)) {
            if (boundingBoxLogicalHeight(o, renderLineBreak->linesBoundingBox()))
                return true;
            continue;
        }
        if (auto* renderBox = dynamicDowncast<RenderBox>(*o)) {
            if (roundToInt(renderBox->logicalHeight()))
                return true;
            continue;
        }
        if (auto* renderInline = dynamicDowncast<RenderInline>(*o)) {
            if (isEmptyInline(*renderInline) && boundingBoxLogicalHeight(o, renderInline->linesBoundingBox()))
                return true;
            continue;
        }
    }
    return false;
}

bool Position::nodeIsUserSelectNone(Node* node)
{
    if (!node)
        return false;
    return node->renderer() && (node->renderer()->style().usedUserSelect() == UserSelect::None);
}

bool Position::nodeIsUserSelectAll(const Node* node)
{
    if (!node)
        return false;
    CheckedPtr renderer = node->renderer();
    return renderer && renderer->style().usedUserSelect() == UserSelect::All;
}

RefPtr<Node> Position::rootUserSelectAllForNode(Node* node)
{
    if (!node || !nodeIsUserSelectAll(node))
        return nullptr;
    RefPtr parent = node->parentNode();
    if (!parent)
        return node;

    RefPtr candidateRoot = node;
    while (parent) {
        if (!parent->renderer()) {
            parent = parent->parentNode();
            continue;
        }
        if (!nodeIsUserSelectAll(parent.get()))
            break;
        candidateRoot = WTFMove(parent);
        parent = candidateRoot->parentNode();
    }
    return candidateRoot;
}

// This function should be kept in sync with PositionIterator::isCandidate().
bool Position::isCandidate() const
{
    if (isNull())
        return false;

    RefPtr node = deprecatedNode();
    CheckedPtr renderer = node->renderer();
    if (!renderer)
        return false;

    if (renderer->style().visibility() != Visibility::Visible)
        return false;

    if (renderer->isBR()) {
        // FIXME: The condition should be m_anchorType == PositionIsBeforeAnchor, but for now we still need to support legacy positions.
        return !m_offset && m_anchorType != PositionIsAfterAnchor && !nodeIsUserSelectNone(node->parentNode());
    }

    if (auto* renderText = dynamicDowncast<RenderText>(*renderer))
        return !nodeIsUserSelectNone(node.get()) && renderText->containsCaretOffset(m_offset);

    if (positionBeforeOrAfterNodeIsCandidate(*node)) {
        return ((atFirstEditingPositionForNode() && m_anchorType == PositionIsBeforeAnchor)
            || (atLastEditingPositionForNode() && m_anchorType == PositionIsAfterAnchor))
            && !nodeIsUserSelectNone(node->parentNode());
    }

    if (is<HTMLHtmlElement>(*m_anchorNode))
        return false;

    if (auto* block = dynamicDowncast<RenderBlock>(*renderer)) {
        if (is<RenderBlockFlow>(*block) || is<RenderGrid>(*block) || is<RenderFlexibleBox>(*block)) {
            if (block->logicalHeight() || is<HTMLBodyElement>(*m_anchorNode) || m_anchorNode->isRootEditableElement()) {
                if (!Position::hasRenderedNonAnonymousDescendantsWithHeight(*block))
                    return atFirstEditingPositionForNode() && !Position::nodeIsUserSelectNone(node.get());
                return m_anchorNode->hasEditableStyle() && !Position::nodeIsUserSelectNone(node.get()) && atEditingBoundary();
            }
            return false;
        }
    }

    return m_anchorNode->hasEditableStyle() && !Position::nodeIsUserSelectNone(node.get()) && atEditingBoundary();
}

bool Position::isRenderedCharacter() const
{
    auto* text = dynamicDowncast<Text>(deprecatedNode());
    CheckedPtr renderer = text ? text->renderer() : nullptr;
    return renderer && renderer->containsRenderedCharacterOffset(m_offset);
}

static bool inSameEnclosingBlockFlowElement(Node* a, Node* b)
{
    return a && b && deprecatedEnclosingBlockFlowElement(a) == deprecatedEnclosingBlockFlowElement(b);
}

bool Position::rendersInDifferentPosition(const Position& position) const
{
    if (isNull() || position.isNull())
        return false;

    RefPtr node = deprecatedNode();
    CheckedPtr renderer = node->renderer();
    if (!renderer)
        return false;

    RefPtr positionNode = position.deprecatedNode();
    CheckedPtr positionRenderer = positionNode->renderer();
    if (!positionRenderer)
        return false;

    if (renderer->style().visibility() != Visibility::Visible || positionRenderer->style().visibility() != Visibility::Visible)
        return false;

    if (node == positionNode) {
        if (is<HTMLBRElement>(*node))
            return false;

        if (m_offset == static_cast<unsigned>(position.deprecatedEditingOffset()))
            return false;

        if (!is<Text>(*node))
            return true;
    }

    if (is<HTMLBRElement>(*node) && position.isCandidate())
        return true;

    if (is<HTMLBRElement>(*positionNode) && isCandidate())
        return true;

    if (!inSameEnclosingBlockFlowElement(node.get(), positionNode.get()))
        return true;

    auto* textRenderer = dynamicDowncast<RenderText>(*renderer);
    if (textRenderer && !textRenderer->containsCaretOffset(m_offset))
        return false;

    auto* textPositionRenderer = dynamicDowncast<RenderText>(*positionRenderer);
    if (textPositionRenderer && !textPositionRenderer->containsCaretOffset(position.m_offset))
        return false;

    unsigned thisRenderedOffset = textRenderer ? textRenderer->countRenderedCharacterOffsetsUntil(m_offset) : m_offset;
    unsigned positionRenderedOffset = textPositionRenderer ? textPositionRenderer->countRenderedCharacterOffsetsUntil(position.m_offset) : position.m_offset;

    if (renderer == positionRenderer && thisRenderedOffset == positionRenderedOffset)
        return false;

    auto box1 = inlineBoxAndOffset(Affinity::Downstream).box;
    auto box2 = position.inlineBoxAndOffset(Affinity::Downstream).box;

    LOG(Editing, "renderer:               %p\n", renderer.get());
    LOG(Editing, "thisRenderedOffset:         %d\n", thisRenderedOffset);
    LOG(Editing, "posRenderer:            %p\n", positionRenderer.get());
    LOG(Editing, "posRenderedOffset:      %d\n", positionRenderedOffset);
    LOG(Editing, "node min/max:           %d:%d\n", caretMinOffset(*node), caretMaxOffset(*node));
    LOG(Editing, "pos node min/max:       %d:%d\n", caretMinOffset(*positionNode), caretMaxOffset(*positionNode));
    LOG(Editing, "----------------------------------------------------------------------\n");

    if (!box1 || !box2)
        return false;

    if (box1->lineBox() != box2->lineBox())
        return true;

    if (nextRenderedEditable(node.get()) == positionNode && thisRenderedOffset == static_cast<unsigned>(caretMaxOffset(*positionNode)) && !positionRenderedOffset)
        return false;
    
    if (previousRenderedEditable(node.get()) == positionNode && !thisRenderedOffset && positionRenderedOffset == static_cast<unsigned>(caretMaxOffset(*positionNode)))
        return false;

    return true;
}

// This assumes that it starts in editable content.
Position Position::leadingWhitespacePosition(Affinity affinity, bool considerNonCollapsibleWhitespace) const
{
    ASSERT(isEditablePosition(*this));
    if (isNull())
        return { };
    
    if (is<HTMLBRElement>(*upstream().deprecatedNode()))
        return { };

    Position prev = previousCharacterPosition(affinity);
    RefPtr node = deprecatedNode();
    RefPtr previousNode = prev.deprecatedNode();
    if (prev != *this && inSameEnclosingBlockFlowElement(node.get(), previousNode.get())) {
        if (auto* previousText = dynamicDowncast<Text>(*previousNode)) {
            UChar c = previousText->data()[prev.deprecatedEditingOffset()];
            if (considerNonCollapsibleWhitespace ? (isASCIIWhitespace(c) || c == noBreakSpace) : deprecatedIsCollapsibleWhitespace(c)) {
                if (isEditablePosition(prev))
                    return prev;
            }
        }
    }

    return { };
}

// This assumes that it starts in editable content.
Position Position::trailingWhitespacePosition(Affinity, bool considerNonCollapsibleWhitespace) const
{
    ASSERT(isEditablePosition(*this));
    if (isNull())
        return { };
    
    VisiblePosition v(*this);
    UChar c = v.characterAfter();
    // The space must not be in another paragraph and it must be editable.
    if (!isEndOfParagraph(v) && v.next(CannotCrossEditingBoundary).isNotNull())
        if (considerNonCollapsibleWhitespace ? (isASCIIWhitespace(c) || c == noBreakSpace) : deprecatedIsCollapsibleWhitespace(c))
            return *this;
    
    return { };
}

InlineBoxAndOffset Position::inlineBoxAndOffset(Affinity affinity) const
{
    return inlineBoxAndOffset(affinity, primaryDirection());
}

static bool isNonTextLeafChild(RenderObject& object)
{
    if (auto* renderElement = dynamicDowncast<RenderElement>(object))
        return !renderElement->firstChild();
    return false;
}

static InlineIterator::TextBoxIterator searchAheadForBetterMatch(RenderText& renderer)
{
    CheckedPtr container = renderer.containingBlock();
    CheckedPtr<RenderObject> next = &renderer;
    while ((next = next->nextInPreOrder(container.get()))) {
        if (is<RenderBlock>(next))
            return { };
        if (next->isBR())
            return { };
        if (isNonTextLeafChild(*next))
            return { };
        if (CheckedPtr renderText = dynamicDowncast<RenderText>(*next)) {
            if (auto box = InlineIterator::firstTextBoxInLogicalOrderFor(*renderText).first)
                return box;
        }
    }
    return { };
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

InlineBoxAndOffset Position::inlineBoxAndOffset(Affinity affinity, TextDirection primaryDirection) const
{
    auto caretOffset = static_cast<unsigned>(deprecatedEditingOffset());

    auto node = protectedDeprecatedNode();
    if (!node)
        return { { }, caretOffset };
    auto renderer = node->renderer();
    if (!renderer)
        return { { }, caretOffset };

    InlineIterator::LeafBoxIterator box;

    if (auto* lineBreakRenderer = dynamicDowncast<RenderLineBreak>(*renderer); lineBreakRenderer && lineBreakRenderer->isBR()) {
        if (!caretOffset)
            box = InlineIterator::boxFor(*lineBreakRenderer);
    } else if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(*renderer)) {
        auto textBox = InlineIterator::firstTextBoxFor(*textRenderer);
        InlineIterator::TextBoxIterator candidate;

        for (; textBox; ++textBox) {
            unsigned caretMinOffset = textBox->minimumCaretOffset();
            unsigned caretMaxOffset = textBox->maximumCaretOffset();

            if (caretOffset < caretMinOffset || caretOffset > caretMaxOffset || (caretOffset == caretMaxOffset && textBox->isLineBreak()))
                continue;

            if (caretOffset > caretMinOffset && caretOffset < caretMaxOffset)
                return { textBox, caretOffset };

            if ((caretOffset == caretMaxOffset) ^ (affinity == Affinity::Downstream))
                break;

            if ((caretOffset == caretMinOffset) ^ (affinity == Affinity::Upstream))
                break;

            if (caretOffset == caretMaxOffset) {
                auto nextOnLine = textBox->nextOnLine();
                if (nextOnLine && nextOnLine->isLineBreak())
                    break;
            }

            candidate = textBox;
        }

        if (candidate && !candidate->nextTextBox() && affinity == Affinity::Downstream) {
            textBox = searchAheadForBetterMatch(*textRenderer);
            if (textBox)
                caretOffset = textBox->minimumCaretOffset();
        }

        box = textBox ? textBox : candidate;
    } else {
        RefPtr node = deprecatedNode();
        if (canHaveChildrenForEditing(*node)) {
            CheckedPtr renderBlockFlow = dynamicDowncast<RenderBlockFlow>(*renderer);
            if (renderBlockFlow && hasRenderedNonAnonymousDescendantsWithHeight(*renderBlockFlow)) {
                // Try a visually equivalent position with possibly opposite editability. This helps in case |this| is in
                // an editable block but surrounded by non-editable positions. It acts to negate the logic at the beginning
                // of RenderObject::createVisiblePosition().
                Position equivalent = downstreamIgnoringEditingBoundaries(*this);
                if (equivalent == *this) {
                    equivalent = upstreamIgnoringEditingBoundaries(*this);
                    // FIXME: Can returning nullptr really be correct here?
                    if (equivalent == *this || downstreamIgnoringEditingBoundaries(equivalent) == *this)
                        return { { }, caretOffset };
                }

                return equivalent.inlineBoxAndOffset(Affinity::Upstream, primaryDirection);
            }
        }
        if (auto* renderBox = dynamicDowncast<RenderBox>(*renderer)) {
            box = InlineIterator::boxFor(*renderBox);
            if (box && caretOffset > box->minimumCaretOffset() && caretOffset < box->maximumCaretOffset())
                return { box, caretOffset };
        }
    }

    if (!box)
        return { { }, caretOffset };

    unsigned char level = box->bidiLevel();

    if (box->direction() == primaryDirection) {
        if (caretOffset == box->rightmostCaretOffset()) {
            auto nextBox = box->nextOnLine();
            if (!nextBox || nextBox->bidiLevel() >= level)
                return { box, caretOffset };

            level = nextBox->bidiLevel();

            auto previousRun = box->previousOnLine();
            for (; previousRun; previousRun.traversePreviousOnLine()) {
                if (previousRun->bidiLevel() <= level)
                    break;
            }

            if (previousRun && previousRun->bidiLevel() == level) // For example, abc FED 123 ^ CBA
                return { box, caretOffset };

            // For example, abc 123 ^ CBA
            for (; nextBox; nextBox.traverseNextOnLine()) {
                if (nextBox->bidiLevel() < level)
                    break;
                box = nextBox;
            }
            caretOffset = box->rightmostCaretOffset();
        } else {
            auto previousRun = box->previousOnLine();
            if (!previousRun || previousRun->bidiLevel() >= level)
                return { box, caretOffset };

            level = previousRun->bidiLevel();

            auto nextBox = box->nextOnLine();
            for (; nextBox; nextBox.traverseNextOnLine()) {
                if (nextBox->bidiLevel() <= level)
                    break;
            }

            if (nextBox && nextBox->bidiLevel() == level)
                return { box, caretOffset };

            for (; previousRun; previousRun.traversePreviousOnLine()) {
                if (previousRun->bidiLevel() < level)
                    break;
                box = previousRun;
            }

            caretOffset = box->leftmostCaretOffset();
        }
        return { box, caretOffset };
    }

    if (caretOffset == box->leftmostCaretOffset()) {
        auto previousRun = box->previousOnLineIgnoringLineBreak();
        if (!previousRun || previousRun->bidiLevel() < level) {
            // Left edge of a secondary box. Set to the right edge of the entire box.
            for (auto nextBox = box->nextOnLineIgnoringLineBreak(); nextBox; nextBox.traverseNextOnLineIgnoringLineBreak()) {
                if (nextBox->bidiLevel() < level)
                    break;
                box = nextBox;
            }
            caretOffset = box->rightmostCaretOffset();
        } else if (previousRun->bidiLevel() > level) {
            // Right edge of a "tertiary" box. Set to the left edge of that box.
            for (auto tertiaryRun = box->previousOnLineIgnoringLineBreak(); tertiaryRun; tertiaryRun.traversePreviousOnLineIgnoringLineBreak()) {
                if (tertiaryRun->bidiLevel() <= level)
                    break;
                box = tertiaryRun;
            }
            caretOffset = box->leftmostCaretOffset();
        }
    } else {
        auto nextBox = box->nextOnLineIgnoringLineBreak();
        if (!nextBox || nextBox->bidiLevel() < level) {
            // Right edge of a secondary box. Set to the left edge of the entire box.
            for (auto previousRun = box->previousOnLineIgnoringLineBreak(); previousRun; previousRun.traversePreviousOnLineIgnoringLineBreak()) {
                if (previousRun->bidiLevel() < level)
                    break;
                box = previousRun;
            }
            caretOffset = box->leftmostCaretOffset();
        } else if (nextBox->bidiLevel() > level) {
            // Left edge of a "tertiary" box. Set to the right edge of that box.
            for (auto tertiaryRun = box->nextOnLineIgnoringLineBreak(); tertiaryRun; tertiaryRun.traverseNextOnLineIgnoringLineBreak()) {
                if (tertiaryRun->bidiLevel() <= level)
                    break;
                box = tertiaryRun;
            }
            caretOffset = box->rightmostCaretOffset();
        }
    }

    return { box, caretOffset };
}

TextDirection Position::primaryDirection() const
{
    if (!m_anchorNode || !m_anchorNode->renderer())
        return TextDirection::LTR;
    if (auto* blockFlow = lineageOfType<RenderBlockFlow>(*m_anchorNode->renderer()).first())
        return blockFlow->style().writingMode().bidiDirection();
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

String Position::debugDescription() const
{
    if (isNull())
        return "<null>"_s;
    return makeString("offset "_s, m_offset, " of "_s, deprecatedNode()->debugDescription());
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

Node* commonInclusiveAncestor(const Position& a, const Position& b)
{
    RefPtr nodeA = a.containerNode();
    RefPtr nodeB = b.containerNode();
    if (!nodeA || !nodeB)
        return nullptr;
    return commonInclusiveAncestor<ComposedTree>(*nodeA, *nodeB);
}

Position positionInParentBeforeNode(Node* node)
{
    RefPtr currentNode = node;
    RefPtr ancestor = node->parentNode();
    while (ancestor && editingIgnoresContent(*ancestor)) {
        currentNode = ancestor;
        ancestor = ancestor->parentNode();
    }
    ASSERT(ancestor);
    return Position(ancestor, currentNode->computeNodeIndex(), Position::PositionIsOffsetInAnchor);
}

Position positionInParentAfterNode(Node* node)
{
    RefPtr currentNode = node;
    RefPtr ancestor = node->parentNode();
    while (ancestor && editingIgnoresContent(*ancestor)) {
        currentNode = ancestor;
        ancestor = ancestor->parentNode();
    }
    ASSERT(ancestor);
    return Position(ancestor, currentNode->computeNodeIndex() + 1, Position::PositionIsOffsetInAnchor);
}

Position makeContainerOffsetPosition(const BoundaryPoint& point)
{
    return makeContainerOffsetPosition(point.container.copyRef(), point.offset);
}

Position makeDeprecatedLegacyPosition(const BoundaryPoint& point)
{
    return makeDeprecatedLegacyPosition(point.container.copyRef(), point.offset);
}

std::optional<BoundaryPoint> makeBoundaryPoint(const Position& position)
{
    RefPtr container { position.containerNode() };
    if (!container)
        return std::nullopt;
    return BoundaryPoint { container.releaseNonNull(), static_cast<unsigned>(position.computeOffsetInContainerNode()) };
}

template<TreeType treeType> std::partial_ordering treeOrder(const Position& a, const Position& b)
{
    if (a.isNull() || b.isNull())
        return a.isNull() && b.isNull() ? std::partial_ordering::equivalent : std::partial_ordering::unordered;

    auto aContainer = a.containerNode();
    auto bContainer = b.containerNode();

    if (!aContainer || !bContainer) {
        if (!commonInclusiveAncestor<treeType>(*a.anchorNode(), *b.anchorNode()))
            return std::partial_ordering::unordered;
        if (!aContainer && !bContainer && a.anchorType() == b.anchorType())
            return std::partial_ordering::equivalent;
        if (bContainer)
            return a.anchorType() == Position::PositionIsBeforeAnchor ? std::partial_ordering::less : std::partial_ordering::greater;
        return b.anchorType() == Position::PositionIsBeforeAnchor ? std::partial_ordering::greater : std::partial_ordering::less;
    }

    // FIXME: Avoid computing node offset for cases where we don't need to.

    return treeOrder<treeType>(*makeBoundaryPoint(a), *makeBoundaryPoint(b));
}

std::partial_ordering documentOrder(const Position& a, const Position& b)
{
    return treeOrder<ComposedTree>(a, b);
}

template std::partial_ordering treeOrder<ComposedTree>(const Position&, const Position&);
template std::partial_ordering treeOrder<ShadowIncludingTree>(const Position&, const Position&);

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
