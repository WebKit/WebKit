/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
#import "Device.h"
#import "TextureView.h"
#import <wtf/MathExtras.h>

namespace WebGPU {

static std::optional<WGPUFeatureName> featureRequirementForFormat(WGPUTextureFormat format)
{
    switch (format) {
    case WGPUTextureFormat_Depth24UnormStencil8: // FIXME: This has to be guarded by MTLDevice.depth24Stencil8PixelFormatSupported
        return WGPUFeatureName_Depth24UnormStencil8;
    case WGPUTextureFormat_Depth32FloatStencil8:
        return WGPUFeatureName_Depth32FloatStencil8;
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return WGPUFeatureName_TextureCompressionBC;
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
        return WGPUFeatureName_TextureCompressionETC2;
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return WGPUFeatureName_TextureCompressionASTC;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Force32:
        return std::nullopt;
    }
}

static bool isCompressedFormat(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#packed-formats
    // "A compressed format is any format with a block size greater than 1 × 1."
    switch (format) {
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return true;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
    case WGPUTextureFormat_Force32:
        return false;
    }
}

static std::optional<WGPUTextureFormat> depthSpecificFormat(WGPUTextureFormat textureFormat)
{
    // https://gpuweb.github.io/gpuweb/#aspect-specific-format

    switch (textureFormat) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_Stencil8:
        return std::nullopt;
    case WGPUTextureFormat_Depth16Unorm:
        return WGPUTextureFormat_Depth16Unorm;
    case WGPUTextureFormat_Depth24Plus:
        return WGPUTextureFormat_Depth24Plus;
    case WGPUTextureFormat_Depth24PlusStencil8:
        return WGPUTextureFormat_Depth24Plus;
    case WGPUTextureFormat_Depth24UnormStencil8:
        return WGPUTextureFormat_Undefined; // Return a non-nullopt value for the benefit of containsDepthAspect().
    case WGPUTextureFormat_Depth32Float:
        return WGPUTextureFormat_Depth32Float;
    case WGPUTextureFormat_Depth32FloatStencil8:
        return WGPUTextureFormat_Depth32Float;
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return std::nullopt;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

static std::optional<WGPUTextureFormat> stencilSpecificFormat(WGPUTextureFormat textureFormat)
{
    // https://gpuweb.github.io/gpuweb/#aspect-specific-format

    switch (textureFormat) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        return std::nullopt;
    // Depth-stencil formats:
    case WGPUTextureFormat_Stencil8:
        return WGPUTextureFormat_Stencil8;
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
        return std::nullopt;
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
        return WGPUTextureFormat_Stencil8;
    case WGPUTextureFormat_Depth32Float:
        return std::nullopt;
    case WGPUTextureFormat_Depth32FloatStencil8:
        return WGPUTextureFormat_Stencil8;
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return std::nullopt;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

bool Texture::containsDepthAspect(WGPUTextureFormat textureFormat)
{
    return depthSpecificFormat(textureFormat).has_value();
}

bool Texture::containsStencilAspect(WGPUTextureFormat textureFormat)
{
    return stencilSpecificFormat(textureFormat).has_value();
}

bool Texture::isDepthOrStencilFormat(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#depth-formats
    // "A depth-or-stencil format is any format with depth and/or stencil aspects."
    return containsDepthAspect(format) || containsStencilAspect(format);
}

uint32_t Texture::texelBlockWidth(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#texel-block-width
    switch (format) {
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return 4;
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
        return 4;
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
        return 4;
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
        return 5;
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
        return 6;
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
        return 8;
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
        return 10;
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return 12;
    // "For pixel-based GPUTextureFormats, the texel block width and texel block height are always 1."
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
    case WGPUTextureFormat_Force32:
        return 1;
    }
}

uint32_t Texture::texelBlockHeight(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#texel-block-height
    switch (format) {
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return 4;
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
        return 4;
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
        return 4;
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
        return 5;
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
        return 6;
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
        return 5;
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
        return 6;
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
        return 8;
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
        return 5;
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
        return 6;
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
        return 8;
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
        return 10;
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return 12;
    // "For pixel-based GPUTextureFormats, the texel block width and texel block height are always 1."
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
    case WGPUTextureFormat_Force32:
        return 1;
    }
}

static bool isRenderableFormat(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#renderable-format
    // "If a format is listed in § 25.1.1 Plain color formats with RENDER_ATTACHMENT capability, it is a color renderable format. Any other format is not a color renderable format. All depth-or-stencil formats are renderable."
    // https://gpuweb.github.io/gpuweb/#plain-color-formats
    switch (format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return true;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
    case WGPUTextureFormat_Force32:
        return false;
    }
}

static bool supportsMultisampling(WGPUTextureFormat format)
{
    switch (format) {
    // https://gpuweb.github.io/gpuweb/#texture-format-caps
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    // https://gpuweb.github.io/gpuweb/#depth-formats
    // "All of these formats support multisampling."
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return true;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
    case WGPUTextureFormat_Force32:
        return false;
    }
}

static uint32_t maximumMiplevelCount(WGPUTextureDimension dimension, WGPUExtent3D size)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-maximum-miplevel-count

    // "Calculate the max dimension value m:"
    uint32_t m = 0;

    // "If dimension is:"
    switch (dimension) {
    case WGPUTextureDimension_1D:
        // "Return 1."
        return 1;
    case WGPUTextureDimension_2D:
        // "Let m = max(size.width, size.height)."
        m = std::max(size.width, size.height);
        break;
    case WGPUTextureDimension_3D:
        // "Let m = max(max(size.width, size.height), size.depthOrArrayLayer)."
        m = std::max(std::max(size.width, size.height), size.depthOrArrayLayers);
        break;
    case WGPUTextureDimension_Force32:
        return 0;
    }

    // "Return floor(log2(m)) + 1."
    auto isPowerOf2 = !(m & (m - 1));
    if (isPowerOf2)
        return WTF::fastLog2(m) + 1;
    return WTF::fastLog2(m);
}

static bool hasStorageBindingCapability(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#plain-color-formats
    switch (format) {
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        return true;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
    case WGPUTextureFormat_Force32:
        return false;
    }
}

bool Device::validateCreateTexture(const WGPUTextureDescriptor& descriptor)
{
    // FIXME: "this must be a valid GPUDevice."

    // "descriptor.usage must not be 0."
    if (!descriptor.usage)
        return false;

    // "descriptor.size.width, descriptor.size.height, and descriptor.size.depthOrArrayLayers must be greater than zero."
    if (!descriptor.size.width || !descriptor.size.height || !descriptor.size.depthOrArrayLayers)
        return false;

    // "descriptor.mipLevelCount must be greater than zero."
    if (!descriptor.mipLevelCount)
        return false;

    // "descriptor.sampleCount must be either 1 or 4."
    if (descriptor.sampleCount != 1 && descriptor.sampleCount != 4)
        return false;

    // "If descriptor.dimension is:"
    switch (descriptor.dimension) {
    case WGPUTextureDimension_1D:
        // FIXME: "descriptor.size.width must be less than or equal to this.limits.maxTextureDimension1D."

        // "descriptor.size.height must be 1."
        if (descriptor.size.height != 1)
            return false;

        // "descriptor.size.depthOrArrayLayers must be 1."
        if (descriptor.size.depthOrArrayLayers != 1)
            return false;

        // "descriptor.sampleCount must be 1."
        if (descriptor.sampleCount != 1)
            return false;

        // "descriptor.format must not be a compressed format or depth-or-stencil format."
        if (isCompressedFormat(descriptor.format) || Texture::isDepthOrStencilFormat(descriptor.format))
            return false;
        break;
    case WGPUTextureDimension_2D:
        // FIXME: "descriptor.size.width must be less than or equal to this.limits.maxTextureDimension2D."

        // FIXME: "descriptor.size.height must be less than or equal to this.limits.maxTextureDimension2D."

        // FIXME: "descriptor.size.depthOrArrayLayers must be less than or equal to this.limits.maxTextureArrayLayers."
        break;
    case WGPUTextureDimension_3D:
        // FIXME: "descriptor.size.width must be less than or equal to this.limits.maxTextureDimension3D."

        // FIXME: "descriptor.size.height must be less than or equal to this.limits.maxTextureDimension3D."

        // FIXME: "descriptor.size.depthOrArrayLayers must be less than or equal to this.limits.maxTextureDimension3D."

        // "descriptor.sampleCount must be 1."
        if (descriptor.sampleCount != 1)
            return false;

        // "descriptor.format must not be a compressed format or depth-or-stencil format."
        if (isCompressedFormat(descriptor.format) || Texture::isDepthOrStencilFormat(descriptor.format))
            return false;
        break;
    case WGPUTextureDimension_Force32:
        return false;
    }

    // "descriptor.size.width must be multiple of texel block width."
    if (descriptor.size.width % Texture::texelBlockWidth(descriptor.format))
        return false;

    // "descriptor.size.height must be multiple of texel block height."
    if (descriptor.size.height % Texture::texelBlockHeight(descriptor.format))
        return false;

    // "If descriptor.sampleCount > 1:"
    if (descriptor.sampleCount > 1) {
        // "descriptor.mipLevelCount must be 1."
        if (descriptor.mipLevelCount != 1)
            return false;

        // "descriptor.size.depthOrArrayLayers must be 1."
        if (descriptor.size.depthOrArrayLayers != 1)
            return false;

        // "descriptor.usage must not include the STORAGE_BINDING bit."
        if (descriptor.usage & WGPUTextureUsage_StorageBinding)
            return false;

        // "descriptor.format must be a renderable format"
        if (!isRenderableFormat(descriptor.format))
            return false;

        // "and support multisampling according to § 25.1 Texture Format Capabilities."
        if (!supportsMultisampling(descriptor.format))
            return false;
    }

    // "descriptor.mipLevelCount must be less than or equal to maximum mipLevel count(descriptor.dimension, descriptor.size)."
    if (descriptor.mipLevelCount > maximumMiplevelCount(descriptor.dimension, descriptor.size))
        return false;

    // "descriptor.usage must be a combination of GPUTextureUsage values."

    // "If descriptor.usage includes the RENDER_ATTACHMENT bit:"
    if (descriptor.usage & WGPUTextureUsage_RenderAttachment) {
        // "descriptor.format must be a renderable format."
        if (!isRenderableFormat(descriptor.format))
            return false;

        // "descriptor.dimension must be "2d"."
        if (descriptor.dimension != WGPUTextureDimension_2D)
            return false;
    }

    // "If descriptor.usage includes the STORAGE_BINDING bit:"
    if (descriptor.usage & WGPUTextureUsage_StorageBinding) {
        // "descriptor.format must be listed in § 25.1.1 Plain color formats table with STORAGE_BINDING capability."
        if (!hasStorageBindingCapability(descriptor.format))
            return false;
    }

    // FIXME: "For each viewFormat in descriptor.viewFormats, descriptor.format and viewFormat must be texture view format compatible."

    return true;
}

static MTLTextureUsage usage(WGPUTextureUsageFlags usage)
{
    MTLTextureUsage result = MTLTextureUsageUnknown;
    if (usage & WGPUTextureUsage_TextureBinding)
        result |= MTLTextureUsageShaderRead;
    if (usage & WGPUTextureUsage_StorageBinding)
        result |= MTLTextureUsageShaderWrite;
    if (usage & WGPUTextureUsage_RenderAttachment)
        result |= MTLTextureUsageRenderTarget;
    // FIXME: Determine whether MTLTextureUsagePixelFormatView should be set or not
    return result;
}

static MTLPixelFormat pixelFormat(WGPUTextureFormat textureFormat)
{
    switch (textureFormat) {
    case WGPUTextureFormat_R8Unorm:
        return MTLPixelFormatR8Unorm;
    case WGPUTextureFormat_R8Snorm:
        return MTLPixelFormatR8Snorm;
    case WGPUTextureFormat_R8Uint:
        return MTLPixelFormatR8Uint;
    case WGPUTextureFormat_R8Sint:
        return MTLPixelFormatR8Sint;
    case WGPUTextureFormat_R16Uint:
        return MTLPixelFormatR16Uint;
    case WGPUTextureFormat_R16Sint:
        return MTLPixelFormatR16Sint;
    case WGPUTextureFormat_R16Float:
        return MTLPixelFormatR16Float;
    case WGPUTextureFormat_RG8Unorm:
        return MTLPixelFormatRG8Unorm;
    case WGPUTextureFormat_RG8Snorm:
        return MTLPixelFormatRG8Snorm;
    case WGPUTextureFormat_RG8Uint:
        return MTLPixelFormatRG8Uint;
    case WGPUTextureFormat_RG8Sint:
        return MTLPixelFormatRG8Sint;
    case WGPUTextureFormat_R32Float:
        return MTLPixelFormatR32Float;
    case WGPUTextureFormat_R32Uint:
        return MTLPixelFormatR32Uint;
    case WGPUTextureFormat_R32Sint:
        return MTLPixelFormatR32Sint;
    case WGPUTextureFormat_RG16Uint:
        return MTLPixelFormatRG16Uint;
    case WGPUTextureFormat_RG16Sint:
        return MTLPixelFormatRG16Sint;
    case WGPUTextureFormat_RG16Float:
        return MTLPixelFormatRG16Float;
    case WGPUTextureFormat_RGBA8Unorm:
        return MTLPixelFormatRGBA8Unorm;
    case WGPUTextureFormat_RGBA8UnormSrgb:
        return MTLPixelFormatRGBA8Unorm_sRGB;
    case WGPUTextureFormat_RGBA8Snorm:
        return MTLPixelFormatRGBA8Snorm;
    case WGPUTextureFormat_RGBA8Uint:
        return MTLPixelFormatRGBA8Uint;
    case WGPUTextureFormat_RGBA8Sint:
        return MTLPixelFormatRGBA8Sint;
    case WGPUTextureFormat_BGRA8Unorm:
        return MTLPixelFormatBGRA8Unorm;
    case WGPUTextureFormat_BGRA8UnormSrgb:
        return MTLPixelFormatBGRA8Unorm_sRGB;
    case WGPUTextureFormat_RGB10A2Unorm:
        return MTLPixelFormatRGB10A2Unorm;
    case WGPUTextureFormat_RG11B10Ufloat:
        return MTLPixelFormatRG11B10Float;
    case WGPUTextureFormat_RGB9E5Ufloat:
        return MTLPixelFormatRGB9E5Float;
    case WGPUTextureFormat_RG32Float:
        return MTLPixelFormatRG32Float;
    case WGPUTextureFormat_RG32Uint:
        return MTLPixelFormatRG32Uint;
    case WGPUTextureFormat_RG32Sint:
        return MTLPixelFormatRG32Sint;
    case WGPUTextureFormat_RGBA16Uint:
        return MTLPixelFormatRGBA16Uint;
    case WGPUTextureFormat_RGBA16Sint:
        return MTLPixelFormatRGBA16Sint;
    case WGPUTextureFormat_RGBA16Float:
        return MTLPixelFormatRGBA16Float;
    case WGPUTextureFormat_RGBA32Float:
        return MTLPixelFormatRGBA32Float;
    case WGPUTextureFormat_RGBA32Uint:
        return MTLPixelFormatRGBA32Uint;
    case WGPUTextureFormat_RGBA32Sint:
        return MTLPixelFormatRGBA32Sint;
    case WGPUTextureFormat_Stencil8:
        return MTLPixelFormatStencil8;
    case WGPUTextureFormat_Depth16Unorm:
        return MTLPixelFormatDepth16Unorm;
    case WGPUTextureFormat_Depth24Plus:
        return MTLPixelFormatDepth32Float;
    case WGPUTextureFormat_Depth24PlusStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
    case WGPUTextureFormat_Depth32Float:
        return MTLPixelFormatDepth32Float;
    case WGPUTextureFormat_Depth32FloatStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
    case WGPUTextureFormat_ETC2RGB8Unorm:
        return MTLPixelFormatETC2_RGB8;
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
        return MTLPixelFormatETC2_RGB8_sRGB;
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
        return MTLPixelFormatETC2_RGB8A1;
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
        return MTLPixelFormatETC2_RGB8A1_sRGB;
    case WGPUTextureFormat_ETC2RGBA8Unorm:
        return MTLPixelFormatEAC_RGBA8;
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
        return MTLPixelFormatEAC_RGBA8_sRGB;
    case WGPUTextureFormat_EACR11Unorm:
        return MTLPixelFormatEAC_R11Unorm;
    case WGPUTextureFormat_EACR11Snorm:
        return MTLPixelFormatEAC_R11Snorm;
    case WGPUTextureFormat_EACRG11Unorm:
        return MTLPixelFormatEAC_RG11Unorm;
    case WGPUTextureFormat_EACRG11Snorm:
        return MTLPixelFormatEAC_RG11Snorm;
    case WGPUTextureFormat_ASTC4x4Unorm:
        return MTLPixelFormatASTC_4x4_LDR;
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
        return MTLPixelFormatASTC_4x4_sRGB;
    case WGPUTextureFormat_ASTC5x4Unorm:
        return MTLPixelFormatASTC_5x4_LDR;
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
        return MTLPixelFormatASTC_5x4_sRGB;
    case WGPUTextureFormat_ASTC5x5Unorm:
        return MTLPixelFormatASTC_5x5_LDR;
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
        return MTLPixelFormatASTC_5x5_sRGB;
    case WGPUTextureFormat_ASTC6x5Unorm:
        return MTLPixelFormatASTC_6x5_LDR;
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
        return MTLPixelFormatASTC_6x5_sRGB;
    case WGPUTextureFormat_ASTC6x6Unorm:
        return MTLPixelFormatASTC_6x6_LDR;
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
        return MTLPixelFormatASTC_6x6_sRGB;
    case WGPUTextureFormat_ASTC8x5Unorm:
        return MTLPixelFormatASTC_8x5_LDR;
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
        return MTLPixelFormatASTC_8x5_sRGB;
    case WGPUTextureFormat_ASTC8x6Unorm:
        return MTLPixelFormatASTC_8x6_LDR;
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
        return MTLPixelFormatASTC_8x6_sRGB;
    case WGPUTextureFormat_ASTC8x8Unorm:
        return MTLPixelFormatASTC_8x8_LDR;
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
        return MTLPixelFormatASTC_8x8_sRGB;
    case WGPUTextureFormat_ASTC10x5Unorm:
        return MTLPixelFormatASTC_10x5_LDR;
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
        return MTLPixelFormatASTC_10x5_sRGB;
    case WGPUTextureFormat_ASTC10x6Unorm:
        return MTLPixelFormatASTC_10x6_LDR;
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
        return MTLPixelFormatASTC_10x6_sRGB;
    case WGPUTextureFormat_ASTC10x8Unorm:
        return MTLPixelFormatASTC_10x8_LDR;
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
        return MTLPixelFormatASTC_10x8_sRGB;
    case WGPUTextureFormat_ASTC10x10Unorm:
        return MTLPixelFormatASTC_10x10_LDR;
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
        return MTLPixelFormatASTC_10x10_sRGB;
    case WGPUTextureFormat_ASTC12x10Unorm:
        return MTLPixelFormatASTC_12x10_LDR;
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
        return MTLPixelFormatASTC_12x10_sRGB;
    case WGPUTextureFormat_ASTC12x12Unorm:
        return MTLPixelFormatASTC_12x12_LDR;
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return MTLPixelFormatASTC_12x12_sRGB;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    case WGPUTextureFormat_Depth24UnormStencil8:
        return MTLPixelFormatDepth24Unorm_Stencil8;
    case WGPUTextureFormat_BC1RGBAUnorm:
        return MTLPixelFormatBC1_RGBA;
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
        return MTLPixelFormatBC1_RGBA_sRGB;
    case WGPUTextureFormat_BC2RGBAUnorm:
        return MTLPixelFormatBC2_RGBA;
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
        return MTLPixelFormatBC2_RGBA_sRGB;
    case WGPUTextureFormat_BC3RGBAUnorm:
        return MTLPixelFormatBC3_RGBA;
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
        return MTLPixelFormatBC3_RGBA_sRGB;
    case WGPUTextureFormat_BC4RUnorm:
        return MTLPixelFormatBC4_RUnorm;
    case WGPUTextureFormat_BC4RSnorm:
        return MTLPixelFormatBC4_RSnorm;
    case WGPUTextureFormat_BC5RGUnorm:
        return MTLPixelFormatBC5_RGUnorm;
    case WGPUTextureFormat_BC5RGSnorm:
        return MTLPixelFormatBC5_RGSnorm;
    case WGPUTextureFormat_BC6HRGBUfloat:
        return MTLPixelFormatBC6H_RGBUfloat;
    case WGPUTextureFormat_BC6HRGBFloat:
        return MTLPixelFormatBC6H_RGBFloat;
    case WGPUTextureFormat_BC7RGBAUnorm:
        return MTLPixelFormatBC7_RGBAUnorm;
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return MTLPixelFormatBC7_RGBAUnorm_sRGB;
#else
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return MTLPixelFormatInvalid;
#endif
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        return MTLPixelFormatInvalid;
    }
}

uint32_t Texture::texelBlockSize(WGPUTextureFormat format) // Bytes
{
    // For depth-stencil textures, the input value to this function
    // needs to be the output of aspectSpecificFormat().
    ASSERT(!containsDepthAspect(format) || !containsStencilAspect(format));
    switch (format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
        return 1;
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
        return 2;
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
        return 4;
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
        return 8;
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        return 16;
    case WGPUTextureFormat_Stencil8:
        return 1;
    case WGPUTextureFormat_Depth16Unorm:
        return 2;
    case WGPUTextureFormat_Depth24Plus:
        ASSERT(pixelFormat(format) == MTLPixelFormatDepth32Float);
        return 4;
    case WGPUTextureFormat_Depth24PlusStencil8:
        ASSERT_NOT_REACHED();
        return 0;
    case WGPUTextureFormat_Depth24UnormStencil8:
        ASSERT_NOT_REACHED();
        return 0;
    case WGPUTextureFormat_Depth32Float:
        return 4;
    case WGPUTextureFormat_Depth32FloatStencil8:
        ASSERT_NOT_REACHED();
        return 0;
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
        return 8;
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
        return 8;
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
        return 16;
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
        return 8;
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
        return 16;
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
        return 16;
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return 16;
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
        return 8;
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
        return 8;
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
        return 16;
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return 16;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

WGPUTextureFormat Texture::aspectSpecificFormat(WGPUTextureFormat format, WGPUTextureAspect aspect)
{
    // https://gpuweb.github.io/gpuweb/#aspect-specific-format

    switch (aspect) {
    case WGPUTextureAspect_All:
        return format;
    case WGPUTextureAspect_StencilOnly: {
        auto result = stencilSpecificFormat(format);
        ASSERT(result);
        return *result;
    }
    case WGPUTextureAspect_DepthOnly: {
        auto result = depthSpecificFormat(format);
        ASSERT(result);
        return *result;
    }
    case WGPUTextureAspect_Force32:
        ASSERT_NOT_REACHED();
        return WGPUTextureFormat_Undefined;
    }
}

static std::optional<MTLPixelFormat> depthOnlyAspectMetalFormat(WGPUTextureFormat textureFormat)
{
    switch (textureFormat) {
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_Stencil8:
        return std::nullopt;
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        // This is a bit surprising, but it should be correct.
        // When using the view as a render target, we'll bind it to MTLRenderPassDescriptor.depthAttachment, which will ignore the stencil bits.
        // When attaching the view as a shader resource view, only the depth aspect is visible anyway.
        // When copying to/from the texture, we can use MTLBlitOption.MTLBlitOptionDepthFromDepthStencil to ignore the stencil bits.
        return pixelFormat(textureFormat);
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
    case WGPUTextureFormat_Force32:
        return std::nullopt;
    }
}

static std::optional<MTLPixelFormat> stencilOnlyAspectMetalFormat(WGPUTextureFormat textureFormat)
{
    switch (textureFormat) {
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        return std::nullopt;
    case WGPUTextureFormat_Stencil8:
        return pixelFormat(textureFormat);
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
        return std::nullopt;
    case WGPUTextureFormat_Depth24PlusStencil8:
        ASSERT(pixelFormat(textureFormat) == MTLPixelFormatDepth32Float_Stencil8);
        return MTLPixelFormatX32_Stencil8;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    case WGPUTextureFormat_Depth24UnormStencil8:
        return MTLPixelFormatX24_Stencil8;
#else
    case WGPUTextureFormat_Depth24UnormStencil8:
        return std::nullopt;
#endif
    case WGPUTextureFormat_Depth32Float:
        return std::nullopt;
    case WGPUTextureFormat_Depth32FloatStencil8:
        return MTLPixelFormatX32_Stencil8;
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
    case WGPUTextureFormat_Force32:
        return std::nullopt;
    }
}

static MTLStorageMode storageMode(bool deviceHasUnifiedMemory)
{
    if (deviceHasUnifiedMemory)
        return MTLStorageModeShared;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    return MTLStorageModeManaged;
#else
    return MTLStorageModePrivate;
#endif
}

RefPtr<Texture> Device::createTexture(const WGPUTextureDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return nullptr;

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createtexture

    // "If descriptor.format is a GPUTextureFormat that requires a feature (see § 25.1 Texture Format Capabilities)
    if (featureRequirementForFormat(descriptor.format)) {
        // FIXME: "but this.[[device]].[[features]] does not contain the feature, throw a TypeError."
        return nullptr;
    }

    // "If any of the following requirements are unmet:"
    if (!validateCreateTexture(descriptor)) {
        // "Generate a validation error."
        generateAValidationError("Validation failure."_s);
        // FIXME: "Return a new invalid GPUTexture."
        return nullptr;
    }

    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor new];

    textureDescriptor.usage = usage(descriptor.usage);

    switch (descriptor.dimension) {
    case WGPUTextureDimension_1D:
        textureDescriptor.width = descriptor.size.width;
        if (descriptor.size.depthOrArrayLayers > 1) {
            textureDescriptor.textureType = MTLTextureType1DArray;
            textureDescriptor.arrayLength = descriptor.size.depthOrArrayLayers;
        } else
            textureDescriptor.textureType = MTLTextureType1D;
        break;
    case WGPUTextureDimension_2D:
        textureDescriptor.width = descriptor.size.width;
        textureDescriptor.height = descriptor.size.height;
        if (descriptor.size.depthOrArrayLayers > 1) {
            textureDescriptor.arrayLength = descriptor.size.depthOrArrayLayers;
            if (descriptor.sampleCount > 1) {
#if PLATFORM(WATCHOS)
                return nullptr;
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
    case WGPUTextureDimension_3D:
        textureDescriptor.width = descriptor.size.width;
        textureDescriptor.height = descriptor.size.height;
        textureDescriptor.depth = descriptor.size.depthOrArrayLayers;
        textureDescriptor.textureType = MTLTextureType3D;
        break;
    case WGPUTextureDimension_Force32:
        return nullptr;
    }

    textureDescriptor.pixelFormat = pixelFormat(descriptor.format);

    textureDescriptor.mipmapLevelCount = descriptor.mipLevelCount;

    textureDescriptor.sampleCount = descriptor.sampleCount;

    textureDescriptor.storageMode = storageMode(hasUnifiedMemory());

    // FIXME(PERFORMANCE): Consider write-combining CPU cache mode.
    // FIXME(PERFORMANCE): Consider implementing hazard tracking ourself.

    id<MTLTexture> texture = [m_device newTextureWithDescriptor:textureDescriptor];
    if (!texture)
        return nullptr;

    texture.label = fromAPI(descriptor.label);

    // "Let t be a new GPUTexture object."
    // "Set t.[[descriptor]] to descriptor."
    // "Return t."

    return Texture::create(texture, descriptor, *this);
}

Texture::Texture(id<MTLTexture> texture, const WGPUTextureDescriptor& descriptor, Device& device)
    : m_texture(texture)
    , m_descriptor(descriptor)
    , m_device(device)
{
}

Texture::~Texture() = default;

std::optional<WGPUTextureViewDescriptor> Texture::resolveTextureViewDescriptorDefaults(const WGPUTextureViewDescriptor& descriptor) const
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-resolving-gputextureviewdescriptor-defaults

    // "Let resolved be a copy of descriptor."
    WGPUTextureViewDescriptor resolved = descriptor;

    // "If resolved.format is undefined"
    if (resolved.format == WGPUTextureFormat_Undefined) {
        // "set resolved.format to texture.[[descriptor]].format."
        resolved.format = m_descriptor.format;
    }

    // "If resolved.mipLevelCount is undefined"
    if (resolved.mipLevelCount == WGPU_MIP_LEVEL_COUNT_UNDEFINED) {
        // "set resolved.mipLevelCount to texture.[[descriptor]].mipLevelCount − resolved.baseMipLevel."
        // FIXME: Use checked arithmetic.
        resolved.mipLevelCount = m_descriptor.mipLevelCount - resolved.baseMipLevel;
    }

    // "If resolved.dimension is undefined"
    if (resolved.dimension == WGPUTextureViewDimension_Undefined) {
        // "and texture.[[descriptor]].dimension is:"
        switch (m_descriptor.dimension) {
        case WGPUTextureDimension_1D:
            // "Set resolved.dimension to "1d"."
            resolved.dimension = WGPUTextureViewDimension_1D;
            break;
        case WGPUTextureDimension_2D:
            // "Set resolved.dimension to "2d"."
            resolved.dimension = WGPUTextureViewDimension_2D;
            break;
        case WGPUTextureDimension_3D:
            // "Set resolved.dimension to "3d"."
            resolved.dimension = WGPUTextureViewDimension_3D;
            break;
        case WGPUTextureDimension_Force32:
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    }

    // "If resolved.arrayLayerCount is undefined"
    if (resolved.arrayLayerCount == WGPU_ARRAY_LAYER_COUNT_UNDEFINED) {
        // "and resolved.dimension is:"
        switch (resolved.dimension) {
        case WGPUTextureViewDimension_Undefined:
            ASSERT_NOT_REACHED();
            return std::nullopt;
        case WGPUTextureViewDimension_1D:
        case WGPUTextureViewDimension_2D:
        case WGPUTextureViewDimension_3D:
            // "Set resolved.arrayLayerCount to 1."
            resolved.arrayLayerCount = 1;
            break;
        case WGPUTextureViewDimension_Cube:
            // "Set resolved.arrayLayerCount to 6."
            resolved.arrayLayerCount = 6;
            break;
        case WGPUTextureViewDimension_2DArray:
        case WGPUTextureViewDimension_CubeArray:
            // "Set resolved.arrayLayerCount to texture.[[descriptor]].size.depthOrArrayLayers − baseArrayLayer."
            // FIXME: Use checked arithmetic
            resolved.arrayLayerCount = m_descriptor.size.depthOrArrayLayers - resolved.baseArrayLayer;
            break;
        case WGPUTextureViewDimension_Force32:
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    }

    // "Return resolved."
    return resolved;
}

uint32_t Texture::arrayLayerCount() const
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-array-layer-count

    // "If texture.[[descriptor]].dimension is:"
    switch (m_descriptor.dimension) {
    case WGPUTextureDimension_1D:
        // "Return 1."
        return 1;
    case WGPUTextureDimension_2D:
        // "Return texture.[[descriptor]].size.depthOrArrayLayers."
        return m_descriptor.size.depthOrArrayLayers;
    case WGPUTextureDimension_3D:
        // "Return 1."
        return 1;
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return 1;
    }
}

bool Texture::validateCreateView(const WGPUTextureViewDescriptor& descriptor) const
{
    // FIXME: "this is valid"

    // "If the descriptor.aspect is"
    switch (descriptor.aspect) {
    case WGPUTextureAspect_All:
        break;
    case WGPUTextureAspect_StencilOnly:
        // "this.[[descriptor]].format must be a depth-or-stencil format which has a stencil aspect."
        if (!containsStencilAspect(m_descriptor.format))
            return false;
        break;
    case WGPUTextureAspect_DepthOnly:
        // "this.[[descriptor]].format must be a depth-or-stencil format which has a depth aspect."
        if (!containsDepthAspect(m_descriptor.format))
            return false;
        break;
    case WGPUTextureAspect_Force32:
        return false;
    }

    // "descriptor.mipLevelCount must be > 0."
    if (!descriptor.mipLevelCount)
        return false;

    // "descriptor.baseMipLevel + descriptor.mipLevelCount must be ≤ this.[[descriptor]].mipLevelCount."
    // FIXME: Use checked arithmetic.
    if (descriptor.baseMipLevel + descriptor.mipLevelCount > m_descriptor.mipLevelCount)
        return false;

    // "descriptor.arrayLayerCount must be > 0."
    if (!descriptor.arrayLayerCount)
        return false;

    // "descriptor.baseArrayLayer + descriptor.arrayLayerCount must be ≤ the array layer count of this."
    // FIXME: Use checked arithmetic.
    if (descriptor.baseArrayLayer + descriptor.arrayLayerCount > arrayLayerCount())
        return false;

    // FIXME: "descriptor.format must be equal to either this.[[descriptor]].format or one of the formats in this.[[descriptor]].viewFormats."
    if (descriptor.format != m_descriptor.format)
        return false;

    // "If descriptor.dimension is:"
    switch (descriptor.dimension) {
    case WGPUTextureViewDimension_Undefined:
        return false;
    case WGPUTextureViewDimension_1D:
        // "this.[[descriptor]].dimension must be "1d"."
        if (m_descriptor.dimension != WGPUTextureDimension_1D)
            return false;

        // "descriptor.arrayLayerCount must be 1."
        if (descriptor.arrayLayerCount != 1)
            return false;
        break;
    case WGPUTextureViewDimension_2D:
        // "this.[[descriptor]].dimension must be "2d"."
        if (m_descriptor.dimension != WGPUTextureDimension_2D)
            return false;

        // "descriptor.arrayLayerCount must be 1."
        if (descriptor.arrayLayerCount != 1)
            return false;
        break;
    case WGPUTextureViewDimension_2DArray:
        // "this.[[descriptor]].dimension must be "2d"."
        if (m_descriptor.dimension != WGPUTextureDimension_2D)
            return false;
        break;
    case WGPUTextureViewDimension_Cube:
        // "this.[[descriptor]].dimension must be "2d"."
        if (m_descriptor.dimension != WGPUTextureDimension_2D)
            return false;

        // "descriptor.arrayLayerCount must be 6."
        if (descriptor.arrayLayerCount != 6)
            return false;

        // "this.[[descriptor]].size.width must be this.[[descriptor]].size.height."
        if (m_descriptor.size.width != m_descriptor.size.height)
            return false;
        break;
    case WGPUTextureViewDimension_CubeArray:
        // "this.[[descriptor]].dimension must be "2d"."
        if (m_descriptor.dimension != WGPUTextureDimension_2D)
            return false;

        // "descriptor.arrayLayerCount must be a multiple of 6."
        if (descriptor.arrayLayerCount % 6)
            return false;

        // "this.[[descriptor]].size.width must be this.[[descriptor]].size.height."
        if (m_descriptor.size.width != m_descriptor.size.height)
            return false;
        break;
    case WGPUTextureViewDimension_3D:
        // "this.[[descriptor]].dimension must be "3d"."
        if (m_descriptor.dimension != WGPUTextureDimension_3D)
            return false;

        // "descriptor.arrayLayerCount must be 1."
        if (descriptor.arrayLayerCount != 1)
            return false;
        break;
    case WGPUTextureViewDimension_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

static WGPUExtent3D computeRenderExtent(const WGPUExtent3D& baseSize, uint32_t mipLevel)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-compute-render-extent

    // "Let extent be a new GPUExtent3DDict object."
    WGPUExtent3D extent { };

    // "Set extent.width to max(1, baseSize.width ≫ mipLevel)."
    extent.width = std::max(static_cast<uint32_t>(1), baseSize.width >> mipLevel);

    // "Set extent.height to max(1, baseSize.height ≫ mipLevel)."
    extent.height = std::max(static_cast<uint32_t>(1), baseSize.height >> mipLevel);

    // "Set extent.depthOrArrayLayers to 1."
    extent.depthOrArrayLayers = 1;

    // "Return extent."
    return extent;
}

RefPtr<TextureView> Texture::createView(const WGPUTextureViewDescriptor& inputDescriptor)
{
    if (inputDescriptor.nextInChain)
        return nullptr;

    // https://gpuweb.github.io/gpuweb/#dom-gputexture-createview

    // "Set descriptor to the result of resolving GPUTextureViewDescriptor defaults with descriptor."
    auto descriptor = resolveTextureViewDescriptorDefaults(inputDescriptor);
    if (!descriptor)
        return nullptr;

    // "If any of the following requirements are unmet:"
    if (!validateCreateView(*descriptor)) {
        // "Generate a validation error."
        m_device->generateAValidationError("Validation failure."_s);

        // FIXME: "Return a new invalid GPUTextureView."
        return nullptr;
    }

    std::optional<MTLPixelFormat> pixelFormat;
    if (isDepthOrStencilFormat(descriptor->format)) {
        switch (descriptor->aspect) {
        case WGPUTextureAspect_All:
            pixelFormat = WebGPU::pixelFormat(descriptor->format);
            break;
        case WGPUTextureAspect_StencilOnly:
            pixelFormat = stencilOnlyAspectMetalFormat(descriptor->format);
            break;
        case WGPUTextureAspect_DepthOnly:
            pixelFormat = depthOnlyAspectMetalFormat(descriptor->format);
            break;
        case WGPUTextureAspect_Force32:
            return nullptr;
        }
    }
    if (!pixelFormat)
        return nullptr;
    ASSERT(*pixelFormat != MTLPixelFormatInvalid);

    MTLTextureType textureType;
    switch (descriptor->dimension) {
    case WGPUTextureViewDimension_Undefined:
        ASSERT_NOT_REACHED();
        return nullptr;
    case WGPUTextureViewDimension_1D:
        if (descriptor->arrayLayerCount == 1)
            textureType = MTLTextureType1D;
        else
            textureType = MTLTextureType1DArray;
        break;
    case WGPUTextureViewDimension_2D:
        if (m_descriptor.sampleCount > 1)
            textureType = MTLTextureType2DMultisample;
        else
            textureType = MTLTextureType2D;
        break;
    case WGPUTextureViewDimension_2DArray:
        if (m_descriptor.sampleCount > 1) {
#if PLATFORM(WATCHOS)
            return nullptr;
#else
            textureType = MTLTextureType2DMultisampleArray;
#endif
        } else
            textureType = MTLTextureType2DArray;
        break;
    case WGPUTextureViewDimension_Cube:
        textureType = MTLTextureTypeCube;
        break;
    case WGPUTextureViewDimension_CubeArray:
        textureType = MTLTextureTypeCubeArray;
        break;
    case WGPUTextureViewDimension_3D:
        textureType = MTLTextureType3D;
        break;
    case WGPUTextureViewDimension_Force32:
        return nullptr;
    }

    auto levels = NSMakeRange(descriptor->baseMipLevel, descriptor->mipLevelCount);

    auto slices = NSMakeRange(descriptor->baseArrayLayer, descriptor->arrayLayerCount);

    id<MTLTexture> texture = [m_texture newTextureViewWithPixelFormat:*pixelFormat textureType:textureType levels:levels slices:slices];
    if (!texture)
        return nullptr;

    texture.label = fromAPI(descriptor->label);

    // "Let view be a new GPUTextureView object."
    // "Set view.[[texture]] to this."
    // "Set view.[[descriptor]] to descriptor."

    // "If this.[[descriptor]].usage contains RENDER_ATTACHMENT:"
    std::optional<WGPUExtent3D> renderExtent;
    if  (m_descriptor.usage & WGPUTextureUsage_RenderAttachment) {
        // "Let renderExtent be compute render extent(this.[[descriptor]].size, descriptor.baseMipLevel)."
        // "Set view.[[renderExtent]] to renderExtent."
        renderExtent = computeRenderExtent(m_descriptor.size, descriptor->baseMipLevel);
    }

    // "Return view."
    return TextureView::create(texture, *descriptor, renderExtent);
}

void Texture::destroy()
{
}

void Texture::setLabel(String&& label)
{
    m_texture.label = label;
}

WGPUExtent3D Texture::logicalMiplevelSpecificTextureExtent(uint32_t mipLevel)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-logical-miplevel-specific-texture-extent

    switch (m_descriptor.dimension) {
    case WGPUTextureDimension_1D:
        return {
            // "Set extent.width to max(1, descriptor.size.width ≫ mipLevel)."
            std::max(static_cast<uint32_t>(1), m_descriptor.size.width >> mipLevel),
            // "Set extent.height to 1."
            1,
            // "Set extent.depthOrArrayLayers to descriptor.size.depthOrArrayLayers."
            m_descriptor.size.depthOrArrayLayers };
    case WGPUTextureDimension_2D:
        return {
            // "Set extent.width to max(1, descriptor.size.width ≫ mipLevel)."
            std::max(static_cast<uint32_t>(1), m_descriptor.size.width >> mipLevel),
            // "Set extent.height to max(1, descriptor.size.height ≫ mipLevel)."
            std::max(static_cast<uint32_t>(1), m_descriptor.size.height >> mipLevel),
            // "Set extent.depthOrArrayLayers to descriptor.size.depthOrArrayLayers."
            m_descriptor.size.depthOrArrayLayers };
    case WGPUTextureDimension_3D:
        return {
            // "Set extent.width to max(1, descriptor.size.width ≫ mipLevel)."
            std::max(static_cast<uint32_t>(1), m_descriptor.size.width >> mipLevel),
            // "Set extent.height to max(1, descriptor.size.height ≫ mipLevel)."
            std::max(static_cast<uint32_t>(1), m_descriptor.size.height >> mipLevel),
            // "Set extent.depthOrArrayLayers to max(1, descriptor.size.depthOrArrayLayers ≫ mipLevel)."
            std::max(static_cast<uint32_t>(1), m_descriptor.size.depthOrArrayLayers >> mipLevel) };
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return WGPUExtent3D { };
    }
}

WGPUExtent3D Texture::physicalMiplevelSpecificTextureExtent(uint32_t mipLevel)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-physical-miplevel-specific-texture-extent

    // "Let logicalExtent be logical miplevel-specific texture extent(descriptor, mipLevel)."
    auto logicalExtent = logicalMiplevelSpecificTextureExtent(mipLevel);

    switch (m_descriptor.dimension) {
    case WGPUTextureDimension_1D:
        return {
            // "Set extent.width to logicalExtent.width rounded up to the nearest multiple of descriptor’s texel block width."
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockWidth(m_descriptor.format), logicalExtent.width)),
            // "Set extent.height to 1."
            1,
            // "Set extent.depthOrArrayLayers to logicalExtent.depthOrArrayLayers."
            logicalExtent.depthOrArrayLayers };
    case WGPUTextureDimension_2D:
        return {
            // "Set extent.width to logicalExtent.width rounded up to the nearest multiple of descriptor’s texel block width."
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockWidth(m_descriptor.format), logicalExtent.width)),
            // "Set extent.height to logicalExtent.height rounded up to the nearest multiple of descriptor’s texel block height."
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockHeight(m_descriptor.format), logicalExtent.height)),
            // "Set extent.depthOrArrayLayers to logicalExtent.depthOrArrayLayers."
            logicalExtent.depthOrArrayLayers };
    case WGPUTextureDimension_3D:
        return {
            // "Set extent.width to logicalExtent.width rounded up to the nearest multiple of descriptor’s texel block width."
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockWidth(m_descriptor.format), logicalExtent.width)),
            // "Set extent.height to logicalExtent.height rounded up to the nearest multiple of descriptor’s texel block height."
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockHeight(m_descriptor.format), logicalExtent.height)),
            // "Set extent.depthOrArrayLayers to logicalExtent.depthOrArrayLayers."
            logicalExtent.depthOrArrayLayers };
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return WGPUExtent3D { };
    }
}

static WGPUExtent3D imageCopyTextureSubresourceSize(const WGPUImageCopyTexture& imageCopyTexture)
{
    // https://gpuweb.github.io/gpuweb/#imagecopytexture-subresource-size

    // "Its width, height and depthOrArrayLayers are the width, height, and depth, respectively, of the physical size of imageCopyTexture.texture subresource at mipmap level imageCopyTexture.mipLevel."
    return fromAPI(imageCopyTexture.texture).physicalMiplevelSpecificTextureExtent(imageCopyTexture.mipLevel);
}

bool Texture::validateImageCopyTexture(const WGPUImageCopyTexture& imageCopyTexture, const WGPUExtent3D& copySize)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpuimagecopytexture

    // "blockWidth be the texel block width of imageCopyTexture.texture.[[descriptor]].format."
    uint32_t blockWidth = Texture::texelBlockWidth(fromAPI(imageCopyTexture.texture).descriptor().format);

    // "blockHeight be the texel block height of imageCopyTexture.texture.[[descriptor]].format."
    uint32_t blockHeight = Texture::texelBlockHeight(fromAPI(imageCopyTexture.texture).descriptor().format);

    // FIXME: "imageCopyTexture.texture must be a valid GPUTexture."

    // "imageCopyTexture.mipLevel must be less than the [[descriptor]].mipLevelCount of imageCopyTexture.texture."
    if (imageCopyTexture.mipLevel >= fromAPI(imageCopyTexture.texture).descriptor().mipLevelCount)
        return false;

    // "imageCopyTexture.origin.x must be a multiple of blockWidth."
    if (imageCopyTexture.origin.x % blockWidth)
        return false;

    // "imageCopyTexture.origin.y must be a multiple of blockHeight."
    if (imageCopyTexture.origin.y % blockHeight)
        return false;

    // "imageCopyTexture.texture.[[descriptor]].format is a depth-stencil format."
    // "imageCopyTexture.texture.[[descriptor]].sampleCount is greater than 1."
    if (Texture::isDepthOrStencilFormat(fromAPI(imageCopyTexture.texture).descriptor().format)
        || fromAPI(imageCopyTexture.texture).descriptor().sampleCount > 1) {
        // "The imageCopyTexture subresource size of imageCopyTexture is equal to copySize"
        auto subresourceSize = imageCopyTextureSubresourceSize(imageCopyTexture);
        if (subresourceSize.width != copySize.width
            || subresourceSize.height != copySize.height
            || subresourceSize.depthOrArrayLayers != copySize.depthOrArrayLayers)
            return false;
    }

    return true;
}

bool Texture::refersToSingleAspect(WGPUTextureFormat format, WGPUTextureAspect aspect)
{
    switch (aspect) {
    case WGPUTextureAspect_All:
        ASSERT(Texture::containsDepthAspect(format) || Texture::containsStencilAspect(format));
        if (Texture::containsDepthAspect(format) && Texture::containsStencilAspect(format))
            return false;
        break;
    case WGPUTextureAspect_StencilOnly:
        if (!Texture::containsStencilAspect(format))
            return false;
        break;
    case WGPUTextureAspect_DepthOnly:
        if (!Texture::containsDepthAspect(format))
            return false;
        break;
    case WGPUTextureAspect_Force32:
        return false;
    }

    return true;
}

bool Texture::isValidImageCopySource(WGPUTextureFormat format, WGPUTextureAspect aspect)
{
    // https://gpuweb.github.io/gpuweb/#depth-formats

    switch (format) {
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        ASSERT_NOT_REACHED();
        return false;
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
        return true;
    case WGPUTextureFormat_Depth24Plus:
        return false;
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
        return aspect == WGPUTextureAspect_StencilOnly;
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return true;
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool Texture::isValidImageCopyDestination(WGPUTextureFormat format, WGPUTextureAspect aspect)
{
    // https://gpuweb.github.io/gpuweb/#depth-formats

    switch (format) {
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        ASSERT_NOT_REACHED();
        return false;
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
        return true;
    case WGPUTextureFormat_Depth24Plus:
        return false;
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
        return aspect == WGPUTextureAspect_StencilOnly;
    case WGPUTextureFormat_Depth32Float:
        return false;
    case WGPUTextureFormat_Depth32FloatStencil8:
        return aspect == WGPUTextureAspect_StencilOnly;
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool Texture::validateTextureCopyRange(const WGPUImageCopyTexture& imageCopyTexture, const WGPUExtent3D& copySize)
{
    // https://gpuweb.github.io/gpuweb/#validating-texture-copy-range

    // "Let blockWidth be the texel block width of imageCopyTexture.texture.[[descriptor]].format."
    auto blockWidth = Texture::texelBlockWidth(fromAPI(imageCopyTexture.texture).descriptor().format);

    // "Let blockHeight be the texel block height of imageCopyTexture.texture.[[descriptor]].format."
    auto blockHeight = Texture::texelBlockHeight(fromAPI(imageCopyTexture.texture).descriptor().format);

    // "Let subresourceSize be the imageCopyTexture subresource size of imageCopyTexture."
    auto subresourceSize = imageCopyTextureSubresourceSize(imageCopyTexture);

    // "(imageCopyTexture.origin.x + copySize.width) ≤ subresourceSize.width"
    // FIXME: Used checked arithmetic
    if (imageCopyTexture.origin.x + copySize.width > subresourceSize.width)
        return false;

    // "(imageCopyTexture.origin.y + copySize.height) ≤ subresourceSize.height"
    // FIXME: Used checked arithmetic
    if ((imageCopyTexture.origin.y + copySize.height) > subresourceSize.height)
        return false;

    // "(imageCopyTexture.origin.z + copySize.depthOrArrayLayers) ≤ subresourceSize.depthOrArrayLayers"
    // FIXME: Used checked arithmetic
    if ((imageCopyTexture.origin.z + copySize.depthOrArrayLayers) > subresourceSize.depthOrArrayLayers)
        return false;

    // "copySize.width must be a multiple of blockWidth."
    if (copySize.width % blockWidth)
        return false;

    // "copySize.height must be a multiple of blockHeight."
    if (copySize.height % blockHeight)
        return false;

    return true;
}

bool Texture::validateLinearTextureData(const WGPUTextureDataLayout& layout, uint64_t byteSize, WGPUTextureFormat format, WGPUExtent3D copyExtent)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-linear-texture-data

    // "Let blockWidth, blockHeight, and blockSize be the texel block width, height, and size of format."
    uint32_t blockWidth = Texture::texelBlockWidth(format);
    uint32_t blockHeight = Texture::texelBlockHeight(format);
    uint32_t blockSize = Texture::texelBlockSize(format);

    // "It is assumed that copyExtent.width is a multiple of blockWidth and copyExtent.height is a multiple of blockHeight."

    // "widthInBlocks be copyExtent.width ÷ blockWidth."
    auto widthInBlocks = copyExtent.width / blockWidth;

    // "heightInBlocks be copyExtent.height ÷ blockHeight."
    auto heightInBlocks = copyExtent.height / blockHeight;

    // "bytesInLastRow be blockSize × widthInBlocks."
    // FIXME: Use checked arithmetic.
    auto bytesInLastRow = blockSize * widthInBlocks;

    // "If heightInBlocks > 1"
    if (heightInBlocks > 1) {
        // "layout.bytesPerRow must be specified."
        if (layout.bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
            return false;
    }

    // "If copyExtent.depthOrArrayLayers > 1"
    if (copyExtent.depthOrArrayLayers > 1) {
        // "layout.bytesPerRow and layout.rowsPerImage must be specified."
        if (layout.bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED || layout.rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED)
            return false;
    }

    // "If specified"
    if (layout.bytesPerRow != WGPU_COPY_STRIDE_UNDEFINED) {
        // "layout.bytesPerRow must be greater than or equal to bytesInLastRow."
        if (layout.bytesPerRow < bytesInLastRow)
            return false;
    }

    // "If specified"
    if (layout.rowsPerImage != WGPU_COPY_STRIDE_UNDEFINED) {
        // "layout.rowsPerImage must be greater than or equal to heightInBlocks."
        if (layout.rowsPerImage < heightInBlocks)
            return false;
    }

    // "Let requiredBytesInCopy be 0."
    uint64_t requiredBytesInCopy = 0;

    // "If copyExtent.depthOrArrayLayers > 1:"
    if (copyExtent.depthOrArrayLayers > 1) {
        // "Let bytesPerImage be layout.bytesPerRow × layout.rowsPerImage."
        // FIXME: Use checked arithmetic.
        auto bytesPerImage = layout.bytesPerRow * layout.rowsPerImage;

        // "Let bytesBeforeLastImage be bytesPerImage × (copyExtent.depthOrArrayLayers − 1)."
        // FIXME: Use checked arithmetic.
        auto bytesBeforeLastImage = bytesPerImage * (copyExtent.depthOrArrayLayers - 1);

        // "Add bytesBeforeLastImage to requiredBytesInCopy."
        // FIXME: Use checked arithmetic.
        requiredBytesInCopy += bytesBeforeLastImage;
    }

    // "If copyExtent.depthOrArrayLayers > 0:"
    if (copyExtent.depthOrArrayLayers > 0) {
        // "If heightInBlocks > 1"
        if (heightInBlocks > 1) {
            // "add layout.bytesPerRow × (heightInBlocks − 1) to requiredBytesInCopy."
            // FIXME: Use checked arithmetic.
            requiredBytesInCopy += layout.bytesPerRow * (heightInBlocks - 1);
        }

        // "If heightInBlocks > 0"
        if (heightInBlocks > 0) {
            // "add bytesInLastRow to requiredBytesInCopy."
            // FIXME: Use checked arithmetic.
            requiredBytesInCopy += bytesInLastRow;
        }
    }

    // "Fail if the following conditions are not satisfied:"
    // "layout.offset + requiredBytesInCopy ≤ byteSize."
    // FIXME: Use checked arithmetic.
    if (layout.offset + requiredBytesInCopy > byteSize)
        return false;

    return true;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuTextureRelease(WGPUTexture texture)
{
    WebGPU::fromAPI(texture).deref();
}

WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(texture).createView(*descriptor));
}

void wgpuTextureDestroy(WGPUTexture texture)
{
    WebGPU::fromAPI(texture).destroy();
}

void wgpuTextureSetLabel(WGPUTexture texture, const char* label)
{
    WebGPU::fromAPI(texture).setLabel(WebGPU::fromAPI(label));
}
