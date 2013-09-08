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

#ifndef NodeRenderingTraversal_h
#define NodeRenderingTraversal_h

#include "ContainerNode.h"

namespace WebCore {

namespace NodeRenderingTraversal {

ContainerNode* parent(const Node*);
Node* nextSibling(const Node*);
Node* previousSibling(const Node*);

Node* nextInScope(const Node*);
Node* previousInScope(const Node*);
Node* parentInScope(const Node*);
Node* lastChildInScope(const Node*);

ContainerNode* parentSlow(const Node*);
Node* nextSiblingSlow(const Node*);
Node* previousSiblingSlow(const Node*);

inline ContainerNode* parent(const Node* node)
{
    if (node->needsNodeRenderingTraversalSlowPath())
        return parentSlow(node);

    ASSERT(node->parentNode() == parentSlow(node));
    return node->parentNodeGuaranteedHostFree();
}

inline Node* nextSibling(const Node* node)
{
    if (node->needsNodeRenderingTraversalSlowPath())
        return nextSiblingSlow(node);

    ASSERT(nextSiblingSlow(node) == node->nextSibling());
    return node->nextSibling();
}

inline Node* previousSibling(const Node* node)
{
    if (node->needsNodeRenderingTraversalSlowPath())
        return previousSiblingSlow(node);

    ASSERT(previousSiblingSlow(node) == node->previousSibling());
    return node->previousSibling();
}

}

} // namespace WebCore

#endif
