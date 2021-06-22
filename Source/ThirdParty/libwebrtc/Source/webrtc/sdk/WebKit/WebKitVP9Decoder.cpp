/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "WebKitVP9Decoder.h"

#include "WebKitDecoderReceiver.h"
#include "modules/video_coding/codecs/vp9/vp9_impl.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/cpu_info.h"

namespace webrtc {

static OSStatus createWebKitVP9Decoder(FigVideoCodecType, CFAllocatorRef allocator, VTVideoDecoderRef*);
void registerWebKitVP9Decoder()
{
    VTRegisterVideoDecoder('vp09', createWebKitVP9Decoder);
}

typedef struct {
    std::unique_ptr<VP9DecoderImpl> m_instance;
    std::unique_ptr<WebKitDecoderReceiver> m_receiver;
} WebKitVP9Decoder;

static OSStatus invalidateVP9Decoder(CMBaseObjectRef);
static void finalizeVP9Decoder(CMBaseObjectRef);
static CFStringRef copyVP9DecoderDebugDescription(CMBaseObjectRef);

static const CMBaseClass WebKitVP9Decoder_BaseClass =
{
    kCMBaseObject_ClassVersion_1,
    sizeof(WebKitVP9Decoder),
    nullptr, // Comparison by pointer equality
    invalidateVP9Decoder,
    finalizeVP9Decoder,
    copyVP9DecoderDebugDescription,
    nullptr, // CopyProperty
    nullptr, // SetProperty
    nullptr,
    nullptr
};

static OSStatus startVP9DecoderSession(VTVideoDecoderRef, VTVideoDecoderSession, CMVideoFormatDescriptionRef);
static OSStatus decodeVP9DecoderFrame(VTVideoDecoderRef, VTVideoDecoderFrame, CMSampleBufferRef, VTDecodeFrameFlags, VTDecodeInfoFlags*);

static const VTVideoDecoderClass WebKitVP9Decoder_VideoDecoderClass =
{
    kVTVideoDecoder_ClassVersion_1,
    startVP9DecoderSession,
    decodeVP9DecoderFrame,
    nullptr, // VTVideoDecoderFunction_CopySupportedPropertyDictionary,
    nullptr, // VTVideoDecoderFunction_SetProperties
    nullptr, // VTVideoDecoderFunction_CopySerializableProperties
    nullptr, // VTVideoDecoderFunction_CanAcceptFormatDescription
    nullptr, // VTVideoDecoderFunction_FinishDelayedFrames
    nullptr, // VTVideoDecoderFunction_StartTileSession
    nullptr, // VTVideoDecoderFunction_DecodeTile
    nullptr // VTVideoDecoderFunction_FinishDelayedTiles
};

static const VTVideoDecoderVTable WebKitVP9DecoderVTable =
{
    { nullptr, &WebKitVP9Decoder_BaseClass },
    &WebKitVP9Decoder_VideoDecoderClass
};

OSStatus createWebKitVP9Decoder(FigVideoCodecType, CFAllocatorRef allocator, VTVideoDecoderRef* decoderOut)
{
    if (!decoderOut) {
        RTC_LOG(LS_ERROR) << "VP9 decoder creation failed, no decoder output";
        return kVTParameterErr;
    }

    VTVideoDecoderRef decoder = nullptr;
    auto error = CMDerivedObjectCreate(allocator, &WebKitVP9DecoderVTable.base, VTVideoDecoderGetClassID(), (CMBaseObjectRef*)&decoder);

    if (!decoder) {
        RTC_LOG(LS_ERROR) << "VP9 decoder creation failed, CMDerivedObjectCreate failed with error " << error;
        return kVTAllocationFailedErr;
    }

    if(error) {
        CFRelease(decoder);
        decoder = NULL;
    }
    *decoderOut = decoder;
    return error;
}

OSStatus invalidateVP9Decoder(CMBaseObjectRef instance)
{
    auto* decoder = static_cast<WebKitVP9Decoder*>(CMBaseObjectGetDerivedStorage(instance));
    if (!decoder)
        RTC_LOG(LS_ERROR) << "VP9 decoder: invalidation failed as instance has no decoder";
    else {
        decoder->m_instance = nullptr;
        decoder->m_receiver = nullptr;
    }
    return noErr;
}

void finalizeVP9Decoder(CMBaseObjectRef instance)
{
    invalidateVP9Decoder(instance);
}

CFStringRef copyVP9DecoderDebugDescription(CMBaseObjectRef)
{
    return CFSTR("WebKit VP9 decoder");
}

WebKitVP9Decoder* webKitVP9DecoderFromVTDecoder(VTVideoDecoderRef decoder)
{
    return static_cast<WebKitVP9Decoder*>(CMBaseObjectGetDerivedStorage(reinterpret_cast<CMBaseObjectRef>(decoder)));
}
OSStatus startVP9DecoderSession(VTVideoDecoderRef instance, VTVideoDecoderSession session, CMVideoFormatDescriptionRef formatDescription)
{
    auto* decoder = webKitVP9DecoderFromVTDecoder(instance);
    if (!decoder) {
        RTC_LOG(LS_ERROR) << "VP9 decoder: failed to get decoder from instance while starting";
        return kVTParameterErr;
    }

    decoder->m_instance = std::make_unique<VP9DecoderImpl>();
    decoder->m_receiver = std::make_unique<WebKitDecoderReceiver>(session);
    decoder->m_receiver->initializeFromFormatDescription(formatDescription);

    decoder->m_instance->RegisterDecodeCompleteCallback(decoder->m_receiver.get());

    VideoCodec codec;
    auto dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
    codec.width = dimensions.width;
    codec.height = dimensions.height;

    decoder->m_instance->InitDecode(&codec, CpuInfo::DetectNumberOfCores());

    return noErr;
}

static OSStatus decodeVP9DecoderFrameFromContiguousBlock(WebKitVP9Decoder& decoder, VTVideoDecoderFrame frame, CMBlockBufferRef encodedBuffer)
{
    RTC_DCHECK(CMBlockBufferIsRangeContiguous(encodedBuffer, 0, 0));

    auto size = CMBlockBufferGetDataLength(encodedBuffer);
    char* data = nullptr;
    if (auto error = CMBlockBufferGetDataPointer(encodedBuffer, 0, nullptr, nullptr, &data)) {
        RTC_LOG(LS_ERROR) << "VP9 decoder: CMBlockBufferGetDataPointer failed with error " << error;
        return error;
    }

    RTC_DCHECK(!decoder.m_receiver->currentFrame());
    decoder.m_receiver->setCurrentFrame(frame);

    EncodedImage image { reinterpret_cast<uint8_t*>(data), size, size };
    // We set those values as VP9DecoderImpl checks for getting a full key frame as first frame.
    image._frameType = VideoFrameType::kVideoFrameKey;
    image._completeFrame = true;
    auto error = decoder.m_instance->Decode(image, false, 0);
    if (error)
        return decoder.m_receiver->decoderFailed(error);

    RTC_DCHECK(!decoder.m_receiver->currentFrame());

    return 0;
}

OSStatus decodeVP9DecoderFrame(VTVideoDecoderRef instance, VTVideoDecoderFrame frame, CMSampleBufferRef sampleBuffer, VTDecodeFrameFlags, VTDecodeInfoFlags*)
{
    auto* decoder = webKitVP9DecoderFromVTDecoder(instance);
    if (!decoder || !decoder->m_instance || !decoder->m_receiver) {
        RTC_LOG(LS_ERROR) << "VP9 decoder: failed to get decoder from instance while decoding";
        return kVTParameterErr;
    }

    auto encodedBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!encodedBuffer) {
        RTC_LOG(LS_ERROR) << "VP9 decoder: failed to get data buffer";
        return kVTParameterErr;
    }

    CMBlockBufferRef contiguousBuffer = nullptr;
    // Make sure block buffer is contiguous.
    if (!CMBlockBufferIsRangeContiguous(encodedBuffer, 0, 0)) {
        auto status = CMBlockBufferCreateContiguous(nullptr, encodedBuffer, nullptr, nullptr, 0, 0, 0, &contiguousBuffer);
        if (status != noErr) {
            RTC_LOG(LS_ERROR) << "VP9 decoder: failed to create contiguous block buffer with error " << status;
            return kVTAllocationFailedErr;
        }
        encodedBuffer = contiguousBuffer;
    }

    auto result = decodeVP9DecoderFrameFromContiguousBlock(*decoder, frame, encodedBuffer);

    if (contiguousBuffer)
        CFRelease(contiguousBuffer);

    return result;
}

}
