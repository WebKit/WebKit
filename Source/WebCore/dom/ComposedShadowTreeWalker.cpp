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
#include "ComposedShadowTreeWalker.h"

#include "ContentDistributor.h"
#include "Element.h"
#include "ElementShadow.h"
#include "InsertionPoint.h"

namespace WebCore {

static inline ElementShadow* shadowFor(const Node* node)
{
    if (node && node->isElementNode())
        return toElement(node)->shadow();
    return 0;
}

static inline ElementShadow* shadowOfParent(const Node* node)
{
    if (!node)
        return 0;
    if (Node* parent = node->parentNode())
        if (parent->isElementNode())
            return toElement(parent)->shadow();
    return 0;
}

inline void ComposedShadowTreeWalker::ParentTraversalDetails::didTraverseInsertionPoint(InsertionPoint* insertionPoint)
{
    if (!m_insertionPoint)
        m_insertionPoint = insertionPoint;
}

inline void ComposedShadowTreeWalker::ParentTraversalDetails::didTraverseShadowRoot(const ShadowRoot* root)
{
    m_resetStyleInheritance  = m_resetStyleInheritance || root->resetStyleInheritance();
}

inline void ComposedShadowTreeWalker::ParentTraversalDetails::didFindNode(ContainerNode* node)
{
    if (!m_outOfComposition)
        m_node = node;
}

ComposedShadowTreeWalker ComposedShadowTreeWalker::fromFirstChild(const Node* node, Policy policy)
{
    ComposedShadowTreeWalker walker(node, policy);
    walker.firstChild();
    return walker;
}

void ComposedShadowTreeWalker::findParent(const Node* node, ParentTraversalDetails* details)
{
    ComposedShadowTreeWalker walker(node, CrossUpperBoundary, CanStartFromShadowBoundary);
    ContainerNode* found = toContainerNode(walker.traverseParent(walker.get(), details));
    if (found)
        details->didFindNode(found);
}

void ComposedShadowTreeWalker::firstChild()
{
    assertPrecondition();
    m_node = traverseChild(m_node, TraversalDirectionForward);
    assertPostcondition();
}

Node* ComposedShadowTreeWalker::traverseFirstChild(const Node* node) const
{
    ASSERT(node);
    return traverseChild(node, TraversalDirectionForward);
}

void ComposedShadowTreeWalker::lastChild()
{
    assertPrecondition();
    m_node = traverseLastChild(m_node);
    assertPostcondition();
}

Node* ComposedShadowTreeWalker::traverseLastChild(const Node* node) const
{
    ASSERT(node);
    return traverseChild(node, TraversalDirectionBackward);
}

Node* ComposedShadowTreeWalker::traverseChild(const Node* node, TraversalDirection direction) const
{
    ASSERT(node);
    if (canCrossUpperBoundary()) {
        ElementShadow* shadow = shadowFor(node);
        return shadow ? traverseLightChildren(shadow->youngestShadowRoot(), direction)
            : traverseLightChildren(node, direction);
    }
    if (isShadowHost(node))
        return 0;
    return traverseLightChildren(node, direction);
}

Node* ComposedShadowTreeWalker::traverseLightChildren(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    if (Node* child = (direction == TraversalDirectionForward ? node->firstChild() : node->lastChild()))
        return traverseNode(child, direction);
    return 0;
}

Node* ComposedShadowTreeWalker::traverseNode(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    if (!isInsertionPoint(node))
        return const_cast<Node*>(node);
    const InsertionPoint* insertionPoint = toInsertionPoint(node);
    if (!insertionPoint->isActive())
        return const_cast<Node*>(node);
    if (Node* next = (direction == TraversalDirectionForward ? insertionPoint->first() : insertionPoint->last()))
        return traverseNode(next, direction);
    return traverseLightChildren(node, direction);
}

void ComposedShadowTreeWalker::nextSibling()
{
    assertPrecondition();
    m_node = traverseSiblingOrBackToInsertionPoint(m_node, TraversalDirectionForward);
    assertPostcondition();
}

void ComposedShadowTreeWalker::previousSibling()
{
    assertPrecondition();
    m_node = traverseSiblingOrBackToInsertionPoint(m_node, TraversalDirectionBackward);
    assertPostcondition();
}

Node* ComposedShadowTreeWalker::traverseSiblingOrBackToInsertionPoint(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    ElementShadow* shadow = shadowOfParent(node);
    if (!shadow)
        return traverseSiblingInCurrentTree(node, direction);
    InsertionPoint* insertionPoint = shadow->insertionPointFor(node);
    if (!insertionPoint)
        return traverseSiblingInCurrentTree(node, direction);
    if (Node* next = (direction == TraversalDirectionForward ? insertionPoint->nextTo(node) : insertionPoint->previousTo(node)))
        return traverseNode(next, direction);
    return traverseSiblingOrBackToInsertionPoint(insertionPoint, direction);
}

Node* ComposedShadowTreeWalker::traverseSiblingInCurrentTree(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    if (Node* next = (direction == TraversalDirectionForward ? node->nextSibling() : node->previousSibling()))
        return traverseNode(next, direction);
    if (Node* next = traverseSiblingOrBackToYoungerShadowRoot(node, direction))
        return next;
    return escapeFallbackContentElement(node, direction);
}

Node* ComposedShadowTreeWalker::traverseSiblingOrBackToYoungerShadowRoot(const Node* node, TraversalDirection direction)
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

inline Node* ComposedShadowTreeWalker::escapeFallbackContentElement(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    if (node->parentNode() && isActiveInsertionPoint(node->parentNode()))
        return traverseSiblingOrBackToInsertionPoint(node->parentNode(), direction);
    return 0;
}

inline Node* ComposedShadowTreeWalker::traverseNodeEscapingFallbackContents(const Node* node, ParentTraversalDetails* details) const
{
    ASSERT(node);
    if (!isInsertionPoint(node))
        return const_cast<Node*>(node);
    const InsertionPoint* insertionPoint = toInsertionPoint(node);
    return insertionPoint->hasDistribution() ? 0 :
        insertionPoint->isActive() ? traverseParent(node, details) : const_cast<Node*>(node);
}

void ComposedShadowTreeWalker::parent()
{
    assertPrecondition();
    m_node = traverseParent(m_node);
    assertPostcondition();
}

// FIXME: Use an iterative algorithm so that it can be inlined.
// https://bugs.webkit.org/show_bug.cgi?id=90415
Node* ComposedShadowTreeWalker::traverseParent(const Node* node, ParentTraversalDetails* details) const
{
    if (!canCrossUpperBoundary() && node->isShadowRoot()) {
        ASSERT(toShadowRoot(node)->isYoungest());
        return 0;
    }
    if (ElementShadow* shadow = shadowOfParent(node)) {
        shadow->ensureDistribution();
        if (InsertionPoint* insertionPoint = shadow->insertionPointFor(node)) {
            if (details)
                details->didTraverseInsertionPoint(insertionPoint);
            return traverseParent(insertionPoint, details);
        }

        // The node is a non-distributed light child or older shadow's child.
        if (details)
            details->childWasOutOfComposition();
    }
    return traverseParentInCurrentTree(node, details);
}

inline Node* ComposedShadowTreeWalker::traverseParentInCurrentTree(const Node* node, ParentTraversalDetails* details) const
{
    if (Node* parent = node->parentNode())
        return parent->isShadowRoot() ? traverseParentBackToYoungerShadowRootOrHost(toShadowRoot(parent), details) : traverseNodeEscapingFallbackContents(parent, details);
    return 0;
}

Node* ComposedShadowTreeWalker::traverseParentBackToYoungerShadowRootOrHost(const ShadowRoot* shadowRoot, ParentTraversalDetails* details) const
{
    ASSERT(shadowRoot);
    if (shadowRoot->isYoungest()) {
        if (canCrossUpperBoundary()) {
            if (details)
                details->didTraverseShadowRoot(shadowRoot);
            return shadowRoot->host();
        }

        return const_cast<ShadowRoot*>(shadowRoot);
    }

    if (InsertionPoint* assignedInsertionPoint = shadowRoot->assignedTo()) {
        if (details)
            details->didTraverseShadowRoot(shadowRoot);
        return traverseParent(assignedInsertionPoint, details);
    }

    return 0;
}

Node* ComposedShadowTreeWalker::traverseNextSibling(const Node* node)
{
    ASSERT(node);
    return traverseSiblingOrBackToInsertionPoint(node, TraversalDirectionForward);
}

Node* ComposedShadowTreeWalker::traversePreviousSibling(const Node* node)
{
    ASSERT(node);
    return traverseSiblingOrBackToInsertionPoint(node, TraversalDirectionBackward);
}

void ComposedShadowTreeWalker::next()
{
    assertPrecondition();
    if (Node* next = traverseFirstChild(m_node))
        m_node = next;
    else if (Node* next = traverseNextSibling(m_node))
        m_node = next;
    else {
        const Node* n = m_node;
        while (n && !traverseNextSibling(n))
            n = traverseParent(n);
        m_node = n ? traverseNextSibling(n) : 0;
    }
    assertPostcondition();
}

void ComposedShadowTreeWalker::previous()
{
    assertPrecondition();
    if (Node* n = traversePreviousSibling(m_node)) {
        while (Node* child = traverseLastChild(n))
            n = child;
        m_node = n;
    } else
        parent();
    assertPostcondition();
}

ComposedShadowTreeParentWalker::ComposedShadowTreeParentWalker(const Node* node)
    : m_node(node)
{
}

void ComposedShadowTreeParentWalker::parentIncludingInsertionPointAndShadowRoot()
{
    ASSERT(m_node);
    m_node = traverseParentIncludingInsertionPointAndShadowRoot(m_node);
}

Node* ComposedShadowTreeParentWalker::traverseParentIncludingInsertionPointAndShadowRoot(const Node* node) const
{
    if (ElementShadow* shadow = shadowOfParent(node)) {
        if (InsertionPoint* insertionPoint = shadow->insertionPointFor(node))
            return insertionPoint;
    }
    if (!node->isShadowRoot())
        return node->parentNode();
    const ShadowRoot* shadowRoot = toShadowRoot(node);
    if (InsertionPoint* insertionPoint = shadowRoot->assignedTo())
        return insertionPoint;
    return shadowRoot->host();
}

} // namespace
