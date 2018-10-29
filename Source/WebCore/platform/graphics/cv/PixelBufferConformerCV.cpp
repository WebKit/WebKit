/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if HAVE(CORE_VIDEO)

#include "GraphicsContextCG.h"
#include "ImageBufferUtilitiesCG.h"
#include "Logging.h"
#include <wtf/SoftLinking.h>

#include "CoreVideoSoftLink.h"

#if USE(VIDEOTOOLBOX)
SOFT_LINK_FRAMEWORK_OPTIONAL(VideoToolbox)
SOFT_LINK(VideoToolbox, VTPixelBufferConformerCreateWithAttributes, OSStatus, (CFAllocatorRef allocator, CFDictionaryRef attributes, VTPixelBufferConformerRef* conformerOut), (allocator, attributes, conformerOut));
SOFT_LINK(VideoToolbox, VTPixelBufferConformerIsConformantPixelBuffer, Boolean, (VTPixelBufferConformerRef conformer, CVPixelBufferRef pixBuf), (conformer, pixBuf))
SOFT_LINK(VideoToolbox, VTPixelBufferConformerCopyConformedPixelBuffer, OSStatus, (VTPixelBufferConformerRef conformer, CVPixelBufferRef sourceBuffer, Boolean ensureModifiable, CVPixelBufferRef* conformedBufferOut), (conformer, sourceBuffer, ensureModifiable, conformedBufferOut))
#endif

namespace WebCore {

PixelBufferConformerCV::PixelBufferConformerCV(CFDictionaryRef attributes)
{
#if USE(VIDEOTOOLBOX)
    VTPixelBufferConformerRef conformer = nullptr;
    VTPixelBufferConformerCreateWithAttributes(kCFAllocatorDefault, attributes, &conformer);
    ASSERT(conformer);
    m_pixelConformer = adoptCF(conformer);
#else
    UNUSED_PARAM(attributes);
    ASSERT(!attributes);
#endif
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
    verifyImageBufferIsBigEnough(address, CVPixelBufferGetDataSize(info->pixelBuffer.get()));
    RELEASE_LOG_INFO(Media, "CVPixelBufferGetBytePointerCallback() returning bytePointer: %p, size: %zu", address, CVPixelBufferGetDataSize(info->pixelBuffer.get()));
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
#if USE(VIDEOTOOLBOX)
    RetainPtr<CVPixelBufferRef> buffer { rawBuffer };

    if (!VTPixelBufferConformerIsConformantPixelBuffer(m_pixelConformer.get(), buffer.get())) {
        CVPixelBufferRef outputBuffer = nullptr;
        OSStatus status = VTPixelBufferConformerCopyConformedPixelBuffer(m_pixelConformer.get(), buffer.get(), false, &outputBuffer);
        if (status != noErr || !outputBuffer)
            return nullptr;
        return adoptCF(outputBuffer);
    }
#else
    UNUSED_PARAM(rawBuffer);
#endif
    return nullptr;
}

RetainPtr<CGImageRef> PixelBufferConformerCV::createImageFromPixelBuffer(CVPixelBufferRef rawBuffer)
{
    RetainPtr<CVPixelBufferRef> buffer { rawBuffer };
    size_t width = CVPixelBufferGetWidth(buffer.get());
    size_t height = CVPixelBufferGetHeight(buffer.get());

#if USE(VIDEOTOOLBOX)
    if (!VTPixelBufferConformerIsConformantPixelBuffer(m_pixelConformer.get(), buffer.get())) {
        CVPixelBufferRef outputBuffer = nullptr;
        OSStatus status = VTPixelBufferConformerCopyConformedPixelBuffer(m_pixelConformer.get(), buffer.get(), false, &outputBuffer);
        if (status != noErr || !outputBuffer)
            return nullptr;
        buffer = adoptCF(outputBuffer);
    }
#endif

    CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaFirst;
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(buffer.get());
    size_t byteLength = CVPixelBufferGetDataSize(buffer.get());

    ASSERT(byteLength);
    if (!byteLength)
        return nullptr;

    CVPixelBufferInfo* info = new CVPixelBufferInfo();
    info->pixelBuffer = WTFMove(buffer);
    info->lockCount = 0;

    CGDataProviderDirectCallbacks providerCallbacks = { 0, CVPixelBufferGetBytePointerCallback, CVPixelBufferReleaseBytePointerCallback, 0, CVPixelBufferReleaseInfoCallback };
    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateDirect(info, byteLength, &providerCallbacks));

    return adoptCF(CGImageCreate(width, height, 8, 32, bytesPerRow, sRGBColorSpaceRef(), bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault));
}

}

#endif // HAVE(CORE_VIDEO)
