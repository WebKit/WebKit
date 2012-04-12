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

#include "config.h"
#include "V8BindingPerContextData.h"

#include "SafeAllocation.h"

namespace WebCore {

void V8BindingPerContextData::dispose()
{
    {
        WrapperBoilerplateMap::iterator it = m_wrapperBoilerplates.begin();
        for (; it != m_wrapperBoilerplates.end(); ++it) {
            v8::Persistent<v8::Object> wrapper = it->second;
            wrapper.Dispose();
        }
        m_wrapperBoilerplates.clear();
    }

    {
        ConstructorMap::iterator it = m_constructorMap.begin();
        for (; it != m_constructorMap.end(); ++it) {
            v8::Persistent<v8::Function> wrapper = it->second;
            wrapper.Dispose();
        }
        m_constructorMap.clear();
    }
}

bool V8BindingPerContextData::init()
{
    ASSERT(m_objectPrototype.get().IsEmpty());

    v8::Handle<v8::String> objectString = v8::String::New("Object");
    v8::Handle<v8::String> prototypeString = v8::String::New("prototype");
    if (objectString.IsEmpty() || prototypeString.IsEmpty())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(m_context->Global()->Get(objectString));
    if (object.IsEmpty())
        return false;
    v8::Handle<v8::Value> objectPrototype = object->Get(prototypeString);
    if (objectPrototype.IsEmpty())
        return false;

    m_objectPrototype.set(objectPrototype);
    return true;
}

v8::Local<v8::Object> V8BindingPerContextData::createWrapperFromCacheSlowCase(WrapperTypeInfo* type)
{
    ASSERT(!m_objectPrototype.get().IsEmpty());

    v8::Context::Scope scope(m_context);
    v8::Local<v8::Function> function = constructorForType(type);
    v8::Local<v8::Object> instance = SafeAllocation::newInstance(function);
    if (!instance.IsEmpty()) {
        m_wrapperBoilerplates.set(type, v8::Persistent<v8::Object>::New(instance));
        return instance->Clone();
    }
    return v8::Local<v8::Object>();
}

v8::Local<v8::Function> V8BindingPerContextData::constructorForTypeSlowCase(WrapperTypeInfo* type)
{
    ASSERT(!m_objectPrototype.get().IsEmpty());

    v8::Context::Scope scope(m_context);
    v8::Handle<v8::FunctionTemplate> functionTemplate = type->getTemplate();
    // Getting the function might fail if we're running out of stack or memory.
    v8::TryCatch tryCatch;
    v8::Local<v8::Function> function = functionTemplate->GetFunction();
    if (function.IsEmpty())
        return v8::Local<v8::Function>();

    function->SetPrototype(m_objectPrototype.get());

    m_constructorMap.set(type, v8::Persistent<v8::Function>::New(function));

    return function;
}

} // namespace WebCore
