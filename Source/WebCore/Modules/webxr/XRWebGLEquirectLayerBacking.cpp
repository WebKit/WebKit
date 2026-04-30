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
#include "XRWebGLEquirectLayerBacking.h"

#if ENABLE(WEBXR_LAYERS)

#include "WebXRSession.h"
#include "WebXRWebGLLayer.h"
#include "WebXRWebGLSwapchain.h"
#include "XREquirectLayerInit.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRWebGLEquirectLayerBacking);

ExceptionOr<Ref<XRWebGLEquirectLayerBacking>> XRWebGLEquirectLayerBacking::create(WebXRSession& session, WebGLRenderingContextBase& context, const XREquirectLayerInit& init)
{
    auto device = session.device();
    if (!device)
        return Exception { ExceptionCode::OperationError, "Cannot create an equirect layer without a valid device."_s };

    auto computeEquirectLayerData = [](XREquirectLayerInit init) -> std::pair<IntSize, PlatformXR::LayerLayout> {
        switch (init.layout) {
        case XRLayerLayout::Mono:
            return { IntSize { static_cast<int>(init.viewPixelWidth), static_cast<int>(init.viewPixelHeight) }, PlatformXR::LayerLayout::Mono };
        case XRLayerLayout::Stereo:
        case XRLayerLayout::StereoLeftRight:
            return { IntSize { static_cast<int>(init.viewPixelWidth * 2), static_cast<int>(init.viewPixelHeight) }, PlatformXR::LayerLayout::StereoLeftRight };
        case XRLayerLayout::StereoTopBottom:
            return { IntSize { static_cast<int>(init.viewPixelWidth), static_cast<int>(init.viewPixelHeight * 2) }, PlatformXR::LayerLayout::StereoTopBottom };
        default:
        case XRLayerLayout::Default:
            ASSERT_NOT_REACHED_WITH_MESSAGE("Default layout is not supported for non-projection Layers");
            return { IntSize(), PlatformXR::LayerLayout::Mono };
        };
    };

    auto [ equirectLayerSize, equirectLayerLayout ] = computeEquirectLayerData(init);

    auto layerInfo = device->createCompositionLayer(PlatformXR::CompositionLayerType::Equirect, equirectLayerSize, equirectLayerLayout);
    if (!layerInfo)
        return Exception { ExceptionCode::OperationError, "Unable to create an equirect layer."_s };

    auto colorSwapchain = WebXRWebGLSharedImageSwapchain::create(context, WebXRSwapchain::SwapchainTargetFlags::Color, init.colorFormat, init.clearOnAccess, layerInfo->numImages);
    if (!colorSwapchain)
        return Exception { ExceptionCode::OperationError, "Failed to create a WebGL swapchain."_s };

    return adoptRef(*new XRWebGLEquirectLayerBacking(layerInfo->handle, WTF::move(colorSwapchain), nullptr, init));
}

XRWebGLEquirectLayerBacking::XRWebGLEquirectLayerBacking(PlatformXR::LayerHandle handle, std::unique_ptr<WebXRWebGLSwapchain>&& colorSwapchain, std::unique_ptr<WebXRWebGLSwapchain>&& depthSwapchain, const XREquirectLayerInit& init)
    : XRWebGLLayerBacking(handle, WTF::move(colorSwapchain), WTF::move(depthSwapchain))
    , m_init(init)
{
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
