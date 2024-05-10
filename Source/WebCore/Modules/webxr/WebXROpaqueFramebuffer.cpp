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
#include <wtf/SystemTracing.h>

#if PLATFORM(COCOA)
#include "GraphicsContextGLCocoa.h"
#endif

namespace WebCore {

using GL = GraphicsContextGL;

#if PLATFORM(COCOA)

static void ensure(GL& gl, GCGLOwnedFramebuffer& framebuffer)
{
    if (!framebuffer) {
        auto object = gl.createFramebuffer();
        if (!object)
            return;
        framebuffer.adopt(gl, object);
    }
}

static void createAndBindCompositorBuffer(GL& gl, WebXRExternalRenderbuffer& buffer, GCGLenum internalFormat, GL::ExternalImageSource source, GCGLint layer)
{
    if (!buffer.renderBufferObject) {
        auto object = gl.createRenderbuffer();
        if (!object)
            return;
        buffer.renderBufferObject.adopt(gl, object);
    }
    gl.bindRenderbuffer(GL::RENDERBUFFER, buffer.renderBufferObject);
    auto image = gl.createExternalImage(WTFMove(source), internalFormat, layer);
    if (!image)
        return;
    gl.bindExternalImage(GL::RENDERBUFFER, image);
    buffer.image.adopt(gl, image);
}

static GL::ExternalImageSource makeExternalImageSource(std::tuple<WTF::MachSendRight, bool>&& imageSource)
{
    auto [imageHandle, isSharedTexture] = WTFMove(imageSource);
    if (isSharedTexture)
        return GraphicsContextGLExternalImageSourceMTLSharedTextureHandle { WTFMove(imageHandle) };
    return GraphicsContextGLExternalImageSourceIOSurfaceHandle { WTFMove(imageHandle) };
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
    , m_drawFramebuffer(WTFMove(framebuffer))
    , m_context(context)
    , m_attributes(WTFMove(attributes))
    , m_framebufferSize(framebufferSize)
{
}

WebXROpaqueFramebuffer::~WebXROpaqueFramebuffer()
{
    if (RefPtr gl = m_context.graphicsContextGL()) {
#if PLATFORM(COCOA)
        for (auto& layer : m_displayAttachments)
            layer.release(*gl);
#endif
        m_drawAttachments.release(*gl);
        m_resolveAttachments.release(*gl);
        m_resolvedFBO.release(*gl);
        m_context.deleteFramebuffer(m_drawFramebuffer.ptr());
    } else {
        // The GraphicsContextGL is gone, so disarm the GCGLOwned objects so
        // their destructors don't assert.
#if PLATFORM(COCOA)
        for (auto& layer : m_displayAttachments)
            layer.leakObject();
#endif
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

#if PLATFORM(COCOA)
    if (data.layerSetup) {
        // The drawing target can change size at any point during the session. If this happens, we need
        // to recreate the framebuffer.
        if (!setupFramebuffer(*gl, *data.layerSetup))
            return;

        m_completionSyncEvent = MachSendRight(data.layerSetup->completionSyncEvent);
    }

    int layerCount = (m_displayLayout == PlatformXR::Layout::Layered) ? 2 : 1;
    for (int layer = 0; layer < layerCount; ++layer) {
        auto colorTextureSource = makeExternalImageSource(std::tuple<MachSendRight, bool> { data.colorTexture });
        createAndBindCompositorBuffer(*gl, m_displayAttachments[layer].colorBuffer, GL::BGRA_EXT, WTFMove(colorTextureSource), layer);
        ASSERT(m_displayAttachments[layer].colorBuffer.image);
        if (!m_displayAttachments[layer].colorBuffer.image)
            return;

        auto depthStencilBufferSource = makeExternalImageSource(std::tuple<MachSendRight, bool> { data.depthStencilBuffer });
        createAndBindCompositorBuffer(*gl, m_displayAttachments[layer].depthStencilBuffer, GL::DEPTH24_STENCIL8, WTFMove(depthStencilBufferSource), layer);
    }

    m_renderingFrameIndex = data.renderingFrameIndex;

#else
    m_framebufferSize = data.framebufferSize;
    m_colorTexture = data.opaqueTexture;
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

#if PLATFORM(COCOA)
    if (m_completionSyncEvent) {
        auto completionSync = gl->createExternalSync(std::tuple(m_completionSyncEvent, m_renderingFrameIndex));
        ASSERT(completionSync);
        if (completionSync) {
            gl->flush();
            gl->deleteExternalSync(completionSync);
        }
    } else
        gl->finish();

    int layerCount = (m_displayLayout == PlatformXR::Layout::Layered) ? 2 : 1;
    for (int layer = 0; layer < layerCount; ++layer) {
        m_displayAttachments[layer].colorBuffer.destroyImage(*gl);
        m_displayAttachments[layer].depthStencilBuffer.destroyImage(*gl);
    }
#else
    // FIXME: We have to call finish rather than flush because we only want to disconnect
    // the IOSurface and signal the DeviceProxy when we know the content has been rendered.
    // It might be possible to set this up so the completion of the rendering triggers
    // the endFrame call.
    gl->finish();
#endif

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
#if PLATFORM(COCOA)
    ASSERT(!m_resolvedFBO, "blitShared should not require intermediate resolve buffers");

    ensure(gl, m_displayFBO);
    gl.bindFramebuffer(GL::FRAMEBUFFER, m_displayFBO);
    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, m_displayAttachments[0].colorBuffer.renderBufferObject);
    ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
    resolveMSAAFramebuffer(gl);
#else
    gl.bindFramebuffer(GL::FRAMEBUFFER, m_drawFramebuffer->object());
    gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_colorTexture, 0);
    ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
#endif
}

void WebXROpaqueFramebuffer::blitSharedToLayered(GraphicsContextGL& gl)
{
#if PLATFORM(COCOA)
    ensure(gl, m_displayFBO);

    PlatformGLObject readFBO = (m_resolvedFBO && m_attributes.antialias) ? m_resolvedFBO : m_drawFramebuffer->object();
    ASSERT(readFBO, "readFBO shouldn't be the default framebuffer");
    PlatformGLObject drawFBO = m_displayFBO;
    ASSERT(drawFBO, "drawFBO shouldn't be the default framebuffer");

    IntSize phyiscalSize = m_leftPhysicalSize;
    IntRect viewport = m_leftViewport;

    if (m_resolvedFBO && m_attributes.antialias)
        resolveMSAAFramebuffer(gl);

    for (int layer = 0; layer < 2; ++layer) {
        gl.bindFramebuffer(GL::READ_FRAMEBUFFER, readFBO);
        gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, drawFBO);

        GCGLbitfield buffers = GL::COLOR_BUFFER_BIT;
        gl.framebufferRenderbuffer(GL::DRAW_FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, m_displayAttachments[layer].colorBuffer.renderBufferObject);

        if (m_displayAttachments[layer].depthStencilBuffer.image) {
            buffers |= GL::DEPTH_BUFFER_BIT;
            gl.framebufferRenderbuffer(GL::DRAW_FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_displayAttachments[layer].depthStencilBuffer.renderBufferObject);
        }
        ASSERT(gl.checkFramebufferStatus(GL::DRAW_FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);

        int horizontalOffset = viewport.x() / static_cast<double>(m_screenSize.width()) * phyiscalSize.width();
        int verticalOffset = viewport.y() / static_cast<double>(m_screenSize.height()) * phyiscalSize.height();
        int adjustedWidth = viewport.width() / static_cast<double>(m_screenSize.width()) * phyiscalSize.width();
        int adjustedHeight = viewport.height() / static_cast<double>(m_screenSize.height()) * phyiscalSize.height();

        gl.blitFramebuffer(horizontalOffset, verticalOffset, horizontalOffset + adjustedWidth, verticalOffset + adjustedHeight, 0, 0, adjustedWidth, adjustedHeight, buffers, GL::NEAREST);

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=272104 - [WebXR] Compositor expects reverse-Z values
        gl.clearDepth(FLT_MIN);
        gl.clear(GL::DEPTH_BUFFER_BIT | GL::STENCIL_BUFFER_BIT);

        phyiscalSize = m_rightPhysicalSize;
        viewport = m_rightViewport;
    }
#else
    UNUSED_PARAM(gl);
    ASSERT_NOT_REACHED();
#endif
}

bool WebXROpaqueFramebuffer::supportsDynamicViewportScaling() const
{
#if PLATFORM(VISION)
    return false;
#else
    return true;
#endif
}

IntSize WebXROpaqueFramebuffer::displayFramebufferSize() const
{
    return m_framebufferSize;
}

IntSize WebXROpaqueFramebuffer::drawFramebufferSize() const
{
    switch (m_displayLayout) {
    case PlatformXR::Layout::Layered:
        return { 2*m_framebufferSize.width(), m_framebufferSize.height() };
    default:
        return m_framebufferSize;
    }
}

#if PLATFORM(COCOA)
static IntRect convertViewportToPhysicalCoordinates(const IntRect& viewport, const IntSize& screenSize, const IntSize& phyiscalSize)
{
    return IntRect(
        viewport.x() / static_cast<double>(screenSize.width()) * phyiscalSize.width(),
        viewport.y() / static_cast<double>(screenSize.height()) * phyiscalSize.height(),
        viewport.width() / static_cast<double>(screenSize.width()) * phyiscalSize.width(),
        viewport.height() / static_cast<double>(screenSize.height()) * phyiscalSize.height());
}
#endif

IntRect WebXROpaqueFramebuffer::drawViewport(PlatformXR::Eye eye) const
{
#if PLATFORM(COCOA)
    switch (eye) {
    case PlatformXR::Eye::None:
        return IntRect(IntPoint::zero(), drawFramebufferSize());
    case PlatformXR::Eye::Left:
        return m_usingFoveation ? convertViewportToPhysicalCoordinates(m_leftViewport, m_screenSize, m_leftPhysicalSize) : m_leftViewport;
    case PlatformXR::Eye::Right:
        return m_usingFoveation ? convertViewportToPhysicalCoordinates(m_rightViewport, m_screenSize, m_rightPhysicalSize) : m_rightViewport;
    }
#else
    UNUSED_PARAM(eye);
    return IntRect(IntPoint::zero(), drawFramebufferSize());
#endif
}

#if PLATFORM(COCOA)
static PlatformXR::Layout displayLayout(const PlatformXR::FrameData::LayerSetupData& data)
{
    return data.physicalSize[1][0] > 0 ? PlatformXR::Layout::Layered : PlatformXR::Layout::Shared;
}
#endif

static IntSize toIntSize(const auto& size)
{
    return IntSize(size[0], size[1]);
}

#if PLATFORM(COCOA)
bool WebXROpaqueFramebuffer::setupFramebuffer(GraphicsContextGL& gl, const PlatformXR::FrameData::LayerSetupData& data)
{
    auto framebufferSize = IntSize(data.framebufferSize[0], data.framebufferSize[1]);
    bool framebufferResize = m_framebufferSize != framebufferSize || m_displayLayout != displayLayout(data);
    bool foveationChange = !data.horizontalSamples[0].empty() && !data.verticalSamples.empty() && !data.horizontalSamples[1].empty();
    m_usingFoveation = foveationChange;

    m_framebufferSize = framebufferSize;
    m_displayLayout = displayLayout(data);
    m_leftPhysicalSize = toIntSize(data.physicalSize[0]);
    m_rightPhysicalSize = toIntSize(data.physicalSize[1]);
    m_screenSize = data.screenSize;

    const bool layeredLayout = m_displayLayout == PlatformXR::Layout::Layered;
    const bool needsIntermediateResolve = m_attributes.antialias && layeredLayout;

    // Set up recommended samples for WebXR.
    auto sampleCount = m_attributes.antialias ? std::min(4, m_context.maxSamples()) : 0;

    IntSize size = drawFramebufferSize();

    // Drawing target
    if (framebufferResize) {
        // FIXME: We always allocate a new drawing target
        allocateAttachments(gl, m_drawAttachments, sampleCount, size);

        gl.bindFramebuffer(GL::FRAMEBUFFER, m_drawFramebuffer->object());
        bindAttachments(gl, m_drawAttachments);
        ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
    }

    // Calculate viewports of each eye
    if (foveationChange) {
        if (!gl.addFoveation(toIntSize(data.physicalSize[0]), toIntSize(data.physicalSize[1]), data.screenSize, data.horizontalSamples[0], data.verticalSamples, data.horizontalSamples[1]))
            return false;
        gl.enableFoveation(m_drawAttachments.colorBuffer);
    }

    m_leftViewport = calculateViewportShared(PlatformXR::Eye::Left, foveationChange, data.viewports[0], data.viewports[1]);
    m_rightViewport = calculateViewportShared(PlatformXR::Eye::Right, foveationChange, data.viewports[0], data.viewports[1]);
    // Intermediate resolve target
    if ((!m_resolvedFBO || framebufferResize) && needsIntermediateResolve) {
        allocateAttachments(gl, m_resolveAttachments, 0, size);

        ensure(gl, m_resolvedFBO);
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_resolvedFBO);
        bindAttachments(gl, m_resolveAttachments);
        ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
        if (gl.checkFramebufferStatus(GL::FRAMEBUFFER) != GL::FRAMEBUFFER_COMPLETE)
            return false;
    }

    return gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE;
}
#endif

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
#if !PLATFORM(VISION)
    RELEASE_ASSERT(!isFoveated, "Foveated rendering is not supported");
#endif

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

#endif // ENABLE(WEBXR)
