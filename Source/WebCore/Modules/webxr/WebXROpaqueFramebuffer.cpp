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

std::unique_ptr<WebXROpaqueFramebuffer> WebXROpaqueFramebuffer::create(PlatformXR::LayerHandle handle, WebGLRenderingContextBase& context, Attributes&& attributes, IntSize framebufferSize)
{
    auto framebuffer = WebGLFramebuffer::createOpaque(context);
    auto opaque = std::unique_ptr<WebXROpaqueFramebuffer>(new WebXROpaqueFramebuffer(handle, WTFMove(framebuffer), context, WTFMove(attributes), framebufferSize));
    if (!opaque->setupFramebuffer())
        return nullptr;
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
        m_opaqueTexture.release(*gl);
#endif
        m_stencilBuffer.release(*gl);
        m_depthStencilBuffer.release(*gl);
        m_multisampleColorBuffer.release(*gl);
        m_resolvedFBO.release(*gl);
        m_context.deleteFramebuffer(m_framebuffer.ptr());
    } else {
        // The GraphicsContextGL is gone, so disarm the GCGLOwned objects so
        // their destructors don't assert.
#if USE(IOSURFACE_FOR_XR_LAYER_DATA) || USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
        m_opaqueTexture.leakObject();
#endif
        m_stencilBuffer.leakObject();
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

#if USE(IOSURFACE_FOR_XR_LAYER_DATA) || USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
    IntSize bufferSize;
#endif
    auto [textureTarget, textureTargetBinding] = gl.externalImageTextureBindingPoint();

    m_framebuffer->setOpaqueActive(true);

    GCGLint boundFBO { 0 };
    GCGLint boundTexture { 0 };
    gl.getIntegerv(GL::FRAMEBUFFER_BINDING, std::span(&boundFBO, 1));
    gl.getIntegerv(textureTargetBinding, std::span(&boundTexture, 1));

    auto scopedBindings = makeScopeExit([&gl, boundFBO, boundTexture, textureTarget]() {
        gl.bindFramebuffer(GL::FRAMEBUFFER, boundFBO);
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
    auto colorTextureHandle = data.surface->createSendRight();
    bool colorTextureIsShared = false;
#elif USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
    // Tell the GraphicsContextGL to use the IOSurface as the backing store for m_opaqueTexture.
    auto [colorTextureHandle, colorTextureIsShared] = data.colorTexture;
#endif

#if USE(IOSURFACE_FOR_XR_LAYER_DATA) || USE(MTLTEXTURE_FOR_XR_LAYER_DATA)
    m_opaqueTexture.ensure(gl);
    gl.bindTexture(textureTarget, m_opaqueTexture);
    gl.texParameteri(textureTarget, GL::TEXTURE_MAG_FILTER, GL::LINEAR);
    gl.texParameteri(textureTarget, GL::TEXTURE_MIN_FILTER, GL::LINEAR);
    gl.texParameteri(textureTarget, GL::TEXTURE_WRAP_S, GL::CLAMP_TO_EDGE);
    gl.texParameteri(textureTarget, GL::TEXTURE_WRAP_T, GL::CLAMP_TO_EDGE);

    auto colorTextureSource = (colorTextureIsShared) ? GL::EGLImageSource(GL::EGLImageSourceMTLSharedTextureHandle { colorTextureHandle }) : GL::EGLImageSource(GL::EGLImageSourceIOSurfaceHandle { colorTextureHandle });

    auto colorTextureAttachment = gl.createAndBindEGLImage(textureTarget, colorTextureSource);
    if (!colorTextureAttachment) {
        m_opaqueTexture.release(gl);
        return;
    }

    std::tie(m_opaqueImage, bufferSize) = colorTextureAttachment.value();
    if (bufferSize.isEmpty())
        return;

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
    gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, textureTarget, m_opaqueTexture, 0);

    // At this point the framebuffer should be "complete".
    ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
#else
    m_opaqueTexture = data.opaqueTexture;
    if (!m_multisampleColorBuffer)
        gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_opaqueTexture, 0);
#endif

#if USE(MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION)
    m_completionSyncEvent = data.completionSyncEvent;
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

        gl.bindFramebuffer(GL::READ_FRAMEBUFFER, m_framebuffer->object());
        gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, m_resolvedFBO);
        gl.blitFramebufferANGLE(0, 0, width(), height(), 0, 0, width(), height(), GL::COLOR_BUFFER_BIT, GL::NEAREST);
    }

#if USE(MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION)
    if (std::get<0>(m_completionSyncEvent)) {
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
    if (m_opaqueImage) {
        gl.destroyEGLImage(m_opaqueImage);
        m_opaqueImage = nullptr;
    }
#endif
}

bool WebXROpaqueFramebuffer::setupFramebuffer()
{
    if (!m_context.graphicsContextGL())
        return false;
    auto& gl = *m_context.graphicsContextGL();

    // Restore bindings when exiting the function.
    GCGLint boundFBO { 0 };
    GCGLint boundRenderbuffer { 0 };
    gl.getIntegerv(GL::FRAMEBUFFER_BINDING, std::span(&boundFBO, 1));
    gl.getIntegerv(GL::RENDERBUFFER_BINDING, std::span(&boundRenderbuffer, 1));

    auto scopedBindings = makeScopeExit([&gl, boundFBO, boundRenderbuffer]() {
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
    }

    if (hasDepthOrStencil) {
        auto [depthBuffer, stencilBuffer] = allocateDepthStencilStorage(gl, sampleCount, m_framebufferSize);
        bindDepthStencilBuffer(gl, depthBuffer, stencilBuffer);

        m_depthStencilBuffer.adopt(gl, depthBuffer);
        m_stencilBuffer.adopt(gl, stencilBuffer != depthBuffer ? stencilBuffer : 0);
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
    constexpr auto colorFormat = GL::SRGB8_ALPHA8;
    return allocateRenderbufferStorage(gl, samples, colorFormat, size);
}

std::tuple<PlatformGLObject, PlatformGLObject> WebXROpaqueFramebuffer::allocateDepthStencilStorage(GraphicsContextGL& gl, GCGLsizei samples, IntSize size)
{
    PlatformGLObject depthBuffer = 0;
    PlatformGLObject stencilBuffer = 0;

    // FIXME: Does this need to be optional?
    bool platformSupportsPackedDepthStencil = true;
    if (platformSupportsPackedDepthStencil) {
        depthBuffer = allocateRenderbufferStorage(gl, samples, GL::DEPTH24_STENCIL8, size);
        stencilBuffer = depthBuffer;
    } else {
        if (m_attributes.stencil)
            stencilBuffer = allocateRenderbufferStorage(gl, samples, GL::STENCIL_INDEX8, size);
        if (m_attributes.depth)
            depthBuffer = allocateRenderbufferStorage(gl, samples, GL::DEPTH_COMPONENT, size);
    }

    return std::make_tuple(depthBuffer, stencilBuffer);
}

void WebXROpaqueFramebuffer::bindColorBuffer(GraphicsContextGL& gl, PlatformGLObject colorBuffer)
{
    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, colorBuffer);
}

void WebXROpaqueFramebuffer::bindDepthStencilBuffer(GraphicsContextGL& gl, PlatformGLObject depthBuffer, PlatformGLObject stencilBuffer)
{
    if (depthBuffer == stencilBuffer && !m_context.isWebGL2()) {
        ASSERT(m_attributes.stencil || m_attributes.depth);
        gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, depthBuffer);
    } else {
        if (m_attributes.depth)
            gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, depthBuffer);
        if (m_attributes.stencil)
            gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, stencilBuffer);
    }
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
