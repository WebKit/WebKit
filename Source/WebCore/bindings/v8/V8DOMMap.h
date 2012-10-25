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

#ifndef V8DOMMap_h
#define V8DOMMap_h

#include "DOMWrapperMap.h"
#include "Node.h"
#include "WebCoreMemoryInstrumentation.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/OwnPtr.h>
#include <v8.h>

namespace WebCore {

    class DOMDataStore;

    // A utility class to manage the lifetime of set of DOM wrappers.
    class DOMDataStoreHandle {
    public:
        explicit DOMDataStoreHandle(bool initialize);
        ~DOMDataStoreHandle();

        DOMDataStore* getStore() const { return m_store.get(); }

    private:
        OwnPtr<DOMDataStore> m_store;
    };

    // A map from DOM node to its JS wrapper.
    DOMWrapperMap<Node>& getDOMNodeMap(v8::Isolate* = 0);
    DOMWrapperMap<Node>& getActiveDOMNodeMap(v8::Isolate* = 0);
    void visitActiveDOMNodes(DOMWrapperVisitor<Node>*);

    class NodeWrapperVisitor {
    public:
        virtual ~NodeWrapperVisitor();
        virtual void visitNodeWrapper(Node*, v8::Persistent<v8::Object> wrapper) = 0;
    };
    void visitAllDOMNodes(NodeWrapperVisitor*);

    // A map from a DOM object (non-node) to its JS wrapper. This map does not contain the DOM objects which can have pending activity (active dom objects).
    DOMWrapperMap<void>& getDOMObjectMap(v8::Isolate* = 0);
    void visitDOMObjects(DOMWrapperVisitor<void>*);

    // A map from a DOM object to its JS wrapper for DOM objects which can have pending activity.
    DOMWrapperMap<void>& getActiveDOMObjectMap(v8::Isolate* = 0);
    void visitActiveDOMObjects(DOMWrapperVisitor<void>*);

} // namespace WebCore

#endif // V8DOMMap_h
