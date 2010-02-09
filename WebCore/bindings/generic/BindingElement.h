/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef BindingElement_h
#define BindingElement_h

#include "Attr.h"
#include "BindingSecurity.h"
#include "Element.h"
#include "ExceptionCode.h"

#include <wtf/RefPtr.h>

namespace WebCore {

template <class Binding>
class BindingElement {
public:
    static void setAttribute(State<Binding>*, Element*, const AtomicString&, const AtomicString&, ExceptionCode&);
    static RefPtr<Attr> setAttributeNode(State<Binding>*, Element*, Attr*, ExceptionCode&);
    static void setAttributeNS(State<Binding>*, Element*, const AtomicString&, const AtomicString&, const AtomicString&, ExceptionCode&);
    static RefPtr<Attr> setAttributeNodeNS(State<Binding>*, Element*, Attr*, ExceptionCode&);
};

// Implementations of templated methods must be in this file.

template <class Binding>
void BindingElement<Binding>::setAttribute(State<Binding>* state, Element* element, const AtomicString& name, const AtomicString& value, ExceptionCode& ec)
{
    ASSERT(element);

    if (!BindingSecurity<Binding>::allowSettingSrcToJavascriptURL(state, element, name, value))
        return;

    element->setAttribute(name, value, ec);
}

template <class Binding>
RefPtr<Attr> BindingElement<Binding>::setAttributeNode(State<Binding>* state, Element* element, Attr* newAttr, ExceptionCode& ec)
{
    ASSERT(element);
    ASSERT(newAttr);

    if (!BindingSecurity<Binding>::allowSettingSrcToJavascriptURL(state, element, newAttr->name(), newAttr->value()))
        return 0;

    return element->setAttributeNode(newAttr, ec);
}

template <class Binding>
void BindingElement<Binding>::setAttributeNS(State<Binding>* state, Element* element, const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value, ExceptionCode& ec)
{
    ASSERT(element);

    if (!BindingSecurity<Binding>::allowSettingSrcToJavascriptURL(state, element, qualifiedName, value))
        return;

    element->setAttributeNS(namespaceURI, qualifiedName, value, ec);
}

template <class Binding>
RefPtr<Attr> BindingElement<Binding>::setAttributeNodeNS(State<Binding>* state, Element* element, Attr* newAttr, ExceptionCode& ec)
{
    ASSERT(element);
    ASSERT(newAttr);

    if (!BindingSecurity<Binding>::allowSettingSrcToJavascriptURL(state, element, newAttr->name(), newAttr->value()))
        return 0;

    return element->setAttributeNodeNS(newAttr, ec);
}

} // namespace WebCore

#endif // BindingElement_h
