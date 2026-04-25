/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "VideoDecoderVTB.h"

#if USE(AVFOUNDATION)

#import <CoreFoundation/CoreFoundation.h>
#import <CoreMedia/CMFormatDescription.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

RefPtr<VideoDecoderVTB> VideoDecoderVTB::create(CMVideoFormatDescriptionRef videoFormatDescription, CFDictionaryRef pixelBufferAttributes)
{
    auto videoDecoderSpecification = @{ (__bridge NSString *)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder: @YES };

    VTDecompressionSessionRef decompressionSession = nullptr;
    auto result = VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription, (__bridge CFDictionaryRef)videoDecoderSpecification, pixelBufferAttributes, nullptr, &decompressionSession);
    if (result != noErr)
        return nullptr;
    return adoptRef(*new VideoDecoderVTB(adoptCF(decompressionSession)));
}

VideoDecoderVTB::~VideoDecoderVTB() = default;

OSStatus VideoDecoderVTB::flush()
{
    return VTDecompressionSessionWaitForAsynchronousFrames(m_decompressionSession.get());
}

bool VideoDecoderVTB::isHardwareAccelerated() const
{
    CFBooleanRef isHardwareAccelerated = NULL;
    VTSessionCopyProperty(m_decompressionSession.get(), kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder, kCFAllocatorDefault, &isHardwareAccelerated);
    return isHardwareAccelerated && isHardwareAccelerated == kCFBooleanTrue;
}

bool VideoDecoderVTB::canAccept(CMVideoFormatDescriptionRef videoFormatDescription) const
{
    return VTDecompressionSessionCanAcceptFormatDescription(m_decompressionSession.get(), videoFormatDescription);
}

void VideoDecoderVTB::setProperty(CFStringRef key, CFTypeRef value)
{
    VTSessionSetProperty(m_decompressionSession.get(), key, value);
}

void VideoDecoderVTB::decodeFrame(CMSampleBufferRef sample, VTDecodeInfoFlags flags, Callback&& callback)
{
    auto result = VTDecompressionSessionDecodeFrameWithOutputHandler(m_decompressionSession.get(), sample, flags, nullptr, makeBlockPtr([callback](OSStatus status, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTime presentationTime, CMTime) mutable {
        callback(status, (CVPixelBufferRef)imageBuffer, presentationTime);
    }).get());
    if (result != noErr) {
        // If VTDecompressionSessionDecodeFrameWithOutputHandler returns an error, the handler will not be called.
        callback(result, nullptr, PAL::kCMTimeInvalid);
    }
}

void VideoDecoderVTB::decodeMultiImageFrame(CMSampleBufferRef sample, VTDecodeInfoFlags flags, CallbackMultiImage&& callback)
{
    auto result = VTDecompressionSessionDecodeFrameWithMultiImageCapableOutputHandler(m_decompressionSession.get(), sample, flags, nullptr, callback.get());
    if (result != noErr) {
        // If VTDecompressionSessionDecodeFrameWithMultiImageCapableOutputHandler returns an error, the handler will not be called.
        callback(result, 0, nullptr, nullptr, PAL::kCMTimeInvalid, PAL::kCMTimeInvalid);
    }
}

} // namespace WebCore

#endif
