/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
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

#if ENABLE(WEBXR) && PLATFORM(COCOA)

#include "GraphicsContextGLCocoa.h"
#include "IntSize.h"
#include "Logging.h"
#include "WebGL2RenderingContext.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderingContext.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLUtilities.h"
#include <wtf/Scope.h>
#include <wtf/SystemTracing.h>

namespace WebCore {

using GL = GraphicsContextGL;

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

static GL::ExternalImageSource makeExternalImageSource(const PlatformXR::FrameData::ExternalTexture& imageSource)
{
    if (imageSource.isSharedTexture)
        return GraphicsContextGLExternalImageSourceMTLSharedTextureHandle { MachSendRight(imageSource.handle) };
    return GraphicsContextGLExternalImageSourceIOSurfaceHandle { MachSendRight(imageSource.handle) };
}

static void createAndBindTempBuffer(GL& gl, WebXRExternalRenderbuffer& buffer, GCGLenum internalFormat, IntSize size)
{
    if (!buffer.renderBufferObject) {
        auto object = gl.createRenderbuffer();
        if (!object)
            return;
        buffer.renderBufferObject.adopt(gl, object);
    }
    gl.bindRenderbuffer(GL::RENDERBUFFER, buffer.renderBufferObject);
    gl.renderbufferStorage(GL::RENDERBUFFER, internalFormat, size.width(), size.height());
}

static IntSize toIntSize(const auto& size)
{
    return IntSize(size[0], size[1]);
}

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
    releaseAllDisplayAttachments();

    if (RefPtr gl = m_context.graphicsContextGL()) {
        m_drawAttachments.release(*gl);
        m_resolveAttachments.release(*gl);
        m_displayFBO.release(*gl);
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

void WebXROpaqueFramebuffer::startFrame(PlatformXR::FrameData::LayerData& data)
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

    if (data.layerSetup) {
        // The drawing target can change size at any point during the session. If this happens, we need
        // to recreate the framebuffer.
        if (!setupFramebuffer(*gl, *data.layerSetup))
            return;

        m_completionSyncEvent = MachSendRight(data.layerSetup->completionSyncEvent);
    }

    bindCompositorTexturesForDisplay(*gl, data);
    auto displayAttachmentSet = reusableDisplayAttachmentsAtIndex(m_currentDisplayAttachmentIndex);
    ASSERT(displayAttachmentSet);
    if (!displayAttachmentSet) {
        RELEASE_LOG_ERROR(XR, "WebXROpaqueFramebuffer::startFrame(): unable to find display attachments at index: %zu", m_currentDisplayAttachmentIndex);
        return;
    }
    if (!(*displayAttachmentSet)[0].colorBuffer.image) {
        RELEASE_LOG_ERROR(XR, "WebXROpaqueFramebuffer::startFrame(): no color texture at index: %zu", m_currentDisplayAttachmentIndex);
        return;
    }

    m_renderingFrameIndex = data.renderingFrameIndex;
    m_blitDepth = data.requestDepth;

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

    if (m_completionSyncEvent) {
        auto completionSync = gl->createExternalSync(std::tuple(m_completionSyncEvent, m_renderingFrameIndex));
        ASSERT(completionSync);
        if (completionSync) {
            gl->flush();
            gl->deleteExternalSync(completionSync);
        }
    } else
        gl->finish();
}

bool WebXROpaqueFramebuffer::usesLayeredMode() const
{
    return m_displayLayout == PlatformXR::Layout::Layered;
}

void WebXROpaqueFramebuffer::releaseAllDisplayAttachments()
{
    for (size_t i = 0; i < m_displayAttachmentsSets.size(); ++i)
        releaseDisplayAttachmentsAtIndex(i);
    m_displayAttachmentsSets.clear();
}

void WebXROpaqueFramebuffer::resolveMSAAFramebuffer(GraphicsContextGL& gl)
{
#if PLATFORM(VISION) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    // End of rendering. Discard the MSAA buffers to avoid writing them back to
    // memory since we only need the resolved versions.
    Vector<GCGLenum, 3> discardAttachments = { GL::COLOR_ATTACHMENT0, GL::DEPTH_ATTACHMENT, GL::STENCIL_ATTACHMENT };
    gl.framebufferDiscard(GL::FRAMEBUFFER, discardAttachments);
#else
    IntSize size = m_framebufferSize; // Physical Space
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
#endif
}

void WebXROpaqueFramebuffer::blitShared(GraphicsContextGL& gl)
{
    ASSERT(!m_resolvedFBO, "blitShared should not require intermediate resolve buffers");

    auto displayAttachmentSet = reusableDisplayAttachmentsAtIndex(m_currentDisplayAttachmentIndex);
    ASSERT(displayAttachmentSet);
    if (!displayAttachmentSet) {
        RELEASE_LOG_ERROR(XR, "WebXROpaqueFramebuffer::blitShared(): unable to find display attachments at index: %zu", m_currentDisplayAttachmentIndex);
        return;
    }

    ensure(gl, m_displayFBO);
    gl.bindFramebuffer(GL::FRAMEBUFFER, m_displayFBO);
    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, (*displayAttachmentSet)[0].colorBuffer.renderBufferObject);
    ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
    resolveMSAAFramebuffer(gl);
}

void WebXROpaqueFramebuffer::blitSharedToLayered(GraphicsContextGL& gl)
{
    ensure(gl, m_displayFBO);

    PlatformGLObject readFBO = (m_resolvedFBO && m_attributes.antialias) ? m_resolvedFBO : m_drawFramebuffer->object();
    ASSERT(readFBO, "readFBO shouldn't be the default framebuffer");
    PlatformGLObject drawFBO = m_displayFBO;
    ASSERT(drawFBO, "drawFBO shouldn't be the default framebuffer");

    auto displayAttachmentSet = reusableDisplayAttachmentsAtIndex(m_currentDisplayAttachmentIndex);
    ASSERT(displayAttachmentSet);
    if (!displayAttachmentSet) {
        RELEASE_LOG_ERROR(XR, "WebXROpaqueFramebuffer::blitSharedToLayered(): unable to find display attachments at index: %zu", m_currentDisplayAttachmentIndex);
        return;
    }

    GCGLint xOffset = 0;
    GCGLint width = m_leftPhysicalSize.width();
    GCGLint height = m_leftPhysicalSize.height();

    if (m_resolvedFBO && m_attributes.antialias)
        resolveMSAAFramebuffer(gl);

    for (int layer = 0; layer < 2; ++layer) {
        gl.bindFramebuffer(GL::READ_FRAMEBUFFER, readFBO);
        gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, drawFBO);

        GCGLbitfield buffers = GL::COLOR_BUFFER_BIT;
        gl.framebufferRenderbuffer(GL::DRAW_FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, (*displayAttachmentSet)[layer].colorBuffer.renderBufferObject);

        if (m_blitDepth && (*displayAttachmentSet)[layer].depthStencilBuffer.image) {
            buffers |= GL::DEPTH_BUFFER_BIT;
            gl.framebufferRenderbuffer(GL::DRAW_FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, (*displayAttachmentSet)[layer].depthStencilBuffer.renderBufferObject);
        }
        ASSERT(gl.checkFramebufferStatus(GL::DRAW_FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);

        gl.blitFramebuffer(xOffset, 0, xOffset + width, height, 0, 0, width, height, buffers, GL::NEAREST);

        xOffset += width;
        width = m_rightPhysicalSize.width();
        height = m_rightPhysicalSize.height();
    }
}

bool WebXROpaqueFramebuffer::supportsDynamicViewportScaling() const
{
#if PLATFORM(VISION)
    return false;
#else
    return true;
#endif
}

IntSize WebXROpaqueFramebuffer::drawFramebufferSize() const
{
    auto framebufferRect = unionRect(m_leftViewport, m_rightViewport);
    RELEASE_ASSERT(framebufferRect.location().isZero());
    // rdar://127893021 - Games exported by Unity set the viewport to the reported size of the framebuffer and
    // adjust the rendering for each eye's viewport in shaders, not with WebGL setViewport/setScissor calls.
    return framebufferRect.size();
}

IntRect WebXROpaqueFramebuffer::drawViewport(PlatformXR::Eye eye) const
{
    switch (eye) {
    case PlatformXR::Eye::None:
        RELEASE_ASSERT(!m_usingFoveation);
        return IntRect(IntPoint::zero(), drawFramebufferSize());
    case PlatformXR::Eye::Left:
        return m_leftViewport;
    case PlatformXR::Eye::Right:
        return m_rightViewport;
    }
}

static PlatformXR::Layout displayLayout(const PlatformXR::FrameData::LayerSetupData& data)
{
    return data.physicalSize[1][0] > 0 ? PlatformXR::Layout::Layered : PlatformXR::Layout::Shared;
}

static IntSize calcFramebufferPhysicalSize(const IntSize& leftPhysicalSize, const IntSize& rightPhysicalSize)
{
    if (rightPhysicalSize.isEmpty())
        return leftPhysicalSize;
    RELEASE_ASSERT(leftPhysicalSize.height() == rightPhysicalSize.height(), "Only side-by-side shared framebuffer layout is supported");
    return { leftPhysicalSize.width() + rightPhysicalSize.width(), leftPhysicalSize.height() };
}

bool WebXROpaqueFramebuffer::setupFramebuffer(GraphicsContextGL& gl, const PlatformXR::FrameData::LayerSetupData& data)
{
    auto leftPhysicalSize = toIntSize(data.physicalSize[0]);
    auto rightPhysicalSize = toIntSize(data.physicalSize[1]);
    auto framebufferSize = calcFramebufferPhysicalSize(leftPhysicalSize, rightPhysicalSize);
    bool framebufferResize = !m_drawAttachments || m_framebufferSize != framebufferSize || m_displayLayout != displayLayout(data);
    bool usingFoveation = !data.foveationRateMapDesc.screenSize.isEmpty();
    bool foveationChange = m_usingFoveation ^ usingFoveation;

    m_displayLayout = displayLayout(data);
    m_framebufferSize = framebufferSize;
    m_leftViewport = data.viewports[0];
    m_leftPhysicalSize = leftPhysicalSize;
    m_rightViewport = data.viewports[1];
    m_rightPhysicalSize = rightPhysicalSize;
    m_usingFoveation = usingFoveation;

    const bool layeredLayout = m_displayLayout == PlatformXR::Layout::Layered;
    const bool needsIntermediateResolve = m_attributes.antialias && layeredLayout;

    // Set up recommended samples for WebXR.
    auto sampleCount = m_attributes.antialias ? std::min(4, m_context.maxSamples()) : 0;

    // Drawing target
    if (framebufferResize) {
        // FIXME: We always allocate a new drawing target
        allocateAttachments(gl, m_drawAttachments, sampleCount, m_framebufferSize);

        gl.bindFramebuffer(GL::FRAMEBUFFER, m_drawFramebuffer->object());
        bindAttachments(gl, m_drawAttachments);
        ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
    }

    // Calculate viewports of each eye
    if (foveationChange) {
        if (m_usingFoveation) {
            const auto& frmd = data.foveationRateMapDesc;
            if (!gl.addFoveation(leftPhysicalSize, rightPhysicalSize, frmd.screenSize, frmd.horizontalSamplesLeft, frmd.verticalSamples, frmd.horizontalSamplesRight))
                return false;
            gl.enableFoveation(m_drawAttachments.colorBuffer);
        } else
            gl.disableFoveation();
    }

    // Intermediate resolve target
    if ((!m_resolvedFBO || framebufferResize) && needsIntermediateResolve) {
        allocateAttachments(gl, m_resolveAttachments, 0, m_framebufferSize);

#if PLATFORM(VISION) && !PLATFORM(IOS_FAMILY_SIMULATOR)
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_drawFramebuffer->object());
        bindResolveAttachments(gl, m_resolveAttachments);
        ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
#endif

        ensure(gl, m_resolvedFBO);
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_resolvedFBO);
        bindAttachments(gl, m_resolveAttachments);
        ASSERT(gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
        if (gl.checkFramebufferStatus(GL::FRAMEBUFFER) != GL::FRAMEBUFFER_COMPLETE)
            return false;
    }

    return gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE;
}

const std::array<WebXRExternalAttachments, 2>* WebXROpaqueFramebuffer::reusableDisplayAttachments(const PlatformXR::FrameData::ExternalTextureData& textureData) const
{
    auto [imageHandle, isSharedTexture] = textureData.colorTexture;
    if (imageHandle)
        return nullptr;

    auto reusableTextureIndex = textureData.reusableTextureIndex;
    if (reusableTextureIndex >= m_displayAttachmentsSets.size() || !m_displayAttachmentsSets[reusableTextureIndex][0]) {
        RELEASE_LOG_FAULT(XR, "Unable to find reusable texture at index: %zu", reusableTextureIndex);
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return &m_displayAttachmentsSets[reusableTextureIndex];
}

void WebXROpaqueFramebuffer::bindCompositorTexturesForDisplay(GraphicsContextGL& gl, const PlatformXR::FrameData::LayerData& layerData)
{
    int layerCount = (m_displayLayout == PlatformXR::Layout::Layered) ? 2 : 1;

    m_currentDisplayAttachmentIndex = layerData.textureData ? layerData.textureData->reusableTextureIndex : 0;
    if (m_displayAttachmentsSets.size() <= m_currentDisplayAttachmentIndex)
        m_displayAttachmentsSets.resizeToFit(m_currentDisplayAttachmentIndex + 1);

    if (!layerData.textureData) {
        m_currentDisplayAttachmentIndex = 0;

        for (int layer = 0; layer < layerCount; ++layer) {
            IntSize framebufferSize = layerData.layerSetup ? toIntSize(layerData.layerSetup->physicalSize[0]) : IntSize(32, 32);
            createAndBindTempBuffer(gl, m_displayAttachmentsSets[m_currentDisplayAttachmentIndex][layer].colorBuffer, GL::RGBA8, framebufferSize);
        }
        return;
    }

    auto displayAttachments = reusableDisplayAttachments(*(layerData.textureData));
    if (displayAttachments)
        return;

    releaseDisplayAttachmentsAtIndex(m_currentDisplayAttachmentIndex);

    for (int layer = 0; layer < layerCount; ++layer) {
        ASSERT(layerData.textureData->colorTexture.handle);
        if (!layerData.textureData->colorTexture.handle)
            return;

        auto colorTextureSource = makeExternalImageSource(layerData.textureData->colorTexture);
        createAndBindCompositorBuffer(gl, m_displayAttachmentsSets[m_currentDisplayAttachmentIndex][layer].colorBuffer, GL::BGRA_EXT, WTFMove(colorTextureSource), layer);
        ASSERT(m_displayAttachmentsSets[m_currentDisplayAttachmentIndex][layer].colorBuffer.image);
        if (!m_displayAttachmentsSets[m_currentDisplayAttachmentIndex][layer].colorBuffer.image)
            return;

        if (layerData.textureData->depthStencilBuffer.handle) {
            auto depthStencilBufferSource = makeExternalImageSource(layerData.textureData->depthStencilBuffer);
            createAndBindCompositorBuffer(gl, m_displayAttachmentsSets[m_currentDisplayAttachmentIndex][layer].depthStencilBuffer, GL::DEPTH24_STENCIL8, WTFMove(depthStencilBufferSource), layer);
        }
    }
}

const std::array<WebXRExternalAttachments, 2>* WebXROpaqueFramebuffer::reusableDisplayAttachmentsAtIndex(size_t index)
{
    if (index >= m_displayAttachmentsSets.size())
        return nullptr;

    return &m_displayAttachmentsSets[index];
}

void WebXROpaqueFramebuffer::releaseDisplayAttachmentsAtIndex(size_t index)
{
    if (index >= m_displayAttachmentsSets.size())
        return;

    RefPtr gl = m_context.graphicsContextGL();
    for (auto& attachments : m_displayAttachmentsSets[index]) {
        if (gl)
            attachments.release(*gl);
        else
            attachments.leakObject();
    }
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


void WebXROpaqueFramebuffer::bindResolveAttachments(GraphicsContextGL& gl, WebXRAttachments& attachments)
{
    gl.framebufferResolveRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, attachments.colorBuffer);
    // NOTE: In WebGL2, GL::DEPTH_STENCIL_ATTACHMENT is an alias to set GL::DEPTH_ATTACHMENT and GL::STENCIL_ATTACHMENT, which is all we require.
    ASSERT((m_attributes.stencil || m_attributes.depth) && attachments.depthStencilBuffer);
    gl.framebufferResolveRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, attachments.depthStencilBuffer);
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

#endif // ENABLE(WEBXR) && PLATFORM(COCOA)
