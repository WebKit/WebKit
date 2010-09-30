/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(ACCELERATED_2D_CANVAS)

#include "DrawingBuffer.h"

#include "SharedGraphicsContext3D.h"
#include "WebGLLayer.h"

#import "BlockExceptions.h"

namespace WebCore {

DrawingBuffer::DrawingBuffer(SharedGraphicsContext3D* context, const IntSize& size, unsigned framebuffer)
    : m_context(context)
    , m_size(size)
    , m_framebuffer(framebuffer)
{
    context->bindFramebuffer(framebuffer);

    // Create the WebGLLayer
    BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_platformLayer.adoptNS([[WebGLLayer alloc] initWithGraphicsContext3D:0]);
#ifndef NDEBUG
        [m_platformLayer.get() setName:@"WebGL Layer"];
#endif    
    END_BLOCK_OBJC_EXCEPTIONS
}

DrawingBuffer::~DrawingBuffer()
{
    m_context->bindFramebuffer(m_framebuffer);
    m_context->deleteFramebuffer(m_framebuffer);
}

void DrawingBuffer::reset(const IntSize& newSize)
{
    if (m_size == newSize)
        return;
    m_size = newSize;

    m_context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, m_size.width(), m_size.height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, 0);
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* DrawingBuffer::platformLayer()
{
    return m_platformLayer.get();
}
#endif

}

#endif
