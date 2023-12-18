/*
 * Copyright (C) 2021 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "OpenXRLayer.h"

#if ENABLE(WEBXR) && USE(OPENXR)

using namespace WebCore;

namespace PlatformXR {

// OpenXRLayerProjection

std::unique_ptr<OpenXRLayerProjection> OpenXRLayerProjection::create(XrInstance instance, XrSession session, uint32_t width, uint32_t height, int64_t format, uint32_t sampleCount)
{
    if (!width || !height || !sampleCount)
        return nullptr;

    auto info = createStructure<XrSwapchainCreateInfo, XR_TYPE_SWAPCHAIN_CREATE_INFO>();
    info.arraySize = 1;
    info.format = format;
    info.width = width;
    info.height = height;
    info.mipCount = 1;
    info.faceCount = 1;
    info.arraySize = 1;
    info.sampleCount = sampleCount;
    info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

    auto swapchain = OpenXRSwapchain::create(instance, session, info);
    if (!swapchain)
        return nullptr;

    return std::unique_ptr<OpenXRLayerProjection>(new OpenXRLayerProjection(makeUniqueRefFromNonNullUniquePtr(WTFMove(swapchain))));
}

OpenXRLayerProjection::OpenXRLayerProjection(UniqueRef<OpenXRSwapchain>&& swapchain)
    : m_swapchain(WTFMove(swapchain))
    , m_layerProjection(createStructure<XrCompositionLayerProjection, XR_TYPE_COMPOSITION_LAYER_PROJECTION>())
{
}

std::optional<FrameData::LayerData> OpenXRLayerProjection::startFrame()
{
    auto texture = m_swapchain->acquireImage();
    if (!texture)
        return std::nullopt;

    return FrameData::LayerData { *texture };
}

XrCompositionLayerBaseHeader* OpenXRLayerProjection::endFrame(const Device::Layer& layer, XrSpace space, const Vector<XrView>& frameViews)
{
    ASSERT(layer.views.size() == frameViews.size());

    auto viewCount = frameViews.size();
    m_projectionViews.fill(createStructure<XrCompositionLayerProjectionView, XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW>(), viewCount);
    m_layerProjection.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    for (uint32_t i = 0; i < viewCount; ++i) {
        m_projectionViews[i].pose = frameViews[i].pose;
        m_projectionViews[i].fov = frameViews[i].fov;
        m_projectionViews[i].subImage.swapchain = m_swapchain->swapchain();

        auto& viewport = layer.views[i].viewport;

        m_projectionViews[i].subImage.imageRect.offset = { viewport.x(), viewport.y() };
        m_projectionViews[i].subImage.imageRect.extent = { viewport.width(), viewport.height() };
    }

    m_layerProjection.space = space;
    m_layerProjection.viewCount = m_projectionViews.size();
    m_layerProjection.views = m_projectionViews.data();

    m_swapchain->releaseImage();

    return reinterpret_cast<XrCompositionLayerBaseHeader*>(&m_layerProjection);
}


} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
