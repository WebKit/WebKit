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
#include "Node.h"

#include "Document.h"
#include "EventListener.h"

#include "V8AbstractEventListener.h"
#include "V8Attr.h"
#include "V8Binding.h"
#include "V8BindingState.h"
#include "V8CDATASection.h"
#include "V8Comment.h"
#include "V8Document.h"
#include "V8DocumentFragment.h"
#include "V8DocumentType.h"
#include "V8Element.h"
#include "V8Entity.h"
#include "V8EntityReference.h"
#include "V8EventListener.h"
#include "V8Node.h"
#include "V8Notation.h"
#include "V8ProcessingInstruction.h"
#include "V8Proxy.h"
#include "V8Text.h"

#include <wtf/RefPtr.h>

namespace WebCore {

// This function is customized to take advantage of the optional 4th argument: shouldLazyAttach
v8::Handle<v8::Value> V8Node::insertBeforeCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Node.insertBefore");
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8Node::toNative(holder);
    ExceptionCode ec = 0;
    Node* newChild = V8Node::HasInstance(args[0]) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    Node* refChild = V8Node::HasInstance(args[1]) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[1])) : 0;
    bool success = imp->insertBefore(newChild, refChild, ec, true);
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    if (success)
        return args[0];
    return v8::Null();
}

// This function is customized to take advantage of the optional 4th argument: shouldLazyAttach
v8::Handle<v8::Value> V8Node::replaceChildCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Node.replaceChild");
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8Node::toNative(holder);
    ExceptionCode ec = 0;
    Node* newChild = V8Node::HasInstance(args[0]) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    Node* oldChild = V8Node::HasInstance(args[1]) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[1])) : 0;
    bool success = imp->replaceChild(newChild, oldChild, ec, true);
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    if (success)
        return args[1];
    return v8::Null();
}

v8::Handle<v8::Value> V8Node::removeChildCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Node.removeChild");
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8Node::toNative(holder);
    ExceptionCode ec = 0;
    Node* oldChild = V8Node::HasInstance(args[0]) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    bool success = imp->removeChild(oldChild, ec);
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    if (success)
        return args[0];
    return v8::Null();
}

// This function is customized to take advantage of the optional 4th argument: shouldLazyAttach
v8::Handle<v8::Value> V8Node::appendChildCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Node.appendChild");
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8Node::toNative(holder);
    ExceptionCode ec = 0;
    Node* newChild = V8Node::HasInstance(args[0]) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    bool success = imp->appendChild(newChild, ec, true );
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    if (success)
        return args[0];
    return v8::Null();
}

v8::Handle<v8::Value> toV8Slow(Node* impl, bool forceNewObject)
{
    if (!impl)
        return v8::Null();

    if (!forceNewObject) {
        v8::Handle<v8::Value> wrapper = V8DOMWrapper::getWrapper(impl);
        if (!wrapper.IsEmpty())
            return wrapper;
    }
    switch (impl->nodeType()) {
    case Node::ELEMENT_NODE:
        return toV8(static_cast<Element*>(impl), forceNewObject);
    case Node::ATTRIBUTE_NODE:
        return toV8(static_cast<Attr*>(impl), forceNewObject);
    case Node::TEXT_NODE:
        return toV8(static_cast<Text*>(impl), forceNewObject);
    case Node::CDATA_SECTION_NODE:
        return toV8(static_cast<CDATASection*>(impl), forceNewObject);
    case Node::ENTITY_REFERENCE_NODE:
        return toV8(static_cast<EntityReference*>(impl), forceNewObject);
    case Node::ENTITY_NODE:
        return toV8(static_cast<Entity*>(impl), forceNewObject);
    case Node::PROCESSING_INSTRUCTION_NODE:
        return toV8(static_cast<ProcessingInstruction*>(impl), forceNewObject);
    case Node::COMMENT_NODE:
        return toV8(static_cast<Comment*>(impl), forceNewObject);
    case Node::DOCUMENT_NODE:
        return toV8(static_cast<Document*>(impl), forceNewObject);
    case Node::DOCUMENT_TYPE_NODE:
        return toV8(static_cast<DocumentType*>(impl), forceNewObject);
    case Node::DOCUMENT_FRAGMENT_NODE:
        return toV8(static_cast<DocumentFragment*>(impl), forceNewObject);
    case Node::NOTATION_NODE:
        return toV8(static_cast<Notation*>(impl), forceNewObject);
    default: break; // XPATH_NAMESPACE_NODE
    }
    return V8Node::wrap(impl, forceNewObject);
}
} // namespace WebCore
