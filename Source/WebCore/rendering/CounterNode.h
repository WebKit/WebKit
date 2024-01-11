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

#include <wtf/CheckedPtr.h>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

// This implements a counter tree that is used for finding parents in counters() lookup,
// and for propagating count changes when nodes are added or removed.

// Parents represent unique counters and their scope, which are created either explicitly
// by "counter-reset" style rules or implicitly by referring to a counter that is not in scope.
// Such nodes are tagged as "reset" nodes, although they are not all due to "counter-reset".

// Not that render tree children are often counter tree siblings due to counter scoping rules.

namespace WebCore {

class RenderCounter;
class RenderElement;

class CounterNode : public RefCounted<CounterNode>, public CanMakeSingleThreadWeakPtr<CounterNode> {
public:
    enum class Type : uint8_t { Increment, Reset, Set };

    static Ref<CounterNode> create(RenderElement&, OptionSet<Type>, int value);
    ~CounterNode();
    bool actsAsReset() const { return hasResetType() || !m_parent; }
    bool hasResetType() const { return m_type.contains(Type::Reset); }
    bool hasSetType() const { return m_type.contains(Type::Set); }
    int value() const { return m_value; }
    int countInParent() const { return m_countInParent; }
    RenderElement& owner() const { return m_owner; }
    void addRenderer(RenderCounter&);
    void removeRenderer(RenderCounter&);

    // Invalidates the text in the renderers of this counter, if any.
    void resetRenderers();

    CounterNode* parent() const { return const_cast<CounterNode*>(m_parent.get()); }
    CounterNode* previousSibling() const { return const_cast<CounterNode*>(m_previousSibling.get()); }
    CounterNode* nextSibling() const { return const_cast<CounterNode*>(m_nextSibling.get()); }
    CounterNode* firstChild() const { return const_cast<CounterNode*>(m_firstChild.get()); }
    CounterNode* lastChild() const { return const_cast<CounterNode*>(m_lastChild.get()); }
    CounterNode* lastDescendant() const;
    CounterNode* previousInPreOrder() const;
    CounterNode* nextInPreOrder(const CounterNode* stayWithin = nullptr) const;
    CounterNode* nextInPreOrderAfterChildren(const CounterNode* stayWithin = nullptr) const;

    void insertAfter(CounterNode& newChild, CounterNode* beforeChild, const AtomString& identifier);
    // identifier must match the identifier of this counter.
    void removeChild(CounterNode&);

private:
    CounterNode(RenderElement&, OptionSet<Type>, int value);
    int computeCountInParent() const;
    // Invalidates the text in the renderer of this counter, if any,
    // and in the renderers of all descendants of this counter, if any.
    void resetThisAndDescendantsRenderers();
    void recount();

    OptionSet<Type> m_type { };
    int m_value;
    int m_countInParent { 0 };
    RenderElement& m_owner;
    RenderCounter* m_rootRenderer { nullptr };

    SingleThreadWeakPtr<CounterNode> m_parent;
    SingleThreadWeakPtr<CounterNode> m_previousSibling;
    SingleThreadWeakPtr<CounterNode> m_nextSibling;
    SingleThreadWeakPtr<CounterNode> m_firstChild;
    SingleThreadWeakPtr<CounterNode> m_lastChild;
};

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showCounterTree(const WebCore::CounterNode*);
#endif
