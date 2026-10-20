/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Texture.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "CommandEncoder.h"
#import "Device.h"
#import "RenderBundle.h"
#import "TextureView.h"
#import <bmalloc/Algorithm.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/MathExtras.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebGPU::Metal {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Texture);

bool Texture::isCompressedFormat(WebGPU::TextureFormat format)
{
    return Texture::compressedFormatType(format).has_value();
}

std::optional<Texture::CompressFormat> Texture::compressedFormatType(WebGPU::TextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#packed-formats
    switch (format) {
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return Texture::CompressFormat::BC;
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
        return Texture::CompressFormat::ETC;
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return Texture::CompressFormat::ASTC;
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return std::nullopt;
    }
}

static std::optional<WebGPU::TextureFormat> LEGACY_NODELETE depthSpecificFormat(WebGPU::TextureFormat textureFormat)
{
    // https://gpuweb.github.io/gpuweb/#aspect-specific-format

    switch (textureFormat) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Stencil8:
        return std::nullopt;
    case WebGPU::TextureFormat::Depth16unorm:
        return WebGPU::TextureFormat::Depth16unorm;
    case WebGPU::TextureFormat::Depth24plus:
        return WebGPU::TextureFormat::Depth24plus;
    case WebGPU::TextureFormat::Depth24plusStencil8:
        return WebGPU::TextureFormat::Depth24plus;
    case WebGPU::TextureFormat::Depth32float:
        return WebGPU::TextureFormat::Depth32float;
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return WebGPU::TextureFormat::Depth32float;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return std::nullopt;
    }
}

static std::optional<WebGPU::TextureFormat> LEGACY_NODELETE stencilSpecificFormat(WebGPU::TextureFormat textureFormat)
{
    // https://gpuweb.github.io/gpuweb/#aspect-specific-format

    switch (textureFormat) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        return std::nullopt;
    case WebGPU::TextureFormat::Stencil8:
        return WebGPU::TextureFormat::Stencil8;
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
        return std::nullopt;
    case WebGPU::TextureFormat::Depth24plusStencil8:
        return WebGPU::TextureFormat::Stencil8;
    case WebGPU::TextureFormat::Depth32float:
        return std::nullopt;
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return WebGPU::TextureFormat::Stencil8;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return std::nullopt;
    }
}

bool Texture::containsDepthAspect(WebGPU::TextureFormat textureFormat)
{
    return depthSpecificFormat(textureFormat).has_value();
}

bool Texture::containsStencilAspect(WebGPU::TextureFormat textureFormat)
{
    return stencilSpecificFormat(textureFormat).has_value();
}

bool Texture::isDepthOrStencilFormat(WebGPU::TextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#depth-formats
    return containsDepthAspect(format) || containsStencilAspect(format);
}

uint32_t Texture::texelBlockWidth(WebGPU::TextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#texel-block-width
    switch (format) {
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return 4;
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
        return 4;
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
        return 4;
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
        return 5;
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
        return 6;
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
        return 8;
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
        return 10;
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return 12;
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return 1;
    }
}

uint32_t Texture::texelBlockHeight(WebGPU::TextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#texel-block-height
    switch (format) {
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return 4;
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
        return 4;
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
        return 4;
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
        return 5;
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
        return 6;
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
        return 5;
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
        return 6;
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
        return 8;
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
        return 5;
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
        return 6;
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
        return 8;
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
        return 10;
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return 12;
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return 1;
    }
}

bool Texture::isColorRenderableFormat(WebGPU::TextureFormat format, const Device& device)
{
    // https://gpuweb.github.io/gpuweb/#renderable-format
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        return true;
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1) || device.hasFeature(WebGPU::FeatureName::Rg11b10ufloatRenderable);
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return false;
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rgba8snorm:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return false;
    }
}

bool Texture::isDepthStencilRenderableFormat(WebGPU::TextureFormat format, const Device&)
{
    // https://gpuweb.github.io/gpuweb/#renderable-format
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
        return false;
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return true;
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return false;
    }
}

bool Texture::isRenderableFormat(WebGPU::TextureFormat format, const Device& device)
{
    // https://gpuweb.github.io/gpuweb/#renderable-format
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return true;
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rgba8snorm:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1) || device.hasFeature(WebGPU::FeatureName::Rg11b10ufloatRenderable);
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return false;
    }
}

uint32_t Texture::renderTargetPixelByteCost(WebGPU::TextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#renderable-format
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
        return 1;
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
        return 2;
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
        return 4;
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
        return 8;
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
        return 4;
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
        return 8;
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        return 16;
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return 8;
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return 0;
    }
}

uint32_t Texture::renderTargetPixelByteAlignment(WebGPU::TextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#renderable-format
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
        return 1;
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
        return 2;
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        return 4;
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return 0;
    }
}

bool Texture::supportsMultisampling(WebGPU::TextureFormat format, const Device& device)
{
    switch (format) {
    // https://gpuweb.github.io/gpuweb/#texture-format-caps
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    // https://gpuweb.github.io/gpuweb/#depth-formats
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return true;
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rgba8snorm:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1) || device.hasFeature(WebGPU::FeatureName::Rg11b10ufloatRenderable);
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return false;
    }
}

bool Texture::supportsResolve(WebGPU::TextureFormat format, const Device& device)
{
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rgba16float:
        return true;
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
        return false;
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rgba8snorm:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1) || device.hasFeature(WebGPU::FeatureName::Rg11b10ufloatRenderable);
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return false;
    }
}

bool Texture::supportsBlending(WebGPU::TextureFormat format, const Device& device)
{
    switch (format) {
    // https://gpuweb.github.io/gpuweb/#texture-format-caps
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgb10a2unorm:
        return true;
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1) || device.hasFeature(WebGPU::FeatureName::Rg11b10ufloatRenderable);
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rgba32float:
        return device.hasFeature(WebGPU::FeatureName::Float32Blendable);
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rgba8snorm:
        return device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return false;
    }
}

static uint32_t maximumMiplevelCount(WebGPU::TextureDimension dimension, const WebGPU::Extent3D& size)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-maximum-miplevel-count

    uint32_t m = 0;

    switch (dimension) {
    case WebGPU::TextureDimension::_1d:
        return 1;
    case WebGPU::TextureDimension::_2d:
        m = std::max(size.width, size.height);
        break;
    case WebGPU::TextureDimension::_3d:
        m = std::max(std::max(size.width, size.height), size.depthOrArrayLayers);
        break;
    }

    if (isPowerOfTwo(m))
        return WTF::fastLog2(m) + 1;
    return WTF::fastLog2(m);
}

bool Texture::hasStorageBindingCapability(WebGPU::TextureFormat format, const Device& device, std::optional<WebGPU::StorageTextureAccess> access)
{
    // https://gpuweb.github.io/gpuweb/#plain-color-formats
    switch (format) {
    // These formats always support read-only and write-only storage access;
    // texture-formats-tier2 additionally grants read-write storage access.
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        return (!access || *access != WebGPU::StorageTextureAccess::ReadWrite) || device.hasFeature(WebGPU::FeatureName::TextureFormatsTier2);
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
        return !access || *access != WebGPU::StorageTextureAccess::ReadWrite;
    case WebGPU::TextureFormat::Bgra8unorm:
        return (!access || *access == WebGPU::StorageTextureAccess::WriteOnly) && device.hasFeature(WebGPU::FeatureName::Bgra8unormStorage);
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
        return true;
    // These formats support storage access only with texture-formats-tier1;
    // texture-formats-tier2 additionally grants read-write storage access.
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
        return ((!access || *access != WebGPU::StorageTextureAccess::ReadWrite) || device.hasFeature(WebGPU::FeatureName::TextureFormatsTier2)) && device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rg16float:
        return (!access || *access != WebGPU::StorageTextureAccess::ReadWrite) && device.hasFeature(WebGPU::FeatureName::TextureFormatsTier1);
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return false;
    }
}

WebGPU::TextureFormat Texture::removeSRGBSuffix(WebGPU::TextureFormat format)
{
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
        return format;
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
        return WebGPU::TextureFormat::Rgba8unorm;
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
        return format;
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
        return WebGPU::TextureFormat::Bgra8unorm;
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return format;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
        return WebGPU::TextureFormat::Bc1RgbaUnorm;
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
        return WebGPU::TextureFormat::Bc2RgbaUnorm;
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
        return WebGPU::TextureFormat::Bc3RgbaUnorm;
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
        return format;
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return WebGPU::TextureFormat::Bc7RgbaUnorm;
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
        return WebGPU::TextureFormat::Etc2Rgb8unorm;
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
        return WebGPU::TextureFormat::Etc2Rgb8a1unorm;
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
        return WebGPU::TextureFormat::Etc2Rgba8unorm;
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
        return format;
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
        return WebGPU::TextureFormat::Astc4x4Unorm;
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
        return WebGPU::TextureFormat::Astc5x4Unorm;
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
        return WebGPU::TextureFormat::Astc5x5Unorm;
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
        return WebGPU::TextureFormat::Astc6x5Unorm;
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
        return WebGPU::TextureFormat::Astc6x6Unorm;
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
        return WebGPU::TextureFormat::Astc8x5Unorm;
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
        return WebGPU::TextureFormat::Astc8x6Unorm;
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
        return WebGPU::TextureFormat::Astc8x8Unorm;
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
        return WebGPU::TextureFormat::Astc10x5Unorm;
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
        return WebGPU::TextureFormat::Astc10x6Unorm;
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
        return WebGPU::TextureFormat::Astc10x8Unorm;
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
        return WebGPU::TextureFormat::Astc10x10Unorm;
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
        return WebGPU::TextureFormat::Astc12x10Unorm;
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return WebGPU::TextureFormat::Astc12x12Unorm;
    }
}

static bool LEGACY_NODELETE textureViewFormatCompatible(WebGPU::TextureFormat format1, WebGPU::TextureFormat format2)
{
    // https://gpuweb.github.io/gpuweb/#texture-view-format-compatible

    if (format1 == format2)
        return true;

    return Texture::removeSRGBSuffix(format1) == Texture::removeSRGBSuffix(format2);
}

NSString *Device::errorValidatingTextureCreation(const WebGPU::TextureDescriptor& descriptor)
{
    if (!isValid())
        return @"createTexture: Device is not valid";

    if (descriptor.usage.isEmpty())
        return @"createTexture: descriptor.usage is zero";

    if (descriptor.usage.contains(WebGPU::TextureUsage::Invalid))
        return @"createTexture: descriptor.usage contains a usage bit that is not defined";

    if (!descriptor.size.width || !descriptor.size.height || !descriptor.size.depthOrArrayLayers)
        return @"createTexture: descriptor.size.width/height/depth is zero";

    if (!descriptor.mipLevelCount)
        return @"createTexture: descriptor.mipLevelCount is zero";

    if (descriptor.sampleCount != 1 && descriptor.sampleCount != 4)
        return @"createTexture: descriptor.sampleCount is neither 1 nor 4";

    auto format = descriptor.format;

    switch (descriptor.dimension) {
    case WebGPU::TextureDimension::_1d:
        if (descriptor.size.width > limits().maxTextureDimension1D)
            return @"createTexture: descriptor.size.width is greater than limits().maxTextureDimension1D";

        if (descriptor.size.height != 1)
            return @"createTexture: descriptor.size.height != 1";

        if (descriptor.size.depthOrArrayLayers != 1)
            return @"createTexture: descriptor.size.depthOrArrayLayers != 1";

        if (descriptor.sampleCount != 1)
            return @"createTexture: descriptor.sampleCount != 1";

        if (Texture::isCompressedFormat(format) || Texture::isDepthOrStencilFormat(format))
            return @"createTexture: descriptor.format is compressed or a depth stencil format";
        break;
    case WebGPU::TextureDimension::_2d:
        if (descriptor.size.width > limits().maxTextureDimension2D)
            return @"createTexture: descriptor.size.width is greater than limits().maxTextureDimension2D";

        if (descriptor.size.height > limits().maxTextureDimension2D)
            return @"createTexture: descriptor.size.height is greater than limits().maxTextureDimension2D";

        if (descriptor.size.depthOrArrayLayers > limits().maxTextureArrayLayers)
            return @"createTexture: descriptor.size.depthOrArrayLayers > limits().maxTextureArrayLayers";
        break;
    case WebGPU::TextureDimension::_3d:
        if (descriptor.size.width > limits().maxTextureDimension3D)
            return @"createTexture: descriptor.size.width > limits().maxTextureDimension3D";

        if (descriptor.size.height > limits().maxTextureDimension3D)
            return @"createTexture: descriptor.size.height > limits().maxTextureDimension3D";

        if (descriptor.size.depthOrArrayLayers > limits().maxTextureDimension3D)
            return @"createTexture: descriptor.size.depthOrArrayLayers > limits().maxTextureDimension3D";

        if (descriptor.sampleCount != 1)
            return @"createTexture: descriptor.sampleCount != 1";

        if (auto compressedFormatType = Texture::compressedFormatType(format)) {
            switch (*compressedFormatType) {
            case Texture::CompressFormat::BC:
                if (!hasFeature(WebGPU::FeatureName::TextureCompressionBcSliced3d))
                    return @"createTexture: descriptor.format is a compressed format but BC sliced 3D extension is not enabled";
                break;
            case Texture::CompressFormat::ASTC:
                if (!hasFeature(WebGPU::FeatureName::TextureCompressionAstcSliced3d))
                    return @"createTexture: descriptor.format is a compressed format but ASTC sliced 3D extension is not enabled";
                break;
            case Texture::CompressFormat::ETC:
                return @"createTexture: descriptor.format is a ETC compressed format which is not supported for 3D textures";
            }
        }
        if (Texture::isDepthOrStencilFormat(format))
            return @"createTexture: descriptor.format is a depth stencil format, this is not allowed for 3D textures";
        break;
    }

    if (descriptor.size.width % Texture::texelBlockWidth(format))
        return @"createTexture: descriptor.size.width % Texture::texelBlockWidth(descriptor.format)";

    if (descriptor.size.height % Texture::texelBlockHeight(format))
        return @"createTexture: descriptor.size.height % Texture::texelBlockHeight(descriptor.format)";

    if (descriptor.sampleCount > 1) {
        if (descriptor.mipLevelCount != 1)
            return @"createTexture: descriptor.sampleCount > 1 and descriptor.mipLevelCount != 1";

        if (descriptor.size.depthOrArrayLayers != 1)
            return @"createTexture: descriptor.sampleCount > 1 and descriptor.size.depthOrArrayLayers != 1";

        if (descriptor.usage.contains(WebGPU::TextureUsage::StorageBinding) || !descriptor.usage.contains(WebGPU::TextureUsage::RenderAttachment))
            return @"createTexture: descriptor.sampleCount > 1 and (descriptor.usage & WGPUTextureUsage_StorageBinding) || !(descriptor.usage & WGPUTextureUsage_RenderAttachment)";

        if (!Texture::isRenderableFormat(format, *this))
            return @"createTexture: descriptor.sampleCount > 1 and !isRenderableFormat(descriptor.format, *this)";

        if (!Texture::supportsMultisampling(format, *this))
            return @"createTexture: descriptor.sampleCount > 1 and !supportsMultisampling(descriptor.format, *this)";
    }

    if (descriptor.mipLevelCount > maximumMiplevelCount(descriptor.dimension, descriptor.size))
        return @"createTexture: descriptor.mipLevelCount > maximumMiplevelCount(descriptor.dimension, descriptor.size)";

    if (descriptor.usage.contains(WebGPU::TextureUsage::RenderAttachment)) {
        if (!Texture::isRenderableFormat(format, *this))
            return @"createTexture: descriptor.usage & WGPUTextureUsage_RenderAttachment && !isRenderableFormat(descriptor.format, *this)";

        if (descriptor.dimension == WebGPU::TextureDimension::_1d)
            return @"createTexture: descriptor.usage & WGPUTextureUsage_RenderAttachment && descriptor.dimension == WebGPU::TextureDimension::_1d";
    }

    if (descriptor.usage.contains(WebGPU::TextureUsage::StorageBinding)) {
        if (!Texture::hasStorageBindingCapability(format, *this))
            return @"createTexture: descriptor.usage & WGPUTextureUsage_StorageBinding && !hasStorageBindingCapability(descriptor.format)";
    }

    for (auto viewFormat : descriptor.viewFormats) {
        if (!textureViewFormatCompatible(format, viewFormat))
            return @"createTexture: !textureViewFormatCompatible(descriptor.format, viewFormat)";
    }

    if (descriptor.usage.contains(WebGPU::TextureUsage::Transient)) {
        if (descriptor.usage != OptionSet { WebGPU::TextureUsage::Transient, WebGPU::TextureUsage::RenderAttachment })
            return @"createTexture: descriptor usage must be exactly Transient | Render_Attachment when using Transient textures";
        if (descriptor.dimension != WebGPU::TextureDimension::_2d)
            return @"createTexture: descriptor dimension must be 2D when using Transient textures";
        if (descriptor.mipLevelCount != 1)
            return @"createTexture: descriptor mipLevelCount must be 1 when using Transient textures";

        if (descriptor.size.depthOrArrayLayers != 1)
            return @"createTexture: descriptor.size.depthOrArrayLayers must be 1 when using Transient textures";

        if (!descriptor.viewFormats.empty())
            return @"createTexture: descriptor.viewFormats must be empty when using Transient textures";
    }

    return nil;
}

MTLTextureUsage Texture::usage(OptionSet<WebGPU::TextureUsage> usage, WebGPU::TextureFormat format)
{
    MTLTextureUsage result = MTLTextureUsageUnknown;
    if (usage.contains(WebGPU::TextureUsage::TextureBinding))
        result |= MTLTextureUsageShaderRead;
    if (usage.contains(WebGPU::TextureUsage::StorageBinding))
        result |= MTLTextureUsageShaderWrite;
    if (usage.contains(WebGPU::TextureUsage::RenderAttachment))
        result |= MTLTextureUsageRenderTarget;
    if (Texture::isDepthOrStencilFormat(format) || Texture::isCompressedFormat(format))
        result |= MTLTextureUsagePixelFormatView;
    return result;
}

MTLPixelFormat Texture::pixelFormat(WebGPU::TextureFormat textureFormat)
{
    switch (textureFormat) {
    case WebGPU::TextureFormat::R8unorm:
        return MTLPixelFormatR8Unorm;
    case WebGPU::TextureFormat::R8snorm:
        return MTLPixelFormatR8Snorm;
    case WebGPU::TextureFormat::R8uint:
        return MTLPixelFormatR8Uint;
    case WebGPU::TextureFormat::R8sint:
        return MTLPixelFormatR8Sint;
    case WebGPU::TextureFormat::R16unorm:
        return MTLPixelFormatR16Unorm;
    case WebGPU::TextureFormat::R16snorm:
        return MTLPixelFormatR16Snorm;
    case WebGPU::TextureFormat::R16uint:
        return MTLPixelFormatR16Uint;
    case WebGPU::TextureFormat::R16sint:
        return MTLPixelFormatR16Sint;
    case WebGPU::TextureFormat::R16float:
        return MTLPixelFormatR16Float;
    case WebGPU::TextureFormat::Rg8unorm:
        return MTLPixelFormatRG8Unorm;
    case WebGPU::TextureFormat::Rg8snorm:
        return MTLPixelFormatRG8Snorm;
    case WebGPU::TextureFormat::Rg8uint:
        return MTLPixelFormatRG8Uint;
    case WebGPU::TextureFormat::Rg8sint:
        return MTLPixelFormatRG8Sint;
    case WebGPU::TextureFormat::R32float:
        return MTLPixelFormatR32Float;
    case WebGPU::TextureFormat::R32uint:
        return MTLPixelFormatR32Uint;
    case WebGPU::TextureFormat::R32sint:
        return MTLPixelFormatR32Sint;
    case WebGPU::TextureFormat::Rg16unorm:
        return MTLPixelFormatRG16Unorm;
    case WebGPU::TextureFormat::Rg16snorm:
        return MTLPixelFormatRG16Snorm;
    case WebGPU::TextureFormat::Rg16uint:
        return MTLPixelFormatRG16Uint;
    case WebGPU::TextureFormat::Rg16sint:
        return MTLPixelFormatRG16Sint;
    case WebGPU::TextureFormat::Rg16float:
        return MTLPixelFormatRG16Float;
    case WebGPU::TextureFormat::Rgba8unorm:
        return MTLPixelFormatRGBA8Unorm;
    case WebGPU::TextureFormat::Rgba8unormSRGB:
        return MTLPixelFormatRGBA8Unorm_sRGB;
    case WebGPU::TextureFormat::Rgba8snorm:
        return MTLPixelFormatRGBA8Snorm;
    case WebGPU::TextureFormat::Rgba8uint:
        return MTLPixelFormatRGBA8Uint;
    case WebGPU::TextureFormat::Rgba8sint:
        return MTLPixelFormatRGBA8Sint;
    case WebGPU::TextureFormat::Bgra8unorm:
        return MTLPixelFormatBGRA8Unorm;
    case WebGPU::TextureFormat::Bgra8unormSRGB:
        return MTLPixelFormatBGRA8Unorm_sRGB;
    case WebGPU::TextureFormat::Rgb10a2unorm:
        return MTLPixelFormatRGB10A2Unorm;
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return MTLPixelFormatRG11B10Float;
    case WebGPU::TextureFormat::Rgb9e5ufloat:
        return MTLPixelFormatRGB9E5Float;
    case WebGPU::TextureFormat::Rgb10a2uint:
        return MTLPixelFormatRGB10A2Uint;
    case WebGPU::TextureFormat::Rg32float:
        return MTLPixelFormatRG32Float;
    case WebGPU::TextureFormat::Rg32uint:
        return MTLPixelFormatRG32Uint;
    case WebGPU::TextureFormat::Rg32sint:
        return MTLPixelFormatRG32Sint;
    case WebGPU::TextureFormat::Rgba16unorm:
        return MTLPixelFormatRGBA16Unorm;
    case WebGPU::TextureFormat::Rgba16snorm:
        return MTLPixelFormatRGBA16Snorm;
    case WebGPU::TextureFormat::Rgba16uint:
        return MTLPixelFormatRGBA16Uint;
    case WebGPU::TextureFormat::Rgba16sint:
        return MTLPixelFormatRGBA16Sint;
    case WebGPU::TextureFormat::Rgba16float:
        return MTLPixelFormatRGBA16Float;
    case WebGPU::TextureFormat::Rgba32float:
        return MTLPixelFormatRGBA32Float;
    case WebGPU::TextureFormat::Rgba32uint:
        return MTLPixelFormatRGBA32Uint;
    case WebGPU::TextureFormat::Rgba32sint:
        return MTLPixelFormatRGBA32Sint;
    case WebGPU::TextureFormat::Stencil8:
        return MTLPixelFormatStencil8;
    case WebGPU::TextureFormat::Depth16unorm:
        return MTLPixelFormatDepth16Unorm;
    case WebGPU::TextureFormat::Depth24plus:
        return MTLPixelFormatDepth32Float;
    case WebGPU::TextureFormat::Depth24plusStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
    case WebGPU::TextureFormat::Depth32float:
        return MTLPixelFormatDepth32Float;
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
        return MTLPixelFormatETC2_RGB8;
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
        return MTLPixelFormatETC2_RGB8_sRGB;
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
        return MTLPixelFormatETC2_RGB8A1;
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
        return MTLPixelFormatETC2_RGB8A1_sRGB;
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
        return MTLPixelFormatEAC_RGBA8;
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
        return MTLPixelFormatEAC_RGBA8_sRGB;
    case WebGPU::TextureFormat::EacR11unorm:
        return MTLPixelFormatEAC_R11Unorm;
    case WebGPU::TextureFormat::EacR11snorm:
        return MTLPixelFormatEAC_R11Snorm;
    case WebGPU::TextureFormat::EacRg11unorm:
        return MTLPixelFormatEAC_RG11Unorm;
    case WebGPU::TextureFormat::EacRg11snorm:
        return MTLPixelFormatEAC_RG11Snorm;
    case WebGPU::TextureFormat::Astc4x4Unorm:
        return MTLPixelFormatASTC_4x4_LDR;
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
        return MTLPixelFormatASTC_4x4_sRGB;
    case WebGPU::TextureFormat::Astc5x4Unorm:
        return MTLPixelFormatASTC_5x4_LDR;
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
        return MTLPixelFormatASTC_5x4_sRGB;
    case WebGPU::TextureFormat::Astc5x5Unorm:
        return MTLPixelFormatASTC_5x5_LDR;
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
        return MTLPixelFormatASTC_5x5_sRGB;
    case WebGPU::TextureFormat::Astc6x5Unorm:
        return MTLPixelFormatASTC_6x5_LDR;
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
        return MTLPixelFormatASTC_6x5_sRGB;
    case WebGPU::TextureFormat::Astc6x6Unorm:
        return MTLPixelFormatASTC_6x6_LDR;
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
        return MTLPixelFormatASTC_6x6_sRGB;
    case WebGPU::TextureFormat::Astc8x5Unorm:
        return MTLPixelFormatASTC_8x5_LDR;
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
        return MTLPixelFormatASTC_8x5_sRGB;
    case WebGPU::TextureFormat::Astc8x6Unorm:
        return MTLPixelFormatASTC_8x6_LDR;
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
        return MTLPixelFormatASTC_8x6_sRGB;
    case WebGPU::TextureFormat::Astc8x8Unorm:
        return MTLPixelFormatASTC_8x8_LDR;
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
        return MTLPixelFormatASTC_8x8_sRGB;
    case WebGPU::TextureFormat::Astc10x5Unorm:
        return MTLPixelFormatASTC_10x5_LDR;
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
        return MTLPixelFormatASTC_10x5_sRGB;
    case WebGPU::TextureFormat::Astc10x6Unorm:
        return MTLPixelFormatASTC_10x6_LDR;
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
        return MTLPixelFormatASTC_10x6_sRGB;
    case WebGPU::TextureFormat::Astc10x8Unorm:
        return MTLPixelFormatASTC_10x8_LDR;
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
        return MTLPixelFormatASTC_10x8_sRGB;
    case WebGPU::TextureFormat::Astc10x10Unorm:
        return MTLPixelFormatASTC_10x10_LDR;
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
        return MTLPixelFormatASTC_10x10_sRGB;
    case WebGPU::TextureFormat::Astc12x10Unorm:
        return MTLPixelFormatASTC_12x10_LDR;
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
        return MTLPixelFormatASTC_12x10_sRGB;
    case WebGPU::TextureFormat::Astc12x12Unorm:
        return MTLPixelFormatASTC_12x12_LDR;
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return MTLPixelFormatASTC_12x12_sRGB;
#if !PLATFORM(WATCHOS)
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
        return MTLPixelFormatBC1_RGBA;
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
        return MTLPixelFormatBC1_RGBA_sRGB;
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
        return MTLPixelFormatBC2_RGBA;
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
        return MTLPixelFormatBC2_RGBA_sRGB;
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
        return MTLPixelFormatBC3_RGBA;
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
        return MTLPixelFormatBC3_RGBA_sRGB;
    case WebGPU::TextureFormat::Bc4RUnorm:
        return MTLPixelFormatBC4_RUnorm;
    case WebGPU::TextureFormat::Bc4RSnorm:
        return MTLPixelFormatBC4_RSnorm;
    case WebGPU::TextureFormat::Bc5RgUnorm:
        return MTLPixelFormatBC5_RGUnorm;
    case WebGPU::TextureFormat::Bc5RgSnorm:
        return MTLPixelFormatBC5_RGSnorm;
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
        return MTLPixelFormatBC6H_RGBUfloat;
    case WebGPU::TextureFormat::Bc6hRgbFloat:
        return MTLPixelFormatBC6H_RGBFloat;
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
        return MTLPixelFormatBC7_RGBAUnorm;
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return MTLPixelFormatBC7_RGBAUnorm_sRGB;
#else
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return MTLPixelFormatInvalid;
#endif
    }
}

std::optional<WebGPU::TextureFormat> Texture::textureFormat(MTLPixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case MTLPixelFormatR8Unorm:
        return WebGPU::TextureFormat::R8unorm;
    case MTLPixelFormatR8Snorm:
        return WebGPU::TextureFormat::R8snorm;
    case MTLPixelFormatR8Uint:
        return WebGPU::TextureFormat::R8uint;
    case MTLPixelFormatR8Sint:
        return WebGPU::TextureFormat::R8sint;
    case MTLPixelFormatR16Uint:
        return WebGPU::TextureFormat::R16uint;
    case MTLPixelFormatR16Sint:
        return WebGPU::TextureFormat::R16sint;
    case MTLPixelFormatR16Float:
        return WebGPU::TextureFormat::R16float;
    case MTLPixelFormatRG8Unorm:
        return WebGPU::TextureFormat::Rg8unorm;
    case MTLPixelFormatRG8Snorm:
        return WebGPU::TextureFormat::Rg8snorm;
    case MTLPixelFormatRG8Uint:
        return WebGPU::TextureFormat::Rg8uint;
    case MTLPixelFormatRG8Sint:
        return WebGPU::TextureFormat::Rg8sint;
    case MTLPixelFormatR32Float:
        return WebGPU::TextureFormat::R32float;
    case MTLPixelFormatR32Uint:
        return WebGPU::TextureFormat::R32uint;
    case MTLPixelFormatR32Sint:
        return WebGPU::TextureFormat::R32sint;
    case MTLPixelFormatRG16Uint:
        return WebGPU::TextureFormat::Rg16uint;
    case MTLPixelFormatRG16Sint:
        return WebGPU::TextureFormat::Rg16sint;
    case MTLPixelFormatRG16Float:
        return WebGPU::TextureFormat::Rg16float;
    case MTLPixelFormatRGBA8Unorm:
        return WebGPU::TextureFormat::Rgba8unorm;
    case MTLPixelFormatRGBA8Unorm_sRGB:
        return WebGPU::TextureFormat::Rgba8unormSRGB;
    case MTLPixelFormatRGBA8Snorm:
        return WebGPU::TextureFormat::Rgba8snorm;
    case MTLPixelFormatRGBA8Uint:
        return WebGPU::TextureFormat::Rgba8uint;
    case MTLPixelFormatRGBA8Sint:
        return WebGPU::TextureFormat::Rgba8sint;
    case MTLPixelFormatBGRA8Unorm:
        return WebGPU::TextureFormat::Bgra8unorm;
    case MTLPixelFormatBGRA8Unorm_sRGB:
        return WebGPU::TextureFormat::Bgra8unormSRGB;
    case MTLPixelFormatRGB10A2Unorm:
        return WebGPU::TextureFormat::Rgb10a2unorm;
    case MTLPixelFormatRG11B10Float:
        return WebGPU::TextureFormat::Rg11b10ufloat;
    case MTLPixelFormatRGB9E5Float:
        return WebGPU::TextureFormat::Rgb9e5ufloat;
    case MTLPixelFormatRGB10A2Uint:
        return WebGPU::TextureFormat::Rgb10a2uint;
    case MTLPixelFormatRG32Float:
        return WebGPU::TextureFormat::Rg32float;
    case MTLPixelFormatRG32Uint:
        return WebGPU::TextureFormat::Rg32uint;
    case MTLPixelFormatRG32Sint:
        return WebGPU::TextureFormat::Rg32sint;
    case MTLPixelFormatRGBA16Uint:
        return WebGPU::TextureFormat::Rgba16uint;
    case MTLPixelFormatRGBA16Sint:
        return WebGPU::TextureFormat::Rgba16sint;
    case MTLPixelFormatRGBA16Float:
        return WebGPU::TextureFormat::Rgba16float;
    case MTLPixelFormatRGBA32Float:
        return WebGPU::TextureFormat::Rgba32float;
    case MTLPixelFormatRGBA32Uint:
        return WebGPU::TextureFormat::Rgba32uint;
    case MTLPixelFormatRGBA32Sint:
        return WebGPU::TextureFormat::Rgba32sint;
    case MTLPixelFormatStencil8:
        return WebGPU::TextureFormat::Stencil8;
    case MTLPixelFormatDepth16Unorm:
        return WebGPU::TextureFormat::Depth16unorm;
    case MTLPixelFormatDepth32Float:
        return WebGPU::TextureFormat::Depth24plus;
    case MTLPixelFormatDepth32Float_Stencil8:
        return WebGPU::TextureFormat::Depth24plusStencil8;
    case MTLPixelFormatETC2_RGB8:
        return WebGPU::TextureFormat::Etc2Rgb8unorm;
    case MTLPixelFormatETC2_RGB8_sRGB:
        return WebGPU::TextureFormat::Etc2Rgb8unormSRGB;
    case MTLPixelFormatETC2_RGB8A1:
        return WebGPU::TextureFormat::Etc2Rgb8a1unorm;
    case MTLPixelFormatETC2_RGB8A1_sRGB:
        return WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB;
    case MTLPixelFormatEAC_RGBA8:
        return WebGPU::TextureFormat::Etc2Rgba8unorm;
    case MTLPixelFormatEAC_RGBA8_sRGB:
        return WebGPU::TextureFormat::Etc2Rgba8unormSRGB;
    case MTLPixelFormatEAC_R11Unorm:
        return WebGPU::TextureFormat::EacR11unorm;
    case MTLPixelFormatEAC_R11Snorm:
        return WebGPU::TextureFormat::EacR11snorm;
    case MTLPixelFormatEAC_RG11Unorm:
        return WebGPU::TextureFormat::EacRg11unorm;
    case MTLPixelFormatEAC_RG11Snorm:
        return WebGPU::TextureFormat::EacRg11snorm;
    case MTLPixelFormatASTC_4x4_LDR:
        return WebGPU::TextureFormat::Astc4x4Unorm;
    case MTLPixelFormatASTC_4x4_sRGB:
        return WebGPU::TextureFormat::Astc4x4UnormSRGB;
    case MTLPixelFormatASTC_5x4_LDR:
        return WebGPU::TextureFormat::Astc5x4Unorm;
    case MTLPixelFormatASTC_5x4_sRGB:
        return WebGPU::TextureFormat::Astc5x4UnormSRGB;
    case MTLPixelFormatASTC_5x5_LDR:
        return WebGPU::TextureFormat::Astc5x5Unorm;
    case MTLPixelFormatASTC_5x5_sRGB:
        return WebGPU::TextureFormat::Astc5x5UnormSRGB;
    case MTLPixelFormatASTC_6x5_LDR:
        return WebGPU::TextureFormat::Astc6x5Unorm;
    case MTLPixelFormatASTC_6x5_sRGB:
        return WebGPU::TextureFormat::Astc6x5UnormSRGB;
    case MTLPixelFormatASTC_6x6_LDR:
        return WebGPU::TextureFormat::Astc6x6Unorm;
    case MTLPixelFormatASTC_6x6_sRGB:
        return WebGPU::TextureFormat::Astc6x6UnormSRGB;
    case MTLPixelFormatASTC_8x5_LDR:
        return WebGPU::TextureFormat::Astc8x5Unorm;
    case MTLPixelFormatASTC_8x5_sRGB:
        return WebGPU::TextureFormat::Astc8x5UnormSRGB;
    case MTLPixelFormatASTC_8x6_LDR:
        return WebGPU::TextureFormat::Astc8x6Unorm;
    case MTLPixelFormatASTC_8x6_sRGB:
        return WebGPU::TextureFormat::Astc8x6UnormSRGB;
    case MTLPixelFormatASTC_8x8_LDR:
        return WebGPU::TextureFormat::Astc8x8Unorm;
    case MTLPixelFormatASTC_8x8_sRGB:
        return WebGPU::TextureFormat::Astc8x8UnormSRGB;
    case MTLPixelFormatASTC_10x5_LDR:
        return WebGPU::TextureFormat::Astc10x5Unorm;
    case MTLPixelFormatASTC_10x5_sRGB:
        return WebGPU::TextureFormat::Astc10x5UnormSRGB;
    case MTLPixelFormatASTC_10x6_LDR:
        return WebGPU::TextureFormat::Astc10x6Unorm;
    case MTLPixelFormatASTC_10x6_sRGB:
        return WebGPU::TextureFormat::Astc10x6UnormSRGB;
    case MTLPixelFormatASTC_10x8_LDR:
        return WebGPU::TextureFormat::Astc10x8Unorm;
    case MTLPixelFormatASTC_10x8_sRGB:
        return WebGPU::TextureFormat::Astc10x8UnormSRGB;
    case MTLPixelFormatASTC_10x10_LDR:
        return WebGPU::TextureFormat::Astc10x10Unorm;
    case MTLPixelFormatASTC_10x10_sRGB:
        return WebGPU::TextureFormat::Astc10x10UnormSRGB;
    case MTLPixelFormatASTC_12x10_LDR:
        return WebGPU::TextureFormat::Astc12x10Unorm;
    case MTLPixelFormatASTC_12x10_sRGB:
        return WebGPU::TextureFormat::Astc12x10UnormSRGB;
    case MTLPixelFormatASTC_12x12_LDR:
        return WebGPU::TextureFormat::Astc12x12Unorm;
    case MTLPixelFormatASTC_12x12_sRGB:
        return WebGPU::TextureFormat::Astc12x12UnormSRGB;
#if !PLATFORM(WATCHOS)
    case MTLPixelFormatBC1_RGBA:
        return WebGPU::TextureFormat::Bc1RgbaUnorm;
    case MTLPixelFormatBC1_RGBA_sRGB:
        return WebGPU::TextureFormat::Bc1RgbaUnormSRGB;
    case MTLPixelFormatBC2_RGBA:
        return WebGPU::TextureFormat::Bc2RgbaUnorm;
    case MTLPixelFormatBC2_RGBA_sRGB:
        return WebGPU::TextureFormat::Bc2RgbaUnormSRGB;
    case MTLPixelFormatBC3_RGBA:
        return WebGPU::TextureFormat::Bc3RgbaUnorm;
    case MTLPixelFormatBC3_RGBA_sRGB:
        return WebGPU::TextureFormat::Bc3RgbaUnormSRGB;
    case MTLPixelFormatBC4_RUnorm:
        return WebGPU::TextureFormat::Bc4RUnorm;
    case MTLPixelFormatBC4_RSnorm:
        return WebGPU::TextureFormat::Bc4RSnorm;
    case MTLPixelFormatBC5_RGUnorm:
        return WebGPU::TextureFormat::Bc5RgUnorm;
    case MTLPixelFormatBC5_RGSnorm:
        return WebGPU::TextureFormat::Bc5RgSnorm;
    case MTLPixelFormatBC6H_RGBUfloat:
        return WebGPU::TextureFormat::Bc6hRgbUfloat;
    case MTLPixelFormatBC6H_RGBFloat:
        return WebGPU::TextureFormat::Bc6hRgbFloat;
    case MTLPixelFormatBC7_RGBAUnorm:
        return WebGPU::TextureFormat::Bc7RgbaUnorm;
    case MTLPixelFormatBC7_RGBAUnorm_sRGB:
        return WebGPU::TextureFormat::Bc7RgbaUnormSRGB;
#endif
    case MTLPixelFormatInvalid:
    default:
        return std::nullopt;
    }
}

NSUInteger Texture::bytesPerRow(WebGPU::TextureFormat format, uint32_t textureWidth, uint32_t sampleCount)
{
    NSUInteger blockWidth = Texture::texelBlockWidth(format);
    if (!blockWidth) {
        ASSERT_NOT_REACHED();
        return NSUIntegerMax;
    }
    NSUInteger add = 0;
    if (textureWidth % blockWidth)
        add = 1;

    NSUInteger blocksInWidth = textureWidth / blockWidth + add;
    auto product = checkedProduct<NSUInteger>(Texture::texelBlockSize(format), blocksInWidth, sampleCount);
    return product.hasOverflowed() ? NSUIntegerMax : product.value();
}

Checked<uint32_t> Texture::texelBlockSize(WebGPU::TextureFormat format) // Bytes
{
    // For depth-stencil textures, the input value to this function
    // needs to be the output of aspectSpecificFormat().
    ASSERT(!containsDepthAspect(format) || !containsStencilAspect(format));
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
        return 1;
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
        return 2;
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
        return 4;
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
        return 8;
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        return 16;
    case WebGPU::TextureFormat::Stencil8:
        return 1;
    case WebGPU::TextureFormat::Depth16unorm:
        return 2;
    case WebGPU::TextureFormat::Depth24plus:
        ASSERT(pixelFormat(format) == MTLPixelFormatDepth32Float);
        return 4;
    case WebGPU::TextureFormat::Depth24plusStencil8:
        ASSERT_NOT_REACHED();
        return 0;
    case WebGPU::TextureFormat::Depth32float:
        return 4;
    case WebGPU::TextureFormat::Depth32floatStencil8:
        ASSERT_NOT_REACHED();
        return 0;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
        return 8;
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
        return 16;
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
        return 16;
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
        return 8;
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
        return 16;
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
        return 16;
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return 16;
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
        return 8;
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
        return 8;
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
        return 16;
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return 16;
    }
}

std::optional<WebGPU::TextureFormat> Texture::aspectSpecificFormat(WebGPU::TextureFormat format, WebGPU::TextureAspect aspect)
{
    // https://gpuweb.github.io/gpuweb/#aspect-specific-format

    switch (aspect) {
    case WebGPU::TextureAspect::All:
        return format;
    case WebGPU::TextureAspect::StencilOnly:
        return stencilSpecificFormat(format);
    case WebGPU::TextureAspect::DepthOnly:
        return depthSpecificFormat(format);
    }
}

std::optional<MTLPixelFormat> Texture::depthOnlyAspectMetalFormat(WebGPU::TextureFormat textureFormat)
{
    switch (textureFormat) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
    case WebGPU::TextureFormat::Stencil8:
        return std::nullopt;
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        // This is a bit surprising, but it should be correct.
        // When using the view as a render target, we'll bind it to MTLRenderPassDescriptor.depthAttachment, which will ignore the stencil bits.
        // When attaching the view as a shader resource view, only the depth aspect is visible anyway.
        // When copying to/from the texture, we can use MTLBlitOption.MTLBlitOptionDepthFromDepthStencil to ignore the stencil bits.
        return pixelFormat(textureFormat);
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return std::nullopt;
    }
}

std::optional<MTLPixelFormat> Texture::stencilOnlyAspectMetalFormat(WebGPU::TextureFormat textureFormat)
{
    switch (textureFormat) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        return std::nullopt;
    case WebGPU::TextureFormat::Stencil8:
        return pixelFormat(textureFormat);
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
        return std::nullopt;
    case WebGPU::TextureFormat::Depth24plusStencil8:
        ASSERT(pixelFormat(textureFormat) == MTLPixelFormatDepth32Float_Stencil8);
        return MTLPixelFormatX32_Stencil8;
    case WebGPU::TextureFormat::Depth32float:
        return std::nullopt;
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return MTLPixelFormatX32_Stencil8;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return std::nullopt;
    }
}

static MTLStorageMode LEGACY_NODELETE storageMode(bool deviceHasUnifiedMemory, bool supportsNonPrivateDepthStencilTextures, bool supportsMemorylessTextures, OptionSet<WebGPU::TextureUsage> usage)
{
    if (usage.contains(WebGPU::TextureUsage::Transient))
        return supportsMemorylessTextures ? MTLStorageModeMemoryless : MTLStorageModePrivate;

    // FIXME: only perform this check if the texture is a depth/stencil texture.
    if (!supportsNonPrivateDepthStencilTextures)
        return MTLStorageModePrivate;

    if (deviceHasUnifiedMemory)
        return MTLStorageModeShared;

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return MTLStorageModeManaged;
    ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return MTLStorageModePrivate;
#endif
}

RefPtr<WebGPU::Texture> Device::createTexture(const WebGPU::TextureDescriptor& descriptor)
{
    if (!isValid())
        return Texture::createInvalid(*this);

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createtexture

    if (NSString *error = errorValidatingTextureCreation(descriptor)) {
        generateAValidationError(error);
        return Texture::createInvalid(*this);
    }

    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor new];

    auto format = descriptor.format;
    textureDescriptor.usage = Texture::usage(descriptor.usage, format);

    switch (descriptor.dimension) {
    case WebGPU::TextureDimension::_1d:
        textureDescriptor.width = descriptor.size.width;
        if (descriptor.size.depthOrArrayLayers > 1) {
            textureDescriptor.textureType = MTLTextureType1DArray;
            textureDescriptor.arrayLength = descriptor.size.depthOrArrayLayers;
        } else
            textureDescriptor.textureType = MTLTextureType1D;
        break;
    case WebGPU::TextureDimension::_2d:
        textureDescriptor.width = descriptor.size.width;
        textureDescriptor.height = descriptor.size.height;
        if (descriptor.size.depthOrArrayLayers > 1) {
            textureDescriptor.arrayLength = descriptor.size.depthOrArrayLayers;
            if (descriptor.sampleCount > 1) {
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
                return Texture::createInvalid(*this);
#else
                textureDescriptor.textureType = MTLTextureType2DMultisampleArray;
#endif
            } else
                textureDescriptor.textureType = MTLTextureType2DArray;
        } else {
            if (descriptor.sampleCount > 1)
                textureDescriptor.textureType = MTLTextureType2DMultisample;
            else
                textureDescriptor.textureType = MTLTextureType2D;
        }
        break;
    case WebGPU::TextureDimension::_3d:
        textureDescriptor.width = descriptor.size.width;
        textureDescriptor.height = descriptor.size.height;
        textureDescriptor.depth = descriptor.size.depthOrArrayLayers;
        textureDescriptor.textureType = MTLTextureType3D;
        break;
    }

    textureDescriptor.pixelFormat = Texture::pixelFormat(format);
    if (textureDescriptor.pixelFormat == MTLPixelFormatInvalid) {
        generateAValidationError("GPUDevice.createTexture: invalid texture format"_s);
        return Texture::createInvalid(*this);
    }

    textureDescriptor.mipmapLevelCount = descriptor.mipLevelCount;

    textureDescriptor.sampleCount = descriptor.sampleCount;

    textureDescriptor.storageMode = storageMode(hasUnifiedMemory(), baseCapabilities().supportsNonPrivateDepthStencilTextures, baseCapabilities().supportsMemorylessTextures, descriptor.usage);

    // FIXME(PERFORMANCE): Consider write-combining CPU cache mode.
    // FIXME(PERFORMANCE): Consider implementing hazard tracking ourself.

    id<MTLTexture> texture = [m_device newTextureWithDescriptor:textureDescriptor];

    if (!texture) {
        generateAnOutOfMemoryError("out of memory"_s);
        return Texture::createInvalid(*this);
    }

    setOwnerWithIdentity(texture);
    texture.label = descriptor.label.createNSString().get();

    Ref result = Texture::create(texture, descriptor, Vector<WebGPU::TextureFormat> { descriptor.viewFormats }, *this);
    addTexture(result);
    return result;
}

Texture::Texture(id<MTLTexture> texture, const WebGPU::TextureDescriptor& descriptor, Vector<WebGPU::TextureFormat>&& viewFormats, Device& device)
    : m_texture(texture)
    , m_width(descriptor.size.width)
    , m_height(descriptor.size.height)
    , m_depthOrArrayLayers(descriptor.size.depthOrArrayLayers)
    , m_mipLevelCount(descriptor.mipLevelCount)
    , m_sampleCount(descriptor.sampleCount)
    , m_dimension(descriptor.dimension)
    , m_format(descriptor.format)
    , m_usage(descriptor.usage)
    , m_viewFormats(WTF::move(viewFormats))
    , m_device(device)
{
}

Texture::Texture(Device& device)
    : m_device(device)
{
}

Texture::~Texture() = default;

std::optional<ResolvedTextureViewDescriptor> Texture::resolveTextureViewDescriptorDefaults(const WebGPU::TextureViewDescriptor& descriptor) const
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-resolving-gputextureviewdescriptor-defaults

    // Only invalid textures have no format, and createView() does not resolve their views.
    if (!m_format)
        return std::nullopt;

    auto format = descriptor.format;
    if (!format)
        format = Texture::resolveTextureFormat(*m_format, descriptor.aspect).value_or(*m_format);

    auto mipLevelCount = descriptor.mipLevelCount;
    if (!mipLevelCount) {
        auto remainingMipLevelCount = checkedDifference<uint32_t>(m_mipLevelCount, descriptor.baseMipLevel);
        if (remainingMipLevelCount.hasOverflowed())
            return std::nullopt;
        mipLevelCount = remainingMipLevelCount.value();
    }

    auto dimension = descriptor.dimension;
    if (!dimension) {
        switch (m_texture.textureType) {
        case MTLTextureType1D:
            dimension = WebGPU::TextureViewDimension::_1d;
            break;
        case MTLTextureType1DArray:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case MTLTextureType2D:
        case MTLTextureType2DMultisample:
            dimension = WebGPU::TextureViewDimension::_2d;
            break;
        case MTLTextureType2DArray:
        case MTLTextureType2DMultisampleArray:
            dimension = WebGPU::TextureViewDimension::_2dArray;
            break;
        case MTLTextureTypeCube:
            dimension = WebGPU::TextureViewDimension::Cube;
            break;
        case MTLTextureTypeCubeArray:
            dimension = WebGPU::TextureViewDimension::CubeArray;
            break;
        case MTLTextureType3D:
            dimension = WebGPU::TextureViewDimension::_3d;
            break;
        case MTLTextureTypeTextureBuffer:
        default:
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    }

    auto arrayLayerCount = descriptor.arrayLayerCount;
    if (!arrayLayerCount) {
        switch (*dimension) {
        case WebGPU::TextureViewDimension::_1d:
        case WebGPU::TextureViewDimension::_2d:
        case WebGPU::TextureViewDimension::_3d:
            arrayLayerCount = 1;
            break;
        case WebGPU::TextureViewDimension::Cube:
            arrayLayerCount = 6;
            break;
        case WebGPU::TextureViewDimension::_2dArray:
        case WebGPU::TextureViewDimension::CubeArray: {
            auto remainingArrayLayerCount = checkedDifference<uint32_t>(m_depthOrArrayLayers, descriptor.baseArrayLayer);
            if (remainingArrayLayerCount.hasOverflowed())
                return std::nullopt;
            arrayLayerCount = remainingArrayLayerCount.value();
            break;
        }
        }
    }

    return ResolvedTextureViewDescriptor {
        .format = *format,
        .dimension = *dimension,
        .baseMipLevel = descriptor.baseMipLevel,
        .mipLevelCount = *mipLevelCount,
        .baseArrayLayer = descriptor.baseArrayLayer,
        .arrayLayerCount = *arrayLayerCount,
        .aspect = descriptor.aspect,
        // An empty usage means the view inherits every usage of the texture it is a view of.
        .usage = descriptor.usage.isEmpty() ? m_usage : descriptor.usage,
    };
}

std::optional<WebGPU::TextureFormat> Texture::resolveTextureFormat(WebGPU::TextureFormat format, WebGPU::TextureAspect aspect)
{
    switch (aspect) {
    case WebGPU::TextureAspect::All:
        return format;
    case WebGPU::TextureAspect::DepthOnly:
        return depthSpecificFormat(format);
    case WebGPU::TextureAspect::StencilOnly:
        return stencilSpecificFormat(format);
    default:
        return { };
    }
}

uint32_t Texture::arrayLayerCount() const
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-array-layer-count

    switch (m_dimension) {
    case WebGPU::TextureDimension::_1d:
        return 1;
    case WebGPU::TextureDimension::_2d:
        return m_depthOrArrayLayers;
    case WebGPU::TextureDimension::_3d:
        return 1;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

WebGPU::TextureDimension Texture::dimension() const
{
    return m_dimension;
}

WebGPU::TextureFormat Texture::format() const
{
    return m_format.value_or(WebGPU::TextureFormat::Rgba8unorm);
}

NSString* Texture::errorValidatingTextureViewCreation(const ResolvedTextureViewDescriptor& descriptor) const
{
#define ERROR_STRING(...) ([NSString stringWithFormat:@"GPUTexture.createView: %@", __VA_ARGS__])
    ASSERT(isValid());

    if (descriptor.aspect == WebGPU::TextureAspect::All) {
        if (descriptor.format != m_format && !m_viewFormats.contains(descriptor.format))
            return ERROR_STRING(@"aspect == all and (format != parentTexture's format and !viewFormats.contains(parentTexture's format))");
    } else {
        if (descriptor.format != Texture::resolveTextureFormat(*m_format, descriptor.aspect))
            return ERROR_STRING(@"aspect == All and (format != resolveTextureFormat(format, aspect))");
    }

    // The view's usage narrows the texture's, and each usage it keeps has to be supported by the
    // view's own format rather than by the format of the texture it is a view of.
    if (!m_usage.containsAll(descriptor.usage))
        return ERROR_STRING([NSString stringWithFormat:@"view usage(%llu) is not a subset of the texture's usage(%llu)", toAPI(descriptor.usage), toAPI(m_usage)]);

    // Transient textures never leave tile memory, so a view of one cannot drop any of the
    // texture's usages. A zero usage has already been resolved to the texture's usage above.
    if (m_usage.contains(WebGPU::TextureUsage::Transient) && descriptor.usage != m_usage)
        return ERROR_STRING([NSString stringWithFormat:@"view usage(%llu) must be either zero or exactly the transient texture's usage(%llu)", toAPI(descriptor.usage), toAPI(m_usage)]);

    auto format = descriptor.format;

    if (descriptor.usage.contains(WebGPU::TextureUsage::StorageBinding) && !hasStorageBindingCapability(format, m_device, WebGPU::StorageTextureAccess::WriteOnly))
        return ERROR_STRING(@"view usage contains storage binding and the view's format does not support it");

    if (descriptor.usage.contains(WebGPU::TextureUsage::RenderAttachment) && !isDepthOrStencilFormat(format) && !isColorRenderableFormat(format, m_device))
        return ERROR_STRING(@"view usage contains render attachment and the view's format is not color renderable");

    if (!descriptor.mipLevelCount)
        return ERROR_STRING(@"!mipLevelCount");

    auto endMipLevel = checkedSum<uint32_t>(descriptor.baseMipLevel, descriptor.mipLevelCount);
    if (endMipLevel.hasOverflowed() || endMipLevel.value() > m_mipLevelCount)
        return ERROR_STRING(@"endMipLevel is not valid");

    if (!descriptor.arrayLayerCount)
        return ERROR_STRING(@"!arrayLayerCount");

    auto endArrayLayer = checkedSum<uint32_t>(descriptor.baseArrayLayer, descriptor.arrayLayerCount);
    if (endArrayLayer.hasOverflowed() || endArrayLayer.value() > arrayLayerCount())
        return ERROR_STRING([NSString stringWithFormat:@"endArrayLayer(%u) is not valid. Base texture array count is %u", endArrayLayer.value(), arrayLayerCount()]);

    if (m_sampleCount > 1) {
        if (descriptor.dimension != WebGPU::TextureViewDimension::_2d)
            return ERROR_STRING(@"sampleCount > 1 and dimension != 2D");
    }

    switch (descriptor.dimension) {
    case WebGPU::TextureViewDimension::_1d:
        if (m_dimension != WebGPU::TextureDimension::_1d)
            return ERROR_STRING(@"attempting to create 1D texture view from non-1D base texture");

        if (descriptor.arrayLayerCount != 1)
            return ERROR_STRING(@"attempting to create 1D texture view with array layers");
        break;
    case WebGPU::TextureViewDimension::_2d:
        if (m_dimension != WebGPU::TextureDimension::_2d)
            return ERROR_STRING(@"attempting to create 2D texture view from non-2D base texture");

        if (descriptor.arrayLayerCount != 1)
            return ERROR_STRING(@"attempting to create 2D texture view with array layers");
        break;
    case WebGPU::TextureViewDimension::_2dArray:
        if (m_dimension != WebGPU::TextureDimension::_2d)
            return ERROR_STRING(@"attempting to create 2D texture array view from non-2D parent texture");
        break;
    case WebGPU::TextureViewDimension::Cube:
        if (m_dimension != WebGPU::TextureDimension::_2d)
            return ERROR_STRING(@"attempting to create cube texture view from non-2D parent texture");

        if (descriptor.arrayLayerCount != 6)
            return ERROR_STRING(@"attempting to create cube texture view with arrayLayerCount != 6");

        if (m_width != m_height)
            return ERROR_STRING(@"attempting to create cube texture view from non-square parent texture");
        break;
    case WebGPU::TextureViewDimension::CubeArray:
        if (m_dimension != WebGPU::TextureDimension::_2d)
            return ERROR_STRING(@"attempting to create cube array texture view from non-2D parent texture");

        if (descriptor.arrayLayerCount % 6)
            return ERROR_STRING(@"attempting to create cube array texture view with (arrayLayerCount % 6) != 0");

        if (m_width != m_height)
            return ERROR_STRING(@"attempting to create cube array texture view from non-square parent texture");
        break;
    case WebGPU::TextureViewDimension::_3d:
        if (m_dimension != WebGPU::TextureDimension::_3d)
            return ERROR_STRING(@"attempting to create 3D texture view from non-3D parent texture");

        if (descriptor.arrayLayerCount != 1)
            return ERROR_STRING(@"attempting to create 3D texture view with array layers");
        break;
    }
#undef ERROR_STRING
    return nil;
}

static WebGPU::Extent3D computeRenderExtent(const WebGPU::Extent3D& baseSize, uint32_t mipLevel)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-compute-render-extent

    WebGPU::Extent3D extent { };

    extent.width = std::max(static_cast<uint32_t>(1), baseSize.width >> mipLevel);

    extent.height = std::max(static_cast<uint32_t>(1), baseSize.height >> mipLevel);

    extent.depthOrArrayLayers = 1;

    return extent;
}

static MTLPixelFormat LEGACY_NODELETE resolvedPixelFormat(MTLPixelFormat viewPixelFormat, MTLPixelFormat sourcePixelFormat)
{
    switch (viewPixelFormat) {
    case MTLPixelFormatStencil8:
        return sourcePixelFormat == MTLPixelFormatDepth32Float_Stencil8 ? MTLPixelFormatX32_Stencil8 : sourcePixelFormat;
    case MTLPixelFormatDepth32Float:
        return sourcePixelFormat;
    default:
        return viewPixelFormat;
    }
}

RefPtr<WebGPU::TextureView> Texture::createView(const std::optional<WebGPU::TextureViewDescriptor>& optionalDescriptor)
{
    auto device = m_device;

    if (m_destroyed)
        return TextureView::createInvalid(*this, device.get());

    // The texture-component-swizzle feature is not supported, so it can never be enabled.
    if (optionalDescriptor && optionalDescriptor->swizzle != WebGPU::TextureComponentSwizzle { }) {
        device->generateAValidationError("GPUTexture.createView: swizzle is not \"rgba\" and the texture-component-swizzle feature is not enabled"_s);
        return TextureView::createInvalid(*this, device.get());
    }

    // https://gpuweb.github.io/gpuweb/#dom-gputexture-createview

    if (!isValid()) {
        device->generateAValidationError("GPUTexture.createView: texture is not valid"_s);
        return TextureView::createInvalid(*this, device.get());
    }

    const WebGPU::TextureViewDescriptor defaultDescriptor;
    const auto& inputDescriptor = optionalDescriptor ? *optionalDescriptor : defaultDescriptor;

    auto descriptor = resolveTextureViewDescriptorDefaults(inputDescriptor);
    if (!descriptor) {
        device->generateAValidationError("Validation failure in GPUTexture.createView: descriptor could not be resolved"_s);
        return TextureView::createInvalid(*this, device.get());
    }

    if (NSString *error = errorValidatingTextureViewCreation(*descriptor)) {
        device->generateAValidationError(error);
        return TextureView::createInvalid(*this, device.get());
    }

    auto pixelFormat = Texture::pixelFormat(descriptor->format);
    if (pixelFormat == MTLPixelFormatInvalid) {
        device->generateAValidationError("GPUTexture.createView: invalid texture format"_s);
        return TextureView::createInvalid(*this, device.get());
    }

    if (!inputDescriptor.usage.isEmpty() && !m_usage.containsAll(inputDescriptor.usage)) {
        device->generateAValidationError([NSString stringWithFormat:@"GPUTexture.createView: when the view's usage(%llu) is specified it must be a subset of the Texture's usage(%llu)", toAPI(inputDescriptor.usage), toAPI(m_usage)]);
        return TextureView::createInvalid(*this, device.get());
    }

    MTLTextureType textureType;
    switch (descriptor->dimension) {
    case WebGPU::TextureViewDimension::_1d:
        if (descriptor->arrayLayerCount == 1)
            textureType = MTLTextureType1D;
        else
            textureType = MTLTextureType1DArray;
        break;
    case WebGPU::TextureViewDimension::_2d:
        if (m_sampleCount > 1)
            textureType = MTLTextureType2DMultisample;
        else
            textureType = MTLTextureType2D;
        break;
    case WebGPU::TextureViewDimension::_2dArray:
        if (m_sampleCount > 1) {
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
            return TextureView::createInvalid(*this, device.get());
#else
            textureType = MTLTextureType2DMultisampleArray;
#endif
        } else
            textureType = MTLTextureType2DArray;
        break;
    case WebGPU::TextureViewDimension::Cube:
        textureType = MTLTextureTypeCube;
        break;
    case WebGPU::TextureViewDimension::CubeArray:
        textureType = MTLTextureTypeCubeArray;
        break;
    case WebGPU::TextureViewDimension::_3d:
        textureType = MTLTextureType3D;
        break;
    }

    auto levels = NSMakeRange(descriptor->baseMipLevel, descriptor->mipLevelCount);

    auto slices = NSMakeRange(descriptor->baseArrayLayer, descriptor->arrayLayerCount);

    id<MTLTexture> texture = m_texture.storageMode == MTLStorageModeMemoryless ? m_texture : [m_texture newTextureViewWithPixelFormat:resolvedPixelFormat(pixelFormat, m_texture.pixelFormat) textureType:textureType levels:levels slices:slices];
    if (!texture)
        return TextureView::createInvalid(*this, device.get());

    texture.label = inputDescriptor.label.createNSString().get();
    if (!texture.label.length)
        texture.label = m_texture.label;

    std::optional<WebGPU::Extent3D> renderExtent;
    if (m_usage.contains(WebGPU::TextureUsage::RenderAttachment))
        renderExtent = computeRenderExtent({ m_width, m_height, m_depthOrArrayLayers }, descriptor->baseMipLevel);

    auto result = TextureView::create(texture, *descriptor, renderExtent, *this, device.get());
    m_textureViews.append(result);
    return result;
}

void Texture::recreateIfNeeded()
{
    if (!m_canvasBacking)
        return;

    m_destroyed = false;
    // Every one of these is the canvas handing this backing out for a new frame, either directly or
    // through the undestroy the Web process sends in place of the round trip it elides once the
    // render buffers wrap around. A frame starts as transparent black rather than holding whatever
    // the frame that last used this backing left behind, so forget having cleared it and let the
    // next use initialize it again.
    setPreviouslyCleared(0, 0, false);
    // The cost recorded against this backing belongs to the frame that is over, not the one starting now.
    resetGPUFrameCost();
}

void Texture::makeCanvasBacking()
{
    m_canvasBacking = true;
}

bool Texture::waitForCommandBufferCompletion()
{
    bool result = true;
    for (auto commandEncoder : m_commandEncoders) {
        if (RefPtr ptr = m_device->commandEncoderFromIdentifier(commandEncoder))
            result = ptr->waitForCommandBufferCompletion() && result;
    }

    return result;
}

void Texture::recordGPUExecutionWindow(double startTime, double endTime) const
{
    double duration = endTime - startTime;
    if (duration <= 0)
        return;
    Locker locker { m_gpuFrameCostLock };
    m_gpuFrameCostSeconds = std::max(m_gpuFrameCostSeconds, duration);
}

Seconds Texture::gpuFrameCost() const
{
    Locker locker { m_gpuFrameCostLock };
    return m_gpuFrameCostSeconds > 0 ? Seconds { m_gpuFrameCostSeconds } : 0_s;
}

void Texture::resetGPUFrameCost() const
{
    Locker locker { m_gpuFrameCostLock };
    m_gpuFrameCostSeconds = 0;
}

void Texture::setCommandEncoder(CommandEncoder& commandEncoder) const
{
    commandEncoder.trackEncoderForTexture(*this, m_commandEncoders);
    commandEncoder.addTexture(*this);
    if (!m_canvasBacking && isDestroyed())
        commandEncoder.makeSubmitInvalid();
}

ASCIILiteral Texture::formatToString(WebGPU::TextureFormat format)
{
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
        return "r8unorm"_s;
    case WebGPU::TextureFormat::R8snorm:
        return "r8snorm"_s;
    case WebGPU::TextureFormat::R8uint:
        return "r8uint"_s;
    case WebGPU::TextureFormat::R8sint:
        return "r8sint"_s;
    case WebGPU::TextureFormat::R16uint:
        return "r16uint"_s;
    case WebGPU::TextureFormat::R16sint:
        return "r16sint"_s;
    case WebGPU::TextureFormat::R16float:
        return "r16float"_s;
    case WebGPU::TextureFormat::Rg8unorm:
        return "rg8unorm"_s;
    case WebGPU::TextureFormat::Rg8snorm:
        return "rg8snorm"_s;
    case WebGPU::TextureFormat::Rg8uint:
        return "rg8uint"_s;
    case WebGPU::TextureFormat::Rg8sint:
        return "rg8sint"_s;
    case WebGPU::TextureFormat::R32float:
        return "r32float"_s;
    case WebGPU::TextureFormat::R32uint:
        return "r32uint"_s;
    case WebGPU::TextureFormat::R32sint:
        return "r32sint"_s;
    case WebGPU::TextureFormat::Rg16uint:
        return "rg16uint"_s;
    case WebGPU::TextureFormat::Rg16sint:
        return "rg16sint"_s;
    case WebGPU::TextureFormat::Rg16float:
        return "rg16float"_s;
    case WebGPU::TextureFormat::Rgba8unorm:
        return "rgba8unorm"_s;
    case WebGPU::TextureFormat::Rgba8unormSRGB:
        return "rgba8unorm-srgb"_s;
    case WebGPU::TextureFormat::Rgba8snorm:
        return "rgba8snorm"_s;
    case WebGPU::TextureFormat::Rgba8uint:
        return "rgba8uint"_s;
    case WebGPU::TextureFormat::Rgba8sint:
        return "rgba8sint"_s;
    case WebGPU::TextureFormat::Bgra8unorm:
        return "bgra8unorm"_s;
    case WebGPU::TextureFormat::Bgra8unormSRGB:
        return "bgra8unorm-srgb"_s;
    case WebGPU::TextureFormat::Rgb10a2uint:
        return "rgb10a2uint"_s;
    case WebGPU::TextureFormat::Rgb10a2unorm:
        return "rgb10a2unorm"_s;
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return "rg11b10ufloat"_s;
    case WebGPU::TextureFormat::Rgb9e5ufloat:
        return "rgb9e5ufloat"_s;
    case WebGPU::TextureFormat::Rg32float:
        return "rg32float"_s;
    case WebGPU::TextureFormat::Rg32uint:
        return "rg32uint"_s;
    case WebGPU::TextureFormat::Rg32sint:
        return "rg32sint"_s;
    case WebGPU::TextureFormat::Rgba16uint:
        return "rgba16uint"_s;
    case WebGPU::TextureFormat::Rgba16sint:
        return "rgba16sint"_s;
    case WebGPU::TextureFormat::Rgba16float:
        return "rgba16float"_s;
    case WebGPU::TextureFormat::Rgba32float:
        return "rgba32float"_s;
    case WebGPU::TextureFormat::Rgba32uint:
        return "rgba32uint"_s;
    case WebGPU::TextureFormat::Rgba32sint:
        return "rgba32sint"_s;
    case WebGPU::TextureFormat::Stencil8:
        return "stencil8"_s;
    case WebGPU::TextureFormat::Depth16unorm:
        return "depth16unorm"_s;
    case WebGPU::TextureFormat::Depth24plus:
        return "depth24plus"_s;
    case WebGPU::TextureFormat::Depth24plusStencil8:
        return "depth24plus-stencil8"_s;
    case WebGPU::TextureFormat::Depth32float:
        return "depth32float"_s;
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return "depth32float-stencil8"_s;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
        return "bc1-rgba-unorm"_s;
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
        return "bc1-rgba-unorm-srgb"_s;
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
        return "bc2-rgba-unorm"_s;
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
        return "bc2-rgba-unorm-srgb"_s;
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
        return "bc3-rgba-unorm"_s;
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
        return "bc3-rgba-unorm-srgb"_s;
    case WebGPU::TextureFormat::Bc4RUnorm:
        return "bc4-r-unorm"_s;
    case WebGPU::TextureFormat::Bc4RSnorm:
        return "bc4-r-snorm"_s;
    case WebGPU::TextureFormat::Bc5RgUnorm:
        return "bc5-rg-unorm"_s;
    case WebGPU::TextureFormat::Bc5RgSnorm:
        return "bc5-rg-snorm"_s;
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
        return "bc6h-rgb-ufloat"_s;
    case WebGPU::TextureFormat::Bc6hRgbFloat:
        return "bc6h-rgb-float"_s;
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
        return "bc7-rgba-unorm"_s;
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return "bc7-rgba-unorm-srgb"_s;
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
        return "etc2-rgb8unorm"_s;
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
        return "etc2-rgb8unorm-srgb"_s;
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
        return "etc2-rgb8a1unorm"_s;
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
        return "etc2-rgb8a1unorm-srgb"_s;
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
        return "etc2-rgba8unorm"_s;
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
        return "etc2-rgba8unorm-srgb"_s;
    case WebGPU::TextureFormat::EacR11unorm:
        return "eac-r11unorm"_s;
    case WebGPU::TextureFormat::EacR11snorm:
        return "eac-r11snorm"_s;
    case WebGPU::TextureFormat::EacRg11unorm:
        return "eac-rg11unorm"_s;
    case WebGPU::TextureFormat::EacRg11snorm:
        return "eac-rg11snorm"_s;
    case WebGPU::TextureFormat::Astc4x4Unorm:
        return "astc-4x4-unorm"_s;
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
        return "astc-4x4-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc5x4Unorm:
        return "astc-5x4-unorm"_s;
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
        return "astc-5x4-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc5x5Unorm:
        return "astc-5x5-unorm"_s;
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
        return "astc-5x5-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc6x5Unorm:
        return "astc-6x5-unorm"_s;
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
        return "astc-6x5-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc6x6Unorm:
        return "astc-6x6-unorm"_s;
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
        return "astc-6x6-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc8x5Unorm:
        return "astc-8x5-unorm"_s;
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
        return "astc-8x5-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc8x6Unorm:
        return "astc-8x6-unorm"_s;
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
        return "astc-8x6-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc8x8Unorm:
        return "astc-8x8-unorm"_s;
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
        return "astc-8x8-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc10x5Unorm:
        return "astc-10x5-unorm"_s;
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
        return "astc-10x5-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc10x6Unorm:
        return "astc-10x6-unorm"_s;
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
        return "astc-10x6-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc10x8Unorm:
        return "astc-10x8-unorm"_s;
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
        return "astc-10x8-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc10x10Unorm:
        return "astc-10x10-unorm"_s;
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
        return "astc-10x10-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc12x10Unorm:
        return "astc-12x10-unorm"_s;
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
        return "astc-12x10-unorm-srgb"_s;
    case WebGPU::TextureFormat::Astc12x12Unorm:
        return "astc-12x12-unorm"_s;
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return "astc-12x12-unorm-srgb"_s;
    case WebGPU::TextureFormat::R16unorm:
        return "r16unorm"_s;
    case WebGPU::TextureFormat::R16snorm:
        return "r16snorm"_s;
    case WebGPU::TextureFormat::Rg16unorm:
        return "rg16unorm"_s;
    case WebGPU::TextureFormat::Rg16snorm:
        return "rg16snorm"_s;
    case WebGPU::TextureFormat::Rgba16unorm:
        return "rgba16unorm"_s;
    case WebGPU::TextureFormat::Rgba16snorm:
        return "rgba16snorm"_s;
    }
}

void Texture::undestroy()
{
    recreateIfNeeded();
}

void Texture::destroy()
{
    // https://gpuweb.github.io/gpuweb/#dom-gputexture-destroy
    if (!m_canvasBacking)
        m_texture = protect(m_device)->placeholderTexture(format());
    m_destroyed = true;
    if (!m_canvasBacking) {
        for (auto& weakView : m_textureViews) {
            if (RefPtr view = weakView.get())
                view->destroy();
        }
        for (Ref bindGroup : m_bindGroups.values())
            bindGroup->releaseDestroyedResources();
        m_bindGroups.clear();
        for (Ref renderBundle : m_renderBundles.values())
            renderBundle->releaseDestroyedResources();
        m_renderBundles.clear();
    }
    if (!m_canvasBacking)
        m_device->makeSubmitInvalidClearingEncoders(m_commandEncoders);

    m_commandEncoders.clear();

    m_textureViews.clear();
}

void Texture::addResourceHolder(const BindGroup& bindGroup) const
{
    if (m_destroyed)
        return;

    m_bindGroups.add(bindGroup);
}

void Texture::addResourceHolder(const RenderBundle& renderBundle) const
{
    if (m_destroyed)
        return;

    m_renderBundles.add(renderBundle);
}

void Texture::setLabel(String&& label)
{
    m_texture.label = label.createNSString().get();
}

WebGPU::Extent3D Texture::logicalMiplevelSpecificTextureExtent(uint32_t mipLevel)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-logical-miplevel-specific-texture-extent

    switch (m_dimension) {
    case WebGPU::TextureDimension::_1d:
        return {
            .width = std::max(static_cast<uint32_t>(1), m_width >> mipLevel),
            .height = 1,
            .depthOrArrayLayers = m_depthOrArrayLayers };
    case WebGPU::TextureDimension::_2d:
        return {
            .width = std::max(static_cast<uint32_t>(1), m_width >> mipLevel),
            .height = std::max(static_cast<uint32_t>(1), m_height >> mipLevel),
            .depthOrArrayLayers = m_depthOrArrayLayers };
    case WebGPU::TextureDimension::_3d:
        return {
            .width = std::max(static_cast<uint32_t>(1), m_width >> mipLevel),
            .height = std::max(static_cast<uint32_t>(1), m_height >> mipLevel),
            .depthOrArrayLayers = std::max(static_cast<uint32_t>(1), m_depthOrArrayLayers >> mipLevel) };
    }
    RELEASE_ASSERT_NOT_REACHED();
}

WebGPU::Extent3D Texture::physicalMiplevelSpecificTextureExtent(uint32_t mipLevel)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-physical-miplevel-specific-texture-extent
    return physicalTextureExtent(dimension(), format(), logicalMiplevelSpecificTextureExtent(mipLevel));
}

WebGPU::Extent3D Texture::physicalTextureExtent(WebGPU::TextureDimension dimension, WebGPU::TextureFormat format, WebGPU::Extent3D logicalExtent)
{
    auto blockWidth = texelBlockWidth(format);
    auto blockHeight = texelBlockHeight(format);
    if (!blockWidth || !blockHeight) {
        return WebGPU::Extent3D {
            .width = 1,
            .height = 1,
            .depthOrArrayLayers = 1
        };
    }

    switch (dimension) {
    case WebGPU::TextureDimension::_1d:
        return {
            .width = roundUpToMultipleOfNonPowerOfTwo(blockWidth, logicalExtent.width),
            .height = 1,
            .depthOrArrayLayers = logicalExtent.depthOrArrayLayers };
    case WebGPU::TextureDimension::_2d:
        return {
            .width = roundUpToMultipleOfNonPowerOfTwo(blockWidth, logicalExtent.width),
            .height = roundUpToMultipleOfNonPowerOfTwo(blockHeight, logicalExtent.height),
            .depthOrArrayLayers = logicalExtent.depthOrArrayLayers };
    case WebGPU::TextureDimension::_3d:
        return {
            .width = roundUpToMultipleOfNonPowerOfTwo(blockWidth, logicalExtent.width),
            .height = roundUpToMultipleOfNonPowerOfTwo(blockHeight, logicalExtent.height),
            .depthOrArrayLayers = logicalExtent.depthOrArrayLayers };
    }
}

static WebGPU::Extent3D imageCopyTextureSubresourceSize(const WebGPU::TexelCopyTextureInfo& imageCopyTexture)
{
    // https://gpuweb.github.io/gpuweb/#imagecopytexture-subresource-size

    return protect(metal(imageCopyTexture.texture))->physicalMiplevelSpecificTextureExtent(imageCopyTexture.mipLevel);
}

NSString* Texture::errorValidatingImageCopyTexture(const WebGPU::TexelCopyTextureInfo& imageCopyTexture, const WebGPU::Extent3D& copySize)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpuimagecopytexture

    uint32_t blockWidth = Texture::texelBlockWidth(metal(imageCopyTexture.texture).format());

    uint32_t blockHeight = Texture::texelBlockHeight(metal(imageCopyTexture.texture).format());

    if (!metal(imageCopyTexture.texture).isValid())
        return @"imageCopyTexture is not valid";

    if (imageCopyTexture.mipLevel >= metal(imageCopyTexture.texture).mipLevelCount())
        return [NSString stringWithFormat:@"imageCopyTexture mip level(%u) is greater than or equal to the mipLevelCount(%u) in the texture", imageCopyTexture.mipLevel, metal(imageCopyTexture.texture).mipLevelCount()];

    if (imageCopyTexture.origin.x % blockWidth)
        return [NSString stringWithFormat:@"imageCopyTexture.origin.x(%u) is not a multiple of the texture blockWidth(%u)", imageCopyTexture.origin.x, blockWidth];

    if (imageCopyTexture.origin.y % blockHeight)
        return [NSString stringWithFormat:@"imageCopyTexture.origin.y(%u) is not a multiple of the texture blockHeight(%u)", imageCopyTexture.origin.y, blockHeight];

    if (Texture::isDepthOrStencilFormat(metal(imageCopyTexture.texture).format())
        || metal(imageCopyTexture.texture).sampleCount() > 1) {
        auto subresourceSize = imageCopyTextureSubresourceSize(imageCopyTexture);
        if (subresourceSize.width != copySize.width
            || (copySize.height > 1 && subresourceSize.height != copySize.height))
            return [NSString stringWithFormat:@"subresourceSize.width(%u) != copySize.width(%u) || subresourceSize.height(%u) != copySize.height(%u) || subresourceSize.depthOrArrayLayers(%u) != copySize.depthOrArrayLayers(%u)", subresourceSize.width, copySize.width, subresourceSize.height, copySize.height, subresourceSize.depthOrArrayLayers, copySize.depthOrArrayLayers];
    }

    return nil;
}

bool Texture::refersToSingleAspect(WebGPU::TextureFormat format, WebGPU::TextureAspect aspect)
{
    switch (aspect) {
    case WebGPU::TextureAspect::All:
        if (Texture::containsDepthAspect(format) && Texture::containsStencilAspect(format))
            return false;
        break;
    case WebGPU::TextureAspect::StencilOnly:
        if (!Texture::containsStencilAspect(format))
            return false;
        break;
    case WebGPU::TextureAspect::DepthOnly:
        if (!Texture::containsDepthAspect(format))
            return false;
        break;
    }

    return true;
}

bool Texture::isValidDepthStencilCopySource(WebGPU::TextureFormat format, WebGPU::TextureAspect aspect)
{
    // https://gpuweb.github.io/gpuweb/#depth-formats
    ASSERT(Texture::isDepthOrStencilFormat(format));

    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        ASSERT_NOT_REACHED();
        return false;
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
        return true;
    case WebGPU::TextureFormat::Depth24plus:
        return false;
    case WebGPU::TextureFormat::Depth24plusStencil8:
        return aspect == WebGPU::TextureAspect::StencilOnly;
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return true;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool Texture::isValid() const
{
    return isDestroyed() || m_texture;
}

bool Texture::isValidDepthStencilCopyDestination(WebGPU::TextureFormat format, WebGPU::TextureAspect aspect)
{
    // https://gpuweb.github.io/gpuweb/#depth-formats
    ASSERT(Texture::isDepthOrStencilFormat(format));

    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        return false;
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
        return true;
    case WebGPU::TextureFormat::Depth24plus:
        return false;
    case WebGPU::TextureFormat::Depth24plusStencil8:
        return aspect == WebGPU::TextureAspect::StencilOnly;
    case WebGPU::TextureFormat::Depth32float:
        return false;
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return aspect == WebGPU::TextureAspect::StencilOnly;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return false;
    }
}

NSString* Texture::errorValidatingTextureCopyRange(const WebGPU::TexelCopyTextureInfo& imageCopyTexture, const WebGPU::Extent3D& copySize)
{
    // https://gpuweb.github.io/gpuweb/#validating-texture-copy-range

    auto blockWidth = Texture::texelBlockWidth(metal(imageCopyTexture.texture).format());

    auto blockHeight = Texture::texelBlockHeight(metal(imageCopyTexture.texture).format());

    auto subresourceSize = imageCopyTextureSubresourceSize(imageCopyTexture);

    auto endX = checkedSum<uint32_t>(imageCopyTexture.origin.x, copySize.width);
    if (endX.hasOverflowed() || endX.value() > subresourceSize.width) {
        NSString* s = [NSString stringWithFormat:@"endX(%u) > subresourceSize.width(%u)", endX.hasOverflowed() ? UINT32_MAX : endX.value(), subresourceSize.width];
        return s;
    }

    auto endY = checkedSum<uint32_t>(imageCopyTexture.origin.y, copySize.height);
    if (endY.hasOverflowed() || endY.value() > subresourceSize.height)
        return [NSString stringWithFormat:@"endY(%u) > subresourceSize.height(%u)", endY.hasOverflowed() ? UINT32_MAX : endY.value(), subresourceSize.height];

    auto endZ = checkedSum<uint32_t>(imageCopyTexture.origin.z, copySize.depthOrArrayLayers);
    if (endZ.hasOverflowed() || endZ.value() > subresourceSize.depthOrArrayLayers)
        return [NSString stringWithFormat:@"endZ(%u) > subresourceSize.depthOrArrayLayers(%u)", endZ.hasOverflowed() ? UINT32_MAX : endZ.value(), subresourceSize.depthOrArrayLayers];

    if (copySize.width % blockWidth)
        return [NSString stringWithFormat:@"copySize.width(%u) is not divisible by blockWidth(%u)", copySize.width, blockWidth];

    if (copySize.height % blockHeight)
        return [NSString stringWithFormat:@"copySize.height(%u) is not divisible by blockHeight(%u)", copySize.height, blockHeight];

    return nil;
}

NSString* Texture::errorValidatingLinearTextureData(const WebGPU::TexelCopyBufferLayout& layout, uint64_t byteSize, WebGPU::TextureFormat format, const WebGPU::Extent3D& copyExtent)
{
#define ERROR_STRING(...) ([NSString stringWithFormat:@"GPUTexture.validateLinearTextureData: %@", __VA_ARGS__])
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-linear-texture-data
    uint32_t blockWidth = Texture::texelBlockWidth(format);
    uint32_t blockHeight = Texture::texelBlockHeight(format);
    uint32_t blockSize = Texture::texelBlockSize(format);

    // The checks below and their messages use WGPU_COPY_STRIDE_UNDEFINED for a missing stride.
    uint32_t bytesPerRow = layout.bytesPerRow.value_or(WGPU_COPY_STRIDE_UNDEFINED);
    uint32_t rowsPerImage = layout.rowsPerImage.value_or(WGPU_COPY_STRIDE_UNDEFINED);

    auto widthInBlocks = copyExtent.width / blockWidth;
    if (copyExtent.width % blockWidth)
        return ERROR_STRING([NSString stringWithFormat:@"copyExtent.width(%u) is not divisible by blockWidth(%u)", copyExtent.width, blockWidth]);

    auto heightInBlocks = copyExtent.height / blockHeight;
    if (copyExtent.height % blockHeight)
        return ERROR_STRING([NSString stringWithFormat:@"copyExtent.height(%u) is not divisible by blockHeight(%u)", copyExtent.height, blockHeight]);

    auto bytesInLastRow = checkedProduct<uint64_t>(blockSize, widthInBlocks);
    if (bytesInLastRow.hasOverflowed())
        return ERROR_STRING([NSString stringWithFormat:@"bytesInLastRow = blockSize(%u + widthInBlocks(%u) overflowed", blockSize, widthInBlocks]);

    if (heightInBlocks > 1) {
        if (bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
            return ERROR_STRING([NSString stringWithFormat:@"bytesPerRow is undefined, but heightInBlocks(%u) > 1, this is not allowed", heightInBlocks]);
    }

    if (copyExtent.depthOrArrayLayers > 1) {
        if (bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED || rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED)
            return ERROR_STRING([NSString stringWithFormat:@"depthOrArrayLayers(%u) > 1 but bytesPerRow(%u) or rowsPerImage(%u) is undefined, this is not allowed", copyExtent.depthOrArrayLayers, bytesPerRow, rowsPerImage]);
    }

    if (bytesPerRow != WGPU_COPY_STRIDE_UNDEFINED) {
        if (bytesPerRow < bytesInLastRow.value())
            return ERROR_STRING([NSString stringWithFormat:@"bytesPerRow(%u) is less than bytesInLastRow(%llu)", bytesPerRow, bytesInLastRow.value()]);
    }

    if (rowsPerImage != WGPU_COPY_STRIDE_UNDEFINED) {
        if (rowsPerImage < heightInBlocks)
            return ERROR_STRING([NSString stringWithFormat:@"rowsPerImage(%u) is less than heightInBlocks(%u)", rowsPerImage, heightInBlocks]);
    }

    auto requiredBytesInCopy = CheckedUint64(0);

    if (copyExtent.depthOrArrayLayers > 1) {
        auto bytesPerImage = checkedProduct<uint64_t>(bytesPerRow, rowsPerImage);
        auto bytesBeforeLastImage = checkedProduct<uint64_t>(bytesPerImage, checkedDifference<uint64_t>(copyExtent.depthOrArrayLayers, 1));

        requiredBytesInCopy += bytesBeforeLastImage;
    }

    if (copyExtent.depthOrArrayLayers > 0) {
        if (heightInBlocks > 1)
            requiredBytesInCopy += checkedProduct<uint64_t>(bytesPerRow, checkedDifference<uint64_t>(heightInBlocks, 1));

        if (heightInBlocks > 0)
            requiredBytesInCopy += bytesInLastRow;
    }

    auto end = checkedSum<uint64_t>(layout.offset, requiredBytesInCopy);
    if (end.hasOverflowed())
        return ERROR_STRING([NSString stringWithFormat:@"layout.offset(%llu) + requiredBytesInCopy(%llu) overflows", layout.offset, requiredBytesInCopy.hasOverflowed() ? ULLONG_MAX : requiredBytesInCopy.value()]);

    if (end.value() > byteSize)
        return ERROR_STRING([NSString stringWithFormat:@"(layout.offset + requiredBytesInCopy)(%llu) is less than byteSize(%llu)", end.value(), byteSize]);

#undef ERROR_STRING
    return nil;
}

bool Texture::previouslyCleared() const
{
    for (uint32_t m = 0; m < mipLevelCount(); ++m) {
        for (uint32_t s = 0; s < arrayLayerCount(); ++s) {
            if (!previouslyCleared(m, s))
                return false;
        }
    }

    return true;
}

void Texture::setPreviouslyCleared()
{
    for (uint32_t m = 0; m < mipLevelCount(); ++m) {
        for (uint32_t s = 0; s < arrayLayerCount(); ++s)
            setPreviouslyCleared(m, s);
    }
}

bool Texture::previouslyCleared(uint32_t mipLevel, uint32_t slice) const
{
    if (isDestroyed())
        return true;

    if (auto it = m_clearedToZero.find(mipLevel); it != m_clearedToZero.end())
        return it->value.contains(slice);

    return false;
}

void Texture::setPreviouslyCleared(uint32_t mipLevel, uint32_t slice, bool setCleared)
{
    if (!setCleared) {
        if (auto it = m_clearedToZero.find(mipLevel); it != m_clearedToZero.end())
            it->value.remove(slice);
        return;
    }

    if (auto it = m_clearedToZero.find(mipLevel); it != m_clearedToZero.end()) {
        it->value.add(slice);
        return;
    }

    ClearedToZeroInnerContainer set;
    set.add(slice);
    m_clearedToZero.add(mipLevel, set);
}

id<MTLSharedEvent> Texture::sharedEvent() const
{
    return m_sharedEvent;
}

uint64_t Texture::sharedEventSignalValue() const
{
    return m_sharedEventSignalValue;
}

void Texture::updateCompletionEvent(const std::pair<id<MTLSharedEvent>, uint64_t>& completionEvent)
{
    m_sharedEvent = completionEvent.first;
    m_sharedEventSignalValue = completionEvent.second;
}

} // namespace WebGPU::Metal

#pragma mark WGPU Stubs

void LEGACY_NODELETE wgpuTextureAddRef(WGPUTexture texture)
{
    WebGPU::Metal::fromAPI(texture).ref();
}

void wgpuTextureRelease(WGPUTexture texture)
{
    WebGPU::Metal::fromAPI(texture).deref();
}

WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor* descriptor)
{
    Ref protectedTexture = WebGPU::Metal::fromAPI(texture);
    // A null descriptor is a descriptor with all members at their defaults.
    std::optional<WebGPU::TextureViewDescriptor> apiDescriptor;
    if (descriptor) {
        apiDescriptor = WebGPU::Metal::fromAPI(*descriptor);
        if (!apiDescriptor) {
            Ref device = protectedTexture->device();
            device->generateAValidationError("GPUTextureViewDescriptor has an invalid enum value or usage bit"_s);
            return WebGPU::Metal::releaseToAPI(WebGPU::Metal::TextureView::createInvalid(protectedTexture, device));
        }
    }
    // Every WebGPU::TextureView that a WebGPU::Metal::Texture creates is a WebGPU::Metal::TextureView.
    Ref view = downcast<WebGPU::Metal::TextureView>(*protectedTexture->createView(apiDescriptor));
    return WebGPU::Metal::releaseToAPI(WTF::move(view));
}

void wgpuTextureDestroy(WGPUTexture texture)
{
    protect(WebGPU::Metal::fromAPI(texture))->destroy();
}

void wgpuTextureSetLabel(WGPUTexture texture, WGPUStringView label)
{
    protect(WebGPU::Metal::fromAPI(texture))->setLabel(WebGPU::Metal::fromAPI(label));
}

uint32_t wgpuTextureGetDepthOrArrayLayers(WGPUTexture texture)
{
    return protect(WebGPU::Metal::fromAPI(texture))->depthOrArrayLayers();
}

WGPUTextureDimension wgpuTextureGetDimension(WGPUTexture texture)
{
    return WebGPU::Metal::toAPI(protect(WebGPU::Metal::fromAPI(texture))->dimension());
}

WGPUTextureFormat wgpuTextureGetFormat(WGPUTexture texture)
{
    auto format = protect(WebGPU::Metal::fromAPI(texture))->optionalFormat();
    return format ? WebGPU::Metal::toAPI(*format) : WGPUTextureFormat_Undefined;
}

uint32_t wgpuTextureGetHeight(WGPUTexture texture)
{
    return protect(WebGPU::Metal::fromAPI(texture))->height();
}

uint32_t wgpuTextureGetWidth(WGPUTexture texture)
{
    return protect(WebGPU::Metal::fromAPI(texture))->width();
}

uint32_t wgpuTextureGetMipLevelCount(WGPUTexture texture)
{
    return protect(WebGPU::Metal::fromAPI(texture))->mipLevelCount();
}

uint32_t wgpuTextureGetSampleCount(WGPUTexture texture)
{
    return protect(WebGPU::Metal::fromAPI(texture))->sampleCount();
}

WGPUTextureUsage wgpuTextureGetUsage(WGPUTexture texture)
{
    return WebGPU::Metal::toAPI(protect(WebGPU::Metal::fromAPI(texture))->usage());
}
