/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "ReifiedTreeTraversal.h"

#include "Element.h"
#include "HTMLContentSelector.h"
#include "InsertionPoint.h"
#include "ShadowTree.h"

namespace WebCore {

static inline bool isShadowHost(const Node* node)
{
    return node && node->isElementNode() && toElement(node)->hasShadowRoot();
}

static inline ShadowTree* shadowTreeFor(const Node* node)
{
    if (node && node->isElementNode())
        return toElement(node)->shadowTree();
    return 0;
}

static inline ShadowTree* shadowTreeOfParent(const Node* node)
{
    if (node && node->parentNode())
        return shadowTreeFor(node->parentNode());
    return 0;
}

Node* ReifiedTreeTraversal::firstChild(const Node* node)
{
    ASSERT(node);
    Node* child = traverseChild(node, ReifiedTreeTraversalDirectionForward);
    ASSERT(!child || !child->isShadowRoot());
    return child;
}

Node* ReifiedTreeTraversal::lastChild(const Node* node)
{
    ASSERT(node);
    Node* child = traverseChild(node, ReifiedTreeTraversalDirectionBackward);
    ASSERT(!child || !child->isShadowRoot());
    return child;
}

Node* ReifiedTreeTraversal::lastChildWithoutCrossingUpperBoundary(const Node* node)
{
    ASSERT(node);
    Node* child = traverseLightChildren(node, ReifiedTreeTraversalDirectionBackward);
    ASSERT(!child || !child->isShadowRoot());
    return child;
}

Node* ReifiedTreeTraversal::traverseChild(const Node* node, ReifiedTreeTraversalDirection direction)
{
    ASSERT(node);
    // FIXME: Add an assertion once InsertionPoint have isActive() function.
    // https://bugs.webkit.org/show_bug.cgi?id=82010
    // ASSERT(!isInsertionPoint(node) || !toInsertionPoint(node)->isActive());
    ASSERT(!node->isShadowRoot());
    ShadowTree* shadowTree = shadowTreeFor(node);
    if (shadowTree && shadowTree->hasShadowRoot())
        return traverseLightChildren(shadowTree->youngestShadowRoot(), direction);
    return traverseLightChildren(node, direction);
}

Node* ReifiedTreeTraversal::traverseLightChildren(const Node* node, ReifiedTreeTraversalDirection direction)
{
    ASSERT(node);
    if (Node* child = (direction == ReifiedTreeTraversalDirectionForward ? node->firstChild() : node->lastChild()))
        return traverseNode(child, direction);
    return 0;
}

Node* ReifiedTreeTraversal::traverseNode(const Node* node, ReifiedTreeTraversalDirection direction)
{
    ASSERT(node);
    if (isInsertionPoint(node)) {
        const HTMLContentSelectionList* selectionList = toInsertionPoint(node)->selections();
        if (HTMLContentSelection* selection = (direction == ReifiedTreeTraversalDirectionForward ? selectionList->first() : selectionList->last()))
            return traverseNode(selection->node(), direction);
        return traverseLightChildren(node, direction);
    }
    return const_cast<Node*>(node);
}

Node* ReifiedTreeTraversal::nextSibling(const Node* node)
{
    ASSERT(node);
    ASSERT(!node->isShadowRoot());
    Node* next = traverseSiblingOrBackToInsertionPoint(node, ReifiedTreeTraversalDirectionForward);
    ASSERT(!next || !next->isShadowRoot());
    return next;
}

Node* ReifiedTreeTraversal::previousSibling(const Node* node)
{
    ASSERT(node);
    ASSERT(!node->isShadowRoot());
    Node* next = traverseSiblingOrBackToInsertionPoint(node, ReifiedTreeTraversalDirectionBackward);
    ASSERT(!next || !next->isShadowRoot());
    return next;
}

Node* ReifiedTreeTraversal::traverseSiblingOrBackToInsertionPoint(const Node* node, ReifiedTreeTraversalDirection direction)
{
    ASSERT(node);
    ShadowTree* shadowTree = shadowTreeOfParent(node);
    if (!shadowTree)
        return traverseSiblingInCurrentTree(node, direction);
    HTMLContentSelection* selection = shadowTree->selectionFor(node);
    if (!selection)
        return traverseSiblingInCurrentTree(node, direction);
    if (HTMLContentSelection* nextSelection = (direction == ReifiedTreeTraversalDirectionForward ? selection->next() : selection->previous()))
        return traverseNode(nextSelection->node(), direction);
    return traverseSiblingOrBackToInsertionPoint(selection->insertionPoint(), direction);
}

Node* ReifiedTreeTraversal::traverseSiblingInCurrentTree(const Node* node, ReifiedTreeTraversalDirection direction)
{
    ASSERT(node);
    if (Node* next = (direction == ReifiedTreeTraversalDirectionForward ? node->nextSibling() : node->previousSibling()))
        return traverseNode(next, direction);
    if (Node* next = traverseSiblingOrBackToYoungerShadowRoot(node, direction))
        return next;
    return escapeFallbackContentElement(node, direction);
}

Node* ReifiedTreeTraversal::traverseSiblingOrBackToYoungerShadowRoot(const Node* node, ReifiedTreeTraversalDirection direction)
{
    ASSERT(node);
    if (node->parentNode() && node->parentNode()->isShadowRoot()) {
        ShadowRoot* parentShadowRoot = toShadowRoot(node->parentNode());
        if (!parentShadowRoot->isYoungest()) {
            InsertionPoint* assignedInsertionPoint = parentShadowRoot->assignedTo();
            ASSERT(assignedInsertionPoint);
            return traverseSiblingInCurrentTree(assignedInsertionPoint, direction);
        }
    }
    return 0;
}

Node* ReifiedTreeTraversal::escapeFallbackContentElement(const Node* node, ReifiedTreeTraversalDirection direction)
{
    ASSERT(node);
    if (node->parentNode() && isInsertionPoint(node->parentNode()))
        return traverseSiblingOrBackToInsertionPoint(node->parentNode(), direction);
    return 0;
}

Node* ReifiedTreeTraversal::traverseNodeEscapingFallbackContents(const Node* node, CrossedUpperBoundary& crossed)
{
    ASSERT(node);
    if (isInsertionPoint(node))
        return parentNodeOrBackToInsertionPoint(node, crossed);
    crossed = NotCrossed;
    return const_cast<Node*>(node);
}

Node* ReifiedTreeTraversal::parentNode(const Node* node, CrossedUpperBoundary& crossed)
{
    ASSERT(node);
    ASSERT(!node->isShadowRoot());
    crossed = NotCrossed;
    // FIXME: Add an assertion once InsertionPoint have isActive() function.
    // https://bugs.webkit.org/show_bug.cgi?id=82010
    // ASSERT(!isInsertionPoint(node) || !toInsertionPoint(node)->isActive());
    return parentNodeOrBackToInsertionPoint(node, crossed);
}

Node* ReifiedTreeTraversal::parentNode(const Node* node)
{
    CrossedUpperBoundary crossed;
    Node* parent = parentNode(node, crossed);
    UNUSED_PARAM(crossed);
    return parent;
}

Node* ReifiedTreeTraversal::parentNodeWithoutCrossingUpperBoundary(const Node* node)
{
    ASSERT(node);
    ASSERT(!node->isShadowRoot());
    // FIXME: Add an assertion once InsertionPoint have isActive() function.
    // https://bugs.webkit.org/show_bug.cgi?id=82010
    // ASSERT(!isInsertionPoint(node) || !toInsertionPoint(node)->isActive());
    CrossedUpperBoundary crossed;
    Node* parent = parentNodeOrBackToInsertionPoint(node, crossed);
    if (crossed == Crossed)
        return 0;
    return parent;
}

Node* ReifiedTreeTraversal::parentNodeOrBackToInsertionPoint(const Node* node, CrossedUpperBoundary& crossed)
{
    ASSERT(crossed == NotCrossed);
    if (ShadowTree* shadowTree = shadowTreeOfParent(node)) {
        if (HTMLContentSelection* selection = shadowTree->selectionFor(node))
            return parentNodeOrBackToInsertionPoint(selection->insertionPoint(), crossed);
    }
    return parentNodeInCurrentTree(node, crossed);
}

Node* ReifiedTreeTraversal::parentNodeInCurrentTree(const Node* node, CrossedUpperBoundary& crossed)
{
    ASSERT(crossed == NotCrossed);
    if (Node* parent = node->parentNode()) {
        if (parent->isShadowRoot())
            return parentNodeBackToYoungerShadowRootOrHost(toShadowRoot(parent), crossed);
        return traverseNodeEscapingFallbackContents(parent, crossed);
    }
    return 0;
}

Node* ReifiedTreeTraversal::parentNodeBackToYoungerShadowRootOrHost(const ShadowRoot* shadowRoot, CrossedUpperBoundary& crossed)
{
    ASSERT(shadowRoot);
    ASSERT(crossed == NotCrossed);
    if (shadowRoot->isYoungest()) {
        crossed = Crossed;
        return shadowRoot->host();
    }
    InsertionPoint* assignedInsertionPoint = shadowRoot->assignedTo();
    ASSERT(assignedInsertionPoint);
    return parentNodeOrBackToInsertionPoint(assignedInsertionPoint, crossed);
}

Node* ReifiedTreeTraversal::adjustedParentNode(const Node* node)
{
    if (ShadowTree* shadowTree = shadowTreeOfParent(node)) {
        if (HTMLContentSelection* selection = shadowTree->selectionFor(node))
            return selection->insertionPoint();
    }
    if (node->isShadowRoot()) {
        const ShadowRoot* shadowRoot = toShadowRoot(node);
        if (!shadowRoot->isYoungest()) {
            InsertionPoint* assignedInsertionPoint = shadowRoot->assignedTo();
            ASSERT(assignedInsertionPoint);
            return assignedInsertionPoint;
        }
        return shadowRoot->host();
    }
    return node->parentNode();
}

Node* ReifiedTreeTraversal::traverseNextNode(const Node* node)
{
    if (Node* next = firstChild(node))
        return next;
    if (Node* next = nextSibling(node))
        return next;
    const Node* n = node;
    while (n && !nextSibling(n))
        n = parentNode(n);
    if (n)
        return nextSibling(n);
    return 0;
}

Node* ReifiedTreeTraversal::traverseNextNodeWithoutCrossingUpperBoundary(const Node* node)
{
    if (!isShadowHost(node)) {
        if (Node* next = traverseLightChildren(node, ReifiedTreeTraversalDirectionForward))
            return next;
    }
    if (Node* next = nextSibling(node))
        return next;
    const Node* n = node;
    while (n && !nextSibling(n))
        n = parentNodeWithoutCrossingUpperBoundary(n);
    if (n)
        return nextSibling(n);
    return 0;
}

Node* ReifiedTreeTraversal::traversePreviousNode(const Node* node)
{
    if (Node* n = previousSibling(node)) {
        while (Node* child = ReifiedTreeTraversal::lastChild(n))
            n = child;
        return n;
    }
    return parentNode(node);
}

Node* ReifiedTreeTraversal::traversePreviousNodeWithoutCrossingUpperBoundary(const Node* node)
{
    if (Node* n = previousSibling(node)) {
        while (!isShadowHost(n)) {
            if (Node* child = ReifiedTreeTraversal::traverseLightChildren(n, ReifiedTreeTraversalDirectionBackward))
                n = child;
            else
                break;
        }
        return n;
    }
    return parentNodeWithoutCrossingUpperBoundary(node);
}

} // namespace
