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

#ifndef CoreVideoSoftLink_h
#define CoreVideoSoftLink_h

#include "SoftLinking.h"
#include <CoreVideo/CoreVideo.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(WebCore, CoreVideo)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetWidth, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetWidth softLink_CoreVideo_CVPixelBufferGetWidth
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetHeight, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetHeight softLink_CoreVideo_CVPixelBufferGetHeight
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetBaseAddress, void*, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetBaseAddress softLink_CoreVideo_CVPixelBufferGetBaseAddress
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetBytesPerRow, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetBytesPerRow softLink_CoreVideo_CVPixelBufferGetBytesPerRow
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetDataSize, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetDataSize softLink_CoreVideo_CVPixelBufferGetDataSize
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferGetPixelFormatType, OSType, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
#define CVPixelBufferGetPixelFormatType softLink_CoreVideo_CVPixelBufferGetPixelFormatType
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferLockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
#define CVPixelBufferLockBaseAddress softLink_CoreVideo_CVPixelBufferLockBaseAddress
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreVideo, CVPixelBufferUnlockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
#define CVPixelBufferUnlockBaseAddress softLink_CoreVideo_CVPixelBufferUnlockBaseAddress

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferPixelFormatTypeKey, CFStringRef)
#define kCVPixelBufferPixelFormatTypeKey get_CoreVideo_kCVPixelBufferPixelFormatTypeKey()

#if PLATFORM(IOS)
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

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey, CFStringRef)
#define kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey get_CoreVideo_kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey()
#else
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

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreVideo, kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey, CFStringRef)
#define kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey get_CoreVideo_kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey()
#endif

#endif // CoreVideoSoftLink_h
