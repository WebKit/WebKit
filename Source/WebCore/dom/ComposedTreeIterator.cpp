/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#include "ElementInlines.h"
#include "ElementRareData.h"
#include "HTMLSlotElement.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ComposedTreeIterator::Context::Context()
{
}

ComposedTreeIterator::Context::Context(ContainerNode& root, FirstChildTag)
    : iterator(root, ElementAndTextDescendantIterator::FirstChild)
{
}

ComposedTreeIterator::Context::Context(ContainerNode& root, Node& node)
    : iterator(root, &node)
{
}

ComposedTreeIterator::Context::Context(ContainerNode& root, Node& node, SlottedTag)
    : iterator(root, &node)
    , end(iterator)
{
    end.traverseNextSkippingChildren();
}

ComposedTreeIterator::ComposedTreeIterator(ContainerNode& root, FirstChildTag)
    : m_rootIsInShadowTree(root.isInShadowTree())
{
    ASSERT(!is<ShadowRoot>(root));

    if (auto* slot = dynamicDowncast<HTMLSlotElement>(root)) {
        if (auto* assignedNodes = slot->assignedNodes()) {
            initializeContextStack(root, *assignedNodes->at(0));
            return;
        }
    }
    if (auto* shadowRoot = root.shadowRoot()) {
        ElementAndTextDescendantIterator firstChild(*shadowRoot, ElementAndTextDescendantIterator::FirstChild);
        initializeContextStack(root, firstChild ? *firstChild : root);
        return;
    }

    m_contextStack.append(Context(root, FirstChild));
}

ComposedTreeIterator::ComposedTreeIterator(ContainerNode& root, Node& current)
    : m_rootIsInShadowTree(root.isInShadowTree())
{
    ASSERT(!is<ShadowRoot>(root));
    ASSERT(!is<ShadowRoot>(current));

    bool mayNeedShadowStack = root.shadowRoot() || (&current != &root && current.parentNode() != &root);
    if (mayNeedShadowStack)
        initializeContextStack(root, current);
    else
        m_contextStack.append(Context(root, current));
}

void ComposedTreeIterator::initializeContextStack(ContainerNode& root, Node& current)
{
    // This code sets up the iterator for arbitrary node/root pair. It is not needed in common cases
    // or completes fast because node and root are close (like in composedTreeChildren(*parent).at(node) case).
    auto* node = &current;
    auto* contextCurrent = node;
    size_t currentSlotNodeIndex = notFound;
    while (node != &root) {
        auto* parent = node->parentNode();
        if (!parent) {
            *this = { };
            return;
        }
        if (auto* shadowRoot = dynamicDowncast<ShadowRoot>(*parent)) {
            m_contextStack.append(Context(*shadowRoot, *contextCurrent));
            m_contextStack.last().slotNodeIndex = currentSlotNodeIndex;

            node = shadowRoot->host();
            contextCurrent = node;
            currentSlotNodeIndex = notFound;
            continue;
        }
        if (auto* shadowRoot = parent->shadowRoot()) {
            m_contextStack.append(Context(*parent, *contextCurrent, Context::Slotted));
            m_contextStack.last().slotNodeIndex = currentSlotNodeIndex;

            auto* assignedSlot = shadowRoot->findAssignedSlot(*node);
            if (assignedSlot) {
                currentSlotNodeIndex = assignedSlot->assignedNodes()->find(node);
                ASSERT(currentSlotNodeIndex != notFound);
                node = assignedSlot;
                contextCurrent = assignedSlot;
                continue;
            }
            // The node is not part of the composed tree.
            *this = { };
            return;
        }
        node = parent;
    }
    m_contextStack.append(Context(root, *contextCurrent));
    m_contextStack.last().slotNodeIndex = currentSlotNodeIndex;

    m_contextStack.reverse();
}

void ComposedTreeIterator::dropAssertions()
{
    for (auto& context : m_contextStack)
        context.iterator.dropAssertions();
    m_didDropAssertions = true;
}

void ComposedTreeIterator::traverseShadowRoot(ShadowRoot& shadowRoot)
{
    Context shadowContext(shadowRoot, FirstChild);
    if (!shadowContext.iterator) {
        // Empty shadow root.
        traverseNextSkippingChildren();
        return;
    }

    if (m_didDropAssertions)
        shadowContext.iterator.dropAssertions();

    m_contextStack.append(WTFMove(shadowContext));
}

void ComposedTreeIterator::traverseNextInShadowTree()
{
    ASSERT(m_contextStack.size() > 1 || m_rootIsInShadowTree);

    if (auto* slot = dynamicDowncast<HTMLSlotElement>(current())) {
        if (auto* assignedNodes = slot->assignedNodes()) {
            context().slotNodeIndex = 0;
            auto* assignedNode = assignedNodes->at(0).get();
            ASSERT(assignedNode);
            ASSERT(assignedNode->parentElement());
            m_contextStack.append(Context(*assignedNode->parentElement(), *assignedNode, Context::Slotted));
            return;
        }
    }

    context().iterator.traverseNext();

    if (context().iterator == context().end)
        traverseNextLeavingContext();
}

void ComposedTreeIterator::traverseNextLeavingContext()
{
    while (context().iterator == context().end && m_contextStack.size() > 1) {
        m_contextStack.removeLast();
        if (is<HTMLSlotElement>(current()) && advanceInSlot(1))
            return;
        if (context().iterator == context().end)
            return;
        context().iterator.traverseNextSkippingChildren();
    }
}

bool ComposedTreeIterator::advanceInSlot(int direction)
{
    ASSERT(context().slotNodeIndex != notFound);

    auto& assignedNodes = *downcast<HTMLSlotElement>(current()).assignedNodes();
    // It is fine to underflow this.
    context().slotNodeIndex += direction;
    if (context().slotNodeIndex >= assignedNodes.size())
        return false;

    auto* slotNode = assignedNodes.at(context().slotNodeIndex).get();
    ASSERT(slotNode);
    ASSERT(slotNode->parentElement());
    m_contextStack.append(Context(*slotNode->parentElement(), *slotNode, Context::Slotted));
    return true;
}

void ComposedTreeIterator::traverseSiblingInSlot(int direction)
{
    ASSERT(m_contextStack.size() > 1);
    ASSERT(current().parentNode()->shadowRoot());

    m_contextStack.removeLast();

    if (!advanceInSlot(direction))
        *this = { };
}

String composedTreeAsText(ContainerNode& root, ComposedTreeAsTextMode mode)
{
    TextStream stream;
    auto descendants = composedTreeDescendants(root);
    for (auto it = descendants.begin(), end = descendants.end(); it != end; ++it) {
        writeIndent(stream, it.depth());

        if (is<Text>(*it)) {
            stream << "#text";
            if (mode == ComposedTreeAsTextMode::WithPointers)
                stream << " " << &*it;
            stream << "\n";
            continue;
        }
        auto& element = downcast<Element>(*it);
        stream << element.localName();
        if (element.shadowRoot())
            stream << " (shadow root)";
        if (mode == ComposedTreeAsTextMode::WithPointers)
            stream << " " << &*it;
        stream << "\n";
    }
    return stream.release();
}

}
