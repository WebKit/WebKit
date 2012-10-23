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
#include "V8DOMMap.h"

#include "DOMData.h"
#include "DOMDataStore.h"
#include "ScopedDOMDataStore.h"
#include "V8Binding.h"
#include <wtf/MainThread.h>

namespace WebCore {

DOMDataStoreHandle::DOMDataStoreHandle(bool initialize)
    : m_store(adoptPtr(!initialize ? 0 : new ScopedDOMDataStore()))
{
    if (m_store)
        V8PerIsolateData::current()->registerDOMDataStore(m_store.get());
}

DOMDataStoreHandle::~DOMDataStoreHandle()
{
    if (m_store)
        V8PerIsolateData::current()->unregisterDOMDataStore(m_store.get());
}

NodeWrapperVisitor::~NodeWrapperVisitor()
{
}

DOMNodeMapping& getDOMNodeMap(v8::Isolate* isolate)
{
    return DOMData::getCurrentStore(isolate).domNodeMap();
}

DOMNodeMapping& getActiveDOMNodeMap(v8::Isolate* isolate)
{
    return DOMData::getCurrentStore(isolate).activeDomNodeMap();
}

DOMWrapperMap<void>& getDOMObjectMap(v8::Isolate* isolate)
{
    return DOMData::getCurrentStore(isolate).domObjectMap();
}

DOMWrapperMap<void>& getActiveDOMObjectMap(v8::Isolate* isolate)
{
    return DOMData::getCurrentStore(isolate).activeDomObjectMap();
}

void removeAllDOMObjects()
{
    DOMDataStore& store = DOMData::getCurrentStore();

    v8::HandleScope scope;
    ASSERT(!isMainThread());

    // Note: We skip the Node wrapper maps because they exist only on the main thread.
    DOMData::removeObjectsFromWrapperMap<void>(&store, store.domObjectMap());
    DOMData::removeObjectsFromWrapperMap<void>(&store, store.activeDomObjectMap());
}

void visitAllDOMNodes(NodeWrapperVisitor* visitor)
{
    v8::HandleScope scope;

    class VisitorAdapter : public v8::PersistentHandleVisitor {
    public:
        explicit VisitorAdapter(NodeWrapperVisitor* visitor)
            : m_visitor(visitor)
        {
        }

        virtual void VisitPersistentHandle(v8::Persistent<v8::Value> value, uint16_t classId)
        {
            if (classId != v8DOMSubtreeClassId)
                return;
            ASSERT(V8Node::HasInstance(value));
            ASSERT(value->IsObject());
            ASSERT(!value.IsIndependent());
            v8::Persistent<v8::Object> wrapper = v8::Persistent<v8::Object>::Cast(value);
            m_visitor->visitNodeWrapper(V8Node::toNative(wrapper), wrapper);
        }

    private:
        NodeWrapperVisitor* m_visitor;
    } visitorAdapter(visitor);

    v8::V8::VisitHandlesWithClassIds(&visitorAdapter);
}

void visitActiveDOMNodes(DOMWrapperMap<Node>::Visitor* visitor)
{
    v8::HandleScope scope;

    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];

        store->activeDomNodeMap().visit(store, visitor);
    }
}

void visitDOMObjects(DOMWrapperMap<void>::Visitor* visitor)
{
    v8::HandleScope scope;

    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];

        store->domObjectMap().visit(store, visitor);
    }
}

void visitActiveDOMObjects(DOMWrapperMap<void>::Visitor* visitor)
{
    v8::HandleScope scope;

    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];

        store->activeDomObjectMap().visit(store, visitor);
    }
}

} // namespace WebCore
