/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GraphicsContextGLOpenGL.h"

#if ENABLE(WEBGL)

#include "ExtensionsGL.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "MediaPlayerPrivate.h"
#include <wtf/UniqueArray.h>

#if USE(AVFOUNDATION)
#include "GraphicsContextGLCV.h"
#endif

namespace WebCore {

void GraphicsContextGLOpenGL::resetBuffersToAutoClear()
{
    GCGLuint buffers = GraphicsContextGL::COLOR_BUFFER_BIT;
    // The GraphicsContextGL's attributes (as opposed to
    // WebGLRenderingContext's) indicate whether there is an
    // implicitly-allocated stencil buffer, for example.
    auto attrs = contextAttributes();
    if (attrs.depth)
        buffers |= GraphicsContextGL::DEPTH_BUFFER_BIT;
    if (attrs.stencil)
        buffers |= GraphicsContextGL::STENCIL_BUFFER_BIT;
    setBuffersToAutoClear(buffers);
}

void GraphicsContextGLOpenGL::setBuffersToAutoClear(GCGLbitfield buffers)
{
    auto attrs = contextAttributes();
    if (!attrs.preserveDrawingBuffer)
        m_buffersToAutoClear = buffers;
    else
        ASSERT(!m_buffersToAutoClear);
}

GCGLbitfield GraphicsContextGLOpenGL::getBuffersToAutoClear() const
{
    return m_buffersToAutoClear;
}

void GraphicsContextGLOpenGL::enablePreserveDrawingBuffer()
{
    GraphicsContextGL::enablePreserveDrawingBuffer();
    // After dynamically transitioning to preserveDrawingBuffer:true
    // for canvas capture, clear out any buffer auto-clearing state.
    m_buffersToAutoClear = 0;
}

#if !USE(ANGLE)
bool GraphicsContextGLOpenGL::texImage2DResourceSafe(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLint unpackAlignment)
{
    ASSERT(unpackAlignment == 1 || unpackAlignment == 2 || unpackAlignment == 4 || unpackAlignment == 8);
    UniqueArray<unsigned char> zero;
    unsigned size = 0;
    if (width > 0 && height > 0) {
        PixelStoreParams params;
        params.alignment = unpackAlignment;
        GCGLenum error = computeImageSizeInBytes(format, type, width, height, 1, params, &size, nullptr, nullptr);
        if (error != GraphicsContextGL::NO_ERROR) {
            synthesizeGLError(error);
            return false;
        }
        zero = makeUniqueArray<unsigned char>(size);
        if (!zero) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE);
            return false;
        }
        memset(zero.get(), 0, size);
    }
    texImage2D(target, level, internalformat, width, height, border, format, type, makeGCGLSpan(zero.get(), size));
    return true;
}
#endif


#if !USE(ANGLE)
bool GraphicsContextGLOpenGL::possibleFormatAndTypeForInternalFormat(GCGLenum internalFormat, GCGLenum& format, GCGLenum& type)
{
#define POSSIBLE_FORMAT_TYPE_CASE(internalFormatMacro, formatMacro, typeMacro) case internalFormatMacro: \
        format = formatMacro; \
        type = typeMacro; \
        break;

    switch (internalFormat) {
        POSSIBLE_FORMAT_TYPE_CASE(RGB, RGB, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA, RGBA, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(LUMINANCE_ALPHA, LUMINANCE_ALPHA, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(LUMINANCE, LUMINANCE, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(ALPHA, ALPHA, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(R8, RED, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(R8_SNORM, RED, BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(R16F, RED, HALF_FLOAT);
        POSSIBLE_FORMAT_TYPE_CASE(R32F, RED, FLOAT);
        POSSIBLE_FORMAT_TYPE_CASE(R8UI, RED_INTEGER, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(R8I, RED_INTEGER, BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(R16UI, RED_INTEGER, UNSIGNED_SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(R16I, RED_INTEGER, SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(R32UI, RED_INTEGER, UNSIGNED_INT);
        POSSIBLE_FORMAT_TYPE_CASE(R32I, RED_INTEGER, INT);
        POSSIBLE_FORMAT_TYPE_CASE(RG8, RG, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RG8_SNORM, RG, BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RG16F, RG, HALF_FLOAT);
        POSSIBLE_FORMAT_TYPE_CASE(RG32F, RG, FLOAT);
        POSSIBLE_FORMAT_TYPE_CASE(RG8UI, RG_INTEGER, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RG8I, RG_INTEGER, BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RG16UI, RG_INTEGER, UNSIGNED_SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(RG16I, RG_INTEGER, SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(RG32UI, RG_INTEGER, UNSIGNED_INT);
        POSSIBLE_FORMAT_TYPE_CASE(RG32I, RG_INTEGER, INT);
        POSSIBLE_FORMAT_TYPE_CASE(RGB8, RGB, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(SRGB8, RGB, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RGB565, RGB, UNSIGNED_SHORT_5_6_5);
        POSSIBLE_FORMAT_TYPE_CASE(RGB8_SNORM, RGB, BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(R11F_G11F_B10F, RGB, UNSIGNED_INT_10F_11F_11F_REV);
        POSSIBLE_FORMAT_TYPE_CASE(RGB9_E5, RGB, UNSIGNED_INT_5_9_9_9_REV);
        POSSIBLE_FORMAT_TYPE_CASE(RGB16F, RGB, HALF_FLOAT);
        POSSIBLE_FORMAT_TYPE_CASE(RGB32F, RGB, FLOAT);
        POSSIBLE_FORMAT_TYPE_CASE(RGB8UI, RGB_INTEGER, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RGB8I, RGB_INTEGER, BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RGB16UI, RGB_INTEGER, UNSIGNED_SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(RGB16I, RGB_INTEGER, SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(RGB32UI, RGB_INTEGER, UNSIGNED_INT);
        POSSIBLE_FORMAT_TYPE_CASE(RGB32I, RGB_INTEGER, INT);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA8, RGBA, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(SRGB8_ALPHA8, RGBA, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA8_SNORM, RGBA, BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RGB5_A1, RGBA, UNSIGNED_SHORT_5_5_5_1);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA4, RGBA, UNSIGNED_SHORT_4_4_4_4);
        POSSIBLE_FORMAT_TYPE_CASE(RGB10_A2, RGBA, UNSIGNED_INT_2_10_10_10_REV);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA16F, RGBA, HALF_FLOAT);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA32F, RGBA, FLOAT);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA8UI, RGBA_INTEGER, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA8I, RGBA_INTEGER, BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(RGB10_A2UI, RGBA_INTEGER, UNSIGNED_INT_2_10_10_10_REV);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA16UI, RGBA_INTEGER, UNSIGNED_SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA16I, RGBA_INTEGER, SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA32I, RGBA_INTEGER, INT);
        POSSIBLE_FORMAT_TYPE_CASE(RGBA32UI, RGBA_INTEGER, UNSIGNED_INT);
        POSSIBLE_FORMAT_TYPE_CASE(DEPTH_COMPONENT16, DEPTH_COMPONENT, UNSIGNED_SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(DEPTH_COMPONENT, DEPTH_COMPONENT, UNSIGNED_SHORT);
        POSSIBLE_FORMAT_TYPE_CASE(DEPTH_COMPONENT24, DEPTH_COMPONENT, UNSIGNED_INT);
        POSSIBLE_FORMAT_TYPE_CASE(DEPTH_COMPONENT32F, DEPTH_COMPONENT, FLOAT);
        POSSIBLE_FORMAT_TYPE_CASE(DEPTH_STENCIL, DEPTH_STENCIL, UNSIGNED_INT_24_8);
        POSSIBLE_FORMAT_TYPE_CASE(DEPTH24_STENCIL8, DEPTH_STENCIL, UNSIGNED_INT_24_8);
        POSSIBLE_FORMAT_TYPE_CASE(DEPTH32F_STENCIL8, DEPTH_STENCIL, FLOAT_32_UNSIGNED_INT_24_8_REV);
        POSSIBLE_FORMAT_TYPE_CASE(ExtensionsGL::SRGB_EXT, ExtensionsGL::SRGB_EXT, UNSIGNED_BYTE);
        POSSIBLE_FORMAT_TYPE_CASE(ExtensionsGL::SRGB_ALPHA_EXT, ExtensionsGL::SRGB_ALPHA_EXT, UNSIGNED_BYTE);
    default:
        return false;
    }
#undef POSSIBLE_FORMAT_TYPE_CASE

    return true;
}
#endif // !USE(ANGLE)


#if !PLATFORM(COCOA)
void GraphicsContextGLOpenGL::setContextVisibility(bool)
{
}

void GraphicsContextGLOpenGL::simulateContextChanged()
{
}

void GraphicsContextGLOpenGL::prepareForDisplay()
{
}
#endif

void GraphicsContextGLOpenGL::paintRenderingResultsToCanvas(ImageBuffer& imageBuffer)
{
    if (!makeContextCurrent())
        return;
    if (getInternalFramebufferSize().isEmpty())
        return;
    auto imageData = readRenderingResults();
    if (!imageData)
        return;
    paintToCanvas(contextAttributes(), imageData.releaseNonNull(), imageBuffer.backendSize(), imageBuffer.context());
}

void GraphicsContextGLOpenGL::paintCompositedResultsToCanvas(ImageBuffer& imageBuffer)
{
    if (!makeContextCurrent())
        return;
    if (getInternalFramebufferSize().isEmpty())
        return;
    auto imageData = readCompositedResults();
    if (!imageData)
        return;
    paintToCanvas(contextAttributes(), imageData.releaseNonNull(), imageBuffer.backendSize(), imageBuffer.context());
}

RefPtr<ImageData> GraphicsContextGLOpenGL::paintRenderingResultsToImageData()
{
    // Reading premultiplied alpha would involve unpremultiplying, which is lossy.
    if (contextAttributes().premultipliedAlpha)
        return nullptr;
    return readRenderingResultsForPainting();
}

RefPtr<ImageData> GraphicsContextGLOpenGL::readRenderingResultsForPainting()
{
    if (!makeContextCurrent())
        return nullptr;
    if (getInternalFramebufferSize().isEmpty())
        return nullptr;
    return readRenderingResults();
}

RefPtr<ImageData> GraphicsContextGLOpenGL::readCompositedResultsForPainting()
{
    if (!makeContextCurrent())
        return nullptr;
    if (getInternalFramebufferSize().isEmpty())
        return nullptr;
    return readCompositedResults();
}

#if !PLATFORM(COCOA)
RefPtr<ImageData> GraphicsContextGLOpenGL::readCompositedResults()
{
    return readRenderingResults();
}
#endif

#if ENABLE(VIDEO)
bool GraphicsContextGLOpenGL::copyTextureFromMedia(MediaPlayer& player, PlatformGLObject outputTexture, GCGLenum outputTarget, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY)
{
#if USE(AVFOUNDATION)
    auto pixelBuffer = player.pixelBufferForCurrentTime();
    if (!pixelBuffer)
        return false;

    auto contextCV = asCV();
    if (!contextCV)
        return false;

    UNUSED_VARIABLE(premultiplyAlpha);
    ASSERT_UNUSED(outputTarget, outputTarget == GraphicsContextGL::TEXTURE_2D);
    return contextCV->copyPixelBufferToTexture(pixelBuffer, outputTexture, level, internalFormat, format, type, GraphicsContextGL::FlipY(flipY));
#else
    return player.copyVideoTextureToPlatformTexture(this, outputTexture, outputTarget, level, internalFormat, format, type, premultiplyAlpha, flipY);
#endif
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGL)
