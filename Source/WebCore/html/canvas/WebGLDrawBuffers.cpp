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
#include "WebGLDrawBuffers.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLDrawBuffers);

WebGLDrawBuffers::WebGLDrawBuffers(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
{
    context.graphicsContextGL()->ensureExtensionEnabled("GL_EXT_draw_buffers"_s);
}

WebGLDrawBuffers::~WebGLDrawBuffers() = default;

WebGLExtension::ExtensionName WebGLDrawBuffers::getName() const
{
    return WebGLExtension::WebGLDrawBuffersName;
}

bool WebGLDrawBuffers::supported(WebGLRenderingContextBase& context)
{
    return context.graphicsContextGL()->supportsExtension("GL_EXT_draw_buffers"_s);
}

void WebGLDrawBuffers::drawBuffersWEBGL(const Vector<GCGLenum>& buffers)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return;
    GCGLsizei n = buffers.size();
    const GCGLenum* bufs = buffers.data();
    if (!context->m_framebufferBinding) {
        if (n != 1) {
            context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffersWEBGL", "more or fewer than one buffer");
            return;
        }
        if (bufs[0] != GraphicsContextGL::BACK && bufs[0] != GraphicsContextGL::NONE) {
            context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffersWEBGL", "BACK or NONE");
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GCGLenum value[1] { bufs[0] == GraphicsContextGL::BACK ? GraphicsContextGL::COLOR_ATTACHMENT0 : GraphicsContextGL::NONE };
        context->graphicsContextGL()->drawBuffersEXT(value);
        context->setBackDrawBuffer(bufs[0]);
    } else {
        if (n > context->getMaxDrawBuffers()) {
            context->synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "drawBuffersWEBGL", "more than max draw buffers");
            return;
        }
        for (GCGLsizei i = 0; i < n; ++i) {
            if (bufs[i] != GraphicsContextGL::NONE && bufs[i] != GraphicsContextGL::COLOR_ATTACHMENT0_EXT + i) {
                context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffersWEBGL", "COLOR_ATTACHMENTi_EXT or NONE");
                return;
            }
        }
        context->m_framebufferBinding->drawBuffers(buffers);
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
