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

#include "WebKitVP9Decoder.h"

#include "WebKitDecoderReceiver.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/logging.h"

namespace webrtc {

WebKitDecoderReceiver::WebKitDecoderReceiver(VTVideoDecoderSession session)
    : m_session(session)
{
}

WebKitDecoderReceiver::~WebKitDecoderReceiver()
{
    if (m_pixelBufferPool)
        CFRelease(m_pixelBufferPool);
}

void WebKitDecoderReceiver::initializeFromFormatDescription(CMFormatDescriptionRef formatDescription)
{
    // CoreAnimation doesn't support full-planar YUV, so we must convert the buffers output
    // by libvpx to bi-planar YUV. Create pixel buffer attributes and give those to the
    // decoder session for use in creating its own internal CVPixelBufferPool, which we
    // will use post-decode.
    m_isFullRange = false;
    m_is10bit = false;

    auto extensions = CMFormatDescriptionGetExtensions(formatDescription);
    if (!extensions)
        return;

    CFTypeRef extensionAtoms = CFDictionaryGetValue(extensions, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
    if (!extensionAtoms || CFGetTypeID(extensionAtoms) != CFDictionaryGetTypeID())
        return;

    auto configurationRecord = static_cast<CFDataRef>(CFDictionaryGetValue((CFDictionaryRef)extensionAtoms, CFSTR("vpcC")));
    if (!configurationRecord || CFGetTypeID(configurationRecord) != CFDataGetTypeID())
        return;

    auto configurationRecordSize = CFDataGetLength(configurationRecord);
    if (configurationRecordSize < 12)
        return;

    auto configurationRecordData = CFDataGetBytePtr(configurationRecord);
    auto bitDepthChromaAndRange = *(configurationRecordData + 6);

    if ((bitDepthChromaAndRange >> 4) == 10)
        m_is10bit = true;

    if (bitDepthChromaAndRange & 0x1)
        m_isFullRange = true;
}

CVPixelBufferPoolRef WebKitDecoderReceiver::pixelBufferPool(size_t pixelBufferWidth, size_t pixelBufferHeight, bool is10bit)
{
    if (m_pixelBufferPool && m_pixelBufferWidth == pixelBufferWidth && m_pixelBufferHeight == pixelBufferHeight && m_is10bit == is10bit)
        return m_pixelBufferPool;

    OSType pixelFormat;
    if (is10bit)
        pixelFormat = m_isFullRange ? kCVPixelFormatType_420YpCbCr10BiPlanarFullRange : kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
    else
        pixelFormat = m_isFullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;

    auto createPixelFormatAttributes = [] (OSType pixelFormat) {
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

    auto pixelBufferAttributes = createPixelFormatAttributes(pixelFormat);
    VTDecoderSessionSetPixelBufferAttributes(m_session, pixelBufferAttributes);
    CFRelease(pixelBufferAttributes);

    if (m_pixelBufferPool) {
        CFRelease(m_pixelBufferPool);
        m_pixelBufferPool = nullptr;
    }

    m_pixelBufferPool = VTDecoderSessionGetPixelBufferPool(m_session);
    if (m_pixelBufferPool)
        CFRetain(m_pixelBufferPool);

    m_pixelBufferWidth = pixelBufferWidth;
    m_pixelBufferHeight = pixelBufferHeight;
    m_is10bit = is10bit;

    return m_pixelBufferPool;
}

OSStatus WebKitDecoderReceiver::decoderFailed(int error)
{
    OSStatus vtError;
    if (error == WEBRTC_VIDEO_CODEC_NO_OUTPUT)
        vtError = noErr;
    else if (error == WEBRTC_VIDEO_CODEC_UNINITIALIZED)
        vtError = kVTVideoDecoderMalfunctionErr;
    else if (error == WEBRTC_VIDEO_CODEC_MEMORY)
        vtError = kVTAllocationFailedErr;
    else
        vtError = kVTVideoDecoderBadDataErr;

    VTDecoderSessionEmitDecodedFrame(m_session, m_currentFrame, vtError, vtError ? 0 : kVTDecodeInfo_FrameDropped, nullptr);
    m_currentFrame = nullptr;

    RTC_LOG(LS_ERROR) << "VP9 decoder: decoder failed with error " << error << ", vtError " << vtError;
    return vtError;
}

int32_t WebKitDecoderReceiver::Decoded(VideoFrame& frame)
{
    auto pixelBuffer = createPixelBufferFromFrame(frame, [this](size_t width, size_t height, BufferType type) -> CVPixelBufferRef {
        auto pixelBufferPool = this->pixelBufferPool(width, height, type == BufferType::I010);
        if (!pixelBufferPool)
            return nullptr;

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

int32_t WebKitDecoderReceiver::Decoded(VideoFrame& frame, int64_t)
{
    Decoded(frame);
    return 0;
}

void WebKitDecoderReceiver::Decoded(VideoFrame& frame, absl::optional<int32_t>, absl::optional<uint8_t>)
{
    Decoded(frame);
}

}
