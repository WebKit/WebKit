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
#include "WebXRWebGLSwapchain.h"
#include "XREquirectLayerInit.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRWebGLEquirectLayerBacking);

ExceptionOr<Ref<XRWebGLEquirectLayerBacking>> XRWebGLEquirectLayerBacking::create(WebXRSession& session, WebGLRenderingContextBase& context, const XREquirectLayerInit& init)
{
    auto swapchains = XRWebGLLayerBacking::createCompositionLayerSwapchains(session, context, PlatformXR::CompositionLayerType::Equirect, init);
    if (swapchains.hasException())
        return swapchains.releaseException();
    auto [handle, colorSwapchain, depthSwapchain, arrayLength] = swapchains.releaseReturnValue();
    return adoptRef(*new XRWebGLEquirectLayerBacking(handle, WTF::move(colorSwapchain), WTF::move(depthSwapchain), arrayLength, init));
}

XRWebGLEquirectLayerBacking::XRWebGLEquirectLayerBacking(PlatformXR::LayerHandle handle, std::unique_ptr<WebXRWebGLSwapchain>&& colorSwapchain, std::unique_ptr<WebXRWebGLSwapchain>&& depthSwapchain, uint32_t colorTextureArrayLength, const XREquirectLayerInit& init)
    : XRWebGLLayerBacking(handle, WTF::move(colorSwapchain), WTF::move(depthSwapchain), colorTextureArrayLength)
    , m_init(init)
{
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
