/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "GPUCompareFunction.h"
#include "GPUIntegralTypes.h"
#include "GPUStencilFaceState.h"
#include "GPUTextureFormat.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUDepthStencilState.h>

namespace WebCore {

struct GPUDepthStencilState {
    PAL::WebGPU::DepthStencilState convertToBacking() const
    {
        return {
            WebCore::convertToBacking(format),
            depthWriteEnabled,
            WebCore::convertToBacking(depthCompare),
            stencilFront.convertToBacking(),
            stencilBack.convertToBacking(),
            stencilReadMask ? std::optional { *stencilReadMask } : std::nullopt,
            stencilWriteMask ? std::optional { *stencilWriteMask } : std::nullopt,
            depthBias,
            depthBiasSlopeScale,
            depthBiasClamp,
        };
    }

    GPUTextureFormat format { GPUTextureFormat::R8unorm };

    bool depthWriteEnabled { false };
    GPUCompareFunction depthCompare { GPUCompareFunction::Always };

    GPUStencilFaceState stencilFront;
    GPUStencilFaceState stencilBack;

    std::optional<GPUStencilValue> stencilReadMask;
    std::optional<GPUStencilValue> stencilWriteMask;

    GPUDepthBias depthBias { 0 };
    float depthBiasSlopeScale { 0 };
    float depthBiasClamp { 0 };
};

}
