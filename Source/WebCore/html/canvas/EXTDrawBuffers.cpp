/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)

#include "EXTDrawBuffers.h"

#include "Extensions3D.h"

namespace WebCore {

EXTDrawBuffers::EXTDrawBuffers(WebGLRenderingContext* context)
    : WebGLExtension(context)
{
}

EXTDrawBuffers::~EXTDrawBuffers()
{
}

WebGLExtension::ExtensionName EXTDrawBuffers::getName() const
{
    return WebGLExtension::EXTDrawBuffersName;
}

PassOwnPtr<EXTDrawBuffers> EXTDrawBuffers::create(WebGLRenderingContext* context)
{
    return adoptPtr(new EXTDrawBuffers(context));
}

// static
bool EXTDrawBuffers::supported(WebGLRenderingContext* context)
{
    Extensions3D* extensions = context->graphicsContext3D()->getExtensions();
    return extensions->supports("GL_EXT_draw_buffers");
}

void EXTDrawBuffers::drawBuffersEXT(const Vector<GC3Denum>& buffers)
{
    if (m_context->isContextLost())
        return;
    GC3Dsizei n = buffers.size();
    const GC3Denum* bufs = buffers.data();
    if (!m_context->m_framebufferBinding) {
        if (n != 1) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "drawBuffersEXT", "more than one buffer");
            return;
        }
        if (bufs[0] != GraphicsContext3D::BACK && bufs[0] != GraphicsContext3D::NONE) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "drawBuffersEXT", "BACK or NONE");
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GC3Denum value = (bufs[0] == GraphicsContext3D::BACK) ? GraphicsContext3D::COLOR_ATTACHMENT0 : GraphicsContext3D::NONE;
        m_context->graphicsContext3D()->getExtensions()->drawBuffersEXT(1, &value);
    } else {
        if (n > m_context->getMaxDrawBuffers()) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "drawBuffersEXT", "more than max draw buffers");
            return;
        }
        for (GC3Dsizei i = 0; i < n; ++i) {
            if (bufs[i] != GraphicsContext3D::NONE && bufs[i] != static_cast<GC3Denum>(Extensions3D::COLOR_ATTACHMENT0_EXT + i)) {
                m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "drawBuffersEXT", "COLOR_ATTACHMENTi_EXT or NONE");
                return;
            }
        }
        m_context->m_framebufferBinding->drawBuffers(buffers);
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
