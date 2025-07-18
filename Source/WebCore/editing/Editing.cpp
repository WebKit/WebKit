/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
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
#include "Editing.h"

#include "AXObjectCache.h"
#include "CachedImage.h"
#include "ContainerNodeInlines.h"
#include "DocumentInlines.h"
#include "EditingInlines.h"
#include "Editor.h"
#include "ElementChildIteratorInlines.h"
#include "ElementInlines.h"
#include "GraphicsLayer.h"
#include "HTMLBodyElement.h"
#include "HTMLDListElement.h"
#include "HTMLDivElement.h"
#include "HTMLElementFactory.h"
#include "HTMLImageElement.h"
#include "HTMLInterchange.h"
#include "HTMLLIElement.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLParagraphElement.h"
#include "HTMLPictureElement.h"
#include "HTMLSpanElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextFormControlElement.h"
#include "HTMLUListElement.h"
#include "HitTestSource.h"
#include "ImageOverlay.h"
#include "LocalFrame.h"
#include "NodeTraversal.h"
#include "PositionIterator.h"
#include "Range.h"
#include "RenderBlock.h"
#include "RenderElement.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderObjectInlines.h"
#include "RenderStyleInlines.h"
#include "RenderTableCell.h"
#include "RenderTextControlSingleLine.h"
#include "RenderedPosition.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include "WritingMode.h"
#include <wtf/Assertions.h>
#include <wtf/IterationStatus.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace HTMLNames;

static bool isVisiblyAdjacent(const Position&, const Position&);

bool canHaveChildrenForEditing(const Node& node)
{
    return !is<Text>(node) && !is<HTMLPictureElement>(node) && node.canContainRangeEndPoint();
}

// Atomic means that the node has no children, or has children which are ignored for the purposes of editing.
bool isAtomicNode(const Node* node)
{
    return node && (!node->hasChildNodes() || editingIgnoresContent(*node));
}

RefPtr<ContainerNode> highestEditableRoot(const Position& position, EditableType editableType)
{
    RefPtr<ContainerNode> highestEditableRoot = editableRootForPosition(position, editableType);
    if (!highestEditableRoot)
        return nullptr;

    for (RefPtr<ContainerNode> node = highestEditableRoot; !is<HTMLBodyElement>(*node); ) {
        node = node->parentNode();
        if (!node)
            break;
        // FIXME: Can this ever be a Document or DocumentFragment? If not, this should return Element* instead.
        if (hasEditableStyle(*node, editableType))
            highestEditableRoot = node;
    }

    return highestEditableRoot;
}

Element* lowestEditableAncestor(Node* node)
{
    for (; node; node = node->parentNode()) {
        if (node->hasEditableStyle())
            return node->rootEditableElement();
        if (is<HTMLBodyElement>(*node))
            break;
    }
    return nullptr;
}

static bool isEditableToAccessibility(const Node& node)
{
    ASSERT(AXObjectCache::accessibilityEnabled());
    ASSERT(node.document().existingAXObjectCache());

    if (CheckedPtr cache = node.document().existingAXObjectCache())
        return cache->rootAXEditableElement(&node);

    return false;
}

static bool computeEditability(const Node& node, EditableType editableType, Node::ShouldUpdateStyle shouldUpdateStyle)
{
    if (node.computeEditability(Node::UserSelectAllTreatment::NotEditable, shouldUpdateStyle) != Node::Editability::ReadOnly)
        return true;

    switch (editableType) {
    case ContentIsEditable:
        return false;
    case HasEditableAXRole:
        return isEditableToAccessibility(node);
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool hasEditableStyle(const Node& node, EditableType editableType)
{
    return computeEditability(node, editableType, Node::ShouldUpdateStyle::DoNotUpdate);
}

bool isEditableNode(const Node& node)
{
    return computeEditability(node, ContentIsEditable, Node::ShouldUpdateStyle::Update);
}

bool isEditablePosition(const Position& position, EditableType editableType)
{
    RefPtr node = position.containerNode();
    return node && computeEditability(*node, editableType, Node::ShouldUpdateStyle::Update);
}

bool isAtUnsplittableElement(const Position& position)
{
    Node* node = position.containerNode();
    return node == editableRootForPosition(position) || node == enclosingNodeOfType(position, isTableCell);
}

bool isRichlyEditablePosition(const Position& position)
{
    RefPtr node = position.containerNode();
    return node && node->hasRichlyEditableStyle();
}

Element* editableRootForPosition(const Position& position, EditableType editableType)
{
    RefPtr node = position.containerNode();
    if (!node)
        return nullptr;

    switch (editableType) {
    case HasEditableAXRole:
        if (CheckedPtr cache = node->document().existingAXObjectCache())
            return const_cast<Element*>(cache->rootAXEditableElement(node.get()));
        [[fallthrough]];
    case ContentIsEditable:
        return node->rootEditableElement();
    }
    return nullptr;
}

// Finds the enclosing element until which the tree can be split.
// When a user hits ENTER, he/she won't expect this element to be split into two.
// You may pass it as the second argument of splitTreeToNode.
Element* unsplittableElementForPosition(const Position& position)
{
    // Since enclosingNodeOfType won't search beyond the highest root editable node,
    // this code works even if the closest table cell was outside of the root editable node.
    if (auto enclosingCell = downcast<Element>(enclosingNodeOfType(position, &isTableCell)))
        return enclosingCell.get();
    return editableRootForPosition(position);
}

Position nextCandidate(const Position& position)
{
    for (PositionIterator nextPosition = position; !nextPosition.atEnd(); ) {
        nextPosition.increment();
        if (nextPosition.isCandidate())
            return nextPosition;
    }
    return { };
}

Position nextVisuallyDistinctCandidate(const Position& position, SkipDisplayContents skipDisplayContents)
{
    // FIXME: Use PositionIterator instead.
    Position nextPosition = position;
    Position downstreamStart = nextPosition.downstream();
    while (!nextPosition.atEndOfTree()) {
        nextPosition = nextPosition.next(Character);
        if (nextPosition.isCandidate() && nextPosition.downstream() != downstreamStart)
            return nextPosition;
        if (RefPtr node = nextPosition.containerNode()) {
            if (!node->renderer()) {
                if (skipDisplayContents == SkipDisplayContents::No) {
                    if (auto element = dynamicDowncast<Element>(node); element && element->hasDisplayContents())
                        continue;
                }
                nextPosition = lastPositionInOrAfterNode(node.get());
            }
        }
    }
    return { };
}

Position previousCandidate(const Position& position)
{
    PositionIterator previousPosition = position;
    while (!previousPosition.atStart()) {
        previousPosition.decrement();
        if (previousPosition.isCandidate())
            return previousPosition;
    }
    return { };
}

Position previousVisuallyDistinctCandidate(const Position& position)
{
    // FIXME: Use PositionIterator instead.
    Position previousPosition = position;
    Position downstreamStart = previousPosition.downstream();
    while (!previousPosition.atStartOfTree()) {
        previousPosition = previousPosition.previous(Character);
        if (previousPosition.isCandidate() && previousPosition.downstream() != downstreamStart)
            return previousPosition;
        if (RefPtr node = previousPosition.containerNode()) {
            if (!node->renderer())
                previousPosition = firstPositionInOrBeforeNode(node.get());
        }
    }
    return { };
}

Position firstEditablePositionAfterPositionInRoot(const Position& position, ContainerNode* highestRoot)
{
    if (!highestRoot)
        return { };

    // position falls before highestRoot.
    if (position < firstPositionInNode(highestRoot) && highestRoot->hasEditableStyle())
        return firstPositionInNode(highestRoot);

    Position candidate = position;

    if (&position.deprecatedNode()->treeScope() != &highestRoot->treeScope()) {
        RefPtr shadowAncestor = highestRoot->treeScope().ancestorNodeInThisScope(position.protectedDeprecatedNode().get());
        if (!shadowAncestor)
            return { };

        candidate = positionAfterNode(shadowAncestor.get());
    }

    while (candidate.deprecatedNode() && !isEditablePosition(candidate) && candidate.protectedDeprecatedNode()->isDescendantOf(*highestRoot))
        candidate = isAtomicNode(candidate.deprecatedNode()) ? positionInParentAfterNode(candidate.protectedDeprecatedNode().get()) : nextVisuallyDistinctCandidate(candidate);

    if (candidate.deprecatedNode() && !candidate.protectedDeprecatedNode()->isInclusiveDescendantOf(*highestRoot))
        return { };

    return candidate;
}

Position lastEditablePositionBeforePositionInRoot(const Position& position, ContainerNode* highestRoot)
{
    if (!highestRoot)
        return { };

    // When position falls after highestRoot, the result is easy to compute.
    if (position > lastPositionInNode(highestRoot))
        return lastPositionInNode(highestRoot);

    Position candidate = position;

    if (&position.deprecatedNode()->treeScope() != &highestRoot->treeScope()) {
        RefPtr shadowAncestor = highestRoot->treeScope().ancestorNodeInThisScope(position.protectedDeprecatedNode().get());
        if (!shadowAncestor)
            return { };

        candidate = firstPositionInOrBeforeNode(shadowAncestor.get());
    }

    while (candidate.deprecatedNode() && !isEditablePosition(candidate) && candidate.protectedDeprecatedNode()->isDescendantOf(*highestRoot))
        candidate = isAtomicNode(candidate.deprecatedNode()) ? positionInParentBeforeNode(candidate.protectedDeprecatedNode().get()) : previousVisuallyDistinctCandidate(candidate);
    
    if (candidate.deprecatedNode() && !candidate.protectedDeprecatedNode()->isInclusiveDescendantOf(*highestRoot))
        return { };
    
    return candidate;
}

// FIXME: The function name, comment, and code say three different things here!
// Whether or not content before and after this node will collapse onto the same line as it.
bool isBlock(const Node& node)
{
    return node.renderer() && !node.renderer()->isInline();
}

bool isInline(const Node& node)
{
    return node.renderer() && node.renderer()->isInline();
}

// FIXME: Deploy this in all of the places where enclosingBlockFlow/enclosingBlockFlowOrTableElement are used.
// FIXME: Pass a position to this function. The enclosing block of [table, x] for example, should be the 
// block that contains the table and not the table, and this function should be the only one responsible for 
// knowing about these kinds of special cases.
RefPtr<Element> enclosingBlock(RefPtr<Node> node, EditingBoundaryCrossingRule rule)
{
    return dynamicDowncast<Element>(enclosingNodeOfType(firstPositionInOrBeforeNode(node.get()), isBlock, rule));
}

TextDirection directionOfEnclosingBlock(const Position& position)
{
    auto block = enclosingBlock(position.protectedContainerNode());
    if (!block)
        return TextDirection::LTR;
    auto renderer = block->renderer();
    if (!renderer)
        return TextDirection::LTR;
    return renderer->writingMode().bidiDirection();
}

// This method is used to create positions in the DOM. It returns the maximum valid offset
// in a node. It returns 1 for some elements even though they do not have children, which
// creates technically invalid DOM Positions. Be sure to call parentAnchoredEquivalent
// on a Position before using it to create a DOM Range, or an exception will be thrown.
int lastOffsetForEditing(const Node& node)
{
    if (node.isCharacterDataNode() || node.hasChildNodes())
        return node.length();

    // FIXME: Might be more helpful to return 1 for any node where editingIgnoresContent is true, even one that happens to have child nodes, like a select element with option node children.
    return editingIgnoresContent(node) ? 1 : 0;
}

bool isAmbiguousBoundaryCharacter(char16_t character)
{
    // These are characters that can behave as word boundaries, but can appear within words.
    // If they are just typed, i.e. if they are immediately followed by a caret, we want to delay text checking until the next character has been typed.
    // FIXME: this is required until <rdar://problem/6853027> is fixed and text checking can do this for us.
    return character == '\'' || character == '@' || character == rightSingleQuotationMark || character == hebrewPunctuationGershayim;
}

String stringWithRebalancedWhitespace(const String& string, bool startIsStartOfParagraph, bool shouldEmitNBSPbeforeEnd)
{
    StringBuilder rebalancedString;

    bool previousCharacterWasSpace = false;
    unsigned length = string.length();
    for (unsigned i = 0; i < length; ++i) {
        auto character = string[i];
        if (!deprecatedIsEditingWhitespace(character)) {
            previousCharacterWasSpace = false;
            continue;
        }
        LChar selectedWhitespaceCharacter;
        // We need to ensure there is no next sibling text node. See https://bugs.webkit.org/show_bug.cgi?id=123163
        if (previousCharacterWasSpace || (!i && startIsStartOfParagraph) || (i == length - 1 && shouldEmitNBSPbeforeEnd)) {
            selectedWhitespaceCharacter = noBreakSpace;
            previousCharacterWasSpace = false;
        } else {
            selectedWhitespaceCharacter = ' ';
            previousCharacterWasSpace = true;
        }
        if (character == selectedWhitespaceCharacter)
            continue;
        rebalancedString.reserveCapacity(length);
        rebalancedString.appendSubstring(string, rebalancedString.length(), i - rebalancedString.length());
        rebalancedString.append(selectedWhitespaceCharacter);
    }

    if (rebalancedString.isEmpty())
        return string;

    rebalancedString.reserveCapacity(length);
    rebalancedString.appendSubstring(string, rebalancedString.length(), length - rebalancedString.length());
    return rebalancedString.toString();
}

bool isTableStructureNode(const Node& node)
{
    auto* renderer = node.renderer();
    return renderer && (renderer->isRenderTableCell() || renderer->isRenderTableRow() || renderer->isRenderTableSection() || renderer->isRenderTableCol());
}

const String& nonBreakingSpaceString()
{
    static NeverDestroyed<String> nonBreakingSpaceString(span(noBreakSpace));
    return nonBreakingSpaceString;
}

RefPtr<Element> isFirstPositionAfterTable(const VisiblePosition& position)
{
    Position upstream(position.deepEquivalent().upstream());
    RefPtr node = upstream.deprecatedNode();
    if (!node)
        return nullptr;
    auto* renderer = node->renderer();
    if (!renderer || !renderer->isRenderTable() || !upstream.atLastEditingPositionForNode())
        return nullptr;
    return downcast<Element>(node.releaseNonNull());
}

RefPtr<Element> isLastPositionBeforeTable(const VisiblePosition& position)
{
    Position downstream(position.deepEquivalent().downstream());
    RefPtr node = downstream.deprecatedNode();
    if (!node)
        return nullptr;
    auto* renderer = node->renderer();
    if (!renderer || !renderer->isRenderTable() || !downstream.atFirstEditingPositionForNode())
        return nullptr;
    return downcast<Element>(node.releaseNonNull());
}

// Returns the visible position at the beginning of a node
VisiblePosition visiblePositionBeforeNode(Node& node)
{
    if (node.hasChildNodes())
        return VisiblePosition(firstPositionInOrBeforeNode(&node));
    ASSERT(node.parentNode());
    ASSERT(!node.parentNode()->isShadowRoot());
    return positionInParentBeforeNode(&node);
}

// Returns the visible position at the ending of a node
VisiblePosition visiblePositionAfterNode(Node& node)
{
    if (node.hasChildNodes())
        return VisiblePosition(lastPositionInOrAfterNode(&node));
    ASSERT(node.parentNode());
    ASSERT(!node.parentNode()->isShadowRoot());
    return positionInParentAfterNode(&node);
}

VisiblePosition closestEditablePositionInElementForAbsolutePoint(const Element& element, const IntPoint& point)
{
    if (!element.isConnected() || !element.document().frame())
        return { };

    Ref<const Element> protectedElement { element };
    element.protectedDocument()->updateLayoutIgnorePendingStylesheets();

    CheckedPtr renderer = element.renderer();
    // Look at the inner element of a form control, not the control itself, as it is the editable part.
    if (RefPtr formControlElement = dynamicDowncast<HTMLTextFormControlElement>(element)) {
        if (!formControlElement->isInnerTextElementEditable())
            return { };
        if (RefPtr innerTextElement = formControlElement->innerTextElement())
            renderer = innerTextElement->renderer();
    }
    if (!renderer)
        return { };
    auto absoluteBoundingBox = renderer->absoluteBoundingBoxRect();
    auto constrainedAbsolutePoint = point.constrainedBetween(absoluteBoundingBox.minXMinYCorner(), absoluteBoundingBox.maxXMaxYCorner());
    auto localPoint = renderer->absoluteToLocal(constrainedAbsolutePoint, UseTransforms);
    auto visiblePosition = renderer->positionForPoint(flooredLayoutPoint(localPoint), HitTestSource::User, nullptr);
    return isEditablePosition(visiblePosition.deepEquivalent()) ? visiblePosition : VisiblePosition { };
}

bool isListHTMLElement(Node* node)
{
    return node && (is<HTMLUListElement>(*node) || is<HTMLOListElement>(*node) || is<HTMLDListElement>(*node));
}

bool isListItem(const Node& node)
{
    return isListHTMLElement(node.parentNode()) || (node.renderer() && node.renderer()->isRenderListItem());
}

Element* enclosingElementWithTag(const Position& position, const QualifiedName& tagName)
{
    auto root = highestEditableRoot(position);
    for (RefPtr node = position.deprecatedNode(); node; node = node->parentNode()) {
        if (root && !node->hasEditableStyle())
            continue;
        auto* element = dynamicDowncast<Element>(*node);
        if (!element)
            continue;
        if (element->hasTagName(tagName))
            return element;
        if (node == root)
            return nullptr;
    }
    return nullptr;
}

RefPtr<Node> enclosingNodeOfType(const Position& position, bool (*nodeIsOfType)(const Node&), EditingBoundaryCrossingRule rule)
{
    // FIXME: support CanSkipCrossEditingBoundary
    ASSERT(rule == CanCrossEditingBoundary || rule == CannotCrossEditingBoundary);
    auto root = rule == CannotCrossEditingBoundary ? highestEditableRoot(position) : nullptr;
    for (RefPtr n = position.deprecatedNode(); n; n = n->parentNode()) {
        // Don't return a non-editable node if the input position was editable, since
        // the callers from editing will no doubt want to perform editing inside the returned node.
        if (root && !n->hasEditableStyle())
            continue;
        if (nodeIsOfType(*n))
            return n;
        if (n == root)
            return nullptr;
    }
    return nullptr;
}

RefPtr<Node> highestEnclosingNodeOfType(const Position& position, bool (*nodeIsOfType)(const Node&), EditingBoundaryCrossingRule rule, Node* stayWithin)
{
    RefPtr<Node> highest;
    RefPtr root = rule == CannotCrossEditingBoundary ? highestEditableRoot(position) : nullptr;
    for (RefPtr<Node> n = position.containerNode(); n && n != stayWithin; n = n->parentNode()) {
        if (root && !n->hasEditableStyle())
            continue;
        if (nodeIsOfType(*n))
            highest = n.get();
        if (n == root)
            break;
    }
    return highest;
}

static bool hasARenderedDescendant(Node* node, Node* excludedNode)
{
    for (RefPtr n = node->firstChild(); n;) {
        if (n == excludedNode) {
            n = NodeTraversal::nextSkippingChildren(*n, node);
            continue;
        }
        if (n->renderer())
            return true;
        n = NodeTraversal::next(*n, node);
    }
    return false;
}

RefPtr<Node> highestNodeToRemoveInPruning(Node* node)
{
    RefPtr<Node> previousNode;
    RefPtr rootEditableElement = node ? node->rootEditableElement() : nullptr;
    for (RefPtr currentNode = node; currentNode; currentNode = currentNode->parentNode()) {
        if (auto* renderer = currentNode->renderer()) {
            if (!renderer->canHaveChildren() || hasARenderedDescendant(currentNode.get(), previousNode.get()) || rootEditableElement == currentNode.get())
                return previousNode;
        }
        previousNode = currentNode.get();
    }
    return nullptr;
}

RefPtr<Element> enclosingTableCell(const Position& position)
{
    return downcast<Element>(enclosingNodeOfType(position, isTableCell));
}

RefPtr<Element> enclosingAnchorElement(const Position& p)
{
    for (RefPtr node = p.deprecatedNode(); node; node = node->parentNode()) {
        if (RefPtr element = dynamicDowncast<Element>(*node); element && element->isLink())
            return element;
    }
    return nullptr;
}

RefPtr<HTMLElement> enclosingList(Node* node)
{
    if (!node)
        return nullptr;
        
    RefPtr root = highestEditableRoot(firstPositionInOrBeforeNode(node));
    
    for (RefPtr ancestor = node->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        auto* htmlElement = dynamicDowncast<HTMLElement>(*ancestor);
        if (htmlElement && (is<HTMLUListElement>(*htmlElement) || is<HTMLOListElement>(*htmlElement)))
            return htmlElement;
        if (ancestor == root)
            return nullptr;
    }
    
    return nullptr;
}

RefPtr<Node> enclosingListChild(Node* node)
{
    if (!node)
        return nullptr;

    // Check for a list item element, or for a node whose parent is a list element. Such a node
    // will appear visually as a list item (but without a list marker)
    RefPtr root = highestEditableRoot(firstPositionInOrBeforeNode(node));
    
    // FIXME: This function is inappropriately named since it starts with node instead of node->parentNode()
    for (RefPtr n = node; n && n->parentNode(); n = n->parentNode()) {
        if (is<HTMLLIElement>(*n) || (isListHTMLElement(n->parentNode()) && n != root))
            return n;
        if (n == root || isTableCell(*n))
            return nullptr;
    }

    return nullptr;
}

// FIXME: This function should not need to call isStartOfParagraph/isEndOfParagraph.
RefPtr<Node> enclosingEmptyListItem(const VisiblePosition& position)
{
    // Check that position is on a line by itself inside a list item
    RefPtr listChildNode = enclosingListChild(position.deepEquivalent().protectedDeprecatedNode().get());
    if (!listChildNode || !isStartOfParagraph(position) || !isEndOfParagraph(position))
        return nullptr;

    VisiblePosition firstInListChild(firstPositionInOrBeforeNode(listChildNode.get()));
    VisiblePosition lastInListChild(lastPositionInOrAfterNode(listChildNode.get()));

    if (firstInListChild != position || lastInListChild != position)
        return nullptr;

    return listChildNode;
}

RefPtr<HTMLElement> outermostEnclosingList(Node* node, Node* rootList)
{
    RefPtr list = enclosingList(node);
    if (!list)
        return nullptr;

    while (RefPtr nextList = enclosingList(list.get())) {
        if (nextList == rootList)
            break;
        list = WTFMove(nextList);
    }

    return list;
}

bool canMergeLists(Element* firstList, Element* secondList)
{
    auto* first = dynamicDowncast<HTMLElement>(firstList);
    auto* second = dynamicDowncast<HTMLElement>(secondList);
    if (!first || !second)
        return false;

    return first->localName() == second->localName() // make sure the list types match (ol vs. ul)
        && first->hasEditableStyle() && second->hasEditableStyle() // both lists are editable
        && first->rootEditableElement() == second->rootEditableElement() // don't cross editing boundaries
        // Make sure there is no visible content between this li and the previous list.
        && isVisiblyAdjacent(positionInParentAfterNode(first), positionInParentBeforeNode(second));
}

static Node* previousNodeConsideringAtomicNodes(const Node* node)
{
    if (node->previousSibling()) {
        Node* n = node->previousSibling();
        while (!isAtomicNode(n) && n->lastChild())
            n = n->lastChild();
        return n;
    }
    
    return node->parentNode();
}

static Node* nextNodeConsideringAtomicNodes(const Node* node)
{
    if (!isAtomicNode(node) && node->firstChild())
        return node->firstChild();
    if (node->nextSibling())
        return node->nextSibling();
    RefPtr<const Node> n = node;
    while (n && !n->nextSibling())
        n = n->parentNode();
    if (n)
        return n->nextSibling();
    return nullptr;
}

Node* previousLeafNode(const Node* node)
{
    while ((node = previousNodeConsideringAtomicNodes(node))) {
        if (isAtomicNode(node))
            return const_cast<Node*>(node);
    }
    return nullptr;
}

Node* nextLeafNode(const Node* node)
{
    while ((node = nextNodeConsideringAtomicNodes(node))) {
        if (isAtomicNode(node))
            return const_cast<Node*>(node);
    }
    return nullptr;
}

// FIXME: Do not require renderer, so that this can be used within fragments.
bool isRenderedTable(const Node* node)
{
    RefPtr element = dynamicDowncast<HTMLElement>(node);
    if (!element)
        return false;
    auto* renderer = element->renderer();
    return renderer && renderer->isRenderTable();
}

bool isTableCell(const Node& node)
{
    auto* renderer = node.renderer();
    if (!renderer)
        return node.hasTagName(tdTag) || node.hasTagName(thTag);
    return renderer->isRenderTableCell();
}

bool isEmptyTableCell(const Node* node)
{
    // Returns true IFF the passed in node is one of:
    //   .) a table cell with no children,
    //   .) a table cell with a single BR child, and which has no other child renderers, including :before and :after renderers
    //   .) the BR child of such a table cell

    // Find rendered node
    while (node && !node->renderer())
        node = node->parentNode();
    if (!node)
        return false;

    // Make sure the rendered node is a table cell or <br>.
    // If it's a <br>, then the parent node has to be a table cell.
    CheckedPtr renderer = node->renderer();
    if (renderer->isBR()) {
        renderer = renderer->parent();
        if (!renderer)
            return false;
    }
    auto* renderTableCell = dynamicDowncast<RenderTableCell>(*renderer);
    if (!renderTableCell)
        return false;

    // Check that the table cell contains no child renderers except for perhaps a single <br>.
    CheckedPtr childRenderer = renderTableCell->firstChild();
    if (!childRenderer)
        return true;
    if (!childRenderer->isBR())
        return false;
    return !childRenderer->nextSibling();
}

Ref<HTMLElement> createDefaultParagraphElement(Document& document)
{
    switch (document.editor().defaultParagraphSeparator()) {
    case EditorParagraphSeparator::div:
        return HTMLDivElement::create(document);
    case EditorParagraphSeparator::p:
        break;
    }
    return HTMLParagraphElement::create(document);
}

Ref<HTMLElement> createHTMLElement(Document& document, const QualifiedName& name)
{
    return HTMLElementFactory::createElement(name, document);
}

Ref<HTMLElement> createHTMLElement(Document& document, const AtomString& tagName)
{
    return createHTMLElement(document, QualifiedName(nullAtom(), tagName, xhtmlNamespaceURI));
}

HTMLSpanElement* tabSpanNode(Node* node)
{
    if (auto* span = dynamicDowncast<HTMLSpanElement>(node); span && span->attributeWithoutSynchronization(classAttr) == AppleTabSpanClass)
        return span;
    return nullptr;
}

HTMLSpanElement* parentTabSpanNode(Node* node)
{
    return is<Text>(node) ? tabSpanNode(node->parentNode()) : nullptr;
}

static Ref<Element> createTabSpanElement(Document& document, Text& tabTextNode)
{
    auto spanElement = HTMLSpanElement::create(document);

    spanElement->setAttributeWithoutSynchronization(classAttr, AppleTabSpanClass);
    spanElement->setAttribute(styleAttr, "white-space:pre"_s);

    spanElement->appendChild(tabTextNode);

    return spanElement;
}

Ref<Element> createTabSpanElement(Document& document, String&& tabText)
{
    return createTabSpanElement(document, document.createTextNode(WTFMove(tabText)));
}

Ref<Element> createTabSpanElement(Document& document)
{
    return createTabSpanElement(document, document.createEditingTextNode("\t"_s));
}

unsigned numEnclosingMailBlockquotes(const Position& position)
{
    unsigned count = 0;
    for (RefPtr node = position.deprecatedNode(); node; node = node->parentNode()) {
        if (isMailBlockquote(*node))
            ++count;
    }
    return count;
}

void updatePositionForNodeRemoval(Position& position, Node& node)
{
    if (position.isNull())
        return;
    switch (position.anchorType()) {
    case Position::PositionIsBeforeChildren:
        if (node.isShadowIncludingInclusiveAncestorOf(position.containerNode()))
            position = positionInParentBeforeNode(&node);
        break;
    case Position::PositionIsAfterChildren:
        if (node.isShadowIncludingInclusiveAncestorOf(position.containerNode()))
            position = positionInParentBeforeNode(&node);
        break;
    case Position::PositionIsOffsetInAnchor:
        if (position.containerNode() == node.parentNode() && static_cast<unsigned>(position.offsetInContainerNode()) > node.computeNodeIndex())
            position.moveToOffset(position.offsetInContainerNode() - 1);
        else if (node.isShadowIncludingInclusiveAncestorOf(position.containerNode()))
            position = positionInParentBeforeNode(&node);
        break;
    case Position::PositionIsAfterAnchor:
        if (node.isShadowIncludingInclusiveAncestorOf(position.anchorNode()))
            position = positionInParentAfterNode(&node);
        break;
    case Position::PositionIsBeforeAnchor:
        if (node.isShadowIncludingInclusiveAncestorOf(position.anchorNode()))
            position = positionInParentBeforeNode(&node);
        break;
    }
}

bool isMailBlockquote(const Node& node)
{
    auto* htmlElement = dynamicDowncast<HTMLElement>(node);
    if (!htmlElement || !htmlElement->hasTagName(blockquoteTag))
        return false;
    return htmlElement->attributeWithoutSynchronization(typeAttr) == "cite"_s;
}

int caretMinOffset(const Node& node)
{
    auto* renderer = node.renderer();
    ASSERT(!node.isCharacterDataNode() || !renderer || renderer->isRenderText());

    if (renderer && renderer->isRenderText())
        return renderer->caretMinOffset();

    if (RefPtr pictureElement = dynamicDowncast<HTMLPictureElement>(node)) {
        if (RefPtr firstImage = childrenOfType<HTMLImageElement>(*pictureElement).first())
            return firstImage->computeNodeIndex();
    }

    return 0;
}

// If a node can contain candidates for VisiblePositions, return the offset of the last candidate, otherwise 
// return the number of children for container nodes and the length for unrendered text nodes.
int caretMaxOffset(const Node& node)
{
    // For rendered text nodes, return the last position that a caret could occupy.
    if (auto* text = dynamicDowncast<Text>(node)) {
        if (auto* renderer = text->renderer())
            return renderer->caretMaxOffset();
    }
    return lastOffsetForEditing(node);
}

bool lineBreakExistsAtVisiblePosition(const VisiblePosition& position)
{
    return lineBreakExistsAtPosition(position.deepEquivalent().downstream());
}

bool lineBreakExistsAtPosition(const Position& position)
{
    if (position.isNull())
        return false;

    if (position.anchorNode()->hasTagName(brTag) && position.atFirstEditingPositionForNode())
        return true;

    if (!position.anchorNode()->renderer())
        return false;

    RefPtr textNode = dynamicDowncast<Text>(*position.anchorNode());
    if (!textNode || !textNode->renderer()->style().preserveNewline())
        return false;

    unsigned offset = position.offsetInContainerNode();
    return offset < textNode->length() && textNode->data()[offset] == '\n';
}

// Modifies selections that have an end point at the edge of a table
// that contains the other endpoint so that they don't confuse
// code that iterates over selected paragraphs.
VisibleSelection selectionForParagraphIteration(const VisibleSelection& original)
{
    VisibleSelection newSelection(original);
    VisiblePosition startOfSelection(newSelection.visibleStart());
    VisiblePosition endOfSelection(newSelection.visibleEnd());
    
    // If the end of the selection to modify is just after a table, and
    // if the start of the selection is inside that table, then the last paragraph
    // that we'll want modify is the last one inside the table, not the table itself
    // (a table is itself a paragraph).
    if (RefPtr table = isFirstPositionAfterTable(endOfSelection)) {
        if (startOfSelection.deepEquivalent().deprecatedNode()->isDescendantOf(*table))
            newSelection = VisibleSelection(startOfSelection, endOfSelection.previous(CannotCrossEditingBoundary));
    }
    
    // If the start of the selection to modify is just before a table,
    // and if the end of the selection is inside that table, then the first paragraph
    // we'll want to modify is the first one inside the table, not the paragraph
    // containing the table itself.
    if (RefPtr table = isLastPositionBeforeTable(startOfSelection)) {
        if (endOfSelection.deepEquivalent().deprecatedNode()->isDescendantOf(*table))
            newSelection = VisibleSelection(startOfSelection.next(CannotCrossEditingBoundary), endOfSelection);
    }
    
    return newSelection;
}

// FIXME: indexForVisiblePosition and visiblePositionForIndex use TextIterators to convert between
// VisiblePositions and indices. But TextIterator iteration using TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions
// does not exactly match VisiblePosition iteration, so using them to preserve a selection during an editing
// opertion is unreliable. TextIterator's TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions mode needs to be fixed,
// or these functions need to be changed to iterate using actual VisiblePositions.
// FIXME: Deploy these functions everywhere that TextIterators are used to convert between VisiblePositions and indices.
int indexForVisiblePosition(const VisiblePosition& visiblePosition, RefPtr<ContainerNode>& scope)
{
    if (visiblePosition.isNull())
        return 0;

    auto position = visiblePosition.deepEquivalent();
    Ref document = *position.document();

    auto editableRoot = highestEditableRoot(position, AXObjectCache::accessibilityEnabled() ? HasEditableAXRole : ContentIsEditable);
    if (editableRoot && !document->inDesignMode())
        scope = editableRoot;
    else {
        if (position.containerNode()->isInShadowTree())
            scope = position.containerNode()->containingShadowRoot();
        else
            scope = WTFMove(document);
    }

    auto range = *makeSimpleRange(makeBoundaryPointBeforeNodeContents(*scope), position);
    return characterCount(range, TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions);
}

// FIXME: Merge this function with the one above.
int indexForVisiblePosition(Node& node, const VisiblePosition& visiblePosition, TextIteratorBehaviors behaviors)
{
    if (visiblePosition.isNull())
        return 0;

    auto range = makeSimpleRange(makeBoundaryPointBeforeNodeContents(node), visiblePosition);
    return range ? characterCount(*range, behaviors) : 0;
}

VisiblePosition visiblePositionForPositionWithOffset(const VisiblePosition& position, int offset)
{
    RefPtr<ContainerNode> root;
    unsigned startIndex = indexForVisiblePosition(position, root);
    if (!root)
        return { };

    return visiblePositionForIndex(startIndex + offset, root.get());
}

VisiblePosition visiblePositionForIndex(int index, Node* scope, TextIteratorBehaviors behaviors)
{
    if (!scope)
        return { };
    return { makeDeprecatedLegacyPosition(resolveCharacterLocation(makeRangeSelectingNodeContents(*scope), index, behaviors)) };
}

VisiblePosition visiblePositionForIndexUsingCharacterIterator(Node& node, int index)
{
    if (index <= 0)
        return { firstPositionInOrBeforeNode(&node) };

    auto range = makeRangeSelectingNodeContents(node);
    CharacterIterator it(range);
    if (!it.atEnd())
        it.advance(index - 1);

    if (!it.atEnd() && it.text().length() == 1 && it.text()[0] == '\n') {
        // FIXME: workaround for collapsed range (where only start position is correct) emitted for some emitted newlines.
        it.advance(1);
        if (!it.atEnd())
            return { makeDeprecatedLegacyPosition(it.range().start) };
    }

    return { makeDeprecatedLegacyPosition((it.atEnd() ? range : it.range()).end), Affinity::Upstream };
}

// Determines whether two positions are visibly next to each other (first then second)
// while ignoring whitespaces and unrendered nodes
static bool isVisiblyAdjacent(const Position& first, const Position& second)
{
    return VisiblePosition(first) == VisiblePosition(second.upstream());
}

// Determines whether a node is inside a range or visibly starts and ends at the boundaries of the range.
// Call this function to determine whether a node is visibly fit inside selectedRange
bool isNodeVisiblyContainedWithin(Node& node, const SimpleRange& range)
{
    if (contains<ComposedTree>(range, node))
        return true;

    auto startPosition = makeDeprecatedLegacyPosition(range.start);
    auto endPosition = makeDeprecatedLegacyPosition(range.end);

    bool startIsVisuallySame = visiblePositionBeforeNode(node) == startPosition;
    if (startIsVisuallySame && positionInParentAfterNode(&node) < endPosition)
        return true;

    bool endIsVisuallySame = visiblePositionAfterNode(node) == endPosition;
    if (endIsVisuallySame && startPosition < positionInParentBeforeNode(&node))
        return true;

    return startIsVisuallySame && endIsVisuallySame;
}

bool isRenderedAsNonInlineTableImageOrHR(const Node* node)
{
    if (!node)
        return false;
    RenderObject* renderer = node->renderer();
    return renderer && !renderer->isInline() && (renderer->isRenderTable() || renderer->isImage() || renderer->isHR());
}

Element* elementIfEquivalent(const Element& first, Node& second)
{
    auto* secondElement = dynamicDowncast<Element>(second);
    if (secondElement && first.hasTagName(secondElement->tagQName()) && first.hasEquivalentAttributes(*secondElement))
        return secondElement;
    return nullptr;
}

bool isNonTableCellHTMLBlockElement(const Node* node)
{
    return node->hasTagName(listingTag)
        || node->hasTagName(olTag)
        || node->hasTagName(preTag)
        || is<HTMLTableElement>(*node)
        || node->hasTagName(ulTag)
        || node->hasTagName(xmpTag)
        || node->hasTagName(h1Tag)
        || node->hasTagName(h2Tag)
        || node->hasTagName(h3Tag)
        || node->hasTagName(h4Tag)
        || node->hasTagName(h5Tag);
}

Position adjustedSelectionStartForStyleComputation(const VisibleSelection& selection)
{
    // This function is used by range style computations to avoid bugs like:
    // <rdar://problem/4017641> REGRESSION (Mail): you can only bold/unbold a selection starting from end of line once
    // It is important to skip certain irrelevant content at the start of the selection, so we do not wind up 
    // with a spurious "mixed" style.

    auto visiblePosition = selection.visibleStart();
    if (visiblePosition.isNull())
        return { };

    // if the selection is a caret, just return the position, since the style
    // behind us is relevant
    if (selection.isCaret())
        return visiblePosition.deepEquivalent();

    // if the selection starts just before a paragraph break, skip over it
    if (isEndOfParagraph(visiblePosition))
        return visiblePosition.next().deepEquivalent().downstream();

    // otherwise, make sure to be at the start of the first selected node,
    // instead of possibly at the end of the last node before the selection
    return visiblePosition.deepEquivalent().downstream();
}

// FIXME: Should this be deprecated like deprecatedEnclosingBlockFlowElement is?
static Element* elementIfBlockFlow(Node& node)
{
    auto* element = dynamicDowncast<Element>(node);
    if (!element)
        return nullptr;
    auto* renderer = element->renderer();
    return renderer && renderer->isRenderBlockFlow() ? element : nullptr;
}

bool isBlockFlowElement(const Node& node)
{
    return elementIfBlockFlow(const_cast<Node&>(node));
}

Element* deprecatedEnclosingBlockFlowElement(Node* node)
{
    if (!node)
        return nullptr;
    if (auto* element = elementIfBlockFlow(*node))
        return element;
    while ((node = node->parentNode())) {
        if (auto* element = elementIfBlockFlow(*node))
            return element;
        if (auto* body = dynamicDowncast<HTMLBodyElement>(*node))
            return body;
    }
    return nullptr;
}

static inline bool caretRendersInsideNode(const Node& node)
{
    return !isRenderedTable(&node) && !editingIgnoresContent(node);
}

RenderBlock* rendererForCaretPainting(const Node* node)
{
    if (!node)
        return nullptr;

    auto* renderer = node->renderer();
    if (!renderer)
        return nullptr;

    // If caretNode is a block and caret is inside it, then caret should be painted by that block.
    if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(*renderer)) {
        if (caretRendersInsideNode(*node))
            return blockFlow;
    }
    return renderer->containingBlock();
}

LayoutRect localCaretRectInRendererForCaretPainting(const VisiblePosition& caretPosition, RenderBlock*& caretPainter)
{
    if (caretPosition.isNull())
        return LayoutRect();
    ASSERT(caretPosition.deepEquivalent().deprecatedNode()->renderer());
    auto [localRect, renderer] = caretPosition.localCaretRect();
    return localCaretRectInRendererForRect(localRect, caretPosition.deepEquivalent().deprecatedNode(), renderer, caretPainter);
}

LayoutRect localCaretRectInRendererForRect(LayoutRect& localRect, Node* node, RenderObject* renderer, RenderBlock*& caretPainter)
{
    // Get the renderer that will be responsible for painting the caret
    // (which is either the renderer we just found, or one of its containers).
    caretPainter = rendererForCaretPainting(node);

    // Compute an offset between the renderer and the caretPainter.
    while (renderer != caretPainter) {
        CheckedPtr containerObject = renderer->container();
        if (!containerObject)
            return LayoutRect();
        localRect.move(renderer->offsetFromContainer(*containerObject, localRect.location()));
        renderer = containerObject.get();
    }

    return localRect;
}

IntRect absoluteBoundsForLocalCaretRect(RenderBlock* rendererForCaretPainting, const LayoutRect& rect, bool* insideFixed)
{
    if (insideFixed)
        *insideFixed = false;

    if (!rendererForCaretPainting || rect.isEmpty())
        return IntRect();

    LayoutRect localRect(rect);
    rendererForCaretPainting->flipForWritingMode(localRect);
    return rendererForCaretPainting->localToAbsoluteQuad(FloatRect(localRect), UseTransforms, insideFixed).enclosingBoundingBox();
}

HashSet<RefPtr<HTMLImageElement>> visibleImageElementsInRangeWithNonLoadedImages(const SimpleRange& range)
{
    HashSet<RefPtr<HTMLImageElement>> result;
    for (TextIterator iterator(range); !iterator.atEnd(); iterator.advance()) {
        RefPtr imageElement = dynamicDowncast<HTMLImageElement>(iterator.node());
        if (!imageElement)
            continue;

        auto* cachedImage = imageElement->cachedImage();
        if (cachedImage && cachedImage->isLoading())
            result.add(WTFMove(imageElement));
    }
    return result;
}

enum class RangeEndpointsToAdjust : uint8_t {
    Start = 1 << 0,
    End = 1 << 1,
};

static std::optional<unsigned> visualDistanceOnSameLine(const RenderedPosition& first, const RenderedPosition& second)
{
    if (first.isNull() || second.isNull())
        return std::nullopt;

    if (first.box() == second.box())
        return std::max(first.offset(), second.offset()) - std::min(first.offset(), second.offset());

    enum class VisualBoundary : bool { Left, Right };
    auto distanceFromOffsetToVisualBoundary = [](const RenderedPosition& position, VisualBoundary boundary) {
        auto box = position.box();
        auto offset = position.offset();
        return (boundary == VisualBoundary::Left) == (box->direction() == TextDirection::LTR)
            ? std::max(box->minimumCaretOffset(), offset) - box->minimumCaretOffset()
            : std::max(box->maximumCaretOffset(), offset) - offset;
    };

    unsigned distance = 0;
    bool foundFirst = false;
    bool foundSecond = false;
    for (auto box = first.lineBox()->lineLeftmostLeafBox(); box; box = box->nextLineRightwardOnLine()) {
        if (box == first.box()) {
            distance += distanceFromOffsetToVisualBoundary(first, foundSecond ? VisualBoundary::Left : VisualBoundary::Right);
            foundFirst = true;
        } else if (box == second.box()) {
            distance += distanceFromOffsetToVisualBoundary(second, foundFirst ? VisualBoundary::Left : VisualBoundary::Right);
            foundSecond = true;
        } else if (foundFirst || foundSecond)
            distance += box->maximumCaretOffset() - box->minimumCaretOffset();

        if (foundFirst && foundSecond)
            return distance;
    }

    return std::nullopt;
}

static std::optional<BoundaryPoint> findBidiBoundary(const RenderedPosition& position, unsigned bidiLevel, SelectionExtentMovement movement, TextDirection selectionDirection)
{
    auto leftBoundary = position.leftBoundaryOfBidiRun(bidiLevel);
    auto rightBoundary = position.rightBoundaryOfBidiRun(bidiLevel);

    bool moveLeft = [&] {
        switch (movement) {
        case SelectionExtentMovement::Left:
            return true;
        case SelectionExtentMovement::Right:
            return false;
        case SelectionExtentMovement::Closest: {
            auto distanceToLeft = visualDistanceOnSameLine(position, leftBoundary);
            if (!distanceToLeft)
                return false;

            auto distanceToRight = visualDistanceOnSameLine(position, rightBoundary);
            if (!distanceToRight)
                return true;

            return *distanceToLeft < *distanceToRight;
        }
        }
        ASSERT_NOT_REACHED();
        return false;
    }();
    // This looks unintuitive, but is necessary to ensure that the boundary is moved
    // (visually) to the left or right, respectively, in both LTR and RTL paragraphs.
    return (position.box()->direction() == selectionDirection) == moveLeft ? leftBoundary.boundaryPoint() : rightBoundary.boundaryPoint();
}

enum class BoxIterationDirection : bool { SameAsLine, OppositeOfLine };
static InlineIterator::LeafBoxIterator advanceInDirection(InlineIterator::LeafBoxIterator box, TextDirection direction, BoxIterationDirection iterationDirection)
{
    bool shouldMoveRight = (iterationDirection == BoxIterationDirection::SameAsLine) == (direction == TextDirection::LTR);
    return shouldMoveRight ? box->nextLineRightwardOnLine() : box->nextLineLeftwardOnLine();
}

static void forEachRenderedBoxBetween(const RenderedPosition& first, const RenderedPosition& second, NOESCAPE const Function<IterationStatus(InlineIterator::LeafBoxIterator)>& callback)
{
    if (first.isNull()) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (second.isNull()) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (first.lineBox().atEnd()) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (first.box() == second.box()) {
        callback(first.box());
        return;
    }

    bool foundOneEndpoint = false;
    for (auto box = first.lineBox()->lineLeftmostLeafBox(); box; box = box->nextLineRightwardOnLine()) {
        bool atFirstEndpoint = box == first.box();
        bool atSecondEndpoint = box == second.box();
        bool atEndpoint = atFirstEndpoint || atSecondEndpoint;
        bool atLastEndpoint = atEndpoint && foundOneEndpoint;
        if (!atEndpoint && !foundOneEndpoint)
            continue;

        foundOneEndpoint = true;

        bool shouldSkipBox = [&] {
            if (!atEndpoint)
                return false;

            auto& position = (atFirstEndpoint ? first : second);
            return atLastEndpoint ? position.atLeftmostOffsetInBox() : position.atRightmostOffsetInBox();
        }();

        if (shouldSkipBox)
            continue;

        if (callback(box) == IterationStatus::Done)
            return;

        if (atLastEndpoint)
            return;
    }
}

PositionRange positionsForRange(const SimpleRange& range)
{
    return {
        makeContainerOffsetPosition(range.start).downstream(),
        makeContainerOffsetPosition(range.end).upstream()
    };
}

static InlineIterator::LeafBoxIterator boxWithMinimumBidiLevelBetween(const RenderedPosition& start, const RenderedPosition& end)
{
    InlineIterator::LeafBoxIterator result;
    forEachRenderedBoxBetween(start, end, [&](auto box) {
        if (!result || box->bidiLevel() < result->bidiLevel())
            result = box;
        return IterationStatus::Continue;
    });
    return result;
}

TextDirection primaryDirectionForSingleLineRange(const Position& start, const Position& end)
{
    ASSERT(inSameLine(start, end));

    RenderedPosition renderedStart { start, Affinity::Downstream };
    RenderedPosition renderedEnd { end, Affinity::Upstream };

    auto direction = start.primaryDirection();
    if (renderedStart.isNull() || renderedEnd.isNull())
        return direction;

    if (auto box = boxWithMinimumBidiLevelBetween(renderedStart, renderedEnd))
        return box->direction();

    return direction;
}

static std::optional<SimpleRange> makeVisuallyContiguousIfNeeded(const SimpleRange& range, OptionSet<RangeEndpointsToAdjust> endpoints, SelectionExtentMovement movement)
{
    if (range.collapsed())
        return std::nullopt;

    auto [start, end] = positionsForRange(range);
    auto firstLineDirection = TextDirection::LTR;
    RenderedPosition renderedStart { start };
    if (renderedStart.isNull() || renderedStart.lineBox().atEnd())
        return std::nullopt;

    auto lastLineDirection = TextDirection::LTR;
    RenderedPosition renderedEnd { end };
    if (renderedEnd.isNull() || renderedEnd.lineBox().atEnd())
        return std::nullopt;

    if (renderedStart.box() == renderedEnd.box())
        return std::nullopt;

    auto bidiLevelAtStart = renderedStart.box()->bidiLevel();
    auto bidiLevelAtEnd = renderedEnd.box()->bidiLevel();
    auto targetBidiLevelAtStart = bidiLevelAtStart;
    auto targetBidiLevelAtEnd = bidiLevelAtEnd;
    std::optional<BoundaryPoint> adjustedStart;
    std::optional<BoundaryPoint> adjustedEnd;
    if (inSameLine(start, end)) {
        if (auto box = boxWithMinimumBidiLevelBetween(renderedStart, renderedEnd)) {
            targetBidiLevelAtStart = box->bidiLevel();
            targetBidiLevelAtEnd = targetBidiLevelAtStart;
            firstLineDirection = box->direction();
            lastLineDirection = firstLineDirection;
        }
    } else {
        bool firstLineOnlyContainsSelectedTextInOppositeDirection = true;
        firstLineDirection = start.primaryDirection();
        std::optional<BoundaryPoint> firstPositionForSelectedTextInOppositeDirectionOnFirstLine;
        for (auto box = renderedStart.box(); box; box = advanceInDirection(box, firstLineDirection, BoxIterationDirection::SameAsLine)) {
            targetBidiLevelAtStart = std::min(targetBidiLevelAtStart, box->bidiLevel());

            if (box->direction() == firstLineDirection)
                firstLineOnlyContainsSelectedTextInOppositeDirection = false;

            if (!firstLineOnlyContainsSelectedTextInOppositeDirection)
                continue;

            if (RefPtr node = box->renderer().node()) {
                if (box->isText())
                    firstPositionForSelectedTextInOppositeDirectionOnFirstLine.emplace(node.releaseNonNull(), box->minimumCaretOffset());
                else
                    firstPositionForSelectedTextInOppositeDirectionOnFirstLine = makeBoundaryPointBeforeNode(node.releaseNonNull());
            }
        }

        if (firstLineOnlyContainsSelectedTextInOppositeDirection)
            adjustedStart = WTFMove(firstPositionForSelectedTextInOppositeDirectionOnFirstLine);

        bool lastLineOnlyContainsSelectedTextInOppositeDirection = true;
        lastLineDirection = end.primaryDirection();
        std::optional<BoundaryPoint> lastPositionForSelectedTextInOppositeDirectionOnLastLine;
        for (auto box = renderedEnd.box(); box; box = advanceInDirection(box, lastLineDirection, BoxIterationDirection::OppositeOfLine)) {
            targetBidiLevelAtEnd = std::min(targetBidiLevelAtEnd, box->bidiLevel());

            if (box->direction() == lastLineDirection)
                lastLineOnlyContainsSelectedTextInOppositeDirection = false;

            if (!lastLineOnlyContainsSelectedTextInOppositeDirection)
                continue;

            if (RefPtr node = box->renderer().node()) {
                if (box->isText())
                    lastPositionForSelectedTextInOppositeDirectionOnLastLine.emplace(node.releaseNonNull(), box->maximumCaretOffset());
                else
                    lastPositionForSelectedTextInOppositeDirectionOnLastLine = makeBoundaryPointAfterNode(node.releaseNonNull());
            }
        }

        if (lastLineOnlyContainsSelectedTextInOppositeDirection)
            adjustedEnd = WTFMove(lastPositionForSelectedTextInOppositeDirectionOnLastLine);
    }

    if (!adjustedStart && bidiLevelAtStart > targetBidiLevelAtStart && start != logicalStartOfLine(start) && endpoints.contains(RangeEndpointsToAdjust::Start))
        adjustedStart = findBidiBoundary(renderedStart, targetBidiLevelAtStart + 1, movement, firstLineDirection);

    if (!adjustedEnd && bidiLevelAtEnd > targetBidiLevelAtEnd && end != logicalEndOfLine(end) && endpoints.contains(RangeEndpointsToAdjust::End))
        adjustedEnd = findBidiBoundary(renderedEnd, targetBidiLevelAtEnd + 1, movement, lastLineDirection);

    if (!adjustedStart && !adjustedEnd)
        return std::nullopt;

    auto adjustedRange = range;
    if (adjustedStart)
        adjustedRange.start = WTFMove(*adjustedStart);

    if (adjustedEnd)
        adjustedRange.end = WTFMove(*adjustedEnd);

    if (!is_lt(treeOrder(adjustedRange.start, adjustedRange.end)))
        return std::nullopt;

    return adjustedRange;
}

SimpleRange adjustToVisuallyContiguousRange(const SimpleRange& range)
{
    return makeVisuallyContiguousIfNeeded(range, {
        RangeEndpointsToAdjust::Start,
        RangeEndpointsToAdjust::End
    }, SelectionExtentMovement::Closest).value_or(range);
}

EnclosingLayerInfomation computeEnclosingLayer(const SimpleRange& range)
{
    auto [start, end] = positionsForRange(range);

    if (start.isOrphan() || end.isOrphan())
        return { };

    if (!isEditablePosition(start) && range.collapsed())
        return { };

    auto findEnclosingLayer = [](const Position& position) -> RenderLayer* {
        RefPtr container = position.containerNode();
        if (!container)
            return nullptr;

        CheckedPtr renderer = container->renderer();
        if (!renderer)
            return nullptr;

        return renderer->enclosingLayer();
    };

    auto [startLayer, endLayer] = [&] -> std::pair<CheckedPtr<RenderLayer>, CheckedPtr<RenderLayer>> {
        if (RefPtr container = start.containerNode(); container && ImageOverlay::isInsideOverlay(*container)) {
            RefPtr host = container->shadowHost();
            if (!host) {
                ASSERT_NOT_REACHED();
                return { };
            }

            CheckedPtr renderer = host->renderer();
            if (!renderer)
                return { };

            CheckedPtr enclosingLayer = renderer->enclosingLayer();
            return { enclosingLayer, enclosingLayer };
        }

        return { findEnclosingLayer(start), findEnclosingLayer(end) };
    }();

    if (!startLayer)
        return { };

    if (!endLayer)
        return { };

    for (CheckedPtr layer = startLayer->commonAncestorWithLayer(*endLayer); layer; layer = layer->enclosingContainingBlockLayer(CrossFrameBoundaries::Yes)) {
        if (!layer->isComposited())
            continue;

        RefPtr graphicsLayer = [layer] -> RefPtr<GraphicsLayer> {
            auto* backing = layer->backing();
            if (RefPtr scrolledContentsLayer = backing->scrolledContentsLayer())
                return scrolledContentsLayer;

            if (RefPtr foregroundLayer = backing->foregroundLayer())
                return foregroundLayer;

            if (backing->isFrameLayerWithTiledBacking())
                return backing->parentForSublayers();

            return backing->graphicsLayer();
        }();

        if (!graphicsLayer)
            continue;

        auto identifier = graphicsLayer->layerIDIgnoringStructuralLayer();
        if (!identifier)
            continue;

        return { WTFMove(startLayer), WTFMove(endLayer), WTFMove(layer), WTFMove(graphicsLayer), WTFMove(identifier) };
    }
    return { };
}

void adjustVisibleExtentPreservingVisualContiguity(const VisiblePosition& base, VisiblePosition& extent, SelectionExtentMovement movement)
{
    auto start = makeBoundaryPoint(base.deepEquivalent());
    auto end = makeBoundaryPoint(extent.deepEquivalent());
    if (!start || !end)
        return;

    OptionSet<RangeEndpointsToAdjust> endpoints;
    auto baseExtentOrder = treeOrder<ComposedTree>(*start, *end);
    bool startIsMoving = is_gt(baseExtentOrder);
    bool endIsMoving = is_lt(baseExtentOrder);
    if (startIsMoving) {
        std::swap(start, end);
        endpoints.add(RangeEndpointsToAdjust::Start);
    } else if (endIsMoving)
        endpoints.add(RangeEndpointsToAdjust::End);
    else
        return;

    auto adjustedRange = makeVisuallyContiguousIfNeeded({ WTFMove(*start), WTFMove(*end) }, endpoints, movement);
    if (!adjustedRange)
        return;

    extent = { makeContainerOffsetPosition(startIsMoving ? adjustedRange->start : adjustedRange->end) };
}

bool crossesBidiTextBoundaryInSameLine(const VisiblePosition& position, const VisiblePosition& other)
{
    if (!inSameLine(position, other))
        return false;

    std::optional<unsigned char> currentLevel;
    bool foundDifferentBidiLevel = false;
    forEachRenderedBoxBetween(RenderedPosition { position }, RenderedPosition { other }, [&](auto box) {
        auto bidiLevel = box->bidiLevel();
        if (!currentLevel) {
            currentLevel = bidiLevel;
            return IterationStatus::Continue;
        }

        if (currentLevel == bidiLevel)
            return IterationStatus::Continue;

        foundDifferentBidiLevel = true;
        return IterationStatus::Done;
    });

    return foundDifferentBidiLevel;
}

} // namespace WebCore
