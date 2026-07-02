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
 * \brief Fuzzy image comparison.
 *//*--------------------------------------------------------------------*/

#include "tcuFuzzyImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "deMath.h"
#include "deRandom.hpp"

#include <vector>

namespace tcu
{

enum
{
    MIN_ERR_THRESHOLD = 4 // Magic to make small differences go away
};

using std::vector;

template <int Channel>
static inline uint8_t getChannel(uint32_t color)
{
    return (uint8_t)((color >> (Channel * 8)) & 0xff);
}

static inline uint8_t getChannel(uint32_t color, int channel)
{
    return (uint8_t)((color >> (channel * 8)) & 0xff);
}

static inline uint32_t setChannel(uint32_t color, int channel, uint8_t val)
{
    return (color & ~(0xffu << (8 * channel))) | (val << (8 * channel));
}

static inline Vec4 toFloatVec(uint32_t color)
{
    return Vec4((float)getChannel<0>(color), (float)getChannel<1>(color), (float)getChannel<2>(color),
                (float)getChannel<3>(color));
}

static inline uint8_t roundToUint8Sat(float v)
{
    return (uint8_t)de::clamp((int)(v + 0.5f), 0, 255);
}

static inline uint32_t toColor(Vec4 v)
{
    return roundToUint8Sat(v[0]) | (roundToUint8Sat(v[1]) << 8) | (roundToUint8Sat(v[2]) << 16) |
           (roundToUint8Sat(v[3]) << 24);
}

template <int NumChannels>
static inline uint32_t readUnorm8(const tcu::ConstPixelBufferAccess &src, int x, int y)
{
    const uint8_t *ptr = (const uint8_t *)src.getDataPtr() + src.getRowPitch() * y + x * NumChannels;
    uint32_t v         = 0;

    for (int c = 0; c < NumChannels; c++)
        v |= ptr[c] << (c * 8);

    if (NumChannels < 4)
        v |= 0xffu << 24;

    return v;
}

#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
template <>
inline uint32_t readUnorm8<4>(const tcu::ConstPixelBufferAccess &src, int x, int y)
{
    return *(const uint32_t *)((const uint8_t *)src.getDataPtr() + src.getRowPitch() * y + x * 4);
}
#endif

template <int NumChannels>
static inline void writeUnorm8(const tcu::PixelBufferAccess &dst, int x, int y, uint32_t val)
{
    uint8_t *ptr = (uint8_t *)dst.getDataPtr() + dst.getRowPitch() * y + x * NumChannels;

    for (int c = 0; c < NumChannels; c++)
        ptr[c] = getChannel(val, c);
}

#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
template <>
inline void writeUnorm8<4>(const tcu::PixelBufferAccess &dst, int x, int y, uint32_t val)
{
    *(uint32_t *)((uint8_t *)dst.getDataPtr() + dst.getRowPitch() * y + x * 4) = val;
}
#endif

static inline uint32_t colorDistSquared(uint32_t pa, uint32_t pb)
{
    const int r = de::max<int>(de::abs((int)getChannel<0>(pa) - (int)getChannel<0>(pb)) - MIN_ERR_THRESHOLD, 0);
    const int g = de::max<int>(de::abs((int)getChannel<1>(pa) - (int)getChannel<1>(pb)) - MIN_ERR_THRESHOLD, 0);
    const int b = de::max<int>(de::abs((int)getChannel<2>(pa) - (int)getChannel<2>(pb)) - MIN_ERR_THRESHOLD, 0);
    const int a = de::max<int>(de::abs((int)getChannel<3>(pa) - (int)getChannel<3>(pb)) - MIN_ERR_THRESHOLD, 0);

    return uint32_t(r * r + g * g + b * b + a * a);
}

template <int NumChannels>
inline uint32_t bilinearSample(const ConstPixelBufferAccess &src, float u, float v)
{
    int w = src.getWidth();
    int h = src.getHeight();

    int x0 = deFloorFloatToInt32(u - 0.5f);
    int x1 = x0 + 1;
    int y0 = deFloorFloatToInt32(v - 0.5f);
    int y1 = y0 + 1;

    int i0 = de::clamp(x0, 0, w - 1);
    int i1 = de::clamp(x1, 0, w - 1);
    int j0 = de::clamp(y0, 0, h - 1);
    int j1 = de::clamp(y1, 0, h - 1);

    float a = deFloatFrac(u - 0.5f);
    float b = deFloatFrac(v - 0.5f);

    uint32_t p00 = readUnorm8<NumChannels>(src, i0, j0);
    uint32_t p10 = readUnorm8<NumChannels>(src, i1, j0);
    uint32_t p01 = readUnorm8<NumChannels>(src, i0, j1);
    uint32_t p11 = readUnorm8<NumChannels>(src, i1, j1);
    uint32_t dst = 0;

    // Interpolate.
    for (int c = 0; c < NumChannels; c++)
    {
        float f = (getChannel(p00, c) * (1.0f - a) * (1.0f - b)) + (getChannel(p10, c) * (a) * (1.0f - b)) +
                  (getChannel(p01, c) * (1.0f - a) * (b)) + (getChannel(p11, c) * (a) * (b));
        dst = setChannel(dst, c, roundToUint8Sat(f));
    }

    return dst;
}

template <int DstChannels, int SrcChannels>
static void separableConvolve(const PixelBufferAccess &dst, const ConstPixelBufferAccess &src, int shiftX, int shiftY,
                              const std::vector<float> &kernelX, const std::vector<float> &kernelY)
{
    DE_ASSERT(dst.getWidth() == src.getWidth() && dst.getHeight() == src.getHeight());

    TextureLevel tmp(dst.getFormat(), dst.getHeight(), dst.getWidth());
    PixelBufferAccess tmpAccess = tmp.getAccess();

    int kw = (int)kernelX.size();
    int kh = (int)kernelY.size();

    // Horizontal pass
    // \note Temporary surface is written in column-wise order
    for (int j = 0; j < src.getHeight(); j++)
    {
        for (int i = 0; i < src.getWidth(); i++)
        {
            Vec4 sum(0);

            for (int kx = 0; kx < kw; kx++)
            {
                float f    = kernelX[kw - kx - 1];
                uint32_t p = readUnorm8<SrcChannels>(src, de::clamp(i + kx - shiftX, 0, src.getWidth() - 1), j);

                sum += toFloatVec(p) * f;
            }

            writeUnorm8<DstChannels>(tmpAccess, j, i, toColor(sum));
        }
    }

    // Vertical pass
    for (int j = 0; j < src.getHeight(); j++)
    {
        for (int i = 0; i < src.getWidth(); i++)
        {
            Vec4 sum(0.0f);

            for (int ky = 0; ky < kh; ky++)
            {
                float f    = kernelY[kh - ky - 1];
                uint32_t p = readUnorm8<DstChannels>(tmpAccess, de::clamp(j + ky - shiftY, 0, tmp.getWidth() - 1), i);

                sum += toFloatVec(p) * f;
            }

            writeUnorm8<DstChannels>(dst, i, j, toColor(sum));
        }
    }
}

template <int NumChannels>
static uint32_t distSquaredToNeighbor(de::Random &rnd, uint32_t pixel, const ConstPixelBufferAccess &surface, int x,
                                      int y)
{
    // (x, y) + (0, 0)
    uint32_t minDist = colorDistSquared(pixel, readUnorm8<NumChannels>(surface, x, y));

    if (minDist == 0)
        return minDist;

    // Area around (x, y)
    static const int s_coords[8][2] = {{-1, -1}, {0, -1}, {+1, -1}, {-1, 0}, {+1, 0}, {-1, +1}, {0, +1}, {+1, +1}};

    for (int d = 0; d < (int)DE_LENGTH_OF_ARRAY(s_coords); d++)
    {
        int dx = x + s_coords[d][0];
        int dy = y + s_coords[d][1];

        if (!deInBounds32(dx, 0, surface.getWidth()) || !deInBounds32(dy, 0, surface.getHeight()))
            continue;

        minDist = de::min(minDist, colorDistSquared(pixel, readUnorm8<NumChannels>(surface, dx, dy)));
        if (minDist == 0)
            return minDist;
    }

    // Random bilinear-interpolated samples around (x, y)
    for (int s = 0; s < 32; s++)
    {
        float dx = (float)x + rnd.getFloat() * 2.0f - 0.5f;
        float dy = (float)y + rnd.getFloat() * 2.0f - 0.5f;

        uint32_t sample = bilinearSample<NumChannels>(surface, dx, dy);

        minDist = de::min(minDist, colorDistSquared(pixel, sample));
        if (minDist == 0)
            return minDist;
    }

    return minDist;
}

static inline float toGrayscale(const Vec4 &c)
{
    return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

static bool isFormatSupported(const TextureFormat &format)
{
    return format.type == TextureFormat::UNORM_INT8 &&
           (format.order == TextureFormat::RGB || format.order == TextureFormat::RGBA);
}

float fuzzyCompare(const FuzzyCompareParams &params, const ConstPixelBufferAccess &ref,
                   const ConstPixelBufferAccess &cmp, const PixelBufferAccess &errorMask)
{
    DE_ASSERT(ref.getWidth() == cmp.getWidth() && ref.getHeight() == cmp.getHeight());
    DE_ASSERT(errorMask.getWidth() == ref.getWidth() && errorMask.getHeight() == ref.getHeight());

    if (!isFormatSupported(ref.getFormat()) || !isFormatSupported(cmp.getFormat()))
        throw InternalError("Unsupported format in fuzzy comparison", nullptr, __FILE__, __LINE__);

    int width  = ref.getWidth();
    int height = ref.getHeight();
    de::Random rnd(667);

    // Filtered
    TextureLevel refFiltered(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), width, height);
    TextureLevel cmpFiltered(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), width, height);

    // Kernel = {0.1, 0.8, 0.1}
    vector<float> kernel(3);
    kernel[0] = kernel[2] = 0.1f;
    kernel[1]             = 0.8f;
    int shift             = (int)(kernel.size() - 1) / 2;

    switch (ref.getFormat().order)
    {
    case TextureFormat::RGBA:
        separableConvolve<4, 4>(refFiltered, ref, shift, shift, kernel, kernel);
        break;
    case TextureFormat::RGB:
        separableConvolve<4, 3>(refFiltered, ref, shift, shift, kernel, kernel);
        break;
    default:
        DE_ASSERT(false);
    }

    switch (cmp.getFormat().order)
    {
    case TextureFormat::RGBA:
        separableConvolve<4, 4>(cmpFiltered, cmp, shift, shift, kernel, kernel);
        break;
    case TextureFormat::RGB:
        separableConvolve<4, 3>(cmpFiltered, cmp, shift, shift, kernel, kernel);
        break;
    default:
        DE_ASSERT(false);
    }

    int numSamples    = 0;
    uint64_t distSum4 = 0ull;
    uint32_t distMax2 = 0u;

    // Clear error mask to green.
    clear(errorMask, Vec4(0.0f, 1.0f, 0.0f, 1.0f));

    ConstPixelBufferAccess refAccess = refFiltered.getAccess();
    ConstPixelBufferAccess cmpAccess = cmpFiltered.getAccess();

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x<width - 1; x += params.maxSampleSkip> 0 ? (int)rnd.getInt(1, params.maxSampleSkip) : 1)
        {
            const uint32_t minDist2RefToCmp =
                distSquaredToNeighbor<4>(rnd, readUnorm8<4>(refAccess, x, y), cmpAccess, x, y);
            const uint32_t minDist2CmpToRef =
                distSquaredToNeighbor<4>(rnd, readUnorm8<4>(cmpAccess, x, y), refAccess, x, y);
            const uint32_t minDist2 = de::min(minDist2RefToCmp, minDist2CmpToRef);
            const uint64_t newSum4  = distSum4 + minDist2 * minDist2;

            distSum4 = (newSum4 >= distSum4) ? newSum4 : ~0ull; // In case of overflow
            distMax2 = de::max(distMax2, minDist2);
            numSamples += 1;

            // Build error image.
            {
                const int scale  = 255 - MIN_ERR_THRESHOLD;
                const float err2 = float(minDist2) / float(scale * scale);
                const float err4 = err2 * err2;
                const float red  = err4 * 500.0f;
                const float luma = toGrayscale(cmp.getPixel(x, y));
                const float rF   = 0.7f + 0.3f * luma;

                errorMask.setPixel(Vec4(red * rF, (1.0f - red) * rF, 0.0f, 1.0f), x, y);
            }
        }
    }

    if (params.returnMaxError)
    {
        return sqrtf(float(distMax2)) / 255.0f;
    }
    else
    {
        // Scale error sum based on number of samples taken
        const double pSamples    = double((width - 2) * (height - 2)) / double(numSamples);
        const uint64_t colScale  = uint64_t(255 - MIN_ERR_THRESHOLD);
        const uint64_t colScale4 = colScale * colScale * colScale * colScale;

        return float(double(distSum4) / double(colScale4) * pSamples);
    }
}

} // namespace tcu
