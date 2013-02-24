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

#ifndef V8HTMLCustomElement_h
#define V8HTMLCustomElement_h

#include "V8Binding.h"
#include "V8DOMWrapper.h"
#include "V8HTMLElement.h"
#include "V8HTMLUnknownElement.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

class HTMLElement;

#if ENABLE(CUSTOM_ELEMENTS)

class V8HTMLCustomElement {
public:
    static WrapperTypeInfo info;
    static v8::Handle<v8::Value> toV8(HTMLElement*, v8::Handle<v8::Object> creationContext = v8::Handle<v8::Object>(), v8::Isolate* = 0);
    static v8::Handle<v8::Object> wrap(HTMLElement*, v8::Handle<v8::Object> creationContext, v8::Isolate*);

private:
    static v8::Handle<v8::Object> createWrapper(PassRefPtr<HTMLElement>, v8::Handle<v8::Object>, v8::Isolate*);
};

inline v8::Handle<v8::Value> V8HTMLCustomElement::toV8(HTMLElement* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (UNLIKELY(!impl))
        return v8NullWithCheck(isolate);
    v8::Handle<v8::Object> wrapper = DOMDataStore::getWrapper(impl, isolate);
    if (!wrapper.IsEmpty())
        return wrapper;
    return V8HTMLCustomElement::wrap(impl, creationContext, isolate);
}

inline v8::Handle<v8::Object> V8HTMLCustomElement::wrap(HTMLElement* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    ASSERT(DOMDataStore::getWrapper(impl, isolate).IsEmpty());
    return V8HTMLCustomElement::createWrapper(impl, creationContext, isolate);
}

#else // ENABLE(CUSTOM_ELEMENTS)

class V8HTMLCustomElement {
public:
    static v8::Handle<v8::Object> wrap(HTMLElement*, v8::Handle<v8::Object> creationContext = v8::Handle<v8::Object>(), v8::Isolate* = 0);
};

inline v8::Handle<v8::Object> HTMLCustomElement::wrap(HTMLElement* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return wrap(toHTMLUnknownElement(impl), creationContext, isolate);
}

#endif // ENABLE(CUSTOM_ELEMENTS)

} // namespace WebCore

#endif // V8HTMLCustomElement_h
