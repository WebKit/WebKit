/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)
#include "WebGLDefaultFramebuffer.h"

#include "IntRect.h"
#include "WebGL2RenderingContext.h"
#include "WebGLFramebuffer.h"
#include "WebGLUtilities.h"
#include <algorithm>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebGLDefaultFramebuffer);

std::unique_ptr<WebGLDefaultFramebuffer> WebGLDefaultFramebuffer::create(WebGLRenderingContextBase& context)
{
    return std::unique_ptr<WebGLDefaultFramebuffer> { new WebGLDefaultFramebuffer(context) };
}

WebGLDefaultFramebuffer::WebGLDefaultFramebuffer(WebGLRenderingContextBase& context)
    : m_context(context)
{
    auto& attributes = context.attributes();
    bool depth = attributes.depth;
    bool stencil = attributes.stencil;
    Ref gl = *context.graphicsContextGL();
    if (depth && stencil) {
        if (gl->supportsExtension(GCGLExtension::OES_packed_depth_stencil)) {
            m_depthStencilFormat = GraphicsContextGL::DEPTH24_STENCIL8;
            m_depthStencilAttachment = GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT;
        } else {
            // Combined buffer not supported, prefer depth when both requested.
            m_depthStencilFormat = gl->supportsExtension(GCGLExtension::OES_depth24)
                ? GraphicsContextGL::DEPTH_COMPONENT24 : GraphicsContextGL::DEPTH_COMPONENT16;
            m_depthStencilAttachment = GraphicsContextGL::DEPTH_ATTACHMENT;
            stencil = false;
            context.m_attributes.stencil = false;
        }
    } else if (stencil) {
        m_depthStencilFormat = GraphicsContextGL::STENCIL_INDEX8;
        m_depthStencilAttachment = GraphicsContextGL::STENCIL_ATTACHMENT;
    } else if (depth) {
        m_depthStencilFormat = gl->supportsExtension(GCGLExtension::OES_depth24)
            ? GraphicsContextGL::DEPTH_COMPONENT24 : GraphicsContextGL::DEPTH_COMPONENT16;
        m_depthStencilAttachment = GraphicsContextGL::DEPTH_ATTACHMENT;
    }

    if (!attributes.preserveDrawingBuffer) {
        m_unpreservedBuffers = GraphicsContextGL::COLOR_BUFFER_BIT;
        if (stencil)
            m_unpreservedBuffers |= GraphicsContextGL::STENCIL_BUFFER_BIT;
        if (depth)
            m_unpreservedBuffers |= GraphicsContextGL::DEPTH_BUFFER_BIT;
    }

    if (attributes.antialias || attributes.preserveDrawingBuffer)
        m_fbo = gl->createFramebuffer();
}

WebGLDefaultFramebuffer::~WebGLDefaultFramebuffer()
{
    Ref context = m_context.get();
    if (context->isContextLost())
        return;
    RefPtr gl = context->graphicsContextGL();
    if (!gl)
        return;
    if (m_depthStencilBuffer)
        gl->deleteRenderbuffer(m_depthStencilBuffer);
    if (m_colorBuffer)
        gl->deleteRenderbuffer(m_colorBuffer);
    if (m_fbo)
        gl->deleteFramebuffer(m_fbo);
}

void WebGLDefaultFramebuffer::resolveColorIntoResult(std::optional<IntRect> rect)
{
    if (!m_fbo) // Aliased renders straight into the result FBO; nothing to resolve.
        return;
    Ref context = m_context.get();
    Ref gl = *context->graphicsContextGL();
    ScopedWebGLRestoreFramebuffer restoreFramebuffer { context };
    ScopedDisableScissorTest scopedScissor { context };
    gl->bindFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, m_fbo);
    gl->bindFramebuffer(GraphicsContextGL::DRAW_FRAMEBUFFER, 0);
    IntRect srcRect = (context->isWebGL2() && rect) ? *rect : IntRect { { }, m_size };
    gl->blitFramebuffer(srcRect.x(), srcRect.y(), srcRect.maxX(), srcRect.maxY(),
        srcRect.x(), srcRect.y(), srcRect.maxX(), srcRect.maxY(),
        GraphicsContextGL::COLOR_BUFFER_BIT, GraphicsContextGL::NEAREST);
}

std::optional<ScopedWebGLRestoreFramebuffer> WebGLDefaultFramebuffer::prepareForReadWhenBound(std::optional<IntRect> rect)
{
    Ref context = m_context.get();
    if (!m_fbo || !context->attributes().antialias)
        return std::nullopt;
    Ref gl = *context->graphicsContextGL();
    ScopedDisableScissorTest scopedScissor { context };
    gl->bindFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, m_fbo);
    gl->bindFramebuffer(GraphicsContextGL::DRAW_FRAMEBUFFER, 0);
    IntRect srcRect = (context->isWebGL2() && rect) ? *rect : IntRect { { }, m_size };
    gl->blitFramebuffer(srcRect.x(), srcRect.y(), srcRect.maxX(), srcRect.maxY(),
        srcRect.x(), srcRect.y(), srcRect.maxX(), srcRect.maxY(),
        GraphicsContextGL::COLOR_BUFFER_BIT, GraphicsContextGL::NEAREST);
    gl->bindFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, 0);
    return ScopedWebGLRestoreFramebuffer { context };
}

void WebGLDefaultFramebuffer::reshape(IntSize size)
{
    m_size = size;
    Ref context = m_context.get();
    Ref gl = *context->graphicsContextGL();
    auto& attributes = context->attributes();

    ScopedWebGLRestoreRenderbuffer restoreRenderbuffer { context };
    ScopedWebGLRestoreFramebuffer restoreFramebuffer { context };

    GCGLsizei sampleCount = attributes.antialias ? std::min(4, context->maxSamples()) : 0;
    GCGLenum colorFormat = attributes.alpha ? GraphicsContextGL::RGBA8 : GraphicsContextGL::RGB8;

    gl->reshape(size.width(), size.height());

    gl->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_fbo);

    if (m_fbo) {
        if (!m_colorBuffer) {
            m_colorBuffer = gl->createRenderbuffer();
            gl->bindRenderbuffer(GraphicsContextGL::RENDERBUFFER, m_colorBuffer);
            gl->framebufferRenderbuffer(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::COLOR_ATTACHMENT0, GraphicsContextGL::RENDERBUFFER, m_colorBuffer);
        } else
            gl->bindRenderbuffer(GraphicsContextGL::RENDERBUFFER, m_colorBuffer);
        if (sampleCount)
            gl->renderbufferStorageMultisampleANGLE(GraphicsContextGL::RENDERBUFFER, sampleCount, colorFormat, size.width(), size.height());
        else
            gl->renderbufferStorage(GraphicsContextGL::RENDERBUFFER, colorFormat, size.width(), size.height());
    }
    if (m_depthStencilFormat) {
        if (!m_depthStencilBuffer) {
            m_depthStencilBuffer = gl->createRenderbuffer();
            gl->bindRenderbuffer(GraphicsContextGL::RENDERBUFFER, m_depthStencilBuffer);
            gl->framebufferRenderbuffer(GraphicsContextGL::FRAMEBUFFER, m_depthStencilAttachment, GraphicsContextGL::RENDERBUFFER, m_depthStencilBuffer);
        } else
            gl->bindRenderbuffer(GraphicsContextGL::RENDERBUFFER, m_depthStencilBuffer);
        if (sampleCount)
            gl->renderbufferStorageMultisampleANGLE(GraphicsContextGL::RENDERBUFFER, sampleCount, m_depthStencilFormat, size.width(), size.height());
        else
            gl->renderbufferStorage(GraphicsContextGL::RENDERBUFFER, m_depthStencilFormat, size.width(), size.height());
    }

    ScopedDisableScissorTest scopedScissor { context };
    ScopedClearColorAndMask scopedColor { context, 0, 0, 0, 0, true, true, true, true };
    ScopedClearDepthAndMask scopedDepth { context, 1.0f, true, hasDepth() };
    ScopedClearStencilAndMask scopedStencil { context, 0, 0xffffffff, hasStencil() };

    GCGLbitfield depthStencilMask = 0;
    if (hasDepth())
        depthStencilMask |= GraphicsContextGL::DEPTH_BUFFER_BIT;
    if (hasStencil())
        depthStencilMask |= GraphicsContextGL::STENCIL_BUFFER_BIT;
    gl->clear(GraphicsContextGL::COLOR_BUFFER_BIT | depthStencilMask);

    // If m_fbo is distinct from GraphicsContextGL FBO 0, clear color of FBO 0.
    if (m_fbo) {
        gl->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, 0);
        gl->clear(GraphicsContextGL::COLOR_BUFFER_BIT);
    }
}

void WebGLDefaultFramebuffer::markBuffersClear(GCGLbitfield clearBuffers)
{
    m_dirtyBuffers &= ~clearBuffers;
}

void WebGLDefaultFramebuffer::markAllUnpreservedBuffersDirty()
{
    m_dirtyBuffers = m_unpreservedBuffers;
}

void WebGLDefaultFramebuffer::markAllBuffersDirty()
{
    m_dirtyBuffers |= GraphicsContextGL::COLOR_BUFFER_BIT;
    if (hasStencil())
        m_dirtyBuffers |= GraphicsContextGL::STENCIL_BUFFER_BIT;
    if (hasDepth())
        m_dirtyBuffers |= GraphicsContextGL::DEPTH_BUFFER_BIT;
}

}

#endif
