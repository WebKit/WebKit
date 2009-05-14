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

#include "ExceptionCode.h"
#include "MessagePort.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8ObjectEventListener.h"
#include "V8Proxy.h"
#include "V8Utilities.h"

namespace WebCore {

ACCESSOR_GETTER(MessagePortOnmessage)
{
    INC_STATS("DOM.MessagePort.onmessage._get");
    MessagePort* messagePort = V8Proxy::ToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, info.Holder());
    return V8Proxy::EventListenerToV8Object(messagePort->onmessage());
}

ACCESSOR_SETTER(MessagePortOnmessage)
{
    INC_STATS("DOM.MessagePort.onmessage._set");
    MessagePort* messagePort = V8Proxy::ToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, info.Holder());
    if (value->IsNull()) {
        if (messagePort->onmessage()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(messagePort->onmessage());
            removeHiddenDependency(info.Holder(), listener->getListenerObject(), V8Custom::kMessagePortRequestCacheIndex);
        }

        // Clear the listener.
        messagePort->setOnmessage(0);

    } else {
        V8Proxy* proxy = V8Proxy::retrieve(messagePort->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            messagePort->setOnmessage(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kMessagePortRequestCacheIndex);
        }
    }
}

ACCESSOR_GETTER(MessagePortOnclose)
{
    INC_STATS("DOM.MessagePort.onclose._get");
    MessagePort* messagePort = V8Proxy::ToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, info.Holder());
    return V8Proxy::EventListenerToV8Object(messagePort->onclose());
}

ACCESSOR_SETTER(MessagePortOnclose)
{
    INC_STATS("DOM.MessagePort.onclose._set");
    MessagePort* messagePort = V8Proxy::ToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, info.Holder());
    if (value->IsNull()) {
        if (messagePort->onclose()) {
            V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(messagePort->onclose());
            removeHiddenDependency(info.Holder(), listener->getListenerObject(), V8Custom::kXMLHttpRequestCacheIndex);
        }

        // Clear the listener.
        messagePort->setOnclose(0);
    } else {
        V8Proxy* proxy = V8Proxy::retrieve(messagePort->scriptExecutionContext());
        if (!proxy)
            return;

        RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(value, false);
        if (listener) {
            messagePort->setOnclose(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kMessagePortRequestCacheIndex);
        }
    }
}

CALLBACK_FUNC_DECL(MessagePortStartConversation)
{
    INC_STATS("DOM.MessagePort.StartConversation()");
    if (args.Length() < 1)
        return throwError("Not enough arguments", V8Proxy::SYNTAX_ERROR);
    MessagePort* messagePort = V8Proxy::ToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, args.Holder());

    V8Proxy* proxy = V8Proxy::retrieve(messagePort->scriptExecutionContext());
    if (!proxy)
        return v8::Undefined();

    RefPtr<MessagePort> port = messagePort->startConversation(messagePort->scriptExecutionContext(), toWebCoreString(args[0]));
    v8::Handle<v8::Value> wrapper = V8Proxy::ToV8Object(V8ClassIndex::MESSAGEPORT, port.get());
    return wrapper;
}

CALLBACK_FUNC_DECL(MessagePortAddEventListener)
{
    INC_STATS("DOM.MessagePort.AddEventListener()");
    MessagePort* messagePort = V8Proxy::ToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, args.Holder());

    V8Proxy* proxy = V8Proxy::retrieve(messagePort->scriptExecutionContext());
    if (!proxy)
        return v8::Undefined();

    RefPtr<EventListener> listener = proxy->FindOrCreateObjectEventListener(args[1], false);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        messagePort->addEventListener(type, listener, useCapture);

        createHiddenDependency(args.Holder(), args[1], V8Custom::kMessagePortRequestCacheIndex);
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(MessagePortRemoveEventListener)
{
    INC_STATS("DOM.MessagePort.RemoveEventListener()");
    MessagePort* messagePort = V8Proxy::ToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, args.Holder());

    V8Proxy* proxy = V8Proxy::retrieve(messagePort->scriptExecutionContext());
    if (!proxy)
        return v8::Undefined(); // probably leaked

    RefPtr<EventListener> listener = proxy->FindObjectEventListener(args[1], false);

    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        messagePort->removeEventListener(type, listener.get(), useCapture);

        removeHiddenDependency(args.Holder(), args[1], V8Custom::kMessagePortRequestCacheIndex);
    }

    return v8::Undefined();
}

} // namespace WebCore
