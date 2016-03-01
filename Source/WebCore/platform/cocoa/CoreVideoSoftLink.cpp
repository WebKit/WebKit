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

#include "SoftLinking.h"
#include <CoreVideo/CoreVideo.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebCore, CoreVideo)

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetWidth, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetHeight, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetBaseAddress, void*, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetBytesPerRow, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetDataSize, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferGetPixelFormatType, OSType, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferLockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVPixelBufferUnlockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferPixelFormatTypeKey, CFStringRef)

#if PLATFORM(IOS)
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLESTextureCacheCreate, CVReturn, (CFAllocatorRef allocator, CFDictionaryRef cacheAttributes, CVEAGLContext eaglContext, CFDictionaryRef textureAttributes, CVOpenGLESTextureCacheRef* cacheOut), (allocator, cacheAttributes, eaglContext, textureAttributes, cacheOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLESTextureCacheCreateTextureFromImage, CVReturn, (CFAllocatorRef allocator, CVOpenGLESTextureCacheRef textureCache, CVImageBufferRef sourceImage, CFDictionaryRef textureAttributes, GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, size_t planeIndex, CVOpenGLESTextureRef* textureOut), (allocator, textureCache, sourceImage, textureAttributes, target, internalFormat, width, height, format, type, planeIndex, textureOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLESTextureCacheFlush, void, (CVOpenGLESTextureCacheRef textureCache, CVOptionFlags options), (textureCache, options))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLESTextureGetTarget, GLenum, (CVOpenGLESTextureRef image), (image))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLESTextureGetName, GLuint, (CVOpenGLESTextureRef image), (image))
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey, CFStringRef)
#else
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLTextureCacheCreate, CVReturn, (CFAllocatorRef allocator, CFDictionaryRef cacheAttributes, CGLContextObj cglContext, CGLPixelFormatObj cglPixelFormat, CFDictionaryRef textureAttributes, CVOpenGLTextureCacheRef* cacheOut), (allocator, cacheAttributes, cglContext, cglPixelFormat, textureAttributes, cacheOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLTextureCacheCreateTextureFromImage, CVReturn, (CFAllocatorRef allocator, CVOpenGLTextureCacheRef textureCache, CVImageBufferRef sourceImage, CFDictionaryRef attributes, CVOpenGLTextureRef* textureOut), (allocator, textureCache, sourceImage, attributes, textureOut))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLTextureCacheFlush, void, (CVOpenGLTextureCacheRef textureCache, CVOptionFlags options), (textureCache, options))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLTextureGetTarget, GLenum, (CVOpenGLTextureRef image), (image))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, CoreVideo, CVOpenGLTextureGetName, GLuint, (CVOpenGLTextureRef image), (image))
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, CoreVideo, kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey, CFStringRef)
#endif
