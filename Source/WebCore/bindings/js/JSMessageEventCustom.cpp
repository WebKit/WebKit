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
#include "JSMessageEvent.h"

#include "JSBlob.h"
#include "JSDOMBinding.h"
#include "JSDOMWindow.h"
#include "JSEventTarget.h"
#include "JSMessagePortCustom.h"
#include "MessageEvent.h"
#include <runtime/JSArray.h>
#include <runtime/JSArrayBuffer.h>

using namespace JSC;

namespace WebCore {

JSValue JSMessageEvent::data(ExecState& state) const
{
    if (JSValue cachedValue = m_data.get()) {
        // We cannot use a cached object if we are in a different world than the one it was created in.
        if (!cachedValue.isObject() || &worldForDOMObject(cachedValue.getObject()) == &currentWorld(&state))
            return cachedValue;
        ASSERT_NOT_REACHED();
    }

    MessageEvent& event = wrapped();
    JSValue result;
    switch (event.dataType()) {
    case MessageEvent::DataTypeScriptValue: {
        JSValue dataValue = event.dataAsScriptValue();
        if (!dataValue)
            result = jsNull();
        else {
            // We need to make sure MessageEvents do not leak objects in their state property across isolated DOM worlds.
            // Ideally, we would check that the worlds have different privileges but that's not possible yet.
            if (dataValue.isObject() && &worldForDOMObject(dataValue.getObject()) != &currentWorld(&state)) {
                RefPtr<SerializedScriptValue> serializedValue = event.trySerializeData(&state);
                if (serializedValue)
                    result = serializedValue->deserialize(&state, globalObject(), nullptr);
                else
                    result = jsNull();
            } else
                result = dataValue;
        }
        break;
    }

    case MessageEvent::DataTypeSerializedScriptValue:
        if (RefPtr<SerializedScriptValue> serializedValue = event.dataAsSerializedScriptValue()) {
            MessagePortArray ports = wrapped().ports();
            // FIXME: Why does this suppress exceptions?
            result = serializedValue->deserialize(&state, globalObject(), &ports, NonThrowing);
        } else
            result = jsNull();
        break;

    case MessageEvent::DataTypeString:
        result = jsStringWithCache(&state, event.dataAsString());
        break;

    case MessageEvent::DataTypeBlob:
        result = toJS(&state, globalObject(), event.dataAsBlob());
        break;

    case MessageEvent::DataTypeArrayBuffer:
        result = toJS(&state, globalObject(), event.dataAsArrayBuffer());
        break;
    }

    // Save the result so we don't have to deserialize the value again.
    m_data.set(state.vm(), this, result);
    return result;
}

static JSC::JSValue handleInitMessageEvent(JSMessageEvent* jsEvent, JSC::ExecState& state)
{
    const String& typeArg = state.argument(0).toString(&state)->value(&state);
    bool canBubbleArg = state.argument(1).toBoolean(&state);
    bool cancelableArg = state.argument(2).toBoolean(&state);
    const String originArg = valueToUSVString(&state, state.argument(4));
    const String lastEventIdArg = state.argument(5).toString(&state)->value(&state);
    DOMWindow* sourceArg = JSDOMWindow::toWrapped(state, state.argument(6));
    std::unique_ptr<MessagePortArray> messagePorts;
    std::unique_ptr<ArrayBufferArray> arrayBuffers;
    if (!state.argument(7).isUndefinedOrNull()) {
        messagePorts = std::make_unique<MessagePortArray>();
        arrayBuffers = std::make_unique<ArrayBufferArray>();
        fillMessagePortArray(state, state.argument(7), *messagePorts, *arrayBuffers);
        if (state.hadException())
            return jsUndefined();
    }
    Deprecated::ScriptValue dataArg(state.vm(), state.argument(3));
    if (state.hadException())
        return jsUndefined();

    MessageEvent& event = jsEvent->wrapped();
    event.initMessageEvent(typeArg, canBubbleArg, cancelableArg, dataArg, originArg, lastEventIdArg, sourceArg, WTFMove(messagePorts));
    jsEvent->m_data.set(state.vm(), jsEvent, dataArg.jsValue());
    return jsUndefined();
}

JSC::JSValue JSMessageEvent::initMessageEvent(JSC::ExecState& state)
{
    return handleInitMessageEvent(this, state);
}

JSC::JSValue JSMessageEvent::webkitInitMessageEvent(JSC::ExecState& state)
{
    return handleInitMessageEvent(this, state);
}

} // namespace WebCore
