/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "V8GCController.h"

#include "Attr.h"
#include "HTMLImageElement.h"
#include "MemoryUsageSupport.h"
#include "TraceEvent.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8MessagePort.h"
#include "V8MutationObserver.h"
#include "V8Node.h"
#include "V8RecursionScope.h"
#include "WrapperTypeInfo.h"
#include <algorithm>

namespace WebCore {

class ImplicitConnection {
public:
    ImplicitConnection(void* root, v8::Persistent<v8::Value> wrapper)
        : m_root(root)
        , m_wrapper(wrapper)
    {
    }

    void* root() const { return m_root; }
    v8::Persistent<v8::Value> wrapper() const { return m_wrapper; }

private:
    void* m_root;
    v8::Persistent<v8::Value> m_wrapper;
};

bool operator<(const ImplicitConnection& left, const ImplicitConnection& right)
{
    return left.root() < right.root();
}

class WrapperGrouper {
public:
    WrapperGrouper()
    {
        m_liveObjects.append(V8PerIsolateData::current()->ensureLiveRoot());
    }

    void addToGroup(void* root, v8::Persistent<v8::Value> wrapper)
    {
        m_connections.append(ImplicitConnection(root, wrapper));
    }

    void keepAlive(v8::Persistent<v8::Value> wrapper)
    {
        m_liveObjects.append(wrapper);
    }

    void apply()
    {
        if (m_liveObjects.size() > 1)
            v8::V8::AddObjectGroup(m_liveObjects.data(), m_liveObjects.size());

        std::sort(m_connections.begin(), m_connections.end());
        Vector<v8::Persistent<v8::Value> > group;
        size_t i = 0;
        while (i < m_connections.size()) {
            void* root = m_connections[i].root();

            do {
                group.append(m_connections[i++].wrapper());
            } while (i < m_connections.size() && root == m_connections[i].root());

            if (group.size() > 1)
                v8::V8::AddObjectGroup(group.data(), group.size(), 0);

            group.shrink(0);
        }
    }

private:
    Vector<v8::Persistent<v8::Value> > m_liveObjects;
    Vector<ImplicitConnection> m_connections;
};

// FIXME: This should use opaque GC roots.
static void addImplicitReferencesForNodeWithEventListeners(Node* node, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(node->hasEventListeners());

    Vector<v8::Persistent<v8::Value> > listeners;

    EventListenerIterator iterator(node);
    while (EventListener* listener = iterator.nextListener()) {
        if (listener->type() != EventListener::JSEventListenerType)
            continue;
        V8AbstractEventListener* v8listener = static_cast<V8AbstractEventListener*>(listener);
        if (!v8listener->hasExistingListenerObject())
            continue;
        listeners.append(v8listener->existingListenerObjectPersistentHandle());
    }

    if (listeners.isEmpty())
        return;

    v8::V8::AddImplicitReferences(wrapper, listeners.data(), listeners.size());
}

void* V8GCController::opaqueRootForGC(Node* node)
{
    // FIXME: Remove the special handling for image elements.
    // The same special handling is in V8GCController::gcTree().
    // Maybe should image elements be active DOM nodes?
    // See https://code.google.com/p/chromium/issues/detail?id=164882
    if (node->inDocument() || (node->hasTagName(HTMLNames::imgTag) && static_cast<HTMLImageElement*>(node)->hasPendingActivity()))
        return node->document();

    if (node->isAttributeNode()) {
        Node* ownerElement = static_cast<Attr*>(node)->ownerElement();
        if (!ownerElement)
            return node;
        node = ownerElement;
    }

    while (Node* parent = node->parentOrHostNode())
        node = parent;

    return node;
}

class WrapperVisitor : public v8::PersistentHandleVisitor {
public:
    virtual void VisitPersistentHandle(v8::Persistent<v8::Value> value, uint16_t classId) OVERRIDE
    {
        ASSERT(value->IsObject());
        v8::Persistent<v8::Object> wrapper = v8::Persistent<v8::Object>::Cast(value);

        if (classId != v8DOMNodeClassId && classId != v8DOMObjectClassId)
            return;

        ASSERT(V8DOMWrapper::maybeDOMWrapper(value));

        if (value.IsIndependent())
            return;

        WrapperTypeInfo* type = toWrapperTypeInfo(wrapper);
        void* object = toNative(wrapper);

        if (V8MessagePort::info.equals(type)) {
            // Mark each port as in-use if it's entangled. For simplicity's sake,
            // we assume all ports are remotely entangled, since the Chromium port
            // implementation can't tell the difference.
            MessagePort* port = static_cast<MessagePort*>(object);
            if (port->isEntangled() || port->hasPendingActivity())
                m_grouper.keepAlive(wrapper);
#if ENABLE(MUTATION_OBSERVERS)
        } else if (V8MutationObserver::info.equals(type)) {
            // FIXME: Allow opaqueRootForGC to operate on multiple roots and move this logic into V8MutationObserverCustom.
            MutationObserver* observer = static_cast<MutationObserver*>(object);
            HashSet<Node*> observedNodes = observer->getObservedNodes();
            for (HashSet<Node*>::iterator it = observedNodes.begin(); it != observedNodes.end(); ++it)
                m_grouper.addToGroup(V8GCController::opaqueRootForGC(*it), wrapper);
#endif // ENABLE(MUTATION_OBSERVERS)
        } else {
            ActiveDOMObject* activeDOMObject = type->toActiveDOMObject(wrapper);
            if (activeDOMObject && activeDOMObject->hasPendingActivity())
                m_grouper.keepAlive(wrapper);
        }

        if (classId == v8DOMNodeClassId) {
            ASSERT(V8Node::HasInstance(wrapper));
            ASSERT(!wrapper.IsIndependent());

            Node* node = static_cast<Node*>(object);

            if (node->hasEventListeners())
                addImplicitReferencesForNodeWithEventListeners(node, wrapper);

            m_grouper.addToGroup(V8GCController::opaqueRootForGC(node), wrapper);
        } else if (classId == v8DOMObjectClassId) {
            m_grouper.addToGroup(type->opaqueRootForGC(object, wrapper), wrapper);
        } else {
            ASSERT_NOT_REACHED();
        }
    }

    void notifyFinished()
    {
        m_grouper.apply();
    }

private:
    WrapperGrouper m_grouper;
};

// Regarding a minor GC algorithm for DOM nodes, see this document:
// https://docs.google.com/a/google.com/presentation/d/1uifwVYGNYTZDoGLyCb7sXa7g49mWNMW2gaWvMN5NLk8/edit#slide=id.p
//
// m_edenNodes stores nodes that have wrappers that have been created since the last minor/major GC.
Vector<Node*>* V8GCController::m_edenNodes = 0;

static void gcTree(Node* startNode)
{
    Vector<v8::Persistent<v8::Value>, initialNodeVectorSize> newSpaceWrappers;

    // We traverse a DOM tree in the DFS order starting from startNode.
    // The traversal order does not matter for correctness but does matter for performance.
    Node* node = startNode;
    // To make each minor GC time bounded, we might need to give up
    // traversing at some point for a large DOM tree. That being said,
    // I could not observe the need even in pathological test cases.
    do {
        ASSERT(node);
        if (!node->wrapper().IsEmpty()) {
            // FIXME: Remove the special handling for image elements.
            // The same special handling is in V8GCController::opaqueRootForGC().
            // Maybe should image elements be active DOM nodes?
            // See https://code.google.com/p/chromium/issues/detail?id=164882
            if (!node->isV8CollectableDuringMinorGC() || (node->hasTagName(HTMLNames::imgTag) && static_cast<HTMLImageElement*>(node)->hasPendingActivity())) {
                // The fact that we encounter a node that is not in the Eden space
                // implies that its wrapper might be in the old space of V8.
                // This indicates that the minor GC cannot anyway judge reachability
                // of this DOM tree. Thus we give up traversing the DOM tree.
                return;
            }
            // A once traversed node is removed from the Eden space.
            node->setV8CollectableDuringMinorGC(false);
            newSpaceWrappers.append(node->wrapper());
        }
        if (node->firstChild()) {
            node = node->firstChild();
            continue;
        }
        while (!node->nextSibling()) {
            if (!node->parentNode())
                break;
            node = node->parentNode();
        }
        if (node->parentNode())
            node = node->nextSibling();
    } while (node != startNode);

    // We completed the DOM tree traversal. All wrappers in the DOM tree are
    // stored in newSpaceWrappers and are expected to exist in the new space of V8.
    // We report those wrappers to V8 as an object group.
    for (size_t i = 0; i < newSpaceWrappers.size(); i++)
        newSpaceWrappers[i].MarkPartiallyDependent();
    if (newSpaceWrappers.size() > 0)
        v8::V8::AddObjectGroup(&newSpaceWrappers[0], newSpaceWrappers.size());
}

void V8GCController::didCreateWrapperForNode(Node* node)
{
    // To make minor GC cycle time bounded, we limit the number of wrappers handled
    // by each minor GC cycle to 10000. This value was selected so that the minor
    // GC cycle time is bounded to 20 ms in a case where the new space size
    // is 16 MB and it is full of wrappers (which is almost the worst case).
    // Practically speaking, as far as I crawled real web applications,
    // the number of wrappers handled by each minor GC cycle is at most 3000.
    // So this limit is mainly for pathological micro benchmarks.
    const unsigned wrappersHandledByEachMinorGC = 10000;
    ASSERT(!node->wrapper().IsEmpty());
    if (!m_edenNodes)
        m_edenNodes = adoptPtr(new Vector<Node*>).leakPtr();
    if (m_edenNodes->size() <= wrappersHandledByEachMinorGC) {
        // A node of a newly created wrapper is put into the Eden space.
        m_edenNodes->append(node);
        node->setV8CollectableDuringMinorGC(true);
    }
}

void V8GCController::gcPrologue(v8::GCType type, v8::GCCallbackFlags flags)
{
    if (type == v8::kGCTypeScavenge)
        minorGCPrologue();
    else if (type == v8::kGCTypeMarkSweepCompact)
        majorGCPrologue();

    if (isMainThreadOrGCThread() && m_edenNodes) {
        // The Eden space is cleared at every minor/major GC.
        m_edenNodes->clear();
    }
}

void V8GCController::minorGCPrologue()
{
    if (isMainThreadOrGCThread() && m_edenNodes) {
        for (size_t i = 0; i < m_edenNodes->size(); i++) {
            ASSERT(!m_edenNodes->at(i)->wrapper().IsEmpty());
            if (m_edenNodes->at(i)->isV8CollectableDuringMinorGC()) // This branch is just for performance.
                gcTree(m_edenNodes->at(i));
        }
    }
}

// Create object groups for DOM tree nodes.
void V8GCController::majorGCPrologue()
{
    TRACE_EVENT_BEGIN0("v8", "GC");

    v8::HandleScope scope;

    WrapperVisitor visitor;
    v8::V8::VisitHandlesWithClassIds(&visitor);
    visitor.notifyFinished();

    V8PerIsolateData* data = V8PerIsolateData::current();
    data->stringCache()->clearOnGC();
}

static int workingSetEstimateMB = 0;

static Mutex& workingSetEstimateMBMutex()
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
    return mutex;
}

void V8GCController::gcEpilogue(v8::GCType type, v8::GCCallbackFlags flags)
{
    if (type == v8::kGCTypeScavenge)
        minorGCEpilogue();
    else if (type == v8::kGCTypeMarkSweepCompact)
        majorGCEpilogue();
}

void V8GCController::minorGCEpilogue()
{
}

void V8GCController::majorGCEpilogue()
{
    v8::HandleScope scope;

    // The GC can happen on multiple threads in case of dedicated workers which run in-process.
    {
        MutexLocker locker(workingSetEstimateMBMutex());
        workingSetEstimateMB = MemoryUsageSupport::actualMemoryUsageMB();
    }

    TRACE_EVENT_END0("v8", "GC");
}

void V8GCController::checkMemoryUsage()
{
    const int lowMemoryUsageMB = MemoryUsageSupport::lowMemoryUsageMB();
    const int highMemoryUsageMB = MemoryUsageSupport::highMemoryUsageMB();
    const int highUsageDeltaMB = MemoryUsageSupport::highUsageDeltaMB();
    int memoryUsageMB = MemoryUsageSupport::memoryUsageMB();
    int workingSetEstimateMBCopy;
    {
        MutexLocker locker(workingSetEstimateMBMutex());
        workingSetEstimateMBCopy = workingSetEstimateMB;
    }
    if (memoryUsageMB > lowMemoryUsageMB && memoryUsageMB > 2 * workingSetEstimateMBCopy) {
        // Memory usage is large and doubled since the last GC.
        // Check if we need to send low memory notification.
        v8::HeapStatistics heapStatistics;
        v8::V8::GetHeapStatistics(&heapStatistics);
        int heapSizeMB = heapStatistics.total_heap_size() >> 20;
        // Do not send low memory notification if V8 heap size is more than 7/8
        // of total memory usage. Let V8 to schedule GC itself in this case.
        if (heapSizeMB < memoryUsageMB / 8 * 7)
            v8::V8::LowMemoryNotification();
    } else if (memoryUsageMB > highMemoryUsageMB && memoryUsageMB > workingSetEstimateMBCopy + highUsageDeltaMB) {
        // We are approaching OOM and memory usage increased by highUsageDeltaMB since the last GC.
        v8::V8::LowMemoryNotification();
    }
}

void V8GCController::hintForCollectGarbage()
{
    V8PerIsolateData* data = V8PerIsolateData::current();
    if (!data->shouldCollectGarbageSoon())
        return;
    const int longIdlePauseInMS = 1000;
    data->clearShouldCollectGarbageSoon();
    v8::V8::ContextDisposedNotification();
    v8::V8::IdleNotification(longIdlePauseInMS);
}

void V8GCController::collectGarbage()
{
    v8::HandleScope handleScope;

    ScopedPersistent<v8::Context> context;

    context.adopt(v8::Context::New());
    if (context.isEmpty())
        return;

    {
        v8::Context::Scope scope(context.get());
        v8::Local<v8::String> source = v8::String::New("if (gc) gc();");
        v8::Local<v8::String> name = v8::String::New("gc");
        v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
        if (!script.IsEmpty()) {
            V8RecursionScope::MicrotaskSuppression scope;
            script->Run();
        }
    }

    context.clear();
}

}  // namespace WebCore
