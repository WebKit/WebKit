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
#include "JSDOMConvertBufferSource.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMWindow.h"
#include "JSEventTarget.h"
#include "JSMessagePort.h"
#include <runtime/JSArray.h>
#include <runtime/JSArrayBuffer.h>


namespace WebCore {
using namespace JSC;

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
                    result = serializedValue->deserialize(state, globalObject());
                else
                    result = jsNull();
            } else
                result = dataValue;
        }
        break;
    }

    case MessageEvent::DataTypeSerializedScriptValue:
        if (RefPtr<SerializedScriptValue> serializedValue = event.dataAsSerializedScriptValue()) {
            // FIXME: Why does this suppress exceptions?
            result = serializedValue->deserialize(state, globalObject(), wrapped().ports(), SerializationErrorMode::NonThrowing);
        } else
            result = jsNull();
        break;

    case MessageEvent::DataTypeString:
        result = toJS<IDLDOMString>(state, event.dataAsString());
        break;

    case MessageEvent::DataTypeBlob:
        result = toJS<IDLInterface<Blob>>(state, *globalObject(), event.dataAsBlob());
        break;

    case MessageEvent::DataTypeArrayBuffer:
        result = toJS<IDLInterface<ArrayBuffer>>(state, *globalObject(), event.dataAsArrayBuffer());
        break;
    }

    // Save the result so we don't have to deserialize the value again.
    m_data.set(state.vm(), this, result);
    return result;
}

} // namespace WebCore
