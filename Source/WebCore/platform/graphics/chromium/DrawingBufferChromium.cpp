/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "DrawingBuffer.h"

#include "GraphicsContext3D.h"

namespace WebCore {

static unsigned generateColorTexture(GraphicsContext3D* context, const IntSize& size)
{
    unsigned offscreenColorTexture = context->createTexture();
    if (!offscreenColorTexture)
        return 0;

    context->bindTexture(GraphicsContext3D::TEXTURE_2D, offscreenColorTexture);
    context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::NEAREST);
    context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::NEAREST);
    context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    context->texImage2DResourceSafe(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, size.width(), size.height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE);
    context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, offscreenColorTexture, 0);

    return offscreenColorTexture;
}

DrawingBuffer::DrawingBuffer(GraphicsContext3D* context,
                             const IntSize& size,
                             bool multisampleExtensionSupported,
                             bool packedDepthStencilExtensionSupported)
    : m_context(context)
    , m_size(-1, -1)
    , m_multisampleExtensionSupported(multisampleExtensionSupported)
    , m_packedDepthStencilExtensionSupported(packedDepthStencilExtensionSupported)
    , m_fbo(0)
    , m_colorBuffer(0)
    , m_depthStencilBuffer(0)
    , m_depthBuffer(0)
    , m_stencilBuffer(0)
    , m_multisampleFBO(0)
    , m_multisampleColorBuffer(0)
{
    m_fbo = context->createFramebuffer();
    context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    m_colorBuffer = generateColorTexture(context, size);
    createSecondaryBuffers();
    if (!reset(size)) {
        m_context.clear();
        return;
    }
}

DrawingBuffer::~DrawingBuffer()
{
    if (!m_context)
        return;

    clear();
}

#if USE(ACCELERATED_COMPOSITING)
void DrawingBuffer::publishToPlatformLayer()
{
    if (!m_context)
        return;

    if (multisample())
        commit();
    m_context->makeContextCurrent();
    m_context->flush();
}
#endif

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* DrawingBuffer::platformLayer()
{
    return 0;
}
#endif

Platform3DObject DrawingBuffer::platformColorBuffer() const
{
    return m_colorBuffer;
}

}
