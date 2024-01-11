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
#import "Device.h"
#import "TextureView.h"
#import <bmalloc/Algorithm.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/MathExtras.h>

namespace WebGPU {

static std::optional<WGPUFeatureName> featureRequirementForFormat(WGPUTextureFormat format)
{
    switch (format) {
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
    case WGPUTextureFormat_RGB10A2Uint:
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
        return std::nullopt;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

bool Texture::isCompressedFormat(WGPUTextureFormat format)
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
    case WGPUTextureFormat_RGB10A2Uint:
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
    case WGPUTextureFormat_Depth32FloatStencil8:
        return false;
    case WGPUTextureFormat_Undefined:
        return false;
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
    case WGPUTextureFormat_RGB10A2Uint:
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
        return std::nullopt;
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
    case WGPUTextureFormat_RGB10A2Uint:
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
        return std::nullopt;
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
    case WGPUTextureFormat_RGB10A2Uint:
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
    case WGPUTextureFormat_Depth32FloatStencil8:
        return 1;
    case WGPUTextureFormat_Undefined:
        return 0;
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
    case WGPUTextureFormat_RGB10A2Uint:
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
    case WGPUTextureFormat_Depth32FloatStencil8:
        return 1;
    case WGPUTextureFormat_Undefined:
        return 0;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

bool Texture::isColorRenderableFormat(WGPUTextureFormat format, const Device& device)
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
    case WGPUTextureFormat_RGB10A2Uint:
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
        return true;
    case WGPUTextureFormat_RG11B10Ufloat:
        return device.hasFeature(WGPUFeatureName_RG11B10UfloatRenderable);
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return false;
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RGBA8Snorm:
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
        return false;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool Texture::isDepthStencilRenderableFormat(WGPUTextureFormat format, const Device&)
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
    case WGPUTextureFormat_RGB10A2Uint:
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
    case WGPUTextureFormat_RG11B10Ufloat:
        return false;
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return true;
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RGBA8Snorm:
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
        return false;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool Texture::isRenderableFormat(WGPUTextureFormat format, const Device& device)
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
    case WGPUTextureFormat_RGB10A2Uint:
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
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return true;
    case WGPUTextureFormat_RG11B10Ufloat:
        return device.hasFeature(WGPUFeatureName_RG11B10UfloatRenderable);
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RGBA8Snorm:
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
        return false;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

uint32_t Texture::renderTargetPixelByteCost(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#renderable-format
    switch (format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
        return 1;
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
        return 2;
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
        return 4;
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
        return 8;
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
        return 4;
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGB10A2Unorm:
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
    case WGPUTextureFormat_RG11B10Ufloat:
        return 8;
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RGBA8Snorm:
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
        return 0;
    case WGPUTextureFormat_Undefined:
        return 0;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

uint32_t Texture::renderTargetPixelByteAlignment(WGPUTextureFormat format)
{
    // https://gpuweb.github.io/gpuweb/#renderable-format
    switch (format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
        return 1;
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
        return 2;
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_RG11B10Ufloat:
        return 4;
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RGBA8Snorm:
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
        return 0;
    case WGPUTextureFormat_Undefined:
        return 0;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

bool Texture::supportsMultisampling(WGPUTextureFormat format, const Device& device)
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
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    // https://gpuweb.github.io/gpuweb/#depth-formats
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return true;
    case WGPUTextureFormat_RG11B10Ufloat:
        return device.hasFeature(WGPUFeatureName_RG11B10UfloatRenderable);
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
        return false;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool Texture::supportsResolve(WGPUTextureFormat format, const Device& device)
{
    switch (format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RGBA16Float:
        return true;
    case WGPUTextureFormat_RG11B10Ufloat:
        return device.hasFeature(WGPUFeatureName_RG11B10UfloatRenderable);
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
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
        return false;
    case WGPUTextureFormat_Force32:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

bool Texture::supportsBlending(WGPUTextureFormat format, const Device& device)
{
    switch (format) {
    // https://gpuweb.github.io/gpuweb/#texture-format-caps
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGB10A2Unorm:
        return true;
    case WGPUTextureFormat_RG11B10Ufloat:
        return device.hasFeature(WGPUFeatureName_RG11B10UfloatRenderable);
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
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
        return false;
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

bool Texture::hasStorageBindingCapability(WGPUTextureFormat format, const Device& device, WGPUStorageTextureAccess access)
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
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
        return access != WGPUStorageTextureAccess_ReadWrite;
    case WGPUTextureFormat_BGRA8Unorm:
        return access != WGPUStorageTextureAccess_ReadWrite && device.hasFeature(WGPUFeatureName_BGRA8UnormStorage);
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
        return true;
    case WGPUTextureFormat_RGBA8UnormSrgb:
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
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
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
        return false;
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
    case WGPUTextureFormat_RGB10A2Uint:
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
        return WGPUTextureFormat_Undefined;
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

NSString *Device::errorValidatingTextureCreation(const WGPUTextureDescriptor& descriptor, const Vector<WGPUTextureFormat>& viewFormats)
{
    if (!isValid())
        return @"createTexture: Device is not valid";

    if (!descriptor.usage)
        return @"createTexture: descriptor.usage is zero";

    if (!descriptor.size.width || !descriptor.size.height || !descriptor.size.depthOrArrayLayers)
        return @"createTexture: descriptor.size.width/height/depth is zero";

    if (!descriptor.mipLevelCount)
        return @"createTexture: descriptor.mipLevelCount is zero";

    if (descriptor.sampleCount != 1 && descriptor.sampleCount != 4)
        return @"createTexture: descriptor.sampleCount is neither 1 nor 4";

    switch (descriptor.dimension) {
    case WGPUTextureDimension_1D:
        if (descriptor.size.width > limits().maxTextureDimension1D)
            return @"createTexture: descriptor.size.width is greater than limits().maxTextureDimension1D";

        if (descriptor.size.height != 1)
            return @"createTexture: descriptor.size.height != 1";

        if (descriptor.size.depthOrArrayLayers != 1)
            return @"createTexture: descriptor.size.depthOrArrayLayers != 1";

        if (descriptor.sampleCount != 1)
            return @"createTexture: descriptor.sampleCount != 1";

        if (Texture::isCompressedFormat(descriptor.format) || Texture::isDepthOrStencilFormat(descriptor.format))
            return @"createTexture: descriptor.format is compressed or a depth stencil format";
        break;
    case WGPUTextureDimension_2D:
        if (descriptor.size.width > limits().maxTextureDimension2D)
            return @"createTexture: descriptor.size.width is greater than limits().maxTextureDimension2D";

        if (descriptor.size.height > limits().maxTextureDimension2D)
            return @"createTexture: descriptor.size.height is greater than limits().maxTextureDimension2D";

        if (descriptor.size.depthOrArrayLayers > limits().maxTextureArrayLayers)
            return @"createTexture: descriptor.size.depthOrArrayLayers > limits().maxTextureArrayLayers";
        break;
    case WGPUTextureDimension_3D:
        if (descriptor.size.width > limits().maxTextureDimension3D)
            return @"createTexture: descriptor.size.width > limits().maxTextureDimension3D";

        if (descriptor.size.height > limits().maxTextureDimension3D)
            return @"createTexture: descriptor.size.height > limits().maxTextureDimension3D";

        if (descriptor.size.depthOrArrayLayers > limits().maxTextureDimension3D)
            return @"createTexture: descriptor.size.depthOrArrayLayers > limits().maxTextureDimension3D";

        if (descriptor.sampleCount != 1)
            return @"createTexture: descriptor.sampleCount != 1";

        if (Texture::isCompressedFormat(descriptor.format) || Texture::isDepthOrStencilFormat(descriptor.format))
            return @"createTexture: descriptor.format is compressed or a depth stencil format";
        break;
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return @"createTexture: descriptor.dimension is WGPUTextureDimension_Force32";
    }

    if (descriptor.size.width % Texture::texelBlockWidth(descriptor.format))
        return @"createTexture: descriptor.size.width % Texture::texelBlockWidth(descriptor.format)";

    if (descriptor.size.height % Texture::texelBlockHeight(descriptor.format))
        return @"createTexture: descriptor.size.height % Texture::texelBlockHeight(descriptor.format)";

    if (descriptor.sampleCount > 1) {
        if (descriptor.mipLevelCount != 1)
            return @"createTexture: descriptor.sampleCount > 1 and descriptor.mipLevelCount != 1";

        if (descriptor.size.depthOrArrayLayers != 1)
            return @"createTexture: descriptor.sampleCount > 1 and descriptor.size.depthOrArrayLayers != 1";

        if ((descriptor.usage & WGPUTextureUsage_StorageBinding) || !(descriptor.usage & WGPUTextureUsage_RenderAttachment))
            return @"createTexture: descriptor.sampleCount > 1 and (descriptor.usage & WGPUTextureUsage_StorageBinding) || !(descriptor.usage & WGPUTextureUsage_RenderAttachment)";

        if (!Texture::isRenderableFormat(descriptor.format, *this))
            return @"createTexture: descriptor.sampleCount > 1 and !isRenderableFormat(descriptor.format, *this)";

        if (!Texture::supportsMultisampling(descriptor.format, *this))
            return @"createTexture: descriptor.sampleCount > 1 and !supportsMultisampling(descriptor.format, *this)";
    }

    if (descriptor.mipLevelCount > maximumMiplevelCount(descriptor.dimension, descriptor.size))
        return @"createTexture: descriptor.mipLevelCount > maximumMiplevelCount(descriptor.dimension, descriptor.size)";

    if (descriptor.usage & WGPUTextureUsage_RenderAttachment) {
        if (!Texture::isRenderableFormat(descriptor.format, *this))
            return @"createTexture: descriptor.usage & WGPUTextureUsage_RenderAttachment && !isRenderableFormat(descriptor.format, *this)";

        if (descriptor.dimension == WGPUTextureDimension_1D)
            return @"createTexture: descriptor.usage & WGPUTextureUsage_RenderAttachment && descriptor.dimension == WGPUTextureDimension_1D";
    }

    if (descriptor.usage & WGPUTextureUsage_StorageBinding) {
        if (!Texture::hasStorageBindingCapability(descriptor.format, *this))
            return @"createTexture: descriptor.usage & WGPUTextureUsage_StorageBinding && !hasStorageBindingCapability(descriptor.format)";
    }

    for (auto viewFormat : viewFormats) {
        if (!textureViewFormatCompatible(descriptor.format, viewFormat))
            return @"createTexture: !textureViewFormatCompatible(descriptor.format, viewFormat)";
    }

    return nil;
}

MTLTextureUsage Texture::usage(WGPUTextureUsageFlags usage, WGPUTextureFormat format)
{
    MTLTextureUsage result = MTLTextureUsageUnknown;
    if (usage & WGPUTextureUsage_TextureBinding)
        result |= MTLTextureUsageShaderRead;
    if (usage & WGPUTextureUsage_StorageBinding)
        result |= MTLTextureUsageShaderWrite;
    if (usage & WGPUTextureUsage_RenderAttachment)
        result |= MTLTextureUsageRenderTarget;
    if (Texture::isDepthOrStencilFormat(format) || Texture::isCompressedFormat(format))
        result |= MTLTextureUsagePixelFormatView;
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
    case WGPUTextureFormat_RGB10A2Uint:
        return MTLPixelFormatRGB10A2Uint;
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
        return MTLPixelFormatInvalid;
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
    case WGPUTextureFormat_RGB10A2Uint:
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
        return 16;
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
        return 8;
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
        return 8;
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
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
        return 0;
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
    case WGPUTextureFormat_RGB10A2Uint:
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
        return std::nullopt;
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
    case WGPUTextureFormat_RGB10A2Uint:
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
        return std::nullopt;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

static MTLStorageMode storageMode(bool deviceHasUnifiedMemory, bool supportsNonPrivateDepthStencilTextures)
{

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
    if (descriptor.nextInChain || !isValid())
        return Texture::createInvalid(*this);

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createtexture

    if (auto requirement = featureRequirementForFormat(descriptor.format)) {
        if (!hasFeature(*requirement)) {
            // FIXME: "throw a TypeError."
            return Texture::createInvalid(*this);
        }
    }

    Vector viewFormats = { descriptor.viewFormats, descriptor.viewFormatCount };

    if (NSString *error = errorValidatingTextureCreation(descriptor, viewFormats)) {
        generateAValidationError(error);
        return Texture::createInvalid(*this);
    }

    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor new];

    textureDescriptor.usage = Texture::usage(descriptor.usage, descriptor.format);

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

    textureDescriptor.storageMode = storageMode(hasUnifiedMemory(), baseCapabilities().supportsNonPrivateDepthStencilTextures);

    // FIXME(PERFORMANCE): Consider write-combining CPU cache mode.
    // FIXME(PERFORMANCE): Consider implementing hazard tracking ourself.

    id<MTLTexture> texture = [m_device newTextureWithDescriptor:textureDescriptor];

    if (!texture)
        return Texture::createInvalid(*this);

    texture.label = fromAPI(descriptor.label);

    return Texture::create(texture, descriptor, WTFMove(viewFormats), *this);
}

Texture::Texture(id<MTLTexture> texture, const WGPUTextureDescriptor& descriptor, Vector<WGPUTextureFormat>&& viewFormats, Device& device)
    : m_texture(texture)
    , m_width(descriptor.size.width)
    , m_height(descriptor.size.height)
    , m_depthOrArrayLayers(descriptor.size.depthOrArrayLayers)
    , m_mipLevelCount(descriptor.mipLevelCount)
    , m_sampleCount(descriptor.sampleCount)
    , m_dimension(descriptor.dimension)
    , m_format(descriptor.format)
    , m_usage(descriptor.usage)
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
        if (auto format = resolveTextureFormat(m_format, descriptor.aspect))
            resolved.format = *format;
        else
            resolved.format = m_format;
    }

    if (resolved.mipLevelCount == WGPU_MIP_LEVEL_COUNT_UNDEFINED) {
        auto mipLevelCount = checkedDifference<uint32_t>(m_mipLevelCount, resolved.baseMipLevel);
        if (mipLevelCount.hasOverflowed())
            return std::nullopt;
        resolved.mipLevelCount = mipLevelCount.value();
    }

    if (resolved.dimension == WGPUTextureViewDimension_Undefined) {
        switch (m_texture.textureType) {
        case MTLTextureType1D:
            resolved.dimension = WGPUTextureViewDimension_1D;
            break;
        case MTLTextureType1DArray:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case MTLTextureType2D:
        case MTLTextureType2DMultisample:
            resolved.dimension = WGPUTextureViewDimension_2D;
            break;
        case MTLTextureType2DArray:
        case MTLTextureType2DMultisampleArray:
            resolved.dimension = WGPUTextureViewDimension_2DArray;
            break;
        case MTLTextureTypeCube:
            resolved.dimension = WGPUTextureViewDimension_Cube;
            break;
        case MTLTextureTypeCubeArray:
            resolved.dimension = WGPUTextureViewDimension_CubeArray;
            break;
        case MTLTextureType3D:
            resolved.dimension = WGPUTextureViewDimension_3D;
            break;
        case MTLTextureTypeTextureBuffer:
            ASSERT_NOT_REACHED();
            break;
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
            auto arrayLayerCount = checkedDifference<uint32_t>(m_depthOrArrayLayers, resolved.baseArrayLayer);
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

    switch (m_dimension) {
    case WGPUTextureDimension_1D:
        return 1;
    case WGPUTextureDimension_2D:
        return m_depthOrArrayLayers;
    case WGPUTextureDimension_3D:
        return 1;
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return 1;
    }
}

NSString* Texture::errorValidatingTextureViewCreation(const WGPUTextureViewDescriptor& descriptor) const
{
#define ERROR_STRING(x) (@"GPUTexture.createView: " x)
    if (!isValid())
        return ERROR_STRING(@"texture is not valid");

    if (descriptor.aspect == WGPUTextureAspect_All) {
        if (descriptor.format != m_format && !m_viewFormats.contains(descriptor.format))
            return ERROR_STRING(@"aspect == all and (format != parentTexture's format and !viewFormats.contains(parentTexture's format))");
    } else {
        if (descriptor.format != resolveTextureFormat(m_format, descriptor.aspect))
            return ERROR_STRING(@"aspect == All and (format != resolveTextureFormat(format, aspect))");
    }

    if (!descriptor.mipLevelCount)
        return ERROR_STRING(@"!mipLevelCount");

    auto endMipLevel = checkedSum<uint32_t>(descriptor.baseMipLevel, descriptor.mipLevelCount);
    if (endMipLevel.hasOverflowed() || endMipLevel.value() > m_mipLevelCount)
        return ERROR_STRING(@"endMipLevel is not valid");

    if (!descriptor.arrayLayerCount)
        return ERROR_STRING(@"!arrayLayerCount");

    auto endArrayLayer = checkedSum<uint32_t>(descriptor.baseArrayLayer, descriptor.arrayLayerCount);
    if (endArrayLayer.hasOverflowed() || endArrayLayer.value() > arrayLayerCount())
        return ERROR_STRING(@"endArrayLayer is not valid");

    if (m_sampleCount > 1) {
        if (descriptor.dimension != WGPUTextureViewDimension_2D)
            return ERROR_STRING(@"sampleCount > 1 and dimension != 2D");
    }

    switch (descriptor.dimension) {
    case WGPUTextureViewDimension_Undefined:
        return ERROR_STRING(@"dimension is undefined");
    case WGPUTextureViewDimension_1D:
        if (m_dimension != WGPUTextureDimension_1D)
            return ERROR_STRING(@"attempting to create 1D texture view from non-1D base texture");

        if (descriptor.arrayLayerCount != 1)
            return ERROR_STRING(@"attempting to create 1D texture view with array layers");
        break;
    case WGPUTextureViewDimension_2D:
        if (m_dimension != WGPUTextureDimension_2D)
            return ERROR_STRING(@"attempting to create 2D texture view from non-2D base texture");

        if (descriptor.arrayLayerCount != 1)
            return ERROR_STRING(@"attempting to create 2D texture view with array layers");
        break;
    case WGPUTextureViewDimension_2DArray:
        if (m_dimension != WGPUTextureDimension_2D)
            return ERROR_STRING(@"attempting to create 2D texture array view from non-2D parent texture");
        break;
    case WGPUTextureViewDimension_Cube:
        if (m_dimension != WGPUTextureDimension_2D)
            return ERROR_STRING(@"attempting to create cube texture view from non-2D parent texture");

        if (descriptor.arrayLayerCount != 6)
            return ERROR_STRING(@"attempting to create cube texture view with arrayLayerCount != 6");

        if (m_width != m_height)
            return ERROR_STRING(@"attempting to create cube texture view from non-square parent texture");
        break;
    case WGPUTextureViewDimension_CubeArray:
        if (m_dimension != WGPUTextureDimension_2D)
            return ERROR_STRING(@"attempting to create cube array texture view from non-2D parent texture");

        if (descriptor.arrayLayerCount % 6)
            return ERROR_STRING(@"attempting to create cube array texture view with (arrayLayerCount % 6) != 0");

        if (m_width != m_height)
            return ERROR_STRING(@"attempting to create cube array texture view from non-square parent texture");
        break;
    case WGPUTextureViewDimension_3D:
        if (m_dimension != WGPUTextureDimension_3D)
            return ERROR_STRING(@"attempting to create 3D texture view from non-3D parent texture");

        if (descriptor.arrayLayerCount != 1)
            return ERROR_STRING(@"attempting to create 3D texture view with array layers");
        break;
    case WGPUTextureViewDimension_Force32:
        ASSERT_NOT_REACHED();
        return ERROR_STRING(@"descriptor.dimension is invalid value");
    }
#undef ERROR_STRING
    return nil;
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

static MTLPixelFormat resolvedPixelFormat(MTLPixelFormat viewPixelFormat, MTLPixelFormat sourcePixelFormat)
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

Ref<TextureView> Texture::createView(const WGPUTextureViewDescriptor& inputDescriptor)
{
    if (inputDescriptor.nextInChain || m_destroyed)
        return TextureView::createInvalid(*this, m_device);

    // https://gpuweb.github.io/gpuweb/#dom-gputexture-createview

    auto descriptor = resolveTextureViewDescriptorDefaults(inputDescriptor);
    if (!descriptor) {
        m_device->generateAValidationError("Validation failure in GPUTexture.createView: descriptor could not be resolved"_s);
        return TextureView::createInvalid(*this, m_device);
    }

    if (NSString *error = errorValidatingTextureViewCreation(*descriptor)) {
        m_device->generateAValidationError(error);
        return TextureView::createInvalid(*this, m_device);
    }

    auto pixelFormat = Texture::pixelFormat(descriptor->format);
    ASSERT(pixelFormat != MTLPixelFormatInvalid);

    MTLTextureType textureType;
    switch (descriptor->dimension) {
    case WGPUTextureViewDimension_Undefined:
        ASSERT_NOT_REACHED();
        return TextureView::createInvalid(*this, m_device);
    case WGPUTextureViewDimension_1D:
        if (descriptor->arrayLayerCount == 1)
            textureType = MTLTextureType1D;
        else
            textureType = MTLTextureType1DArray;
        break;
    case WGPUTextureViewDimension_2D:
        if (m_sampleCount > 1)
            textureType = MTLTextureType2DMultisample;
        else
            textureType = MTLTextureType2D;
        break;
    case WGPUTextureViewDimension_2DArray:
        if (m_sampleCount > 1) {
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
            return TextureView::createInvalid(*this, m_device);
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
        return TextureView::createInvalid(*this, m_device);
    }

    auto levels = NSMakeRange(descriptor->baseMipLevel, descriptor->mipLevelCount);

    auto slices = NSMakeRange(descriptor->baseArrayLayer, descriptor->arrayLayerCount);

    id<MTLTexture> texture = [m_texture newTextureViewWithPixelFormat:resolvedPixelFormat(pixelFormat, m_texture.pixelFormat) textureType:textureType levels:levels slices:slices];
    if (!texture)
        return TextureView::createInvalid(*this, m_device);

    texture.label = fromAPI(descriptor->label);
    if (!texture.label.length)
        texture.label = m_texture.label;

    std::optional<WGPUExtent3D> renderExtent;
    if (m_usage & WGPUTextureUsage_RenderAttachment)
        renderExtent = computeRenderExtent({ m_width, m_height, m_depthOrArrayLayers }, descriptor->baseMipLevel);

    auto result = TextureView::create(texture, *descriptor, renderExtent, *this, m_device);
    m_textureViews.append(result);
    return result;
}

void Texture::recreateIfNeeded()
{
    RELEASE_ASSERT(m_texture.iosurface && m_canvasBacking);
    m_destroyed = false;
}

void Texture::makeCanvasBacking()
{
    m_canvasBacking = true;
}

void Texture::destroy()
{
    // https://gpuweb.github.io/gpuweb/#dom-gputexture-destroy
    if (!m_canvasBacking)
        m_texture = nil;
    m_destroyed = true;
    for (auto& view : m_textureViews) {
        if (view.get())
            view->destroy();
    }

    m_textureViews.clear();
}

void Texture::setLabel(String&& label)
{
    m_texture.label = label;
}

WGPUExtent3D Texture::logicalMiplevelSpecificTextureExtent(uint32_t mipLevel)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-logical-miplevel-specific-texture-extent

    switch (m_dimension) {
    case WGPUTextureDimension_1D:
        return {
            std::max(static_cast<uint32_t>(1), m_width >> mipLevel),
            1,
            m_depthOrArrayLayers };
    case WGPUTextureDimension_2D:
        return {
            std::max(static_cast<uint32_t>(1), m_width >> mipLevel),
            std::max(static_cast<uint32_t>(1), m_height >> mipLevel),
            m_depthOrArrayLayers };
    case WGPUTextureDimension_3D:
        return {
            std::max(static_cast<uint32_t>(1), m_width >> mipLevel),
            std::max(static_cast<uint32_t>(1), m_height >> mipLevel),
            std::max(static_cast<uint32_t>(1), m_depthOrArrayLayers >> mipLevel) };
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return WGPUExtent3D { };
    }
}

WGPUExtent3D Texture::physicalMiplevelSpecificTextureExtent(uint32_t mipLevel)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-physical-miplevel-specific-texture-extent

    auto logicalExtent = logicalMiplevelSpecificTextureExtent(mipLevel);

    auto roundUpToMultipleOf = [](size_t a, size_t b) {
        return static_cast<uint32_t>(bmalloc::roundUpToMultipleOfNonPowerOfTwo(a, b));
    };

    switch (m_dimension) {
    case WGPUTextureDimension_1D:
        return {
            roundUpToMultipleOf(texelBlockWidth(m_format), logicalExtent.width),
            1,
            logicalExtent.depthOrArrayLayers };
    case WGPUTextureDimension_2D:
        return {
            roundUpToMultipleOf(texelBlockWidth(m_format), logicalExtent.width),
            roundUpToMultipleOf(texelBlockHeight(m_format), logicalExtent.height),
            logicalExtent.depthOrArrayLayers };
    case WGPUTextureDimension_3D:
        return {
            roundUpToMultipleOf(texelBlockWidth(m_format), logicalExtent.width),
            roundUpToMultipleOf(texelBlockHeight(m_format), logicalExtent.height),
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

    uint32_t blockWidth = Texture::texelBlockWidth(fromAPI(imageCopyTexture.texture).format());

    uint32_t blockHeight = Texture::texelBlockHeight(fromAPI(imageCopyTexture.texture).format());

    if (!fromAPI(imageCopyTexture.texture).isValid())
        return false;

    if (imageCopyTexture.mipLevel >= fromAPI(imageCopyTexture.texture).mipLevelCount())
        return false;

    if (imageCopyTexture.origin.x % blockWidth)
        return false;

    if (imageCopyTexture.origin.y % blockHeight)
        return false;

    UNUSED_PARAM(copySize);

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

bool Texture::isValidDepthStencilCopySource(WGPUTextureFormat format, WGPUTextureAspect aspect)
{
    // https://gpuweb.github.io/gpuweb/#depth-formats
    ASSERT(Texture::isDepthOrStencilFormat(format));

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
    case WGPUTextureFormat_RGB10A2Uint:
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

bool Texture::isValidDepthStencilCopyDestination(WGPUTextureFormat format, WGPUTextureAspect aspect)
{
    // https://gpuweb.github.io/gpuweb/#depth-formats
    ASSERT(Texture::isDepthOrStencilFormat(format));

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
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        return false;
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
        return true;
    case WGPUTextureFormat_Depth24Plus:
        return false;
    case WGPUTextureFormat_Depth24PlusStencil8:
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
        return false;
    case WGPUTextureFormat_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool Texture::validateTextureCopyRange(const WGPUImageCopyTexture& imageCopyTexture, const WGPUExtent3D& copySize)
{
    // https://gpuweb.github.io/gpuweb/#validating-texture-copy-range

    auto blockWidth = Texture::texelBlockWidth(fromAPI(imageCopyTexture.texture).format());

    auto blockHeight = Texture::texelBlockHeight(fromAPI(imageCopyTexture.texture).format());

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
    if (copyExtent.width % blockWidth)
        return false;

    auto heightInBlocks = copyExtent.height / blockHeight;
    if (copyExtent.height % blockHeight)
        return false;

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

    if (copyExtent.height > 1 && layout.rowsPerImage != WGPU_COPY_STRIDE_UNDEFINED) {
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

bool Texture::previouslyCleared(uint32_t mipLevel, uint32_t slice) const
{
    if (auto it = m_clearedToZero.find(mipLevel); it != m_clearedToZero.end())
        return it->value.contains(slice);

    return false;
}

void Texture::setPreviouslyCleared(uint32_t mipLevel, uint32_t slice)
{
    if (auto it = m_clearedToZero.find(mipLevel); it != m_clearedToZero.end()) {
        it->value.add(slice);
        return;
    }

    ClearedToZeroInnerContainer set;
    set.add(slice);
    m_clearedToZero.add(mipLevel, set);
}

bool Texture::isDestroyed() const
{
    return m_destroyed;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuTextureReference(WGPUTexture texture)
{
    WebGPU::fromAPI(texture).ref();
}

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

uint32_t wgpuTextureGetDepthOrArrayLayers(WGPUTexture texture)
{
    return WebGPU::fromAPI(texture).depthOrArrayLayers();
}

WGPUTextureDimension wgpuTextureGetDimension(WGPUTexture texture)
{
    return WebGPU::fromAPI(texture).dimension();
}

WGPUTextureFormat wgpuTextureGetFormat(WGPUTexture texture)
{
    return WebGPU::fromAPI(texture).format();
}

uint32_t wgpuTextureGetHeight(WGPUTexture texture)
{
    return WebGPU::fromAPI(texture).height();
}

uint32_t wgpuTextureGetWidth(WGPUTexture texture)
{
    return WebGPU::fromAPI(texture).width();
}

uint32_t wgpuTextureGetMipLevelCount(WGPUTexture texture)
{
    return WebGPU::fromAPI(texture).mipLevelCount();
}

uint32_t wgpuTextureGetSampleCount(WGPUTexture texture)
{
    return WebGPU::fromAPI(texture).sampleCount();
}

WGPUTextureUsageFlags wgpuTextureGetUsage(WGPUTexture texture)
{
    return WebGPU::fromAPI(texture).usage();
}
