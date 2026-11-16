#ifndef _TCUTEXTUREUTIL_HPP
#define _TCUTEXTUREUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Texture utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTexture.hpp"

namespace tcu
{

// PixelBufferAccess utilities.
PixelBufferAccess getSubregion(const PixelBufferAccess &access, int x, int y, int z, int width, int height, int depth);
ConstPixelBufferAccess getSubregion(const ConstPixelBufferAccess &access, int x, int y, int z, int width, int height,
                                    int depth);

PixelBufferAccess getSubregion(const PixelBufferAccess &access, int x, int y, int width, int height);
ConstPixelBufferAccess getSubregion(const ConstPixelBufferAccess &access, int x, int y, int width, int height);

PixelBufferAccess flipYAccess(const PixelBufferAccess &access);
ConstPixelBufferAccess flipYAccess(const ConstPixelBufferAccess &access);

bool isCombinedDepthStencilType(TextureFormat::ChannelType type);
bool hasStencilComponent(TextureFormat::ChannelOrder order);
bool hasDepthComponent(TextureFormat::ChannelOrder order);

// sRGB - linear conversion.
float linearChannelToSRGB(float cl);
float sRGBChannelToLinear(float cl);
Vec4 sRGBToLinear(const Vec4 &cs);
Vec4 sRGB8ToLinear(const UVec4 &cs);
Vec4 sRGBA8ToLinear(const UVec4 &cs);
Vec4 linearToSRGB(const Vec4 &cl);
bool isSRGB(TextureFormat format);

/*--------------------------------------------------------------------*//*!
 * \brief Color channel storage type
 *//*--------------------------------------------------------------------*/
enum TextureChannelClass
{
    TEXTURECHANNELCLASS_SIGNED_FIXED_POINT = 0,
    TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT,
    TEXTURECHANNELCLASS_SIGNED_INTEGER,
    TEXTURECHANNELCLASS_UNSIGNED_INTEGER,
    TEXTURECHANNELCLASS_FLOATING_POINT,

    TEXTURECHANNELCLASS_LAST
};

TextureChannelClass getTextureChannelClass(TextureFormat::ChannelType channelType);

/*--------------------------------------------------------------------*//*!
 * \brief Texture access type
 *//*--------------------------------------------------------------------*/
enum TextureAccessType
{
    TEXTUREACCESSTYPE_FLOAT = 0,    //!< Read (getPixel) or write as floating-point data
    TEXTUREACCESSTYPE_SIGNED_INT,   //!< Read (getPixelInt) or write as signed integer data
    TEXTUREACCESSTYPE_UNSIGNED_INT, //!< Read (getPixelUint) or write as unsigned integer data

    TEXTUREACCESSTYPE_LAST
};

bool isAccessValid(TextureFormat format, TextureAccessType type);

/*--------------------------------------------------------------------*//*!
 * \brief Standard parameters for texture format testing
 *//*--------------------------------------------------------------------*/
struct TextureFormatInfo
{
    Vec4 valueMin;
    Vec4 valueMax;
    Vec4 lookupScale;
    Vec4 lookupBias;

    TextureFormatInfo(const Vec4 &valueMin_, const Vec4 &valueMax_, const Vec4 &lookupScale_, const Vec4 &lookupBias_)
        : valueMin(valueMin_)
        , valueMax(valueMax_)
        , lookupScale(lookupScale_)
        , lookupBias(lookupBias_)
    {
    }
} DE_WARN_UNUSED_TYPE;

TextureFormatInfo getTextureFormatInfo(const TextureFormat &format);
IVec4 getTextureFormatBitDepth(const TextureFormat &format);
IVec4 getTextureFormatMantissaBitDepth(const TextureFormat &format);
BVec4 getTextureFormatChannelMask(const TextureFormat &format);

IVec4 getFormatMinIntValue(const TextureFormat &format);
IVec4 getFormatMaxIntValue(const TextureFormat &format);

UVec4 getFormatMaxUintValue(const TextureFormat &format);

// Texture fill.
void clear(const PixelBufferAccess &access, const Vec4 &color);
void clear(const PixelBufferAccess &access, const IVec4 &color);
void clear(const PixelBufferAccess &access, const UVec4 &color);
void clearDepth(const PixelBufferAccess &access, float depth);
void clearStencil(const PixelBufferAccess &access, int stencil);
void fillWithComponentGradients(const PixelBufferAccess &access, const Vec4 &minVal, const Vec4 &maxVal);
void fillWithComponentGradients2(const PixelBufferAccess &access, const Vec4 &minVal, const Vec4 &maxVal);
void fillWithComponentGradients3(const PixelBufferAccess &access, const Vec4 &minVal, const Vec4 &maxVal);
void fillWithGrid(const PixelBufferAccess &access, int cellSize, const Vec4 &colorA, const Vec4 &colorB);
void fillWithRepeatableGradient(const PixelBufferAccess &access, const Vec4 &colorA, const Vec4 &colorB);
void fillWithMetaballs(const PixelBufferAccess &access, int numMetaballs, uint32_t seed);
void fillWithRGBAQuads(const PixelBufferAccess &access);

//! Copies contents of src to dst. If formats of dst and src are equal, a bit-exact copy is made.
void copy(const PixelBufferAccess &dst, const ConstPixelBufferAccess &src, const bool clearUnused = true);

void scale(const PixelBufferAccess &dst, const ConstPixelBufferAccess &src, Sampler::FilterMode filter);

void estimatePixelValueRange(const ConstPixelBufferAccess &access, Vec4 &minVal, Vec4 &maxVal);
void computePixelScaleBias(const ConstPixelBufferAccess &access, Vec4 &scale, Vec4 &bias);

int getCubeArrayFaceIndex(CubeFace face);

//! FP32->U8 with RTE rounding (extremely fast, always accurate).
inline uint8_t floatToU8(float fv)
{
    union
    {
        float fv;
        uint32_t uv;
        int32_t iv;
    } v;
    v.fv = fv;

    const uint32_t e = (uint32_t)(126 - (v.iv >> 23));
    uint32_t m       = v.uv;

    m &= 0x00ffffffu;
    m |= 0x00800000u;
    m = (m << 8) - m;
    m = (e > 8) ? e : (0x00800000u + (m >> e));

    return (uint8_t)(m >> 24);
}

uint32_t packRGB999E5(const tcu::Vec4 &color);

/*--------------------------------------------------------------------*//*!
 * \brief Depth-stencil utilities
 *//*--------------------------------------------------------------------*/

TextureFormat getEffectiveDepthStencilTextureFormat(const TextureFormat &baseFormat, Sampler::DepthStencilMode mode);

//! returns the currently effective access to an access with a given sampler mode, e.g.
//! for combined depth stencil accesses and for sampler set to sample stencil returns
//! stencil access. Identity for non-combined formats.
PixelBufferAccess getEffectiveDepthStencilAccess(const PixelBufferAccess &baseAccess, Sampler::DepthStencilMode mode);
ConstPixelBufferAccess getEffectiveDepthStencilAccess(const ConstPixelBufferAccess &baseAccess,
                                                      Sampler::DepthStencilMode mode);

//! returns the currently effective view to an texture with a given sampler mode. Uses
//! storage for access storage storage

tcu::Texture1DView getEffectiveTextureView(const tcu::Texture1DView &src,
                                           std::vector<tcu::ConstPixelBufferAccess> &storage,
                                           const tcu::Sampler &sampler);
tcu::Texture2DView getEffectiveTextureView(const tcu::Texture2DView &src,
                                           std::vector<tcu::ConstPixelBufferAccess> &storage,
                                           const tcu::Sampler &sampler);
tcu::Texture3DView getEffectiveTextureView(const tcu::Texture3DView &src,
                                           std::vector<tcu::ConstPixelBufferAccess> &storage,
                                           const tcu::Sampler &sampler);
tcu::Texture1DArrayView getEffectiveTextureView(const tcu::Texture1DArrayView &src,
                                                std::vector<tcu::ConstPixelBufferAccess> &storage,
                                                const tcu::Sampler &sampler);
tcu::Texture2DArrayView getEffectiveTextureView(const tcu::Texture2DArrayView &src,
                                                std::vector<tcu::ConstPixelBufferAccess> &storage,
                                                const tcu::Sampler &sampler);
tcu::TextureCubeView getEffectiveTextureView(const tcu::TextureCubeView &src,
                                             std::vector<tcu::ConstPixelBufferAccess> &storage,
                                             const tcu::Sampler &sampler);
tcu::TextureCubeArrayView getEffectiveTextureView(const tcu::TextureCubeArrayView &src,
                                                  std::vector<tcu::ConstPixelBufferAccess> &storage,
                                                  const tcu::Sampler &sampler);

template <typename ScalarType>
tcu::Vector<ScalarType, 4> sampleTextureBorder(const TextureFormat &format, const Sampler &sampler);

} // namespace tcu

#endif // _TCUTEXTUREUTIL_HPP
