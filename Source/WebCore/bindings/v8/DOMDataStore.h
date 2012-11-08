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

    static DOMDataStore* current(v8::Isolate*);

    template<typename T>
    inline v8::Handle<v8::Object> get(T* object) const
    {
        if (wrapperIsStoredInObject(object))
            return getWrapperFromObject(object);
        return m_domObjectMap->get(object);
    }

    template<typename T>
    inline void set(T* object, v8::Persistent<v8::Object> wrapper)
    {
        if (setWrapperInObject(object, wrapper))
            return;
        m_domObjectMap->set(object, wrapper);
    }

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    bool wrapperIsStoredInObject(void*) const { return false; }
    bool wrapperIsStoredInObject(Node*) const { return m_type == MainWorld; }

    v8::Handle<v8::Object> getWrapperFromObject(void*) const
    {
        ASSERT_NOT_REACHED();
        return v8::Handle<v8::Object>();
    }

    v8::Handle<v8::Object> getWrapperFromObject(Node* object) const
    {
        ASSERT(m_type == MainWorld);
        return object->wrapper();
    }

    bool setWrapperInObject(void*, v8::Persistent<v8::Object>) { return false; }
    bool setWrapperInObject(Node* object, v8::Persistent<v8::Object> wrapper)
    {
        if (m_type != MainWorld)
            return false;
        ASSERT(object->wrapper().IsEmpty());
        object->setWrapper(wrapper);
        wrapper.MakeWeak(object, weakCallback);
        return true;
    }

    static void weakCallback(v8::Persistent<v8::Value>, void* context);

    Type m_type;
    OwnPtr<DOMWrapperMap<void> > m_domObjectMap;
};

} // namespace WebCore

#endif // DOMDataStore_h
