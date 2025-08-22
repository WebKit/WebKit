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
#include <WebCore/GLDisplay.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(GBM)
#include <WebCore/GBMDevice.h>
#include <WebCore/GBMVersioning.h>
#include <drm_fourcc.h>
#endif

#if ENABLE(WEBXR) && USE(OPENXR)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRLayer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRLayerProjection);

OpenXRLayer::OpenXRLayer(UniqueRef<OpenXRSwapchain>&& swapchain)
    : m_swapchain(WTFMove(swapchain))
{
}

OpenXRLayer::~OpenXRLayer()
{
    ASSERT(WebCore::GLContext::current());
#if USE(GBM)
    if (m_fbosForBlitting[0])
        glDeleteFramebuffers(2, m_fbosForBlitting);
    for (auto texture : m_exportedTexturesMap.values())
        glDeleteTextures(1, &texture);
#endif
}

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRLayer::exportOpenXRTextureDMABuf(WebCore::GLDisplay& display, WebCore::GLContext& context, PlatformGLObject openxrTexture)
{
    // Texture must be bound to be exported.
    glBindTexture(GL_TEXTURE_2D, openxrTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    auto image = display.createImage(context.platformContext(), EGL_GL_TEXTURE_2D, (EGLClientBuffer)(uint64_t)openxrTexture, { });

    auto releaseImageOnError = makeScopeExit([&] {
        if (image)
            display.destroyImage(image);
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

    display.destroyImage(image);

    releaseImageOnError.release();

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

#if USE(GBM)
void OpenXRLayer::setGBMDevice(RefPtr<WebCore::GBMDevice> gbmDevice)
{
    m_gbmDevice = gbmDevice;
}

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRLayer::exportOpenXRTextureGBM(WebCore::GLDisplay& display, PlatformGLObject openxrTexture)
{
    auto preferredDMABufFormat = m_swapchain->format() == GL_RGBA8 ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888;
    WebCore::GLDisplay::DMABufFormat format;
    const auto& supportedFormats = display.dmabufFormats();
    for (const auto& supportedFormat : supportedFormats) {
        if (supportedFormat.fourcc == preferredDMABufFormat) {
            format = supportedFormat;
            break;
        }
    }
    if (!format.fourcc) {
        RELEASE_LOG(XR, "OpenXR texture format not supported");
        return std::nullopt;
    }

    auto* buffer = gbm_bo_create_with_modifiers2(m_gbmDevice->device(), m_swapchain->width(), m_swapchain->height(), format.fourcc, format.modifiers.span().data(), format.modifiers.size(), GBM_BO_USE_RENDERING);
    if (!buffer)
        buffer = gbm_bo_create(m_gbmDevice->device(), m_swapchain->width(), m_swapchain->height(), format.fourcc, GBM_BO_USE_RENDERING);
    if (!buffer) {
        RELEASE_LOG(XR, "Failed to allocate GBM buffer for OpenXR texture");
        return std::nullopt;
    }

    Vector<UnixFileDescriptor> fds;
    Vector<uint32_t> offsets;
    Vector<uint32_t> strides;
    uint32_t fourcc = gbm_bo_get_format(buffer);
    uint64_t modifier = gbm_bo_get_modifier(buffer);
    int planeCount = gbm_bo_get_plane_count(buffer);

    Vector<EGLAttrib> attributes = {
        EGL_WIDTH, static_cast<EGLAttrib>(gbm_bo_get_width(buffer)),
        EGL_HEIGHT, static_cast<EGLAttrib>(gbm_bo_get_height(buffer)),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(fourcc),
    };

#define ADD_PLANE_ATTRIBUTES(planeIndex) { \
    fds.append(UnixFileDescriptor { gbm_bo_get_fd_for_plane(buffer, planeIndex), UnixFileDescriptor::Adopt }); \
    offsets.append(gbm_bo_get_offset(buffer, planeIndex)); \
    strides.append(gbm_bo_get_stride_for_plane(buffer, planeIndex)); \
    std::array<EGLAttrib, 6> planeAttributes { \
        EGL_DMA_BUF_PLANE##planeIndex##_FD_EXT, fds.last().value(), \
        EGL_DMA_BUF_PLANE##planeIndex##_OFFSET_EXT, static_cast<EGLAttrib>(offsets.last()), \
        EGL_DMA_BUF_PLANE##planeIndex##_PITCH_EXT, static_cast<EGLAttrib>(strides.last()) \
    }; \
    attributes.append(std::span<const EGLAttrib> { planeAttributes }); \
    if (modifier != DRM_FORMAT_MOD_INVALID) { \
        std::array<EGLAttrib, 4> modifierAttributes { \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_HI_EXT, static_cast<EGLAttrib>(modifier >> 32), \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_LO_EXT, static_cast<EGLAttrib>(modifier & 0xffffffff) \
        }; \
        attributes.append(std::span<const EGLAttrib> { modifierAttributes }); \
    } \
    }

    if (planeCount > 0)
        ADD_PLANE_ATTRIBUTES(0);
    if (planeCount > 1)
        ADD_PLANE_ATTRIBUTES(1);
    if (planeCount > 2)
        ADD_PLANE_ATTRIBUTES(2);
    if (planeCount > 3)
        ADD_PLANE_ATTRIBUTES(3);

#undef ADD_PLANE_ATTRIBS

    attributes.append(EGL_NONE);

    auto image = display.createImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    gbm_bo_destroy(buffer);

    if (!image) {
        RELEASE_LOG(XR, "Failed to create EGL image from OpenXR texture");
        return std::nullopt;
    }

    GLint boundTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    PlatformGLObject exportedTexture;
    glGenTextures(1, &exportedTexture);
    glBindTexture(GL_TEXTURE_2D, exportedTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    glBindTexture(GL_TEXTURE_2D, boundTexture);

    display.destroyImage(image);

    m_exportedTexturesMap.add(openxrTexture, exportedTexture);

    return PlatformXR::FrameData::ExternalTexture {
        .fds = WTFMove(fds),
        .strides = WTFMove(strides),
        .offsets = WTFMove(offsets),
        .fourcc = fourcc,
        .modifier = modifier
    };
}

void OpenXRLayer::blitTexture() const
{
    auto openxrTexture = m_swapchain->acquiredTexture();
    ASSERT(openxrTexture);

    auto exportedTexture = m_exportedTexturesMap.get(openxrTexture);
    ASSERT(exportedTexture);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbosForBlitting[0]);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, exportedTexture, 0);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbosForBlitting[1]);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, openxrTexture, 0);

    auto width = m_swapchain->width();
    auto height = m_swapchain->height();
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
#endif

std::optional<PlatformXR::FrameData::ExternalTexture> OpenXRLayer::exportOpenXRTexture(PlatformGLObject openxrTexture)
{
    auto* glContext = WebCore::GLContext::current();
    ASSERT(glContext);

    auto display = glContext->display();
    ASSERT(display);

    if (display->extensions().MESA_image_dma_buf_export)
        return exportOpenXRTextureDMABuf(*display, *glContext, openxrTexture);

#if USE(GBM)
    if (m_gbmDevice)
        return exportOpenXRTextureGBM(*display, openxrTexture);
#endif

    RELEASE_LOG(XR, "Failed to export OpenXR texture");
    return std::nullopt;
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
    if (!externalTexture) {
        m_swapchain->releaseImage();
        return std::nullopt;
    }
    layerData.textureData->colorTexture = WTFMove(externalTexture.value());

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

XrCompositionLayerBaseHeader* OpenXRLayerProjection::endFrame(const XRDeviceLayer& layer, XrSpace space, const Vector<XrView>& frameViews)
{
#if USE(GBM)
    if (m_gbmDevice) {
        if (!m_fbosForBlitting[0])
            glGenFramebuffers(2, m_fbosForBlitting);
        blitTexture();
    }
#endif
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
