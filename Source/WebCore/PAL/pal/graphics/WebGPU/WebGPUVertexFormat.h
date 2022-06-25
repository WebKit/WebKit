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

enum class VertexFormat : uint8_t {
    Uint8x2,
    Uint8x4,
    Sint8x2,
    Sint8x4,
    Unorm8x2,
    Unorm8x4,
    Snorm8x2,
    Snorm8x4,
    Uint16x2,
    Uint16x4,
    Sint16x2,
    Sint16x4,
    Unorm16x2,
    Unorm16x4,
    Snorm16x2,
    Snorm16x4,
    Float16x2,
    Float16x4,
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Uint32,
    Uint32x2,
    Uint32x3,
    Uint32x4,
    Sint32,
    Sint32x2,
    Sint32x3,
    Sint32x4,
};

} // namespace PAL::WebGPU

namespace WTF {

template<> struct EnumTraits<PAL::WebGPU::VertexFormat> {
    using values = EnumValues<
        PAL::WebGPU::VertexFormat,
        PAL::WebGPU::VertexFormat::Uint8x2,
        PAL::WebGPU::VertexFormat::Uint8x4,
        PAL::WebGPU::VertexFormat::Sint8x2,
        PAL::WebGPU::VertexFormat::Sint8x4,
        PAL::WebGPU::VertexFormat::Unorm8x2,
        PAL::WebGPU::VertexFormat::Unorm8x4,
        PAL::WebGPU::VertexFormat::Snorm8x2,
        PAL::WebGPU::VertexFormat::Snorm8x4,
        PAL::WebGPU::VertexFormat::Uint16x2,
        PAL::WebGPU::VertexFormat::Uint16x4,
        PAL::WebGPU::VertexFormat::Sint16x2,
        PAL::WebGPU::VertexFormat::Sint16x4,
        PAL::WebGPU::VertexFormat::Unorm16x2,
        PAL::WebGPU::VertexFormat::Unorm16x4,
        PAL::WebGPU::VertexFormat::Snorm16x2,
        PAL::WebGPU::VertexFormat::Snorm16x4,
        PAL::WebGPU::VertexFormat::Float16x2,
        PAL::WebGPU::VertexFormat::Float16x4,
        PAL::WebGPU::VertexFormat::Float32,
        PAL::WebGPU::VertexFormat::Float32x2,
        PAL::WebGPU::VertexFormat::Float32x3,
        PAL::WebGPU::VertexFormat::Float32x4,
        PAL::WebGPU::VertexFormat::Uint32,
        PAL::WebGPU::VertexFormat::Uint32x2,
        PAL::WebGPU::VertexFormat::Uint32x3,
        PAL::WebGPU::VertexFormat::Uint32x4,
        PAL::WebGPU::VertexFormat::Sint32,
        PAL::WebGPU::VertexFormat::Sint32x2,
        PAL::WebGPU::VertexFormat::Sint32x3,
        PAL::WebGPU::VertexFormat::Sint32x4
    >;
};

} // namespace WTF
