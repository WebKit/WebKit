/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SerializedScriptValue.h"

#include "BlobRegistry.h"
#include "CryptoKeyAES.h"
#include "CryptoKeyEC.h"
#include "CryptoKeyHMAC.h"
#include "CryptoKeyRSA.h"
#include "CryptoKeyRSAComponents.h"
#include "CryptoKeyRaw.h"
#include "IDBValue.h"
#include "JSBlob.h"
#include "JSCryptoKey.h"
#include "JSDOMBinding.h"
#include "JSDOMConvertBufferSource.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMMatrix.h"
#include "JSDOMPoint.h"
#include "JSDOMQuad.h"
#include "JSDOMRect.h"
#include "JSFile.h"
#include "JSFileList.h"
#include "JSImageBitmap.h"
#include "JSImageData.h"
#include "JSMessagePort.h"
#include "JSNavigator.h"
#include "JSRTCCertificate.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "SharedBuffer.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/BooleanObject.h>
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/DateInstance.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/IterationKind.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSDataView.h>
#include <JavaScriptCore/JSMap.h>
#include <JavaScriptCore/JSMapIterator.h>
#include <JavaScriptCore/JSSet.h>
#include <JavaScriptCore/JSSetIterator.h>
#include <JavaScriptCore/JSTypedArrays.h>
#include <JavaScriptCore/JSWebAssemblyModule.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/RegExp.h>
#include <JavaScriptCore/RegExpObject.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/TypedArrays.h>
#include <JavaScriptCore/WasmModule.h>
#include <JavaScriptCore/YarrFlags.h>
#include <limits>
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>

#if CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN) || CPU(NEEDS_ALIGNED_ACCESS)
#define ASSUME_LITTLE_ENDIAN 0
#else
#define ASSUME_LITTLE_ENDIAN 1
#endif

namespace WebCore {
using namespace JSC;

static const unsigned maximumFilterRecursion = 40000;

enum class SerializationReturnCode {
    SuccessfullyCompleted,
    StackOverflowError,
    InterruptedExecutionError,
    ValidationError,
    ExistingExceptionError,
    DataCloneError,
    UnspecifiedError
};

enum WalkerState { StateUnknown, ArrayStartState, ArrayStartVisitMember, ArrayEndVisitMember,
    ObjectStartState, ObjectStartVisitMember, ObjectEndVisitMember,
    MapDataStartVisitEntry, MapDataEndVisitKey, MapDataEndVisitValue,
    SetDataStartVisitEntry, SetDataEndVisitKey };

// These can't be reordered, and any new types must be added to the end of the list
// When making changes to these lists please cover your new type(s) in the API test "IndexedDB.StructuredCloneBackwardCompatibility"
enum SerializationTag {
    ArrayTag = 1,
    ObjectTag = 2,
    UndefinedTag = 3,
    NullTag = 4,
    IntTag = 5,
    ZeroTag = 6,
    OneTag = 7,
    FalseTag = 8,
    TrueTag = 9,
    DoubleTag = 10,
    DateTag = 11,
    FileTag = 12,
    FileListTag = 13,
    ImageDataTag = 14,
    BlobTag = 15,
    StringTag = 16,
    EmptyStringTag = 17,
    RegExpTag = 18,
    ObjectReferenceTag = 19,
    MessagePortReferenceTag = 20,
    ArrayBufferTag = 21,
    ArrayBufferViewTag = 22,
    ArrayBufferTransferTag = 23,
    TrueObjectTag = 24,
    FalseObjectTag = 25,
    StringObjectTag = 26,
    EmptyStringObjectTag = 27,
    NumberObjectTag = 28,
    SetObjectTag = 29,
    MapObjectTag = 30,
    NonMapPropertiesTag = 31,
    NonSetPropertiesTag = 32,
#if ENABLE(WEB_CRYPTO)
    CryptoKeyTag = 33,
#endif
    SharedArrayBufferTag = 34,
#if ENABLE(WEBASSEMBLY)
    WasmModuleTag = 35,
#endif
    DOMPointReadOnlyTag = 36,
    DOMPointTag = 37,
    DOMRectReadOnlyTag = 38,
    DOMRectTag = 39,
    DOMMatrixReadOnlyTag = 40,
    DOMMatrixTag = 41,
    DOMQuadTag = 42,
    ImageBitmapTransferTag = 43,
#if ENABLE(WEB_RTC)
    RTCCertificateTag = 44,
#endif
    ErrorTag = 255
};

enum ArrayBufferViewSubtag {
    DataViewTag = 0,
    Int8ArrayTag = 1,
    Uint8ArrayTag = 2,
    Uint8ClampedArrayTag = 3,
    Int16ArrayTag = 4,
    Uint16ArrayTag = 5,
    Int32ArrayTag = 6,
    Uint32ArrayTag = 7,
    Float32ArrayTag = 8,
    Float64ArrayTag = 9
};

static unsigned typedArrayElementSize(ArrayBufferViewSubtag tag)
{
    switch (tag) {
    case DataViewTag:
    case Int8ArrayTag:
    case Uint8ArrayTag:
    case Uint8ClampedArrayTag:
        return 1;
    case Int16ArrayTag:
    case Uint16ArrayTag:
        return 2;
    case Int32ArrayTag:
    case Uint32ArrayTag:
    case Float32ArrayTag:
        return 4;
    case Float64ArrayTag:
        return 8;
    default:
        return 0;
    }

}

#if ENABLE(WEB_CRYPTO)

const uint32_t currentKeyFormatVersion = 1;

enum class CryptoKeyClassSubtag {
    HMAC = 0,
    AES = 1,
    RSA = 2,
    EC = 3,
    Raw = 4,
};
const uint8_t cryptoKeyClassSubtagMaximumValue = 4;

enum class CryptoKeyAsymmetricTypeSubtag {
    Public = 0,
    Private = 1
};
const uint8_t cryptoKeyAsymmetricTypeSubtagMaximumValue = 1;

enum class CryptoKeyUsageTag {
    Encrypt = 0,
    Decrypt = 1,
    Sign = 2,
    Verify = 3,
    DeriveKey = 4,
    DeriveBits = 5,
    WrapKey = 6,
    UnwrapKey = 7
};
const uint8_t cryptoKeyUsageTagMaximumValue = 7;

enum class CryptoAlgorithmIdentifierTag {
    RSAES_PKCS1_v1_5 = 0,
    RSASSA_PKCS1_v1_5 = 1,
    RSA_PSS = 2,
    RSA_OAEP = 3,
    ECDSA = 4,
    ECDH = 5,
    AES_CTR = 6,
    AES_CBC = 7,
    AES_GCM = 9,
    AES_CFB = 10,
    AES_KW = 11,
    HMAC = 12,
    SHA_1 = 14,
    SHA_224 = 15,
    SHA_256 = 16,
    SHA_384 = 17,
    SHA_512 = 18,
    HKDF = 20,
    PBKDF2 = 21,
};
const uint8_t cryptoAlgorithmIdentifierTagMaximumValue = 21;

static unsigned countUsages(CryptoKeyUsageBitmap usages)
{
    // Fast bit count algorithm for sparse bit maps.
    unsigned count = 0;
    while (usages) {
        usages = usages & (usages - 1);
        ++count;
    }
    return count;
}

#endif

/* CurrentVersion tracks the serialization version so that persistent stores
 * are able to correctly bail out in the case of encountering newer formats.
 *
 * Initial version was 1.
 * Version 2. added the ObjectReferenceTag and support for serialization of cyclic graphs.
 * Version 3. added the FalseObjectTag, TrueObjectTag, NumberObjectTag, StringObjectTag
 * and EmptyStringObjectTag for serialization of Boolean, Number and String objects.
 * Version 4. added support for serializing non-index properties of arrays.
 * Version 5. added support for Map and Set types.
 * Version 6. added support for 8-bit strings.
 * Version 7. added support for File's lastModified attribute.
 */
static const unsigned CurrentVersion = 7;
static const unsigned TerminatorTag = 0xFFFFFFFF;
static const unsigned StringPoolTag = 0xFFFFFFFE;
static const unsigned NonIndexPropertiesTag = 0xFFFFFFFD;

// The high bit of a StringData's length determines the character size.
static const unsigned StringDataIs8BitFlag = 0x80000000;

/*
 * Object serialization is performed according to the following grammar, all tags
 * are recorded as a single uint8_t.
 *
 * IndexType (used for the object pool and StringData's constant pool) is the
 * minimum sized unsigned integer type required to represent the maximum index
 * in the constant pool.
 *
 * SerializedValue :- <CurrentVersion:uint32_t> Value
 * Value :- Array | Object | Map | Set | Terminal
 *
 * Array :-
 *     ArrayTag <length:uint32_t>(<index:uint32_t><value:Value>)* TerminatorTag
 *
 * Object :-
 *     ObjectTag (<name:StringData><value:Value>)* TerminatorTag
 *
 * Map :- MapObjectTag MapData
 *
 * Set :- SetObjectTag SetData
 *
 * MapData :- (<key:Value><value:Value>)* NonMapPropertiesTag (<name:StringData><value:Value>)* TerminatorTag
 * SetData :- (<key:Value>)* NonSetPropertiesTag (<name:StringData><value:Value>)* TerminatorTag
 *
 * Terminal :-
 *      UndefinedTag
 *    | NullTag
 *    | IntTag <value:int32_t>
 *    | ZeroTag
 *    | OneTag
 *    | FalseTag
 *    | TrueTag
 *    | FalseObjectTag
 *    | TrueObjectTag
 *    | DoubleTag <value:double>
 *    | NumberObjectTag <value:double>
 *    | DateTag <value:double>
 *    | String
 *    | EmptyStringTag
 *    | EmptyStringObjectTag
 *    | File
 *    | FileList
 *    | ImageData
 *    | Blob
 *    | ObjectReference
 *    | MessagePortReferenceTag <value:uint32_t>
 *    | ArrayBuffer
 *    | ArrayBufferViewTag ArrayBufferViewSubtag <byteOffset:uint32_t> <byteLength:uint32_t> (ArrayBuffer | ObjectReference)
 *    | ArrayBufferTransferTag <value:uint32_t>
 *    | CryptoKeyTag <wrappedKeyLength:uint32_t> <factor:byte{wrappedKeyLength}>
 *    | DOMPoint
 *    | DOMRect
 *    | DOMMatrix
 *    | DOMQuad
 *    | ImageBitmapTransferTag <value:uint32_t>
 *    | RTCCertificateTag
 *
 * Inside certificate, data is serialized in this format as per spec:
 *
 * <expires:double> <certificate:StringData> <origin:StringData> <keyingMaterial:StringData>
 * We also add fingerprints to make sure we expose to JavaScript the same information.
 *
 * Inside wrapped crypto key, data is serialized in this format:
 *
 * <keyFormatVersion:uint32_t> <extractable:int32_t> <usagesCount:uint32_t> <usages:byte{usagesCount}> CryptoKeyClassSubtag (CryptoKeyHMAC | CryptoKeyAES | CryptoKeyRSA)
 *
 * String :-
 *      EmptyStringTag
 *      StringTag StringData
 *
 * StringObject:
 *      EmptyStringObjectTag
 *      StringObjectTag StringData
 *
 * StringData :-
 *      StringPoolTag <cpIndex:IndexType>
 *      (not (TerminatorTag | StringPoolTag))<is8Bit:uint32_t:1><length:uint32_t:31><characters:CharType{length}> // Added to constant pool when seen, string length 0xFFFFFFFF is disallowed
 *
 * File :-
 *    FileTag FileData
 *
 * FileData :-
 *    <path:StringData> <url:StringData> <type:StringData> <name:StringData> <lastModified:double>
 *
 * FileList :-
 *    FileListTag <length:uint32_t>(<file:FileData>){length}
 *
 * ImageData :-
 *    ImageDataTag <width:int32_t><height:int32_t><length:uint32_t><data:uint8_t{length}>
 *
 * Blob :-
 *    BlobTag <url:StringData><type:StringData><size:long long>
 *
 * RegExp :-
 *    RegExpTag <pattern:StringData><flags:StringData>
 *
 * ObjectReference :-
 *    ObjectReferenceTag <opIndex:IndexType>
 *
 * ArrayBuffer :-
 *    ArrayBufferTag <length:uint32_t> <contents:byte{length}>
 *
 * CryptoKeyHMAC :-
 *    <keySize:uint32_t> <keyData:byte{keySize}> CryptoAlgorithmIdentifierTag // Algorithm tag inner hash function.
 *
 * CryptoKeyAES :-
 *    CryptoAlgorithmIdentifierTag <keySize:uint32_t> <keyData:byte{keySize}>
 *
 * CryptoKeyRSA :-
 *    CryptoAlgorithmIdentifierTag <isRestrictedToHash:int32_t> CryptoAlgorithmIdentifierTag? CryptoKeyAsymmetricTypeSubtag CryptoKeyRSAPublicComponents CryptoKeyRSAPrivateComponents?
 *
 * CryptoKeyRSAPublicComponents :-
 *    <modulusSize:uint32_t> <modulus:byte{modulusSize}> <exponentSize:uint32_t> <exponent:byte{exponentSize}>
 *
 * CryptoKeyRSAPrivateComponents :-
 *    <privateExponentSize:uint32_t> <privateExponent:byte{privateExponentSize}> <primeCount:uint32_t> FirstPrimeInfo? PrimeInfo{primeCount - 1}
 *
 * // CRT data could be computed from prime factors. It is only serialized to reuse a code path that's needed for JWK.
 * FirstPrimeInfo :-
 *    <factorSize:uint32_t> <factor:byte{factorSize}> <crtExponentSize:uint32_t> <crtExponent:byte{crtExponentSize}>
 *
 * PrimeInfo :-
 *    <factorSize:uint32_t> <factor:byte{factorSize}> <crtExponentSize:uint32_t> <crtExponent:byte{crtExponentSize}> <crtCoefficientSize:uint32_t> <crtCoefficient:byte{crtCoefficientSize}>
 *
 * CryptoKeyEC :-
 *    CryptoAlgorithmIdentifierTag <namedCurve:StringData> CryptoKeyAsymmetricTypeSubtag <keySize:uint32_t> <keyData:byte{keySize}>
 *
 * CryptoKeyRaw :-
 *    CryptoAlgorithmIdentifierTag <keySize:uint32_t> <keyData:byte{keySize}>
 *
 * DOMPoint :-
 *        DOMPointReadOnlyTag DOMPointData
 *      | DOMPointTag DOMPointData
 *
 * DOMPointData :-
 *      <x:double> <y:double> <z:double> <w:double>
 *
 * DOMRect :-
 *        DOMRectReadOnlyTag DOMRectData
 *      | DOMRectTag DOMRectData
 *
 * DOMRectData :-
 *      <x:double> <y:double> <width:double> <height:double>
 *
 * DOMMatrix :-
 *        DOMMatrixReadOnlyTag DOMMatrixData
 *      | DOMMatrixTag DOMMatrixData
 *
 * DOMMatrixData :-
 *        <is2D:uint8_t:true> <m11:double> <m12:double> <m21:double> <m22:double> <m41:double> <m42:double>
 *      | <is2D:uint8_t:false> <m11:double> <m12:double> <m13:double> <m14:double> <m21:double> <m22:double> <m23:double> <m24:double> <m31:double> <m32:double> <m33:double> <m34:double> <m41:double> <m42:double> <m43:double> <m44:double>
 *
 * DOMQuad :-
 *      DOMQuadTag DOMQuadData
 *
 * DOMQuadData :-
 *      <p1:DOMPointData> <p2:DOMPointData> <p3:DOMPointData> <p4:DOMPointData>
 *
 */

using DeserializationResult = std::pair<JSC::JSValue, SerializationReturnCode>;

class CloneBase {
protected:
    CloneBase(ExecState* exec)
        : m_exec(exec)
        , m_failed(false)
    {
    }

    bool shouldTerminate()
    {
        VM& vm = m_exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        return scope.exception();
    }

    void fail()
    {
        m_failed = true;
    }

    ExecState* m_exec;
    bool m_failed;
    MarkedArgumentBuffer m_gcBuffer;
};

#if ENABLE(WEB_CRYPTO)
static bool wrapCryptoKey(ExecState* exec, const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey)
{
    ScriptExecutionContext* scriptExecutionContext = scriptExecutionContextFromExecState(exec);
    if (!scriptExecutionContext)
        return false;
    return scriptExecutionContext->wrapCryptoKey(key, wrappedKey);
}

static bool unwrapCryptoKey(ExecState* exec, const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key)
{
    ScriptExecutionContext* scriptExecutionContext = scriptExecutionContextFromExecState(exec);
    if (!scriptExecutionContext)
        return false;
    return scriptExecutionContext->unwrapCryptoKey(wrappedKey, key);
}
#endif

#if ASSUME_LITTLE_ENDIAN
template <typename T> static void writeLittleEndian(Vector<uint8_t>& buffer, T value)
{
    buffer.append(reinterpret_cast<uint8_t*>(&value), sizeof(value));
}
#else
template <typename T> static void writeLittleEndian(Vector<uint8_t>& buffer, T value)
{
    for (unsigned i = 0; i < sizeof(T); i++) {
        buffer.append(value & 0xFF);
        value >>= 8;
    }
}
#endif

template <> void writeLittleEndian<uint8_t>(Vector<uint8_t>& buffer, uint8_t value)
{
    buffer.append(value);
}

template <typename T> static bool writeLittleEndian(Vector<uint8_t>& buffer, const T* values, uint32_t length)
{
    if (length > std::numeric_limits<uint32_t>::max() / sizeof(T))
        return false;

#if ASSUME_LITTLE_ENDIAN
    buffer.append(reinterpret_cast<const uint8_t*>(values), length * sizeof(T));
#else
    for (unsigned i = 0; i < length; i++) {
        T value = values[i];
        for (unsigned j = 0; j < sizeof(T); j++) {
            buffer.append(static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }
#endif
    return true;
}

template <> bool writeLittleEndian<uint8_t>(Vector<uint8_t>& buffer, const uint8_t* values, uint32_t length)
{
    buffer.append(values, length);
    return true;
}

class CloneSerializer : CloneBase {
public:
    static SerializationReturnCode serialize(ExecState* exec, JSValue value, Vector<RefPtr<MessagePort>>& messagePorts, Vector<RefPtr<JSC::ArrayBuffer>>& arrayBuffers, const Vector<RefPtr<ImageBitmap>>& imageBitmaps,
#if ENABLE(WEBASSEMBLY)
            WasmModuleArray& wasmModules,
#endif
        Vector<String>& blobURLs, Vector<uint8_t>& out, SerializationContext context, ArrayBufferContentsArray& sharedBuffers)
    {
        CloneSerializer serializer(exec, messagePorts, arrayBuffers, imageBitmaps,
#if ENABLE(WEBASSEMBLY)
            wasmModules,
#endif
            blobURLs, out, context, sharedBuffers);
        return serializer.serialize(value);
    }

    static bool serialize(StringView string, Vector<uint8_t>& out)
    {
        writeLittleEndian(out, CurrentVersion);
        if (string.isEmpty()) {
            writeLittleEndian<uint8_t>(out, EmptyStringTag);
            return true;
        }
        writeLittleEndian<uint8_t>(out, StringTag);
        if (string.is8Bit()) {
            writeLittleEndian(out, string.length() | StringDataIs8BitFlag);
            return writeLittleEndian(out, string.characters8(), string.length());
        }
        writeLittleEndian(out, string.length());
        return writeLittleEndian(out, string.characters16(), string.length());
    }

private:
    typedef HashMap<JSObject*, uint32_t> ObjectPool;

    CloneSerializer(ExecState* exec, Vector<RefPtr<MessagePort>>& messagePorts, Vector<RefPtr<JSC::ArrayBuffer>>& arrayBuffers, const Vector<RefPtr<ImageBitmap>>& imageBitmaps,
#if ENABLE(WEBASSEMBLY)
            WasmModuleArray& wasmModules,
#endif
        Vector<String>& blobURLs, Vector<uint8_t>& out, SerializationContext context, ArrayBufferContentsArray& sharedBuffers)
        : CloneBase(exec)
        , m_buffer(out)
        , m_blobURLs(blobURLs)
        , m_emptyIdentifier(Identifier::fromString(exec->vm(), emptyString()))
        , m_context(context)
        , m_sharedBuffers(sharedBuffers)
#if ENABLE(WEBASSEMBLY)
        , m_wasmModules(wasmModules)
#endif
    {
        write(CurrentVersion);
        fillTransferMap(messagePorts, m_transferredMessagePorts);
        fillTransferMap(arrayBuffers, m_transferredArrayBuffers);
        fillTransferMap(imageBitmaps, m_transferredImageBitmaps);
    }

    template <class T>
    void fillTransferMap(const Vector<RefPtr<T>>& input, ObjectPool& result)
    {
        if (input.isEmpty())
            return;
        JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(m_exec->lexicalGlobalObject());
        for (size_t i = 0; i < input.size(); i++) {
            JSC::JSValue value = toJS(m_exec, globalObject, input[i].get());
            JSC::JSObject* obj = value.getObject();
            if (obj && !result.contains(obj))
                result.add(obj, i);
        }
    }

    SerializationReturnCode serialize(JSValue in);

    bool isArray(VM& vm, JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return object->inherits<JSArray>(vm);
    }

    bool isMap(VM& vm, JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return object->inherits<JSMap>(vm);
    }
    bool isSet(VM& vm, JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return object->inherits<JSSet>(vm);
    }

    bool checkForDuplicate(JSObject* object)
    {
        // Record object for graph reconstruction
        ObjectPool::const_iterator found = m_objectPool.find(object);

        // Handle duplicate references
        if (found != m_objectPool.end()) {
            write(ObjectReferenceTag);
            ASSERT(found->value < m_objectPool.size());
            writeObjectIndex(found->value);
            return true;
        }

        return false;
    }

    void recordObject(JSObject* object)
    {
        m_objectPool.add(object, m_objectPool.size());
        m_gcBuffer.appendWithCrashOnOverflow(object);
    }

    bool startObjectInternal(JSObject* object)
    {
        if (checkForDuplicate(object))
            return false;
        recordObject(object);
        return true;
    }

    bool startObject(JSObject* object)
    {
        if (!startObjectInternal(object))
            return false;
        write(ObjectTag);
        return true;
    }

    bool startArray(JSArray* array)
    {
        if (!startObjectInternal(array))
            return false;

        unsigned length = array->length();
        write(ArrayTag);
        write(length);
        return true;
    }

    bool startSet(JSSet* set)
    {
        if (!startObjectInternal(set))
            return false;

        write(SetObjectTag);
        return true;
    }

    bool startMap(JSMap* map)
    {
        if (!startObjectInternal(map))
            return false;

        write(MapObjectTag);
        return true;
    }

    void endObject()
    {
        write(TerminatorTag);
    }

    JSValue getProperty(VM& vm, JSObject* object, const Identifier& propertyName)
    {
        PropertySlot slot(object, PropertySlot::InternalMethodType::Get);
        if (object->methodTable(vm)->getOwnPropertySlot(object, m_exec, propertyName, slot))
            return slot.getValue(m_exec, propertyName);
        return JSValue();
    }

    void dumpImmediate(JSValue value)
    {
        if (value.isNull())
            write(NullTag);
        else if (value.isUndefined())
            write(UndefinedTag);
        else if (value.isNumber()) {
            if (value.isInt32()) {
                if (!value.asInt32())
                    write(ZeroTag);
                else if (value.asInt32() == 1)
                    write(OneTag);
                else {
                    write(IntTag);
                    write(static_cast<uint32_t>(value.asInt32()));
                }
            } else {
                write(DoubleTag);
                write(value.asDouble());
            }
        } else if (value.isBoolean()) {
            if (value.isTrue())
                write(TrueTag);
            else
                write(FalseTag);
        }
    }

    void dumpString(const String& string)
    {
        if (string.isEmpty())
            write(EmptyStringTag);
        else {
            write(StringTag);
            write(string);
        }
    }

    void dumpStringObject(const String& string)
    {
        if (string.isEmpty())
            write(EmptyStringObjectTag);
        else {
            write(StringObjectTag);
            write(string);
        }
    }

    JSC::JSValue toJSArrayBuffer(ArrayBuffer& arrayBuffer)
    {
        auto& vm = m_exec->vm();
        auto* globalObject = m_exec->lexicalGlobalObject();
        if (globalObject->inherits<JSDOMGlobalObject>(vm))
            return toJS(m_exec, jsCast<JSDOMGlobalObject*>(globalObject), &arrayBuffer);

        if (auto* buffer = arrayBuffer.m_wrapper.get())
            return buffer;

        return JSC::JSArrayBuffer::create(vm, globalObject->arrayBufferStructure(arrayBuffer.sharingMode()), &arrayBuffer);
    }

    bool dumpArrayBufferView(JSObject* obj, SerializationReturnCode& code)
    {
        VM& vm = m_exec->vm();
        write(ArrayBufferViewTag);
        if (obj->inherits<JSDataView>(vm))
            write(DataViewTag);
        else if (obj->inherits<JSUint8ClampedArray>(vm))
            write(Uint8ClampedArrayTag);
        else if (obj->inherits<JSInt8Array>(vm))
            write(Int8ArrayTag);
        else if (obj->inherits<JSUint8Array>(vm))
            write(Uint8ArrayTag);
        else if (obj->inherits<JSInt16Array>(vm))
            write(Int16ArrayTag);
        else if (obj->inherits<JSUint16Array>(vm))
            write(Uint16ArrayTag);
        else if (obj->inherits<JSInt32Array>(vm))
            write(Int32ArrayTag);
        else if (obj->inherits<JSUint32Array>(vm))
            write(Uint32ArrayTag);
        else if (obj->inherits<JSFloat32Array>(vm))
            write(Float32ArrayTag);
        else if (obj->inherits<JSFloat64Array>(vm))
            write(Float64ArrayTag);
        else
            return false;

        RefPtr<ArrayBufferView> arrayBufferView = toPossiblySharedArrayBufferView(vm, obj);
        write(static_cast<uint32_t>(arrayBufferView->byteOffset()));
        write(static_cast<uint32_t>(arrayBufferView->byteLength()));
        RefPtr<ArrayBuffer> arrayBuffer = arrayBufferView->possiblySharedBuffer();
        if (!arrayBuffer) {
            code = SerializationReturnCode::ValidationError;
            return true;
        }

        return dumpIfTerminal(toJSArrayBuffer(*arrayBuffer), code);
    }

    void dumpDOMPoint(const DOMPointReadOnly& point)
    {
        write(point.x());
        write(point.y());
        write(point.z());
        write(point.w());
    }

    void dumpDOMPoint(JSObject* obj)
    {
        VM& vm = m_exec->vm();
        if (obj->inherits<JSDOMPoint>(vm))
            write(DOMPointTag);
        else
            write(DOMPointReadOnlyTag);

        dumpDOMPoint(jsCast<JSDOMPointReadOnly*>(obj)->wrapped());
    }

    void dumpDOMRect(JSObject* obj)
    {
        VM& vm = m_exec->vm();
        if (obj->inherits<JSDOMRect>(vm))
            write(DOMRectTag);
        else
            write(DOMRectReadOnlyTag);

        auto& rect = jsCast<JSDOMRectReadOnly*>(obj)->wrapped();
        write(rect.x());
        write(rect.y());
        write(rect.width());
        write(rect.height());
    }

    void dumpDOMMatrix(JSObject* obj)
    {
        VM& vm = m_exec->vm();
        if (obj->inherits<JSDOMMatrix>(vm))
            write(DOMMatrixTag);
        else
            write(DOMMatrixReadOnlyTag);

        auto& matrix = jsCast<JSDOMMatrixReadOnly*>(obj)->wrapped();
        bool is2D = matrix.is2D();
        write(static_cast<uint8_t>(is2D));
        if (is2D) {
            write(matrix.m11());
            write(matrix.m12());
            write(matrix.m21());
            write(matrix.m22());
            write(matrix.m41());
            write(matrix.m42());
        } else {
            write(matrix.m11());
            write(matrix.m12());
            write(matrix.m13());
            write(matrix.m14());
            write(matrix.m21());
            write(matrix.m22());
            write(matrix.m23());
            write(matrix.m24());
            write(matrix.m31());
            write(matrix.m32());
            write(matrix.m33());
            write(matrix.m34());
            write(matrix.m41());
            write(matrix.m42());
            write(matrix.m43());
            write(matrix.m44());
        }
    }

    void dumpDOMQuad(JSObject* obj)
    {
        write(DOMQuadTag);

        auto& quad = jsCast<JSDOMQuad*>(obj)->wrapped();
        dumpDOMPoint(quad.p1());
        dumpDOMPoint(quad.p2());
        dumpDOMPoint(quad.p3());
        dumpDOMPoint(quad.p4());
    }

    void dumpImageBitmap(JSObject* obj, SerializationReturnCode& code)
    {
        auto index = m_transferredImageBitmaps.find(obj);
        if (index != m_transferredImageBitmaps.end()) {
            write(ImageBitmapTransferTag);
            write(index->value);
            return;
        }

        // Copying ImageBitmaps is not yet supported.
        code = SerializationReturnCode::ValidationError;
    }

    bool dumpIfTerminal(JSValue value, SerializationReturnCode& code)
    {
        if (!value.isCell()) {
            dumpImmediate(value);
            return true;
        }
        ASSERT(value.isCell());

        if (value.isString()) {
            dumpString(asString(value)->value(m_exec));
            return true;
        }

        if (value.isSymbol()) {
            code = SerializationReturnCode::DataCloneError;
            return true;
        }

        VM& vm = m_exec->vm();
        if (isArray(vm, value))
            return false;

        if (value.isObject()) {
            auto* obj = asObject(value);
            if (auto* dateObject = jsDynamicCast<DateInstance*>(vm, obj)) {
                write(DateTag);
                write(dateObject->internalNumber());
                return true;
            }
            if (auto* booleanObject = jsDynamicCast<BooleanObject*>(vm, obj)) {
                if (!startObjectInternal(booleanObject)) // handle duplicates
                    return true;
                write(booleanObject->internalValue().toBoolean(m_exec) ? TrueObjectTag : FalseObjectTag);
                return true;
            }
            if (auto* stringObject = jsDynamicCast<StringObject*>(vm, obj)) {
                if (!startObjectInternal(stringObject)) // handle duplicates
                    return true;
                String str = asString(stringObject->internalValue())->value(m_exec);
                dumpStringObject(str);
                return true;
            }
            if (auto* numberObject = jsDynamicCast<NumberObject*>(vm, obj)) {
                if (!startObjectInternal(numberObject)) // handle duplicates
                    return true;
                write(NumberObjectTag);
                write(numberObject->internalValue().asNumber());
                return true;
            }
            if (auto* file = JSFile::toWrapped(vm, obj)) {
                write(FileTag);
                write(*file);
                return true;
            }
            if (auto* list = JSFileList::toWrapped(vm, obj)) {
                write(FileListTag);
                write(list->length());
                for (auto& file : list->files())
                    write(file.get());
                return true;
            }
            if (auto* blob = JSBlob::toWrapped(vm, obj)) {
                write(BlobTag);
                m_blobURLs.append(blob->url());
                write(blob->url());
                write(blob->type());
                write(blob->size());
                return true;
            }
            if (auto* data = JSImageData::toWrapped(vm, obj)) {
                write(ImageDataTag);
                write(data->width());
                write(data->height());
                write(data->data()->length());
                write(data->data()->data(), data->data()->length());
                return true;
            }
            if (auto* regExp = jsDynamicCast<RegExpObject*>(vm, obj)) {
                char flags[3];
                int flagCount = 0;
                if (regExp->regExp()->global())
                    flags[flagCount++] = 'g';
                if (regExp->regExp()->ignoreCase())
                    flags[flagCount++] = 'i';
                if (regExp->regExp()->multiline())
                    flags[flagCount++] = 'm';
                write(RegExpTag);
                write(regExp->regExp()->pattern());
                write(String(flags, flagCount));
                return true;
            }
            if (obj->inherits<JSMessagePort>(vm)) {
                auto index = m_transferredMessagePorts.find(obj);
                if (index != m_transferredMessagePorts.end()) {
                    write(MessagePortReferenceTag);
                    write(index->value);
                    return true;
                }
                // MessagePort object could not be found in transferred message ports
                code = SerializationReturnCode::ValidationError;
                return true;
            }
            if (auto* arrayBuffer = toPossiblySharedArrayBuffer(vm, obj)) {
                if (arrayBuffer->isNeutered()) {
                    code = SerializationReturnCode::ValidationError;
                    return true;
                }
                auto index = m_transferredArrayBuffers.find(obj);
                if (index != m_transferredArrayBuffers.end()) {
                    write(ArrayBufferTransferTag);
                    write(index->value);
                    return true;
                }
                if (!startObjectInternal(obj)) // handle duplicates
                    return true;
                
                if (arrayBuffer->isShared() && m_context == SerializationContext::WorkerPostMessage) {
                    uint32_t index = m_sharedBuffers.size();
                    ArrayBufferContents contents;
                    if (arrayBuffer->shareWith(contents)) {
                        write(SharedArrayBufferTag);
                        m_sharedBuffers.append(WTFMove(contents));
                        write(index);
                        return true;
                    }
                }
                
                write(ArrayBufferTag);
                write(arrayBuffer->byteLength());
                write(static_cast<const uint8_t*>(arrayBuffer->data()), arrayBuffer->byteLength());
                return true;
            }
            if (obj->inherits<JSArrayBufferView>(vm)) {
                if (checkForDuplicate(obj))
                    return true;
                bool success = dumpArrayBufferView(obj, code);
                recordObject(obj);
                return success;
            }
#if ENABLE(WEB_CRYPTO)
            if (auto* key = JSCryptoKey::toWrapped(vm, obj)) {
                write(CryptoKeyTag);
                Vector<uint8_t> serializedKey;
                Vector<String> dummyBlobURLs;
                Vector<RefPtr<MessagePort>> dummyMessagePorts;
                Vector<RefPtr<JSC::ArrayBuffer>> dummyArrayBuffers;
#if ENABLE(WEBASSEMBLY)
                WasmModuleArray dummyModules;
#endif
                ArrayBufferContentsArray dummySharedBuffers;
                CloneSerializer rawKeySerializer(m_exec, dummyMessagePorts, dummyArrayBuffers, { },
#if ENABLE(WEBASSEMBLY)
                    dummyModules,
#endif
                    dummyBlobURLs, serializedKey, SerializationContext::Default, dummySharedBuffers);
                rawKeySerializer.write(key);
                Vector<uint8_t> wrappedKey;
                if (!wrapCryptoKey(m_exec, serializedKey, wrappedKey))
                    return false;
                write(wrappedKey);
                return true;
            }
#endif
#if ENABLE(WEB_RTC)
            if (auto* rtcCertificate = JSRTCCertificate::toWrapped(vm, obj)) {
                write(RTCCertificateTag);
                write(rtcCertificate->expires());
                write(rtcCertificate->pemCertificate());
                write(rtcCertificate->origin().toString());
                write(rtcCertificate->pemPrivateKey());
                write(static_cast<unsigned>(rtcCertificate->getFingerprints().size()));
                for (const auto& fingerprint : rtcCertificate->getFingerprints()) {
                    write(fingerprint.algorithm);
                    write(fingerprint.value);
                }
                return true;
            }
#endif
#if ENABLE(WEBASSEMBLY)
            if (JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(vm, obj)) {
                if (m_context != SerializationContext::WorkerPostMessage && m_context != SerializationContext::WindowPostMessage)
                    return false;

                uint32_t index = m_wasmModules.size(); 
                m_wasmModules.append(makeRef(module->module()));
                write(WasmModuleTag);
                write(index);
                return true;
            }
#endif
            if (obj->inherits<JSDOMPointReadOnly>(vm)) {
                dumpDOMPoint(obj);
                return true;
            }
            if (obj->inherits<JSDOMRectReadOnly>(vm)) {
                dumpDOMRect(obj);
                return true;
            }
            if (obj->inherits<JSDOMMatrixReadOnly>(vm)) {
                dumpDOMMatrix(obj);
                return true;
            }
            if (obj->inherits<JSDOMQuad>(vm)) {
                dumpDOMQuad(obj);
                return true;
            }
            if (obj->inherits(vm, JSImageBitmap::info())) {
                dumpImageBitmap(obj, code);
                return true;
            }
            return false;
        }
        // Any other types are expected to serialize as null.
        write(NullTag);
        return true;
    }

    void write(SerializationTag tag)
    {
        writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(ArrayBufferViewSubtag tag)
    {
        writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

#if ENABLE(WEB_CRYPTO)
    void write(CryptoKeyClassSubtag tag)
    {
        writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(CryptoKeyAsymmetricTypeSubtag tag)
    {
        writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(CryptoKeyUsageTag tag)
    {
        writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(CryptoAlgorithmIdentifierTag tag)
    {
        writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }
#endif

    void write(uint8_t c)
    {
        writeLittleEndian(m_buffer, c);
    }

    void write(uint32_t i)
    {
        writeLittleEndian(m_buffer, i);
    }

    void write(double d)
    {
        union {
            double d;
            int64_t i;
        } u;
        u.d = d;
        writeLittleEndian(m_buffer, u.i);
    }

    void write(int32_t i)
    {
        writeLittleEndian(m_buffer, i);
    }

    void write(unsigned long long i)
    {
        writeLittleEndian(m_buffer, i);
    }
    
    void write(uint16_t ch)
    {
        writeLittleEndian(m_buffer, ch);
    }

    void writeStringIndex(unsigned i)
    {
        writeConstantPoolIndex(m_constantPool, i);
    }
    
    void writeObjectIndex(unsigned i)
    {
        writeConstantPoolIndex(m_objectPool, i);
    }

    template <class T> void writeConstantPoolIndex(const T& constantPool, unsigned i)
    {
        ASSERT(i < constantPool.size());
        if (constantPool.size() <= 0xFF)
            write(static_cast<uint8_t>(i));
        else if (constantPool.size() <= 0xFFFF)
            write(static_cast<uint16_t>(i));
        else
            write(static_cast<uint32_t>(i));
    }

    void write(const Identifier& ident)
    {
        const String& str = ident.string();
        StringConstantPool::AddResult addResult = m_constantPool.add(ident.impl(), m_constantPool.size());
        if (!addResult.isNewEntry) {
            write(StringPoolTag);
            writeStringIndex(addResult.iterator->value);
            return;
        }

        unsigned length = str.length();

        // Guard against overflow
        if (length > (std::numeric_limits<uint32_t>::max() - sizeof(uint32_t)) / sizeof(UChar)) {
            fail();
            return;
        }

        if (str.is8Bit())
            writeLittleEndian<uint32_t>(m_buffer, length | StringDataIs8BitFlag);
        else
            writeLittleEndian<uint32_t>(m_buffer, length);

        if (!length)
            return;
        if (str.is8Bit()) {
            if (!writeLittleEndian(m_buffer, str.characters8(), length))
                fail();
            return;
        }
        if (!writeLittleEndian(m_buffer, str.characters16(), length))
            fail();
    }

    void write(const String& str)
    {
        if (str.isNull())
            write(m_emptyIdentifier);
        else
            write(Identifier::fromString(m_exec->vm(), str));
    }

    void write(const Vector<uint8_t>& vector)
    {
        uint32_t size = vector.size();
        write(size);
        writeLittleEndian(m_buffer, vector.data(), size);
    }

    void write(const File& file)
    {
        m_blobURLs.append(file.url());
        write(file.path());
        write(file.url());
        write(file.type());
        write(file.name());
        write(static_cast<double>(file.lastModifiedOverride().valueOr(-1)));
    }

#if ENABLE(WEB_CRYPTO)
    void write(CryptoAlgorithmIdentifier algorithm)
    {
        switch (algorithm) {
        case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
            write(CryptoAlgorithmIdentifierTag::RSAES_PKCS1_v1_5);
            break;
        case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
            write(CryptoAlgorithmIdentifierTag::RSASSA_PKCS1_v1_5);
            break;
        case CryptoAlgorithmIdentifier::RSA_PSS:
            write(CryptoAlgorithmIdentifierTag::RSA_PSS);
            break;
        case CryptoAlgorithmIdentifier::RSA_OAEP:
            write(CryptoAlgorithmIdentifierTag::RSA_OAEP);
            break;
        case CryptoAlgorithmIdentifier::ECDSA:
            write(CryptoAlgorithmIdentifierTag::ECDSA);
            break;
        case CryptoAlgorithmIdentifier::ECDH:
            write(CryptoAlgorithmIdentifierTag::ECDH);
            break;
        case CryptoAlgorithmIdentifier::AES_CTR:
            write(CryptoAlgorithmIdentifierTag::AES_CTR);
            break;
        case CryptoAlgorithmIdentifier::AES_CBC:
            write(CryptoAlgorithmIdentifierTag::AES_CBC);
            break;
        case CryptoAlgorithmIdentifier::AES_GCM:
            write(CryptoAlgorithmIdentifierTag::AES_GCM);
            break;
        case CryptoAlgorithmIdentifier::AES_CFB:
            write(CryptoAlgorithmIdentifierTag::AES_CFB);
            break;
        case CryptoAlgorithmIdentifier::AES_KW:
            write(CryptoAlgorithmIdentifierTag::AES_KW);
            break;
        case CryptoAlgorithmIdentifier::HMAC:
            write(CryptoAlgorithmIdentifierTag::HMAC);
            break;
        case CryptoAlgorithmIdentifier::SHA_1:
            write(CryptoAlgorithmIdentifierTag::SHA_1);
            break;
        case CryptoAlgorithmIdentifier::SHA_224:
            write(CryptoAlgorithmIdentifierTag::SHA_224);
            break;
        case CryptoAlgorithmIdentifier::SHA_256:
            write(CryptoAlgorithmIdentifierTag::SHA_256);
            break;
        case CryptoAlgorithmIdentifier::SHA_384:
            write(CryptoAlgorithmIdentifierTag::SHA_384);
            break;
        case CryptoAlgorithmIdentifier::SHA_512:
            write(CryptoAlgorithmIdentifierTag::SHA_512);
            break;
        case CryptoAlgorithmIdentifier::HKDF:
            write(CryptoAlgorithmIdentifierTag::HKDF);
            break;
        case CryptoAlgorithmIdentifier::PBKDF2:
            write(CryptoAlgorithmIdentifierTag::PBKDF2);
            break;
        }
    }

    void write(CryptoKeyRSAComponents::Type type)
    {
        switch (type) {
        case CryptoKeyRSAComponents::Type::Public:
            write(CryptoKeyAsymmetricTypeSubtag::Public);
            return;
        case CryptoKeyRSAComponents::Type::Private:
            write(CryptoKeyAsymmetricTypeSubtag::Private);
            return;
        }
    }

    void write(const CryptoKeyRSAComponents& key)
    {
        write(key.type());
        write(key.modulus());
        write(key.exponent());
        if (key.type() == CryptoKeyRSAComponents::Type::Public)
            return;

        write(key.privateExponent());

        unsigned primeCount = key.hasAdditionalPrivateKeyParameters() ? key.otherPrimeInfos().size() + 2 : 0;
        write(primeCount);
        if (!primeCount)
            return;

        write(key.firstPrimeInfo().primeFactor);
        write(key.firstPrimeInfo().factorCRTExponent);
        write(key.secondPrimeInfo().primeFactor);
        write(key.secondPrimeInfo().factorCRTExponent);
        write(key.secondPrimeInfo().factorCRTCoefficient);
        for (unsigned i = 2; i < primeCount; ++i) {
            write(key.otherPrimeInfos()[i].primeFactor);
            write(key.otherPrimeInfos()[i].factorCRTExponent);
            write(key.otherPrimeInfos()[i].factorCRTCoefficient);
        }
    }

    void write(const CryptoKey* key)
    {
        write(currentKeyFormatVersion);

        write(key->extractable());

        CryptoKeyUsageBitmap usages = key->usagesBitmap();
        write(countUsages(usages));
        if (usages & CryptoKeyUsageEncrypt)
            write(CryptoKeyUsageTag::Encrypt);
        if (usages & CryptoKeyUsageDecrypt)
            write(CryptoKeyUsageTag::Decrypt);
        if (usages & CryptoKeyUsageSign)
            write(CryptoKeyUsageTag::Sign);
        if (usages & CryptoKeyUsageVerify)
            write(CryptoKeyUsageTag::Verify);
        if (usages & CryptoKeyUsageDeriveKey)
            write(CryptoKeyUsageTag::DeriveKey);
        if (usages & CryptoKeyUsageDeriveBits)
            write(CryptoKeyUsageTag::DeriveBits);
        if (usages & CryptoKeyUsageWrapKey)
            write(CryptoKeyUsageTag::WrapKey);
        if (usages & CryptoKeyUsageUnwrapKey)
            write(CryptoKeyUsageTag::UnwrapKey);

        switch (key->keyClass()) {
        case CryptoKeyClass::HMAC:
            write(CryptoKeyClassSubtag::HMAC);
            write(downcast<CryptoKeyHMAC>(*key).key());
            write(downcast<CryptoKeyHMAC>(*key).hashAlgorithmIdentifier());
            break;
        case CryptoKeyClass::AES:
            write(CryptoKeyClassSubtag::AES);
            write(key->algorithmIdentifier());
            write(downcast<CryptoKeyAES>(*key).key());
            break;
        case CryptoKeyClass::EC:
            write(CryptoKeyClassSubtag::EC);
            write(key->algorithmIdentifier());
            write(downcast<CryptoKeyEC>(*key).namedCurveString());
            switch (key->type()) {
            case CryptoKey::Type::Public: {
                write(CryptoKeyAsymmetricTypeSubtag::Public);
                auto result = downcast<CryptoKeyEC>(*key).exportRaw();
                ASSERT(!result.hasException());
                write(result.releaseReturnValue());
                break;
            }
            case CryptoKey::Type::Private: {
                write(CryptoKeyAsymmetricTypeSubtag::Private);
                // Use the standard complied method is not very efficient, but simple/reliable.
                auto result = downcast<CryptoKeyEC>(*key).exportPkcs8();
                ASSERT(!result.hasException());
                write(result.releaseReturnValue());
                break;
            }
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        case CryptoKeyClass::Raw:
            write(CryptoKeyClassSubtag::Raw);
            write(key->algorithmIdentifier());
            write(downcast<CryptoKeyRaw>(*key).key());
            break;
        case CryptoKeyClass::RSA:
            write(CryptoKeyClassSubtag::RSA);
            write(key->algorithmIdentifier());
            CryptoAlgorithmIdentifier hash;
            bool isRestrictedToHash = downcast<CryptoKeyRSA>(*key).isRestrictedToHash(hash);
            write(isRestrictedToHash);
            if (isRestrictedToHash)
                write(hash);
            write(*downcast<CryptoKeyRSA>(*key).exportData());
            break;
        }
    }
#endif

    void write(const uint8_t* data, unsigned length)
    {
        m_buffer.append(data, length);
    }

    Vector<uint8_t>& m_buffer;
    Vector<String>& m_blobURLs;
    ObjectPool m_objectPool;
    ObjectPool m_transferredMessagePorts;
    ObjectPool m_transferredArrayBuffers;
    ObjectPool m_transferredImageBitmaps;
    typedef HashMap<RefPtr<UniquedStringImpl>, uint32_t, IdentifierRepHash> StringConstantPool;
    StringConstantPool m_constantPool;
    Identifier m_emptyIdentifier;
    SerializationContext m_context;
    ArrayBufferContentsArray& m_sharedBuffers;
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray& m_wasmModules;
#endif
};

SerializationReturnCode CloneSerializer::serialize(JSValue in)
{
    VM& vm = m_exec->vm();
    Vector<uint32_t, 16> indexStack;
    Vector<uint32_t, 16> lengthStack;
    Vector<PropertyNameArray, 16> propertyStack;
    Vector<JSObject*, 32> inputObjectStack;
    Vector<JSMapIterator*, 4> mapIteratorStack;
    Vector<JSSetIterator*, 4> setIteratorStack;
    Vector<JSValue, 4> mapIteratorValueStack;
    Vector<WalkerState, 16> stateStack;
    WalkerState state = StateUnknown;
    JSValue inValue = in;
    while (1) {
        switch (state) {
            arrayStartState:
            case ArrayStartState: {
                ASSERT(isArray(vm, inValue));
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;

                JSArray* inArray = asArray(inValue);
                unsigned length = inArray->length();
                if (!startArray(inArray))
                    break;
                inputObjectStack.append(inArray);
                indexStack.append(0);
                lengthStack.append(length);
            }
            arrayStartVisitMember:
            FALLTHROUGH;
            case ArrayStartVisitMember: {
                JSObject* array = inputObjectStack.last();
                uint32_t index = indexStack.last();
                if (index == lengthStack.last()) {
                    indexStack.removeLast();
                    lengthStack.removeLast();

                    propertyStack.append(PropertyNameArray(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    array->methodTable(vm)->getOwnNonIndexPropertyNames(array, m_exec, propertyStack.last(), EnumerationMode());
                    if (propertyStack.last().size()) {
                        write(NonIndexPropertiesTag);
                        indexStack.append(0);
                        goto objectStartVisitMember;
                    }
                    propertyStack.removeLast();

                    endObject();
                    inputObjectStack.removeLast();
                    break;
                }
                inValue = array->getDirectIndex(m_exec, index);
                if (!inValue) {
                    indexStack.last()++;
                    goto arrayStartVisitMember;
                }

                write(index);
                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                if (dumpIfTerminal(inValue, terminalCode)) {
                    if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                        return terminalCode;
                    indexStack.last()++;
                    goto arrayStartVisitMember;
                }
                stateStack.append(ArrayEndVisitMember);
                goto stateUnknown;
            }
            case ArrayEndVisitMember: {
                indexStack.last()++;
                goto arrayStartVisitMember;
            }
            objectStartState:
            case ObjectStartState: {
                ASSERT(inValue.isObject());
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;
                JSObject* inObject = asObject(inValue);
                if (!startObject(inObject))
                    break;
                // At this point, all supported objects other than Object
                // objects have been handled. If we reach this point and
                // the input is not an Object object then we should throw
                // a DataCloneError.
                if (inObject->classInfo(vm) != JSFinalObject::info())
                    return SerializationReturnCode::DataCloneError;
                inputObjectStack.append(inObject);
                indexStack.append(0);
                propertyStack.append(PropertyNameArray(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                inObject->methodTable(vm)->getOwnPropertyNames(inObject, m_exec, propertyStack.last(), EnumerationMode());
            }
            objectStartVisitMember:
            FALLTHROUGH;
            case ObjectStartVisitMember: {
                JSObject* object = inputObjectStack.last();
                uint32_t index = indexStack.last();
                PropertyNameArray& properties = propertyStack.last();
                if (index == properties.size()) {
                    endObject();
                    inputObjectStack.removeLast();
                    indexStack.removeLast();
                    propertyStack.removeLast();
                    break;
                }
                inValue = getProperty(vm, object, properties[index]);
                if (shouldTerminate())
                    return SerializationReturnCode::ExistingExceptionError;

                if (!inValue) {
                    // Property was removed during serialisation
                    indexStack.last()++;
                    goto objectStartVisitMember;
                }
                write(properties[index]);

                if (shouldTerminate())
                    return SerializationReturnCode::ExistingExceptionError;

                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                if (!dumpIfTerminal(inValue, terminalCode)) {
                    stateStack.append(ObjectEndVisitMember);
                    goto stateUnknown;
                }
                if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                    return terminalCode;
                FALLTHROUGH;
            }
            case ObjectEndVisitMember: {
                if (shouldTerminate())
                    return SerializationReturnCode::ExistingExceptionError;

                indexStack.last()++;
                goto objectStartVisitMember;
            }
            mapStartState: {
                ASSERT(inValue.isObject());
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;
                JSMap* inMap = jsCast<JSMap*>(inValue);
                if (!startMap(inMap))
                    break;
                JSMapIterator* iterator = JSMapIterator::create(vm, vm.mapIteratorStructure(), inMap, IterateKeyValue);
                m_gcBuffer.appendWithCrashOnOverflow(inMap);
                m_gcBuffer.appendWithCrashOnOverflow(iterator);
                mapIteratorStack.append(iterator);
                inputObjectStack.append(inMap);
                goto mapDataStartVisitEntry;
            }
            mapDataStartVisitEntry:
            case MapDataStartVisitEntry: {
                JSMapIterator* iterator = mapIteratorStack.last();
                JSValue key, value;
                if (!iterator->nextKeyValue(m_exec, key, value)) {
                    mapIteratorStack.removeLast();
                    JSObject* object = inputObjectStack.last();
                    ASSERT(jsDynamicCast<JSMap*>(vm, object));
                    propertyStack.append(PropertyNameArray(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    object->methodTable(vm)->getOwnPropertyNames(object, m_exec, propertyStack.last(), EnumerationMode());
                    write(NonMapPropertiesTag);
                    indexStack.append(0);
                    goto objectStartVisitMember;
                }
                inValue = key;
                m_gcBuffer.appendWithCrashOnOverflow(value);
                mapIteratorValueStack.append(value);
                stateStack.append(MapDataEndVisitKey);
                goto stateUnknown;
            }
            case MapDataEndVisitKey: {
                inValue = mapIteratorValueStack.last();
                mapIteratorValueStack.removeLast();
                stateStack.append(MapDataEndVisitValue);
                goto stateUnknown;
            }
            case MapDataEndVisitValue: {
                goto mapDataStartVisitEntry;
            }

            setStartState: {
                ASSERT(inValue.isObject());
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;
                JSSet* inSet = jsCast<JSSet*>(inValue);
                if (!startSet(inSet))
                    break;
                JSSetIterator* iterator = JSSetIterator::create(vm, vm.setIteratorStructure(), inSet, IterateKey);
                m_gcBuffer.appendWithCrashOnOverflow(inSet);
                m_gcBuffer.appendWithCrashOnOverflow(iterator);
                setIteratorStack.append(iterator);
                inputObjectStack.append(inSet);
                goto setDataStartVisitEntry;
            }
            setDataStartVisitEntry:
            case SetDataStartVisitEntry: {
                JSSetIterator* iterator = setIteratorStack.last();
                JSValue key;
                if (!iterator->next(m_exec, key)) {
                    setIteratorStack.removeLast();
                    JSObject* object = inputObjectStack.last();
                    ASSERT(jsDynamicCast<JSSet*>(vm, object));
                    propertyStack.append(PropertyNameArray(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    object->methodTable(vm)->getOwnPropertyNames(object, m_exec, propertyStack.last(), EnumerationMode());
                    write(NonSetPropertiesTag);
                    indexStack.append(0);
                    goto objectStartVisitMember;
                }
                inValue = key;
                stateStack.append(SetDataEndVisitKey);
                goto stateUnknown;
            }
            case SetDataEndVisitKey: {
                goto setDataStartVisitEntry;
            }

            stateUnknown:
            case StateUnknown: {
                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                if (dumpIfTerminal(inValue, terminalCode)) {
                    if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                        return terminalCode;
                    break;
                }

                if (isArray(vm, inValue))
                    goto arrayStartState;
                if (isMap(vm, inValue))
                    goto mapStartState;
                if (isSet(vm, inValue))
                    goto setStartState;
                goto objectStartState;
            }
        }
        if (stateStack.isEmpty())
            break;

        state = stateStack.last();
        stateStack.removeLast();
    }
    if (m_failed)
        return SerializationReturnCode::UnspecifiedError;

    return SerializationReturnCode::SuccessfullyCompleted;
}

class CloneDeserializer : CloneBase {
public:
    static String deserializeString(const Vector<uint8_t>& buffer)
    {
        if (buffer.isEmpty())
            return String();
        const uint8_t* ptr = buffer.begin();
        const uint8_t* end = buffer.end();
        uint32_t version;
        if (!readLittleEndian(ptr, end, version) || version > CurrentVersion)
            return String();
        uint8_t tag;
        if (!readLittleEndian(ptr, end, tag) || tag != StringTag)
            return String();
        uint32_t length;
        if (!readLittleEndian(ptr, end, length))
            return String();
        bool is8Bit = length & StringDataIs8BitFlag;
        length &= ~StringDataIs8BitFlag;
        String str;
        if (!readString(ptr, end, str, length, is8Bit))
            return String();
        return str;
    }

    static DeserializationResult deserialize(ExecState* exec, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, Vector<std::pair<std::unique_ptr<ImageBuffer>, bool>>&& imageBuffers, ArrayBufferContentsArray* arrayBufferContentsArray, const Vector<uint8_t>& buffer, const Vector<String>& blobURLs, const Vector<String> blobFilePaths, ArrayBufferContentsArray* sharedBuffers
#if ENABLE(WEBASSEMBLY)
        , WasmModuleArray* wasmModules
#endif
        )
    {
        if (!buffer.size())
            return std::make_pair(jsNull(), SerializationReturnCode::UnspecifiedError);
        CloneDeserializer deserializer(exec, globalObject, messagePorts, arrayBufferContentsArray, buffer, blobURLs, blobFilePaths, sharedBuffers, WTFMove(imageBuffers)
#if ENABLE(WEBASSEMBLY)
            , wasmModules
#endif
            );
        if (!deserializer.isValid())
            return std::make_pair(JSValue(), SerializationReturnCode::ValidationError);
        return deserializer.deserialize();
    }

private:
    struct CachedString {
        CachedString(const String& string)
            : m_string(string)
        {
        }

        JSValue jsString(ExecState* exec)
        {
            if (!m_jsString)
                m_jsString = JSC::jsString(exec->vm(), m_string);
            return m_jsString;
        }
        const String& string() { return m_string; }
        String takeString() { return WTFMove(m_string); }

    private:
        String m_string;
        JSValue m_jsString;
    };

    struct CachedStringRef {
        CachedStringRef()
            : m_base(0)
            , m_index(0)
        {
        }
        CachedStringRef(Vector<CachedString>* base, size_t index)
            : m_base(base)
            , m_index(index)
        {
        }
        
        CachedString* operator->() { ASSERT(m_base); return &m_base->at(m_index); }
        
    private:
        Vector<CachedString>* m_base;
        size_t m_index;
    };

    CloneDeserializer(ExecState* exec, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, ArrayBufferContentsArray* arrayBufferContents, Vector<std::pair<std::unique_ptr<ImageBuffer>, bool>>&& imageBuffers,
#if ENABLE(WEBASSEMBLY)
        WasmModuleArray* wasmModules,
#endif
        const Vector<uint8_t>& buffer)
        : CloneBase(exec)
        , m_globalObject(globalObject)
        , m_isDOMGlobalObject(globalObject->inherits<JSDOMGlobalObject>(globalObject->vm()))
        , m_ptr(buffer.data())
        , m_end(buffer.data() + buffer.size())
        , m_version(0xFFFFFFFF)
        , m_messagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_imageBuffers(WTFMove(imageBuffers))
        , m_imageBitmaps(m_imageBuffers.size())
#if ENABLE(WEBASSEMBLY)
        , m_wasmModules(wasmModules)
#endif
    {
        if (!read(m_version))
            m_version = 0xFFFFFFFF;
    }

    CloneDeserializer(ExecState* exec, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, ArrayBufferContentsArray* arrayBufferContents, const Vector<uint8_t>& buffer, const Vector<String>& blobURLs, const Vector<String> blobFilePaths, ArrayBufferContentsArray* sharedBuffers, Vector<std::pair<std::unique_ptr<ImageBuffer>, bool>>&& imageBuffers
#if ENABLE(WEBASSEMBLY)
        , WasmModuleArray* wasmModules
#endif
        )
        : CloneBase(exec)
        , m_globalObject(globalObject)
        , m_isDOMGlobalObject(globalObject->inherits<JSDOMGlobalObject>(globalObject->vm()))
        , m_ptr(buffer.data())
        , m_end(buffer.data() + buffer.size())
        , m_version(0xFFFFFFFF)
        , m_messagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_blobURLs(blobURLs)
        , m_blobFilePaths(blobFilePaths)
        , m_sharedBuffers(sharedBuffers)
        , m_imageBuffers(WTFMove(imageBuffers))
        , m_imageBitmaps(m_imageBuffers.size())
#if ENABLE(WEBASSEMBLY)
        , m_wasmModules(wasmModules)
#endif
    {
        if (!read(m_version))
            m_version = 0xFFFFFFFF;
    }

    DeserializationResult deserialize();

    bool isValid() const { return m_version <= CurrentVersion; }

    template <typename T> bool readLittleEndian(T& value)
    {
        if (m_failed || !readLittleEndian(m_ptr, m_end, value)) {
            fail();
            return false;
        }
        return true;
    }
#if ASSUME_LITTLE_ENDIAN
    template <typename T> static bool readLittleEndian(const uint8_t*& ptr, const uint8_t* end, T& value)
    {
        if (ptr > end - sizeof(value))
            return false;

        if (sizeof(T) == 1)
            value = *ptr++;
        else {
            value = *reinterpret_cast<const T*>(ptr);
            ptr += sizeof(T);
        }
        return true;
    }
#else
    template <typename T> static bool readLittleEndian(const uint8_t*& ptr, const uint8_t* end, T& value)
    {
        if (ptr > end - sizeof(value))
            return false;

        if (sizeof(T) == 1)
            value = *ptr++;
        else {
            value = 0;
            for (unsigned i = 0; i < sizeof(T); i++)
                value += ((T)*ptr++) << (i * 8);
        }
        return true;
    }
#endif

    bool read(uint32_t& i)
    {
        return readLittleEndian(i);
    }

    bool read(int32_t& i)
    {
        return readLittleEndian(*reinterpret_cast<uint32_t*>(&i));
    }

    bool read(uint16_t& i)
    {
        return readLittleEndian(i);
    }

    bool read(uint8_t& i)
    {
        return readLittleEndian(i);
    }

    bool read(double& d)
    {
        union {
            double d;
            uint64_t i64;
        } u;
        if (!readLittleEndian(u.i64))
            return false;
        d = u.d;
        return true;
    }

    bool read(unsigned long long& i)
    {
        return readLittleEndian(i);
    }

    bool readStringIndex(uint32_t& i)
    {
        return readConstantPoolIndex(m_constantPool, i);
    }

    template <class T> bool readConstantPoolIndex(const T& constantPool, uint32_t& i)
    {
        if (constantPool.size() <= 0xFF) {
            uint8_t i8;
            if (!read(i8))
                return false;
            i = i8;
            return true;
        }
        if (constantPool.size() <= 0xFFFF) {
            uint16_t i16;
            if (!read(i16))
                return false;
            i = i16;
            return true;
        }
        return read(i);
    }

    static bool readString(const uint8_t*& ptr, const uint8_t* end, String& str, unsigned length, bool is8Bit)
    {
        if (length >= std::numeric_limits<int32_t>::max() / sizeof(UChar))
            return false;

        if (is8Bit) {
            if ((end - ptr) < static_cast<int>(length))
                return false;
            str = String(reinterpret_cast<const LChar*>(ptr), length);
            ptr += length;
            return true;
        }

        unsigned size = length * sizeof(UChar);
        if ((end - ptr) < static_cast<int>(size))
            return false;

#if ASSUME_LITTLE_ENDIAN
        str = String(reinterpret_cast<const UChar*>(ptr), length);
        ptr += length * sizeof(UChar);
#else
        Vector<UChar> buffer;
        buffer.reserveCapacity(length);
        for (unsigned i = 0; i < length; i++) {
            uint16_t ch;
            readLittleEndian(ptr, end, ch);
            buffer.append(ch);
        }
        str = String::adopt(WTFMove(buffer));
#endif
        return true;
    }

    bool readStringData(CachedStringRef& cachedString)
    {
        bool scratch;
        return readStringData(cachedString, scratch);
    }

    bool readStringData(CachedStringRef& cachedString, bool& wasTerminator)
    {
        if (m_failed)
            return false;
        uint32_t length = 0;
        if (!read(length))
            return false;
        if (length == TerminatorTag) {
            wasTerminator = true;
            return false;
        }
        if (length == StringPoolTag) {
            unsigned index = 0;
            if (!readStringIndex(index)) {
                fail();
                return false;
            }
            if (index >= m_constantPool.size()) {
                fail();
                return false;
            }
            cachedString = CachedStringRef(&m_constantPool, index);
            return true;
        }
        bool is8Bit = length & StringDataIs8BitFlag;
        length &= ~StringDataIs8BitFlag;
        String str;
        if (!readString(m_ptr, m_end, str, length, is8Bit)) {
            fail();
            return false;
        }
        m_constantPool.append(str);
        cachedString = CachedStringRef(&m_constantPool, m_constantPool.size() - 1);
        return true;
    }

    SerializationTag readTag()
    {
        if (m_ptr >= m_end)
            return ErrorTag;
        return static_cast<SerializationTag>(*m_ptr++);
    }

    bool readArrayBufferViewSubtag(ArrayBufferViewSubtag& tag)
    {
        if (m_ptr >= m_end)
            return false;
        tag = static_cast<ArrayBufferViewSubtag>(*m_ptr++);
        return true;
    }

    void putProperty(JSObject* object, unsigned index, JSValue value)
    {
        object->putDirectIndex(m_exec, index, value);
    }

    void putProperty(JSObject* object, const Identifier& property, JSValue value)
    {
        object->putDirectMayBeIndex(m_exec, property, value);
    }

    bool readFile(RefPtr<File>& file)
    {
        CachedStringRef path;
        if (!readStringData(path))
            return false;
        CachedStringRef url;
        if (!readStringData(url))
            return false;
        CachedStringRef type;
        if (!readStringData(type))
            return false;
        CachedStringRef name;
        if (!readStringData(name))
            return false;
        Optional<int64_t> optionalLastModified;
        if (m_version > 6) {
            double lastModified;
            if (!read(lastModified))
                return false;
            if (lastModified >= 0)
                optionalLastModified = lastModified;
        }

        // If the blob URL for this file has an associated blob file path, prefer that one over the "built-in" path.
        String filePath = blobFilePathForBlobURL(url->string());
        if (filePath.isEmpty())
            filePath = path->string();

        if (m_isDOMGlobalObject)
            file = File::deserialize(jsCast<JSDOMGlobalObject*>(m_globalObject)->scriptExecutionContext()->sessionID(), filePath, URL(URL(), url->string()), type->string(), name->string(), optionalLastModified);
        return true;
    }

    bool readArrayBuffer(RefPtr<ArrayBuffer>& arrayBuffer)
    {
        uint32_t length;
        if (!read(length))
            return false;
        if (m_ptr + length > m_end)
            return false;
        arrayBuffer = ArrayBuffer::create(m_ptr, length);
        m_ptr += length;
        return true;
    }

    bool readArrayBufferView(VM& vm, JSValue& arrayBufferView)
    {
        ArrayBufferViewSubtag arrayBufferViewSubtag;
        if (!readArrayBufferViewSubtag(arrayBufferViewSubtag))
            return false;
        uint32_t byteOffset;
        if (!read(byteOffset))
            return false;
        uint32_t byteLength;
        if (!read(byteLength))
            return false;
        JSObject* arrayBufferObj = asObject(readTerminal());
        if (!arrayBufferObj || !arrayBufferObj->inherits<JSArrayBuffer>(vm))
            return false;

        unsigned elementSize = typedArrayElementSize(arrayBufferViewSubtag);
        if (!elementSize)
            return false;
        unsigned length = byteLength / elementSize;
        if (length * elementSize != byteLength)
            return false;

        RefPtr<ArrayBuffer> arrayBuffer = toPossiblySharedArrayBuffer(vm, arrayBufferObj);
        switch (arrayBufferViewSubtag) {
        case DataViewTag:
            arrayBufferView = toJS(m_exec, m_globalObject, DataView::create(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        case Int8ArrayTag:
            arrayBufferView = toJS(m_exec, m_globalObject, Int8Array::tryCreate(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        case Uint8ArrayTag:
            arrayBufferView = toJS(m_exec, m_globalObject, Uint8Array::tryCreate(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        case Uint8ClampedArrayTag:
            arrayBufferView = toJS(m_exec, m_globalObject, Uint8ClampedArray::tryCreate(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        case Int16ArrayTag:
            arrayBufferView = toJS(m_exec, m_globalObject, Int16Array::tryCreate(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        case Uint16ArrayTag:
            arrayBufferView = toJS(m_exec, m_globalObject, Uint16Array::tryCreate(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        case Int32ArrayTag:
            arrayBufferView = toJS(m_exec, m_globalObject, Int32Array::tryCreate(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        case Uint32ArrayTag:
            arrayBufferView = toJS(m_exec, m_globalObject, Uint32Array::tryCreate(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        case Float32ArrayTag:
            arrayBufferView = toJS(m_exec, m_globalObject, Float32Array::tryCreate(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        case Float64ArrayTag:
            arrayBufferView = toJS(m_exec, m_globalObject, Float64Array::tryCreate(WTFMove(arrayBuffer), byteOffset, length).get());
            return true;
        default:
            return false;
        }
    }

    bool read(Vector<uint8_t>& result)
    {
        ASSERT(result.isEmpty());
        uint32_t size;
        if (!read(size))
            return false;
        if (m_ptr + size > m_end)
            return false;
        result.append(m_ptr, size);
        m_ptr += size;
        return true;
    }

#if ENABLE(WEB_CRYPTO)
    bool read(CryptoAlgorithmIdentifier& result)
    {
        uint8_t algorithmTag;
        if (!read(algorithmTag))
            return false;
        if (algorithmTag > cryptoAlgorithmIdentifierTagMaximumValue)
            return false;
        switch (static_cast<CryptoAlgorithmIdentifierTag>(algorithmTag)) {
        case CryptoAlgorithmIdentifierTag::RSAES_PKCS1_v1_5:
            result = CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5;
            break;
        case CryptoAlgorithmIdentifierTag::RSASSA_PKCS1_v1_5:
            result = CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5;
            break;
        case CryptoAlgorithmIdentifierTag::RSA_PSS:
            result = CryptoAlgorithmIdentifier::RSA_PSS;
            break;
        case CryptoAlgorithmIdentifierTag::RSA_OAEP:
            result = CryptoAlgorithmIdentifier::RSA_OAEP;
            break;
        case CryptoAlgorithmIdentifierTag::ECDSA:
            result = CryptoAlgorithmIdentifier::ECDSA;
            break;
        case CryptoAlgorithmIdentifierTag::ECDH:
            result = CryptoAlgorithmIdentifier::ECDH;
            break;
        case CryptoAlgorithmIdentifierTag::AES_CTR:
            result = CryptoAlgorithmIdentifier::AES_CTR;
            break;
        case CryptoAlgorithmIdentifierTag::AES_CBC:
            result = CryptoAlgorithmIdentifier::AES_CBC;
            break;
        case CryptoAlgorithmIdentifierTag::AES_GCM:
            result = CryptoAlgorithmIdentifier::AES_GCM;
            break;
        case CryptoAlgorithmIdentifierTag::AES_CFB:
            result = CryptoAlgorithmIdentifier::AES_CFB;
            break;
        case CryptoAlgorithmIdentifierTag::AES_KW:
            result = CryptoAlgorithmIdentifier::AES_KW;
            break;
        case CryptoAlgorithmIdentifierTag::HMAC:
            result = CryptoAlgorithmIdentifier::HMAC;
            break;
        case CryptoAlgorithmIdentifierTag::SHA_1:
            result = CryptoAlgorithmIdentifier::SHA_1;
            break;
        case CryptoAlgorithmIdentifierTag::SHA_224:
            result = CryptoAlgorithmIdentifier::SHA_224;
            break;
        case CryptoAlgorithmIdentifierTag::SHA_256:
            result = CryptoAlgorithmIdentifier::SHA_256;
            break;
        case CryptoAlgorithmIdentifierTag::SHA_384:
            result = CryptoAlgorithmIdentifier::SHA_384;
            break;
        case CryptoAlgorithmIdentifierTag::SHA_512:
            result = CryptoAlgorithmIdentifier::SHA_512;
            break;
        case CryptoAlgorithmIdentifierTag::HKDF:
            result = CryptoAlgorithmIdentifier::HKDF;
            break;
        case CryptoAlgorithmIdentifierTag::PBKDF2:
            result = CryptoAlgorithmIdentifier::PBKDF2;
            break;
        }
        return true;
    }

    bool read(CryptoKeyClassSubtag& result)
    {
        uint8_t tag;
        if (!read(tag))
            return false;
        if (tag > cryptoKeyClassSubtagMaximumValue)
            return false;
        result = static_cast<CryptoKeyClassSubtag>(tag);
        return true;
    }

    bool read(CryptoKeyUsageTag& result)
    {
        uint8_t tag;
        if (!read(tag))
            return false;
        if (tag > cryptoKeyUsageTagMaximumValue)
            return false;
        result = static_cast<CryptoKeyUsageTag>(tag);
        return true;
    }

    bool read(CryptoKeyAsymmetricTypeSubtag& result)
    {
        uint8_t tag;
        if (!read(tag))
            return false;
        if (tag > cryptoKeyAsymmetricTypeSubtagMaximumValue)
            return false;
        result = static_cast<CryptoKeyAsymmetricTypeSubtag>(tag);
        return true;
    }

    bool readHMACKey(bool extractable, CryptoKeyUsageBitmap usages, RefPtr<CryptoKey>& result)
    {
        Vector<uint8_t> keyData;
        if (!read(keyData))
            return false;
        CryptoAlgorithmIdentifier hash;
        if (!read(hash))
            return false;
        result = CryptoKeyHMAC::importRaw(0, hash, WTFMove(keyData), extractable, usages);
        return true;
    }

    bool readAESKey(bool extractable, CryptoKeyUsageBitmap usages, RefPtr<CryptoKey>& result)
    {
        CryptoAlgorithmIdentifier algorithm;
        if (!read(algorithm))
            return false;
        if (!CryptoKeyAES::isValidAESAlgorithm(algorithm))
            return false;
        Vector<uint8_t> keyData;
        if (!read(keyData))
            return false;
        result = CryptoKeyAES::importRaw(algorithm, WTFMove(keyData), extractable, usages);
        return true;
    }

    bool readRSAKey(bool extractable, CryptoKeyUsageBitmap usages, RefPtr<CryptoKey>& result)
    {
        CryptoAlgorithmIdentifier algorithm;
        if (!read(algorithm))
            return false;

        int32_t isRestrictedToHash;
        CryptoAlgorithmIdentifier hash;
        if (!read(isRestrictedToHash))
            return false;
        if (isRestrictedToHash && !read(hash))
            return false;

        CryptoKeyAsymmetricTypeSubtag type;
        if (!read(type))
            return false;

        Vector<uint8_t> modulus;
        if (!read(modulus))
            return false;
        Vector<uint8_t> exponent;
        if (!read(exponent))
            return false;

        if (type == CryptoKeyAsymmetricTypeSubtag::Public) {
            auto keyData = CryptoKeyRSAComponents::createPublic(modulus, exponent);
            auto key = CryptoKeyRSA::create(algorithm, hash, isRestrictedToHash, *keyData, extractable, usages);
            result = WTFMove(key);
            return true;
        }

        Vector<uint8_t> privateExponent;
        if (!read(privateExponent))
            return false;

        uint32_t primeCount;
        if (!read(primeCount))
            return false;

        if (!primeCount) {
            auto keyData = CryptoKeyRSAComponents::createPrivate(modulus, exponent, privateExponent);
            auto key = CryptoKeyRSA::create(algorithm, hash, isRestrictedToHash, *keyData, extractable, usages);
            result = WTFMove(key);
            return true;
        }

        if (primeCount < 2)
            return false;

        CryptoKeyRSAComponents::PrimeInfo firstPrimeInfo;
        CryptoKeyRSAComponents::PrimeInfo secondPrimeInfo;
        Vector<CryptoKeyRSAComponents::PrimeInfo> otherPrimeInfos(primeCount - 2);

        if (!read(firstPrimeInfo.primeFactor))
            return false;
        if (!read(firstPrimeInfo.factorCRTExponent))
            return false;
        if (!read(secondPrimeInfo.primeFactor))
            return false;
        if (!read(secondPrimeInfo.factorCRTExponent))
            return false;
        if (!read(secondPrimeInfo.factorCRTCoefficient))
            return false;
        for (unsigned i = 2; i < primeCount; ++i) {
            if (!read(otherPrimeInfos[i].primeFactor))
                return false;
            if (!read(otherPrimeInfos[i].factorCRTExponent))
                return false;
            if (!read(otherPrimeInfos[i].factorCRTCoefficient))
                return false;
        }

        auto keyData = CryptoKeyRSAComponents::createPrivateWithAdditionalData(modulus, exponent, privateExponent, firstPrimeInfo, secondPrimeInfo, otherPrimeInfos);
        auto key = CryptoKeyRSA::create(algorithm, hash, isRestrictedToHash, *keyData, extractable, usages);
        result = WTFMove(key);
        return true;
    }

    bool readECKey(bool extractable, CryptoKeyUsageBitmap usages, RefPtr<CryptoKey>& result)
    {
        CryptoAlgorithmIdentifier algorithm;
        if (!read(algorithm))
            return false;
        if (!CryptoKeyEC::isValidECAlgorithm(algorithm))
            return false;
        CachedStringRef curve;
        if (!readStringData(curve))
            return false;
        CryptoKeyAsymmetricTypeSubtag type;
        if (!read(type))
            return false;
        Vector<uint8_t> keyData;
        if (!read(keyData))
            return false;

        switch (type) {
        case CryptoKeyAsymmetricTypeSubtag::Public:
            result = CryptoKeyEC::importRaw(algorithm, curve->string(), WTFMove(keyData), extractable, usages);
            break;
        case CryptoKeyAsymmetricTypeSubtag::Private:
            result = CryptoKeyEC::importPkcs8(algorithm, curve->string(), WTFMove(keyData), extractable, usages);
            break;
        }

        return true;
    }

    bool readRawKey(CryptoKeyUsageBitmap usages, RefPtr<CryptoKey>& result)
    {
        CryptoAlgorithmIdentifier algorithm;
        if (!read(algorithm))
            return false;
        Vector<uint8_t> keyData;
        if (!read(keyData))
            return false;
        result = CryptoKeyRaw::create(algorithm, WTFMove(keyData), usages);
        return true;
    }

    bool readCryptoKey(JSValue& cryptoKey)
    {
        uint32_t keyFormatVersion;
        if (!read(keyFormatVersion) || keyFormatVersion > currentKeyFormatVersion)
            return false;

        int32_t extractable;
        if (!read(extractable))
            return false;

        uint32_t usagesCount;
        if (!read(usagesCount))
            return false;

        CryptoKeyUsageBitmap usages = 0;
        for (uint32_t i = 0; i < usagesCount; ++i) {
            CryptoKeyUsageTag usage;
            if (!read(usage))
                return false;
            switch (usage) {
            case CryptoKeyUsageTag::Encrypt:
                usages |= CryptoKeyUsageEncrypt;
                break;
            case CryptoKeyUsageTag::Decrypt:
                usages |= CryptoKeyUsageDecrypt;
                break;
            case CryptoKeyUsageTag::Sign:
                usages |= CryptoKeyUsageSign;
                break;
            case CryptoKeyUsageTag::Verify:
                usages |= CryptoKeyUsageVerify;
                break;
            case CryptoKeyUsageTag::DeriveKey:
                usages |= CryptoKeyUsageDeriveKey;
                break;
            case CryptoKeyUsageTag::DeriveBits:
                usages |= CryptoKeyUsageDeriveBits;
                break;
            case CryptoKeyUsageTag::WrapKey:
                usages |= CryptoKeyUsageWrapKey;
                break;
            case CryptoKeyUsageTag::UnwrapKey:
                usages |= CryptoKeyUsageUnwrapKey;
                break;
            }
        }

        CryptoKeyClassSubtag cryptoKeyClass;
        if (!read(cryptoKeyClass))
            return false;
        RefPtr<CryptoKey> result;
        switch (cryptoKeyClass) {
        case CryptoKeyClassSubtag::HMAC:
            if (!readHMACKey(extractable, usages, result))
                return false;
            break;
        case CryptoKeyClassSubtag::AES:
            if (!readAESKey(extractable, usages, result))
                return false;
            break;
        case CryptoKeyClassSubtag::RSA:
            if (!readRSAKey(extractable, usages, result))
                return false;
            break;
        case CryptoKeyClassSubtag::EC:
            if (!readECKey(extractable, usages, result))
                return false;
            break;
        case CryptoKeyClassSubtag::Raw:
            if (!readRawKey(usages, result))
                return false;
            break;
        }
        cryptoKey = getJSValue(result.get());
        return true;
    }
#endif

    template<class T>
    JSValue getJSValue(T* nativeObj)
    {
        return toJS(m_exec, jsCast<JSDOMGlobalObject*>(m_globalObject), nativeObj);
    }

    template<class T>
    JSValue getJSValue(T& nativeObj)
    {
        return toJS(m_exec, jsCast<JSDOMGlobalObject*>(m_globalObject), nativeObj);
    }

    template<class T>
    JSValue readDOMPoint()
    {
        double x;
        if (!read(x))
            return { };
        double y;
        if (!read(y))
            return { };
        double z;
        if (!read(z))
            return { };
        double w;
        if (!read(w))
            return { };

        return toJSNewlyCreated(m_exec, jsCast<JSDOMGlobalObject*>(m_globalObject), T::create(x, y, z, w));
    }

    template<class T>
    JSValue readDOMMatrix()
    {
        uint8_t is2D;
        if (!read(is2D))
            return { };

        if (is2D) {
            double m11;
            if (!read(m11))
                return { };
            double m12;
            if (!read(m12))
                return { };
            double m21;
            if (!read(m21))
                return { };
            double m22;
            if (!read(m22))
                return { };
            double m41;
            if (!read(m41))
                return { };
            double m42;
            if (!read(m42))
                return { };

            TransformationMatrix matrix(m11, m12, m21, m22, m41, m42);
            return toJSNewlyCreated(m_exec, jsCast<JSDOMGlobalObject*>(m_globalObject), T::create(WTFMove(matrix), DOMMatrixReadOnly::Is2D::Yes));
        } else {
            double m11;
            if (!read(m11))
                return { };
            double m12;
            if (!read(m12))
                return { };
            double m13;
            if (!read(m13))
                return { };
            double m14;
            if (!read(m14))
                return { };
            double m21;
            if (!read(m21))
                return { };
            double m22;
            if (!read(m22))
                return { };
            double m23;
            if (!read(m23))
                return { };
            double m24;
            if (!read(m24))
                return { };
            double m31;
            if (!read(m31))
                return { };
            double m32;
            if (!read(m32))
                return { };
            double m33;
            if (!read(m33))
                return { };
            double m34;
            if (!read(m34))
                return { };
            double m41;
            if (!read(m41))
                return { };
            double m42;
            if (!read(m42))
                return { };
            double m43;
            if (!read(m43))
                return { };
            double m44;
            if (!read(m44))
                return { };

            TransformationMatrix matrix(m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44);
            return toJSNewlyCreated(m_exec, jsCast<JSDOMGlobalObject*>(m_globalObject), T::create(WTFMove(matrix), DOMMatrixReadOnly::Is2D::No));
        }
    }

    template<class T>
    JSValue readDOMRect()
    {
        double x;
        if (!read(x))
            return { };
        double y;
        if (!read(y))
            return { };
        double width;
        if (!read(width))
            return { };
        double height;
        if (!read(height))
            return { };

        return toJSNewlyCreated(m_exec, jsCast<JSDOMGlobalObject*>(m_globalObject), T::create(x, y, width, height));
    }

    Optional<DOMPointInit> readDOMPointInit()
    {
        DOMPointInit point;
        if (!read(point.x))
            return WTF::nullopt;
        if (!read(point.y))
            return WTF::nullopt;
        if (!read(point.z))
            return WTF::nullopt;
        if (!read(point.w))
            return WTF::nullopt;

        return point;
    }

    JSValue readDOMQuad()
    {
        auto p1 = readDOMPointInit();
        if (!p1)
            return JSValue();
        auto p2 = readDOMPointInit();
        if (!p2)
            return JSValue();
        auto p3 = readDOMPointInit();
        if (!p3)
            return JSValue();
        auto p4 = readDOMPointInit();
        if (!p4)
            return JSValue();

        return toJSNewlyCreated(m_exec, jsCast<JSDOMGlobalObject*>(m_globalObject), DOMQuad::create(p1.value(), p2.value(), p3.value(), p4.value()));
    }

    JSValue readImageBitmap()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_imageBuffers.size()) {
            fail();
            return JSValue();
        }

        if (!m_imageBitmaps[index])
            m_imageBitmaps[index] = ImageBitmap::create(WTFMove(m_imageBuffers.at(index)));

        auto bitmap = m_imageBitmaps[index].get();
        return getJSValue(bitmap);
    }

#if ENABLE(WEB_RTC)
    JSValue readRTCCertificate()
    {
        double expires;
        if (!read(expires)) {
            fail();
            return JSValue();
        }
        CachedStringRef certificate;
        if (!readStringData(certificate)) {
            fail();
            return JSValue();
        }
        CachedStringRef origin;
        if (!readStringData(origin)) {
            fail();
            return JSValue();
        }
        CachedStringRef keyedMaterial;
        if (!readStringData(keyedMaterial)) {
            fail();
            return JSValue();
        }
        unsigned size = 0;
        if (!read(size))
            return JSValue();

        Vector<RTCCertificate::DtlsFingerprint> fingerprints;
        fingerprints.reserveInitialCapacity(size);
        for (unsigned i = 0; i < size; i++) {
            CachedStringRef algorithm;
            if (!readStringData(algorithm))
                return JSValue();
            CachedStringRef value;
            if (!readStringData(value))
                return JSValue();
            fingerprints.uncheckedAppend(RTCCertificate::DtlsFingerprint { algorithm->string(), value->string() });
        }

        if (!m_isDOMGlobalObject)
            return constructEmptyObject(m_exec, m_globalObject->objectPrototype());

        auto rtcCertificate = RTCCertificate::create(SecurityOrigin::createFromString(origin->string()), expires, WTFMove(fingerprints), certificate->takeString(), keyedMaterial->takeString());
        return toJSNewlyCreated(m_exec, jsCast<JSDOMGlobalObject*>(m_globalObject), WTFMove(rtcCertificate));
    }
#endif

    JSValue readTerminal()
    {
        SerializationTag tag = readTag();
        switch (tag) {
        case UndefinedTag:
            return jsUndefined();
        case NullTag:
            return jsNull();
        case IntTag: {
            int32_t i;
            if (!read(i))
                return JSValue();
            return jsNumber(i);
        }
        case ZeroTag:
            return jsNumber(0);
        case OneTag:
            return jsNumber(1);
        case FalseTag:
            return jsBoolean(false);
        case TrueTag:
            return jsBoolean(true);
        case FalseObjectTag: {
            BooleanObject* obj = BooleanObject::create(m_exec->vm(), m_globalObject->booleanObjectStructure());
            obj->setInternalValue(m_exec->vm(), jsBoolean(false));
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case TrueObjectTag: {
            BooleanObject* obj = BooleanObject::create(m_exec->vm(), m_globalObject->booleanObjectStructure());
            obj->setInternalValue(m_exec->vm(), jsBoolean(true));
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case DoubleTag: {
            double d;
            if (!read(d))
                return JSValue();
            return jsNumber(d);
        }
        case NumberObjectTag: {
            double d;
            if (!read(d))
                return JSValue();
            NumberObject* obj = constructNumber(m_exec, m_globalObject, jsNumber(d));
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case DateTag: {
            double d;
            if (!read(d))
                return JSValue();
            return DateInstance::create(m_exec->vm(), m_globalObject->dateStructure(), d);
        }
        case FileTag: {
            RefPtr<File> file;
            if (!readFile(file))
                return JSValue();
            if (!m_isDOMGlobalObject)
                return jsNull();
            return toJS(m_exec, jsCast<JSDOMGlobalObject*>(m_globalObject), file.get());
        }
        case FileListTag: {
            unsigned length = 0;
            if (!read(length))
                return JSValue();
            Vector<Ref<File>> files;
            for (unsigned i = 0; i < length; i++) {
                RefPtr<File> file;
                if (!readFile(file))
                    return JSValue();
                if (m_isDOMGlobalObject)
                    files.append(file.releaseNonNull());
            }
            if (!m_isDOMGlobalObject)
                return jsNull();
            return getJSValue(FileList::create(WTFMove(files)).get());
        }
        case ImageDataTag: {
            uint32_t width;
            if (!read(width))
                return JSValue();
            uint32_t height;
            if (!read(height))
                return JSValue();
            uint32_t length;
            if (!read(length))
                return JSValue();
            if (static_cast<uint32_t>(m_end - m_ptr) < length) {
                fail();
                return JSValue();
            }
            if (!m_isDOMGlobalObject) {
                m_ptr += length;
                return jsNull();
            }
            IntSize imageSize(width, height);
            RELEASE_ASSERT(!length || (imageSize.area() * 4).unsafeGet() <= length);
            auto result = ImageData::create(imageSize);
            if (!result) {
                fail();
                return JSValue();
            }
            if (length)
                memcpy(result->data()->data(), m_ptr, length);
            else
                result->data()->zeroFill();
            m_ptr += length;
            return getJSValue(result.get());
        }
        case BlobTag: {
            CachedStringRef url;
            if (!readStringData(url))
                return JSValue();
            CachedStringRef type;
            if (!readStringData(type))
                return JSValue();
            unsigned long long size = 0;
            if (!read(size))
                return JSValue();
            if (!m_isDOMGlobalObject)
                return jsNull();
            return getJSValue(Blob::deserialize(jsCast<JSDOMGlobalObject*>(m_globalObject)->scriptExecutionContext()->sessionID(), URL(URL(), url->string()), type->string(), size, blobFilePathForBlobURL(url->string())).get());
        }
        case StringTag: {
            CachedStringRef cachedString;
            if (!readStringData(cachedString))
                return JSValue();
            return cachedString->jsString(m_exec);
        }
        case EmptyStringTag:
            return jsEmptyString(m_exec->vm());
        case StringObjectTag: {
            CachedStringRef cachedString;
            if (!readStringData(cachedString))
                return JSValue();
            StringObject* obj = constructString(m_exec->vm(), m_globalObject, cachedString->jsString(m_exec));
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case EmptyStringObjectTag: {
            VM& vm = m_exec->vm();
            StringObject* obj = constructString(vm, m_globalObject, jsEmptyString(vm));
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case RegExpTag: {
            CachedStringRef pattern;
            if (!readStringData(pattern))
                return JSValue();
            CachedStringRef flags;
            if (!readStringData(flags))
                return JSValue();
            auto reFlags = Yarr::parseFlags(flags->string());
            ASSERT(reFlags.hasValue());
            VM& vm = m_exec->vm();
            RegExp* regExp = RegExp::create(vm, pattern->string(), reFlags.value());
            return RegExpObject::create(vm, m_globalObject->regExpStructure(), regExp);
        }
        case ObjectReferenceTag: {
            unsigned index = 0;
            if (!readConstantPoolIndex(m_gcBuffer, index)) {
                fail();
                return JSValue();
            }
            return m_gcBuffer.at(index);
        }
        case MessagePortReferenceTag: {
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || index >= m_messagePorts.size()) {
                fail();
                return JSValue();
            }
            return getJSValue(m_messagePorts[index].get());
        }
#if ENABLE(WEBASSEMBLY)
        case WasmModuleTag: {
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_wasmModules || index >= m_wasmModules->size()) {
                fail();
                return JSValue();
            }
            auto scope = DECLARE_THROW_SCOPE(m_exec->vm());
            JSValue result = JSC::JSWebAssemblyModule::createStub(m_exec->vm(), m_exec, m_globalObject->webAssemblyModuleStructure(), m_wasmModules->at(index));
            // Since we are cloning a JSWebAssemblyModule, it's impossible for that
            // module to not have been a valid module. Therefore, createStub should
            // not trow.
            scope.releaseAssertNoException();
            m_gcBuffer.appendWithCrashOnOverflow(result);
            return result;
        }
#endif
        case ArrayBufferTag: {
            RefPtr<ArrayBuffer> arrayBuffer;
            if (!readArrayBuffer(arrayBuffer)) {
                fail();
                return JSValue();
            }
            Structure* structure = m_globalObject->arrayBufferStructure(arrayBuffer->sharingMode());
            // A crazy RuntimeFlags mismatch could mean that we are not equipped to handle shared
            // array buffers while the sender is. In that case, we would see a null structure here.
            if (!structure) {
                fail();
                return JSValue();
            }
            JSValue result = JSArrayBuffer::create(m_exec->vm(), structure, WTFMove(arrayBuffer));
            m_gcBuffer.appendWithCrashOnOverflow(result);
            return result;
        }
        case ArrayBufferTransferTag: {
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || index >= m_arrayBuffers.size()) {
                fail();
                return JSValue();
            }

            if (!m_arrayBuffers[index])
                m_arrayBuffers[index] = ArrayBuffer::create(WTFMove(m_arrayBufferContents->at(index)));

            return getJSValue(m_arrayBuffers[index].get());
        }
        case SharedArrayBufferTag: {
            uint32_t index = UINT_MAX;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_sharedBuffers || index >= m_sharedBuffers->size()) {
                fail();
                return JSValue();
            }
            
            RELEASE_ASSERT(m_sharedBuffers->at(index));
            auto buffer = ArrayBuffer::create(WTFMove(m_sharedBuffers->at(index)));
            JSValue result = getJSValue(buffer.get());
            m_gcBuffer.appendWithCrashOnOverflow(result);
            return result;
        }
        case ArrayBufferViewTag: {
            JSValue arrayBufferView;
            if (!readArrayBufferView(m_exec->vm(), arrayBufferView)) {
                fail();
                return JSValue();
            }
            m_gcBuffer.appendWithCrashOnOverflow(arrayBufferView);
            return arrayBufferView;
        }
#if ENABLE(WEB_CRYPTO)
        case CryptoKeyTag: {
            Vector<uint8_t> wrappedKey;
            if (!read(wrappedKey)) {
                fail();
                return JSValue();
            }
            Vector<uint8_t> serializedKey;
            if (!unwrapCryptoKey(m_exec, wrappedKey, serializedKey)) {
                fail();
                return JSValue();
            }
            JSValue cryptoKey;
            Vector<RefPtr<MessagePort>> dummyMessagePorts;
            CloneDeserializer rawKeyDeserializer(m_exec, m_globalObject, dummyMessagePorts, nullptr, { },
#if ENABLE(WEBASSEMBLY)
                nullptr,
#endif
                serializedKey);
            if (!rawKeyDeserializer.readCryptoKey(cryptoKey)) {
                fail();
                return JSValue();
            }
            m_gcBuffer.appendWithCrashOnOverflow(cryptoKey);
            return cryptoKey;
        }
#endif
        case DOMPointReadOnlyTag:
            return readDOMPoint<DOMPointReadOnly>();
        case DOMPointTag:
            return readDOMPoint<DOMPoint>();
        case DOMRectReadOnlyTag:
            return readDOMRect<DOMRectReadOnly>();
        case DOMRectTag:
            return readDOMRect<DOMRect>();
        case DOMMatrixReadOnlyTag:
            return readDOMMatrix<DOMMatrixReadOnly>();
        case DOMMatrixTag:
            return readDOMMatrix<DOMMatrix>();
        case DOMQuadTag:
            return readDOMQuad();
        case ImageBitmapTransferTag:
            return readImageBitmap();
#if ENABLE(WEB_RTC)
        case RTCCertificateTag:
            return readRTCCertificate();

#endif
        default:
            m_ptr--; // Push the tag back
            return JSValue();
        }
    }

    template<SerializationTag Tag>
    bool consumeCollectionDataTerminationIfPossible()
    {
        if (readTag() == Tag)
            return true;
        m_ptr--;
        return false;
    }

    JSGlobalObject* m_globalObject;
    bool m_isDOMGlobalObject;
    const uint8_t* m_ptr;
    const uint8_t* m_end;
    unsigned m_version;
    Vector<CachedString> m_constantPool;
    const Vector<RefPtr<MessagePort>>& m_messagePorts;
    ArrayBufferContentsArray* m_arrayBufferContents;
    Vector<RefPtr<JSC::ArrayBuffer>> m_arrayBuffers;
    Vector<String> m_blobURLs;
    Vector<String> m_blobFilePaths;
    ArrayBufferContentsArray* m_sharedBuffers;
    Vector<std::pair<std::unique_ptr<ImageBuffer>, bool>> m_imageBuffers;
    Vector<RefPtr<ImageBitmap>> m_imageBitmaps;
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray* m_wasmModules;
#endif

    String blobFilePathForBlobURL(const String& blobURL)
    {
        size_t i = 0;
        for (; i < m_blobURLs.size(); ++i) {
            if (m_blobURLs[i] == blobURL)
                break;
        }

        return i < m_blobURLs.size() ? m_blobFilePaths[i] : String();
    }
};

DeserializationResult CloneDeserializer::deserialize()
{
    VM& vm = m_exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<uint32_t, 16> indexStack;
    Vector<Identifier, 16> propertyNameStack;
    Vector<JSObject*, 32> outputObjectStack;
    Vector<JSValue, 4> mapKeyStack;
    Vector<JSMap*, 4> mapStack;
    Vector<JSSet*, 4> setStack;
    Vector<WalkerState, 16> stateStack;
    WalkerState state = StateUnknown;
    JSValue outValue;

    while (1) {
        switch (state) {
        arrayStartState:
        case ArrayStartState: {
            uint32_t length;
            if (!read(length)) {
                fail();
                goto error;
            }
            JSArray* outArray = constructEmptyArray(m_exec, 0, m_globalObject, length);
            if (UNLIKELY(scope.exception()))
                goto error;
            m_gcBuffer.appendWithCrashOnOverflow(outArray);
            outputObjectStack.append(outArray);
        }
        arrayStartVisitMember:
        FALLTHROUGH;
        case ArrayStartVisitMember: {
            uint32_t index;
            if (!read(index)) {
                fail();
                goto error;
            }
            if (index == TerminatorTag) {
                JSObject* outArray = outputObjectStack.last();
                outValue = outArray;
                outputObjectStack.removeLast();
                break;
            } else if (index == NonIndexPropertiesTag) {
                goto objectStartVisitMember;
            }

            if (JSValue terminal = readTerminal()) {
                putProperty(outputObjectStack.last(), index, terminal);
                goto arrayStartVisitMember;
            }
            if (m_failed)
                goto error;
            indexStack.append(index);
            stateStack.append(ArrayEndVisitMember);
            goto stateUnknown;
        }
        case ArrayEndVisitMember: {
            JSObject* outArray = outputObjectStack.last();
            putProperty(outArray, indexStack.last(), outValue);
            indexStack.removeLast();
            goto arrayStartVisitMember;
        }
        objectStartState:
        case ObjectStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion)
                return std::make_pair(JSValue(), SerializationReturnCode::StackOverflowError);
            JSObject* outObject = constructEmptyObject(m_exec, m_globalObject->objectPrototype());
            m_gcBuffer.appendWithCrashOnOverflow(outObject);
            outputObjectStack.append(outObject);
        }
        objectStartVisitMember:
        FALLTHROUGH;
        case ObjectStartVisitMember: {
            CachedStringRef cachedString;
            bool wasTerminator = false;
            if (!readStringData(cachedString, wasTerminator)) {
                if (!wasTerminator)
                    goto error;

                JSObject* outObject = outputObjectStack.last();
                outValue = outObject;
                outputObjectStack.removeLast();
                break;
            }

            if (JSValue terminal = readTerminal()) {
                putProperty(outputObjectStack.last(), Identifier::fromString(vm, cachedString->string()), terminal);
                goto objectStartVisitMember;
            }
            stateStack.append(ObjectEndVisitMember);
            propertyNameStack.append(Identifier::fromString(vm, cachedString->string()));
            goto stateUnknown;
        }
        case ObjectEndVisitMember: {
            putProperty(outputObjectStack.last(), propertyNameStack.last(), outValue);
            propertyNameStack.removeLast();
            goto objectStartVisitMember;
        }
        mapObjectStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion)
                return std::make_pair(JSValue(), SerializationReturnCode::StackOverflowError);
            JSMap* map = JSMap::create(m_exec, m_exec->vm(), m_globalObject->mapStructure());
            if (UNLIKELY(scope.exception()))
                goto error;
            m_gcBuffer.appendWithCrashOnOverflow(map);
            outputObjectStack.append(map);
            mapStack.append(map);
            goto mapDataStartVisitEntry;
        }
        mapDataStartVisitEntry:
        case MapDataStartVisitEntry: {
            if (consumeCollectionDataTerminationIfPossible<NonMapPropertiesTag>()) {
                mapStack.removeLast();
                goto objectStartVisitMember;
            }
            stateStack.append(MapDataEndVisitKey);
            goto stateUnknown;
        }
        case MapDataEndVisitKey: {
            mapKeyStack.append(outValue);
            stateStack.append(MapDataEndVisitValue);
            goto stateUnknown;
        }
        case MapDataEndVisitValue: {
            mapStack.last()->set(m_exec, mapKeyStack.last(), outValue);
            mapKeyStack.removeLast();
            goto mapDataStartVisitEntry;
        }

        setObjectStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion)
                return std::make_pair(JSValue(), SerializationReturnCode::StackOverflowError);
            JSSet* set = JSSet::create(m_exec, m_exec->vm(), m_globalObject->setStructure());
            if (UNLIKELY(scope.exception()))
                goto error;
            m_gcBuffer.appendWithCrashOnOverflow(set);
            outputObjectStack.append(set);
            setStack.append(set);
            goto setDataStartVisitEntry;
        }
        setDataStartVisitEntry:
        case SetDataStartVisitEntry: {
            if (consumeCollectionDataTerminationIfPossible<NonSetPropertiesTag>()) {
                setStack.removeLast();
                goto objectStartVisitMember;
            }
            stateStack.append(SetDataEndVisitKey);
            goto stateUnknown;
        }
        case SetDataEndVisitKey: {
            JSSet* set = setStack.last();
            set->add(m_exec, outValue);
            goto setDataStartVisitEntry;
        }

        stateUnknown:
        case StateUnknown:
            if (JSValue terminal = readTerminal()) {
                outValue = terminal;
                break;
            }
            SerializationTag tag = readTag();
            if (tag == ArrayTag)
                goto arrayStartState;
            if (tag == ObjectTag)
                goto objectStartState;
            if (tag == MapObjectTag)
                goto mapObjectStartState;
            if (tag == SetObjectTag)
                goto setObjectStartState;
            goto error;
        }
        if (stateStack.isEmpty())
            break;

        state = stateStack.last();
        stateStack.removeLast();
    }
    ASSERT(outValue);
    ASSERT(!m_failed);
    return std::make_pair(outValue, SerializationReturnCode::SuccessfullyCompleted);
error:
    fail();
    return std::make_pair(JSValue(), SerializationReturnCode::ValidationError);
}

SerializedScriptValue::~SerializedScriptValue() = default;

SerializedScriptValue::SerializedScriptValue(Vector<uint8_t>&& buffer)
    : m_data(WTFMove(buffer))
{
}

SerializedScriptValue::SerializedScriptValue(Vector<uint8_t>&& buffer, std::unique_ptr<ArrayBufferContentsArray> arrayBufferContentsArray)
    : m_data(WTFMove(buffer))
    , m_arrayBufferContentsArray(WTFMove(arrayBufferContentsArray))
{
}

SerializedScriptValue::SerializedScriptValue(Vector<uint8_t>&& buffer, const Vector<String>& blobURLs, std::unique_ptr<ArrayBufferContentsArray> arrayBufferContentsArray, std::unique_ptr<ArrayBufferContentsArray> sharedBufferContentsArray, Vector<std::pair<std::unique_ptr<ImageBuffer>, bool>>&& imageBuffers
#if ENABLE(WEBASSEMBLY)
        , std::unique_ptr<WasmModuleArray> wasmModulesArray
#endif
        )
    : m_data(WTFMove(buffer))
    , m_arrayBufferContentsArray(WTFMove(arrayBufferContentsArray))
    , m_sharedBufferContentsArray(WTFMove(sharedBufferContentsArray))
    , m_imageBuffers(WTFMove(imageBuffers))
#if ENABLE(WEBASSEMBLY)
    , m_wasmModulesArray(WTFMove(wasmModulesArray))
#endif
{
    // Since this SerializedScriptValue is meant to be passed between threads, its String data members
    // need to be isolatedCopies so we don't run into thread safety issues for the StringImpls.
    m_blobURLs.reserveInitialCapacity(blobURLs.size());
    for (auto& url : blobURLs)
        m_blobURLs.uncheckedAppend(url.isolatedCopy());
}

static ExceptionOr<std::unique_ptr<ArrayBufferContentsArray>> transferArrayBuffers(VM& vm, const Vector<RefPtr<JSC::ArrayBuffer>>& arrayBuffers)
{
    if (arrayBuffers.isEmpty())
        return nullptr;

    auto contents = makeUnique<ArrayBufferContentsArray>(arrayBuffers.size());

    HashSet<JSC::ArrayBuffer*> visited;
    for (size_t arrayBufferIndex = 0; arrayBufferIndex < arrayBuffers.size(); arrayBufferIndex++) {
        if (visited.contains(arrayBuffers[arrayBufferIndex].get()))
            continue;
        visited.add(arrayBuffers[arrayBufferIndex].get());

        bool result = arrayBuffers[arrayBufferIndex]->transferTo(vm, contents->at(arrayBufferIndex));
        if (!result)
            return Exception { TypeError };
    }

    return contents;
}

static void maybeThrowExceptionIfSerializationFailed(ExecState& state, SerializationReturnCode code)
{
    auto& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (code) {
    case SerializationReturnCode::SuccessfullyCompleted:
        break;
    case SerializationReturnCode::StackOverflowError:
        throwException(&state, scope, createStackOverflowError(&state));
        break;
    case SerializationReturnCode::ValidationError:
        throwTypeError(&state, scope, "Unable to deserialize data."_s);
        break;
    case SerializationReturnCode::DataCloneError:
        throwDataCloneError(state, scope);
        break;
    case SerializationReturnCode::ExistingExceptionError:
    case SerializationReturnCode::UnspecifiedError:
        break;
    case SerializationReturnCode::InterruptedExecutionError:
        ASSERT_NOT_REACHED();
    }
}

static Exception exceptionForSerializationFailure(SerializationReturnCode code)
{
    ASSERT(code != SerializationReturnCode::SuccessfullyCompleted);
    
    switch (code) {
    case SerializationReturnCode::StackOverflowError:
        return Exception { StackOverflowError };
    case SerializationReturnCode::ValidationError:
        return Exception { TypeError };
    case SerializationReturnCode::DataCloneError:
        return Exception { DataCloneError };
    case SerializationReturnCode::ExistingExceptionError:
        return Exception { ExistingExceptionError };
    case SerializationReturnCode::UnspecifiedError:
        return Exception { TypeError };
    case SerializationReturnCode::SuccessfullyCompleted:
    case SerializationReturnCode::InterruptedExecutionError:
        ASSERT_NOT_REACHED();
        return Exception { TypeError };
    }
    ASSERT_NOT_REACHED();
    return Exception { TypeError };
}

RefPtr<SerializedScriptValue> SerializedScriptValue::create(ExecState& exec, JSValue value, SerializationErrorMode throwExceptions)
{
    Vector<uint8_t> buffer;
    Vector<String> blobURLs;
    Vector<RefPtr<MessagePort>> dummyMessagePorts;
    Vector<RefPtr<ImageBitmap>> dummyImageBitmaps;
    Vector<RefPtr<JSC::ArrayBuffer>> dummyArrayBuffers;
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray dummyModules;
#endif
    ArrayBufferContentsArray dummySharedBuffers;
    auto code = CloneSerializer::serialize(&exec, value, dummyMessagePorts, dummyArrayBuffers, dummyImageBitmaps,
#if ENABLE(WEBASSEMBLY)
        dummyModules,
#endif
        blobURLs, buffer, SerializationContext::Default, dummySharedBuffers);

#if ENABLE(WEBASSEMBLY)
    ASSERT_WITH_MESSAGE(dummyModules.isEmpty(), "Wasm::Module serialization is only allowed in the postMessage context");
#endif

    if (throwExceptions == SerializationErrorMode::Throwing)
        maybeThrowExceptionIfSerializationFailed(exec, code);

    if (code != SerializationReturnCode::SuccessfullyCompleted)
        return nullptr;

    return adoptRef(*new SerializedScriptValue(WTFMove(buffer), blobURLs, nullptr, nullptr, { }
#if ENABLE(WEBASSEMBLY)
        , nullptr
#endif
            ));
}

static bool containsDuplicates(const Vector<RefPtr<ImageBitmap>>& imageBitmaps)
{
    HashSet<ImageBitmap*> visited;
    for (auto& imageBitmap : imageBitmaps) {
        if (!visited.add(imageBitmap.get()))
            return true;
    }
    return false;
}

ExceptionOr<Ref<SerializedScriptValue>> SerializedScriptValue::create(ExecState& state, JSValue value, Vector<JSC::Strong<JSC::JSObject>>&& transferList, Vector<RefPtr<MessagePort>>& messagePorts, SerializationContext context)
{
    VM& vm = state.vm();
    Vector<RefPtr<JSC::ArrayBuffer>> arrayBuffers;
    Vector<RefPtr<ImageBitmap>> imageBitmaps;
    for (auto& transferable : transferList) {
        if (auto arrayBuffer = toPossiblySharedArrayBuffer(vm, transferable.get())) {
            if (arrayBuffer->isNeutered())
                return Exception { DataCloneError };
            if (arrayBuffer->isLocked()) {
                auto scope = DECLARE_THROW_SCOPE(vm);
                throwVMTypeError(&state, scope, errorMesasgeForTransfer(arrayBuffer));
                return Exception { ExistingExceptionError };
            }
            arrayBuffers.append(WTFMove(arrayBuffer));
            continue;
        }
        if (auto port = JSMessagePort::toWrapped(vm, transferable.get())) {
            // FIXME: This should check if the port is detached as per https://html.spec.whatwg.org/multipage/infrastructure.html#istransferable.
            messagePorts.append(WTFMove(port));
            continue;
        }

        if (auto imageBitmap = JSImageBitmap::toWrapped(vm, transferable.get())) {
            if (imageBitmap->isDetached())
                return Exception { DataCloneError };

            imageBitmaps.append(WTFMove(imageBitmap));
            continue;
        }

        return Exception { DataCloneError };
    }

    if (containsDuplicates(imageBitmaps))
        return Exception { DataCloneError };

    Vector<uint8_t> buffer;
    Vector<String> blobURLs;
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray wasmModules;
#endif
    std::unique_ptr<ArrayBufferContentsArray> sharedBuffers = makeUnique<ArrayBufferContentsArray>();
    auto code = CloneSerializer::serialize(&state, value, messagePorts, arrayBuffers, imageBitmaps,
#if ENABLE(WEBASSEMBLY)
        wasmModules, 
#endif
        blobURLs, buffer, context, *sharedBuffers);

    if (code != SerializationReturnCode::SuccessfullyCompleted)
        return exceptionForSerializationFailure(code);

    auto arrayBufferContentsArray = transferArrayBuffers(vm, arrayBuffers);
    if (arrayBufferContentsArray.hasException())
        return arrayBufferContentsArray.releaseException();

    auto imageBuffers = ImageBitmap::detachBitmaps(WTFMove(imageBitmaps));

    return adoptRef(*new SerializedScriptValue(WTFMove(buffer), blobURLs, arrayBufferContentsArray.releaseReturnValue(), context == SerializationContext::WorkerPostMessage ? WTFMove(sharedBuffers) : nullptr, WTFMove(imageBuffers)
#if ENABLE(WEBASSEMBLY)
                , makeUnique<WasmModuleArray>(wasmModules)
#endif
                ));
}

RefPtr<SerializedScriptValue> SerializedScriptValue::create(StringView string)
{
    Vector<uint8_t> buffer;
    if (!CloneSerializer::serialize(string, buffer))
        return nullptr;
    return adoptRef(*new SerializedScriptValue(WTFMove(buffer)));
}

RefPtr<SerializedScriptValue> SerializedScriptValue::create(JSContextRef originContext, JSValueRef apiValue, JSValueRef* exception)
{
    ExecState* exec = toJS(originContext);
    VM& vm = exec->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue value = toJS(exec, apiValue);
    auto serializedValue = SerializedScriptValue::create(*exec, value);
    if (UNLIKELY(scope.exception())) {
        if (exception)
            *exception = toRef(exec, scope.exception()->value());
        scope.clearException();
        return nullptr;
    }
    ASSERT(serializedValue);
    return serializedValue;
}

String SerializedScriptValue::toString()
{
    return CloneDeserializer::deserializeString(m_data);
}

JSValue SerializedScriptValue::deserialize(ExecState& exec, JSGlobalObject* globalObject, SerializationErrorMode throwExceptions)
{
    return deserialize(exec, globalObject, { }, throwExceptions);
}

JSValue SerializedScriptValue::deserialize(ExecState& exec, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, SerializationErrorMode throwExceptions)
{
    Vector<String> dummyBlobs;
    Vector<String> dummyPaths;
    return deserialize(exec, globalObject, messagePorts, dummyBlobs, dummyPaths, throwExceptions);
}

JSValue SerializedScriptValue::deserialize(ExecState& exec, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths, SerializationErrorMode throwExceptions)
{
    DeserializationResult result = CloneDeserializer::deserialize(&exec, globalObject, messagePorts, WTFMove(m_imageBuffers), m_arrayBufferContentsArray.get(), m_data, blobURLs, blobFilePaths, m_sharedBufferContentsArray.get()
#if ENABLE(WEBASSEMBLY)
        , m_wasmModulesArray.get()
#endif
        );
    if (throwExceptions == SerializationErrorMode::Throwing)
        maybeThrowExceptionIfSerializationFailed(exec, result.second);
    return result.first ? result.first : jsNull();
}

JSValueRef SerializedScriptValue::deserialize(JSContextRef destinationContext, JSValueRef* exception)
{
    ExecState* exec = toJS(destinationContext);
    VM& vm = exec->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue value = deserialize(*exec, exec->lexicalGlobalObject());
    if (UNLIKELY(scope.exception())) {
        if (exception)
            *exception = toRef(exec, scope.exception()->value());
        scope.clearException();
        return nullptr;
    }
    ASSERT(value);
    return toRef(exec, value);
}

Ref<SerializedScriptValue> SerializedScriptValue::nullValue()
{
    return adoptRef(*new SerializedScriptValue(Vector<uint8_t>()));
}

uint32_t SerializedScriptValue::wireFormatVersion()
{
    return CurrentVersion;
}

#if ENABLE(INDEXED_DATABASE)
Vector<String> SerializedScriptValue::blobURLsIsolatedCopy() const
{
    Vector<String> result;
    result.reserveInitialCapacity(m_blobURLs.size());
    for (auto& url : m_blobURLs)
        result.uncheckedAppend(url.isolatedCopy());

    return result;
}

void SerializedScriptValue::writeBlobsToDiskForIndexedDB(PAL::SessionID sessionID, CompletionHandler<void(IDBValue&&)>&& completionHandler)
{
    ASSERT(isMainThread());
    ASSERT(hasBlobURLs());

    blobRegistry().writeBlobsToTemporaryFiles(sessionID, m_blobURLs, [completionHandler = WTFMove(completionHandler), this, protectedThis = makeRef(*this)] (auto&& blobFilePaths) mutable {
        ASSERT(isMainThread());

        if (blobFilePaths.isEmpty()) {
            // We should have successfully written blobs to temporary files.
            // If we failed, then we can't successfully store this record.
            completionHandler({ });
            return;
        }

        ASSERT(m_blobURLs.size() == blobFilePaths.size());

        completionHandler({ *this, m_blobURLs, blobFilePaths });
    });
}

IDBValue SerializedScriptValue::writeBlobsToDiskForIndexedDBSynchronously(PAL::SessionID sessionID)
{
    ASSERT(!isMainThread());

    IDBValue value;
    Lock lock;
    Condition condition;
    lock.lock();

    RunLoop::main().dispatch([this, sessionID, conditionPtr = &condition, valuePtr = &value] {
        writeBlobsToDiskForIndexedDB(sessionID, [conditionPtr, valuePtr](IDBValue&& result) {
            ASSERT(isMainThread());
            valuePtr->setAsIsolatedCopy(result);

            conditionPtr->notifyAll();
        });
    });

    condition.wait(lock);

    return value;
}

#endif // ENABLE(INDEXED_DATABASE)

} // namespace WebCore
