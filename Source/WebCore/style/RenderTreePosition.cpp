/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderTreePosition.h"

#include "ComposedTreeIterator.h"
#include "HTMLSlotElement.h"
#include "PseudoElement.h"
#include "RenderObject.h"
#include "ShadowRoot.h"

namespace WebCore {

void RenderTreePosition::computeNextSibling(const Node& node)
{
    ASSERT(!node.renderer());
    if (m_hasValidNextSibling) {
#if !ASSERT_DISABLED
        const unsigned oNSquaredAvoidanceLimit = 20;
        bool skipAssert = m_parent.isRenderView() || ++m_assertionLimitCounter > oNSquaredAvoidanceLimit;
        ASSERT(skipAssert || nextSiblingRenderer(node) == m_nextSibling);
#endif
        return;
    }
    m_nextSibling = nextSiblingRenderer(node);
    m_hasValidNextSibling = true;
}

void RenderTreePosition::invalidateNextSibling(const RenderObject& siblingRenderer)
{
    if (!m_hasValidNextSibling)
        return;
    if (m_nextSibling == &siblingRenderer)
        m_hasValidNextSibling = false;
}

RenderObject* RenderTreePosition::previousSiblingRenderer(const Text& textNode) const
{
    if (textNode.renderer())
        return textNode.renderer()->previousSibling();

    auto* parentElement = m_parent.element();

    auto composedChildren = composedTreeChildren(*parentElement);
    for (auto it = composedChildren.at(textNode), end = composedChildren.end(); it != end; --it) {
        RenderObject* renderer = it->renderer();
        if (renderer && !RenderTreePosition::isRendererReparented(*renderer))
            return renderer;
    }
    if (auto* before = parentElement->beforePseudoElement())
        return before->renderer();
    return nullptr;
}

RenderObject* RenderTreePosition::nextSiblingRenderer(const Node& node) const
{
    auto* parentElement = m_parent.element();
    if (!parentElement)
        return nullptr;
    if (node.isAfterPseudoElement())
        return nullptr;

    auto composedDescendants = composedTreeDescendants(*parentElement);
    auto it = node.isBeforePseudoElement() ? composedDescendants.begin() : composedDescendants.at(node);
    auto end = composedDescendants.end();

    while (it != end) {
        auto& node = *it;
        bool hasDisplayContents = is<Element>(node) && downcast<Element>(node).hasDisplayContents();
        if (hasDisplayContents) {
            it.traverseNext();
            continue;
        }
        RenderObject* renderer = node.renderer();
        if (renderer && !isRendererReparented(*renderer))
            return renderer;
        
        it.traverseNextSkippingChildren();
    }
    if (PseudoElement* after = parentElement->afterPseudoElement())
        return after->renderer();
    return nullptr;
}

bool RenderTreePosition::isRendererReparented(const RenderObject& renderer)
{
    if (!renderer.node()->isElementNode())
        return false;
    if (renderer.style().hasFlowInto())
        return true;
    return false;
}

}
