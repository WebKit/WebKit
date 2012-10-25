/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef DOMWrapperMap_h
#define DOMWrapperMap_h

#include "WebCoreMemoryInstrumentation.h"
#include "WrapperTypeInfo.h"
#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/MemoryInstrumentationHashMap.h>

namespace WebCore {

class DOMDataStore;

template<class KeyType>
class DOMWrapperVisitor {
public:
    virtual void visitDOMWrapper(DOMDataStore*, KeyType*, v8::Persistent<v8::Object> wrapper) = 0;
    virtual ~DOMWrapperVisitor() { }
};

template<class KeyType>
class DOMWrapperMap {
public:
    virtual ~DOMWrapperMap() { }

    virtual v8::Persistent<v8::Object> get(KeyType*) = 0;
    virtual void set(KeyType*, v8::Persistent<v8::Object>) = 0;
    virtual void visit(DOMDataStore*, DOMWrapperVisitor<KeyType>*) = 0;
    virtual void clear() = 0;

    virtual void reportMemoryUsage(MemoryObjectInfo*) const = 0;
};

template<class KeyType>
class DOMWrapperHashMap : public DOMWrapperMap<KeyType> {
public:
    typedef HashMap<KeyType*, v8::Persistent<v8::Object> > MapType;

    explicit DOMWrapperHashMap(v8::WeakReferenceCallback callback = &defaultWeakCallback)
        : m_callback(callback)
    {
    }

    virtual v8::Persistent<v8::Object> get(KeyType* key) OVERRIDE
    {
        return m_map.get(key);
    }

    virtual void set(KeyType* key, v8::Persistent<v8::Object> wrapper) OVERRIDE
    {
        ASSERT(!m_map.contains(key));
        ASSERT(static_cast<KeyType*>(toNative(wrapper)) == key);
        wrapper.MakeWeak(this, m_callback);
        m_map.set(key, wrapper);
    }

    virtual void visit(DOMDataStore* store, DOMWrapperVisitor<KeyType>* visitor) OVERRIDE
    {
        for (typename MapType::iterator it = m_map.begin(); it != m_map.end(); ++it)
            visitor->visitDOMWrapper(store, it->key, it->value);
    }

    virtual void clear() OVERRIDE
    {
        for (typename MapType::iterator it = m_map.begin(); it != m_map.end(); ++it) {
            v8::Persistent<v8::Object> wrapper = it->value;
            toWrapperTypeInfo(wrapper)->derefObject(it->key);
            wrapper.Dispose();
        }
        m_map.clear();
    }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const OVERRIDE
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Binding);
        info.addMember(m_map);
    }

    virtual void remove(KeyType* key, v8::Persistent<v8::Object> wrapper)
    {
        typename MapType::iterator it = m_map.find(key);
        ASSERT(it != m_map.end());
        ASSERT(it->value == wrapper);
        m_map.remove(it);
    }

private:
    static void defaultWeakCallback(v8::Persistent<v8::Value> value, void* context)
    {
        DOMWrapperHashMap<KeyType>* map = static_cast<DOMWrapperHashMap<KeyType>*>(context);
        ASSERT(value->IsObject());
        v8::Persistent<v8::Object> wrapper = v8::Persistent<v8::Object>::Cast(value);
        WrapperTypeInfo* type = toWrapperTypeInfo(wrapper);
        KeyType* key = static_cast<KeyType*>(toNative(wrapper));

        map->remove(key, wrapper);
        wrapper.Dispose();
        type->derefObject(key);
    }

    v8::WeakReferenceCallback m_callback;
    MapType m_map;
};

} // namespace WebCore

#endif // DOMWrapperMap_h
