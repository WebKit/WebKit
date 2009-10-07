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
#include "SerializedScriptValue.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8MessagePortCustom.h"
#include "V8MessagePort.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(MessagePortAddEventListener)
{
    INC_STATS("DOM.MessagePort.addEventListener()");
    MessagePort* messagePort = V8DOMWrapper::convertToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, args.Holder());
    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(messagePort, args[1], false, ListenerFindOrCreate);
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
    INC_STATS("DOM.MessagePort.removeEventListener()");
    MessagePort* messagePort = V8DOMWrapper::convertToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, args.Holder());
    RefPtr<EventListener> listener = V8DOMWrapper::getEventListener(messagePort, args[1], false, ListenerFindOnly);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        messagePort->removeEventListener(type, listener.get(), useCapture);

        removeHiddenDependency(args.Holder(), args[1], V8Custom::kMessagePortRequestCacheIndex);
    }

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(MessagePortPostMessage)
{
    INC_STATS("DOM.MessagePort.postMessage");
    MessagePort* messagePort = V8DOMWrapper::convertToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, args.Holder());
    RefPtr<SerializedScriptValue> message = SerializedScriptValue::create(toWebCoreString(args[0]));
    MessagePortArray portArray;
    if (args.Length() > 1) {
        if (!getMessagePortArray(args[1], portArray))
            return v8::Undefined();
    }
    ExceptionCode ec = 0;
    messagePort->postMessage(message.release(), &portArray, ec);
    return throwError(ec);
}

bool getMessagePortArray(v8::Local<v8::Value> value, MessagePortArray& portArray)
{
    if (isUndefinedOrNull(value)) {
        portArray.resize(0);
        return true;
    }

    if (!value->IsObject()) {
        throwError("MessagePortArray argument must be an object");
        return false;
    }
    uint32_t length = 0;
    v8::Local<v8::Object> ports = v8::Local<v8::Object>::Cast(value);

    if (value->IsArray()) {
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);
        length = array->Length();
    } else {
        // Sequence-type object - get the length attribute
        v8::Local<v8::Value> sequenceLength = ports->Get(v8::String::New("length"));
        if (!sequenceLength->IsNumber()) {
            throwError("MessagePortArray argument has no length attribute");
            return false;
        }
        length = sequenceLength->Uint32Value();
    }
    portArray.resize(length);

    for (unsigned int i = 0; i < length; ++i) {
        v8::Local<v8::Value> port = ports->Get(v8::Integer::New(i));
        // Validation of non-null objects, per HTML5 spec 8.3.3.
        if (isUndefinedOrNull(port)) {
            throwError(INVALID_STATE_ERR);
            return false;
        }
        // Validation of Objects implementing an interface, per WebIDL spec 4.1.15.
        if (!V8MessagePort::HasInstance(port)) {
            throwError("MessagePortArray argument must contain only MessagePorts");
            return false;
        }
        portArray[i] = V8DOMWrapper::convertToNativeObject<MessagePort>(V8ClassIndex::MESSAGEPORT, v8::Handle<v8::Object>::Cast(port));
    }
    return true;
}

} // namespace WebCore
