/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebGPUXRSubImageImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUConvertToBackingContext.h"
#include "WebGPUDevice.h"
#include "WebGPUTextureDimension.h"
#include "WebGPUTextureImpl.h"

namespace WebCore::WebGPU {

static TextureFormat fromBacking(WGPUTextureFormat textureFormat)
{
    switch (textureFormat) {
    case WGPUTextureFormat_R8Unorm:
        return TextureFormat::R8unorm;
    case WGPUTextureFormat_R8Snorm:
        return TextureFormat::R8snorm;
    case WGPUTextureFormat_R8Uint:
        return TextureFormat::R8uint;
    case WGPUTextureFormat_R8Sint:
        return TextureFormat::R8sint;
    case WGPUTextureFormat_R16Uint:
        return TextureFormat::R16uint;
    case WGPUTextureFormat_R16Sint:
        return TextureFormat::R16sint;
    case WGPUTextureFormat_R16Float:
        return TextureFormat::R16float;
    case WGPUTextureFormat_RG8Unorm:
        return TextureFormat::Rg8unorm;
    case WGPUTextureFormat_RG8Snorm:
        return TextureFormat::Rg8snorm;
    case WGPUTextureFormat_RG8Uint:
        return TextureFormat::Rg8uint;
    case WGPUTextureFormat_RG8Sint:
        return TextureFormat::Rg8sint;
    case WGPUTextureFormat_R32Uint:
        return TextureFormat::R32uint;
    case WGPUTextureFormat_R32Sint:
        return TextureFormat::R32sint;
    case WGPUTextureFormat_R32Float:
        return TextureFormat::R32float;
    case WGPUTextureFormat_RG16Uint:
        return TextureFormat::Rg16uint;
    case WGPUTextureFormat_RG16Sint:
        return TextureFormat::Rg16sint;
    case WGPUTextureFormat_RG16Float:
        return TextureFormat::Rg16float;
    case WGPUTextureFormat_RGBA8Unorm:
        return TextureFormat::Rgba8unorm;
    case WGPUTextureFormat_RGBA8UnormSrgb:
        return TextureFormat::Rgba8unormSRGB;
    case WGPUTextureFormat_RGBA8Snorm:
        return TextureFormat::Rgba8snorm;
    case WGPUTextureFormat_RGBA8Uint:
        return TextureFormat::Rgba8uint;
    case WGPUTextureFormat_RGBA8Sint:
        return TextureFormat::Rgba8sint;
    case WGPUTextureFormat_BGRA8Unorm:
        return TextureFormat::Bgra8unorm;
    case WGPUTextureFormat_BGRA8UnormSrgb:
        return TextureFormat::Bgra8unormSRGB;
    case WGPUTextureFormat_RGB9E5Ufloat:
        return TextureFormat::Rgb9e5ufloat;
    case WGPUTextureFormat_RGB10A2Uint:
        return TextureFormat::Rgb10a2uint;
    case WGPUTextureFormat_RGB10A2Unorm:
        return TextureFormat::Rgb10a2unorm;
    case WGPUTextureFormat_RG11B10Ufloat:
        return TextureFormat::Rg11b10ufloat;
    case WGPUTextureFormat_RG32Uint:
        return TextureFormat::Rg32uint;
    case WGPUTextureFormat_RG32Sint:
        return TextureFormat::Rg32sint;
    case WGPUTextureFormat_RG32Float:
        return TextureFormat::Rg32float;
    case WGPUTextureFormat_RGBA16Uint:
        return TextureFormat::Rgba16uint;
    case WGPUTextureFormat_RGBA16Sint:
        return TextureFormat::Rgba16sint;
    case WGPUTextureFormat_RGBA16Float:
        return TextureFormat::Rgba16float;
    case WGPUTextureFormat_RGBA32Uint:
        return TextureFormat::Rgba32uint;
    case WGPUTextureFormat_RGBA32Sint:
        return TextureFormat::Rgba32sint;
    case WGPUTextureFormat_RGBA32Float:
        return TextureFormat::Rgba32float;
    case WGPUTextureFormat_Stencil8:
        return TextureFormat::Stencil8;
    case WGPUTextureFormat_Depth16Unorm:
        return TextureFormat::Depth16unorm;
    case WGPUTextureFormat_Depth24Plus:
        return TextureFormat::Depth24plus;
    case WGPUTextureFormat_Depth24PlusStencil8:
        return TextureFormat::Depth24plusStencil8;
    case WGPUTextureFormat_Depth32Float:
        return TextureFormat::Depth32float;
    case WGPUTextureFormat_BC1RGBAUnorm:
        return TextureFormat::Bc1RgbaUnorm;
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
        return TextureFormat::Bc1RgbaUnormSRGB;
    case WGPUTextureFormat_BC2RGBAUnorm:
        return TextureFormat::Bc2RgbaUnorm;
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
        return TextureFormat::Bc2RgbaUnormSRGB;
    case WGPUTextureFormat_BC3RGBAUnorm:
        return TextureFormat::Bc3RgbaUnorm;
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
        return TextureFormat::Bc3RgbaUnormSRGB;
    case WGPUTextureFormat_BC4RUnorm:
        return TextureFormat::Bc4RUnorm;
    case WGPUTextureFormat_BC4RSnorm:
        return TextureFormat::Bc4RSnorm;
    case WGPUTextureFormat_BC5RGUnorm:
        return TextureFormat::Bc5RgUnorm;
    case WGPUTextureFormat_BC5RGSnorm:
        return TextureFormat::Bc5RgSnorm;
    case WGPUTextureFormat_BC6HRGBUfloat:
        return TextureFormat::Bc6hRgbUfloat;
    case WGPUTextureFormat_BC6HRGBFloat:
        return TextureFormat::Bc6hRgbFloat;
    case WGPUTextureFormat_BC7RGBAUnorm:
        return TextureFormat::Bc7RgbaUnorm;
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return TextureFormat::Bc7RgbaUnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ETC2RGB8Unorm):
        return TextureFormat::Etc2Rgb8unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ETC2RGB8UnormSrgb):
        return TextureFormat::Etc2Rgb8unormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ETC2RGB8A1Unorm):
        return TextureFormat::Etc2Rgb8a1unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ETC2RGB8A1UnormSrgb):
        return TextureFormat::Etc2Rgb8a1unormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ETC2RGBA8Unorm):
        return TextureFormat::Etc2Rgba8unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ETC2RGBA8UnormSrgb):
        return TextureFormat::Etc2Rgba8unormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_EACR11Unorm):
        return TextureFormat::EacR11unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_EACR11Snorm):
        return TextureFormat::EacR11snorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_EACRG11Unorm):
        return TextureFormat::EacRg11unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_EACRG11Snorm):
        return TextureFormat::EacRg11snorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC4x4Unorm):
        return TextureFormat::Astc4x4Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC4x4UnormSrgb):
        return TextureFormat::Astc4x4UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC5x4Unorm):
        return TextureFormat::Astc5x4Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC5x4UnormSrgb):
        return TextureFormat::Astc5x4UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC5x5Unorm):
        return TextureFormat::Astc5x5Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC5x5UnormSrgb):
        return TextureFormat::Astc5x5UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC6x5Unorm):
        return TextureFormat::Astc6x5Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC6x5UnormSrgb):
        return TextureFormat::Astc6x5UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC6x6Unorm):
        return TextureFormat::Astc6x6Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC6x6UnormSrgb):
        return TextureFormat::Astc6x6UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC8x5Unorm):
        return TextureFormat::Astc8x5Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC8x5UnormSrgb):
        return TextureFormat::Astc8x5UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC8x6Unorm):
        return TextureFormat::Astc8x6Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC8x6UnormSrgb):
        return TextureFormat::Astc8x6UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC8x8Unorm):
        return TextureFormat::Astc8x8Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC8x8UnormSrgb):
        return TextureFormat::Astc8x8UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC10x5Unorm):
        return TextureFormat::Astc10x5Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC10x5UnormSrgb):
        return TextureFormat::Astc10x5UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC10x6Unorm):
        return TextureFormat::Astc10x6Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC10x6UnormSrgb):
        return TextureFormat::Astc10x6UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC10x8Unorm):
        return TextureFormat::Astc10x8Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC10x8UnormSrgb):
        return TextureFormat::Astc10x8UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC10x10Unorm):
        return TextureFormat::Astc10x10Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC10x10UnormSrgb):
        return TextureFormat::Astc10x10UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC12x10Unorm):
        return TextureFormat::Astc12x10Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC12x10UnormSrgb):
        return TextureFormat::Astc12x10UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC12x12Unorm):
        return TextureFormat::Astc12x12Unorm;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_ASTC12x12UnormSrgb):
        return TextureFormat::Astc12x12UnormSRGB;
    case static_cast<WGPUTextureFormat>(WGPUTextureFormat_Depth32FloatStencil8):
        return TextureFormat::Depth32floatStencil8;
    case WGPUTextureFormat_R16Unorm:
        return TextureFormat::R16unorm;
    case WGPUTextureFormat_R16Snorm:
        return TextureFormat::R16snorm;
    case WGPUTextureFormat_RG16Unorm:
        return TextureFormat::Rg16unorm;
    case WGPUTextureFormat_RG16Snorm:
        return TextureFormat::Rg16snorm;
    case WGPUTextureFormat_RGBA16Unorm:
        return TextureFormat::Rgba16snorm;
    case WGPUTextureFormat_RGBA16Snorm:
        return TextureFormat::Rgba16snorm;
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_Force32:
        return TextureFormat::R8unorm;
    }
}

XRSubImageImpl::XRSubImageImpl(WebGPUPtr<WGPUXRSubImage>&& backing, ConvertToBackingContext& convertToBackingContext)
    : m_backing(backing)
    , m_convertToBackingContext(convertToBackingContext)
{
}

XRSubImageImpl::~XRSubImageImpl() = default;

RefPtr<Texture> XRSubImageImpl::colorTexture()
{
    auto texturePtr = wgpuXRSubImageGetColorTexture(m_backing.get());
    if (!texturePtr)
        return nullptr;

    return TextureImpl::create(WebGPUPtr<WGPUTexture> { texturePtr }, fromBacking(wgpuTextureGetFormat(texturePtr)), TextureDimension::_2d, m_convertToBackingContext);
}

RefPtr<Texture> XRSubImageImpl::depthStencilTexture()
{
    auto texturePtr = wgpuXRSubImageGetDepthStencilTexture(m_backing.get());
    if (!texturePtr)
        return nullptr;

    return TextureImpl::create(WebGPUPtr<WGPUTexture> { texturePtr }, fromBacking(wgpuTextureGetFormat(texturePtr)), TextureDimension::_2d, m_convertToBackingContext);
}

RefPtr<Texture> XRSubImageImpl::motionVectorTexture()
{
    return nullptr;
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
