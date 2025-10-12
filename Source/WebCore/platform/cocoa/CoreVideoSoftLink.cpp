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

#include <CoreVideo/CoreVideo.h>
#include <wtf/SoftLinking.h>

typedef struct __IOSurface* IOSurfaceRef;

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebCore, CoreVideo)

#if HAVE(CVBUFFERCOPYATTACHMENTS)
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVBufferCopyAttachments, CFDictionaryRef, (CVBufferRef buffer, CVAttachmentMode mode), (buffer, mode))
#endif
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVBufferGetAttachment, CFTypeRef, (CVBufferRef buffer, CFStringRef key, CVAttachmentMode* attachmentMode), (buffer, key, attachmentMode))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVBufferGetAttachments, CFDictionaryRef, (CVBufferRef buffer, CVAttachmentMode mode), (buffer, mode))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVBufferRemoveAttachment, void, (CVBufferRef buffer, CFStringRef key), (buffer, key))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVBufferSetAttachment, void, (CVBufferRef buffer, CFStringRef key, CFTypeRef value, CVAttachmentMode attachmentMode), (buffer, key, value, attachmentMode))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetTypeID, CFTypeID, (), ())
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetWidth, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetWidthOfPlane, size_t, (CVPixelBufferRef pixelBuffer, size_t planeIndex), (pixelBuffer, planeIndex))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetHeight, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetHeightOfPlane, size_t, (CVPixelBufferRef pixelBuffer, size_t planeIndex), (pixelBuffer, planeIndex))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetBaseAddress, void*, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetBytesPerRow, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetBytesPerRowOfPlane, size_t, (CVPixelBufferRef pixelBuffer, size_t planeIndex), (pixelBuffer, planeIndex))
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(WebCore, CoreVideo, CVPixelBufferGetPixelFormatType, OSType, (CVPixelBufferRef pixelBuffer), (pixelBuffer), WEBCORE_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetPlaneCount, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetBaseAddressOfPlane, void *, (CVPixelBufferRef pixelBuffer, size_t planeIndex), (pixelBuffer, planeIndex));
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferLockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferUnlockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferPoolCreate, CVReturn,(CFAllocatorRef allocator, CFDictionaryRef poolAttributes, CFDictionaryRef pixelBufferAttributes, CVPixelBufferPoolRef* poolOut), (allocator, poolAttributes, pixelBufferAttributes, poolOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferPoolCreatePixelBuffer, CVReturn, (CFAllocatorRef allocator, CVPixelBufferPoolRef pixelBufferPool, CVPixelBufferRef* pixelBufferOut), (allocator, pixelBufferPool, pixelBufferOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferPoolCreatePixelBufferWithAuxAttributes, CVReturn, (CFAllocatorRef allocator, CVPixelBufferPoolRef pixelBufferPool, CFDictionaryRef auxiliaryAttributes, CVPixelBufferRef* pixelBufferOut), (allocator, pixelBufferPool, auxiliaryAttributes, pixelBufferOut))
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(WebCore, CoreVideo, CVPixelBufferGetIOSurface, IOSurfaceRef, (CVPixelBufferRef pixelBuffer), (pixelBuffer), WEBCORE_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVImageBufferCreateColorSpaceFromAttachments, CGColorSpaceRef, (CFDictionaryRef attachments), (attachments))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVIsCompressedPixelFormatAvailable, Boolean, (OSType pixelFormatType), (pixelFormatType))

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(WebCore, CoreVideo, kCVPixelBufferPixelFormatTypeKey, CFStringRef, WEBCORE_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferCGBitmapContextCompatibilityKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferCGImageCompatibilityKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferWidthKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferHeightKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelFormatOpenGLCompatibility, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelFormatOpenGLESCompatibility, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferIOSurfaceCoreAnimationCompatibilityKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferIOSurfacePropertiesKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferPoolMinimumBufferCountKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferPoolAllocationThresholdKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferYCbCrMatrixKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_ITU_R_709_2, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_ITU_R_601_4, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_SMPTE_240M_1995, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferColorPrimaries_EBU_3213, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferColorPrimaries_ITU_R_709_2, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferColorPrimaries_SMPTE_C, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferColorPrimariesKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferTransferFunctionKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferTransferFunction_ITU_R_709_2, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferTransferFunction_SMPTE_240M_1995, CFStringRef)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_DCI_P3, CFStringRef)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_P3_D65, CFStringRef)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_ITU_R_2020, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferCGColorSpaceKey, CFStringRef)

SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferCleanApertureKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferCleanApertureHeightKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferCleanApertureWidthKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferCleanApertureHorizontalOffsetKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVImageBufferCleanApertureVerticalOffsetKey, CFStringRef)

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferOpenGLESCompatibilityKey, CFStringRef)
#endif

#if PLATFORM(MAC)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferExtendedPixelsRightKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferExtendedPixelsBottomKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferOpenGLCompatibilityKey, CFStringRef)
#endif

SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferPreferRealTimeCacheModeIfEveryoneDoesKey, CFStringRef)

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferCreate, CVReturn, (CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType, CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut), (allocator, width, height, pixelFormatType, pixelBufferAttributes, pixelBufferOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferCreateWithBytes, CVReturn, (CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType, void* data, size_t bytesPerRow, void (*releaseCallback)(void*, const void*), void* releasePointer, CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut), (allocator, width, height, pixelFormatType, data, bytesPerRow, releaseCallback, releasePointer, pixelBufferAttributes, pixelBufferOut))
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(WebCore, CoreVideo, CVPixelBufferCreateWithIOSurface, CVReturn, (CFAllocatorRef allocator, IOSurfaceRef surface, CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef * pixelBufferOut), (allocator, surface, pixelBufferAttributes, pixelBufferOut), WEBCORE_EXPORT)

