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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

class SerializedScriptValue : public ThreadSafeRefCounted<SerializedScriptValue> {
public:
    WEBCORE_EXPORT static RefPtr<SerializedScriptValue> create(JSC::ExecState*, JSC::JSValue, MessagePortArray*, ArrayBufferArray*, SerializationErrorMode = Throwing);

    WEBCORE_EXPORT static RefPtr<SerializedScriptValue> create(const String&);
    static Ref<SerializedScriptValue> adopt(Vector<uint8_t>&& buffer)
    {
        return adoptRef(*new SerializedScriptValue(WTFMove(buffer)));
    }

    static Ref<SerializedScriptValue> nullValue();

    WEBCORE_EXPORT JSC::JSValue deserialize(JSC::ExecState*, JSC::JSGlobalObject*, MessagePortArray*, SerializationErrorMode = Throwing);

    static uint32_t wireFormatVersion();

    String toString();

    // API implementation helpers. These don't expose special behavior for ArrayBuffers or MessagePorts.
    WEBCORE_EXPORT static RefPtr<SerializedScriptValue> create(JSContextRef, JSValueRef, JSValueRef* exception);
    WEBCORE_EXPORT JSValueRef deserialize(JSContextRef, JSValueRef* exception);

    const Vector<uint8_t>& data() const { return m_data; }
    bool hasBlobURLs() const { return !m_blobURLs.isEmpty(); }
    void blobURLs(Vector<String>&) const;

    static Ref<SerializedScriptValue> createFromWireBytes(Vector<uint8_t>&& data)
    {
        return adoptRef(*new SerializedScriptValue(WTFMove(data)));
    }
    const Vector<uint8_t>& toWireBytes() const { return m_data; }

    WEBCORE_EXPORT ~SerializedScriptValue();

private:
    typedef Vector<JSC::ArrayBufferContents> ArrayBufferContentsArray;
    static void maybeThrowExceptionIfSerializationFailed(JSC::ExecState*, SerializationReturnCode);
    static bool serializationDidCompleteSuccessfully(SerializationReturnCode);
    static std::unique_ptr<ArrayBufferContentsArray> transferArrayBuffers(JSC::ExecState*, ArrayBufferArray&, SerializationReturnCode&);
    void addBlobURL(const String&);

    WEBCORE_EXPORT SerializedScriptValue(Vector<unsigned char>&&);
    SerializedScriptValue(Vector<unsigned char>&&, const Vector<String>& blobURLs);
    SerializedScriptValue(Vector<unsigned char>&&, const Vector<String>& blobURLs, std::unique_ptr<ArrayBufferContentsArray>&&);

    Vector<unsigned char> m_data;
    std::unique_ptr<ArrayBufferContentsArray> m_arrayBufferContentsArray;
    Vector<Vector<uint16_t>> m_blobURLs;
};

}

#endif // SerializedScriptValue_h
