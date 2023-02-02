/*
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "CounterNode.h"

#include "RenderCounter.h"
#include "RenderElement.h"
#include <stdio.h>

namespace WebCore {

CounterNode::CounterNode(RenderElement& owner, bool hasResetType, int value)
    : m_hasResetType(hasResetType)
    , m_value(value)
    , m_owner(owner)
{
}

CounterNode::~CounterNode()
{
    // Ideally this would be an assert and this would never be reached. In reality this happens a lot
    // so we need to handle these cases. The node is still connected to the tree so we need to detach it.
    if (m_parent || m_previousSibling || m_nextSibling || m_firstChild || m_lastChild) {
        CheckedPtr<CounterNode> oldParent;
        CheckedPtr<CounterNode> oldPreviousSibling;
        // Instead of calling removeChild() we do this safely as the tree is likely broken if we get here.
        if (m_parent) {
            if (m_parent->m_firstChild == this)
                m_parent->m_firstChild = m_nextSibling;
            if (m_parent->m_lastChild == this)
                m_parent->m_lastChild = m_previousSibling;
            oldParent = m_parent;
            m_parent = nullptr;
        }
        if (m_previousSibling) {
            if (m_previousSibling->m_nextSibling == this)
                m_previousSibling->m_nextSibling = m_nextSibling;
            oldPreviousSibling = m_previousSibling;
            m_previousSibling = nullptr;
        }
        if (m_nextSibling) {
            if (m_nextSibling->m_previousSibling == this)
                m_nextSibling->m_previousSibling = oldPreviousSibling;
            m_nextSibling = nullptr;
        }
        if (m_firstChild) {
            // The node's children are reparented to the old parent.
            for (CheckedPtr child = m_firstChild; child; ) {
                CheckedPtr nextChild = child->m_nextSibling;
                CheckedPtr<CounterNode> nextSibling;
                child->m_parent = oldParent;
                if (oldPreviousSibling) {
                    nextSibling = oldPreviousSibling->m_nextSibling;
                    child->m_previousSibling = oldPreviousSibling;
                    oldPreviousSibling->m_nextSibling = child;
                    child->m_nextSibling = nextSibling;
                    nextSibling->m_previousSibling = child;
                    oldPreviousSibling = child;
                }
                child = nextChild;
            }
        }
    }
    resetRenderers();
}

Ref<CounterNode> CounterNode::create(RenderElement& owner, bool hasResetType, int value)
{
    return adoptRef(*new CounterNode(owner, hasResetType, value));
}

CounterNode* CounterNode::nextInPreOrderAfterChildren(const CounterNode* stayWithin) const
{
    if (this == stayWithin)
        return nullptr;

    CheckedPtr current = const_cast<CounterNode*>(this);
    CheckedPtr<CounterNode> next;
    while (!(next = current->m_nextSibling)) {
        current = current->m_parent.get();
        if (!current || current == stayWithin)
            return nullptr;
    }
    return next.get();
}

CounterNode* CounterNode::nextInPreOrder(const CounterNode* stayWithin) const
{
    if (auto* next = const_cast<CounterNode*>(m_firstChild.get()))
        return next;

    return nextInPreOrderAfterChildren(stayWithin);
}

CounterNode* CounterNode::lastDescendant() const
{
    CheckedPtr last = m_lastChild;
    if (!last)
        return nullptr;

    while (CheckedPtr lastChild = last->m_lastChild)
        last = lastChild;

    return const_cast<CounterNode*>(last.get());
}

CounterNode* CounterNode::previousInPreOrder() const
{
    CheckedPtr previous = m_previousSibling;
    if (!previous)
        return const_cast<CounterNode*>(m_parent.get());

    while (CheckedPtr lastChild = previous->m_lastChild)
        previous = lastChild;

    return const_cast<CounterNode*>(previous.get());
}

int CounterNode::computeCountInParent() const
{
    int increment = actsAsReset() ? 0 : m_value;
    if (m_previousSibling)
        return m_previousSibling->m_countInParent + increment;
    ASSERT(m_parent->m_firstChild == this);
    return m_parent->m_value + increment;
}

void CounterNode::addRenderer(RenderCounter& renderer)
{
    ASSERT(!renderer.m_counterNode);
    ASSERT(!renderer.m_nextForSameCounter);
    renderer.m_nextForSameCounter = m_rootRenderer;
    m_rootRenderer = &renderer;
    renderer.m_counterNode = this;
}

void CounterNode::removeRenderer(RenderCounter& renderer)
{
    ASSERT(renderer.m_counterNode && renderer.m_counterNode == this);
    RenderCounter* previous = nullptr;
    for (auto* current = m_rootRenderer; current; previous = current, current = current->m_nextForSameCounter) {
        if (current != &renderer)
            continue;

        if (previous)
            previous->m_nextForSameCounter = renderer.m_nextForSameCounter;
        else
            m_rootRenderer = renderer.m_nextForSameCounter;
        renderer.m_nextForSameCounter = nullptr;
        renderer.m_counterNode = nullptr;
        return;
    }
    ASSERT_NOT_REACHED();
}

void CounterNode::resetRenderers()
{
    if (!m_rootRenderer)
        return;
    bool skipLayoutAndPerfWidthsRecalc = m_rootRenderer->renderTreeBeingDestroyed();
    auto* current = m_rootRenderer;
    while (current) {
        if (!skipLayoutAndPerfWidthsRecalc)
            current->setNeedsLayoutAndPrefWidthsRecalc();
        auto* next = current->m_nextForSameCounter;
        current->m_nextForSameCounter = nullptr;
        current->m_counterNode = nullptr;
        current = next;
    }
    m_rootRenderer = nullptr;
}

void CounterNode::resetThisAndDescendantsRenderers()
{
    CheckedPtr node = this;
    do {
        node->resetRenderers();
        node = node->nextInPreOrder(this);
    } while (node);
}

void CounterNode::recount()
{
    for (CheckedPtr node = this; node; node = node->m_nextSibling) {
        int oldCount = node->m_countInParent;
        int newCount = node->computeCountInParent();
        if (oldCount == newCount)
            break;
        node->m_countInParent = newCount;
        node->resetThisAndDescendantsRenderers();
    }
}

void CounterNode::insertAfter(CounterNode& newChild, CounterNode* beforeChild, const AtomString& identifier)
{
    ASSERT(!newChild.m_parent);
    ASSERT(!newChild.m_previousSibling);
    ASSERT(!newChild.m_nextSibling);
    // If the beforeChild is not our child we can not complete the request. This hardens against bugs in RenderCounter.
    // When renderers are reparented it may request that we insert counter nodes improperly.
    if (beforeChild && beforeChild->m_parent != this)
        return;

    if (newChild.m_hasResetType) {
        while (m_lastChild != beforeChild)
            RenderCounter::destroyCounterNode(m_lastChild->owner(), identifier);
    }

    CheckedPtr<CounterNode> next;

    if (beforeChild) {
        next = beforeChild->m_nextSibling;
        beforeChild->m_nextSibling = &newChild;
    } else {
        next = m_firstChild;
        m_firstChild = &newChild;
    }

    newChild.m_parent = this;
    newChild.m_previousSibling = beforeChild;

    if (next) {
        ASSERT(next->m_previousSibling == beforeChild);
        next->m_previousSibling = &newChild;
        newChild.m_nextSibling = next;
    } else {
        ASSERT(m_lastChild == beforeChild);
        m_lastChild = &newChild;
    }

    if (!newChild.m_firstChild || newChild.m_hasResetType) {
        newChild.m_countInParent = newChild.computeCountInParent();
        newChild.resetThisAndDescendantsRenderers();
        if (next)
            next->recount();
        return;
    }

    // The code below handles the case when a formerly root increment counter is loosing its root position
    // and therefore its children become next siblings.
    CheckedPtr<CounterNode> last = newChild.m_lastChild;
    CheckedPtr<CounterNode> first = newChild.m_firstChild;

    if (first) {
        ASSERT(last);
        newChild.m_nextSibling = first;
        if (m_lastChild == &newChild)
            m_lastChild = last;

        first->m_previousSibling = &newChild;
    
        // The case when the original next sibling of the inserted node becomes a child of
        // one of the former children of the inserted node is not handled as it is believed
        // to be impossible since:
        // 1. if the increment counter node lost it's root position as a result of another
        //    counter node being created, it will be inserted as the last child so next is null.
        // 2. if the increment counter node lost it's root position as a result of a renderer being
        //    inserted into the document's render tree, all its former children counters are attached
        //    to children of the inserted renderer and hence cannot be in scope for counter nodes
        //    attached to renderers that were already in the document's render tree.
        last->m_nextSibling = next;
        if (next) {
            ASSERT(next->m_previousSibling == &newChild);
            next->m_previousSibling = last;
        } else
            m_lastChild = last;
        for (next = first; ; next = next->m_nextSibling) {
            next->m_parent = this;
            if (last == next)
                break;
        }
    }
    newChild.m_firstChild = nullptr;
    newChild.m_lastChild = nullptr;
    newChild.m_countInParent = newChild.computeCountInParent();
    newChild.resetRenderers();
    first->recount();
}

void CounterNode::removeChild(CounterNode& oldChild)
{
    ASSERT(!oldChild.m_firstChild);
    ASSERT(!oldChild.m_lastChild);

    CheckedPtr<CounterNode> next = oldChild.m_nextSibling;
    CheckedPtr<CounterNode> previous = oldChild.m_previousSibling;

    oldChild.m_nextSibling = nullptr;
    oldChild.m_previousSibling = nullptr;
    oldChild.m_parent = nullptr;

    if (previous) 
        previous->m_nextSibling = next;
    else {
        ASSERT(m_firstChild == &oldChild);
        m_firstChild = next;
    }

    if (next)
        next->m_previousSibling = previous;
    else {
        ASSERT(m_lastChild == &oldChild);
        m_lastChild = previous;
    }

    if (next)
        next->recount();
}

#if ENABLE(TREE_DEBUGGING)

static void showTreeAndMark(const CounterNode* node)
{
    const CounterNode* root = node;
    while (root->parent())
        root = root->parent();

    for (const CounterNode* current = root; current; current = current->nextInPreOrder()) {
        fprintf(stderr, "%c", (current == node) ? '*' : ' ');
        for (const CounterNode* parent = current; parent && parent != root; parent = parent->parent())
            fprintf(stderr, "    ");
        fprintf(stderr, "%p %s: %d %d P:%p PS:%p NS:%p R:%p\n",
            current, current->actsAsReset() ? "reset____" : "increment", current->value(),
            current->countInParent(), current->parent(), current->previousSibling(),
            current->nextSibling(), &current->owner());
    }
    fflush(stderr);
}

#endif

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showCounterTree(const WebCore::CounterNode* counter)
{
    if (counter)
        showTreeAndMark(counter);
}

#endif
