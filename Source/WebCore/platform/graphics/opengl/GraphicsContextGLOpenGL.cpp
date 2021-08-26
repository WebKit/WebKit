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
#include "MediaPlayerPrivate.h"
#include "PixelBuffer.h"
#include <wtf/UniqueArray.h>

#if USE(AVFOUNDATION)
#include "GraphicsContextGLCV.h"
#endif

#include <memory>

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


#if !PLATFORM(COCOA)
void GraphicsContextGLOpenGL::setContextVisibility(bool)
{
}

void GraphicsContextGLOpenGL::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (event == SimulatedEventForTesting::GPUStatusFailure)
        m_failNextStatusCheck = true;
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
    auto pixelBuffer = readRenderingResults();
    if (!pixelBuffer)
        return;
    paintToCanvas(contextAttributes(), WTFMove(*pixelBuffer), imageBuffer.backendSize(), imageBuffer.context());
}

void GraphicsContextGLOpenGL::paintCompositedResultsToCanvas(ImageBuffer& imageBuffer)
{
    if (!makeContextCurrent())
        return;
    if (getInternalFramebufferSize().isEmpty())
        return;
    auto pixelBuffer = readCompositedResults();
    if (!pixelBuffer)
        return;
    paintToCanvas(contextAttributes(), WTFMove(*pixelBuffer), imageBuffer.backendSize(), imageBuffer.context());
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::paintRenderingResultsToPixelBuffer()
{
    // Reading premultiplied alpha would involve unpremultiplying, which is lossy.
    if (contextAttributes().premultipliedAlpha)
        return std::nullopt;
    auto results = readRenderingResultsForPainting();
    if (results && !results->size().isEmpty()) {
        ASSERT(results->format().pixelFormat == PixelFormat::RGBA8 || results->format().pixelFormat == PixelFormat::BGRA8);
        // FIXME: Make PixelBufferConversions support negative rowBytes and in-place conversions.
        const auto size = results->size();
        const size_t rowStride = size.width() * 4;
        uint8_t* top = results->data().data();
        uint8_t* bottom = top + (size.height() - 1) * rowStride;
        std::unique_ptr<uint8_t[]> temp(new uint8_t[rowStride]);
        for (; top < bottom; top += rowStride, bottom -= rowStride) {
            memcpy(temp.get(), bottom, rowStride);
            memcpy(bottom, top, rowStride);
            memcpy(top, temp.get(), rowStride);
        }
    }
    return results;
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readRenderingResultsForPainting()
{
    if (!makeContextCurrent())
        return std::nullopt;
    if (getInternalFramebufferSize().isEmpty())
        return std::nullopt;
    return readRenderingResults();
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readCompositedResultsForPainting()
{
    if (!makeContextCurrent())
        return std::nullopt;
    if (getInternalFramebufferSize().isEmpty())
        return std::nullopt;
    return readCompositedResults();
}

#if !PLATFORM(COCOA)
std::optional<PixelBuffer> GraphicsContextGLOpenGL::readCompositedResults()
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
    return contextCV->copyPixelBufferToTexture(pixelBuffer.get(), outputTexture, level, internalFormat, format, type, GraphicsContextGL::FlipY(flipY));
#else
    return player.copyVideoTextureToPlatformTexture(this, outputTexture, outputTarget, level, internalFormat, format, type, premultiplyAlpha, flipY);
#endif
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGL)
