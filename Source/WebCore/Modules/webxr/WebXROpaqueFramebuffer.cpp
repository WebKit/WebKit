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
#include <wtf/Scope.h>

#if USE(IOSURFACE_FOR_XR_LAYER_DATA) || USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
#include "GraphicsContextGLCocoa.h"
#endif

namespace WebCore {

using GL = GraphicsContextGL;

#if USE(IOSURFACE_FOR_XR_LAYER_DATA) || USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
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
    auto opaque = std::unique_ptr<WebXROpaqueFramebuffer>(new WebXROpaqueFramebuffer(handle, WTFMove(framebuffer), context, WTFMove(attributes), framebufferSize));
    return opaque;
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
    if (auto gl = m_context.graphicsContextGL()) {
#if USE(IOSURFACE_FOR_XR_LAYER_DATA) || USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
        m_colorTexture.release(*gl);
#endif
        m_depthStencilBuffer.release(*gl);
        m_multisampleColorBuffer.release(*gl);
        m_resolvedFBO.release(*gl);
        m_context.deleteFramebuffer(m_framebuffer.ptr());
    } else {
        // The GraphicsContextGL is gone, so disarm the GCGLOwned objects so
        // their destructors don't assert.
#if USE(IOSURFACE_FOR_XR_LAYER_DATA) || USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
        m_colorTexture.release(*gl);
#endif
        m_depthStencilBuffer.leakObject();
        m_multisampleColorBuffer.leakObject();
        m_resolvedFBO.leakObject();
    }
}

void WebXROpaqueFramebuffer::startFrame(const PlatformXR::Device::FrameData::LayerData& data)
{
    if (!m_context.graphicsContextGL())
        return;
    auto& gl = *m_context.graphicsContextGL();

    auto [textureTarget, textureTargetBinding] = gl.externalImageTextureBindingPoint();

    m_framebuffer->setOpaqueActive(true);

    GCGLint boundFBO = gl.getInteger(GL::FRAMEBUFFER_BINDING);
    GCGLint boundRenderbuffer = gl.getInteger(GL::RENDERBUFFER_BINDING);
    GCGLint boundTexture = gl.getInteger(textureTargetBinding);

    auto scopedBindings = makeScopeExit([=, textureTarget = textureTarget, &gl]() {
        gl.bindFramebuffer(GL::FRAMEBUFFER, boundFBO);
        gl.bindRenderbuffer(GL::RENDERBUFFER, boundRenderbuffer);
        gl.bindTexture(textureTarget, boundTexture);
    });

    gl.bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_framebuffer->object());
    // https://immersive-web.github.io/webxr/#opaque-framebuffer
    // The buffers attached to an opaque framebuffer MUST be cleared to the values in the provided table when first created,
    // or prior to the processing of each XR animation frame.
    // FIXME: Actually do the clearing (not using invalidateFramebuffer). This will have to be done after we've attached
    // the textures/renderbuffers.

#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
    // FIXME: This is temporary until Cocoa-specific platforms migrate to MTLTEXTURE_FOR_XR_LAYER_DATA.
    auto colorTextureSource = makeEGLImageSource({ data.surface->createSendRight(), false });
    auto depthStencilBufferSource = makeEGLImageSource({ MachSendRight(), false });
#elif USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
    auto colorTextureSource = makeEGLImageSource(data.colorTexture);
    auto depthStencilBufferSource = makeEGLImageSource(data.depthStencilBuffer);
#endif

#if USE(IOSURFACE_FOR_XR_LAYER_DATA) || USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
    auto colorTextureAttachment = createAndBindCompositorTexture(gl, textureTarget, m_colorTexture, colorTextureSource);
    auto depthStencilBufferAttachment = createAndBindCompositorBuffer(gl, m_depthStencilBuffer, depthStencilBufferSource);

    if (!colorTextureAttachment)
        return;

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
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_resolvedFBO);
    gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, textureTarget, m_colorTexture, 0);
    if (m_depthStencilBuffer)
        gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);

    // At this point the framebuffer should be "complete".
    ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
#else
    m_colorTexture = data.opaqueTexture;
    if (!m_multisampleColorBuffer)
        gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_colorTexture, 0);
#endif

#if USE(MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION)
    m_completionSyncEvent = std::tuple(data.completionSyncEvent);
#endif
}

void WebXROpaqueFramebuffer::endFrame()
{
    m_framebuffer->setOpaqueActive(false);

    if (!m_context.graphicsContextGL())
        return;

    auto& gl = *m_context.graphicsContextGL();

    if (m_multisampleColorBuffer) {
        // FIXME: These may be needed when using ANGLE, but it didn't compile in the initial implementation.
        // https://bugs.webkit.org/show_bug.cgi?id=245210
        // TemporaryOpenGLSetting scopedScissor(GL::SCISSOR_TEST, 0);
        // TemporaryOpenGLSetting scopedDither(GL::DITHER, 0);
        // TemporaryOpenGLSetting scopedDepth(GL::DEPTH_TEST, 0);
        // TemporaryOpenGLSetting scopedStencil(GL::STENCIL_TEST, 0);

        GCGLint boundFBO { 0 };
        GCGLint boundReadFBO { 0 };
        GCGLint boundDrawFBO { 0 };
        gl.getIntegerv(GL::FRAMEBUFFER_BINDING, std::span(&boundFBO, 1));
        gl.getIntegerv(GL::READ_FRAMEBUFFER_BINDING, std::span(&boundReadFBO, 1));
        gl.getIntegerv(GL::DRAW_FRAMEBUFFER_BINDING, std::span(&boundDrawFBO, 1));

        auto scopedBindings = makeScopeExit([&gl, boundFBO, boundReadFBO, boundDrawFBO]() {
            gl.bindFramebuffer(GL::FRAMEBUFFER, boundFBO);
            gl.bindFramebuffer(GL::READ_FRAMEBUFFER, boundReadFBO);
            gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, boundDrawFBO);
        });

        GCGLbitfield buffers = GL::COLOR_BUFFER_BIT;
        if (m_depthStencilBuffer)
            buffers |= GL::DEPTH_BUFFER_BIT | GL::STENCIL_BUFFER_BIT;

        gl.bindFramebuffer(GL::READ_FRAMEBUFFER, m_framebuffer->object());
        gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, m_resolvedFBO);
        gl.blitFramebufferANGLE(0, 0, width(), height(), 0, 0, width(), height(), buffers, GL::NEAREST);
    }

#if USE(MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION)
    if (std::get<MachSendRight>(m_completionSyncEvent)) {
        auto completionSync = gl.createEGLSync(m_completionSyncEvent);
        ASSERT(completionSync);
        constexpr uint64_t kTimeout = 1'000'000'000; // 1 second
        gl.clientWaitEGLSyncWithFlush(completionSync, kTimeout);
        gl.destroyEGLSync(completionSync);
    } else
        gl.finish();
#else
    // FIXME: We have to call finish rather than flush because we only want to disconnect
    // the IOSurface and signal the DeviceProxy when we know the content has been rendered.
    // It might be possible to set this up so the completion of the rendering triggers
    // the endFrame call.
    gl.finish();
#endif

#if USE(IOSURFACE_FOR_XR_LAYER_DATA) || USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
    if (m_colorImage) {
        gl.destroyEGLImage(m_colorImage);
        m_colorImage = nullptr;
    }
    if (m_depthStencilImage) {
        gl.destroyEGLImage(m_depthStencilImage);
        m_depthStencilImage = nullptr;
    }
#endif
}

bool WebXROpaqueFramebuffer::setupFramebuffer()
{
    if (!m_context.graphicsContextGL())
        return false;
    auto& gl = *m_context.graphicsContextGL();

    // Restore bindings when exiting the function.
    GCGLint boundFBO = gl.getInteger(GL::FRAMEBUFFER_BINDING);
    GCGLint boundRenderbuffer = gl.getInteger(GL::RENDERBUFFER_BINDING);

    auto scopedBindings = makeScopeExit([=, &gl]() {
        gl.bindFramebuffer(GL::FRAMEBUFFER, boundFBO);
        gl.bindRenderbuffer(GL::RENDERBUFFER, boundRenderbuffer);
    });

    // Set up color, depth and stencil formats
    const bool hasDepthOrStencil = m_attributes.stencil || m_attributes.depth;

    // Set up recommended samples for WebXR.
    auto sampleCount = [](GraphicsContextGL& gl, bool isAntialias) {
        if (!isAntialias)
            return 0;

        // FIXME: check if we can get recommended values from each device platform.
        GCGLint maxSampleCount;
        gl.getIntegerv(GL::MAX_SAMPLES, std::span(&maxSampleCount, 1));
        // Cap the maximum multisample count at 4. Any more than this is likely overkill and will impact performance.
        return std::min(4, maxSampleCount);
    }(gl, m_attributes.antialias);

    gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebuffer->object());

    if (m_attributes.antialias) {
        m_resolvedFBO.ensure(gl);

        auto colorBuffer = allocateColorStorage(gl, sampleCount, m_framebufferSize);
        bindColorBuffer(gl, colorBuffer);
        m_multisampleColorBuffer.adopt(gl, colorBuffer);

        if (hasDepthOrStencil) {
            auto depthStencilBuffer = allocateDepthStencilStorage(gl, sampleCount, m_framebufferSize);
            bindDepthStencilBuffer(gl, depthStencilBuffer);
            m_multisampleDepthStencilBuffer.adopt(gl, depthStencilBuffer);
        }
    } else if (hasDepthOrStencil && !m_depthStencilBuffer) {
        auto depthStencilBuffer = allocateDepthStencilStorage(gl, sampleCount, m_framebufferSize);
        bindDepthStencilBuffer(gl, depthStencilBuffer);

        m_depthStencilBuffer.adopt(gl, depthStencilBuffer);
    }

    return gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE;
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
