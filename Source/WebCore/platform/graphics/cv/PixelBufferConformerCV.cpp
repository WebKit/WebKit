/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "PixelBufferConformerCV.h"

#include "CVUtilities.h"
#include "GraphicsContextCG.h"
#include "ImageBufferUtilitiesCG.h"
#include "Logging.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>

#include "CoreVideoSoftLink.h"
#include "VideoToolboxSoftLink.h"

namespace WebCore {

PixelBufferConformerCV::PixelBufferConformerCV(CFDictionaryRef attributes)
{
    VTPixelBufferConformerRef conformer = nullptr;
    VTPixelBufferConformerCreateWithAttributes(kCFAllocatorDefault, attributes, &conformer);
    ASSERT(conformer);
    m_pixelConformer = adoptCF(conformer);
}

struct CVPixelBufferInfo {
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    int lockCount { 0 };
};

static const void* CVPixelBufferGetBytePointerCallback(void* refcon)
{
    ASSERT(refcon);
    if (!refcon) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferGetBytePointerCallback() called with NULL refcon");
        RELEASE_LOG_STACKTRACE(Media);
        return nullptr;
    }
    auto info = static_cast<CVPixelBufferInfo*>(refcon);

    CVReturn result = CVPixelBufferLockBaseAddress(info->pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);

    ASSERT(result == kCVReturnSuccess);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferLockBaseAddress() returned error code %d", result);
        RELEASE_LOG_STACKTRACE(Media);
        return nullptr;
    }

    ++info->lockCount;
    void* address = CVPixelBufferGetBaseAddress(info->pixelBuffer.get());
    if (!address) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferGetBaseAddress returned null");
        RELEASE_LOG_STACKTRACE(Media);
        return nullptr;
    }

    size_t byteLength = CVPixelBufferGetBytesPerRow(info->pixelBuffer.get()) * CVPixelBufferGetHeight(info->pixelBuffer.get());

    verifyImageBufferIsBigEnough(address, byteLength);
    RELEASE_LOG_INFO(Media, "CVPixelBufferGetBytePointerCallback() returning bytePointer: %p, size: %zu", address, byteLength);
    return address;
}

static void CVPixelBufferReleaseBytePointerCallback(void* refcon, const void*)
{
    ASSERT(refcon);
    if (!refcon) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferReleaseBytePointerCallback() called with NULL refcon");
        RELEASE_LOG_STACKTRACE(Media);
        return;
    }
    auto info = static_cast<CVPixelBufferInfo*>(refcon);

    CVReturn result = CVPixelBufferUnlockBaseAddress(info->pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    ASSERT(result == kCVReturnSuccess);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferLockBaseAddress() returned error code %d", result);
        RELEASE_LOG_STACKTRACE(Media);
        return;
    }

    ASSERT(info->lockCount);
    if (!info->lockCount) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferReleaseBytePointerCallback() called without matching CVPixelBufferGetBytePointerCallback()");
        RELEASE_LOG_STACKTRACE(Media);
    }
    --info->lockCount;
}

static void CVPixelBufferReleaseInfoCallback(void* refcon)
{
    ASSERT(refcon);
    if (!refcon) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferReleaseInfoCallback() called with NULL refcon");
        RELEASE_LOG_STACKTRACE(Media);
        return;
    }
    auto info = static_cast<CVPixelBufferInfo*>(refcon);

    ASSERT(!info->lockCount);
    if (info->lockCount) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferReleaseInfoCallback() called with a non-zero lockCount: %d", info->lockCount);
        RELEASE_LOG_STACKTRACE(Media);

        CVReturn result = CVPixelBufferUnlockBaseAddress(info->pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
        ASSERT(result == kCVReturnSuccess);
        if (result != kCVReturnSuccess) {
            RELEASE_LOG_ERROR(Media, "CVPixelBufferLockBaseAddress() returned error code %d", result);
            RELEASE_LOG_STACKTRACE(Media);
        }
    }

    info->pixelBuffer = nullptr;
    delete info;
}

RetainPtr<CVPixelBufferRef> PixelBufferConformerCV::convert(CVPixelBufferRef rawBuffer)
{
    RetainPtr<CVPixelBufferRef> buffer { rawBuffer };

    if (!VTPixelBufferConformerIsConformantPixelBuffer(m_pixelConformer.get(), buffer.get())) {
        CVPixelBufferRef outputBuffer = nullptr;
        OSStatus status = VTPixelBufferConformerCopyConformedPixelBuffer(m_pixelConformer.get(), buffer.get(), false, &outputBuffer);
        if (status != noErr || !outputBuffer)
            return nullptr;
        return adoptCF(outputBuffer);
    }
    return nullptr;
}

RetainPtr<CGImageRef> PixelBufferConformerCV::createImageFromPixelBuffer(CVPixelBufferRef rawBuffer)
{
    RetainPtr<CVPixelBufferRef> buffer { rawBuffer };

    if (!VTPixelBufferConformerIsConformantPixelBuffer(m_pixelConformer.get(), buffer.get())) {
        CVPixelBufferRef outputBuffer = nullptr;
        OSStatus status = VTPixelBufferConformerCopyConformedPixelBuffer(m_pixelConformer.get(), buffer.get(), false, &outputBuffer);
        if (status != noErr || !outputBuffer)
            return nullptr;
        buffer = adoptCF(outputBuffer);
    }

    auto colorSpace = createCGColorSpaceForCVPixelBuffer(rawBuffer);
    return imageFrom32BGRAPixelBuffer(WTFMove(buffer), colorSpace.get());
}

RetainPtr<CGImageRef> PixelBufferConformerCV::imageFrom32BGRAPixelBuffer(RetainPtr<CVPixelBufferRef>&& buffer, CGColorSpaceRef colorSpace)
{
    size_t width = CVPixelBufferGetWidth(buffer.get());
    size_t height = CVPixelBufferGetHeight(buffer.get());

    CGBitmapInfo bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaFirst);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(buffer.get());
    size_t byteLength = bytesPerRow * height;

    ASSERT(byteLength);
    if (!byteLength)
        return nullptr;

    CVPixelBufferInfo* info = new CVPixelBufferInfo();
    info->pixelBuffer = WTFMove(buffer);
    info->lockCount = 0;

    CGDataProviderDirectCallbacks providerCallbacks = { 0, CVPixelBufferGetBytePointerCallback, CVPixelBufferReleaseBytePointerCallback, 0, CVPixelBufferReleaseInfoCallback };
    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateDirect(info, byteLength, &providerCallbacks));

    RetainPtr<CGImageRef> image = adoptCF(CGImageCreate(width, height, 8, 32, bytesPerRow, colorSpace, bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault));

    // For historical reasons, CoreAnimation will adjust certain video color
    // spaces when displaying the video. If the video frame derived image we
    // create here is drawn to an accelerated image buffer (e.g. for a canvas),
    // CA may not do this same adjustment, resulting in the canvas pixels not
    // matching the source video. Setting this CGImage property (despite the
    // image not being IOSurface backed), avoids this non-adjustment of the
    // image color space. <rdar://88804270>
    CGImageSetProperty(image.get(), CFSTR("CA_IOSURFACE_IMAGE"), kCFBooleanTrue);

    return image;
}

}
