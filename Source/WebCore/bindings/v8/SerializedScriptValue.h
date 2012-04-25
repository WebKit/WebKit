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
#include <wtf/ArrayBuffer.h>
#include <wtf/Threading.h>

namespace WebCore {

class MessagePort;

typedef Vector<RefPtr<MessagePort>, 1> MessagePortArray;
typedef Vector<RefPtr<WTF::ArrayBuffer>, 1> ArrayBufferArray;

class SerializedScriptValue : public ThreadSafeRefCounted<SerializedScriptValue> {
public:
    // If a serialization error occurs (e.g., cyclic input value) this
    // function returns an empty representation, schedules a V8 exception to
    // be thrown using v8::ThrowException(), and sets |didThrow|. In this case
    // the caller must not invoke any V8 operations until control returns to
    // V8. When serialization is successful, |didThrow| is false.
    static PassRefPtr<SerializedScriptValue> create(v8::Handle<v8::Value>,
                                                    MessagePortArray*, ArrayBufferArray*,
                                                    bool& didThrow, v8::Isolate* = 0);
    static PassRefPtr<SerializedScriptValue> create(v8::Handle<v8::Value>, v8::Isolate* = 0);
    static PassRefPtr<SerializedScriptValue> createFromWire(const String& data);
    static PassRefPtr<SerializedScriptValue> create(const String& data, v8::Isolate* = 0);
    static PassRefPtr<SerializedScriptValue> create();

    static SerializedScriptValue* nullValue(v8::Isolate* = 0);
    static PassRefPtr<SerializedScriptValue> undefinedValue(v8::Isolate* = 0);
    static PassRefPtr<SerializedScriptValue> booleanValue(bool value, v8::Isolate* = 0);
    static PassRefPtr<SerializedScriptValue> numberValue(double value, v8::Isolate* = 0);

    PassRefPtr<SerializedScriptValue> release();

    String toWireString() const { return m_data; }

    // Deserializes the value (in the current context). Returns a null value in
    // case of failure.
    v8::Handle<v8::Value> deserialize(MessagePortArray* = 0, v8::Isolate* = 0);

#if ENABLE(INSPECTOR)
    ScriptValue deserializeForInspector(ScriptState*, v8::Isolate* = 0);
#endif

    const Vector<String>& blobURLs() const { return m_blobURLs; }

private:
    enum StringDataMode {
        StringValue,
        WireData
    };
    typedef Vector<WTF::ArrayBufferContents, 1> ArrayBufferContentsArray;

    SerializedScriptValue();
    SerializedScriptValue(v8::Handle<v8::Value>, MessagePortArray*, ArrayBufferArray*, bool& didThrow, v8::Isolate*);
    explicit SerializedScriptValue(const String& wireData);

    static PassOwnPtr<ArrayBufferContentsArray> transferArrayBuffers(ArrayBufferArray&, bool& didThrow);

    String m_data;
    OwnPtr<ArrayBufferContentsArray> m_arrayBufferContentsArray;
    Vector<String> m_blobURLs;
};

} // namespace WebCore

#endif // SerializedScriptValue_h
