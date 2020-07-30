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

#if defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)

#include "WebKitUtilities.h"
#include <CoreMedia/CMBaseObject.h>
#include <VideoToolbox/VTVideoDecoder.h>
#include <VideoToolbox/VTVideoDecoderRegistration.h>
#include "modules/video_coding/codecs/vp9/vp9_impl.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/cpu_info.h"

namespace webrtc {

static OSStatus createWebKitVP9Decoder(FigVideoCodecType, CFAllocatorRef allocator, VTVideoDecoderRef*);
void registerWebKitVP9Decoder()
{
    VTRegisterVideoDecoder('vp09', createWebKitVP9Decoder);
}

class WebKitVP9DecoderReceiver;
typedef struct {
    std::unique_ptr<VP9DecoderImpl> m_instance;
    std::unique_ptr<WebKitVP9DecoderReceiver> m_receiver;
} WebKitVP9Decoder;

class WebKitVP9DecoderReceiver final : public DecodedImageCallback {
public:
    explicit WebKitVP9DecoderReceiver(VTVideoDecoderSession);
    ~WebKitVP9DecoderReceiver();

    VTVideoDecoderFrame currentFrame() const { return m_currentFrame; }
    void setCurrentFrame(VTVideoDecoderFrame currentFrame) { m_currentFrame = currentFrame; }
    OSStatus decoderFailed(int error);

    void createPixelBufferPoolForFormatDescription(CMFormatDescriptionRef);

private:
    int32_t Decoded(VideoFrame&) final;
    int32_t Decoded(VideoFrame&, int64_t decode_time_ms) final;
    void Decoded(VideoFrame&, absl::optional<int32_t> decode_time_ms, absl::optional<uint8_t> qp) final;

    VTVideoDecoderSession m_session { nullptr };
    VTVideoDecoderFrame m_currentFrame { nullptr };
    size_t m_pixelBufferWidth { 0 };
    size_t m_pixelBufferHeight { 0 };
    CVPixelBufferPoolRef m_pixelBufferPool { nullptr };
};

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
    nullptr // SetProperty
};

static OSStatus startVP9DecoderSession(VTVideoDecoderRef, VTVideoDecoderSession, CMVideoFormatDescriptionRef);
static OSStatus decodeVP9DecoderFrame(VTVideoDecoderRef, VTVideoDecoderFrame, CMSampleBufferRef, VTDecodeFrameFlags, VTDecodeInfoFlags*);

static const VTVideoDecoderClass WebKitVP9Decoder_VideoEncoderClass =
{
    kVTVideoDecoder_ClassVersion_3,
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
    &WebKitVP9Decoder_VideoEncoderClass
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
    decoder->m_receiver = std::make_unique<WebKitVP9DecoderReceiver>(session);
    decoder->m_receiver->createPixelBufferPoolForFormatDescription(formatDescription);

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

WebKitVP9DecoderReceiver::WebKitVP9DecoderReceiver(VTVideoDecoderSession session)
    : m_session(session)
{
}

WebKitVP9DecoderReceiver::~WebKitVP9DecoderReceiver()
{
    if (m_pixelBufferPool)
        CFRelease(m_pixelBufferPool);
}

void WebKitVP9DecoderReceiver::createPixelBufferPoolForFormatDescription(CMFormatDescriptionRef formatDescription)
{
    // CoreAnimation doesn't support full-planar YUV, so we must convert the buffers output
    // by libvpx to bi-planar YUV. Create pixel buffer attributes and give those to the
    // decoder session for use in creating its own internal CVPixelBufferPool, which we
    // will use post-decode.
    bool isFullRange = false;
    bool is10Bit = false;

    do {
        auto extensions = CMFormatDescriptionGetExtensions(formatDescription);
        if (!extensions)
            break;

        CFTypeRef extensionAtoms = CFDictionaryGetValue(extensions, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
        if (!extensionAtoms || CFGetTypeID(extensionAtoms) != CFDictionaryGetTypeID())
            break;

        auto configurationRecord = static_cast<CFDataRef>(CFDictionaryGetValue((CFDictionaryRef)extensionAtoms, CFSTR("vpcC")));
        if (!configurationRecord || CFGetTypeID(configurationRecord) != CFDataGetTypeID())
            break;

        auto configurationRecordSize = CFDataGetLength(configurationRecord);
        if (configurationRecordSize < 12)
            break;

        auto configurationRecordData = CFDataGetBytePtr(configurationRecord);
        auto bitDepthChromaAndRange = *(configurationRecordData + 6);

        if ((bitDepthChromaAndRange >> 4) == 10)
            is10Bit = true;

        if (bitDepthChromaAndRange & 0x1)
            isFullRange = true;
    } while (false);

    OSType pixelFormat;
    if (is10Bit)
        pixelFormat = isFullRange ? kCVPixelFormatType_420YpCbCr10BiPlanarFullRange : kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
    else
        pixelFormat = isFullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;

    auto createPixelFormatAttributes = [] (OSType pixelFormat, int32_t borderPixels) {
        auto createNumber = [] (int32_t format) -> CFNumberRef {
            return CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &format);
        };
        auto cfPixelFormats = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
        auto formatNumber = createNumber(pixelFormat);
        CFArrayAppendValue(cfPixelFormats, formatNumber);
        CFRelease(formatNumber);

        auto borderPixelsValue = createNumber(32);

        const void* keys[] = {
            kCVPixelBufferPixelFormatTypeKey,
            kCVPixelBufferExtendedPixelsLeftKey,
            kCVPixelBufferExtendedPixelsRightKey,
            kCVPixelBufferExtendedPixelsTopKey,
            kCVPixelBufferExtendedPixelsBottomKey,
        };
        const void* values[] = {
            cfPixelFormats,
            borderPixelsValue,
            borderPixelsValue,
            borderPixelsValue,
            borderPixelsValue,
        };
        auto attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFRelease(borderPixelsValue);
        CFRelease(cfPixelFormats);
        return attributes;
    };

    auto pixelBufferAttributes = createPixelFormatAttributes(pixelFormat, 32);
    VTDecoderSessionSetPixelBufferAttributes(m_session, pixelBufferAttributes);
    CFRelease(pixelBufferAttributes);

    if (m_pixelBufferPool) {
        CFRelease(m_pixelBufferPool);
        m_pixelBufferPool = nullptr;
    }

    m_pixelBufferPool = VTDecoderSessionGetPixelBufferPool(m_session);
    if (m_pixelBufferPool)
        CFRetain(m_pixelBufferPool);
}

OSStatus WebKitVP9DecoderReceiver::decoderFailed(int error)
{
    OSStatus vtError = kVTVideoDecoderBadDataErr;
    if (error == WEBRTC_VIDEO_CODEC_NO_OUTPUT)
        vtError = noErr;
    else if (error == WEBRTC_VIDEO_CODEC_UNINITIALIZED)
        vtError = kVTVideoDecoderMalfunctionErr;
    else if (error == WEBRTC_VIDEO_CODEC_MEMORY)
        vtError = kVTAllocationFailedErr;
    VTDecoderSessionEmitDecodedFrame(m_session, m_currentFrame, vtError, 0, nullptr);
    m_currentFrame = nullptr;

    RTC_LOG(LS_ERROR) << "VP9 decoder: decoder failed with error " << error << ", vtError " << vtError;
    return vtError;
}

int32_t WebKitVP9DecoderReceiver::Decoded(VideoFrame& frame)
{
    auto pixelBuffer = pixelBufferFromFrame(frame, [this](size_t width, size_t height) -> CVPixelBufferRef {
        CVPixelBufferRef pixelBuffer = nullptr;
        if (CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_pixelBufferPool, &pixelBuffer) == kCVReturnSuccess)
            return pixelBuffer;
        return nullptr;
    });

    VTDecoderSessionEmitDecodedFrame(m_session, m_currentFrame, pixelBuffer ? noErr : -1, 0, pixelBuffer);
    m_currentFrame = nullptr;
    if (pixelBuffer)
        CFRelease(pixelBuffer);
    return 0;
}

int32_t WebKitVP9DecoderReceiver::Decoded(VideoFrame& frame, int64_t)
{
    Decoded(frame);
    return 0;
}

void WebKitVP9DecoderReceiver::Decoded(VideoFrame& frame, absl::optional<int32_t>, absl::optional<uint8_t>)
{
    Decoded(frame);
}

}

#else // defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)

namespace webrtc {

void registerWebKitVP9Decoder()
{
}

}
#endif // defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)
