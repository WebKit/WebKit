/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#include "Element.h"

#include "Attr.h"
#include "CSSHelper.h"
#include "Document.h"
#include "EventListener.h"
#include "ExceptionCode.h"
#include "HTMLFrameElementBase.h"
#include "HTMLNames.h"
#include "Node.h"

#include "V8Attr.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8CustomEventListener.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {

CALLBACK_FUNC_DECL(ElementSetAttribute)
{
    INC_STATS("DOM.Element.setAttribute()");
    Element* element = V8DOMWrapper::convertDOMWrapperToNode<Element>(args.Holder());
    String name = toWebCoreString(args[0]);
    String value = toWebCoreString(args[1]);

    if (!allowSettingSrcToJavascriptURL(element, name, value))
        return v8::Undefined();

    ExceptionCode ec = 0;
    element->setAttribute(name, value, ec);
    if (ec)
        return throwError(ec);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(ElementSetAttributeNode)
{
    INC_STATS("DOM.Element.setAttributeNode()");
    if (!V8Attr::HasInstance(args[0]))
        return throwError(TYPE_MISMATCH_ERR);

    Attr* newAttr = V8DOMWrapper::convertDOMWrapperToNode<Attr>(args[0]);
    Element* element = V8DOMWrapper::convertDOMWrapperToNode<Element>(args.Holder());

    if (!allowSettingSrcToJavascriptURL(element, newAttr->name(), newAttr->value()))
        return v8::Undefined();

    ExceptionCode ec = 0;
    RefPtr<Attr> result = element->setAttributeNode(newAttr, ec);
    if (ec)
        throwError(ec);

    return V8DOMWrapper::convertNodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(ElementSetAttributeNS)
{
    INC_STATS("DOM.Element.setAttributeNS()");
    Element* element = V8DOMWrapper::convertDOMWrapperToNode<Element>(args.Holder());
    String namespaceURI = toWebCoreStringWithNullCheck(args[0]);
    String qualifiedName = toWebCoreString(args[1]);
    String value = toWebCoreString(args[2]);

    if (!allowSettingSrcToJavascriptURL(element, qualifiedName, value))
        return v8::Undefined();

    ExceptionCode ec = 0;
    element->setAttributeNS(namespaceURI, qualifiedName, value, ec);
    if (ec)
        throwError(ec);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(ElementSetAttributeNodeNS)
{
    INC_STATS("DOM.Element.setAttributeNodeNS()");
    if (!V8Attr::HasInstance(args[0]))
        return throwError(TYPE_MISMATCH_ERR);

    Attr* newAttr = V8DOMWrapper::convertDOMWrapperToNode<Attr>(args[0]);
    Element* element = V8DOMWrapper::convertDOMWrapperToNode<Element>(args.Holder());

    if (!allowSettingSrcToJavascriptURL(element, newAttr->name(), newAttr->value()))
        return v8::Undefined();

    ExceptionCode ec = 0;
    RefPtr<Attr> result = element->setAttributeNodeNS(newAttr, ec);
    if (ec)
        throwError(ec);

    return V8DOMWrapper::convertNodeToV8Object(result.get());
}

static inline String toEventType(v8::Local<v8::String> value)
{
    String key = toWebCoreString(value);
    ASSERT(key.startsWith("on"));
    return key.substring(2);
}

ACCESSOR_SETTER(ElementEventHandler)
{
    Node* node = V8DOMWrapper::convertDOMWrapperToNode<Node>(info.Holder());

    String eventType = toEventType(name);

    // Set handler if the value is a function.  Otherwise, clear the
    // event handler.
    if (value->IsFunction()) {
        V8Proxy* proxy = V8Proxy::retrieve(node->document()->frame());
        // the document might be created using createDocument,
        // which does not have a frame, use the active frame
        if (!proxy)
            proxy = V8Proxy::retrieve(V8Proxy::retrieveFrameForEnteredContext());
        if (!proxy)
            return;

        if (RefPtr<EventListener> listener = proxy->findOrCreateV8EventListener(value, true))
            node->setAttributeEventListener(eventType, listener);
    } else
        node->clearAttributeEventListener(eventType);
}

ACCESSOR_GETTER(ElementEventHandler)
{
    Node* node = V8DOMWrapper::convertDOMWrapperToNode<Node>(info.Holder());

    EventListener* listener = node->getAttributeEventListener(toEventType(name));
    return V8DOMWrapper::convertEventListenerToV8Object(listener);
}

} // namespace WebCore
