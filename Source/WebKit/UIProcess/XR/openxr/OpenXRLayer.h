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

#pragma once

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRSwapchain.h"
#include "OpenXRUtils.h"

#include <WebCore/PlatformXR.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace WebKit {

class OpenXRGraphicsBinding;

class OpenXRLayer {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRLayer);
    WTF_MAKE_NONCOPYABLE(OpenXRLayer);
public:
    virtual ~OpenXRLayer();

    virtual std::optional<PlatformXR::FrameData::LayerData> startFrame(OpenXRGraphicsBinding&) = 0;
    virtual Vector<XrCompositionLayerBaseHeader*> endFrame(OpenXRGraphicsBinding&, const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) = 0;

protected:
    OpenXRLayer(UniqueRef<OpenXRSwapchain>&&);

    UniqueRef<OpenXRSwapchain> m_swapchain;

    uint64_t m_renderingFrameIndex { 0 };
    using ReusableTextureIndex = uint64_t;
    HashMap<uint64_t, ReusableTextureIndex> m_exportedTextures;
    ReusableTextureIndex m_nextReusableTextureIndex { 0 };
};

class OpenXRLayerProjection final: public OpenXRLayer  {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRLayerProjection);
    WTF_MAKE_NONCOPYABLE(OpenXRLayerProjection);
public:
    static std::unique_ptr<OpenXRLayerProjection> create(std::unique_ptr<OpenXRSwapchain>&&);
private:
    explicit OpenXRLayerProjection(UniqueRef<OpenXRSwapchain>&&);

    std::optional<PlatformXR::FrameData::LayerData> startFrame(OpenXRGraphicsBinding&) override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(OpenXRGraphicsBinding&, const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

    XrCompositionLayerProjection m_layerProjection;
    Vector<XrCompositionLayerProjectionView> m_projectionViews;
};

#if ENABLE(WEBXR_LAYERS)

class OpenXRCompositionLayer : public OpenXRLayer {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRCompositionLayer);
    WTF_MAKE_NONCOPYABLE(OpenXRCompositionLayer);
public:
    std::optional<PlatformXR::FrameData::LayerData> startFrame(OpenXRGraphicsBinding&) override = 0;
    Vector<XrCompositionLayerBaseHeader*> endFrame(OpenXRGraphicsBinding&, const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override = 0;

protected:
    OpenXRCompositionLayer(UniqueRef<OpenXRSwapchain>&&, PlatformXR::LayerLayout);

    PlatformXR::LayerLayout m_layout;
};

class OpenXRQuadLayer final : public OpenXRCompositionLayer {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRQuadLayer);
    WTF_MAKE_NONCOPYABLE(OpenXRQuadLayer);
public:
    static std::unique_ptr<OpenXRQuadLayer> create(std::unique_ptr<OpenXRSwapchain>&&, PlatformXR::LayerLayout);

    std::optional<PlatformXR::FrameData::LayerData> startFrame(OpenXRGraphicsBinding&) override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(OpenXRGraphicsBinding&, const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

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

    std::optional<PlatformXR::FrameData::LayerData> startFrame(OpenXRGraphicsBinding&) override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(OpenXRGraphicsBinding&, const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

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

    std::optional<PlatformXR::FrameData::LayerData> startFrame(OpenXRGraphicsBinding&) override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(OpenXRGraphicsBinding&, const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

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

    std::optional<PlatformXR::FrameData::LayerData> startFrame(OpenXRGraphicsBinding&) override;
    Vector<XrCompositionLayerBaseHeader*> endFrame(OpenXRGraphicsBinding&, const PlatformXR::DeviceLayer&, XrSpace, const Vector<XrView>&) override;

private:
    OpenXRCubeLayer(UniqueRef<OpenXRSwapchain>&&, std::unique_ptr<OpenXRSwapchain>&& rightSwapchain, PlatformXR::LayerLayout);

    uint32_t cubeCount() const { return m_layout == PlatformXR::LayerLayout::Mono ? 1 : 2; }
    OpenXRSwapchain& swapchainForCube(uint32_t cube) { return cube && m_rightSwapchain ? *m_rightSwapchain : m_swapchain.get(); }

    static constexpr uint32_t faceCount = 6;

    Vector<XrCompositionLayerCubeKHR> m_layers;
    std::unique_ptr<OpenXRSwapchain> m_rightSwapchain;
};
#endif

#endif

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
