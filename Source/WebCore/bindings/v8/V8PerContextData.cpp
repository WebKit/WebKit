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
#include "V8PerContextData.h"

#include "V8Binding.h"
#include "V8ObjectConstructor.h"

namespace WebCore {

void V8PerContextData::dispose()
{
    v8::HandleScope handleScope;
    v8::Isolate* isolate = m_context->GetIsolate();
    m_context->SetAlignedPointerInEmbedderData(v8ContextPerContextDataIndex, 0);

    {
        WrapperBoilerplateMap::iterator it = m_wrapperBoilerplates.begin();
        for (; it != m_wrapperBoilerplates.end(); ++it) {
            v8::Persistent<v8::Object> wrapper = it->value;
            wrapper.Dispose(isolate);
            wrapper.Clear();
        }
        m_wrapperBoilerplates.clear();
    }

    {
        ConstructorMap::iterator it = m_constructorMap.begin();
        for (; it != m_constructorMap.end(); ++it) {
            v8::Persistent<v8::Function> wrapper = it->value;
            wrapper.Dispose(isolate);
            wrapper.Clear();
        }
        m_constructorMap.clear();
    }
}

#define V8_STORE_PRIMORDIAL(name, Name) \
{ \
    ASSERT(m_##name##Prototype.get().IsEmpty()); \
    v8::Handle<v8::String> symbol = v8::String::NewSymbol(#Name); \
    if (symbol.IsEmpty()) \
        return false; \
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(m_context->Global()->Get(symbol)); \
    if (object.IsEmpty()) \
        return false; \
    v8::Handle<v8::Value> prototypeValue = object->Get(prototypeString); \
    if (prototypeValue.IsEmpty()) \
        return false; \
    m_##name##Prototype.set(prototypeValue); \
}

bool V8PerContextData::init()
{
    m_context->SetAlignedPointerInEmbedderData(v8ContextPerContextDataIndex, this);

    v8::Handle<v8::String> prototypeString = v8::String::NewSymbol("prototype");
    if (prototypeString.IsEmpty())
        return false;

    V8_STORE_PRIMORDIAL(error, Error);
    V8_STORE_PRIMORDIAL(object, Object);

    return true;
}

#undef V8_STORE_PRIMORDIAL

v8::Local<v8::Object> V8PerContextData::createWrapperFromCacheSlowCase(WrapperTypeInfo* type)
{
    ASSERT(!m_errorPrototype.isEmpty());
    ASSERT(!m_objectPrototype.isEmpty());

    v8::Context::Scope scope(m_context);
    v8::Local<v8::Function> function = constructorForType(type);
    v8::Local<v8::Object> instance = V8ObjectConstructor::newInstance(function);
    if (!instance.IsEmpty()) {
        m_wrapperBoilerplates.set(type, v8::Persistent<v8::Object>::New(m_context->GetIsolate(), instance));
        return instance->Clone();
    }
    return v8::Local<v8::Object>();
}

v8::Local<v8::Function> V8PerContextData::constructorForTypeSlowCase(WrapperTypeInfo* type)
{
    ASSERT(!m_errorPrototype.isEmpty());
    ASSERT(!m_objectPrototype.isEmpty());

    v8::Context::Scope scope(m_context);
    v8::Handle<v8::FunctionTemplate> functionTemplate = type->getTemplate(m_context->GetIsolate(), worldType(m_context->GetIsolate()));
    // Getting the function might fail if we're running out of stack or memory.
    v8::TryCatch tryCatch;
    v8::Local<v8::Function> function = functionTemplate->GetFunction();
    if (function.IsEmpty())
        return v8::Local<v8::Function>();

    function->SetPrototype(m_objectPrototype.get());
    v8::Local<v8::Value> prototypeValue = function->Get(v8::String::NewSymbol("prototype"));
    if (!prototypeValue.IsEmpty() && prototypeValue->IsObject()) {
        v8::Local<v8::Object> prototypeObject = v8::Local<v8::Object>::Cast(prototypeValue);
        type->installPerContextPrototypeProperties(prototypeObject, m_context->GetIsolate());
        if (type->wrapperTypePrototype == WrapperTypeErrorPrototype)
            prototypeObject->SetPrototype(m_errorPrototype.get());
    }

    m_constructorMap.set(type, v8::Persistent<v8::Function>::New(m_context->GetIsolate(), function));

    return function;
}

} // namespace WebCore
