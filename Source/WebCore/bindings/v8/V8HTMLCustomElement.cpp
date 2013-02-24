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
#include "V8HTMLSpanElement.h"

namespace WebCore {

// FIXME: Each custom elements should have its own GetTemplate method so that it can be derived from different super element.
WrapperTypeInfo V8HTMLCustomElement::info = { &V8HTMLElement::GetTemplate, V8HTMLElement::derefObject, 0, V8HTMLElement::toEventTarget, 0, 0, &V8HTMLElement::info, WrapperTypeObjectPrototype };

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

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &info, impl.get(), isolate);
    if (wrapper.IsEmpty())
        return wrapper;

    // The constructor and registered lifecycle callbacks should be visible only from main world.
    // FIXME: This shouldn't be needed once each custom element has its own FunctionTemplate
    // https://bugs.webkit.org/show_bug.cgi?id=108138
    if (CustomElementHelpers::isFeatureAllowed(creationContext->CreationContext())) {
        v8::Handle<v8::Value> wrapperValue = WebCore::toV8(constructor.get(), creationContext, isolate);
        if (wrapperValue.IsEmpty() || !wrapperValue->IsObject())
            return v8::Handle<v8::Object>();
        v8::Handle<v8::Object> constructorWapper = v8::Handle<v8::Object>::Cast(wrapperValue);
        wrapper->SetPrototype(constructorWapper->Get(v8String("prototype", isolate)));
    }

    V8DOMWrapper::associateObjectWithWrapper(impl, &info, wrapper, isolate, WrapperConfiguration::Dependent);
    return wrapper;
}

} // namespace WebCore

#endif // ENABLE(CUSTOM_ELEMENTS)
