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
#include "V8DOMWrapper.h"

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
        virtual ~DOMData();

        static DOMData* getCurrent();
        virtual DOMDataStore& getStore() = 0;

        template<typename T>
        static void handleWeakObject(DOMDataStore::DOMWrapperMapType, v8::Persistent<v8::Object>, T* domObject);

        template<typename T>
        static void removeObjectsFromWrapperMap(AbstractWeakReferenceMap<T, v8::Object>& domMap);

        ThreadIdentifier owningThread() const { return m_owningThread; }

    private:
        static void derefObject(WrapperTypeInfo* type, void* domObject);

        template<typename T>
        class WrapperMapObjectRemover : public WeakReferenceMap<T, v8::Object>::Visitor {
        public:
            virtual void visitDOMWrapper(T* domObject, v8::Persistent<v8::Object> v8Object)
            {
                WrapperTypeInfo* type = V8DOMWrapper::domWrapperType(v8Object);
                derefObject(type, domObject);
                v8Object.Dispose();
            }
        };

        ThreadIdentifier m_owningThread;
    };

    template<typename T>
    void DOMData::handleWeakObject(DOMDataStore::DOMWrapperMapType mapType, v8::Persistent<v8::Object> v8Object, T* domObject)
    {
        DOMDataList& list = DOMDataStore::allStores();
        for (size_t i = 0; i < list.size(); ++i) {
            DOMDataStore* store = list[i];
            ASSERT(store->domData()->owningThread() == WTF::currentThread());

            DOMWrapperMap<T>* domMap = static_cast<DOMWrapperMap<T>*>(store->getDOMWrapperMap(mapType));
            if (domMap->removeIfPresent(domObject, v8Object))
                store->domData()->derefObject(V8DOMWrapper::domWrapperType(v8Object), domObject);
        }
    }

    template<typename T>
    void DOMData::removeObjectsFromWrapperMap(AbstractWeakReferenceMap<T, v8::Object>& domMap)
    {
        WrapperMapObjectRemover<T> remover;
        domMap.visit(&remover);
        domMap.clear();
    }

} // namespace WebCore

#endif // DOMData_h
