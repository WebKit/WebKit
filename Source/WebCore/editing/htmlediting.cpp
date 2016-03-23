/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "htmlediting.h"

#include "AXObjectCache.h"
#include "Document.h"
#include "Editor.h"
#include "ExceptionCodePlaceholder.h"
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLDivElement.h"
#include "HTMLElementFactory.h"
#include "HTMLInterchange.h"
#include "HTMLLIElement.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLObjectElement.h"
#include "HTMLParagraphElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextFormControlElement.h"
#include "HTMLUListElement.h"
#include "NodeTraversal.h"
#include "PositionIterator.h"
#include "RenderBlock.h"
#include "RenderElement.h"
#include "RenderTableCell.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace HTMLNames;

// Atomic means that the node has no children, or has children which are ignored for the
// purposes of editing.
bool isAtomicNode(const Node *node)
{
    return node && (!node->hasChildNodes() || editingIgnoresContent(node));
}

// Compare two positions, taking into account the possibility that one or both
// could be inside a shadow tree. Only works for non-null values.
int comparePositions(const Position& a, const Position& b)
{
    TreeScope* commonScope = commonTreeScope(a.containerNode(), b.containerNode());

    if (!commonScope)
        return 0;

    Node* nodeA = commonScope->ancestorInThisScope(a.containerNode());
    ASSERT(nodeA);
    bool hasDescendentA = nodeA != a.containerNode();
    int offsetA = hasDescendentA ? 0 : a.computeOffsetInContainerNode();

    Node* nodeB = commonScope->ancestorInThisScope(b.containerNode());
    ASSERT(nodeB);
    bool hasDescendentB = nodeB != b.containerNode();
    int offsetB = hasDescendentB ? 0 : b.computeOffsetInContainerNode();

    int bias = 0;
    if (nodeA == nodeB) {
        if (hasDescendentA)
            bias = -1;
        else if (hasDescendentB)
            bias = 1;
    }

    int result = Range::compareBoundaryPoints(nodeA, offsetA, nodeB, offsetB, IGNORE_EXCEPTION);
    return result ? result : bias;
}

int comparePositions(const VisiblePosition& a, const VisiblePosition& b)
{
    return comparePositions(a.deepEquivalent(), b.deepEquivalent());
}

Node* highestEditableRoot(const Position& position, EditableType editableType)
{
    Node* node = position.deprecatedNode();
    if (!node)
        return 0;

    Node* highestEditableRoot = editableRootForPosition(position, editableType);
    if (!highestEditableRoot)
        return 0;

    node = highestEditableRoot;
    while (!node->hasTagName(bodyTag)) {
        node = node->parentNode();
        if (!node)
            break;
        if (hasEditableStyle(*node, editableType))
            highestEditableRoot = node;
    }

    return highestEditableRoot;
}

Node* lowestEditableAncestor(Node* node)
{
    if (!node)
        return 0;
    
    while (node) {
        if (node->hasEditableStyle())
            return node->rootEditableElement();
        if (node->hasTagName(bodyTag))
            break;
        node = node->parentNode();
    }
    
    return 0;
}

static bool isEditableToAccessibility(const Node& node)
{
    ASSERT(AXObjectCache::accessibilityEnabled());
    ASSERT(node.document().existingAXObjectCache());

    if (AXObjectCache* cache = node.document().existingAXObjectCache())
        return cache->rootAXEditableElement(&node);

    return false;
}

static bool computeEditability(const Node& node, EditableType editableType, Node::ShouldUpdateStyle shouldUpdateStyle)
{
    if (node.computeEditability(Node::UserSelectAllIsAlwaysNonEditable, shouldUpdateStyle) != Node::Editability::ReadOnly)
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
    Node* node = position.containerNode();
    return node && computeEditability(*node, editableType, Node::ShouldUpdateStyle::Update);
}

bool isAtUnsplittableElement(const Position& pos)
{
    Node* node = pos.containerNode();
    return (node == editableRootForPosition(pos) || node == enclosingNodeOfType(pos, &isTableCell));
}
    
    
bool isRichlyEditablePosition(const Position& position)
{
    Node* node = position.containerNode();
    if (!node)
        return false;
    return node->hasRichlyEditableStyle();
}

Element* editableRootForPosition(const Position& p, EditableType editableType)
{
    Node* node = p.containerNode();
    if (!node)
        return 0;

    switch (editableType) {
    case HasEditableAXRole:
        if (AXObjectCache* cache = node->document().existingAXObjectCache())
            return const_cast<Element*>(cache->rootAXEditableElement(node));
        FALLTHROUGH;
    case ContentIsEditable:
        return node->rootEditableElement();
    }
    return 0;
}

// Finds the enclosing element until which the tree can be split.
// When a user hits ENTER, he/she won't expect this element to be split into two.
// You may pass it as the second argument of splitTreeToNode.
Element* unsplittableElementForPosition(const Position& p)
{
    // Since enclosingNodeOfType won't search beyond the highest root editable node,
    // this code works even if the closest table cell was outside of the root editable node.
    Element* enclosingCell = downcast<Element>(enclosingNodeOfType(p, &isTableCell));
    if (enclosingCell)
        return enclosingCell;

    return editableRootForPosition(p);
}

Position nextCandidate(const Position& position)
{
    PositionIterator p = position;
    while (!p.atEnd()) {
        p.increment();
        if (p.isCandidate())
            return p;
    }
    return Position();
}

Position nextVisuallyDistinctCandidate(const Position& position)
{
    // FIXME: Use PositionIterator instead.
    Position p = position;
    Position downstreamStart = p.downstream();
    while (!p.atEndOfTree()) {
        p = p.next(Character);
        if (p.isCandidate() && p.downstream() != downstreamStart)
            return p;
        if (auto* node = p.containerNode()) {
            if (!node->renderer())
                p = lastPositionInOrAfterNode(node);
        }
    }
    return Position();
}

Position previousCandidate(const Position& position)
{
    PositionIterator p = position;
    while (!p.atStart()) {
        p.decrement();
        if (p.isCandidate())
            return p;
    }
    return Position();
}

Position previousVisuallyDistinctCandidate(const Position& position)
{
    // FIXME: Use PositionIterator instead.
    Position p = position;
    Position downstreamStart = p.downstream();
    while (!p.atStartOfTree()) {
        p = p.previous(Character);
        if (p.isCandidate() && p.downstream() != downstreamStart)
            return p;
        if (auto* node = p.containerNode()) {
            if (!node->renderer())
                p = firstPositionInOrBeforeNode(node);
        }
    }
    return Position();
}

Position firstEditablePositionAfterPositionInRoot(const Position& position, Node* highestRoot)
{
    if (!highestRoot)
        return Position();

    // position falls before highestRoot.
    if (comparePositions(position, firstPositionInNode(highestRoot)) == -1 && highestRoot->hasEditableStyle())
        return firstPositionInNode(highestRoot);

    Position p = position;

    if (&position.deprecatedNode()->treeScope() != &highestRoot->treeScope()) {
        Node* shadowAncestor = highestRoot->treeScope().ancestorInThisScope(p.deprecatedNode());
        if (!shadowAncestor)
            return Position();

        p = positionAfterNode(shadowAncestor);
    }

    while (p.deprecatedNode() && !isEditablePosition(p) && p.deprecatedNode()->isDescendantOf(highestRoot))
        p = isAtomicNode(p.deprecatedNode()) ? positionInParentAfterNode(p.deprecatedNode()) : nextVisuallyDistinctCandidate(p);
    
    if (p.deprecatedNode() && p.deprecatedNode() != highestRoot && !p.deprecatedNode()->isDescendantOf(highestRoot))
        return Position();
    
    return p;
}

Position lastEditablePositionBeforePositionInRoot(const Position& position, Node* highestRoot)
{
    if (!highestRoot)
        return Position();

    // When position falls after highestRoot, the result is easy to compute.
    if (comparePositions(position, lastPositionInNode(highestRoot)) == 1)
        return lastPositionInNode(highestRoot);

    Position p = position;

    if (&position.deprecatedNode()->treeScope() != &highestRoot->treeScope()) {
        Node* shadowAncestor = highestRoot->treeScope().ancestorInThisScope(p.deprecatedNode());
        if (!shadowAncestor)
            return Position();

        p = firstPositionInOrBeforeNode(shadowAncestor);
    }
    
    while (p.deprecatedNode() && !isEditablePosition(p) && p.deprecatedNode()->isDescendantOf(highestRoot))
        p = isAtomicNode(p.deprecatedNode()) ? positionInParentBeforeNode(p.deprecatedNode()) : previousVisuallyDistinctCandidate(p);
    
    if (p.deprecatedNode() && p.deprecatedNode() != highestRoot && !p.deprecatedNode()->isDescendantOf(highestRoot))
        return Position();
    
    return p;
}

// FIXME: The method name, comment, and code say three different things here!
// Whether or not content before and after this node will collapse onto the same line as it.
bool isBlock(const Node* node)
{
    return node && node->renderer() && !node->renderer()->isInline() && !node->renderer()->isRubyText();
}

bool isInline(const Node* node)
{
    return node && node->renderer() && node->renderer()->isInline();
}

// FIXME: Deploy this in all of the places where enclosingBlockFlow/enclosingBlockFlowOrTableElement are used.
// FIXME: Pass a position to this function. The enclosing block of [table, x] for example, should be the 
// block that contains the table and not the table, and this function should be the only one responsible for 
// knowing about these kinds of special cases.
Element* enclosingBlock(Node* node, EditingBoundaryCrossingRule rule)
{
    Node* enclosingNode = enclosingNodeOfType(firstPositionInOrBeforeNode(node), isBlock, rule);
    return is<Element>(enclosingNode) ? downcast<Element>(enclosingNode) : nullptr;
}

TextDirection directionOfEnclosingBlock(const Position& position)
{
    auto block = enclosingBlock(position.containerNode());
    if (!block)
        return LTR;
    auto renderer = block->renderer();
    if (!renderer)
        return LTR;
    return renderer->style().direction();
}

// This method is used to create positions in the DOM. It returns the maximum valid offset
// in a node. It returns 1 for some elements even though they do not have children, which
// creates technically invalid DOM Positions. Be sure to call parentAnchoredEquivalent
// on a Position before using it to create a DOM Range, or an exception will be thrown.
int lastOffsetForEditing(const Node* node)
{
    ASSERT(node);
    if (!node)
        return 0;
    if (node->offsetInCharacters())
        return node->maxCharacterOffset();

    if (node->hasChildNodes())
        return node->countChildNodes();

    // NOTE: This should preempt the countChildNodes() for, e.g., select nodes (what does this mean)?
    if (editingIgnoresContent(node))
        return 1;

    return 0;
}

String stringWithRebalancedWhitespace(const String& string, bool startIsStartOfParagraph, bool endIsEndOfParagraph)
{
    Vector<UChar> rebalancedString(string.length());
    StringView(string).getCharactersWithUpconvert(rebalancedString.data());

    bool previousCharacterWasSpace = false;
    for (size_t i = 0; i < rebalancedString.size(); i++) {
        if (!isWhitespace(rebalancedString[i])) {
            previousCharacterWasSpace = false;
            continue;
        }

        if (previousCharacterWasSpace || (!i && startIsStartOfParagraph) || (i + 1 == rebalancedString.size() && endIsEndOfParagraph)) {
            rebalancedString[i] = noBreakSpace;
            previousCharacterWasSpace = false;
        } else {
            rebalancedString[i] = ' ';
            previousCharacterWasSpace = true;
        }
    }

    return String::adopt(rebalancedString);
}

bool isTableStructureNode(const Node *node)
{
    RenderObject* renderer = node->renderer();
    return (renderer && (renderer->isTableCell() || renderer->isTableRow() || renderer->isTableSection() || renderer->isRenderTableCol()));
}

const String& nonBreakingSpaceString()
{
    static NeverDestroyed<String> nonBreakingSpaceString(&noBreakSpace, 1);
    return nonBreakingSpaceString;
}

// FIXME: need to dump this
bool isSpecialElement(const Node *n)
{
    if (!n)
        return false;
        
    if (!n->isHTMLElement())
        return false;

    if (n->isLink())
        return true;

    RenderObject* renderer = n->renderer();
    if (!renderer)
        return false;
        
    if (renderer->style().display() == TABLE || renderer->style().display() == INLINE_TABLE)
        return true;

    if (renderer->style().isFloating())
        return true;

    if (renderer->style().position() != StaticPosition)
        return true;
        
    return false;
}

static Node* firstInSpecialElement(const Position& pos)
{
    Node* rootEditableElement = pos.containerNode()->rootEditableElement();
    for (Node* n = pos.deprecatedNode(); n && n->rootEditableElement() == rootEditableElement; n = n->parentNode())
        if (isSpecialElement(n)) {
            VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);
            VisiblePosition firstInElement = VisiblePosition(firstPositionInOrBeforeNode(n), DOWNSTREAM);
            if (isRenderedTable(n) && vPos == firstInElement.next())
                return n;
            if (vPos == firstInElement)
                return n;
        }
    return 0;
}

static Node* lastInSpecialElement(const Position& pos)
{
    Node* rootEditableElement = pos.containerNode()->rootEditableElement();
    for (Node* n = pos.deprecatedNode(); n && n->rootEditableElement() == rootEditableElement; n = n->parentNode())
        if (isSpecialElement(n)) {
            VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);
            VisiblePosition lastInElement = VisiblePosition(lastPositionInOrAfterNode(n), DOWNSTREAM);
            if (isRenderedTable(n) && vPos == lastInElement.previous())
                return n;
            if (vPos == lastInElement)
                return n;
        }
    return 0;
}

bool isFirstVisiblePositionInSpecialElement(const Position& pos)
{
    return firstInSpecialElement(pos);
}

Position positionBeforeContainingSpecialElement(const Position& pos, Node** containingSpecialElement)
{
    Node* n = firstInSpecialElement(pos);
    if (!n)
        return pos;
    Position result = positionInParentBeforeNode(n);
    if (result.isNull() || result.deprecatedNode()->rootEditableElement() != pos.deprecatedNode()->rootEditableElement())
        return pos;
    if (containingSpecialElement)
        *containingSpecialElement = n;
    return result;
}

bool isLastVisiblePositionInSpecialElement(const Position& pos)
{
    return lastInSpecialElement(pos);
}

Position positionAfterContainingSpecialElement(const Position& pos, Node **containingSpecialElement)
{
    Node* n = lastInSpecialElement(pos);
    if (!n)
        return pos;
    Position result = positionInParentAfterNode(n);
    if (result.isNull() || result.deprecatedNode()->rootEditableElement() != pos.deprecatedNode()->rootEditableElement())
        return pos;
    if (containingSpecialElement)
        *containingSpecialElement = n;
    return result;
}

Position positionOutsideContainingSpecialElement(const Position &pos, Node **containingSpecialElement)
{
    if (isFirstVisiblePositionInSpecialElement(pos))
        return positionBeforeContainingSpecialElement(pos, containingSpecialElement);
    if (isLastVisiblePositionInSpecialElement(pos))
        return positionAfterContainingSpecialElement(pos, containingSpecialElement);
    return pos;
}

Node* isFirstPositionAfterTable(const VisiblePosition& visiblePosition)
{
    Position upstream(visiblePosition.deepEquivalent().upstream());
    if (upstream.deprecatedNode() && upstream.deprecatedNode()->renderer() && upstream.deprecatedNode()->renderer()->isTable() && upstream.atLastEditingPositionForNode())
        return upstream.deprecatedNode();
    
    return 0;
}

Node* isLastPositionBeforeTable(const VisiblePosition& visiblePosition)
{
    Position downstream(visiblePosition.deepEquivalent().downstream());
    if (downstream.deprecatedNode() && downstream.deprecatedNode()->renderer() && downstream.deprecatedNode()->renderer()->isTable() && downstream.atFirstEditingPositionForNode())
        return downstream.deprecatedNode();
    
    return 0;
}

// Returns the visible position at the beginning of a node
VisiblePosition visiblePositionBeforeNode(Node* node)
{
    ASSERT(node);
    if (node->hasChildNodes())
        return VisiblePosition(firstPositionInOrBeforeNode(node), DOWNSTREAM);
    ASSERT(node->parentNode());
    ASSERT(!node->parentNode()->isShadowRoot());
    return positionInParentBeforeNode(node);
}

// Returns the visible position at the ending of a node
VisiblePosition visiblePositionAfterNode(Node* node)
{
    ASSERT(node);
    if (node->hasChildNodes())
        return VisiblePosition(lastPositionInOrAfterNode(node), DOWNSTREAM);
    ASSERT(node->parentNode());
    ASSERT(!node->parentNode()->isShadowRoot());
    return positionInParentAfterNode(node);
}

bool isListElement(Node *n)
{
    return (n && (n->hasTagName(ulTag) || n->hasTagName(olTag) || n->hasTagName(dlTag)));
}

bool isListItem(const Node *n)
{
    return n && (isListElement(n->parentNode()) || (n->renderer() && n->renderer()->isListItem()));
}

Element* enclosingElementWithTag(const Position& position, const QualifiedName& tagName)
{
    if (position.isNull())
        return nullptr;

    Node* root = highestEditableRoot(position);
    for (Node* node = position.deprecatedNode(); node; node = node->parentNode()) {
        if (root && !node->hasEditableStyle())
            continue;
        if (!is<Element>(*node))
            continue;
        if (downcast<Element>(*node).hasTagName(tagName))
            return downcast<Element>(node);
        if (node == root)
            return nullptr;
    }

    return nullptr;
}

Node* enclosingNodeOfType(const Position& p, bool (*nodeIsOfType)(const Node*), EditingBoundaryCrossingRule rule)
{
    // FIXME: support CanSkipCrossEditingBoundary
    ASSERT(rule == CanCrossEditingBoundary || rule == CannotCrossEditingBoundary);
    if (p.isNull())
        return 0;
        
    Node* root = rule == CannotCrossEditingBoundary ? highestEditableRoot(p) : 0;
    for (Node* n = p.deprecatedNode(); n; n = n->parentNode()) {
        // Don't return a non-editable node if the input position was editable, since
        // the callers from editing will no doubt want to perform editing inside the returned node.
        if (root && !n->hasEditableStyle())
            continue;
        if (nodeIsOfType(n))
            return n;
        if (n == root)
            return 0;
    }
    
    return 0;
}

Node* highestEnclosingNodeOfType(const Position& p, bool (*nodeIsOfType)(const Node*), EditingBoundaryCrossingRule rule, Node* stayWithin)
{
    Node* highest = nullptr;
    Node* root = rule == CannotCrossEditingBoundary ? highestEditableRoot(p) : 0;
    for (Node* n = p.containerNode(); n && n != stayWithin; n = n->parentNode()) {
        if (root && !n->hasEditableStyle())
            continue;
        if (nodeIsOfType(n))
            highest = n;
        if (n == root)
            break;
    }
    
    return highest;
}

static bool hasARenderedDescendant(Node* node, Node* excludedNode)
{
    for (Node* n = node->firstChild(); n;) {
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

Node* highestNodeToRemoveInPruning(Node* node)
{
    Node* previousNode = nullptr;
    Node* rootEditableElement = node ? node->rootEditableElement() : 0;
    for (; node; node = node->parentNode()) {
        if (RenderObject* renderer = node->renderer()) {
            if (!renderer->canHaveChildren() || hasARenderedDescendant(node, previousNode) || rootEditableElement == node)
                return previousNode;
        }
        previousNode = node;
    }
    return 0;
}

Node* enclosingTableCell(const Position& p)
{
    return downcast<Element>(enclosingNodeOfType(p, isTableCell));
}

Element* enclosingAnchorElement(const Position& p)
{
    if (p.isNull())
        return nullptr;

    for (Node* node = p.deprecatedNode(); node; node = node->parentNode()) {
        if (is<Element>(*node) && node->isLink())
            return downcast<Element>(node);
    }
    return nullptr;
}

HTMLElement* enclosingList(Node* node)
{
    if (!node)
        return nullptr;
        
    Node* root = highestEditableRoot(firstPositionInOrBeforeNode(node));
    
    for (ContainerNode* ancestor = node->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        if (is<HTMLUListElement>(*ancestor) || is<HTMLOListElement>(*ancestor))
            return downcast<HTMLElement>(ancestor);
        if (ancestor == root)
            return nullptr;
    }
    
    return nullptr;
}

Node* enclosingListChild(Node *node)
{
    if (!node)
        return nullptr;
    // Check for a list item element, or for a node whose parent is a list element. Such a node
    // will appear visually as a list item (but without a list marker)
    Node* root = highestEditableRoot(firstPositionInOrBeforeNode(node));
    
    // FIXME: This function is inappropriately named if it starts with node instead of node->parentNode()
    for (Node* n = node; n && n->parentNode(); n = n->parentNode()) {
        if (n->hasTagName(liTag) || (isListElement(n->parentNode()) && n != root))
            return n;
        if (n == root || isTableCell(n))
            return nullptr;
    }
    
    return nullptr;
}

static HTMLElement* embeddedSublist(Node* listItem)
{
    // Check the DOM so that we'll find collapsed sublists without renderers.
    for (Node* n = listItem->firstChild(); n; n = n->nextSibling()) {
        if (isListElement(n))
            return downcast<HTMLElement>(n);
    }
    
    return nullptr;
}

static Node* appendedSublist(Node* listItem)
{
    // Check the DOM so that we'll find collapsed sublists without renderers.
    for (Node* n = listItem->nextSibling(); n; n = n->nextSibling()) {
        if (isListElement(n))
            return downcast<HTMLElement>(n);
        if (isListItem(listItem))
            return nullptr;
    }
    
    return nullptr;
}

// FIXME: This method should not need to call isStartOfParagraph/isEndOfParagraph
Node* enclosingEmptyListItem(const VisiblePosition& visiblePos)
{
    // Check that position is on a line by itself inside a list item
    Node* listChildNode = enclosingListChild(visiblePos.deepEquivalent().deprecatedNode());
    if (!listChildNode || !isStartOfParagraph(visiblePos) || !isEndOfParagraph(visiblePos))
        return 0;

    VisiblePosition firstInListChild(firstPositionInOrBeforeNode(listChildNode));
    VisiblePosition lastInListChild(lastPositionInOrAfterNode(listChildNode));

    if (firstInListChild != visiblePos || lastInListChild != visiblePos)
        return 0;
    
    if (embeddedSublist(listChildNode) || appendedSublist(listChildNode))
        return 0;
        
    return listChildNode;
}

HTMLElement* outermostEnclosingList(Node* node, Node* rootList)
{
    HTMLElement* list = enclosingList(node);
    if (!list)
        return 0;

    while (HTMLElement* nextList = enclosingList(list)) {
        if (nextList == rootList)
            break;
        list = nextList;
    }

    return list;
}

bool canMergeLists(Element* firstList, Element* secondList)
{
    if (!firstList || !secondList || !firstList->isHTMLElement() || !secondList->isHTMLElement())
        return false;

    return firstList->hasTagName(secondList->tagQName()) // make sure the list types match (ol vs. ul)
    && firstList->hasEditableStyle() && secondList->hasEditableStyle() // both lists are editable
    && firstList->rootEditableElement() == secondList->rootEditableElement() // don't cross editing boundaries
    && isVisiblyAdjacent(positionInParentAfterNode(firstList), positionInParentBeforeNode(secondList));
    // Make sure there is no visible content between this li and the previous list
}

static Node* previousNodeConsideringAtomicNodes(const Node* node)
{
    if (node->previousSibling()) {
        Node* n = node->previousSibling();
        while (!isAtomicNode(n) && n->lastChild())
            n = n->lastChild();
        return n;
    }
    if (node->parentNode())
        return node->parentNode();
    return 0;
}

static Node* nextNodeConsideringAtomicNodes(const Node* node)
{
    if (!isAtomicNode(node) && node->firstChild())
        return node->firstChild();
    if (node->nextSibling())
        return node->nextSibling();
    const Node* n = node;
    while (n && !n->nextSibling())
        n = n->parentNode();
    if (n)
        return n->nextSibling();
    return 0;
}

Node* previousLeafNode(const Node* node)
{
    Node* n = previousNodeConsideringAtomicNodes(node);
    while (n) {
        if (isAtomicNode(n))
            return n;
        n = previousNodeConsideringAtomicNodes(n);
    }
    return 0;
}

Node* nextLeafNode(const Node* node)
{
    Node* n = nextNodeConsideringAtomicNodes(node);
    while (n) {
        if (isAtomicNode(n))
            return n;
        n = nextNodeConsideringAtomicNodes(n);
    }
    return 0;
}

// FIXME: do not require renderer, so that this can be used within fragments
bool isRenderedTable(const Node* n)
{
    if (!n || !n->isElementNode())
        return false;

    RenderObject* renderer = n->renderer();
    return (renderer && renderer->isTable());
}

bool isTableCell(const Node* node)
{
    RenderObject* r = node->renderer();
    if (!r)
        return node->hasTagName(tdTag) || node->hasTagName(thTag);
    
    return r->isTableCell();
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
    RenderObject* renderer = node->renderer();
    if (renderer->isBR()) {
        renderer = renderer->parent();
        if (!renderer)
            return false;
    }
    if (!is<RenderTableCell>(*renderer))
        return false;

    // Check that the table cell contains no child renderers except for perhaps a single <br>.
    RenderObject* childRenderer = downcast<RenderTableCell>(*renderer).firstChild();
    if (!childRenderer)
        return true;
    if (!childRenderer->isBR())
        return false;
    return !childRenderer->nextSibling();
}

Ref<HTMLElement> createDefaultParagraphElement(Document& document)
{
    switch (document.frame()->editor().defaultParagraphSeparator()) {
    case EditorParagraphSeparatorIsDiv:
        return HTMLDivElement::create(document);
    case EditorParagraphSeparatorIsP:
        break;
    }
    return HTMLParagraphElement::create(document);
}

Ref<HTMLElement> createBreakElement(Document& document)
{
    return HTMLBRElement::create(document);
}

Ref<HTMLElement> createOrderedListElement(Document& document)
{
    return HTMLOListElement::create(document);
}

Ref<HTMLElement> createUnorderedListElement(Document& document)
{
    return HTMLUListElement::create(document);
}

Ref<HTMLElement> createListItemElement(Document& document)
{
    return HTMLLIElement::create(document);
}

Ref<HTMLElement> createHTMLElement(Document& document, const QualifiedName& name)
{
    return HTMLElementFactory::createElement(name, document);
}

Ref<HTMLElement> createHTMLElement(Document& document, const AtomicString& tagName)
{
    return createHTMLElement(document, QualifiedName(nullAtom, tagName, xhtmlNamespaceURI));
}

bool isTabSpanNode(const Node *node)
{
    return node && node->hasTagName(spanTag) && node->isElementNode() && static_cast<const Element *>(node)->getAttribute(classAttr) == AppleTabSpanClass;
}

bool isTabSpanTextNode(const Node *node)
{
    return node && node->isTextNode() && node->parentNode() && isTabSpanNode(node->parentNode());
}

Node* tabSpanNode(const Node *node)
{
    return isTabSpanTextNode(node) ? node->parentNode() : 0;
}
    
Position positionOutsideTabSpan(const Position& pos)
{
    Node* node = pos.containerNode();
    if (isTabSpanTextNode(node))
        node = tabSpanNode(node);
    else if (!isTabSpanNode(node))
        return pos;

    if (node && VisiblePosition(pos) == lastPositionInNode(node))
        return positionInParentAfterNode(node);

    return positionInParentBeforeNode(node);
}

Ref<Element> createTabSpanElement(Document& document, RefPtr<Node>&& tabTextNode)
{
    // Make the span to hold the tab.
    Ref<Element> spanElement = document.createElement(spanTag, false);
    spanElement->setAttribute(classAttr, AppleTabSpanClass);
    spanElement->setAttribute(styleAttr, "white-space:pre");

    // Add tab text to that span.
    if (!tabTextNode)
        tabTextNode = document.createEditingTextNode("\t");

    spanElement->appendChild(tabTextNode.releaseNonNull(), ASSERT_NO_EXCEPTION);

    return spanElement;
}

Ref<Element> createTabSpanElement(Document& document, const String& tabText)
{
    return createTabSpanElement(document, document.createTextNode(tabText));
}

Ref<Element> createTabSpanElement(Document& document)
{
    return createTabSpanElement(document, RefPtr<Node>());
}

bool isNodeRendered(const Node* node)
{
    if (!node)
        return false;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return false;

    return renderer->style().visibility() == VISIBLE;
}

unsigned numEnclosingMailBlockquotes(const Position& p)
{
    unsigned num = 0;
    for (Node* n = p.deprecatedNode(); n; n = n->parentNode())
        if (isMailBlockquote(n))
            num++;
    
    return num;
}

void updatePositionForNodeRemoval(Position& position, Node& node)
{
    if (position.isNull())
        return;
    switch (position.anchorType()) {
    case Position::PositionIsBeforeChildren:
        if (node.containsIncludingShadowDOM(position.containerNode()))
            position = positionInParentBeforeNode(&node);
        break;
    case Position::PositionIsAfterChildren:
        if (node.containsIncludingShadowDOM(position.containerNode()))
            position = positionInParentBeforeNode(&node);
        break;
    case Position::PositionIsOffsetInAnchor:
        if (position.containerNode() == node.parentNode() && static_cast<unsigned>(position.offsetInContainerNode()) > node.computeNodeIndex())
            position.moveToOffset(position.offsetInContainerNode() - 1);
        else if (node.containsIncludingShadowDOM(position.containerNode()))
            position = positionInParentBeforeNode(&node);
        break;
    case Position::PositionIsAfterAnchor:
        if (node.containsIncludingShadowDOM(position.anchorNode()))
            position = positionInParentAfterNode(&node);
        break;
    case Position::PositionIsBeforeAnchor:
        if (node.containsIncludingShadowDOM(position.anchorNode()))
            position = positionInParentBeforeNode(&node);
        break;
    }
}

bool isMailBlockquote(const Node *node)
{
    if (!node || !node->hasTagName(blockquoteTag))
        return false;
        
    return static_cast<const Element *>(node)->getAttribute("type") == "cite";
}

int caretMinOffset(const Node* n)
{
    RenderObject* r = n->renderer();
    ASSERT(!n->isCharacterDataNode() || !r || r->isText()); // FIXME: This was a runtime check that seemingly couldn't fail; changed it to an assertion for now.
    return r ? r->caretMinOffset() : 0;
}

// If a node can contain candidates for VisiblePositions, return the offset of the last candidate, otherwise 
// return the number of children for container nodes and the length for unrendered text nodes.
int caretMaxOffset(const Node* n)
{
    // For rendered text nodes, return the last position that a caret could occupy.
    if (n->isTextNode() && n->renderer())
        return n->renderer()->caretMaxOffset();
    // For containers return the number of children. For others do the same as above.
    return lastOffsetForEditing(n);
}

bool lineBreakExistsAtVisiblePosition(const VisiblePosition& visiblePosition)
{
    return lineBreakExistsAtPosition(visiblePosition.deepEquivalent().downstream());
}

bool lineBreakExistsAtPosition(const Position& position)
{
    if (position.isNull())
        return false;
    
    if (position.anchorNode()->hasTagName(brTag) && position.atFirstEditingPositionForNode())
        return true;
    
    if (!position.anchorNode()->renderer())
        return false;
    
    if (!is<Text>(*position.anchorNode()) || !position.anchorNode()->renderer()->style().preserveNewline())
        return false;
    
    Text& textNode = downcast<Text>(*position.anchorNode());
    unsigned offset = position.offsetInContainerNode();
    return offset < textNode.length() && textNode.data()[offset] == '\n';
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
    if (Node* table = isFirstPositionAfterTable(endOfSelection))
        if (startOfSelection.deepEquivalent().deprecatedNode()->isDescendantOf(table))
            newSelection = VisibleSelection(startOfSelection, endOfSelection.previous(CannotCrossEditingBoundary));
    
    // If the start of the selection to modify is just before a table,
    // and if the end of the selection is inside that table, then the first paragraph
    // we'll want to modify is the first one inside the table, not the paragraph
    // containing the table itself.
    if (Node* table = isLastPositionBeforeTable(startOfSelection))
        if (endOfSelection.deepEquivalent().deprecatedNode()->isDescendantOf(table))
            newSelection = VisibleSelection(startOfSelection.next(CannotCrossEditingBoundary), endOfSelection);
    
    return newSelection;
}

// FIXME: indexForVisiblePosition and visiblePositionForIndex use TextIterators to convert between 
// VisiblePositions and indices. But TextIterator iteration using TextIteratorEmitsCharactersBetweenAllVisiblePositions 
// does not exactly match VisiblePosition iteration, so using them to preserve a selection during an editing 
// opertion is unreliable. TextIterator's TextIteratorEmitsCharactersBetweenAllVisiblePositions mode needs to be fixed, 
// or these functions need to be changed to iterate using actual VisiblePositions.
// FIXME: Deploy these functions everywhere that TextIterators are used to convert between VisiblePositions and indices.
int indexForVisiblePosition(const VisiblePosition& visiblePosition, RefPtr<ContainerNode>& scope)
{
    if (visiblePosition.isNull())
        return 0;

    Position p(visiblePosition.deepEquivalent());
    Document& document = p.anchorNode()->document();
    ShadowRoot* shadowRoot = p.anchorNode()->containingShadowRoot();

    if (shadowRoot)
        scope = shadowRoot;
    else
        scope = document.documentElement();

    RefPtr<Range> range = Range::create(document, firstPositionInNode(scope.get()), p.parentAnchoredEquivalent());
    return TextIterator::rangeLength(range.get(), true);
}

// FIXME: Merge these two functions.
int indexForVisiblePosition(Node* node, const VisiblePosition& visiblePosition, bool forSelectionPreservation)
{
    ASSERT(node);
    RefPtr<Range> range = Range::create(node->document(), firstPositionInNode(node), visiblePosition.deepEquivalent().parentAnchoredEquivalent());
    return TextIterator::rangeLength(range.get(), forSelectionPreservation);
}

VisiblePosition visiblePositionForIndex(int index, ContainerNode* scope)
{
    RefPtr<Range> range = TextIterator::rangeFromLocationAndLength(scope, index, 0, true);
    // Check for an invalid index. Certain editing operations invalidate indices because 
    // of problems with TextIteratorEmitsCharactersBetweenAllVisiblePositions.
    if (!range)
        return VisiblePosition();
    return VisiblePosition(range->startPosition());
}

VisiblePosition visiblePositionForIndexUsingCharacterIterator(Node* node, int index)
{
    ASSERT(node);
    if (index <= 0)
        return VisiblePosition(firstPositionInOrBeforeNode(node), DOWNSTREAM);

    RefPtr<Range> range = Range::create(node->document());
    range->selectNodeContents(node, IGNORE_EXCEPTION);
    CharacterIterator it(*range);
    it.advance(index - 1);
    return VisiblePosition(it.atEnd() ? range->endPosition() : it.range()->endPosition(), UPSTREAM);
}

// Determines whether two positions are visibly next to each other (first then second)
// while ignoring whitespaces and unrendered nodes
bool isVisiblyAdjacent(const Position& first, const Position& second)
{
    return VisiblePosition(first) == VisiblePosition(second.upstream());
}

// Determines whether a node is inside a range or visibly starts and ends at the boundaries of the range.
// Call this function to determine whether a node is visibly fit inside selectedRange
bool isNodeVisiblyContainedWithin(Node* node, const Range* selectedRange)
{
    ASSERT(node);
    ASSERT(selectedRange);
    // If the node is inside the range, then it surely is contained within
    if (selectedRange->compareNode(node, IGNORE_EXCEPTION) == Range::NODE_INSIDE)
        return true;

    bool startIsVisuallySame = visiblePositionBeforeNode(node) == selectedRange->startPosition();
    if (startIsVisuallySame && comparePositions(positionInParentAfterNode(node), selectedRange->endPosition()) < 0)
        return true;

    bool endIsVisuallySame = visiblePositionAfterNode(node) == selectedRange->endPosition();
    if (endIsVisuallySame && comparePositions(selectedRange->startPosition(), positionInParentBeforeNode(node)) < 0)
        return true;

    return startIsVisuallySame && endIsVisuallySame;
}

bool isRenderedAsNonInlineTableImageOrHR(const Node* node)
{
    if (!node)
        return false;
    RenderObject* renderer = node->renderer();
    return renderer && ((renderer->isTable() && !renderer->isInline()) || (renderer->isImage() && !renderer->isInline()) || renderer->isHR());
}

bool areIdenticalElements(const Node* first, const Node* second)
{
    if (!is<Element>(*first) || !is<Element>(*second))
        return false;

    const Element& firstElement = downcast<Element>(*first);
    const Element& secondElement = downcast<Element>(*second);
    if (!firstElement.hasTagName(secondElement.tagQName()))
        return false;

    return firstElement.hasEquivalentAttributes(&secondElement);
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

    VisiblePosition visiblePosition = selection.start();
    if (visiblePosition.isNull())
        return Position();

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
bool isBlockFlowElement(const Node* node)
{
    if (!node->isElementNode())
        return false;
    RenderObject* renderer = node->renderer();
    return renderer && renderer->isRenderBlockFlow();
}

Element* deprecatedEnclosingBlockFlowElement(Node* node)
{
    if (!node)
        return nullptr;
    if (isBlockFlowElement(node))
        return downcast<Element>(node);
    while ((node = node->parentNode())) {
        if (isBlockFlowElement(node) || is<HTMLBodyElement>(*node))
            return downcast<Element>(node);
    }
    return nullptr;
}

static inline bool caretRendersInsideNode(Node* node)
{
    return node && !isRenderedTable(node) && !editingIgnoresContent(node);
}

RenderBlock* rendererForCaretPainting(Node* node)
{
    if (!node)
        return nullptr;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nullptr;

    // If caretNode is a block and caret is inside it, then caret should be painted by that block.
    bool paintedByBlock = is<RenderBlockFlow>(*renderer) && caretRendersInsideNode(node);
    return paintedByBlock ? downcast<RenderBlock>(renderer) : renderer->containingBlock();
}

LayoutRect localCaretRectInRendererForCaretPainting(const VisiblePosition& caretPosition, RenderBlock*& caretPainter)
{
    if (caretPosition.isNull())
        return LayoutRect();

    ASSERT(caretPosition.deepEquivalent().deprecatedNode()->renderer());

    // First compute a rect local to the renderer at the selection start.
    RenderObject* renderer;
    LayoutRect localRect = caretPosition.localCaretRect(renderer);

    return localCaretRectInRendererForRect(localRect, caretPosition.deepEquivalent().deprecatedNode(), renderer, caretPainter);
}

LayoutRect localCaretRectInRendererForRect(LayoutRect& localRect, Node* node, RenderObject* renderer, RenderBlock*& caretPainter)
{
    // Get the renderer that will be responsible for painting the caret
    // (which is either the renderer we just found, or one of its containers).
    caretPainter = rendererForCaretPainting(node);

    // Compute an offset between the renderer and the caretPainter.
    while (renderer != caretPainter) {
        RenderElement* containerObject = renderer->container();
        if (!containerObject)
            return LayoutRect();
        localRect.move(renderer->offsetFromContainer(*containerObject, localRect.location()));
        renderer = containerObject;
    }

    return localRect;
}

IntRect absoluteBoundsForLocalCaretRect(RenderBlock* rendererForCaretPainting, const LayoutRect& rect)
{
    if (!rendererForCaretPainting || rect.isEmpty())
        return IntRect();

    LayoutRect localRect(rect);
    rendererForCaretPainting->flipForWritingMode(localRect);
    return rendererForCaretPainting->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
}

} // namespace WebCore
