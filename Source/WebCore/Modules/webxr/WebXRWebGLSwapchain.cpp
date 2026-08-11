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

void WebXRWebGLSwapchain::clearAttachmentRegion(GraphicsContextGL& gl, const IntRect& viewport, NOESCAPE const BindAttachmentFunction& bindAttachment)
{
    ASSERT(m_context);
    ASSERT(m_framebufferForClearing);

    GCGLenum clearMask = 0;
    if (m_targetFlags.contains(SwapchainTargetFlags::Color))
        clearMask |= GL::COLOR_BUFFER_BIT;
    if (m_targetFlags.contains(SwapchainTargetFlags::Depth))
        clearMask |= GL::DEPTH_BUFFER_BIT;
    if (m_targetFlags.contains(SwapchainTargetFlags::Stencil))
        clearMask |= GL::STENCIL_BUFFER_BIT;

    GCGLenum attachment = GL::COLOR_ATTACHMENT0;
    if (m_targetFlags.contains(SwapchainTargetFlags::Depth) && m_targetFlags.contains(SwapchainTargetFlags::Stencil))
        attachment = GL::DEPTH_STENCIL_ATTACHMENT;
    else if (m_targetFlags.contains(SwapchainTargetFlags::Depth))
        attachment = GL::DEPTH_ATTACHMENT;
    else if (m_targetFlags.contains(SwapchainTargetFlags::Stencil))
        attachment = GL::STENCIL_ATTACHMENT;
    else
        ASSERT(m_targetFlags.contains(SwapchainTargetFlags::Color));

    ScopedWebGLRestoreFramebuffer restoreFramebuffer { *m_context };
    ScopedDisableRasterizerDiscard disableRasterizerDiscard { *m_context };
    ScopedEnableBackbuffer enableBackBuffer { *m_context };
    ScopedScissorTestForRegion scopedScissor { *m_context, viewport };
    ScopedClearColorAndMask zeroClear { *m_context, 0.f, 0.f, 0.f, 0.f, true, true, true, true };
    ScopedClearDepthAndMask zeroDepth { *m_context, 1.0f, true, m_targetFlags.contains(SwapchainTargetFlags::Depth) };
    ScopedClearStencilAndMask zeroStencil { *m_context, 0, 0xFFFFFFFF, m_targetFlags.contains(SwapchainTargetFlags::Stencil) };

    gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebufferForClearing->object());
    bindAttachment(attachment);
    gl.clear(clearMask);
}

void WebXRWebGLSwapchain::clearTextureRegion(GraphicsContextGL& gl, const IntRect& viewport, std::optional<GCGLint> slice)
{
    UNUSED_PARAM(slice);
    if (!m_framebufferForClearing)
        return;

    auto texture = currentTexture();
    if (!texture)
        return;

    clearAttachmentRegion(gl, viewport, [&](GCGLenum attachment) {
        gl.framebufferTexture2D(GL::FRAMEBUFFER, attachment, GL::TEXTURE_2D, texture, 0);
    });
}

static IntSize toIntSize(const auto& size)
{
    return IntSize(size[0], size[1]);
}

static IntSize calcImagePhysicalSize(const IntSize& leftPhysicalSize, const IntSize& rightPhysicalSize)
{
    if (rightPhysicalSize.isEmpty())
        return leftPhysicalSize;
    RELEASE_ASSERT(leftPhysicalSize.height() == rightPhysicalSize.height(), "Only side-by-side shared framebuffer layout is supported");
    return { leftPhysicalSize.width() + rightPhysicalSize.width(), leftPhysicalSize.height() };
}

void WebXRWebGLSwapchain::setupExternalImage(const PlatformXR::FrameData::LayerSetupData& data)
{
    auto leftPhysicalSize = toIntSize(data.physicalSize[0]);
    auto rightPhysicalSize = toIntSize(data.physicalSize[1]);

    m_texSize = calcImagePhysicalSize(leftPhysicalSize, rightPhysicalSize);
}

void WebXRWebGLSwapchain::clearTextureIfNeeded(const IntRect& viewport, std::optional<GCGLint> slice)
{
    if (!m_clearOnAccess)
        return;
    RefPtr gl = m_context->graphicsContextGL();
    if (!gl)
        return;
    clearTextureRegion(*gl, viewport, slice);
}

void WebXRWebGLSwapchain::signalEndFrame(GraphicsContextGL& gl, PlatformXR::DeviceLayer& layerData)
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    if (auto sync = gl.createExternalSync({ })) {
        layerData.fenceFD = gl.exportExternalSync(sync);
        gl.deleteExternalSync(sync);
        return;
    }
#else
    UNUSED_PARAM(layerData);
#endif
    gl.finish();
}

RefPtr<WebGLRenderingContextBase> WebXRWebGLSwapchain::context()
{
    return m_context;
}

std::unique_ptr<WebXRWebGLSharedImageSwapchain> WebXRWebGLSharedImageSwapchain::create(WebGLRenderingContextBase& context, SwapchainTargets targets, GCGLenum format, IntSize initialSize, bool clearOnAccess, size_t imageCount)
{
    return std::unique_ptr<WebXRWebGLSharedImageSwapchain>(new WebXRWebGLSharedImageSwapchain(context, targets, format, initialSize, clearOnAccess, imageCount));
}

WebXRWebGLSharedImageSwapchain::WebXRWebGLSharedImageSwapchain(WebGLRenderingContextBase& context, SwapchainTargets targets, GCGLenum format, IntSize initialSize, bool clearOnAccess, size_t imageCount)
    : WebXRWebGLSwapchain(context, targets, clearOnAccess, imageCount)
    , m_format(format)
{
    m_texSize = initialSize;
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
    // DMABuf does not support sharing texture arrays so this is always 0.
    // FIXME: eventually check for other texture sharing mechanisms that might support texture arrays sharing.
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
        setupExternalImage(*data.layerSetup);

    bindCompositorTexturesForDisplay(*gl, data);
}

void WebXRWebGLSharedImageSwapchain::endFrame(PlatformXR::DeviceLayer& layerData)
{
    RefPtr gl = m_context->graphicsContextGL();
    if (!gl)
        return;

    ASSERT(gl->checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE);
    signalEndFrame(*gl, layerData);
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
    size_t index = m_imageAttributes.textureType == GL::TEXTURE_CUBE_MAP ? m_currentImageIndex * m_imageAttributes.arrayLength : m_currentImageIndex;
    RELEASE_ASSERT(m_textures.size() > index);
    return m_textures[index];
}

PlatformGLObject WebXRWebGLStaticImageSwapchain::currentTextureAtIndex(uint32_t cubeIndex)
{
    if (m_imageAttributes.textureType != GL::TEXTURE_CUBE_MAP)
        return currentTexture();
    size_t index = m_currentImageIndex * m_imageAttributes.arrayLength + cubeIndex;
    RELEASE_ASSERT(m_textures.size() > index);
    return m_textures[index];
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
    m_currentImageIndex = layerData.textureData ? layerData.textureData->reusableTextureIndex : 0;

    if (m_imageAttributes.textureType == GL::TEXTURE_CUBE_MAP) {
        size_t baseIndex = m_currentImageIndex * m_imageAttributes.arrayLength;
        if (m_textures.size() > baseIndex && m_textures[baseIndex])
            return;
        m_textures.resizeToFit(baseIndex + m_imageAttributes.arrayLength);
        for (uint32_t i = 0; i < m_imageAttributes.arrayLength; ++i) {
            PlatformGLObject texture = gl.createTexture();
            gl.bindTexture(GL::TEXTURE_CUBE_MAP, texture);
            gl.texStorage2D(GL::TEXTURE_CUBE_MAP, 1, m_imageAttributes.internalFormat, m_texSize.width(), m_texSize.height());
            m_textures[baseIndex + i] = texture;
        }
        return;
    }

    bool shouldCreateNewTexture = false;
    if (m_textures.size() <= m_currentImageIndex) {
        m_textures.resizeToFit(m_currentImageIndex + 1);
        shouldCreateNewTexture = true;
    }

    if (!shouldCreateNewTexture)
        return;

    releaseDisplayImagesAtIndex(m_currentImageIndex);

    GCGLenum target = m_imageAttributes.textureType;
    PlatformGLObject texture = gl.createTexture();
    gl.bindTexture(target, texture);

    if (m_imageAttributes.textureType == GL::TEXTURE_2D_ARRAY)
        gl.texStorage3D(target, 1, m_imageAttributes.internalFormat, m_texSize.width(), m_texSize.height(), m_imageAttributes.arrayLength);
    else if (m_context->isWebGL2())
        gl.texStorage2D(target, 1, m_imageAttributes.internalFormat, m_texSize.width(), m_texSize.height());
    else
        gl.texImage2D(target, 0, m_imageAttributes.internalFormat, m_texSize.width(), m_texSize.height(), 0, m_imageAttributes.format, GL::UNSIGNED_INT, { });

    m_textures[m_currentImageIndex] = texture;
}

void WebXRWebGLStaticImageSwapchain::clearTextureRegion(GraphicsContextGL& gl, const IntRect& viewport, std::optional<GCGLint> slice)
{
    if (!m_framebufferForClearing)
        return;

    switch (m_imageAttributes.textureType) {
    case GL::TEXTURE_CUBE_MAP: {
        uint32_t cubeIndex = slice ? static_cast<uint32_t>(*slice) : 0;
        auto texture = currentTextureAtIndex(cubeIndex);
        if (!texture)
            return;
        for (uint32_t face = 0; face < 6; ++face) {
            clearAttachmentRegion(gl, viewport, [&](GCGLenum attachment) {
                gl.framebufferTexture2D(GL::FRAMEBUFFER, attachment, GL::TEXTURE_CUBE_MAP_POSITIVE_X + face, texture, 0);
            });
        }
        return;
    }
    case GL::TEXTURE_2D_ARRAY: {
        auto texture = currentTexture();
        if (!texture)
            return;
        ASSERT(slice);
        clearAttachmentRegion(gl, viewport, [&](GCGLenum attachment) {
            gl.framebufferTextureLayer(GL::FRAMEBUFFER, attachment, texture, 0, *slice);
        });
        return;
    }
    default:
        WebXRWebGLSwapchain::clearTextureRegion(gl, viewport, slice);
    }
}

void WebXRWebGLStaticImageSwapchain::startFrame(PlatformXR::FrameData::LayerData& data)
{
    RefPtr gl = m_context->graphicsContextGL();
    if (!gl)
        return;

    bindCompositorTexturesForDisplay(*gl, data);
}

void WebXRWebGLStaticImageSwapchain::endFrame(PlatformXR::DeviceLayer&)
{

}

bool WebXRWebGLStaticImageSwapchain::allTexturesAreBound() const
{
    size_t expected = m_imageAttributes.textureType == GL::TEXTURE_CUBE_MAP ? m_imageCount * m_imageAttributes.arrayLength : m_imageCount;
    return m_textures.size() == expected && std::all_of(m_textures.begin(), m_textures.end(), [](const auto& texture) {
        return texture;
    });
}

WebXRWebGLMultiTextureSwapchain::WebXRWebGLMultiTextureSwapchain(WebGLRenderingContextBase& context, SwapchainTargets targets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount)
    : WebXRWebGLSwapchain(context, targets, clearOnAccess, imageCount)
    , m_internalFormat(internalFormat)
{
}

WebXRWebGLMultiTextureSwapchain::~WebXRWebGLMultiTextureSwapchain()
{
    for (size_t i = 0; i < m_textureSets.size(); ++i)
        releaseTexturesAtIndex(i);
    m_textureSets.clear();

    if (RefPtr gl = m_context->graphicsContextGL()) {
        if (m_blitReadFBO)
            gl->deleteFramebuffer(m_blitReadFBO);
        if (m_blitDrawFBO)
            gl->deleteFramebuffer(m_blitDrawFBO);
    }
}

void WebXRWebGLMultiTextureSwapchain::TextureSet::release(GraphicsContextGL& gl)
{
    for (auto& texture : renderTextures) {
        if (texture)
            gl.deleteTexture(texture);
    }
    renderTextures.clear();
    sharedImage.release(gl);
}

void WebXRWebGLMultiTextureSwapchain::TextureSet::leakObject()
{
    renderTextures.clear();
    sharedImage.leakObject();
}

PlatformGLObject WebXRWebGLMultiTextureSwapchain::currentTexture()
{
    return currentTextureAtIndex(0);
}

PlatformGLObject WebXRWebGLMultiTextureSwapchain::currentTextureAtIndex(uint32_t index)
{
    if (m_textureSets.isEmpty())
        return 0;
    RELEASE_ASSERT(m_textureSets.size() > m_currentImageIndex);
    auto& renderTextures = m_textureSets[m_currentImageIndex].renderTextures;
    return index < renderTextures.size() ? renderTextures[index] : 0;
}

void WebXRWebGLMultiTextureSwapchain::releaseTexturesAtIndex(size_t index)
{
    if (index >= m_textureSets.size())
        return;

    if (RefPtr gl = m_context->graphicsContextGL())
        m_textureSets[index].release(*gl);
    else
        m_textureSets[index].leakObject();
}

const WebXRExternalImages* WebXRWebGLMultiTextureSwapchain::reusableTextures(const PlatformXR::FrameData::ExternalTextureData& textureData) const
{
    if (textureData.colorTexture)
        return nullptr;

    auto reusableTextureIndex = textureData.reusableTextureIndex;
    if (reusableTextureIndex >= m_textureSets.size() || !m_textureSets[reusableTextureIndex]) {
        RELEASE_LOG_FAULT(XR, "Unable to find reusable texture at index: %" PRIu64, reusableTextureIndex);
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return &m_textureSets[reusableTextureIndex].sharedImage;
}

void WebXRWebGLMultiTextureSwapchain::startFrame(PlatformXR::FrameData::LayerData& data)
{
    RefPtr gl = m_context->graphicsContextGL();
    if (!gl)
        return;

    if (data.layerSetup)
        setupExternalImage(*data.layerSetup);

    bindCompositorTexturesForDisplay(*gl, data);
}

void WebXRWebGLMultiTextureSwapchain::endFrame(PlatformXR::DeviceLayer& layerData)
{
    RefPtr gl = m_context->graphicsContextGL();
    if (!gl)
        return;

    blitToSharedImage(*gl);
    signalEndFrame(*gl, layerData);
}

bool WebXRWebGLMultiTextureSwapchain::allTexturesAreBound() const
{
    return m_textureSets.size() == m_imageCount && std::all_of(m_textureSets.begin(), m_textureSets.end(), [](const auto& set) {
        return static_cast<bool>(set);
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRWebGLTextureArraySwapchain);

std::unique_ptr<WebXRWebGLTextureArraySwapchain> WebXRWebGLTextureArraySwapchain::create(WebGLRenderingContextBase& context, SwapchainTargets targets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount, uint32_t arrayLength)
{
    ASSERT(arrayLength);
    return std::unique_ptr<WebXRWebGLTextureArraySwapchain>(new WebXRWebGLTextureArraySwapchain(context, targets, internalFormat, clearOnAccess, imageCount, arrayLength));
}

WebXRWebGLTextureArraySwapchain::WebXRWebGLTextureArraySwapchain(WebGLRenderingContextBase& context, SwapchainTargets targets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount, uint32_t arrayLength)
    : WebXRWebGLMultiTextureSwapchain(context, targets, internalFormat, clearOnAccess, imageCount)
    , m_arrayLength(arrayLength)
{
}

void WebXRWebGLTextureArraySwapchain::bindCompositorTexturesForDisplay(GraphicsContextGL& gl, PlatformXR::FrameData::LayerData& layerData)
{
    m_currentImageIndex = layerData.textureData ? layerData.textureData->reusableTextureIndex : 0;
    if (m_textureSets.size() <= m_currentImageIndex)
        m_textureSets.resizeToFit(m_currentImageIndex + 1);

    if (!layerData.textureData) {
        m_currentImageIndex = 0;
        return;
    }

    auto& currentSet = m_textureSets[m_currentImageIndex];

    // If the shared image is already bound (reusable), only create the array texture if needed.
    auto* reusable = reusableTextures(*(layerData.textureData));
    if (!reusable) {
        // New frame data: release old textures and bind the new shared image.
        releaseTexturesAtIndex(m_currentImageIndex);

        if (!layerData.textureData->colorTexture)
            return;

        IntSize texSize = layerData.layerSetup ? toIntSize(layerData.layerSetup->physicalSize[0]) : IntSize(32, 32);
        auto colorTextureSource = makeExternalImageSource(layerData.textureData->colorTexture, texSize);
#if PLATFORM(COCOA)
        constexpr auto kColorFormat = GL::BGRA_EXT;
#else
        constexpr auto kColorFormat = GL::RGBA8;
#endif
        // DMABuf does not support sharing texture arrays so this is always 0.
        // FIXME: eventually check for other texture sharing mechanisms that might support texture arrays sharing.
        GCGLint layer = 0;
        createAndBindCompositorTexture(gl, currentSet.sharedImage.colorBuffer, kColorFormat, WTF::move(colorTextureSource), layer);
        if (!currentSet.sharedImage.colorBuffer.tex)
            return;
    }

    // Create the GL_TEXTURE_2D_ARRAY that the WebXR app will render into. Each array layer has the per-eye width.
    if (currentSet.renderTextures.isEmpty()) {
        auto sliceWidth = m_texSize.width() / static_cast<int>(m_arrayLength);
        PlatformGLObject arrayTexture = gl.createTexture();
        gl.bindTexture(GL::TEXTURE_2D_ARRAY, arrayTexture);
        gl.texStorage3D(GL::TEXTURE_2D_ARRAY, 1, m_internalFormat, sliceWidth, m_texSize.height(), m_arrayLength);
        currentSet.renderTextures.append(arrayTexture);
    }
}

void WebXRWebGLTextureArraySwapchain::blitToSharedImage(GraphicsContextGL& gl)
{
    auto& currentSet = m_textureSets[m_currentImageIndex];
    if (currentSet.renderTextures.isEmpty() || !currentSet.sharedImage.colorBuffer.tex)
        return;

    if (!m_blitReadFBO)
        m_blitReadFBO = gl.createFramebuffer();
    if (!m_blitDrawFBO)
        m_blitDrawFBO = gl.createFramebuffer();

    auto arrayTexture = currentSet.renderTextures[0];
    auto sliceWidth = m_texSize.width() / static_cast<int>(m_arrayLength);
    auto sliceHeight = m_texSize.height();

    gl.bindFramebuffer(GL::READ_FRAMEBUFFER, m_blitReadFBO);
    gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, m_blitDrawFBO);
    gl.framebufferTexture2D(GL::DRAW_FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, currentSet.sharedImage.colorBuffer.tex, 0);

    for (uint32_t layer = 0; layer < m_arrayLength; ++layer) {
        gl.framebufferTextureLayer(GL::READ_FRAMEBUFFER, GL::COLOR_ATTACHMENT0, arrayTexture, 0, static_cast<GCGLint>(layer));
        auto dstX = static_cast<GCGLint>(layer) * sliceWidth;
        gl.blitFramebuffer(0, 0, sliceWidth, sliceHeight, dstX, 0, dstX + sliceWidth, sliceHeight, GL::COLOR_BUFFER_BIT, GL::NEAREST);
    }

    gl.bindFramebuffer(GL::FRAMEBUFFER, 0);
}

void WebXRWebGLTextureArraySwapchain::clearTextureRegion(GraphicsContextGL& gl, const IntRect& viewport, std::optional<GCGLint> slice)
{
    if (!m_framebufferForClearing)
        return;

    auto texture = currentTexture();
    if (!texture)
        return;

    ASSERT(slice);
    clearAttachmentRegion(gl, viewport, [&](GCGLenum attachment) {
        gl.framebufferTextureLayer(GL::FRAMEBUFFER, attachment, texture, 0, *slice);
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRWebGLCubeSwapchain);

std::unique_ptr<WebXRWebGLCubeSwapchain> WebXRWebGLCubeSwapchain::create(WebGLRenderingContextBase& context, SwapchainTargets targets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount, uint32_t cubeCount)
{
    return std::unique_ptr<WebXRWebGLCubeSwapchain>(new WebXRWebGLCubeSwapchain(context, targets, internalFormat, clearOnAccess, imageCount, cubeCount));
}

WebXRWebGLCubeSwapchain::WebXRWebGLCubeSwapchain(WebGLRenderingContextBase& context, SwapchainTargets targets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount, uint32_t cubeCount)
    : WebXRWebGLMultiTextureSwapchain(context, targets, internalFormat, clearOnAccess, imageCount)
    , m_cubeCount(cubeCount)
{
}

void WebXRWebGLCubeSwapchain::bindCompositorTexturesForDisplay(GraphicsContextGL& gl, PlatformXR::FrameData::LayerData& layerData)
{
    m_currentImageIndex = layerData.textureData ? layerData.textureData->reusableTextureIndex : 0;
    if (m_textureSets.size() <= m_currentImageIndex)
        m_textureSets.resizeToFit(m_currentImageIndex + 1);

    if (!layerData.textureData) {
        m_currentImageIndex = 0;
        return;
    }

    auto& currentSet = m_textureSets[m_currentImageIndex];

    auto* reusable = reusableTextures(*(layerData.textureData));
    if (!reusable) {
        releaseTexturesAtIndex(m_currentImageIndex);

        if (!layerData.textureData->colorTexture)
            return;

        // The shared image is the side-by-side 2D texture (cubeCount*faceCount faces laid out horizontally) that the UIProcess reconstructs into cube swapchain faces.
        IntSize texSize = layerData.layerSetup ? toIntSize(layerData.layerSetup->physicalSize[0]) : IntSize(32, 32);
        auto colorTextureSource = makeExternalImageSource(layerData.textureData->colorTexture, texSize);
#if PLATFORM(COCOA)
        constexpr auto kColorFormat = GL::BGRA_EXT;
#else
        constexpr auto kColorFormat = GL::RGBA8;
#endif
        GCGLint layer = 0;
        createAndBindCompositorTexture(gl, currentSet.sharedImage.colorBuffer, kColorFormat, WTF::move(colorTextureSource), layer);
        if (!currentSet.sharedImage.colorBuffer.tex)
            return;
    }

    // One square cubemap per eye for the app to render into, the shared image is cubeCount*faceCount faces wide.
    if (currentSet.renderTextures.isEmpty()) {
        auto faceSize = m_texSize.height();
        for (uint32_t cube = 0; cube < m_cubeCount; ++cube) {
            PlatformGLObject cubeTexture = gl.createTexture();
            gl.bindTexture(GL::TEXTURE_CUBE_MAP, cubeTexture);
            gl.texStorage2D(GL::TEXTURE_CUBE_MAP, 1, m_internalFormat, faceSize, faceSize);
            currentSet.renderTextures.append(cubeTexture);
        }
    }
}

void WebXRWebGLCubeSwapchain::blitToSharedImage(GraphicsContextGL& gl)
{
    auto& currentSet = m_textureSets[m_currentImageIndex];
    if (currentSet.renderTextures.isEmpty() || !currentSet.sharedImage.colorBuffer.tex)
        return;

    if (!m_blitReadFBO)
        m_blitReadFBO = gl.createFramebuffer();
    if (!m_blitDrawFBO)
        m_blitDrawFBO = gl.createFramebuffer();

    auto faceSize = m_texSize.height();

    gl.bindFramebuffer(GL::READ_FRAMEBUFFER, m_blitReadFBO);
    gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, m_blitDrawFBO);
    gl.framebufferTexture2D(GL::DRAW_FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, currentSet.sharedImage.colorBuffer.tex, 0);

    // Region (cube*faceCount + face) holds GL_TEXTURE_CUBE_MAP_POSITIVE_X + face of that cube. UIProcess reconstruction must use the same ordering.
    for (uint32_t cube = 0; cube < currentSet.renderTextures.size(); ++cube) {
        for (uint32_t face = 0; face < faceCount; ++face) {
            gl.framebufferTexture2D(GL::READ_FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_CUBE_MAP_POSITIVE_X + face, currentSet.renderTextures[cube], 0);
            auto dstX = static_cast<GCGLint>(cube * faceCount + face) * faceSize;
            gl.blitFramebuffer(0, 0, faceSize, faceSize, dstX, 0, dstX + faceSize, faceSize, GL::COLOR_BUFFER_BIT, GL::NEAREST);
        }
    }

    gl.bindFramebuffer(GL::FRAMEBUFFER, 0);
}

void WebXRWebGLCubeSwapchain::clearTextureRegion(GraphicsContextGL& gl, const IntRect& viewport, std::optional<GCGLint> slice)
{
    if (!m_framebufferForClearing || m_textureSets.isEmpty())
        return;

    // slice selects the cube (eye) othewise clearing all cubes would wipe the other eye's content.
    auto& renderTextures = m_textureSets[m_currentImageIndex].renderTextures;
    auto cube = static_cast<size_t>(slice.value_or(0));
    if (cube >= renderTextures.size())
        return;

    for (uint32_t face = 0; face < faceCount; ++face) {
        clearAttachmentRegion(gl, viewport, [&](GCGLenum attachment) {
            gl.framebufferTexture2D(GL::FRAMEBUFFER, attachment, GL::TEXTURE_CUBE_MAP_POSITIVE_X + face, renderTextures[cube], 0);
        });
    }
}

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
