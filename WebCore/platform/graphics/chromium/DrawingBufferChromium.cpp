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

#include "Canvas2DLayerChromium.h"
#include "GraphicsContext3D.h"
#include "SharedGraphicsContext3D.h"

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>

namespace WebCore {

struct DrawingBufferInternal {
    unsigned offscreenColorTexture;
    RefPtr<Canvas2DLayerChromium> platformLayer;
};

static unsigned generateColorTexture(SharedGraphicsContext3D* context, const IntSize& size)
{
    unsigned offscreenColorTexture = context->createTexture();
    if (!offscreenColorTexture)
        return 0;

    context->bindTexture(GraphicsContext3D::TEXTURE_2D, offscreenColorTexture);
    context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::NEAREST);
    context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::NEAREST);
    context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, size.width(), size.height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, 0);
    context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, offscreenColorTexture, 0);

    return offscreenColorTexture;
}


DrawingBuffer::DrawingBuffer(SharedGraphicsContext3D* context, const IntSize& size, unsigned framebuffer)
    : m_context(context)
    , m_size(size)
    , m_framebuffer(framebuffer)
    , m_internal(new DrawingBufferInternal)
{
    context->bindFramebuffer(framebuffer);
    m_internal->offscreenColorTexture = generateColorTexture(context, size);
}

DrawingBuffer::~DrawingBuffer()
{
    m_context->deleteFramebuffer(m_framebuffer);
}

void DrawingBuffer::publishToPlatformLayer()
{
    if (m_callback)
        m_callback->willPublish();
    unsigned parentTexture = m_internal->platformLayer->textureId();
    // FIXME: We do the copy in the canvas' (child) context so that it executes in the correct order relative to
    // other commands in the child context.  This ensures that the parent texture always contains a complete
    // frame and not some intermediate result.  However, there is no synchronization to ensure that this copy
    // happens before the compositor draws.  This means we might draw stale frames sometimes.  Ideally this
    // would insert a fence into the child command stream that the compositor could wait for.
    m_context->makeContextCurrent();
    glCopyTextureToParentTexture(m_internal->offscreenColorTexture, parentTexture);
    glFlush();
}

void DrawingBuffer::reset(const IntSize& newSize)
{
    if (m_size == newSize)
        return;
    m_size = newSize;
    m_context->deleteTexture(m_internal->offscreenColorTexture);
    m_context->bindFramebuffer(m_framebuffer);
    m_internal->offscreenColorTexture = generateColorTexture(m_context, m_size);

    if (m_internal->platformLayer)
        m_internal->platformLayer->setTextureChanged();
}

PlatformLayer* DrawingBuffer::platformLayer()
{
    if (!m_internal->platformLayer)
        m_internal->platformLayer = Canvas2DLayerChromium::create(this, 0);
    return m_internal->platformLayer.get();
}

}
