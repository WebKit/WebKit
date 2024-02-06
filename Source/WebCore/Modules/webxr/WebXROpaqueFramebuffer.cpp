/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2021-2023 Apple, Inc. All rights reserved.
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

#if ENABLE(WEBXR)

#include "IntSize.h"
#include "WebGLFramebuffer.h"
#include "WebGL2RenderingContext.h"
#include "WebGLRenderingContext.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLUtilities.h"
#include <wtf/Scope.h>

#if PLATFORM(COCOA)
#include "GraphicsContextGLCocoa.h"
#endif

namespace WebCore {

using GL = GraphicsContextGL;

#if PLATFORM(COCOA)
static std::optional<GL::EGLImageAttachResult> createAndBindCompositorTexture(GL& gl, GCGLenum target, GCGLOwnedTexture& texture, GL::EGLImageSource source)
{
    texture.ensure(gl);
    gl.bindTexture(target, texture);
    gl.texParameteri(target, GL::TEXTURE_MAG_FILTER, GL::LINEAR);
    gl.texParameteri(target, GL::TEXTURE_MIN_FILTER, GL::LINEAR);
    gl.texParameteri(target, GL::TEXTURE_WRAP_S, GL::CLAMP_TO_EDGE);
    gl.texParameteri(target, GL::TEXTURE_WRAP_T, GL::CLAMP_TO_EDGE);

    auto attachResult = gl.createAndBindEGLImage(target, source);
    if (!attachResult || !std::get<GCEGLImage>(*attachResult) || std::get<IntSize>(*attachResult).isEmpty()) {
        texture.release(gl);
        return std::nullopt;
    }

    return attachResult;
}

static std::optional<GL::EGLImageAttachResult> createAndBindCompositorBuffer(GL& gl, GCGLOwnedRenderbuffer& buffer, GL::EGLImageSource source)
{
    buffer.ensure(gl);
    gl.bindRenderbuffer(GL::RENDERBUFFER, buffer);

    auto attachResult = gl.createAndBindEGLImage(GL::RENDERBUFFER, source);
    if (!attachResult || !std::get<GCEGLImage>(*attachResult) || std::get<IntSize>(*attachResult).isEmpty()) {
        buffer.release(gl);
        return std::nullopt;
    }

    return attachResult;
}

static GL::EGLImageSource makeEGLImageSource(const std::tuple<WTF::MachSendRight, bool>& imageSource)
{
    auto [imageHandle, isSharedTexture] = imageSource;
    if (isSharedTexture)
        return GL::EGLImageSourceMTLSharedTextureHandle { WTF::MachSendRight(imageHandle) };
    return GL::EGLImageSourceIOSurfaceHandle { WTF::MachSendRight(imageHandle) };
}
#endif

std::unique_ptr<WebXROpaqueFramebuffer> WebXROpaqueFramebuffer::create(PlatformXR::LayerHandle handle, WebGLRenderingContextBase& context, Attributes&& attributes, IntSize framebufferSize)
{
    auto framebuffer = WebGLFramebuffer::createOpaque(context);
    if (!framebuffer)
        return nullptr;
    return std::unique_ptr<WebXROpaqueFramebuffer>(new WebXROpaqueFramebuffer(handle, framebuffer.releaseNonNull(), context, WTFMove(attributes), framebufferSize));
}

WebXROpaqueFramebuffer::WebXROpaqueFramebuffer(PlatformXR::LayerHandle handle, Ref<WebGLFramebuffer>&& framebuffer, WebGLRenderingContextBase& context, Attributes&& attributes, IntSize framebufferSize)
    : m_handle(handle)
    , m_framebuffer(WTFMove(framebuffer))
    , m_context(context)
    , m_attributes(WTFMove(attributes))
    , m_framebufferSize(framebufferSize)
{
}

WebXROpaqueFramebuffer::~WebXROpaqueFramebuffer()
{
    if (RefPtr gl = m_context.graphicsContextGL()) {
#if PLATFORM(COCOA)
        m_colorTexture.release(*gl);
#endif
        m_depthStencilBuffer.release(*gl);
        m_multisampleColorBuffer.release(*gl);
        m_resolvedFBO.release(*gl);
        m_context.deleteFramebuffer(m_framebuffer.ptr());
    } else {
        // The GraphicsContextGL is gone, so disarm the GCGLOwned objects so
        // their destructors don't assert.
#if PLATFORM(COCOA)
        m_colorTexture.release(*gl);
#endif
        m_depthStencilBuffer.leakObject();
        m_multisampleColorBuffer.leakObject();
        m_resolvedFBO.leakObject();
    }
}

void WebXROpaqueFramebuffer::startFrame(const PlatformXR::FrameData::LayerData& data)
{
    RefPtr gl = m_context.graphicsContextGL();
    if (!gl)
        return;

    auto [textureTarget, textureTargetBinding] = gl->externalImageTextureBindingPoint();

    ScopedWebGLRestoreFramebuffer restoreFramebuffer { m_context };
    ScopedWebGLRestoreTexture restoreTexture { m_context, textureTarget };
    ScopedWebGLRestoreRenderbuffer restoreRenderBuffer { m_context };

    gl->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_framebuffer->object());
    // https://immersive-web.github.io/webxr/#opaque-framebuffer
    // The buffers attached to an opaque framebuffer MUST be cleared to the values in the provided table when first created,
    // or prior to the processing of each XR animation frame.
    // FIXME: Actually do the clearing (not using invalidateFramebuffer). This will have to be done after we've attached
    // the textures/renderbuffers.

#if PLATFORM(COCOA)
    auto colorTextureSource = makeEGLImageSource(data.colorTexture);
    auto colorTextureAttachment = createAndBindCompositorTexture(*gl, textureTarget, m_colorTexture, colorTextureSource);

    if (!colorTextureAttachment)
        return;

    auto depthStencilBufferSource = makeEGLImageSource(data.depthStencilBuffer);
    auto depthStencilBufferAttachment = createAndBindCompositorBuffer(*gl, m_depthStencilBuffer, depthStencilBufferSource);

    IntSize bufferSize;
    std::tie(m_colorImage, bufferSize) = colorTextureAttachment.value();
    if (depthStencilBufferAttachment)
        std::tie(m_depthStencilImage, std::ignore) = depthStencilBufferAttachment.value();

    // The drawing target can change size at any point during the session. If this happens, we need
    // to recreate the framebuffer.
    if (m_framebufferSize != bufferSize) {
        m_framebufferSize = bufferSize;
        if (!setupFramebuffer())
            return;
    }

    // Set up the framebuffer to use the texture that points to the IOSurface. If we're not multisampling,
    // the target framebuffer is m_framebuffer->object() (bound above). If we are multisampling, the target
    // is the resolved framebuffer we created in setupFramebuffer.
    if (m_multisampleColorBuffer)
        gl->bindFramebuffer(GL::FRAMEBUFFER, m_resolvedFBO);
    gl->framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, textureTarget, m_colorTexture, 0);
    if (m_depthStencilBuffer)
        gl->framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);

    // At this point the framebuffer should be "complete".
    ASSERT(gl->checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);

    m_completionSyncEvent = std::tuple(data.completionSyncEvent);
#else
    m_colorTexture = data.opaqueTexture;
    if (!m_multisampleColorBuffer)
        gl->framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_colorTexture, 0);
#endif

    // WebXR must always clear for the rAF of the session. Currently we assume content does not do redundant initial clear,
    // as the spec says the buffer always starts cleared.
    ScopedDisableRasterizerDiscard disableRasterizerDiscard { m_context };
    ScopedEnableBackbuffer enableBackBuffer { m_context };
    ScopedDisableScissorTest disableScissorTest { m_context };
    ScopedClearColorAndMask zeroClear { m_context, 0.f, 0.f, 0.f, 0.f, true, true, true, true, };
    ScopedClearDepthAndMask zeroDepth { m_context, 1.0f, true, m_attributes.depth };
    ScopedClearStencilAndMask zeroStencil { m_context, 0, GL::FRONT, 0xFFFFFFFF, m_attributes.stencil };
    GCGLenum clearMask = GL::COLOR_BUFFER_BIT;
    if (m_attributes.depth)
        clearMask |= GL::DEPTH_BUFFER_BIT;
    if (m_attributes.stencil)
        clearMask |= GL::STENCIL_BUFFER_BIT;
    gl->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_framebuffer->object());
    gl->clear(clearMask);
}

void WebXROpaqueFramebuffer::endFrame()
{
    RefPtr gl = m_context.graphicsContextGL();
    if (!gl)
        return;

    if (m_multisampleColorBuffer) {
        ScopedWebGLRestoreFramebuffer restoreFramebuffer { m_context };

        GCGLbitfield buffers = GL::COLOR_BUFFER_BIT;
        if (m_depthStencilBuffer)
            buffers |= GL::DEPTH_BUFFER_BIT | GL::STENCIL_BUFFER_BIT;

        gl->bindFramebuffer(GL::READ_FRAMEBUFFER, m_framebuffer->object());
        gl->bindFramebuffer(GL::DRAW_FRAMEBUFFER, m_resolvedFBO);
        gl->blitFramebufferANGLE(0, 0, width(), height(), 0, 0, width(), height(), buffers, GL::NEAREST);
    }

#if PLATFORM(COCOA)
    if (std::get<MachSendRight>(m_completionSyncEvent)) {
        auto completionSync = gl->createEGLSync(m_completionSyncEvent);
        ASSERT(completionSync);
        constexpr uint64_t kTimeout = 1'000'000'000; // 1 second
        gl->clientWaitEGLSyncWithFlush(completionSync, kTimeout);
        gl->destroyEGLSync(completionSync);
    } else
        gl->finish();

    if (m_colorImage) {
        gl->destroyEGLImage(m_colorImage);
        m_colorImage = nullptr;
    }
    if (m_depthStencilImage) {
        gl->destroyEGLImage(m_depthStencilImage);
        m_depthStencilImage = nullptr;
    }
#else
    // FIXME: We have to call finish rather than flush because we only want to disconnect
    // the IOSurface and signal the DeviceProxy when we know the content has been rendered.
    // It might be possible to set this up so the completion of the rendering triggers
    // the endFrame call.
    gl->finish();
#endif
}

bool WebXROpaqueFramebuffer::setupFramebuffer()
{
    RefPtr gl = m_context.graphicsContextGL();
    if (!gl)
        return false;

    // Set up color, depth and stencil formats
    const bool hasDepthOrStencil = m_attributes.stencil || m_attributes.depth;

    // Set up recommended samples for WebXR.
    auto sampleCount = m_attributes.antialias ? std::min(4, m_context.maxSamples()) : 0;

    if (m_attributes.antialias) {
        m_resolvedFBO.ensure(*gl);

        auto colorBuffer = allocateColorStorage(*gl, sampleCount, m_framebufferSize);
        bindColorBuffer(*gl, colorBuffer);
        m_multisampleColorBuffer.adopt(*gl, colorBuffer);

        if (hasDepthOrStencil) {
            auto depthStencilBuffer = allocateDepthStencilStorage(*gl, sampleCount, m_framebufferSize);
            bindDepthStencilBuffer(*gl, depthStencilBuffer);
            m_multisampleDepthStencilBuffer.adopt(*gl, depthStencilBuffer);
        }
    } else if (hasDepthOrStencil && !m_depthStencilBuffer) {
        auto depthStencilBuffer = allocateDepthStencilStorage(*gl, sampleCount, m_framebufferSize);
        bindDepthStencilBuffer(*gl, depthStencilBuffer);

        m_depthStencilBuffer.adopt(*gl, depthStencilBuffer);
    }

    return gl->checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE;
}

PlatformGLObject WebXROpaqueFramebuffer::allocateRenderbufferStorage(GraphicsContextGL& gl, GCGLsizei samples, GCGLenum internalFormat, IntSize size)
{
    PlatformGLObject renderbuffer = gl.createRenderbuffer();
    ASSERT(renderbuffer);
    gl.bindRenderbuffer(GL::RENDERBUFFER, renderbuffer);
    gl.renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, samples, internalFormat, size.width(), size.height());

    return renderbuffer;
}

PlatformGLObject WebXROpaqueFramebuffer::allocateColorStorage(GraphicsContextGL& gl, GCGLsizei samples, IntSize size)
{
    return allocateRenderbufferStorage(gl, samples, GL::SRGB8_ALPHA8, size);
}

PlatformGLObject WebXROpaqueFramebuffer::allocateDepthStencilStorage(GraphicsContextGL& gl, GCGLsizei samples, IntSize size)
{
    return allocateRenderbufferStorage(gl, samples, GL::DEPTH24_STENCIL8, size);
}

void WebXROpaqueFramebuffer::bindColorBuffer(GraphicsContextGL& gl, PlatformGLObject colorBuffer)
{
    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, colorBuffer);
}

void WebXROpaqueFramebuffer::bindDepthStencilBuffer(GraphicsContextGL& gl, PlatformGLObject depthStencilBuffer)
{
    // NOTE: In WebGL2, GL::DEPTH_STENCIL_ATTACHMENT is an alias to set GL::DEPTH_ATTACHMENT and GL::STENCIL_ATTACHMENT, which is all we require.
    ASSERT(m_attributes.stencil || m_attributes.depth);
    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, depthStencilBuffer);
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
