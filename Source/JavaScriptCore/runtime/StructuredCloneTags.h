/*
 * Copyright (C) 2009-2026 Apple Inc. All rights reserved.
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
 */

#pragma once

#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <wtf/PrintStream.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringCommon.h>

// Most CPUs we support are little-endian and accept unaligned loads of the
// integer widths the wire format uses, so the read/write helpers fast-path
// to a single load/store. Architectures that need byte-by-byte handling
// (big-endian, middle-endian, alignment-strict) take the portable path.
#if CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN) || CPU(NEEDS_ALIGNED_ACCESS)
#define JSC_ASSUME_LITTLE_ENDIAN 0
#else
#define JSC_ASSUME_LITTLE_ENDIAN 1
#endif

namespace JSC {

/*
 * Object serialization is performed according to the following grammar, all tags
 * are recorded as a single uint8_t. 
 * 
 * Note: This includes the grammar for WebCore types simply so we don't have to
 * duplicate the enum and can have the description in one place.
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
 *    | RTCEncodedAudioFrameTag <identifier:uint32_t>
 *    | RTCEncodedVideoFrameTag <identifier:uint32_t>
 *    | ReadableStreamTag <identifier:uint32_t><messagePortIdentifier:uint32_t>
 *    | WritableStreamTag <identifier:uint32_t><messagePortIdentifier:uint32_t>
 *    | TransformStreamTag <identifier:uint32_t><messagePortIdentifiers:uint32_t>
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
 * PredefinedColorSpaceTag :
 *        PredefinedColorSpaceTag::SRGB
 *      | PredefinedColorSpaceTag::DisplayP3
 *      | PredefinedColorSpaceTag::SRGBLinear
 *      | PredefinedColorSpaceTag::DisplayP3Linear
 *
 * DestinationColorSpace :-
 *        DestinationColorSpaceSRGBTag
 *      | DestinationColorSpaceLinearSRGBTag
 *      | DestinationColorSpaceDisplayP3Tag
 *      | DestinationColorSpaceCGColorSpaceNameTag <nameDataLength:uint32_t> <nameData:uint8_t>{nameDataLength}
 *      | DestinationColorSpaceCGColorSpacePropertyListTag <propertyListDataLength:uint32_t> <propertyListData:uint8_t>{propertyListDataLength}
 *      | DestinationColorSpaceLinearDisplayP3Tag
 */

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
#if ENABLE(WEB_RTC)
    RTCEncodedAudioFrameTag = 62,
    RTCEncodedVideoFrameTag = 63,
#endif
#if ENABLE(MEDIA_STREAM)
    MediaStreamTrackHandleTag = 64,
#endif
    ReadableStreamTag = 65,
    WritableStreamTag = 66,
    TransformStreamTag = 67,
    FileSystemHandleTag = 68,
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

enum class SerializationReturnCode {
    SuccessfullyCompleted,
    StackOverflowError,
    InterruptedExecutionError,
    ValidationError,
    ExistingExceptionError,
    DataCloneError,
    UnspecifiedError
};

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

struct ErrorInformation {
    String errorTypeString;
    String message;
    unsigned line { 0 };
    unsigned column { 0 };
    String sourceURL;
    String stack;
    String cause;
};

class ErrorInstance;
JS_EXPORT_PRIVATE std::optional<ErrorInformation> extractErrorInformationFromErrorInstance(JSGlobalObject*, ErrorInstance&);

inline SerializableErrorType errorNameToSerializableErrorType(const String& name)
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

inline ErrorType toErrorType(SerializableErrorType value)
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

constexpr unsigned CurrentMajorVersion = 15;
constexpr unsigned CurrentMinorVersion = 0;
inline constexpr unsigned NODELETE majorVersionFor(unsigned version) { return version & 0x00FFFFFF; }
inline constexpr unsigned NODELETE minorVersionFor(unsigned version) { return version >> 24; }
inline constexpr unsigned NODELETE makeVersion(unsigned major, unsigned minor)
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
// FIXME: We should have two versions one for JSC version changes and one for WebCore version changes.
inline constexpr unsigned NODELETE currentVersion() { return makeVersion(CurrentMajorVersion, CurrentMinorVersion); }
constexpr unsigned TerminatorTag = 0xFFFFFFFF;
constexpr unsigned StringPoolTag = 0xFFFFFFFE;
constexpr unsigned NonIndexPropertiesTag = 0xFFFFFFFD;
constexpr uint32_t ImageDataPoolTag = 0xFFFFFFFE;

static_assert(TerminatorTag > MAX_ARRAY_INDEX);

// The high bit of a StringData's length determines the character size.
constexpr uint32_t StringDataIs8BitFlag = 0x80000000U;

// This function is only used for a sanity check mechanism used in
// CloneSerializer::addToObjectPoolIfNotDupe() and CloneDeserializer::addToObjectPool().
inline constexpr bool NODELETE canBeAddedToObjectPool(SerializationTag tag)
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

inline ASCIILiteral name(SerializationTag tag)
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
    case MediaStreamTrackHandleTag: return "MediaStreamTrackHandleTag"_s;
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    case MediaSourceHandleTransferTag: return "MediaSourceHandleTransferTag"_s;
#endif
#if ENABLE(WEB_RTC)
    case RTCEncodedAudioFrameTag: return "RTCEncodedAudioFrameTag"_s;
    case RTCEncodedVideoFrameTag: return "RTCEncodedVideoFrameTag"_s;
#endif
    case ReadableStreamTag: return "ReadableStreamTag"_s;
    case WritableStreamTag: return "WritableStreamTag"_s;
    case TransformStreamTag : return "TransformStreamTag"_s;
    case FileSystemHandleTag: return "FileSystemHandleTag"_s;
    case ErrorTag: return "ErrorTag"_s;
    }
    return "<unknown tag>"_s;
}

} // namespace JSC

namespace WTF {

JS_EXPORT_PRIVATE void printInternal(PrintStream&, JSC::SerializationTag);

} // namespace WTF
