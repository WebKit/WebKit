/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2021-2024 Apple, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebXROpaqueFramebuffer.h"

#if ENABLE(WEBXR) && !PLATFORM(COCOA)

#include "IntSize.h"
#include "WebGLFramebuffer.h"
#include "WebGL2RenderingContext.h"
#include "WebGLRenderingContext.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLUtilities.h"
#include <wtf/Scope.h>
#include <wtf/SystemTracing.h>

namespace WebCore {

using GL = GraphicsContextGL;

std::unique_ptr<WebXROpaqueFramebuffer> WebXROpaqueFramebuffer::create(PlatformXR::LayerHandle handle, WebGLRenderingContextBase& context, Attributes&& attributes, IntSize framebufferSize)
{
    auto framebuffer = WebGLFramebuffer::createOpaque(context);
    if (!framebuffer)
        return nullptr;
    return std::unique_ptr<WebXROpaqueFramebuffer>(new WebXROpaqueFramebuffer(handle, framebuffer.releaseNonNull(), context, WTFMove(attributes), framebufferSize));
}

WebXROpaqueFramebuffer::WebXROpaqueFramebuffer(PlatformXR::LayerHandle handle, Ref<WebGLFramebuffer>&& framebuffer, WebGLRenderingContextBase& context, Attributes&& attributes, IntSize framebufferSize)
    : m_handle(handle)
    , m_drawFramebuffer(WTFMove(framebuffer))
    , m_context(context)
    , m_attributes(WTFMove(attributes))
    , m_framebufferSize(framebufferSize)
{
}

WebXROpaqueFramebuffer::~WebXROpaqueFramebuffer()
{
    if (RefPtr gl = m_context.graphicsContextGL()) {
        m_drawAttachments.release(*gl);
        m_resolveAttachments.release(*gl);
        m_resolvedFBO.release(*gl);
        m_context.deleteFramebuffer(m_drawFramebuffer.ptr());
    } else {
        // The GraphicsContextGL is gone, so disarm the GCGLOwned objects so
        // their destructors don't assert.
        m_drawAttachments.leakObject();
        m_resolveAttachments.leakObject();
        m_displayFBO.leakObject();
        m_resolvedFBO.leakObject();
    }
}

void WebXROpaqueFramebuffer::startFrame(const PlatformXR::FrameData::LayerData& data)
{
    RefPtr gl = m_context.graphicsContextGL();
    if (!gl)
        return;

    tracePoint(WebXRLayerStartFrameStart);
    auto scopeExit = makeScopeExit([&]() {
        tracePoint(WebXRLayerStartFrameEnd);
    });

    auto [textureTarget, textureTargetBinding] = gl->externalImageTextureBindingPoint();

    ScopedWebGLRestoreFramebuffer restoreFramebuffer { m_context };
    ScopedWebGLRestoreTexture restoreTexture { m_context, textureTarget };
    ScopedWebGLRestoreRenderbuffer restoreRenderBuffer { m_context };

    gl->bindFramebuffer(GL::FRAMEBUFFER, m_drawFramebuffer->object());
    // https://immersive-web.github.io/webxr/#opaque-framebuffer
    // The buffers attached to an opaque framebuffer MUST be cleared to the values in the provided table when first created,
    // or prior to the processing of each XR animation frame.
    // FIXME: Actually do the clearing (not using invalidateFramebuffer). This will have to be done after we've attached
    // the textures/renderbuffers.

    m_framebufferSize = data.framebufferSize;
    m_colorTexture = data.opaqueTexture;

    // WebXR must always clear for the rAF of the session. Currently we assume content does not do redundant initial clear,
    // as the spec says the buffer always starts cleared.
    ScopedDisableRasterizerDiscard disableRasterizerDiscard { m_context };
    ScopedEnableBackbuffer enableBackBuffer { m_context };
    ScopedDisableScissorTest disableScissorTest { m_context };
    ScopedClearColorAndMask zeroClear { m_context, 0.f, 0.f, 0.f, 0.f, true, true, true, true, };
    ScopedClearDepthAndMask zeroDepth { m_context, 1.0f, true, m_attributes.depth };
    ScopedClearStencilAndMask zeroStencil { m_context, 0, 0xFFFFFFFF, m_attributes.stencil };
    GCGLenum clearMask = GL::COLOR_BUFFER_BIT;
    if (m_attributes.depth)
        clearMask |= GL::DEPTH_BUFFER_BIT;
    if (m_attributes.stencil)
        clearMask |= GL::STENCIL_BUFFER_BIT;
    gl->bindFramebuffer(GL::FRAMEBUFFER, m_drawFramebuffer->object());
    gl->clear(clearMask);
}

void WebXROpaqueFramebuffer::endFrame()
{
    RefPtr gl = m_context.graphicsContextGL();
    if (!gl)
        return;

    tracePoint(WebXRLayerEndFrameStart);
    gl->disableFoveation();

    auto scopeExit = makeScopeExit([&]() {
        tracePoint(WebXRLayerEndFrameEnd);
    });

    ScopedWebGLRestoreFramebuffer restoreFramebuffer { m_context };
    switch (m_displayLayout) {
    case PlatformXR::Layout::Shared:
        blitShared(*gl);
        break;
    case PlatformXR::Layout::Layered:
        blitSharedToLayered(*gl);
        break;
    }

    // FIXME: We have to call finish rather than flush because we only want to disconnect
    // the IOSurface and signal the DeviceProxy when we know the content has been rendered.
    // It might be possible to set this up so the completion of the rendering triggers
    // the endFrame call.
    gl->finish();
}

bool WebXROpaqueFramebuffer::usesLayeredMode() const
{
    return m_displayLayout == PlatformXR::Layout::Layered;
}

void WebXROpaqueFramebuffer::resolveMSAAFramebuffer(GraphicsContextGL& gl)
{
    IntSize size = drawFramebufferSize();
    PlatformGLObject readFBO = m_drawFramebuffer->object();
    PlatformGLObject drawFBO = m_resolvedFBO ? m_resolvedFBO : m_displayFBO;

    GCGLbitfield buffers = GL::COLOR_BUFFER_BIT;
    if (m_drawAttachments.depthStencilBuffer) {
        // FIXME: Is it necessary to resolve stencil?
        buffers |= GL::DEPTH_BUFFER_BIT | GL::STENCIL_BUFFER_BIT;
    }

    gl.bindFramebuffer(GL::READ_FRAMEBUFFER, readFBO);
    ASSERT(gl.checkFramebufferStatus(GL::READ_FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
    gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, drawFBO);
    ASSERT(gl.checkFramebufferStatus(GL::DRAW_FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
    gl.blitFramebuffer(0, 0, size.width(), size.height(), 0, 0, size.width(), size.height(), buffers, GL::NEAREST);
}

void WebXROpaqueFramebuffer::blitShared(GraphicsContextGL& gl)
{
    gl.bindFramebuffer(GL::FRAMEBUFFER, m_drawFramebuffer->object());
    gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_colorTexture, 0);
    ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
}

void WebXROpaqueFramebuffer::blitSharedToLayered(GraphicsContextGL& gl)
{
    UNUSED_PARAM(gl);
    ASSERT_NOT_REACHED();
}

bool WebXROpaqueFramebuffer::supportsDynamicViewportScaling() const
{
    return true;
}

IntSize WebXROpaqueFramebuffer::drawFramebufferSize() const
{
    return m_framebufferSize;
}

IntRect WebXROpaqueFramebuffer::drawViewport(PlatformXR::Eye eye) const
{
    UNUSED_PARAM(eye);
    return IntRect(IntPoint::zero(), drawFramebufferSize());
}

static IntSize toIntSize(const auto& size)
{
    return IntSize(size[0], size[1]);
}

void WebXROpaqueFramebuffer::allocateRenderbufferStorage(GraphicsContextGL& gl, GCGLOwnedRenderbuffer& buffer, GCGLsizei samples, GCGLenum internalFormat, IntSize size)
{
    PlatformGLObject renderbuffer = gl.createRenderbuffer();
    ASSERT(renderbuffer);
    gl.bindRenderbuffer(GL::RENDERBUFFER, renderbuffer);
    gl.renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, samples, internalFormat, size.width(), size.height());
    buffer.adopt(gl, renderbuffer);
}

void WebXROpaqueFramebuffer::allocateAttachments(GraphicsContextGL& gl, WebXRAttachments& attachments, GCGLsizei samples, IntSize size)
{
    const bool hasDepthOrStencil = m_attributes.stencil || m_attributes.depth;
    allocateRenderbufferStorage(gl, attachments.colorBuffer, samples, GL::RGBA8, size);
    if (hasDepthOrStencil)
        allocateRenderbufferStorage(gl, attachments.depthStencilBuffer, samples, GL::DEPTH24_STENCIL8, size);
}

void WebXROpaqueFramebuffer::bindAttachments(GraphicsContextGL& gl, WebXRAttachments& attachments)
{
    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, attachments.colorBuffer);
    // NOTE: In WebGL2, GL::DEPTH_STENCIL_ATTACHMENT is an alias to set GL::DEPTH_ATTACHMENT and GL::STENCIL_ATTACHMENT, which is all we require.
    ASSERT((m_attributes.stencil || m_attributes.depth) && attachments.depthStencilBuffer);
    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, attachments.depthStencilBuffer);
}

IntRect WebXROpaqueFramebuffer::calculateViewportShared(PlatformXR::Eye eye, bool isFoveated, const IntRect& leftViewport, const IntRect& rightViewport)
{
    switch (eye) {
    case PlatformXR::Eye::None:
        ASSERT_NOT_REACHED();
        return IntRect();
    case PlatformXR::Eye::Left:
        return isFoveated ? leftViewport : IntRect(0, 0, m_framebufferSize.width(), m_framebufferSize.height());
    case PlatformXR::Eye::Right:
        return isFoveated ? IntRect(leftViewport.width() + rightViewport.x(), rightViewport.y(), rightViewport.width(), rightViewport.height()) : IntRect(m_framebufferSize.width(), 0, m_framebufferSize.width(), m_framebufferSize.height());
    }

    return IntRect();
}

void WebXRExternalRenderbuffer::destroyImage(GraphicsContextGL& gl)
{
    image.release(gl);
}

void WebXRExternalRenderbuffer::release(GraphicsContextGL& gl)
{
    renderBufferObject.release(gl);
    image.release(gl);
}

void WebXRExternalRenderbuffer::leakObject()
{
    renderBufferObject.leakObject();
    image.leakObject();
}

} // namespace WebCore

#endif // ENABLE(WEBXR) && !PLATFORM(COCOA)
