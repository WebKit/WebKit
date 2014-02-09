/*
 * Copyright (C) 2009, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SerializedScriptValue_h
#define SerializedScriptValue_h

#include "ScriptState.h"
#include <bindings/ScriptValue.h>
#include <heap/Strong.h>
#include <runtime/ArrayBuffer.h>
#include <runtime/JSCJSValue.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

typedef const struct OpaqueJSContext* JSContextRef;
typedef const struct OpaqueJSValue* JSValueRef;

namespace WebCore {

class MessagePort;
typedef Vector<RefPtr<MessagePort>, 1> MessagePortArray;
typedef Vector<RefPtr<JSC::ArrayBuffer>, 1> ArrayBufferArray;
 
enum SerializationReturnCode {
    SuccessfullyCompleted,
    StackOverflowError,
    InterruptedExecutionError,
    ValidationError,
    ExistingExceptionError,
    DataCloneError,
    UnspecifiedError
};
    
enum SerializationErrorMode { NonThrowing, Throwing };

class SharedBuffer;

class SerializedScriptValue :
#if ENABLE(INDEXED_DATABASE)
    public ThreadSafeRefCounted<SerializedScriptValue> {
#else
    public RefCounted<SerializedScriptValue> {
#endif
public:
    static PassRefPtr<SerializedScriptValue> create(JSC::ExecState*, JSC::JSValue, MessagePortArray*, ArrayBufferArray*, SerializationErrorMode = Throwing);

    static PassRefPtr<SerializedScriptValue> create(const String&);
    static PassRefPtr<SerializedScriptValue> adopt(Vector<uint8_t>& buffer)
    {
        return adoptRef(new SerializedScriptValue(buffer));
    }

    static PassRefPtr<SerializedScriptValue> nullValue();

    JSC::JSValue deserialize(JSC::ExecState*, JSC::JSGlobalObject*, MessagePortArray*, SerializationErrorMode = Throwing);

    static PassRefPtr<SerializedScriptValue> create(const Deprecated::ScriptValue&, JSC::ExecState*, MessagePortArray*, ArrayBufferArray*, bool& didThrow);
    static Deprecated::ScriptValue deserialize(JSC::ExecState*, SerializedScriptValue*, SerializationErrorMode = Throwing);

    static uint32_t wireFormatVersion();

    String toString();

    // API implementation helpers. These don't expose special behavior for ArrayBuffers or MessagePorts.
    static PassRefPtr<SerializedScriptValue> create(JSContextRef, JSValueRef, JSValueRef* exception);
    JSValueRef deserialize(JSContextRef, JSValueRef* exception);

    const Vector<uint8_t>& data() const { return m_data; }
    const Vector<String>& blobURLs() const { return m_blobURLs; }

#if ENABLE(INDEXED_DATABASE)
    // FIXME: Get rid of these. The only caller immediately deserializes the result, so it's a very roundabout way to create a JSValue.
    static PassRefPtr<SerializedScriptValue> numberValue(double value);
    static PassRefPtr<SerializedScriptValue> undefinedValue();
#endif

    static PassRefPtr<SerializedScriptValue> createFromWireBytes(const Vector<uint8_t>& data)
    {
        return adoptRef(new SerializedScriptValue(data));
    }
    const Vector<uint8_t>& toWireBytes() const { return m_data; }

    ~SerializedScriptValue();

private:
    typedef Vector<JSC::ArrayBufferContents> ArrayBufferContentsArray;
    static void maybeThrowExceptionIfSerializationFailed(JSC::ExecState*, SerializationReturnCode);
    static bool serializationDidCompleteSuccessfully(SerializationReturnCode);
    static PassOwnPtr<ArrayBufferContentsArray> transferArrayBuffers(JSC::ExecState*, ArrayBufferArray&, SerializationReturnCode&);

    SerializedScriptValue(const Vector<unsigned char>&);
    SerializedScriptValue(Vector<unsigned char>&);
    SerializedScriptValue(Vector<unsigned char>&, Vector<String>& blobURLs);
    SerializedScriptValue(Vector<unsigned char>&, Vector<String>& blobURLs, PassOwnPtr<ArrayBufferContentsArray>);

    Vector<unsigned char> m_data;
    OwnPtr<ArrayBufferContentsArray> m_arrayBufferContentsArray;
    Vector<String> m_blobURLs;
};

}

#endif // SerializedScriptValue_h
