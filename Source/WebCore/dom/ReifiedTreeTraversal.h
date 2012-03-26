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

#ifndef ReifiedTreeTraversal_h
#define ReifiedTreeTraversal_h

namespace WebCore {

class Node;
class ShadowRoot;

enum ReifiedTreeTraversalDirection {
    ReifiedTreeTraversalDirectionForward,
    ReifiedTreeTraversalDirectionBackward
};

// FIXME: Make some functions inline to optimise the performance.
// https://bugs.webkit.org/show_bug.cgi?id=82019
class ReifiedTreeTraversal {
public:
    enum CrossedUpperBoundary {
        NotCrossed,
        Crossed,
    };

    static Node* firstChild(const Node*);
    static Node* lastChild(const Node*);
    static Node* lastChildWithoutCrossingUpperBoundary(const Node*);

    static Node* nextSibling(const Node*);
    static Node* previousSibling(const Node*);

    static Node* parentNode(const Node*, CrossedUpperBoundary&);
    static Node* parentNode(const Node*);
    static Node* parentNodeWithoutCrossingUpperBoundary(const Node*);

    static Node* traverseNextNode(const Node*);
    static Node* traversePreviousNode(const Node*);

    // FIXME: An 'adjusted' is not good name for general use case. Rename this.
    // https://bugs.webkit.org/show_bug.cgi?id=82022
    // This may return ShadowRoot or InsertionPoint.
    static Node* adjustedParentNode(const Node*);

    static Node* traverseNextNodeWithoutCrossingUpperBoundary(const Node*);
    static Node* traversePreviousNodeWithoutCrossingUpperBoundary(const Node*);

private:
    static Node* traverseChild(const Node*, ReifiedTreeTraversalDirection);
    static Node* traverseNode(const Node*, ReifiedTreeTraversalDirection);
    static Node* traverseLightChildren(const Node*, ReifiedTreeTraversalDirection);

    static Node* traverseSiblingOrBackToInsertionPoint(const Node*, ReifiedTreeTraversalDirection);
    static Node* traverseSiblingInCurrentTree(const Node*, ReifiedTreeTraversalDirection);

    static Node* traverseSiblingOrBackToYoungerShadowRoot(const Node*, ReifiedTreeTraversalDirection);
    static Node* escapeFallbackContentElement(const Node*, ReifiedTreeTraversalDirection);

    static Node* traverseNodeEscapingFallbackContents(const Node*, CrossedUpperBoundary&);

    static Node* parentNodeOrBackToInsertionPoint(const Node*, CrossedUpperBoundary&);
    static Node* parentNodeInCurrentTree(const Node*, CrossedUpperBoundary&);
    static Node* parentNodeBackToYoungerShadowRootOrHost(const ShadowRoot*, CrossedUpperBoundary&);
};

} // namespace

#endif
