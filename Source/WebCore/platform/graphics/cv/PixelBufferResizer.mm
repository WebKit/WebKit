/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "PixelBufferResizer.h"

#if USE(VIDEOTOOLBOX)

#import "Logging.h"
#import <wtf/SoftLinking.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"

namespace WebCore {

PixelBufferResizer::PixelBufferResizer(IntSize size, OSType videoFormat)
{
    VTPixelTransferSessionRef transferSession;
    VTPixelTransferSessionCreate(NULL, &transferSession);
    ASSERT(transferSession);
    m_transferSession = adoptCF(transferSession);

    VTSessionSetProperty(transferSession, kVTPixelTransferPropertyKey_ScalingMode, kVTScalingMode_Trim);

    CFDictionaryRef sourcePixelBufferOptions = (__bridge CFDictionaryRef) @{
        (__bridge NSString *)kCVPixelBufferWidthKey :@(size.width()),
        (__bridge NSString *)kCVPixelBufferHeightKey:@(size.height()),
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey:@(videoFormat),
#if PLATFORM(IOS) && !PLATFORM(IOSMAC)
        (__bridge NSString *)kCVPixelFormatOpenGLESCompatibility : @(YES),
#else
        (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @(YES),
#endif
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ /*empty dictionary*/ },
    };
    CVPixelBufferPoolRef bufferPool;
    auto status = CVPixelBufferPoolCreate(kCFAllocatorDefault, NULL, sourcePixelBufferOptions, &bufferPool);
    ASSERT(!status);
    if (status != kCVReturnSuccess)
        return;

    m_bufferPool = adoptCF(bufferPool);
    m_size = size;
}

RetainPtr<CVPixelBufferRef> PixelBufferResizer::resize(CVPixelBufferRef inputBuffer)
{
    RetainPtr<CVPixelBufferRef> result;
    CVPixelBufferRef outputBuffer = nullptr;

    auto status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_bufferPool.get(), &outputBuffer);
    if (status) {
        RELEASE_LOG(Media, "PixelBufferResizer::resize, CVPixelBufferPoolCreatePixelBuffer failed with error %d", static_cast<int>(status));
        return nullptr;
    }
    result = adoptCF(outputBuffer);

    auto err = VTPixelTransferSessionTransferImage(m_transferSession.get(), inputBuffer, outputBuffer);
    if (err) {
        RELEASE_LOG(Media, "PixelBufferResizer::resize, VTPixelTransferSessionTransferImage failed with error %d", static_cast<int>(err));
        return nullptr;
    }

    return result;
}

RetainPtr<CMSampleBufferRef> PixelBufferResizer::resize(CMSampleBufferRef sampleBuffer)
{
    auto convertedPixelBuffer = resize(CMSampleBufferGetImageBuffer(sampleBuffer));
    if (!convertedPixelBuffer)
        return nullptr;

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    auto status = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, convertedPixelBuffer.get(), &formatDescription);
    if (status != noErr) {
        RELEASE_LOG(Media, "PixelBufferResizer::resize: failed to create CMVideoFormatDescription with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    CMItemCount itemCount = 0;
    status = CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, 1, nullptr, &itemCount);
    if (status != noErr) {
        RELEASE_LOG(Media, "PixelBufferResizer::resize: CMSampleBufferGetSampleTimingInfoArray failed with error code: %d", static_cast<int>(status));
        return nullptr;
    }
    Vector<CMSampleTimingInfo> timingInfoArray;
    CMSampleTimingInfo* timeingInfoPtr = nullptr;
    if (itemCount) {
        timingInfoArray.grow(itemCount);
        status = CMSampleBufferGetSampleTimingInfoArray(sampleBuffer, itemCount, timingInfoArray.data(), nullptr);
        if (status != noErr) {
            RELEASE_LOG(Media, "PixelBufferResizer::resize: CMSampleBufferGetSampleTimingInfoArray failed with error code: %d", static_cast<int>(status));
            return nullptr;
        }
        timeingInfoPtr = timingInfoArray.data();
    }

    CMSampleBufferRef resizedSampleBuffer;
    status = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, convertedPixelBuffer.get(), formatDescription, timeingInfoPtr, &resizedSampleBuffer);
    CFRelease(formatDescription);
    if (status != noErr) {
        RELEASE_LOG(Media, "PixelBufferResizer::resize: failed to create CMSampleBuffer with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    return adoptCF(resizedSampleBuffer);
}

} // namespace WebCore

#endif // USE(VIDEOTOOLBOX)
