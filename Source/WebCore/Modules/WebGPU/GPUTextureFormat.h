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
#include <pal/graphics/WebGPU/WebGPUTextureFormat.h>

namespace WebCore {

enum class GPUTextureFormat : uint8_t {
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

inline PAL::WebGPU::TextureFormat convertToBacking(GPUTextureFormat textureFormat)
{
    switch (textureFormat) {
    case GPUTextureFormat::R8unorm:
        return PAL::WebGPU::TextureFormat::R8unorm;
    case GPUTextureFormat::R8snorm:
        return PAL::WebGPU::TextureFormat::R8snorm;
    case GPUTextureFormat::R8uint:
        return PAL::WebGPU::TextureFormat::R8uint;
    case GPUTextureFormat::R8sint:
        return PAL::WebGPU::TextureFormat::R8sint;
    case GPUTextureFormat::R16uint:
        return PAL::WebGPU::TextureFormat::R16uint;
    case GPUTextureFormat::R16sint:
        return PAL::WebGPU::TextureFormat::R16sint;
    case GPUTextureFormat::R16float:
        return PAL::WebGPU::TextureFormat::R16float;
    case GPUTextureFormat::Rg8unorm:
        return PAL::WebGPU::TextureFormat::Rg8unorm;
    case GPUTextureFormat::Rg8snorm:
        return PAL::WebGPU::TextureFormat::Rg8snorm;
    case GPUTextureFormat::Rg8uint:
        return PAL::WebGPU::TextureFormat::Rg8uint;
    case GPUTextureFormat::Rg8sint:
        return PAL::WebGPU::TextureFormat::Rg8sint;
    case GPUTextureFormat::R32uint:
        return PAL::WebGPU::TextureFormat::R32uint;
    case GPUTextureFormat::R32sint:
        return PAL::WebGPU::TextureFormat::R32sint;
    case GPUTextureFormat::R32float:
        return PAL::WebGPU::TextureFormat::R32float;
    case GPUTextureFormat::Rg16uint:
        return PAL::WebGPU::TextureFormat::Rg16uint;
    case GPUTextureFormat::Rg16sint:
        return PAL::WebGPU::TextureFormat::Rg16sint;
    case GPUTextureFormat::Rg16float:
        return PAL::WebGPU::TextureFormat::Rg16float;
    case GPUTextureFormat::Rgba8unorm:
        return PAL::WebGPU::TextureFormat::Rgba8unorm;
    case GPUTextureFormat::Rgba8unormSRGB:
        return PAL::WebGPU::TextureFormat::Rgba8unormSRGB;
    case GPUTextureFormat::Rgba8snorm:
        return PAL::WebGPU::TextureFormat::Rgba8snorm;
    case GPUTextureFormat::Rgba8uint:
        return PAL::WebGPU::TextureFormat::Rgba8uint;
    case GPUTextureFormat::Rgba8sint:
        return PAL::WebGPU::TextureFormat::Rgba8sint;
    case GPUTextureFormat::Bgra8unorm:
        return PAL::WebGPU::TextureFormat::Bgra8unorm;
    case GPUTextureFormat::Bgra8unormSRGB:
        return PAL::WebGPU::TextureFormat::Bgra8unormSRGB;
    case GPUTextureFormat::Rgb9e5ufloat:
        return PAL::WebGPU::TextureFormat::Rgb9e5ufloat;
    case GPUTextureFormat::Rgb10a2unorm:
        return PAL::WebGPU::TextureFormat::Rgb10a2unorm;
    case GPUTextureFormat::Rg11b10ufloat:
        return PAL::WebGPU::TextureFormat::Rg11b10ufloat;
    case GPUTextureFormat::Rg32uint:
        return PAL::WebGPU::TextureFormat::Rg32uint;
    case GPUTextureFormat::Rg32sint:
        return PAL::WebGPU::TextureFormat::Rg32sint;
    case GPUTextureFormat::Rg32float:
        return PAL::WebGPU::TextureFormat::Rg32float;
    case GPUTextureFormat::Rgba16uint:
        return PAL::WebGPU::TextureFormat::Rgba16uint;
    case GPUTextureFormat::Rgba16sint:
        return PAL::WebGPU::TextureFormat::Rgba16sint;
    case GPUTextureFormat::Rgba16float:
        return PAL::WebGPU::TextureFormat::Rgba16float;
    case GPUTextureFormat::Rgba32uint:
        return PAL::WebGPU::TextureFormat::Rgba32uint;
    case GPUTextureFormat::Rgba32sint:
        return PAL::WebGPU::TextureFormat::Rgba32sint;
    case GPUTextureFormat::Rgba32float:
        return PAL::WebGPU::TextureFormat::Rgba32float;
    case GPUTextureFormat::Stencil8:
        return PAL::WebGPU::TextureFormat::Stencil8;
    case GPUTextureFormat::Depth16unorm:
        return PAL::WebGPU::TextureFormat::Depth16unorm;
    case GPUTextureFormat::Depth24plus:
        return PAL::WebGPU::TextureFormat::Depth24plus;
    case GPUTextureFormat::Depth24plusStencil8:
        return PAL::WebGPU::TextureFormat::Depth24plusStencil8;
    case GPUTextureFormat::Depth32float:
        return PAL::WebGPU::TextureFormat::Depth32float;
    case GPUTextureFormat::Depth32floatStencil8:
        return PAL::WebGPU::TextureFormat::Depth32floatStencil8;
    case GPUTextureFormat::Bc1RgbaUnorm:
        return PAL::WebGPU::TextureFormat::Bc1RgbaUnorm;
    case GPUTextureFormat::Bc1RgbaUnormSRGB:
        return PAL::WebGPU::TextureFormat::Bc1RgbaUnormSRGB;
    case GPUTextureFormat::Bc2RgbaUnorm:
        return PAL::WebGPU::TextureFormat::Bc2RgbaUnorm;
    case GPUTextureFormat::Bc2RgbaUnormSRGB:
        return PAL::WebGPU::TextureFormat::Bc2RgbaUnormSRGB;
    case GPUTextureFormat::Bc3RgbaUnorm:
        return PAL::WebGPU::TextureFormat::Bc3RgbaUnorm;
    case GPUTextureFormat::Bc3RgbaUnormSRGB:
        return PAL::WebGPU::TextureFormat::Bc3RgbaUnormSRGB;
    case GPUTextureFormat::Bc4RUnorm:
        return PAL::WebGPU::TextureFormat::Bc4RUnorm;
    case GPUTextureFormat::Bc4RSnorm:
        return PAL::WebGPU::TextureFormat::Bc4RSnorm;
    case GPUTextureFormat::Bc5RgUnorm:
        return PAL::WebGPU::TextureFormat::Bc5RgUnorm;
    case GPUTextureFormat::Bc5RgSnorm:
        return PAL::WebGPU::TextureFormat::Bc5RgSnorm;
    case GPUTextureFormat::Bc6hRgbUfloat:
        return PAL::WebGPU::TextureFormat::Bc6hRgbUfloat;
    case GPUTextureFormat::Bc6hRgbFloat:
        return PAL::WebGPU::TextureFormat::Bc6hRgbFloat;
    case GPUTextureFormat::Bc7RgbaUnorm:
        return PAL::WebGPU::TextureFormat::Bc7RgbaUnorm;
    case GPUTextureFormat::Bc7RgbaUnormSRGB:
        return PAL::WebGPU::TextureFormat::Bc7RgbaUnormSRGB;
    case GPUTextureFormat::Etc2Rgb8unorm:
        return PAL::WebGPU::TextureFormat::Etc2Rgb8unorm;
    case GPUTextureFormat::Etc2Rgb8unormSRGB:
        return PAL::WebGPU::TextureFormat::Etc2Rgb8unormSRGB;
    case GPUTextureFormat::Etc2Rgb8a1unorm:
        return PAL::WebGPU::TextureFormat::Etc2Rgb8a1unorm;
    case GPUTextureFormat::Etc2Rgb8a1unormSRGB:
        return PAL::WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB;
    case GPUTextureFormat::Etc2Rgba8unorm:
        return PAL::WebGPU::TextureFormat::Etc2Rgba8unorm;
    case GPUTextureFormat::Etc2Rgba8unormSRGB:
        return PAL::WebGPU::TextureFormat::Etc2Rgba8unormSRGB;
    case GPUTextureFormat::EacR11unorm:
        return PAL::WebGPU::TextureFormat::EacR11unorm;
    case GPUTextureFormat::EacR11snorm:
        return PAL::WebGPU::TextureFormat::EacR11snorm;
    case GPUTextureFormat::EacRg11unorm:
        return PAL::WebGPU::TextureFormat::EacRg11unorm;
    case GPUTextureFormat::EacRg11snorm:
        return PAL::WebGPU::TextureFormat::EacRg11snorm;
    case GPUTextureFormat::Astc4x4Unorm:
        return PAL::WebGPU::TextureFormat::Astc4x4Unorm;
    case GPUTextureFormat::Astc4x4UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc4x4UnormSRGB;
    case GPUTextureFormat::Astc5x4Unorm:
        return PAL::WebGPU::TextureFormat::Astc5x4Unorm;
    case GPUTextureFormat::Astc5x4UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc5x4UnormSRGB;
    case GPUTextureFormat::Astc5x5Unorm:
        return PAL::WebGPU::TextureFormat::Astc5x5Unorm;
    case GPUTextureFormat::Astc5x5UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc5x5UnormSRGB;
    case GPUTextureFormat::Astc6x5Unorm:
        return PAL::WebGPU::TextureFormat::Astc6x5Unorm;
    case GPUTextureFormat::Astc6x5UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc6x5UnormSRGB;
    case GPUTextureFormat::Astc6x6Unorm:
        return PAL::WebGPU::TextureFormat::Astc6x6Unorm;
    case GPUTextureFormat::Astc6x6UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc6x6UnormSRGB;
    case GPUTextureFormat::Astc8x5Unorm:
        return PAL::WebGPU::TextureFormat::Astc8x5Unorm;
    case GPUTextureFormat::Astc8x5UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc8x5UnormSRGB;
    case GPUTextureFormat::Astc8x6Unorm:
        return PAL::WebGPU::TextureFormat::Astc8x6Unorm;
    case GPUTextureFormat::Astc8x6UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc8x6UnormSRGB;
    case GPUTextureFormat::Astc8x8Unorm:
        return PAL::WebGPU::TextureFormat::Astc8x8Unorm;
    case GPUTextureFormat::Astc8x8UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc8x8UnormSRGB;
    case GPUTextureFormat::Astc10x5Unorm:
        return PAL::WebGPU::TextureFormat::Astc10x5Unorm;
    case GPUTextureFormat::Astc10x5UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc10x5UnormSRGB;
    case GPUTextureFormat::Astc10x6Unorm:
        return PAL::WebGPU::TextureFormat::Astc10x6Unorm;
    case GPUTextureFormat::Astc10x6UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc10x6UnormSRGB;
    case GPUTextureFormat::Astc10x8Unorm:
        return PAL::WebGPU::TextureFormat::Astc10x8Unorm;
    case GPUTextureFormat::Astc10x8UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc10x8UnormSRGB;
    case GPUTextureFormat::Astc10x10Unorm:
        return PAL::WebGPU::TextureFormat::Astc10x10Unorm;
    case GPUTextureFormat::Astc10x10UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc10x10UnormSRGB;
    case GPUTextureFormat::Astc12x10Unorm:
        return PAL::WebGPU::TextureFormat::Astc12x10Unorm;
    case GPUTextureFormat::Astc12x10UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc12x10UnormSRGB;
    case GPUTextureFormat::Astc12x12Unorm:
        return PAL::WebGPU::TextureFormat::Astc12x12Unorm;
    case GPUTextureFormat::Astc12x12UnormSRGB:
        return PAL::WebGPU::TextureFormat::Astc12x12UnormSRGB;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

}
