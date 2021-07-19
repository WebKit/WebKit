/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2021 Apple, Inc. All rights reserved.
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

#if !USE(ANGLE)
#include "ExtensionsGLOpenGLCommon.h"
#endif
#if USE(OPENGL_ES)
#include "ExtensionsGLOpenGLES.h"
#endif
#if !USE(ANGLE)
#include "GraphicsContextGL.h"
#endif
#include "IntSize.h"
#if !USE(ANGLE)
#include "TemporaryOpenGLSetting.h"
#endif
#include "WebGLFramebuffer.h"
#include "WebGLRenderingContext.h"
#if ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"
#endif
#include "WebGLRenderingContextBase.h"
#include <wtf/Scope.h>

#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
#include "ANGLEHeaders.h"
#include "GraphicsContextGLOpenGL.h"
#endif

namespace WebCore {

using GL = GraphicsContextGL;

std::unique_ptr<WebXROpaqueFramebuffer> WebXROpaqueFramebuffer::create(PlatformXR::LayerHandle handle, WebGLRenderingContextBase& context, Attributes&& attributes, uint32_t width, uint32_t height)
{
    auto framebuffer = WebGLFramebuffer::createOpaque(context);
    auto opaque = std::unique_ptr<WebXROpaqueFramebuffer>(new WebXROpaqueFramebuffer(handle, WTFMove(framebuffer), context, WTFMove(attributes), width, height));
    if (!opaque->setupFramebuffer())
        return nullptr;
    return opaque;
}

WebXROpaqueFramebuffer::WebXROpaqueFramebuffer(PlatformXR::LayerHandle handle, Ref<WebGLFramebuffer>&& framebuffer, WebGLRenderingContextBase& context, Attributes&& attributes, uint32_t width, uint32_t height)
    : m_handle(handle)
    , m_framebuffer(WTFMove(framebuffer))
    , m_context(context)
    , m_attributes(WTFMove(attributes))
    , m_width(width)
    , m_height(height)
{
}

WebXROpaqueFramebuffer::~WebXROpaqueFramebuffer()
{
    if (auto gl = m_context.graphicsContextGL()) {
#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
        if (m_opaqueTexture)
            gl->deleteTexture(m_opaqueTexture);
#endif
        if (m_stencilBuffer)
            gl->deleteRenderbuffer(m_stencilBuffer);
        if (m_depthStencilBuffer)
            gl->deleteRenderbuffer(m_depthStencilBuffer);
        if (m_multisampleColorBuffer)
            gl->deleteRenderbuffer(m_multisampleColorBuffer);
        if (m_resolvedFBO)
            gl->deleteFramebuffer(m_resolvedFBO);
        m_context.deleteFramebuffer(m_framebuffer.ptr());
    }
}

void WebXROpaqueFramebuffer::startFrame(const PlatformXR::Device::FrameData::LayerData& data)
{
    if (!m_context.graphicsContextGL())
        return;
    auto& gl = *m_context.graphicsContextGL();

    m_framebuffer->setOpaqueActive(true);

    GCGLint boundFBO { 0 };

    gl.getIntegerv(GL::FRAMEBUFFER_BINDING, makeGCGLSpan(&boundFBO, 1));
    auto scopedFBOs = makeScopeExit([&gl, boundFBO]() {
        gl.bindFramebuffer(GL::FRAMEBUFFER, boundFBO);
    });

    gl.bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_framebuffer->object());
    // https://immersive-web.github.io/webxr/#opaque-framebuffer
    // The buffers attached to an opaque framebuffer MUST be cleared to the values in the provided table when first created,
    // or prior to the processing of each XR animation frame.
    // FIXME: Actually do the clearing (not using invalidateFramebuffer). This will have to be done after we've attached
    // the textures/renderbuffers.

#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
    ASSERT(data.surface);

    auto gCGL = static_cast<GraphicsContextGLOpenGL*>(m_context.graphicsContextGL());
    GCGLenum textureTarget = GraphicsContextGLOpenGL::drawingBufferTextureTarget();


    if (!m_opaqueTexture)
        m_opaqueTexture = gCGL->createTexture();

    gCGL->bindTexture(textureTarget, m_opaqueTexture);
    gCGL->texParameteri(textureTarget, GL::TEXTURE_MAG_FILTER, GL::LINEAR);
    gCGL->texParameteri(textureTarget, GL::TEXTURE_MIN_FILTER, GL::LINEAR);
    gCGL->texParameteri(textureTarget, GL::TEXTURE_WRAP_S, GL::CLAMP_TO_EDGE);
    gCGL->texParameteri(textureTarget, GL::TEXTURE_WRAP_T, GL::CLAMP_TO_EDGE);

    // Tell the GraphicsContextGL to use the IOSurface as the backing store for m_opaqueTexture.
    if (data.isShared) {
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
        m_ioSurfaceTextureHandle = gCGL->attachIOSurfaceToSharedTexture(textureTarget, data.surface.get());
        m_ioSurfaceTextureHandleIsShared = true;
#else
        ASSERT_NOT_REACHED();
#endif
    } else {
        auto size = data.surface->size();
        if (!size.width() || !size.height())
            return;
        m_ioSurfaceTextureHandle = gCGL->createPbufferAndAttachIOSurface(textureTarget, GraphicsContextGLOpenGL::PbufferAttachmentUsage::Write, GL::BGRA, size.width(), size.height(), GL::UNSIGNED_BYTE, data.surface->surface(), 0);
        m_ioSurfaceTextureHandleIsShared = false;
    }

    if (!m_ioSurfaceTextureHandle) {
        gCGL->deleteTexture(m_opaqueTexture);
        m_opaqueTexture = 0;
        return;
    }

    // Now set up the framebuffer to use the texture that points to the IOSurface. The depth and
    // stencil buffers were attached by startFrame.
    gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_opaqueTexture, 0);

    // FIXME: This is assuming multisampling is turned off and we're rendering directly into the framebuffer.

    // At this point the framebuffer should be "complete".
    ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
#else
    m_opaqueTexture = data.opaqueTexture;

#if USE(OPENGL_ES)
    auto& extensions = reinterpret_cast<ExtensionsGLOpenGLES&>(gl.getExtensions());
    if (m_attributes.antialias && extensions.isImagination()) {
        extensions.framebufferTexture2DMultisampleIMG(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_opaqueTexture, 0, m_sampleCount);
        return;
    }
#endif

    if (!m_multisampleColorBuffer)
        gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_opaqueTexture, 0);
#endif
}

void WebXROpaqueFramebuffer::endFrame()
{
    m_framebuffer->setOpaqueActive(false);

    if (!m_context.graphicsContextGL())
        return;

    auto& gl = *m_context.graphicsContextGL();

#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
    // FIXME: We have to call finish rather than flush because we only want to disconnect
    // the IOSurface and signal the DeviceProxy when we know the content has been rendered.
    // It might be possible to set this up so the completion of the rendering triggers
    // the endFrame call.
    gl.finish();

    if (m_ioSurfaceTextureHandle) {
        auto gCGL = static_cast<GraphicsContextGLOpenGL*>(&gl);
        if (m_ioSurfaceTextureHandleIsShared) {
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
            gCGL->detachIOSurfaceFromSharedTexture(m_ioSurfaceTextureHandle);
#else
            ASSERT_NOT_REACHED();
#endif
        } else
            gCGL->destroyPbufferAndDetachIOSurface(m_ioSurfaceTextureHandle);
        m_ioSurfaceTextureHandle = nullptr;
        m_ioSurfaceTextureHandleIsShared = false;
    }
#else

    if (m_multisampleColorBuffer) {
#if !USE(ANGLE)
        // FIXME: These may be needed when using ANGLE, but it didn't compile in the initial implementation.
        TemporaryOpenGLSetting scopedScissor(GL::SCISSOR_TEST, 0);
        TemporaryOpenGLSetting scopedDither(GL::DITHER, 0);
        TemporaryOpenGLSetting scopedDepth(GL::DEPTH_TEST, 0);
        TemporaryOpenGLSetting scopedStencil(GL::STENCIL_TEST, 0);
#endif

        GCGLint boundReadFBO { 0 };
        GCGLint boundDrawFBO { 0 };
        gl.getIntegerv(GL::READ_FRAMEBUFFER_BINDING, makeGCGLSpan(&boundReadFBO, 1));
        gl.getIntegerv(GL::DRAW_FRAMEBUFFER_BINDING, makeGCGLSpan(&boundDrawFBO, 1));

        auto scopedFBOs = makeScopeExit([&gl, boundReadFBO, boundDrawFBO]() {
            gl.bindFramebuffer(GL::READ_FRAMEBUFFER, boundReadFBO);
            gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, boundDrawFBO);
        });

        gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, m_resolvedFBO);
        gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_opaqueTexture, 0);

        // Resolve multisample framebuffer
        gl.bindFramebuffer(GL::READ_FRAMEBUFFER, m_framebuffer->object());
        gl.blitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL::COLOR_BUFFER_BIT, GL::LINEAR);
    }

    gl.flush();
#endif
}

bool WebXROpaqueFramebuffer::setupFramebuffer()
{
    if (!m_context.graphicsContextGL())
        return false;
    auto& gl = *m_context.graphicsContextGL();

    // Bind current FBOs when exiting the function
    GCGLint boundFBO { 0 };
    GCGLint boundRenderbuffer { 0 };
    gl.getIntegerv(GL::FRAMEBUFFER_BINDING, makeGCGLSpan(&boundFBO, 1));
    gl.getIntegerv(GL::RENDERBUFFER_BINDING, makeGCGLSpan(&boundRenderbuffer, 1));
    auto scopedFBO = makeScopeExit([&gl, boundFBO, boundRenderbuffer]() {
        gl.bindFramebuffer(GL::FRAMEBUFFER, boundFBO);
        gl.bindRenderbuffer(GL::RENDERBUFFER, boundRenderbuffer);
    });

    // Set up color, depth and stencil formats
    bool hasDepthOrStencil = m_attributes.stencil || m_attributes.depth;
    auto colorFormat = m_attributes.alpha ? GL::RGBA8 : GL::RGB8;
#if USE(OPENGL_ES)
    auto& extensions = reinterpret_cast<ExtensionsGLOpenGLES&>(gl.getExtensions());
    bool platformSupportsPackedDepthStencil = hasDepthOrStencil && extensions.supports("GL_OES_packed_depth_stencil");
    auto depthFormat = platformSupportsPackedDepthStencil ? GL::DEPTH24_STENCIL8 : GL::DEPTH_COMPONENT16;
    auto stencilFormat = GL::STENCIL_INDEX8;
#elif USE(ANGLE)
    bool platformSupportsPackedDepthStencil = true;
    auto depthFormat = platformSupportsPackedDepthStencil ? GL::DEPTH24_STENCIL8 : GL::DEPTH_COMPONENT;
    auto stencilFormat = GL::STENCIL_INDEX8;
#else
    auto& extensions = reinterpret_cast<ExtensionsGLOpenGLCommon&>(gl.getExtensions());
    bool platformSupportsPackedDepthStencil = hasDepthOrStencil && extensions.supports("GL_EXT_packed_depth_stencil");
    auto depthFormat = platformSupportsPackedDepthStencil ? GL::DEPTH24_STENCIL8 : GL::DEPTH_COMPONENT;
    auto stencilFormat = GL::STENCIL_COMPONENT;
#endif

    // Set up recommended samples for WebXR.
    // FIXME: check if we can get recommended values from each device platform.
    if (m_attributes.antialias) {
        GCGLint maxSampleCount;
#if USE(ANGLE)
        gl.getIntegerv(GL::MAX_SAMPLES, makeGCGLSpan(&maxSampleCount, 1));
#else
        gl.getIntegerv(ExtensionsGL::MAX_SAMPLES, makeGCGLSpan(&maxSampleCount, 1));
#endif
        // Cap the maxiumum multisample count at 4. Any more than this is likely overkill and will impact performance.
        m_sampleCount = std::min(4, maxSampleCount);
    }

#if USE(OPENGL_ES)
    // Use multisampled_render_to_texture extension if available.
    if (m_attributes.antialias && extensions.isImagination()) {
        // framebufferTexture2DMultisampleIMG is set up in startFrame call.
        if (!hasDepthOrStencil)
            return true;
        
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebuffer->object());
        m_depthStencilBuffer = gl.createRenderbuffer();
        if (platformSupportsPackedDepthStencil) {
            gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
            extensions.renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, m_sampleCount, depthFormat, m_width, m_height);
            if (m_attributes.stencil)
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            if (m_attributes.depth)
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
        } else {
            if (m_attributes.stencil) {
                m_stencilBuffer = gl.createRenderbuffer();
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_stencilBuffer);
                extensions.renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, m_sampleCount, stencilFormat, m_width, m_height);
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_stencilBuffer);
            }
            if (m_attributes.depth) {
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
                extensions.renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, m_sampleCount, depthFormat, m_width, m_height);
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            }
        }
        return true;
    }
#endif // USE(OPENGL_ES)

    if (m_attributes.antialias && m_context.isWebGL2()) {
        m_resolvedFBO = gl.createFramebuffer();
        m_multisampleColorBuffer = gl.createRenderbuffer();
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebuffer->object());
        gl.bindRenderbuffer(GL::RENDERBUFFER, m_multisampleColorBuffer);
        gl.renderbufferStorageMultisample(GL::RENDERBUFFER, m_sampleCount, colorFormat, m_width, m_height);
        gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, m_multisampleColorBuffer);
        if (hasDepthOrStencil) {
            m_depthStencilBuffer = gl.createRenderbuffer();
            if (platformSupportsPackedDepthStencil) {
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
                gl.renderbufferStorageMultisample(GL::RENDERBUFFER, m_sampleCount, depthFormat, m_width, m_height);
                if (m_attributes.stencil)
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
                if (m_attributes.depth)
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            } else {
                if (m_attributes.stencil) {
                    m_stencilBuffer = gl.createRenderbuffer();
                    gl.bindRenderbuffer(GL::RENDERBUFFER, m_stencilBuffer);
                    gl.renderbufferStorageMultisample(GL::RENDERBUFFER, m_sampleCount, stencilFormat, m_width, m_height);
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_stencilBuffer);
                }
                if (m_attributes.depth) {
                    gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
                    gl.renderbufferStorageMultisample(GL::RENDERBUFFER, m_sampleCount, depthFormat, m_width, m_height);
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
                }
            }
        }
        return gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE;
    }

    if (hasDepthOrStencil) {
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebuffer->object());
        m_depthStencilBuffer = gl.createRenderbuffer();
        if (platformSupportsPackedDepthStencil) {
            gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
            gl.renderbufferStorage(GL::RENDERBUFFER, depthFormat, m_width, m_height);
            if (m_context.isWebGL2()) {
                if (m_attributes.stencil)
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
                if (m_attributes.depth)
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            } else {
                if (m_attributes.stencil || m_attributes.depth)
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            }
        } else {
            if (m_attributes.stencil) {
                m_stencilBuffer = gl.createRenderbuffer();
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_stencilBuffer);
                gl.renderbufferStorage(GL::RENDERBUFFER, stencilFormat, m_width, m_height);
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_stencilBuffer);
            }
            if (m_attributes.depth) {
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
                gl.renderbufferStorage(GL::RENDERBUFFER, depthFormat, m_width, m_height);
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            }
        }
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
