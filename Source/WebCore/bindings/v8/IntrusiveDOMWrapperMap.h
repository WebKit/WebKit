/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef IntrusiveDOMWrapperMap_h
#define IntrusiveDOMWrapperMap_h

#include "DOMWrapperMap.h"

namespace WebCore {

template<class KeyType>
class IntrusiveDOMWrapperMap : public DOMWrapperMap<KeyType> {
public:
    virtual v8::Persistent<v8::Object> get(KeyType* key) OVERRIDE
    {
        return key->wrapper();
    }

    virtual void set(KeyType* key, v8::Persistent<v8::Object> wrapper) OVERRIDE
    {
        ASSERT(key && key->wrapper().IsEmpty());
        key->setWrapper(wrapper);
        wrapper.MakeWeak(key, weakCallback);
    }

    virtual void clear() OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const OVERRIDE
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Binding);
        UNUSED_PARAM(info);
    }

private:
    static void weakCallback(v8::Persistent<v8::Value> value, void* context)
    {
        KeyType* key = static_cast<KeyType*>(context);
        ASSERT(value->IsObject());
        ASSERT(key->wrapper() == v8::Persistent<v8::Object>::Cast(value));

        key->clearWrapper();
        value.Dispose();
        value.Clear();
        key->deref();
    }
};

} // namespace WebCore

#endif
