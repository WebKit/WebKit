/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
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

#if ENABLE(WEBXR_LAYERS)

#include "GPUTextureFormat.h"
#include "GPUTextureUsage.h"
#include "WebGPUXRProjectionLayer.h"
#include "XRTextureType.h"

namespace WebCore {

// https://immersive-web.github.io/layers/#xrprojectionlayerinittype
struct XRGPUProjectionLayerInit {
    WebGPU::XRProjectionLayerInit convertToBacking()
    {
        std::optional<WebGPU::TextureFormat> optionalDepthStencilFormat;
        if (depthStencilFormat)
            optionalDepthStencilFormat = WebCore::convertToBacking(*depthStencilFormat);
        return WebGPU::XRProjectionLayerInit {
            .colorFormat = WebCore::convertToBacking(colorFormat),
            .depthStencilFormat = optionalDepthStencilFormat,
            .textureUsage = WebCore::convertTextureUsageFlagsToBacking(textureUsage),
            .scaleFactor = scaleFactor,
        };
    }

    GPUTextureFormat colorFormat { GPUTextureFormat::Bgra8unorm };
    std::optional<GPUTextureFormat> depthStencilFormat;
    GPUTextureUsageFlags textureUsage { GPUTextureUsage::RENDER_ATTACHMENT };
    double scaleFactor { 1.0 };
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
