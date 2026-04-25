/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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
#include "WebXRWebGLSwapchain.h"

#if ENABLE(WEBXR_LAYERS)

#include "IntSize.h"
#include "Logging.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLUtilities.h"
#include <wtf/Scope.h>
#include <wtf/SystemTracing.h>

namespace WebCore {

using GL = GraphicsContextGL;

WebXRWebGLSwapchain::~WebXRWebGLSwapchain() = default;

WebXRWebGLSwapchain::WebXRWebGLSwapchain(WebGLRenderingContextBase& context, SwapchainTargets targets, bool clearOnAccess, size_t imageCount)
    : WebXRSwapchain(targets, clearOnAccess)
    , m_context(context)
    , m_imageCount(imageCount)
{
    if (clearOnAccess)
        m_framebufferForClearing = m_context->createFramebuffer();
}

void WebXRWebGLSwapchain::clearCurrentTexture(GraphicsContextGL& gl)
{
    ASSERT(m_context);

    if (!m_framebufferForClearing)
        return;

    auto texture = currentTexture();
    if (!texture)
        return;

    auto computeClearMask = [](const SwapchainTargets& targets) {
        GCGLenum clearMask = 0;
        if (targets.contains(SwapchainTargetFlags::Color))
            clearMask |= GL::COLOR_BUFFER_BIT;
        if (targets.contains(SwapchainTargetFlags::Depth))
            clearMask |= GL::DEPTH_BUFFER_BIT;
        if (targets.contains(SwapchainTargetFlags::Stencil))
            clearMask |= GL::STENCIL_BUFFER_BIT;
        return clearMask;
    };

    auto computeAttachment = [](const SwapchainTargets& target) {
        if (target.contains(SwapchainTargetFlags::Color))
            return GL::COLOR_ATTACHMENT0;
        if (target.contains(SwapchainTargetFlags::Depth) && target.contains(SwapchainTargetFlags::Stencil))
            return GL::DEPTH_STENCIL_ATTACHMENT;
        if (target.contains(SwapchainTargetFlags::Depth))
            return GL::DEPTH_ATTACHMENT;
        ASSERT(target.contains(SwapchainTargetFlags::Stencil));
        return GL::STENCIL_ATTACHMENT;
    };

    ScopedWebGLRestoreFramebuffer restoreFramebuffer { *m_context };
    ScopedDisableRasterizerDiscard disableRasterizerDiscard { *m_context };
    ScopedEnableBackbuffer enableBackBuffer { *m_context };
    ScopedDisableScissorTest disableScissorTest { *m_context };
    ScopedClearColorAndMask zeroClear { *m_context, 0.f, 0.f, 0.f, 0.f, true, true, true, true, };
    ScopedClearDepthAndMask zeroDepth { *m_context, 1.0f, true, m_targetFlags.contains(SwapchainTargetFlags::Depth) };
    ScopedClearStencilAndMask zeroStencil { *m_context, 0, 0xFFFFFFFF, m_targetFlags.contains(SwapchainTargetFlags::Stencil) };
    GCGLenum clearMask = computeClearMask(m_targetFlags);
    gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebufferForClearing->object());
    gl.framebufferTexture2D(GL::FRAMEBUFFER, computeAttachment(m_targetFlags), GL::TEXTURE_2D, texture, 0);
    gl.clear(clearMask);
}

RefPtr<WebGLRenderingContextBase> WebXRWebGLSwapchain::context()
{
    return m_context;
}

std::unique_ptr<WebXRWebGLSharedImageSwapchain> WebXRWebGLSharedImageSwapchain::create(WebGLRenderingContextBase& context, SwapchainTargets targets, GCGLenum format, bool clearOnAccess, size_t imageCount)
{
    return std::unique_ptr<WebXRWebGLSharedImageSwapchain>(new WebXRWebGLSharedImageSwapchain(context, targets, format, clearOnAccess, imageCount));
}

WebXRWebGLSharedImageSwapchain::WebXRWebGLSharedImageSwapchain(WebGLRenderingContextBase& context, SwapchainTargets targets, GCGLenum format, bool clearOnAccess, size_t imageCount)
    : WebXRWebGLSwapchain(context, targets, clearOnAccess, imageCount)
    , m_format(format)
{
}

WebXRWebGLSharedImageSwapchain::~WebXRWebGLSharedImageSwapchain()
{
    for (size_t i = 0; i < m_displayImagesSets.size(); ++i)
        releaseTexturesAtIndex(i);
    m_displayImagesSets.clear();
}

PlatformGLObject WebXRWebGLSharedImageSwapchain::currentTexture()
{
    // If we haven't received any frame data from the XR compositor yet, return an invalid texture id.
    if (m_displayImagesSets.isEmpty())
        return 0;

    RELEASE_ASSERT(m_displayImagesSets.size() > m_currentImageIndex);
    return m_displayImagesSets[m_currentImageIndex].colorBuffer.tex;
}

static IntSize calcImagePhysicalSize(const IntSize& leftPhysicalSize, const IntSize& rightPhysicalSize)
{
    if (rightPhysicalSize.isEmpty())
        return leftPhysicalSize;
    RELEASE_ASSERT(leftPhysicalSize.height() == rightPhysicalSize.height(), "Only side-by-side shared framebuffer layout is supported");
    return { leftPhysicalSize.width() + rightPhysicalSize.width(), leftPhysicalSize.height() };
}

static IntSize toIntSize(const auto& size)
{
    return IntSize(size[0], size[1]);
}

void WebXRWebGLSharedImageSwapchain::setupExternalImage(GraphicsContextGL&, const PlatformXR::FrameData::LayerSetupData& data)
{
    auto leftPhysicalSize = toIntSize(data.physicalSize[0]);
    auto rightPhysicalSize = toIntSize(data.physicalSize[1]);
    m_texSize = calcImagePhysicalSize(leftPhysicalSize, rightPhysicalSize);
}

const WebXRExternalImages* WebXRWebGLSharedImageSwapchain::reusableTextures(const PlatformXR::FrameData::ExternalTextureData& textureData) const
{
    if (textureData.colorTexture)
        return nullptr;

    auto reusableTextureIndex = textureData.reusableTextureIndex;
    if (reusableTextureIndex >= m_displayImagesSets.size() || !m_displayImagesSets[reusableTextureIndex]) {
        RELEASE_LOG_FAULT(XR, "Unable to find reusable texture at index: %" PRIu64, reusableTextureIndex);
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return &m_displayImagesSets[reusableTextureIndex];
}

void WebXRWebGLSharedImageSwapchain::releaseTexturesAtIndex(size_t index)
{
    if (index >= m_displayImagesSets.size())
        return;


    if (RefPtr gl = m_context->graphicsContextGL())
        m_displayImagesSets[index].release(*gl);
    else
        m_displayImagesSets[index].leakObject();
}

static void createAndBindCompositorTexture(GL& gl, WebXRExternalImage& externalImage, GCGLenum internalFormat, GL::ExternalImageSource source, GCGLint layer)
{
    auto image = gl.createExternalImage(WTF::move(source), internalFormat, layer);
    if (!image)
        return;

    externalImage.tex = gl.createTexture();
    gl.bindTexture(GL::TEXTURE_2D, externalImage.tex);
    gl.bindExternalImage(GL::TEXTURE_2D, image);
    externalImage.image.adopt(gl, image);
}

static GL::ExternalImageSource makeExternalImageSource(PlatformXR::FrameData::ExternalTexture& imageSource, const IntSize& size)
{
#if PLATFORM(GTK) || PLATFORM(WPE)
#if OS(ANDROID)
    return GraphicsContextGLExternalImageSource {
        .hardwareBuffer = imageSource,
        .size = size,
    };
#else
    return GraphicsContextGLExternalImageSource {
        .fds = WTF::move(imageSource.fds),
        .strides = WTF::move(imageSource.strides),
        .offsets = WTF::move(imageSource.offsets),
        .fourcc = imageSource.fourcc,
        .modifier = imageSource.modifier,
        .size = size
    };
#endif // OS(ANDROID)
#else
    UNUSED_PARAM(imageSource);
    UNUSED_PARAM(size);
    return GraphicsContextGLExternalImageSource { };
#endif
}

void WebXRWebGLSharedImageSwapchain::bindCompositorTexturesForDisplay(GraphicsContextGL& gl, PlatformXR::FrameData::LayerData& layerData)
{
    m_currentImageIndex = layerData.textureData ? layerData.textureData->reusableTextureIndex : 0;
    if (m_displayImagesSets.size() <= m_currentImageIndex)
        m_displayImagesSets.resizeToFit(m_currentImageIndex + 1);

    IntSize texSize = layerData.layerSetup ? toIntSize(layerData.layerSetup->physicalSize[0]) : IntSize(32, 32);
    if (!layerData.textureData) {
        m_currentImageIndex = 0;
        return;
    }

    auto displayAttachments = reusableTextures(*(layerData.textureData));
    if (displayAttachments)
        return;

    releaseTexturesAtIndex(m_currentImageIndex);

    if (!layerData.textureData->colorTexture)
        return;

    auto colorTextureSource = makeExternalImageSource(layerData.textureData->colorTexture, texSize);
#if PLATFORM(COCOA)
    constexpr auto kColorFormat = GL::BGRA_EXT;
#else
    constexpr auto kColorFormat = GL::RGBA8;
#endif
    // FIXME: add support for texture arrays.
    GCGLint layer = 0;
    createAndBindCompositorTexture(gl, m_displayImagesSets[m_currentImageIndex].colorBuffer, kColorFormat, WTF::move(colorTextureSource), layer);
    ASSERT(m_displayImagesSets[m_currentImageIndex].colorBuffer.tex);
    if (!m_displayImagesSets[m_currentImageIndex].colorBuffer.tex)
        return;

    // We should be able to use depth textures from the XR compositor passed via layerData.textureData->depthStencilBuffer. However that would force us to change the design
    // of the XR swapchains as it currently assumes that a given swapchain does only provide one type of texture. This could be revisited in the future.
}

void WebXRWebGLSharedImageSwapchain::startFrame(PlatformXR::FrameData::LayerData& data)
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    ASSERT(!m_fenceFD);
#endif

    RefPtr gl = m_context->graphicsContextGL();
    if (!gl)
        return;

    if (data.layerSetup)
        setupExternalImage(*gl, *data.layerSetup);

    bindCompositorTexturesForDisplay(*gl, data);

    if (m_clearOnAccess)
        clearCurrentTexture(*gl);
}

void WebXRWebGLSharedImageSwapchain::endFrame(PlatformXR::DeviceLayer& layerData)
{
    RefPtr gl = m_context->graphicsContextGL();
    if (!gl)
        return;

    ASSERT(gl->checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);

#if PLATFORM(GTK) || PLATFORM(WPE)
    if (auto sync = gl->createExternalSync({ })) {
        layerData.fenceFD = gl->exportExternalSync(sync);
        gl->deleteExternalSync(sync);
        return;
    }
#else
    UNUSED_PARAM(layerData);
#endif
    gl->finish();
}

void WebXRExternalImage::destroyImage(GraphicsContextGL& gl)
{
    gl.deleteTexture(tex);
    image.release(gl);
}

void WebXRExternalImage::release(GraphicsContextGL& gl)
{
    gl.deleteTexture(tex);
    image.release(gl);
}

void WebXRExternalImage::leakObject()
{
    tex = { };
    image.leakObject();
}

bool WebXRWebGLSharedImageSwapchain::allTexturesAreBound() const
{
    return m_displayImagesSets.size() == m_imageCount && std::all_of(m_displayImagesSets.begin(), m_displayImagesSets.end(), [](const auto& imageSet) {
        return imageSet && imageSet.colorBuffer.tex;
    });
}

std::unique_ptr<WebXRWebGLStaticImageSwapchain> WebXRWebGLStaticImageSwapchain::create(WebGLRenderingContextBase& context, StaticImageAttributes attributes)
{
    return std::unique_ptr<WebXRWebGLStaticImageSwapchain>(new WebXRWebGLStaticImageSwapchain(context, attributes));
}

WebXRWebGLStaticImageSwapchain::WebXRWebGLStaticImageSwapchain(WebGLRenderingContextBase& context, StaticImageAttributes attributes)
    : WebXRWebGLSwapchain(context, attributes.targets, attributes.clearOnAccess, attributes.imageCount)
    , m_imageAttributes(attributes)
{
    m_texSize = attributes.size;
}

WebXRWebGLStaticImageSwapchain::~WebXRWebGLStaticImageSwapchain()
{
    for (size_t i = 0; i < m_textures.size(); ++i)
        releaseDisplayImagesAtIndex(i);
    m_textures.clear();
}

PlatformGLObject WebXRWebGLStaticImageSwapchain::currentTexture()
{
    RELEASE_ASSERT(m_textures.size() > m_currentImageIndex);
    return m_textures[m_currentImageIndex];
}

void WebXRWebGLStaticImageSwapchain::releaseDisplayImagesAtIndex(size_t index)
{
    if (index >= m_textures.size())
        return;

    RefPtr gl = m_context->graphicsContextGL();
    if (!gl)
        return;

    gl->deleteTexture(std::exchange(m_textures[index], { }));
}

void WebXRWebGLStaticImageSwapchain::bindCompositorTexturesForDisplay(GraphicsContextGL& gl, PlatformXR::FrameData::LayerData& layerData)
{
    bool shouldCreateNewTexture = false;
    m_currentImageIndex = layerData.textureData ? layerData.textureData->reusableTextureIndex : 0;
    if (m_textures.size() <= m_currentImageIndex) {
        m_textures.resizeToFit(m_currentImageIndex + 1);
        shouldCreateNewTexture = true;
    }

    if (!shouldCreateNewTexture)
        return;

    releaseDisplayImagesAtIndex(m_currentImageIndex);

    // FIXME: add support for texture arrays.
    GCGLenum target = GL::TEXTURE_2D;
    PlatformGLObject texture = gl.createTexture();
    gl.bindTexture(target, texture);

    if (m_context->isWebGL2())
        gl.texStorage2D(target, 1, m_imageAttributes.internalFormat, m_texSize.width(), m_texSize.height());
    else
        gl.texImage2D(target, 0, m_imageAttributes.internalFormat, m_texSize.width(), m_texSize.height(), 0, m_imageAttributes.format, GL::UNSIGNED_INT, { });

    m_textures[m_currentImageIndex] = texture;
}

void WebXRWebGLStaticImageSwapchain::startFrame(PlatformXR::FrameData::LayerData& data)
{
    RefPtr gl = m_context->graphicsContextGL();
    if (!gl)
        return;

    bindCompositorTexturesForDisplay(*gl, data);

    if (m_clearOnAccess)
        clearCurrentTexture(*gl);
}

void WebXRWebGLStaticImageSwapchain::endFrame(PlatformXR::DeviceLayer&)
{

}

bool WebXRWebGLStaticImageSwapchain::allTexturesAreBound() const
{
    return m_textures.size() == m_imageCount && std::all_of(m_textures.begin(), m_textures.end(), [](const auto& texture) {
        return texture;
    });
}


} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
