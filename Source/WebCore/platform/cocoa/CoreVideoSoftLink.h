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

#pragma once

#include <CoreVideo/CoreVideo.h>
#include <wtf/SoftLinking.h>

typedef struct __IOSurface* IOSurfaceRef;

SOFT_LINK_FRAMEWORK_FOR_HEADER(WebCore, CoreVideo)

#if HAVE(CVBUFFERCOPYATTACHMENTS)
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVBufferCopyAttachments, CFDictionaryRef, (CVBufferRef buffer, CVAttachmentMode mode), (buffer, mode))
#define CVBufferCopyAttachments softLink_CoreVideo_CVBufferCopyAttachments
#endif
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVBufferGetAttachment, CFTypeRef, (CVBufferRef buffer, CFStringRef key, CVAttachmentMode* attachmentMode), (buffer, key, attachmentMode))
#define CVBufferGetAttachment softLink_CoreVideo_CVBufferGetAttachment
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVBufferGetAttachments, CFDictionaryRef, (CVBufferRef buffer, CVAttachmentMode mode), (buffer, mode))
#define CVBufferGetAttachments softLink_CoreVideo_CVBufferGetAttachments
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetTypeID, CFTypeID, (), ())
#define CVPixelBufferGetTypeID softLink_CoreVideo_CVPixelBufferGetTypeID
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetWidth, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetWidth softLink_CoreVideo_CVPixelBufferGetWidth
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetWidthOfPlane, size_t, (CVPixelBufferRef pixelBuffer, size_t planeIndex), (pixelBuffer, planeIndex))
#define CVPixelBufferGetWidthOfPlane softLink_CoreVideo_CVPixelBufferGetWidthOfPlane
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetHeight, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetHeight softLink_CoreVideo_CVPixelBufferGetHeight
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetHeightOfPlane, size_t, (CVPixelBufferRef pixelBuffer, size_t planeIndex), (pixelBuffer, planeIndex))
#define CVPixelBufferGetHeightOfPlane softLink_CoreVideo_CVPixelBufferGetHeightOfPlane
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetBaseAddress, void*, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetBaseAddress softLink_CoreVideo_CVPixelBufferGetBaseAddress
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetBytesPerRow, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetBytesPerRow softLink_CoreVideo_CVPixelBufferGetBytesPerRow
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetBytesPerRowOfPlane, size_t, (CVPixelBufferRef pixelBuffer, size_t planeIndex), (pixelBuffer, planeIndex))
#define CVPixelBufferGetBytesPerRowOfPlane softLink_CoreVideo_CVPixelBufferGetBytesPerRowOfPlane
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetPixelFormatType, OSType, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetPixelFormatType softLink_CoreVideo_CVPixelBufferGetPixelFormatType
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetPlaneCount, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetPlaneCount softLink_CoreVideo_CVPixelBufferGetPlaneCount
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetBaseAddressOfPlane, void *, (CVPixelBufferRef pixelBuffer, size_t planeIndex), (pixelBuffer, planeIndex));
#define CVPixelBufferGetBaseAddressOfPlane softLink_CoreVideo_CVPixelBufferGetBaseAddressOfPlane
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferLockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
#define CVPixelBufferLockBaseAddress softLink_CoreVideo_CVPixelBufferLockBaseAddress
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferUnlockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
#define CVPixelBufferUnlockBaseAddress softLink_CoreVideo_CVPixelBufferUnlockBaseAddress
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferPoolCreate, CVReturn, (CFAllocatorRef allocator, CFDictionaryRef poolAttributes, CFDictionaryRef pixelBufferAttributes, CVPixelBufferPoolRef* poolOut), (allocator, poolAttributes, pixelBufferAttributes, poolOut))
#define CVPixelBufferPoolCreate softLink_CoreVideo_CVPixelBufferPoolCreate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferPoolCreatePixelBuffer, CVReturn, (CFAllocatorRef allocator, CVPixelBufferPoolRef pixelBufferPool, CVPixelBufferRef* pixelBufferOut), (allocator, pixelBufferPool, pixelBufferOut))
#define CVPixelBufferPoolCreatePixelBuffer softLink_CoreVideo_CVPixelBufferPoolCreatePixelBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferPoolCreatePixelBufferWithAuxAttributes, CVReturn, (CFAllocatorRef allocator, CVPixelBufferPoolRef pixelBufferPool, CFDictionaryRef auxiliaryAttributes, CVPixelBufferRef* pixelBufferOut), (allocator, pixelBufferPool, auxiliaryAttributes, pixelBufferOut))
#define CVPixelBufferPoolCreatePixelBufferWithAuxAttributes softLink_CoreVideo_CVPixelBufferPoolCreatePixelBufferWithAuxAttributes
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetIOSurface, IOSurfaceRef, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetIOSurface softLink_CoreVideo_CVPixelBufferGetIOSurface
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferPoolGetPixelBufferAttributes, CFDictionaryRef, (CVPixelBufferPoolRef pool), (pool))
#define CVPixelBufferPoolGetPixelBufferAttributes softLink_CoreVideo_CVPixelBufferPoolGetPixelBufferAttributes
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVImageBufferCreateColorSpaceFromAttachments, CGColorSpaceRef, (CFDictionaryRef attachments), (attachments))
#define CVImageBufferCreateColorSpaceFromAttachments softLink_CoreVideo_CVImageBufferCreateColorSpaceFromAttachments

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferPixelFormatTypeKey, CFStringRef)
#define kCVPixelBufferPixelFormatTypeKey get_CoreVideo_kCVPixelBufferPixelFormatTypeKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferWidthKey, CFStringRef)
#define kCVPixelBufferWidthKey get_CoreVideo_kCVPixelBufferWidthKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferHeightKey, CFStringRef)
#define kCVPixelBufferHeightKey get_CoreVideo_kCVPixelBufferHeightKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelFormatOpenGLCompatibility, CFStringRef)
#define kCVPixelFormatOpenGLCompatibility get_CoreVideo_kCVPixelFormatOpenGLCompatibility()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelFormatOpenGLESCompatibility, CFStringRef)
#define kCVPixelFormatOpenGLESCompatibility get_CoreVideo_kCVPixelFormatOpenGLESCompatibility()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferIOSurfacePropertiesKey, CFStringRef)
#define kCVPixelBufferIOSurfacePropertiesKey get_CoreVideo_kCVPixelBufferIOSurfacePropertiesKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferPoolMinimumBufferCountKey, CFStringRef)
#define kCVPixelBufferPoolMinimumBufferCountKey get_CoreVideo_kCVPixelBufferPoolMinimumBufferCountKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferPoolAllocationThresholdKey, CFStringRef)
#define kCVPixelBufferPoolAllocationThresholdKey get_CoreVideo_kCVPixelBufferPoolAllocationThresholdKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferYCbCrMatrixKey, CFStringRef)
#define kCVImageBufferYCbCrMatrixKey get_CoreVideo_kCVImageBufferYCbCrMatrixKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_ITU_R_709_2, CFStringRef)
#define kCVImageBufferYCbCrMatrix_ITU_R_709_2 get_CoreVideo_kCVImageBufferYCbCrMatrix_ITU_R_709_2()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_ITU_R_601_4, CFStringRef)
#define kCVImageBufferYCbCrMatrix_ITU_R_601_4 get_CoreVideo_kCVImageBufferYCbCrMatrix_ITU_R_601_4()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_SMPTE_240M_1995, CFStringRef)
#define kCVImageBufferYCbCrMatrix_SMPTE_240M_1995 get_CoreVideo_kCVImageBufferYCbCrMatrix_SMPTE_240M_1995()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_DCI_P3, CFStringRef)
#define kCVImageBufferYCbCrMatrix_DCI_P3 get_CoreVideo_kCVImageBufferYCbCrMatrix_DCI_P3()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_P3_D65, CFStringRef)
#define kCVImageBufferYCbCrMatrix_P3_D65 get_CoreVideo_kCVImageBufferYCbCrMatrix_P3_D65()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferYCbCrMatrix_ITU_R_2020, CFStringRef)
#define kCVImageBufferYCbCrMatrix_ITU_R_2020 get_CoreVideo_kCVImageBufferYCbCrMatrix_ITU_R_2020()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferCGBitmapContextCompatibilityKey, CFStringRef)
#define kCVPixelBufferCGBitmapContextCompatibilityKey get_CoreVideo_kCVPixelBufferCGBitmapContextCompatibilityKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferCGImageCompatibilityKey, CFStringRef)
#define kCVPixelBufferCGImageCompatibilityKey get_CoreVideo_kCVPixelBufferCGImageCompatibilityKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferColorPrimaries_EBU_3213, CFStringRef)
#define kCVImageBufferColorPrimaries_EBU_3213 get_CoreVideo_kCVImageBufferColorPrimaries_EBU_3213()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferColorPrimaries_ITU_R_709_2, CFStringRef)
#define kCVImageBufferColorPrimaries_ITU_R_709_2 get_CoreVideo_kCVImageBufferColorPrimaries_ITU_R_709_2()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferColorPrimaries_SMPTE_C, CFStringRef)
#define kCVImageBufferColorPrimaries_SMPTE_C get_CoreVideo_kCVImageBufferColorPrimaries_SMPTE_C()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferColorPrimariesKey, CFStringRef)
#define kCVImageBufferColorPrimariesKey get_CoreVideo_kCVImageBufferColorPrimariesKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferTransferFunctionKey, CFStringRef)
#define kCVImageBufferTransferFunctionKey get_CoreVideo_kCVImageBufferTransferFunctionKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferTransferFunction_ITU_R_709_2, CFStringRef)
#define kCVImageBufferTransferFunction_ITU_R_709_2 get_CoreVideo_kCVImageBufferTransferFunction_ITU_R_709_2()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferTransferFunction_SMPTE_240M_1995, CFStringRef)
#define kCVImageBufferTransferFunction_SMPTE_240M_1995 get_CoreVideo_kCVImageBufferTransferFunction_SMPTE_240M_1995()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVImageBufferCGColorSpaceKey, CFStringRef)
#define kCVImageBufferCGColorSpaceKey get_CoreVideo_kCVImageBufferCGColorSpaceKey()

#if USE(OPENGL_ES)
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLESTextureCacheCreate, CVReturn, (CFAllocatorRef allocator, CFDictionaryRef cacheAttributes, CVEAGLContext eaglContext, CFDictionaryRef textureAttributes, CVOpenGLESTextureCacheRef* cacheOut), (allocator, cacheAttributes, eaglContext, textureAttributes, cacheOut))
#define CVOpenGLESTextureCacheCreate softLink_CoreVideo_CVOpenGLESTextureCacheCreate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLESTextureCacheCreateTextureFromImage, CVReturn, (CFAllocatorRef allocator, CVOpenGLESTextureCacheRef textureCache, CVImageBufferRef sourceImage, CFDictionaryRef textureAttributes, GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, size_t planeIndex, CVOpenGLESTextureRef* textureOut), (allocator, textureCache, sourceImage, textureAttributes, target, internalFormat, width, height, format, type, planeIndex, textureOut))
#define CVOpenGLESTextureCacheCreateTextureFromImage softLink_CoreVideo_CVOpenGLESTextureCacheCreateTextureFromImage
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLESTextureCacheFlush, void, (CVOpenGLESTextureCacheRef textureCache, CVOptionFlags options), (textureCache, options))
#define CVOpenGLESTextureCacheFlush softLink_CoreVideo_CVOpenGLESTextureCacheFlush
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLESTextureGetTarget, GLenum, (CVOpenGLESTextureRef image), (image))
#define CVOpenGLESTextureGetTarget softLink_CoreVideo_CVOpenGLESTextureGetTarget
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLESTextureGetName, GLuint, (CVOpenGLESTextureRef image), (image))
#define CVOpenGLESTextureGetName softLink_CoreVideo_CVOpenGLESTextureGetName
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLESTextureGetCleanTexCoords, void, (CVOpenGLESTextureRef image, GLfloat lowerLeft[2], GLfloat lowerRight[2], GLfloat upperLeft[2], GLfloat upperRight[2]), (image, lowerLeft, lowerRight, upperLeft, upperRight))
#define CVOpenGLESTextureGetCleanTexCoords softLink_CoreVideo_CVOpenGLESTextureGetCleanTexCoords

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey, CFStringRef)
#define kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey get_CoreVideo_kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey()
#elif USE(OPENGL)
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLTextureCacheCreate, CVReturn, (CFAllocatorRef allocator, CFDictionaryRef cacheAttributes, CGLContextObj cglContext, CGLPixelFormatObj cglPixelFormat, CFDictionaryRef textureAttributes, CVOpenGLTextureCacheRef* cacheOut), (allocator, cacheAttributes, cglContext, cglPixelFormat, textureAttributes, cacheOut))
#define CVOpenGLTextureCacheCreate softLink_CoreVideo_CVOpenGLTextureCacheCreate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLTextureCacheCreateTextureFromImage, CVReturn, (CFAllocatorRef allocator, CVOpenGLTextureCacheRef textureCache, CVImageBufferRef sourceImage, CFDictionaryRef attributes, CVOpenGLTextureRef* textureOut), (allocator, textureCache, sourceImage, attributes, textureOut))
#define CVOpenGLTextureCacheCreateTextureFromImage softLink_CoreVideo_CVOpenGLTextureCacheCreateTextureFromImage
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLTextureCacheFlush, void, (CVOpenGLTextureCacheRef textureCache, CVOptionFlags options), (textureCache, options))
#define CVOpenGLTextureCacheFlush softLink_CoreVideo_CVOpenGLTextureCacheFlush
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLTextureGetTarget, GLenum, (CVOpenGLTextureRef image), (image))
#define CVOpenGLTextureGetTarget softLink_CoreVideo_CVOpenGLTextureGetTarget
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLTextureGetName, GLuint, (CVOpenGLTextureRef image), (image))
#define CVOpenGLTextureGetName softLink_CoreVideo_CVOpenGLTextureGetName
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVOpenGLTextureGetCleanTexCoords, void, (CVOpenGLTextureRef image, GLfloat lowerLeft[2], GLfloat lowerRight[2], GLfloat upperLeft[2], GLfloat upperRight[2]), (image, lowerLeft, lowerRight, upperLeft, upperRight))
#define CVOpenGLTextureGetCleanTexCoords softLink_CoreVideo_CVOpenGLTextureGetCleanTexCoords

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey, CFStringRef)
#define kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey get_CoreVideo_kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey()
#endif

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferOpenGLESCompatibilityKey, CFStringRef)
#define kCVPixelBufferExtendedPixelsRightKey get_CoreVideo_kCVPixelBufferOpenGLESCompatibilityKey()
#endif

#if PLATFORM(MAC)
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferExtendedPixelsRightKey, CFStringRef)
#define kCVPixelBufferExtendedPixelsRightKey get_CoreVideo_kCVPixelBufferExtendedPixelsRightKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferExtendedPixelsBottomKey, CFStringRef)
#define kCVPixelBufferExtendedPixelsBottomKey get_CoreVideo_kCVPixelBufferExtendedPixelsBottomKey()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferOpenGLCompatibilityKey, CFStringRef)
#define kCVPixelBufferOpenGLCompatibilityKey get_CoreVideo_kCVPixelBufferOpenGLCompatibilityKey()
#endif

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferCreate, CVReturn, (CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType, CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut), (allocator, width, height, pixelFormatType, pixelBufferAttributes, pixelBufferOut))
#define CVPixelBufferCreate softLink_CoreVideo_CVPixelBufferCreate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferCreateWithBytes, CVReturn, (CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType, void* data, size_t bytesPerRow, void (*releaseCallback)(void*, const void*), void* releasePointer, CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut), (allocator, width, height, pixelFormatType, data, bytesPerRow, releaseCallback, releasePointer, pixelBufferAttributes, pixelBufferOut))
#define CVPixelBufferCreateWithBytes softLink_CoreVideo_CVPixelBufferCreateWithBytes
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferCreateWithIOSurface, CVReturn, (CFAllocatorRef allocator, IOSurfaceRef surface, CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef * pixelBufferOut), (allocator, surface, pixelBufferAttributes, pixelBufferOut))
#define CVPixelBufferCreateWithIOSurface softLink_CoreVideo_CVPixelBufferCreateWithIOSurface
