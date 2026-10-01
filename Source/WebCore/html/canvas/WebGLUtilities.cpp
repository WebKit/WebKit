/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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
#include "WebGLUtilities.h"

#include "InspectorInstrumentation.h"
#include "WebGLDefaultFramebuffer.h"
#include "WebGLFramebuffer.h"

namespace WebCore {

bool ScopedInspectorShaderProgramHighlight::shouldApply(WebGLRenderingContextBase& context)
{
    RefPtr currentProgram = context.m_currentProgram;
    if (!currentProgram || !InspectorInstrumentation::isWebGLProgramHighlighted(context, *currentProgram)) [[likely]]
        return false;
    if (context.m_framebufferBinding)
        return false;
    return true;
}

void ScopedInspectorShaderProgramHighlight::showHighlight()
{
    Ref gl = *m_context->graphicsContextGL();
    // When OES_draw_buffers_indexed extension is enabled,
    // these state queries return the state for draw buffer 0.
    // Constant blend color is always the same for all draw buffers.
    gl->getFloatv(GraphicsContextGL::BLEND_COLOR, m_savedBlend.color);
    m_savedBlend.equationRGB = gl->getInteger(GraphicsContextGL::BLEND_EQUATION_RGB);
    m_savedBlend.equationAlpha = gl->getInteger(GraphicsContextGL::BLEND_EQUATION_ALPHA);
    m_savedBlend.srcRGB = gl->getInteger(GraphicsContextGL::BLEND_SRC_RGB);
    m_savedBlend.dstRGB = gl->getInteger(GraphicsContextGL::BLEND_DST_RGB);
    m_savedBlend.srcAlpha = gl->getInteger(GraphicsContextGL::BLEND_SRC_ALPHA);
    m_savedBlend.dstAlpha = gl->getInteger(GraphicsContextGL::BLEND_DST_ALPHA);
    m_savedBlend.enabled = gl->isEnabled(GraphicsContextGL::BLEND);

    static const GCGLfloat red = 111.0 / 255.0;
    static const GCGLfloat green = 168.0 / 255.0;
    static const GCGLfloat blue = 220.0 / 255.0;
    static const GCGLfloat alpha = 2.0 / 3.0;
    gl->blendColor(red, green, blue, alpha);

    if (m_context->m_oesDrawBuffersIndexed) {
        gl->enableiOES(GraphicsContextGL::BLEND, 0);
        gl->blendEquationiOES(0, GraphicsContextGL::FUNC_ADD);
        gl->blendFunciOES(0, GraphicsContextGL::CONSTANT_COLOR, GraphicsContextGL::ONE_MINUS_SRC_ALPHA);
    } else {
        gl->enable(GraphicsContextGL::BLEND);
        gl->blendEquation(GraphicsContextGL::FUNC_ADD);
        gl->blendFunc(GraphicsContextGL::CONSTANT_COLOR, GraphicsContextGL::ONE_MINUS_SRC_ALPHA);
    }
}

void ScopedInspectorShaderProgramHighlight::hideHighlight()
{
    Ref gl = *m_context->graphicsContextGL();
    gl->blendColor(m_savedBlend.color[0], m_savedBlend.color[1], m_savedBlend.color[2], m_savedBlend.color[3]);

    if (m_context->m_oesDrawBuffersIndexed) {
        gl->blendEquationSeparateiOES(0, m_savedBlend.equationRGB, m_savedBlend.equationAlpha);
        gl->blendFuncSeparateiOES(0, m_savedBlend.srcRGB, m_savedBlend.dstRGB, m_savedBlend.srcAlpha, m_savedBlend.dstAlpha);
        if (!m_savedBlend.enabled)
            gl->disableiOES(GraphicsContextGL::BLEND, 0);
    } else {
        gl->blendEquationSeparate(m_savedBlend.equationRGB, m_savedBlend.equationAlpha);
        gl->blendFuncSeparate(m_savedBlend.srcRGB, m_savedBlend.dstRGB, m_savedBlend.srcAlpha, m_savedBlend.dstAlpha);
        if (!m_savedBlend.enabled)
            gl->disable(GraphicsContextGL::BLEND);
    }
}

static void setDrawBuffers(WebGLRenderingContextBase& context, std::span<const GCGLenum> buffers)
{
    if (context.isWebGL2())
        protect(context.graphicsContextGL())->drawBuffers(buffers);
    else
        protect(context.graphicsContextGL())->drawBuffersEXT(buffers);
}

ScopedEnableDrawBuffer0::ScopedEnableDrawBuffer0(WebGLRenderingContextBase& context, WebGLDefaultFramebuffer& framebuffer)
{
    if (!framebuffer.drawBufferIsNone())
        return;
    m_context = context;
    m_restoreDrawBuffers = { GraphicsContextGL::NONE };
    setDrawBuffers(context, std::array { GraphicsContextGL::COLOR_ATTACHMENT0 });
}

ScopedEnableDrawBuffer0::ScopedEnableDrawBuffer0(WebGLRenderingContextBase& context, WebGLFramebuffer& framebuffer)
{
    if (framebuffer.getDrawBuffer(GraphicsContextGL::DRAW_BUFFER0_EXT) == GraphicsContextGL::COLOR_ATTACHMENT0)
        return;
    // Draw buffers the application never set are at the GL default, which is handled above, so the
    // tracked draw buffers are known here.
    ASSERT(!framebuffer.getDrawBuffers().isEmpty());
    m_context = context;
    m_restoreDrawBuffers = framebuffer.getDrawBuffers();
    setDrawBuffers(context, std::array { GraphicsContextGL::COLOR_ATTACHMENT0 });
}

ScopedEnableDrawBuffer0::~ScopedEnableDrawBuffer0()
{
    RefPtr context = m_context.get();
    if (!context)
        return;
    setDrawBuffers(*context, m_restoreDrawBuffers.span());
}

ScopedEnableReadBuffer0::ScopedEnableReadBuffer0(WebGLRenderingContextBase& context, WebGLDefaultFramebuffer& framebuffer)
{
    if (!framebuffer.readBufferIsNone())
        return;
    // readBuffer() is WebGL 2 only, and so is the only way the read buffer becomes NONE.
    ASSERT(context.isWebGL2());
    m_context = context;
    m_restoreReadBuffer = GraphicsContextGL::NONE;
    protect(context.graphicsContextGL())->readBuffer(GraphicsContextGL::COLOR_ATTACHMENT0);
}

ScopedEnableReadBuffer0::ScopedEnableReadBuffer0(WebGLRenderingContextBase& context, WebGLFramebuffer& framebuffer)
{
    if (framebuffer.getReadBuffer() == GraphicsContextGL::COLOR_ATTACHMENT0)
        return;
    // readBuffer() is WebGL 2 only, and so is the only way the read buffer differs from the default.
    ASSERT(context.isWebGL2());
    m_context = context;
    m_restoreReadBuffer = framebuffer.getReadBuffer();
    protect(context.graphicsContextGL())->readBuffer(GraphicsContextGL::COLOR_ATTACHMENT0);
}

ScopedEnableReadBuffer0::~ScopedEnableReadBuffer0()
{
    RefPtr context = m_context.get();
    if (!context)
        return;
    protect(context->graphicsContextGL())->readBuffer(m_restoreReadBuffer);
}

}

#endif
