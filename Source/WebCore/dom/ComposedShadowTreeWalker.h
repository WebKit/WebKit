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

#ifndef ComposedShadowTreeWalker_h
#define ComposedShadowTreeWalker_h

#include "InsertionPoint.h"
#include "ShadowRoot.h"

namespace WebCore {

class Node;
class ShadowRoot;

// FIXME: Make some functions inline to optimise the performance.
// https://bugs.webkit.org/show_bug.cgi?id=82702
class ComposedShadowTreeWalker {
public:
    enum Policy {
        CrossUpperBoundary,
        DoNotCrossUpperBoundary,
    };

    ComposedShadowTreeWalker(const Node*, Policy = CrossUpperBoundary);

    // For a common use case such as:
    // for (ComposedShadowTreeWalker walker = ComposedShadowTreeWalker::fromFirstChild(node); walker.get(); walker.nextSibling())
    static ComposedShadowTreeWalker fromFirstChild(const Node*, Policy = CrossUpperBoundary);

    Node* get() const { return const_cast<Node*>(m_node); }

    void firstChild();
    void lastChild();

    void nextSibling();
    void previousSibling();

    void parent();
    // This function ignores policy and always crosses an upper boundary.
    void parentIncludingInsertionPointAndShadowRoot();

    void next();
    void previous();

private:
    enum TraversalDirection {
        TraversalDirectionForward,
        TraversalDirectionBackward
    };

    bool canCrossUpperBoundary() const { return m_policy == CrossUpperBoundary; }

    void assertPrecondition() const
    {
#ifndef NDEBUG
        ASSERT(m_node);
        if (canCrossUpperBoundary())
            ASSERT(!m_node->isShadowRoot());
        else
            ASSERT(!m_node->isShadowRoot() || toShadowRoot(m_node)->isYoungest());
        ASSERT(!isInsertionPoint(m_node) || !toInsertionPoint(m_node)->isActive());
#endif
    }

    void assertPostcondition() const
    {
#ifndef NDEBUG
        if (m_node)
            assertPrecondition();
#endif
    }

    static Node* traverseNode(const Node*, TraversalDirection);
    static Node* traverseLightChildren(const Node*, TraversalDirection);

    Node* traverseFirstChild(const Node*) const;
    Node* traverseLastChild(const Node*) const;
    Node* traverseChild(const Node*, TraversalDirection) const;
    Node* traverseParent(const Node*) const;
    Node* traverseParentIncludingInsertionPointAndShadowRoot(const Node*) const;

    static Node* traverseNextSibling(const Node*);
    static Node* traversePreviousSibling(const Node*);

    static Node* traverseSiblingOrBackToInsertionPoint(const Node*, TraversalDirection);
    static Node* traverseSiblingInCurrentTree(const Node*, TraversalDirection);

    static Node* traverseSiblingOrBackToYoungerShadowRoot(const Node*, TraversalDirection);
    static Node* escapeFallbackContentElement(const Node*, TraversalDirection);

    Node* traverseNodeEscapingFallbackContents(const Node*) const;
    Node* traverseParentInCurrentTree(const Node*) const;
    Node* traverseParentBackToYoungerShadowRootOrHost(const ShadowRoot*) const;

    const Node* m_node;
    Policy m_policy;
};

} // namespace

#endif
