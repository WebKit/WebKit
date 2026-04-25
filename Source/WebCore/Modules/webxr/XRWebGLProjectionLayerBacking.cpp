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
#include "XRWebGLProjectionLayerBacking.h"

#if ENABLE(WEBXR_LAYERS)

#include "WebXRSession.h"
#include "WebXRWebGLLayer.h"
#include "WebXRWebGLSwapchain.h"
#include "XRProjectionLayerInit.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRWebGLProjectionLayerBacking);

// Arbitrary value for minimum texture scaling. Below this threshold the resulting texture would be too small to see.
constexpr double MinTextureScalingFactor = 0.2;

ExceptionOr<Ref<XRWebGLProjectionLayerBacking>> XRWebGLProjectionLayerBacking::create(WebXRSession& session, WebGLRenderingContextBase& context, const XRProjectionLayerInit& init)
{
    auto device = session.device();
    if (!device)
        return Exception { ExceptionCode::OperationError, "Cannot create a projection layer without a valid device."_s };

    float scaleFactor = std::clamp(init.scaleFactor, MinTextureScalingFactor, device->maxFramebufferScalingFactor());
    FloatSize recommendedSize = session.recommendedWebGLFramebufferResolution();
    IntSize size = expandedIntSize(recommendedSize.scaled(scaleFactor));

    auto layerInfo = device->createLayerProjection(size.width(), size.height(), true);
    if (!layerInfo)
        return Exception { ExceptionCode::OperationError, "Unable to create a projection layer."_s };

    auto colorSwapchain = WebXRWebGLSharedImageSwapchain::create(context, WebXRSwapchain::SwapchainTargetFlags::Color, init.colorFormat, init.clearOnAccess, layerInfo->numImages);
    if (!colorSwapchain)
        return Exception { ExceptionCode::OperationError, "Failed to create a WebGL swapchain."_s };

    std::unique_ptr<WebXRWebGLSwapchain> depthStencilSwapchain;
    if (init.depthFormat)
        depthStencilSwapchain = XRWebGLLayerBacking::createDepthSwapchain(context, init.depthFormat, size, init.clearOnAccess, layerInfo->numImages);

    return adoptRef(*new XRWebGLProjectionLayerBacking(layerInfo->handle, WTF::move(colorSwapchain), WTF::move(depthStencilSwapchain)));
}

XRWebGLProjectionLayerBacking::XRWebGLProjectionLayerBacking(PlatformXR::LayerHandle handle, std::unique_ptr<WebXRWebGLSwapchain>&& colorSwapchain, std::unique_ptr<WebXRWebGLSwapchain>&& depthSwapchain)
    : XRWebGLLayerBacking(handle, WTF::move(colorSwapchain), WTF::move(depthSwapchain))
{
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
