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
#include "MessageEvent.h"
#include "SerializedScriptValue.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8DOMWindow.h"
#include "V8MessagePortCustom.h"
#include "V8Proxy.h"

namespace WebCore {

ACCESSOR_GETTER(MessageEventPorts)
{
    INC_STATS("DOM.MessageEvent.ports");
    MessageEvent* event = V8DOMWrapper::convertToNativeObject<MessageEvent>(V8ClassIndex::MESSAGEEVENT, info.Holder());

    MessagePortArray* ports = event->ports();
    if (!ports || ports->isEmpty())
        return v8::Null();

    v8::Local<v8::Array> portArray = v8::Array::New(ports->size());
    for (size_t i = 0; i < ports->size(); ++i)
        portArray->Set(v8::Integer::New(i), V8DOMWrapper::convertToV8Object(V8ClassIndex::MESSAGEPORT, (*ports)[i].get()));

    return portArray;
}

CALLBACK_FUNC_DECL(MessageEventInitMessageEvent)
{
    INC_STATS("DOM.MessageEvent.initMessageEvent");
    MessageEvent* event = V8DOMWrapper::convertToNativeObject<MessageEvent>(V8ClassIndex::MESSAGEEVENT, args.Holder());
    String typeArg = v8ValueToWebCoreString(args[0]);
    bool canBubbleArg = args[1]->BooleanValue();
    bool cancelableArg = args[2]->BooleanValue();
    RefPtr<SerializedScriptValue> dataArg = SerializedScriptValue::create(v8ValueToWebCoreString(args[3]));
    String originArg = v8ValueToWebCoreString(args[4]);
    String lastEventIdArg = v8ValueToWebCoreString(args[5]);
    DOMWindow* sourceArg = V8DOMWindow::HasInstance(args[6]) ? V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, v8::Handle<v8::Object>::Cast(args[6])) : 0;
    OwnPtr<MessagePortArray> portArray;

    if (!isUndefinedOrNull(args[7])) {
        portArray = new MessagePortArray();
        if (!getMessagePortArray(args[7], *portArray))
            return v8::Undefined();
    }
    event->initMessageEvent(typeArg, canBubbleArg, cancelableArg, dataArg.release(), originArg, lastEventIdArg, sourceArg, portArray.release());
    return v8::Undefined();
  }

} // namespace WebCore
