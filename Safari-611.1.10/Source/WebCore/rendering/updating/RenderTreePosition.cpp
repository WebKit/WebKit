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
#include "PseudoElement.h"
#include "RenderInline.h"
#include "RenderObject.h"
#include "ShadowRoot.h"

namespace WebCore {

void RenderTreePosition::computeNextSibling(const Node& node)
{
    ASSERT(!node.renderer());
    if (m_hasValidNextSibling) {
#if ASSERT_ENABLED
        const unsigned oNSquaredAvoidanceLimit = 20;
        bool skipAssert = m_parent.isRenderView() || ++m_assertionLimitCounter > oNSquaredAvoidanceLimit;
        ASSERT(skipAssert || nextSiblingRenderer(node) == m_nextSibling);
#endif
        return;
    }
    m_nextSibling = makeWeakPtr(nextSiblingRenderer(node));
    m_hasValidNextSibling = true;
}

void RenderTreePosition::invalidateNextSibling(const RenderObject& siblingRenderer)
{
    if (!m_hasValidNextSibling)
        return;
    if (m_nextSibling == &siblingRenderer)
        m_hasValidNextSibling = false;
}

RenderObject* RenderTreePosition::nextSiblingRenderer(const Node& node) const
{
    ASSERT(!node.renderer());

    auto* parentElement = m_parent.element();
    if (!parentElement)
        return nullptr;
    // FIXME: PlugingReplacement shadow trees are very wrong.
    if (parentElement == &node)
        return nullptr;

    Vector<Element*, 30> elementStack;

    // In the common case ancestor == parentElement immediately and this just pushes parentElement into stack.
    auto* ancestor = node.parentElementInComposedTree();
    while (true) {
        elementStack.append(ancestor);
        if (ancestor == parentElement)
            break;
        ancestor = ancestor->parentElementInComposedTree();
        ASSERT(ancestor);
    }
    elementStack.reverse();

    auto composedDescendants = composedTreeDescendants(*parentElement);

    auto initializeIteratorConsideringPseudoElements = [&] {
        if (is<PseudoElement>(node)) {
            auto* host = downcast<PseudoElement>(node).hostElement();
            if (node.isBeforePseudoElement()) {
                if (host != parentElement)
                    return composedDescendants.at(*host).traverseNext();
                return composedDescendants.begin();
            }
            ASSERT(node.isAfterPseudoElement());
            elementStack.removeLast();
            if (host != parentElement)
                return composedDescendants.at(*host).traverseNextSkippingChildren();
            return composedDescendants.end();
        }
        return composedDescendants.at(node).traverseNextSkippingChildren();
    };

    auto pushCheckingForAfterPseudoElementRenderer = [&] (Element& element) -> RenderElement* {
        ASSERT(!element.isPseudoElement());
        if (auto* before = element.beforePseudoElement()) {
            if (auto* renderer = before->renderer())
                return renderer;
        }
        elementStack.append(&element);
        return nullptr;
    };

    auto popCheckingForAfterPseudoElementRenderers = [&] (unsigned iteratorDepthToMatch) -> RenderElement* {
        while (elementStack.size() > iteratorDepthToMatch) {
            auto& element = *elementStack.takeLast();
            if (auto* after = element.afterPseudoElement()) {
                if (auto* renderer = after->renderer())
                    return renderer;
            }
        }
        return nullptr;
    };

    auto it = initializeIteratorConsideringPseudoElements();
    auto end = composedDescendants.end();

    while (it != end) {
        if (auto* renderer = popCheckingForAfterPseudoElementRenderers(it.depth()))
            return renderer;

        if (auto* renderer = it->renderer())
            return renderer;

        if (is<Element>(*it)) {
            auto& element = downcast<Element>(*it);
            if (element.hasDisplayContents()) {
                if (auto* renderer = pushCheckingForAfterPseudoElementRenderer(element))
                    return renderer;
                it.traverseNext();
                continue;
            }
        }

        it.traverseNextSkippingChildren();
    }

    return popCheckingForAfterPseudoElementRenderers(0);
}

}
