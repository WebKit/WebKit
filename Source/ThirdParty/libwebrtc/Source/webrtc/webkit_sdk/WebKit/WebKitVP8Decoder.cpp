/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "WebKitVP8Decoder.h"

#include "WebKitDecoder.h"
#include "WebKitDecoderReceiver.h"
#include "api/environment/environment_factory.h"
#include "modules/video_coding/codecs/vp8/libvpx_vp8_decoder.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/cpu_info.h"

namespace webrtc {

static OSStatus createWebKitVP8Decoder(FigVideoCodecType, CFAllocatorRef allocator, VTVideoDecoderRef*);
void registerWebKitVP8Decoder()
{
    VTRegisterVideoDecoder('vp08', createWebKitVP8Decoder);
}

typedef struct {
    std::unique_ptr<LibvpxVp8Decoder> m_instance;
    std::unique_ptr<WebKitDecoderReceiver> m_receiver;
} WebKitVP8Decoder;

static OSStatus invalidateVP8Decoder(CMBaseObjectRef);
static void finalizeVP8Decoder(CMBaseObjectRef);
static CFStringRef copyVP8DecoderDebugDescription(CMBaseObjectRef);

#if defined(CMBASE_OBJECT_NEEDS_ALIGNMENT) && CMBASE_OBJECT_NEEDS_ALIGNMENT
    constexpr size_t padSize = 4;
#else
    constexpr size_t padSize = 0;
#endif

#pragma pack(push, 4)
struct DecoderBaseClass {
    uint8_t pad[padSize];
    CMBaseClass alignedClass;
};

static const DecoderBaseClass WebKitVP8Decoder_BaseClass {
    { },
    {
        kCMBaseObject_ClassVersion_1,
        sizeof(WebKitVP8Decoder),
        nullptr, // Comparison by pointer equality
        invalidateVP8Decoder,
        finalizeVP8Decoder,
        copyVP8DecoderDebugDescription,
        nullptr, // CopyProperty
        nullptr, // SetProperty
        nullptr,
        nullptr
    }
};
#pragma pack(pop)

#if defined(CMBASE_OBJECT_NEEDS_ALIGNMENT) && CMBASE_OBJECT_NEEDS_ALIGNMENT
    static_assert(sizeof(WebKitVP8Decoder_BaseClass.alignedClass.version) == sizeof(uint32_t), "CMBaseClass fixup is required!");
#else
    static_assert(sizeof(WebKitVP8Decoder_BaseClass.alignedClass.version) == sizeof(uintptr_t), "CMBaseClass fixup is not required!");
#endif
static_assert(offsetof(DecoderBaseClass, alignedClass) == padSize, "CMBaseClass offset is incorrect!");
static_assert(alignof(DecoderBaseClass) == 4, "CMBaseClass must have 4 byte alignment");

static OSStatus startVP8DecoderSession(VTVideoDecoderRef, VTVideoDecoderSession, CMVideoFormatDescriptionRef);
static OSStatus decodeVP8DecoderFrame(VTVideoDecoderRef, VTVideoDecoderFrame, CMSampleBufferRef, VTDecodeFrameFlags, VTDecodeInfoFlags*);

#pragma pack(push, 4)
struct DecoderClass {
    uint8_t pad[padSize];
    VTVideoDecoderClass alignedClass;
};

static const DecoderClass WebKitVP8Decoder_VideoDecoderClass =
{
    { },
    {
        kVTVideoDecoder_ClassVersion_1,
        startVP8DecoderSession,
        decodeVP8DecoderFrame,
        nullptr, // VTVideoDecoderFunction_CopySupportedPropertyDictionary,
        nullptr, // VTVideoDecoderFunction_SetProperties
        nullptr, // VTVideoDecoderFunction_CopySerializableProperties
        nullptr, // VTVideoDecoderFunction_CanAcceptFormatDescription
        nullptr, // VTVideoDecoderFunction_FinishDelayedFrames
        nullptr, // VTVideoDecoderFunction_StartTileSession
        nullptr, // VTVideoDecoderFunction_DecodeTile
        nullptr // VTVideoDecoderFunction_FinishDelayedTiles
    }
};
#pragma pack(pop)

#if defined(CMBASE_OBJECT_NEEDS_ALIGNMENT) && CMBASE_OBJECT_NEEDS_ALIGNMENT
    static_assert(sizeof(WebKitVP8Decoder_VideoDecoderClass.alignedClass.version) == sizeof(uint32_t), "CMBaseClass fixup is required!");
#else
    static_assert(sizeof(WebKitVP8Decoder_VideoDecoderClass.alignedClass.version) == sizeof(uintptr_t), "CMBaseClass fixup is not required!");
#endif
static_assert(offsetof(DecoderClass, alignedClass) == padSize, "CMBaseClass offset is incorrect!");
static_assert(alignof(DecoderClass) == 4, "CMBaseClass must have 4 byte alignment");

static const VTVideoDecoderVTable WebKitVP8DecoderVTable =
{
    { nullptr, &WebKitVP8Decoder_BaseClass.alignedClass },
    &WebKitVP8Decoder_VideoDecoderClass.alignedClass
};

OSStatus createWebKitVP8Decoder(FigVideoCodecType, CFAllocatorRef allocator, VTVideoDecoderRef* decoderOut)
{
    if (!decoderOut) {
        RTC_LOG(LS_ERROR) << "VP8 decoder creation failed, no decoder output";
        return kVTParameterErr;
    }

    VTVideoDecoderRef decoder = nullptr;
    auto error = CMDerivedObjectCreate(allocator, &WebKitVP8DecoderVTable.base, VTVideoDecoderGetClassID(), (CMBaseObjectRef*)&decoder);

    if (!decoder) {
        RTC_LOG(LS_ERROR) << "VP8 decoder creation failed, CMDerivedObjectCreate failed with error " << error;
        return kVTAllocationFailedErr;
    }

    if(error) {
        CFRelease(decoder);
        decoder = NULL;
    }
    *decoderOut = decoder;
    return error;
}

OSStatus invalidateVP8Decoder(CMBaseObjectRef instance)
{
    auto* decoder = static_cast<WebKitVP8Decoder*>(CMBaseObjectGetDerivedStorage(instance));
    if (!decoder)
        RTC_LOG(LS_ERROR) << "VP8 decoder: invalidation failed as instance has no decoder";
    else {
        decoder->m_instance = nullptr;
        decoder->m_receiver = nullptr;
    }
    return noErr;
}

void finalizeVP8Decoder(CMBaseObjectRef instance)
{
    invalidateVP8Decoder(instance);
}

CFStringRef copyVP8DecoderDebugDescription(CMBaseObjectRef)
{
    return CFSTR("WebKit VP8 decoder");
}

WebKitVP8Decoder* webKitVP8DecoderFromVTDecoder(VTVideoDecoderRef decoder)
{
    return static_cast<WebKitVP8Decoder*>(CMBaseObjectGetDerivedStorage(reinterpret_cast<CMBaseObjectRef>(decoder)));
}
OSStatus startVP8DecoderSession(VTVideoDecoderRef instance, VTVideoDecoderSession session, CMVideoFormatDescriptionRef formatDescription)
{
    auto* decoder = webKitVP8DecoderFromVTDecoder(instance);
    if (!decoder) {
        RTC_LOG(LS_ERROR) << "VP8 decoder: failed to get decoder from instance while starting";
        return kVTParameterErr;
    }

    decoder->m_instance = std::make_unique<LibvpxVp8Decoder>(webrtc::EnvironmentFactory().Create());
    decoder->m_receiver = std::make_unique<WebKitDecoderReceiver>(session);
    decoder->m_receiver->initializeFromFormatDescription(formatDescription);

    decoder->m_instance->RegisterDecodeCompleteCallback(decoder->m_receiver.get());

    VideoDecoder::Settings settings;
    settings.set_number_of_cores(CpuInfo::DetectNumberOfCores());

    decoder->m_instance->Configure(settings);

    return noErr;
}

static OSStatus decodeVP8DecoderFrameFromContiguousBlock(WebKitVP8Decoder& decoder, VTVideoDecoderFrame frame, CMBlockBufferRef encodedBuffer)
{
    RTC_DCHECK(CMBlockBufferIsRangeContiguous(encodedBuffer, 0, 0));

    auto size = CMBlockBufferGetDataLength(encodedBuffer);
    char* data = nullptr;
    if (auto error = CMBlockBufferGetDataPointer(encodedBuffer, 0, nullptr, nullptr, &data)) {
        RTC_LOG(LS_ERROR) << "VP8 decoder: CMBlockBufferGetDataPointer failed with error " << error;
        return error;
    }

    RTC_DCHECK(!decoder.m_receiver->currentFrame());
    decoder.m_receiver->setCurrentFrame(frame);

    EncodedImage image;
    image.SetEncodedData(WebKitEncodedImageBufferWrapper::create(reinterpret_cast<uint8_t*>(data), size));
    // We set those values as VP8DecoderImpl checks for getting a full key frame as first frame.
    image._frameType = VideoFrameType::kVideoFrameKey;
    auto error = decoder.m_instance->Decode(image, false, 0);
    if (error)
        return decoder.m_receiver->decoderFailed(error);

    RTC_DCHECK(!decoder.m_receiver->currentFrame());

    return 0;
}

OSStatus decodeVP8DecoderFrame(VTVideoDecoderRef instance, VTVideoDecoderFrame frame, CMSampleBufferRef sampleBuffer, VTDecodeFrameFlags, VTDecodeInfoFlags*)
{
    auto* decoder = webKitVP8DecoderFromVTDecoder(instance);
    if (!decoder || !decoder->m_instance || !decoder->m_receiver) {
        RTC_LOG(LS_ERROR) << "VP8 decoder: failed to get decoder from instance while decoding";
        return kVTParameterErr;
    }

    auto encodedBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!encodedBuffer) {
        RTC_LOG(LS_ERROR) << "VP8 decoder: failed to get data buffer";
        return kVTParameterErr;
    }

    CMBlockBufferRef contiguousBuffer = nullptr;
    // Make sure block buffer is contiguous.
    if (!CMBlockBufferIsRangeContiguous(encodedBuffer, 0, 0)) {
        auto status = CMBlockBufferCreateContiguous(nullptr, encodedBuffer, nullptr, nullptr, 0, 0, 0, &contiguousBuffer);
        if (status != noErr) {
            RTC_LOG(LS_ERROR) << "VP8 decoder: failed to create contiguous block buffer with error " << status;
            return kVTAllocationFailedErr;
        }
        encodedBuffer = contiguousBuffer;
    }

    auto result = decodeVP8DecoderFrameFromContiguousBlock(*decoder, frame, encodedBuffer);

    if (contiguousBuffer)
        CFRelease(contiguousBuffer);

    return result;
}

}
