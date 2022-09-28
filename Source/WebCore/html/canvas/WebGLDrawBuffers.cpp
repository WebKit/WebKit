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

// static
bool WebGLDrawBuffers::satisfiesWebGLRequirements(WebGLRenderingContextBase& webglContext)
{
    GraphicsContextGL* context = webglContext.graphicsContextGL();

    // This is called after we make sure GL_EXT_draw_buffers is supported.
    GCGLint maxDrawBuffers = context->getInteger(GraphicsContextGL::MAX_DRAW_BUFFERS_EXT);
    GCGLint maxColorAttachments = context->getInteger(GraphicsContextGL::MAX_COLOR_ATTACHMENTS_EXT);
    if (maxDrawBuffers < 4 || maxColorAttachments < 4)
        return false;

    PlatformGLObject fbo = context->createFramebuffer();
    context->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, fbo);

    const unsigned char buffer[4] = { 0, 0, 0, 0 }; // textures are required to be initialized for other ports.
    bool supportsDepth = context->supportsExtension("GL_OES_depth_texture"_s)
        || context->supportsExtension("GL_ARB_depth_texture"_s);
    bool supportsDepthStencil = (context->supportsExtension("GL_EXT_packed_depth_stencil"_s)
        || context->supportsExtension("GL_OES_packed_depth_stencil"_s));
    PlatformGLObject depthStencil = 0;
    if (supportsDepthStencil) {
        depthStencil = context->createTexture();
        context->bindTexture(GraphicsContextGL::TEXTURE_2D, depthStencil);
        context->texImage2D(GraphicsContextGL::TEXTURE_2D, 0, GraphicsContextGL::DEPTH_STENCIL, 1, 1, 0, GraphicsContextGL::DEPTH_STENCIL, GraphicsContextGL::UNSIGNED_INT_24_8, buffer);
    }
    PlatformGLObject depth = 0;
    if (supportsDepth) {
        depth = context->createTexture();
        context->bindTexture(GraphicsContextGL::TEXTURE_2D, depth);
        context->texImage2D(GraphicsContextGL::TEXTURE_2D, 0, GraphicsContextGL::DEPTH_COMPONENT, 1, 1, 0, GraphicsContextGL::DEPTH_COMPONENT, GraphicsContextGL::UNSIGNED_INT, buffer);
    }

    Vector<PlatformGLObject> colors;
    bool ok = true;
    GCGLint maxAllowedBuffers = std::min(maxDrawBuffers, maxColorAttachments);
    for (GCGLint i = 0; i < maxAllowedBuffers; ++i) {
        PlatformGLObject color = context->createTexture();
        colors.append(color);
        context->bindTexture(GraphicsContextGL::TEXTURE_2D, color);
        context->texImage2D(GraphicsContextGL::TEXTURE_2D, 0, GraphicsContextGL::RGBA, 1, 1, 0, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, buffer);
        context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::COLOR_ATTACHMENT0 + i, GraphicsContextGL::TEXTURE_2D, color, 0);
        if (context->checkFramebufferStatus(GraphicsContextGL::FRAMEBUFFER) != GraphicsContextGL::FRAMEBUFFER_COMPLETE) {
            ok = false;
            break;
        }
        if (supportsDepth) {
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::DEPTH_ATTACHMENT, GraphicsContextGL::TEXTURE_2D, depth, 0);
            if (context->checkFramebufferStatus(GraphicsContextGL::FRAMEBUFFER) != GraphicsContextGL::FRAMEBUFFER_COMPLETE) {
                ok = false;
                break;
            }
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::DEPTH_ATTACHMENT, GraphicsContextGL::TEXTURE_2D, 0, 0);
        }
        if (supportsDepthStencil) {
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::DEPTH_ATTACHMENT, GraphicsContextGL::TEXTURE_2D, depthStencil, 0);
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::STENCIL_ATTACHMENT, GraphicsContextGL::TEXTURE_2D, depthStencil, 0);
            if (context->checkFramebufferStatus(GraphicsContextGL::FRAMEBUFFER) != GraphicsContextGL::FRAMEBUFFER_COMPLETE) {
                ok = false;
                break;
            }
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::DEPTH_ATTACHMENT, GraphicsContextGL::TEXTURE_2D, 0, 0);
            context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::STENCIL_ATTACHMENT, GraphicsContextGL::TEXTURE_2D, 0, 0);
        }
    }

    webglContext.restoreCurrentFramebuffer();
    context->deleteFramebuffer(fbo);
    webglContext.restoreCurrentTexture2D();
    if (supportsDepth)
        context->deleteTexture(depth);
    if (supportsDepthStencil)
        context->deleteTexture(depthStencil);
    for (auto& color : colors)
        context->deleteTexture(color);
    return ok;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
