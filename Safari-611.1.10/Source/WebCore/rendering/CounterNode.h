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

#pragma once

#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

// This implements a counter tree that is used for finding parents in counters() lookup,
// and for propagating count changes when nodes are added or removed.

// Parents represent unique counters and their scope, which are created either explicitly
// by "counter-reset" style rules or implicitly by referring to a counter that is not in scope.
// Such nodes are tagged as "reset" nodes, although they are not all due to "counter-reset".

// Not that render tree children are often counter tree siblings due to counter scoping rules.

namespace WebCore {

class RenderCounter;
class RenderElement;

class CounterNode : public RefCounted<CounterNode> {
public:
    static Ref<CounterNode> create(RenderElement&, bool isReset, int value);
    ~CounterNode();
    bool actsAsReset() const { return m_hasResetType || !m_parent; }
    bool hasResetType() const { return m_hasResetType; }
    int value() const { return m_value; }
    int countInParent() const { return m_countInParent; }
    RenderElement& owner() const { return m_owner; }
    void addRenderer(RenderCounter&);
    void removeRenderer(RenderCounter&);

    // Invalidates the text in the renderers of this counter, if any.
    void resetRenderers();

    CounterNode* parent() const { return m_parent; }
    CounterNode* previousSibling() const { return m_previousSibling; }
    CounterNode* nextSibling() const { return m_nextSibling; }
    CounterNode* firstChild() const { return m_firstChild; }
    CounterNode* lastChild() const { return m_lastChild; }
    CounterNode* lastDescendant() const;
    CounterNode* previousInPreOrder() const;
    CounterNode* nextInPreOrder(const CounterNode* stayWithin = nullptr) const;
    CounterNode* nextInPreOrderAfterChildren(const CounterNode* stayWithin = nullptr) const;

    void insertAfter(CounterNode& newChild, CounterNode* beforeChild, const AtomString& identifier);
    // identifier must match the identifier of this counter.
    void removeChild(CounterNode&);

private:
    CounterNode(RenderElement&, bool isReset, int value);
    int computeCountInParent() const;
    // Invalidates the text in the renderer of this counter, if any,
    // and in the renderers of all descendants of this counter, if any.
    void resetThisAndDescendantsRenderers();
    void recount();

    bool m_hasResetType;
    int m_value;
    int m_countInParent;
    RenderElement& m_owner;
    RenderCounter* m_rootRenderer { nullptr };

    CounterNode* m_parent { nullptr };
    CounterNode* m_previousSibling { nullptr };
    CounterNode* m_nextSibling { nullptr };
    CounterNode* m_firstChild { nullptr };
    CounterNode* m_lastChild { nullptr };
};

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showCounterTree(const WebCore::CounterNode*);
#endif
