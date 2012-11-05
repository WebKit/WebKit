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
#include "Node.h"
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
    enum Type {
        MainWorld,
        IsolatedWorld,
        Worker,
    };

    explicit DOMDataStore(Type);
    ~DOMDataStore();

    static DOMDataStore* current(v8::Isolate* = 0);

    // Callers need to be careful when calling get() and set() that they use
    // the correct compile-time type. We use the compile-time type to decide
    // whether the wrapper is stored in a hashmap or whether the wrapper is
    // stored inline in the object. If callers use the wrong type, we'll either
    // store the wrapper in the wrong place or look for it in the wrong place.

    template<typename T>
    inline v8::Handle<v8::Object> get(T* object) const
    {
        if (wrapperIsStoredInObject(object))
            return getWrapperFromObject(object);
        return m_wrapperMap.get(object);
    }

    // Takes ownership of |object| and |wrapper|.
    template<typename T>
    inline void set(T* object, v8::Persistent<v8::Object> wrapper)
    {
        if (setWrapperInObject(object, wrapper))
            return;
        m_wrapperMap.set(object, wrapper);
    }

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    inline bool wrapperIsStoredInObject(void*) const { return false; }
    inline bool wrapperIsStoredInObject(ScriptWrappable*) const { return m_type == MainWorld; }

    inline v8::Handle<v8::Object> getWrapperFromObject(void* object) const { ASSERT_NOT_REACHED(); return v8::Handle<v8::Object>(); }
    inline v8::Handle<v8::Object> getWrapperFromObject(ScriptWrappable* key) const
    {
        ASSERT(m_type == MainWorld);
        return key->wrapper();
    }

    inline bool setWrapperInObject(void*, v8::Persistent<v8::Object>) { return false; }
    inline bool setWrapperInObject(ScriptWrappable* key, v8::Persistent<v8::Object> wrapper)
    {
        if (m_type != MainWorld)
            return false;
        ASSERT(key->wrapper().IsEmpty());
        ASSERT(m_wrapperMap.get(toNative(wrapper)).IsEmpty());
        key->setWrapper(wrapper);
        wrapper.MakeWeak(key, weakCallback);
        return true;
    }

    static void weakCallback(v8::Persistent<v8::Value>, void* context);

    Type m_type;
    DOMWrapperHashMap<void> m_wrapperMap;
};

} // namespace WebCore

#endif // DOMDataStore_h
