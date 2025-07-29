/*
 * Copyright (C) 2025 Igalia, S.L.
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
#if USE(LIBEPOXY)
#define __GBM__ 1
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#endif

#include "XRDeviceLayer.h"
#include <WebCore/GLContext.h>
#include <WebCore/PlatformDisplay.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if ENABLE(WEBXR) && USE(OPENXR)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRLayer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRLayerProjection);

OpenXRLayer::OpenXRLayer(UniqueRef<OpenXRSwapchain>&& swapchain)
    : m_swapchain(WTFMove(swapchain))
{
}

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRLayer::exportOpenXRTexture(PlatformGLObject openxrTexture)
{
    // Texture must be bound to be exported.
    glBindTexture(GL_TEXTURE_2D, openxrTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_swapchain->width(), m_swapchain->height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    auto* glContext = WebCore::GLContext::current();
    if (!glContext) {
        RELEASE_LOG(XR, "No current GL context to export OpenXR texture");
        return std::nullopt;
    }

    auto& display = glContext->display();
    auto image = display.createEGLImage(glContext->platformContext(), EGL_GL_TEXTURE_2D, (EGLClientBuffer)(uint64_t)openxrTexture, { });

    auto releaseTextureOnError = makeScopeExit([&] {
        if (image)
            display.destroyEGLImage(image);
        m_swapchain->releaseImage();
    });

    if (!image) {
        RELEASE_LOG(XR, "Failed to create EGL image from OpenXR texture");
        return std::nullopt;
    }

    int fourcc, planeCount;
    uint64_t modifier;
    if (!eglExportDMABUFImageQueryMESA(display.eglDisplay(), image, &fourcc, &planeCount, &modifier)) {
        RELEASE_LOG(XR, "eglExportDMABUFImageQueryMESA failed");
        return std::nullopt;
    }

    Vector<int> fdsOut(planeCount);
    Vector<int> stridesOut(planeCount);
    Vector<int> offsetsOut(planeCount);
    if (!eglExportDMABUFImageMESA(display.eglDisplay(), image, fdsOut.mutableSpan().data(), stridesOut.mutableSpan().data(), offsetsOut.mutableSpan().data())) {
        RELEASE_LOG(XR, "eglExportDMABUFImageMESA failed");
        return std::nullopt;
    }

    display.destroyEGLImage(image);

    releaseTextureOnError.release();

    Vector<UnixFileDescriptor> fds = fdsOut.map([](int fd) {
        return UnixFileDescriptor(fd, UnixFileDescriptor::Adopt);
    });
    Vector<uint32_t> strides = stridesOut.map([](int stride) {
        return static_cast<uint32_t>(stride);
    });
    Vector<uint32_t> offsets = offsetsOut.map([](int offset) {
        return static_cast<uint32_t>(offset);
    });

    return PlatformXR::FrameData::ExternalTexture {
        .fds = WTFMove(fds),
        .strides = WTFMove(strides),
        .offsets = WTFMove(offsets),
        .fourcc = static_cast<uint32_t>(fourcc),
        .modifier = modifier,
    };
}

// OpenXRLayerProjection

std::unique_ptr<OpenXRLayerProjection> OpenXRLayerProjection::create(std::unique_ptr<OpenXRSwapchain>&& swapchain)
{
    return std::unique_ptr<OpenXRLayerProjection>(new OpenXRLayerProjection(makeUniqueRefFromNonNullUniquePtr(WTFMove(swapchain))));
}

OpenXRLayerProjection::OpenXRLayerProjection(UniqueRef<OpenXRSwapchain>&& swapchain)
    : OpenXRLayer(WTFMove(swapchain))
    , m_layerProjection(createOpenXRStruct<XrCompositionLayerProjection, XR_TYPE_COMPOSITION_LAYER_PROJECTION>())
{
}

std::optional<PlatformXR::FrameData::LayerData> OpenXRLayerProjection::startFrame()
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

    auto externalTexture = exportOpenXRTexture(*texture);
    if (!externalTexture)
        return std::nullopt;
    layerData.textureData->colorTexture = WTFMove(externalTexture.value());

    auto halfWidth = m_swapchain->width() / 2;
    layerData.layerSetup = {
        .physicalSize = { static_cast<uint16_t>(m_swapchain->width()), static_cast<uint16_t>(m_swapchain->height()) },
        .viewports = { },
        .foveationRateMapDesc = { }
    };
    layerData.layerSetup->viewports[0] = { 0, 0, halfWidth, m_swapchain->height() };
    layerData.layerSetup->viewports[1] = { halfWidth, 0, halfWidth, m_swapchain->height() };

    return layerData;
}

XrCompositionLayerBaseHeader* OpenXRLayerProjection::endFrame(const XRDeviceLayer& layer, XrSpace space, const Vector<XrView>& frameViews)
{
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

    return reinterpret_cast<XrCompositionLayerBaseHeader*>(&m_layerProjection);
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
