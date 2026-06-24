/*
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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
#include "ClientOrigin.h"
#include "ContextDestructionObserverInlines.h"
#include "CryptoKeyAES.h"
#include "CryptoKeyEC.h"
#include "CryptoKeyHMAC.h"
#include "CryptoKeyOKP.h"
#include "CryptoKeyRSA.h"
#include "CryptoKeyRSAComponents.h"
#include "CryptoKeyRaw.h"
#include "Document.h"
#include "DocumentQuirks.h"
#include "FileSystemDirectoryHandle.h"
#include "FileSystemFileHandle.h"
#include "FileSystemHandleGlobalIdentifier.h"
#include "HTMLCanvasElement.h"
#include "IDBValue.h"
#include "ImageBuffer.h"
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
#include "JSFileSystemDirectoryHandle.h"
#include "JSFileSystemFileHandle.h"
#include "JSIDBSerializationGlobalObject.h"
#include "JSImageBitmap.h"
#include "JSImageData.h"
#include "JSMediaSourceHandle.h"
#include "JSMediaStreamTrack.h"
#include "JSMediaStreamTrackHandle.h"
#include "JSMessagePort.h"
#include "JSNavigator.h"
#include "JSRTCCertificate.h"
#include "JSRTCDataChannel.h"
#include "JSRTCEncodedAudioFrame.h"
#include "JSRTCEncodedVideoFrame.h"
#include "JSReadableStream.h"
#include "JSTransformStream.h"
#include "JSWebCodecsAudioData.h"
#include "JSWebCodecsEncodedAudioChunk.h"
#include "JSWebCodecsEncodedVideoChunk.h"
#include "JSWebCodecsVideoFrame.h"
#include "JSWritableStream.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValueInternals.h"
#include "SharedBuffer.h"
#include "WebCodecsEncodedAudioChunk.h"
#include "WebCodecsEncodedVideoChunk.h"
#include "WebCoreJSClientData.h"
#include "WorkerSTWParticipation.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/ArrayConventions.h>
#include <JavaScriptCore/BigIntObject.h>
#include <JavaScriptCore/BooleanObject.h>
#include <JavaScriptCore/CloneBase.h>
#include <JavaScriptCore/CloneDeserializerBase.h>
#include <JavaScriptCore/CloneSerializerBase.h>
#include <JavaScriptCore/DateInstance.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/ErrorInstance.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/IterationKind.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCellInlines.h>
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
#include <JavaScriptCore/ObjectPrototype.h>
#include <JavaScriptCore/Options.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/RegExp.h>
#include <JavaScriptCore/RegExpObject.h>
#include <JavaScriptCore/Strong.h>
#include <JavaScriptCore/StructuredCloneTags.h>
#include <JavaScriptCore/TopExceptionScope.h>
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
#include <wtf/text/ParsingUtilities.h>
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
#include "PlaceholderRenderingContext.h"
#endif

namespace WebCore {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(SerializedScriptValueInternals);

using namespace JSC;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SerializedScriptValue);

static constexpr unsigned maximumFilterRecursion = 40000;
static constexpr uint64_t autoLengthMarker = UINT64_MAX;

enum WalkerState { StateUnknown, ArrayStartState, ArrayStartVisitIndexedMember, ArrayEndVisitIndexedMember,
    ArrayStartVisitNamedMember, ArrayEndVisitNamedMember,
    ObjectStartState, ObjectStartVisitNamedMember, ObjectEndVisitNamedMember,
    MapDataStartVisitEntry, MapDataEndVisitKey, MapDataEndVisitValue,
    SetDataStartVisitEntry, SetDataEndVisitKey };

static bool NODELETE isTypeExposedToGlobalObject(JSC::JSGlobalObject& globalObject, SerializationTag tag)
{
#if ENABLE(WEB_AUDIO)
    if (!is<JSAudioWorkletGlobalScope>(globalObject))
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
    case MediaStreamTrackHandleTag:
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    case MediaSourceHandleTransferTag:
#endif
#if ENABLE(WEB_RTC)
    case RTCEncodedAudioFrameTag:
#endif
#if ENABLE(WEB_RTC)
    case RTCEncodedVideoFrameTag:
#endif
    case ReadableStreamTag:
    case WritableStreamTag:
    case TransformStreamTag:
    case FileSystemHandleTag:
        break;
    }
    return false;
#else
    UNUSED_PARAM(globalObject);
    UNUSED_PARAM(tag);
    return true;
#endif
}

static unsigned NODELETE typedArrayElementSize(ArrayBufferViewSubtag tag)
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

enum class PredefinedColorSpaceTag : uint8_t {
    SRGB = 0,
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    DisplayP3 = 1,
#endif
    SRGBLinear = 2,
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    DisplayP3Linear = 3,
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
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
    DestinationColorSpaceLinearDisplayP3Tag = 5,
#endif
};

namespace {

enum class ImageBitmapSerializationFlags : uint8_t {
    OriginClean              = 1 << 0, // ImageBitmap is always clean if serialized. However, at some point non-clean bitmaps were serialized. Can be removed once version is increased.
    PremultiplyAlpha         = 1 << 1,
    ForciblyPremultiplyAlpha = 1 << 2
};

}

static String agentClusterIDFromGlobalObject(JSGlobalObject& globalObject)
{
    auto& domGlobalObject = downcast<JSDOMGlobalObject>(globalObject);
    RefPtr context = domGlobalObject.scriptExecutionContext();
    ASSERT(context);
    if (!context)
        return nullString();
    auto result = context->agentClusterID();
    ASSERT(!result.isNull());
    return result;
}

static bool isCrossOriginIsolatedContext(JSGlobalObject* globalObject)
{
    auto* domGlobalObject = dynamicDowncast<JSDOMGlobalObject>(globalObject);
    if (!domGlobalObject)
        return false;
    RefPtr context = domGlobalObject->scriptExecutionContext();
    return context && context->crossOriginIsolated();
}

const uint32_t currentKeyFormatVersion = 1;

enum class CryptoKeyClassSubtag : uint8_t {
    HMAC = 0,
    AES = 1,
    RSA = 2,
    EC = 3,
    Raw = 4,
    OKP = 5,
};
const uint8_t cryptoKeyClassSubtagMaximumValue = 5;

enum class CryptoKeyAsymmetricTypeSubtag : bool {
    Public = 0,
    Private = 1
};
const uint8_t cryptoKeyAsymmetricTypeSubtagMaximumValue = 1;

enum class CryptoKeyUsageTag : uint8_t {
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
    DEPRECATED_SHA_224 = 15,
    SHA_256 = 16,
    SHA_384 = 17,
    SHA_512 = 18,
    HKDF = 20,
    PBKDF2 = 21,
    ED25519 = 22,
    X25519 = 23,
};

const uint8_t cryptoAlgorithmIdentifierTagMaximumValue = 23;

static unsigned NODELETE countUsages(CryptoKeyUsageBitmap usages)
{
    // Fast bit count algorithm for sparse bit maps.
    unsigned count = 0;
    while (usages) {
        usages = usages & (usages - 1);
        ++count;
    }
    return count;
}

enum class CryptoKeyOKPOpNameTag : bool {
    X25519 = 0,
    ED25519 = 1,
};
const uint8_t cryptoKeyOKPOpNameTagMaximumValue = 1;

// See JavaScriptCore's StructuredCloneTags.h for a description of the wire format.

struct DeserializationResult {
    JSC::JSValue value;
    SerializationReturnCode code;
};

static std::optional<Vector<uint8_t>> serializeAndWrapCryptoKey(JSGlobalObject* lexicalGlobalObject, WebCore::CryptoKeyData&& key)
{
    RefPtr context = executionContext(lexicalGlobalObject);
    if (!context)
        return std::nullopt;

    return context->serializeAndWrapCryptoKey(WTF::move(key));
}

static std::optional<Vector<uint8_t>> unwrapCryptoKey(JSGlobalObject* lexicalGlobalObject, const Vector<uint8_t>& wrappedKey)
{
    RefPtr context = executionContext(lexicalGlobalObject);
    if (!context)
        return std::nullopt;

    return context->unwrapCryptoKey(wrappedKey);
}

Ref<SerializedScriptValue> SerializedScriptValue::createFromWireBytes(Vector<uint8_t>&& data)
{
    Internals internals;
    internals.data = WTF::move(data);
    return adoptRef(*new SerializedScriptValue(WTF::move(internals)));
}

class CloneSerializer;
#if ASSERT_ENABLED
static void validateSerializedResult(CloneSerializer&, SerializationReturnCode, Vector<uint8_t>& result, JSGlobalObject*, Vector<Ref<MessagePort>>&, ArrayBufferContentsArray&, ArrayBufferContentsArray& sharedBuffers, Vector<Ref<MessagePort>>&);
#endif

class CloneSerializer : public JSC::CloneSerializerBase<CloneSerializer> {
    using Base = JSC::CloneSerializerBase<CloneSerializer>;

    WTF_FORBID_HEAP_ALLOCATION;
public:
    static Vector<uint8_t> serializeCryptoKey(const CryptoKey& key)
    {
        Vector<uint8_t> serializedKey;
        Vector<URLKeepingBlobAlive> dummyBlobHandles;
        Vector<Ref<MessagePort>> dummyMessagePorts;
        Vector<Ref<JSC::ArrayBuffer>> dummyArrayBuffers;
#if ENABLE(WEB_CODECS)
        Vector<Ref<WebCodecsEncodedVideoChunkStorage>> dummyVideoChunks;
        Vector<RefPtr<WebCodecsVideoFrame>> dummyVideoFrames;
        Vector<Ref<WebCodecsEncodedAudioChunkStorage>> dummyAudioChunks;
        Vector<RefPtr<WebCodecsAudioData>> dummyAudioData;
#endif
#if ENABLE(WEB_RTC)
        Vector<RefPtr<RTCEncodedAudioFrame>> dummyRTCEncodedAudioFrames;
        Vector<RefPtr<RTCEncodedVideoFrame>> dummyRTCEncodedVideoFrames;
#endif
#if ENABLE(MEDIA_STREAM)
        Vector<Ref<MediaStreamTrack>> dummyMediaStreamTracks;
        Vector<Ref<MediaStreamTrackHandle>> dummyMediaStreamTrackHandles;
#endif
#if ENABLE(WEBASSEMBLY)
        WasmModuleArray dummyModules;
        WasmMemoryHandleArray dummyMemoryHandles;
#endif
        ArrayBufferContentsArray dummySharedBuffers;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        Vector<Ref<OffscreenCanvas>> dummyInMemoryOffscreenCanvases;
#endif
        Vector<Ref<MessagePort>> dummyInMemoryMessagePorts;
        CloneSerializer rawKeySerializer(nullptr, dummyMessagePorts, dummyArrayBuffers, { },
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            { },
            dummyInMemoryOffscreenCanvases,
#endif
            dummyInMemoryMessagePorts,
#if ENABLE(WEB_RTC)
            { }, dummyRTCEncodedAudioFrames, dummyRTCEncodedVideoFrames,
#endif
            { },
            { },
            { },
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
            dummyMediaStreamTrackHandles,
#endif
#if ENABLE(WEBASSEMBLY)
            dummyModules,
            dummyMemoryHandles,
#endif
            dummyBlobHandles, serializedKey, SerializationContext::Default, dummySharedBuffers, SerializationForStorage::No);
        rawKeySerializer.write(&key);
        return serializedKey;
    }

    static SerializationReturnCode serialize(JSGlobalObject* lexicalGlobalObject, JSValue value, Vector<Ref<MessagePort>>& messagePorts, Vector<Ref<JSC::ArrayBuffer>>& arrayBuffers, const Vector<Ref<ImageBitmap>>& imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            const Vector<Ref<OffscreenCanvas>>& offscreenCanvases,
            Vector<Ref<OffscreenCanvas>>& inMemoryOffscreenCanvases,
#endif
            Vector<Ref<MessagePort>>& inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
            const Vector<Ref<RTCDataChannel>>& rtcDataChannels,
            Vector<RefPtr<RTCEncodedAudioFrame>>& serializedRTCEncodedAudioFrames,
            Vector<RefPtr<RTCEncodedVideoFrame>>& serializedRTCEncodedVideoFrames,
#endif
            const Vector<Ref<ReadableStream>>& readableStreams,
            const Vector<Ref<WritableStream>>& writableStreams,
            const Vector<Ref<TransformStream>>& transformStreams,
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            const Vector<Ref<MediaSourceHandle>>& mediaSourceHandles,
#endif
#if ENABLE(WEB_CODECS)
            Vector<Ref<WebCodecsEncodedVideoChunkStorage>>& serializedVideoChunks,
            Vector<RefPtr<WebCodecsVideoFrame>>& serializedVideoFrames,
            Vector<Ref<WebCodecsEncodedAudioChunkStorage>>& serializedAudioChunks,
            Vector<RefPtr<WebCodecsAudioData>>& serializedAudioData,
#endif
#if ENABLE(MEDIA_STREAM)
            Vector<Ref<MediaStreamTrack>>& detachedMediaStreamTracks,
            const Vector<Ref<MediaStreamTrackHandle>>& detachedMediaStreamTrackHandles,
#endif
#if ENABLE(WEBASSEMBLY)
            WasmModuleArray& wasmModules,
            WasmMemoryHandleArray& wasmMemoryHandles,
#endif
        Vector<URLKeepingBlobAlive>& blobHandles, Vector<uint8_t>& out, SerializationContext context, ArrayBufferContentsArray& sharedBuffers,
        SerializationForStorage forStorage,
        Vector<FileSystemHandleKeepAlive>& fileSystemHandleKeepAlives)
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
            serializedRTCEncodedAudioFrames,
            serializedRTCEncodedVideoFrames,
#endif
            readableStreams,
            writableStreams,
            transformStreams,
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
            detachedMediaStreamTracks,
            detachedMediaStreamTrackHandles,
#endif
#if ENABLE(WEBASSEMBLY)
            wasmModules,
            wasmMemoryHandles,
#endif
            blobHandles, out, context, sharedBuffers, forStorage);
        auto code = serializer.serialize(value);
        fileSystemHandleKeepAlives = WTF::move(serializer.m_fileSystemHandleKeepAlives);
#if ENABLE(MEDIA_STREAM)
        for (auto& track : std::exchange(serializer.m_serializedMediaStreamTracks , { }))
            detachedMediaStreamTracks.append(track.releaseNonNull());
#endif
#if ASSERT_ENABLED
        RETURN_IF_EXCEPTION(scope, code);
        validateSerializedResult(serializer, code, out, lexicalGlobalObject, messagePorts, sharedBuffers, sharedBuffers, inMemoryMessagePorts);
#endif
        return code;
    }

    static bool serialize(StringView string, Vector<uint8_t>& out)
    {
        JSC::StructuredCloneInternal::writeLittleEndian(out, currentVersion());
        if (string.isEmpty()) {
            JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(out, EmptyStringTag);
            return true;
        }
        JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(out, StringTag);
        if (string.is8Bit()) {
            JSC::StructuredCloneInternal::writeLittleEndian(out, string.length() | StringDataIs8BitFlag);
            return JSC::StructuredCloneInternal::writeLittleEndian(out, string.span8());
        }
        JSC::StructuredCloneInternal::writeLittleEndian(out, string.length());
        return JSC::StructuredCloneInternal::writeLittleEndian(out, string.span16());
    }

#if ASSERT_ENABLED
    bool didSeeComplexCases() const { return m_didSeeComplexCases; }
    void setDidSeeComplexCases() { m_didSeeComplexCases = true; }
#else
    ALWAYS_INLINE void setDidSeeComplexCases() { }
#endif

private:
    using ObjectPoolMap = HashMap<JSObject*, uint32_t>;

    CloneSerializer(JSGlobalObject* lexicalGlobalObject, Vector<Ref<MessagePort>>& messagePorts, Vector<Ref<JSC::ArrayBuffer>>& arrayBuffers, const Vector<Ref<ImageBitmap>>& imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            const Vector<Ref<OffscreenCanvas>>& offscreenCanvases,
            Vector<Ref<OffscreenCanvas>>& inMemoryOffscreenCanvases,
#endif
            Vector<Ref<MessagePort>>& inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
            const Vector<Ref<RTCDataChannel>>& rtcDataChannels,
            Vector<RefPtr<RTCEncodedAudioFrame>>& serializedRTCEncodedAudioFrames,
            Vector<RefPtr<RTCEncodedVideoFrame>>& serializedRTCEncodedVideoFrames,
#endif
            const Vector<Ref<ReadableStream>>& readableStreams,
            const Vector<Ref<WritableStream>>& writableStreams,
            const Vector<Ref<TransformStream>>& transformStreams,
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            const Vector<Ref<MediaSourceHandle>>& mediaSourceHandles,
#endif
#if ENABLE(WEB_CODECS)
            Vector<Ref<WebCodecsEncodedVideoChunkStorage>>& serializedVideoChunks,
            Vector<RefPtr<WebCodecsVideoFrame>>& serializedVideoFrames,
            Vector<Ref<WebCodecsEncodedAudioChunkStorage>>& serializedAudioChunks,
            Vector<RefPtr<WebCodecsAudioData>>& serializedAudioData,
#endif
#if ENABLE(MEDIA_STREAM)
            const Vector<Ref<MediaStreamTrack>>& mediaStreamTracks,
            const Vector<Ref<MediaStreamTrackHandle>>& mediaStreamTrackHandles,
#endif
#if ENABLE(WEBASSEMBLY)
            WasmModuleArray& wasmModules,
            WasmMemoryHandleArray& wasmMemoryHandles,
#endif
        Vector<URLKeepingBlobAlive>& blobHandles, Vector<uint8_t>& out, SerializationContext context, ArrayBufferContentsArray& sharedBuffers, SerializationForStorage forStorage)
        : Base(lexicalGlobalObject, out)
        , m_blobHandles(blobHandles)
#if ENABLE(WEB_RTC)
        , m_serializedRTCEncodedAudioFrames(serializedRTCEncodedAudioFrames)
        , m_serializedRTCEncodedVideoFrames(serializedRTCEncodedVideoFrames)
#endif
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
        fillTransferMap(readableStreams, m_transferredReadableStreams);
        fillTransferMap(writableStreams, m_transferredWritableStreams);
        fillTransferMap(transformStreams, m_transferredTransformStreams);
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        fillTransferMap(mediaSourceHandles, m_transferredMediaSourceHandles);
#endif
#if ENABLE(MEDIA_STREAM)
        fillTransferMap(mediaStreamTracks, m_transferredMediaStreamTracks);
        fillTransferMap(mediaStreamTrackHandles, m_transferredMediaStreamTrackHandles);
#endif
    }

    template<typename T>
    JSValue toJSOrNull(JSDOMGlobalObject* globalObject, const Ref<T>& value)
    {
        return toJS(globalObject, globalObject, value);
    }

    template<typename T>
    JSValue toJSOrNull(JSDOMGlobalObject* globalObject, const RefPtr<T>& value)
    {
        if (!value)
            return jsNull();
        return toJS(globalObject, globalObject, *value);
    }

    template<typename T>
    void fillTransferMap(const Vector<T>& input, ObjectPoolMap& result)
    {
        if (input.isEmpty())
            return;

        auto* globalObject = downcast<JSDOMGlobalObject>(m_lexicalGlobalObject);
        for (size_t i = 0; i < input.size(); ++i) {
            if (auto* object = toJSOrNull(globalObject, input[i]).getObject())
                result.add(object, i);
        }
    }

    SerializationReturnCode serialize(JSValue in);

    bool NODELETE isArray(JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return object->inherits<JSArray>();
    }

    bool NODELETE isMap(JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return object->inherits<JSMap>();
    }
    bool NODELETE isSet(JSValue value)
    {
        if (!value.isObject())
            return false;
        JSObject* object = asObject(value);
        return object->inherits<JSSet>();
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

    JSC::JSValue toJSArrayBuffer(ArrayBuffer& arrayBuffer)
    {
        auto& vm = m_lexicalGlobalObject->vm();
        auto* globalObject = m_lexicalGlobalObject;
        if (auto* domGlobalObject = dynamicDowncast<JSDOMGlobalObject>(*globalObject))
            return toJS(globalObject, domGlobalObject, arrayBuffer);

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

        if (uncheckedDowncast<JSArrayBufferView>(obj)->isOutOfBounds()) [[unlikely]] {
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

        dumpDOMPoint(protect(downcast<JSDOMPointReadOnly>(obj)->wrapped()));
    }

    void dumpDOMRect(JSObject* obj)
    {
        if (obj->inherits<JSDOMRect>())
            write(DOMRectTag);
        else
            write(DOMRectReadOnlyTag);

        Ref rect = downcast<JSDOMRectReadOnly>(obj)->wrapped();
        write(rect->x());
        write(rect->y());
        write(rect->width());
        write(rect->height());
    }

    void dumpDOMMatrix(JSObject* obj)
    {
        if (obj->inherits<JSDOMMatrix>())
            write(DOMMatrixTag);
        else
            write(DOMMatrixReadOnlyTag);

        Ref matrix = downcast<JSDOMMatrixReadOnly>(obj)->wrapped();
        bool is2D = matrix->is2D();
        write(is2D);
        if (is2D) {
            write(matrix->m11());
            write(matrix->m12());
            write(matrix->m21());
            write(matrix->m22());
            write(matrix->m41());
            write(matrix->m42());
        } else {
            write(matrix->m11());
            write(matrix->m12());
            write(matrix->m13());
            write(matrix->m14());
            write(matrix->m21());
            write(matrix->m22());
            write(matrix->m23());
            write(matrix->m24());
            write(matrix->m31());
            write(matrix->m32());
            write(matrix->m33());
            write(matrix->m34());
            write(matrix->m41());
            write(matrix->m42());
            write(matrix->m43());
            write(matrix->m44());
        }
    }

    void dumpDOMQuad(JSObject* obj)
    {
        write(DOMQuadTag);

        Ref quad = downcast<JSDOMQuad>(obj)->wrapped();
        dumpDOMPoint(quad->p1());
        dumpDOMPoint(quad->p2());
        dumpDOMPoint(quad->p3());
        dumpDOMPoint(quad->p4());
    }

    void dumpImageBitmap(JSObject* obj, SerializationReturnCode& code)
    {
        Ref imageBitmap = downcast<JSImageBitmap>(obj)->wrapped();
        auto index = m_transferredImageBitmaps.find(obj);
        if (index != m_transferredImageBitmaps.end()) {
            write(ImageBitmapTransferTag);
            write(index->value);
            return;
        }

        if (!imageBitmap->originClean()) {
            code = SerializationReturnCode::DataCloneError;
            return;
        }

        RefPtr buffer = imageBitmap->buffer();
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

        auto arrayBuffer = protect(pixelBuffer->data())->possiblySharedBuffer();
        if (!arrayBuffer) {
            code = SerializationReturnCode::ValidationError;
            return;
        }
        OptionSet<ImageBitmapSerializationFlags> flags;
        // Origin must be clean to transfer, but the check was not always in place. Ensure tainted ImageBitmaps are not
        // loaded anymore.
        flags.add(ImageBitmapSerializationFlags::OriginClean);
        if (imageBitmap->premultiplyAlpha())
            flags.add(ImageBitmapSerializationFlags::PremultiplyAlpha);
        if (imageBitmap->forciblyPremultiplyAlpha())
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
            m_inMemoryOffscreenCanvases.append(protect(downcast<JSOffscreenCanvas>(obj)->wrapped()));
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
    void dumpRTCEncodedAudioFrame(JSObject* obj)
    {
        Ref frame = downcast<JSRTCEncodedAudioFrame>(obj)->wrapped();

        auto index = m_serializedRTCEncodedAudioFrames.find(frame.ptr());
        if (index == notFound) {
            index = m_serializedRTCEncodedAudioFrames.size();
            m_serializedRTCEncodedAudioFrames.append(WTF::move(frame));
        }

        write(RTCEncodedAudioFrameTag);
        write(static_cast<uint32_t>(index));
    }
    void dumpRTCEncodedVideoFrame(JSObject* obj)
    {
        Ref frame = downcast<JSRTCEncodedVideoFrame>(obj)->wrapped();

        auto index = m_serializedRTCEncodedVideoFrames.find(frame.ptr());
        if (index == notFound) {
            index = m_serializedRTCEncodedVideoFrames.size();
            m_serializedRTCEncodedVideoFrames.append(WTF::move(frame));
        }

        write(RTCEncodedVideoFrameTag);
        write(static_cast<uint32_t>(index));
    }
#endif
    void dumpReadableStream(JSObject* obj, SerializationReturnCode& code)
    {
        auto index = m_transferredReadableStreams.find(obj);
        if (index != m_transferredReadableStreams.end()) {
            write(ReadableStreamTag);
            write(index->value + 1);
            write(m_transferredMessagePorts.size() + index->value);
            return;
        }

        code = SerializationReturnCode::DataCloneError;
    }
    void dumpWritableStream(JSObject* obj, SerializationReturnCode& code)
    {
        auto index = m_transferredWritableStreams.find(obj);
        if (index != m_transferredWritableStreams.end()) {
            write(WritableStreamTag);
            write(index->value + 1);
            write(m_transferredMessagePorts.size() + m_transferredReadableStreams.size() + index->value);
            return;
        }

        code = SerializationReturnCode::DataCloneError;
    }
    void dumpTransformStream(JSObject* obj, SerializationReturnCode& code)
    {
        auto index = m_transferredTransformStreams.find(obj);
        if (index != m_transferredTransformStreams.end()) {
            write(TransformStreamTag);
            write(index->value + 1);
            write(m_transferredMessagePorts.size() + m_transferredReadableStreams.size() + m_transferredWritableStreams.size() + + 2 * index->value);
            return;
        }

        code = SerializationReturnCode::DataCloneError;
    }
#if ENABLE(WEB_CODECS)
    void dumpWebCodecsEncodedVideoChunk(JSObject* obj)
    {
        Ref videoChunk = downcast<JSWebCodecsEncodedVideoChunk>(obj)->wrapped();

        auto index = m_serializedVideoChunks.find(&videoChunk->storage());
        if (index == notFound) {
            index = m_serializedVideoChunks.size();
            m_serializedVideoChunks.append(videoChunk->storage());
        }

        write(WebCodecsEncodedVideoChunkTag);
        write(static_cast<uint32_t>(index));
    }

    bool dumpWebCodecsVideoFrame(JSObject* obj)
    {
        Ref videoFrame = downcast<JSWebCodecsVideoFrame>(obj)->wrapped();
        if (videoFrame->isDetached())
            return false;

        auto index = m_serializedVideoFrames.find(videoFrame.ptr());
        if (index == notFound) {
            index = m_serializedVideoFrames.size();
            m_serializedVideoFrames.append(WTF::move(videoFrame));
        }
        write(WebCodecsVideoFrameTag);
        write(static_cast<uint32_t>(index));
        return true;
    }

    void dumpWebCodecsEncodedAudioChunk(JSObject* obj)
    {
        Ref audioChunk = downcast<JSWebCodecsEncodedAudioChunk>(obj)->wrapped();

        auto index = m_serializedAudioChunks.find(&audioChunk->storage());
        if (index == notFound) {
            index = m_serializedAudioChunks.size();
            m_serializedAudioChunks.append(audioChunk->storage());
        }

        write(WebCodecsEncodedAudioChunkTag);
        write(static_cast<uint32_t>(index));
    }

    bool dumpWebCodecsAudioData(JSObject* obj)
    {
        Ref audioData = downcast<JSWebCodecsAudioData>(obj)->wrapped();
        if (audioData->isDetached())
            return false;

        auto index = m_serializedAudioData.find(audioData.ptr());
        if (index == notFound) {
            index = m_serializedAudioData.size();
            m_serializedAudioData.append(WTF::move(audioData));
        }
        write(WebCodecsAudioDataTag);
        write(static_cast<uint32_t>(index));
        return true;
    }
#endif
#if ENABLE(MEDIA_STREAM)
    void dumpMediaStreamTrack(JSObject* obj, SerializationReturnCode& code)
    {
        auto index = m_transferredMediaStreamTracks.find(obj);
        if (index == m_transferredMediaStreamTracks.end()) {
            bool shouldAllowMediaStreamTrackSerialization = [&] {
                RefPtr context = downcast<JSDOMGlobalObject>(m_lexicalGlobalObject)->scriptExecutionContext();
                RefPtr document = dynamicDowncast<Document>(context);
                return document && document->quirks().shouldAllowMediaStreamTrackSerializationQuirk();
            }();
            if (!shouldAllowMediaStreamTrackSerialization) {
                code = SerializationReturnCode::DataCloneError;
                return;
            }

            m_serializedMediaStreamTracks.append(protect(downcast<JSMediaStreamTrack>(obj)->wrapped())->clone());
            index = m_transferredMediaStreamTracks.add(obj, m_transferredMediaStreamTracks.size()).iterator;
        }

        write(MediaStreamTrackTag);
        write(index->value);
    }
    void dumpMediaStreamTrackHandle(JSObject* obj, SerializationReturnCode& code)
    {
        auto index = m_transferredMediaStreamTrackHandles.find(obj);
        if (index != m_transferredMediaStreamTrackHandles.end()) {
            write(MediaStreamTrackHandleTag);
            write(index->value);
            return;
        }

        code = SerializationReturnCode::DataCloneError;
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
        if (RefPtr exception = JSDOMException::toWrapped(m_lexicalGlobalObject->vm(), obj)) {
            write(DOMExceptionTag);
            write(exception->message());
            write(exception->name());
            return;
        }

        code = SerializationReturnCode::DataCloneError;
    }

public:
    bool dumpDerivedTerminal(JSValue value, SerializationReturnCode& code)
    {
        ASSERT(value.isCell());

        if (value.isSymbol()) {
            code = SerializationReturnCode::DataCloneError;
            return true;
        }

        VM& vm = m_lexicalGlobalObject->vm();
        if (isArray(value))
            return false;

        if (value.isObject()) {
            auto* obj = asObject(value);
            if (RefPtr file = JSFile::toWrapped(vm, obj)) {
                write(FileTag);
                write(*file);
                return true;
            }
            if (RefPtr list = JSFileList::toWrapped(vm, obj)) {
                write(FileListTag);
                write(list->length());
                for (auto& file : list->files())
                    write(file.get());
                return true;
            }
            if (RefPtr blob = JSBlob::toWrapped(vm, obj)) {
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
            if (RefPtr data = JSImageData::toWrapped(vm, obj)) {
                write(ImageDataTag);
                auto addResult = m_imageDataPool.add(*data, m_imageDataPool.size());
                if (!addResult.isNewEntry) {
                    write(ImageDataPoolTag);
                    writeImageDataIndex(addResult.iterator->value);
                    return true;
                }
                write(static_cast<uint32_t>(data->width()));
                write(static_cast<uint32_t>(data->height()));
                CheckedUint32 dataLength = data->data().byteLength();
                if (dataLength.hasOverflowed()) {
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }
                write(dataLength);
                write(protect(data->data().arrayBufferView())->span());
                write(data->colorSpace());
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
                    m_inMemoryMessagePorts.append(protect(downcast<JSMessagePort>(obj)->wrapped()));
                    return true;
                }
                // MessagePort object could not be found in transferred message ports
                code = SerializationReturnCode::ValidationError;
                return true;
            }
            if (RefPtr arrayBuffer = toPossiblySharedArrayBuffer(vm, obj)) {
                if (arrayBuffer->isDetached()) {
                    code = SerializationReturnCode::DataCloneError;
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
                
                if (arrayBuffer->isShared()) {
                    // https://html.spec.whatwg.org/multipage/structured-data.html#structuredserializeinternal
                    if (m_forStorage == SerializationForStorage::Yes) {
                        code = SerializationReturnCode::DataCloneError;
                        return true;
                    }
                    if (isCrossOriginIsolatedContext(m_lexicalGlobalObject) || JSC::Options::useSharedArrayBuffer()) {
                        uint32_t index = m_sharedBuffers.size();
                        ArrayBufferContents contents;
                        if (arrayBuffer->shareWith(contents)) {
                            appendObjectPoolTag(SharedArrayBufferTag);
                            write(SharedArrayBufferTag);
                            write(agentClusterIDFromGlobalObject(*m_lexicalGlobalObject));
                            m_sharedBuffers.append(WTF::move(contents));
                            write(index);
                            return true;
                        }
                    }
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }
                
                if (arrayBuffer->isResizableOrGrowableShared()) {
                    appendObjectPoolTag(ResizableArrayBufferTag);
                    write(ResizableArrayBufferTag);
                    writeResizableArrayBuffer(arrayBuffer->span(), arrayBuffer->maxByteLength().value_or(0));
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
            if (RefPtr key = JSCryptoKey::toWrapped(vm, obj)) {
                write(CryptoKeyTag);
                auto wrappedKey = serializeAndWrapCryptoKey(m_lexicalGlobalObject, key->data());
                if (!wrappedKey) {
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }

                write(*wrappedKey);
                return true;
            }
#if ENABLE(WEB_RTC)
            if (RefPtr rtcCertificate = JSRTCCertificate::toWrapped(vm, obj)) {
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
            if (JSWebAssemblyModule* module = dynamicDowncast<JSWebAssemblyModule>(obj)) {
                if (m_forStorage == SerializationForStorage::Yes) {
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }

                uint32_t index = m_wasmModules.size();
                m_wasmModules.append(Ref { module->module() });
                write(WasmModuleTag);
                write(agentClusterIDFromGlobalObject(*m_lexicalGlobalObject));
                write(index);
                return true;
            }
            if (JSWebAssemblyMemory* memory = dynamicDowncast<JSWebAssemblyMemory>(obj)) {
                if (m_forStorage == SerializationForStorage::Yes || memory->memory().sharingMode() != JSC::MemorySharingMode::Shared) {
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }
                if (!isCrossOriginIsolatedContext(m_lexicalGlobalObject) && !JSC::Options::useSharedArrayBuffer()) {
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
            if (obj->inherits<JSRTCEncodedAudioFrame>()) {
                dumpRTCEncodedAudioFrame(obj);
                return true;
            }
            if (obj->inherits<JSRTCEncodedVideoFrame>()) {
                dumpRTCEncodedVideoFrame(obj);
                return true;
            }
#endif
            if (obj->inherits<JSReadableStream>()) {
                dumpReadableStream(obj, code);
                return true;
            }
            if (obj->inherits<JSWritableStream>()) {
                dumpWritableStream(obj, code);
                return true;
            }
            if (obj->inherits<JSTransformStream>()) {
                dumpTransformStream(obj, code);
                return true;
            }
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
                dumpMediaStreamTrack(obj, code);
                return true;
            }
            if (obj->inherits<JSMediaStreamTrackHandle>()) {
                if (m_forStorage == SerializationForStorage::Yes)
                    return false;
                dumpMediaStreamTrackHandle(obj, code);
                return true;
            }
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            if (obj->inherits<JSMediaSourceHandle>()) {
                dumpMediaSourceHandle(obj, code);
                return true;
            }
#endif

            auto serializeFileSystemHandle = [&](FileSystemHandle& handle) {
                if (handle.isClosed()) {
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }
                RefPtr context = handle.scriptExecutionContext();
                if (!context || !context->settingsValues().fileSystemHandleSerializationEnabled) {
                    code = SerializationReturnCode::DataCloneError;
                    return true;
                }
                write(FileSystemHandleTag);
                write(std::to_underlying(handle.kind()));
                write(handle.name());
                write(std::span<const uint8_t> { handle.globalIdentifier().toRawValue().span() });
                ASSERT(!context->securityOrigin()->isOpaque());
                write(context->securityOrigin()->toString());
                if (RefPtr connection = fileSystemStorageConnectionForContext(*context)) {
                    auto origin = clientOriginForContext(*context);
                    m_fileSystemHandleKeepAlives.append({ WTF::move(origin), handle.globalIdentifier(), connection.releaseNonNull() });
                }
                return true;
            };
            if (auto* fileHandle = dynamicDowncast<JSFileSystemFileHandle>(obj))
                return serializeFileSystemHandle(fileHandle->wrapped());
            if (auto* dirHandle = dynamicDowncast<JSFileSystemDirectoryHandle>(obj))
                return serializeFileSystemHandle(dirHandle->wrapped());

            return false;
        }
        // Any other types are expected to serialize as null.
        write(NullTag);
        return true;
    }

private:
    using Base::write;

    void write(DestinationColorSpaceTag tag)
    {
        JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(CryptoKeyClassSubtag tag)
    {
        JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(CryptoKeyAsymmetricTypeSubtag tag)
    {
        JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(CryptoKeyUsageTag tag)
    {
        JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(CryptoAlgorithmIdentifierTag tag)
    {
        JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(CryptoKeyOKPOpNameTag tag)
    {
        JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void writeImageDataIndex(unsigned i)
    {
        writeConstantPoolIndex(m_imageDataPool, i);
    }

    void write(const Vector<uint8_t>& vector)
    {
        uint32_t size = vector.size();
        write(size);
        JSC::StructuredCloneInternal::writeLittleEndian(m_buffer, vector.span());
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
            JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(PredefinedColorSpaceTag::SRGB));
            break;
        case PredefinedColorSpace::SRGBLinear:
            JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(PredefinedColorSpaceTag::SRGBLinear));
            break;
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        case PredefinedColorSpace::DisplayP3:
            JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(PredefinedColorSpaceTag::DisplayP3));
            break;
        case PredefinedColorSpace::DisplayP3Linear:
            JSC::StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(PredefinedColorSpaceTag::DisplayP3Linear));
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

        if (destinationColorSpace == DestinationColorSpace::LinearDisplayP3()) {
            write(DestinationColorSpaceLinearDisplayP3Tag);
            return;
        }
#endif

#if PLATFORM(COCOA)
        RetainPtr colorSpace = destinationColorSpace.platformColorSpace();

        if (RetainPtr name = CGColorSpaceGetName(colorSpace.get())) {
            auto data = adoptCF(CFStringCreateExternalRepresentation(nullptr, name.get(), kCFStringEncodingUTF8, 0));
            if (!data) {
                write(DestinationColorSpaceSRGBTag);
                return;
            }

            write(DestinationColorSpaceCGColorSpaceNameTag);
            write(data);
            return;
        }

        if (auto propertyList = adoptCF(CGColorSpaceCopyPropertyList(colorSpace.get()))) {
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
        case CryptoAlgorithmIdentifier::DEPRECATED_SHA_224:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE(sha224DeprecationMessage);
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

    void writeResizableArrayBuffer(std::span<const uint8_t> data, size_t maxByteLength)
    {
        write(static_cast<uint64_t>(data.size()));
        write(static_cast<uint64_t>(maxByteLength));
        write(data);
    }

    // m_buffer lives in the JSC::CloneSerializerBase template.
    Vector<URLKeepingBlobAlive>& m_blobHandles;
    ObjectPoolMap m_transferredMessagePorts;
    ObjectPoolMap m_transferredArrayBuffers;
    ObjectPoolMap m_transferredImageBitmaps;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    ObjectPoolMap m_transferredOffscreenCanvases;
#endif
#if ENABLE(WEB_RTC)
    ObjectPoolMap m_transferredRTCDataChannels;
    Vector<RefPtr<RTCEncodedAudioFrame>>& m_serializedRTCEncodedAudioFrames;
    Vector<RefPtr<RTCEncodedVideoFrame>>& m_serializedRTCEncodedVideoFrames;
#endif
    ObjectPoolMap m_transferredReadableStreams;
    ObjectPoolMap m_transferredWritableStreams;
    ObjectPoolMap m_transferredTransformStreams;
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    ObjectPoolMap m_transferredMediaSourceHandles;
#endif
    using ImageDataPool = HashMap<Ref<ImageData>, uint32_t>;
    ImageDataPool m_imageDataPool;
    SerializationContext m_context;
    ArrayBufferContentsArray& m_sharedBuffers;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<Ref<OffscreenCanvas>>& m_inMemoryOffscreenCanvases;
#endif
    Vector<Ref<MessagePort>>& m_inMemoryMessagePorts;
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray& m_wasmModules;
    WasmMemoryHandleArray& m_wasmMemoryHandles;
#endif
#if ENABLE(WEB_CODECS)
    Vector<Ref<WebCodecsEncodedVideoChunkStorage>>& m_serializedVideoChunks;
    Vector<RefPtr<WebCodecsVideoFrame>>& m_serializedVideoFrames;
    Vector<Ref<WebCodecsEncodedAudioChunkStorage>>& m_serializedAudioChunks;
    Vector<RefPtr<WebCodecsAudioData>>& m_serializedAudioData;
#endif
#if ENABLE(MEDIA_STREAM)
    ObjectPoolMap m_transferredMediaStreamTracks;
    Vector<RefPtr<MediaStreamTrack>> m_serializedMediaStreamTracks;
    ObjectPoolMap m_transferredMediaStreamTrackHandles;
#endif
    SerializationForStorage m_forStorage;
    Vector<FileSystemHandleKeepAlive> m_fileSystemHandleKeepAlives;

#if ASSERT_ENABLED
    bool m_didSeeComplexCases { false };
#endif
};
static_assert(JSC::StructuredCloneSerializerHandler<CloneSerializer>);

SerializationReturnCode CloneSerializer::serialize(JSValue in)
{
    VM& vm = m_lexicalGlobalObject->vm();
    Vector<uint32_t, 16> indexStack;
    Vector<uint32_t, 16> lengthStack;
    Vector<PropertyNameArrayBuilder, 16> propertyStack;
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
            [[fallthrough]];
            case ArrayStartVisitIndexedMember: {
                JSObject* array = inputObjectStack.last();
                uint32_t index = indexStack.last();
                if (index == lengthStack.last()) {
                    indexStack.removeLast();
                    lengthStack.removeLast();
                    write(TerminatorTag); // Terminate the indexed property section.

                    propertyStack.append(PropertyNameArrayBuilder(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    array->getOwnNonIndexPropertyNames(m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                    if (scope.exception()) [[unlikely]]
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
                if (scope.exception()) [[unlikely]]
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
                if (inObject->classInfo() != JSFinalObject::info() && inObject->classInfo() != ObjectPrototype::info())
                    return SerializationReturnCode::DataCloneError;
                inputObjectStack.append(inObject);
                indexStack.append(0);
                propertyStack.append(PropertyNameArrayBuilder(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                inObject->methodTable()->getOwnPropertyNames(inObject, m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                if (scope.exception()) [[unlikely]]
                    return SerializationReturnCode::ExistingExceptionError;
            }
            startVisitNamedMember:
            [[fallthrough]];
            case ObjectStartVisitNamedMember: {
                JSObject* object = inputObjectStack.last();
                uint32_t index = indexStack.last();
                PropertyNameArrayBuilder& properties = propertyStack.last();
                if (index == properties.size()) {
                    endObject();
                    inputObjectStack.removeLast();
                    indexStack.removeLast();
                    propertyStack.removeLast();
                    break;
                }
                inValue = getProperty(object, properties[index]);
                if (scope.exception()) [[unlikely]]
                    return SerializationReturnCode::ExistingExceptionError;

                if (!inValue) {
                    // Property was removed during serialisation
                    indexStack.last()++;
                    goto startVisitNamedMember;
                }
                write(properties[index].string());

                if (scope.exception()) [[unlikely]]
                    return SerializationReturnCode::ExistingExceptionError;

                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                if (!dumpIfTerminal(inValue, terminalCode)) {
                    stateStack.append(ObjectEndVisitNamedMember);
                    goto stateUnknown;
                }
                if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                    return terminalCode;
                [[fallthrough]];
            }
            case ObjectEndVisitNamedMember: {
                if (scope.exception()) [[unlikely]]
                    return SerializationReturnCode::ExistingExceptionError;

                indexStack.last()++;
                goto startVisitNamedMember;
            }
            mapStartState: {
                ASSERT(inValue.isObject());
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;
                JSMap* inMap = downcast<JSMap>(inValue);
                if (!addToObjectPoolIfNotDupe<MapObjectTag>(inMap))
                    break;
                write(MapObjectTag);
                JSMapIterator* iterator = JSMapIterator::create(vm, m_lexicalGlobalObject->mapIteratorStructure(), inMap, IterationKind::Entries);
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
                    ASSERT(is<JSMap>(*object));
                    propertyStack.append(PropertyNameArrayBuilder(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    object->methodTable()->getOwnPropertyNames(object, m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                    if (scope.exception()) [[unlikely]]
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
                JSSet* inSet = downcast<JSSet>(inValue);
                if (!addToObjectPoolIfNotDupe<SetObjectTag>(inSet))
                    break;
                write(SetObjectTag);
                JSSetIterator* iterator = JSSetIterator::create(vm, m_lexicalGlobalObject->setIteratorStructure(), inSet, IterationKind::Keys);
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
                    ASSERT(is<JSSet>(*object));
                    propertyStack.append(PropertyNameArrayBuilder(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    object->methodTable()->getOwnPropertyNames(object, m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                    if (scope.exception()) [[unlikely]]
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

class CloneDeserializer : public JSC::CloneDeserializerBase<CloneDeserializer> {
    using Base = JSC::CloneDeserializerBase<CloneDeserializer>;
    WTF_FORBID_HEAP_ALLOCATION;
public:
    static String deserializeString(const Vector<uint8_t>& buffer, ShouldAtomize shouldAtomize = ShouldAtomize::No)
    {
        if (buffer.isEmpty())
            return String();
        auto span = buffer.span();
        uint32_t version;
        if (!JSC::StructuredCloneInternal::readLittleEndian(span, version) || majorVersionFor(version) > CurrentMajorVersion)
            return String();
        uint8_t tag;
        if (!JSC::StructuredCloneInternal::readLittleEndian(span, tag) || tag != StringTag)
            return String();
        uint32_t length;
        if (!JSC::StructuredCloneInternal::readLittleEndian(span, length))
            return String();
        bool is8Bit = length & StringDataIs8BitFlag;
        length &= ~StringDataIs8BitFlag;
        String str;
        if (!readString(span, str, length, is8Bit, shouldAtomize))
            return String();
        return str;
    }

    static DeserializationResult deserialize(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, Vector<Ref<MessagePort>>& messagePorts, Vector<std::optional<DetachedImageBitmap>>&& detachedImageBitmaps
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases
        , const Vector<Ref<OffscreenCanvas>>& inMemoryOffscreenCanvases
#endif
        , const Vector<Ref<MessagePort>>& inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
        , Vector<Ref<RTCRtpTransformableFrame>>&& serializedRTCEncodedAudioFrames
        , Vector<Ref<RTCRtpTransformableFrame>>&& serializedRTCEncodedVideoFrames
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
        , Vector<Ref<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks
        , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames
        , Vector<Ref<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks
        , Vector<WebCodecsAudioInternalData>&& serializedAudioData
#endif
#if ENABLE(MEDIA_STREAM)
        , Vector<std::unique_ptr<MediaStreamTrackDataHolder>>&& detachedMediaStreamTracks
        , Vector<std::unique_ptr<MediaStreamTrackHandle::DataHolder>>&& detachedMediaStreamTrackHandles
#endif
        , uint64_t exposedMessagePortCount
        )
    {
        if (!buffer.size())
            return { jsNull(), SerializationReturnCode::UnspecifiedError };
        CloneDeserializer deserializer(lexicalGlobalObject, globalObject, messagePorts, arrayBufferContentsArray, buffer, blobURLs, blobFilePaths, sharedBuffers, WTF::move(detachedImageBitmaps)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            , WTF::move(detachedOffscreenCanvases)
            , inMemoryOffscreenCanvases
#endif
            , inMemoryMessagePorts
#if ENABLE(WEB_RTC)
            , WTF::move(detachedRTCDataChannels)
            , WTF::move(serializedRTCEncodedAudioFrames)
            , WTF::move(serializedRTCEncodedVideoFrames)
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
            , WTF::move(detachedMediaSourceHandles)
#endif
#if ENABLE(WEBASSEMBLY)
            , wasmModules
            , wasmMemoryHandles
#endif
#if ENABLE(WEB_CODECS)
            , WTF::move(serializedVideoChunks)
            , WTF::move(serializedVideoFrames)
            , WTF::move(serializedAudioChunks)
            , WTF::move(serializedAudioData)
#endif
#if ENABLE(MEDIA_STREAM)
            , WTF::move(detachedMediaStreamTracks)
            , WTF::move(detachedMediaStreamTrackHandles)
#endif
            );
        if (!deserializer.isValid())
            return { JSValue(), SerializationReturnCode::ValidationError };

        auto result = deserializer.deserialize();
        // Deserialize again if data may have wrong version number, see rdar://118775332.
        if (result.code != SerializationReturnCode::SuccessfullyCompleted && deserializer.shouldRetryWithVersionUpgrade()) [[unlikely]] {
        CloneDeserializer newDeserializer(lexicalGlobalObject, globalObject, messagePorts, arrayBufferContentsArray, buffer, blobURLs, blobFilePaths, sharedBuffers, deserializer.takeDetachedImageBitmaps()
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
            , deserializer.takeDetachedOffscreenCanvases()
            , inMemoryOffscreenCanvases
#endif
            , inMemoryMessagePorts
#if ENABLE(WEB_RTC)
            , deserializer.takeDetachedRTCDataChannels()
            , deserializer.takeSerializedRTCEncodedAudioFrames()
            , deserializer.takeSerializedRTCEncodedVideoFrames()
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
            , deserializer.takeDetachedMediaStreamTracks()
            , deserializer.takeDetachedMediaStreamTrackHandles()
#endif
            );
            newDeserializer.upgradeVersion();
            result = newDeserializer.deserialize();
        }

        if (messagePorts.size() > exposedMessagePortCount)
            messagePorts.resize(exposedMessagePortCount);

        return result;
    }

private:
    CloneDeserializer(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<Ref<MessagePort>>& messagePorts, ArrayBufferContentsArray* arrayBufferContents, Vector<std::optional<DetachedImageBitmap>>&& detachedImageBitmaps, const Vector<uint8_t>& buffer
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases = { }
        , const Vector<Ref<OffscreenCanvas>>& inMemoryOffscreenCanvases = { }
#endif
        , const Vector<Ref<MessagePort>>& inMemoryMessagePorts = { }
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels = { }
        , Vector<Ref<RTCRtpTransformableFrame>>&& serializedRTCEncodedAudioFrames = { }
        , Vector<Ref<RTCRtpTransformableFrame>>&& serializedRTCEncodedVideoFrames = { }
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , Vector<RefPtr<DetachedMediaSourceHandle>>&& detachedMediaSourceHandles = { }
#endif
#if ENABLE(WEBASSEMBLY)
        , WasmModuleArray* wasmModules = nullptr
        , WasmMemoryHandleArray* wasmMemoryHandles = nullptr
#endif
#if ENABLE(WEB_CODECS)
        , Vector<Ref<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks = { }
        , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames = { }
        , Vector<Ref<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks = { }
        , Vector<WebCodecsAudioInternalData>&& serializedAudioData = { }
#endif
#if ENABLE(MEDIA_STREAM)
        , Vector<std::unique_ptr<MediaStreamTrackDataHolder>>&& detachedMediaStreamTracks = { }
        , Vector<std::unique_ptr<MediaStreamTrackHandle::DataHolder>>&& detachedMediaStreamTrackHandles = { }
#endif
        )
        : Base(lexicalGlobalObject, globalObject, buffer.span())
        , m_isDOMGlobalObject(globalObject->inherits<JSDOMGlobalObject>())
        , m_canCreateDOMObject(m_isDOMGlobalObject && !globalObject->inherits<JSIDBSerializationGlobalObject>())
        , m_messagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_detachedImageBitmaps(WTF::move(detachedImageBitmaps))
        , m_imageBitmaps(m_detachedImageBitmaps.size())
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , m_detachedOffscreenCanvases(WTF::move(detachedOffscreenCanvases))
        , m_offscreenCanvases(m_detachedOffscreenCanvases.size())
        , m_inMemoryOffscreenCanvases(inMemoryOffscreenCanvases)
#endif
        , m_inMemoryMessagePorts(inMemoryMessagePorts)
#if ENABLE(WEB_RTC)
        , m_detachedRTCDataChannels(WTF::move(detachedRTCDataChannels))
        , m_rtcDataChannels(m_detachedRTCDataChannels.size())
        , m_serializedRTCEncodedAudioFrames(WTF::move(serializedRTCEncodedAudioFrames))
        , m_rtcEncodedAudioFrames(m_serializedRTCEncodedAudioFrames.size())
        , m_serializedRTCEncodedVideoFrames(WTF::move(serializedRTCEncodedVideoFrames))
        , m_rtcEncodedVideoFrames(m_serializedRTCEncodedVideoFrames.size())
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , m_detachedMediaSourceHandles(WTF::move(detachedMediaSourceHandles))
        , m_mediaSourceHandles(m_detachedMediaSourceHandles.size())
#endif
#if ENABLE(WEBASSEMBLY)
        , m_wasmModules(wasmModules)
        , m_wasmMemoryHandles(wasmMemoryHandles)
#endif
#if ENABLE(WEB_CODECS)
        , m_serializedVideoChunks(WTF::move(serializedVideoChunks))
        , m_videoChunks(m_serializedVideoChunks.size())
        , m_serializedVideoFrames(WTF::move(serializedVideoFrames))
        , m_videoFrames(m_serializedVideoFrames.size())
        , m_serializedAudioChunks(WTF::move(serializedAudioChunks))
        , m_audioChunks(m_serializedAudioChunks.size())
        , m_serializedAudioData(WTF::move(serializedAudioData))
        , m_audioData(m_serializedAudioData.size())
#endif
#if ENABLE(MEDIA_STREAM)
        , m_detachedMediaStreamTracks(WTF::move(detachedMediaStreamTracks))
        , m_mediaStreamTracks(m_detachedMediaStreamTracks.size())
        , m_detachedMediaStreamTrackHandles(WTF::move(detachedMediaStreamTrackHandles))
        , m_mediaStreamTrackHandles(m_detachedMediaStreamTrackHandles.size())
#endif
    {
        readAndStoreVersion();
    }

    CloneDeserializer(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const Vector<Ref<MessagePort>>& messagePorts, ArrayBufferContentsArray* arrayBufferContents, const Vector<uint8_t>& buffer, const Vector<String>& blobURLs, const Vector<String> blobFilePaths, ArrayBufferContentsArray* sharedBuffers, Vector<std::optional<DetachedImageBitmap>>&& detachedImageBitmaps
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , Vector<std::unique_ptr<DetachedOffscreenCanvas>>&& detachedOffscreenCanvases
        , const Vector<Ref<OffscreenCanvas>>& inMemoryOffscreenCanvases
#endif
        , const Vector<Ref<MessagePort>>& inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , Vector<std::unique_ptr<DetachedRTCDataChannel>>&& detachedRTCDataChannels
        , Vector<Ref<RTCRtpTransformableFrame>>&& serializedRTCEncodedAudioFrames
        , Vector<Ref<RTCRtpTransformableFrame>>&& serializedRTCEncodedVideoFrames
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , Vector<RefPtr<DetachedMediaSourceHandle>>&& detachedMediaSourceHandles
#endif
#if ENABLE(WEBASSEMBLY)
        , WasmModuleArray* wasmModules
        , WasmMemoryHandleArray* wasmMemoryHandles
#endif
#if ENABLE(WEB_CODECS)
        , Vector<Ref<WebCodecsEncodedVideoChunkStorage>>&& serializedVideoChunks = { }
        , Vector<WebCodecsVideoFrameData>&& serializedVideoFrames = { }
        , Vector<Ref<WebCodecsEncodedAudioChunkStorage>>&& serializedAudioChunks = { }
        , Vector<WebCodecsAudioInternalData>&& serializedAudioData = { }
#endif
#if ENABLE(MEDIA_STREAM)
        , Vector<std::unique_ptr<MediaStreamTrackDataHolder>>&& detachedMediaStreamTracks = { }
        , Vector<std::unique_ptr<MediaStreamTrackHandle::DataHolder>>&& detachedMediaStreamTrackHandles = { }
#endif
        )
        : JSC::CloneDeserializerBase<CloneDeserializer>(lexicalGlobalObject, globalObject, buffer.span())
        , m_isDOMGlobalObject(globalObject->inherits<JSDOMGlobalObject>())
        , m_canCreateDOMObject(m_isDOMGlobalObject && !globalObject->inherits<JSIDBSerializationGlobalObject>())
        , m_messagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_blobURLs(blobURLs)
        , m_blobFilePaths(blobFilePaths)
        , m_sharedBuffers(sharedBuffers)
        , m_detachedImageBitmaps(WTF::move(detachedImageBitmaps))
        , m_imageBitmaps(m_detachedImageBitmaps.size())
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , m_detachedOffscreenCanvases(WTF::move(detachedOffscreenCanvases))
        , m_offscreenCanvases(m_detachedOffscreenCanvases.size())
        , m_inMemoryOffscreenCanvases(inMemoryOffscreenCanvases)
#endif
        , m_inMemoryMessagePorts(inMemoryMessagePorts)
#if ENABLE(WEB_RTC)
        , m_detachedRTCDataChannels(WTF::move(detachedRTCDataChannels))
        , m_rtcDataChannels(m_detachedRTCDataChannels.size())
        , m_serializedRTCEncodedAudioFrames(WTF::move(serializedRTCEncodedAudioFrames))
        , m_rtcEncodedAudioFrames(m_serializedRTCEncodedAudioFrames.size())
        , m_serializedRTCEncodedVideoFrames(WTF::move(serializedRTCEncodedVideoFrames))
        , m_rtcEncodedVideoFrames(m_serializedRTCEncodedVideoFrames.size())
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , m_detachedMediaSourceHandles(WTF::move(detachedMediaSourceHandles))
        , m_mediaSourceHandles(m_detachedMediaSourceHandles.size())
#endif
#if ENABLE(WEBASSEMBLY)
        , m_wasmModules(wasmModules)
        , m_wasmMemoryHandles(wasmMemoryHandles)
#endif
#if ENABLE(WEB_CODECS)
        , m_serializedVideoChunks(WTF::move(serializedVideoChunks))
        , m_videoChunks(m_serializedVideoChunks.size())
        , m_serializedVideoFrames(WTF::move(serializedVideoFrames))
        , m_videoFrames(m_serializedVideoFrames.size())
        , m_serializedAudioChunks(WTF::move(serializedAudioChunks))
        , m_audioChunks(m_serializedAudioChunks.size())
        , m_serializedAudioData(WTF::move(serializedAudioData))
        , m_audioData(m_serializedAudioData.size())
#endif
#if ENABLE(MEDIA_STREAM)
        , m_detachedMediaStreamTracks(WTF::move(detachedMediaStreamTracks))
        , m_mediaStreamTracks(m_detachedMediaStreamTracks.size())
        , m_detachedMediaStreamTrackHandles(WTF::move(detachedMediaStreamTrackHandles))
        , m_mediaStreamTrackHandles(m_detachedMediaStreamTrackHandles.size())
#endif
    {
        readAndStoreVersion();
    }

    enum class VisitNamedMemberResult : uint8_t { Error, Break, Start, Unknown };

    template<WalkerState endState>
    ALWAYS_INLINE VisitNamedMemberResult startVisitNamedMember(MarkedVector<JSObject*, 32>& outputObjectStack, Vector<Identifier, 16>& propertyNameStack, Vector<WalkerState, 16>& stateStack, JSValue& outValue)
    {
        static_assert(endState == ArrayEndVisitNamedMember || endState == ObjectEndVisitNamedMember);
        VM& vm = m_lexicalGlobalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
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

        JSValue terminal = readTerminal();
        if (scope.exception()) [[unlikely]]
            return VisitNamedMemberResult::Error;
        if (terminal) {
            putProperty(outputObjectStack.last(), identifier, terminal);
            if (scope.exception()) [[unlikely]]
                return VisitNamedMemberResult::Error;
            return VisitNamedMemberResult::Start;
        }

        stateStack.append(endState);
        propertyNameStack.append(identifier);
        return VisitNamedMemberResult::Unknown;
    }

    ALWAYS_INLINE void objectEndVisitNamedMember(MarkedVector<JSObject*, 32>& outputObjectStack, Vector<Identifier, 16>& propertyNameStack, JSValue& outValue)
    {
        VM& vm = m_lexicalGlobalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        putProperty(outputObjectStack.last(), propertyNameStack.last(), outValue);
        if (scope.exception()) [[unlikely]]
            return;
        propertyNameStack.removeLast();
    }

    DeserializationResult deserialize();

    Vector<std::optional<DetachedImageBitmap>> takeDetachedImageBitmaps() { return std::exchange(m_detachedImageBitmaps, { }); }
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<std::unique_ptr<DetachedOffscreenCanvas>> takeDetachedOffscreenCanvases() { return std::exchange(m_detachedOffscreenCanvases, { }); }
#endif
#if ENABLE(WEB_RTC)
    Vector<std::unique_ptr<DetachedRTCDataChannel>> takeDetachedRTCDataChannels() { return std::exchange(m_detachedRTCDataChannels, { }); }
    Vector<Ref<RTCRtpTransformableFrame>> takeSerializedRTCEncodedAudioFrames() { return std::exchange(m_serializedRTCEncodedAudioFrames, { }); }
    Vector<Ref<RTCRtpTransformableFrame>> takeSerializedRTCEncodedVideoFrames() { return std::exchange(m_serializedRTCEncodedVideoFrames, { }); }
#endif
#if ENABLE(WEB_CODECS)
    Vector<Ref<WebCodecsEncodedVideoChunkStorage>> takeSerializedVideoChunks() { return std::exchange(m_serializedVideoChunks, { }); }
    Vector<WebCodecsVideoFrameData> takeSerializedVideoFrames() { return std::exchange(m_serializedVideoFrames, { }); }
    Vector<Ref<WebCodecsEncodedAudioChunkStorage>> takeSerializedAudioChunks() { return std::exchange(m_serializedAudioChunks, { }); }
    Vector<WebCodecsAudioInternalData> takeSerializedAudioData() { return std::exchange(m_serializedAudioData, { }); }
#endif
#if ENABLE(MEDIA_STREAM)
    Vector<std::unique_ptr<MediaStreamTrackDataHolder>> takeDetachedMediaStreamTracks() { return std::exchange(m_detachedMediaStreamTracks, { }); }
    Vector<std::unique_ptr<MediaStreamTrackHandle::DataHolder>> takeDetachedMediaStreamTrackHandles() { return std::exchange(m_detachedMediaStreamTrackHandles, { }); }
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<RefPtr<DetachedMediaSourceHandle>> takeDetachedMediaSourceHandles() { return std::exchange(m_detachedMediaSourceHandles, { }); }
#endif

    using Base::read;

    std::optional<uint32_t> NODELETE readImageDataIndex()
    {
        return readConstantPoolIndex(m_imageDataPool);
    }

    bool NODELETE readArrayBufferViewSubtag(ArrayBufferViewSubtag& tag)
    {
        if (m_data.empty())
            return false;
        tag = static_cast<ArrayBufferViewSubtag>(consume(m_data));
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

        file = File::deserialize(protect(executionContext(m_lexicalGlobalObject)).get(), filePath, URL { url->string() }, type->string(), name->string(), optionalLastModified);
        return true;
    }

    template<typename LengthType>
    bool readArrayBufferImpl(RefPtr<ArrayBuffer>& arrayBuffer)
    {
        LengthType length;
        if (!read(length))
            return false;
        if (m_data.size() < length)
            return false;
        arrayBuffer = ArrayBuffer::tryCreate(m_data.first(length));
        if (!arrayBuffer)
            return false;
        skip(m_data, length);
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
        if (m_data.size() < byteLength)
            return false;
        arrayBuffer = ArrayBuffer::tryCreate(byteLength, 1, maxByteLength);
        if (!arrayBuffer)
            return false;
        ASSERT(arrayBuffer->isResizableNonShared());
        memcpySpan(arrayBuffer->mutableSpan(), consumeSpan(m_data, byteLength));
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

        if (!ArrayBufferView::verifySubRangeLength(arrayBuffer->byteLength(), byteOffset, length.value_or(0), 1))
            return false;

        auto makeArrayBufferView = [&](auto&& view) -> bool {
            if (!view)
                return false;
            arrayBufferView = toJS(m_lexicalGlobalObject, downcast<JSDOMGlobalObject>(m_globalObject), view.releaseNonNull());
            return true;
        };

        switch (arrayBufferViewSubtag) {
        case DataViewTag:
            return makeArrayBufferView(DataView::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Int8ArrayTag:
            return makeArrayBufferView(Int8Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Uint8ArrayTag:
            return makeArrayBufferView(Uint8Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Uint8ClampedArrayTag:
            return makeArrayBufferView(Uint8ClampedArray::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Int16ArrayTag:
            return makeArrayBufferView(Int16Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Uint16ArrayTag:
            return makeArrayBufferView(Uint16Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Int32ArrayTag:
            return makeArrayBufferView(Int32Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Uint32ArrayTag:
            return makeArrayBufferView(Uint32Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Float16ArrayTag:
            return makeArrayBufferView(Float16Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Float32ArrayTag:
            return makeArrayBufferView(Float32Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case Float64ArrayTag:
            return makeArrayBufferView(Float64Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case BigInt64ArrayTag:
            return makeArrayBufferView(BigInt64Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        case BigUint64ArrayTag:
            return makeArrayBufferView(BigUint64Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
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
        if (static_cast<uint32_t>(m_data.size()) < size)
            return false;
        result.append(consumeSpan(m_data, size));
        return true;
    }

    bool NODELETE read(PredefinedColorSpace& result)
    {
        uint8_t tag;
        if (!read(tag))
            return false;

        switch (static_cast<PredefinedColorSpaceTag>(tag)) {
        case PredefinedColorSpaceTag::SRGB:
            result = PredefinedColorSpace::SRGB;
            return true;
        case PredefinedColorSpaceTag::SRGBLinear:
            result = PredefinedColorSpace::SRGBLinear;
            return true;
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        case PredefinedColorSpaceTag::DisplayP3:
            result = PredefinedColorSpace::DisplayP3;
            return true;
        case PredefinedColorSpaceTag::DisplayP3Linear:
            result = PredefinedColorSpace::DisplayP3Linear;
            return true;
#endif
        default:
            return false;
        }
    }

    bool NODELETE read(DestinationColorSpaceTag& tag)
    {
        if (m_data.empty())
            return false;
        tag = static_cast<DestinationColorSpaceTag>(consume(m_data));
        return true;
    }

#if PLATFORM(COCOA)
    bool read(RetainPtr<CFDataRef>& data)
    {
        uint32_t dataLength;
        if (!read(dataLength) || static_cast<uint32_t>(m_data.size()) < dataLength)
            return false;

        data = toCFDataNoCopy(m_data.first(dataLength), kCFAllocatorNull);
        if (!data)
            return false;

        skip(m_data, dataLength);
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
        case DestinationColorSpaceLinearDisplayP3Tag:
            destinationColorSpace = DestinationColorSpace::LinearDisplayP3();
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

    bool NODELETE read(CryptoKeyOKP::NamedCurve& result)
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

    bool NODELETE read(CryptoAlgorithmIdentifier& result)
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
        case CryptoAlgorithmIdentifierTag::DEPRECATED_SHA_224:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE(sha224DeprecationMessage);
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

    bool NODELETE read(CryptoKeyClassSubtag& result)
    {
        uint8_t tag;
        if (!read(tag))
            return false;
        if (tag > cryptoKeyClassSubtagMaximumValue)
            return false;
        result = static_cast<CryptoKeyClassSubtag>(tag);
        return true;
    }

    bool NODELETE read(CryptoKeyUsageTag& result)
    {
        uint8_t tag;
        if (!read(tag))
            return false;
        if (tag > cryptoKeyUsageTagMaximumValue)
            return false;
        result = static_cast<CryptoKeyUsageTag>(tag);
        return true;
    }

    bool NODELETE read(CryptoKeyAsymmetricTypeSubtag& result)
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
        result = CryptoKeyHMAC::importRaw(0, hash, WTF::move(keyData), extractable, usages);
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
        result = CryptoKeyAES::importRaw(algorithm, WTF::move(keyData), extractable, usages);
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
            result = WTF::move(key);
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
            result = WTF::move(key);
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
        result = WTF::move(key);
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
            result = CryptoKeyEC::importRaw(algorithm, curve->string(), WTF::move(keyData), extractable, usages);
            break;
        case CryptoKeyAsymmetricTypeSubtag::Private:
            result = CryptoKeyEC::importPkcs8(algorithm, curve->string(), WTF::move(keyData), extractable, usages);
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

        result = CryptoKeyOKP::importRaw(algorithm, namedCurve, WTF::move(keyData), extractable, usages);
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
        result = CryptoKeyRaw::create(algorithm, WTF::move(keyData), usages);
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
        ASSERT(result);
        cryptoKey = getJSValue(result.releaseNonNull());
        return true;
    }

    template<class T>
    JSValue getJSValue(T&& nativeObj)
    {
        if (!m_isDOMGlobalObject)
            return { };
        return toJS(m_lexicalGlobalObject, downcast<JSDOMGlobalObject>(m_globalObject), std::forward<T>(nativeObj));
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

        if (!m_isDOMGlobalObject)
            return { };
        return toJSNewlyCreated(m_lexicalGlobalObject, downcast<JSDOMGlobalObject>(m_globalObject), T::create(x, y, z, w));
    }

    template<class T>
    JSValue readDOMMatrix()
    {
        // Always read is2D as a single byte.
        // FIXME: Why not read this back as a bool since it's written as one?
        uint8_t is2DByte;
        if (!read(is2DByte))
            return { };
        bool is2D = !!is2DByte;

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
            if (!m_isDOMGlobalObject)
                return { };
            return toJSNewlyCreated(m_lexicalGlobalObject, downcast<JSDOMGlobalObject>(m_globalObject), T::create(WTF::move(matrix), DOMMatrixReadOnly::Is2D::Yes));
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
            if (!m_isDOMGlobalObject)
                return { };
            return toJSNewlyCreated(m_lexicalGlobalObject, downcast<JSDOMGlobalObject>(m_globalObject), T::create(WTF::move(matrix), DOMMatrixReadOnly::Is2D::No));
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

        if (!m_isDOMGlobalObject)
            return { };
        return toJSNewlyCreated(m_lexicalGlobalObject, downcast<JSDOMGlobalObject>(m_globalObject), T::create(x, y, width, height));
    }

    std::optional<DOMPointInit> NODELETE readDOMPointInit()
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

        if (!m_isDOMGlobalObject)
            return { };
        return toJSNewlyCreated(m_lexicalGlobalObject, downcast<JSDOMGlobalObject>(m_globalObject), DOMQuad::create(p1.value(), p2.value(), p3.value(), p4.value()));
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
            m_imageBitmaps[index] = ImageBitmap::create(*protect(executionContext(m_lexicalGlobalObject)).get(), WTF::move(*m_detachedImageBitmaps.at(index)));

        RefPtr bitmap = m_imageBitmaps[index];
        if (!bitmap)
            return jsNull();
        return getJSValue(bitmap.releaseNonNull());
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
            m_offscreenCanvases[index] = OffscreenCanvas::create(*protect(executionContext(m_lexicalGlobalObject)), WTF::move(m_detachedOffscreenCanvases.at(index)));
        return getJSValue(protect(*m_offscreenCanvases[index]));
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
        return getJSValue(m_inMemoryOffscreenCanvases[index]);
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
        if (!fingerprints.tryReserveInitialCapacity(size)) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
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
            return JSC::constructEmptyObject(m_lexicalGlobalObject, m_globalObject->objectPrototype());

        auto rtcCertificate = RTCCertificate::create(SecurityOrigin::createFromString(origin->string()), expires, WTF::move(fingerprints), certificate->takeString(), keyedMaterial->takeString());
        return toJSNewlyCreated(m_lexicalGlobalObject, downcast<JSDOMGlobalObject>(m_globalObject), WTF::move(rtcCertificate));
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
            auto detachedChannel = WTF::move(m_detachedRTCDataChannels.at(index));
            m_rtcDataChannels[index] = RTCDataChannel::create(*protect(executionContext(m_lexicalGlobalObject)), detachedChannel->identifier, WTF::move(detachedChannel->label), WTF::move(detachedChannel->options), detachedChannel->state);
        }
        return getJSValue(protect(*m_rtcDataChannels[index]));
    }

    JSValue readRTCEncodedAudioFrame()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_serializedRTCEncodedAudioFrames.size()) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        if (!m_rtcEncodedAudioFrames[index])
            m_rtcEncodedAudioFrames[index] = RTCEncodedAudioFrame::create(WTF::move(m_serializedRTCEncodedAudioFrames.at(index)));
        return getJSValue(protect(*m_rtcEncodedAudioFrames[index]));
    }

    JSValue readRTCEncodedVideoFrame()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_serializedRTCEncodedVideoFrames.size()) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        if (!m_rtcEncodedVideoFrames[index])
            m_rtcEncodedVideoFrames[index] = RTCEncodedVideoFrame::create(WTF::move(m_serializedRTCEncodedVideoFrames.at(index)));
        return getJSValue(protect(*m_rtcEncodedVideoFrames[index]));
    }
#endif
    JSValue readReadableStream()
    {
        uint32_t readableStreamIndex;
        if (!read(readableStreamIndex) || !readableStreamIndex) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
        uint32_t messagePortIndex;
        if (!read(messagePortIndex) || messagePortIndex >= m_messagePorts.size()) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        auto addResult = m_readableStreams.add(readableStreamIndex, nullptr);
        if (addResult.isNewEntry) {
            auto readableStreamOrError = ReadableStream::runTransferReceivingSteps(*downcast<JSDOMGlobalObject>(m_lexicalGlobalObject), { m_messagePorts.at(messagePortIndex).get() });
            if (readableStreamOrError.hasException()) {
                SERIALIZE_TRACE("FAIL creating readable stream");
                fail();
                return JSValue();
            }
            addResult.iterator->value = readableStreamOrError.releaseReturnValue();
        }
        return getJSValue(protect(*addResult.iterator->value));
    }
    JSValue readWritableStream()
    {
        uint32_t writableStreamIndex;
        if (!read(writableStreamIndex) || !writableStreamIndex) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
        uint32_t messagePortIndex;
        if (!read(messagePortIndex) || messagePortIndex >= m_messagePorts.size()) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        auto addResult = m_writableStreams.add(writableStreamIndex, nullptr);
        if (addResult.isNewEntry) {
            auto writableStreamOrError = WritableStream::runTransferReceivingSteps(*downcast<JSDOMGlobalObject>(m_lexicalGlobalObject), { m_messagePorts.at(messagePortIndex).get() });
            if (writableStreamOrError.hasException()) {
                SERIALIZE_TRACE("FAIL creating writable stream");
                fail();
                return JSValue();
            }
            addResult.iterator->value = writableStreamOrError.releaseReturnValue();
        }
        return getJSValue(protect(*addResult.iterator->value));
    }
    JSValue readTransformStream()
    {
        uint32_t transformStreamIndex;
        if (!read(transformStreamIndex) || !transformStreamIndex) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
        uint32_t messagePortsIndex;
        if (!read(messagePortsIndex) || !m_messagePorts.size()  || messagePortsIndex >= m_messagePorts.size() - 1) {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }

        auto addResult = m_transformStreams.add(transformStreamIndex, nullptr);
        if (addResult.isNewEntry) {
            auto transformStreamOrError = TransformStream::runTransferReceivingSteps(*downcast<JSDOMGlobalObject>(m_lexicalGlobalObject), { m_messagePorts.at(messagePortsIndex).get(), m_messagePorts.at(messagePortsIndex + 1).get() });
            if (transformStreamOrError.hasException()) {
                SERIALIZE_TRACE("FAIL creating writable stream");
                fail();
                return JSValue();
            }
            addResult.iterator->value = transformStreamOrError.releaseReturnValue();
        }
        return getJSValue(protect(*addResult.iterator->value));
    }
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
            m_videoChunks[index] = WebCodecsEncodedVideoChunk::create(WTF::move(m_serializedVideoChunks.at(index)));
        return getJSValue(protect(*m_videoChunks[index]));
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
            m_videoFrames[index] = WebCodecsVideoFrame::create(*protect(executionContext(m_lexicalGlobalObject)), WTF::move(m_serializedVideoFrames.at(index)));
        return getJSValue(protect(*m_videoFrames[index]));
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
            m_audioChunks[index] = WebCodecsEncodedAudioChunk::create(WTF::move(m_serializedAudioChunks.at(index)));
        return getJSValue(protect(*m_audioChunks[index]));
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
            m_audioData[index] = WebCodecsAudioData::create(*protect(executionContext(m_lexicalGlobalObject)), WTF::move(m_serializedAudioData.at(index)));
        return getJSValue(protect(*m_audioData[index]));
    }
#endif

#if ENABLE(MEDIA_STREAM)
    JSValue readMediaStreamTrack()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_detachedMediaStreamTracks.size()) {
            fail();
            return JSValue();
        }

        if (!m_mediaStreamTracks[index])
            m_mediaStreamTracks[index] = MediaStreamTrack::create(*protect(executionContext(m_lexicalGlobalObject)), makeUniqueRefFromNonNullUniquePtr(std::exchange(m_detachedMediaStreamTracks.at(index), { })));
        return getJSValue(protect(*m_mediaStreamTracks[index]));
    }
    JSValue readMediaStreamTrackHandle()
    {
        uint32_t index;
        bool indexSuccessfullyRead = read(index);
        if (!indexSuccessfullyRead || index >= m_detachedMediaStreamTrackHandles.size()) {
            fail();
            return JSValue();
        }

        if (!m_mediaStreamTrackHandles[index])
            m_mediaStreamTrackHandles[index] = MediaStreamTrackHandle::create(WTF::move(*std::exchange(m_detachedMediaStreamTrackHandles.at(index), { })));

        return getJSValue(protect(*m_mediaStreamTrackHandles[index]));
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
        return getJSValue(protect(*m_mediaSourceHandles[index]));
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

        auto buffer = ImageBitmap::createImageBuffer(*protect(executionContext(m_lexicalGlobalObject)), logicalSize, RenderingMode::Unaccelerated, colorSpace, resolutionScale);
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
        Ref bitmap = ImageBitmap::create(buffer.releaseNonNull(), originClean, flags.contains(ImageBitmapSerializationFlags::PremultiplyAlpha), flags.contains(ImageBitmapSerializationFlags::ForciblyPremultiplyAlpha));
        return getJSValue(WTF::move(bitmap));
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

    JSValue readFileSystemHandle()
    {
        uint8_t kindByte;
        if (!read(kindByte) || (kindByte != std::to_underlying(FileSystemHandleKind::File) && kindByte != std::to_underlying(FileSystemHandleKind::Directory))) {
            SERIALIZE_TRACE("FAIL readFileSystemHandle: invalid kind");
            fail();
            return JSValue();
        }
        auto kind = static_cast<FileSystemHandleKind>(kindByte);

        CachedStringRef name;
        if (!readStringData(name)) {
            SERIALIZE_TRACE("FAIL readFileSystemHandle: cannot read name");
            fail();
            return JSValue();
        }

        static constexpr size_t uuidSize = 16;
        if (m_data.size() < uuidSize) {
            SERIALIZE_TRACE("FAIL readFileSystemHandle: not enough data for UUID");
            fail();
            return JSValue();
        }
        auto uuid = WTF::UUID(m_data.first(uuidSize));
        skip(m_data, uuidSize);
        if (!uuid) {
            SERIALIZE_TRACE("FAIL readFileSystemHandle: invalid UUID");
            fail();
            return JSValue();
        }
        auto globalIdentifier = FileSystemHandleGlobalIdentifier(uuid);

        CachedStringRef origin;
        if (!readStringData(origin)) {
            fail();
            return JSValue();
        }

        RefPtr context = executionContext(m_lexicalGlobalObject);
        if (!context) {
            fail();
            return JSValue();
        }

        if (context->securityOrigin()->toString() != origin->string()) {
            fail();
            return JSValue();
        }

        RefPtr<FileSystemStorageConnection> connection = fileSystemStorageConnectionForContext(*context);
        if (!connection) {
            fail();
            return JSValue();
        }

        auto clientOrigin = clientOriginForContext(*context);
        if (kind == FileSystemHandleKind::File) {
            Ref handle = FileSystemFileHandle::create(*context, String { name->string() }, globalIdentifier);
            handle->markAsUnresolved(WTF::move(clientOrigin), connection.releaseNonNull());
            return getJSValue(handle);
        }
        Ref handle = FileSystemDirectoryHandle::create(*context, String { name->string() }, globalIdentifier);
        handle->markAsUnresolved(WTF::move(clientOrigin), connection.releaseNonNull());
        return getJSValue(handle);
    }

public:
    bool isTagExposed(SerializationTag tag) const
    {
        return isTypeExposedToGlobalObject(*m_globalObject, tag);
    }

    JSValue readDerivedTerminal(SerializationTag tag)
    {
        switch (tag) {
        case FileTag: {
            RefPtr<File> file;
            if (!readFile(file))
                return JSValue();
            if (!m_canCreateDOMObject)
                return jsNull();
            return toJS(m_lexicalGlobalObject, downcast<JSDOMGlobalObject>(m_globalObject), file.releaseNonNull());
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
                if (m_canCreateDOMObject)
                    files.append(file.releaseNonNull());
            }
            if (!m_canCreateDOMObject)
                return jsNull();
            return getJSValue(FileList::create(WTF::move(files)).get());
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
            if (static_cast<uint32_t>(m_data.size()) < length) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            auto bufferStart = m_data;
            skip(m_data, length);

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

            auto result = ImageData::create(width, height, resultColorSpace, { }, bufferStart.first(length));
            if (result.hasException()) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
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
            return getJSValue(Blob::deserialize(protect(executionContext(m_lexicalGlobalObject)).get(), URL { url->string() }, type->string(), size, memoryCost, blobFilePathForBlobURL(url->string())).get());
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
            // https://webassembly.github.io/spec/web-api/index.html#serialization
            CachedStringRef agentClusterID;
            bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
            if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != agentClusterIDFromGlobalObject(*m_globalObject)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_wasmModules || index >= m_wasmModules->size()) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return JSC::JSWebAssemblyModule::create(m_lexicalGlobalObject->vm(), m_globalObject->webAssemblyModuleStructure(), m_wasmModules->at(index).copyRef());
        }
        case WasmMemoryTag: {
            CachedStringRef agentClusterID;
            bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
            if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != agentClusterIDFromGlobalObject(*m_globalObject)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_wasmMemoryHandles || index >= m_wasmMemoryHandles->size()) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }

            auto& vm = m_lexicalGlobalObject->vm();
            JSWebAssemblyMemory* result = JSC::JSWebAssemblyMemory::create(vm, m_globalObject->webAssemblyMemoryStructure());
            RefPtr<Wasm::Memory> memory;
            auto handler = [&vm, result] (Wasm::Memory::GrowSuccess, PageCount oldPageCount, PageCount newPageCount) { result->growSuccessCallback(vm, oldPageCount, newPageCount); };
            if (RefPtr<SharedArrayBufferContents> contents = m_wasmMemoryHandles->at(index)) {
                if (!contents->memoryHandle()) {
                    SERIALIZE_TRACE("FAIL deserialize");
                    fail();
                    return JSValue();
                }
                memory = Wasm::Memory::create(contents.releaseNonNull(), result->memory().addressType(), WTF::move(handler));
            } else {
                // zero size & max-size.
                memory = Wasm::Memory::createZeroSized(JSC::MemorySharingMode::Shared, result->memory().addressType(), WTF::move(handler));
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
            if (!structure) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            JSValue result = JSArrayBuffer::create(m_lexicalGlobalObject->vm(), structure, WTF::move(arrayBuffer));
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
            if (!structure) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            JSValue result = JSArrayBuffer::create(m_lexicalGlobalObject->vm(), structure, WTF::move(arrayBuffer));
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
                m_arrayBuffers[index] = ArrayBuffer::create(WTF::move(m_arrayBufferContents->at(index)));
            return getJSValue(protect(*m_arrayBuffers[index]));
        }
        case SharedArrayBufferTag: {
            // https://html.spec.whatwg.org/multipage/structured-data.html#structureddeserialize
            CachedStringRef agentClusterID;
            bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
            uint32_t index = UINT_MAX;
            bool indexSuccessfullyRead = read(index);
            if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != agentClusterIDFromGlobalObject(*m_globalObject)
                || !indexSuccessfullyRead || !m_sharedBuffers || index >= m_sharedBuffers->size()) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            
            RELEASE_ASSERT(m_sharedBuffers->at(index));
            ArrayBufferContents arrayBufferContents;
            m_sharedBuffers->at(index).shareWith(arrayBufferContents);
            auto buffer = ArrayBuffer::create(WTF::move(arrayBufferContents));
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
            if (auto* globalObject = dynamicDowncast<JSDOMGlobalObject>(m_globalObject)) {
                if (RefPtr context = globalObject->scriptExecutionContext(); context && !context->isSecureContext()) {
                    SERIALIZE_TRACE("FAIL deserialize");
                    fail();
                    return JSValue();
                }
            }
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
        case RTCEncodedAudioFrameTag:
            return readRTCEncodedAudioFrame();
        case RTCEncodedVideoFrameTag:
            return readRTCEncodedVideoFrame();
#endif
        case ReadableStreamTag:
            return readReadableStream();
        case WritableStreamTag:
            return readWritableStream();
        case TransformStreamTag:
            return readTransformStream();
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
        case MediaStreamTrackHandleTag:
            return readMediaStreamTrackHandle();
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        case MediaSourceHandleTransferTag:
            return readMediaSourceHandle();
#endif
        case DOMExceptionTag:
            return readDOMException();

        case FileSystemHandleTag:
            return readFileSystemHandle();

        default:
            // Tag is not a terminal — let JSC::CloneDeserializerBase::readTerminal
            // rewind m_data so the structural walker can re-read it.
            return JSValue();
        }
    }

private:

    template<SerializationTag Tag>
    bool consumeCollectionDataTerminationIfPossible()
    {
        auto originalData = m_data;
        if (readTag() == Tag)
            return true;
        m_data = originalData;
        return false;
    }

    const bool m_isDOMGlobalObject;
    const bool m_canCreateDOMObject;
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
    const Vector<Ref<OffscreenCanvas>>& m_inMemoryOffscreenCanvases;
#endif
    const Vector<Ref<MessagePort>>& m_inMemoryMessagePorts;
#if ENABLE(WEB_RTC)
    Vector<std::unique_ptr<DetachedRTCDataChannel>> m_detachedRTCDataChannels;
    Vector<RefPtr<RTCDataChannel>> m_rtcDataChannels;
    Vector<Ref<RTCRtpTransformableFrame>> m_serializedRTCEncodedAudioFrames;
    Vector<RefPtr<RTCEncodedAudioFrame>> m_rtcEncodedAudioFrames;
    Vector<Ref<RTCRtpTransformableFrame>> m_serializedRTCEncodedVideoFrames;
    Vector<RefPtr<RTCEncodedVideoFrame>> m_rtcEncodedVideoFrames;
#endif
    HashMap<uint32_t, RefPtr<ReadableStream>> m_readableStreams;
    HashMap<uint32_t, RefPtr<WritableStream>> m_writableStreams;
    HashMap<uint32_t, RefPtr<TransformStream>> m_transformStreams;
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<RefPtr<DetachedMediaSourceHandle>> m_detachedMediaSourceHandles;
    Vector<RefPtr<MediaSourceHandle>> m_mediaSourceHandles;
#endif
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray* const m_wasmModules;
    WasmMemoryHandleArray* const m_wasmMemoryHandles;
#endif
#if ENABLE(WEB_CODECS)
    Vector<Ref<WebCodecsEncodedVideoChunkStorage>> m_serializedVideoChunks;
    Vector<RefPtr<WebCodecsEncodedVideoChunk>> m_videoChunks;
    Vector<WebCodecsVideoFrameData> m_serializedVideoFrames;
    Vector<RefPtr<WebCodecsVideoFrame>> m_videoFrames;
    Vector<Ref<WebCodecsEncodedAudioChunkStorage>> m_serializedAudioChunks;
    Vector<RefPtr<WebCodecsEncodedAudioChunk>> m_audioChunks;
    Vector<WebCodecsAudioInternalData> m_serializedAudioData;
    Vector<RefPtr<WebCodecsAudioData>> m_audioData;
#endif
#if ENABLE(MEDIA_STREAM)
    Vector<std::unique_ptr<MediaStreamTrackDataHolder>> m_detachedMediaStreamTracks;
    Vector<RefPtr<MediaStreamTrack>> m_mediaStreamTracks;
    Vector<std::unique_ptr<MediaStreamTrackHandle::DataHolder>> m_detachedMediaStreamTrackHandles;
    Vector<RefPtr<MediaStreamTrackHandle>> m_mediaStreamTrackHandles;
#endif

    String NODELETE blobFilePathForBlobURL(const String& blobURL)
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
static_assert(JSC::StructuredCloneDeserializerHandler<CloneDeserializer>);

DeserializationResult CloneDeserializer::deserialize()
{
    VM& vm = m_lexicalGlobalObject->vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

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
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            addToObjectPool<ArrayTag>(outArray);
            outputObjectStack.append(outArray);
        }
        arrayStartVisitIndexedMember:
        [[fallthrough]];
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

            JSValue terminal = readTerminal();
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            if (terminal) {
                putProperty(outputObjectStack.last(), index, terminal);
                if (scope.exception()) [[unlikely]] {
                    SERIALIZE_TRACE("FAIL deserialize");
                    fail();
                    goto error;
                }
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
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            indexStack.removeLast();
            goto arrayStartVisitIndexedMember;
        }
        arrayStartVisitNamedMember:
        case ArrayStartVisitNamedMember: {
            auto result = startVisitNamedMember<ArrayEndVisitNamedMember>(outputObjectStack, propertyNameStack, stateStack, outValue);
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            switch (result) {
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
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            goto arrayStartVisitNamedMember;
        }
        objectStartState:
        case ObjectStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion)
                return { JSValue(), SerializationReturnCode::StackOverflowError };
            JSObject* outObject = JSC::constructEmptyObject(m_lexicalGlobalObject, m_globalObject->objectPrototype());
            addToObjectPool<ObjectTag>(outObject);
            outputObjectStack.append(outObject);
        }
        startVisitNamedMember:
        [[fallthrough]];
        case ObjectStartVisitNamedMember: {
            auto result = startVisitNamedMember<ObjectEndVisitNamedMember>(outputObjectStack, propertyNameStack, stateStack, outValue);
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            switch (result) {
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
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            goto startVisitNamedMember;
        }
        mapStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion) {
                SERIALIZE_TRACE("FAIL deserialize");
                return { JSValue(), SerializationReturnCode::StackOverflowError };
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
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            mapKeyStack.removeLast();
            goto mapDataStartVisitEntry;
        }

        setStartState: {
            if (outputObjectStack.size() > maximumFilterRecursion) {
                SERIALIZE_TRACE("FAIL deserialize");
                return { JSValue(), SerializationReturnCode::StackOverflowError };
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
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            goto setDataStartVisitEntry;
        }

        stateUnknown:
        case StateUnknown:
            JSValue terminal = readTerminal();
            if (scope.exception()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                goto error;
            }
            if (terminal) {
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
    return { outValue, SerializationReturnCode::SuccessfullyCompleted };
error:
    fail();
    return { JSValue(), SerializationReturnCode::ValidationError };
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
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    JSGlobalObject* globalObject = lexicalGlobalObject;
    Vector<String> blobURLs;
    Vector<String> blobFilePaths;
    Vector<std::optional<WebCore::DetachedImageBitmap>> detachedImageBitmaps;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<std::unique_ptr<DetachedOffscreenCanvas>> detachedOffscreenCanvases;
    Vector<Ref<OffscreenCanvas>> inMemoryOffscreenCanvases;
#endif
#if ENABLE(WEB_RTC)
    Vector<std::unique_ptr<DetachedRTCDataChannel>> detachedRTCDataChannels;
    Vector<Ref<RTCRtpTransformableFrame>> serializedRTCEncodedAudioFrames;
    Vector<Ref<RTCRtpTransformableFrame>> serializedRTCEncodedVideoFrames;
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<RefPtr<DetachedMediaSourceHandle>> detachedMediaSourceHandles;
#endif
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray* wasmModules = nullptr;
    WasmMemoryHandleArray* wasmMemoryHandles = nullptr;
#endif
#if ENABLE(WEB_CODECS)
    Vector<Ref<WebCodecsEncodedVideoChunkStorage>> serializedVideoChunks;
    Vector<WebCodecsVideoFrameData> serializedVideoFrames;
    Vector<Ref<WebCodecsEncodedAudioChunkStorage>> serializedAudioChunks;
    Vector<WebCodecsAudioInternalData> serializedAudioData;
#endif

    CloneDeserializer deserializer(lexicalGlobalObject, globalObject, messagePorts, &arrayBufferContentsArray, result, blobURLs, blobFilePaths, &sharedBuffers, WTF::move(detachedImageBitmaps)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , WTF::move(detachedOffscreenCanvases)
        , inMemoryOffscreenCanvases
#endif
        , inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , WTF::move(detachedRTCDataChannels), WTF::move(serializedRTCEncodedAudioFrames), WTF::move(serializedRTCEncodedVideoFrames)
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , WTF::move(detachedMediaSourceHandles)
#endif

#if ENABLE(WEBASSEMBLY)
        , wasmModules
        , wasmMemoryHandles
#endif
#if ENABLE(WEB_CODECS)
        , WTF::move(serializedVideoChunks)
        , WTF::move(serializedVideoFrames)
        , WTF::move(serializedAudioChunks)
        , WTF::move(serializedAudioData)
#endif
        );
    RELEASE_ASSERT(deserializer.isValid());
    auto deserializationResult = deserializer.deserialize();
    if (scope.exception()) {
        scope.clearException();
        SERIALIZE_TRACE("validation abort due to exception");
        return;
    }

    if (deserializationResult.code != SerializationReturnCode::SuccessfullyCompleted) {
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

static std::unique_ptr<ArrayBufferContentsArray> copyArrayBufferContentsArray(const std::unique_ptr<ArrayBufferContentsArray>& source)
{
    if (!source)
        return nullptr;
    auto result = makeUnique<ArrayBufferContentsArray>();
    result->reserveInitialCapacity(source->size());
    for (auto& content : *source) {
        result->append(JSC::ArrayBufferContents());
        content.shareWith(result->last());
    }
    return result;
}

Ref<SerializedScriptValue> SerializedScriptValue::clone() const
{
    return create(m_internals->clone());
}

SerializedScriptValueInternals SerializedScriptValueInternals::clone() const
{
    return {
        .data = data,
        .arrayBufferContentsArray = copyArrayBufferContentsArray(arrayBufferContentsArray),
#if ENABLE(WEB_RTC)
        .detachedRTCDataChannels = detachedRTCDataChannels.map([](const auto& channel) {
            return makeUnique<DetachedRTCDataChannel>(channel->identifier, channel->label.isolatedCopy(), channel->options.isolatedCopy(), channel->state);
        }),
#endif
#if ENABLE(WEB_CODECS)
        .serializedVideoChunks = serializedVideoChunks,
        .serializedAudioChunks = serializedAudioChunks,
#endif
        .exposedMessagePortCount = exposedMessagePortCount,
        .nonSerializedDataToken = nonSerializedDataToken,
        .fileSystemHandleKeepAlives = fileSystemHandleKeepAlives.map([](const auto& alive) { return alive.copy(); }),
#if ENABLE(WEB_CODECS)
        .serializedVideoFrames = serializedVideoFrames,
        .serializedAudioData = serializedAudioData,
#endif
#if ENABLE(WEB_RTC)
        .serializedRTCEncodedAudioFrames = serializedRTCEncodedAudioFrames.map([](const auto& frame) { return frame->clone(); }),
        .serializedRTCEncodedVideoFrames = serializedRTCEncodedVideoFrames.map([](const auto& frame) { return frame->clone(); }),
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        .detachedMediaSourceHandles = detachedMediaSourceHandles,
#endif
#if ENABLE(MEDIA_STREAM)
        .detachedMediaStreamTracks = detachedMediaStreamTracks.map([](const auto& track) {
            return track->copy();
        }),
        .detachedMediaStreamTrackHandles = detachedMediaStreamTrackHandles.map([](const auto& handle) {
            return makeUnique<MediaStreamTrackHandleDataHolder>(MediaStreamTrackHandleDataHolder { handle->contextIdentifier, handle->track, handle->trackKeeper, handle->trackSourceObserver });
        }),
#endif
        .sharedBufferContentsArray = copyArrayBufferContentsArray(sharedBufferContentsArray),
        .detachedImageBitmaps = detachedImageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        .detachedOffscreenCanvases = detachedOffscreenCanvases.map([](const auto& canvas) {
            return makeUnique<DetachedOffscreenCanvas>(canvas->size(), canvas->originClean(), RefPtr { canvas->placeholderSource() });
        }),
        .inMemoryOffscreenCanvases = inMemoryOffscreenCanvases,
#endif
        .inMemoryMessagePorts = inMemoryMessagePorts,
#if ENABLE(WEBASSEMBLY)
        .wasmModulesArray = wasmModulesArray ? makeUnique<WasmModuleArray>(*wasmModulesArray) : nullptr,
        .wasmMemoryHandlesArray = wasmMemoryHandlesArray ? makeUnique<WasmMemoryHandleArray>(*wasmMemoryHandlesArray) : nullptr,
#endif
        .blobHandles = blobHandles.map([](const auto& handle) { return handle.isolatedCopy(); }),
        .memoryCost = memoryCost
    };
}

SerializedScriptValue::SerializedScriptValue(Internals&& internals)
    : m_internals(makeUnique<Internals>(WTF::move(internals)))
{
    m_internals->memoryCost = computeMemoryCost();
}

bool SerializedScriptValue::hasBlobURLs() const
{
    return !m_internals->blobHandles.isEmpty();
}

Vector<URLKeepingBlobAlive> SerializedScriptValue::blobHandles() const
{
    return crossThreadCopy(m_internals->blobHandles);
}

const Vector<uint8_t>& SerializedScriptValue::wireBytes() const
{
    return m_internals->data;
}

size_t SerializedScriptValue::memoryCost() const
{
    return m_internals->memoryCost;
}

std::unique_ptr<Vector<JSC::ArrayBufferContents>>& SerializedScriptValue::sharedBufferContentsArray()
{
    return m_internals->sharedBufferContentsArray;
}

std::optional<SerializedScriptValue::NonSerializedDataToken> SerializedScriptValue::nonSerializedDataToken() const
{
    return m_internals->nonSerializedDataToken;
}

void SerializedScriptValue::setNonSerializedDataToken(std::optional<NonSerializedDataToken> token)
{
    m_internals->nonSerializedDataToken = token;
}

RefPtr<SerializedScriptValue> SerializedScriptValue::convert(JSGlobalObject& globalObject, JSValue value)
{
    return create(globalObject, value, SerializationForStorage::Yes);
}

size_t SerializedScriptValue::computeMemoryCost() const
{
    size_t cost = m_internals->data.size();

    if (m_internals->arrayBufferContentsArray) {
        for (auto& content : *m_internals->arrayBufferContentsArray)
            cost += content.sizeInBytes();
    }

    if (m_internals->sharedBufferContentsArray) {
        for (auto& content : *m_internals->sharedBufferContentsArray)
            cost += content.sizeInBytes();
    }

    for (auto& detachedImageBitmap : m_internals->detachedImageBitmaps) {
        if (detachedImageBitmap)
            cost += detachedImageBitmap->memoryCost();
    }

#if ENABLE(WEB_RTC)
    for (auto& channel : m_internals->detachedRTCDataChannels) {
        if (channel)
            cost += channel->memoryCost();
    }
#endif
#if ENABLE(WEBASSEMBLY)
    // We are not supporting WebAssembly Module memory estimation yet.
    if (m_internals->wasmMemoryHandlesArray) {
        for (auto& content : *m_internals->wasmMemoryHandlesArray)
            cost += content->sizeInBytes(std::memory_order_relaxed);
    }
#endif
#if ENABLE(WEB_CODECS)
    for (auto& chunk : m_internals->serializedVideoChunks)
        cost += chunk->memoryCost();
    for (auto& frame : m_internals->serializedVideoFrames)
        cost += frame.memoryCost();
    for (auto& chunk : m_internals->serializedAudioChunks)
        cost += chunk->memoryCost();
    for (auto& data : m_internals->serializedAudioData)
        cost += data.memoryCost();
#endif

    for (auto& handle : m_internals->blobHandles)
        cost += handle.url().string().sizeInBytes();

    return cost;
}

static ExceptionOr<std::unique_ptr<ArrayBufferContentsArray>> transferArrayBuffers(VM& vm, const Vector<Ref<JSC::ArrayBuffer>>& arrayBuffers)
{
    if (arrayBuffers.isEmpty())
        return nullptr;

    auto contents = makeUnique<ArrayBufferContentsArray>(arrayBuffers.size());

    HashSet<Ref<JSC::ArrayBuffer>> visited;
    for (size_t arrayBufferIndex = 0; arrayBufferIndex < arrayBuffers.size(); arrayBufferIndex++) {
        if (visited.contains(arrayBuffers[arrayBufferIndex].ptr()))
            continue;
        visited.add(arrayBuffers[arrayBufferIndex].get());

        bool result = Ref { arrayBuffers[arrayBufferIndex] }->transferTo(vm, contents->at(arrayBufferIndex));
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

static bool containsDuplicates(const Vector<Ref<ImageBitmap>>& imageBitmaps)
{
    HashSet<Ref<ImageBitmap>> visited;
    for (auto& imageBitmap : imageBitmaps) {
        if (!visited.add(imageBitmap.get()))
            return true;
    }
    return false;
}


#if ENABLE(WEB_RTC)
static bool canDetachRTCDataChannels(const Vector<Ref<RTCDataChannel>>& channels)
{
    HashSet<Ref<RTCDataChannel>> visited;
    for (auto& channel : channels) {
        if (!channel->canDetach())
            return false;
        // Check the return value of add, we should not encounter duplicates.
        if (!visited.add(channel.get()))
            return false;
    }
    return true;
}
#endif

template<typename Transferable>
static std::optional<HashSet<Ref<Transferable>>> canTransfer(const Vector<Ref<Transferable>>& transferables)
{
    HashSet<Ref<Transferable>> visited;
    for (auto& transferable : transferables) {
        if (!transferable->canTransfer())
            return { };
        // Check the return value of add, we should not encounter duplicates.
        if (!visited.add(transferable.get()))
            return { };
    }
    return visited;
}

static bool validateStreams(const HashSet<Ref<ReadableStream>>& readableStreams, const HashSet<Ref<WritableStream>>& writableStreams, const Vector<Ref<TransformStream>>& transformStreams)
{
    for (auto& transform : transformStreams) {
        if (readableStreams.contains(Ref { transform->readable() }) || writableStreams.contains(Ref { transform->writable() }))
            return false;
    }
    return true;
}

#if ENABLE(MEDIA_STREAM)
static bool canDetachMediaStreamTracks(const Vector<Ref<MediaStreamTrack>>& tracks)
{
    HashSet<Ref<MediaStreamTrack>> visited;
    for (auto& track : tracks) {
        if (!visited.add(track.get()))
            return false;
    }
    return true;
}

static bool canDetachMediaStreamTrackHandles(const Vector<Ref<MediaStreamTrackHandle>>& handles)
{
    HashSet<Ref<MediaStreamTrackHandle>> visited;
    for (auto& handle : handles) {
        if (!visited.add(handle.get()).isNewEntry)
            return false;
    }
    return true;
}
#endif

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
static bool canDetachMediaSourceHandles(const Vector<Ref<MediaSourceHandle>>& handles)
{
    HashSet<Ref<MediaSourceHandle>> visited;
    for (auto& handle : handles) {
        if (!handle->canDetach())
            return false;
        // Check the return value of add, we should not encounter duplicates.
        if (!visited.add(handle.get()))
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
    return create(globalObject, value, WTF::move(transferList), messagePorts, forStorage, SerializationErrorMode::NonThrowing, serializationContext);
}

ExceptionOr<Ref<SerializedScriptValue>> SerializedScriptValue::create(JSGlobalObject& lexicalGlobalObject, JSValue value, Vector<JSC::Strong<JSC::JSObject>>&& transferList, Vector<Ref<MessagePort>>& messagePorts, SerializationForStorage forStorage, SerializationErrorMode throwExceptions, SerializationContext context)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<Ref<JSC::ArrayBuffer>> arrayBuffers;
    Vector<Ref<ImageBitmap>> imageBitmaps;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<Ref<OffscreenCanvas>> offscreenCanvases;
#endif
#if ENABLE(WEB_RTC)
    Vector<Ref<RTCDataChannel>> dataChannels;
#endif
    Vector<Ref<ReadableStream>> readableStreams;
    Vector<Ref<WritableStream>> writableStreams;
    Vector<Ref<TransformStream>> transformStreams;
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<Ref<MediaSourceHandle>> mediaSourceHandles;
#endif
#if ENABLE(WEB_CODECS)
    Vector<Ref<WebCodecsVideoFrame>> transferredVideoFrames;
    Vector<Ref<WebCodecsAudioData>> transferredAudioData;
#endif
#if ENABLE(MEDIA_STREAM)
    Vector<Ref<MediaStreamTrack>> transferredMediaStreamTracks;
    Vector<Ref<MediaStreamTrackHandle>> transferredMediaStreamTrackHandles;
#endif

    // Step 1: Check for duplicates and classify transferables into their types.
    // Per spec, detached/validity checks happen AFTER serialization of the message value.
    HashSet<JSC::Strong<JSC::JSObject>> visited;
    for (auto& transferable : transferList) {
        if (!visited.add(JSC::Strong<JSC::JSObject> { vm, transferable.get() }).isNewEntry)
            return Exception { ExceptionCode::DataCloneError, "Duplicate transferable for structured clone"_s };

        if (RefPtr arrayBuffer = toPossiblySharedArrayBuffer(vm, transferable.get())) {
            arrayBuffers.append(arrayBuffer.releaseNonNull());
            continue;
        }
        if (RefPtr port = JSMessagePort::toWrapped(vm, transferable.get())) {
            messagePorts.append(port.releaseNonNull());
            continue;
        }
        if (RefPtr imageBitmap = JSImageBitmap::toWrapped(vm, transferable.get())) {
            imageBitmaps.append(imageBitmap.releaseNonNull());
            continue;
        }
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        if (RefPtr offscreenCanvas = JSOffscreenCanvas::toWrapped(vm, transferable.get())) {
            offscreenCanvases.append(offscreenCanvas.releaseNonNull());
            continue;
        }
#endif
#if ENABLE(WEB_RTC)
        if (RefPtr channel = JSRTCDataChannel::toWrapped(vm, transferable.get())) {
            dataChannels.append(channel.releaseNonNull());
            continue;
        }
#endif
        if (RefPtr readableStream = JSReadableStream::toWrapped(vm, transferable.get())) {
            readableStreams.append(readableStream.releaseNonNull());
            continue;
        }
        if (RefPtr writableStream = JSWritableStream::toWrapped(vm, transferable.get())) {
            writableStreams.append(writableStream.releaseNonNull());
            continue;
        }
        if (RefPtr transformStream = JSTransformStream::toWrapped(vm, transferable.get())) {
            transformStreams.append(transformStream.releaseNonNull());
            continue;
        }
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        if (RefPtr handle = JSMediaSourceHandle::toWrapped(vm, transferable.get())) {
            mediaSourceHandles.append(handle.releaseNonNull());
            continue;
        }
#endif
#if ENABLE(WEB_CODECS)
        if (RefPtr videoFrame = JSWebCodecsVideoFrame::toWrapped(vm, transferable.get())) {
            transferredVideoFrames.append(videoFrame.releaseNonNull());
            continue;
        }
        if (RefPtr audioData = JSWebCodecsAudioData::toWrapped(vm, transferable.get())) {
            transferredAudioData.append(audioData.releaseNonNull());
            continue;
        }
#endif
#if ENABLE(MEDIA_STREAM)
        if (RefPtr track = JSMediaStreamTrack::toWrapped(vm, transferable.get())) {
            transferredMediaStreamTracks.append(track.releaseNonNull());
            continue;
        }
        if (RefPtr handle = JSMediaStreamTrackHandle::toWrapped(vm, transferable.get())) {
            transferredMediaStreamTrackHandles.append(handle.releaseNonNull());
            continue;
        }
#endif
        return Exception { ExceptionCode::DataCloneError };
    }

    Vector<uint8_t> buffer;
    Vector<URLKeepingBlobAlive> blobHandles;
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<Ref<OffscreenCanvas>> inMemoryOffscreenCanvases;
#endif
    Vector<Ref<MessagePort>> inMemoryMessagePorts;
#if ENABLE(WEBASSEMBLY)
    WasmModuleArray wasmModules;
    WasmMemoryHandleArray wasmMemoryHandles;
#endif
    std::unique_ptr<ArrayBufferContentsArray> sharedBuffers = makeUnique<ArrayBufferContentsArray>();
#if ENABLE(WEB_RTC)
    Vector<RefPtr<RTCEncodedAudioFrame>> serializedRTCEncodedAudioFrames;
    Vector<RefPtr<RTCEncodedVideoFrame>> serializedRTCEncodedVideoFrames;
#endif
#if ENABLE(WEB_CODECS)
    Vector<Ref<WebCodecsEncodedVideoChunkStorage>> serializedVideoChunks;
    Vector<RefPtr<WebCodecsVideoFrame>> serializedVideoFrames;
    Vector<Ref<WebCodecsEncodedAudioChunkStorage>> serializedAudioChunks;
    Vector<RefPtr<WebCodecsAudioData>> serializedAudioData;
#endif

    Vector<FileSystemHandleKeepAlive> fileSystemHandleKeepAlives;
    auto exposedMessagePortsCount = messagePorts.size();
    auto code = CloneSerializer::serialize(&lexicalGlobalObject, value, messagePorts, arrayBuffers, imageBitmaps,
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        offscreenCanvases,
        inMemoryOffscreenCanvases,
#endif
        inMemoryMessagePorts,
#if ENABLE(WEB_RTC)
        dataChannels,
        serializedRTCEncodedAudioFrames,
        serializedRTCEncodedVideoFrames,
#endif
        readableStreams,
        writableStreams,
        transformStreams,
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
        transferredMediaStreamTracks,
        transferredMediaStreamTrackHandles,
#endif
#if ENABLE(WEBASSEMBLY)
        wasmModules,
        wasmMemoryHandles,
#endif
        blobHandles, buffer, context, *sharedBuffers, forStorage,
        fileSystemHandleKeepAlives);

    // Serialization may throw an exception. If we see one, we should exit early. To satisfy
    // exception checks, we need to check the exception here. When we throw an exception, we
    // need to make sure that our exception code is set, so we raise this in the error code.
    if (scope.exception()) [[unlikely]]
        code = SerializationReturnCode::ExistingExceptionError;

    if (throwExceptions == SerializationErrorMode::Throwing)
        maybeThrowExceptionIfSerializationFailed(lexicalGlobalObject, code);

    // If maybeThrowExceptionIfSerializationFailed threw just now, or we failed with a status code
    // other than success, we should exit right now.
    if (code != SerializationReturnCode::SuccessfullyCompleted)
        return exceptionForSerializationFailure(code);

    // Step 2: Now that serialization is done, validate transferable states.
    // Per spec, detached/validity checks happen after serialization of the message value,
    // because serialization may run getters that throw or modify transferables.
    for (auto& arrayBuffer : arrayBuffers) {
        if (arrayBuffer->isDetached() || arrayBuffer->isShared())
            return Exception { ExceptionCode::DataCloneError };
        if (!arrayBuffer->isDetachable()) {
            throwVMTypeError(&lexicalGlobalObject, scope, errorMessageForTransfer(arrayBuffer.ptr()));
            return Exception { ExceptionCode::ExistingExceptionError };
        }
    }
    for (size_t i = 0; i < exposedMessagePortsCount; ++i) {
        if (messagePorts[i]->isDetached())
            return Exception { ExceptionCode::DataCloneError, "MessagePort is detached"_s };
    }
    for (auto& imageBitmap : imageBitmaps) {
        if (imageBitmap->isDetached())
            return Exception { ExceptionCode::DataCloneError };
        if (!imageBitmap->originClean())
            return Exception { ExceptionCode::DataCloneError };
    }
    if (containsDuplicates(imageBitmaps))
        return Exception { ExceptionCode::DataCloneError };
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    for (auto& offscreenCanvas : offscreenCanvases) {
        if (offscreenCanvas->renderingContext())
            return Exception { ExceptionCode::InvalidStateError };
        if (offscreenCanvas->isDetached())
            return Exception { ExceptionCode::DataCloneError };
    }
#endif
#if ENABLE(WEB_RTC)
    if (!canDetachRTCDataChannels(dataChannels))
        return Exception { ExceptionCode::DataCloneError };
#endif
    auto readableStreamSet = canTransfer<ReadableStream>(readableStreams);
    if (!readableStreamSet)
        return Exception { ExceptionCode::DataCloneError };
    auto writableStreamSet = canTransfer<WritableStream>(writableStreams);
    if (!writableStreamSet)
        return Exception { ExceptionCode::DataCloneError };
    if (!canTransfer<TransformStream>(transformStreams))
        return Exception { ExceptionCode::DataCloneError };
    if (!validateStreams(*readableStreamSet, *writableStreamSet, transformStreams))
        return Exception { ExceptionCode::DataCloneError };
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    if (!canDetachMediaSourceHandles(mediaSourceHandles))
        return Exception { ExceptionCode::DataCloneError };
#endif
#if ENABLE(WEB_CODECS)
    for (auto& videoFrame : transferredVideoFrames) {
        if (videoFrame->isDetached())
            return Exception { ExceptionCode::DataCloneError };
    }
    for (auto& audioData : transferredAudioData) {
        if (audioData->isDetached())
            return Exception { ExceptionCode::DataCloneError };
    }
#endif
#if ENABLE(MEDIA_STREAM)
    if (!canDetachMediaStreamTracks(transferredMediaStreamTracks))
        return Exception { ExceptionCode::DataCloneError };
    if (!canDetachMediaStreamTrackHandles(transferredMediaStreamTrackHandles))
        return Exception { ExceptionCode::DataCloneError };
#endif

    auto arrayBufferContentsArray = transferArrayBuffers(vm, arrayBuffers);
    if (arrayBufferContentsArray.hasException())
        return arrayBufferContentsArray.releaseException();

    auto detachedImageBitmaps = map(WTF::move(imageBitmaps), [](auto&& imageBitmap) -> std::optional<DetachedImageBitmap> {
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
    auto serializedRTCEncodedAudioFrameStorages = map(serializedRTCEncodedAudioFrames, [](auto& frame) -> Ref<RTCRtpTransformableFrame> {
        return frame->serialize();
    });
    auto serializedRTCEncodedVideoFrameStorages = map(serializedRTCEncodedVideoFrames, [](auto& frame) -> Ref<RTCRtpTransformableFrame> {
        return frame->serialize();
    });
#endif
    for (auto& readableStream : readableStreams) {
        auto detachedOrException = readableStream->runTransferSteps(downcast<JSDOMGlobalObject>(lexicalGlobalObject));
        if (detachedOrException.hasException())
            return detachedOrException.releaseException();
        messagePorts.append(detachedOrException.releaseReturnValue().readableStreamPort);
    }
    for (auto& writableStream : writableStreams) {
        auto detachedOrException = writableStream->runTransferSteps(downcast<JSDOMGlobalObject>(lexicalGlobalObject));
        if (detachedOrException.hasException())
            return detachedOrException.releaseException();
        messagePorts.append(detachedOrException.releaseReturnValue().writableStreamPort);
    }
    for (auto& transformStream : transformStreams) {
        auto detachedOrException = transformStream->runTransferSteps(downcast<JSDOMGlobalObject>(lexicalGlobalObject));
        if (detachedOrException.hasException())
            return detachedOrException.releaseException();
        auto detachedTransform = detachedOrException.releaseReturnValue();
        messagePorts.append(WTF::move(detachedTransform.readablePort));
        messagePorts.append(WTF::move(detachedTransform.writablePort));
    }
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
    auto detachedMediaStreamTrackStorages = map(transferredMediaStreamTracks, [](auto& track) -> std::unique_ptr<MediaStreamTrackDataHolder> {
        return track->detach().moveToUniquePtr();
    });
    for (auto& track : transferredMediaStreamTracks)
        track->stopTrack();
    auto detachedMediaStreamTrackHandleStorages = map(transferredMediaStreamTrackHandles, [](auto& handle) -> std::unique_ptr<MediaStreamTrackHandle::DataHolder> {
        return handle->detach().moveToUniquePtr();
    });
#endif

    return adoptRef(*new SerializedScriptValue(Internals {
        .data = WTF::move(buffer)
        , .arrayBufferContentsArray = arrayBufferContentsArray.releaseReturnValue()
#if ENABLE(WEB_RTC)
        , .detachedRTCDataChannels = WTF::move(detachedRTCDataChannels)
#endif
#if ENABLE(WEB_CODECS)
        , .serializedVideoChunks = WTF::move(serializedVideoChunks)
        , .serializedAudioChunks = WTF::move(serializedAudioChunks)
#endif
        , .exposedMessagePortCount = exposedMessagePortsCount
        , .fileSystemHandleKeepAlives = WTF::move(fileSystemHandleKeepAlives)
#if ENABLE(WEB_CODECS)
        , .serializedVideoFrames = WTF::move(serializedVideoFrameData)
        , .serializedAudioData = WTF::move(serializedAudioInternalData)
#endif
#if ENABLE(WEB_RTC)
        , .serializedRTCEncodedAudioFrames = WTF::move(serializedRTCEncodedAudioFrameStorages)
        , .serializedRTCEncodedVideoFrames = WTF::move(serializedRTCEncodedVideoFrameStorages)
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , .detachedMediaSourceHandles = WTF::move(detachedMediaSourceHandles)
#endif
#if ENABLE(MEDIA_STREAM)
        , .detachedMediaStreamTracks = WTF::move(detachedMediaStreamTrackStorages)
        , .detachedMediaStreamTrackHandles = WTF::move(detachedMediaStreamTrackHandleStorages)
#endif
        , .sharedBufferContentsArray = WTF::move(sharedBuffers)
        , .detachedImageBitmaps = WTF::move(detachedImageBitmaps)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , .detachedOffscreenCanvases = WTF::move(detachedCanvases)
        , .inMemoryOffscreenCanvases = WTF::move(inMemoryOffscreenCanvases)
#endif
        , .inMemoryMessagePorts = WTF::move(inMemoryMessagePorts)
#if ENABLE(WEBASSEMBLY)
        , .wasmModulesArray = wasmModules.isEmpty() ? nullptr : makeUnique<WasmModuleArray>(WTF::move(wasmModules))
        , .wasmMemoryHandlesArray = makeUnique<WasmMemoryHandleArray>(WTF::move(wasmMemoryHandles))
#endif
        , .blobHandles = crossThreadCopy(WTF::move(blobHandles))
    }));
}

RefPtr<SerializedScriptValue> SerializedScriptValue::create(StringView string)
{
    Vector<uint8_t> buffer;
    if (!CloneSerializer::serialize(string, buffer))
        return nullptr;

    Internals internals;
    internals.data = WTF::move(buffer);
    return adoptRef(*new SerializedScriptValue(WTF::move(internals)));
}

RefPtr<SerializedScriptValue> SerializedScriptValue::create(JSContextRef originContext, JSValueRef apiValue, JSValueRef* exception)
{
    JSGlobalObject* lexicalGlobalObject = toJS(originContext);
    VM& vm = lexicalGlobalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    JSValue value = toJS(lexicalGlobalObject, apiValue);
    auto serializedValue = SerializedScriptValue::create(*lexicalGlobalObject, value, SerializationForStorage::No);
    if (scope.exception()) [[unlikely]] {
        if (exception)
            *exception = toRef(lexicalGlobalObject, scope.exception()->value());
        scope.clearException();
        return nullptr;
    }
    ASSERT(serializedValue);
    return serializedValue;
}

Vector<uint8_t> SerializedScriptValue::serializeCryptoKey(const WebCore::CryptoKey& key)
{
    return CloneSerializer::serializeCryptoKey(key);
}

String SerializedScriptValue::toString() const
{
    return CloneDeserializer::deserializeString(m_internals->data);
}

JSValue SerializedScriptValue::deserialize(JSGlobalObject& lexicalGlobalObject, JSGlobalObject* globalObject, SerializationErrorMode throwExceptions, bool* didFail)
{
    Vector<Ref<MessagePort>> messagePorts;
    return deserialize(lexicalGlobalObject, globalObject, messagePorts, throwExceptions, didFail);
}

JSValue SerializedScriptValue::deserialize(JSGlobalObject& lexicalGlobalObject, JSGlobalObject* globalObject, Vector<Ref<MessagePort>>& messagePorts, SerializationErrorMode throwExceptions, bool* didFail)
{
    Vector<String> dummyBlobs;
    Vector<String> dummyPaths;
    return deserialize(lexicalGlobalObject, globalObject, messagePorts, dummyBlobs, dummyPaths, throwExceptions, didFail);
}

JSValue SerializedScriptValue::deserialize(JSGlobalObject& lexicalGlobalObject, JSGlobalObject* globalObject, Vector<Ref<MessagePort>>& messagePorts, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths, SerializationErrorMode throwExceptions, bool* didFail)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    DeserializationResult result = CloneDeserializer::deserialize(&lexicalGlobalObject, globalObject, messagePorts, WTF::move(m_internals->detachedImageBitmaps)
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        , WTF::move(m_internals->detachedOffscreenCanvases)
        , m_internals->inMemoryOffscreenCanvases
#endif
        , m_internals->inMemoryMessagePorts
#if ENABLE(WEB_RTC)
        , WTF::move(m_internals->detachedRTCDataChannels)
        , WTF::move(m_internals->serializedRTCEncodedAudioFrames)
        , WTF::move(m_internals->serializedRTCEncodedVideoFrames)
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        , WTF::move(m_internals->detachedMediaSourceHandles)
#endif
        , m_internals->arrayBufferContentsArray.get(), m_internals->data, blobURLs, blobFilePaths, m_internals->sharedBufferContentsArray.get()
#if ENABLE(WEBASSEMBLY)
        , m_internals->wasmModulesArray.get()
        , m_internals->wasmMemoryHandlesArray.get()
#endif
#if ENABLE(WEB_CODECS)
        , WTF::move(m_internals->serializedVideoChunks)
        , WTF::move(m_internals->serializedVideoFrames)
        , WTF::move(m_internals->serializedAudioChunks)
        , WTF::move(m_internals->serializedAudioData)
#endif
#if ENABLE(MEDIA_STREAM)
        , WTF::move(m_internals->detachedMediaStreamTracks)
        , WTF::move(m_internals->detachedMediaStreamTrackHandles)
#endif
        , m_internals->exposedMessagePortCount
        );
    if (didFail)
        *didFail = result.code != SerializationReturnCode::SuccessfullyCompleted;

    // Deserialize may throw an exception. Similar to serialize (SerializedScriptValue::create),
    // we'll ensure that we raise this.
    if (scope.exception()) [[unlikely]]
        result.code = SerializationReturnCode::ValidationError;

    if (scope.exception()) [[unlikely]]
        maybeThrowExceptionIfSerializationFailed(lexicalGlobalObject, result.code);
    if (throwExceptions == SerializationErrorMode::Throwing)
        maybeThrowExceptionIfSerializationFailed(lexicalGlobalObject, result.code);

    // Handling newly thrown exceptions is a bit simpler here since we don't deal with return codes.
    RETURN_IF_EXCEPTION(scope, jsNull());
    return result.value ? result.value : jsNull();
}

JSValueRef SerializedScriptValue::deserialize(JSContextRef destinationContext, JSValueRef* exception)
{
    JSGlobalObject* lexicalGlobalObject = toJS(destinationContext);
    VM& vm = lexicalGlobalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    JSValue value = deserialize(*lexicalGlobalObject, lexicalGlobalObject);
    if (scope.exception()) [[unlikely]] {
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
    return adoptRef(*new SerializedScriptValue(Internals { }));
}

Vector<String> SerializedScriptValue::blobURLs() const
{
    return m_internals->blobHandles.map([](auto& handle) {
        return handle.url().string().isolatedCopy();
    });
}

Vector<FileSystemHandleGlobalIdentifier> SerializedScriptValue::fileSystemHandleGlobalIdentifiers() const
{
    Vector<FileSystemHandleGlobalIdentifier> result;
    result.reserveInitialCapacity(m_internals->fileSystemHandleKeepAlives.size());
    for (auto& keepAlive : m_internals->fileSystemHandleKeepAlives) {
        if (auto identifier = keepAlive.globalIdentifier())
            result.append(*identifier);
    }
    return result;
}

void SerializedScriptValue::writeBlobsToDiskForIndexedDB(bool isEphemeral, CompletionHandler<void(IDBValue&&)>&& completionHandler)
{
    ASSERT(isMainThread());
    ASSERT(hasBlobURLs());

    // FIXME: Blobs are not supported in private browsing yet (webkit.org/b/156347).
    if (isEphemeral)
        return completionHandler({ });

    blobRegistry()->writeBlobsToTemporaryFilesForIndexedDB(blobURLs(), [completionHandler = WTF::move(completionHandler), this, protectedThis = Ref { *this }] (auto&& blobFilePaths) mutable {
        ASSERT(isMainThread());

        if (blobFilePaths.isEmpty()) {
            // We should have successfully written blobs to temporary files.
            // If we failed, then we can't successfully store this record.
            completionHandler({ });
            return;
        }

        ASSERT(m_internals->blobHandles.size() == blobFilePaths.size());

        completionHandler({ *this, blobURLs(), blobFilePaths });
    });
}

IDBValue SerializedScriptValue::writeBlobsToDiskForIndexedDBSynchronously(bool isEphemeral, JSC::VM& vm)
{
    ASSERT(!isMainThread());

    BinarySemaphore semaphore;
    IDBValue value;
    Ref protectedThis { *this };
    callOnMainThread([&protectedThis, &semaphore, &value, isEphemeral] {
        protectedThis->writeBlobsToDiskForIndexedDB(isEphemeral, [&semaphore, &value](IDBValue&& result) {
            ASSERT(isMainThread());
            value.setAsIsolatedCopy(result);

            semaphore.signal();
        });
    });
    waitWithSTWParticipation(semaphore, vm);

    return value;
}

auto SerializedScriptValue::deserializationBehavior(JSC::JSObject& object) -> DeserializationBehavior
{
    // These correspond to legacy use of m_canCreateDOMObject and m_isDOMGlobalObject.
    if (object.inherits<JSBlob>()
        || object.inherits<JSFile>()
        || object.inherits<JSFileList>()
        || object.inherits<JSImageData>())
        return DeserializationBehavior::LegacyMapToNull;

    if (object.inherits<JSDOMPoint>()
        || object.inherits<JSDOMPointReadOnly>()
        || object.inherits<JSDOMRect>()
        || object.inherits<JSDOMRectReadOnly>()
        || object.inherits<JSDOMMatrix>()
        || object.inherits<JSDOMMatrixReadOnly>()
        || object.inherits<JSDOMQuad>()
        || object.inherits<JSDOMException>())
        return DeserializationBehavior::LegacyMapToUndefined;

#if ENABLE(WEB_RTC)
    if (object.inherits<JSRTCCertificate>())
        return DeserializationBehavior::LegacyMapToEmptyObject;
#endif

    if (object.inherits<DateInstance>()
        || object.inherits<BooleanObject>()
        || object.inherits<StringObject>()
        || object.inherits<NumberObject>()
        || object.inherits<BigIntObject>()
        || object.inherits<RegExpObject>()
        || object.inherits<ErrorInstance>()
        || object.inherits<JSMessagePort>()
        || object.inherits<JSArrayBuffer>()
        || object.inherits<JSArrayBufferView>()
        || object.inherits<JSCryptoKey>()
#if ENABLE(WEBASSEMBLY)
        || object.inherits<JSWebAssemblyModule>()
        || object.inherits<JSWebAssemblyMemory>()
#endif
        || object.inherits<JSImageBitmap>()
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
        || object.inherits<JSOffscreenCanvas>()
#endif
#if ENABLE(WEB_RTC)
        || object.inherits<JSRTCDataChannel>()
        || object.inherits<JSRTCEncodedAudioFrame>()
        || object.inherits<JSRTCEncodedVideoFrame>()
#endif
#if ENABLE(WEB_CODECS)
        || object.inherits<JSWebCodecsEncodedVideoChunk>()
        || object.inherits<JSWebCodecsVideoFrame>()
        || object.inherits<JSWebCodecsEncodedAudioChunk>()
        || object.inherits<JSWebCodecsAudioData>()
#endif
#if ENABLE(MEDIA_STREAM)
        || object.inherits<JSMediaStreamTrack>()
        || object.inherits<JSMediaStreamTrackHandle>()
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
        || object.inherits<JSMediaSourceHandle>()
#endif
        || object.inherits<JSMap>()
        || object.inherits<JSSet>()
        || object.classInfo() == JSFinalObject::info())
        return DeserializationBehavior::Succeed;

    return DeserializationBehavior::Fail;
}

} // namespace WebCore
