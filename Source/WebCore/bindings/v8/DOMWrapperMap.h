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

#include "V8Utilities.h"
#include "WebCoreMemoryInstrumentation.h"
#include "WrapperTypeInfo.h"
#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/MemoryInstrumentationHashMap.h>

namespace WebCore {

template<class KeyType>
class DOMWrapperMap {
public:
    typedef HashMap<KeyType*, v8::Persistent<v8::Object> > MapType;

    explicit DOMWrapperMap(v8::Isolate* isolate)
        : m_isolate(isolate)
    {
    }

    v8::Handle<v8::Object> get(KeyType* key)
    {
        return m_map.get(key);
    }

    void set(KeyType* key, v8::Handle<v8::Object> wrapper, const WrapperConfiguration& configuration)
    {
        ASSERT(!m_map.contains(key));
        ASSERT(static_cast<KeyType*>(toNative(wrapper)) == key);
        v8::Persistent<v8::Object> persistent = v8::Persistent<v8::Object>::New(m_isolate, wrapper);
        configuration.configureWrapper(persistent, m_isolate);
        WeakHandleListener<DOMWrapperMap<KeyType> >::makeWeak(m_isolate, persistent, this);
        m_map.set(key, persistent);
    }

    void clear()
    {
        for (typename MapType::iterator it = m_map.begin(); it != m_map.end(); ++it) {
            v8::Persistent<v8::Object> wrapper = it->value;
            toWrapperTypeInfo(wrapper)->derefObject(it->key);
            wrapper.Dispose(m_isolate);
            wrapper.Clear();
        }
        m_map.clear();
    }

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Binding);
        info.addMember(m_map, "map");
    }

    void removeAndDispose(KeyType* key, v8::Handle<v8::Object> value, v8::Isolate* isolate)
    {
        v8::Persistent<v8::Object> wrapper(*value);
        typename MapType::iterator it = m_map.find(key);
        ASSERT(it != m_map.end());
        ASSERT(it->value == wrapper);
        m_map.remove(it);
        wrapper.Dispose(isolate);
        value.Clear();
    }

private:
    v8::Isolate* m_isolate;
    MapType m_map;
};

} // namespace WebCore

#endif // DOMWrapperMap_h
