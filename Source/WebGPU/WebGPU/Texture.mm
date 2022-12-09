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
#import <wtf/CheckedArithmetic.h>
#import <wtf/MathExtras.h>

namespace WebGPU {

static std::optional<WGPUFeatureName> featureRequirementForFormat(WGPUTextureFormat format)
{
    switch (format) {
    case WGPUTextureFormat_Depth24UnormStencil8:
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
        return std::nullopt;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

static bool isCompressedFormat(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#packed-formats
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
        return false;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
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
        return 1;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return 0;
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
        return 1;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

static bool isRenderableFormat(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#renderable-format
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
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
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
        return false;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
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
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    // https://gpuweb.github.io/gpuweb/#depth-formats
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth24UnormStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return true;
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_RGB9E5Ufloat:
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
        return false;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

static uint32_t maximumMiplevelCount(WGPUTextureDimension dimension, WGPUExtent3D size)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-maximum-miplevel-count

    uint32_t m = 0;

    switch (dimension) {
    case WGPUTextureDimension_1D:
        return 1;
    case WGPUTextureDimension_2D:
        m = std::max(size.width, size.height);
        break;
    case WGPUTextureDimension_3D:
        m = std::max(std::max(size.width, size.height), size.depthOrArrayLayers);
        break;
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return 0;
    }

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
        return false;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

WGPUTextureFormat Texture::removeSRGBSuffix(WGPUTextureFormat format)
{
    switch (format) {
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
        return format;
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
        return WGPUTextureFormat_RGBA8Unorm;
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
        return format;
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
        return WGPUTextureFormat_BGRA8Unorm;
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
        return format;
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
        return WGPUTextureFormat_BC1RGBAUnorm;
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
        return WGPUTextureFormat_BC2RGBAUnorm;
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
        return WGPUTextureFormat_BC3RGBAUnorm;
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
        return format;
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return WGPUTextureFormat_BC7RGBAUnorm;
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
        return WGPUTextureFormat_ETC2RGB8Unorm;
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
        return WGPUTextureFormat_ETC2RGB8A1Unorm;
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
        return WGPUTextureFormat_ETC2RGBA8Unorm;
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
        return format;
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
        return WGPUTextureFormat_ASTC4x4Unorm;
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
        return WGPUTextureFormat_ASTC5x4Unorm;
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
        return WGPUTextureFormat_ASTC5x5Unorm;
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
        return WGPUTextureFormat_ASTC6x5Unorm;
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
        return WGPUTextureFormat_ASTC6x6Unorm;
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
        return WGPUTextureFormat_ASTC8x5Unorm;
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
        return WGPUTextureFormat_ASTC8x6Unorm;
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
        return WGPUTextureFormat_ASTC8x8Unorm;
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
        return WGPUTextureFormat_ASTC10x5Unorm;
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
        return WGPUTextureFormat_ASTC10x6Unorm;
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
        return WGPUTextureFormat_ASTC10x8Unorm;
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
        return WGPUTextureFormat_ASTC10x10Unorm;
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
        return WGPUTextureFormat_ASTC12x10Unorm;
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return WGPUTextureFormat_ASTC12x12Unorm;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return WGPUTextureFormat_Undefined;
    }
}

static bool textureViewFormatCompatible(WGPUTextureFormat format1, WGPUTextureFormat format2)
{
    // https://gpuweb.github.io/gpuweb/#texture-view-format-compatible

    if (format1 == format2)
        return true;

    return Texture::removeSRGBSuffix(format1) == Texture::removeSRGBSuffix(format2);
}

bool Device::validateCreateTexture(const WGPUTextureDescriptor& descriptor, const Vector<WGPUTextureFormat>& viewFormats)
{
    if (!isValid())
        return false;

    if (!descriptor.usage)
        return false;

    if (!descriptor.size.width || !descriptor.size.height || !descriptor.size.depthOrArrayLayers)
        return false;

    if (!descriptor.mipLevelCount)
        return false;

    if (descriptor.sampleCount != 1 && descriptor.sampleCount != 4)
        return false;

    switch (descriptor.dimension) {
    case WGPUTextureDimension_1D:
        if (descriptor.size.width > limits().maxTextureDimension1D)
            return false;

        if (descriptor.size.height != 1)
            return false;

        if (descriptor.size.depthOrArrayLayers != 1)
            return false;

        if (descriptor.sampleCount != 1)
            return false;

        if (isCompressedFormat(descriptor.format) || Texture::isDepthOrStencilFormat(descriptor.format))
            return false;
        break;
    case WGPUTextureDimension_2D:
        if (descriptor.size.width > limits().maxTextureDimension2D)
            return false;

        if (descriptor.size.height > limits().maxTextureDimension2D)
            return false;

        if (descriptor.size.depthOrArrayLayers > limits().maxTextureArrayLayers)
            return false;
        break;
    case WGPUTextureDimension_3D:
        if (descriptor.size.width > limits().maxTextureDimension3D)
            return false;

        if (descriptor.size.height > limits().maxTextureDimension3D)
            return false;

        if (descriptor.size.depthOrArrayLayers > limits().maxTextureDimension3D)
            return false;

        if (descriptor.sampleCount != 1)
            return false;

        if (isCompressedFormat(descriptor.format) || Texture::isDepthOrStencilFormat(descriptor.format))
            return false;
        break;
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }

    if (descriptor.size.width % Texture::texelBlockWidth(descriptor.format))
        return false;

    if (descriptor.size.height % Texture::texelBlockHeight(descriptor.format))
        return false;

    if (descriptor.sampleCount > 1) {
        if (descriptor.mipLevelCount != 1)
            return false;

        if (descriptor.size.depthOrArrayLayers != 1)
            return false;

        if (descriptor.usage & WGPUTextureUsage_StorageBinding)
            return false;

        if (!isRenderableFormat(descriptor.format))
            return false;

        if (!supportsMultisampling(descriptor.format))
            return false;
    }

    if (descriptor.mipLevelCount > maximumMiplevelCount(descriptor.dimension, descriptor.size))
        return false;

    if (descriptor.usage & WGPUTextureUsage_RenderAttachment) {
        if (!isRenderableFormat(descriptor.format))
            return false;

        if (descriptor.dimension != WGPUTextureDimension_2D)
            return false;
    }

    if (descriptor.usage & WGPUTextureUsage_StorageBinding) {
        if (!hasStorageBindingCapability(descriptor.format))
            return false;
    }

    for (auto viewFormat : viewFormats) {
        if (!textureViewFormatCompatible(descriptor.format, viewFormat))
            return false;
    }

    return true;
}

bool Device::validateCreateIOSurfaceBackedTexture(const WGPUTextureDescriptor& descriptor, const Vector<WGPUTextureFormat>& viewFormats, IOSurfaceRef backing)
{
    if (!isValid())
        return false;

    if (!backing)
        return false;

    if (!(descriptor.usage & WGPUTextureUsage_RenderAttachment))
        return false;

    // Metal only supports binding into BGRA8 and RGBA16Float IOTextures.
    // FIXME: add support for RGBA16 if necessary.
    if (descriptor.format != WGPUTextureFormat_BGRA8Unorm)
        return false;
    if (IOSurfaceGetPixelFormat(backing) != 'BGRA')
        return false;
    // BGRA8 is non-planar, check that the IOSurface is non-planar.
    if (IOSurfaceGetPlaneCount(backing))
        return false;

    // Check that the texture is a 2D texture with valid width/height, and has the same dimensions as the IOSurface.
    if (descriptor.dimension != WGPUTextureDimension_2D)
        return false;
    if (!descriptor.size.width || descriptor.size.width != IOSurfaceGetWidth(backing) || descriptor.size.width > limits().maxTextureDimension2D || descriptor.size.width % Texture::texelBlockWidth(descriptor.format))
        return false;
    if (!descriptor.size.height || descriptor.size.height != IOSurfaceGetHeight(backing) || descriptor.size.height > limits().maxTextureDimension2D || descriptor.size.height % Texture::texelBlockHeight(descriptor.format))
        return false;
    if (descriptor.size.depthOrArrayLayers != 1)
        return false;

    if (descriptor.mipLevelCount != 1)
        return false;

    // IOSurface-backed textures do not support multisampling.
    if (descriptor.sampleCount != 1)
        return false;

    for (auto viewFormat : viewFormats) {
        if (!textureViewFormatCompatible(descriptor.format, viewFormat))
            return false;
    }

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
    // FIXME(PERFORMANCE): Consider setting MTLTextureUsagePixelFormatView on all depth/stencil textures,
    // because supposedly it's free and it could be useful in writeBuffer().
    return result;
}

MTLPixelFormat Texture::pixelFormat(WGPUTextureFormat textureFormat)
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
        ASSERT_NOT_REACHED();
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

std::optional<MTLPixelFormat> Texture::depthOnlyAspectMetalFormat(WGPUTextureFormat textureFormat)
{
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
        return std::nullopt;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

std::optional<MTLPixelFormat> Texture::stencilOnlyAspectMetalFormat(WGPUTextureFormat textureFormat)
{
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
        return std::nullopt;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

static MTLStorageMode storageMode(bool deviceHasUnifiedMemory, bool supportsNonPrivateDepthStencilTextures, bool isBackedByIOSurface)
{
    // Metal driver requires IOSurface-backed texture to be MTLStorageModeManaged.
    if (isBackedByIOSurface)
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        return MTLStorageModeManaged;
#else
        return MTLStorageModeShared;
#endif

    // FIXME: only perform this check if the texture is a depth/stencil texture.
    if (!supportsNonPrivateDepthStencilTextures)
        return MTLStorageModePrivate;

    if (deviceHasUnifiedMemory)
        return MTLStorageModeShared;

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    return MTLStorageModeManaged;
#else
    return MTLStorageModePrivate;
#endif
}

Ref<Texture> Device::createTexture(const WGPUTextureDescriptor& descriptor)
{
    IOSurfaceRef ioSurfaceBacking = nullptr;
    Vector<WGPUTextureFormat> viewFormats;
    const auto* current = descriptor.nextInChain;
    while (current) {
        bool viewFormatsSpecified = false;
        if (current->sType == static_cast<WGPUSType>(WGPUSTypeExtended_TextureDescriptorViewFormats) && !viewFormatsSpecified) {
            if (viewFormatsSpecified)
                return Texture::createInvalid(*this);

            viewFormatsSpecified = true;

            const auto& descriptorViewFormats = reinterpret_cast<const WGPUTextureDescriptorViewFormats&>(*current);
            viewFormats = Vector { descriptorViewFormats.viewFormats, descriptorViewFormats.viewFormatsCount };
        } else if (current->sType == static_cast<WGPUSType>(WGPUSTypeExtended_TextureDescriptorCocoaSurfaceBacking)) {
            const auto& descriptorIOSurface = reinterpret_cast<const WGPUTextureDescriptorCocoaCustomSurface&>(*current);
            ioSurfaceBacking = descriptorIOSurface.surface;
        } else
            return Texture::createInvalid(*this);

        current = current->next;
    }

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createtexture

    if (auto requirement = featureRequirementForFormat(descriptor.format)) {
        if (!hasFeature(*requirement)) {
            // FIXME: "throw a TypeError."
            return Texture::createInvalid(*this);
        }
    }

    bool validationResult = ioSurfaceBacking ? validateCreateIOSurfaceBackedTexture(descriptor, viewFormats, ioSurfaceBacking) : validateCreateTexture(descriptor, viewFormats);
    if (!validationResult) {
        generateAValidationError("Validation failure."_s);
        return Texture::createInvalid(*this);
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
    case WGPUTextureDimension_3D:
        textureDescriptor.width = descriptor.size.width;
        textureDescriptor.height = descriptor.size.height;
        textureDescriptor.depth = descriptor.size.depthOrArrayLayers;
        textureDescriptor.textureType = MTLTextureType3D;
        break;
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return Texture::createInvalid(*this);
    }

    textureDescriptor.pixelFormat = Texture::pixelFormat(descriptor.format);

    textureDescriptor.mipmapLevelCount = descriptor.mipLevelCount;

    textureDescriptor.sampleCount = descriptor.sampleCount;

    textureDescriptor.storageMode = storageMode(hasUnifiedMemory(), baseCapabilities().supportsNonPrivateDepthStencilTextures, ioSurfaceBacking);

    // FIXME(PERFORMANCE): Consider write-combining CPU cache mode.
    // FIXME(PERFORMANCE): Consider implementing hazard tracking ourself.

    id<MTLTexture> texture = nil;
    if (ioSurfaceBacking)
        texture = [m_device newTextureWithDescriptor:textureDescriptor iosurface:ioSurfaceBacking plane:0];
    else
        texture = [m_device newTextureWithDescriptor:textureDescriptor];

    if (!texture)
        return Texture::createInvalid(*this);

    texture.label = fromAPI(descriptor.label);

    return Texture::create(texture, descriptor, WTFMove(viewFormats), *this);
}

Texture::Texture(id<MTLTexture> texture, const WGPUTextureDescriptor& descriptor, Vector<WGPUTextureFormat>&& viewFormats, Device& device)
    : m_texture(texture)
    , m_descriptor(descriptor)
    , m_viewFormats(WTFMove(viewFormats))
    , m_device(device)
{
}

Texture::Texture(Device& device)
    : m_device(device)
{
}

Texture::~Texture() = default;

std::optional<WGPUTextureViewDescriptor> Texture::resolveTextureViewDescriptorDefaults(const WGPUTextureViewDescriptor& descriptor) const
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-resolving-gputextureviewdescriptor-defaults

    WGPUTextureViewDescriptor resolved = descriptor;

    if (resolved.format == WGPUTextureFormat_Undefined) {
        if (auto format = resolveTextureFormat(m_descriptor.format, descriptor.aspect))
            resolved.format = *format;
        else
            resolved.format = m_descriptor.format;
    }

    if (resolved.mipLevelCount == WGPU_MIP_LEVEL_COUNT_UNDEFINED) {
        auto mipLevelCount = checkedDifference<uint32_t>(m_descriptor.mipLevelCount, resolved.baseMipLevel);
        if (mipLevelCount.hasOverflowed())
            return std::nullopt;
        resolved.mipLevelCount = mipLevelCount.value();
    }

    if (resolved.dimension == WGPUTextureViewDimension_Undefined) {
        switch (m_descriptor.dimension) {
        case WGPUTextureDimension_1D:
            resolved.dimension = WGPUTextureViewDimension_1D;
            break;
        case WGPUTextureDimension_2D:
            resolved.dimension = WGPUTextureViewDimension_2D;
            break;
        case WGPUTextureDimension_3D:
            resolved.dimension = WGPUTextureViewDimension_3D;
            break;
        case WGPUTextureDimension_Force32:
            ASSERT_NOT_REACHED();
            return resolved;
        }
    }

    if (resolved.arrayLayerCount == WGPU_ARRAY_LAYER_COUNT_UNDEFINED) {
        switch (resolved.dimension) {
        case WGPUTextureViewDimension_Undefined:
            return resolved;
        case WGPUTextureViewDimension_1D:
        case WGPUTextureViewDimension_2D:
        case WGPUTextureViewDimension_3D:
            resolved.arrayLayerCount = 1;
            break;
        case WGPUTextureViewDimension_Cube:
            resolved.arrayLayerCount = 6;
            break;
        case WGPUTextureViewDimension_2DArray:
        case WGPUTextureViewDimension_CubeArray: {
            auto arrayLayerCount = checkedDifference<uint32_t>(m_descriptor.size.depthOrArrayLayers, resolved.baseArrayLayer);
            if (arrayLayerCount.hasOverflowed())
                return std::nullopt;
            resolved.arrayLayerCount = arrayLayerCount.value();
            break;
        }
        case WGPUTextureViewDimension_Force32:
            ASSERT_NOT_REACHED();
            return resolved;
        }
    }

    return resolved;
}

std::optional<WGPUTextureFormat> Texture::resolveTextureFormat(WGPUTextureFormat format, WGPUTextureAspect aspect)
{
    switch (aspect) {
    case WGPUTextureAspect_All:
        return format;
    case WGPUTextureAspect_DepthOnly:
        return depthSpecificFormat(format);
    case WGPUTextureAspect_StencilOnly:
        return stencilSpecificFormat(format);
    case WGPUTextureAspect_Force32:
    default:
        return { };
    }
}

uint32_t Texture::arrayLayerCount() const
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-array-layer-count

    switch (m_descriptor.dimension) {
    case WGPUTextureDimension_1D:
        return 1;
    case WGPUTextureDimension_2D:
        return m_descriptor.size.depthOrArrayLayers;
    case WGPUTextureDimension_3D:
        return 1;
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return 1;
    }
}

bool Texture::validateCreateView(const WGPUTextureViewDescriptor& descriptor) const
{
    if (!isValid())
        return false;

    if (descriptor.aspect == WGPUTextureAspect_All) {
        if (descriptor.format != m_descriptor.format && !m_viewFormats.contains(descriptor.format))
            return false;
    } else {
        if (descriptor.format != resolveTextureFormat(m_descriptor.format, descriptor.aspect))
            return false;
    }

    if (!descriptor.mipLevelCount)
        return false;

    auto endMipLevel = checkedSum<uint32_t>(descriptor.baseMipLevel, descriptor.mipLevelCount);
    if (endMipLevel.hasOverflowed() || endMipLevel.value() > m_descriptor.mipLevelCount)
        return false;

    if (!descriptor.arrayLayerCount)
        return false;

    auto endArrayLayer = checkedSum<uint32_t>(descriptor.baseArrayLayer, descriptor.arrayLayerCount);
    if (endArrayLayer.hasOverflowed() || endArrayLayer.value() > arrayLayerCount())
        return false;

    if (m_descriptor.sampleCount > 1) {
        if (descriptor.dimension != WGPUTextureViewDimension_2D)
            return false;
    }

    switch (descriptor.dimension) {
    case WGPUTextureViewDimension_Undefined:
        return false;
    case WGPUTextureViewDimension_1D:
        if (m_descriptor.dimension != WGPUTextureDimension_1D)
            return false;

        if (descriptor.arrayLayerCount != 1)
            return false;
        break;
    case WGPUTextureViewDimension_2D:
        if (m_descriptor.dimension != WGPUTextureDimension_2D)
            return false;

        if (descriptor.arrayLayerCount != 1)
            return false;
        break;
    case WGPUTextureViewDimension_2DArray:
        if (m_descriptor.dimension != WGPUTextureDimension_2D)
            return false;
        break;
    case WGPUTextureViewDimension_Cube:
        if (m_descriptor.dimension != WGPUTextureDimension_2D)
            return false;

        if (descriptor.arrayLayerCount != 6)
            return false;

        if (m_descriptor.size.width != m_descriptor.size.height)
            return false;
        break;
    case WGPUTextureViewDimension_CubeArray:
        if (m_descriptor.dimension != WGPUTextureDimension_2D)
            return false;

        if (descriptor.arrayLayerCount % 6)
            return false;

        if (m_descriptor.size.width != m_descriptor.size.height)
            return false;
        break;
    case WGPUTextureViewDimension_3D:
        if (m_descriptor.dimension != WGPUTextureDimension_3D)
            return false;

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

    WGPUExtent3D extent { };

    extent.width = std::max(static_cast<uint32_t>(1), baseSize.width >> mipLevel);

    extent.height = std::max(static_cast<uint32_t>(1), baseSize.height >> mipLevel);

    extent.depthOrArrayLayers = 1;

    return extent;
}

Ref<TextureView> Texture::createView(const WGPUTextureViewDescriptor& inputDescriptor)
{
    if (inputDescriptor.nextInChain)
        return TextureView::createInvalid(m_device);

    // https://gpuweb.github.io/gpuweb/#dom-gputexture-createview

    auto descriptor = resolveTextureViewDescriptorDefaults(inputDescriptor);

    if (!descriptor || !validateCreateView(*descriptor)) {
        m_device->generateAValidationError("Validation failure."_s);
        return TextureView::createInvalid(m_device);
    }

    auto pixelFormat = Texture::pixelFormat(descriptor->format);
    ASSERT(pixelFormat != MTLPixelFormatInvalid);

    MTLTextureType textureType;
    switch (descriptor->dimension) {
    case WGPUTextureViewDimension_Undefined:
        ASSERT_NOT_REACHED();
        return TextureView::createInvalid(m_device);
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
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
            return TextureView::createInvalid(m_device);
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
        ASSERT_NOT_REACHED();
        return TextureView::createInvalid(m_device);
    }

    auto levels = NSMakeRange(descriptor->baseMipLevel, descriptor->mipLevelCount);

    auto slices = NSMakeRange(descriptor->baseArrayLayer, descriptor->arrayLayerCount);

    id<MTLTexture> texture = [m_texture newTextureViewWithPixelFormat:pixelFormat textureType:textureType levels:levels slices:slices];
    if (!texture)
        return TextureView::createInvalid(m_device);

    texture.label = fromAPI(descriptor->label);

    std::optional<WGPUExtent3D> renderExtent;
    if  (m_descriptor.usage & WGPUTextureUsage_RenderAttachment)
        renderExtent = computeRenderExtent(m_descriptor.size, descriptor->baseMipLevel);

    return TextureView::create(texture, *descriptor, renderExtent, m_device);
}

void Texture::destroy()
{
    // https://gpuweb.github.io/gpuweb/#dom-gputexture-destroy

    m_texture = nil;
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
            std::max(static_cast<uint32_t>(1), m_descriptor.size.width >> mipLevel),
            1,
            m_descriptor.size.depthOrArrayLayers };
    case WGPUTextureDimension_2D:
        return {
            std::max(static_cast<uint32_t>(1), m_descriptor.size.width >> mipLevel),
            std::max(static_cast<uint32_t>(1), m_descriptor.size.height >> mipLevel),
            m_descriptor.size.depthOrArrayLayers };
    case WGPUTextureDimension_3D:
        return {
            std::max(static_cast<uint32_t>(1), m_descriptor.size.width >> mipLevel),
            std::max(static_cast<uint32_t>(1), m_descriptor.size.height >> mipLevel),
            std::max(static_cast<uint32_t>(1), m_descriptor.size.depthOrArrayLayers >> mipLevel) };
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return WGPUExtent3D { };
    }
}

WGPUExtent3D Texture::physicalMiplevelSpecificTextureExtent(uint32_t mipLevel)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-physical-miplevel-specific-texture-extent

    auto logicalExtent = logicalMiplevelSpecificTextureExtent(mipLevel);

    switch (m_descriptor.dimension) {
    case WGPUTextureDimension_1D:
        return {
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockWidth(m_descriptor.format), logicalExtent.width)),
            1,
            logicalExtent.depthOrArrayLayers };
    case WGPUTextureDimension_2D:
        return {
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockWidth(m_descriptor.format), logicalExtent.width)),
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockHeight(m_descriptor.format), logicalExtent.height)),
            logicalExtent.depthOrArrayLayers };
    case WGPUTextureDimension_3D:
        return {
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockWidth(m_descriptor.format), logicalExtent.width)),
            static_cast<uint32_t>(WTF::roundUpToMultipleOf(texelBlockHeight(m_descriptor.format), logicalExtent.height)),
            logicalExtent.depthOrArrayLayers };
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return WGPUExtent3D { };
    }
}

static WGPUExtent3D imageCopyTextureSubresourceSize(const WGPUImageCopyTexture& imageCopyTexture)
{
    // https://gpuweb.github.io/gpuweb/#imagecopytexture-subresource-size

    return fromAPI(imageCopyTexture.texture).physicalMiplevelSpecificTextureExtent(imageCopyTexture.mipLevel);
}

bool Texture::validateImageCopyTexture(const WGPUImageCopyTexture& imageCopyTexture, const WGPUExtent3D& copySize)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpuimagecopytexture

    uint32_t blockWidth = Texture::texelBlockWidth(fromAPI(imageCopyTexture.texture).descriptor().format);

    uint32_t blockHeight = Texture::texelBlockHeight(fromAPI(imageCopyTexture.texture).descriptor().format);

    if (!fromAPI(imageCopyTexture.texture).isValid())
        return false;

    if (imageCopyTexture.mipLevel >= fromAPI(imageCopyTexture.texture).descriptor().mipLevelCount)
        return false;

    if (imageCopyTexture.origin.x % blockWidth)
        return false;

    if (imageCopyTexture.origin.y % blockHeight)
        return false;

    if (Texture::isDepthOrStencilFormat(fromAPI(imageCopyTexture.texture).descriptor().format)
        || fromAPI(imageCopyTexture.texture).descriptor().sampleCount > 1) {
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
        ASSERT_NOT_REACHED();
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
        return false;
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
        return true;
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

    auto blockWidth = Texture::texelBlockWidth(fromAPI(imageCopyTexture.texture).descriptor().format);

    auto blockHeight = Texture::texelBlockHeight(fromAPI(imageCopyTexture.texture).descriptor().format);

    auto subresourceSize = imageCopyTextureSubresourceSize(imageCopyTexture);

    auto endX = checkedSum<uint32_t>(imageCopyTexture.origin.x, copySize.width);
    if (endX.hasOverflowed() || endX.value() > subresourceSize.width)
        return false;

    auto endY = checkedSum<uint32_t>(imageCopyTexture.origin.y, copySize.height);
    if (endY.hasOverflowed() || endY.value() > subresourceSize.height)
        return false;

    auto endZ = checkedSum<uint32_t>(imageCopyTexture.origin.z, copySize.depthOrArrayLayers);
    if (endZ.hasOverflowed() || endZ.value() > subresourceSize.depthOrArrayLayers)
        return false;

    if (copySize.width % blockWidth)
        return false;

    if (copySize.height % blockHeight)
        return false;

    return true;
}

bool Texture::validateLinearTextureData(const WGPUTextureDataLayout& layout, uint64_t byteSize, WGPUTextureFormat format, WGPUExtent3D copyExtent)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-linear-texture-data

    uint32_t blockWidth = Texture::texelBlockWidth(format);
    uint32_t blockHeight = Texture::texelBlockHeight(format);
    uint32_t blockSize = Texture::texelBlockSize(format);

    auto widthInBlocks = copyExtent.width / blockWidth;

    auto heightInBlocks = copyExtent.height / blockHeight;

    auto bytesInLastRow = checkedProduct<uint64_t>(blockSize, widthInBlocks);
    if (bytesInLastRow.hasOverflowed())
        return false;

    if (heightInBlocks > 1) {
        if (layout.bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
            return false;
    }

    if (copyExtent.depthOrArrayLayers > 1) {
        if (layout.bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED || layout.rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED)
            return false;
    }

    if (layout.bytesPerRow != WGPU_COPY_STRIDE_UNDEFINED) {
        if (layout.bytesPerRow < bytesInLastRow.value())
            return false;
    }

    if (layout.rowsPerImage != WGPU_COPY_STRIDE_UNDEFINED) {
        if (layout.rowsPerImage < heightInBlocks)
            return false;
    }

    auto requiredBytesInCopy = CheckedUint64(0);

    if (copyExtent.depthOrArrayLayers > 1) {
        auto bytesPerImage = checkedProduct<uint64_t>(layout.bytesPerRow, layout.rowsPerImage);

        auto bytesBeforeLastImage = checkedProduct<uint64_t>(bytesPerImage, checkedDifference<uint64_t>(copyExtent.depthOrArrayLayers, 1));

        requiredBytesInCopy += bytesBeforeLastImage;
    }

    if (copyExtent.depthOrArrayLayers > 0) {
        if (heightInBlocks > 1)
            requiredBytesInCopy += checkedProduct<uint64_t>(layout.bytesPerRow, checkedDifference<uint64_t>(heightInBlocks, 1));

        if (heightInBlocks > 0)
            requiredBytesInCopy += bytesInLastRow;
    }

    auto end = checkedSum<uint64_t>(layout.offset, requiredBytesInCopy);
    if (end.hasOverflowed() || end.value() > byteSize)
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
