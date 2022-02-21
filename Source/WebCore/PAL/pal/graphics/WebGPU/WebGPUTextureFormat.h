/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

enum class TextureFormat : uint8_t {
    // 8-bit formats
    R8unorm,
    R8snorm,
    R8uint,
    R8sint,

    // 16-bit formats
    R16uint,
    R16sint,
    R16float,
    Rg8unorm,
    Rg8snorm,
    Rg8uint,
    Rg8sint,

    // 32-bit formats
    R32uint,
    R32sint,
    R32float,
    Rg16uint,
    Rg16sint,
    Rg16float,
    Rgba8unorm,
    Rgba8unormSRGB,
    Rgba8snorm,
    Rgba8uint,
    Rgba8sint,
    Bgra8unorm,
    Bgra8unormSRGB,
    // Packed 32-bit formats
    Rgb9e5ufloat,
    Rgb10a2unorm,
    Rg11b10ufloat,

    // 64-bit formats
    Rg32uint,
    Rg32sint,
    Rg32float,
    Rgba16uint,
    Rgba16sint,
    Rgba16float,

    // 128-bit formats
    Rgba32uint,
    Rgba32sint,
    Rgba32float,

    // Depth/stencil formats
    Stencil8,
    Depth16unorm,
    Depth24plus,
    Depth24plusStencil8,
    Depth32float,

    // depth24unorm-stencil8 feature
    Depth24unormStencil8,

    // depth32float-stencil8 feature
    Depth32floatStencil8,

    // BC compressed formats usable if texture-compression-bc is both
    // supported by the device/user agent and enabled in requestDevice.
    Bc1RgbaUnorm,
    Bc1RgbaUnormSRGB,
    Bc2RgbaUnorm,
    Bc2RgbaUnormSRGB,
    Bc3RgbaUnorm,
    Bc3RgbaUnormSRGB,
    Bc4RUnorm,
    Bc4RSnorm,
    Bc5RgUnorm,
    Bc5RgSnorm,
    Bc6hRgbUfloat,
    Bc6hRgbFloat,
    Bc7RgbaUnorm,
    Bc7RgbaUnormSRGB,

    // ETC2 compressed formats usable if texture-compression-etc2 is both
    // supported by the device/user agent and enabled in requestDevice.
    Etc2Rgb8unorm,
    Etc2Rgb8unormSRGB,
    Etc2Rgb8a1unorm,
    Etc2Rgb8a1unormSRGB,
    Etc2Rgba8unorm,
    Etc2Rgba8unormSRGB,
    EacR11unorm,
    EacR11snorm,
    EacRg11unorm,
    EacRg11snorm,

    // ASTC compressed formats usable if texture-compression-astc is both
    // supported by the device/user agent and enabled in requestDevice.
    Astc4x4Unorm,
    Astc4x4UnormSRGB,
    Astc5x4Unorm,
    Astc5x4UnormSRGB,
    Astc5x5Unorm,
    Astc5x5UnormSRGB,
    Astc6x5Unorm,
    Astc6x5UnormSRGB,
    Astc6x6Unorm,
    Astc6x6UnormSRGB,
    Astc8x5Unorm,
    Astc8x5UnormSRGB,
    Astc8x6Unorm,
    Astc8x6UnormSRGB,
    Astc8x8Unorm,
    Astc8x8UnormSRGB,
    Astc10x5Unorm,
    Astc10x5UnormSRGB,
    Astc10x6Unorm,
    Astc10x6UnormSRGB,
    Astc10x8Unorm,
    Astc10x8UnormSRGB,
    Astc10x10Unorm,
    Astc10x10UnormSRGB,
    Astc12x10Unorm,
    Astc12x10UnormSRGB,
    Astc12x12Unorm,
    Astc12x12UnormSRGB,
};

} // namespace PAL::WebGPU

namespace WTF {

template<> struct EnumTraits<PAL::WebGPU::TextureFormat> {
    using values = EnumValues<
        PAL::WebGPU::TextureFormat,
        PAL::WebGPU::TextureFormat::R8unorm,
        PAL::WebGPU::TextureFormat::R8snorm,
        PAL::WebGPU::TextureFormat::R8uint,
        PAL::WebGPU::TextureFormat::R8sint,
        PAL::WebGPU::TextureFormat::R16uint,
        PAL::WebGPU::TextureFormat::R16sint,
        PAL::WebGPU::TextureFormat::R16float,
        PAL::WebGPU::TextureFormat::Rg8unorm,
        PAL::WebGPU::TextureFormat::Rg8snorm,
        PAL::WebGPU::TextureFormat::Rg8uint,
        PAL::WebGPU::TextureFormat::Rg8sint,
        PAL::WebGPU::TextureFormat::R32uint,
        PAL::WebGPU::TextureFormat::R32sint,
        PAL::WebGPU::TextureFormat::R32float,
        PAL::WebGPU::TextureFormat::Rg16uint,
        PAL::WebGPU::TextureFormat::Rg16sint,
        PAL::WebGPU::TextureFormat::Rg16float,
        PAL::WebGPU::TextureFormat::Rgba8unorm,
        PAL::WebGPU::TextureFormat::Rgba8unormSRGB,
        PAL::WebGPU::TextureFormat::Rgba8snorm,
        PAL::WebGPU::TextureFormat::Rgba8uint,
        PAL::WebGPU::TextureFormat::Rgba8sint,
        PAL::WebGPU::TextureFormat::Bgra8unorm,
        PAL::WebGPU::TextureFormat::Bgra8unormSRGB,
        PAL::WebGPU::TextureFormat::Rgb9e5ufloat,
        PAL::WebGPU::TextureFormat::Rgb10a2unorm,
        PAL::WebGPU::TextureFormat::Rg11b10ufloat,
        PAL::WebGPU::TextureFormat::Rg32uint,
        PAL::WebGPU::TextureFormat::Rg32sint,
        PAL::WebGPU::TextureFormat::Rg32float,
        PAL::WebGPU::TextureFormat::Rgba16uint,
        PAL::WebGPU::TextureFormat::Rgba16sint,
        PAL::WebGPU::TextureFormat::Rgba16float,
        PAL::WebGPU::TextureFormat::Rgba32uint,
        PAL::WebGPU::TextureFormat::Rgba32sint,
        PAL::WebGPU::TextureFormat::Rgba32float,
        PAL::WebGPU::TextureFormat::Stencil8,
        PAL::WebGPU::TextureFormat::Depth16unorm,
        PAL::WebGPU::TextureFormat::Depth24plus,
        PAL::WebGPU::TextureFormat::Depth24plusStencil8,
        PAL::WebGPU::TextureFormat::Depth32float,
        PAL::WebGPU::TextureFormat::Depth24unormStencil8,
        PAL::WebGPU::TextureFormat::Depth32floatStencil8,
        PAL::WebGPU::TextureFormat::Bc1RgbaUnorm,
        PAL::WebGPU::TextureFormat::Bc1RgbaUnormSRGB,
        PAL::WebGPU::TextureFormat::Bc2RgbaUnorm,
        PAL::WebGPU::TextureFormat::Bc2RgbaUnormSRGB,
        PAL::WebGPU::TextureFormat::Bc3RgbaUnorm,
        PAL::WebGPU::TextureFormat::Bc3RgbaUnormSRGB,
        PAL::WebGPU::TextureFormat::Bc4RUnorm,
        PAL::WebGPU::TextureFormat::Bc4RSnorm,
        PAL::WebGPU::TextureFormat::Bc5RgUnorm,
        PAL::WebGPU::TextureFormat::Bc5RgSnorm,
        PAL::WebGPU::TextureFormat::Bc6hRgbUfloat,
        PAL::WebGPU::TextureFormat::Bc6hRgbFloat,
        PAL::WebGPU::TextureFormat::Bc7RgbaUnorm,
        PAL::WebGPU::TextureFormat::Bc7RgbaUnormSRGB,
        PAL::WebGPU::TextureFormat::Etc2Rgb8unorm,
        PAL::WebGPU::TextureFormat::Etc2Rgb8unormSRGB,
        PAL::WebGPU::TextureFormat::Etc2Rgb8a1unorm,
        PAL::WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB,
        PAL::WebGPU::TextureFormat::Etc2Rgba8unorm,
        PAL::WebGPU::TextureFormat::Etc2Rgba8unormSRGB,
        PAL::WebGPU::TextureFormat::EacR11unorm,
        PAL::WebGPU::TextureFormat::EacR11snorm,
        PAL::WebGPU::TextureFormat::EacRg11unorm,
        PAL::WebGPU::TextureFormat::EacRg11snorm,
        PAL::WebGPU::TextureFormat::Astc4x4Unorm,
        PAL::WebGPU::TextureFormat::Astc4x4UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc5x4Unorm,
        PAL::WebGPU::TextureFormat::Astc5x4UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc5x5Unorm,
        PAL::WebGPU::TextureFormat::Astc5x5UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc6x5Unorm,
        PAL::WebGPU::TextureFormat::Astc6x5UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc6x6Unorm,
        PAL::WebGPU::TextureFormat::Astc6x6UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc8x5Unorm,
        PAL::WebGPU::TextureFormat::Astc8x5UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc8x6Unorm,
        PAL::WebGPU::TextureFormat::Astc8x6UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc8x8Unorm,
        PAL::WebGPU::TextureFormat::Astc8x8UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc10x5Unorm,
        PAL::WebGPU::TextureFormat::Astc10x5UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc10x6Unorm,
        PAL::WebGPU::TextureFormat::Astc10x6UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc10x8Unorm,
        PAL::WebGPU::TextureFormat::Astc10x8UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc10x10Unorm,
        PAL::WebGPU::TextureFormat::Astc10x10UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc12x10Unorm,
        PAL::WebGPU::TextureFormat::Astc12x10UnormSRGB,
        PAL::WebGPU::TextureFormat::Astc12x12Unorm,
        PAL::WebGPU::TextureFormat::Astc12x12UnormSRGB
    >;
};

} // namespace WTF
