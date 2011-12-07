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
#include "MutationRecord.h"
#include "Node.h"
#include "StaticNodeList.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

namespace {

// ChildListMutationAccumulator expects that all removals from a parent take place in order
// and precede any additions. If this is violated (i.e. because of code changes elsewhere
// in WebCore) it will likely result in both (a) ASSERTions failing, and (b) mutation records
// being enqueued for delivery before the outer-most scope closes.
class ChildListMutationAccumulator {
    WTF_MAKE_NONCOPYABLE(ChildListMutationAccumulator);
public:
    ChildListMutationAccumulator(PassRefPtr<Node>, PassOwnPtr<MutationObserverInterestGroup> observers);
    ~ChildListMutationAccumulator();

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

class MutationAccumulationRouter {
    WTF_MAKE_NONCOPYABLE(MutationAccumulationRouter);
public:
    ~MutationAccumulationRouter();

    static MutationAccumulationRouter* instance();

    void incrementScopingLevel(Node*);
    void decrementScopingLevel(Node*);

    void childAdded(Node* target, Node* child);
    void willRemoveChild(Node* target, Node* child);

private:
    MutationAccumulationRouter();
    static void initialize();

    typedef HashMap<Node*, unsigned> ScopingLevelMap;
    ScopingLevelMap m_scopingLevels;
    HashMap<Node*, OwnPtr<ChildListMutationAccumulator> > m_accumulations;

    static MutationAccumulationRouter* s_instance;
};

ChildListMutationAccumulator::ChildListMutationAccumulator(PassRefPtr<Node> target, PassOwnPtr<MutationObserverInterestGroup> observers)
    : m_target(target)
    , m_observers(observers)
{
    clear();
}

ChildListMutationAccumulator::~ChildListMutationAccumulator()
{
    ASSERT(isEmpty());
}

inline bool ChildListMutationAccumulator::isAddedNodeInOrder(Node* child)
{
    return isEmpty() || (m_lastAdded == child->previousSibling() && m_nextSibling == child->nextSibling());
}

void ChildListMutationAccumulator::childAdded(PassRefPtr<Node> prpChild)
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

inline bool ChildListMutationAccumulator::isRemovedNodeInOrder(Node* child)
{
    return isEmpty() || m_nextSibling.get() == child;
}

void ChildListMutationAccumulator::willRemoveChild(PassRefPtr<Node> prpChild)
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

void ChildListMutationAccumulator::enqueueMutationRecord()
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

void ChildListMutationAccumulator::clear()
{
    if (!m_removedNodes.isEmpty())
        m_removedNodes.clear();
    if (!m_addedNodes.isEmpty())
        m_addedNodes.clear();
    m_previousSibling = 0;
    m_nextSibling = 0;
    m_lastAdded = 0;
}

bool ChildListMutationAccumulator::isEmpty()
{
    return m_removedNodes.isEmpty() && m_addedNodes.isEmpty();
}

MutationAccumulationRouter* MutationAccumulationRouter::s_instance = 0;

MutationAccumulationRouter::MutationAccumulationRouter()
{
}

MutationAccumulationRouter::~MutationAccumulationRouter()
{
    ASSERT(m_scopingLevels.isEmpty());
    ASSERT(m_accumulations.isEmpty());
}

void MutationAccumulationRouter::initialize()
{
    ASSERT(!s_instance);
    s_instance = adoptPtr(new MutationAccumulationRouter).leakPtr();
}


MutationAccumulationRouter* MutationAccumulationRouter::instance()
{
    if (!s_instance)
        initialize();

    return s_instance;
}

void MutationAccumulationRouter::childAdded(Node* target, Node* child)
{
    HashMap<Node*, OwnPtr<ChildListMutationAccumulator> >::iterator iter = m_accumulations.find(target);
    ASSERT(iter != m_accumulations.end());

    if (iter->second)
        iter->second->childAdded(child);
}

void MutationAccumulationRouter::willRemoveChild(Node* target, Node* child)
{
    HashMap<Node*, OwnPtr<ChildListMutationAccumulator> >::iterator iter = m_accumulations.find(target);
    ASSERT(iter != m_accumulations.end());

    if (iter->second)
        iter->second->willRemoveChild(child);
}

void MutationAccumulationRouter::incrementScopingLevel(Node* target)
{
    pair<ScopingLevelMap::iterator, bool> result = m_scopingLevels.add(target, 1);
    if (!result.second) {
        ++(result.first->second);
        return;
    }

    OwnPtr<MutationObserverInterestGroup> observers = MutationObserverInterestGroup::createForChildListMutation(target);
    if (observers->isEmpty())
        m_accumulations.set(target, nullptr);
    else
        m_accumulations.set(target, adoptPtr(new ChildListMutationAccumulator(target, observers.release())));
}

void MutationAccumulationRouter::decrementScopingLevel(Node* target)
{
    ScopingLevelMap::iterator iter = m_scopingLevels.find(target);
    ASSERT(iter != m_scopingLevels.end());

    --(iter->second);
    if (iter->second > 0)
        return;

    m_scopingLevels.remove(iter);

    OwnPtr<ChildListMutationAccumulator> record = m_accumulations.take(target);
    if (record)
        record->enqueueMutationRecord();
}

} // namespace

ChildListMutationScope::ChildListMutationScope(Node* target)
    : m_target(target)
{
    MutationAccumulationRouter::instance()->incrementScopingLevel(m_target);
}

ChildListMutationScope::~ChildListMutationScope()
{
    MutationAccumulationRouter::instance()->decrementScopingLevel(m_target);
}

void ChildListMutationScope::childAdded(Node* child)
{
    MutationAccumulationRouter::instance()->childAdded(m_target, child);
}

void ChildListMutationScope::willRemoveChild(Node* child)
{
    MutationAccumulationRouter::instance()->willRemoveChild(m_target, child);
}

} // namespace WebCore

#endif // ENABLE(MUTATION_OBSERVERS)
