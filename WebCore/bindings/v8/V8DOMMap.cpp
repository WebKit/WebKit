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

#include "DOMObjectsInclude.h"

#include <v8.h>

#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Vector.h>

#include "V8IsolatedWorld.h"

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

static void weakNodeCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
static void weakDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
void weakActiveDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
#if ENABLE(SVG)
static void weakSVGElementInstanceCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
// SVG non-node elements may have a reference to a context node which should be notified when the element is change.
static void weakSVGObjectWithContextCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
#endif

class DOMData;
class DOMDataStore;
typedef WTF::Vector<DOMDataStore*> DOMDataList;

// DOMDataStore
//
// DOMDataStore is the backing store that holds the maps between DOM objects
// and JavaScript objects.  In general, each thread can have multiple backing
// stores, one per isolated world.
//
// This class doesn't manage the lifetime of the store.  The data store
// lifetime is managed by subclasses.
//
class DOMDataStore : Noncopyable {
public:
    enum DOMWrapperMapType {
        DOMNodeMap,
        DOMObjectMap,
        ActiveDOMObjectMap,
#if ENABLE(SVG)
        DOMSVGElementInstanceMap,
        DOMSVGObjectWithContextMap
#endif
    };

    template <class KeyType>
    class InternalDOMWrapperMap : public DOMWrapperMap<KeyType> {
    public:
        InternalDOMWrapperMap(DOMData* domData, v8::WeakReferenceCallback callback)
            : DOMWrapperMap<KeyType>(callback)
            , m_domData(domData) { }

        virtual void forget(KeyType*);

        void forgetOnly(KeyType* object)
        {
            DOMWrapperMap<KeyType>::forget(object);
        }

    private:
        DOMData* m_domData;
    };

    // A list of all DOMDataStore objects.  Traversed during GC to find a thread-specific map that
    // contains the object - so we can schedule the object to be deleted on the thread which created it.
    static DOMDataList& allStores()
    {
        DEFINE_STATIC_LOCAL(DOMDataList, staticDOMDataList, ());
        return staticDOMDataList;
    }

    // Mutex to protect against concurrent access of DOMDataList.
    static WTF::Mutex& allStoresMutex()
    {
        DEFINE_STATIC_LOCAL(WTF::Mutex, staticDOMDataListMutex, ());
        return staticDOMDataListMutex;
    }

    DOMDataStore(DOMData* domData);
    virtual ~DOMDataStore();

    DOMData* domData() const { return m_domData; }

    void* getDOMWrapperMap(DOMWrapperMapType type)
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

    InternalDOMWrapperMap<Node>& domNodeMap() { return *m_domNodeMap; }
    InternalDOMWrapperMap<void>& domObjectMap() { return *m_domObjectMap; }
    InternalDOMWrapperMap<void>& activeDomObjectMap() { return *m_activeDomObjectMap; }
#if ENABLE(SVG)
    InternalDOMWrapperMap<SVGElementInstance>& domSvgElementInstanceMap() { return *m_domSvgElementInstanceMap; }
    InternalDOMWrapperMap<void>& domSvgObjectWithContextMap() { return *m_domSvgObjectWithContextMap; }
#endif

protected:
    InternalDOMWrapperMap<Node>* m_domNodeMap;
    InternalDOMWrapperMap<void>* m_domObjectMap;
    InternalDOMWrapperMap<void>* m_activeDomObjectMap;
#if ENABLE(SVG)
    InternalDOMWrapperMap<SVGElementInstance>* m_domSvgElementInstanceMap;
    InternalDOMWrapperMap<void>* m_domSvgObjectWithContextMap;
#endif

private:
    // A back-pointer to the DOMData to which we belong.
    DOMData* m_domData;
};

// ScopedDOMDataStore
//
// ScopedDOMDataStore is a DOMDataStore that controls limits the lifetime of
// the store to the lifetime of the object itself.  In other words, when the
// ScopedDOMDataStore object is deallocated, the maps that belong to the store
// are deallocated as well.
//
class ScopedDOMDataStore : public DOMDataStore {
public:
    ScopedDOMDataStore(DOMData* domData) : DOMDataStore(domData)
    {
        m_domNodeMap = new InternalDOMWrapperMap<Node>(domData, weakNodeCallback);
        m_domObjectMap = new InternalDOMWrapperMap<void>(domData, weakDOMObjectCallback);
        m_activeDomObjectMap = new InternalDOMWrapperMap<void>(domData, weakActiveDOMObjectCallback);
#if ENABLE(SVG)
        m_domSvgElementInstanceMap = new InternalDOMWrapperMap<SVGElementInstance>(domData, weakSVGElementInstanceCallback);
        m_domSvgObjectWithContextMap = new InternalDOMWrapperMap<void>(domData, weakSVGObjectWithContextCallback);
#endif
    }

    // This can be called when WTF thread is tearing down.
    // We assume that all child threads running V8 instances are created by WTF.
    virtual ~ScopedDOMDataStore()
    {
        delete m_domNodeMap;
        delete m_domObjectMap;
        delete m_activeDomObjectMap;
#if ENABLE(SVG)
        delete m_domSvgElementInstanceMap;
        delete m_domSvgObjectWithContextMap;
#endif
    }
};

// StaticDOMDataStore
//
// StaticDOMDataStore is a DOMDataStore that manages the lifetime of the store
// statically.  This encapsulates thread-specific DOM data for the main
// thread.  All the maps in it are static.  This is because we are unable to
// rely on WTF::ThreadSpecificThreadExit to do the cleanup since the place that
// tears down the main thread can not call any WTF functions.
class StaticDOMDataStore : public DOMDataStore {
public:
    StaticDOMDataStore(DOMData* domData)
        : DOMDataStore(domData)
        , m_staticDomNodeMap(domData, weakNodeCallback)
        , m_staticDomObjectMap(domData, weakDOMObjectCallback)
        , m_staticActiveDomObjectMap(domData, weakActiveDOMObjectCallback)
#if ENABLE(SVG)
        , m_staticDomSvgElementInstanceMap(domData, weakSVGElementInstanceCallback)
        , m_staticDomSvgObjectWithContextMap(domData, weakSVGObjectWithContextCallback)
#endif
    {
        m_domNodeMap = &m_staticDomNodeMap;
        m_domObjectMap = &m_staticDomObjectMap;
        m_activeDomObjectMap = &m_staticActiveDomObjectMap;
#if ENABLE(SVG)
        m_domSvgElementInstanceMap = &m_staticDomSvgElementInstanceMap;
        m_domSvgObjectWithContextMap = &m_staticDomSvgObjectWithContextMap;
#endif
    }

private:
    InternalDOMWrapperMap<Node> m_staticDomNodeMap;
    InternalDOMWrapperMap<void> m_staticDomObjectMap;
    InternalDOMWrapperMap<void> m_staticActiveDomObjectMap;
    InternalDOMWrapperMap<SVGElementInstance> m_staticDomSvgElementInstanceMap;
    InternalDOMWrapperMap<void> m_staticDomSvgObjectWithContextMap;
};

typedef WTF::Vector<DOMDataStore*> DOMDataStoreList;

// DOMData
//
// DOMData represents the all the DOM wrappers for a given thread.  In
// particular, DOMData holds wrappers for all the isolated worlds in the
// thread.  The DOMData for the main thread and the DOMData for child threads
// use different subclasses.
//
class DOMData: Noncopyable {
public:
    DOMData()
        : m_delayedProcessingScheduled(false)
        , m_isMainThread(WTF::isMainThread())
        , m_owningThread(WTF::currentThread())
    {
    }

    static DOMData* getCurrent();
    virtual DOMDataStore& getStore() = 0;

    template<typename T>
    static void handleWeakObject(DOMDataStore::DOMWrapperMapType mapType, v8::Handle<v8::Object> v8Object, T* domObject);

    void forgetDelayedObject(void* object) { m_delayedObjectMap.take(object); }

    // This is to ensure that we will deref DOM objects from the owning thread,
    // not the GC thread.  The helper function will be scheduled by the GC
    // thread to get called from the owning thread.
    static void derefDelayedObjectsInCurrentThread(void*);
    void derefDelayedObjects();

    template<typename T>
    static void removeObjectsFromWrapperMap(DOMWrapperMap<T>& domMap);

    ThreadIdentifier owningThread() const { return m_owningThread; }

private:
    typedef WTF::HashMap<void*, V8ClassIndex::V8WrapperType> DelayedObjectMap;

    void ensureDeref(V8ClassIndex::V8WrapperType type, void* domObject);
    static void derefObject(V8ClassIndex::V8WrapperType type, void* domObject);

    // Stores all the DOM objects that are delayed to be processed when the owning thread gains control.
    DelayedObjectMap m_delayedObjectMap;

    // The flag to indicate if the task to do the delayed process has already been posted.
    bool m_delayedProcessingScheduled;

    bool m_isMainThread;
    ThreadIdentifier m_owningThread;
};

class MainThreadDOMData : public DOMData {
public:
    MainThreadDOMData() : m_defaultStore(this) { }

    DOMDataStore& getStore()
    {
        ASSERT(WTF::isMainThread());
        V8IsolatedWorld* world = V8IsolatedWorld::getEntered();
        if (world)
            return *world->getDOMDataStore();
        return m_defaultStore;
    }

private:
    StaticDOMDataStore m_defaultStore;
    // Note: The DOMDataStores for isolated world are owned by the world object.
};

class ChildThreadDOMData : public DOMData {
public:
    ChildThreadDOMData() : m_defaultStore(this) { }

    DOMDataStore& getStore() {
        ASSERT(!WTF::isMainThread());
        // Currently, child threads have only one world.
        return m_defaultStore;
    }

private:
    ScopedDOMDataStore m_defaultStore;
};

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

DOMDataStoreHandle::DOMDataStoreHandle()
    : m_store(new ScopedDOMDataStore(DOMData::getCurrent()))
{
}

DOMDataStoreHandle::~DOMDataStoreHandle()
{
}

template <class KeyType>
void DOMDataStore::InternalDOMWrapperMap<KeyType>::forget(KeyType* object)
{
    DOMWrapperMap<KeyType>::forget(object);
    m_domData->forgetDelayedObject(object);
}

DOMWrapperMap<Node>& getDOMNodeMap()
{
    return DOMData::getCurrent()->getStore().domNodeMap();
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

// static
DOMData* DOMData::getCurrent()
{
    if (WTF::isMainThread()) {
        DEFINE_STATIC_LOCAL(MainThreadDOMData, mainThreadDOMData, ());
        return &mainThreadDOMData;
    }
    DEFINE_STATIC_LOCAL(WTF::ThreadSpecific<ChildThreadDOMData>, childThreadDOMData, ());
    return childThreadDOMData;
}

// Called when the dead object is not in GC thread's map. Go through all thread maps to find the one containing it.
// Then clear the JS reference and push the DOM object into the delayed queue for it to be deref-ed at later time from the owning thread.
// * This is called when the GC thread is not the owning thread.
// * This can be called on any thread that has GC running.
// * Only one V8 instance is running at a time due to V8::Locker. So we don't need to worry about concurrency.
template<typename T>
// static
void DOMData::handleWeakObject(DOMDataStore::DOMWrapperMapType mapType, v8::Handle<v8::Object> v8Object, T* domObject)
{

    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());
    DOMDataList& list = DOMDataStore::allStores();
    for (size_t i = 0; i < list.size(); ++i) {
        DOMDataStore* store = list[i];

        DOMDataStore::InternalDOMWrapperMap<T>* domMap = static_cast<DOMDataStore::InternalDOMWrapperMap<T>*>(store->getDOMWrapperMap(mapType));

        v8::Handle<v8::Object> wrapper = domMap->get(domObject);
        if (*wrapper == *v8Object) {
            // Clear the JS reference.
            domMap->forgetOnly(domObject);
            store->domData()->ensureDeref(V8DOMWrapper::domWrapperType(v8Object), domObject);
        }
    }
}

void DOMData::ensureDeref(V8ClassIndex::V8WrapperType type, void* domObject)
{
    if (m_owningThread == WTF::currentThread()) {
        // No need to delay the work.  We can deref right now.
        derefObject(type, domObject);
        return;
    }

    // We need to do the deref on the correct thread.
    m_delayedObjectMap.set(domObject, type);

    // Post a task to the owning thread in order to process the delayed queue.
    // FIXME: For now, we can only post to main thread due to WTF task posting limitation. We will fix this when we work on nested worker.
    if (!m_delayedProcessingScheduled) {
        m_delayedProcessingScheduled = true;
        if (isMainThread())
            WTF::callOnMainThread(&derefDelayedObjectsInCurrentThread, 0);
    }
}

// Called when the object is near death (not reachable from JS roots).
// It is time to remove the entry from the table and dispose the handle.
static void weakDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject(DOMDataStore::DOMObjectMap, v8::Handle<v8::Object>::Cast(v8Object), domObject);
}

void weakActiveDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject(DOMDataStore::ActiveDOMObjectMap, v8::Handle<v8::Object>::Cast(v8Object), domObject);
}

static void weakNodeCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject<Node>(DOMDataStore::DOMNodeMap, v8::Handle<v8::Object>::Cast(v8Object), static_cast<Node*>(domObject));
}

#if ENABLE(SVG)

static void weakSVGElementInstanceCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject(DOMDataStore::DOMSVGElementInstanceMap, v8::Handle<v8::Object>::Cast(v8Object), static_cast<SVGElementInstance*>(domObject));
}

static void weakSVGObjectWithContextCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());
    DOMData::handleWeakObject(DOMDataStore::DOMSVGObjectWithContextMap, v8::Handle<v8::Object>::Cast(v8Object), domObject);
}

#endif  // ENABLE(SVG)

// static
void DOMData::derefObject(V8ClassIndex::V8WrapperType type, void* domObject)
{
    switch (type) {
    case V8ClassIndex::NODE:
        static_cast<Node*>(domObject)->deref();
        break;

#define MakeCase(type, name)   \
        case V8ClassIndex::type: static_cast<name*>(domObject)->deref(); break;
    DOM_OBJECT_TYPES(MakeCase)   // This includes both active and non-active.
#undef MakeCase

#if ENABLE(SVG)
#define MakeCase(type, name)     \
        case V8ClassIndex::type: static_cast<name*>(domObject)->deref(); break;
    SVG_OBJECT_TYPES(MakeCase)   // This also includes SVGElementInstance.
#undef MakeCase

#define MakeCase(type, name)     \
        case V8ClassIndex::type:    \
            static_cast<V8SVGPODTypeWrapper<name>*>(domObject)->deref(); break;
    SVG_POD_NATIVE_TYPES(MakeCase)
#undef MakeCase
#endif

    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void DOMData::derefDelayedObjects()
{
    WTF::MutexLocker locker(DOMDataStore::allStoresMutex());

    m_delayedProcessingScheduled = false;

    for (DelayedObjectMap::iterator iter(m_delayedObjectMap.begin()); iter != m_delayedObjectMap.end(); ++iter)
        derefObject(iter->second, iter->first);

    m_delayedObjectMap.clear();
}

// static
void DOMData::derefDelayedObjectsInCurrentThread(void*)
{
    getCurrent()->derefDelayedObjects();
}

// static
template<typename T>
void DOMData::removeObjectsFromWrapperMap(DOMWrapperMap<T>& domMap)
{
    for (typename WTF::HashMap<T*, v8::Object*>::iterator iter(domMap.impl().begin()); iter != domMap.impl().end(); ++iter) {
        T* domObject = static_cast<T*>(iter->first);
        v8::Persistent<v8::Object> v8Object(iter->second);

        V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(v8::Handle<v8::Object>::Cast(v8Object));

        // Deref the DOM object.
        derefObject(type, domObject);

        // Clear the JS wrapper.
        v8Object.Dispose();
    }
    domMap.impl().clear();
}

static void removeAllDOMObjectsInCurrentThreadHelper()
{
    v8::HandleScope scope;

    // Deref all objects in the delayed queue.
    DOMData::getCurrent()->derefDelayedObjects();

    // Remove all DOM nodes.
    DOMData::removeObjectsFromWrapperMap<Node>(getDOMNodeMap());

    // Remove all DOM objects in the wrapper map.
    DOMData::removeObjectsFromWrapperMap<void>(getDOMObjectMap());

    // Remove all active DOM objects in the wrapper map.
    DOMData::removeObjectsFromWrapperMap<void>(getActiveDOMObjectMap());

#if ENABLE(SVG)
    // Remove all SVG element instances in the wrapper map.
    DOMData::removeObjectsFromWrapperMap<SVGElementInstance>(getDOMSVGElementInstanceMap());

    // Remove all SVG objects with context in the wrapper map.
    DOMData::removeObjectsFromWrapperMap<void>(getDOMSVGObjectWithContextMap());
#endif
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
