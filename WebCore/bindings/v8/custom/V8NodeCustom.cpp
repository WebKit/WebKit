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
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8CustomEventListener.h"
#include "V8Node.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {

CALLBACK_FUNC_DECL(NodeAddEventListener)
{
    INC_STATS("DOM.Node.addEventListener()");
    Node* node = V8DOMWrapper::convertDOMWrapperToNode<Node>(args.Holder());

    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(node, args[1], false, ListenerFindOrCreate);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        node->addEventListener(type, listener, useCapture);
        createHiddenDependency(args.Holder(), args[1], V8Custom::kNodeEventListenerCacheIndex);
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(NodeRemoveEventListener)
{
    INC_STATS("DOM.Node.removeEventListener()");
    Node* node = V8DOMWrapper::convertDOMWrapperToNode<Node>(args.Holder());

    // It is possbile that the owner document of the node is detached
    // from the frame.
    // See issue http://b/878909
    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(node, args[1], false, ListenerFindOnly);
    if (listener) {
        AtomicString type = v8ValueToAtomicWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        node->removeEventListener(type, listener.get(), useCapture);
        removeHiddenDependency(args.Holder(), args[1], V8Custom::kNodeEventListenerCacheIndex);
    }

    return v8::Undefined();
}

// This function is customized to take advantage of the optional 4th argument: shouldLazyAttach
CALLBACK_FUNC_DECL(NodeInsertBefore)
{
    INC_STATS("DOM.Node.insertBefore");
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8DOMWrapper::convertDOMWrapperToNode<Node>(holder);
    ExceptionCode ec = 0;
    Node* newChild = V8Node::HasInstance(args[0]) ? V8DOMWrapper::convertDOMWrapperToNode<Node>(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    Node* refChild = V8Node::HasInstance(args[1]) ? V8DOMWrapper::convertDOMWrapperToNode<Node>(v8::Handle<v8::Object>::Cast(args[1])) : 0;
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
CALLBACK_FUNC_DECL(NodeReplaceChild)
{
    INC_STATS("DOM.Node.replaceChild");
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8DOMWrapper::convertDOMWrapperToNode<Node>(holder);
    ExceptionCode ec = 0;
    Node* newChild = V8Node::HasInstance(args[0]) ? V8DOMWrapper::convertDOMWrapperToNode<Node>(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    Node* oldChild = V8Node::HasInstance(args[1]) ? V8DOMWrapper::convertDOMWrapperToNode<Node>(v8::Handle<v8::Object>::Cast(args[1])) : 0;
    bool success = imp->replaceChild(newChild, oldChild, ec, true);
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    if (success)
        return args[1];
    return v8::Null();
}

CALLBACK_FUNC_DECL(NodeRemoveChild)
{
    INC_STATS("DOM.Node.removeChild");
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8DOMWrapper::convertDOMWrapperToNode<Node>(holder);
    ExceptionCode ec = 0;
    Node* oldChild = V8Node::HasInstance(args[0]) ? V8DOMWrapper::convertDOMWrapperToNode<Node>(v8::Handle<v8::Object>::Cast(args[0])) : 0;
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
CALLBACK_FUNC_DECL(NodeAppendChild)
{
    INC_STATS("DOM.Node.appendChild");
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8DOMWrapper::convertDOMWrapperToNode<Node>(holder);
    ExceptionCode ec = 0;
    Node* newChild = V8Node::HasInstance(args[0]) ? V8DOMWrapper::convertDOMWrapperToNode<Node>(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    bool success = imp->appendChild(newChild, ec, true );
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    if (success)
        return args[0];
    return v8::Null();
}

} // namespace WebCore
