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

#if ENABLE(GRAPHICS_CONTEXT_GL)

#include "ExtensionsGL.h"
#include <wtf/UniqueArray.h>

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
    if (width > 0 && height > 0) {
        unsigned size;
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
    return texImage2D(target, level, internalformat, width, height, border, format, type, zero.get());
}
#endif


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

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_GL)
