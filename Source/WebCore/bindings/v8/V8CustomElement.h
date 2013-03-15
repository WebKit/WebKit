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

#ifndef V8CustomElement_h
#define V8CustomElement_h

#include "CustomElementConstructor.h"
#include "CustomElementRegistry.h"
#include "Document.h"
#include "V8Binding.h"
#include "V8DOMWrapper.h"
#include "V8HTMLElement.h"
#include "V8HTMLUnknownElement.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

class Element;
class CustomElementConstructor;

#if ENABLE(CUSTOM_ELEMENTS)

class V8CustomElement {
public:
    //
    // You can just use toV8(Node*) to get correct wrapper objects, even for custom elements.
    // Then generated ElementWrapperFactories call V8CustomElement::wrap() with proper CustomElementConstructor instances
    // accordingly.
    //
    static v8::Handle<v8::Object> wrap(Element*, v8::Handle<v8::Object> creationContext, PassRefPtr<CustomElementConstructor>, v8::Isolate*);
    static PassRefPtr<CustomElementConstructor> constructorOf(Element*);

private:
    static v8::Handle<v8::Object> createWrapper(PassRefPtr<Element>, v8::Handle<v8::Object>, PassRefPtr<CustomElementConstructor>, v8::Isolate*);
};

inline v8::Handle<v8::Object> V8CustomElement::wrap(Element* impl, v8::Handle<v8::Object> creationContext, PassRefPtr<CustomElementConstructor> constructor, v8::Isolate* isolate)
{
    ASSERT(impl);
    ASSERT(DOMDataStore::getWrapper(impl, isolate).IsEmpty());
    return V8CustomElement::createWrapper(impl, creationContext, constructor, isolate);
}

inline PassRefPtr<CustomElementConstructor> V8CustomElement::constructorOf(Element* element)
{
    if (CustomElementRegistry* registry = element->document()->registry())
        return registry->findFor(element);
    return 0;
}

#endif // ENABLE(CUSTOM_ELEMENTS)

} // namespace WebCore

#endif // V8CustomElement_h
