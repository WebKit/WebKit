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
#include "DOMObjectsInclude.h"
#include "MainThreadDOMData.h"
#include "ScopedDOMDataStore.h"

namespace WebCore {

DOMDataStoreHandle::DOMDataStoreHandle()
    : m_store(new ScopedDOMDataStore(DOMData::getCurrent()))
{
}

DOMDataStoreHandle::~DOMDataStoreHandle()
{
}

DOMWrapperMap<Node>& getDOMNodeMap()
{
    return MainThreadDOMData::getCurrentMainThreadStore().domNodeMap();
}

DOMWrapperMap<void>& getDOMObjectMap()
{
    return DOMData::getCurrent()->getStore().domObjectMap();
}

DOMWrapperMap<void>& getActiveDOMObjectMap()
{
    return DOMData::getCurrent()->getStore().activeDomObjectMap();
}

#if ENABLE(SVG)

DOMWrapperMap<SVGElementInstance>& getDOMSVGElementInstanceMap()
{
    return DOMData::getCurrent()->getStore().domSvgElementInstanceMap();
}

// Map of SVG objects with contexts to V8 objects
DOMWrapperMap<void>& getDOMSVGObjectWithContextMap()
{
    return DOMData::getCurrent()->getStore().domSvgObjectWithContextMap();
}

#endif // ENABLE(SVG)

static void removeAllDOMObjectsInCurrentThreadHelper()
{
    v8::HandleScope scope;

    // Deref all objects in the delayed queue.
    DOMData::getCurrent()->derefDelayedObjects();

    // The DOM objects with the following types only exist on the main thread.
    if (WTF::isMainThread()) {
        // Remove all DOM nodes.
        DOMData::removeObjectsFromWrapperMap<Node>(getDOMNodeMap());

#if ENABLE(SVG)
        // Remove all SVG element instances in the wrapper map.
        DOMData::removeObjectsFromWrapperMap<SVGElementInstance>(getDOMSVGElementInstanceMap());

        // Remove all SVG objects with context in the wrapper map.
        DOMData::removeObjectsFromWrapperMap<void>(getDOMSVGObjectWithContextMap());
#endif
    }

    // Remove all DOM objects in the wrapper map.
    DOMData::removeObjectsFromWrapperMap<void>(getDOMObjectMap());

    // Remove all active DOM objects in the wrapper map.
    DOMData::removeObjectsFromWrapperMap<void>(getActiveDOMObjectMap());
}

void removeAllDOMObjectsInCurrentThread()
{
    // Use the locker only if it has already been invoked before, as by worker thread.
    if (v8::Locker::IsActive()) {
        v8::Locker locker;
        removeAllDOMObjectsInCurrentThreadHelper();
    } else
        removeAllDOMObjectsInCurrentThreadHelper();
}


void visitDOMNodesInCurrentThread(DOMWrapperMap<Node>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<Node*, v8::Object*>& map = store->domNodeMap().impl();
        for (HashMap<Node*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

void visitDOMObjectsInCurrentThread(DOMWrapperMap<void>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<void*, v8::Object*> & map = store->domObjectMap().impl();
        for (HashMap<void*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

void visitActiveDOMObjectsInCurrentThread(DOMWrapperMap<void>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<void*, v8::Object*>& map = store->activeDomObjectMap().impl();
        for (HashMap<void*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

#if ENABLE(SVG)

void visitDOMSVGElementInstancesInCurrentThread(DOMWrapperMap<SVGElementInstance>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<SVGElementInstance*, v8::Object*> & map = store->domSvgElementInstanceMap().impl();
        for (HashMap<SVGElementInstance*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

void visitSVGObjectsInCurrentThread(DOMWrapperMap<void>::Visitor* visitor)
{
    v8::HandleScope scope;

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];
        if (!store->domData()->owningThread() == WTF::currentThread())
            continue;

        HashMap<void*, v8::Object*>& map = store->domSvgObjectWithContextMap().impl();
        for (HashMap<void*, v8::Object*>::iterator it = map.begin(); it != map.end(); ++it)
            visitor->visitDOMWrapper(it->first, v8::Persistent<v8::Object>(it->second));
    }
}

#endif

} // namespace WebCore
