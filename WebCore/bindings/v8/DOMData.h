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

#ifndef DOMData_h
#define DOMData_h

#include "DOMDataStore.h"

namespace WebCore {

    // DOMData
    //
    // DOMData represents the all the DOM wrappers for a given thread.  In
    // particular, DOMData holds wrappers for all the isolated worlds in the
    // thread.  The DOMData for the main thread and the DOMData for child threads
    // use different subclasses.
    //
    class DOMData : public Noncopyable {
    public:
        DOMData();

        static DOMData* getCurrent();
        static DOMData* getCurrentMainThread(); // Caller must be on the main thread.
        virtual DOMDataStore& getStore() = 0;

        template<typename T>
        static void handleWeakObject(DOMDataStore::DOMWrapperMapType, v8::Handle<v8::Object>, T* domObject);

        void forgetDelayedObject(void* object) { m_delayedObjectMap.take(object); }

        // This is to ensure that we will deref DOM objects from the owning thread,
        // not the GC thread. The helper function will be scheduled by the GC
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

        // Stores all the DOM objects that are delayed to be processed when the
        // owning thread gains control.
        DelayedObjectMap m_delayedObjectMap;

        // The flag to indicate if the task to do the delayed process has
        // already been posted.
        bool m_delayedProcessingScheduled;

        bool m_isMainThread;
        ThreadIdentifier m_owningThread;
    };

    // Called when the dead object is not in GC thread's map. Go through all
    // thread maps to find the one containing it.  Then clear the JS reference
    // and push the DOM object into the delayed queue for it to be deref-ed at
    // later time from the owning thread.
    //
    // * This is called when the GC thread is not the owning thread.
    // * This can be called on any thread that has GC running.
    // * Only one V8 instance is running at a time due to V8::Locker. So we don't need to worry about concurrency.
    //
    template<typename T>
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

} // namespace WebCore

#endif // DOMData_h
