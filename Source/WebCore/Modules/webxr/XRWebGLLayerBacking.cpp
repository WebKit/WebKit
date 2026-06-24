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
#include "XRWebGLLayerBacking.h"

#if ENABLE(WEBXR_LAYERS)

#include "FloatSize.h"
#include "GraphicsContextGL.h"
#include "PlatformXR.h"
#include "WebGLOpaqueTexture.h"
#include "WebGLRenderingContextBase.h"
#include "WebXROpaqueFramebuffer.h"
#include "WebXRSession.h"
#include "WebXRWebGLSwapchain.h"
#include "XRLayerInit.h"
#include "XRLayerLayout.h"
#include "XRProjectionLayerInit.h"
#include "XRTextureType.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRWebGLLayerBacking);

using GL = GraphicsContextGL;

XRWebGLLayerBacking::XRWebGLLayerBacking(PlatformXR::LayerHandle handle, std::unique_ptr<WebXRWebGLSwapchain>&& colorSwapchain, std::unique_ptr<WebXRWebGLSwapchain>&& depthSwapchain, uint32_t colorTextureArrayLength)
    : m_colorSwapchain(WTF::move(colorSwapchain))
    , m_depthSwapchain(WTF::move(depthSwapchain))
    , m_colorTextureArrayLength(colorTextureArrayLength)
{
    setHandle(handle);
}

uint32_t XRWebGLLayerBacking::colorTextureWidth() const
{
    return m_colorSwapchain->size().width();
};

uint32_t XRWebGLLayerBacking::colorTextureHeight() const
{
    return m_colorSwapchain->size().height();
};

uint32_t XRWebGLLayerBacking::colorTextureArrayLength() const
{
    return m_colorTextureArrayLength;
};

std::optional<uint32_t> XRWebGLLayerBacking::depthTextureWidth() const
{
    return m_depthSwapchain ? std::make_optional(m_depthSwapchain->size().width()) : std::nullopt;
}

std::optional<uint32_t> XRWebGLLayerBacking::depthTextureHeight() const
{
    return m_depthSwapchain ? std::make_optional(m_depthSwapchain->size().height()) : std::nullopt;
}

#if PLATFORM(COCOA)
void XRWebGLLayerBacking::startFrame(size_t, MachSendRight&&, MachSendRight&&, MachSendRight&&, size_t, PlatformXR::RateMapDescription&&)
{
}

void XRWebGLLayerBacking::endFrame()
{
}

#else
void XRWebGLLayerBacking::startFrame(PlatformXR::FrameData& data)
{
    ASSERT(m_colorSwapchain);

    auto it = data.layers.find(handle());
    if (it == data.layers.end()) {
        m_shouldSkipFrame = true;
        return;
    }

    m_colorSwapchain->startFrame(it->value);
    if (m_depthSwapchain)
        m_depthSwapchain->startFrame(it->value);
    m_shouldSkipFrame = false;
}

void XRWebGLLayerBacking::endFrame(PlatformXR::DeviceLayer& layerData)
{
    ASSERT(m_colorSwapchain);
    if (m_shouldSkipFrame)
        return;
    m_colorSwapchain->endFrame(layerData);
    if (m_depthSwapchain)
        m_depthSwapchain->endFrame(layerData);
    m_shouldSkipFrame = true;
}
#endif

RefPtr<WebGLOpaqueTexture> XRWebGLLayerBacking::currentColorTexture(uint32_t index) const
{
    if (auto texture = m_colorSwapchain->currentTextureAtIndex(index))
        return WebGLOpaqueTexture::create(*m_colorSwapchain->context(), texture, m_colorSwapchain->textureTarget());
    return nullptr;
}

bool XRWebGLLayerBacking::requiresPerViewColorTextures() const
{
    // Non-array stereo cube layers use two separate GL_TEXTURE_CUBE_MAP objects (one per eye);
    // all other layer types (including TextureArray cube, which uses one TEXTURE_2D_ARRAY) use one texture.
    // That's because you cannot concat multiple cube map textures into a single side-by-side cube map texture.
    return m_colorSwapchain->textureTarget() == GraphicsContextGL::TEXTURE_CUBE_MAP && m_colorTextureArrayLength > 1;
}

RefPtr<WebGLOpaqueTexture> XRWebGLLayerBacking::currentDepthTexture(uint32_t index) const
{
    if (!m_depthSwapchain)
        return nullptr;
    if (auto texture = m_depthSwapchain->currentTextureAtIndex(index))
        return WebGLOpaqueTexture::create(*m_depthSwapchain->context(), texture, m_depthSwapchain->textureTarget());
    return nullptr;
}

static std::pair<IntSize, PlatformXR::LayerLayout> computeNonProjectionLayerSize(uint32_t viewPixelWidth, uint32_t viewPixelHeight, XRLayerLayout layout)
{
    switch (layout) {
    case XRLayerLayout::Mono:
        return { IntSize { static_cast<int>(viewPixelWidth), static_cast<int>(viewPixelHeight) }, PlatformXR::LayerLayout::Mono };
    case XRLayerLayout::Stereo:
    case XRLayerLayout::StereoLeftRight:
        return { IntSize { static_cast<int>(viewPixelWidth * 2), static_cast<int>(viewPixelHeight) }, PlatformXR::LayerLayout::StereoLeftRight };
    case XRLayerLayout::StereoTopBottom:
        return { IntSize { static_cast<int>(viewPixelWidth), static_cast<int>(viewPixelHeight * 2) }, PlatformXR::LayerLayout::StereoTopBottom };
    default:
    case XRLayerLayout::Default:
        ASSERT_NOT_REACHED_WITH_MESSAGE("Default layout is not supported for non-projection Layers");
        return { IntSize(), PlatformXR::LayerLayout::Mono };
    };
}

static uint32_t computeArrayLength(bool useTextureArray, uint32_t textureArrayLength)
{
    return useTextureArray ? textureArrayLength : 1;
}

ExceptionOr<XRWebGLLayerBacking::XRLayerSwapchains> XRWebGLLayerBacking::createCompositionLayerSwapchains(WebXRSession& session, WebGLRenderingContextBase& context, PlatformXR::CompositionLayerType layerType, const XRLayerInit& init)
{
    auto device = session.device();
    if (!device)
        return Exception { ExceptionCode::OperationError, "Cannot create a composition layer without a valid device."_s };

    bool useTextureArray = init.textureType == XRTextureType::TextureArray;

    IntSize layerSize;
    PlatformXR::LayerLayout layerLayout;
    GCGLenum colorTextureType;
    uint32_t arrayLength;

    if (layerType == PlatformXR::CompositionLayerType::Cube) {
        bool isStereo = init.layout == XRLayerLayout::Stereo;
        layerSize = IntSize { static_cast<int>(init.viewPixelWidth), static_cast<int>(init.viewPixelHeight) };
        layerLayout = init.layout == XRLayerLayout::Stereo ? PlatformXR::LayerLayout::Stereo : PlatformXR::LayerLayout::Mono;
        // TextureArray: one TEXTURE_2D_ARRAY with 6 faces per eye (spec: +X,-X,+Y,-Y,+Z,-Z; right eye starts at layer 6 for stereo)
        // Non-array: one GL_TEXTURE_CUBE_MAP per eye
        colorTextureType = useTextureArray ? GL::TEXTURE_2D_ARRAY : GL::TEXTURE_CUBE_MAP;
        arrayLength = useTextureArray ? (isStereo ? 12 : 6) : (isStereo ? 2 : 1);
    } else {
        std::tie(layerSize, layerLayout) = computeNonProjectionLayerSize(init.viewPixelWidth, init.viewPixelHeight, init.layout);
        colorTextureType = useTextureArray ? GL::TEXTURE_2D_ARRAY : GL::TEXTURE_2D;
        uint32_t slicesPerLayer = layerLayout == PlatformXR::LayerLayout::Mono ? 1 : 2;
        arrayLength = computeArrayLength(useTextureArray, slicesPerLayer);
    }

    auto layerInfo = device->createCompositionLayer(layerType, layerSize, layerLayout);
    if (!layerInfo)
        return Exception { ExceptionCode::OperationError, "Unable to create a composition layer."_s };

    return createColorAndDepthSwapchains(context, layerInfo->handle, init.colorFormat, init.depthFormat, layerSize, init.clearOnAccess, layerInfo->numImages, arrayLength, colorTextureType);
}

ExceptionOr<XRWebGLLayerBacking::XRLayerSwapchains> XRWebGLLayerBacking::createProjectionLayerSwapchains(WebXRSession& session, WebGLRenderingContextBase& context, const XRProjectionLayerInit& init)
{
    constexpr double MinTextureScalingFactor = 0.2;
    auto device = session.device();
    if (!device)
        return Exception { ExceptionCode::OperationError, "Cannot create a projection layer without a valid device."_s };

    double clampedScaleFactor = std::clamp(init.scaleFactor, MinTextureScalingFactor, device->maxFramebufferScalingFactor());
    FloatSize recommendedSize = session.recommendedWebGLFramebufferResolution();
    IntSize size = expandedIntSize(recommendedSize.scaled(static_cast<float>(clampedScaleFactor)));

    auto layerInfo = device->createLayerProjection(size.width(), size.height(), true);
    if (!layerInfo)
        return Exception { ExceptionCode::OperationError, "Unable to create a projection layer."_s };

    bool useTextureArray = init.textureType == XRTextureType::TextureArray;
    GCGLenum colorTextureType = useTextureArray ? GL::TEXTURE_2D_ARRAY : GL::TEXTURE_2D;
    uint32_t arrayLength = computeArrayLength(useTextureArray, static_cast<uint32_t>(session.views().size()));

    return XRWebGLLayerBacking::createColorAndDepthSwapchains(context, layerInfo->handle, init.colorFormat, init.depthFormat, size, init.clearOnAccess, layerInfo->numImages, arrayLength, colorTextureType);
}

ExceptionOr<XRWebGLLayerBacking::XRLayerSwapchains> XRWebGLLayerBacking::createColorAndDepthSwapchains(WebGLRenderingContextBase& context, PlatformXR::LayerHandle handle, GCGLenum colorFormat, std::optional<GCGLenum> depthFormat, IntSize size, bool clearOnAccess, size_t numImages, uint32_t arrayLength, GCGLenum colorTextureType)
{
    std::unique_ptr<WebXRWebGLSwapchain> colorSwapchain;
    std::unique_ptr<WebXRWebGLSwapchain> depthSwapchain;

    bool useTextureArray = colorTextureType == GL::TEXTURE_2D_ARRAY;
    bool isCube = colorTextureType == GL::TEXTURE_CUBE_MAP;
    if (isCube) {
        auto colorFormats = swapchainFormatsForLayerFormat(colorFormat);
        colorSwapchain = WebXRWebGLCubeSwapchain::create(context, WebXRSwapchain::SwapchainTargetFlags::Color, colorFormats.internalFormat, clearOnAccess, numImages, arrayLength);
    } else if (useTextureArray) {
        auto colorFormats = swapchainFormatsForLayerFormat(colorFormat);
        colorSwapchain = WebXRWebGLTextureArraySwapchain::create(context, WebXRSwapchain::SwapchainTargetFlags::Color, colorFormats.internalFormat, clearOnAccess, numImages, arrayLength);
    } else
        colorSwapchain = WebXRWebGLSharedImageSwapchain::create(context, WebXRSwapchain::SwapchainTargetFlags::Color, colorFormat, size, clearOnAccess, numImages);

    if (!colorSwapchain)
        return Exception { ExceptionCode::OperationError, "Failed to create a WebGL swapchain."_s };

    if (depthFormat && *depthFormat) {
        IntSize depthSize = (!isCube && useTextureArray) ? IntSize(size.width() / static_cast<int>(arrayLength), size.height()) : size;
        depthSwapchain = createDepthSwapchain(context, *depthFormat, depthSize, clearOnAccess, numImages, arrayLength, colorTextureType);
    }

    return XRLayerSwapchains { handle, WTF::move(colorSwapchain), WTF::move(depthSwapchain), arrayLength };
}

SwapchainFormats swapchainFormatsForLayerFormat(GCGLenum layerFormat)
{
    switch (layerFormat) {
    case GL::RGBA:
    case GL::RGBA8:
        return { GL::RGBA, GL::RGBA8 };

    case GL::RGB:
    case GL::RGB8:
        return { GL::RGB, GL::RGB8 };

    case GL::SRGB_EXT:
    case GL::SRGB8:
        return { GL::SRGB_EXT, GL::SRGB8 };

    case GL::SRGB_ALPHA_EXT:
    case GL::SRGB8_ALPHA8:
        return { GL::SRGB_ALPHA_EXT, GL::SRGB8_ALPHA8 };

    case GL::DEPTH_COMPONENT:
    case GL::DEPTH_COMPONENT24:
        return { GL::DEPTH_COMPONENT, GL::DEPTH_COMPONENT24 };

    case GL::DEPTH_STENCIL:
    case GL::DEPTH24_STENCIL8:
        return { GL::DEPTH_STENCIL, GL::DEPTH24_STENCIL8 };

    default:
        ASSERT_NOT_REACHED();
        return { 0, 0 };
    };
}

std::unique_ptr<WebXRWebGLSwapchain> XRWebGLLayerBacking::createDepthSwapchain(WebGLRenderingContextBase& context, GCGLenum depthFormat, IntSize size, bool clearOnAccess, size_t imageCount, uint32_t arrayLength, GCGLenum textureType)
{
    ASSERT(depthFormat);

    auto formatHasStencil = [](GCGLenum format) {
        switch (format) {
        case GL::DEPTH_STENCIL:
        case GL::DEPTH24_STENCIL8:
            return true;
        default:
            return false;
        };
    };

    auto formats = swapchainFormatsForLayerFormat(depthFormat);
    WebXRSwapchain::SwapchainTargets targets = { WebXRSwapchain::SwapchainTargetFlags::Depth };
    if (formatHasStencil(depthFormat))
        targets.add(WebXRSwapchain::SwapchainTargetFlags::Stencil);
    WebXRWebGLStaticImageSwapchain::StaticImageAttributes attributes = {
        .format = formats.format,
        .internalFormat = formats.internalFormat,
        .size = size,
        .clearOnAccess = clearOnAccess,
        .targets = targets,
        .imageCount = imageCount,
        .arrayLength = arrayLength,
        .textureType = textureType,
    };
    return WebXRWebGLStaticImageSwapchain::create(context, attributes);
}

bool XRWebGLLayerBacking::allColorTexturesAreBound() const
{
    return m_colorSwapchain->allTexturesAreBound();
}

void XRWebGLLayerBacking::clearTexturesIfNeeded(const IntRect& viewport, std::optional<uint32_t> slice)
{
    std::optional<GCGLint> glSlice;
    if (slice)
        glSlice = static_cast<GCGLint>(*slice);
    m_colorSwapchain->clearTextureIfNeeded(viewport, glSlice);
    if (m_depthSwapchain)
        m_depthSwapchain->clearTextureIfNeeded(viewport, glSlice);
}

} // namespace WebCore


#endif // ENABLE(WEBXR_LAYERS)
