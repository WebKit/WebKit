/*
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
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
#include "TreeScopeAdjuster.h"

#include "Node.h"
#include "TreeScope.h"

namespace WebCore {

TreeScopeAdjuster::TreeScopeAdjuster(TreeScope* treeScope)
    : m_treeScope(treeScope)
{
}

Node* TreeScopeAdjuster::ancestorInThisScope(Node* node)
{
    do {
        if (node->treeScope() == treeScope())
            return node;
        if (!node->isInShadowTree())
            return 0;
    } while ((node = node->shadowAncestorNode()));

    return 0;
}

Position TreeScopeAdjuster::adjustPositionBefore(const Position& currentPosition)
{
    if (Node* ancestor = ancestorInThisScope(currentPosition.anchorNode()))
        return positionBeforeNode(ancestor);

    if (Node* lastChild = treeScope()->rootNode()->lastChild())
        return positionAfterNode(lastChild);

    return Position();
}

Position TreeScopeAdjuster::adjustPositionAfter(const Position& currentPosition)
{
    if (Node* ancestor = ancestorInThisScope(currentPosition.anchorNode()))
        return positionAfterNode(ancestor);

    if (Node* firstChild = treeScope()->rootNode()->firstChild())
        return positionBeforeNode(firstChild);

    return Position();
}

} // namespace WebCore

