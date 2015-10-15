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

#include "ShadowRoot.h"

namespace WebCore {

namespace NodeRenderingTraversal {

static ContainerNode* traverseParent(const Node* node)
{
    if (node->isShadowRoot())
        return nullptr;

    ContainerNode* parent = node->parentNode();
    if (parent && parent->shadowRoot())
        return nullptr;

    return parent;
}

static Node* traverseFirstChild(const Node* node)
{
    ASSERT(node);
    if (node->shadowRoot())
        return nullptr;
    return node->firstChild();
}

static Node* traverseLastChild(const Node* node)
{
    ASSERT(node);
    if (node->shadowRoot())
        return nullptr;
    return node->lastChild();
}

static Node* traverseNextSibling(const Node* node)
{
    ASSERT(node);
    return node->nextSibling();
}

static Node* traversePreviousSibling(const Node* node)
{
    ASSERT(node);
    return node->previousSibling();
}

Node* nextInScope(const Node* node)
{
    if (Node* next = traverseFirstChild(node))
        return next;
    if (Node* next = traverseNextSibling(node))
        return next;
    const Node* current = node;
    while (current && !traverseNextSibling(current))
        current = traverseParent(current);
    return current ? traverseNextSibling(current) : 0;
}

Node* previousInScope(const Node* node)
{
    if (Node* current = traversePreviousSibling(node)) {
        while (Node* child = traverseLastChild(current))
            current = child;
        return current;
    }
    return traverseParent(node);
}

Node* parentInScope(const Node* node)
{
    return traverseParent(node);
}

Node* lastChildInScope(const Node* node)
{
    return traverseLastChild(node);
}

}

} // namespace
