/*
 * Copyright (C) 2025-2026 Igalia, S.L.
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

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRLayer.h"

#include "OpenXRGraphicsBinding.h"
#include <openxr/openxr.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRLayer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRLayerProjection);
WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRCompositionLayer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRQuadLayer);
#if defined(XR_KHR_composition_layer_equirect2)
WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXREquirectLayer);
#endif
#if defined(XR_KHR_composition_layer_cube)
WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRCubeLayer);
#endif

OpenXRLayer::OpenXRLayer(UniqueRef<OpenXRSwapchain>&& swapchain)
    : m_swapchain(WTF::move(swapchain))
{
}

OpenXRLayer::~OpenXRLayer() = default;

// OpenXRLayerProjection

std::unique_ptr<OpenXRLayerProjection> OpenXRLayerProjection::create(std::unique_ptr<OpenXRSwapchain>&& swapchain)
{
    return std::unique_ptr<OpenXRLayerProjection>(new OpenXRLayerProjection(makeUniqueRefFromNonNullUniquePtr(WTF::move(swapchain))));
}

OpenXRLayerProjection::OpenXRLayerProjection(UniqueRef<OpenXRSwapchain>&& swapchain)
    : OpenXRLayer(WTF::move(swapchain))
    , m_layerProjection(createOpenXRStruct<XrCompositionLayerProjection, XR_TYPE_COMPOSITION_LAYER_PROJECTION>())
{
}

std::optional<PlatformXR::FrameData::LayerData> OpenXRLayerProjection::startFrame(OpenXRGraphicsBinding& graphicsBinding)
{
    auto texture = m_swapchain->acquireImage();
    if (!texture)
        return std::nullopt;

    auto addResult = m_exportedTextures.add(*texture, m_nextReusableTextureIndex);
    bool needsExport = addResult.isNewEntry;

    PlatformXR::FrameData::LayerData layerData;
    layerData.renderingFrameIndex = m_renderingFrameIndex++;
    layerData.textureData = {
        .reusableTextureIndex = addResult.iterator->value,
        .colorTexture = { },
        .depthStencilBuffer = { },
    };

    if (!needsExport)
        return layerData;
    m_nextReusableTextureIndex++;

    auto externalTexture = graphicsBinding.exportTexture(*texture, m_swapchain, OpenXRGraphicsBinding::TextureType::Texture2D, m_swapchain->width(), m_swapchain->height());
    if (!externalTexture)
        return std::nullopt;

    layerData.textureData->colorTexture = WTF::move(externalTexture.value());

    auto halfWidth = m_swapchain->width() / 2;
    layerData.layerSetup = {
        .physicalSize = { { { static_cast<uint16_t>(m_swapchain->width()), static_cast<uint16_t>(m_swapchain->height()) } } },
        .viewports = { },
        .foveationRateMapDesc = { }
    };
    layerData.layerSetup->viewports[0] = { 0, 0, halfWidth, m_swapchain->height() };
    layerData.layerSetup->viewports[1] = { halfWidth, 0, halfWidth, m_swapchain->height() };

    return layerData;
}

Vector<XrCompositionLayerBaseHeader*> OpenXRLayerProjection::endFrame(OpenXRGraphicsBinding& graphicsBinding, const PlatformXR::DeviceLayer& layer, XrSpace space, const Vector<XrView>& frameViews)
{
    ASSERT(m_swapchain->acquiredTexture());

    graphicsBinding.commitFrame(m_swapchain->acquiredTexture(), m_swapchain, OpenXRGraphicsBinding::TextureType::Texture2D, { m_swapchain->acquiredTexture() });
    auto viewCount = frameViews.size();
    m_projectionViews.fill(createOpenXRStruct<XrCompositionLayerProjectionView, XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW>(), viewCount);
    for (uint32_t i = 0; i < viewCount; ++i) {
        m_projectionViews[i].pose = frameViews[i].pose;
        m_projectionViews[i].fov = frameViews[i].fov;
        m_projectionViews[i].subImage.swapchain = m_swapchain->swapchain();

        auto& viewport = layer.views[i].viewport;

        m_projectionViews[i].subImage.imageRect.offset = { viewport.x(), viewport.y() };
        m_projectionViews[i].subImage.imageRect.extent = { viewport.width(), viewport.height() };
    }

    m_layerProjection.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    m_layerProjection.space = space;
    m_layerProjection.viewCount = m_projectionViews.size();
    m_layerProjection.views = m_projectionViews.span().data();

    m_swapchain->releaseImage();

    return { reinterpret_cast<XrCompositionLayerBaseHeader*>(&m_layerProjection) };
}

#if ENABLE(WEBXR_LAYERS)

OpenXRCompositionLayer::OpenXRCompositionLayer(UniqueRef<OpenXRSwapchain>&& swapchain, PlatformXR::LayerLayout layout)
    : OpenXRLayer(WTF::move(swapchain))
    , m_layout(layout)
{
}

std::unique_ptr<OpenXRQuadLayer> OpenXRQuadLayer::create(std::unique_ptr<OpenXRSwapchain>&& swapchain, PlatformXR::LayerLayout layout)
{
    return std::unique_ptr<OpenXRQuadLayer>(new OpenXRQuadLayer(makeUniqueRefFromNonNullUniquePtr(WTF::move(swapchain)), layout));
}

OpenXRQuadLayer::OpenXRQuadLayer(UniqueRef<OpenXRSwapchain>&& swapchain, PlatformXR::LayerLayout layout)
    : OpenXRCompositionLayer(WTF::move(swapchain), layout)
{
    m_layers.resize(layout == PlatformXR::LayerLayout::Mono ? 1 : 2);
    m_layers.fill(createOpenXRStruct<XrCompositionLayerQuad, XR_TYPE_COMPOSITION_LAYER_QUAD>());
    int xOffset = 0;
    int yOffset = 0;
    int subImageWidth = layout == PlatformXR::LayerLayout::StereoLeftRight ? m_swapchain->width() / 2 : m_swapchain->width();
    int subImageHeight = layout == PlatformXR::LayerLayout::StereoTopBottom ? m_swapchain->height() / 2 : m_swapchain->height();
    XrExtent2Di subImageExtent = { subImageWidth, subImageHeight };
    for (auto& xrLayer : m_layers) {
        xrLayer.subImage.swapchain = m_swapchain->swapchain();
        xrLayer.subImage.imageRect.offset = { xOffset, yOffset };
        xrLayer.subImage.imageRect.extent = subImageExtent;
        xrLayer.subImage.imageArrayIndex = 0;

        xOffset += layout == PlatformXR::LayerLayout::StereoLeftRight ? subImageWidth : 0;
        yOffset += layout == PlatformXR::LayerLayout::StereoTopBottom ? subImageHeight : 0;
    }
}

std::optional<PlatformXR::FrameData::LayerData> OpenXRQuadLayer::startFrame(OpenXRGraphicsBinding& graphicsBinding)
{
    auto texture = m_swapchain->acquireImage();
    if (!texture)
        return std::nullopt;

    auto addResult = m_exportedTextures.add(*texture, m_nextReusableTextureIndex);
    bool needsExport = addResult.isNewEntry;

    PlatformXR::FrameData::LayerData layerData;
    layerData.renderingFrameIndex = m_renderingFrameIndex++;
    layerData.textureData = {
        .reusableTextureIndex = addResult.iterator->value,
        .colorTexture = { },
        .depthStencilBuffer = { },
    };

    if (!needsExport)
        return layerData;
    m_nextReusableTextureIndex++;

    auto externalTexture = graphicsBinding.exportTexture(*texture, m_swapchain, OpenXRGraphicsBinding::TextureType::Texture2D, m_swapchain->width(), m_swapchain->height());
    if (!externalTexture)
        return std::nullopt;

    layerData.textureData->colorTexture = WTF::move(externalTexture.value());

    layerData.layerSetup = {
        .physicalSize = { { { static_cast<uint16_t>(m_swapchain->width()), static_cast<uint16_t>(m_swapchain->height()) } } },
        .viewports = { },
        .foveationRateMapDesc = { }
    };

    return layerData;
}

Vector<XrCompositionLayerBaseHeader*> OpenXRQuadLayer::endFrame(OpenXRGraphicsBinding& graphicsBinding, const PlatformXR::DeviceLayer& layer, XrSpace space, const Vector<XrView>& frameViews)
{
    ASSERT(m_swapchain->acquiredTexture());

    graphicsBinding.commitFrame(m_swapchain->acquiredTexture(), m_swapchain, OpenXRGraphicsBinding::TextureType::Texture2D, { m_swapchain->acquiredTexture() });

    auto eyeVisibility = [](bool isLeftEye, bool isMonoPresentation) {
        if (isMonoPresentation)
            return XR_EYE_VISIBILITY_BOTH;
        return isLeftEye ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
    };
    auto flags = layer.blendTextureSourceAlpha ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;

    auto isLeftEyeIndex = [this](int layerIndex) {
        switch (m_layout) {
        case PlatformXR::LayerLayout::Mono:
        case PlatformXR::LayerLayout::StereoLeftRight:
            return !layerIndex;
        case PlatformXR::LayerLayout::StereoTopBottom:
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES) || defined(XR_USE_GRAPHICS_API_OPENGL)
            // Origin of coordinates in OpenGL is bottom left, so the origin is on the half for the right side.
            return layerIndex == 1;
#elif defined(XR_USE_GRAPHICS_API_VULKAN)
            // Origin of coordinates in Vulkan is top left, so the origin is on the half for the left side.
            return !layerIndex;
#endif
        default:
            ASSERT_NOT_REACHED_WITH_MESSAGE("Unrecognized layout for quad layer");
            return false;
        };
    };

    const auto numLayers = m_layers.size();
    Vector<XrCompositionLayerBaseHeader*> layerHeaders;
    layerHeaders.reserveCapacity(numLayers);

    bool isMonoPresentation = layer.forceMonoPresentation || m_layout == PlatformXR::LayerLayout::Mono;
    for (size_t i = 0; i < numLayers; ++i) {
        // WebXR requires right eye to display the left eye image in mono presentation mode. No need to pass more than one header.
        if (isMonoPresentation && !isLeftEyeIndex(i))
            continue;

        auto& xrLayer = m_layers[i];
        xrLayer.layerFlags = flags;
        xrLayer.eyeVisibility = eyeVisibility(isLeftEyeIndex(i), isMonoPresentation);
        xrLayer.space = space;

        ASSERT(layer.quadLayerData);
        auto pose = layer.quadLayerData->poseInLocalSpace;
        xrLayer.pose.position = { pose.position.x(), pose.position.y(), pose.position.z() };
        xrLayer.pose.orientation = { pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w };
        xrLayer.size = { layer.quadLayerData->worldSize.width(), layer.quadLayerData->worldSize.height() };

        layerHeaders.append(reinterpret_cast<XrCompositionLayerBaseHeader*>(&xrLayer));
    }

    m_swapchain->releaseImage();

    return layerHeaders;
}

#if defined(XR_KHR_composition_layer_equirect2)

std::unique_ptr<OpenXREquirectLayer> OpenXREquirectLayer::create(std::unique_ptr<OpenXRSwapchain>&& swapchain, PlatformXR::LayerLayout layout)
{
    return std::unique_ptr<OpenXREquirectLayer>(new OpenXREquirectLayer(makeUniqueRefFromNonNullUniquePtr(WTF::move(swapchain)), layout));
}

OpenXREquirectLayer::OpenXREquirectLayer(UniqueRef<OpenXRSwapchain>&& swapchain, PlatformXR::LayerLayout layout)
    : OpenXRCompositionLayer(WTF::move(swapchain), layout)
{
    m_layers.resize(layout == PlatformXR::LayerLayout::Mono ? 1 : 2);
    m_layers.fill(createOpenXRStruct<XrCompositionLayerEquirect2KHR, XR_TYPE_COMPOSITION_LAYER_EQUIRECT2_KHR>());
    int xOffset = 0;
    int yOffset = 0;
    int subImageWidth = layout == PlatformXR::LayerLayout::StereoLeftRight ? m_swapchain->width() / 2 : m_swapchain->width();
    int subImageHeight = layout == PlatformXR::LayerLayout::StereoTopBottom ? m_swapchain->height() / 2 : m_swapchain->height();
    XrExtent2Di subImageExtent = { subImageWidth, subImageHeight };
    for (auto& xrLayer : m_layers) {
        xrLayer.subImage.swapchain = m_swapchain->swapchain();
        xrLayer.subImage.imageRect.offset = { xOffset, yOffset };
        xrLayer.subImage.imageRect.extent = subImageExtent;
        xrLayer.subImage.imageArrayIndex = 0;

        xOffset += layout == PlatformXR::LayerLayout::StereoLeftRight ? subImageWidth : 0;
        yOffset += layout == PlatformXR::LayerLayout::StereoTopBottom ? subImageHeight : 0;
    }
}

std::optional<PlatformXR::FrameData::LayerData> OpenXREquirectLayer::startFrame(OpenXRGraphicsBinding& graphicsBinding)
{
    auto texture = m_swapchain->acquireImage();
    if (!texture)
        return std::nullopt;

    auto addResult = m_exportedTextures.add(*texture, m_nextReusableTextureIndex);
    bool needsExport = addResult.isNewEntry;

    PlatformXR::FrameData::LayerData layerData;
    layerData.renderingFrameIndex = m_renderingFrameIndex++;
    layerData.textureData = {
        .reusableTextureIndex = addResult.iterator->value,
        .colorTexture = { },
        .depthStencilBuffer = { },
    };

    if (!needsExport)
        return layerData;
    m_nextReusableTextureIndex++;

    auto externalTexture = graphicsBinding.exportTexture(*texture, m_swapchain, OpenXRGraphicsBinding::TextureType::Texture2D, m_swapchain->width(), m_swapchain->height());
    if (!externalTexture)
        return std::nullopt;

    layerData.textureData->colorTexture = WTF::move(externalTexture.value());

    layerData.layerSetup = {
        .physicalSize = { { { static_cast<uint16_t>(m_swapchain->width()), static_cast<uint16_t>(m_swapchain->height()) } } },
        .viewports = { },
        .foveationRateMapDesc = { }
    };

    return layerData;
}

Vector<XrCompositionLayerBaseHeader*> OpenXREquirectLayer::endFrame(OpenXRGraphicsBinding& graphicsBinding, const PlatformXR::DeviceLayer& layer, XrSpace space, const Vector<XrView>& frameViews)
{
    ASSERT(m_swapchain->acquiredTexture());

    graphicsBinding.commitFrame(m_swapchain->acquiredTexture(), m_swapchain, OpenXRGraphicsBinding::TextureType::Texture2D, { m_swapchain->acquiredTexture() });

    auto eyeVisibility = [](bool isLeftEye, bool isMonoPresentation) {
        if (isMonoPresentation)
            return XR_EYE_VISIBILITY_BOTH;
        return isLeftEye ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
    };
    auto flags = layer.blendTextureSourceAlpha ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;

    auto isLeftEyeIndex = [layout = m_layout](int layerIndex) {
        switch (layout) {
        case PlatformXR::LayerLayout::Mono:
        case PlatformXR::LayerLayout::StereoLeftRight:
            return !layerIndex;
        case PlatformXR::LayerLayout::StereoTopBottom:
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES) || defined(XR_USE_GRAPHICS_API_OPENGL)
            return layerIndex == 1;
#elif defined(XR_USE_GRAPHICS_API_VULKAN)
            return !layerIndex;
#endif
        default:
            ASSERT_NOT_REACHED_WITH_MESSAGE("Unrecognized layout for equirect layer");
            return false;
        };
    };

    const auto numLayers = m_layers.size();
    Vector<XrCompositionLayerBaseHeader*> layerHeaders;
    layerHeaders.reserveCapacity(numLayers);

    bool isMonoPresentation = layer.forceMonoPresentation || m_layout == PlatformXR::LayerLayout::Mono;
    for (size_t i = 0; i < numLayers; ++i) {
        if (isMonoPresentation && !isLeftEyeIndex(i))
            continue;

        auto& xrLayer = m_layers[i];
        xrLayer.layerFlags = flags;
        xrLayer.eyeVisibility = eyeVisibility(isLeftEyeIndex(i), isMonoPresentation);
        xrLayer.space = space;

        ASSERT(layer.equirectLayerData);
        auto& equirectData = *layer.equirectLayerData;
        xrLayer.pose.position = { equirectData.poseInLocalSpace.position.x(), equirectData.poseInLocalSpace.position.y(), equirectData.poseInLocalSpace.position.z() };
        xrLayer.pose.orientation = { equirectData.poseInLocalSpace.orientation.x, equirectData.poseInLocalSpace.orientation.y, equirectData.poseInLocalSpace.orientation.z, equirectData.poseInLocalSpace.orientation.w };
        xrLayer.radius = equirectData.radius;
        xrLayer.centralHorizontalAngle = equirectData.centralHorizontalAngle;
        xrLayer.upperVerticalAngle = equirectData.upperVerticalAngle;
        xrLayer.lowerVerticalAngle = equirectData.lowerVerticalAngle;

        layerHeaders.append(reinterpret_cast<XrCompositionLayerBaseHeader*>(&xrLayer));
    }

    m_swapchain->releaseImage();

    return layerHeaders;
}

#endif

#if defined(XR_KHR_composition_layer_cylinder)

std::unique_ptr<OpenXRCylinderLayer> OpenXRCylinderLayer::create(std::unique_ptr<OpenXRSwapchain>&& swapchain, PlatformXR::LayerLayout layout)
{
    return std::unique_ptr<OpenXRCylinderLayer>(new OpenXRCylinderLayer(makeUniqueRefFromNonNullUniquePtr(WTF::move(swapchain)), layout));
}

OpenXRCylinderLayer::OpenXRCylinderLayer(UniqueRef<OpenXRSwapchain>&& swapchain, PlatformXR::LayerLayout layout)
    : OpenXRCompositionLayer(WTF::move(swapchain), layout)
{
    m_layers.resize(layout == PlatformXR::LayerLayout::Mono ? 1 : 2);
    m_layers.fill(createOpenXRStruct<XrCompositionLayerCylinderKHR, XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR>());
    int xOffset = 0;
    int yOffset = 0;
    int subImageWidth = layout == PlatformXR::LayerLayout::StereoLeftRight ? m_swapchain->width() / 2 : m_swapchain->width();
    int subImageHeight = layout == PlatformXR::LayerLayout::StereoTopBottom ? m_swapchain->height() / 2 : m_swapchain->height();
    XrExtent2Di subImageExtent = { subImageWidth, subImageHeight };
    for (auto& xrLayer : m_layers) {
        xrLayer.subImage.swapchain = m_swapchain->swapchain();
        xrLayer.subImage.imageRect.offset = { xOffset, yOffset };
        xrLayer.subImage.imageRect.extent = subImageExtent;
        xrLayer.subImage.imageArrayIndex = 0;

        xOffset += layout == PlatformXR::LayerLayout::StereoLeftRight ? subImageWidth : 0;
        yOffset += layout == PlatformXR::LayerLayout::StereoTopBottom ? subImageHeight : 0;
    }
}

std::optional<PlatformXR::FrameData::LayerData> OpenXRCylinderLayer::startFrame(OpenXRGraphicsBinding& graphicsBinding)
{
    auto texture = m_swapchain->acquireImage();
    if (!texture)
        return std::nullopt;

    auto addResult = m_exportedTextures.add(*texture, m_nextReusableTextureIndex);
    bool needsExport = addResult.isNewEntry;

    PlatformXR::FrameData::LayerData layerData;
    layerData.renderingFrameIndex = m_renderingFrameIndex++;
    layerData.textureData = {
        .reusableTextureIndex = addResult.iterator->value,
        .colorTexture = { },
        .depthStencilBuffer = { },
    };

    if (!needsExport)
        return layerData;
    m_nextReusableTextureIndex++;

    auto externalTexture = graphicsBinding.exportTexture(*texture, m_swapchain, OpenXRGraphicsBinding::TextureType::Texture2D, m_swapchain->width(), m_swapchain->height());
    if (!externalTexture)
        return std::nullopt;

    layerData.textureData->colorTexture = WTF::move(externalTexture.value());

    layerData.layerSetup = {
        .physicalSize = { { { static_cast<uint16_t>(m_swapchain->width()), static_cast<uint16_t>(m_swapchain->height()) } } },
        .viewports = { },
        .foveationRateMapDesc = { }
    };

    return layerData;
}

Vector<XrCompositionLayerBaseHeader*> OpenXRCylinderLayer::endFrame(OpenXRGraphicsBinding& graphicsBinding, const PlatformXR::DeviceLayer& layer, XrSpace space, const Vector<XrView>& frameViews)
{
    ASSERT(m_swapchain->acquiredTexture());

    graphicsBinding.commitFrame(m_swapchain->acquiredTexture(), m_swapchain, OpenXRGraphicsBinding::TextureType::Texture2D, { m_swapchain->acquiredTexture() });

    auto eyeVisibility = [](bool isLeftEye, bool isMonoPresentation) {
        if (isMonoPresentation)
            return XR_EYE_VISIBILITY_BOTH;
        return isLeftEye ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
    };
    auto flags = layer.blendTextureSourceAlpha ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;

    auto isLeftEyeIndex = [layout = m_layout](int layerIndex) {
        switch (layout) {
        case PlatformXR::LayerLayout::Mono:
        case PlatformXR::LayerLayout::StereoLeftRight:
            return !layerIndex;
        case PlatformXR::LayerLayout::StereoTopBottom:
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES) || defined(XR_USE_GRAPHICS_API_OPENGL)
            return layerIndex == 1;
#elif defined(XR_USE_GRAPHICS_API_VULKAN)
            return !layerIndex;
#endif
        default:
            ASSERT_NOT_REACHED_WITH_MESSAGE("Unrecognized layout for cylinder layer");
            return false;
        };
    };

    const auto numLayers = m_layers.size();
    Vector<XrCompositionLayerBaseHeader*> layerHeaders;
    layerHeaders.reserveCapacity(numLayers);

    bool isMonoPresentation = layer.forceMonoPresentation || m_layout == PlatformXR::LayerLayout::Mono;
    for (size_t i = 0; i < numLayers; ++i) {
        if (isMonoPresentation && !isLeftEyeIndex(i))
            continue;

        auto& xrLayer = m_layers[i];
        xrLayer.layerFlags = flags;
        xrLayer.eyeVisibility = eyeVisibility(isLeftEyeIndex(i), isMonoPresentation);
        xrLayer.space = space;

        ASSERT(layer.cylinderLayerData);
        auto& cylinderData = *layer.cylinderLayerData;
        xrLayer.pose.position = { cylinderData.poseInLocalSpace.position.x(), cylinderData.poseInLocalSpace.position.y(), cylinderData.poseInLocalSpace.position.z() };
        xrLayer.pose.orientation = { cylinderData.poseInLocalSpace.orientation.x, cylinderData.poseInLocalSpace.orientation.y, cylinderData.poseInLocalSpace.orientation.z, cylinderData.poseInLocalSpace.orientation.w };
        xrLayer.radius = cylinderData.radius;
        xrLayer.centralAngle = cylinderData.centralAngle;
        xrLayer.aspectRatio = cylinderData.aspectRatio;

        layerHeaders.append(reinterpret_cast<XrCompositionLayerBaseHeader*>(&xrLayer));
    }

    m_swapchain->releaseImage();

    return layerHeaders;
}

#endif

#if defined(XR_KHR_composition_layer_cube)

std::unique_ptr<OpenXRCubeLayer> OpenXRCubeLayer::create(std::unique_ptr<OpenXRSwapchain>&& swapchain, std::unique_ptr<OpenXRSwapchain>&& rightSwapchain, PlatformXR::LayerLayout layout)
{
    return std::unique_ptr<OpenXRCubeLayer>(new OpenXRCubeLayer(makeUniqueRefFromNonNullUniquePtr(WTF::move(swapchain)), WTF::move(rightSwapchain), layout));
}

OpenXRCubeLayer::OpenXRCubeLayer(UniqueRef<OpenXRSwapchain>&& swapchain, std::unique_ptr<OpenXRSwapchain>&& rightSwapchain, PlatformXR::LayerLayout layout)
    : OpenXRCompositionLayer(WTF::move(swapchain), layout)
    , m_rightSwapchain(WTF::move(rightSwapchain))
{
    m_layers.resize(cubeCount());
    m_layers.fill(createOpenXRStruct<XrCompositionLayerCubeKHR, XR_TYPE_COMPOSITION_LAYER_CUBE_KHR>());
    for (uint32_t cube = 0; cube < cubeCount(); ++cube) {
        m_layers[cube].swapchain = swapchainForCube(cube).swapchain();
        m_layers[cube].imageArrayIndex = 0;
        m_layers[cube].eyeVisibility = cubeCount() == 1 ? XR_EYE_VISIBILITY_BOTH : (!cube ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT);
    }
}

std::optional<PlatformXR::FrameData::LayerData> OpenXRCubeLayer::startFrame(OpenXRGraphicsBinding& graphicsBinding)
{
    auto texture = m_swapchain->acquireImage();
    if (!texture)
        return std::nullopt;

    auto releaseSwapchainImagesOnError = makeScopeExit([&] {
        m_swapchain->releaseImage();
        if (m_rightSwapchain)
            m_rightSwapchain->releaseImage();
    });

    if (m_rightSwapchain && !m_rightSwapchain->acquireImage())
        return std::nullopt;

    auto addResult = m_exportedTextures.add(*texture, m_nextReusableTextureIndex);
    bool needsExport = addResult.isNewEntry;

    PlatformXR::FrameData::LayerData layerData;
    layerData.renderingFrameIndex = m_renderingFrameIndex++;
    layerData.textureData = {
        .reusableTextureIndex = addResult.iterator->value,
        .colorTexture = { },
        .depthStencilBuffer = { },
    };

    if (!needsExport) {
        releaseSwapchainImagesOnError.release();
        return layerData;
    }
    m_nextReusableTextureIndex++;

    // The cube faces are laid out side by side (cubeCount * faceCount squares) in a single 2D buffer.
    uint32_t faceSize = m_swapchain->width();
    uint32_t sideBySideWidth = faceCount * cubeCount() * faceSize;

    auto externalTexture = graphicsBinding.exportTexture(*texture, m_swapchain, OpenXRGraphicsBinding::TextureType::Cubemap, sideBySideWidth, faceSize);
    if (!externalTexture)
        return std::nullopt;

    layerData.textureData->colorTexture = WTF::move(externalTexture.value());
    layerData.layerSetup = {
        .physicalSize = { { { static_cast<uint16_t>(sideBySideWidth), static_cast<uint16_t>(faceSize) } } },
        .viewports = { },
        .foveationRateMapDesc = { }
    };

    releaseSwapchainImagesOnError.release();
    return layerData;
}

Vector<XrCompositionLayerBaseHeader*> OpenXRCubeLayer::endFrame(OpenXRGraphicsBinding& graphicsBinding, const PlatformXR::DeviceLayer& layer, XrSpace space, const Vector<XrView>&)
{
    ASSERT(m_swapchain->acquiredTexture());

    Vector<uint64_t> cubeImages;
    cubeImages.reserveCapacity(cubeCount());
    for (uint32_t cube = 0; cube < cubeCount(); ++cube)
        cubeImages.append(swapchainForCube(cube).acquiredTexture());
    graphicsBinding.commitFrame(m_swapchain->acquiredTexture(), m_swapchain, OpenXRGraphicsBinding::TextureType::Cubemap, cubeImages);

    auto flags = layer.blendTextureSourceAlpha ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
    XrQuaternionf orientation { 0, 0, 0, 1 };
    ASSERT(layer.cubeLayerData);
    if (layer.cubeLayerData) {
        auto& cubeOrientation = layer.cubeLayerData->orientation;
        orientation = { cubeOrientation.x, cubeOrientation.y, cubeOrientation.z, cubeOrientation.w };
    }

    Vector<XrCompositionLayerBaseHeader*> layerHeaders;
    layerHeaders.reserveCapacity(m_layers.size());
    for (auto& xrLayer : m_layers) {
        xrLayer.layerFlags = flags;
        xrLayer.space = space;
        xrLayer.orientation = orientation;
        layerHeaders.append(reinterpret_cast<XrCompositionLayerBaseHeader*>(&xrLayer));
    }

    m_swapchain->releaseImage();
    if (m_rightSwapchain)
        m_rightSwapchain->releaseImage();

    return layerHeaders;
}

#endif

#endif

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
