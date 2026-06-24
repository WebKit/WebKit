/*
 * Copyright (C) 2009, 2013, 2016 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/Forward.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/StructuredCloneTags.h>
#include <WebCore/FileSystemHandleGlobalIdentifier.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

typedef const struct OpaqueJSContext* JSContextRef;
typedef const struct OpaqueJSValue* JSValueRef;

namespace IPC {
template<typename> struct ArgumentCoder;
}

#if ENABLE(WEBASSEMBLY)
namespace JSC { namespace Wasm {
class Module;
class MemoryHandle;
} }
#endif

namespace JSC {
class ArrayBufferContents;
class ErrorInstance;
class JSGlobalObject;
class JSObject;
class JSValue;
}

namespace WebCore {

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
class DetachedOffscreenCanvas;
class OffscreenCanvas;
#endif
class CryptoKey;
class IDBValue;
class MessagePort;
class DetachedImageBitmap;
class FragmentedSharedBuffer;
class URLKeepingBlobAlive;
struct NonSerializedDataToken;
struct SerializedScriptValueInternals;
template<typename> class ExceptionOr;

enum class SerializationErrorMode : bool { NonThrowing, Throwing };
enum class SerializationContext : bool { Default, CloneAcrossWorlds };
enum class SerializationForStorage : bool { No, Yes };

using JSC::ErrorInformation;
using JSC::extractErrorInformationFromErrorInstance;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SerializedScriptValue);
class SerializedScriptValue : public ThreadSafeRefCounted<SerializedScriptValue> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(SerializedScriptValue, SerializedScriptValue);
public:
    WEBCORE_EXPORT static ExceptionOr<Ref<SerializedScriptValue>> create(JSC::JSGlobalObject&, JSC::JSValue, Vector<JSC::Strong<JSC::JSObject>>&& transfer, Vector<Ref<MessagePort>>&, SerializationForStorage, SerializationContext = SerializationContext::Default);
    WEBCORE_EXPORT static RefPtr<SerializedScriptValue> create(JSC::JSGlobalObject&, JSC::JSValue, SerializationForStorage, SerializationErrorMode = SerializationErrorMode::Throwing, SerializationContext = SerializationContext::Default);
    WEBCORE_EXPORT static RefPtr<SerializedScriptValue> convert(JSC::JSGlobalObject&, JSC::JSValue);

    WEBCORE_EXPORT static RefPtr<SerializedScriptValue> create(StringView);

    static Ref<SerializedScriptValue> nullValue();

    WEBCORE_EXPORT JSC::JSValue deserialize(JSC::JSGlobalObject&, JSC::JSGlobalObject*, SerializationErrorMode = SerializationErrorMode::Throwing, bool* didFail = nullptr);
    WEBCORE_EXPORT JSC::JSValue deserialize(JSC::JSGlobalObject&, JSC::JSGlobalObject*, Vector<Ref<MessagePort>>&, SerializationErrorMode = SerializationErrorMode::Throwing, bool* didFail = nullptr);
    JSC::JSValue deserialize(JSC::JSGlobalObject&, JSC::JSGlobalObject*, Vector<Ref<MessagePort>>&, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths, SerializationErrorMode = SerializationErrorMode::Throwing, bool* didFail = nullptr);

    WEBCORE_EXPORT String toString() const;

    // API implementation helpers. These don't expose special behavior for ArrayBuffers or MessagePorts.
    WEBCORE_EXPORT static RefPtr<SerializedScriptValue> create(JSContextRef, JSValueRef, JSValueRef* exception);
    WEBCORE_EXPORT JSValueRef deserialize(JSContextRef, JSValueRef* exception);
    WEBCORE_EXPORT static Vector<uint8_t> serializeCryptoKey(const WebCore::CryptoKey&);

    WEBCORE_EXPORT bool hasBlobURLs() const;

    Vector<String> blobURLs() const;
    WEBCORE_EXPORT Vector<URLKeepingBlobAlive> blobHandles() const;
    Vector<FileSystemHandleGlobalIdentifier> fileSystemHandleGlobalIdentifiers() const;
    void writeBlobsToDiskForIndexedDB(bool isEphemeral, CompletionHandler<void(IDBValue&&)>&&);
    IDBValue writeBlobsToDiskForIndexedDBSynchronously(bool isEphemeral, JSC::VM&);
    WEBCORE_EXPORT static Ref<SerializedScriptValue> createFromWireBytes(Vector<uint8_t>&&);
    WEBCORE_EXPORT const Vector<uint8_t>& wireBytes() const LIFETIME_BOUND;

    WEBCORE_EXPORT size_t memoryCost() const;

    using NonSerializedDataToken = WebCore::NonSerializedDataToken;

    WEBCORE_EXPORT std::unique_ptr<Vector<JSC::ArrayBufferContents>>& sharedBufferContentsArray();
    WEBCORE_EXPORT std::optional<NonSerializedDataToken> nonSerializedDataToken() const;
    WEBCORE_EXPORT void setNonSerializedDataToken(std::optional<NonSerializedDataToken>);

    WEBCORE_EXPORT ~SerializedScriptValue();

    enum class DeserializationBehavior : uint8_t { Fail, Succeed, LegacyMapToNull, LegacyMapToUndefined, LegacyMapToEmptyObject };
    WEBCORE_EXPORT static DeserializationBehavior NODELETE deserializationBehavior(JSC::JSObject&);

    WEBCORE_EXPORT Ref<SerializedScriptValue> clone() const;

private:
    friend struct IPC::ArgumentCoder<SerializedScriptValue>;

    static ExceptionOr<Ref<SerializedScriptValue>> create(JSC::JSGlobalObject&, JSC::JSValue, Vector<JSC::Strong<JSC::JSObject>>&& transfer, Vector<Ref<MessagePort>>&, SerializationForStorage, SerializationErrorMode, SerializationContext);

    size_t computeMemoryCost() const;

    using Internals = SerializedScriptValueInternals;
    friend struct IPC::ArgumentCoder<Internals>;

    static Ref<SerializedScriptValue> create(Internals&& internals)
    {
        return adoptRef(*new SerializedScriptValue(WTF::move(internals)));
    }

    static Ref<SerializedScriptValue> create(std::unique_ptr<Internals>&& internals)
    {
        return adoptRef(*new SerializedScriptValue(WTF::move(*internals)));
    }

    WEBCORE_EXPORT explicit SerializedScriptValue(Internals&&);

    std::unique_ptr<Internals> m_internals;
};

}
