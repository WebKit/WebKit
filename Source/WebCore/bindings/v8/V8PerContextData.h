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

#ifndef V8PerContextData_h
#define V8PerContextData_h

#include "OwnHandle.h"
#include "WrapperTypeInfo.h"
#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class V8PerContextData {
public:
    static PassOwnPtr<V8PerContextData> create(v8::Handle<v8::Context> context)
    {
        return adoptPtr(new V8PerContextData(context));
    }

    ~V8PerContextData()
    {
        dispose();
    }

    bool init();

    // To create JS Wrapper objects, we create a cache of a 'boiler plate'
    // object, and then simply Clone that object each time we need a new one.
    // This is faster than going through the full object creation process.
    v8::Local<v8::Object> createWrapperFromCache(WrapperTypeInfo* type)
    {
        v8::Persistent<v8::Object> boilerplate = m_wrapperBoilerplates.get(type);
        return !boilerplate.IsEmpty() ? boilerplate->Clone() : createWrapperFromCacheSlowCase(type);
    }

    v8::Local<v8::Function> constructorForType(WrapperTypeInfo* type)
    {
        v8::Persistent<v8::Function> function = m_constructorMap.get(type);
        if (!function.IsEmpty())
            return v8::Local<v8::Function>::New(function);
        return constructorForTypeSlowCase(type);
    }

private:
    explicit V8PerContextData(v8::Handle<v8::Context> context)
        : m_context(context)
    {
    }

    void dispose();

    v8::Local<v8::Object> createWrapperFromCacheSlowCase(WrapperTypeInfo*);
    v8::Local<v8::Function> constructorForTypeSlowCase(WrapperTypeInfo*);

    // For each possible type of wrapper, we keep a boilerplate object.
    // The boilerplate is used to create additional wrappers of the same type.
    typedef WTF::HashMap<WrapperTypeInfo*, v8::Persistent<v8::Object> > WrapperBoilerplateMap;
    WrapperBoilerplateMap m_wrapperBoilerplates;

    typedef WTF::HashMap<WrapperTypeInfo*, v8::Persistent<v8::Function> > ConstructorMap;
    ConstructorMap m_constructorMap;

    v8::Handle<v8::Context> m_context;
    OwnHandle<v8::Value> m_errorPrototype;
    OwnHandle<v8::Value> m_objectPrototype;
};

} // namespace WebCore

#endif // V8PerContextData_h
