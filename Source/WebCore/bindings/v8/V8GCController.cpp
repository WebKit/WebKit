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

#include "ActiveDOMObject.h"
#include "Attr.h"
#include "DOMDataStore.h"
#include "DOMImplementation.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "IntrusiveDOMWrapperMap.h"
#include "MemoryUsageSupport.h"
#include "MessagePort.h"
#include "RetainedDOMInfo.h"
#include "RetainedObjectInfo.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8CSSRule.h"
#include "V8CSSRuleList.h"
#include "V8CSSStyleDeclaration.h"
#include "V8DOMImplementation.h"
#include "V8MessagePort.h"
#include "V8Node.h"
#include "V8RecursionScope.h"
#include "V8StyleSheet.h"
#include "V8StyleSheetList.h"
#include "WrapperTypeInfo.h"

#include <algorithm>
#include <utility>
#include <v8-debug.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

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

        WrapperTypeInfo* type = V8DOMWrapper::domWrapperType(wrapper);
        void* object = toNative(wrapper);

        if (V8MessagePort::info.equals(type)) {
            // Mark each port as in-use if it's entangled. For simplicity's sake,
            // we assume all ports are remotely entangled, since the Chromium port
            // implementation can't tell the difference.
            MessagePort* port = static_cast<MessagePort*>(object);
            if (port->isEntangled() || port->hasPendingActivity())
                m_grouper.keepAlive(wrapper);
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

void V8GCController::gcPrologue(v8::GCType type, v8::GCCallbackFlags flags)
{
    if (type == v8::kGCTypeScavenge)
        minorGCPrologue();
    else if (type == v8::kGCTypeMarkSweepCompact)
        majorGCPrologue();
}

void V8GCController::minorGCPrologue()
{
}

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

#if PLATFORM(CHROMIUM)
static int workingSetEstimateMB = 0;

static Mutex& workingSetEstimateMBMutex()
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
    return mutex;
}
#endif

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

#if PLATFORM(CHROMIUM)
    // The GC can happen on multiple threads in case of dedicated workers which run in-process.
    {
        MutexLocker locker(workingSetEstimateMBMutex());
        workingSetEstimateMB = MemoryUsageSupport::actualMemoryUsageMB();
    }
#endif

    TRACE_EVENT_END0("v8", "GC");
}

void V8GCController::checkMemoryUsage()
{
#if PLATFORM(CHROMIUM)
    const int lowMemoryUsageMB = MemoryUsageSupport::lowMemoryUsageMB();
    const int highMemoryUsageMB = MemoryUsageSupport::highMemoryUsageMB();
    const int highUsageDeltaMB = MemoryUsageSupport::highUsageDeltaMB();
    int memoryUsageMB = MemoryUsageSupport::memoryUsageMB();
    int workingSetEstimateMBCopy;
    {
        MutexLocker locker(workingSetEstimateMBMutex());
        workingSetEstimateMBCopy = workingSetEstimateMB;
    }

    if ((memoryUsageMB > lowMemoryUsageMB && memoryUsageMB > 2 * workingSetEstimateMBCopy) || (memoryUsageMB > highMemoryUsageMB && memoryUsageMB > workingSetEstimateMBCopy + highUsageDeltaMB))
        v8::V8::LowMemoryNotification();
#endif
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
