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

#include "DOMDataStore.h"
#include "V8Binding.h"
#include "V8Node.h"
#include <wtf/MainThread.h>

namespace WebCore {

NodeWrapperVisitor::~NodeWrapperVisitor()
{
}

DOMWrapperMap<Node>& getDOMNodeMap(v8::Isolate* isolate)
{
    if (!isolate)
        isolate = v8::Isolate::GetCurrent();
    return DOMDataStore::current(isolate)->domNodeMap();
}

DOMWrapperMap<Node>& getActiveDOMNodeMap(v8::Isolate* isolate)
{
    if (!isolate)
        isolate = v8::Isolate::GetCurrent();
    return DOMDataStore::current(isolate)->activeDomNodeMap();
}

DOMWrapperMap<void>& getDOMObjectMap(v8::Isolate* isolate)
{
    if (!isolate)
        isolate = v8::Isolate::GetCurrent();
    return DOMDataStore::current(isolate)->domObjectMap();
}

DOMWrapperMap<void>& getActiveDOMObjectMap(v8::Isolate* isolate)
{
    if (!isolate)
        isolate = v8::Isolate::GetCurrent();
    return DOMDataStore::current(isolate)->activeDomObjectMap();
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

void visitDOMObjects(DOMWrapperVisitor<void>* visitor)
{
    v8::HandleScope scope;

    Vector<DOMDataStore*>& list = V8PerIsolateData::current()->allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];

        store->domObjectMap().visit(store, visitor);
    }
}

void visitActiveDOMObjects(DOMWrapperVisitor<void>* visitor)
{
    v8::HandleScope scope;

    Vector<DOMDataStore*>& list = V8PerIsolateData::current()->allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];

        store->activeDomObjectMap().visit(store, visitor);
    }
}

} // namespace WebCore
