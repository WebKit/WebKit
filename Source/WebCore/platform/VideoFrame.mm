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
#include "VideoFrame.h"

#if ENABLE(VIDEO) && PLATFORM(COCOA)

#import "PixelBufferConformerCV.h"
#include "VideoFrameCV.h"
#import <JavaScriptCore/TypedArrayInlines.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"

namespace WebCore {

RefPtr<JSC::Uint8ClampedArray> VideoFrame::getRGBAImageData() const
{
    PixelBufferConformerCV pixelBufferConformer((__bridge CFDictionaryRef)@{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32RGBA) });

    auto pixelBuffer = this->pixelBuffer();
    auto rgbaPixelBuffer = pixelBufferConformer.convert(pixelBuffer);
    auto status = CVPixelBufferLockBaseAddress(rgbaPixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    ASSERT_UNUSED(status, status == noErr);

    void* data = CVPixelBufferGetBaseAddressOfPlane(rgbaPixelBuffer.get(), 0);
    size_t byteLength = CVPixelBufferGetHeight(pixelBuffer) * CVPixelBufferGetWidth(pixelBuffer) * 4;
    auto result = JSC::Uint8ClampedArray::tryCreate(JSC::ArrayBuffer::create(data, byteLength), 0, byteLength);

    status = CVPixelBufferUnlockBaseAddress(rgbaPixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    ASSERT(status == noErr);

    return result;
}

#if USE(AVFOUNDATION)
RefPtr<VideoFrameCV> VideoFrame::asVideoFrameCV()
{
    if (isCV())
        return downcast<VideoFrameCV>(this);

    auto buffer = pixelBuffer();
    if (!buffer)
        return nullptr;
    return VideoFrameCV::create(presentationTime(), isMirrored(), rotation(), buffer);
}
#endif // USE(AVFOUNDATION)

}

#endif // ENABLE(VIDEO) && PLATFORM(COCOA)
