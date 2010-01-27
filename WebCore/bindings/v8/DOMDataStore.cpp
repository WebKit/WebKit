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
// DOM objects should be deref-ed from the owning thread, not the GC thread
// that does not own them. In V8, GC can kick in from any thread. To ensure
// that DOM objects are always deref-ed from the owning thread when running
// V8 in multi-threading environment, we do following:
// 1. Maintain a thread specific DOM wrapper map for each object map.
//    (We're using TLS support from WTF instead of base since V8Bindings
//     does not depend on base. We further assume that all child threads
//     running V8 instances are created by WTF and thus a destructor will
//     be called to clean up all thread specific data.)
// 2. When GC happens:
//    2.1. If the dead object is in GC thread's map, remove the JS reference
//         and deref the DOM object.
//    2.2. Otherwise, go through all thread maps to find the owning thread.
//         Remove the JS reference from the owning thread's map and move the
//         DOM object to a delayed queue. Post a task to the owning thread
//         to have it deref-ed from the owning thread at later time.
// 3. When a thread is tearing down, invoke a cleanup routine to go through
//    all objects in the delayed queue and the thread map and deref all of
//    them.


DOMDataStore::DOMDataStore(DOMData* domData)
    : m_domNodeMap(0)
    , m_domObjectMap(0)
    , m_activeDomObjectMap(0)
#if ENABLE(SVG)
    , m_domSvgElementInstanceMap(0)
    , m_domSvgObjectWithContextMap(0)
#endif
    , m_domData(domData)
{
    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataStore::allStores().append(this);
}

DOMDataStore::~DOMDataStore()
{
    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataStore::allStores().remove(DOMDataStore::allStores().find(this));
}

DOMDataList& DOMDataStore::allStores()
{
  DEFINE_STATIC_LOCAL(DOMDataList, staticDOMDataList, ());
  return staticDOMDataList;
}

WTF::Mutex& DOMDataStore::allStoresMutex()
{
    DEFINE_STATIC_LOCAL(WTF::Mutex, staticDOMDataListMutex, ());
    return staticDOMDataListMutex;
}

void DOMDataStore::forgetDelayedObject(DOMData* domData, void* object)
{
    domData->forgetDelayedObject(object);
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
    case DOMSVGObjectWithContextMap:
        return m_domSvgObjectWithContextMap;
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

void DOMDataStore::weakNodeCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    ASSERT(WTF::isMainThread());

    Node* node = static_cast<Node*>(domObject);

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (store->domNodeMap().removeIfPresent(node, v8Object)) {
            ASSERT(store->domData()->owningThread() == WTF::currentThread());
            node->deref();  // Nobody overrides Node::deref so it's safe
            break;  // There might be at most one wrapper for the node in world's maps
        }
    }
}

bool DOMDataStore::IntrusiveDOMWrapperMap::removeIfPresent(Node* obj, v8::Persistent<v8::Data> value)
{
    ASSERT(obj);
    v8::Persistent<v8::Object>* entry = obj->wrapper();
    if (!entry)
        return false;
    if (*entry != value)
        return false;
    obj->clearWrapper();
    m_table.remove(entry);
    value.Dispose();
    return true;
}

#if ENABLE(SVG)

void DOMDataStore::weakSVGElementInstanceCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject(DOMDataStore::DOMSVGElementInstanceMap, v8::Persistent<v8::Object>::Cast(v8Object), static_cast<SVGElementInstance*>(domObject));
}

void DOMDataStore::weakSVGObjectWithContextCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject(DOMDataStore::DOMSVGObjectWithContextMap, v8::Persistent<v8::Object>::Cast(v8Object), domObject);
}

#endif  // ENABLE(SVG)

} // namespace WebCore
