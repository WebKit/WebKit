/*
 * Copyright (C) 2009-2024 Apple Inc. All rights reserved.
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
#include "JSMediaSourceHandle.h"
#include "JSMediaStreamTrack.h"
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
#include <JavaScriptCore/Options.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/RegExp.h>
#include <JavaScriptCore/RegExpObject.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/TypedArrays.h>
#include <JavaScriptCore/WasmModule.h>
#include <JavaScriptCore/YarrFlags.h>
#include <limits>
#include <optional>
#include <wtf/CheckedArithmetic.h>
#include <wtf/CompletionHandler.h>
#include <wtf/DataLog.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/StackCheck.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/threads/BinarySemaphore.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if PLATFORM(COCOA)
#include <CoreFoundation/CoreFoundation.h>
#include <wtf/cf/VectorCF.h>
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

namespace SerializationHelper {

static constexpr bool verboseTrace = false;

} // namespace SerializationHelper


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

enum WalkerState { StateUnknown, ArrayStartState, ArrayStartVisitIndexedMember, ArrayEndVisitIndexedMember,
    ArrayStartVisitNamedMember, ArrayEndVisitNamedMember,
    ObjectStartState, ObjectStartVisitNamedMember, ObjectEndVisitNamedMember,
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
    CryptoKeyTag = 33,
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
#if ENABLE(MEDIA_STREAM)
    MediaStreamTrackTag = 60,
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    MediaSourceHandleTransferTag = 61,
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
    Float16ArrayTag = 12,
};

static ASCIILiteral name(SerializationTag tag)
{
    switch (tag) {
    case ArrayTag: return "ArrayTag"_s;
    case ObjectTag: return "ObjectTag"_s;
    case UndefinedTag: return "UndefinedTag"_s;
    case NullTag: return "NullTag"_s;
    case IntTag: return "IntTag"_s;
    case ZeroTag: return "ZeroTag"_s;
    case OneTag: return "OneTag"_s;
    case FalseTag: return "FalseTag"_s;
    case TrueTag: return "TrueTag"_s;
    case DoubleTag: return "DoubleTag"_s;
    case DateTag: return "DateTag"_s;
    case FileTag: return "FileTag"_s;
    case FileListTag: return "FileListTag"_s;
    case ImageDataTag: return "ImageDataTag"_s;
    case BlobTag: return "BlobTag"_s;
    case StringTag: return "StringTag"_s;
    case EmptyStringTag: return "EmptyStringTag"_s;
    case RegExpTag: return "RegExpTag"_s;
    case ObjectReferenceTag: return "ObjectReferenceTag"_s;
    case MessagePortReferenceTag: return "MessagePortReferenceTag"_s;
    case ArrayBufferTag: return "ArrayBufferTag"_s;
    case ArrayBufferViewTag: return "ArrayBufferViewTag"_s;
    case ArrayBufferTransferTag: return "ArrayBufferTransferTag"_s;
    case TrueObjectTag: return "TrueObjectTag"_s;
    case FalseObjectTag: return "FalseObjectTag"_s;
    case StringObjectTag: return "StringObjectTag"_s;
    case EmptyStringObjectTag: return "EmptyStringObjectTag"_s;
    case NumberObjectTag: return "NumberObjectTag"_s;
    case SetObjectTag: return "SetObjectTag"_s;
    case MapObjectTag: return "MapObjectTag"_s;
    case NonMapPropertiesTag: return "NonMapPropertiesTag"_s;
    case NonSetPropertiesTag: return "NonSetPropertiesTag"_s;
    case CryptoKeyTag: return "CryptoKeyTag"_s;
    case SharedArrayBufferTag: return "SharedArrayBufferTag"_s;
#if ENABLE(WEBASSEMBLY)
    case WasmModuleTag: return "WasmModuleTag"_s;
#endif
    case DOMPointReadOnlyTag: return "DOMPointReadOnlyTag"_s;
    case DOMPointTag: return "DOMPointTag"_s;
    case DOMRectReadOnlyTag: return "DOMRectReadOnlyTag"_s;
    case DOMRectTag: return "DOMRectTag"_s;
    case DOMMatrixReadOnlyTag: return "DOMMatrixReadOnlyTag"_s;
    case DOMMatrixTag: return "DOMMatrixTag"_s;
    case DOMQuadTag: return "DOMQuadTag"_s;
    case ImageBitmapTransferTag: return "ImageBitmapTransferTag"_s;
#if ENABLE(WEB_RTC)
    case RTCCertificateTag: return "RTCCertificateTag"_s;
#endif
    case ImageBitmapTag: return "ImageBitmapTag"_s;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    case OffscreenCanvasTransferTag: return "OffscreenCanvasTransferTag"_s;
#endif
    case BigIntTag: return "BigIntTag"_s;
    case BigIntObjectTag: return "BigIntObjectTag"_s;
#if ENABLE(WEBASSEMBLY)
    case WasmMemoryTag: return "WasmMemoryTag"_s;
#endif
#if ENABLE(WEB_RTC)
    case RTCDataChannelTransferTag: return "RTCDataChannelTransferTag"_s;
#endif
    case DOMExceptionTag: return "DOMExceptionTag"_s;
#if ENABLE(WEB_CODECS)
    case WebCodecsEncodedVideoChunkTag: return "WebCodecsEncodedVideoChunkTag"_s;
    case WebCodecsVideoFrameTag: return "WebCodecsVideoFrameTag"_s;
#endif
    case ResizableArrayBufferTag: return "ResizableArrayBufferTag"_s;
    case ErrorInstanceTag: return "ErrorInstanceTag"_s;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    case InMemoryOffscreenCanvasTag: return "InMemoryOffscreenCanvasTag"_s;
#endif
    case InMemoryMessagePortTag: return "InMemoryMessagePortTag"_s;
#if ENABLE(WEB_CODECS)
    case WebCodecsEncodedAudioChunkTag: return "WebCodecsEncodedAudioChunkTag"_s;
    case WebCodecsAudioDataTag: return "WebCodecsAudioDataTag"_s;
#endif
#if ENABLE(MEDIA_STREAM)
    case MediaStreamTrackTag: return "MediaStreamTrackTag"_s;
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    case MediaSourceHandleTransferTag: return "MediaSourceHandleTransferTag"_s;
#endif
    case ErrorTag: return "ErrorTag"_s;
    }
    return "<unknown tag>"_s;
}

} // namespace WebCore

namespace WTF {

void printInternal(PrintStream&, WebCore::SerializationTag);

void printInternal(PrintStream& out, WebCore::SerializationTag tag)
{
    auto tagName = WebCore::name(tag);
    if (tagName[0U] != '<')
        out.print(tagName);
    else
        out.print("<unknown tag "_s, static_cast<unsigned>(tag), ">"_s);
}

} // namespace WTF

namespace WebCore {

// This function is only used for a sanity check mechanism used in
// CloneSerializer::addToObjectPoolIfNotDupe() and CloneDeserializer::addToObjectPool().
static constexpr bool canBeAddedToObjectPool(SerializationTag tag)
{
    // If you add a type to the allow ist (i.e. returns true) here, it means
    // that both the serializer and deserializer will push objects of this
    // type onto their m_objectPool. This is important because the order of
    // the objects in the m_objectPool must match for both the serializer and
    // deserializer.
    switch (tag) {
    case ArrayTag:
    case ArrayBufferTag:
    case ArrayBufferViewTag:
    case BigIntObjectTag:
    case EmptyStringObjectTag:
    case FalseObjectTag:
    case MapObjectTag:
    case NumberObjectTag:
    case ObjectTag:
    case ResizableArrayBufferTag:
    case SetObjectTag:
    case SharedArrayBufferTag:
    case StringObjectTag:
    case TrueObjectTag:
        return true;
    default:
        break;
    }
    return false;
}


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
    case CryptoKeyTag:
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
#if ENABLE(MEDIA_STREAM)
    case MediaStreamTrackTag:
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    case MediaSourceHandleTransferTag:
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
    case Float16ArrayTag:
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
    DestinationColorSpaceLinearSRGBTag = 1,
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
    DestinationColorSpaceDisplayP3Tag = 2,
#endif
#if PLATFORM(COCOA)
    DestinationColorSpaceCGColorSpaceNameTag = 3,
    DestinationColorSpaceCGColorSpacePropertyListTag = 4,
#endif
};

namespace {

enum class ImageBitmapSerializationFlags : uint8_t {
    OriginClean              = 1 << 0, // ImageBitmap is always clean if serialized. However, at some point non-clean bitmaps were serialized. Can be removed once version is increased.
    PremultiplyAlpha         = 1 << 1,
    ForciblyPremultiplyAlpha = 1 << 2
};

}

#define SERIALIZE_TRACE(...) do { \
        if constexpr (SerializationHelper::verboseTrace) \
            dataLogLn("TRACE ", __VA_ARGS__, " @ ", __LINE__); \
    } while (false)

#if ENABLE(WEBASSEMBLY)
static String agentClusterIDFromGlobalObject(JSGlobalObject& globalObject)
{
    if (!globalObject.inherits<JSDOMGlobalObject>())
        return JSDOMGlobalObject::defaultAgentClusterID();
    return jsCast<JSDOMGlobalObject*>(&globalObject)->agentClusterID();
}
#endif

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
static constexpr unsigned CurrentMajorVersion = 15;
static constexpr unsigned CurrentMinorVersion = 0;
static constexpr unsigned majorVersionFor(unsigned version) { return version & 0x00FFFFFF; }
static constexpr unsigned minorVersionFor(unsigned version) { return version >> 24; }
static constexpr unsigned makeVersion(unsigned major, unsigned minor)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(major < (1u << 24));
    ASSERT_UNDER_CONSTEXPR_CONTEXT(minor < (1u << 8));
    return (minor << 24) | major;
}
/* currentVersion tracks the serialization version so that persistent stores
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
 * Version 12.1. changed the terminator of the indexed property section in array.
 * Version 13. added support for ErrorInstance objects.
 * Version 14. encode booleans as uint8_t instead of int32_t.
 * Version 15. changed the terminator of the indexed property section in array.
 */
static constexpr unsigned currentVersion() { return makeVersion(CurrentMajorVersion, CurrentMinorVersion); }
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
 * SerializedValue :- <version:uint32_t> Value
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
 *    | ImageBitmapTag <imageBitmapSerializationFlags:uint8_t> <logicalWidth:int32_t> <logicalHeight:int32_t> <resolutionScale:double> DestinationColorSpace <byteLength:uint32_t>(<imageByteData:uint8_t>)
 *    | OffscreenCanvasTransferTag <value:uint32_t>
 *    | WasmMemoryTag <value:uint32_t>
 *    | RTCDataChannelTransferTag <identifier:uint32_t>
 *    | DOMExceptionTag <message:String> <name:String>
 *    | WebCodecsEncodedVideoChunkTag <identifier:uint32_t>
 *    | MediaStreamTrackTag <identifier:uint32_t>
 *    | MediaSourceHandleTransferTag <identifier:uint32_t>
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

#if ASSERT_ENABLED
public:
    const Vector<SerializationTag>& objectPoolTags() const { return m_objectPoolTags; }

protected:
    void appendObjectPoolTag(SerializationTag tag)
    {
        m_objectPoolTags.append(tag);
    }
#else
    ALWAYS_INLINE void appendObjectPoolTag(SerializationTag) { }
#endif
    bool isSafeToRecurse()
    {
        return m_stackCheck.isSafeToRecurse();
    }

    JSGlobalObject* const m_lexicalGlobalObject;
    bool m_failed;
    MarkedArgumentBuffer m_keepAliveBuffer;
    MarkedArgumentBuffer m_objectPool;
#if ASSERT_ENABLED
    Vector<SerializationTag> m_objectPoolTags;
#endif
    StackCheck m_stackCheck;
};

static std::optional<Vector<uint8_t>> wrapCryptoKey(JSGlobalObject* lexicalGlobalObject, const Vector<uint8_t>& key)
{
    auto context = executionContext(lexicalGlobalObject);
    if (!context)
        return std::nullopt;

    return context->wrapCryptoKey(key);
}

static std::optional<Vector<uint8_t>> unwrapCryptoKey(JSGlobalObject* lexicalGlobalObject, const Vector<uint8_t>& wrappedKey)
{
    auto context = executionContext(lexicalGlobalObject);
    if (!context)
        return std::nullopt;

    return context->unwrapCryptoKey(wrappedKey);
}

#if ASSUME_LITTLE_ENDIAN
template <typename T> static void writeLittleEndian(Vector<uint8_t>& buffer, T value)
{
    buffer.append(std::span { reinterpret_cast<uint8_t*>(&value), sizeof(value) });
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

template <typename T> static bool writeLittleEndian(Vector<uint8_t>& buffer, std::span<const T> values)
{
    if (values.size() > std::numeric_limits<uint32_t>::max() / sizeof(T))
        return false;

#if ASSUME_LITTLE_ENDIAN
    buffer.append(asBytes(values));
#else
    for (unsigned i = 0; i < values.size(); i++) {
        T value = values[i];
        for (unsigned j = 0; j < sizeof(T); j++) {
            buffer.append(static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }
#endif
    return true;
}

template <> bool writeLittleEndian<uint8_t>(Vector<uint8_t>& buffer, std::span<const uint8_t> values)
{
    buffer.append(values);
    return true;
}

class CloneSerializer;
#if ASSERT_ENABLED
static void validateSerializedResult(CloneSerializer&, SerializationReturnCode, Vector<uint8_t>& result, JSGlobalObject*, Vector<Ref<MessagePort>>&, ArrayBufferContentsArray&, ArrayBufferContentsArray& sharedBuffers, Vector<Ref<MessagePort>>&);
#endif

class CloneSerializer : public CloneBase {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    static SerializationReturnCode serialize(JSGlobalObject* lexicalGlobalObject, JSValue value, Vector<Ref<MessagePort>>& messagePorts, Vector<RefPtr<JSC::ArrayBuffer>>& arrayBuffers, const Vector<RefPtr<ImageBitmap>>& imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            const Vector<RefPtr<OffscreenCanvas>>& offscreenCanvases,
            Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases,
#endif
            Vector<Ref<MessagePort>>& inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
            const Vector<Ref<RTCDataChannel>>& rtcDataChannels,
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            const Vector<Ref<MediaSourceHandle>>& mediaSourceHandles,
#endif
#if ENABLE(WEB_CODECS)
            Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>& serializedVideoChunks,
            Vector<RefPtr<WebCodecsVideoFrame>>& serializedVideoFrames,
            Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>& serializedAudioChunks,
            Vector<RefPtr<WebCodecsAudioData>>& serializedAudioData,
#endif
#if ENABLE(MEDIA_STREAM)
            Vector<RefPtr<MediaStreamTrack>>& serializedMediaStreamTracks,
#endif
#if ENABLE(WEBASSEMBLY)
            WasmModuleArray& wasmModules,
            WasmMemoryHandleArray& wasmMemoryHandles,
#endif
        Vector<URLKeepingBlobAlive>& blobHandles, Vector<uint8_t>& out, SerializationContext context, ArrayBufferContentsArray& sharedBuffers,
        SerializationForStorage forStorage)
    {
#if ASSERT_ENABLED
        auto& vm = lexicalGlobalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
#endif

        CloneSerializer serializer(lexicalGlobalObject, messagePorts, arrayBuffers, imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            offscreenCanvases,
            inMemoryOffscreenCanvases,
#endif
            inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
            rtcDataChannels,
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            mediaSourceHandles,
#endif
#if ENABLE(WEB_CODECS)
            serializedVideoChunks,
            serializedVideoFrames,
            serializedAudioChunks,
            serializedAudioData,
#endif
#if ENABLE(MEDIA_STREAM)
            serializedMediaStreamTracks,
#endif
#if ENABLE(WEBASSEMBLY)
            wasmModules,
            wasmMemoryHandles,
#endif
            blobHandles, out, context, sharedBuffers, forStorage);
        auto code = serializer.serialize(value);
#if ASSERT_ENABLED
        RETURN_IF_EXCEPTION(scope, code);
        validateSerializedResult(serializer, code, out, lexicalGlobalObject, messagePorts, sharedBuffers, sharedBuffers, inMemoryMessagePorts);
#endif
        return code;
    }

    static bool serialize(StringView string, Vector<uint8_t>& out)
    {
        writeLittleEndian(out, currentVersion());
        if (string.isEmpty()) {
            writeLittleEndian<uint8_t>(out, EmptyStringTag);
            return true;
        }
        writeLittleEndian<uint8_t>(out, StringTag);
        if (string.is8Bit()) {
            writeLittleEndian(out, string.length() | StringDataIs8BitFlag);
            return writeLittleEndian(out, string.span8());
        }
        writeLittleEndian(out, string.length());
        return writeLittleEndian(out, string.span16());
    }

#if ASSERT_ENABLED
    bool didSeeComplexCases() const { return m_didSeeComplexCases; }
    void setDidSeeComplexCases() { m_didSeeComplexCases = true; }
#else
    ALWAYS_INLINE void setDidSeeComplexCases() { }
#endif

private:
    using ObjectPoolMap = UncheckedKeyHashMap<JSObject*, uint32_t>;

    CloneSerializer(JSGlobalObject* lexicalGlobalObject, Vector<Ref<MessagePort>>& messagePorts, Vector<RefPtr<JSC::ArrayBuffer>>& arrayBuffers, const Vector<RefPtr<ImageBitmap>>& imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            const Vector<RefPtr<OffscreenCanvas>>& offscreenCanvases,
            Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases,
#endif
            Vector<Ref<MessagePort>>& inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
            const Vector<Ref<RTCDataChannel>>& rtcDataChannels,
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            const Vector<Ref<MediaSourceHandle>>& mediaSourceHandles,
#endif
#if ENABLE(WEB_CODECS)
            Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>& serializedVideoChunks,
            Vector<RefPtr<WebCodecsVideoFrame>>& serializedVideoFrames,
            Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>& serializedAudioChunks,
            Vector<RefPtr<WebCodecsAudioData>>& serializedAudioData,
#endif
#if ENABLE(MEDIA_STREAM)
            Vector<RefPtr<MediaStreamTrack>>& serializedMediaStreamTracks,
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
#if ENABLE(MEDIA_STREAM)
        , m_serializedMediaStreamTracks(serializedMediaStreamTracks)
#endif
        , m_forStorage(forStorage)
    {
        write(currentVersion());
        fillTransferMap(messagePorts, m_transferredMessagePorts);
        fillTransferMap(arrayBuffers, m_transferredArrayBuffers);
        fillTransferMap(imageBitmaps, m_transferredImageBitmaps);
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        fillTransferMap(offscreenCanvases, m_transferredOffscreenCanvases);
#endif
#if ENABLE(WEB_RTC)
        fillTransferMap(rtcDataChannels, m_transferredRTCDataChannels);
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        fillTransferMap(mediaSourceHandles, m_transferredMediaSourceHandles);
#endif
    }

    template<typename T>
    void fillTransferMap(const Vector<T>& input, ObjectPoolMap& result)
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

    template<SerializationTag tag1, SerializationTag tag2 = ErrorTag, SerializationTag tag3 = ErrorTag>
    bool writeObjectReferenceIfDupe(JSObject* object)
    {
        static_assert(canBeAddedToObjectPool(tag1)
            && (canBeAddedToObjectPool(tag2) || tag2 == ErrorTag)
            && (canBeAddedToObjectPool(tag3) || tag3 == ErrorTag));

        // Record object for graph reconstruction
        auto found = m_objectPoolMap.find(object);

        // Handle duplicate references
        if (found != m_objectPoolMap.end()) {
            write(ObjectReferenceTag);
            ASSERT(found->value < m_objectPoolMap.size());
            writeObjectIndex(found->value);
            return true; // is dupe.
        }
        return false; // not dupe.
    }

    template<SerializationTag tag1, SerializationTag tag2 = ErrorTag, SerializationTag tag3 = ErrorTag>
    bool addToObjectPool(JSObject* object)
    {
        static_assert(canBeAddedToObjectPool(tag1)
            && (canBeAddedToObjectPool(tag2) || tag2 == ErrorTag)
            && (canBeAddedToObjectPool(tag3) || tag3 == ErrorTag));

        m_objectPoolMap.add(object, m_objectPoolMap.size());
        m_objectPool.appendWithCrashOnOverflow(object);

        if constexpr (tag2 == ErrorTag)
            appendObjectPoolTag(tag1);

        return true; // new object added.
    }

    template<SerializationTag tag1, SerializationTag tag2 = ErrorTag, SerializationTag tag3 = ErrorTag>
    bool addToObjectPoolIfNotDupe(JSObject* object)
    {
        static_assert(canBeAddedToObjectPool(tag1)
            && (canBeAddedToObjectPool(tag2) || tag2 == ErrorTag)
            && (canBeAddedToObjectPool(tag3) || tag3 == ErrorTag));

        if (writeObjectReferenceIfDupe<tag1, tag2, tag3>(object))
            return false; // new object NOT added. It's a dupe.

        addToObjectPool<tag1, tag2, tag3>(object);
        return true; // new object added.
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
        if (string.isEmpty()) {
            appendObjectPoolTag(EmptyStringObjectTag);
            write(EmptyStringObjectTag);
        } else {
            appendObjectPoolTag(StringObjectTag);
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
        else if (obj->inherits<JSFloat16Array>())
            write(Float16ArrayTag);
        else if (obj->inherits<JSFloat32Array>())
            write(Float32ArrayTag);
        else if (obj->inherits<JSFloat64Array>())
            write(Float64ArrayTag);
        else if (obj->inherits<JSBigInt64Array>())
            write(BigInt64ArrayTag);
        else if (obj->inherits<JSBigUint64Array>())
            write(BigUint64ArrayTag);
        else {
            // We need to return true here because the client only checks for the error condition if
            // the return value is true (same as all the error cases below).
            code = SerializationReturnCode::DataCloneError;
            return true;
        }

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
        OptionSet<ImageBitmapSerializationFlags> flags;
        // Origin must be clean to transfer, but the check was not always in place. Ensure tainted ImageBitmaps are not
        // loaded anymore.
        flags.add(ImageBitmapSerializationFlags::OriginClean);
        if (imageBitmap.premultiplyAlpha())
            flags.add(ImageBitmapSerializationFlags::PremultiplyAlpha);
        if (imageBitmap.forciblyPremultiplyAlpha())
            flags.add(ImageBitmapSerializationFlags::ForciblyPremultiplyAlpha);
        write(ImageBitmapTag);
        write(static_cast<uint8_t>(flags.toRaw()));
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
        write(arrayBuffer->span());
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
#if ENABLE(MEDIA_STREAM)
    void dumpMediaStreamTrack(JSObject* obj)
    {
        Ref track = jsCast<JSMediaStreamTrack*>(obj)->wrapped();

        auto index = m_serializedMediaStreamTracks.find(track.ptr());
        if (index == notFound) {
            index = m_serializedMediaStreamTracks.size();
            m_serializedMediaStreamTracks.append(WTFMove(track));
        }

        write(MediaStreamTrackTag);
        write(static_cast<uint32_t>(index));
    }
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    void dumpMediaSourceHandle(JSObject* obj, SerializationReturnCode& code)
    {
        auto index = m_transferredMediaSourceHandles.find(obj);
        if (index != m_transferredMediaSourceHandles.end()) {
            write(MediaSourceHandleTransferTag);
            write(index->value);
            return;
        }

        code = SerializationReturnCode::DataCloneError;
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
                if (!addToObjectPoolIfNotDupe<TrueObjectTag, FalseObjectTag>(booleanObject))
                    return true;
                auto tag = booleanObject->internalValue().toBoolean(m_lexicalGlobalObject) ? TrueObjectTag : FalseObjectTag;
                write(tag);
                appendObjectPoolTag(tag);
                return true;
            }
            if (auto* stringObject = jsDynamicCast<StringObject*>(obj)) {
                if (!addToObjectPoolIfNotDupe<EmptyStringObjectTag, StringObjectTag>(stringObject))
                    return true;
                auto str = asString(stringObject->internalValue())->value(m_lexicalGlobalObject);
                dumpStringObject(str);
                return true;
            }
            if (auto* numberObject = jsDynamicCast<NumberObject*>(obj)) {
                if (!addToObjectPoolIfNotDupe<NumberObjectTag>(numberObject))
                    return true;
                write(NumberObjectTag);
                write(numberObject->internalValue().asNumber());
                return true;
            }
            if (auto* bigIntObject = jsDynamicCast<BigIntObject*>(obj)) {
                if (!addToObjectPoolIfNotDupe<BigIntObjectTag>(bigIntObject))
                    return true;
                write(BigIntObjectTag);
                JSValue bigIntValue = bigIntObject->internalValue();
                ASSERT(bigIntValue.isBigInt());
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
                write(data->data().span());
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
                auto errorInformation = extractErrorInformationFromErrorInstance(m_lexicalGlobalObject, *errorInstance);
                if (!errorInformation)
                    return false;

                write(ErrorInstanceTag);
                write(errorNameToSerializableErrorType(errorInformation->errorTypeString));
                writeNullableString(errorInformation->message);
                write(errorInformation->line);
                write(errorInformation->column);
                writeNullableString(errorInformation->sourceURL);
                writeNullableString(errorInformation->stack);
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
                    m_inMemoryMessagePorts.append(jsCast<JSMessagePort*>(obj)->wrapped());
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
                if (!addToObjectPoolIfNotDupe<ArrayBufferTag, ResizableArrayBufferTag, SharedArrayBufferTag>(obj))
                    return true;
                
                if (arrayBuffer->isShared() && (m_context == SerializationContext::WorkerPostMessage || m_forStorage == SerializationForStorage::Yes)) {
                    // https://html.spec.whatwg.org/multipage/structured-data.html#structuredserializeinternal
                    if (!JSC::Options::useSharedArrayBuffer() || m_forStorage == SerializationForStorage::Yes) {
                        code = SerializationReturnCode::DataCloneError;
                        return true;
                    }
                    uint32_t index = m_sharedBuffers.size();
                    ArrayBufferContents contents;
                    if (arrayBuffer->shareWith(contents)) {
                        appendObjectPoolTag(SharedArrayBufferTag);
                        write(SharedArrayBufferTag);
                        m_sharedBuffers.append(WTFMove(contents));
                        write(index);
                        return true;
                    }
                }
                
                if (arrayBuffer->isResizableOrGrowableShared()) {
                    appendObjectPoolTag(ResizableArrayBufferTag);
                    write(ResizableArrayBufferTag);
                    uint64_t byteLength = arrayBuffer->byteLength();
                    write(byteLength);
                    uint64_t maxByteLength = arrayBuffer->maxByteLength().value_or(0);
                    write(maxByteLength);
                    write(arrayBuffer->span());
                    return true;
                }

                appendObjectPoolTag(ArrayBufferTag);
                write(ArrayBufferTag);
                uint64_t byteLength = arrayBuffer->byteLength();
                write(byteLength);
                write(arrayBuffer->span());
                return true;
            }
            if (obj->inherits<JSArrayBufferView>()) {
                // Note: we can't just use addToObjectPoolIfNotDupe() here because the deserializer
                // expects to deserialize the children before it deserializes the JSArrayBufferView.
                // We need to make the serializer follow the same serialization order here by doing
                // this dance with writeObjectReferenceIfDupe() and addToObjectPool().
                if (writeObjectReferenceIfDupe<ArrayBufferViewTag>(obj))
                    return true;
                bool success = dumpArrayBufferView(obj, code);
                addToObjectPool<ArrayBufferViewTag>(obj);
                return success;
            }
            if (auto* key = JSCryptoKey::toWrapped(vm, obj)) {
                write(CryptoKeyTag);
                Vector<uint8_t> serializedKey;
                Vector<URLKeepingBlobAlive> dummyBlobHandles;
                Vector<Ref<MessagePort>> dummyMessagePorts;
                Vector<RefPtr<JSC::ArrayBuffer>> dummyArrayBuffers;
#if ENABLE(WEB_CODECS)
                Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>> dummyVideoChunks;
                Vector<RefPtr<WebCodecsVideoFrame>> dummyVideoFrames;
                Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>> dummyAudioChunks;
                Vector<RefPtr<WebCodecsAudioData>> dummyAudioData;
#endif
#if ENABLE(MEDIA_STREAM)
                Vector<RefPtr<MediaStreamTrack>> dummyMediaStreamTracks;
#endif
#if ENABLE(WEBASSEMBLY)
                WasmModuleArray dummyModules;
                WasmMemoryHandleArray dummyMemoryHandles;
#endif
                ArrayBufferContentsArray dummySharedBuffers;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
                Vector<RefPtr<OffscreenCanvas>> dummyInMemoryOffscreenCanvases;
#endif
                Vector<Ref<MessagePort>> dummyInMemoryMessagePorts;
                CloneSerializer rawKeySerializer(m_lexicalGlobalObject, dummyMessagePorts, dummyArrayBuffers, { },
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
                    { },
                    dummyInMemoryOffscreenCanvases,
#endif
                    dummyInMemoryMessagePorts,
#if ENABLE(WEB_RTC)
                    { },
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
                    { },
#endif
#if ENABLE(WEB_CODECS)
                    dummyVideoChunks,
                    dummyVideoFrames,
                    dummyAudioChunks,
                    dummyAudioData,
#endif
#if ENABLE(MEDIA_STREAM)
                    dummyMediaStreamTracks,
#endif
#if ENABLE(WEBASSEMBLY)
                    dummyModules,
                    dummyMemoryHandles,
#endif
                    dummyBlobHandles, serializedKey, SerializationContext::Default, dummySharedBuffers, m_forStorage);
                rawKeySerializer.write(key);

                auto wrappedKey = wrapCryptoKey(m_lexicalGlobalObject, serializedKey);
                if (!wrappedKey)
                    return false;

                write(*wrappedKey);
                return true;
            }
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
#if ENABLE(MEDIA_STREAM)
            if (obj->inherits<JSMediaStreamTrack>()) {
                if (m_forStorage == SerializationForStorage::Yes)
                    return false;
                dumpMediaStreamTrack(obj);
                return true;
            }
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            if (obj->inherits<JSMediaSourceHandle>()) {
                dumpMediaSourceHandle(obj, code);
                return true;
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
        SERIALIZE_TRACE("serialize ", tag);
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
        writeConstantPoolIndex(m_objectPoolMap, i);
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
            if (!writeLittleEndian(m_buffer, str.span8()))
                fail();
            return;
        }
        if (!writeLittleEndian(m_buffer, str.span16()))
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
        writeLittleEndian(m_buffer, vector.span());
    }

    void write(const File& file)
    {
        m_blobHandles.append(file.handle().isolatedCopy());
        write(file.path());
        write(file.url().string());
        write(file.type());
        write(file.name());
        if (m_forStorage == SerializationForStorage::No)
            write(static_cast<double>(file.lastModifiedOverride().value_or(-1)));
        else {
            if (auto lastModifiedOverride = file.lastModifiedOverride())
                write(static_cast<double>(*lastModifiedOverride));
            else
                write(static_cast<double>(file.lastModified()));
        }
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
        auto dataSpan = span(data.get());
        write(static_cast<uint32_t>(dataSpan.size()));
        write(dataSpan);
    }
#endif

    void write(DestinationColorSpace destinationColorSpace)
    {
        if (destinationColorSpace == DestinationColorSpace::SRGB()) {
            write(DestinationColorSpaceSRGBTag);
            return;
        }

        if (destinationColorSpace == DestinationColorSpace::LinearSRGB()) {
            write(DestinationColorSpaceLinearSRGBTag);
            return;
        }

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

    void write(std::span<const uint8_t> data)
    {
        m_buffer.append(data);
    }

    Vector<uint8_t>& m_buffer;
    Vector<URLKeepingBlobAlive>& m_blobHandles;
    ObjectPoolMap m_objectPoolMap;
    ObjectPoolMap m_transferredMessagePorts;
    ObjectPoolMap m_transferredArrayBuffers;
    ObjectPoolMap m_transferredImageBitmaps;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    ObjectPoolMap m_transferredOffscreenCanvases;
#endif
#if ENABLE(WEB_RTC)
    ObjectPoolMap m_transferredRTCDataChannels;
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    ObjectPoolMap m_transferredMediaSourceHandles;
#endif
    typedef UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, uint32_t, IdentifierRepHash> StringConstantPool;
    StringConstantPool m_constantPool;
    using ImageDataPool = UncheckedKeyHashMap<Ref<ImageData>, uint32_t>;
    ImageDataPool m_imageDataPool;
    Identifier m_emptyIdentifier;
    SerializationContext m_context;
    ArrayBufferContentsArray& m_sharedBuffers;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<RefPtr<OffscreenCanvas>>& m_inMemoryOffscreenCanvases;
#endif
    Vector<Ref<MessagePort>>& m_inMemoryMessagePorts;
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
#if ENABLE(MEDIA_STREAM)
    Vector<RefPtr<MediaStreamTrack>>& m_serializedMediaStreamTracks;
#endif
    SerializationForStorage m_forStorage;

#if ASSERT_ENABLED
    bool m_didSeeComplexCases { false };
#endif
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
                if (!addToObjectPoolIfNotDupe<ArrayTag>(inArray))
                    break;
                write(ArrayTag);
                write(length);
                inputObjectStack.append(inArray);
                indexStack.append(0);
                lengthStack.append(length);
            }
            arrayStartVisitIndexedMember:
            FALLTHROUGH;
            case ArrayStartVisitIndexedMember: {
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
                        goto startVisitNamedMember;
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
                    goto arrayStartVisitIndexedMember;
                }

                write(index);
                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                if (dumpIfTerminal(inValue, terminalCode)) {
                    if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                        return terminalCode;
                    indexStack.last()++;
                    goto arrayStartVisitIndexedMember;
                }
                stateStack.append(ArrayEndVisitIndexedMember);
                goto stateUnknown;
            }
            case ArrayEndVisitIndexedMember: {
                indexStack.last()++;
                goto arrayStartVisitIndexedMember;
            }
            case ArrayStartVisitNamedMember:
            case ArrayEndVisitNamedMember:
                RELEASE_ASSERT_NOT_REACHED();
            objectStartState:
            case ObjectStartState: {
                ASSERT(inValue.isObject());
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;
                JSObject* inObject = asObject(inValue);
                if (!addToObjectPoolIfNotDupe<ObjectTag>(inObject))
                    break;
                write(ObjectTag);
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
            startVisitNamedMember:
            FALLTHROUGH;
            case ObjectStartVisitNamedMember: {
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
                    goto startVisitNamedMember;
                }
                write(properties[index]);

                if (UNLIKELY(scope.exception()))
                    return SerializationReturnCode::ExistingExceptionError;

                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                if (!dumpIfTerminal(inValue, terminalCode)) {
                    stateStack.append(ObjectEndVisitNamedMember);
                    goto stateUnknown;
                }
                if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                    return terminalCode;
                FALLTHROUGH;
            }
            case ObjectEndVisitNamedMember: {
                if (UNLIKELY(scope.exception()))
                    return SerializationReturnCode::ExistingExceptionError;

                indexStack.last()++;
                goto startVisitNamedMember;
            }
            mapStartState: {
                ASSERT(inValue.isObject());
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;
                JSMap* inMap = jsCast<JSMap*>(inValue);
                if (!addToObjectPoolIfNotDupe<MapObjectTag>(inMap))
                    break;
                write(MapObjectTag);
                JSMapIterator* iterator = JSMapIterator::create(m_lexicalGlobalObject, m_lexicalGlobalObject->mapIteratorStructure(), inMap, IterationKind::Entries);
                if (UNLIKELY(scope.exception()))
                    return SerializationReturnCode::ExistingExceptionError;
                m_keepAliveBuffer.appendWithCrashOnOverflow(iterator);
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
                    goto startVisitNamedMember;
                }
                inValue = key;
                m_keepAliveBuffer.appendWithCrashOnOverflow(value);
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
                if (!addToObjectPoolIfNotDupe<SetObjectTag>(inSet))
                    break;
                write(SetObjectTag);
                JSSetIterator* iterator = JSSetIterator::create(m_lexicalGlobalObject, m_lexicalGlobalObject->setIteratorStructure(), inSet, IterationKind::Keys);
                if (UNLIKELY(scope.exception()))
                    return SerializationReturnCode::ExistingExceptionError;
                m_keepAliveBuffer.appendWithCrashOnOverflow(iterator);
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
                    goto startVisitNamedMember;
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

class CloneDeserializer : public CloneBase {
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
        if (!readLittleEndian(ptr, end, version) || majorVersionFor(version) > CurrentMajorVersion)
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

    static DeserializationResult deserialize(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<Ref<MessagePort>>& messagePorts, Vector<std::optional<DetachedImageBitmap>>&& detachedImageBitmaps
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases
        , const Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases
#endif
        , const Vector<Ref<MessagePort>>& inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , Vector<RefPtr<DetachedMediaSourceHandle>>&& detachedMediaSourceHandles
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
#if ENABLE(MEDIA_STREAM)
        , Vector<std::unique_ptr<MediaStreamTrackDataHolder>>&& serializedMediaStreamTracks
#endif
        )
    {
        if (!buffer.size())
            return std::make_pair(jsNull(), SerializationReturnCode::UnspecifiedError);
        CloneDeserializer deserializer(lexicalGlobalObject, globalObject, messagePorts, arrayBufferContentsArray, buffer, blobURLs, blobFilePaths, sharedBuffers, WTFMove(detachedImageBitmaps)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            , WTFMove(detachedOffscreenCanvases)
            , inMemoryOffscreenCanvases
#endif
            , inMemoryMessagePorts
#if ENABLE(WEB_RTC)
            , WTFMove(detachedRTCDataChannels)
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            , WTFMove(detachedMediaSourceHandles)
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
#if ENABLE(MEDIA_STREAM)
            , WTFMove(serializedMediaStreamTracks)
#endif
            );
        if (!deserializer.isValid())
            return std::make_pair(JSValue(), SerializationReturnCode::ValidationError);
        
        auto result = deserializer.deserialize();
        // Deserialize again if data may have wrong version number, see rdar://118775332.
        if (UNLIKELY(result.second != SerializationReturnCode::SuccessfullyCompleted && deserializer.shouldRetryWithVersionUpgrade())) {
        CloneDeserializer newDeserializer(lexicalGlobalObject, globalObject, messagePorts, arrayBufferContentsArray, buffer, blobURLs, blobFilePaths, sharedBuffers, deserializer.takeDetachedImageBitmaps()
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            , deserializer.takeDetachedOffscreenCanvases()
            , inMemoryOffscreenCanvases
#endif
            , inMemoryMessagePorts
#if ENABLE(WEB_RTC)
            , deserializer.takeDetachedRTCDataChannels()
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            , deserializer.takeDetachedMediaSourceHandles()
#endif
#if ENABLE(WEBASSEMBLY)
            , wasmModules
            , wasmMemoryHandles
#endif
#if ENABLE(WEB_CODECS)
            , deserializer.takeSerializedVideoChunks()
            , deserializer.takeSerializedVideoFrames()
            , deserializer.takeSerializedAudioChunks()
            , deserializer.takeSerializedAudioData()
#endif
#if ENABLE(MEDIA_STREAM)
            , deserializer.takeSerializedMediaStreamTracks()
#endif
            );
            newDeserializer.upgradeVersion();
            result = newDeserializer.deserialize();
        }
        return result;
    }

private:
    struct CachedString {
        CachedString(String&& string)
            : m_string(WTFMove(string))
        {
        }

        JSValue jsString(CloneDeserializer& deserializer)
        {
            if (!m_jsString) {
                auto& vm = deserializer.m_lexicalGlobalObject->vm();
                m_jsString = JSC::jsString(vm, m_string);
                deserializer.m_keepAliveBuffer.appendWithCrashOnOverflow(m_jsString);
            }
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

    CloneDeserializer(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<Ref<MessagePort>>& messagePorts, ArrayBufferContentsArray* arrayBufferContents, Vector<std::optional<DetachedImageBitmap>>&& detachedImageBitmaps, const Vector<uint8_t>& buffer
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases = { }
        , const Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases = { }
#endif
        , const Vector<Ref<MessagePort>>& inMemoryMessagePorts = { }
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels = { }
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , Vector<RefPtr<DetachedMediaSourceHandle>>&& detachedMediaSourceHandles = { }
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
#if ENABLE(MEDIA_STREAM)
        , Vector<std::unique_ptr<MediaStreamTrackDataHolder>>&& serializedMediaStreamTracks = { }
#endif
        )
        : CloneBase(lexicalGlobalObject)
        , m_globalObject(globalObject)
        , m_isDOMGlobalObject(globalObject->inherits<JSDOMGlobalObject>())
        , m_canCreateDOMObject(m_isDOMGlobalObject && !globalObject->inherits<JSIDBSerializationGlobalObject>())
        , m_ptr(buffer.data())
        , m_end(buffer.data() + buffer.size())
        , m_majorVersion(0xFFFFFFFF)
        , m_minorVersion(0xFFFFFFFF)
        , m_messagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_detachedImageBitmaps(WTFMove(detachedImageBitmaps))
        , m_imageBitmaps(m_detachedImageBitmaps.size())
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
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , m_detachedMediaSourceHandles(WTFMove(detachedMediaSourceHandles))
        , m_mediaSourceHandles(m_detachedMediaSourceHandles.size())
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
#if ENABLE(MEDIA_STREAM)
        , m_serializedMediaStreamTracks(WTFMove(serializedMediaStreamTracks))
        , m_mediaStreamTracks(m_serializedMediaStreamTracks.size())
#endif
    {
        unsigned version;
        if (read(version)) {
            m_majorVersion = majorVersionFor(version);
            m_minorVersion = minorVersionFor(version);
        }
    }

    CloneDeserializer(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<Ref<MessagePort>>& messagePorts, ArrayBufferContentsArray* arrayBufferContents, const Vector<uint8_t>& buffer, const Vector<String>& blobURLs, const Vector<String> blobFilePaths, ArrayBufferContentsArray* sharedBuffers, Vector<std::optional<DetachedImageBitmap>>&& detachedImageBitmaps
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases
        , const Vector<RefPtr<OffscreenCanvas>>& inMemoryOffscreenCanvases
#endif
        , const Vector<Ref<MessagePort>>& inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , Vector<RefPtr<DetachedMediaSourceHandle>>&& detachedMediaSourceHandles
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
#if ENABLE(MEDIA_STREAM)
        , Vector<std::unique_ptr<MediaStreamTrackDataHolder>>&& serializedMediaStreamTracks = { }
#endif
        )
        : CloneBase(lexicalGlobalObject)
        , m_globalObject(globalObject)
        , m_isDOMGlobalObject(globalObject->inherits<JSDOMGlobalObject>())
        , m_canCreateDOMObject(m_isDOMGlobalObject && !globalObject->inherits<JSIDBSerializationGlobalObject>())
        , m_ptr(buffer.data())
        , m_end(buffer.data() + buffer.size())
        , m_majorVersion(0xFFFFFFFF)
        , m_minorVersion(0xFFFFFFFF)
        , m_messagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_blobURLs(blobURLs)
        , m_blobFilePaths(blobFilePaths)
        , m_sharedBuffers(sharedBuffers)
        , m_detachedImageBitmaps(WTFMove(detachedImageBitmaps))
        , m_imageBitmaps(m_detachedImageBitmaps.size())
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
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , m_detachedMediaSourceHandles(WTFMove(detachedMediaSourceHandles))
        , m_mediaSourceHandles(m_detachedMediaSourceHandles.size())
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
#if ENABLE(MEDIA_STREAM)
        , m_serializedMediaStreamTracks(WTFMove(serializedMediaStreamTracks))
        , m_mediaStreamTracks(m_serializedMediaStreamTracks.size())
#endif
    {
        unsigned version;
        if (read(version)) {
            m_majorVersion = majorVersionFor(version);
            m_minorVersion = minorVersionFor(version);
        }
    }

    enum class VisitNamedMemberResult : uint8_t { Error, Break, Start, Unknown };

    template<WalkerState endState>
    ALWAYS_INLINE VisitNamedMemberResult startVisitNamedMember(MarkedVector<JSObject*, 32>& outputObjectStack, Vector<Identifier, 16>& propertyNameStack, Vector<WalkerState, 16>& stateStack, JSValue& outValue)
    {
        static_assert(endState == ArrayEndVisitNamedMember || endState == ObjectEndVisitNamedMember);
        VM& vm = m_lexicalGlobalObject->vm();
        CachedStringRef cachedString;
        bool wasTerminator = false;
        if (!readStringData(cachedString, wasTerminator, ShouldAtomize::Yes)) {
            if (!wasTerminator) {
                SERIALIZE_TRACE("FAIL deserialize");
                return VisitNamedMemberResult::Error;
            }

            JSObject* outObject = outputObjectStack.last();
            outValue = outObject;
            outputObjectStack.removeLast();
            return VisitNamedMemberResult::Break;
        }

        Identifier identifier = Identifier::fromString(vm, cachedString->string());
        if constexpr (endState == ArrayEndVisitNamedMember)
            RELEASE_ASSERT(identifier != vm.propertyNames->length);

        if (JSValue terminal = readTerminal()) {
            putProperty(outputObjectStack.last(), identifier, terminal);
            return VisitNamedMemberResult::Start;
        }

        stateStack.append(endState);
        propertyNameStack.append(identifier);
        return VisitNamedMemberResult::Unknown;
    }

    ALWAYS_INLINE void objectEndVisitNamedMember(MarkedVector<JSObject*, 32>& outputObjectStack, Vector<Identifier, 16>& propertyNameStack, JSValue& outValue)
    {
        putProperty(outputObjectStack.last(), propertyNameStack.last(), outValue);
        propertyNameStack.removeLast();
    }

    DeserializationResult deserialize();

    Vector<std::optional<DetachedImageBitmap>> takeDetachedImageBitmaps() { return std::exchange(m_detachedImageBitmaps, { }); }
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<std::unique_ptr<DetachedOffscreenCanvas>> takeDetachedOffscreenCanvases() { return std::exchange(m_detachedOffscreenCanvases, { }); }
#endif
#if ENABLE(WEB_RTC)
    Vector<std::unique_ptr<DetachedRTCDataChannel>> takeDetachedRTCDataChannels() { return std::exchange(m_detachedRTCDataChannels, { }); }
#endif
#if ENABLE(WEB_CODECS)
    Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>> takeSerializedVideoChunks() { return std::exchange(m_serializedVideoChunks, { }); }
    Vector<WebCodecsVideoFrameData> takeSerializedVideoFrames() { return std::exchange(m_serializedVideoFrames, { }); }
    Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>> takeSerializedAudioChunks() { return std::exchange(m_serializedAudioChunks, { }); }
    Vector<WebCodecsAudioInternalData> takeSerializedAudioData() { return std::exchange(m_serializedAudioData, { }); }
#endif
#if ENABLE(MEDIA_STREAM)
    Vector<std::unique_ptr<MediaStreamTrackDataHolder>> takeSerializedMediaStreamTracks() { return std::exchange(m_serializedMediaStreamTracks, { }); }
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<RefPtr<DetachedMediaSourceHandle>> takeDetachedMediaSourceHandles() { return std::exchange(m_detachedMediaSourceHandles, { }); }
#endif

    bool isValid() const
    {
        if (m_majorVersion > CurrentMajorVersion)
            return false;
        if (m_majorVersion == 12)
            return m_minorVersion <= 1;
        return !m_minorVersion;
    }
    bool shouldRetryWithVersionUpgrade()
    {
        if (m_majorVersion == 14 && !m_minorVersion)
            return true;
        if (m_majorVersion == 12 && !m_minorVersion)
            return true;
        return false;
    }
    void upgradeVersion()
    {
        ASSERT(shouldRetryWithVersionUpgrade());
        if (m_majorVersion == 14 && !m_minorVersion) {
            m_majorVersion = 15;
            return;
        }
        if (m_majorVersion == 12 && !m_minorVersion)
            m_minorVersion = 1;
    }

    template<SerializationTag tag>
    inline void addToObjectPool(JSValue object)
    {
        static_assert(canBeAddedToObjectPool(tag));
        m_objectPool.appendWithCrashOnOverflow(object);
        appendObjectPoolTag(tag);
    }

    template <typename T> bool readLittleEndian(T& value)
    {
        if (m_failed || !readLittleEndian(m_ptr, m_end, value)) {
            SERIALIZE_TRACE("FAIL deserialize");
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
        if (m_majorVersion >= 14 || forceReadingAs8Bit == ForceReadingAs8Bit::Yes) {
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
                str = AtomString({ ptr, length });
            else
                str = String({ ptr, length });
            ptr += length;
            return true;
        }

        unsigned size = length * sizeof(UChar);
        if ((end - ptr) < static_cast<int>(size))
            return false;

#if ASSUME_LITTLE_ENDIAN
        if (shouldAtomize == ShouldAtomize::Yes)
            str = AtomString({ reinterpret_cast<const UChar*>(ptr), length });
        else
            str = String({ reinterpret_cast<const UChar*>(ptr), length });
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
                SERIALIZE_TRACE("FAIL deserialize");
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
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return false;
        }
        m_constantPool.append(WTFMove(str));
        cachedString = CachedStringRef(&m_constantPool, m_constantPool.size() - 1);
        return true;
    }

    SerializationTag readTag()
    {
        if (m_ptr >= m_end) {
            SERIALIZE_TRACE("FAIL deserialize");
            return ErrorTag;
        }
        auto tag = static_cast<SerializationTag>(*m_ptr++);
        SERIALIZE_TRACE("deserialize ", tag);
        return tag;
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
        if (m_majorVersion > 6) {
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
        arrayBuffer = ArrayBuffer::tryCreate({ m_ptr, static_cast<size_t>(length) });
        if (!arrayBuffer)
            return false;
        m_ptr += length;
        return true;
    }

    bool readArrayBuffer(RefPtr<ArrayBuffer>& arrayBuffer)
    {
        if (m_majorVersion < 10)
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
        if (!isSafeToRecurse())
            return false;
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

        if (!ArrayBufferView::verifySubRangeLength(arrayBuffer->byteLength(), byteOffset, length.value_or(0), 1))
            return false;

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
        case Float16ArrayTag:
            return makeArrayBufferView(Float16Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length).get());
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
        if (!isSafeToRecurse())
            return false;
        if (m_majorVersion < 10)
            return readArrayBufferViewImpl<uint32_t>(vm, arrayBufferView);
        return readArrayBufferViewImpl<uint64_t>(vm, arrayBufferView);
    }

    bool read(Vector<uint8_t>& result)
    {
        ASSERT(result.isEmpty());
        uint32_t size;
        if (!read(size))
            return false;
        if (static_cast<uint32_t>(m_end - m_ptr) < size)
            return false;
        result.append(std::span { m_ptr, size });
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
        case DestinationColorSpaceLinearSRGBTag:
            destinationColorSpace = DestinationColorSpace::LinearSRGB();
            return true;
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
        if (!indexSuccessfullyRead || index >= m_detachedImageBitmaps.size()) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        if (!m_imageBitmaps[index] && m_detachedImageBitmaps.at(index))
            m_imageBitmaps[index] = ImageBitmap::create(*executionContext(m_lexicalGlobalObject), WTFMove(*m_detachedImageBitmaps.at(index)));

        auto bitmap = m_imageBitmaps[index].get();
        return getJSValue(bitmap);
    }

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    JSValue readOffscreenCanvas()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_detachedOffscreenCanvases.size()) {
            SERIALIZE_TRACE("FAIL deserialize");
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
            SERIALIZE_TRACE("FAIL deserialize");
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
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
        CachedStringRef certificate;
        if (!readStringData(certificate)) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
        CachedStringRef origin;
        if (!readStringData(origin)) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
        CachedStringRef keyedMaterial;
        if (!readStringData(keyedMaterial)) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
        unsigned size = 0;
        if (!read(size))
            return JSValue();

        Vector<RTCCertificate::DtlsFingerprint> fingerprints;
        if (!fingerprints.tryReserveInitialCapacity(size))
            return JSValue();
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
            SERIALIZE_TRACE("FAIL deserialize");
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
            SERIALIZE_TRACE("FAIL deserialize");
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
            SERIALIZE_TRACE("FAIL deserialize");
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
            SERIALIZE_TRACE("FAIL deserialize");
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
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        if (!m_audioData[index])
            m_audioData[index] = WebCodecsAudioData::create(*executionContext(m_lexicalGlobalObject), WTFMove(m_serializedAudioData.at(index)));

        return getJSValue(m_audioData[index].get());
    }
#endif

#if ENABLE(MEDIA_STREAM)
    JSValue readMediaStreamTrack()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_serializedMediaStreamTracks.size()) {
            fail();
            return JSValue();
        }

        if (!m_mediaStreamTracks[index])
            m_mediaStreamTracks[index] = MediaStreamTrack::create(*executionContext(m_lexicalGlobalObject), makeUniqueRefFromNonNullUniquePtr(std::exchange(m_serializedMediaStreamTracks.at(index), { })));

        return getJSValue(m_mediaStreamTracks[index].get());
    }
#endif

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    JSValue readMediaSourceHandle()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_detachedMediaSourceHandles.size()) {
            fail();
            return JSValue();
        }

        if (!m_mediaSourceHandles[index])
            m_mediaSourceHandles[index] = MediaSourceHandle::create(std::exchange(m_detachedMediaSourceHandles.at(index), { }).releaseNonNull());

        return getJSValue(m_mediaSourceHandles[index].get());
    }
#endif

    JSValue readImageBitmap()
    {
        uint8_t rawFlags;
        int32_t logicalWidth;
        int32_t logicalHeight;
        double resolutionScale;
        auto colorSpace = DestinationColorSpace::SRGB();
        RefPtr<ArrayBuffer> arrayBuffer;

        if (!read(rawFlags) || !read(logicalWidth) || !read(logicalHeight) || !read(resolutionScale) || (m_majorVersion > 8 && !read(colorSpace)) || !readArrayBufferImpl<uint32_t>(arrayBuffer)) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        auto flags = OptionSet<ImageBitmapSerializationFlags>::fromRaw(rawFlags);
        if (!flags.contains(ImageBitmapSerializationFlags::OriginClean)) {
            fail();
            return JSValue();
        }
        auto logicalSize = IntSize(logicalWidth, logicalHeight);
        auto imageDataSize = logicalSize;
        imageDataSize.scale(resolutionScale);

        auto buffer = ImageBitmap::createImageBuffer(*executionContext(m_lexicalGlobalObject), logicalSize, RenderingMode::Unaccelerated, colorSpace, resolutionScale);
        if (!buffer) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, colorSpace };
        auto pixelBuffer = ByteArrayPixelBuffer::tryCreate(format, imageDataSize, arrayBuffer.releaseNonNull());
        if (!pixelBuffer) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        buffer->putPixelBuffer(*pixelBuffer, { IntPoint::zero(), logicalSize });
        const bool originClean = true;
        auto bitmap = ImageBitmap::create(buffer.releaseNonNull(), originClean, flags.contains(ImageBitmapSerializationFlags::PremultiplyAlpha), flags.contains(ImageBitmapSerializationFlags::ForciblyPremultiplyAlpha));
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
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
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
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            bigInt->setDigit(0, digit64);
            bigInt->setSign(sign);
            bigInt = bigInt->tryRightTrim(m_lexicalGlobalObject->vm());
            if (!bigInt) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return tryConvertToBigInt32(bigInt);
        }
#endif
        JSBigInt* bigInt = nullptr;
        if constexpr (sizeof(JSBigInt::Digit) == sizeof(uint64_t)) {
            bigInt = JSBigInt::tryCreateWithLength(m_lexicalGlobalObject->vm(), numberOfUint64Elements);
            if (!bigInt) {
                SERIALIZE_TRACE("FAIL deserialize");
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
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            bigInt = JSBigInt::tryCreateWithLength(m_lexicalGlobalObject->vm(), actualBigIntLength.value());
            if (!bigInt) {
                SERIALIZE_TRACE("FAIL deserialize");
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
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
        return tryConvertToBigInt32(bigInt);
    }

    JSValue readTerminal()
    {
        if (!isSafeToRecurse())
            return JSValue();
        SerializationTag tag = readTag();
        if (!isTypeExposedToGlobalObject(*m_globalObject, tag)) {
            SERIALIZE_TRACE("FAIL deserialize");
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
            addToObjectPool<FalseObjectTag>(obj);
            return obj;
        }
        case TrueObjectTag: {
            BooleanObject* obj = BooleanObject::create(m_lexicalGlobalObject->vm(), m_globalObject->booleanObjectStructure());
            obj->setInternalValue(m_lexicalGlobalObject->vm(), jsBoolean(true));
            addToObjectPool<TrueObjectTag>(obj);
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
            addToObjectPool<NumberObjectTag>(obj);
            return obj;
        }
        case BigIntObjectTag: {
            JSValue bigInt = readBigInt();
            if (!bigInt)
                return JSValue();
            ASSERT(bigInt.isBigInt());
            BigIntObject* obj = BigIntObject::create(m_lexicalGlobalObject->vm(), m_globalObject, bigInt);
            addToObjectPool<BigIntObjectTag>(obj);
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
                    SERIALIZE_TRACE("FAIL deserialize");
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
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            auto bufferStart = m_ptr;
            m_ptr += length;

            auto resultColorSpace = PredefinedColorSpace::SRGB;
            if (m_majorVersion > 7) {
                if (!read(resultColorSpace))
                    return JSValue();
            }

            if (length) {
                auto area = IntSize(width, height).area<RecordOverflow>() * 4;
                if (area.hasOverflowed() || area.value() != length) {
                    SERIALIZE_TRACE("FAIL deserialize");
                    fail();
                    return JSValue();
                }
            }

            if (!m_isDOMGlobalObject)
                return jsNull();

            auto result = ImageData::createUninitialized(width, height, resultColorSpace);
            if (result.hasException()) {
                SERIALIZE_TRACE("FAIL deserialize");
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
            if (m_majorVersion >= 11 && !read(memoryCost))
                return JSValue();
            if (!m_canCreateDOMObject)
                return jsNull();
            return getJSValue(Blob::deserialize(executionContext(m_lexicalGlobalObject), URL { url->string() }, type->string(), size, memoryCost, blobFilePathForBlobURL(url->string())).get());
        }
        case StringTag: {
            CachedStringRef cachedString;
            if (!readStringData(cachedString))
                return JSValue();
            return cachedString->jsString(*this);
        }
        case EmptyStringTag:
            return jsEmptyString(m_lexicalGlobalObject->vm());
        case StringObjectTag: {
            CachedStringRef cachedString;
            if (!readStringData(cachedString))
                return JSValue();
            StringObject* obj = constructString(m_lexicalGlobalObject->vm(), m_globalObject, cachedString->jsString(*this));
            addToObjectPool<StringObjectTag>(obj);
            return obj;
        }
        case EmptyStringObjectTag: {
            VM& vm = m_lexicalGlobalObject->vm();
            StringObject* obj = constructString(vm, m_globalObject, jsEmptyString(vm));
            addToObjectPool<EmptyStringObjectTag>(obj);
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
            if (!reFlags.has_value())
                return JSValue();
            VM& vm = m_lexicalGlobalObject->vm();
            RegExp* regExp = RegExp::create(vm, pattern->string(), reFlags.value());
            return RegExpObject::create(vm, m_globalObject->regExpStructure(), regExp);
        }
        case ErrorInstanceTag: {
            SerializableErrorType serializedErrorType;
            if (!read(serializedErrorType)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String message;
            if (!readNullableString(message)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t line;
            if (!read(line)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t column;
            if (!read(column)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String sourceURL;
            if (!readNullableString(sourceURL)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String stackString;
            if (!readNullableString(stackString)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return ErrorInstance::create(m_lexicalGlobalObject, WTFMove(message), toErrorType(serializedErrorType), { line, column }, WTFMove(sourceURL), WTFMove(stackString));
        }
        case ObjectReferenceTag: {
            auto index = readConstantPoolIndex(m_objectPool);
            if (!index) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return m_objectPool.at(*index);
        }
        case MessagePortReferenceTag: {
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || index >= m_messagePorts.size()) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return getJSValue(m_messagePorts[index].get());
        }
        case InMemoryMessagePortTag: {
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || index >= m_inMemoryMessagePorts.size()) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return getJSValue(m_inMemoryMessagePorts[index].get());
        }
#if ENABLE(WEBASSEMBLY)
        case WasmModuleTag: {
            if (m_majorVersion >= 12) {
                // https://webassembly.github.io/spec/web-api/index.html#serialization
                CachedStringRef agentClusterID;
                bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
                if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != agentClusterIDFromGlobalObject(*m_globalObject)) {
                    SERIALIZE_TRACE("FAIL deserialize");
                    fail();
                    return JSValue();
                }
            }
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_wasmModules || index >= m_wasmModules->size()) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return JSC::JSWebAssemblyModule::create(m_lexicalGlobalObject->vm(), m_globalObject->webAssemblyModuleStructure(), Ref { *m_wasmModules->at(index) });
        }
        case WasmMemoryTag: {
            if (m_majorVersion >= 12) {
                CachedStringRef agentClusterID;
                bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
                if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != agentClusterIDFromGlobalObject(*m_globalObject)) {
                    SERIALIZE_TRACE("FAIL deserialize");
                    fail();
                    return JSValue();
                }
            }
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_wasmMemoryHandles || index >= m_wasmMemoryHandles->size() || !JSC::Options::useSharedArrayBuffer()) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }

            auto& vm = m_lexicalGlobalObject->vm();
            auto scope = DECLARE_THROW_SCOPE(vm);
            JSWebAssemblyMemory* result = JSC::JSWebAssemblyMemory::tryCreate(m_lexicalGlobalObject, vm, m_globalObject->webAssemblyMemoryStructure());
            // Since we are cloning a JSWebAssemblyMemory, it's impossible for that
            // module to not have been a valid module. Therefore, tryCreate should
            // not throw.
            scope.releaseAssertNoException();

            RefPtr<Wasm::Memory> memory;
            auto handler = [&vm, result] (Wasm::Memory::GrowSuccess, PageCount oldPageCount, PageCount newPageCount) { result->growSuccessCallback(vm, oldPageCount, newPageCount); };
            if (RefPtr<SharedArrayBufferContents> contents = m_wasmMemoryHandles->at(index)) {
                if (!contents->memoryHandle()) {
                    SERIALIZE_TRACE("FAIL deserialize");
                    fail();
                    return JSValue();
                }
                memory = Wasm::Memory::create(vm, contents.releaseNonNull(), WTFMove(handler));
            } else {
                // zero size & max-size.
                memory = Wasm::Memory::createZeroSized(vm, JSC::MemorySharingMode::Shared, WTFMove(handler));
            }

            result->adopt(memory.releaseNonNull());
            return result;
        }
#endif
        case ArrayBufferTag: {
            RefPtr<ArrayBuffer> arrayBuffer;
            if (!readArrayBuffer(arrayBuffer)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            Structure* structure = m_globalObject->arrayBufferStructure(arrayBuffer->sharingMode());
            // A crazy RuntimeFlags mismatch could mean that we are not equipped to handle shared
            // array buffers while the sender is. In that case, we would see a null structure here.
            if (UNLIKELY(!structure)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            JSValue result = JSArrayBuffer::create(m_lexicalGlobalObject->vm(), structure, WTFMove(arrayBuffer));
            addToObjectPool<ArrayBufferTag>(result);
            return result;
        }
        case ResizableArrayBufferTag: {
            RefPtr<ArrayBuffer> arrayBuffer;
            if (!readResizableNonSharedArrayBuffer(arrayBuffer)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            Structure* structure = m_globalObject->arrayBufferStructure(arrayBuffer->sharingMode());
            // A crazy RuntimeFlags mismatch could mean that we are not equipped to handle shared
            // array buffers while the sender is. In that case, we would see a null structure here.
            if (UNLIKELY(!structure)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            JSValue result = JSArrayBuffer::create(m_lexicalGlobalObject->vm(), structure, WTFMove(arrayBuffer));
            addToObjectPool<ResizableArrayBufferTag>(result);
            return result;
        }
        case ArrayBufferTransferTag: {
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || index >= m_arrayBuffers.size()) {
                SERIALIZE_TRACE("FAIL deserialize ArrayBufferTransferTag: indexSuccessfullyRead ", indexSuccessfullyRead, " index ", index, " m_arrayBuffers.size() ", m_arrayBuffers.size());
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
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            
            RELEASE_ASSERT(m_sharedBuffers->at(index));
            ArrayBufferContents arrayBufferContents;
            m_sharedBuffers->at(index).shareWith(arrayBufferContents);
            auto buffer = ArrayBuffer::create(WTFMove(arrayBufferContents));
            JSValue result = getJSValue(buffer.get());
            addToObjectPool<SharedArrayBufferTag>(result);
            return result;
        }
        case ArrayBufferViewTag: {
            JSValue arrayBufferView;
            if (!readArrayBufferView(m_lexicalGlobalObject->vm(), arrayBufferView)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            addToObjectPool<ArrayBufferViewTag>(arrayBufferView);
            return arrayBufferView;
        }
        case CryptoKeyTag: {
            Vector<uint8_t> wrappedKey;
            if (!read(wrappedKey)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            auto serializedKey = unwrapCryptoKey(m_lexicalGlobalObject, wrappedKey);
            if (!serializedKey) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }

            JSValue cryptoKey;
            Vector<Ref<MessagePort>> dummyMessagePorts;
            CloneDeserializer rawKeyDeserializer(m_lexicalGlobalObject, m_globalObject, dummyMessagePorts, nullptr, { }, *serializedKey);
            if (!rawKeyDeserializer.readCryptoKey(cryptoKey)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return cryptoKey;
        }
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
#if ENABLE(MEDIA_STREAM)
        case MediaStreamTrackTag:
            return readMediaStreamTrack();
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        case MediaSourceHandleTransferTag:
            return readMediaSourceHandle();
#endif
        case DOMExceptionTag:
            return readDOMException();

        default:
            SERIALIZE_TRACE("push back ", tag);
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
    unsigned m_majorVersion;
    unsigned m_minorVersion;
    Vector<CachedString> m_constantPool;
    Vector<Ref<ImageData>> m_imageDataPool;
    const Vector<Ref<MessagePort>>& m_messagePorts;
    ArrayBufferContentsArray* m_arrayBufferContents;
    Vector<RefPtr<JSC::ArrayBuffer>> m_arrayBuffers;
    Vector<String> m_blobURLs;
    Vector<String> m_blobFilePaths;
    ArrayBufferContentsArray* m_sharedBuffers;
    Vector<std::optional<DetachedImageBitmap>> m_detachedImageBitmaps;
    Vector<RefPtr<ImageBitmap>> m_imageBitmaps;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<std::unique_ptr<DetachedOffscreenCanvas>> m_detachedOffscreenCanvases;
    Vector<RefPtr<OffscreenCanvas>> m_offscreenCanvases;
    const Vector<RefPtr<OffscreenCanvas>>& m_inMemoryOffscreenCanvases;
#endif
    const Vector<Ref<MessagePort>>& m_inMemoryMessagePorts;
#if ENABLE(WEB_RTC)
    Vector<std::unique_ptr<DetachedRTCDataChannel>> m_detachedRTCDataChannels;
    Vector<RefPtr<RTCDataChannel>> m_rtcDataChannels;
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<RefPtr<DetachedMediaSourceHandle>> m_detachedMediaSourceHandles;
    Vector<RefPtr<MediaSourceHandle>> m_mediaSourceHandles;
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
#if ENABLE(MEDIA_STREAM)
    Vector<std::unique_ptr<MediaStreamTrackDataHolder>> m_serializedMediaStreamTracks;
    Vector<RefPtr<MediaStreamTrack>> m_mediaStreamTracks;
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

#if ASSERT_ENABLED
    friend void validateSerializedResult(CloneSerializer&, SerializationReturnCode, Vector<uint8_t>&, JSGlobalObject*, Vector<Ref<MessagePort>>&, ArrayBufferContentsArray&, ArrayBufferContentsArray&, Vector<Ref<MessagePort>>&);
#endif
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
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            JSArray* outArray = constructEmptyArray(m_globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), length);
            if (UNLIKELY(scope.exception())) {
                SERIALIZE_TRACE("FAIL deserialize");
                goto error;
            }
            addToObjectPool<ArrayTag>(outArray);
            outputObjectStack.append(outArray);
        }
        arrayStartVisitIndexedMember:
        FALLTHROUGH;
        case ArrayStartVisitIndexedMember: {
            uint32_t index;
            if (!read(index)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }

            if (m_majorVersion >= 15 || (m_majorVersion == 12 && m_minorVersion == 1)) {
                if (index == TerminatorTag) {
                    // We reached the end of the indexed properties section.
                    if (!read(index)) {
                        SERIALIZE_TRACE("FAIL deserialize");
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
                        goto arrayStartVisitNamedMember;
                }
            } else {
                if (index == TerminatorTag) {
                    JSObject* outArray = outputObjectStack.last();
                    outValue = outArray;
                    outputObjectStack.removeLast();
                    break;
                } else if (index == NonIndexPropertiesTag)
                    goto arrayStartVisitNamedMember;
            }

            if (JSValue terminal = readTerminal()) {
                putProperty(outputObjectStack.last(), index, terminal);
                goto arrayStartVisitIndexedMember;
            }
            if (m_failed)
                goto error;
            indexStack.append(index);
            stateStack.append(ArrayEndVisitIndexedMember);
            goto stateUnknown;
        }
        case ArrayEndVisitIndexedMember: {
            JSObject* outArray = outputObjectStack.last();
            putProperty(outArray, indexStack.last(), outValue);
            indexStack.removeLast();
            goto arrayStartVisitIndexedMember;
        }
        arrayStartVisitNamedMember:
        case ArrayStartVisitNamedMember: {
            switch (startVisitNamedMember<ArrayEndVisitNamedMember>(outputObjectStack, propertyNameStack, stateStack, outValue)) {
            case VisitNamedMemberResult::Error:
                goto error;
            case VisitNamedMemberResult::Break:
                break;
            case VisitNamedMemberResult::Start:
                goto arrayStartVisitNamedMember;
            case VisitNamedMemberResult::Unknown:
                goto stateUnknown;
            }
            break;
        }
        case ArrayEndVisitNamedMember: {
            objectEndVisitNamedMember(outputObjectStack, propertyNameStack, outValue);
            goto arrayStartVisitNamedMember;
        }
        objectStartState:
        case ObjectStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion)
                return std::make_pair(JSValue(), SerializationReturnCode::StackOverflowError);
            JSObject* outObject = constructEmptyObject(m_lexicalGlobalObject, m_globalObject->objectPrototype());
            addToObjectPool<ObjectTag>(outObject);
            outputObjectStack.append(outObject);
        }
        startVisitNamedMember:
        FALLTHROUGH;
        case ObjectStartVisitNamedMember: {
            switch (startVisitNamedMember<ObjectEndVisitNamedMember>(outputObjectStack, propertyNameStack, stateStack, outValue)) {
            case VisitNamedMemberResult::Error:
                goto error;
            case VisitNamedMemberResult::Break:
                break;
            case VisitNamedMemberResult::Start:
                goto startVisitNamedMember;
            case VisitNamedMemberResult::Unknown:
                goto stateUnknown;
            }
            break;
        }
        case ObjectEndVisitNamedMember: {
            objectEndVisitNamedMember(outputObjectStack, propertyNameStack, outValue);
            goto startVisitNamedMember;
        }
        mapStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion) {
                SERIALIZE_TRACE("FAIL deserialize");
                return std::make_pair(JSValue(), SerializationReturnCode::StackOverflowError);
            }
            JSMap* map = JSMap::create(m_lexicalGlobalObject->vm(), m_globalObject->mapStructure());
            addToObjectPool<MapObjectTag>(map);
            outputObjectStack.append(map);
            mapStack.append(map);
            goto mapDataStartVisitEntry;
        }
        mapDataStartVisitEntry:
        case MapDataStartVisitEntry: {
            if (consumeCollectionDataTerminationIfPossible<NonMapPropertiesTag>()) {
                mapStack.removeLast();
                goto startVisitNamedMember;
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

        setStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion) {
                SERIALIZE_TRACE("FAIL deserialize");
                return std::make_pair(JSValue(), SerializationReturnCode::StackOverflowError);
            }
            JSSet* set = JSSet::create(m_lexicalGlobalObject->vm(), m_globalObject->setStructure());
            addToObjectPool<SetObjectTag>(set);
            outputObjectStack.append(set);
            setStack.append(set);
            goto setDataStartVisitEntry;
        }
        setDataStartVisitEntry:
        case SetDataStartVisitEntry: {
            if (consumeCollectionDataTerminationIfPossible<NonSetPropertiesTag>()) {
                setStack.removeLast();
                goto startVisitNamedMember;
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
                goto mapStartState;
            if (tag == SetObjectTag)
                goto setStartState;
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

#if ASSERT_ENABLED
void validateSerializedResult(CloneSerializer& serializer, SerializationReturnCode code, Vector<uint8_t>& result, JSGlobalObject* lexicalGlobalObject, Vector<Ref<MessagePort>>& messagePorts, ArrayBufferContentsArray& arrayBufferContentsArray, ArrayBufferContentsArray& sharedBuffers, Vector<Ref<MessagePort>>& inMemoryMessagePorts)
{
    if (!JSC::Options::validateSerializedValue())
        return;
    if (serializer.didSeeComplexCases())
        return;
    if (code != SerializationReturnCode::SuccessfullyCompleted)
        return;

    SERIALIZE_TRACE("validation start");

    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* globalObject = lexicalGlobalObject;
    Vector<String> blobURLs;
    Vector<String> blobFilePaths;
    Vector<std::optional<WebCore::DetachedImageBitmap>> detachedImageBitmaps;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<std::unique_ptr<DetachedOffscreenCanvas>> detachedOffscreenCanvases;
    Vector<RefPtr<OffscreenCanvas>> inMemoryOffscreenCanvases;
#endif
#if ENABLE(WEB_RTC)
    Vector<std::unique_ptr<DetachedRTCDataChannel>> detachedRTCDataChannels;
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<RefPtr<DetachedMediaSourceHandle>> detachedMediaSourceHandles;
#endif
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray* wasmModules = nullptr;
    WasmMemoryHandleArray* wasmMemoryHandles = nullptr;
#endif
#if ENABLE(WEB_CODECS)
    Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>> serializedVideoChunks;
    Vector<WebCodecsVideoFrameData> serializedVideoFrames;
    Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>> serializedAudioChunks;
    Vector<WebCodecsAudioInternalData> serializedAudioData;
#endif

    CloneDeserializer deserializer(lexicalGlobalObject, globalObject, messagePorts, &arrayBufferContentsArray, result, blobURLs, blobFilePaths, &sharedBuffers, WTFMove(detachedImageBitmaps)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , WTFMove(detachedOffscreenCanvases)
        , inMemoryOffscreenCanvases
#endif
        , inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , WTFMove(detachedRTCDataChannels)
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , WTFMove(detachedMediaSourceHandles)
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
    RELEASE_ASSERT(deserializer.isValid());
    auto deserializationResult = deserializer.deserialize();
    if (scope.exception()) {
        scope.clearException();
        SERIALIZE_TRACE("validation abort due to exception");
        return;
    }

    if (deserializationResult.second != SerializationReturnCode::SuccessfullyCompleted) {
        SERIALIZE_TRACE("validation abort due to deserialization failure");
        return;
    }

    unsigned numChecks = 0;
    unsigned numMismatches = 0;
#define VALIDATE_EQ(a, b, ...) do { \
        if ((a) != (b)) \
            numMismatches++; \
    } while (false)

    auto& serializerTags = serializer.objectPoolTags();
    auto& deserializerTags = deserializer.objectPoolTags();
    VALIDATE_EQ(serializerTags.size(), deserializerTags.size(), "");
    size_t commonMinSize = std::min(serializerTags.size(), deserializerTags.size());
    size_t maxSize = std::max(serializerTags.size(), deserializerTags.size());
    for (size_t i = 0; i < commonMinSize; ++i)
        VALIDATE_EQ(serializerTags[i], deserializerTags[i], " at i ", i);
    numChecks = 1 + maxSize;
    if (commonMinSize != maxSize)
        numMismatches += maxSize - commonMinSize;

    if (numMismatches) {
        dataLogLn("\n\nERROR: serialization / deserialization mismatch:");
        dataLogLn("\n");
        dataLogLn("    # of serializerTags = ", serializerTags.size());
        dataLogLn("    # of deserializerTags = ", deserializerTags.size());
        dataLogLn("    FOUND ", numMismatches, " mismatches out of ", numChecks, " checks");

        for (size_t i = 0; i < commonMinSize; ++i)
            dataLogLn("      serializerTags[", i, "] (", serializerTags[i], ") deserializerTags[", i, "] (", deserializerTags[i], ")", serializerTags[i] == deserializerTags[i] ? "" : " DIFFERENT");
        for (size_t i = commonMinSize; i < serializerTags.size(); ++i)
            dataLogLn("      serializerTags[", i, "] (", serializerTags[i], ") DIFFERENT");
        for (size_t i = commonMinSize; i < deserializerTags.size(); ++i)
            dataLogLn("      deserializerTags[", i, "] (", deserializerTags[i], ") DIFFERENT");
        dataLogLn("\n\n");
    }
#undef VALIDATE_EQ
    RELEASE_ASSERT(!numMismatches);

    SERIALIZE_TRACE("validation done");
}
#endif // ASSERT_ENABLED

SerializedScriptValue::~SerializedScriptValue() = default;

SerializedScriptValue::SerializedScriptValue(Vector<uint8_t>&& buffer, std::unique_ptr<ArrayBufferContentsArray>&& arrayBufferContentsArray
#if ENABLE(WEB_RTC)
    , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    , Vector<RefPtr<DetachedMediaSourceHandle>>&& detachedMediaSourceHandles
#endif
#if ENABLE(WEB_CODECS)
    , Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks
    , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames
    , Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks
    , Vector<WebCodecsAudioInternalData>&& serializedAudioData
#endif
#if ENABLE(MEDIA_STREAM)
    , Vector<std::unique_ptr<MediaStreamTrackDataHolder>>&& serializedMediaStreamTracks
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
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , .detachedMediaSourceHandles = WTFMove(detachedMediaSourceHandles)
#endif
#if ENABLE(MEDIA_STREAM)
        , .serializedMediaStreamTracks = WTFMove(serializedMediaStreamTracks)
#endif
    }
{
    m_internals.memoryCost = computeMemoryCost();
}

SerializedScriptValue::SerializedScriptValue(Vector<uint8_t>&& buffer, Vector<URLKeepingBlobAlive>&& blobHandles, std::unique_ptr<ArrayBufferContentsArray> arrayBufferContentsArray, std::unique_ptr<ArrayBufferContentsArray> sharedBufferContentsArray, Vector<std::optional<DetachedImageBitmap>>&& detachedImageBitmaps
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases
        , Vector<RefPtr<OffscreenCanvas>>&& inMemoryOffscreenCanvases
#endif
        , Vector<Ref<MessagePort>>&& inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , Vector<RefPtr<DetachedMediaSourceHandle>>&& detachedMediaSourceHandles
#endif
#if ENABLE(WEBASSEMBLY)
        , WasmModuleArray&& wasmModulesArray
        , std::unique_ptr<WasmMemoryHandleArray> wasmMemoryHandlesArray
#endif
#if ENABLE(WEB_CODECS)
        , Vector<RefPtr<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks
        , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames
        , Vector<RefPtr<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks
        , Vector<WebCodecsAudioInternalData>&& serializedAudioData
#endif
#if ENABLE(MEDIA_STREAM)
        , Vector<std::unique_ptr<MediaStreamTrackDataHolder>>&& serializedMediaStreamTracks
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
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , .detachedMediaSourceHandles = WTFMove(detachedMediaSourceHandles)
#endif
#if ENABLE(MEDIA_STREAM)
        , .serializedMediaStreamTracks = WTFMove(serializedMediaStreamTracks)
#endif
        , .sharedBufferContentsArray = WTFMove(sharedBufferContentsArray)
        , .detachedImageBitmaps = WTFMove(detachedImageBitmaps)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , .detachedOffscreenCanvases = WTFMove(detachedOffscreenCanvases)
        , .inMemoryOffscreenCanvases = WTFMove(inMemoryOffscreenCanvases)
#endif
        , .inMemoryMessagePorts = WTFMove(inMemoryMessagePorts)
#if ENABLE(WEBASSEMBLY)
        , .wasmModulesArray = wasmModulesArray.isEmpty() ? nullptr : makeUnique<WasmModuleArray>(WTFMove(wasmModulesArray))
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

    for (auto& detachedImageBitmap : m_internals.detachedImageBitmaps) {
        if (detachedImageBitmap)
            cost += detachedImageBitmap->memoryCost();
    }

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

#if ENABLE(MEDIA_STREAM)
static bool canDetachMediaStreamTracks(const Vector<Ref<MediaStreamTrack>>& tracks)
{
    HashSet<MediaStreamTrack*> visited;
    for (auto& track : tracks) {
        if (!visited.add(track.ptr()))
            return false;
    }
    return true;
}
#endif

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
static bool canDetachMediaSourceHandles(const Vector<Ref<MediaSourceHandle>>& handles)
{
    HashSet<MediaSourceHandle*> visited;
    for (auto& handle : handles) {
        if (!handle->canDetach())
            return false;
        // Check the return value of add, we should not encounter duplicates.
        if (!visited.add(handle.ptr()))
            return false;
    }
    return true;
}
#endif

RefPtr<SerializedScriptValue> SerializedScriptValue::create(JSC::JSGlobalObject& globalObject, JSC::JSValue value, SerializationForStorage forStorage, SerializationErrorMode throwExceptions, SerializationContext serializationContext)
{
    Vector<Ref<MessagePort>> dummyPorts;
    auto result = create(globalObject, value, { }, dummyPorts, forStorage, throwExceptions, serializationContext);
    if (result.hasException())
        return nullptr;
    return result.releaseReturnValue();
}

ExceptionOr<Ref<SerializedScriptValue>> SerializedScriptValue::create(JSGlobalObject& globalObject, JSValue value, Vector<JSC::Strong<JSC::JSObject>>&& transferList, Vector<Ref<MessagePort>>& messagePorts, SerializationForStorage forStorage, SerializationContext serializationContext)
{
    return create(globalObject, value, WTFMove(transferList), messagePorts, forStorage, SerializationErrorMode::NonThrowing, serializationContext);
}

ExceptionOr<Ref<SerializedScriptValue>> SerializedScriptValue::create(JSGlobalObject& lexicalGlobalObject, JSValue value, Vector<JSC::Strong<JSC::JSObject>>&& transferList, Vector<Ref<MessagePort>>& messagePorts, SerializationForStorage forStorage, SerializationErrorMode throwExceptions, SerializationContext context)
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
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<Ref<MediaSourceHandle>> mediaSourceHandles;
#endif
#if ENABLE(WEB_CODECS)
    Vector<Ref<WebCodecsVideoFrame>> transferredVideoFrames;
    Vector<Ref<WebCodecsAudioData>> transferredAudioData;
#endif
#if ENABLE(MEDIA_STREAM)
    Vector<Ref<MediaStreamTrack>> transferredMediaStreamTracks;
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
            messagePorts.append(*port);
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

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        if (auto handle = JSMediaSourceHandle::toWrapped(vm, transferable.get())) {
            if (handle->isDetached())
                return Exception { ExceptionCode::DataCloneError };
            mediaSourceHandles.append(*handle);
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

#if ENABLE(MEDIA_STREAM)
        if (auto track = JSMediaStreamTrack::toWrapped(vm, transferable.get())) {
            if (track->isDetached())
                return Exception { ExceptionCode::DataCloneError };
            transferredMediaStreamTracks.append(*track);
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
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    if (!canDetachMediaSourceHandles(mediaSourceHandles))
        return Exception { ExceptionCode::DataCloneError };
#endif
#if ENABLE(MEDIA_STREAM)
    if (!canDetachMediaStreamTracks(transferredMediaStreamTracks))
        return Exception { ExceptionCode::DataCloneError };
#endif

    Vector<uint8_t> buffer;
    Vector<URLKeepingBlobAlive> blobHandles;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<RefPtr<OffscreenCanvas>> inMemoryOffscreenCanvases;
#endif
    Vector<Ref<MessagePort>> inMemoryMessagePorts;
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
#if ENABLE(MEDIA_STREAM)
    Vector<RefPtr<MediaStreamTrack>> serializedMediaStreamTracks;
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
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        mediaSourceHandles,
#endif
#if ENABLE(WEB_CODECS)
        serializedVideoChunks,
        serializedVideoFrames,
        serializedAudioChunks,
        serializedAudioData,
#endif
#if ENABLE(MEDIA_STREAM)
        serializedMediaStreamTracks,
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

    auto detachedImageBitmaps = map(WTFMove(imageBitmaps), [](auto&& imageBitmap) -> std::optional<DetachedImageBitmap> {
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

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<RefPtr<DetachedMediaSourceHandle>> detachedMediaSourceHandles;
    for (auto& handle : mediaSourceHandles)
        detachedMediaSourceHandles.append(handle->detach());
#endif

#if ENABLE(WEB_CODECS)
    auto serializedVideoFrameData = map(serializedVideoFrames, [](auto& frame) -> WebCodecsVideoFrameData { return frame->data(); });
    for (auto& videoFrame : transferredVideoFrames)
        videoFrame->close();
    auto serializedAudioInternalData = map(serializedAudioData, [](auto& data) -> WebCodecsAudioInternalData { return data->data(); });
    for (auto& audioData : transferredAudioData)
        audioData->close();
#endif
#if ENABLE(MEDIA_STREAM)
    auto serializedMediaStreamTrackStorages = map(serializedMediaStreamTracks, [](auto& track) -> std::unique_ptr<MediaStreamTrackDataHolder> {
        return track->detach().moveToUniquePtr();
    });
    for (auto& track : transferredMediaStreamTracks)
        track->stopTrack();
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
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
                , WTFMove(detachedMediaSourceHandles)
#endif
#if ENABLE(WEBASSEMBLY)
                , WTFMove(wasmModules)
                , context == SerializationContext::WorkerPostMessage ? makeUnique<WasmMemoryHandleArray>(wasmMemoryHandles) : nullptr
#endif
#if ENABLE(WEB_CODECS)
                , WTFMove(serializedVideoChunks)
                , WTFMove(serializedVideoFrameData)
                , WTFMove(serializedAudioChunks)
                , WTFMove(serializedAudioInternalData)
#endif
#if ENABLE(MEDIA_STREAM)
                , WTFMove(serializedMediaStreamTrackStorages)
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

JSValue SerializedScriptValue::deserialize(JSGlobalObject& lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<Ref<MessagePort>>& messagePorts, SerializationErrorMode throwExceptions, bool* didFail)
{
    Vector<String> dummyBlobs;
    Vector<String> dummyPaths;
    return deserialize(lexicalGlobalObject, globalObject, messagePorts, dummyBlobs, dummyPaths, throwExceptions, didFail);
}

JSValue SerializedScriptValue::deserialize(JSGlobalObject& lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<Ref<MessagePort>>& messagePorts, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths, SerializationErrorMode throwExceptions, bool* didFail)
{
    DeserializationResult result = CloneDeserializer::deserialize(&lexicalGlobalObject, globalObject, messagePorts, WTFMove(m_internals.detachedImageBitmaps)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , WTFMove(m_internals.detachedOffscreenCanvases)
        , m_internals.inMemoryOffscreenCanvases
#endif
        , m_internals.inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , WTFMove(m_internals.detachedRTCDataChannels)
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , WTFMove(m_internals.detachedMediaSourceHandles)
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
#if ENABLE(MEDIA_STREAM)
        , WTFMove(m_internals.serializedMediaStreamTracks)
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

std::optional<ErrorInformation> extractErrorInformationFromErrorInstance(JSC::JSGlobalObject* lexicalGlobalObject, ErrorInstance& errorInstance)
{
    ASSERT(lexicalGlobalObject);
    auto& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto errorTypeValue = errorInstance.get(lexicalGlobalObject, vm.propertyNames->name);
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    String errorTypeString = errorTypeValue.toWTFString(lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor messageDescriptor;
    String message;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->message, messageDescriptor) && messageDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        message = messageDescriptor.value().toWTFString(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor lineDescriptor;
    unsigned line = 0;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->line, lineDescriptor) && lineDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        line = lineDescriptor.value().toNumber(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor columnDescriptor;
    unsigned column = 0;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->column, columnDescriptor) && columnDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        column = columnDescriptor.value().toNumber(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor sourceURLDescriptor;
    String sourceURL;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->sourceURL, sourceURLDescriptor) && sourceURLDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        sourceURL = sourceURLDescriptor.value().toWTFString(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor stackDescriptor;
    String stack;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->stack, stackDescriptor) && stackDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        stack = stackDescriptor.value().toWTFString(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    return { ErrorInformation { errorTypeString, message, line, column, sourceURL, stack } };
}

} // namespace WebCore
