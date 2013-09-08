/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NodeRenderingTraversal.h"

#include "InsertionPoint.h"
#include "PseudoElement.h"
#include "ShadowRoot.h"

namespace WebCore {

namespace NodeRenderingTraversal {

static Node* findFirstSiblingEnteringInsertionPoints(const Node*);
static Node* findFirstEnteringInsertionPoints(const Node*);
static Node* findFirstFromDistributedNode(const Node*, const InsertionPoint*);
static Node* findLastSiblingEnteringInsertionPoints(const Node*);
static Node* findLastEnteringInsertionPoints(const Node*);
static Node* findLastFromDistributedNode(const Node*, const InsertionPoint*);

static inline bool nodeCanBeDistributed(const Node* node)
{
    ASSERT(node);
    Node* parent = parentNodeForDistribution(node);
    if (!parent)
        return false;

    if (parent->isShadowRoot())
        return false;

    if (parent->isElementNode() && toElement(parent)->shadowRoot())
        return true;
    
    return false;
}

static Node* findFirstSiblingEnteringInsertionPoints(const Node* node)
{
    for (const Node* sibling = node; sibling; sibling = sibling->nextSibling()) {
        if (Node* found = findFirstEnteringInsertionPoints(sibling))
            return found;
    }
    return nullptr;
}

static Node* findFirstEnteringInsertionPoints(const Node* node)
{
    ASSERT(node);
    if (!isActiveInsertionPoint(node))
        return const_cast<Node*>(node);
    const InsertionPoint* insertionPoint = toInsertionPoint(node);
    if (Node* found = findFirstFromDistributedNode(insertionPoint->firstDistributed(), insertionPoint))
        return found;
    return findFirstSiblingEnteringInsertionPoints(node->firstChild());
}

static Node* findFirstFromDistributedNode(const Node* node, const InsertionPoint* insertionPoint)
{
    for (const Node* next = node; next; next = insertionPoint->nextDistributedTo(next)) {
        if (Node* found = findFirstEnteringInsertionPoints(next))
            return found;
    }
    return nullptr;
}

static Node* findLastSiblingEnteringInsertionPoints(const Node* node)
{
    for (const Node* sibling = node; sibling; sibling = sibling->previousSibling()) {
        if (Node* found = findLastEnteringInsertionPoints(sibling))
            return found;
    }
    return nullptr;
}

static Node* findLastEnteringInsertionPoints(const Node* node)
{
    ASSERT(node);
    if (!isActiveInsertionPoint(node))
        return const_cast<Node*>(node);
    const InsertionPoint* insertionPoint = toInsertionPoint(node);
    if (Node* found = findLastFromDistributedNode(insertionPoint->lastDistributed(), insertionPoint))
        return found;
    return findLastSiblingEnteringInsertionPoints(node->lastChild());
}

static Node* findLastFromDistributedNode(const Node* node, const InsertionPoint* insertionPoint)
{
    for (const Node* next = node; next; next = insertionPoint->previousDistributedTo(next)) {
        if (Node* found = findLastEnteringInsertionPoints(next))
            return found;
    }
    return nullptr;
}

enum ShadowRootCrossing { CrossShadowRoot, DontCrossShadowRoot };

static ContainerNode* traverseParent(const Node* node, ShadowRootCrossing shadowRootCrossing)
{
    if (node->isPseudoElement())
        return toPseudoElement(node)->hostElement();

    if (shadowRootCrossing == DontCrossShadowRoot  && node->isShadowRoot())
        return 0;

    if (nodeCanBeDistributed(node)) {
        if (InsertionPoint* insertionPoint = findInsertionPointOf(node))
            return traverseParent(insertionPoint, shadowRootCrossing);
        return nullptr;
    }
    ContainerNode* parent = node->parentNode();
    if (!parent)
        return nullptr;

    if (parent->isShadowRoot())
        return shadowRootCrossing == CrossShadowRoot ? toShadowRoot(parent)->hostElement() : parent;

    if (parent->isInsertionPoint()) {
        const InsertionPoint* insertionPoint = toInsertionPoint(parent);
        if (insertionPoint->hasDistribution())
            return nullptr;
        if (insertionPoint->isActive())
            return traverseParent(parent, shadowRootCrossing);
    }
    return parent;
}

static Node* traverseFirstChild(const Node* node, ShadowRootCrossing shadowRootCrossing)
{
    ASSERT(node);
    if (node->shadowRoot()) {
        if (shadowRootCrossing == DontCrossShadowRoot)
            return nullptr;
        node = node->shadowRoot();
    }
    return findFirstSiblingEnteringInsertionPoints(node->firstChild());
}

static Node* traverseLastChild(const Node* node, ShadowRootCrossing shadowRootCrossing)
{
    ASSERT(node);
    if (node->shadowRoot()) {
        if (shadowRootCrossing == DontCrossShadowRoot)
            return nullptr;
        node = node->shadowRoot();
    }
    return findLastSiblingEnteringInsertionPoints(node->lastChild());
}

static Node* traverseNextSibling(const Node* node)
{
    ASSERT(node);

    InsertionPoint* insertionPoint;
    if (nodeCanBeDistributed(node) && (insertionPoint = findInsertionPointOf(node))) {
        Node* found = findFirstFromDistributedNode(insertionPoint->nextDistributedTo(node), insertionPoint);
        if (found)
            return found;
        return traverseNextSibling(insertionPoint);
    }

    for (const Node* sibling = node->nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (Node* found = findFirstEnteringInsertionPoints(sibling))
            return found;
    }
    if (node->parentNode() && isActiveInsertionPoint(node->parentNode()))
        return traverseNextSibling(node->parentNode());

    return nullptr;
}

static Node* traversePreviousSibling(const Node* node)
{
    ASSERT(node);

    InsertionPoint* insertionPoint;
    if (nodeCanBeDistributed(node) && (insertionPoint = findInsertionPointOf(node))) {
        Node* found = findLastFromDistributedNode(insertionPoint->previousDistributedTo(node), insertionPoint);
        if (found)
            return found;
        return traversePreviousSibling(insertionPoint);
    }

    for (const Node* sibling = node->previousSibling(); sibling; sibling = sibling->previousSibling()) {
        if (Node* found = findLastEnteringInsertionPoints(sibling))
            return found;
    }
    if (node->parentNode() && isActiveInsertionPoint(node->parentNode()))
        return traversePreviousSibling(node->parentNode());

    return nullptr;
}

ContainerNode* parentSlow(const Node* node)
{
    ASSERT(!node->isShadowRoot());

    return traverseParent(node, CrossShadowRoot);
}

Node* nextSiblingSlow(const Node* node)
{
    ASSERT(!node->isShadowRoot());

    // FIXME: Why do these functions deal with before/after when other code here doesn't?
    Node* nextSibling = 0;
    if (node->isBeforePseudoElement()) {
        nextSibling = traverseParent(node, CrossShadowRoot);
        nextSibling = traverseFirstChild(nextSibling, CrossShadowRoot);
    } else
        nextSibling = traverseNextSibling(node);

    if (nextSibling || node->isAfterPseudoElement())
        return nextSibling;

    Node* parent = traverseParent(node, CrossShadowRoot);
    if (parent && parent->isElementNode())
        return toElement(parent)->afterPseudoElement();

    return 0;
}

Node* previousSiblingSlow(const Node* node)
{
    ASSERT(!node->isShadowRoot());

    Node* previousSibling = 0;
    if (node->isAfterPseudoElement()) {
        ContainerNode* parent = traverseParent(node, CrossShadowRoot);
        previousSibling = traverseLastChild(parent, CrossShadowRoot);
    } else
        previousSibling = traversePreviousSibling(node);

    if (previousSibling || node->isBeforePseudoElement())
        return previousSibling;

    ContainerNode* parent = traverseParent(node, CrossShadowRoot);
    if (parent && parent->isElementNode())
        return toElement(parent)->beforePseudoElement();

    return 0;
}

Node* nextInScope(const Node* node)
{
    ASSERT(!isActiveInsertionPoint(node));

    if (Node* next = traverseFirstChild(node, DontCrossShadowRoot))
        return next;
    if (Node* next = traverseNextSibling(node))
        return next;
    const Node* current = node;
    while (current && !traverseNextSibling(current))
        current = traverseParent(current, DontCrossShadowRoot);
    return current ? traverseNextSibling(current) : 0;
}

Node* previousInScope(const Node* node)
{
    ASSERT(!isActiveInsertionPoint(node));

    if (Node* current = traversePreviousSibling(node)) {
        while (Node* child = traverseLastChild(current, DontCrossShadowRoot))
            current = child;
        return current;
    }
    return traverseParent(node, DontCrossShadowRoot);
}

Node* parentInScope(const Node* node)
{
    ASSERT(!isActiveInsertionPoint(node));

    return traverseParent(node, DontCrossShadowRoot);
}

Node* lastChildInScope(const Node* node)
{
    ASSERT(!isActiveInsertionPoint(node));

    return traverseLastChild(node, DontCrossShadowRoot);
}

}

} // namespace
