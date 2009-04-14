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

static void weakDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
static void weakNodeCallback(v8::Persistent<v8::Value> v8Object, void* domObject);

#if ENABLE(SVG)
static void weakSVGElementInstanceCallback(v8::Persistent<v8::Value> v8Object, void* domObject);

// SVG non-node elements may have a reference to a context node which should be notified when the element is change.
static void weakSVGObjectWithContextCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
#endif

// This is to ensure that we will deref DOM objects from the owning thread, not the GC thread.
// The helper function will be scheduled by the GC thread to get called from the owning thread.
static void derefDelayedObjectsInCurrentThread(void*);

// A list of all ThreadSpecific DOM Data objects. Traversed during GC to find a thread-specific map that
// contains the object - so we can schedule the object to be deleted on the thread which created it.
class ThreadSpecificDOMData;
typedef WTF::Vector<ThreadSpecificDOMData*> DOMDataList;
static DOMDataList& domDataList()
{
    DEFINE_STATIC_LOCAL(DOMDataList, staticDOMDataList, ());
    return staticDOMDataList;
}

// Mutex to protect against concurrent access of DOMDataList.
static WTF::Mutex& domDataListMutex()
{
    DEFINE_STATIC_LOCAL(WTF::Mutex, staticDOMDataListMutex, ());
    return staticDOMDataListMutex;
}

class ThreadSpecificDOMData : Noncopyable {
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

    typedef WTF::HashMap<void*, V8ClassIndex::V8WrapperType> DelayedObjectMap;

    template <class KeyType>
    class InternalDOMWrapperMap : public DOMWrapperMap<KeyType> {
    public:
        InternalDOMWrapperMap(v8::WeakReferenceCallback callback)
            : DOMWrapperMap<KeyType>(callback) { }

        virtual void forget(KeyType*);

        void forgetOnly(KeyType* object)
        {
            DOMWrapperMap<KeyType>::forget(object);
        }
    };

    ThreadSpecificDOMData()
        : m_domNodeMap(0)
        , m_domObjectMap(0)
        , m_activeDomObjectMap(0)
#if ENABLE(SVG)
        , m_domSvgElementInstanceMap(0)
        , m_domSvgObjectWithContextMap(0)
#endif
        , m_delayedProcessingScheduled(false)
        , m_isMainThread(WTF::isMainThread())
    {
        WTF::MutexLocker locker(domDataListMutex());
        domDataList().append(this);
    }

    virtual ~ThreadSpecificDOMData()
    {
        WTF::MutexLocker locker(domDataListMutex());
        domDataList().remove(domDataList().find(this));
    }

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

    DelayedObjectMap& delayedObjectMap() { return m_delayedObjectMap; }
    bool delayedProcessingScheduled() const { return m_delayedProcessingScheduled; }
    void setDelayedProcessingScheduled(bool value) { m_delayedProcessingScheduled = value; }
    bool isMainThread() const { return m_isMainThread; }

protected:
    InternalDOMWrapperMap<Node>* m_domNodeMap;
    InternalDOMWrapperMap<void>* m_domObjectMap;
    InternalDOMWrapperMap<void>* m_activeDomObjectMap;
#if ENABLE(SVG)
    InternalDOMWrapperMap<SVGElementInstance>* m_domSvgElementInstanceMap;
    InternalDOMWrapperMap<void>* m_domSvgObjectWithContextMap;
#endif

    // Stores all the DOM objects that are delayed to be processed when the owning thread gains control.
    DelayedObjectMap m_delayedObjectMap;

    // The flag to indicate if the task to do the delayed process has already been posted.
    bool m_delayedProcessingScheduled;

    bool m_isMainThread;
};

// This encapsulates thread-specific DOM data for non-main thread. All the maps in it are created dynamically.
class NonMainThreadSpecificDOMData : public ThreadSpecificDOMData {
public:
    NonMainThreadSpecificDOMData()
    {
        m_domNodeMap = new InternalDOMWrapperMap<Node>(&weakNodeCallback);
        m_domObjectMap = new InternalDOMWrapperMap<void>(weakDOMObjectCallback);
        m_activeDomObjectMap = new InternalDOMWrapperMap<void>(weakActiveDOMObjectCallback);
#if ENABLE(SVG)
        m_domSvgElementInstanceMap = new InternalDOMWrapperMap<SVGElementInstance>(weakSVGElementInstanceCallback);
        m_domSvgObjectWithContextMap = new InternalDOMWrapperMap<void>(weakSVGObjectWithContextCallback);
#endif
    }

    // This is called when WTF thread is tearing down.
    // We assume that all child threads running V8 instances are created by WTF.
    virtual ~NonMainThreadSpecificDOMData()
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

// This encapsulates thread-specific DOM data for the main thread. All the maps in it are static.
// This is because we are unable to rely on WTF::ThreadSpecificThreadExit to do the cleanup since
// the place that tears down the main thread can not call any WTF functions.
class MainThreadSpecificDOMData : public ThreadSpecificDOMData {
public:
    MainThreadSpecificDOMData()
        : m_staticDomNodeMap(weakNodeCallback)
        , m_staticDomObjectMap(weakDOMObjectCallback)
        , m_staticActiveDomObjectMap(weakActiveDOMObjectCallback)
#if ENABLE(SVG)
        , m_staticDomSvgElementInstanceMap(weakSVGElementInstanceCallback)
        , m_staticDomSvgObjectWithContextMap(weakSVGObjectWithContextCallback)
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

DEFINE_STATIC_LOCAL(WTF::ThreadSpecific<NonMainThreadSpecificDOMData>, threadSpecificDOMData, ());

template<typename T>
static void handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMWrapperMapType mapType, V8ClassIndex::V8WrapperType objectType, T*);

ThreadSpecificDOMData& getThreadSpecificDOMData()
{
    if (WTF::isMainThread()) {
        DEFINE_STATIC_LOCAL(MainThreadSpecificDOMData, mainThreadSpecificDOMData, ());
        return mainThreadSpecificDOMData;
    }
    return *threadSpecificDOMData;
}

template <class KeyType>
void ThreadSpecificDOMData::InternalDOMWrapperMap<KeyType>::forget(KeyType* object)
{
    DOMWrapperMap<KeyType>::forget(object);

    ThreadSpecificDOMData::DelayedObjectMap& delayedObjectMap = getThreadSpecificDOMData().delayedObjectMap();
    delayedObjectMap.take(object);
}

DOMWrapperMap<Node>& getDOMNodeMap()
{
    return getThreadSpecificDOMData().domNodeMap();
}

DOMWrapperMap<void>& getDOMObjectMap()
{
    return getThreadSpecificDOMData().domObjectMap();
}

DOMWrapperMap<void>& getActiveDOMObjectMap()
{
    return getThreadSpecificDOMData().activeDomObjectMap();
}

#if ENABLE(SVG)
DOMWrapperMap<SVGElementInstance>& getDOMSVGElementInstanceMap()
{
    return getThreadSpecificDOMData().domSvgElementInstanceMap();
}

static void weakSVGElementInstanceCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    SVGElementInstance* instance = static_cast<SVGElementInstance*>(domObject);

    ThreadSpecificDOMData::InternalDOMWrapperMap<SVGElementInstance>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<SVGElementInstance>&>(getDOMSVGElementInstanceMap());
    if (map.contains(instance)) {
        instance->deref();
        map.forgetOnly(instance);
    } else
        handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMSVGElementInstanceMap, V8ClassIndex::SVGELEMENTINSTANCE, instance);
}

// Map of SVG objects with contexts to V8 objects
DOMWrapperMap<void>& getDOMSVGObjectWithContextMap()
{
    return getThreadSpecificDOMData().domSvgObjectWithContextMap();
}

static void weakSVGObjectWithContextCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());

    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(v8Object));

    ThreadSpecificDOMData::InternalDOMWrapperMap<void>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<void>&>(getDOMSVGObjectWithContextMap());
    if (map.contains(domObject)) {
        // The forget function removes object from the map and disposes the wrapper.
        map.forgetOnly(domObject);

        switch (type) {
#define MakeCase(type, name)     \
            case V8ClassIndex::type: static_cast<name*>(domObject)->deref(); break;
        SVG_OBJECT_TYPES(MakeCase)
#undef MakeCase
#define MakeCase(type, name)     \
            case V8ClassIndex::type:    \
                static_cast<V8SVGPODTypeWrapper<name>*>(domObject)->deref(); break;
        SVG_POD_NATIVE_TYPES(MakeCase)
#undef MakeCase
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else
        handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMSVGObjectWithContextMap, type, domObject);
}
#endif // ENABLE(SVG)

// Called when the dead object is not in GC thread's map. Go through all thread maps to find the one containing it.
// Then clear the JS reference and push the DOM object into the delayed queue for it to be deref-ed at later time from the owning thread.
// * This is called when the GC thread is not the owning thread.
// * This can be called on any thread that has GC running.
// * Only one V8 instance is running at a time due to V8::Locker. So we don't need to worry about concurrency.
template<typename T>
static void handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMWrapperMapType mapType, V8ClassIndex::V8WrapperType objectType, T* object)
{
    WTF::MutexLocker locker(domDataListMutex());
    DOMDataList& list = domDataList();
    for (size_t i = 0; i < list.size(); ++i) {
        ThreadSpecificDOMData* threadData = list[i];

        ThreadSpecificDOMData::InternalDOMWrapperMap<T>* domMap = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<T>*>(threadData->getDOMWrapperMap(mapType));
        if (domMap->contains(object)) {
            // Clear the JS reference.
            domMap->forgetOnly(object);

            // Push into the delayed queue.
            threadData->delayedObjectMap().set(object, objectType);

            // Post a task to the owning thread in order to process the delayed queue.
            // FIXME: For now, we can only post to main thread due to WTF task posting limitation. We will fix this when we work on nested worker.
            if (!threadData->delayedProcessingScheduled()) {
                threadData->setDelayedProcessingScheduled(true);
                if (threadData->isMainThread())
                    WTF::callOnMainThread(&derefDelayedObjectsInCurrentThread, 0);
            }

            break;
        }
    }
}

// Called when the object is near death (not reachable from JS roots).
// It is time to remove the entry from the table and dispose the handle.
static void weakDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());

    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(v8Object));

    ThreadSpecificDOMData::InternalDOMWrapperMap<void>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<void>&>(getDOMObjectMap());
    if (map.contains(domObject)) {
        // The forget function removes object from the map and disposes the wrapper.
        map.forgetOnly(domObject);

        switch (type) {
#define MakeCase(type, name)   \
            case V8ClassIndex::type: static_cast<name*>(domObject)->deref(); break;
        DOM_OBJECT_TYPES(MakeCase)
#undef MakeCase
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else
        handleWeakObjectInOwningThread(ThreadSpecificDOMData::DOMObjectMap, type, domObject);
}

void weakActiveDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    v8::HandleScope scope;
    ASSERT(v8Object->IsObject());

    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(v8Object));

    ThreadSpecificDOMData::InternalDOMWrapperMap<void>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<void>&>(getActiveDOMObjectMap());
    if (map.contains(domObject)) {
        // The forget function removes object from the map and disposes the wrapper.
        map.forgetOnly(domObject);

        switch (type) {
#define MakeCase(type, name)   \
            case V8ClassIndex::type: static_cast<name*>(domObject)->deref(); break;
        ACTIVE_DOM_OBJECT_TYPES(MakeCase)
#undef MakeCase
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    } else
        handleWeakObjectInOwningThread(ThreadSpecificDOMData::ActiveDOMObjectMap, type, domObject);
}

static void weakNodeCallback(v8::Persistent<v8::Value> v8Object, void* domObject)
{
    Node* node = static_cast<Node*>(domObject);

    ThreadSpecificDOMData::InternalDOMWrapperMap<Node>& map = static_cast<ThreadSpecificDOMData::InternalDOMWrapperMap<Node>&>(getDOMNodeMap());
    if (map.contains(node)) {
        map.forgetOnly(node);
        node->deref();
    } else
        handleWeakObjectInOwningThread<Node>(ThreadSpecificDOMData::DOMNodeMap, V8ClassIndex::NODE, node);
}

static void derefObject(V8ClassIndex::V8WrapperType type, void* domObject)
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

static void derefDelayedObjects()
{
    WTF::MutexLocker locker(domDataListMutex());

    getThreadSpecificDOMData().setDelayedProcessingScheduled(false);

    ThreadSpecificDOMData::DelayedObjectMap& delayedObjectMap = getThreadSpecificDOMData().delayedObjectMap();
    for (ThreadSpecificDOMData::DelayedObjectMap::iterator iter(delayedObjectMap.begin()); iter != delayedObjectMap.end(); ++iter) {
        derefObject(iter->second, iter->first);
    }
    delayedObjectMap.clear();
}

static void derefDelayedObjectsInCurrentThread(void*)
{
    derefDelayedObjects();
}

template<typename T>
static void removeObjectsFromWrapperMap(DOMWrapperMap<T>& domMap)
{
    for (typename WTF::HashMap<T*, v8::Object*>::iterator iter(domMap.impl().begin()); iter != domMap.impl().end(); ++iter) {
        T* domObject = static_cast<T*>(iter->first);
        v8::Persistent<v8::Object> v8Object(iter->second);

        V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(v8Object));

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
    derefDelayedObjects();

    // Remove all DOM nodes.
    removeObjectsFromWrapperMap<Node>(getDOMNodeMap());

    // Remove all DOM objects in the wrapper map.
    removeObjectsFromWrapperMap<void>(getDOMObjectMap());

    // Remove all active DOM objects in the wrapper map.
    removeObjectsFromWrapperMap<void>(getActiveDOMObjectMap());

#if ENABLE(SVG)
    // Remove all SVG element instances in the wrapper map.
    removeObjectsFromWrapperMap<SVGElementInstance>(getDOMSVGElementInstanceMap());

    // Remove all SVG objects with context in the wrapper map.
    removeObjectsFromWrapperMap<void>(getDOMSVGObjectWithContextMap());
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

} // namespace WebCore
