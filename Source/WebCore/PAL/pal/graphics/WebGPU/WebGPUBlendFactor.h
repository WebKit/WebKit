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
#include <wtf/EnumTraits.h>

namespace PAL::WebGPU {

enum class BlendFactor : uint8_t {
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

} // namespace PAL::WebGPU

namespace WTF {

template<> struct EnumTraits<PAL::WebGPU::BlendFactor> {
    using values = EnumValues<
        PAL::WebGPU::BlendFactor,
        PAL::WebGPU::BlendFactor::Zero,
        PAL::WebGPU::BlendFactor::One,
        PAL::WebGPU::BlendFactor::Src,
        PAL::WebGPU::BlendFactor::OneMinusSrc,
        PAL::WebGPU::BlendFactor::SrcAlpha,
        PAL::WebGPU::BlendFactor::OneMinusSrcAlpha,
        PAL::WebGPU::BlendFactor::Dst,
        PAL::WebGPU::BlendFactor::OneMinusDst,
        PAL::WebGPU::BlendFactor::DstAlpha,
        PAL::WebGPU::BlendFactor::OneMinusDstAlpha,
        PAL::WebGPU::BlendFactor::SrcAlphaSaturated,
        PAL::WebGPU::BlendFactor::Constant,
        PAL::WebGPU::BlendFactor::OneMinusConstant
    >;
};

} // namespace WTF
