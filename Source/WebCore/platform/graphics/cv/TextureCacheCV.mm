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

#import "config.h"
#import "TextureCacheCV.h"

#import "GraphicsContextGLOpenGL.h"

#import "CoreVideoSoftLink.h"

namespace WebCore {

std::unique_ptr<TextureCacheCV> TextureCacheCV::create(GraphicsContextGLOpenGL& context)
{
    TextureCacheType cache = nullptr;
#if USE(OPENGL_ES)
    CVEAGLContext eaglContext = static_cast<CVEAGLContext>(context.platformGraphicsContextGL());
    CVReturn error = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, nullptr, eaglContext, nullptr, &cache);
#elif USE(OPENGL)
    CGLContextObj cglContext = static_cast<CGLContextObj>(context.platformGraphicsContextGL());
    CVReturn error = CVOpenGLTextureCacheCreate(kCFAllocatorDefault, nullptr, cglContext, CGLGetPixelFormat(cglContext), nullptr, &cache);
#elif USE(ANGLE)
    // FIXME: figure out how to do this integrating via ANGLE.
    UNUSED_PARAM(context);
    CVReturn error = kCVReturnSuccess + 1;
#endif
    if (error != kCVReturnSuccess)
        return nullptr;

    RetainPtr<TextureCacheType> strongCache = adoptCF(cache);
    return makeUnique<TextureCacheCV>(context, WTFMove(strongCache));
}

TextureCacheCV::TextureCacheCV(GraphicsContextGLOpenGL& context, RetainPtr<TextureCacheType>&& cache)
    : m_context(context)
    , m_cache(cache)
{
}

RetainPtr<TextureCacheCV::TextureType> TextureCacheCV::textureFromImage(CVPixelBufferRef image, GCGLenum outputTarget, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type)
{
#if USE(ANGLE)
    // FIXME: figure out how to do this integrating via ANGLE.
    UNUSED_PARAM(image);
    UNUSED_PARAM(outputTarget);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalFormat);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    return nullptr;
#else
    TextureType bareVideoTexture = nullptr;
#if USE(OPENGL_ES)
    size_t width = CVPixelBufferGetWidth(image);
    size_t height = CVPixelBufferGetHeight(image);
    if (kCVReturnSuccess != CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault, m_cache.get(), image, nullptr, outputTarget, internalFormat, width, height, format, type, level, &bareVideoTexture))
        return nullptr;
#elif USE(OPENGL)
    UNUSED_PARAM(outputTarget);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalFormat);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    if (kCVReturnSuccess != CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, m_cache.get(), image, nullptr, &bareVideoTexture))
        return nullptr;
#endif
    RetainPtr<TextureType> videoTexture = adoptCF(bareVideoTexture);

    auto weakThis = makeWeakPtr(*this);
    dispatch_async(dispatch_get_main_queue(), [weakThis] {
        if (!weakThis)
            return;

        if (auto cache = weakThis->m_cache.get())
#if USE(OPENGL_ES)
            CVOpenGLESTextureCacheFlush(cache, 0);
#elif USE(OPENGL)
            CVOpenGLTextureCacheFlush(cache, 0);
#endif
    });

    return videoTexture;
#endif
}

}
