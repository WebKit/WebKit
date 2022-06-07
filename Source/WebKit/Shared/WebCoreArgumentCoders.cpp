/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCoreArgumentCoders.h"

#include "ShareableBitmap.h"
#include "ShareableResource.h"
#include "StreamConnectionEncoder.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/BlobPart.h>
#include <WebCore/CacheQueryOptions.h>
#include <WebCore/CacheStorageConnection.h>
#include <WebCore/CompositionUnderline.h>
#include <WebCore/Credential.h>
#include <WebCore/Cursor.h>
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/DatabaseDetails.h>
#include <WebCore/DictationAlternative.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DragData.h>
#include <WebCore/EventTrackingRegions.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/File.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FilterOperation.h>
#include <WebCore/FilterOperations.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/Font.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IDBGetResult.h>
#include <WebCore/Image.h>
#include <WebCore/JSDOMExceptionHandling.h>
#include <WebCore/Length.h>
#include <WebCore/LengthBox.h>
#include <WebCore/MediaSelectionOption.h>
#include <WebCore/Pasteboard.h>
#include <WebCore/PluginData.h>
#include <WebCore/PromisedAttachmentInfo.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/RectEdges.h>
#include <WebCore/Region.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ScriptBuffer.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingCoordinator.h>
#include <WebCore/SearchPopupMenu.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SerializedAttachmentData.h>
#include <WebCore/SerializedPlatformDataCueValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerData.h>
#include <WebCore/ShareData.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/TextCheckerClient.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/TimingFunction.h>
#include <WebCore/TransformationMatrix.h>
#include <WebCore/UserStyleSheet.h>
#include <WebCore/VelocityData.h>
#include <WebCore/ViewportArguments.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include <WebCore/SelectionGeometry.h>
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include <WebCore/MediaPlaybackTargetContext.h>
#endif

#if ENABLE(MEDIA_STREAM)
#include <WebCore/CaptureDevice.h>
#include <WebCore/MediaConstraints.h>
#endif

#if ENABLE(IMAGE_ANALYSIS)
#include <WebCore/TextRecognitionResult.h>
#endif

// FIXME: Seems like we could use std::tuple to cut down the code below a lot!

namespace IPC {
using namespace WebCore;
using namespace WebKit;

#define DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(Type) \
    template<typename Encoder> \
    void ArgumentCoder<Type>::encode(Encoder& encoder, const Type& value) { SimpleArgumentCoder<Type>::encode(encoder, value); } \
    bool ArgumentCoder<Type>::decode(Decoder& decoder, Type& value) { return SimpleArgumentCoder<Type>::decode(decoder, value); } \
    std::optional<Type> ArgumentCoder<Type>::decode(Decoder& decoder) \
    { \
        Type value; \
        if (!decode(decoder, value)) \
            return std::nullopt; \
        return value; \
    } \
    template void ArgumentCoder<Type>::encode<Encoder>(Encoder&, const Type&); \
    template void ArgumentCoder<Type>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const Type&);

DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(AffineTransform)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(FloatBoxExtent)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(FloatPoint)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(FloatPoint3D)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(FloatRect)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(FloatRoundedRect)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(FloatSize)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(IntPoint)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(IntRect)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(IntSize)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(LayoutPoint)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(LayoutSize)

#if USE(CG)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(CGRect)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(CGSize)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(CGPoint)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(CGAffineTransform)
#endif

DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(DisplayList::SetInlineFillColor)
DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE(DisplayList::SetInlineStrokeColor)

#undef DEFINE_SIMPLE_ARGUMENT_CODER_FOR_SOURCE

static void encodeSharedBuffer(Encoder& encoder, const FragmentedSharedBuffer* buffer)
{
    bool isNull = !buffer;
    encoder << isNull;
    if (isNull)
        return;

    uint64_t bufferSize = buffer->size();
    encoder << bufferSize;
    if (!bufferSize)
        return;

#if USE(UNIX_DOMAIN_SOCKETS)
    // Do not use shared memory for FragmentedSharedBuffer encoding in Unix, because it's easy to reach the
    // maximum number of file descriptors open per process when sending large data in small chunks
    // over the IPC. ConnectionUnix.cpp already uses shared memory to send any IPC message that is
    // too large. See https://bugs.webkit.org/show_bug.cgi?id=208571.
    for (const auto& element : *buffer)
        encoder.encodeFixedLengthData(element.segment->data(), element.segment->size(), 1);
#else
    SharedMemory::Handle handle;
    {
        auto sharedMemoryBuffer = SharedMemory::copyBuffer(*buffer);
        sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
    }
    encoder << SharedMemory::IPCHandle { WTFMove(handle), bufferSize };
#endif
}

static WARN_UNUSED_RETURN bool decodeSharedBuffer(Decoder& decoder, RefPtr<SharedBuffer>& buffer)
{
    std::optional<bool> isNull;
    decoder >> isNull;
    if (!isNull)
        return false;

    if (*isNull) {
        buffer = nullptr;
        return true;
    }

    uint64_t bufferSize = 0;
    if (!decoder.decode(bufferSize))
        return false;

    if (!bufferSize) {
        buffer = SharedBuffer::create();
        return true;
    }

#if USE(UNIX_DOMAIN_SOCKETS)
    if (!decoder.bufferIsLargeEnoughToContain<uint8_t>(bufferSize))
        return false;

    Vector<uint8_t> data;
    data.grow(bufferSize);
    if (!decoder.decodeFixedLengthData(data.data(), data.size(), 1))
        return false;

    buffer = SharedBuffer::create(WTFMove(data));
#else
    SharedMemory::IPCHandle ipcHandle;
    if (!decoder.decode(ipcHandle))
        return false;

    auto sharedMemoryBuffer = SharedMemory::map(ipcHandle.handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return false;

    if (sharedMemoryBuffer->size() < bufferSize)
        return false;

    buffer = SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryBuffer->data()), bufferSize);
#endif

    return true;
}

static void encodeTypesAndData(Encoder& encoder, const Vector<String>& types, const Vector<RefPtr<SharedBuffer>>& data)
{
    ASSERT(types.size() == data.size());
    encoder << types;
    encoder << static_cast<uint64_t>(data.size());
    for (auto& buffer : data)
        encodeSharedBuffer(encoder, buffer.get());
}

static WARN_UNUSED_RETURN bool decodeTypesAndData(Decoder& decoder, Vector<String>& types, Vector<RefPtr<SharedBuffer>>& data)
{
    if (!decoder.decode(types))
        return false;

    uint64_t dataSize;
    if (!decoder.decode(dataSize))
        return false;

    ASSERT(dataSize == types.size());

    for (uint64_t i = 0; i < dataSize; i++) {
        RefPtr<SharedBuffer> buffer;
        if (!decodeSharedBuffer(decoder, buffer))
            return false;
        data.append(buffer);
    }

    return true;
}

void ArgumentCoder<CacheQueryOptions>::encode(Encoder& encoder, const CacheQueryOptions& options)
{
    encoder << options.ignoreSearch;
    encoder << options.ignoreMethod;
    encoder << options.ignoreVary;
    encoder << options.cacheName;
}

bool ArgumentCoder<CacheQueryOptions>::decode(Decoder& decoder, CacheQueryOptions& options)
{
    bool ignoreSearch;
    if (!decoder.decode(ignoreSearch))
        return false;
    bool ignoreMethod;
    if (!decoder.decode(ignoreMethod))
        return false;
    bool ignoreVary;
    if (!decoder.decode(ignoreVary))
        return false;
    String cacheName;
    if (!decoder.decode(cacheName))
        return false;

    options.ignoreSearch = ignoreSearch;
    options.ignoreMethod = ignoreMethod;
    options.ignoreVary = ignoreVary;
    options.cacheName = WTFMove(cacheName);
    return true;
}

void ArgumentCoder<CharacterRange>::encode(Encoder& encoder, const CharacterRange& range)
{
    encoder << static_cast<uint64_t>(range.location);
    encoder << static_cast<uint64_t>(range.length);
}

std::optional<CharacterRange> ArgumentCoder<CharacterRange>::decode(Decoder& decoder)
{
    std::optional<uint64_t> location;
    decoder >> location;
    if (!location)
        return std::nullopt;

    std::optional<uint64_t> length;
    decoder >> length;
    if (!length)
        return std::nullopt;

    return { { *location, *length } };
}

void ArgumentCoder<DOMCacheEngine::CacheInfo>::encode(Encoder& encoder, const DOMCacheEngine::CacheInfo& info)
{
    encoder << info.identifier;
    encoder << info.name;
}

auto ArgumentCoder<DOMCacheEngine::CacheInfo>::decode(Decoder& decoder) -> std::optional<DOMCacheEngine::CacheInfo>
{
    std::optional<uint64_t> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;
    
    std::optional<String> name;
    decoder >> name;
    if (!name)
        return std::nullopt;
    
    return {{ WTFMove(*identifier), WTFMove(*name) }};
}

void ArgumentCoder<DOMCacheEngine::Record>::encode(Encoder& encoder, const DOMCacheEngine::Record& record)
{
    encoder << record.identifier;

    encoder << record.requestHeadersGuard;
    encoder << record.request;
    encoder << record.options;
    encoder << record.referrer;

    encoder << record.responseHeadersGuard;
    encoder << record.response;
    encoder << record.updateResponseCounter;
    encoder << record.responseBodySize;

    WTF::switchOn(record.responseBody, [&](const Ref<SharedBuffer>& buffer) {
        encoder << true;
        encodeSharedBuffer(encoder, buffer.ptr());
    }, [&](const Ref<FormData>& formData) {
        encoder << false;
        encoder << true;
        formData->encode(encoder);
    }, [&](const std::nullptr_t&) {
        encoder << false;
        encoder << false;
    });
}

std::optional<DOMCacheEngine::Record> ArgumentCoder<DOMCacheEngine::Record>::decode(Decoder& decoder)
{
    uint64_t identifier;
    if (!decoder.decode(identifier))
        return std::nullopt;

    FetchHeaders::Guard requestHeadersGuard;
    if (!decoder.decode(requestHeadersGuard))
        return std::nullopt;

    WebCore::ResourceRequest request;
    if (!decoder.decode(request))
        return std::nullopt;

    std::optional<WebCore::FetchOptions> options;
    decoder >> options;
    if (!options)
        return std::nullopt;

    String referrer;
    if (!decoder.decode(referrer))
        return std::nullopt;

    FetchHeaders::Guard responseHeadersGuard;
    if (!decoder.decode(responseHeadersGuard))
        return std::nullopt;

    WebCore::ResourceResponse response;
    if (!decoder.decode(response))
        return std::nullopt;

    uint64_t updateResponseCounter;
    if (!decoder.decode(updateResponseCounter))
        return std::nullopt;

    uint64_t responseBodySize;
    if (!decoder.decode(responseBodySize))
        return std::nullopt;

    WebCore::DOMCacheEngine::ResponseBody responseBody;
    bool hasSharedBufferBody;
    if (!decoder.decode(hasSharedBufferBody))
        return std::nullopt;

    if (hasSharedBufferBody) {
        RefPtr<SharedBuffer> buffer;
        if (!decodeSharedBuffer(decoder, buffer))
            return std::nullopt;
        if (buffer)
            responseBody = buffer.releaseNonNull();
    } else {
        bool hasFormDataBody;
        if (!decoder.decode(hasFormDataBody))
            return std::nullopt;
        if (hasFormDataBody) {
            auto formData = FormData::decode(decoder);
            if (!formData)
                return std::nullopt;
            responseBody = formData.releaseNonNull();
        }
    }

    return {{ WTFMove(identifier), WTFMove(updateResponseCounter), WTFMove(requestHeadersGuard), WTFMove(request), WTFMove(options.value()), WTFMove(referrer), WTFMove(responseHeadersGuard), WTFMove(response), WTFMove(responseBody), responseBodySize }};
}

void ArgumentCoder<EventTrackingRegions>::encode(Encoder& encoder, const EventTrackingRegions& eventTrackingRegions)
{
    encoder << eventTrackingRegions.asynchronousDispatchRegion;
    encoder << eventTrackingRegions.eventSpecificSynchronousDispatchRegions;
}

bool ArgumentCoder<EventTrackingRegions>::decode(Decoder& decoder, EventTrackingRegions& eventTrackingRegions)
{
    std::optional<Region> asynchronousDispatchRegion;
    decoder >> asynchronousDispatchRegion;
    if (!asynchronousDispatchRegion)
        return false;
    HashMap<String, Region> eventSpecificSynchronousDispatchRegions;
    if (!decoder.decode(eventSpecificSynchronousDispatchRegions))
        return false;
    eventTrackingRegions.asynchronousDispatchRegion = WTFMove(*asynchronousDispatchRegion);
    eventTrackingRegions.eventSpecificSynchronousDispatchRegions = WTFMove(eventSpecificSynchronousDispatchRegions);
    return true;
}

template<typename Encoder>
void ArgumentCoder<TransformationMatrix>::encode(Encoder& encoder, const TransformationMatrix& transformationMatrix)
{
    encoder << transformationMatrix.m11();
    encoder << transformationMatrix.m12();
    encoder << transformationMatrix.m13();
    encoder << transformationMatrix.m14();

    encoder << transformationMatrix.m21();
    encoder << transformationMatrix.m22();
    encoder << transformationMatrix.m23();
    encoder << transformationMatrix.m24();

    encoder << transformationMatrix.m31();
    encoder << transformationMatrix.m32();
    encoder << transformationMatrix.m33();
    encoder << transformationMatrix.m34();

    encoder << transformationMatrix.m41();
    encoder << transformationMatrix.m42();
    encoder << transformationMatrix.m43();
    encoder << transformationMatrix.m44();
}

bool ArgumentCoder<TransformationMatrix>::decode(Decoder& decoder, TransformationMatrix& transformationMatrix)
{
    double m11;
    if (!decoder.decode(m11))
        return false;
    double m12;
    if (!decoder.decode(m12))
        return false;
    double m13;
    if (!decoder.decode(m13))
        return false;
    double m14;
    if (!decoder.decode(m14))
        return false;

    double m21;
    if (!decoder.decode(m21))
        return false;
    double m22;
    if (!decoder.decode(m22))
        return false;
    double m23;
    if (!decoder.decode(m23))
        return false;
    double m24;
    if (!decoder.decode(m24))
        return false;

    double m31;
    if (!decoder.decode(m31))
        return false;
    double m32;
    if (!decoder.decode(m32))
        return false;
    double m33;
    if (!decoder.decode(m33))
        return false;
    double m34;
    if (!decoder.decode(m34))
        return false;

    double m41;
    if (!decoder.decode(m41))
        return false;
    double m42;
    if (!decoder.decode(m42))
        return false;
    double m43;
    if (!decoder.decode(m43))
        return false;
    double m44;
    if (!decoder.decode(m44))
        return false;

    transformationMatrix.setMatrix(m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44);
    return true;
}

template void ArgumentCoder<TransformationMatrix>::encode<Encoder>(Encoder&, const TransformationMatrix&);
template void ArgumentCoder<TransformationMatrix>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const TransformationMatrix&);

void ArgumentCoder<LinearTimingFunction>::encode(Encoder& encoder, const LinearTimingFunction& timingFunction)
{
    encoder << timingFunction.type();
}

bool ArgumentCoder<LinearTimingFunction>::decode(Decoder&, LinearTimingFunction&)
{
    // Type is decoded by the caller. Nothing else to decode.
    return true;
}

void ArgumentCoder<CubicBezierTimingFunction>::encode(Encoder& encoder, const CubicBezierTimingFunction& timingFunction)
{
    encoder << timingFunction.type();
    
    encoder << timingFunction.x1();
    encoder << timingFunction.y1();
    encoder << timingFunction.x2();
    encoder << timingFunction.y2();
    
    encoder << timingFunction.timingFunctionPreset();
}

bool ArgumentCoder<CubicBezierTimingFunction>::decode(Decoder& decoder, CubicBezierTimingFunction& timingFunction)
{
    // Type is decoded by the caller.
    double x1;
    if (!decoder.decode(x1))
        return false;

    double y1;
    if (!decoder.decode(y1))
        return false;

    double x2;
    if (!decoder.decode(x2))
        return false;

    double y2;
    if (!decoder.decode(y2))
        return false;

    CubicBezierTimingFunction::TimingFunctionPreset preset;
    if (!decoder.decode(preset))
        return false;

    timingFunction.setValues(x1, y1, x2, y2);
    timingFunction.setTimingFunctionPreset(preset);

    return true;
}

void ArgumentCoder<StepsTimingFunction>::encode(Encoder& encoder, const StepsTimingFunction& timingFunction)
{
    encoder << timingFunction.type();
    
    encoder << timingFunction.numberOfSteps();
    encoder << timingFunction.stepPosition();
}

bool ArgumentCoder<StepsTimingFunction>::decode(Decoder& decoder, StepsTimingFunction& timingFunction)
{
    // Type is decoded by the caller.
    int numSteps;
    if (!decoder.decode(numSteps))
        return false;

    std::optional<StepsTimingFunction::StepPosition> stepPosition;
    if (!decoder.decode(stepPosition))
        return false;

    timingFunction.setNumberOfSteps(numSteps);
    timingFunction.setStepPosition(stepPosition);

    return true;
}

void ArgumentCoder<SpringTimingFunction>::encode(Encoder& encoder, const SpringTimingFunction& timingFunction)
{
    encoder << timingFunction.type();
    
    encoder << timingFunction.mass();
    encoder << timingFunction.stiffness();
    encoder << timingFunction.damping();
    encoder << timingFunction.initialVelocity();
}

bool ArgumentCoder<SpringTimingFunction>::decode(Decoder& decoder, SpringTimingFunction& timingFunction)
{
    // Type is decoded by the caller.
    double mass;
    if (!decoder.decode(mass))
        return false;

    double stiffness;
    if (!decoder.decode(stiffness))
        return false;

    double damping;
    if (!decoder.decode(damping))
        return false;

    double initialVelocity;
    if (!decoder.decode(initialVelocity))
        return false;

    timingFunction.setValues(mass, stiffness, damping, initialVelocity);

    return true;
}

void ArgumentCoder<RectEdges<bool>>::encode(Encoder& encoder, const RectEdges<bool>& boxEdges)
{
    SimpleArgumentCoder<RectEdges<bool>>::encode(encoder, boxEdges);
}
    
bool ArgumentCoder<RectEdges<bool>>::decode(Decoder& decoder, RectEdges<bool>& boxEdges)
{
    return SimpleArgumentCoder<RectEdges<bool>>::decode(decoder, boxEdges);
}

#if ENABLE(META_VIEWPORT)
void ArgumentCoder<ViewportArguments>::encode(Encoder& encoder, const ViewportArguments& viewportArguments)
{
    SimpleArgumentCoder<ViewportArguments>::encode(encoder, viewportArguments);
}

bool ArgumentCoder<ViewportArguments>::decode(Decoder& decoder, ViewportArguments& viewportArguments)
{
    return SimpleArgumentCoder<ViewportArguments>::decode(decoder, viewportArguments);
}

std::optional<ViewportArguments> ArgumentCoder<ViewportArguments>::decode(Decoder& decoder)
{
    ViewportArguments viewportArguments;
    if (!SimpleArgumentCoder<ViewportArguments>::decode(decoder, viewportArguments))
        return std::nullopt;
    return viewportArguments;
}

#endif // ENABLE(META_VIEWPORT)

void ArgumentCoder<ViewportAttributes>::encode(Encoder& encoder, const ViewportAttributes& viewportAttributes)
{
    SimpleArgumentCoder<ViewportAttributes>::encode(encoder, viewportAttributes);
}

bool ArgumentCoder<ViewportAttributes>::decode(Decoder& decoder, ViewportAttributes& viewportAttributes)
{
    return SimpleArgumentCoder<ViewportAttributes>::decode(decoder, viewportAttributes);
}

void ArgumentCoder<RecentSearch>::encode(Encoder& encoder, const RecentSearch& recentSearch)
{
    encoder << recentSearch.string << recentSearch.time;
}

std::optional<RecentSearch> ArgumentCoder<RecentSearch>::decode(Decoder& decoder)
{
    std::optional<String> string;
    decoder >> string;
    if (!string)
        return std::nullopt;
    
    std::optional<WallTime> time;
    decoder >> time;
    if (!time)
        return std::nullopt;
    
    return {{ WTFMove(*string), WTFMove(*time) }};
}

void ArgumentCoder<Length>::encode(Encoder& encoder, const Length& length)
{
    encoder << length.type() << length.hasQuirk();

    switch (length.type()) {
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::Undefined:
        break;
    case LengthType::Fixed:
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Percent:
        encoder << length.isFloat();
        if (length.isFloat())
            encoder << length.value();
        else
            encoder << length.intValue();
        break;
    case LengthType::Calculated:
        ASSERT_NOT_REACHED();
        break;
    }
}

bool ArgumentCoder<Length>::decode(Decoder& decoder, Length& length)
{
    LengthType type;
    if (!decoder.decode(type))
        return false;

    bool hasQuirk;
    if (!decoder.decode(hasQuirk))
        return false;

    switch (type) {
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::Undefined:
        length = Length(type);
        return true;
    case LengthType::Fixed:
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Percent: {
        bool isFloat;
        if (!decoder.decode(isFloat))
            return false;

        if (isFloat) {
            float value;
            if (!decoder.decode(value))
                return false;

            length = Length(value, type, hasQuirk);
        } else {
            int value;
            if (!decoder.decode(value))
                return false;

            length = Length(value, type, hasQuirk);
        }
        return true;
    }
    case LengthType::Calculated:
        ASSERT_NOT_REACHED();
        return false;
    }

    return false;
}

void ArgumentCoder<VelocityData>::encode(Encoder& encoder, const VelocityData& velocityData)
{
    encoder << velocityData.horizontalVelocity << velocityData.verticalVelocity << velocityData.scaleChangeRate << velocityData.lastUpdateTime;
}

bool ArgumentCoder<VelocityData>::decode(Decoder& decoder, VelocityData& velocityData)
{
    float horizontalVelocity;
    if (!decoder.decode(horizontalVelocity))
        return false;

    float verticalVelocity;
    if (!decoder.decode(verticalVelocity))
        return false;

    float scaleChangeRate;
    if (!decoder.decode(scaleChangeRate))
        return false;

    MonotonicTime lastUpdateTime;
    if (!decoder.decode(lastUpdateTime))
        return false;

    velocityData.horizontalVelocity = horizontalVelocity;
    velocityData.verticalVelocity = verticalVelocity;
    velocityData.scaleChangeRate = scaleChangeRate;
    velocityData.lastUpdateTime = lastUpdateTime;

    return true;
}

void ArgumentCoder<MimeClassInfo>::encode(Encoder& encoder, const MimeClassInfo& mimeClassInfo)
{
    encoder << mimeClassInfo.type << mimeClassInfo.desc << mimeClassInfo.extensions;
}

std::optional<MimeClassInfo> ArgumentCoder<MimeClassInfo>::decode(Decoder& decoder)
{
    MimeClassInfo mimeClassInfo;
    if (!decoder.decode(mimeClassInfo.type))
        return std::nullopt;
    if (!decoder.decode(mimeClassInfo.desc))
        return std::nullopt;
    if (!decoder.decode(mimeClassInfo.extensions))
        return std::nullopt;

    return mimeClassInfo;
}


void ArgumentCoder<PluginInfo>::encode(Encoder& encoder, const PluginInfo& pluginInfo)
{
    encoder << pluginInfo.name;
    encoder << pluginInfo.file;
    encoder << pluginInfo.desc;
    encoder << pluginInfo.mimes;
    encoder << pluginInfo.isApplicationPlugin;
    encoder << pluginInfo.clientLoadPolicy;
    encoder << pluginInfo.bundleIdentifier;
#if PLATFORM(MAC)
    encoder << pluginInfo.versionString;
#endif
}

std::optional<WebCore::PluginInfo> ArgumentCoder<PluginInfo>::decode(Decoder& decoder)
{
    PluginInfo pluginInfo;
    if (!decoder.decode(pluginInfo.name))
        return std::nullopt;
    if (!decoder.decode(pluginInfo.file))
        return std::nullopt;
    if (!decoder.decode(pluginInfo.desc))
        return std::nullopt;
    if (!decoder.decode(pluginInfo.mimes))
        return std::nullopt;
    if (!decoder.decode(pluginInfo.isApplicationPlugin))
        return std::nullopt;
    if (!decoder.decode(pluginInfo.clientLoadPolicy))
        return std::nullopt;
    if (!decoder.decode(pluginInfo.bundleIdentifier))
        return std::nullopt;
#if PLATFORM(MAC)
    if (!decoder.decode(pluginInfo.versionString))
        return std::nullopt;
#endif

    return pluginInfo;
}

void ArgumentCoder<AuthenticationChallenge>::encode(Encoder& encoder, const AuthenticationChallenge& challenge)
{
    encoder << challenge.protectionSpace() << challenge.proposedCredential() << challenge.previousFailureCount() << challenge.failureResponse() << challenge.error();
#if USE(SOUP)
    encoder << challenge.tlsPasswordFlags();
#endif
}

bool ArgumentCoder<AuthenticationChallenge>::decode(Decoder& decoder, AuthenticationChallenge& challenge)
{    
    ProtectionSpace protectionSpace;
    if (!decoder.decode(protectionSpace))
        return false;

    Credential proposedCredential;
    if (!decoder.decode(proposedCredential))
        return false;

    unsigned previousFailureCount;    
    if (!decoder.decode(previousFailureCount))
        return false;

    ResourceResponse failureResponse;
    if (!decoder.decode(failureResponse))
        return false;

    ResourceError error;
    if (!decoder.decode(error))
        return false;

#if USE(SOUP)
    uint32_t tlsPasswordFlags;
    if (!decoder.decode(tlsPasswordFlags))
        return false;
#endif

    challenge = AuthenticationChallenge(protectionSpace, proposedCredential, previousFailureCount, failureResponse, error);

#if USE(SOUP)
    challenge.setTLSPasswordFlags(tlsPasswordFlags);
#endif
    return true;
}


void ArgumentCoder<ProtectionSpace>::encode(Encoder& encoder, const ProtectionSpace& space)
{
    if (space.encodingRequiresPlatformData()) {
        encoder << true;
        encodePlatformData(encoder, space);
        return;
    }

    encoder << false;
    encoder << space.host() << space.port() << space.realm();
    encoder << space.authenticationScheme();
    encoder << space.serverType();
}

bool ArgumentCoder<ProtectionSpace>::decode(Decoder& decoder, ProtectionSpace& space)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;

    if (hasPlatformData)
        return decodePlatformData(decoder, space);

    String host;
    if (!decoder.decode(host))
        return false;

    int port;
    if (!decoder.decode(port))
        return false;

    String realm;
    if (!decoder.decode(realm))
        return false;
    
    ProtectionSpace::AuthenticationScheme authenticationScheme;
    if (!decoder.decode(authenticationScheme))
        return false;

    ProtectionSpace::ServerType serverType;
    if (!decoder.decode(serverType))
        return false;

    space = ProtectionSpace(host, port, serverType, realm, authenticationScheme);
    return true;
}

void ArgumentCoder<Credential>::encode(Encoder& encoder, const Credential& credential)
{
    if (credential.encodingRequiresPlatformData()) {
        encoder << true;
        encodePlatformData(encoder, credential);
        return;
    }

    encoder << false;
    encoder << credential.user() << credential.password();
    encoder << credential.persistence();
}

bool ArgumentCoder<Credential>::decode(Decoder& decoder, Credential& credential)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;

    if (hasPlatformData)
        return decodePlatformData(decoder, credential);

    String user;
    if (!decoder.decode(user))
        return false;

    String password;
    if (!decoder.decode(password))
        return false;

    CredentialPersistence persistence;
    if (!decoder.decode(persistence))
        return false;
    
    credential = Credential(user, password, persistence);
    return true;
}

static void encodeImage(Encoder& encoder, Image& image)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(IntSize(image.size()), { });
    auto graphicsContext = bitmap->createGraphicsContext();
    encoder << !!graphicsContext;
    if (!graphicsContext)
        return;

    graphicsContext->drawImage(image, IntPoint());

    ShareableBitmap::Handle handle;
    bitmap->createHandle(handle);

    encoder << handle;
}

static WARN_UNUSED_RETURN bool decodeImage(Decoder& decoder, RefPtr<Image>& image)
{
    std::optional<bool> didCreateGraphicsContext;
    decoder >> didCreateGraphicsContext;
    if (!didCreateGraphicsContext || !*didCreateGraphicsContext)
        return false;

    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;
    
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle);
    if (!bitmap)
        return false;
    image = bitmap->createImage();
    if (!image)
        return false;
    return true;
}

static void encodeOptionalImage(Encoder& encoder, Image* image)
{
    bool hasImage = !!image;
    encoder << hasImage;

    if (hasImage)
        encodeImage(encoder, *image);
}

static WARN_UNUSED_RETURN bool decodeOptionalImage(Decoder& decoder, RefPtr<Image>& image)
{
    image = nullptr;

    bool hasImage;
    if (!decoder.decode(hasImage))
        return false;

    if (!hasImage)
        return true;

    return decodeImage(decoder, image);
}

void ArgumentCoder<RefPtr<Font>>::encode(Encoder& encoder, const RefPtr<Font>& font)
{
    encoder << !!font;
    if (font)
        encoder << Ref { *font };
}

std::optional<RefPtr<Font>> ArgumentCoder<RefPtr<Font>>::decode(Decoder& decoder)
{
    std::optional<bool> hasFont;
    decoder >> hasFont;
    if (!hasFont)
        return std::nullopt;

    if (!hasFont.value())
        return std::make_optional(nullptr);

    std::optional<Ref<Font>> font;
    decoder >> font;
    if (!font)
        return std::nullopt;

    return { WTFMove(*font) };
}

void ArgumentCoder<Ref<Font>>::encode(Encoder& encoder, const Ref<WebCore::Font>& font)
{
    encoder << font->origin();
    encoder << (font->isInterstitial() ? Font::Interstitial::Yes : Font::Interstitial::No);
    encoder << font->visibility();
    encoder << (font->isTextOrientationFallback() ? Font::OrientationFallback::Yes : Font::OrientationFallback::No);
    encoder << font->renderingResourceIdentifier();
    // Intentionally don't encode m_isBrokenIdeographFallback because it doesn't affect drawGlyphs().

    encodePlatformData(encoder, font);
}

std::optional<Ref<Font>> ArgumentCoder<Ref<Font>>::decode(Decoder& decoder)
{
    std::optional<Font::Origin> origin;
    decoder >> origin;
    if (!origin)
        return std::nullopt;

    std::optional<Font::Interstitial> isInterstitial;
    decoder >> isInterstitial;
    if (!isInterstitial)
        return std::nullopt;

    std::optional<Font::Visibility> visibility;
    decoder >> visibility;
    if (!visibility)
        return std::nullopt;

    std::optional<Font::OrientationFallback> isTextOrientationFallback;
    decoder >> isTextOrientationFallback;
    if (!isTextOrientationFallback)
        return std::nullopt;

    std::optional<RenderingResourceIdentifier> renderingResourceIdentifier;
    decoder >> renderingResourceIdentifier;
    if (!renderingResourceIdentifier)
        return std::nullopt;

    auto platformData = decodePlatformData(decoder);
    if (!platformData)
        return std::nullopt;

    return Font::create(platformData.value(), origin.value(), isInterstitial.value(), visibility.value(), isTextOrientationFallback.value(), renderingResourceIdentifier);
}

void ArgumentCoder<Cursor>::encode(Encoder& encoder, const Cursor& cursor)
{
    encoder << cursor.type();
        
    if (cursor.type() != Cursor::Custom)
        return;

    if (cursor.image()->isNull()) {
        encoder << false; // There is no valid image being encoded.
        return;
    }

    encoder << true;
    encodeImage(encoder, *cursor.image());
    encoder << cursor.hotSpot();
#if ENABLE(MOUSE_CURSOR_SCALE)
    encoder << cursor.imageScaleFactor();
#endif
}

bool ArgumentCoder<Cursor>::decode(Decoder& decoder, Cursor& cursor)
{
    Cursor::Type type;
    if (!decoder.decode(type))
        return false;

    if (type > Cursor::Custom)
        return false;

    if (type != Cursor::Custom) {
        const Cursor& cursorReference = Cursor::fromType(type);
        // Calling platformCursor here will eagerly create the platform cursor for the cursor singletons inside WebCore.
        // This will avoid having to re-create the platform cursors over and over.
        (void)cursorReference.platformCursor();

        cursor = cursorReference;
        return true;
    }

    bool isValidImagePresent;
    if (!decoder.decode(isValidImagePresent))
        return false;

    if (!isValidImagePresent) {
        cursor = Cursor(&Image::nullImage(), IntPoint());
        return true;
    }

    RefPtr<Image> image;
    if (!decodeImage(decoder, image))
        return false;

    IntPoint hotSpot;
    if (!decoder.decode(hotSpot))
        return false;

    if (!image->rect().contains(hotSpot))
        return false;

#if ENABLE(MOUSE_CURSOR_SCALE)
    float scale;
    if (!decoder.decode(scale))
        return false;

    cursor = Cursor(image.get(), hotSpot, scale);
#else
    cursor = Cursor(image.get(), hotSpot);
#endif
    return true;
}

void ArgumentCoder<ResourceRequest>::encode(Encoder& encoder, const ResourceRequest& resourceRequest)
{
    if (resourceRequest.encodingRequiresPlatformData()) {
        encoder << true;
        encodePlatformData(encoder, resourceRequest);
    } else {
        encoder << false;
        resourceRequest.encodeWithoutPlatformData(encoder);
    }

    encoder << resourceRequest.cachePartition();
    encoder << resourceRequest.hiddenFromInspector();

#if USE(SYSTEM_PREVIEW)
    if (resourceRequest.isSystemPreview()) {
        encoder << true;
        encoder << resourceRequest.systemPreviewInfo();
    } else
        encoder << false;
#endif
}

bool ArgumentCoder<ResourceRequest>::decode(Decoder& decoder, ResourceRequest& resourceRequest)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;

    bool decodeSuccess = hasPlatformData ? decodePlatformData(decoder, resourceRequest) : resourceRequest.decodeWithoutPlatformData(decoder);
    if (!decodeSuccess)
        return false;

    String cachePartition;
    if (!decoder.decode(cachePartition))
        return false;
    resourceRequest.setCachePartition(cachePartition);

    bool isHiddenFromInspector;
    if (!decoder.decode(isHiddenFromInspector))
        return false;
    resourceRequest.setHiddenFromInspector(isHiddenFromInspector);

#if USE(SYSTEM_PREVIEW)
    bool isSystemPreview;
    if (!decoder.decode(isSystemPreview))
        return false;

    if (isSystemPreview) {
        SystemPreviewInfo systemPreviewInfo;
        if (!decoder.decode(systemPreviewInfo))
            return false;
        resourceRequest.setSystemPreviewInfo(systemPreviewInfo);
    }
#endif

    return true;
}

void ArgumentCoder<ResourceError>::encode(Encoder& encoder, const ResourceError& resourceError)
{
    encoder << resourceError.type();
    if (resourceError.type() == ResourceError::Type::Null)
        return;
    encodePlatformData(encoder, resourceError);
    encoder << resourceError.isSanitized();
}

bool ArgumentCoder<ResourceError>::decode(Decoder& decoder, ResourceError& resourceError)
{
    ResourceError::Type type;
    if (!decoder.decode(type))
        return false;

    if (type == ResourceError::Type::Null) {
        resourceError = { };
        return true;
    }

    if (!decodePlatformData(decoder, resourceError))
        return false;

    bool isSanitized;
    if (!decoder.decode(isSanitized))
        return false;

    resourceError.setType(type);
    if (isSanitized)
        resourceError.setAsSanitized();

    return true;
}

#if PLATFORM(IOS_FAMILY)

void ArgumentCoder<SelectionGeometry>::encode(Encoder& encoder, const SelectionGeometry& selectionGeometry)
{
    encoder << selectionGeometry.quad();
    encoder << selectionGeometry.behavior();
    encoder << static_cast<uint32_t>(selectionGeometry.direction());
    encoder << selectionGeometry.minX();
    encoder << selectionGeometry.maxX();
    encoder << selectionGeometry.maxY();
    encoder << selectionGeometry.lineNumber();
    encoder << selectionGeometry.isLineBreak();
    encoder << selectionGeometry.isFirstOnLine();
    encoder << selectionGeometry.isLastOnLine();
    encoder << selectionGeometry.containsStart();
    encoder << selectionGeometry.containsEnd();
    encoder << selectionGeometry.isHorizontal();
}

std::optional<SelectionGeometry> ArgumentCoder<SelectionGeometry>::decode(Decoder& decoder)
{
    SelectionGeometry selectionGeometry;
    FloatQuad quad;
    if (!decoder.decode(quad))
        return std::nullopt;
    selectionGeometry.setQuad(quad);

    std::optional<SelectionRenderingBehavior> behavior;
    decoder >> behavior;
    if (!behavior)
        return std::nullopt;
    selectionGeometry.setBehavior(*behavior);

    uint32_t direction;
    if (!decoder.decode(direction))
        return std::nullopt;
    selectionGeometry.setDirection((TextDirection)direction);

    int intValue;
    if (!decoder.decode(intValue))
        return std::nullopt;
    selectionGeometry.setMinX(intValue);

    if (!decoder.decode(intValue))
        return std::nullopt;
    selectionGeometry.setMaxX(intValue);

    if (!decoder.decode(intValue))
        return std::nullopt;
    selectionGeometry.setMaxY(intValue);

    if (!decoder.decode(intValue))
        return std::nullopt;
    selectionGeometry.setLineNumber(intValue);

    bool boolValue;
    if (!decoder.decode(boolValue))
        return std::nullopt;
    selectionGeometry.setIsLineBreak(boolValue);

    if (!decoder.decode(boolValue))
        return std::nullopt;
    selectionGeometry.setIsFirstOnLine(boolValue);

    if (!decoder.decode(boolValue))
        return std::nullopt;
    selectionGeometry.setIsLastOnLine(boolValue);

    if (!decoder.decode(boolValue))
        return std::nullopt;
    selectionGeometry.setContainsStart(boolValue);

    if (!decoder.decode(boolValue))
        return std::nullopt;
    selectionGeometry.setContainsEnd(boolValue);

    if (!decoder.decode(boolValue))
        return std::nullopt;
    selectionGeometry.setIsHorizontal(boolValue);

    return selectionGeometry;
}

#endif

void ArgumentCoder<WindowFeatures>::encode(Encoder& encoder, const WindowFeatures& windowFeatures)
{
    encoder << windowFeatures.x;
    encoder << windowFeatures.y;
    encoder << windowFeatures.width;
    encoder << windowFeatures.height;
    encoder << windowFeatures.menuBarVisible;
    encoder << windowFeatures.statusBarVisible;
    encoder << windowFeatures.toolBarVisible;
    encoder << windowFeatures.locationBarVisible;
    encoder << windowFeatures.scrollbarsVisible;
    encoder << windowFeatures.resizable;
    encoder << windowFeatures.fullscreen;
    encoder << windowFeatures.dialog;
}

bool ArgumentCoder<WindowFeatures>::decode(Decoder& decoder, WindowFeatures& windowFeatures)
{
    if (!decoder.decode(windowFeatures.x))
        return false;
    if (!decoder.decode(windowFeatures.y))
        return false;
    if (!decoder.decode(windowFeatures.width))
        return false;
    if (!decoder.decode(windowFeatures.height))
        return false;
    if (!decoder.decode(windowFeatures.menuBarVisible))
        return false;
    if (!decoder.decode(windowFeatures.statusBarVisible))
        return false;
    if (!decoder.decode(windowFeatures.toolBarVisible))
        return false;
    if (!decoder.decode(windowFeatures.locationBarVisible))
        return false;
    if (!decoder.decode(windowFeatures.scrollbarsVisible))
        return false;
    if (!decoder.decode(windowFeatures.resizable))
        return false;
    if (!decoder.decode(windowFeatures.fullscreen))
        return false;
    if (!decoder.decode(windowFeatures.dialog))
        return false;
    return true;
}

#if ENABLE(DRAG_SUPPORT)
void ArgumentCoder<DragData>::encode(Encoder& encoder, const DragData& dragData)
{
    encoder << dragData.clientPosition();
    encoder << dragData.globalPosition();
    encoder << dragData.draggingSourceOperationMask();
    encoder << dragData.flags();
#if PLATFORM(COCOA)
    encoder << dragData.pasteboardName();
    encoder << dragData.fileNames();
#endif
    encoder << dragData.dragDestinationActionMask();
    encoder << dragData.pageID();
}

bool ArgumentCoder<DragData>::decode(Decoder& decoder, DragData& dragData)
{
    IntPoint clientPosition;
    if (!decoder.decode(clientPosition))
        return false;

    IntPoint globalPosition;
    if (!decoder.decode(globalPosition))
        return false;

    OptionSet<DragOperation> draggingSourceOperationMask;
    if (!decoder.decode(draggingSourceOperationMask))
        return false;

    OptionSet<DragApplicationFlags> applicationFlags;
    if (!decoder.decode(applicationFlags))
        return false;

    String pasteboardName;
    Vector<String> fileNames;
#if PLATFORM(COCOA)
    if (!decoder.decode(pasteboardName))
        return false;

    if (!decoder.decode(fileNames))
        return false;
#endif

    OptionSet<DragDestinationAction> dragDestinationActionMask;
    if (!decoder.decode(dragDestinationActionMask))
        return false;

    std::optional<PageIdentifier> pageID;
    if (!decoder.decode(pageID))
        return false;

    dragData = DragData(pasteboardName, clientPosition, globalPosition, draggingSourceOperationMask, applicationFlags, dragDestinationActionMask, pageID);
    dragData.setFileNames(fileNames);

    return true;
}
#endif

void ArgumentCoder<CompositionUnderline>::encode(Encoder& encoder, const CompositionUnderline& underline)
{
    encoder << underline.startOffset;
    encoder << underline.endOffset;
    encoder << underline.thick;
    encoder << underline.compositionUnderlineColor;
    encoder << underline.color;
}

std::optional<CompositionUnderline> ArgumentCoder<CompositionUnderline>::decode(Decoder& decoder)
{
    CompositionUnderline underline;

    if (!decoder.decode(underline.startOffset))
        return std::nullopt;
    if (!decoder.decode(underline.endOffset))
        return std::nullopt;
    if (!decoder.decode(underline.thick))
        return std::nullopt;
    if (!decoder.decode(underline.compositionUnderlineColor))
        return std::nullopt;
    if (!decoder.decode(underline.color))
        return std::nullopt;

    return underline;
}

void ArgumentCoder<DatabaseDetails>::encode(Encoder& encoder, const DatabaseDetails& details)
{
    encoder << details.name();
    encoder << details.displayName();
    encoder << details.expectedUsage();
    encoder << details.currentUsage();
    encoder << details.creationTime();
    encoder << details.modificationTime();
}
    
bool ArgumentCoder<DatabaseDetails>::decode(Decoder& decoder, DatabaseDetails& details)
{
    String name;
    if (!decoder.decode(name))
        return false;

    String displayName;
    if (!decoder.decode(displayName))
        return false;

    uint64_t expectedUsage;
    if (!decoder.decode(expectedUsage))
        return false;

    uint64_t currentUsage;
    if (!decoder.decode(currentUsage))
        return false;

    std::optional<WallTime> creationTime;
    if (!decoder.decode(creationTime))
        return false;

    std::optional<WallTime> modificationTime;
    if (!decoder.decode(modificationTime))
        return false;

    details = DatabaseDetails(name, displayName, expectedUsage, currentUsage, creationTime, modificationTime);
    return true;
}

template<> struct ArgumentCoder<PasteboardCustomData::Entry> {
    static void encode(Encoder&, const PasteboardCustomData::Entry&);
    static WARN_UNUSED_RETURN bool decode(Decoder&, PasteboardCustomData::Entry&);
};

void ArgumentCoder<PasteboardCustomData::Entry>::encode(Encoder& encoder, const PasteboardCustomData::Entry& data)
{
    encoder << data.type << data.customData;

    auto& platformData = data.platformData;
    bool hasString = std::holds_alternative<String>(platformData);
    encoder << hasString;
    if (hasString)
        encoder << std::get<String>(platformData);

    bool hasBuffer = std::holds_alternative<Ref<SharedBuffer>>(platformData);
    encoder << hasBuffer;
    if (hasBuffer)
        encodeSharedBuffer(encoder, std::get<Ref<SharedBuffer>>(platformData).ptr());
}

bool ArgumentCoder<PasteboardCustomData::Entry>::decode(Decoder& decoder, PasteboardCustomData::Entry& data)
{
    if (!decoder.decode(data.type))
        return false;

    if (!decoder.decode(data.customData))
        return false;

    bool hasString;
    if (!decoder.decode(hasString))
        return false;

    if (hasString) {
        String value;
        if (!decoder.decode(value))
            return false;
        data.platformData = { WTFMove(value) };
    }

    bool hasBuffer;
    if (!decoder.decode(hasBuffer))
        return false;

    if (hasString && hasBuffer)
        return false;

    if (hasBuffer) {
        RefPtr<SharedBuffer> value;
        if (!decodeSharedBuffer(decoder, value) || !value)
            return false;
        data.platformData = { value.releaseNonNull() };
    }

    return true;
}

void ArgumentCoder<PasteboardCustomData>::encode(Encoder& encoder, const PasteboardCustomData& data)
{
    encoder << data.origin();
    encoder << data.data();
}

bool ArgumentCoder<PasteboardCustomData>::decode(Decoder& decoder, PasteboardCustomData& data)
{
    String origin;
    if (!decoder.decode(origin))
        return false;

    Vector<PasteboardCustomData::Entry> items;
    if (!decoder.decode(items))
        return false;

    data = PasteboardCustomData(WTFMove(origin), WTFMove(items));
    return true;
}

void ArgumentCoder<PasteboardURL>::encode(Encoder& encoder, const PasteboardURL& content)
{
    encoder << content.url;
    encoder << content.title;
#if PLATFORM(MAC)
    encoder << content.userVisibleForm;
#endif
#if PLATFORM(GTK)
    encoder << content.markup;
#endif
}

bool ArgumentCoder<PasteboardURL>::decode(Decoder& decoder, PasteboardURL& content)
{
    if (!decoder.decode(content.url))
        return false;

    if (!decoder.decode(content.title))
        return false;

#if PLATFORM(MAC)
    if (!decoder.decode(content.userVisibleForm))
        return false;
#endif
#if PLATFORM(GTK)
    if (!decoder.decode(content.markup))
        return false;
#endif

    return true;
}

#if PLATFORM(IOS_FAMILY)

void ArgumentCoder<InspectorOverlay::Highlight>::encode(Encoder& encoder, const InspectorOverlay::Highlight& highlight)
{
    encoder << static_cast<uint32_t>(highlight.type);
    encoder << highlight.usePageCoordinates;
    encoder << highlight.contentColor;
    encoder << highlight.contentOutlineColor;
    encoder << highlight.paddingColor;
    encoder << highlight.borderColor;
    encoder << highlight.marginColor;
    encoder << highlight.quads;
    encoder << highlight.gridHighlightOverlays;
}

bool ArgumentCoder<InspectorOverlay::Highlight>::decode(Decoder& decoder, InspectorOverlay::Highlight& highlight)
{
    uint32_t type;
    if (!decoder.decode(type))
        return false;
    highlight.type = (InspectorOverlay::Highlight::Type)type;

    if (!decoder.decode(highlight.usePageCoordinates))
        return false;
    if (!decoder.decode(highlight.contentColor))
        return false;
    if (!decoder.decode(highlight.contentOutlineColor))
        return false;
    if (!decoder.decode(highlight.paddingColor))
        return false;
    if (!decoder.decode(highlight.borderColor))
        return false;
    if (!decoder.decode(highlight.marginColor))
        return false;
    if (!decoder.decode(highlight.quads))
        return false;
    if (!decoder.decode(highlight.gridHighlightOverlays))
        return false;
    return true;
}

void ArgumentCoder<PasteboardWebContent>::encode(Encoder& encoder, const PasteboardWebContent& content)
{
    encoder << content.contentOrigin;
    encoder << content.canSmartCopyOrDelete;
    encoder << content.dataInStringFormat;
    encoder << content.dataInHTMLFormat;

    encodeSharedBuffer(encoder, content.dataInWebArchiveFormat.get());
    encodeSharedBuffer(encoder, content.dataInRTFDFormat.get());
    encodeSharedBuffer(encoder, content.dataInRTFFormat.get());
    encodeSharedBuffer(encoder, content.dataInAttributedStringFormat.get());

    encodeTypesAndData(encoder, content.clientTypes, content.clientData);
}

bool ArgumentCoder<PasteboardWebContent>::decode(Decoder& decoder, PasteboardWebContent& content)
{
    if (!decoder.decode(content.contentOrigin))
        return false;
    if (!decoder.decode(content.canSmartCopyOrDelete))
        return false;
    if (!decoder.decode(content.dataInStringFormat))
        return false;
    if (!decoder.decode(content.dataInHTMLFormat))
        return false;
    if (!decodeSharedBuffer(decoder, content.dataInWebArchiveFormat))
        return false;
    if (!decodeSharedBuffer(decoder, content.dataInRTFDFormat))
        return false;
    if (!decodeSharedBuffer(decoder, content.dataInRTFFormat))
        return false;
    if (!decodeSharedBuffer(decoder, content.dataInAttributedStringFormat))
        return false;
    if (!decodeTypesAndData(decoder, content.clientTypes, content.clientData))
        return false;
    return true;
}

void ArgumentCoder<PasteboardImage>::encode(Encoder& encoder, const PasteboardImage& pasteboardImage)
{
    encodeOptionalImage(encoder, pasteboardImage.image.get());
    encoder << pasteboardImage.url.url;
    encoder << pasteboardImage.url.title;
    encoder << pasteboardImage.resourceMIMEType;
    encoder << pasteboardImage.suggestedName;
    encoder << pasteboardImage.imageSize;
    if (pasteboardImage.resourceData)
        encodeSharedBuffer(encoder, pasteboardImage.resourceData.get());
    encodeTypesAndData(encoder, pasteboardImage.clientTypes, pasteboardImage.clientData);
}

bool ArgumentCoder<PasteboardImage>::decode(Decoder& decoder, PasteboardImage& pasteboardImage)
{
    if (!decodeOptionalImage(decoder, pasteboardImage.image))
        return false;
    if (!decoder.decode(pasteboardImage.url.url))
        return false;
    if (!decoder.decode(pasteboardImage.url.title))
        return false;
    if (!decoder.decode(pasteboardImage.resourceMIMEType))
        return false;
    if (!decoder.decode(pasteboardImage.suggestedName))
        return false;
    if (!decoder.decode(pasteboardImage.imageSize))
        return false;
    if (!decodeSharedBuffer(decoder, pasteboardImage.resourceData))
        return false;
    if (!decodeTypesAndData(decoder, pasteboardImage.clientTypes, pasteboardImage.clientData))
        return false;
    return true;
}

#endif

#if USE(LIBWPE)
void ArgumentCoder<PasteboardWebContent>::encode(Encoder& encoder, const PasteboardWebContent& content)
{
    encoder << content.text;
    encoder << content.markup;
}

bool ArgumentCoder<PasteboardWebContent>::decode(Decoder& decoder, PasteboardWebContent& content)
{
    if (!decoder.decode(content.text))
        return false;
    if (!decoder.decode(content.markup))
        return false;
    return true;
}
#endif // USE(LIBWPE)

void ArgumentCoder<DictationAlternative>::encode(Encoder& encoder, const DictationAlternative& alternative)
{
    encoder << alternative.range << alternative.context;
}

std::optional<DictationAlternative> ArgumentCoder<DictationAlternative>::decode(Decoder& decoder)
{
    std::optional<CharacterRange> range;
    decoder >> range;
    if (!range)
        return std::nullopt;

    std::optional<DictationContext> context;
    decoder >> context;
    if (!context)
        return std::nullopt;

    return {{ *range, *context }};
}

void ArgumentCoder<FileChooserSettings>::encode(Encoder& encoder, const FileChooserSettings& settings)
{
    encoder << settings.allowsDirectories;
    encoder << settings.allowsMultipleFiles;
    encoder << settings.acceptMIMETypes;
    encoder << settings.acceptFileExtensions;
    encoder << settings.selectedFiles;
#if ENABLE(MEDIA_CAPTURE)
    encoder << settings.mediaCaptureType;
#endif
}

bool ArgumentCoder<FileChooserSettings>::decode(Decoder& decoder, FileChooserSettings& settings)
{
    if (!decoder.decode(settings.allowsDirectories))
        return false;
    if (!decoder.decode(settings.allowsMultipleFiles))
        return false;
    if (!decoder.decode(settings.acceptMIMETypes))
        return false;
    if (!decoder.decode(settings.acceptFileExtensions))
        return false;
    if (!decoder.decode(settings.selectedFiles))
        return false;
#if ENABLE(MEDIA_CAPTURE)
    if (!decoder.decode(settings.mediaCaptureType))
        return false;
#endif

    return true;
}

void ArgumentCoder<RawFile>::encode(Encoder& encoder, const RawFile& file)
{
    encoder << file.fileName;
    encodeSharedBuffer(encoder, file.fileData.get());
}

bool ArgumentCoder<RawFile>::decode(Decoder& decoder, RawFile& file)
{
    if (!decoder.decode(file.fileName))
        return false;
    if (!decodeSharedBuffer(decoder, file.fileData))
        return false;
    return true;
}
    
void ArgumentCoder<ShareData>::encode(Encoder& encoder, const ShareData& settings)
{
    encoder << settings.title;
    encoder << settings.text;
    encoder << settings.url;
}

bool ArgumentCoder<ShareData>::decode(Decoder& decoder, ShareData& settings)
{
    if (!decoder.decode(settings.title))
        return false;
    if (!decoder.decode(settings.text))
        return false;
    if (!decoder.decode(settings.url))
        return false;
    return true;
}
    
void ArgumentCoder<ShareDataWithParsedURL>::encode(Encoder& encoder, const ShareDataWithParsedURL& settings)
{
    encoder << settings.shareData;
    encoder << settings.url;
    encoder << settings.files;
}

bool ArgumentCoder<ShareDataWithParsedURL>::decode(Decoder& decoder, ShareDataWithParsedURL& settings)
{
    if (!decoder.decode(settings.shareData))
        return false;
    if (!decoder.decode(settings.url))
        return false;
    if (!decoder.decode(settings.files))
        return false;
    return true;
}

void ArgumentCoder<GrammarDetail>::encode(Encoder& encoder, const GrammarDetail& detail)
{
    encoder << detail.range;
    encoder << detail.guesses;
    encoder << detail.userDescription;
}

std::optional<GrammarDetail> ArgumentCoder<GrammarDetail>::decode(Decoder& decoder)
{
    std::optional<CharacterRange> range;
    decoder >> range;
    if (!range)
        return std::nullopt;

    std::optional<Vector<String>> guesses;
    decoder >> guesses;
    if (!guesses)
        return std::nullopt;

    std::optional<String> userDescription;
    decoder >> userDescription;
    if (!userDescription)
        return std::nullopt;

    return { { *range, WTFMove(*guesses), WTFMove(*userDescription) } };
}

void ArgumentCoder<TextCheckingRequestData>::encode(Encoder& encoder, const TextCheckingRequestData& request)
{
    encoder << request.identifier();
    encoder << request.text();
    encoder << request.checkingTypes();
    encoder << request.processType();
}

bool ArgumentCoder<TextCheckingRequestData>::decode(Decoder& decoder, TextCheckingRequestData& request)
{
    std::optional<TextCheckingRequestIdentifier> identifier;
    if (!decoder.decode(identifier))
        return false;

    String text;
    if (!decoder.decode(text))
        return false;

    OptionSet<TextCheckingType> checkingTypes;
    if (!decoder.decode(checkingTypes))
        return false;

    TextCheckingProcessType processType;
    if (!decoder.decode(processType))
        return false;

    request = TextCheckingRequestData { identifier, text, checkingTypes, processType };
    return true;
}

void ArgumentCoder<TextCheckingResult>::encode(Encoder& encoder, const TextCheckingResult& result)
{
    encoder << result.type;
    encoder << result.range;
    encoder << result.details;
    encoder << result.replacement;
}

std::optional<TextCheckingResult> ArgumentCoder<TextCheckingResult>::decode(Decoder& decoder)
{
    TextCheckingType type;
    if (!decoder.decode(type))
        return std::nullopt;
    
    std::optional<CharacterRange> range;
    decoder >> range;
    if (!range)
        return std::nullopt;

    std::optional<Vector<GrammarDetail>> details;
    decoder >> details;
    if (!details)
        return std::nullopt;

    std::optional<String> replacement;
    decoder >> replacement;
    if (!replacement)
        return std::nullopt;

    return { { type, *range, WTFMove(*details), WTFMove(*replacement) } };
}

void ArgumentCoder<UserStyleSheet>::encode(Encoder& encoder, const UserStyleSheet& userStyleSheet)
{
    encoder << userStyleSheet.source();
    encoder << userStyleSheet.url();
    encoder << userStyleSheet.allowlist();
    encoder << userStyleSheet.blocklist();
    encoder << userStyleSheet.injectedFrames();
    encoder << userStyleSheet.level();
    encoder << userStyleSheet.pageID();
}

bool ArgumentCoder<UserStyleSheet>::decode(Decoder& decoder, UserStyleSheet& userStyleSheet)
{
    String source;
    if (!decoder.decode(source))
        return false;

    URL url;
    if (!decoder.decode(url))
        return false;

    Vector<String> allowlist;
    if (!decoder.decode(allowlist))
        return false;

    Vector<String> blocklist;
    if (!decoder.decode(blocklist))
        return false;

    UserContentInjectedFrames injectedFrames;
    if (!decoder.decode(injectedFrames))
        return false;

    UserStyleLevel level;
    if (!decoder.decode(level))
        return false;

    std::optional<std::optional<PageIdentifier>> pageID;
    decoder >> pageID;
    if (!pageID)
        return false;

    userStyleSheet = UserStyleSheet(source, url, WTFMove(allowlist), WTFMove(blocklist), injectedFrames, level, WTFMove(*pageID));
    return true;
}

void ArgumentCoder<ScrollableAreaParameters>::encode(Encoder& encoder, const ScrollableAreaParameters& parameters)
{
    encoder << parameters.horizontalScrollElasticity;
    encoder << parameters.verticalScrollElasticity;

    encoder << parameters.horizontalScrollbarMode;
    encoder << parameters.verticalScrollbarMode;
    
    encoder << parameters.horizontalOverscrollBehavior;
    encoder << parameters.verticalOverscrollBehavior;

    encoder << parameters.allowsHorizontalScrolling;
    encoder << parameters.allowsVerticalScrolling;

    encoder << parameters.horizontalScrollbarHiddenByStyle;
    encoder << parameters.verticalScrollbarHiddenByStyle;

    encoder << parameters.useDarkAppearanceForScrollbars;
}

bool ArgumentCoder<ScrollableAreaParameters>::decode(Decoder& decoder, ScrollableAreaParameters& params)
{
    if (!decoder.decode(params.horizontalScrollElasticity))
        return false;
    if (!decoder.decode(params.verticalScrollElasticity))
        return false;

    if (!decoder.decode(params.horizontalScrollbarMode))
        return false;
    if (!decoder.decode(params.verticalScrollbarMode))
        return false;
    
    if (!decoder.decode(params.horizontalOverscrollBehavior))
        return false;
    if (!decoder.decode(params.verticalOverscrollBehavior))
        return false;

    if (!decoder.decode(params.allowsHorizontalScrolling))
        return false;
    if (!decoder.decode(params.allowsVerticalScrolling))
        return false;

    if (!decoder.decode(params.horizontalScrollbarHiddenByStyle))
        return false;
    if (!decoder.decode(params.verticalScrollbarHiddenByStyle))
        return false;

    if (!decoder.decode(params.useDarkAppearanceForScrollbars))
        return false;

    return true;
}

void ArgumentCoder<FixedPositionViewportConstraints>::encode(Encoder& encoder, const FixedPositionViewportConstraints& viewportConstraints)
{
    encoder << viewportConstraints.alignmentOffset();
    encoder << viewportConstraints.anchorEdges();

    encoder << viewportConstraints.viewportRectAtLastLayout();
    encoder << viewportConstraints.layerPositionAtLastLayout();
}

bool ArgumentCoder<FixedPositionViewportConstraints>::decode(Decoder& decoder, FixedPositionViewportConstraints& viewportConstraints)
{
    FloatSize alignmentOffset;
    if (!decoder.decode(alignmentOffset))
        return false;
    
    ViewportConstraints::AnchorEdges anchorEdges;
    if (!decoder.decode(anchorEdges))
        return false;

    FloatRect viewportRectAtLastLayout;
    if (!decoder.decode(viewportRectAtLastLayout))
        return false;

    FloatPoint layerPositionAtLastLayout;
    if (!decoder.decode(layerPositionAtLastLayout))
        return false;

    viewportConstraints = FixedPositionViewportConstraints();
    viewportConstraints.setAlignmentOffset(alignmentOffset);
    viewportConstraints.setAnchorEdges(anchorEdges);

    viewportConstraints.setViewportRectAtLastLayout(viewportRectAtLastLayout);
    viewportConstraints.setLayerPositionAtLastLayout(layerPositionAtLastLayout);
    
    return true;
}

void ArgumentCoder<AbsolutePositionConstraints>::encode(Encoder& encoder, const AbsolutePositionConstraints& layoutConstraints)
{
    encoder << layoutConstraints.alignmentOffset();
    encoder << layoutConstraints.layerPositionAtLastLayout();
}

bool ArgumentCoder<AbsolutePositionConstraints>::decode(Decoder& decoder, AbsolutePositionConstraints& layoutConstraints)
{
    FloatSize alignmentOffset;
    if (!decoder.decode(alignmentOffset))
        return false;

    FloatPoint layerPosition;
    if (!decoder.decode(layerPosition))
        return false;

    layoutConstraints = { };
    layoutConstraints.setAlignmentOffset(alignmentOffset);
    layoutConstraints.setLayerPositionAtLastLayout(layerPosition);
    return true;
}

void ArgumentCoder<StickyPositionViewportConstraints>::encode(Encoder& encoder, const StickyPositionViewportConstraints& viewportConstraints)
{
    encoder << viewportConstraints.alignmentOffset();
    encoder << viewportConstraints.anchorEdges();

    encoder << viewportConstraints.leftOffset();
    encoder << viewportConstraints.rightOffset();
    encoder << viewportConstraints.topOffset();
    encoder << viewportConstraints.bottomOffset();

    encoder << viewportConstraints.constrainingRectAtLastLayout();
    encoder << viewportConstraints.containingBlockRect();
    encoder << viewportConstraints.stickyBoxRect();

    encoder << viewportConstraints.stickyOffsetAtLastLayout();
    encoder << viewportConstraints.layerPositionAtLastLayout();
}

bool ArgumentCoder<StickyPositionViewportConstraints>::decode(Decoder& decoder, StickyPositionViewportConstraints& viewportConstraints)
{
    FloatSize alignmentOffset;
    if (!decoder.decode(alignmentOffset))
        return false;
    
    ViewportConstraints::AnchorEdges anchorEdges;
    if (!decoder.decode(anchorEdges))
        return false;
    
    float leftOffset;
    if (!decoder.decode(leftOffset))
        return false;

    float rightOffset;
    if (!decoder.decode(rightOffset))
        return false;

    float topOffset;
    if (!decoder.decode(topOffset))
        return false;

    float bottomOffset;
    if (!decoder.decode(bottomOffset))
        return false;
    
    FloatRect constrainingRectAtLastLayout;
    if (!decoder.decode(constrainingRectAtLastLayout))
        return false;

    FloatRect containingBlockRect;
    if (!decoder.decode(containingBlockRect))
        return false;

    FloatRect stickyBoxRect;
    if (!decoder.decode(stickyBoxRect))
        return false;

    FloatSize stickyOffsetAtLastLayout;
    if (!decoder.decode(stickyOffsetAtLastLayout))
        return false;
    
    FloatPoint layerPositionAtLastLayout;
    if (!decoder.decode(layerPositionAtLastLayout))
        return false;

    viewportConstraints = StickyPositionViewportConstraints();
    viewportConstraints.setAlignmentOffset(alignmentOffset);
    viewportConstraints.setAnchorEdges(anchorEdges);

    viewportConstraints.setLeftOffset(leftOffset);
    viewportConstraints.setRightOffset(rightOffset);
    viewportConstraints.setTopOffset(topOffset);
    viewportConstraints.setBottomOffset(bottomOffset);
    
    viewportConstraints.setConstrainingRectAtLastLayout(constrainingRectAtLastLayout);
    viewportConstraints.setContainingBlockRect(containingBlockRect);
    viewportConstraints.setStickyBoxRect(stickyBoxRect);

    viewportConstraints.setStickyOffsetAtLastLayout(stickyOffsetAtLastLayout);
    viewportConstraints.setLayerPositionAtLastLayout(layerPositionAtLastLayout);

    return true;
}

#if !USE(COORDINATED_GRAPHICS)
void ArgumentCoder<FilterOperation>::encode(Encoder& encoder, const FilterOperation& filter)
{
    encoder << filter.type();

    switch (filter.type()) {
    case FilterOperation::NONE:
    case FilterOperation::REFERENCE:
        ASSERT_NOT_REACHED();
        return;
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
        encoder << downcast<BasicColorMatrixFilterOperation>(filter).amount();
        return;
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
        encoder << downcast<BasicComponentTransferFilterOperation>(filter).amount();
        return;
    case FilterOperation::APPLE_INVERT_LIGHTNESS:
        ASSERT_NOT_REACHED(); // APPLE_INVERT_LIGHTNESS is only used in -apple-color-filter.
        return;
    case FilterOperation::BLUR:
        encoder << downcast<BlurFilterOperation>(filter).stdDeviation();
        return;
    case FilterOperation::DROP_SHADOW: {
        const auto& dropShadowFilter = downcast<DropShadowFilterOperation>(filter);
        encoder << dropShadowFilter.location();
        encoder << dropShadowFilter.stdDeviation();
        encoder << dropShadowFilter.color();
        return;
    }
    case FilterOperation::DEFAULT:
        encoder << downcast<DefaultFilterOperation>(filter).representedType();
        return;
    case FilterOperation::PASSTHROUGH:
        return;
    }

    ASSERT_NOT_REACHED();
}

bool decodeFilterOperation(Decoder& decoder, RefPtr<FilterOperation>& filter)
{
    FilterOperation::OperationType type;
    if (!decoder.decode(type))
        return false;

    switch (type) {
    case FilterOperation::NONE:
    case FilterOperation::REFERENCE:
        ASSERT_NOT_REACHED();
        return false;
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE: {
        double amount;
        if (!decoder.decode(amount))
            return false;
        filter = BasicColorMatrixFilterOperation::create(amount, type);
        return true;
    }
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST: {
        double amount;
        if (!decoder.decode(amount))
            return false;
        filter = BasicComponentTransferFilterOperation::create(amount, type);
        return true;
    }
    case FilterOperation::APPLE_INVERT_LIGHTNESS:
        ASSERT_NOT_REACHED(); // APPLE_INVERT_LIGHTNESS is only used in -apple-color-filter.
        return false;
    case FilterOperation::BLUR: {
        Length stdDeviation;
        if (!decoder.decode(stdDeviation))
            return false;
        filter = BlurFilterOperation::create(stdDeviation);
        return true;
    }
    case FilterOperation::DROP_SHADOW: {
        IntPoint location;
        int stdDeviation;
        Color color;
        if (!decoder.decode(location))
            return false;
        if (!decoder.decode(stdDeviation))
            return false;
        if (!decoder.decode(color))
            return false;
        filter = DropShadowFilterOperation::create(location, stdDeviation, color);
        return true;
    }
    case FilterOperation::DEFAULT: {
        FilterOperation::OperationType representedType;
        if (!decoder.decode(representedType))
            return false;
        filter = DefaultFilterOperation::create(representedType);
        return true;
    }
    case FilterOperation::PASSTHROUGH:
        filter = PassthroughFilterOperation::create();
        return true;
    }
            
    ASSERT_NOT_REACHED();
    return false;
}


void ArgumentCoder<FilterOperations>::encode(Encoder& encoder, const FilterOperations& filters)
{
    encoder << static_cast<uint64_t>(filters.size());

    for (const auto& filter : filters.operations())
        encoder << *filter;
}

bool ArgumentCoder<FilterOperations>::decode(Decoder& decoder, FilterOperations& filters)
{
    uint64_t filterCount;
    if (!decoder.decode(filterCount))
        return false;

    for (uint64_t i = 0; i < filterCount; ++i) {
        RefPtr<FilterOperation> filter;
        if (!decodeFilterOperation(decoder, filter))
            return false;
        filters.operations().append(WTFMove(filter));
    }

    return true;
}

void ArgumentCoder<RefPtr<WebCore::FilterOperation>>::encode(Encoder& encoder, const RefPtr<WebCore::FilterOperation>& operation)
{
    encoder << !!operation;
    if (operation)
        encoder << *operation;
}

WARN_UNUSED_RETURN bool ArgumentCoder<RefPtr<WebCore::FilterOperation>>::decode(Decoder& decoder, RefPtr<WebCore::FilterOperation>& value)
{
    std::optional<bool> isNull;
    decoder >> isNull;
    if (!isNull)
        return false;
    
    if (!decodeFilterOperation(decoder, value)) {
        value = nullptr;
        return false;
    }
    return true;
}

#endif // !USE(COORDINATED_GRAPHICS)

void ArgumentCoder<BlobPart>::encode(Encoder& encoder, const BlobPart& blobPart)
{
    encoder << blobPart.type();
    switch (blobPart.type()) {
    case BlobPart::Type::Data:
        encoder << blobPart.data();
        return;
    case BlobPart::Type::Blob:
        encoder << blobPart.url();
        return;
    }
    ASSERT_NOT_REACHED();
}

std::optional<BlobPart> ArgumentCoder<BlobPart>::decode(Decoder& decoder)
{
    std::optional<BlobPart::Type> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    switch (*type) {
    case BlobPart::Type::Data: {
        std::optional<Vector<uint8_t>> data;
        decoder >> data;
        if (!data)
            return std::nullopt;
        return BlobPart(WTFMove(*data));
    }
    case BlobPart::Type::Blob: {
        URL url;
        if (!decoder.decode(url))
            return std::nullopt;
        return BlobPart(url);
    }
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

void ArgumentCoder<TextIndicatorData>::encode(Encoder& encoder, const TextIndicatorData& textIndicatorData)
{
    encoder << textIndicatorData.selectionRectInRootViewCoordinates;
    encoder << textIndicatorData.textBoundingRectInRootViewCoordinates;
    encoder << textIndicatorData.textRectsInBoundingRectCoordinates;
    encoder << textIndicatorData.contentImageWithoutSelectionRectInRootViewCoordinates;
    encoder << textIndicatorData.contentImageScaleFactor;
    encoder << textIndicatorData.estimatedBackgroundColor;
    encoder << textIndicatorData.presentationTransition;
    encoder << textIndicatorData.options;

    encodeOptionalImage(encoder, textIndicatorData.contentImage.get());
    encodeOptionalImage(encoder, textIndicatorData.contentImageWithHighlight.get());
    encodeOptionalImage(encoder, textIndicatorData.contentImageWithoutSelection.get());
}

std::optional<TextIndicatorData> ArgumentCoder<TextIndicatorData>::decode(Decoder& decoder)
{
    TextIndicatorData textIndicatorData;
    if (!decoder.decode(textIndicatorData.selectionRectInRootViewCoordinates))
        return std::nullopt;

    if (!decoder.decode(textIndicatorData.textBoundingRectInRootViewCoordinates))
        return std::nullopt;

    if (!decoder.decode(textIndicatorData.textRectsInBoundingRectCoordinates))
        return std::nullopt;

    if (!decoder.decode(textIndicatorData.contentImageWithoutSelectionRectInRootViewCoordinates))
        return std::nullopt;

    if (!decoder.decode(textIndicatorData.contentImageScaleFactor))
        return std::nullopt;

    if (!decoder.decode(textIndicatorData.estimatedBackgroundColor))
        return std::nullopt;

    if (!decoder.decode(textIndicatorData.presentationTransition))
        return std::nullopt;

    if (!decoder.decode(textIndicatorData.options))
        return std::nullopt;

    if (!decodeOptionalImage(decoder, textIndicatorData.contentImage))
        return std::nullopt;

    if (!decodeOptionalImage(decoder, textIndicatorData.contentImageWithHighlight))
        return std::nullopt;

    if (!decodeOptionalImage(decoder, textIndicatorData.contentImageWithoutSelection))
        return std::nullopt;

    return textIndicatorData;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void ArgumentCoder<MediaPlaybackTargetContext>::encode(Encoder& encoder, const MediaPlaybackTargetContext& target)
{
    bool hasPlatformData = target.encodingRequiresPlatformData();
    encoder << hasPlatformData;

    MediaPlaybackTargetContext::Type contextType = target.type();
    encoder << contextType;

    if (target.encodingRequiresPlatformData()) {
        encodePlatformData(encoder, target);
        return;
    }

    ASSERT(contextType == MediaPlaybackTargetContext::Type::Mock);
    encoder << target.deviceName();
    encoder << target.mockState();
}

bool ArgumentCoder<MediaPlaybackTargetContext>::decode(Decoder& decoder, MediaPlaybackTargetContext& target)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;

    MediaPlaybackTargetContext::Type contextType;
    if (!decoder.decode(contextType))
        return false;

    if (hasPlatformData)
        return decodePlatformData(decoder, contextType, target);

    ASSERT(contextType == MediaPlaybackTargetContext::Type::Mock);
    String deviceName;
    if (!decoder.decode(deviceName))
        return false;

    MediaPlaybackTargetContext::MockState mockState;
    if (!decoder.decode(mockState))
        return false;

    target = MediaPlaybackTargetContext(deviceName, mockState);

    return true;
}
#endif

void ArgumentCoder<DictionaryPopupInfo>::encode(IPC::Encoder& encoder, const DictionaryPopupInfo& info)
{
    encoder << info.origin;
    encoder << info.textIndicator;

    if (info.encodingRequiresPlatformData()) {
        encoder << true;
        encodePlatformData(encoder, info);
        return;
    }

    encoder << false;
}

bool ArgumentCoder<DictionaryPopupInfo>::decode(IPC::Decoder& decoder, DictionaryPopupInfo& result)
{
    if (!decoder.decode(result.origin))
        return false;

    std::optional<TextIndicatorData> textIndicator;
    decoder >> textIndicator;
    if (!textIndicator)
        return false;
    result.textIndicator = WTFMove(*textIndicator);

    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return false;
    if (hasPlatformData)
        return decodePlatformData(decoder, result);
    return true;
}

void ArgumentCoder<ExceptionDetails>::encode(IPC::Encoder& encoder, const ExceptionDetails& info)
{
    encoder << info.message;
    encoder << info.lineNumber;
    encoder << info.columnNumber;
    encoder << info.type;
    encoder << info.sourceURL;
}

bool ArgumentCoder<ExceptionDetails>::decode(IPC::Decoder& decoder, ExceptionDetails& result)
{
    if (!decoder.decode(result.message))
        return false;

    if (!decoder.decode(result.lineNumber))
        return false;

    if (!decoder.decode(result.columnNumber))
        return false;

    if (!decoder.decode(result.type))
        return false;

    if (!decoder.decode(result.sourceURL))
        return false;

    return true;
}

void ArgumentCoder<ResourceLoadStatistics>::encode(Encoder& encoder, const WebCore::ResourceLoadStatistics& statistics)
{
    encoder << statistics.registrableDomain;
    
    encoder << statistics.lastSeen.secondsSinceEpoch().value();
    
    // User interaction
    encoder << statistics.hadUserInteraction;
    encoder << statistics.mostRecentUserInteractionTime.secondsSinceEpoch().value();
    encoder << statistics.grandfathered;

    // Storage access
    encoder << statistics.storageAccessUnderTopFrameDomains;

    // Top frame stats
    encoder << statistics.topFrameUniqueRedirectsTo;
    encoder << statistics.topFrameUniqueRedirectsFrom;
    encoder << statistics.topFrameLoadedThirdPartyScripts;

    // Subframe stats
    encoder << statistics.subframeUnderTopFrameDomains;
    
    // Subresource stats
    encoder << statistics.subresourceUnderTopFrameDomains;
    encoder << statistics.subresourceUniqueRedirectsTo;
    encoder << statistics.subresourceUniqueRedirectsFrom;

    // Prevalent Resource
    encoder << statistics.isPrevalentResource;
    encoder << statistics.isVeryPrevalentResource;
    encoder << statistics.dataRecordsRemoved;

#if ENABLE(WEB_API_STATISTICS)
    encoder << statistics.fontsFailedToLoad;
    encoder << statistics.fontsSuccessfullyLoaded;
    encoder << statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs;
    
    encoder << statistics.canvasActivityRecord;
    
    encoder << statistics.navigatorFunctionsAccessed;
    encoder << statistics.screenFunctionsAccessed;
#endif
}

std::optional<ResourceLoadStatistics> ArgumentCoder<ResourceLoadStatistics>::decode(Decoder& decoder)
{
    ResourceLoadStatistics statistics;
    std::optional<RegistrableDomain> registrableDomain;
    decoder >> registrableDomain;
    if (!registrableDomain)
        return std::nullopt;
    statistics.registrableDomain = WTFMove(*registrableDomain);

    double lastSeenTimeAsDouble;
    if (!decoder.decode(lastSeenTimeAsDouble))
        return std::nullopt;
    statistics.lastSeen = WallTime::fromRawSeconds(lastSeenTimeAsDouble);
    
    // User interaction
    if (!decoder.decode(statistics.hadUserInteraction))
        return std::nullopt;

    double mostRecentUserInteractionTimeAsDouble;
    if (!decoder.decode(mostRecentUserInteractionTimeAsDouble))
        return std::nullopt;
    statistics.mostRecentUserInteractionTime = WallTime::fromRawSeconds(mostRecentUserInteractionTimeAsDouble);

    if (!decoder.decode(statistics.grandfathered))
        return std::nullopt;

    // Storage access
    std::optional<HashSet<RegistrableDomain>> storageAccessUnderTopFrameDomains;
    decoder >> storageAccessUnderTopFrameDomains;
    if (!storageAccessUnderTopFrameDomains)
        return std::nullopt;
    statistics.storageAccessUnderTopFrameDomains = WTFMove(*storageAccessUnderTopFrameDomains);

    // Top frame stats
    std::optional<HashSet<RegistrableDomain>> topFrameUniqueRedirectsTo;
    decoder >> topFrameUniqueRedirectsTo;
    if (!topFrameUniqueRedirectsTo)
        return std::nullopt;
    statistics.topFrameUniqueRedirectsTo = WTFMove(*topFrameUniqueRedirectsTo);

    std::optional<HashSet<RegistrableDomain>> topFrameUniqueRedirectsFrom;
    decoder >> topFrameUniqueRedirectsFrom;
    if (!topFrameUniqueRedirectsFrom)
        return std::nullopt;
    statistics.topFrameUniqueRedirectsFrom = WTFMove(*topFrameUniqueRedirectsFrom);

    std::optional<HashSet<RegistrableDomain>> topFrameLoadedThirdPartyScripts;
    decoder >> topFrameLoadedThirdPartyScripts;
    if (!topFrameLoadedThirdPartyScripts)
        return std::nullopt;
    statistics.topFrameLoadedThirdPartyScripts = WTFMove(*topFrameLoadedThirdPartyScripts);

    // Subframe stats
    std::optional<HashSet<RegistrableDomain>> subframeUnderTopFrameDomains;
    decoder >> subframeUnderTopFrameDomains;
    if (!subframeUnderTopFrameDomains)
        return std::nullopt;
    statistics.subframeUnderTopFrameDomains = WTFMove(*subframeUnderTopFrameDomains);

    // Subresource stats
    std::optional<HashSet<RegistrableDomain>> subresourceUnderTopFrameDomains;
    decoder >> subresourceUnderTopFrameDomains;
    if (!subresourceUnderTopFrameDomains)
        return std::nullopt;
    statistics.subresourceUnderTopFrameDomains = WTFMove(*subresourceUnderTopFrameDomains);

    std::optional<HashSet<RegistrableDomain>> subresourceUniqueRedirectsTo;
    decoder >> subresourceUniqueRedirectsTo;
    if (!subresourceUniqueRedirectsTo)
        return std::nullopt;
    statistics.subresourceUniqueRedirectsTo = WTFMove(*subresourceUniqueRedirectsTo);

    std::optional<HashSet<RegistrableDomain>> subresourceUniqueRedirectsFrom;
    decoder >> subresourceUniqueRedirectsFrom;
    if (!subresourceUniqueRedirectsFrom)
        return std::nullopt;
    statistics.subresourceUniqueRedirectsFrom = WTFMove(*subresourceUniqueRedirectsFrom);

    // Prevalent Resource
    if (!decoder.decode(statistics.isPrevalentResource))
        return std::nullopt;

    if (!decoder.decode(statistics.isVeryPrevalentResource))
        return std::nullopt;
    
    if (!decoder.decode(statistics.dataRecordsRemoved))
        return std::nullopt;
    
#if ENABLE(WEB_API_STATISTICS)
    if (!decoder.decode(statistics.fontsFailedToLoad))
        return std::nullopt;
    
    if (!decoder.decode(statistics.fontsSuccessfullyLoaded))
        return std::nullopt;
    
    if (!decoder.decode(statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs))
        return std::nullopt;
    
    if (!decoder.decode(statistics.canvasActivityRecord))
        return std::nullopt;
    
    if (!decoder.decode(statistics.navigatorFunctionsAccessed))
        return std::nullopt;
    
    if (!decoder.decode(statistics.screenFunctionsAccessed))
        return std::nullopt;
#endif

    return statistics;
}

#if ENABLE(MEDIA_STREAM)
void ArgumentCoder<MediaConstraints>::encode(Encoder& encoder, const WebCore::MediaConstraints& constraint)
{
    encoder << constraint.mandatoryConstraints
        << constraint.advancedConstraints
        << constraint.isValid;
}

bool ArgumentCoder<MediaConstraints>::decode(Decoder& decoder, WebCore::MediaConstraints& constraints)
{
    std::optional<WebCore::MediaTrackConstraintSetMap> mandatoryConstraints;
    decoder >> mandatoryConstraints;
    if (!mandatoryConstraints)
        return false;
    constraints.mandatoryConstraints = WTFMove(*mandatoryConstraints);
    return decoder.decode(constraints.advancedConstraints)
        && decoder.decode(constraints.isValid);
}
#endif

void ArgumentCoder<IDBKeyPath>::encode(Encoder& encoder, const IDBKeyPath& keyPath)
{
    bool isString = std::holds_alternative<String>(keyPath);
    encoder << isString;
    if (isString)
        encoder << std::get<String>(keyPath);
    else
        encoder << std::get<Vector<String>>(keyPath);
}

bool ArgumentCoder<IDBKeyPath>::decode(Decoder& decoder, IDBKeyPath& keyPath)
{
    bool isString;
    if (!decoder.decode(isString))
        return false;
    if (isString) {
        String string;
        if (!decoder.decode(string))
            return false;
        keyPath = string;
    } else {
        Vector<String> vector;
        if (!decoder.decode(vector))
            return false;
        keyPath = vector;
    }
    return true;
}

#if ENABLE(SERVICE_WORKER)
void ArgumentCoder<ServiceWorkerOrClientData>::encode(Encoder& encoder, const ServiceWorkerOrClientData& data)
{
    bool isServiceWorkerData = std::holds_alternative<ServiceWorkerData>(data);
    encoder << isServiceWorkerData;
    if (isServiceWorkerData)
        encoder << std::get<ServiceWorkerData>(data);
    else
        encoder << std::get<ServiceWorkerClientData>(data);
}

bool ArgumentCoder<ServiceWorkerOrClientData>::decode(Decoder& decoder, ServiceWorkerOrClientData& data)
{
    bool isServiceWorkerData;
    if (!decoder.decode(isServiceWorkerData))
        return false;
    if (isServiceWorkerData) {
        std::optional<ServiceWorkerData> workerData;
        decoder >> workerData;
        if (!workerData)
            return false;

        data = WTFMove(*workerData);
    } else {
        std::optional<ServiceWorkerClientData> clientData;
        decoder >> clientData;
        if (!clientData)
            return false;

        data = WTFMove(*clientData);
    }
    return true;
}

void ArgumentCoder<ServiceWorkerOrClientIdentifier>::encode(Encoder& encoder, const ServiceWorkerOrClientIdentifier& identifier)
{
    bool isServiceWorkerIdentifier = std::holds_alternative<ServiceWorkerIdentifier>(identifier);
    encoder << isServiceWorkerIdentifier;
    if (isServiceWorkerIdentifier)
        encoder << std::get<ServiceWorkerIdentifier>(identifier);
    else
        encoder << std::get<ScriptExecutionContextIdentifier>(identifier);
}

bool ArgumentCoder<ServiceWorkerOrClientIdentifier>::decode(Decoder& decoder, ServiceWorkerOrClientIdentifier& identifier)
{
    bool isServiceWorkerIdentifier;
    if (!decoder.decode(isServiceWorkerIdentifier))
        return false;
    if (isServiceWorkerIdentifier) {
        std::optional<ServiceWorkerIdentifier> workerIdentifier;
        decoder >> workerIdentifier;
        if (!workerIdentifier)
            return false;

        identifier = WTFMove(*workerIdentifier);
    } else {
        std::optional<ScriptExecutionContextIdentifier> clientIdentifier;
        decoder >> clientIdentifier;
        if (!clientIdentifier)
            return false;

        identifier = WTFMove(*clientIdentifier);
    }
    return true;
}

#endif

void ArgumentCoder<MediaSelectionOption>::encode(Encoder& encoder, const MediaSelectionOption& option)
{
    encoder << option.displayName;
    encoder << option.type;
}

std::optional<MediaSelectionOption> ArgumentCoder<MediaSelectionOption>::decode(Decoder& decoder)
{
    std::optional<String> displayName;
    decoder >> displayName;
    if (!displayName)
        return std::nullopt;
    
    std::optional<MediaSelectionOption::Type> type;
    decoder >> type;
    if (!type)
        return std::nullopt;
    
    return {{ WTFMove(*displayName), WTFMove(*type) }};
}

void ArgumentCoder<PromisedAttachmentInfo>::encode(Encoder& encoder, const PromisedAttachmentInfo& info)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    encoder << info.attachmentIdentifier;
#endif
    encodeTypesAndData(encoder, info.additionalTypes, info.additionalData);
}

bool ArgumentCoder<PromisedAttachmentInfo>::decode(Decoder& decoder, PromisedAttachmentInfo& info)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    if (!decoder.decode(info.attachmentIdentifier))
        return false;
#endif

    if (!decodeTypesAndData(decoder, info.additionalTypes, info.additionalData))
        return false;

    return true;
}

void ArgumentCoder<RefPtr<SecurityOrigin>>::encode(Encoder& encoder, const RefPtr<SecurityOrigin>& origin)
{
    encoder << *origin;
}
    
std::optional<RefPtr<SecurityOrigin>> ArgumentCoder<RefPtr<SecurityOrigin>>::decode(Decoder& decoder)
{
    auto origin = SecurityOrigin::decode(decoder);
    if (!origin)
        return std::nullopt;
    return origin;
}

void ArgumentCoder<FontAttributes>::encode(Encoder& encoder, const FontAttributes& attributes)
{
    encoder << attributes.backgroundColor << attributes.foregroundColor << attributes.fontShadow << attributes.hasUnderline << attributes.hasStrikeThrough << attributes.textLists;
    encoder << attributes.horizontalAlignment;
    encoder << attributes.subscriptOrSuperscript;
    encoder << attributes.font;
}

std::optional<FontAttributes> ArgumentCoder<FontAttributes>::decode(Decoder& decoder)
{
    FontAttributes attributes;

    if (!decoder.decode(attributes.backgroundColor))
        return std::nullopt;

    if (!decoder.decode(attributes.foregroundColor))
        return std::nullopt;

    if (!decoder.decode(attributes.fontShadow))
        return std::nullopt;

    if (!decoder.decode(attributes.hasUnderline))
        return std::nullopt;

    if (!decoder.decode(attributes.hasStrikeThrough))
        return std::nullopt;

    if (!decoder.decode(attributes.textLists))
        return std::nullopt;

    if (!decoder.decode(attributes.horizontalAlignment))
        return std::nullopt;

    if (!decoder.decode(attributes.subscriptOrSuperscript))
        return std::nullopt;

    std::optional<RefPtr<Font>> font;
    decoder >> font;
    if (!font)
        return std::nullopt;

    attributes.font = WTFMove(*font);

    return attributes;
}

#if ENABLE(ATTACHMENT_ELEMENT)

void ArgumentCoder<SerializedAttachmentData>::encode(IPC::Encoder& encoder, const WebCore::SerializedAttachmentData& data)
{
    encoder << data.identifier << data.mimeType << data.data;
}

std::optional<SerializedAttachmentData> ArgumentCoder<WebCore::SerializedAttachmentData>::decode(IPC::Decoder& decoder)
{
    String identifier;
    if (!decoder.decode(identifier))
        return std::nullopt;

    String mimeType;
    if (!decoder.decode(mimeType))
        return std::nullopt;

    RefPtr<SharedBuffer> buffer;
    if (!decoder.decode(buffer))
        return std::nullopt;

    return { { WTFMove(identifier), WTFMove(mimeType), buffer.releaseNonNull() } };
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(VIDEO)
void ArgumentCoder<WebCore::SerializedPlatformDataCueValue>::encode(Encoder& encoder, const SerializedPlatformDataCueValue& value)
{
    bool hasPlatformData = value.encodingRequiresPlatformData();
    encoder << hasPlatformData;

    encoder << value.platformType();
    if (hasPlatformData)
        encodePlatformData(encoder, value);
}

std::optional<SerializedPlatformDataCueValue> ArgumentCoder<WebCore::SerializedPlatformDataCueValue>::decode(IPC::Decoder& decoder)
{
    bool hasPlatformData;
    if (!decoder.decode(hasPlatformData))
        return std::nullopt;

    WebCore::SerializedPlatformDataCueValue::PlatformType type;
    if (!decoder.decode(type))
        return std::nullopt;

    if (hasPlatformData)
        return decodePlatformData(decoder, type);

    return { SerializedPlatformDataCueValue() };

}
#endif

void ArgumentCoder<RefPtr<WebCore::FragmentedSharedBuffer>>::encode(Encoder& encoder, const RefPtr<WebCore::FragmentedSharedBuffer>& buffer)
{
    encodeSharedBuffer(encoder, buffer.get());
}

std::optional<RefPtr<FragmentedSharedBuffer>> ArgumentCoder<RefPtr<WebCore::FragmentedSharedBuffer>>::decode(Decoder& decoder)
{
    RefPtr<SharedBuffer> buffer;
    if (!decodeSharedBuffer(decoder, buffer))
        return std::nullopt;

    return buffer;
}

void ArgumentCoder<Ref<WebCore::FragmentedSharedBuffer>>::encode(Encoder& encoder, const Ref<WebCore::FragmentedSharedBuffer>& buffer)
{
    encodeSharedBuffer(encoder, buffer.ptr());
}

std::optional<Ref<FragmentedSharedBuffer>> ArgumentCoder<Ref<WebCore::FragmentedSharedBuffer>>::decode(Decoder& decoder)
{
    RefPtr<SharedBuffer> buffer;
    if (!decodeSharedBuffer(decoder, buffer) || !buffer)
        return std::nullopt;

    return buffer.releaseNonNull();
}

void ArgumentCoder<RefPtr<WebCore::SharedBuffer>>::encode(Encoder& encoder, const RefPtr<WebCore::SharedBuffer>& buffer)
{
    encodeSharedBuffer(encoder, buffer.get());
}

std::optional<RefPtr<SharedBuffer>> ArgumentCoder<RefPtr<WebCore::SharedBuffer>>::decode(Decoder& decoder)
{
    RefPtr<SharedBuffer> buffer;
    if (!decodeSharedBuffer(decoder, buffer))
        return std::nullopt;

    return buffer;
}

void ArgumentCoder<Ref<WebCore::SharedBuffer>>::encode(Encoder& encoder, const Ref<WebCore::SharedBuffer>& buffer)
{
    encodeSharedBuffer(encoder, buffer.ptr());
}

std::optional<Ref<SharedBuffer>> ArgumentCoder<Ref<WebCore::SharedBuffer>>::decode(Decoder& decoder)
{
    RefPtr<SharedBuffer> buffer;
    if (!decodeSharedBuffer(decoder, buffer) || !buffer)
        return std::nullopt;

    return buffer.releaseNonNull();
}

#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
static ShareableResource::Handle tryConvertToShareableResourceHandle(const ScriptBuffer& script)
{
    if (!script.containsSingleFileMappedSegment())
        return ShareableResource::Handle { };

    auto& segment = script.buffer()->begin()->segment;
    auto sharedMemory = SharedMemory::wrapMap(const_cast<uint8_t*>(segment->data()), segment->size(), SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return ShareableResource::Handle { };

    auto shareableResource = ShareableResource::create(sharedMemory.releaseNonNull(), 0, segment->size());
    if (!shareableResource)
        return ShareableResource::Handle { };

    ShareableResource::Handle shareableResourceHandle;
    shareableResource->createHandle(shareableResourceHandle);
    return shareableResourceHandle;
}

static std::optional<WebCore::ScriptBuffer> decodeScriptBufferAsShareableResourceHandle(Decoder& decoder)
{
    ShareableResource::Handle handle;
    if (!decoder.decode(handle) || handle.isNull())
        return std::nullopt;
    auto buffer = handle.tryWrapInSharedBuffer();
    if (!buffer)
        return std::nullopt;
    return WebCore::ScriptBuffer { WTFMove(buffer) };
}
#endif

void ArgumentCoder<WebCore::ScriptBuffer>::encode(Encoder& encoder, const WebCore::ScriptBuffer& script)
{
#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
    auto handle = tryConvertToShareableResourceHandle(script);
    bool isShareableResourceHandle = !handle.isNull();
    encoder << isShareableResourceHandle;
    if (isShareableResourceHandle) {
        encoder << handle;
        return;
    }
#endif
    encodeSharedBuffer(encoder, script.buffer());
}

std::optional<WebCore::ScriptBuffer> ArgumentCoder<WebCore::ScriptBuffer>::decode(Decoder& decoder)
{
#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
    std::optional<bool> isShareableResourceHandle;
    decoder >> isShareableResourceHandle;
    if (!isShareableResourceHandle)
        return std::nullopt;
    if (*isShareableResourceHandle)
        return decodeScriptBufferAsShareableResourceHandle(decoder);
#endif

    RefPtr<SharedBuffer> buffer;
    if (!decodeSharedBuffer(decoder, buffer))
        return std::nullopt;

    return WebCore::ScriptBuffer { WTFMove(buffer) };
}

#if ENABLE(ENCRYPTED_MEDIA)
void ArgumentCoder<WebCore::CDMInstanceSession::Message>::encode(Encoder& encoder, const WebCore::CDMInstanceSession::Message& message)
{
    encoder << message.first;

    RefPtr<SharedBuffer> messageData = message.second.copyRef();
    encoder << messageData;
}

std::optional<WebCore::CDMInstanceSession::Message>  ArgumentCoder<WebCore::CDMInstanceSession::Message>::decode(Decoder& decoder)
{
    WebCore::CDMInstanceSession::MessageType type;
    if (!decoder.decode(type))
        return std::nullopt;

    RefPtr<SharedBuffer> buffer;
    if (!decoder.decode(buffer) || !buffer)
        return std::nullopt;

    return std::make_optional<WebCore::CDMInstanceSession::Message>({ type, buffer.releaseNonNull() });
}
#endif // ENABLE(ENCRYPTED_MEDIA)

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
void ArgumentCoder<WebCore::GraphicsContextGLAttributes>::encode(Encoder& encoder, const WebCore::GraphicsContextGLAttributes& attributes)
{
    encoder << attributes.alpha;
    encoder << attributes.depth;
    encoder << attributes.stencil;
    encoder << attributes.antialias;
    encoder << attributes.premultipliedAlpha;
    encoder << attributes.preserveDrawingBuffer;
    encoder << attributes.failIfMajorPerformanceCaveat;
    encoder << attributes.powerPreference;
    encoder << attributes.shareResources;
    encoder << attributes.webGLVersion;
    encoder << attributes.noExtensions;
    encoder << attributes.devicePixelRatio;
    encoder << attributes.initialPowerPreference;
#if ENABLE(WEBXR)
    encoder << attributes.xrCompatible;
#endif
}

std::optional<WebCore::GraphicsContextGLAttributes> ArgumentCoder<WebCore::GraphicsContextGLAttributes>::decode(Decoder& decoder)
{
    GraphicsContextGLAttributes attributes;
    if (!decoder.decode(attributes.alpha))
        return std::nullopt;
    if (!decoder.decode(attributes.depth))
        return std::nullopt;
    if (!decoder.decode(attributes.stencil))
        return std::nullopt;
    if (!decoder.decode(attributes.antialias))
        return std::nullopt;
    if (!decoder.decode(attributes.premultipliedAlpha))
        return std::nullopt;
    if (!decoder.decode(attributes.preserveDrawingBuffer))
        return std::nullopt;
    if (!decoder.decode(attributes.failIfMajorPerformanceCaveat))
        return std::nullopt;
    if (!decoder.decode(attributes.powerPreference))
        return std::nullopt;
    if (!decoder.decode(attributes.shareResources))
        return std::nullopt;
    if (!decoder.decode(attributes.webGLVersion))
        return std::nullopt;
    if (!decoder.decode(attributes.noExtensions))
        return std::nullopt;
    if (!decoder.decode(attributes.devicePixelRatio))
        return std::nullopt;
    if (!decoder.decode(attributes.initialPowerPreference))
        return std::nullopt;
#if ENABLE(WEBXR)
    if (!decoder.decode(attributes.xrCompatible))
        return std::nullopt;
#endif
    return attributes;
}

template<typename Encoder>
void ArgumentCoder<WebCore::GraphicsContextGL::ActiveInfo>::encode(Encoder& encoder, const WebCore::GraphicsContextGL::ActiveInfo& activeInfo)
{
    encoder << activeInfo.name;
    encoder << activeInfo.type;
    encoder << activeInfo.size;
}

template
void ArgumentCoder<WebCore::GraphicsContextGL::ActiveInfo>::encode<Encoder>(Encoder&, const WebCore::GraphicsContextGL::ActiveInfo&);
template
void ArgumentCoder<WebCore::GraphicsContextGL::ActiveInfo>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const WebCore::GraphicsContextGL::ActiveInfo&);

std::optional<WebCore::GraphicsContextGL::ActiveInfo> ArgumentCoder<WebCore::GraphicsContextGL::ActiveInfo>::decode(Decoder& decoder)
{
    WebCore::GraphicsContextGL::ActiveInfo activeInfo;
    if (!decoder.decode(activeInfo.name))
        return std::nullopt;
    if (!decoder.decode(activeInfo.type))
        return std::nullopt;
    if (!decoder.decode(activeInfo.size))
        return std::nullopt;
    return activeInfo;
}

#endif

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(DATA_DETECTION)

void ArgumentCoder<TextRecognitionDataDetector>::encode(Encoder& encoder, const TextRecognitionDataDetector& info)
{
    encodePlatformData(encoder, info);
    encoder << info.normalizedQuads;
}

std::optional<TextRecognitionDataDetector> ArgumentCoder<TextRecognitionDataDetector>::decode(Decoder& decoder)
{
    TextRecognitionDataDetector result;
    if (!decodePlatformData(decoder, result))
        return std::nullopt;

    std::optional<Vector<FloatQuad>> normalizedQuads;
    decoder >> normalizedQuads;
    if (!normalizedQuads)
        return std::nullopt;

    result.normalizedQuads = WTFMove(*normalizedQuads);
    return WTFMove(result);
}

#endif // ENABLE(IMAGE_ANALYSIS) && ENABLE(DATA_DETECTION)

} // namespace IPC
