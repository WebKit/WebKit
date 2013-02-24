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

#include "config.h"
#include "V8DOMWrapper.h"

#include "V8AdaptorFunction.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8HiddenPropertyName.h"
#include "V8ObjectConstructor.h"
#include "V8PerContextData.h"

namespace WebCore {

class V8WrapperInstantiationScope {
public:
    explicit V8WrapperInstantiationScope(v8::Handle<v8::Object> creationContext)
        : m_didEnterContext(false)
        , m_context(v8::Context::GetCurrent())
    {
        if (creationContext.IsEmpty())
            return;
        v8::Handle<v8::Context> contextForWrapper = creationContext->CreationContext();
        // For performance, we enter the context only if the currently running context
        // is different from the context that we are about to enter.
        if (contextForWrapper == m_context)
            return;
        m_context = v8::Local<v8::Context>::New(contextForWrapper);
        m_didEnterContext = true;
        m_context->Enter();
    }

    ~V8WrapperInstantiationScope()
    {
        if (!m_didEnterContext)
            return;
        m_context->Exit();
    }

    v8::Handle<v8::Context> context() const { return m_context; }

private:
    bool m_didEnterContext;
    v8::Handle<v8::Context> m_context;
};

v8::Local<v8::Object> V8DOMWrapper::createWrapper(v8::Handle<v8::Object> creationContext, WrapperTypeInfo* type, void* impl, v8::Isolate* isolate)
{
    V8WrapperInstantiationScope scope(creationContext);

    V8PerContextData* perContextData = V8PerContextData::from(scope.context());
    v8::Local<v8::Object> wrapper = perContextData ? perContextData->createWrapperFromCache(type) : V8ObjectConstructor::newInstance(type->getTemplate(isolate)->GetFunction());

    if (type == &V8HTMLDocument::info && !wrapper.IsEmpty())
        wrapper = V8HTMLDocument::wrapInShadowObject(wrapper, static_cast<Node*>(impl), isolate);

    return wrapper;
}

static bool hasInternalField(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;
    return v8::Handle<v8::Object>::Cast(value)->InternalFieldCount();
}

#ifndef NDEBUG
bool V8DOMWrapper::maybeDOMWrapper(v8::Handle<v8::Value> value)
{
    if (!hasInternalField(value))
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    ASSERT(object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount);

    v8::HandleScope scope;
    ASSERT(object->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex));

    return true;
}
#endif

bool V8DOMWrapper::isDOMWrapper(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(value);
    if (wrapper->InternalFieldCount() < v8DefaultWrapperInternalFieldCount)
        return false;
    ASSERT(wrapper->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex));
    ASSERT(wrapper->GetAlignedPointerFromInternalField(v8DOMWrapperTypeIndex));

    // FIXME: Add class id checks.
    return true;
}

bool V8DOMWrapper::isWrapperOfType(v8::Handle<v8::Value> value, WrapperTypeInfo* type)
{
    if (!hasInternalField(value))
        return false;

    v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(value);
    ASSERT(wrapper->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount);
    ASSERT(wrapper->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex));

    WrapperTypeInfo* typeInfo = static_cast<WrapperTypeInfo*>(wrapper->GetAlignedPointerFromInternalField(v8DOMWrapperTypeIndex));
    return typeInfo == type;
}

#if ENABLE(CUSTOM_ELEMENTS)

v8::Handle<v8::Function> V8DOMWrapper::toFunction(v8::Handle<v8::Value> object)
{
    return V8AdaptorFunction::get(v8::Handle<v8::Object>::Cast(object));
}

v8::Handle<v8::Function> V8DOMWrapper::toFunction(v8::Handle<v8::Object> object, const AtomicString& name, v8::Isolate* isolate)
{
    return V8AdaptorFunction::wrap(object, name, isolate);
}

v8::Handle<v8::Object> V8DOMWrapper::fromFunction(v8::Handle<v8::Object> object)
{
    if (!object->IsFunction())
        return object;
    return V8AdaptorFunction::unwrap(v8::Handle<v8::Function>::Cast(object));
}

#endif // ENABLE(CUSTOM_ELEMENTS)

}  // namespace WebCore
