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

    enum StartPolicy {
        CanStartFromShadowBoundary,
        CannotStartFromShadowBoundary
    };

    class ParentTranversalDetails {
    public:
        ParentTranversalDetails()
            : m_node(0)
            , m_insertionPoint(0)
            , m_resetStyleInheritance(false)
            , m_outOfComposition(false)
        { }

        ContainerNode* node() const { return m_node; }
        InsertionPoint* insertionPoint() const { return m_insertionPoint; }
        bool resetStyleInheritance() const { return m_resetStyleInheritance; }
        bool outOfComposition() const { return m_outOfComposition; }

        void didFindNode(ContainerNode*);
        void didTraverseInsertionPoint(InsertionPoint*);
        void didTraverseShadowRoot(const ShadowRoot*);
        void childWasOutOfComposition() { m_outOfComposition = true; }

    private:
        ContainerNode* m_node;
        InsertionPoint* m_insertionPoint;
        bool m_resetStyleInheritance;
        bool m_outOfComposition;
    };

    ComposedShadowTreeWalker(const Node*, Policy = CrossUpperBoundary, StartPolicy = CannotStartFromShadowBoundary);

    // For a common use case such as:
    // for (ComposedShadowTreeWalker walker = ComposedShadowTreeWalker::fromFirstChild(node); walker.get(); walker.nextSibling())
    static ComposedShadowTreeWalker fromFirstChild(const Node*, Policy = CrossUpperBoundary);
    static void findParent(const Node*, ParentTranversalDetails*);

    Node* get() const { return const_cast<Node*>(m_node); }

    void firstChild();
    void lastChild();

    void nextSibling();
    void previousSibling();

    void parent();

    void next();
    void previous();

private:
    ComposedShadowTreeWalker(const Node*, ParentTranversalDetails*);

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
        ASSERT(!isActiveInsertionPoint(m_node));
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
    Node* traverseParent(const Node*, ParentTranversalDetails* = 0) const;

    static Node* traverseNextSibling(const Node*);
    static Node* traversePreviousSibling(const Node*);

    static Node* traverseSiblingOrBackToInsertionPoint(const Node*, TraversalDirection);
    static Node* traverseSiblingInCurrentTree(const Node*, TraversalDirection);

    static Node* traverseSiblingOrBackToYoungerShadowRoot(const Node*, TraversalDirection);
    static Node* escapeFallbackContentElement(const Node*, TraversalDirection);

    Node* traverseNodeEscapingFallbackContents(const Node*, ParentTranversalDetails* = 0) const;
    Node* traverseParentInCurrentTree(const Node*, ParentTranversalDetails* = 0) const;
    Node* traverseParentBackToYoungerShadowRootOrHost(const ShadowRoot*, ParentTranversalDetails* = 0) const;

    const Node* m_node;
    Policy m_policy;
};

inline ComposedShadowTreeWalker::ComposedShadowTreeWalker(const Node* node, Policy policy, StartPolicy startPolicy)
    : m_node(node)
    , m_policy(policy)
{
    UNUSED_PARAM(startPolicy);
#ifndef NDEBUG
    if (m_node && startPolicy == CannotStartFromShadowBoundary)
        assertPrecondition();
#endif
}

// A special walker class which is only used for traversing a parent node, including
// insertion points and shadow roots.
class ComposedShadowTreeParentWalker {
public:
    ComposedShadowTreeParentWalker(const Node*);
    void parentIncludingInsertionPointAndShadowRoot();
    Node* get() const { return const_cast<Node*>(m_node); }
private:
    Node* traverseParentIncludingInsertionPointAndShadowRoot(const Node*) const;
    const Node* m_node;
};

} // namespace

#endif
