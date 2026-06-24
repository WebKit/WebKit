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

#pragma once

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRSwapchain.h"
#include "OpenXRUtils.h"

#include <WebCore/PlatformXR.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace WebCore {
class GBMDevice;
class GLContext;
class GLDisplay;
}

namespace WebKit {

class OpenXRLayer {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRLayer);
    WTF_MAKE_NONCOPYABLE(OpenXRLayer);
public:
    virtual ~OpenXRLayer();

    virtual std::optional<PlatformXR::FrameData::LayerData> startFrame() = 0;
    virtual Vector<XrCompositionLayerBaseHeader*> endFrame(const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) = 0;

#if USE(GBM)
    void setGBMDevice(RefPtr<WebCore::GBMDevice>);
#endif

protected:
    OpenXRLayer(UniqueRef<OpenXRSwapchain>&&);
#if OS(ANDROID)
    std::optional<PlatformXR::FrameData::ExternalTexture> exportOpenXRTextureAndroid(WebCore::GLDisplay&, PlatformGLObject, uint32_t width, uint32_t height);
    void blitTexture() const;
    inline bool needsBlitTexture() const { return true; }
#else
    std::optional<PlatformXR::FrameData::ExternalTexture> exportOpenXRTextureDMABuf(WebCore::GLDisplay&, WebCore::GLContext&, PlatformGLObject);
#endif
#if USE(GBM)
    std::optional<PlatformXR::FrameData::ExternalTexture> exportOpenXRTextureGBM(WebCore::GLDisplay&, PlatformGLObject, uint32_t width, uint32_t height);
    void blitTexture() const;
    inline bool needsBlitTexture() const { return m_gbmDevice; }
#endif
    std::optional<PlatformXR::FrameData::ExternalTexture> exportOpenXRTexture(PlatformGLObject, uint32_t width, uint32_t height);

    UniqueRef<OpenXRSwapchain> m_swapchain;

    uint64_t m_renderingFrameIndex { 0 };
    using ReusableTextureIndex = uint64_t;
    HashMap<PlatformGLObject, ReusableTextureIndex> m_exportedTextures;
    ReusableTextureIndex m_nextReusableTextureIndex { 0 };

#if USE(GBM) || OS(ANDROID)
    HashMap<PlatformGLObject, PlatformGLObject> m_exportedTexturesMap;
    std::array<PlatformGLObject, 2> m_fbosForBlitting { 0, 0 };
#endif
#if USE(GBM)
    RefPtr<WebCore::GBMDevice> m_gbmDevice;
#endif
};

class OpenXRLayerProjection final: public OpenXRLayer  {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRLayerProjection);
    WTF_MAKE_NONCOPYABLE(OpenXRLayerProjection);
public:
    static std::unique_ptr<OpenXRLayerProjection> create(std::unique_ptr<OpenXRSwapchain>&&);
private:
    explicit OpenXRLayerProjection(UniqueRef<OpenXRSwapchain>&&);

    std::optional<PlatformXR::FrameData::LayerData> startFrame() override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

    XrCompositionLayerProjection m_layerProjection;
    Vector<XrCompositionLayerProjectionView> m_projectionViews;
};

#if ENABLE(WEBXR_LAYERS)

class OpenXRCompositionLayer : public OpenXRLayer {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRCompositionLayer);
    WTF_MAKE_NONCOPYABLE(OpenXRCompositionLayer);
public:
    std::optional<PlatformXR::FrameData::LayerData> startFrame() = 0;
    Vector<XrCompositionLayerBaseHeader*> endFrame(const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) = 0;

protected:
    OpenXRCompositionLayer(UniqueRef<OpenXRSwapchain>&&, PlatformXR::LayerLayout);

    PlatformXR::LayerLayout m_layout;
};

class OpenXRQuadLayer final : public OpenXRCompositionLayer {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRQuadLayer);
    WTF_MAKE_NONCOPYABLE(OpenXRQuadLayer);
public:
    static std::unique_ptr<OpenXRQuadLayer> create(std::unique_ptr<OpenXRSwapchain>&&, PlatformXR::LayerLayout);

    std::optional<PlatformXR::FrameData::LayerData> startFrame() override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

private:
    explicit OpenXRQuadLayer(UniqueRef<OpenXRSwapchain>&&, PlatformXR::LayerLayout);

    Vector<XrCompositionLayerQuad> m_layers;
};

#if defined(XR_KHR_composition_layer_equirect2)
class OpenXREquirectLayer final : public OpenXRCompositionLayer {
    WTF_MAKE_TZONE_ALLOCATED(OpenXREquirectLayer);
    WTF_MAKE_NONCOPYABLE(OpenXREquirectLayer);
public:
    static std::unique_ptr<OpenXREquirectLayer> create(std::unique_ptr<OpenXRSwapchain>&&, PlatformXR::LayerLayout);

    std::optional<PlatformXR::FrameData::LayerData> startFrame() override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

private:
    explicit OpenXREquirectLayer(UniqueRef<OpenXRSwapchain>&&, PlatformXR::LayerLayout);

    Vector<XrCompositionLayerEquirect2KHR> m_layers;
};
#endif

#if defined(XR_KHR_composition_layer_cylinder)
class OpenXRCylinderLayer final : public OpenXRCompositionLayer {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRCylinderLayer);
    WTF_MAKE_NONCOPYABLE(OpenXRCylinderLayer);
public:
    static std::unique_ptr<OpenXRCylinderLayer> create(std::unique_ptr<OpenXRSwapchain>&&, PlatformXR::LayerLayout);

    std::optional<PlatformXR::FrameData::LayerData> startFrame() override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

private:
    explicit OpenXRCylinderLayer(UniqueRef<OpenXRSwapchain>&&, PlatformXR::LayerLayout);

    Vector<XrCompositionLayerCylinderKHR> m_layers;
};
#endif

#if defined(XR_KHR_composition_layer_cube)
// The OpenXR cube swapchain is a cubemap (faceCount=6) that cannot be shared cross-process as a 2D DMABuf,
// so the WebProcess renders into a side-by-side 2D buffer (cubeCount*6 faces laid out horizontally) which
// this layer reconstructs into the cubemap swapchain(s) each frame. Stereo uses one cube swapchain per eye.
class OpenXRCubeLayer final : public OpenXRCompositionLayer {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRCubeLayer);
    WTF_MAKE_NONCOPYABLE(OpenXRCubeLayer);
public:
    static std::unique_ptr<OpenXRCubeLayer> create(std::unique_ptr<OpenXRSwapchain>&&, std::unique_ptr<OpenXRSwapchain>&& rightSwapchain, PlatformXR::LayerLayout);
    ~OpenXRCubeLayer();

    std::optional<PlatformXR::FrameData::LayerData> startFrame() override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

private:
    OpenXRCubeLayer(UniqueRef<OpenXRSwapchain>&&, std::unique_ptr<OpenXRSwapchain>&& rightSwapchain, PlatformXR::LayerLayout);

    void reconstructCubeFaces();
    uint32_t cubeCount() const { return m_layout == PlatformXR::LayerLayout::Mono ? 1 : 2; }
    OpenXRSwapchain& swapchainForCube(uint32_t cube) { return cube && m_rightSwapchain ? *m_rightSwapchain : m_swapchain.get(); }

    static constexpr uint32_t faceCount = 6;

    Vector<XrCompositionLayerCubeKHR> m_layers;
    std::unique_ptr<OpenXRSwapchain> m_rightSwapchain;
    // One side-by-side buffer per swapchain image (keyed by the image), so the WebProcess and the
    // reconstruction don't read/write the same buffer concurrently. Mirrors the per-image reuse of other layers.
    HashMap<PlatformGLObject, PlatformGLObject> m_sideBySideTextures;
    std::array<PlatformGLObject, 2> m_reconstructionFBOs { 0, 0 };
};
#endif

#endif

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
