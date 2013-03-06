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

#if ENABLE(CUSTOM_ELEMENTS)

#include "V8HTMLCustomElement.h"

#include "CustomElementHelpers.h"
#include "CustomElementRegistry.h"
#include "HTMLElement.h"
#include "V8CustomElementConstructor.h"
#include "V8HTMLUnknownElement.h"

namespace WebCore {

static WrapperTypeInfo* findWrapperTypeOf(v8::Handle<v8::Value> chain)
{
    while (!chain.IsEmpty() && chain->IsObject()) {
        v8::Handle<v8::Object> chainObject = v8::Handle<v8::Object>::Cast(chain);
        if (v8PrototypeInternalFieldcount == chainObject->InternalFieldCount())
            return reinterpret_cast<WrapperTypeInfo*>(chainObject->GetAlignedPointerFromInternalField(v8PrototypeTypeIndex));
        chain = chainObject->GetPrototype();
    }

    return 0;
}

v8::Handle<v8::Object> V8HTMLCustomElement::createWrapper(PassRefPtr<HTMLElement> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);

    RefPtr<CustomElementConstructor> constructor = CustomElementRegistry::constructorOf(impl.get());
    if (!constructor) {
        v8::Handle<v8::Value> wrapperValue = WebCore::toV8(toHTMLUnknownElement(impl.get()), creationContext, isolate);
        if (!wrapperValue.IsEmpty() && wrapperValue->IsObject())
            return v8::Handle<v8::Object>::Cast(wrapperValue);
        return v8::Handle<v8::Object>();
    }

    // The constructor and registered lifecycle callbacks should be visible only from main world.
    // FIXME: This shouldn't be needed once each custom element has its own FunctionTemplate
    // https://bugs.webkit.org/show_bug.cgi?id=108138
    if (!CustomElementHelpers::isFeatureAllowed(creationContext->CreationContext())) {
        v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &V8HTMLElement::info, impl.get(), isolate);
        if (!wrapper.IsEmpty())
            V8DOMWrapper::associateObjectWithWrapper(impl, &V8HTMLElement::info, wrapper, isolate, WrapperConfiguration::Dependent);
        return wrapper;
    }

    v8::Handle<v8::Value> constructorValue = WebCore::toV8(constructor.get(), creationContext, isolate);
    if (constructorValue.IsEmpty() || !constructorValue->IsObject())
        return v8::Handle<v8::Object>();
    v8::Handle<v8::Object> constructorWapper = v8::Handle<v8::Object>::Cast(constructorValue);
    v8::Handle<v8::Object> prototype = v8::Handle<v8::Object>::Cast(constructorWapper->Get(v8::String::NewSymbol("prototype")));
    WrapperTypeInfo* typeInfo = findWrapperTypeOf(prototype);
    if (!typeInfo)
        return v8::Handle<v8::Object>();

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, typeInfo, impl.get(), isolate);
    if (wrapper.IsEmpty())
        return v8::Handle<v8::Object>();

    wrapper->SetPrototype(prototype);
    V8DOMWrapper::associateObjectWithWrapper(impl, typeInfo, wrapper, isolate, WrapperConfiguration::Dependent);
    return wrapper;
}

} // namespace WebCore

#endif // ENABLE(CUSTOM_ELEMENTS)
