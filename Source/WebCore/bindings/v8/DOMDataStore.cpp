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
#include "DOMDataStore.h"

#include "DOMData.h"

namespace WebCore {

// DOM binding algorithm:
//
// There are two kinds of DOM objects:
// 1. DOM tree nodes, such as Document, HTMLElement, ...
//    there classes implement TreeShared<T> interface;
// 2. Non-node DOM objects, such as CSSRule, Location, etc.
//    these classes implement a ref-counted scheme.
//
// A DOM object may have a JS wrapper object. If a tree node
// is alive, its JS wrapper must be kept alive even it is not
// reachable from JS roots.
// However, JS wrappers of non-node objects can go away if
// not reachable from other JS objects. It works like a cache.
//
// DOM objects are ref-counted, and JS objects are traced from
// a set of root objects. They can create a cycle. To break
// cycles, we do following:
//   Handles from DOM objects to JS wrappers are always weak,
// so JS wrappers of non-node object cannot create a cycle.
//   Before starting a global GC, we create a virtual connection
// between nodes in the same tree in the JS heap. If the wrapper
// of one node in a tree is alive, wrappers of all nodes in
// the same tree are considered alive. This is done by creating
// object groups in GC prologue callbacks. The mark-compact
// collector will remove these groups after each GC.
//
// DOM objects are accessed and deref-ed from the main thread. All V8 accesses
// to DOM objects happen on the main thread.
// When GC happens, we remove the JS reference and deref the DOM object.

DOMDataStore::DOMDataStore(DOMData* domData)
    : m_domNodeMap(0)
    , m_domObjectMap(0)
    , m_activeDomObjectMap(0)
#if ENABLE(SVG)
    , m_domSvgElementInstanceMap(0)
#endif
    , m_domData(domData)
{
    ASSERT(WTF::isMainThread());
    DOMDataStore::allStores().append(this);
}

DOMDataStore::~DOMDataStore()
{
    ASSERT(WTF::isMainThread());
    DOMDataStore::allStores().remove(DOMDataStore::allStores().find(this));
}

DOMDataList& DOMDataStore::allStores()
{
  DEFINE_STATIC_LOCAL(DOMDataList, staticDOMDataList, ());
  return staticDOMDataList;
}

void* DOMDataStore::getDOMWrapperMap(DOMWrapperMapType type)
{
    switch (type) {
    case DOMNodeMap:
        return m_domNodeMap;
    case DOMObjectMap:
        return m_domObjectMap;
    case ActiveDOMObjectMap:
        return m_activeDomObjectMap;
#if ENABLE(SVG)
    case DOMSVGElementInstanceMap:
        return m_domSvgElementInstanceMap;
#endif
    }

    ASSERT_NOT_REACHED();
    return 0;
}

// Called when the object is near death (not reachable from JS roots).
// It is time to remove the entry from the table and dispose the handle.
void DOMDataStore::weakDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject(DOMDataStore::DOMObjectMap, v8::Persistent<v8::Object>::Cast(v8Object), domObject);
}

void DOMDataStore::weakActiveDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject(DOMDataStore::ActiveDOMObjectMap, v8::Persistent<v8::Object>::Cast(v8Object), domObject);
}

void DOMDataStore::weakNodeCallback(v8::Persistent<v8::Value> value, void* domObject)
{
    ASSERT(WTF::isMainThread());

    Node* node = static_cast<Node*>(domObject);
    // Node wrappers must be JS objects.
    v8::Persistent<v8::Object> v8Object = v8::Persistent<v8::Object>::Cast(value);

    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (store->domNodeMap().removeIfPresent(node, v8Object)) {
            node->deref(); // Nobody overrides Node::deref so it's safe
            return; // There might be at most one wrapper for the node in world's maps
        }
    }

    // If not found, it means map for the wrapper has been already destroyed, just dispose the
    // handle and deref the object to fight memory leak.
    v8Object.Dispose();
    node->deref(); // Nobody overrides Node::deref so it's safe
}

#if ENABLE(SVG)

void DOMDataStore::weakSVGElementInstanceCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject(DOMDataStore::DOMSVGElementInstanceMap, v8::Persistent<v8::Object>::Cast(v8Object), static_cast<SVGElementInstance*>(domObject));
}

#endif  // ENABLE(SVG)

} // namespace WebCore
