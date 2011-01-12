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

#if ENABLE(ACCELERATED_2D_CANVAS) || ENABLE(3D_CANVAS)

#include "DrawingBuffer.h"

#include "Extensions3D.h"

namespace WebCore {

PassRefPtr<DrawingBuffer> DrawingBuffer::create(GraphicsContext3D* context, const IntSize& size)
{
    Extensions3D* extensions = context->getExtensions();
    bool multisampleSupported = extensions->supports("GL_ANGLE_framebuffer_blit") && extensions->supports("GL_ANGLE_framebuffer_multisample");
    if (multisampleSupported) {
        extensions->ensureEnabled("GL_ANGLE_framebuffer_blit");
        extensions->ensureEnabled("GL_ANGLE_framebuffer_multisample");
    }
    bool packedDepthStencilSupported = extensions->supports("GL_OES_packed_depth_stencil");
    if (packedDepthStencilSupported)
        extensions->ensureEnabled("GL_OES_packed_depth_stencil");
    RefPtr<DrawingBuffer> drawingBuffer = adoptRef(new DrawingBuffer(context, size, multisampleSupported, packedDepthStencilSupported));
    return (drawingBuffer->m_context) ? drawingBuffer.release() : 0;
}

void DrawingBuffer::clear()
{
    if (!m_context)
        return;
        
    m_context->makeContextCurrent();
    m_context->deleteTexture(m_colorBuffer);
    m_colorBuffer = 0;
    
    if (m_multisampleColorBuffer) {
        m_context->deleteRenderbuffer(m_multisampleColorBuffer);
        m_multisampleColorBuffer = 0;
    }
    
    if (m_multisampleDepthStencilBuffer) {
        m_context->deleteRenderbuffer(m_multisampleDepthStencilBuffer);
        m_multisampleDepthStencilBuffer = 0;
    }
    
    if (m_depthStencilBuffer) {
        m_context->deleteRenderbuffer(m_depthStencilBuffer);
        m_depthStencilBuffer = 0;
    }
    
    if (m_multisampleFBO) {
        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
        m_context->deleteFramebuffer(m_multisampleFBO);
        m_multisampleFBO = 0;
    }
        
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    m_context->deleteFramebuffer(m_fbo);
    m_fbo = 0;
    
    m_context.clear();
}

void DrawingBuffer::createSecondaryBuffers()
{
    const GraphicsContext3D::Attributes& attributes = m_context->getContextAttributes();

    // Create the stencil and depth buffer if needed
    if (!multisample() && (attributes.stencil || attributes.depth))
        m_depthStencilBuffer = m_context->createRenderbuffer();

    // create a multisample FBO
    if (multisample()) {
        m_multisampleFBO = m_context->createFramebuffer();
        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
        m_multisampleColorBuffer = m_context->createRenderbuffer();
        if (attributes.stencil || attributes.depth)
            m_multisampleDepthStencilBuffer = m_context->createRenderbuffer();
    }
}

void DrawingBuffer::reset(const IntSize& newSize)
{
    if (m_size == newSize)
        return;
    m_size = newSize;

    if (!m_context)
        return;
        
    m_context->makeContextCurrent();
    
    const GraphicsContext3D::Attributes& attributes = m_context->getContextAttributes();
    unsigned long internalColorFormat, colorFormat, internalDepthStencilFormat = 0;
    if (attributes.alpha) {
        internalColorFormat = GraphicsContext3D::RGBA;
        colorFormat = GraphicsContext3D::RGBA;
    } else {
        internalColorFormat = GraphicsContext3D::RGB;
        colorFormat = GraphicsContext3D::RGB;
    }
    if (attributes.stencil || attributes.depth) {
        // We don't allow the logic where stencil is required and depth is not.
        // See GraphicsContext3D constructor.

        // FIXME:  If packed depth/stencil is not supported, we should
        // create separate renderbuffers for depth and stencil.
        if (attributes.stencil && attributes.depth && m_packedDepthStencilExtensionSupported)
            internalDepthStencilFormat = Extensions3D::DEPTH24_STENCIL8;
        else
            internalDepthStencilFormat = GraphicsContext3D::DEPTH_COMPONENT16;
    }

    // resize multisample FBO
    if (multisample()) {
        int maxSampleCount = 0;
        
        m_context->getIntegerv(Extensions3D::MAX_SAMPLES, &maxSampleCount);
        int sampleCount = std::min(8, maxSampleCount);

        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);

        m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_multisampleColorBuffer);
        m_context->getExtensions()->renderbufferStorageMultisample(GraphicsContext3D::RENDERBUFFER, sampleCount, internalColorFormat, m_size.width(), m_size.height());
        m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::RENDERBUFFER, m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth) {
            m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_multisampleDepthStencilBuffer);
            m_context->getExtensions()->renderbufferStorageMultisample(GraphicsContext3D::RENDERBUFFER, sampleCount, internalDepthStencilFormat, m_size.width(), m_size.height());
            if (attributes.stencil)
                m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::STENCIL_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_multisampleDepthStencilBuffer);
            if (attributes.depth)
                m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_multisampleDepthStencilBuffer);
        }
        m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
        if (m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
            // Cleanup
            clear();
            return;
        }
    }

    // resize regular FBO
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);

    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_colorBuffer);
    m_context->texImage2DResourceSafe(GraphicsContext3D::TEXTURE_2D, 0, internalColorFormat, m_size.width(), m_size.height(), 0, colorFormat, GraphicsContext3D::UNSIGNED_BYTE);
    m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_colorBuffer, 0);
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, 0);
    if (!multisample() && (attributes.stencil || attributes.depth)) {
        m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
        m_context->renderbufferStorage(GraphicsContext3D::RENDERBUFFER, internalDepthStencilFormat, m_size.width(), m_size.height());
        if (attributes.stencil)
            m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::STENCIL_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
        if (attributes.depth)
            m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
        m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
    }
    if (m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        // Cleanup
        clear();
        return;
    }

    if (multisample())
        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);

    if (!m_context->getExtensions()->supports("GL_CHROMIUM_resource_safe")) {
        // Initialize renderbuffers (depth/stencil).
        float clearDepth = 0;
        int clearStencil = 0;
        unsigned char depthMask = true;
        unsigned int stencilMask = 0xffffffff;
        unsigned char isScissorEnabled = false;
        unsigned long clearMask = 0;
        if (attributes.depth) {
            m_context->getFloatv(GraphicsContext3D::DEPTH_CLEAR_VALUE, &clearDepth);
            m_context->clearDepth(1);
            m_context->getBooleanv(GraphicsContext3D::DEPTH_WRITEMASK, &depthMask);
            m_context->depthMask(true);
            clearMask |= GraphicsContext3D::DEPTH_BUFFER_BIT;
        }
        if (attributes.stencil) {
            m_context->getIntegerv(GraphicsContext3D::STENCIL_CLEAR_VALUE, &clearStencil);
            m_context->clearStencil(0);
            m_context->getIntegerv(GraphicsContext3D::STENCIL_WRITEMASK, reinterpret_cast<int*>(&stencilMask));
            m_context->stencilMaskSeparate(GraphicsContext3D::FRONT, 0xffffffff);
            clearMask |= GraphicsContext3D::STENCIL_BUFFER_BIT;
        }
        if (clearMask) {
            isScissorEnabled = m_context->isEnabled(GraphicsContext3D::SCISSOR_TEST);
            m_context->disable(GraphicsContext3D::SCISSOR_TEST);

            m_context->clear(clearMask);

            if (attributes.depth) {
                m_context->clearDepth(clearDepth);
                m_context->depthMask(depthMask);
            }
            if (attributes.stencil) {
                m_context->clearStencil(clearStencil);
                m_context->stencilMaskSeparate(GraphicsContext3D::FRONT, stencilMask);
            }
            if (isScissorEnabled)
                m_context->enable(GraphicsContext3D::SCISSOR_TEST);
            else
                m_context->disable(GraphicsContext3D::SCISSOR_TEST);
        }
    }

    m_context->flush();
    
    didReset();
}

void DrawingBuffer::commit(long x, long y, long width, long height)
{
    if (!m_context)
        return;
        
    if (width < 0)
        width = m_size.width();
    if (height < 0)
        height = m_size.height();
        
    m_context->makeContextCurrent();
    
    if (m_multisampleFBO) {
        m_context->bindFramebuffer(Extensions3D::READ_FRAMEBUFFER, m_multisampleFBO);
        m_context->bindFramebuffer(Extensions3D::DRAW_FRAMEBUFFER, m_fbo);
        m_context->getExtensions()->blitFramebuffer(x, y, width, height, x, y, width, height, GraphicsContext3D::COLOR_BUFFER_BIT, GraphicsContext3D::LINEAR);
    }
    
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
}

void DrawingBuffer::bind()
{
    if (!m_context)
        return;
        
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO ? m_multisampleFBO : m_fbo);
    m_context->viewport(0, 0, m_size.width(), m_size.height());
}

} // namespace WebCore

#endif
