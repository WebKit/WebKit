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

#include <cstdint>
#include <pal/graphics/WebGPU/WebGPUBlendFactor.h>

namespace WebCore {

enum class GPUBlendFactor : uint8_t {
    Zero,
    One,
    Src,
    OneMinusSrc,
    SrcAlpha,
    OneMinusSrcAlpha,
    Dst,
    OneMinusDst,
    DstAlpha,
    OneMinusDstAlpha,
    SrcAlphaSaturated,
    Constant,
    OneMinusConstant,
};

inline PAL::WebGPU::BlendFactor convertToBacking(GPUBlendFactor blendFactor)
{
    switch (blendFactor) {
    case GPUBlendFactor::Zero:
        return PAL::WebGPU::BlendFactor::Zero;
    case GPUBlendFactor::One:
        return PAL::WebGPU::BlendFactor::One;
    case GPUBlendFactor::Src:
        return PAL::WebGPU::BlendFactor::Src;
    case GPUBlendFactor::OneMinusSrc:
        return PAL::WebGPU::BlendFactor::OneMinusSrc;
    case GPUBlendFactor::SrcAlpha:
        return PAL::WebGPU::BlendFactor::SrcAlpha;
    case GPUBlendFactor::OneMinusSrcAlpha:
        return PAL::WebGPU::BlendFactor::OneMinusSrcAlpha;
    case GPUBlendFactor::Dst:
        return PAL::WebGPU::BlendFactor::Dst;
    case GPUBlendFactor::OneMinusDst:
        return PAL::WebGPU::BlendFactor::OneMinusDst;
    case GPUBlendFactor::DstAlpha:
        return PAL::WebGPU::BlendFactor::DstAlpha;
    case GPUBlendFactor::OneMinusDstAlpha:
        return PAL::WebGPU::BlendFactor::OneMinusDstAlpha;
    case GPUBlendFactor::SrcAlphaSaturated:
        return PAL::WebGPU::BlendFactor::SrcAlphaSaturated;
    case GPUBlendFactor::Constant:
        return PAL::WebGPU::BlendFactor::Constant;
    case GPUBlendFactor::OneMinusConstant:
        return PAL::WebGPU::BlendFactor::OneMinusConstant;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

}
