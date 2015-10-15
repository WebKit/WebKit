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
#include "ComposedTreeIterator.h"

#include "HTMLSlotElement.h"

namespace WebCore {

void ComposedTreeIterator::initializeShadowStack()
{
    // This code sets up the iterator for arbitrary node/root pair. It is not needed in common cases
    // or completes fast because node and root are close (like in composedTreeChildren(*parent).at(node) case).
    auto* node = m_current;
    while (node != &m_root) {
        auto* parent = node->parentNode();
        if (!parent) {
            m_current = nullptr;
            return;
        }
        if (is<ShadowRoot>(*parent)) {
            auto& shadowRoot = downcast<ShadowRoot>(*parent);
            if (m_shadowStack.isEmpty() || m_shadowStack.last().host != shadowRoot.host())
                m_shadowStack.append(shadowRoot.host());
            node = shadowRoot.host();
            continue;
        }
        if (auto* shadowRoot = parent->shadowRoot()) {
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
            m_shadowStack.append(shadowRoot->host());
            auto* assignedSlot = shadowRoot->findAssignedSlot(*node);
            if (assignedSlot) {
                size_t index = assignedSlot->assignedNodes()->find(node);
                ASSERT(index != notFound);

                m_shadowStack.last().currentSlot = assignedSlot;
                m_shadowStack.last().currentSlotNodeIndex = index;
                node = assignedSlot;
                continue;
            }
            // The node is not part of the composed tree.
#else
            UNUSED_PARAM(shadowRoot);
            m_current = nullptr;
            return;
#endif
        }
        node = parent;
    }
    m_shadowStack.reverse();
}

void ComposedTreeIterator::traverseNextInShadowTree()
{
    ASSERT(!m_shadowStack.isEmpty());

    auto* shadowContext = &m_shadowStack.last();

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    if (is<HTMLSlotElement>(*m_current) && !shadowContext->currentSlot) {
        auto& slot = downcast<HTMLSlotElement>(*m_current);
        if (auto* assignedNodes = slot.assignedNodes()) {
            shadowContext->currentSlot = &slot;
            shadowContext->currentSlotNodeIndex = 0;
            m_current = assignedNodes->at(0);
            return;
        }
    }
#endif

    m_current = NodeTraversal::next(*m_current, shadowContext->host);

    while (!m_current) {
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
        if (auto* slot = shadowContext->currentSlot) {
            bool nextNodeInSameSlot = ++shadowContext->currentSlotNodeIndex < slot->assignedNodes()->size();
            if (nextNodeInSameSlot) {
                m_current = slot->assignedNodes()->at(shadowContext->currentSlotNodeIndex);
                return;
            }
            m_current = NodeTraversal::nextSkippingChildren(*slot, shadowContext->host);
            shadowContext->currentSlot = nullptr;
            continue;
        }
#endif
        auto& previousHost = *shadowContext->host;
        m_shadowStack.removeLast();

        if (m_shadowStack.isEmpty()) {
            m_current = NodeTraversal::nextSkippingChildren(previousHost, &m_root);
            return;
        }
        shadowContext = &m_shadowStack.last();
        m_current = NodeTraversal::nextSkippingChildren(previousHost, shadowContext->host);
    }
}

void ComposedTreeIterator::traverseParentInShadowTree()
{
    ASSERT(!m_shadowStack.isEmpty());

    auto& shadowContext = m_shadowStack.last();

    auto* parent = m_current->parentNode();
    if (is<ShadowRoot>(parent)) {
        ASSERT(shadowContext.host == downcast<ShadowRoot>(*parent).host());

        m_current = shadowContext.host;
        m_shadowStack.removeLast();
        return;
    }
    if (parent->shadowRoot()) {
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
        ASSERT(shadowContext.host == parent);

        auto* slot = shadowContext.currentSlot;
        ASSERT(slot->assignedNodes()->at(shadowContext.currentSlotNodeIndex) == m_current);

        m_current = slot;
        shadowContext.currentSlot = nullptr;
        return;
#else
        m_current = nullptr;
        return;
#endif
    }
    m_current = parent;
}

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
void ComposedTreeIterator::traverseNextSiblingSlot()
{
    ASSERT(m_current->parentNode()->shadowRoot());
    ASSERT(!m_shadowStack.isEmpty());
    ASSERT(m_shadowStack.last().host == m_current->parentNode());

    auto& shadowContext = m_shadowStack.last();

    if (!shadowContext.currentSlot) {
        m_current = nullptr;
        return;
    }
    auto* slotNodes = shadowContext.currentSlot->assignedNodes();
    ASSERT(slotNodes->at(shadowContext.currentSlotNodeIndex) == m_current);

    bool nextNodeInSameSlot = ++shadowContext.currentSlotNodeIndex < slotNodes->size();
    m_current = nextNodeInSameSlot ? slotNodes->at(shadowContext.currentSlotNodeIndex) : nullptr;
}

void ComposedTreeIterator::traversePreviousSiblingSlot()
{
    ASSERT(m_current->parentNode()->shadowRoot());
    ASSERT(!m_shadowStack.isEmpty());
    ASSERT(m_shadowStack.last().host == m_current->parentNode());

    auto& shadowContext = m_shadowStack.last();

    if (!shadowContext.currentSlot) {
        m_current = nullptr;
        return;
    }
    auto* slotNodes = shadowContext.currentSlot->assignedNodes();
    ASSERT(slotNodes->at(shadowContext.currentSlotNodeIndex) == m_current);

    bool previousNodeInSameSlot = shadowContext.currentSlotNodeIndex > 0;
    m_current = previousNodeInSameSlot ? slotNodes->at(--shadowContext.currentSlotNodeIndex) : nullptr;
}
#endif

}
