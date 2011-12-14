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
#include <v8.h>
#include <wtf/Threading.h>

namespace WebCore {

class SerializedScriptValue : public ThreadSafeRefCounted<SerializedScriptValue> {
public:
    static void deserializeAndSetProperty(v8::Handle<v8::Object>, const char* propertyName,
                                          v8::PropertyAttribute, SerializedScriptValue*);
    static void deserializeAndSetProperty(v8::Handle<v8::Object>, const char* propertyName,
                                          v8::PropertyAttribute, PassRefPtr<SerializedScriptValue>);

    // If a serialization error occurs (e.g., cyclic input value) this
    // function returns an empty representation, schedules a V8 exception to
    // be thrown using v8::ThrowException(), and sets |didThrow|. In this case
    // the caller must not invoke any V8 operations until control returns to
    // V8. When serialization is successful, |didThrow| is false.
    static PassRefPtr<SerializedScriptValue> create(v8::Handle<v8::Value> value, bool& didThrow);
    static PassRefPtr<SerializedScriptValue> create(v8::Handle<v8::Value>);
    static PassRefPtr<SerializedScriptValue> createFromWire(String data);
    static PassRefPtr<SerializedScriptValue> create(String data);
    static PassRefPtr<SerializedScriptValue> create();

    static SerializedScriptValue* nullValue();
    static SerializedScriptValue* undefinedValue();

    PassRefPtr<SerializedScriptValue> release();

    String toWireString() const { return m_data; }

    // Deserializes the value (in the current context). Returns a null value in
    // case of failure.
    v8::Handle<v8::Value> deserialize();

private:
    enum StringDataMode {
        StringValue,
        WireData
    };

    SerializedScriptValue();
    SerializedScriptValue(v8::Handle<v8::Value>, bool& didThrow);
    explicit SerializedScriptValue(String wireData);

    String m_data;
};

} // namespace WebCore

#endif // SerializedScriptValue_h
