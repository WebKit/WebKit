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

#ifndef DOMDataStore_h
#define DOMDataStore_h

#include "DOMWrapperMap.h"
#include "DOMWrapperWorld.h"
#include "Node.h"
#include "V8GCController.h"
#include "WrapperTypeInfo.h"
#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Vector.h>

namespace WebCore {

class DOMDataStore {
    WTF_MAKE_NONCOPYABLE(DOMDataStore);
public:
    explicit DOMDataStore(WrapperWorldType);
    ~DOMDataStore();

    static DOMDataStore* current(v8::Isolate*);

    template<typename T, typename HolderContainer, typename Wrappable>
    static v8::Handle<v8::Object> getWrapperFast(T* object, const HolderContainer& container, Wrappable* holder)
    {
        // What we'd really like to check here is whether we're in the
        // main world or in an isolated world. The fastest way to do that
        // is to check that there is no isolated world and the 'object'
        // is an object that can exist in the main world. The second fastest
        // way is to check whether the wrappable's wrapper is the same as
        // the holder.
        if ((!DOMWrapperWorld::isolatedWorldsExist() && isMainWorldObject(object)) || holderContainsWrapper(container, holder)) {
            if (mainWorldWrapperIsStoredInObject(object))
                return getWrapperFromObject(object);
            return mainWorldStore()->m_wrapperMap.get(object);
        }
        return current(container.GetIsolate())->get(object);
    }

    template<typename T>
    static v8::Handle<v8::Object> getWrapper(T* object, v8::Isolate* isolate)
    {
        if (mainWorldWrapperIsStoredInObject(object) && isMainWorldObject(object)) {
            if (LIKELY(!DOMWrapperWorld::isolatedWorldsExist()))
                return getWrapperFromObject(object);
        }
        return current(isolate)->get(object);
    }

    template<typename T>
    static void setWrapper(T* object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        if (mainWorldWrapperIsStoredInObject(object) && isMainWorldObject(object)) {
            if (LIKELY(!DOMWrapperWorld::isolatedWorldsExist())) {
                setWrapperInObject(object, wrapper, isolate, configuration);
                return;
            }
        }
        return current(isolate)->set(object, wrapper, isolate, configuration);
    }

    template<typename T>
    inline v8::Handle<v8::Object> get(T* object)
    {
        if (mainWorldWrapperIsStoredInObject(object) && m_type == MainWorld)
            return getWrapperFromObject(object);
        return m_wrapperMap.get(object);
    }

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    template<typename T>
    inline void set(T* object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        ASSERT(!!object);
        ASSERT(!wrapper.IsEmpty());
        if (mainWorldWrapperIsStoredInObject(object) && m_type == MainWorld) {
            setWrapperInObject(object, wrapper, isolate, configuration);
            return;
        }
        m_wrapperMap.set(object, wrapper, configuration);
    }

    static DOMDataStore* mainWorldStore();

    static bool mainWorldWrapperIsStoredInObject(void*) { return false; }
    static bool mainWorldWrapperIsStoredInObject(ScriptWrappable*) { return true; }

    static bool isMainWorldObject(void*) { return false; }
    static bool isMainWorldObject(Node*) { return true; }

    template<typename HolderContainer>
    static bool holderContainsWrapper(const HolderContainer&, void*)
    {
        return false;
    }
    template<typename HolderContainer>
    static bool holderContainsWrapper(const HolderContainer& container, ScriptWrappable* wrappable)
    {
        // Verify our assumptions about the main world.
        ASSERT(wrappable->wrapper().IsEmpty() || container.Holder() != wrappable->wrapper() || current(v8::Isolate::GetCurrent())->m_type == MainWorld);
        return container.Holder() == wrappable->wrapper();
    }

    static v8::Handle<v8::Object> getWrapperFromObject(void*)
    {
        ASSERT_NOT_REACHED();
        return v8::Handle<v8::Object>();
    }
    static v8::Handle<v8::Object> getWrapperFromObject(ScriptWrappable* object)
    {
        return object->wrapper();
    }

    static void setWrapperInObject(void*, v8::Handle<v8::Object>, v8::Isolate*, const WrapperConfiguration&)
    {
        ASSERT_NOT_REACHED();
    }
    static void setWrapperInObject(ScriptWrappable* object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        object->setWrapper(wrapper, isolate, configuration);
    }

    WrapperWorldType m_type;
    DOMWrapperMap<void> m_wrapperMap;
};

} // namespace WebCore

#endif // DOMDataStore_h
