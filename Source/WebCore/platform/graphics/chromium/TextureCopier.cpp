/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "TextureCopier.h"

#include "LayerRendererChromium.h" // For the GLC() macro

namespace WebCore {

#if USE(ACCELERATED_COMPOSITING)
AcceleratedTextureCopier::AcceleratedTextureCopier(PassRefPtr<GraphicsContext3D> context)
    : m_context(context)
{
    GLC(m_context, m_fbo = m_context->createFramebuffer());
    GLC(m_context, m_positionBuffer = m_context->createBuffer());

    static const float kPositions[4][4] = {
        {-1, -1, 0, 1},
        { 1, -1, 0, 1},
        { 1, 1, 0, 1},
        {-1, 1, 0, 1}
    };

    GLC(m_context.get(), m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_positionBuffer));
    GLC(m_context.get(), m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(kPositions), kPositions, GraphicsContext3D::STATIC_DRAW));
    GLC(m_context.get(), m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, 0));

    m_blitProgram = adoptPtr(new BlitProgram(m_context.get()));
}

AcceleratedTextureCopier::~AcceleratedTextureCopier()
{
    if (m_blitProgram)
        m_blitProgram->cleanup(m_context.get());
    if (m_positionBuffer)
        GLC(m_context, m_context->deleteBuffer(m_positionBuffer));
    if (m_fbo)
        GLC(m_context, m_context->deleteFramebuffer(m_fbo));
}

void AcceleratedTextureCopier::copyTexture(GraphicsContext3D* context, unsigned sourceTextureId, unsigned destTextureId, const IntSize& size)
{
    TRACE_EVENT("TextureCopier::copyTexture", this, 0);

    // Note: this code does not restore the viewport, bound program, 2D texture, framebuffer, buffer or blend enable.
    GLC(context, context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo));
    GLC(context, context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, destTextureId, 0));

#if OS(ANDROID)
    // Clear destination to improve performance on tiling GPUs.
    // TODO: Use EXT_discard_framebuffer or skip clearing if it isn't available.
    GLC(context, context->clear(GraphicsContext3D::COLOR_BUFFER_BIT));
#endif

    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, sourceTextureId));
    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::NEAREST));
    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::NEAREST));

    if (!m_blitProgram->initialized())
        m_blitProgram->initialize(context);

    // TODO: Use EXT_framebuffer_blit if available.
    GLC(context, context->useProgram(m_blitProgram->program()));

    const int kPositionAttribute = 0;
    GLC(context, context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_positionBuffer));
    GLC(context, context->vertexAttribPointer(kPositionAttribute, 4, GraphicsContext3D::FLOAT, false, 0, 0));
    GLC(context, context->enableVertexAttribArray(kPositionAttribute));
    GLC(context, context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, 0));

    GLC(context, context->viewport(0, 0, size.width(), size.height()));
    GLC(context, context->disable(GraphicsContext3D::BLEND));
    GLC(context, context->drawArrays(GraphicsContext3D::TRIANGLE_FAN, 0, 4));

    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(context, context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    GLC(context, context->disableVertexAttribArray(kPositionAttribute));

    GLC(context, context->useProgram(0));

    GLC(context, context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, 0, 0));
    GLC(context, context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, 0));
}

}

#endif // USE(ACCELERATED_COMPOSITING)
