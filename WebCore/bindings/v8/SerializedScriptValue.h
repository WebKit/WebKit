/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

#ifndef SerializedScriptValue_h
#define SerializedScriptValue_h

#include "ScriptValue.h"
#include "V8Binding.h"
#include <v8.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class SerializedScriptValue : public RefCounted<SerializedScriptValue> {
public:
    // Creates a serialized representation of the given V8 value.
    static PassRefPtr<SerializedScriptValue> create(v8::Handle<v8::Value> value)
    {
        return adoptRef(new SerializedScriptValue(value));
    }

    // Creates a serialized value with the given data obtained from a
    // prior call to toWireString().
    static PassRefPtr<SerializedScriptValue> createFromWire(String data)
    {
        return adoptRef(new SerializedScriptValue(data, WireData));
    }

    // Creates a serialized representation of WebCore string.
    static PassRefPtr<SerializedScriptValue> create(String data)
    {
        return adoptRef(new SerializedScriptValue(data, StringValue));
    }

    // Creates an empty serialized value.
    static PassRefPtr<SerializedScriptValue> create()
    {
        return adoptRef(new SerializedScriptValue());
    }

    PassRefPtr<SerializedScriptValue> release()
    {
        RefPtr<SerializedScriptValue> result = adoptRef(new SerializedScriptValue(m_data, WireData));
        m_data = String();
        return result.release();
    }

    String toWireString() const { return m_data; }

    // Deserializes the value (in the current context). Returns an
    // empty handle in case of failure.
    v8::Local<v8::Value> deserialize();

private:
    enum StringDataMode {
        StringValue,
        WireData
    };

    SerializedScriptValue() { }

    explicit SerializedScriptValue(v8::Handle<v8::Value>);

    SerializedScriptValue(String data, StringDataMode mode);

    String m_data;
};

} // namespace WebCore

#endif // SerializedScriptValue_h
