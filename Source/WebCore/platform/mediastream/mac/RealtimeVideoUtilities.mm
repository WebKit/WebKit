/*
 * Copyright (C) 2020 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RealtimeVideoUtilities.h"

#import "CoreVideoSoftLink.h"

namespace WebCore {

RetainPtr<CVPixelBufferPoolRef> createPixelBufferPool(size_t width, size_t height, OSType videoCaptureFormat)
{
    auto pixelAttributes = @{
        (__bridge NSString *)kCVPixelBufferWidthKey: @(width),
        (__bridge NSString *)kCVPixelBufferHeightKey: @(height),
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(videoCaptureFormat),
        (__bridge NSString *)kCVPixelBufferCGImageCompatibilityKey: @NO,
#if PLATFORM(IOS_FAMILY)
        (__bridge NSString *)kCVPixelFormatOpenGLESCompatibility : @YES,
#else
        (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @YES,
#endif
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ }
    };

    CVPixelBufferPoolRef pool = nullptr;
    auto status = CVPixelBufferPoolCreate(kCFAllocatorDefault, nullptr, (__bridge CFDictionaryRef)pixelAttributes, &pool);

    if (status != kCVReturnSuccess)
        return nullptr;

    return adoptCF(pool);
}

RetainPtr<CVPixelBufferRef> createPixelBufferFromPool(CVPixelBufferPoolRef pixelBufferPool)
{
    CVPixelBufferRef pixelBuffer = nullptr;
    auto status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pixelBufferPool, &pixelBuffer);

    if (status != kCVReturnSuccess)
        return nullptr;
    return adoptCF(pixelBuffer);
}

}
