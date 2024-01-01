/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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
#include "ByteArrayPixelBuffer.h"
#include "CryptoKeyAES.h"
#include "CryptoKeyEC.h"
#include "CryptoKeyHMAC.h"
#include "CryptoKeyOKP.h"
#include "CryptoKeyRSA.h"
#include "CryptoKeyRSAComponents.h"
#include "CryptoKeyRaw.h"
#include "IDBValue.h"
#include "ImageBitmapBacking.h"
#include "JSAudioWorkletGlobalScope.h"
#include "JSBlob.h"
#include "JSCryptoKey.h"
#include "JSDOMBinding.h"
#include "JSDOMConvertBufferSource.h"
#include "JSDOMException.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMMatrix.h"
#include "JSDOMPoint.h"
#include "JSDOMQuad.h"
#include "JSDOMRect.h"
#include "JSExecState.h"
#include "JSFile.h"
#include "JSFileList.h"
#include "JSIDBSerializationGlobalObject.h"
#include "JSImageBitmap.h"
#include "JSImageData.h"
#include "JSMessagePort.h"
#include "JSNavigator.h"
#include "JSRTCCertificate.h"
#include "JSRTCDataChannel.h"
#include "JSWebCodecsAudioData.h"
#include "JSWebCodecsEncodedAudioChunk.h"
#include "JSWebCodecsEncodedVideoChunk.h"
#include "JSWebCodecsVideoFrame.h"
#include "ScriptExecutionContext.h"
#include "SharedBuffer.h"
#include "WebCodecsEncodedAudioChunk.h"
#include "WebCodecsEncodedVideoChunk.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/ArrayConventions.h>
#include <JavaScriptCore/BigIntObject.h>
#include <JavaScriptCore/BooleanObject.h>
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/DateInstance.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/ErrorInstance.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/IterationKind.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSDataView.h>
#include <JavaScriptCore/JSMapInlines.h>
#include <JavaScriptCore/JSMapIterator.h>
#include <JavaScriptCore/JSSetInlines.h>
#include <JavaScriptCore/JSSetIterator.h>
#include <JavaScriptCore/JSTypedArrays.h>
#include <JavaScriptCore/JSWebAssemblyMemory.h>
#include <JavaScriptCore/JSWebAssemblyModule.h>
#include <JavaScriptCore/NumberObject.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/RegExp.h>
#include <JavaScriptCore/RegExpObject.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/TypedArrays.h>
#include <JavaScriptCore/WasmModule.h>
#include <JavaScriptCore/YarrFlags.h>
#include <limits>
#include <wtf/CheckedArithmetic.h>
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/threads/BinarySemaphore.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if PLATFORM(COCOA)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
#include "JSOffscreenCanvas.h"
#include "OffscreenCanvas.h"
#endif

#if CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN) || CPU(NEEDS_ALIGNED_ACCESS)
#define ASSUME_LITTLE_ENDIAN 0
#else
#define ASSUME_LITTLE_ENDIAN 1
#endif

namespace WebCore {

using namespace JSC;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SerializedScriptValue);

static constexpr unsigned maximumFilterRecursion = 40000;
static constexpr uint64_t autoLengthMarker = UINT64_MAX;

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
    ImageBitmapTag = 45,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    OffscreenCanvasTransferTag = 46,
#endif
    BigIntTag = 47,
    BigIntObjectTag = 48,
#if ENABLE(WEBASSEMBLY)
    WasmMemoryTag = 49,
#endif
#if ENABLE(WEB_RTC)
    RTCDataChannelTransferTag = 50,
#endif
    DOMExceptionTag = 51,
#if ENABLE(WEB_CODECS)
    WebCodecsEncodedVideoChunkTag = 52,
    WebCodecsVideoFrameTag = 53,
#endif
    ResizableArrayBufferTag = 54,
    ErrorInstanceTag = 55,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    InMemoryOffscreenCanvasTag = 56,
#endif
    InMemoryMessagePortTag = 57,
#if ENABLE(WEB_CODECS)
    WebCodecsEncodedAudioChunkTag = 58,
    WebCodecsAudioDataTag = 59,
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
    Float64ArrayTag = 9,
    BigInt64ArrayTag = 10,
    BigUint64ArrayTag = 11,
};

static bool isTypeExposedToGlobalObject(JSC::JSGlobalObject& globalObject, SerializationTag tag)
{
#if ENABLE(WEB_AUDIO)
    if (!jsDynamicCast<JSAudioWorkletGlobalScope*>(&globalObject))
        return true;

    // Only built-in JS types are exposed to audio worklets.
    switch (tag) {
    case ArrayTag:
    case ObjectTag:
    case UndefinedTag:
    case NullTag:
    case IntTag:
    case ZeroTag:
    case OneTag:
    case FalseTag:
    case TrueTag:
    case DoubleTag:
    case DateTag:
    case StringTag:
    case EmptyStringTag:
    case RegExpTag:
    case ObjectReferenceTag:
    case ArrayBufferTag:
    case ArrayBufferViewTag:
    case ArrayBufferTransferTag:
    case TrueObjectTag:
    case FalseObjectTag:
    case StringObjectTag:
    case EmptyStringObjectTag:
    case NumberObjectTag:
    case SetObjectTag:
    case MapObjectTag:
    case NonMapPropertiesTag:
    case NonSetPropertiesTag:
    case SharedArrayBufferTag:
#if ENABLE(WEBASSEMBLY)
    case WasmModuleTag:
#endif
    case BigIntTag:
    case BigIntObjectTag:
#if ENABLE(WEBASSEMBLY)
    case WasmMemoryTag:
#endif
    case ResizableArrayBufferTag:
    case ErrorInstanceTag:
    case ErrorTag:
    case MessagePortReferenceTag:
        return true;
    case FileTag:
    case FileListTag:
    case ImageDataTag:
    case BlobTag:
#if ENABLE(WEB_CRYPTO)
    case CryptoKeyTag:
#endif
    case DOMPointReadOnlyTag:
    case DOMPointTag:
    case DOMRectReadOnlyTag:
    case DOMRectTag:
    case DOMMatrixReadOnlyTag:
    case DOMMatrixTag:
    case DOMQuadTag:
    case ImageBitmapTransferTag:
#if ENABLE(WEB_RTC)
    case RTCCertificateTag:
#endif
    case ImageBitmapTag:
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    case OffscreenCanvasTransferTag:
    case InMemoryOffscreenCanvasTag:
#endif
    case InMemoryMessagePortTag:
#if ENABLE(WEB_RTC)
    case RTCDataChannelTransferTag:
#endif
    case DOMExceptionTag:
#if ENABLE(WEB_CODECS)
    case WebCodecsEncodedVideoChunkTag:
    case WebCodecsVideoFrameTag:
    case WebCodecsEncodedAudioChunkTag:
    case WebCodecsAudioDataTag:
#endif
        break;
    }
    return false;
#else
    UNUSED_PARAM(globalObject);
    UNUSED_PARAM(tag);
    return true;
#endif
}

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
    case BigInt64ArrayTag:
    case BigUint64ArrayTag:
        return 8;
    default:
        return 0;
    }
}

enum class SerializableErrorType : uint8_t {
    Error,
    EvalError,
    RangeError,
    ReferenceError,
    SyntaxError,
    TypeError,
    URIError,
    Last = URIError
};

static SerializableErrorType errorNameToSerializableErrorType(const String& name)
{
    if (equalLettersIgnoringASCIICase(name, "evalerror"_s))
        return SerializableErrorType::EvalError;
    if (equalLettersIgnoringASCIICase(name, "rangeerror"_s))
        return SerializableErrorType::RangeError;
    if (equalLettersIgnoringASCIICase(name, "referenceerror"_s))
        return SerializableErrorType::ReferenceError;
    if (equalLettersIgnoringASCIICase(name, "syntaxerror"_s))
        return SerializableErrorType::SyntaxError;
    if (equalLettersIgnoringASCIICase(name, "typeerror"_s))
        return SerializableErrorType::TypeError;
    if (equalLettersIgnoringASCIICase(name, "urierror"_s))
        return SerializableErrorType::URIError;
    return SerializableErrorType::Error;
}

static ErrorType toErrorType(SerializableErrorType value)
{
    switch (value) {
    case SerializableErrorType::Error:
        return ErrorType::Error;
    case SerializableErrorType::EvalError:
        return ErrorType::EvalError;
    case SerializableErrorType::RangeError:
        return ErrorType::RangeError;
    case SerializableErrorType::ReferenceError:
        return ErrorType::ReferenceError;
    case SerializableErrorType::SyntaxError:
        return ErrorType::SyntaxError;
    case SerializableErrorType::TypeError:
        return ErrorType::TypeError;
    case SerializableErrorType::URIError:
        return ErrorType::URIError;
    }
    return ErrorType::Error;
}

enum class PredefinedColorSpaceTag : uint8_t {
    SRGB = 0
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    , DisplayP3 = 1
#endif
};

enum DestinationColorSpaceTag {
    DestinationColorSpaceSRGBTag = 0,
#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
    DestinationColorSpaceLinearSRGBTag = 1,
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
    DestinationColorSpaceDisplayP3Tag = 2,
#endif
#if PLATFORM(COCOA)
    DestinationColorSpaceCGColorSpaceNameTag = 3,
    DestinationColorSpaceCGColorSpacePropertyListTag = 4,
#endif
};

#if ENABLE(WEBASSEMBLY)
static String agentClusterIDFromGlobalObject(JSGlobalObject& globalObject)
{
    if (!globalObject.inherits<JSDOMGlobalObject>())
        return JSDOMGlobalObject::defaultAgentClusterID();
    return jsCast<JSDOMGlobalObject*>(&globalObject)->agentClusterID();
}
#endif

#if ENABLE(WEB_CRYPTO)

const uint32_t currentKeyFormatVersion = 1;

enum class CryptoKeyClassSubtag {
    HMAC = 0,
    AES = 1,
    RSA = 2,
    EC = 3,
    Raw = 4,
    OKP = 5,
};
const uint8_t cryptoKeyClassSubtagMaximumValue = 5;

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
    ED25519 = 22,
    X25519 = 23,
};

const uint8_t cryptoAlgorithmIdentifierTagMaximumValue = 23;

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

enum class CryptoKeyOKPOpNameTag {
    X25519 = 0,
    ED25519 = 1,
};
const uint8_t cryptoKeyOKPOpNameTagMaximumValue = 1;


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
 * Version 8. added support for ImageData's colorSpace attribute.
 * Version 9. added support for ImageBitmap color space.
 * Version 10. changed the length (and offsets) of ArrayBuffers (and ArrayBufferViews) from 32 to 64 bits.
 * Version 11. added support for Blob's memory cost.
 * Version 12. added support for agent cluster ID.
 * Version 13. added support for ErrorInstance objects.
 * Version 14. encode booleans as uint8_t instead of int32_t.
 * Version 15. changed the terminator of the indexed property section in array.
 */
static constexpr unsigned CurrentVersion = 15;
static constexpr unsigned TerminatorTag = 0xFFFFFFFF;
static constexpr unsigned StringPoolTag = 0xFFFFFFFE;
static constexpr unsigned NonIndexPropertiesTag = 0xFFFFFFFD;
static constexpr uint32_t ImageDataPoolTag = 0xFFFFFFFE;

// The high bit of a StringData's length determines the character size.
static constexpr unsigned StringDataIs8BitFlag = 0x80000000;

static_assert(TerminatorTag > MAX_ARRAY_INDEX);

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
 *     ArrayTag <length:uint32_t>(<index:uint32_t><value:Value>)* TerminatorTag (NonIndexPropertiesTag (<name:StringData><value:Value>)*) TerminatorTag
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
 *    | BigInt
 *    | File
 *    | FileList
 *    | ImageData
 *    | Blob
 *    | ObjectReference
 *    | MessagePortReferenceTag <value:uint32_t>
 *    | ArrayBuffer
 *    | ArrayBufferViewTag ArrayBufferViewSubtag <byteOffset:uint64_t> <byteLength:uint64_t> (ArrayBuffer | ObjectReference)
 *    | CryptoKeyTag <wrappedKeyLength:uint32_t> <factor:byte{wrappedKeyLength}>
 *    | DOMPoint
 *    | DOMRect
 *    | DOMMatrix
 *    | DOMQuad
 *    | ImageBitmapTransferTag <value:uint32_t>
 *    | RTCCertificateTag
 *    | ImageBitmapTag <originClean:uint8_t> <logicalWidth:int32_t> <logicalHeight:int32_t> <resolutionScale:double> DestinationColorSpace <byteLength:uint32_t>(<imageByteData:uint8_t>)
 *    | OffscreenCanvasTransferTag <value:uint32_t>
 *    | WasmMemoryTag <value:uint32_t>
 *    | RTCDataChannelTransferTag <identifier:uint32_t>
 *    | DOMExceptionTag <message:String> <name:String>
 *    | WebCodecsEncodedVideoChunkTag <identifier:uint32_t>
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
 * BigInt :-
 *      BigIntTag BigIntData
 *      BigIntObjectTag BigIntData
 *
 * BigIntData :-
 *      <sign:uint8_t> <numberOfUint64Elements:uint32_t> <contents:uint64_t{numberOfUint64Elements}>
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
 *    ImageDataTag <width:int32_t> <height:int32_t> <length:uint32_t> <data:uint8_t{length}> <colorSpace:PredefinedColorSpaceTag>
 *
 * Blob :-
 *    BlobTag <url:StringData><type:StringData><size:long long><memoryCost:long long>
 *
 * RegExp :-
 *    RegExpTag <pattern:StringData><flags:StringData>
 *
 * ObjectReference :-
 *    ObjectReferenceTag <opIndex:IndexType>
 *
 * ArrayBuffer :-
 *    ArrayBufferTag <byteLength:uint64_t> <contents:byte{length}>
 *    ResizableArrayBufferTag <byteLength:uint64_t> <maxLength:uint64_t> <contents:byte{length}>
 *    ArrayBufferTransferTag <value:uint32_t>
 *    SharedArrayBufferTag <value:uint32_t>
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
 * DestinationColorSpace :-
 *        DestinationColorSpaceSRGBTag
 *      | DestinationColorSpaceLinearSRGBTag
 *      | DestinationColorSpaceDisplayP3Tag
 *      | DestinationColorSpaceCGColorSpaceNameTag <nameDataLength:uint32_t> <nameData:uint8_t>{nameDataLength}
 *      | DestinationColorSpaceCGColorSpacePropertyListTag <propertyListDataLength:uint32_t> <propertyListData:uint8_t>{propertyListDataLength}
 */

using DeserializationResult = std::pair<JSC::JSValue, SerializationReturnCode>;

class CloneBase {
    WTF_FORBID_HEAP_ALLOCATION;
protected:
    CloneBase(JSGlobalObject* lexicalGlobalObject)
        : m_lexicalGlobalObject(lexicalGlobalObject)
        , m_failed(false)
    {
    }

    void fail()
    {
        m_failed = true;
    }

    JSGlobalObject* const m_lexicalGlobalObject;
    bool m_failed;
    MarkedArgumentBuffer m_gcBuffer;
};

#if ENABLE(WEB_CRYPTO)
static bool wrapCryptoKey(JSGlobalObject* lexicalGlobalObject, const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey)
{
    auto context = executionContext(lexicalGlobalObject);
    return context && context->wrapCryptoKey(key, wrappedKey);
}

static bool unwrapCryptoKey(JSGlobalObject* lexicalGlobalObject, const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key)
{
    auto context = executionContext(lexicalGlobalObject);
    return context && context->unwrapCryptoKey(wrappedKey, key);
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
    WTF_FORBID_HEAP_ALLOCATION;
public:
    static SerializationReturnCode serialize(JSGlobalObject* lexicalGlobalObject, JSValue value, Vector<RefPtr<MessagePort>>& messagePorts, Vector<RefPtr<JSC::ArrayBuffer>>& arrayBuffers, const Vector<RefPtr<ImageBitmap>>& imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            const Vector<RefPtr<OffscreenCanvas>>& offscreenCanvases,
            Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases,
#endif
            Vector<RefPtr<MessagePort>>& inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
            const Vector<Ref<RTCDataChannel>>& rtcDataChannels,
#endif
#if ENABLE(WEB_CODECS)
            Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>& serializedVideoChunks,
            Vector<RefPtr<WebCodecsVideoFrame>>& serializedVideoFrames,
            Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>& serializedAudioChunks,
            Vector<RefPtr<WebCodecsAudioData>>& serializedAudioData,
#endif
#if ENABLE(WEBASSEMBLY)
            WasmModuleArray& wasmModules,
            WasmMemoryHandleArray& wasmMemoryHandles,
#endif
        Vector<URLKeepingBlobAlive>& blobHandles, Vector<uint8_t>& out, SerializationContext context, ArrayBufferContentsArray& sharedBuffers,
        SerializationForStorage forStorage)
    {
        CloneSerializer serializer(lexicalGlobalObject, messagePorts, arrayBuffers, imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            offscreenCanvases,
            inMemoryOffscreenCanvases,
#endif
            inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
            rtcDataChannels,
#endif
#if ENABLE(WEB_CODECS)
            serializedVideoChunks,
            serializedVideoFrames,
            serializedAudioChunks,
            serializedAudioData,
#endif
#if ENABLE(WEBASSEMBLY)
            wasmModules,
            wasmMemoryHandles,
#endif
            blobHandles, out, context, sharedBuffers, forStorage);
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

    CloneSerializer(JSGlobalObject* lexicalGlobalObject, Vector<RefPtr<MessagePort>>& messagePorts, Vector<RefPtr<JSC::ArrayBuffer>>& arrayBuffers, const Vector<RefPtr<ImageBitmap>>& imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            const Vector<RefPtr<OffscreenCanvas>>& offscreenCanvases,
            Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases,
#endif
            Vector<RefPtr<MessagePort>>& inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
            const Vector<Ref<RTCDataChannel>>& rtcDataChannels,
#endif
#if ENABLE(WEB_CODECS)
            Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>& serializedVideoChunks,
            Vector<RefPtr<WebCodecsVideoFrame>>& serializedVideoFrames,
            Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>& serializedAudioChunks,
            Vector<RefPtr<WebCodecsAudioData>>& serializedAudioData,
#endif
#if ENABLE(WEBASSEMBLY)
            WasmModuleArray& wasmModules,
            WasmMemoryHandleArray& wasmMemoryHandles,
#endif
        Vector<URLKeepingBlobAlive>& blobHandles, Vector<uint8_t>& out, SerializationContext context, ArrayBufferContentsArray& sharedBuffers, SerializationForStorage forStorage)
        : CloneBase(lexicalGlobalObject)
        , m_buffer(out)
        , m_blobHandles(blobHandles)
        , m_emptyIdentifier(Identifier::fromString(lexicalGlobalObject->vm(), emptyAtom()))
        , m_context(context)
        , m_sharedBuffers(sharedBuffers)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , m_inMemoryOffscreenCanvases(inMemoryOffscreenCanvases)
#endif
        , m_inMemoryMessagePorts(inMemoryMessagePorts)
#if ENABLE(WEBASSEMBLY)
        , m_wasmModules(wasmModules)
        , m_wasmMemoryHandles(wasmMemoryHandles)
#endif
#if ENABLE(WEB_CODECS)
        , m_serializedVideoChunks(serializedVideoChunks)
        , m_serializedVideoFrames(serializedVideoFrames)
        , m_serializedAudioChunks(serializedAudioChunks)
        , m_serializedAudioData(serializedAudioData)
#endif
        , m_forStorage(forStorage)
    {
        write(CurrentVersion);
        fillTransferMap(messagePorts, m_transferredMessagePorts);
        fillTransferMap(arrayBuffers, m_transferredArrayBuffers);
        fillTransferMap(imageBitmaps, m_transferredImageBitmaps);
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        fillTransferMap(offscreenCanvases, m_transferredOffscreenCanvases);
#endif
#if ENABLE(WEB_RTC)
        fillTransferMap(rtcDataChannels, m_transferredRTCDataChannels);
#endif
    }

    template<typename T>
    void fillTransferMap(const Vector<T>& input, ObjectPool& result)
    {
        if (input.isEmpty())
            return;

        auto* globalObject = jsCast<JSDOMGlobalObject*>(m_lexicalGlobalObject);
        for (size_t i = 0; i < input.size(); ++i) {
            if (auto* object = toJS(globalObject, globalObject, input[i].get()).getObject())
                result.add(object, i);
        }
    }

    SerializationReturnCode serialize(JSValue in);

    bool isArray(JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return object->inherits<JSArray>();
    }

    bool isMap(JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return object->inherits<JSMap>();
    }
    bool isSet(JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return object->inherits<JSSet>();
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

    JSValue getProperty(JSObject* object, const Identifier& propertyName)
    {
        PropertySlot slot(object, PropertySlot::InternalMethodType::Get);
        if (object->methodTable()->getOwnPropertySlot(object, m_lexicalGlobalObject, propertyName, slot))
            return slot.getValue(m_lexicalGlobalObject, propertyName);
        return JSValue();
    }

    void dumpImmediate(JSValue value, SerializationReturnCode& code)
    {
        if (value.isNull()) {
            write(NullTag);
            return;
        }
        if (value.isUndefined()) {
            write(UndefinedTag);
            return;
        }
        if (value.isNumber()) {
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
            return;
        }
        if (value.isBoolean()) {
            if (value.isTrue())
                write(TrueTag);
            else
                write(FalseTag);
            return;
        }
#if USE(BIGINT32)
        if (value.isBigInt32()) {
            write(BigIntTag);
            dumpBigIntData(value);
            return;
        }
#endif

        // Make any new primitive extension safe by throwing an error.
        code = SerializationReturnCode::DataCloneError;
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

    void dumpBigIntData(JSValue value)
    {
        ASSERT(value.isBigInt());
#if USE(BIGINT32)
        if (value.isBigInt32()) {
            dumpBigInt32Data(value.bigInt32AsInt32());
            return;
        }
#endif
        dumpHeapBigIntData(jsCast<JSBigInt*>(value));
    }

#if USE(BIGINT32)
    void dumpBigInt32Data(int32_t integer)
    {
        write(integer < 0);
        if (!integer) {
            write(static_cast<uint32_t>(0)); // Length-in-uint64_t
            return;
        }
        write(static_cast<uint32_t>(1)); // Length-in-uint64_t
        int64_t value = static_cast<int64_t>(integer);
        if (value < 0)
            value = -value;
        write(static_cast<uint64_t>(value));
    }
#endif

    void dumpHeapBigIntData(JSBigInt* bigInt)
    {
        write(bigInt->sign());
        if constexpr (sizeof(JSBigInt::Digit) == sizeof(uint64_t)) {
            write(static_cast<uint32_t>(bigInt->length()));
            for (unsigned index = 0; index < bigInt->length(); ++index)
                write(static_cast<uint64_t>(bigInt->digit(index)));
        } else {
            ASSERT(sizeof(JSBigInt::Digit) == sizeof(uint32_t));
            uint32_t numberOfUint64Elements = bigInt->length() / 2;
            if (bigInt->length() & 0x1)
                ++numberOfUint64Elements;
            write(numberOfUint64Elements);
            uint64_t value = 0;
            for (unsigned index = 0; index < bigInt->length(); ++index) {
                if (!(index & 0x1))
                    value = bigInt->digit(index);
                else {
                    value = (static_cast<uint64_t>(bigInt->digit(index)) << 32) | value;
                    write(static_cast<uint64_t>(value));
                    value = 0;
                }
            }
            if (bigInt->length() & 0x1)
                write(static_cast<uint64_t>(value));
        }
    }

    JSC::JSValue toJSArrayBuffer(ArrayBuffer& arrayBuffer)
    {
        auto& vm = m_lexicalGlobalObject->vm();
        auto* globalObject = m_lexicalGlobalObject;
        if (globalObject->inherits<JSDOMGlobalObject>())
            return toJS(globalObject, jsCast<JSDOMGlobalObject*>(globalObject), &arrayBuffer);

        if (auto* buffer = arrayBuffer.m_wrapper.get())
            return buffer;

        return JSC::JSArrayBuffer::create(vm, globalObject->arrayBufferStructure(arrayBuffer.sharingMode()), &arrayBuffer);
    }

    bool dumpArrayBufferView(JSObject* obj, SerializationReturnCode& code)
    {
        VM& vm = m_lexicalGlobalObject->vm();
        write(ArrayBufferViewTag);
        if (obj->inherits<JSDataView>())
            write(DataViewTag);
        else if (obj->inherits<JSUint8ClampedArray>())
            write(Uint8ClampedArrayTag);
        else if (obj->inherits<JSInt8Array>())
            write(Int8ArrayTag);
        else if (obj->inherits<JSUint8Array>())
            write(Uint8ArrayTag);
        else if (obj->inherits<JSInt16Array>())
            write(Int16ArrayTag);
        else if (obj->inherits<JSUint16Array>())
            write(Uint16ArrayTag);
        else if (obj->inherits<JSInt32Array>())
            write(Int32ArrayTag);
        else if (obj->inherits<JSUint32Array>())
            write(Uint32ArrayTag);
        else if (obj->inherits<JSFloat32Array>())
            write(Float32ArrayTag);
        else if (obj->inherits<JSFloat64Array>())
            write(Float64ArrayTag);
        else if (obj->inherits<JSBigInt64Array>())
            write(BigInt64ArrayTag);
        else if (obj->inherits<JSBigUint64Array>())
            write(BigUint64ArrayTag);
        else
            return false;

        if (UNLIKELY(jsCast<JSArrayBufferView*>(obj)->isOutOfBounds())) {
            code = SerializationReturnCode::DataCloneError;
            return true;
        }

        RefPtr<ArrayBufferView> arrayBufferView = toPossiblySharedArrayBufferView(vm, obj);
        if (arrayBufferView->isResizableOrGrowableShared()) {
            uint64_t byteOffset = arrayBufferView->byteOffsetRaw();
            write(byteOffset);
            uint64_t byteLength = arrayBufferView->byteLengthRaw();
            if (arrayBufferView->isAutoLength())
                byteLength = autoLengthMarker;
            write(byteLength);
        } else {
            uint64_t byteOffset = arrayBufferView->byteOffset();
            write(byteOffset);
            uint64_t byteLength = arrayBufferView->byteLength();
            write(byteLength);
        }
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
        if (obj->inherits<JSDOMPoint>())
            write(DOMPointTag);
        else
            write(DOMPointReadOnlyTag);

        dumpDOMPoint(jsCast<JSDOMPointReadOnly*>(obj)->wrapped());
    }

    void dumpDOMRect(JSObject* obj)
    {
        if (obj->inherits<JSDOMRect>())
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
        if (obj->inherits<JSDOMMatrix>())
            write(DOMMatrixTag);
        else
            write(DOMMatrixReadOnlyTag);

        auto& matrix = jsCast<JSDOMMatrixReadOnly*>(obj)->wrapped();
        bool is2D = matrix.is2D();
        write(is2D);
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

        auto& imageBitmap = jsCast<JSImageBitmap*>(obj)->wrapped();
        if (!imageBitmap.originClean()) {
            code = SerializationReturnCode::DataCloneError;
            return;
        }

        auto* buffer = imageBitmap.buffer();
        if (!buffer) {
            code = SerializationReturnCode::ValidationError;
            return;
        }

        // FIXME: We should try to avoid converting pixel format.
        PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, buffer->colorSpace() };
        const IntSize& logicalSize = buffer->truncatedLogicalSize();
        auto pixelBuffer = dynamicDowncast<ByteArrayPixelBuffer>(buffer->getPixelBuffer(format, { IntPoint::zero(), logicalSize }));
        if (!pixelBuffer) {
            code = SerializationReturnCode::ValidationError;
            return;
        }

        auto arrayBuffer = pixelBuffer->data().possiblySharedBuffer();
        if (!arrayBuffer) {
            code = SerializationReturnCode::ValidationError;
            return;
        }

        write(ImageBitmapTag);
        write(static_cast<uint8_t>(imageBitmap.serializationState().toRaw()));
        write(static_cast<int32_t>(logicalSize.width()));
        write(static_cast<int32_t>(logicalSize.height()));
        write(static_cast<double>(buffer->resolutionScale()));
        write(buffer->colorSpace());

        CheckedUint32 byteLength = arrayBuffer->byteLength();
        if (byteLength.hasOverflowed()) {
            code = SerializationReturnCode::ValidationError;
            return;
        }
        write(byteLength);
        write(static_cast<const uint8_t*>(arrayBuffer->data()), byteLength);
    }

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    void dumpOffscreenCanvas(JSObject* obj, SerializationReturnCode& code)
    {
        auto index = m_transferredOffscreenCanvases.find(obj);
        if (index != m_transferredOffscreenCanvases.end()) {
            write(OffscreenCanvasTransferTag);
            write(index->value);
            return;
        } else if (m_context == SerializationContext::CloneAcrossWorlds) {
            write(InMemoryOffscreenCanvasTag);
            write(static_cast<uint32_t>(m_inMemoryOffscreenCanvases.size()));
            m_inMemoryOffscreenCanvases.append(&jsCast<JSOffscreenCanvas*>(obj)->wrapped());
            return;
        }

        code = SerializationReturnCode::DataCloneError;
    }
#endif

#if ENABLE(WEB_RTC)
    void dumpRTCDataChannel(JSObject* obj, SerializationReturnCode& code)
    {
        auto index = m_transferredRTCDataChannels.find(obj);
        if (index != m_transferredRTCDataChannels.end()) {
            write(RTCDataChannelTransferTag);
            write(index->value);
            return;
        }

        code = SerializationReturnCode::DataCloneError;
    }
#endif
#if ENABLE(WEB_CODECS)
    void dumpWebCodecsEncodedVideoChunk(JSObject* obj)
    {
        auto& videoChunk = jsCast<JSWebCodecsEncodedVideoChunk*>(obj)->wrapped();

        auto index = m_serializedVideoChunks.find(&videoChunk.storage());
        if (index == notFound) {
            index = m_serializedVideoChunks.size();
            m_serializedVideoChunks.append(&videoChunk.storage());
        }

        write(WebCodecsEncodedVideoChunkTag);
        write(static_cast<uint32_t>(index));
    }

    bool dumpWebCodecsVideoFrame(JSObject* obj)
    {
        Ref videoFrame = jsCast<JSWebCodecsVideoFrame*>(obj)->wrapped();
        if (videoFrame->isDetached())
            return false;

        auto index = m_serializedVideoFrames.find(videoFrame.ptr());
        if (index == notFound) {
            index = m_serializedVideoFrames.size();
            m_serializedVideoFrames.append(WTFMove(videoFrame));
        }
        write(WebCodecsVideoFrameTag);
        write(static_cast<uint32_t>(index));
        return true;
    }

    void dumpWebCodecsEncodedAudioChunk(JSObject* obj)
    {
        auto& audioChunk = jsCast<JSWebCodecsEncodedAudioChunk*>(obj)->wrapped();

        auto index = m_serializedAudioChunks.find(&audioChunk.storage());
        if (index == notFound) {
            index = m_serializedAudioChunks.size();
            m_serializedAudioChunks.append(&audioChunk.storage());
        }

        write(WebCodecsEncodedAudioChunkTag);
        write(static_cast<uint32_t>(index));
    }

    bool dumpWebCodecsAudioData(JSObject* obj)
    {
        Ref audioData = jsCast<JSWebCodecsAudioData*>(obj)->wrapped();
        if (audioData->isDetached())
            return false;

        auto index = m_serializedAudioData.find(audioData.ptr());
        if (index == notFound) {
            index = m_serializedAudioData.size();
            m_serializedAudioData.append(WTFMove(audioData));
        }
        write(WebCodecsAudioDataTag);
        write(static_cast<uint32_t>(index));
        return true;
    }
#endif

    void dumpDOMException(JSObject* obj, SerializationReturnCode& code)
    {
        if (auto* exception = JSDOMException::toWrapped(m_lexicalGlobalObject->vm(), obj)) {
            write(DOMExceptionTag);
            write(exception->message());
            write(exception->name());
            return;
        }

        code = SerializationReturnCode::DataCloneError;
    }

    bool dumpIfTerminal(JSValue value, SerializationReturnCode& code)
    {
        if (!value.isCell()) {
            dumpImmediate(value, code);
            return true;
        }
        ASSERT(value.isCell());

        if (value.isString()) {
            dumpString(asString(value)->value(m_lexicalGlobalObject));
            return true;
        }

        if (value.isHeapBigInt()) {
            write(BigIntTag);
            dumpBigIntData(value);
            return true;
        }

        if (value.isSymbol()) {
            code = SerializationReturnCode::DataCloneError;
            return true;
        }

        VM& vm = m_lexicalGlobalObject->vm();
        if (isArray(value))
            return false;

        if (value.isObject()) {
            auto* obj = asObject(value);
            if (auto* dateObject = jsDynamicCast<DateInstance*>(obj)) {
                write(DateTag);
                write(dateObject->internalNumber());
                return true;
            }
            if (auto* booleanObject = jsDynamicCast<BooleanObject*>(obj)) {
                if (!startObjectInternal(booleanObject)) // handle duplicates
                    return true;
                write(booleanObject->internalValue().toBoolean(m_lexicalGlobalObject) ? TrueObjectTag : FalseObjectTag);
                return true;
            }
            if (auto* stringObject = jsDynamicCast<StringObject*>(obj)) {
                if (!startObjectInternal(stringObject)) // handle duplicates
                    return true;
                String str = asString(stringObject->internalValue())->value(m_lexicalGlobalObject);
                dumpStringObject(str);
                return true;
            }
            if (auto* numberObject = jsDynamicCast<NumberObject*>(obj)) {
                if (!startObjectInternal(numberObject)) // handle duplicates
                    return true;
                write(NumberObjectTag);
                write(numberObject->internalValue().asNumber());
                return true;
            }
            if (auto* bigIntObject = jsDynamicCast<BigIntObject*>(obj)) {
                if (!startObjectInternal(bigIntObject)) // handle duplicates
                    return true;
                JSValue bigIntValue = bigIntObject->internalValue();
                ASSERT(bigIntValue.isBigInt());
                write(BigIntObjectTag);
                dumpBigIntData(bigIntValue);
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
                m_blobHandles.append(blob->handle().isolatedCopy());
                write(blob->url().string());
                write(blob->type());
                static_assert(sizeof(uint64_t) == sizeof(decltype(blob->size())));
                uint64_t size = blob->size();
                write(size);
                uint64_t memoryCost = blob->memoryCost();
                write(memoryCost);
                return true;
            }
            if (auto* data = JSImageData::toWrapped(vm, obj)) {
                write(ImageDataTag);
                auto addResult = m_imageDataPool.add(*data, m_imageDataPool.size());
                if (!addResult.isNewEntry) {
                    write(ImageDataPoolTag);
                    writeImageDataIndex(addResult.iterator->value);
                    return true;
                }
                write(static_cast<uint32_t>(data->width()));
                write(static_cast<uint32_t>(data->height()));
                CheckedUint32 dataLength = data->data().length();
                if (dataLength.hasOverflowed()) {
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }
                write(dataLength);
                write(data->data().data(), dataLength);
                write(data->colorSpace());
                return true;
            }
            if (auto* regExp = jsDynamicCast<RegExpObject*>(obj)) {
                write(RegExpTag);
                write(regExp->regExp()->pattern());
                write(String::fromLatin1(JSC::Yarr::flagsString(regExp->regExp()->flags()).data()));
                return true;
            }
            if (auto* errorInstance = jsDynamicCast<ErrorInstance*>(obj)) {
                auto& vm = m_lexicalGlobalObject->vm();
                auto scope = DECLARE_THROW_SCOPE(vm);
                auto errorTypeValue = errorInstance->get(m_lexicalGlobalObject, vm.propertyNames->name);
                RETURN_IF_EXCEPTION(scope, false);
                auto errorTypeString = errorTypeValue.toWTFString(m_lexicalGlobalObject);
                RETURN_IF_EXCEPTION(scope, false);

                String message;
                PropertyDescriptor messageDescriptor;
                if (errorInstance->getOwnPropertyDescriptor(m_lexicalGlobalObject, vm.propertyNames->message, messageDescriptor) && messageDescriptor.isDataDescriptor()) {
                    EXCEPTION_ASSERT(!scope.exception());
                    message = messageDescriptor.value().toWTFString(m_lexicalGlobalObject);
                }
                RETURN_IF_EXCEPTION(scope, false);

                unsigned line = 0;
                PropertyDescriptor lineDescriptor;
                if (errorInstance->getOwnPropertyDescriptor(m_lexicalGlobalObject, vm.propertyNames->line, lineDescriptor) && lineDescriptor.isDataDescriptor()) {
                    EXCEPTION_ASSERT(!scope.exception());
                    line = lineDescriptor.value().toNumber(m_lexicalGlobalObject);
                }
                RETURN_IF_EXCEPTION(scope, false);

                unsigned column = 0;
                PropertyDescriptor columnDescriptor;
                if (errorInstance->getOwnPropertyDescriptor(m_lexicalGlobalObject, vm.propertyNames->column, columnDescriptor) && columnDescriptor.isDataDescriptor()) {
                    EXCEPTION_ASSERT(!scope.exception());
                    column = columnDescriptor.value().toNumber(m_lexicalGlobalObject);
                }
                RETURN_IF_EXCEPTION(scope, false);

                String sourceURL;
                PropertyDescriptor sourceURLDescriptor;
                if (errorInstance->getOwnPropertyDescriptor(m_lexicalGlobalObject, vm.propertyNames->sourceURL, sourceURLDescriptor) && sourceURLDescriptor.isDataDescriptor()) {
                    EXCEPTION_ASSERT(!scope.exception());
                    sourceURL = sourceURLDescriptor.value().toWTFString(m_lexicalGlobalObject);
                }
                RETURN_IF_EXCEPTION(scope, false);

                String stack;
                PropertyDescriptor stackDescriptor;
                if (errorInstance->getOwnPropertyDescriptor(m_lexicalGlobalObject, vm.propertyNames->stack, stackDescriptor) && stackDescriptor.isDataDescriptor()) {
                    EXCEPTION_ASSERT(!scope.exception());
                    stack = stackDescriptor.value().toWTFString(m_lexicalGlobalObject);
                }
                RETURN_IF_EXCEPTION(scope, false);

                write(ErrorInstanceTag);
                write(errorNameToSerializableErrorType(errorTypeString));
                writeNullableString(message);
                write(line);
                write(column);
                writeNullableString(sourceURL);
                writeNullableString(stack);
                return true;
            }
            if (obj->inherits<JSMessagePort>()) {
                auto index = m_transferredMessagePorts.find(obj);
                if (index != m_transferredMessagePorts.end()) {
                    write(MessagePortReferenceTag);
                    write(index->value);
                    return true;
                } else if (m_context == SerializationContext::CloneAcrossWorlds) {
                    write(InMemoryMessagePortTag);
                    write(static_cast<uint32_t>(m_inMemoryMessagePorts.size()));
                    m_inMemoryMessagePorts.append(&jsCast<JSMessagePort*>(obj)->wrapped());
                    return true;
                }
                // MessagePort object could not be found in transferred message ports
                code = SerializationReturnCode::ValidationError;
                return true;
            }
            if (auto* arrayBuffer = toPossiblySharedArrayBuffer(vm, obj)) {
                if (arrayBuffer->isDetached()) {
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
                    // https://html.spec.whatwg.org/multipage/structured-data.html#structuredserializeinternal
                    if (!JSC::Options::useSharedArrayBuffer()) {
                        code = SerializationReturnCode::DataCloneError;
                        return true;
                    }
                    uint32_t index = m_sharedBuffers.size();
                    ArrayBufferContents contents;
                    if (arrayBuffer->shareWith(contents)) {
                        write(SharedArrayBufferTag);
                        m_sharedBuffers.append(WTFMove(contents));
                        write(index);
                        return true;
                    }
                }
                
                if (arrayBuffer->isResizableOrGrowableShared()) {
                    write(ResizableArrayBufferTag);
                    uint64_t byteLength = arrayBuffer->byteLength();
                    write(byteLength);
                    uint64_t maxByteLength = arrayBuffer->maxByteLength().value_or(0);
                    write(maxByteLength);
                    write(static_cast<const uint8_t*>(arrayBuffer->data()), byteLength);
                    return true;
                }

                write(ArrayBufferTag);
                uint64_t byteLength = arrayBuffer->byteLength();
                write(byteLength);
                write(static_cast<const uint8_t*>(arrayBuffer->data()), byteLength);
                return true;
            }
            if (obj->inherits<JSArrayBufferView>()) {
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
                Vector<URLKeepingBlobAlive> dummyBlobHandles;
                Vector<RefPtr<MessagePort>> dummyMessagePorts;
                Vector<RefPtr<JSC::ArrayBuffer>> dummyArrayBuffers;
#if ENABLE(WEB_CODECS)
                Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>> dummyVideoChunks;
                Vector<RefPtr<WebCodecsVideoFrame>> dummyVideoFrames;
                Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>> dummyAudioChunks;
                Vector<RefPtr<WebCodecsAudioData>> dummyAudioData;
#endif
#if ENABLE(WEBASSEMBLY)
                WasmModuleArray dummyModules;
                WasmMemoryHandleArray dummyMemoryHandles;
#endif
                ArrayBufferContentsArray dummySharedBuffers;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
                Vector<RefPtr<OffscreenCanvas>> dummyInMemoryOffscreenCanvases;
#endif
                Vector<RefPtr<MessagePort>> dummyInMemoryMessagePorts;
                CloneSerializer rawKeySerializer(m_lexicalGlobalObject, dummyMessagePorts, dummyArrayBuffers, { },
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
                    { },
                    dummyInMemoryOffscreenCanvases,
#endif
                    dummyInMemoryMessagePorts,
#if ENABLE(WEB_RTC)
                    { },
#endif
#if ENABLE(WEB_CODECS)
                    dummyVideoChunks,
                    dummyVideoFrames,
                    dummyAudioChunks,
                    dummyAudioData,
#endif
#if ENABLE(WEBASSEMBLY)
                    dummyModules,
                    dummyMemoryHandles,
#endif
                    dummyBlobHandles, serializedKey, SerializationContext::Default, dummySharedBuffers, m_forStorage);
                rawKeySerializer.write(key);
                Vector<uint8_t> wrappedKey;
                if (!wrapCryptoKey(m_lexicalGlobalObject, serializedKey, wrappedKey))
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
            if (JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(obj)) {
                if (m_context != SerializationContext::WorkerPostMessage && m_context != SerializationContext::WindowPostMessage)
                    return false;

                uint32_t index = m_wasmModules.size(); 
                m_wasmModules.append(&module->module());
                write(WasmModuleTag);
                write(agentClusterIDFromGlobalObject(*m_lexicalGlobalObject));
                write(index);
                return true;
            }
            if (JSWebAssemblyMemory* memory = jsDynamicCast<JSWebAssemblyMemory*>(obj)) {
                if (!JSC::Options::useSharedArrayBuffer() || memory->memory().sharingMode() != JSC::MemorySharingMode::Shared) {
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }
                if (m_context != SerializationContext::WorkerPostMessage) {
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }
                uint32_t index = m_wasmMemoryHandles.size();
                m_wasmMemoryHandles.append(memory->memory().shared());
                write(WasmMemoryTag);
                write(agentClusterIDFromGlobalObject(*m_lexicalGlobalObject));
                write(index);
                return true;
            }
#endif
            if (obj->inherits<JSDOMPointReadOnly>()) {
                dumpDOMPoint(obj);
                return true;
            }
            if (obj->inherits<JSDOMRectReadOnly>()) {
                dumpDOMRect(obj);
                return true;
            }
            if (obj->inherits<JSDOMMatrixReadOnly>()) {
                dumpDOMMatrix(obj);
                return true;
            }
            if (obj->inherits<JSDOMQuad>()) {
                dumpDOMQuad(obj);
                return true;
            }
            if (obj->inherits<JSImageBitmap>()) {
                dumpImageBitmap(obj, code);
                return true;
            }
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            if (obj->inherits<JSOffscreenCanvas>()) {
                dumpOffscreenCanvas(obj, code);
                return true;
            }
#endif
#if ENABLE(WEB_RTC)
            if (obj->inherits<JSRTCDataChannel>()) {
                dumpRTCDataChannel(obj, code);
                return true;
            }
#endif
            if (obj->inherits<JSDOMException>()) {
                dumpDOMException(obj, code);
                return true;
            }
#if ENABLE(WEB_CODECS)
            if (obj->inherits<JSWebCodecsEncodedVideoChunk>()) {
                if (m_forStorage == SerializationForStorage::Yes)
                    return false;
                dumpWebCodecsEncodedVideoChunk(obj);
                return true;
            }
            if (obj->inherits<JSWebCodecsVideoFrame>()) {
                if (m_forStorage == SerializationForStorage::Yes)
                    return false;
                return dumpWebCodecsVideoFrame(obj);
            }
            if (obj->inherits<JSWebCodecsEncodedAudioChunk>()) {
                if (m_forStorage == SerializationForStorage::Yes)
                    return false;
                dumpWebCodecsEncodedAudioChunk(obj);
                return true;
            }
            if (obj->inherits<JSWebCodecsAudioData>()) {
                if (m_forStorage == SerializationForStorage::Yes)
                    return false;
                return dumpWebCodecsAudioData(obj);
            }
#endif

            return false;
        }
        // Any other types are expected to serialize as null.
        write(NullTag);
        return true;
    }

    void write(SerializableErrorType errorType)
    {
        write(enumToUnderlyingType(errorType));
    }

    void write(SerializationTag tag)
    {
        writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(ArrayBufferViewSubtag tag)
    {
        writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(DestinationColorSpaceTag tag)
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

    void write(CryptoKeyOKPOpNameTag tag)
    {
        writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }
#endif

    void write(bool b)
    {
        write(static_cast<uint8_t>(b));
    }

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

    void write(uint64_t i)
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

    void writeImageDataIndex(unsigned i)
    {
        writeConstantPoolIndex(m_imageDataPool, i);
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
            write(Identifier::fromString(m_lexicalGlobalObject->vm(), str));
    }

    void writeNullableString(const String& str)
    {
        bool isNull = str.isNull();
        write(isNull);
        if (!isNull)
            write(Identifier::fromString(m_lexicalGlobalObject->vm(), str));
    }

    void write(const Vector<uint8_t>& vector)
    {
        uint32_t size = vector.size();
        write(size);
        writeLittleEndian(m_buffer, vector.data(), size);
    }

    void write(const File& file)
    {
        m_blobHandles.append(file.handle().isolatedCopy());
        write(file.path());
        write(file.url().string());
        write(file.type());
        write(file.name());
        write(static_cast<double>(file.lastModifiedOverride().value_or(-1)));
    }

    void write(PredefinedColorSpace colorSpace)
    {
        switch (colorSpace) {
        case PredefinedColorSpace::SRGB:
            writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(PredefinedColorSpaceTag::SRGB));
            break;
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        case PredefinedColorSpace::DisplayP3:
            writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(PredefinedColorSpaceTag::DisplayP3));
            break;
#endif
        }
    }

#if PLATFORM(COCOA)
    void write(const RetainPtr<CFDataRef>& data)
    {
        uint32_t dataLength = CFDataGetLength(data.get());
        write(dataLength);
        write(CFDataGetBytePtr(data.get()), dataLength);
    }
#endif

    void write(DestinationColorSpace destinationColorSpace)
    {
        if (destinationColorSpace == DestinationColorSpace::SRGB()) {
            write(DestinationColorSpaceSRGBTag);
            return;
        }

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
        if (destinationColorSpace == DestinationColorSpace::LinearSRGB()) {
            write(DestinationColorSpaceLinearSRGBTag);
            return;
        }
#endif

#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
        if (destinationColorSpace == DestinationColorSpace::DisplayP3()) {
            write(DestinationColorSpaceDisplayP3Tag);
            return;
        }
#endif

#if PLATFORM(COCOA)
        auto colorSpace = destinationColorSpace.platformColorSpace();

        if (auto name = CGColorSpaceGetName(colorSpace)) {
            auto data = adoptCF(CFStringCreateExternalRepresentation(nullptr, name, kCFStringEncodingUTF8, 0));
            if (!data) {
                write(DestinationColorSpaceSRGBTag);
                return;
            }

            write(DestinationColorSpaceCGColorSpaceNameTag);
            write(data);
            return;
        }

        if (auto propertyList = adoptCF(CGColorSpaceCopyPropertyList(colorSpace))) {
            auto data = adoptCF(CFPropertyListCreateData(nullptr, propertyList.get(), kCFPropertyListBinaryFormat_v1_0, 0, nullptr));
            if (!data) {
                write(DestinationColorSpaceSRGBTag);
                return;
            }

            write(DestinationColorSpaceCGColorSpacePropertyListTag);
            write(data);
            return;
        }
#endif

        ASSERT_NOT_REACHED();
        write(DestinationColorSpaceSRGBTag);
    }

#if ENABLE(WEB_CRYPTO)
    void write(CryptoKeyOKP::NamedCurve curve)
    {
        switch (curve) {
        case CryptoKeyOKP::NamedCurve::X25519:
            write(CryptoKeyOKPOpNameTag::X25519);
            break;
        case CryptoKeyOKP::NamedCurve::Ed25519:
            write(CryptoKeyOKPOpNameTag::ED25519);
            break;
        }
    }

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
        case CryptoAlgorithmIdentifier::Ed25519:
            write(CryptoAlgorithmIdentifierTag::ED25519);
            break;
        case CryptoAlgorithmIdentifier::X25519:
            write(CryptoAlgorithmIdentifierTag::X25519);
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
        case CryptoKeyClass::RSA: {
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
        case CryptoKeyClass::OKP:
            write(CryptoKeyClassSubtag::OKP);
            write(key->algorithmIdentifier());
            write(downcast<CryptoKeyOKP>(*key).namedCurve());
            write(downcast<CryptoKeyOKP>(*key).platformKey());
            break;
        }
    }
#endif

    void write(const uint8_t* data, unsigned length)
    {
        m_buffer.append(data, length);
    }

    Vector<uint8_t>& m_buffer;
    Vector<URLKeepingBlobAlive>& m_blobHandles;
    ObjectPool m_objectPool;
    ObjectPool m_transferredMessagePorts;
    ObjectPool m_transferredArrayBuffers;
    ObjectPool m_transferredImageBitmaps;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    ObjectPool m_transferredOffscreenCanvases;
#endif
#if ENABLE(WEB_RTC)
    ObjectPool m_transferredRTCDataChannels;
#endif
    typedef HashMap<RefPtr<UniquedStringImpl>, uint32_t, IdentifierRepHash> StringConstantPool;
    StringConstantPool m_constantPool;
    using ImageDataPool = HashMap<Ref<ImageData>, uint32_t>;
    ImageDataPool m_imageDataPool;
    Identifier m_emptyIdentifier;
    SerializationContext m_context;
    ArrayBufferContentsArray& m_sharedBuffers;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<RefPtr<OffscreenCanvas>>& m_inMemoryOffscreenCanvases;
#endif
    Vector<RefPtr<MessagePort>>& m_inMemoryMessagePorts;
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray& m_wasmModules;
    WasmMemoryHandleArray& m_wasmMemoryHandles;
#endif
#if ENABLE(WEB_CODECS)
    Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>& m_serializedVideoChunks;
    Vector<RefPtr<WebCodecsVideoFrame>>& m_serializedVideoFrames;
    Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>& m_serializedAudioChunks;
    Vector<RefPtr<WebCodecsAudioData>>& m_serializedAudioData;
#endif
    SerializationForStorage m_forStorage;
};

SerializationReturnCode CloneSerializer::serialize(JSValue in)
{
    VM& vm = m_lexicalGlobalObject->vm();
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
    auto scope = DECLARE_THROW_SCOPE(vm);
    while (1) {
        switch (state) {
            arrayStartState:
            case ArrayStartState: {
                ASSERT(isArray(inValue));
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
                    write(TerminatorTag); // Terminate the indexed property section.

                    propertyStack.append(PropertyNameArray(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    array->getOwnNonIndexPropertyNames(m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                    if (UNLIKELY(scope.exception()))
                        return SerializationReturnCode::ExistingExceptionError;
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
                inValue = array->getDirectIndex(m_lexicalGlobalObject, index);
                if (UNLIKELY(scope.exception()))
                    return SerializationReturnCode::ExistingExceptionError;
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
                if (inObject->classInfo() != JSFinalObject::info())
                    return SerializationReturnCode::DataCloneError;
                inputObjectStack.append(inObject);
                indexStack.append(0);
                propertyStack.append(PropertyNameArray(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                inObject->methodTable()->getOwnPropertyNames(inObject, m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                if (UNLIKELY(scope.exception()))
                    return SerializationReturnCode::ExistingExceptionError;
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
                inValue = getProperty(object, properties[index]);
                if (UNLIKELY(scope.exception()))
                    return SerializationReturnCode::ExistingExceptionError;

                if (!inValue) {
                    // Property was removed during serialisation
                    indexStack.last()++;
                    goto objectStartVisitMember;
                }
                write(properties[index]);

                if (UNLIKELY(scope.exception()))
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
                if (UNLIKELY(scope.exception()))
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
                JSMapIterator* iterator = JSMapIterator::create(vm, m_lexicalGlobalObject->mapIteratorStructure(), inMap, IterationKind::Entries);
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
                if (!iterator->nextKeyValue(m_lexicalGlobalObject, key, value)) {
                    mapIteratorStack.removeLast();
                    JSObject* object = inputObjectStack.last();
                    ASSERT(jsDynamicCast<JSMap*>(object));
                    propertyStack.append(PropertyNameArray(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    object->methodTable()->getOwnPropertyNames(object, m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                    if (UNLIKELY(scope.exception()))
                        return SerializationReturnCode::ExistingExceptionError;
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
                JSSetIterator* iterator = JSSetIterator::create(vm, m_lexicalGlobalObject->setIteratorStructure(), inSet, IterationKind::Keys);
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
                if (!iterator->next(m_lexicalGlobalObject, key)) {
                    setIteratorStack.removeLast();
                    JSObject* object = inputObjectStack.last();
                    ASSERT(jsDynamicCast<JSSet*>(object));
                    propertyStack.append(PropertyNameArray(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    object->methodTable()->getOwnPropertyNames(object, m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                    if (UNLIKELY(scope.exception()))
                        return SerializationReturnCode::ExistingExceptionError;
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

                if (isArray(inValue))
                    goto arrayStartState;
                if (isMap(inValue))
                    goto mapStartState;
                if (isSet(inValue))
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
    WTF_FORBID_HEAP_ALLOCATION;
public:
    enum class ShouldAtomize : bool { No, Yes };
    static String deserializeString(const Vector<uint8_t>& buffer, ShouldAtomize shouldAtomize = ShouldAtomize::No)
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
        if (!readString(ptr, end, str, length, is8Bit, shouldAtomize))
            return String();
        return str;
    }

    static DeserializationResult deserialize(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, Vector<std::optional<ImageBitmapBacking>>&& backingStores
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases
        , const Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases
#endif
        , const Vector<RefPtr<MessagePort>>& inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
#endif
        , ArrayBufferContentsArray* arrayBufferContentsArray, const Vector<uint8_t>& buffer, const Vector<String>& blobURLs, const Vector<String> blobFilePaths, ArrayBufferContentsArray* sharedBuffers
#if ENABLE(WEBASSEMBLY)
        , WasmModuleArray* wasmModules
        , WasmMemoryHandleArray* wasmMemoryHandles
#endif
#if ENABLE(WEB_CODECS)
        , Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks
        , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames
        , Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks
        , Vector<WebCodecsAudioInternalData>&& serializedAudioData
#endif
        )
    {
        if (!buffer.size())
            return std::make_pair(jsNull(), SerializationReturnCode::UnspecifiedError);
        CloneDeserializer deserializer(lexicalGlobalObject, globalObject, messagePorts, arrayBufferContentsArray, buffer, blobURLs, blobFilePaths, sharedBuffers, WTFMove(backingStores)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            , WTFMove(detachedOffscreenCanvases)
            , inMemoryOffscreenCanvases
#endif
            , inMemoryMessagePorts
#if ENABLE(WEB_RTC)
            , WTFMove(detachedRTCDataChannels)
#endif
#if ENABLE(WEBASSEMBLY)
            , wasmModules
            , wasmMemoryHandles
#endif
#if ENABLE(WEB_CODECS)
            , WTFMove(serializedVideoChunks)
            , WTFMove(serializedVideoFrames)
            , WTFMove(serializedAudioChunks)
            , WTFMove(serializedAudioData)
#endif
            );
        if (!deserializer.isValid())
            return std::make_pair(JSValue(), SerializationReturnCode::ValidationError);
        return deserializer.deserialize();
    }

private:
    struct CachedString {
        CachedString(String&& string)
            : m_string(WTFMove(string))
        {
        }

        JSValue jsString(JSGlobalObject* lexicalGlobalObject)
        {
            if (!m_jsString)
                m_jsString = JSC::jsString(lexicalGlobalObject->vm(), m_string);
            return m_jsString;
        }
        const String& string() { return m_string; }
        String takeString() { return WTFMove(m_string); }

    private:
        String m_string;
        JSValue m_jsString;
    };

    struct CachedStringRef {
        CachedStringRef() = default;

        CachedStringRef(Vector<CachedString>* base, size_t index)
            : m_base(base)
            , m_index(index)
        {
        }
        
        CachedString* operator->() { ASSERT(m_base); return &m_base->at(m_index); }
        
    private:
        Vector<CachedString>* m_base { nullptr };
        size_t m_index { 0 };
    };

    CloneDeserializer(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, ArrayBufferContentsArray* arrayBufferContents, Vector<std::optional<ImageBitmapBacking>>&& backingStores, const Vector<uint8_t>& buffer
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases = { }
        , const Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases = { }
#endif
        , const Vector<RefPtr<MessagePort>>& inMemoryMessagePorts = { }
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels = { }
#endif
#if ENABLE(WEBASSEMBLY)
        , WasmModuleArray* wasmModules = nullptr
        , WasmMemoryHandleArray* wasmMemoryHandles = nullptr
#endif
#if ENABLE(WEB_CODECS)
        , Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks = { }
        , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames = { }
        , Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks = { }
        , Vector<WebCodecsAudioInternalData>&& serializedAudioData = { }
#endif
        )
        : CloneBase(lexicalGlobalObject)
        , m_globalObject(globalObject)
        , m_isDOMGlobalObject(globalObject->inherits<JSDOMGlobalObject>())
        , m_canCreateDOMObject(m_isDOMGlobalObject && !globalObject->inherits<JSIDBSerializationGlobalObject>())
        , m_ptr(buffer.data())
        , m_end(buffer.data() + buffer.size())
        , m_version(0xFFFFFFFF)
        , m_messagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_backingStores(WTFMove(backingStores))
        , m_imageBitmaps(m_backingStores.size())
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , m_detachedOffscreenCanvases(WTFMove(detachedOffscreenCanvases))
        , m_offscreenCanvases(m_detachedOffscreenCanvases.size())
        , m_inMemoryOffscreenCanvases(inMemoryOffscreenCanvases)
#endif
        , m_inMemoryMessagePorts(inMemoryMessagePorts)
#if ENABLE(WEB_RTC)
        , m_detachedRTCDataChannels(WTFMove(detachedRTCDataChannels))
        , m_rtcDataChannels(m_detachedRTCDataChannels.size())
#endif
#if ENABLE(WEBASSEMBLY)
        , m_wasmModules(wasmModules)
        , m_wasmMemoryHandles(wasmMemoryHandles)
#endif
#if ENABLE(WEB_CODECS)
        , m_serializedVideoChunks(WTFMove(serializedVideoChunks))
        , m_videoChunks(m_serializedVideoChunks.size())
        , m_serializedVideoFrames(WTFMove(serializedVideoFrames))
        , m_videoFrames(m_serializedVideoFrames.size())
        , m_serializedAudioChunks(WTFMove(serializedAudioChunks))
        , m_audioChunks(m_serializedAudioChunks.size())
        , m_serializedAudioData(WTFMove(serializedAudioData))
        , m_audioData(m_serializedAudioData.size())
#endif
    {
        if (!read(m_version))
            m_version = 0xFFFFFFFF;
    }

    CloneDeserializer(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, ArrayBufferContentsArray* arrayBufferContents, const Vector<uint8_t>& buffer, const Vector<String>& blobURLs, const Vector<String> blobFilePaths, ArrayBufferContentsArray* sharedBuffers, Vector<std::optional<ImageBitmapBacking>>&& backingStores
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases
        , const Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases
#endif
        , const Vector<RefPtr<MessagePort>>& inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
#endif
#if ENABLE(WEBASSEMBLY)
        , WasmModuleArray* wasmModules
        , WasmMemoryHandleArray* wasmMemoryHandles
#endif
#if ENABLE(WEB_CODECS)
        , Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks = { }
        , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames = { }
        , Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks = { }
        , Vector<WebCodecsAudioInternalData>&& serializedAudioData = { }
#endif
        )
        : CloneBase(lexicalGlobalObject)
        , m_globalObject(globalObject)
        , m_isDOMGlobalObject(globalObject->inherits<JSDOMGlobalObject>())
        , m_canCreateDOMObject(m_isDOMGlobalObject && !globalObject->inherits<JSIDBSerializationGlobalObject>())
        , m_ptr(buffer.data())
        , m_end(buffer.data() + buffer.size())
        , m_version(0xFFFFFFFF)
        , m_messagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_blobURLs(blobURLs)
        , m_blobFilePaths(blobFilePaths)
        , m_sharedBuffers(sharedBuffers)
        , m_backingStores(WTFMove(backingStores))
        , m_imageBitmaps(m_backingStores.size())
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , m_detachedOffscreenCanvases(WTFMove(detachedOffscreenCanvases))
        , m_offscreenCanvases(m_detachedOffscreenCanvases.size())
        , m_inMemoryOffscreenCanvases(inMemoryOffscreenCanvases)
#endif
        , m_inMemoryMessagePorts(inMemoryMessagePorts)
#if ENABLE(WEB_RTC)
        , m_detachedRTCDataChannels(WTFMove(detachedRTCDataChannels))
        , m_rtcDataChannels(m_detachedRTCDataChannels.size())
#endif
#if ENABLE(WEBASSEMBLY)
        , m_wasmModules(wasmModules)
        , m_wasmMemoryHandles(wasmMemoryHandles)
#endif
#if ENABLE(WEB_CODECS)
        , m_serializedVideoChunks(WTFMove(serializedVideoChunks))
        , m_videoChunks(m_serializedVideoChunks.size())
        , m_serializedVideoFrames(WTFMove(serializedVideoFrames))
        , m_videoFrames(m_serializedVideoFrames.size())
        , m_serializedAudioChunks(WTFMove(serializedAudioChunks))
        , m_audioChunks(m_serializedAudioChunks.size())
        , m_serializedAudioData(WTFMove(serializedAudioData))
        , m_audioData(m_serializedAudioData.size())
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

    enum class ForceReadingAs8Bit : bool { No, Yes };
    bool read(bool& b, ForceReadingAs8Bit forceReadingAs8Bit = ForceReadingAs8Bit::No)
    {
        if (m_version >= 14 || forceReadingAs8Bit == ForceReadingAs8Bit::Yes) {
            uint8_t integer;
            if (!read(integer) || integer > 1)
                return false;
            b = !!integer;
        } else {
            int32_t integer;
            if (!read(integer) || integer > 1)
                return false;
            b = !!integer;
        }
        return true;
    }

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
        d = purifyNaN(u.d);
        return true;
    }

    bool read(uint64_t& i)
    {
        return readLittleEndian(i);
    }

    std::optional<uint32_t> readStringIndex()
    {
        return readConstantPoolIndex(m_constantPool);
    }

    std::optional<uint32_t> readImageDataIndex()
    {
        return readConstantPoolIndex(m_imageDataPool);
    }

    template<typename T> std::optional<uint32_t> readConstantPoolIndex(const T& constantPool)
    {
        if (constantPool.size() <= 0xFF) {
            uint8_t i8;
            if (!read(i8))
                return std::nullopt;
            return i8;
        }
        if (constantPool.size() <= 0xFFFF) {
            uint16_t i16;
            if (!read(i16))
                return std::nullopt;
            return i16;
        }
        uint32_t i;
        if (!read(i))
            return std::nullopt;
        return i;
    }

    static bool readString(const uint8_t*& ptr, const uint8_t* end, String& str, unsigned length, bool is8Bit, ShouldAtomize shouldAtomize)
    {
        if (length >= std::numeric_limits<int32_t>::max() / sizeof(UChar))
            return false;

        if (is8Bit) {
            if ((end - ptr) < static_cast<int>(length))
                return false;
            if (shouldAtomize == ShouldAtomize::Yes)
                str = AtomString { ptr, length };
            else
                str = String { ptr, length };
            ptr += length;
            return true;
        }

        unsigned size = length * sizeof(UChar);
        if ((end - ptr) < static_cast<int>(size))
            return false;

#if ASSUME_LITTLE_ENDIAN
        if (shouldAtomize == ShouldAtomize::Yes)
            str = AtomString(reinterpret_cast<const UChar*>(ptr), length);
        else
            str = String(reinterpret_cast<const UChar*>(ptr), length);
        ptr += length * sizeof(UChar);
#else
        UChar* characters;
        str = String::createUninitialized(length, characters);
        for (unsigned i = 0; i < length; ++i) {
            uint16_t c;
            readLittleEndian(ptr, end, c);
            characters[i] = c;
        }
        if (shouldAtomize == ShouldAtomize::Yes)
            str = AtomString { str };
#endif
        return true;
    }

    bool readNullableString(String& nullableString, ShouldAtomize shouldAtomize = ShouldAtomize::No)
    {
        bool isNull;
        if (!read(isNull))
            return false;
        if (isNull)
            return true;
        CachedStringRef stringData;
        if (!readStringData(stringData, shouldAtomize))
            return false;
        nullableString = stringData->string();
        return true;
    }

    bool readStringData(CachedStringRef& cachedString, ShouldAtomize shouldAtomize = ShouldAtomize::No)
    {
        bool scratch;
        return readStringData(cachedString, scratch, shouldAtomize);
    }

    bool readStringData(CachedStringRef& cachedString, bool& wasTerminator, ShouldAtomize shouldAtomize = ShouldAtomize::No)
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
            auto index = readStringIndex();
            if (!index || *index >= m_constantPool.size()) {
                fail();
                return false;
            }
            cachedString = CachedStringRef(&m_constantPool, *index);
            return true;
        }
        bool is8Bit = length & StringDataIs8BitFlag;
        length &= ~StringDataIs8BitFlag;
        String str;
        if (!readString(m_ptr, m_end, str, length, is8Bit, shouldAtomize)) {
            fail();
            return false;
        }
        m_constantPool.append(WTFMove(str));
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
        object->putDirectIndex(m_lexicalGlobalObject, index, value);
    }

    void putProperty(JSObject* object, const Identifier& property, JSValue value)
    {
        object->putDirectMayBeIndex(m_lexicalGlobalObject, property, value);
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
        std::optional<int64_t> optionalLastModified;
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

        if (!m_canCreateDOMObject)
            return true;

        file = File::deserialize(executionContext(m_lexicalGlobalObject), filePath, URL { url->string() }, type->string(), name->string(), optionalLastModified);
        return true;
    }

    template<typename LengthType>
    bool readArrayBufferImpl(RefPtr<ArrayBuffer>& arrayBuffer)
    {
        LengthType length;
        if (!read(length))
            return false;
        if (m_ptr + length > m_end)
            return false;
        arrayBuffer = ArrayBuffer::tryCreate(m_ptr, length);
        if (!arrayBuffer)
            return false;
        m_ptr += length;
        return true;
    }

    bool readArrayBuffer(RefPtr<ArrayBuffer>& arrayBuffer)
    {
        if (m_version < 10)
            return readArrayBufferImpl<uint32_t>(arrayBuffer);
        return readArrayBufferImpl<uint64_t>(arrayBuffer);
    }

    bool readResizableNonSharedArrayBuffer(RefPtr<ArrayBuffer>& arrayBuffer)
    {
        uint64_t byteLength;
        if (!read(byteLength))
            return false;
        uint64_t maxByteLength;
        if (!read(maxByteLength))
            return false;
        if (m_ptr + byteLength > m_end)
            return false;
        arrayBuffer = ArrayBuffer::tryCreate(byteLength, 1, maxByteLength);
        if (!arrayBuffer)
            return false;
        ASSERT(arrayBuffer->isResizableNonShared());
        memcpy(arrayBuffer->data(), m_ptr, byteLength);
        m_ptr += byteLength;
        return true;
    }

    template <typename LengthType>
    bool readArrayBufferViewImpl(VM& vm, JSValue& arrayBufferView)
    {
        ArrayBufferViewSubtag arrayBufferViewSubtag;
        if (!readArrayBufferViewSubtag(arrayBufferViewSubtag))
            return false;
        LengthType byteOffset;
        if (!read(byteOffset))
            return false;
        LengthType byteLength;
        if (!read(byteLength))
            return false;
        JSValue arrayBufferValue = readTerminal();
        if (!arrayBufferValue || !arrayBufferValue.inherits<JSArrayBuffer>())
            return false;
        JSObject* arrayBufferObj = asObject(arrayBufferValue);

        unsigned elementSize = typedArrayElementSize(arrayBufferViewSubtag);
        if (!elementSize)
            return false;

        RefPtr<ArrayBuffer> arrayBuffer = toPossiblySharedArrayBuffer(vm, arrayBufferObj);
        if (!arrayBuffer) {
            arrayBufferView = jsNull();
            return true;
        }

        std::optional<size_t> length;
        if (byteLength != autoLengthMarker) {
            LengthType computedLength = byteLength / elementSize;
            if (computedLength * elementSize != byteLength)
                return false;
            length = computedLength;
        } else {
            if (!arrayBuffer->isResizableOrGrowableShared())
                return false;
        }

        auto makeArrayBufferView = [&] (auto view) -> bool {
            if (!view)
                return false;
            arrayBufferView = toJS(m_lexicalGlobalObject, m_globalObject, WTFMove(view));
            if (!arrayBufferView)
                return false;
            return true;
        };

        switch (arrayBufferViewSubtag) {
        case DataViewTag:
            return makeArrayBufferView(DataView::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case Int8ArrayTag:
            return makeArrayBufferView(Int8Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case Uint8ArrayTag:
            return makeArrayBufferView(Uint8Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case Uint8ClampedArrayTag:
            return makeArrayBufferView(Uint8ClampedArray::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case Int16ArrayTag:
            return makeArrayBufferView(Int16Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case Uint16ArrayTag:
            return makeArrayBufferView(Uint16Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case Int32ArrayTag:
            return makeArrayBufferView(Int32Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case Uint32ArrayTag:
            return makeArrayBufferView(Uint32Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case Float32ArrayTag:
            return makeArrayBufferView(Float32Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case Float64ArrayTag:
            return makeArrayBufferView(Float64Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case BigInt64ArrayTag:
            return makeArrayBufferView(BigInt64Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        case BigUint64ArrayTag:
            return makeArrayBufferView(BigUint64Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
        default:
            return false;
        }
    }

    bool readArrayBufferView(VM& vm, JSValue& arrayBufferView)
    {
        if (m_version < 10)
            return readArrayBufferViewImpl<uint32_t>(vm, arrayBufferView);
        return readArrayBufferViewImpl<uint64_t>(vm, arrayBufferView);
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

    bool read(PredefinedColorSpace& result)
    {
        uint8_t tag;
        if (!read(tag))
            return false;

        switch (static_cast<PredefinedColorSpaceTag>(tag)) {
        case PredefinedColorSpaceTag::SRGB:
            result = PredefinedColorSpace::SRGB;
            return true;
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        case PredefinedColorSpaceTag::DisplayP3:
            result = PredefinedColorSpace::DisplayP3;
            return true;
#endif
        default:
            return false;
        }
    }

    bool read(DestinationColorSpaceTag& tag)
    {
        if (m_ptr >= m_end)
            return false;
        tag = static_cast<DestinationColorSpaceTag>(*m_ptr++);
        return true;
    }

#if PLATFORM(COCOA)
    bool read(RetainPtr<CFDataRef>& data)
    {
        uint32_t dataLength;
        if (!read(dataLength) || static_cast<uint32_t>(m_end - m_ptr) < dataLength)
            return false;

        data = adoptCF(CFDataCreateWithBytesNoCopy(nullptr, m_ptr, dataLength, kCFAllocatorNull));
        if (!data)
            return false;

        m_ptr += dataLength;
        return true;
    }
#endif

    bool read(DestinationColorSpace& destinationColorSpace)
    {
        DestinationColorSpaceTag tag;
        if (!read(tag))
            return false;

        switch (tag) {
        case DestinationColorSpaceSRGBTag:
            destinationColorSpace = DestinationColorSpace::SRGB();
            return true;
#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
        case DestinationColorSpaceLinearSRGBTag:
            destinationColorSpace = DestinationColorSpace::LinearSRGB();
            return true;
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
        case DestinationColorSpaceDisplayP3Tag:
            destinationColorSpace = DestinationColorSpace::DisplayP3();
            return true;
#endif
#if PLATFORM(COCOA)
        case DestinationColorSpaceCGColorSpaceNameTag: {
            RetainPtr<CFDataRef> data;
            if (!read(data))
                return false;

            auto name = adoptCF(CFStringCreateFromExternalRepresentation(nullptr, data.get(), kCFStringEncodingUTF8));
            if (!name)
                return false;

            auto colorSpace = adoptCF(CGColorSpaceCreateWithName(name.get()));
            if (!colorSpace)
                return false;

            destinationColorSpace = DestinationColorSpace(colorSpace.get());
            return true;
        }
        case DestinationColorSpaceCGColorSpacePropertyListTag: {
            RetainPtr<CFDataRef> data;
            if (!read(data))
                return false;

            auto propertyList = adoptCF(CFPropertyListCreateWithData(nullptr, data.get(), kCFPropertyListImmutable, nullptr, nullptr));
            if (!propertyList)
                return false;

            auto colorSpace = adoptCF(CGColorSpaceCreateWithPropertyList(propertyList.get()));
            if (!colorSpace)
                return false;

            destinationColorSpace = DestinationColorSpace(colorSpace.get());
            return true;
        }
#endif
        }

        ASSERT_NOT_REACHED();
        return false;
    }

#if ENABLE(WEB_CRYPTO)
    bool read(CryptoKeyOKP::NamedCurve& result)
    {
        uint8_t nameTag;
        if (!read(nameTag))
            return false;
        if (nameTag > cryptoKeyOKPOpNameTagMaximumValue)
            return false;

        switch (static_cast<CryptoKeyOKPOpNameTag>(nameTag)) {
        case CryptoKeyOKPOpNameTag::X25519:
            result = CryptoKeyOKP::NamedCurve::X25519;
            break;
        case CryptoKeyOKPOpNameTag::ED25519:
            result = CryptoKeyOKP::NamedCurve::Ed25519;
            break;
        }

        return true;
    }

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
        case CryptoAlgorithmIdentifierTag::ED25519:
            result = CryptoAlgorithmIdentifier::Ed25519;
            break;
        case CryptoAlgorithmIdentifierTag::X25519:
            result = CryptoAlgorithmIdentifier::X25519;
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

        bool isRestrictedToHash;
        CryptoAlgorithmIdentifier hash = CryptoAlgorithmIdentifier::SHA_1;
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

    bool readOKPKey(bool extractable, CryptoKeyUsageBitmap usages, RefPtr<CryptoKey>& result)
    {
        CryptoAlgorithmIdentifier algorithm;
        if (!read(algorithm))
            return false;
        if (!CryptoKeyOKP::isValidOKPAlgorithm(algorithm))
            return false;
        CryptoKeyOKP::NamedCurve namedCurve;
        if (!read(namedCurve))
            return false;
        Vector<uint8_t> keyData;
        if (!read(keyData))
            return false;

        result = CryptoKeyOKP::importRaw(algorithm, namedCurve, WTFMove(keyData), extractable, usages);
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

        bool extractable;
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
        case CryptoKeyClassSubtag::OKP:
            if (!readOKPKey(extractable, usages, result))
                return false;
            break;
        }
        cryptoKey = getJSValue(result.get());
        return true;
    }
#endif

    bool read(SerializableErrorType& errorType)
    {
        std::underlying_type_t<SerializableErrorType> errorTypeInt;
        if (!read(errorTypeInt) || errorTypeInt > enumToUnderlyingType(SerializableErrorType::Last))
            return false;

        errorType = static_cast<SerializableErrorType>(errorTypeInt);
        return true;
    }

    template<class T>
    JSValue getJSValue(T&& nativeObj)
    {
        return toJS(m_lexicalGlobalObject, jsCast<JSDOMGlobalObject*>(m_globalObject), std::forward<T>(nativeObj));
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

        return toJSNewlyCreated(m_lexicalGlobalObject, jsCast<JSDOMGlobalObject*>(m_globalObject), T::create(x, y, z, w));
    }

    template<class T>
    JSValue readDOMMatrix()
    {
        bool is2D;
        if (!read(is2D, ForceReadingAs8Bit::Yes))
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
            return toJSNewlyCreated(m_lexicalGlobalObject, jsCast<JSDOMGlobalObject*>(m_globalObject), T::create(WTFMove(matrix), DOMMatrixReadOnly::Is2D::Yes));
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
            return toJSNewlyCreated(m_lexicalGlobalObject, jsCast<JSDOMGlobalObject*>(m_globalObject), T::create(WTFMove(matrix), DOMMatrixReadOnly::Is2D::No));
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

        return toJSNewlyCreated(m_lexicalGlobalObject, jsCast<JSDOMGlobalObject*>(m_globalObject), T::create(x, y, width, height));
    }

    std::optional<DOMPointInit> readDOMPointInit()
    {
        DOMPointInit point;
        if (!read(point.x))
            return std::nullopt;
        if (!read(point.y))
            return std::nullopt;
        if (!read(point.z))
            return std::nullopt;
        if (!read(point.w))
            return std::nullopt;

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

        return toJSNewlyCreated(m_lexicalGlobalObject, jsCast<JSDOMGlobalObject*>(m_globalObject), DOMQuad::create(p1.value(), p2.value(), p3.value(), p4.value()));
    }

    JSValue readTransferredImageBitmap()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_backingStores.size()) {
            fail();
            return JSValue();
        }

        if (!m_imageBitmaps[index]) {
            m_backingStores.at(index)->connect(*executionContext(m_lexicalGlobalObject));
            m_imageBitmaps[index] = ImageBitmap::create(WTFMove(m_backingStores.at(index)));
        }

        auto bitmap = m_imageBitmaps[index].get();
        return getJSValue(bitmap);
    }

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    JSValue readOffscreenCanvas()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_detachedOffscreenCanvases.size()) {
            fail();
            return JSValue();
        }

        if (!m_offscreenCanvases[index])
            m_offscreenCanvases[index] = OffscreenCanvas::create(*executionContext(m_lexicalGlobalObject), WTFMove(m_detachedOffscreenCanvases.at(index)));

        auto offscreenCanvas = m_offscreenCanvases[index].get();
        return getJSValue(offscreenCanvas);
    }

    JSValue readInMemoryOffscreenCanvas()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_inMemoryOffscreenCanvases.size()) {
            fail();
            return JSValue();
        }
        return getJSValue(m_inMemoryOffscreenCanvases[index].get());
    }
#endif

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
            fingerprints.append(RTCCertificate::DtlsFingerprint { algorithm->string(), value->string() });
        }

        if (!m_canCreateDOMObject)
            return constructEmptyObject(m_lexicalGlobalObject, m_globalObject->objectPrototype());

        auto rtcCertificate = RTCCertificate::create(SecurityOrigin::createFromString(origin->string()), expires, WTFMove(fingerprints), certificate->takeString(), keyedMaterial->takeString());
        return toJSNewlyCreated(m_lexicalGlobalObject, jsCast<JSDOMGlobalObject*>(m_globalObject), WTFMove(rtcCertificate));
    }

    JSValue readRTCDataChannel()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_detachedRTCDataChannels.size()) {
            fail();
            return JSValue();
        }

        if (!m_rtcDataChannels[index]) {
            auto detachedChannel = WTFMove(m_detachedRTCDataChannels.at(index));
            m_rtcDataChannels[index] = RTCDataChannel::create(*executionContext(m_lexicalGlobalObject), detachedChannel->identifier, WTFMove(detachedChannel->label), WTFMove(detachedChannel->options), detachedChannel->state);
        }

        return getJSValue(m_rtcDataChannels[index].get());
    }
#endif

#if ENABLE(WEB_CODECS)
    JSValue readWebCodecsEncodedVideoChunk()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_serializedVideoChunks.size()) {
            fail();
            return JSValue();
        }

        if (!m_videoChunks[index])
            m_videoChunks[index] = WebCodecsEncodedVideoChunk::create(m_serializedVideoChunks.at(index).releaseNonNull());

        return getJSValue(m_videoChunks[index].get());
    }
    JSValue readWebCodecsVideoFrame()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_serializedVideoFrames.size()) {
            fail();
            return JSValue();
        }

        if (!m_videoFrames[index])
            m_videoFrames[index] = WebCodecsVideoFrame::create(*executionContext(m_lexicalGlobalObject), WTFMove(m_serializedVideoFrames.at(index)));

        return getJSValue(m_videoFrames[index].get());
    }
    JSValue readWebCodecsEncodedAudioChunk()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_serializedAudioChunks.size()) {
            fail();
            return JSValue();
        }

        if (!m_audioChunks[index])
            m_audioChunks[index] = WebCodecsEncodedAudioChunk::create(m_serializedAudioChunks.at(index).releaseNonNull());

        return getJSValue(m_audioChunks[index].get());
    }
    JSValue readWebCodecsAudioData()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_serializedAudioData.size()) {
            fail();
            return JSValue();
        }

        if (!m_audioData[index])
            m_audioData[index] = WebCodecsAudioData::create(*executionContext(m_lexicalGlobalObject), WTFMove(m_serializedAudioData.at(index)));

        return getJSValue(m_audioData[index].get());
    }
#endif

    JSValue readImageBitmap()
    {
        uint8_t serializationState;
        int32_t logicalWidth;
        int32_t logicalHeight;
        double resolutionScale;
        auto colorSpace = DestinationColorSpace::SRGB();
        RefPtr<ArrayBuffer> arrayBuffer;

        if (!read(serializationState) || !read(logicalWidth) || !read(logicalHeight) || !read(resolutionScale) || (m_version > 8 && !read(colorSpace)) || !readArrayBufferImpl<uint32_t>(arrayBuffer)) {
            fail();
            return JSValue();
        }

        auto logicalSize = IntSize(logicalWidth, logicalHeight);
        auto imageDataSize = logicalSize;
        imageDataSize.scale(resolutionScale);

        auto buffer = ImageBitmap::createImageBuffer(*executionContext(m_lexicalGlobalObject), logicalSize, RenderingMode::Unaccelerated, colorSpace, resolutionScale);
        if (!buffer) {
            fail();
            return JSValue();
        }

        PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, colorSpace };
        auto pixelBuffer = ByteArrayPixelBuffer::tryCreate(format, imageDataSize, arrayBuffer.releaseNonNull());
        if (!pixelBuffer) {
            fail();
            return JSValue();
        }

        buffer->putPixelBuffer(*pixelBuffer, { IntPoint::zero(), logicalSize });

        auto bitmap = ImageBitmap::create(ImageBitmapBacking(WTFMove(buffer), OptionSet<SerializationState>::fromRaw(serializationState)));
        return getJSValue(bitmap);
    }

    JSValue readDOMException()
    {
        CachedStringRef message;
        if (!readStringData(message))
            return JSValue();
        CachedStringRef name;
        if (!readStringData(name))
            return JSValue();
        auto exception = DOMException::create(message->string(), name->string());
        return getJSValue(exception);
    }

    JSValue readBigInt()
    {
        bool sign;
        if (!read(sign, ForceReadingAs8Bit::Yes))
            return JSValue();
        uint32_t numberOfUint64Elements = 0;
        if (!read(numberOfUint64Elements))
            return JSValue();

        if (!numberOfUint64Elements) {
#if USE(BIGINT32)
            return jsBigInt32(0);
#else
            JSBigInt* bigInt = JSBigInt::tryCreateZero(m_lexicalGlobalObject->vm());
            if (UNLIKELY(!bigInt)) {
                fail();
                return JSValue();
            }
            m_gcBuffer.appendWithCrashOnOverflow(bigInt);
            return bigInt;
#endif
        }

#if USE(BIGINT32)
        static_assert(sizeof(JSBigInt::Digit) == sizeof(uint64_t));
        if (numberOfUint64Elements == 1) {
            uint64_t digit64 = 0;
            if (!read(digit64))
                return JSValue();
            if (sign) {
                if (digit64 <= static_cast<uint64_t>(-static_cast<int64_t>(INT32_MIN)))
                    return jsBigInt32(static_cast<int32_t>(-static_cast<int64_t>(digit64)));
            } else {
                if (digit64 <= INT32_MAX)
                    return jsBigInt32(static_cast<int32_t>(digit64));
            }
            ASSERT(digit64 != 0);
            JSBigInt* bigInt = JSBigInt::tryCreateWithLength(m_lexicalGlobalObject->vm(), 1);
            if (!bigInt) {
                fail();
                return JSValue();
            }
            bigInt->setDigit(0, digit64);
            bigInt->setSign(sign);
            bigInt = bigInt->tryRightTrim(m_lexicalGlobalObject->vm());
            if (!bigInt) {
                fail();
                return JSValue();
            }
            m_gcBuffer.appendWithCrashOnOverflow(bigInt);
            return tryConvertToBigInt32(bigInt);
        }
#endif
        JSBigInt* bigInt = nullptr;
        if constexpr (sizeof(JSBigInt::Digit) == sizeof(uint64_t)) {
            bigInt = JSBigInt::tryCreateWithLength(m_lexicalGlobalObject->vm(), numberOfUint64Elements);
            if (!bigInt) {
                fail();
                return JSValue();
            }
            for (uint32_t index = 0; index < numberOfUint64Elements; ++index) {
                uint64_t digit64 = 0;
                if (!read(digit64))
                    return JSValue();
                bigInt->setDigit(index, digit64);
            }
        } else {
            ASSERT(sizeof(JSBigInt::Digit) == sizeof(uint32_t));
            auto actualBigIntLength = WTF::checkedProduct<uint32_t>(numberOfUint64Elements, 2);
            if (actualBigIntLength.hasOverflowed()) {
                fail();
                return JSValue();
            }
            bigInt = JSBigInt::tryCreateWithLength(m_lexicalGlobalObject->vm(), actualBigIntLength.value());
            if (!bigInt) {
                fail();
                return JSValue();
            }
            for (uint32_t index = 0; index < numberOfUint64Elements; ++index) {
                uint64_t digit64 = 0;
                if (!read(digit64))
                    return JSValue();
                bigInt->setDigit(index * 2, static_cast<uint32_t>(digit64));
                bigInt->setDigit(index * 2 + 1, static_cast<uint32_t>(digit64 >> 32));
            }
        }
        bigInt->setSign(sign);
        bigInt = bigInt->tryRightTrim(m_lexicalGlobalObject->vm());
        if (!bigInt) {
            fail();
            return JSValue();
        }
        m_gcBuffer.appendWithCrashOnOverflow(bigInt);
        return tryConvertToBigInt32(bigInt);
    }

    JSValue readTerminal()
    {
        SerializationTag tag = readTag();
        if (!isTypeExposedToGlobalObject(*m_globalObject, tag)) {
            fail();
            return JSValue();
        }
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
            BooleanObject* obj = BooleanObject::create(m_lexicalGlobalObject->vm(), m_globalObject->booleanObjectStructure());
            obj->setInternalValue(m_lexicalGlobalObject->vm(), jsBoolean(false));
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case TrueObjectTag: {
            BooleanObject* obj = BooleanObject::create(m_lexicalGlobalObject->vm(), m_globalObject->booleanObjectStructure());
            obj->setInternalValue(m_lexicalGlobalObject->vm(), jsBoolean(true));
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case DoubleTag: {
            double d;
            if (!read(d))
                return JSValue();
            return jsNumber(d);
        }
        case BigIntTag:
            return readBigInt();
        case NumberObjectTag: {
            double d;
            if (!read(d))
                return JSValue();
            NumberObject* obj = constructNumber(m_globalObject, jsNumber(d));
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case BigIntObjectTag: {
            JSValue bigInt = readBigInt();
            if (!bigInt)
                return JSValue();
            ASSERT(bigInt.isBigInt());
            BigIntObject* obj = BigIntObject::create(m_lexicalGlobalObject->vm(), m_globalObject, bigInt);
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case DateTag: {
            double d;
            if (!read(d))
                return JSValue();
            return DateInstance::create(m_lexicalGlobalObject->vm(), m_globalObject->dateStructure(), d);
        }
        case FileTag: {
            RefPtr<File> file;
            if (!readFile(file))
                return JSValue();
            if (!m_canCreateDOMObject)
                return jsNull();
            return toJS(m_lexicalGlobalObject, jsCast<JSDOMGlobalObject*>(m_globalObject), file.get());
        }
        case FileListTag: {
            unsigned length = 0;
            if (!read(length))
                return JSValue();
            ASSERT(m_globalObject->inherits<JSDOMGlobalObject>());
            Vector<Ref<File>> files;
            for (unsigned i = 0; i < length; i++) {
                RefPtr<File> file;
                if (!readFile(file))
                    return JSValue();
                if (m_canCreateDOMObject)
                    files.append(file.releaseNonNull());
            }
            if (!m_canCreateDOMObject)
                return jsNull();
            return getJSValue(FileList::create(WTFMove(files)).get());
        }
        case ImageDataTag: {
            uint32_t width;
            if (!read(width))
                return JSValue();
            if (width == ImageDataPoolTag) {
                auto index = readImageDataIndex();
                if (!index || *index >= m_imageDataPool.size()) {
                    fail();
                    return JSValue();
                }
                return getJSValue(m_imageDataPool[*index]);
            }
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
            auto bufferStart = m_ptr;
            m_ptr += length;

            auto resultColorSpace = PredefinedColorSpace::SRGB;
            if (m_version > 7) {
                if (!read(resultColorSpace))
                    return JSValue();
            }

            if (length && (IntSize(width, height).area() * 4) != length) {
                fail();
                return JSValue();
            }

            if (!m_isDOMGlobalObject)
                return jsNull();

            auto result = ImageData::createUninitialized(width, height, resultColorSpace);
            if (result.hasException()) {
                fail();
                return JSValue();
            }
            if (length)
                memcpy(result.returnValue()->data().data(), bufferStart, length);
            else
                result.returnValue()->data().zeroFill();
            m_imageDataPool.append(result.returnValue().copyRef());
            return getJSValue(result.releaseReturnValue());
        }
        case BlobTag: {
            CachedStringRef url;
            if (!readStringData(url))
                return JSValue();
            CachedStringRef type;
            if (!readStringData(type))
                return JSValue();
            uint64_t size = 0;
            if (!read(size))
                return JSValue();
            uint64_t memoryCost = 0;
            if (m_version >= 11 && !read(memoryCost))
                return JSValue();
            if (!m_canCreateDOMObject)
                return jsNull();
            return getJSValue(Blob::deserialize(executionContext(m_lexicalGlobalObject), URL { url->string() }, type->string(), size, memoryCost, blobFilePathForBlobURL(url->string())).get());
        }
        case StringTag: {
            CachedStringRef cachedString;
            if (!readStringData(cachedString))
                return JSValue();
            return cachedString->jsString(m_lexicalGlobalObject);
        }
        case EmptyStringTag:
            return jsEmptyString(m_lexicalGlobalObject->vm());
        case StringObjectTag: {
            CachedStringRef cachedString;
            if (!readStringData(cachedString))
                return JSValue();
            StringObject* obj = constructString(m_lexicalGlobalObject->vm(), m_globalObject, cachedString->jsString(m_lexicalGlobalObject));
            m_gcBuffer.appendWithCrashOnOverflow(obj);
            return obj;
        }
        case EmptyStringObjectTag: {
            VM& vm = m_lexicalGlobalObject->vm();
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
            ASSERT(reFlags.has_value());
            VM& vm = m_lexicalGlobalObject->vm();
            RegExp* regExp = RegExp::create(vm, pattern->string(), reFlags.value());
            return RegExpObject::create(vm, m_globalObject->regExpStructure(), regExp);
        }
        case ErrorInstanceTag: {
            SerializableErrorType serializedErrorType;
            if (!read(serializedErrorType)) {
                fail();
                return JSValue();
            }
            String message;
            if (!readNullableString(message)) {
                fail();
                return JSValue();
            }
            uint32_t line;
            if (!read(line)) {
                fail();
                return JSValue();
            }
            uint32_t column;
            if (!read(column)) {
                fail();
                return JSValue();
            }
            String sourceURL;
            if (!readNullableString(sourceURL)) {
                fail();
                return JSValue();
            }
            String stackString;
            if (!readNullableString(stackString)) {
                fail();
                return JSValue();
            }
            return ErrorInstance::create(m_lexicalGlobalObject, WTFMove(message), toErrorType(serializedErrorType), line, column, WTFMove(sourceURL), WTFMove(stackString));
        }
        case ObjectReferenceTag: {
            auto index = readConstantPoolIndex(m_gcBuffer);
            if (!index) {
                fail();
                return JSValue();
            }
            return m_gcBuffer.at(*index);
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
        case InMemoryMessagePortTag: {
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || index >= m_inMemoryMessagePorts.size()) {
                fail();
                return JSValue();
            }
            return getJSValue(m_inMemoryMessagePorts[index].get());
        }
#if ENABLE(WEBASSEMBLY)
        case WasmModuleTag: {
            if (m_version >= 12) {
                // https://webassembly.github.io/spec/web-api/index.html#serialization
                CachedStringRef agentClusterID;
                bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
                if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != agentClusterIDFromGlobalObject(*m_globalObject)) {
                    fail();
                    return JSValue();
                }
            }
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_wasmModules || index >= m_wasmModules->size()) {
                fail();
                return JSValue();
            }
            auto scope = DECLARE_THROW_SCOPE(m_lexicalGlobalObject->vm());
            JSValue result = JSC::JSWebAssemblyModule::createStub(m_lexicalGlobalObject->vm(), m_lexicalGlobalObject, m_globalObject->webAssemblyModuleStructure(), m_wasmModules->at(index));
            // Since we are cloning a JSWebAssemblyModule, it's impossible for that
            // module to not have been a valid module. Therefore, createStub should
            // not throw.
            scope.releaseAssertNoException();
            m_gcBuffer.appendWithCrashOnOverflow(result);
            return result;
        }
        case WasmMemoryTag: {
            if (m_version >= 12) {
                CachedStringRef agentClusterID;
                bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
                if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != agentClusterIDFromGlobalObject(*m_globalObject)) {
                    fail();
                    return JSValue();
                }
            }
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_wasmMemoryHandles || index >= m_wasmMemoryHandles->size() || !JSC::Options::useSharedArrayBuffer()) {
                fail();
                return JSValue();
            }

            auto& vm = m_lexicalGlobalObject->vm();
            auto scope = DECLARE_THROW_SCOPE(vm);
            JSWebAssemblyMemory* result = JSC::JSWebAssemblyMemory::tryCreate(m_lexicalGlobalObject, vm, m_globalObject->webAssemblyMemoryStructure());
            // Since we are cloning a JSWebAssemblyMemory, it's impossible for that
            // module to not have been a valid module. Therefore, createStub should
            // not throw.
            scope.releaseAssertNoException();

            RefPtr<Wasm::Memory> memory;
            auto handler = [&vm, result] (Wasm::Memory::GrowSuccess, PageCount oldPageCount, PageCount newPageCount) { result->growSuccessCallback(vm, oldPageCount, newPageCount); };
            if (RefPtr<SharedArrayBufferContents> contents = m_wasmMemoryHandles->at(index)) {
                if (!contents->memoryHandle()) {
                    fail();
                    return JSValue();
                }
                memory = Wasm::Memory::create(contents.releaseNonNull(), WTFMove(handler));
            } else {
                // zero size & max-size.
                memory = Wasm::Memory::createZeroSized(JSC::MemorySharingMode::Shared, WTFMove(handler));
            }

            result->adopt(memory.releaseNonNull());
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
            if (UNLIKELY(!structure)) {
                fail();
                return JSValue();
            }
            JSValue result = JSArrayBuffer::create(m_lexicalGlobalObject->vm(), structure, WTFMove(arrayBuffer));
            m_gcBuffer.appendWithCrashOnOverflow(result);
            return result;
        }
        case ResizableArrayBufferTag: {
            RefPtr<ArrayBuffer> arrayBuffer;
            if (!readResizableNonSharedArrayBuffer(arrayBuffer)) {
                fail();
                return JSValue();
            }
            Structure* structure = m_globalObject->arrayBufferStructure(arrayBuffer->sharingMode());
            // A crazy RuntimeFlags mismatch could mean that we are not equipped to handle shared
            // array buffers while the sender is. In that case, we would see a null structure here.
            if (UNLIKELY(!structure)) {
                fail();
                return JSValue();
            }
            JSValue result = JSArrayBuffer::create(m_lexicalGlobalObject->vm(), structure, WTFMove(arrayBuffer));
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
            // https://html.spec.whatwg.org/multipage/structured-data.html#structureddeserialize
            uint32_t index = UINT_MAX;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_sharedBuffers || index >= m_sharedBuffers->size() || !JSC::Options::useSharedArrayBuffer()) {
                fail();
                return JSValue();
            }
            
            RELEASE_ASSERT(m_sharedBuffers->at(index));
            ArrayBufferContents arrayBufferContents;
            m_sharedBuffers->at(index).shareWith(arrayBufferContents);
            auto buffer = ArrayBuffer::create(WTFMove(arrayBufferContents));
            JSValue result = getJSValue(buffer.get());
            m_gcBuffer.appendWithCrashOnOverflow(result);
            return result;
        }
        case ArrayBufferViewTag: {
            JSValue arrayBufferView;
            if (!readArrayBufferView(m_lexicalGlobalObject->vm(), arrayBufferView)) {
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
            if (!unwrapCryptoKey(m_lexicalGlobalObject, wrappedKey, serializedKey)) {
                fail();
                return JSValue();
            }
            JSValue cryptoKey;
            Vector<RefPtr<MessagePort>> dummyMessagePorts;
            CloneDeserializer rawKeyDeserializer(m_lexicalGlobalObject, m_globalObject, dummyMessagePorts, nullptr, { }, serializedKey);
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
            return readTransferredImageBitmap();
#if ENABLE(WEB_RTC)
        case RTCCertificateTag:
            return readRTCCertificate();

#endif
        case ImageBitmapTag:
            return readImageBitmap();
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        case OffscreenCanvasTransferTag:
            return readOffscreenCanvas();
        case InMemoryOffscreenCanvasTag:
            return readInMemoryOffscreenCanvas();
#endif
#if ENABLE(WEB_RTC)
        case RTCDataChannelTransferTag:
            return readRTCDataChannel();
#endif
#if ENABLE(WEB_CODECS)
        case WebCodecsEncodedVideoChunkTag:
            return readWebCodecsEncodedVideoChunk();
        case WebCodecsVideoFrameTag:
            return readWebCodecsVideoFrame();
        case WebCodecsEncodedAudioChunkTag:
            return readWebCodecsEncodedAudioChunk();
        case WebCodecsAudioDataTag:
            return readWebCodecsAudioData();
#endif
        case DOMExceptionTag:
            return readDOMException();

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

    JSGlobalObject* const m_globalObject;
    const bool m_isDOMGlobalObject;
    const bool m_canCreateDOMObject;
    const uint8_t* m_ptr;
    const uint8_t* const m_end;
    unsigned m_version;
    Vector<CachedString> m_constantPool;
    Vector<Ref<ImageData>> m_imageDataPool;
    const Vector<RefPtr<MessagePort>>& m_messagePorts;
    ArrayBufferContentsArray* m_arrayBufferContents;
    Vector<RefPtr<JSC::ArrayBuffer>> m_arrayBuffers;
    Vector<String> m_blobURLs;
    Vector<String> m_blobFilePaths;
    ArrayBufferContentsArray* m_sharedBuffers;
    Vector<std::optional<ImageBitmapBacking>> m_backingStores;
    Vector<RefPtr<ImageBitmap>> m_imageBitmaps;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<std::unique_ptr<DetachedOffscreenCanvas>> m_detachedOffscreenCanvases;
    Vector<RefPtr<OffscreenCanvas>> m_offscreenCanvases;
    const Vector<RefPtr<OffscreenCanvas>>& m_inMemoryOffscreenCanvases;
#endif
    const Vector<RefPtr<MessagePort>>& m_inMemoryMessagePorts;
#if ENABLE(WEB_RTC)
    Vector<std::unique_ptr<DetachedRTCDataChannel>> m_detachedRTCDataChannels;
    Vector<RefPtr<RTCDataChannel>> m_rtcDataChannels;
#endif
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray* const m_wasmModules;
    WasmMemoryHandleArray* const m_wasmMemoryHandles;
#endif
#if ENABLE(WEB_CODECS)
    Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>> m_serializedVideoChunks;
    Vector<RefPtr<WebCodecsEncodedVideoChunk>> m_videoChunks;
    Vector<WebCodecsVideoFrameData> m_serializedVideoFrames;
    Vector<RefPtr<WebCodecsVideoFrame>> m_videoFrames;
    Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>> m_serializedAudioChunks;
    Vector<RefPtr<WebCodecsEncodedAudioChunk>> m_audioChunks;
    Vector<WebCodecsAudioInternalData> m_serializedAudioData;
    Vector<RefPtr<WebCodecsAudioData>> m_audioData;
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
    VM& vm = m_lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<uint32_t, 16> indexStack;
    Vector<Identifier, 16> propertyNameStack;
    MarkedVector<JSObject*, 32> outputObjectStack;
    MarkedVector<JSValue, 4> mapKeyStack;
    MarkedVector<JSMap*, 4> mapStack;
    MarkedVector<JSSet*, 4> setStack;
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
            JSArray* outArray = constructEmptyArray(m_globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), length);
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

            if (m_version >= 15) {
                if (index == TerminatorTag) {
                    // We reached the end of the indexed properties section.
                    if (!read(index)) {
                        fail();
                        goto error;
                    }
                    // At this point, we're either done with the array or is starting the
                    // non-indexed property section.
                    if (index == TerminatorTag) {
                        JSObject* outArray = outputObjectStack.last();
                        outValue = outArray;
                        outputObjectStack.removeLast();
                        break;
                    }
                    if (index == NonIndexPropertiesTag)
                        goto objectStartVisitMember;
                }
            } else {
                if (index == TerminatorTag) {
                    JSObject* outArray = outputObjectStack.last();
                    outValue = outArray;
                    outputObjectStack.removeLast();
                    break;
                } else if (index == NonIndexPropertiesTag)
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
            JSObject* outObject = constructEmptyObject(m_lexicalGlobalObject, m_globalObject->objectPrototype());
            m_gcBuffer.appendWithCrashOnOverflow(outObject);
            outputObjectStack.append(outObject);
        }
        objectStartVisitMember:
        FALLTHROUGH;
        case ObjectStartVisitMember: {
            CachedStringRef cachedString;
            bool wasTerminator = false;
            if (!readStringData(cachedString, wasTerminator, ShouldAtomize::Yes)) {
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
            JSMap* map = JSMap::create(m_lexicalGlobalObject->vm(), m_globalObject->mapStructure());
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
            mapStack.last()->set(m_lexicalGlobalObject, mapKeyStack.last(), outValue);
            mapKeyStack.removeLast();
            goto mapDataStartVisitEntry;
        }

        setObjectStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion)
                return std::make_pair(JSValue(), SerializationReturnCode::StackOverflowError);
            JSSet* set = JSSet::create(m_lexicalGlobalObject->vm(), m_globalObject->setStructure());
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
            set->add(m_lexicalGlobalObject, outValue);
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

SerializedScriptValue::SerializedScriptValue(Vector<uint8_t>&& buffer, std::unique_ptr<ArrayBufferContentsArray>&& arrayBufferContentsArray
#if ENABLE(WEB_RTC)
    , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
#endif
#if ENABLE(WEB_CODECS)
    , Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks
    , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames
    , Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks
    , Vector<WebCodecsAudioInternalData>&& serializedAudioData
#endif
        )
    : m_internals {
        .data = WTFMove(buffer)
        , .arrayBufferContentsArray = WTFMove(arrayBufferContentsArray)
#if ENABLE(WEB_RTC)
        , .detachedRTCDataChannels = WTFMove(detachedRTCDataChannels)
#endif
#if ENABLE(WEB_CODECS)
        , .serializedVideoChunks = WTFMove(serializedVideoChunks)
        , .serializedAudioChunks = WTFMove(serializedAudioChunks)
        , .serializedVideoFrames = WTFMove(serializedVideoFrames)
        , .serializedAudioData = WTFMove(serializedAudioData)
#endif
    }
{
    m_internals.memoryCost = computeMemoryCost();
}

SerializedScriptValue::SerializedScriptValue(Vector<uint8_t>&& buffer, Vector<URLKeepingBlobAlive>&& blobHandles, std::unique_ptr<ArrayBufferContentsArray> arrayBufferContentsArray, std::unique_ptr<ArrayBufferContentsArray> sharedBufferContentsArray, Vector<std::optional<ImageBitmapBacking>>&& backingStores
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases
        , Vector<RefPtr<OffscreenCanvas>>&& inMemoryOffscreenCanvases
#endif
        , Vector<RefPtr<MessagePort>>&& inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
#endif
#if ENABLE(WEBASSEMBLY)
        , std::unique_ptr<WasmModuleArray> wasmModulesArray
        , std::unique_ptr<WasmMemoryHandleArray> wasmMemoryHandlesArray
#endif
#if ENABLE(WEB_CODECS)
        , Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks
        , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames
        , Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks
        , Vector<WebCodecsAudioInternalData>&& serializedAudioData
#endif
        )
    : m_internals {
        .data = WTFMove(buffer)
        , .arrayBufferContentsArray = WTFMove(arrayBufferContentsArray)
#if ENABLE(WEB_RTC)
        , .detachedRTCDataChannels = WTFMove(detachedRTCDataChannels)
#endif
#if ENABLE(WEB_CODECS)
        , .serializedVideoChunks = WTFMove(serializedVideoChunks)
        , .serializedAudioChunks = WTFMove(serializedAudioChunks)
        , .serializedVideoFrames = WTFMove(serializedVideoFrames)
        , .serializedAudioData = WTFMove(serializedAudioData)
#endif
        , .sharedBufferContentsArray = WTFMove(sharedBufferContentsArray)
        , .backingStores = WTFMove(backingStores)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , .detachedOffscreenCanvases = WTFMove(detachedOffscreenCanvases)
        , .inMemoryOffscreenCanvases = WTFMove(inMemoryOffscreenCanvases)
#endif
        , .inMemoryMessagePorts = WTFMove(inMemoryMessagePorts)
#if ENABLE(WEBASSEMBLY)
        , .wasmModulesArray = WTFMove(wasmModulesArray)
        , .wasmMemoryHandlesArray = WTFMove(wasmMemoryHandlesArray)
#endif
        , .blobHandles = crossThreadCopy(WTFMove(blobHandles))
    }
{
    m_internals.memoryCost = computeMemoryCost();
}

SerializedScriptValue::SerializedScriptValue(Internals&& internals)
    : m_internals(WTFMove(internals))
{
}

size_t SerializedScriptValue::computeMemoryCost() const
{
    size_t cost = m_internals.data.size();

    if (m_internals.arrayBufferContentsArray) {
        for (auto& content : *m_internals.arrayBufferContentsArray)
            cost += content.sizeInBytes();
    }

    if (m_internals.sharedBufferContentsArray) {
        for (auto& content : *m_internals.sharedBufferContentsArray)
            cost += content.sizeInBytes();
    }

    for (auto& backingStore : m_internals.backingStores) {
        if (auto buffer = backingStore ? backingStore->buffer() : nullptr)
            cost += buffer->memoryCost();
    }

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    for (auto& canvas : m_internals.detachedOffscreenCanvases) {
        if (canvas)
            cost += canvas->memoryCost();
    }
#endif
#if ENABLE(WEB_RTC)
    for (auto& channel : m_internals.detachedRTCDataChannels) {
        if (channel)
            cost += channel->memoryCost();
    }
#endif
#if ENABLE(WEBASSEMBLY)
    // We are not supporting WebAssembly Module memory estimation yet.
    if (m_internals.wasmMemoryHandlesArray) {
        for (auto& content : *m_internals.wasmMemoryHandlesArray)
            cost += content->sizeInBytes(std::memory_order_relaxed);
    }
#endif
#if ENABLE(WEB_CODECS)
    for (auto& chunk : m_internals.serializedVideoChunks) {
        if (chunk)
            cost += chunk->memoryCost();
    }
    for (auto& frame : m_internals.serializedVideoFrames)
        cost += frame.memoryCost();
    for (auto& chunk : m_internals.serializedAudioChunks) {
        if (chunk)
            cost += chunk->memoryCost();
    }
    for (auto& data : m_internals.serializedAudioData)
        cost += data.memoryCost();
#endif

    for (auto& handle : m_internals.blobHandles)
        cost += handle.url().string().sizeInBytes();

    return cost;
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
            return Exception { ExceptionCode::TypeError };
    }

    return contents;
}

static void maybeThrowExceptionIfSerializationFailed(JSGlobalObject& lexicalGlobalObject, SerializationReturnCode code)
{
    auto& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (code) {
    case SerializationReturnCode::SuccessfullyCompleted:
        break;
    case SerializationReturnCode::StackOverflowError:
        throwException(&lexicalGlobalObject, scope, createStackOverflowError(&lexicalGlobalObject));
        break;
    case SerializationReturnCode::ValidationError:
        throwTypeError(&lexicalGlobalObject, scope, "Unable to deserialize data."_s);
        break;
    case SerializationReturnCode::DataCloneError:
        throwDataCloneError(lexicalGlobalObject, scope);
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
        return Exception { ExceptionCode::StackOverflowError };
    case SerializationReturnCode::ValidationError:
        return Exception { ExceptionCode::TypeError };
    case SerializationReturnCode::DataCloneError:
        return Exception { ExceptionCode::DataCloneError };
    case SerializationReturnCode::ExistingExceptionError:
        return Exception { ExceptionCode::ExistingExceptionError };
    case SerializationReturnCode::UnspecifiedError:
        return Exception { ExceptionCode::TypeError };
    case SerializationReturnCode::SuccessfullyCompleted:
    case SerializationReturnCode::InterruptedExecutionError:
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::TypeError };
    }
    ASSERT_NOT_REACHED();
    return Exception { ExceptionCode::TypeError };
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

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
static bool canOffscreenCanvasesDetach(const Vector<RefPtr<OffscreenCanvas>>& offscreenCanvases)
{
    HashSet<OffscreenCanvas*> visited;
    for (auto& offscreenCanvas : offscreenCanvases) {
        if (!offscreenCanvas->canDetach())
            return false;
        // Check the return value of add, we should not encounter duplicates.
        if (!visited.add(offscreenCanvas.get()))
            return false;
    }
    return true;
}
#endif

#if ENABLE(WEB_RTC)
static bool canDetachRTCDataChannels(const Vector<Ref<RTCDataChannel>>& channels)
{
    HashSet<RTCDataChannel*> visited;
    for (auto& channel : channels) {
        if (!channel->canDetach())
            return false;
        // Check the return value of add, we should not encounter duplicates.
        if (!visited.add(channel.ptr()))
            return false;
    }
    return true;
}
#endif

RefPtr<SerializedScriptValue> SerializedScriptValue::create(JSC::JSGlobalObject& globalObject, JSC::JSValue value, SerializationForStorage forStorage, SerializationErrorMode throwExceptions, SerializationContext serializationContext)
{
    Vector<RefPtr<MessagePort>> dummyPorts;
    auto result = create(globalObject, value, { }, dummyPorts, forStorage, throwExceptions, serializationContext);
    if (result.hasException())
        return nullptr;
    return result.releaseReturnValue();
}

ExceptionOr<Ref<SerializedScriptValue>> SerializedScriptValue::create(JSGlobalObject& globalObject, JSValue value, Vector<JSC::Strong<JSC::JSObject>>&& transferList, Vector<RefPtr<MessagePort>>& messagePorts, SerializationForStorage forStorage, SerializationContext serializationContext)
{
    return create(globalObject, value, WTFMove(transferList), messagePorts, forStorage, SerializationErrorMode::NonThrowing, serializationContext);
}

ExceptionOr<Ref<SerializedScriptValue>> SerializedScriptValue::create(JSGlobalObject& lexicalGlobalObject, JSValue value, Vector<JSC::Strong<JSC::JSObject>>&& transferList, Vector<RefPtr<MessagePort>>& messagePorts, SerializationForStorage forStorage, SerializationErrorMode throwExceptions, SerializationContext context)
{
    VM& vm = lexicalGlobalObject.vm();
    Vector<RefPtr<JSC::ArrayBuffer>> arrayBuffers;
    Vector<RefPtr<ImageBitmap>> imageBitmaps;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<RefPtr<OffscreenCanvas>> offscreenCanvases;
#endif
#if ENABLE(WEB_RTC)
    Vector<Ref<RTCDataChannel>> dataChannels;
#endif
#if ENABLE(WEB_CODECS)
    Vector<Ref<WebCodecsVideoFrame>> transferredVideoFrames;
    Vector<Ref<WebCodecsAudioData>> transferredAudioData;
#endif
    HashSet<JSC::JSObject*> uniqueTransferables;
    for (auto& transferable : transferList) {
        if (!uniqueTransferables.add(transferable.get()).isNewEntry)
            return Exception { ExceptionCode::DataCloneError, "Duplicate transferable for structured clone"_s };

        if (auto arrayBuffer = toPossiblySharedArrayBuffer(vm, transferable.get())) {
            if (arrayBuffer->isDetached() || arrayBuffer->isShared())
                return Exception { ExceptionCode::DataCloneError };
            if (!arrayBuffer->isDetachable()) {
                auto scope = DECLARE_THROW_SCOPE(vm);
                throwVMTypeError(&lexicalGlobalObject, scope, errorMessageForTransfer(arrayBuffer));
                return Exception { ExceptionCode::ExistingExceptionError };
            }
            arrayBuffers.append(WTFMove(arrayBuffer));
            continue;
        }
        if (auto port = JSMessagePort::toWrapped(vm, transferable.get())) {
            if (port->isDetached())
                return Exception { ExceptionCode::DataCloneError, "MessagePort is detached"_s };
            messagePorts.append(WTFMove(port));
            continue;
        }

        if (auto imageBitmap = JSImageBitmap::toWrapped(vm, transferable.get())) {
            if (imageBitmap->isDetached())
                return Exception { ExceptionCode::DataCloneError };
            if (!imageBitmap->originClean())
                return Exception { ExceptionCode::DataCloneError };

            imageBitmaps.append(WTFMove(imageBitmap));
            continue;
        }

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        if (auto offscreenCanvas = JSOffscreenCanvas::toWrapped(vm, transferable.get())) {
            offscreenCanvases.append(WTFMove(offscreenCanvas));
            continue;
        }
#endif

#if ENABLE(WEB_RTC)
        if (auto channel = JSRTCDataChannel::toWrapped(vm, transferable.get())) {
            dataChannels.append(*channel);
            continue;
        }
#endif

#if ENABLE(WEB_CODECS)
        if (auto videoFrame = JSWebCodecsVideoFrame::toWrapped(vm, transferable.get())) {
            if (videoFrame->isDetached())
                return Exception { ExceptionCode::DataCloneError };
            transferredVideoFrames.append(*videoFrame);
            continue;
        }
        if (auto audioData = JSWebCodecsAudioData::toWrapped(vm, transferable.get())) {
            if (audioData->isDetached())
                return Exception { ExceptionCode::DataCloneError };
            transferredAudioData.append(*audioData);
            continue;
        }
#endif
        return Exception { ExceptionCode::DataCloneError };
    }

    if (containsDuplicates(imageBitmaps))
        return Exception { ExceptionCode::DataCloneError };
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    if (!canOffscreenCanvasesDetach(offscreenCanvases))
        return Exception { ExceptionCode::InvalidStateError };
#endif
#if ENABLE(WEB_RTC)
    if (!canDetachRTCDataChannels(dataChannels))
        return Exception { ExceptionCode::DataCloneError };
#endif

    Vector<uint8_t> buffer;
    Vector<URLKeepingBlobAlive> blobHandles;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<RefPtr<OffscreenCanvas>> inMemoryOffscreenCanvases;
#endif
    Vector<RefPtr<MessagePort>> inMemoryMessagePorts;
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray wasmModules;
    WasmMemoryHandleArray wasmMemoryHandles;
#endif
    std::unique_ptr<ArrayBufferContentsArray> sharedBuffers = makeUnique<ArrayBufferContentsArray>();
#if ENABLE(WEB_CODECS)
    Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>> serializedVideoChunks;
    Vector<RefPtr<WebCodecsVideoFrame>> serializedVideoFrames;
    Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>> serializedAudioChunks;
    Vector<RefPtr<WebCodecsAudioData>> serializedAudioData;
#endif
    auto code = CloneSerializer::serialize(&lexicalGlobalObject, value, messagePorts, arrayBuffers, imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        offscreenCanvases,
        inMemoryOffscreenCanvases,
#endif
        inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
        dataChannels,
#endif
#if ENABLE(WEB_CODECS)
        serializedVideoChunks,
        serializedVideoFrames,
        serializedAudioChunks,
        serializedAudioData,
#endif
#if ENABLE(WEBASSEMBLY)
        wasmModules,
        wasmMemoryHandles,
#endif
        blobHandles, buffer, context, *sharedBuffers, forStorage);

    if (throwExceptions == SerializationErrorMode::Throwing)
        maybeThrowExceptionIfSerializationFailed(lexicalGlobalObject, code);

    if (code != SerializationReturnCode::SuccessfullyCompleted)
        return exceptionForSerializationFailure(code);

    auto arrayBufferContentsArray = transferArrayBuffers(vm, arrayBuffers);
    if (arrayBufferContentsArray.hasException())
        return arrayBufferContentsArray.releaseException();

    auto detachedImageBitmaps = map(WTFMove(imageBitmaps), [](auto&& imageBitmap) -> std::optional<ImageBitmapBacking> {
        return imageBitmap->detach();
    });

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<std::unique_ptr<DetachedOffscreenCanvas>> detachedCanvases;
    for (auto offscreenCanvas : offscreenCanvases)
        detachedCanvases.append(offscreenCanvas->detach());
#endif
#if ENABLE(WEB_RTC)
    Vector<std::unique_ptr<DetachedRTCDataChannel>> detachedRTCDataChannels;
    for (auto& channel : dataChannels)
        detachedRTCDataChannels.append(channel->detach());
#endif

#if ENABLE(WEB_CODECS)
    auto serializedVideoFrameData = map(serializedVideoFrames, [](auto& frame) -> WebCodecsVideoFrameData { return frame->data(); });
    for (auto& videoFrame : transferredVideoFrames)
        videoFrame->close();
    auto serializedAudioInternalData = map(serializedAudioData, [](auto& data) -> WebCodecsAudioInternalData { return data->data(); });
    for (auto& audioData : transferredAudioData)
        audioData->close();
#endif

    return adoptRef(*new SerializedScriptValue(WTFMove(buffer), WTFMove(blobHandles), arrayBufferContentsArray.releaseReturnValue(), context == SerializationContext::WorkerPostMessage ? WTFMove(sharedBuffers) : nullptr, WTFMove(detachedImageBitmaps)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
                , WTFMove(detachedCanvases)
                , WTFMove(inMemoryOffscreenCanvases)
#endif
                , WTFMove(inMemoryMessagePorts)
#if ENABLE(WEB_RTC)
                , WTFMove(detachedRTCDataChannels)
#endif
#if ENABLE(WEBASSEMBLY)
                , makeUnique<WasmModuleArray>(wasmModules)
                , context == SerializationContext::WorkerPostMessage ? makeUnique<WasmMemoryHandleArray>(wasmMemoryHandles) : nullptr
#endif
#if ENABLE(WEB_CODECS)
                , WTFMove(serializedVideoChunks)
                , WTFMove(serializedVideoFrameData)
                , WTFMove(serializedAudioChunks)
                , WTFMove(serializedAudioInternalData)
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
    JSGlobalObject* lexicalGlobalObject = toJS(originContext);
    VM& vm = lexicalGlobalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue value = toJS(lexicalGlobalObject, apiValue);
    auto serializedValue = SerializedScriptValue::create(*lexicalGlobalObject, value);
    if (UNLIKELY(scope.exception())) {
        if (exception)
            *exception = toRef(lexicalGlobalObject, scope.exception()->value());
        scope.clearException();
        return nullptr;
    }
    ASSERT(serializedValue);
    return serializedValue;
}

String SerializedScriptValue::toString() const
{
    return CloneDeserializer::deserializeString(m_internals.data);
}

JSValue SerializedScriptValue::deserialize(JSGlobalObject& lexicalGlobalObject, JSGlobalObject* globalObject, SerializationErrorMode throwExceptions, bool* didFail)
{
    return deserialize(lexicalGlobalObject, globalObject, { }, throwExceptions, didFail);
}

JSValue SerializedScriptValue::deserialize(JSGlobalObject& lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, SerializationErrorMode throwExceptions, bool* didFail)
{
    Vector<String> dummyBlobs;
    Vector<String> dummyPaths;
    return deserialize(lexicalGlobalObject, globalObject, messagePorts, dummyBlobs, dummyPaths, throwExceptions, didFail);
}

JSValue SerializedScriptValue::deserialize(JSGlobalObject& lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<RefPtr<MessagePort>>& messagePorts, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths, SerializationErrorMode throwExceptions, bool* didFail)
{
    DeserializationResult result = CloneDeserializer::deserialize(&lexicalGlobalObject, globalObject, messagePorts, WTFMove(m_internals.backingStores)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , WTFMove(m_internals.detachedOffscreenCanvases)
        , m_internals.inMemoryOffscreenCanvases
#endif
        , m_internals.inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , WTFMove(m_internals.detachedRTCDataChannels)
#endif
        , m_internals.arrayBufferContentsArray.get(), m_internals.data, blobURLs, blobFilePaths, m_internals.sharedBufferContentsArray.get()
#if ENABLE(WEBASSEMBLY)
        , m_internals.wasmModulesArray.get()
        , m_internals.wasmMemoryHandlesArray.get()
#endif
#if ENABLE(WEB_CODECS)
        , WTFMove(m_internals.serializedVideoChunks)
        , WTFMove(m_internals.serializedVideoFrames)
        , WTFMove(m_internals.serializedAudioChunks)
        , WTFMove(m_internals.serializedAudioData)
#endif
        );
    if (didFail)
        *didFail = result.second != SerializationReturnCode::SuccessfullyCompleted;
    if (throwExceptions == SerializationErrorMode::Throwing)
        maybeThrowExceptionIfSerializationFailed(lexicalGlobalObject, result.second);
    return result.first ? result.first : jsNull();
}

JSValueRef SerializedScriptValue::deserialize(JSContextRef destinationContext, JSValueRef* exception)
{
    JSGlobalObject* lexicalGlobalObject = toJS(destinationContext);
    VM& vm = lexicalGlobalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue value = deserialize(*lexicalGlobalObject, lexicalGlobalObject);
    if (UNLIKELY(scope.exception())) {
        if (exception)
            *exception = toRef(lexicalGlobalObject, scope.exception()->value());
        scope.clearException();
        return nullptr;
    }
    ASSERT(value);
    return toRef(lexicalGlobalObject, value);
}

Ref<SerializedScriptValue> SerializedScriptValue::nullValue()
{
    return adoptRef(*new SerializedScriptValue(Vector<uint8_t>()));
}

uint32_t SerializedScriptValue::wireFormatVersion()
{
    return CurrentVersion;
}

Vector<String> SerializedScriptValue::blobURLs() const
{
    return m_internals.blobHandles.map([](auto& handle) {
        return handle.url().string().isolatedCopy();
    });
}

void SerializedScriptValue::writeBlobsToDiskForIndexedDB(CompletionHandler<void(IDBValue&&)>&& completionHandler)
{
    ASSERT(isMainThread());
    ASSERT(hasBlobURLs());

    blobRegistry().writeBlobsToTemporaryFilesForIndexedDB(blobURLs(), [completionHandler = WTFMove(completionHandler), this, protectedThis = Ref { *this }] (auto&& blobFilePaths) mutable {
        ASSERT(isMainThread());

        if (blobFilePaths.isEmpty()) {
            // We should have successfully written blobs to temporary files.
            // If we failed, then we can't successfully store this record.
            completionHandler({ });
            return;
        }

        ASSERT(m_internals.blobHandles.size() == blobFilePaths.size());

        completionHandler({ *this, blobURLs(), blobFilePaths });
    });
}

IDBValue SerializedScriptValue::writeBlobsToDiskForIndexedDBSynchronously()
{
    ASSERT(!isMainThread());

    BinarySemaphore semaphore;
    IDBValue value;
    callOnMainThread([this, &semaphore, &value] {
        writeBlobsToDiskForIndexedDB([&semaphore, &value](IDBValue&& result) {
            ASSERT(isMainThread());
            value.setAsIsolatedCopy(result);

            semaphore.signal();
        });
    });
    semaphore.wait();

    return value;
}

} // namespace WebCore
