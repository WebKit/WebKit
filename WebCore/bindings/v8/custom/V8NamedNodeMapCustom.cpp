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
#include "V8NamedNodeMap.h"

#include "NamedNodeMap.h"
#include "V8Attr.h"
#include "V8Binding.h"
#include "V8BindingState.h"
#include "V8Element.h"
#include "V8Node.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {

v8::Handle<v8::Value> V8NamedNodeMap::indexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.NamedNodeMap.IndexedPropertyGetter");
    NamedNodeMap* imp = V8NamedNodeMap::toNative(info.Holder());
    RefPtr<Node> result = imp->item(index);
    if (!result)
        return notHandledByInterceptor();

    return toV8(result.release());
}

v8::Handle<v8::Value> V8NamedNodeMap::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.NamedNodeMap.NamedPropertyGetter");
    // Search the prototype chain first.
    v8::Handle<v8::Value> value = info.Holder()->GetRealNamedPropertyInPrototypeChain(name);
    if (!value.IsEmpty())
        return value;

    // Then look for IDL defined properties on the object itself.
    if (info.Holder()->HasRealNamedCallbackProperty(name))
        return notHandledByInterceptor();

    // Finally, search the DOM.
    NamedNodeMap* imp = V8NamedNodeMap::toNative(info.Holder());
    RefPtr<Node> result = imp->getNamedItem(toWebCoreString(name));
    if (!result)
        return notHandledByInterceptor();

    return toV8(result.release());
}

v8::Handle<v8::Value> V8NamedNodeMap::setNamedItemNSCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.NamedNodeMap.setNamedItemNS");
    NamedNodeMap* imp = V8NamedNodeMap::toNative(args.Holder());
    Node* newNode = V8Node::HasInstance(args[0]) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;

    if (newNode && newNode->nodeType() == Node::ATTRIBUTE_NODE && imp->element()) {
        if (!V8BindingSecurity::allowSettingSrcToJavascriptURL(V8BindingState::Only(), imp->element(), newNode->nodeName(), newNode->nodeValue()))
            return v8::Handle<v8::Value>();
    }

    ExceptionCode ec = 0;
    RefPtr<Node> result = imp->setNamedItemNS(newNode, ec);
    if (UNLIKELY(ec)) {
        throwError(ec);
        return v8::Handle<v8::Value>();
    }

    return toV8(result.release());
}

v8::Handle<v8::Value> V8NamedNodeMap::setNamedItemCallback(const v8::Arguments & args)
{
    INC_STATS("DOM.NamedNodeMap.setNamedItem");
    NamedNodeMap* imp = V8NamedNodeMap::toNative(args.Holder());
    Node* newNode = V8Node::HasInstance(args[0]) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;

    if (newNode && newNode->nodeType() == Node::ATTRIBUTE_NODE && imp->element()) {
      if (!V8BindingSecurity::allowSettingSrcToJavascriptURL(V8BindingState::Only(), imp->element(), newNode->nodeName(), newNode->nodeValue()))
            return v8::Handle<v8::Value>();
    }

    ExceptionCode ec = 0;
    RefPtr<Node> result = imp->setNamedItem(newNode, ec);
    if (UNLIKELY(ec)) {
        throwError(ec);
        return v8::Handle<v8::Value>();
    }

    return toV8(result.release());
}

v8::Handle<v8::Value> toV8(NamedNodeMap* impl)
{
    if (!impl)
        return v8::Null();
    v8::Handle<v8::Object> wrapper = V8NamedNodeMap::wrap(impl);
    // Add a hidden reference from named node map to its owner node.
    Element* element = impl->element();
    if (!wrapper.IsEmpty() && element)
        V8DOMWrapper::setHiddenReference(wrapper, toV8(element));
    return wrapper;
}

} // namespace WebCore
