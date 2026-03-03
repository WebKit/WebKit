/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebRTCVideoDecoderVTB.h"

#if USE(LIBWEBRTC)

#import <WebCore/CMUtilities.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static RetainPtr<CFDictionaryRef> createPixelBufferAttributes()
{
    static size_t const attributesSize = 3;
    CFTypeRef keys[attributesSize] = {
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        WebCore::kCVPixelBufferOpenGLCompatibilityKey,
#elif PLATFORM(IOS_FAMILY)
        WebCore::kCVPixelBufferExtendedPixelsRightKey,
#endif
        WebCore::kCVPixelBufferIOSurfacePropertiesKey,
        WebCore::kCVPixelBufferPixelFormatTypeKey
    };

    auto ioSurfaceValue = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
    auto pixelFormat = adoptCF(CFNumberCreate(nullptr, kCFNumberLongType, &nv12type));
    CFTypeRef values[attributesSize] = { kCFBooleanTrue, ioSurfaceValue.get(), pixelFormat.get() };
    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, attributesSize, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

int32_t WebRTCVideoDecoderVTB::decodeFrameInternal(int64_t timeStamp, std::span<const uint8_t> data)
{
    if (!m_format)
        return 0;

    RetainPtr sample = sampleBufferFromVideoData(data, m_format.get());
    if (!sample)
        return -1;

    if (!m_decoder || !protect(m_decoder)->canAccept(m_format.get())) {
        m_decoder = VideoDecoderVTB::create(m_format.get(), createPixelBufferAttributes().get());
        if (!m_decoder)
            return -1;
    }

    PAL::CMSampleBufferSetOutputPresentationTimeStamp(sample.get(), PAL::CMTimeMake(timeStamp, 1));
    VTDecodeInfoFlags decodeInfoFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
    protect(m_decoder)->decodeMultiImageFrame(sample.get(), decodeInfoFlags, makeBlockPtr(m_callback.get()));
    return 0;
}

void WebRTCVideoDecoderVTB::setVideoFormat(RetainPtr<CMVideoFormatDescriptionRef>&& format)
{
    m_format = WTF::move(format);
}

void WebRTCVideoDecoderVTB::flush()
{
    protect(m_decoder)->flush();
}

void WebRTCVideoDecoderVTB::setFormat(std::span<const uint8_t>, uint16_t width, uint16_t height)
{
    setFrameSize(width, height);
}

void WebRTCVideoDecoderVTB::setFrameSize(uint16_t width, uint16_t height)
{
    m_width = width;
    m_height = height;
}

BlockPtr<void(OSStatus, VTDecodeInfoFlags, CVImageBufferRef pixelBuffer, CMTaggedBufferGroupRef, CMTime presentationTime, CMTime)> WebRTCVideoDecoderVTB::createMultiImageCallback(WebRTCVideoDecoderCallback callback)
{
    return makeBlockPtr([callback = makeBlockPtr(callback)](OSStatus, VTDecodeInfoFlags, CVImageBufferRef pixelBuffer, CMTaggedBufferGroupRef, CMTime presentationTime, CMTime) mutable {
        if (!pixelBuffer) {
            callback(nil, 0, 0, false);
            return;
        }

        callback((CVPixelBufferRef)pixelBuffer, presentationTime.value, 0, false);
    });
}

}

#endif //  USE(LIBWEBRTC)
