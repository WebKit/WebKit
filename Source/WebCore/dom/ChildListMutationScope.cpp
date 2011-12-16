/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#if ENABLE(MUTATION_OBSERVERS)

#include "ChildListMutationScope.h"

#include "DocumentFragment.h"
#include "Element.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"
#include "Node.h"
#include "StaticNodeList.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

// MutationAccumulator expects that all removals from a parent take place in order
// and precede any additions. If this is violated (i.e. because of code changes elsewhere
// in WebCore) it will likely result in both (a) ASSERTions failing, and (b) mutation records
// being enqueued for delivery before the outer-most scope closes.
class ChildListMutationScope::MutationAccumulator {
    WTF_MAKE_NONCOPYABLE(MutationAccumulator);
public:
    MutationAccumulator(PassRefPtr<Node>, PassOwnPtr<MutationObserverInterestGroup> observers);
    ~MutationAccumulator();

    void childAdded(PassRefPtr<Node>);
    void willRemoveChild(PassRefPtr<Node>);
    void enqueueMutationRecord();

private:
    void clear();
    bool isEmpty();
    bool isAddedNodeInOrder(Node*);
    bool isRemovedNodeInOrder(Node*);
    RefPtr<Node> m_target;

    Vector<RefPtr<Node> > m_removedNodes;
    Vector<RefPtr<Node> > m_addedNodes;
    RefPtr<Node> m_previousSibling;
    RefPtr<Node> m_nextSibling;
    RefPtr<Node> m_lastAdded;

    OwnPtr<MutationObserverInterestGroup> m_observers;
};

ChildListMutationScope::MutationAccumulator::MutationAccumulator(PassRefPtr<Node> target, PassOwnPtr<MutationObserverInterestGroup> observers)
    : m_target(target)
    , m_observers(observers)
{
    clear();
}

ChildListMutationScope::MutationAccumulator::~MutationAccumulator()
{
    ASSERT(isEmpty());
}

inline bool ChildListMutationScope::MutationAccumulator::isAddedNodeInOrder(Node* child)
{
    return isEmpty() || (m_lastAdded == child->previousSibling() && m_nextSibling == child->nextSibling());
}

void ChildListMutationScope::MutationAccumulator::childAdded(PassRefPtr<Node> prpChild)
{
    RefPtr<Node> child = prpChild;

    ASSERT(isAddedNodeInOrder(child.get()));
    if (!isAddedNodeInOrder(child.get()))
        enqueueMutationRecord();

    if (isEmpty()) {
        m_previousSibling = child->previousSibling();
        m_nextSibling = child->nextSibling();
    }

    m_addedNodes.append(child);
    m_lastAdded = child;
}

inline bool ChildListMutationScope::MutationAccumulator::isRemovedNodeInOrder(Node* child)
{
    return isEmpty() || m_nextSibling.get() == child;
}

void ChildListMutationScope::MutationAccumulator::willRemoveChild(PassRefPtr<Node> prpChild)
{
    RefPtr<Node> child = prpChild;

    ASSERT(m_addedNodes.isEmpty() && isRemovedNodeInOrder(child.get()));

    if (!m_addedNodes.isEmpty() || !isRemovedNodeInOrder(child.get()))
        enqueueMutationRecord();

    if (isEmpty()) {
        m_previousSibling = child->previousSibling();
        m_nextSibling = child->nextSibling();
        m_lastAdded = child->previousSibling();
    } else
        m_nextSibling = child->nextSibling();

    m_removedNodes.append(child);
}

void ChildListMutationScope::MutationAccumulator::enqueueMutationRecord()
{
    ASSERT(!isEmpty());
    if (isEmpty()) {
        clear();
        return;
    }

    m_observers->enqueueMutationRecord(MutationRecord::createChildList(
            m_target, StaticNodeList::adopt(m_addedNodes), StaticNodeList::adopt(m_removedNodes), m_previousSibling.release(), m_nextSibling.release()));
    clear();
}

void ChildListMutationScope::MutationAccumulator::clear()
{
    if (!m_removedNodes.isEmpty())
        m_removedNodes.clear();
    if (!m_addedNodes.isEmpty())
        m_addedNodes.clear();
    m_previousSibling = 0;
    m_nextSibling = 0;
    m_lastAdded = 0;
}

bool ChildListMutationScope::MutationAccumulator::isEmpty()
{
    return m_removedNodes.isEmpty() && m_addedNodes.isEmpty();
}

ChildListMutationScope::MutationAccumulationRouter* ChildListMutationScope::MutationAccumulationRouter::s_instance = 0;

ChildListMutationScope::MutationAccumulationRouter::MutationAccumulationRouter()
{
}

ChildListMutationScope::MutationAccumulationRouter::~MutationAccumulationRouter()
{
    ASSERT(m_scopingLevels.isEmpty());
    ASSERT(m_accumulations.isEmpty());
}

void ChildListMutationScope::MutationAccumulationRouter::initialize()
{
    ASSERT(!s_instance);
    s_instance = adoptPtr(new ChildListMutationScope::MutationAccumulationRouter).leakPtr();
}


ChildListMutationScope::MutationAccumulationRouter* ChildListMutationScope::MutationAccumulationRouter::instance()
{
    if (!s_instance)
        initialize();

    return s_instance;
}

void ChildListMutationScope::MutationAccumulationRouter::childAdded(Node* target, Node* child)
{
    HashMap<Node*, OwnPtr<ChildListMutationScope::MutationAccumulator> >::iterator iter = m_accumulations.find(target);
    ASSERT(iter != m_accumulations.end());
    if (iter == m_accumulations.end() || !iter->second)
        return;

    iter->second->childAdded(child);
}

void ChildListMutationScope::MutationAccumulationRouter::willRemoveChild(Node* target, Node* child)
{
    HashMap<Node*, OwnPtr<ChildListMutationScope::MutationAccumulator> >::iterator iter = m_accumulations.find(target);
    ASSERT(iter != m_accumulations.end());
    if (iter == m_accumulations.end() || !iter->second)
        return;

    iter->second->willRemoveChild(child);
}

void ChildListMutationScope::MutationAccumulationRouter::incrementScopingLevel(Node* target)
{
    pair<ScopingLevelMap::iterator, bool> result = m_scopingLevels.add(target, 1);
    if (!result.second) {
        ++(result.first->second);
        return;
    }

    if (OwnPtr<MutationObserverInterestGroup> observers = MutationObserverInterestGroup::createForChildListMutation(target))
        m_accumulations.set(target, adoptPtr(new ChildListMutationScope::MutationAccumulator(target, observers.release())));
#ifndef NDEBUG
    else
        m_accumulations.set(target, nullptr);
#endif
}

void ChildListMutationScope::MutationAccumulationRouter::decrementScopingLevel(Node* target)
{
    ScopingLevelMap::iterator iter = m_scopingLevels.find(target);
    ASSERT(iter != m_scopingLevels.end());

    --(iter->second);
    if (iter->second > 0)
        return;

    m_scopingLevels.remove(iter);

    OwnPtr<MutationAccumulator> record(m_accumulations.take(target));
    if (record)
        record->enqueueMutationRecord();
}

} // namespace WebCore

#endif // ENABLE(MUTATION_OBSERVERS)
