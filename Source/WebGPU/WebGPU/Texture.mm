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
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
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

static bool isDepthOrStencilFormat(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#depth-formats
    // "A depth-or-stencil format is any format with depth and/or stencil aspects."
    switch (format) {
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
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

static uint32_t texelBlockWidth(WGPUTextureFormat format)
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

static uint32_t texelBlockHeight(WGPUTextureFormat format)
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
        if (isCompressedFormat(descriptor.format) || isDepthOrStencilFormat(descriptor.format))
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
        if (isCompressedFormat(descriptor.format) || isDepthOrStencilFormat(descriptor.format))
            return false;
        break;
    case WGPUTextureDimension_Force32:
        return false;
    }

    // "descriptor.size.width must be multiple of texel block width."
    if (descriptor.size.width % texelBlockWidth(descriptor.format))
        return false;

    // "descriptor.size.height must be multiple of texel block height."
    if (descriptor.size.height % texelBlockHeight(descriptor.format))
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

static std::optional<MTLPixelFormat> pixelFormat(WGPUTextureFormat textureFormat)
{
    switch (textureFormat) {
    case WGPUTextureFormat_Undefined:
        return std::nullopt;
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
    case WGPUTextureFormat_Depth24UnormStencil8:
        return MTLPixelFormatDepth24Unorm_Stencil8;
    case WGPUTextureFormat_Depth32Float:
        return MTLPixelFormatDepth32Float;
    case WGPUTextureFormat_Depth32FloatStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
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
    case WGPUTextureFormat_ETC2RGB8Unorm:
        return MTLPixelFormatETC2_RGB8;
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
        return MTLPixelFormatETC2_RGB8_sRGB;
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
        return MTLPixelFormatETC2_RGB8A1;
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
        return MTLPixelFormatETC2_RGB8A1_sRGB;
    case WGPUTextureFormat_ETC2RGBA8Unorm:
        return std::nullopt; // https://github.com/gpuweb/gpuweb/issues/2683 etc2-rgba8unorm and etc2-rgba8unorm-srgb do not exist in Metal
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
        return std::nullopt; // https://github.com/gpuweb/gpuweb/issues/2683 etc2-rgba8unorm and etc2-rgba8unorm-srgb do not exist in Metal
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
            if (descriptor.sampleCount > 1)
                textureDescriptor.textureType = MTLTextureType2DMultisampleArray;
            else
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

    if (auto pixelFormat = WebGPU::pixelFormat(descriptor.format))
        textureDescriptor.pixelFormat = pixelFormat.value();
    else
        return nullptr;

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

    return Texture::create(texture, descriptor);
}

Texture::Texture(id<MTLTexture> texture, const WGPUTextureDescriptor& descriptor)
    : m_texture(texture)
    , m_descriptor(descriptor)
{
}

Texture::~Texture() = default;

RefPtr<TextureView> Texture::createView(const WGPUTextureViewDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return TextureView::create(nil);
}

void Texture::destroy()
{
}

void Texture::setLabel(String&& label)
{
    m_texture.label = label;
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
