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
 * \brief Bilinear image comparison.
 *//*--------------------------------------------------------------------*/

#include "tcuBilinearImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

namespace tcu
{

namespace
{

enum
{
    NUM_SUBPIXEL_BITS = 8 //!< Number of subpixel bits used when doing bilinear interpolation.
};

// \note Algorithm assumes that colors are packed to 32-bit values as dictated by
//         tcu::RGBA::*_SHIFT values.

template <int Channel>
static inline uint8_t getChannel(uint32_t color)
{
    return (uint8_t)((color >> (Channel * 8)) & 0xff);
}

#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
inline uint32_t readRGBA8Raw(const ConstPixelBufferAccess &src, uint32_t x, uint32_t y)
{
    return *(const uint32_t *)((const uint8_t *)src.getDataPtr() + y * src.getRowPitch() + x * 4);
}
#else
inline uint32_t readRGBA8Raw(const ConstPixelBufferAccess &src, uint32_t x, uint32_t y)
{
    return deReverseBytes32(*(const uint32_t *)((const uint8_t *)src.getDataPtr() + y * src.getRowPitch() + x * 4));
}
#endif

inline RGBA readRGBA8(const ConstPixelBufferAccess &src, uint32_t x, uint32_t y)
{
    uint32_t raw = readRGBA8Raw(src, x, y);
    uint32_t res = 0;

    res |= getChannel<0>(raw) << RGBA::RED_SHIFT;
    res |= getChannel<1>(raw) << RGBA::GREEN_SHIFT;
    res |= getChannel<2>(raw) << RGBA::BLUE_SHIFT;
    res |= getChannel<3>(raw) << RGBA::ALPHA_SHIFT;

    return RGBA(res);
}

inline uint8_t interpolateChannel(uint32_t fx1, uint32_t fy1, uint8_t p00, uint8_t p01, uint8_t p10, uint8_t p11)
{
    const uint32_t fx0     = (1u << NUM_SUBPIXEL_BITS) - fx1;
    const uint32_t fy0     = (1u << NUM_SUBPIXEL_BITS) - fy1;
    const uint32_t half    = 1u << (NUM_SUBPIXEL_BITS * 2 - 1);
    const uint32_t sum     = fx0 * fy0 * p00 + fx1 * fy0 * p10 + fx0 * fy1 * p01 + fx1 * fy1 * p11;
    const uint32_t rounded = (sum + half) >> (NUM_SUBPIXEL_BITS * 2);

    DE_ASSERT(de::inRange<uint32_t>(rounded, 0, 0xff));
    return (uint8_t)rounded;
}

RGBA bilinearSampleRGBA8(const ConstPixelBufferAccess &access, uint32_t u, uint32_t v)
{
    uint32_t x0 = u >> NUM_SUBPIXEL_BITS;
    uint32_t y0 = v >> NUM_SUBPIXEL_BITS;
    uint32_t x1 = x0 + 1; //de::min(x0+1, (uint32_t)(access.getWidth()-1));
    uint32_t y1 = y0 + 1; //de::min(y0+1, (uint32_t)(access.getHeight()-1));

    DE_ASSERT(x1 < (uint32_t)access.getWidth());
    DE_ASSERT(y1 < (uint32_t)access.getHeight());

    uint32_t fx1 = u - (x0 << NUM_SUBPIXEL_BITS);
    uint32_t fy1 = v - (y0 << NUM_SUBPIXEL_BITS);

    uint32_t p00 = readRGBA8Raw(access, x0, y0);
    uint32_t p10 = readRGBA8Raw(access, x1, y0);
    uint32_t p01 = readRGBA8Raw(access, x0, y1);
    uint32_t p11 = readRGBA8Raw(access, x1, y1);

    uint32_t res = 0;

    res |= interpolateChannel(fx1, fy1, getChannel<0>(p00), getChannel<0>(p01), getChannel<0>(p10), getChannel<0>(p11))
           << RGBA::RED_SHIFT;
    res |= interpolateChannel(fx1, fy1, getChannel<1>(p00), getChannel<1>(p01), getChannel<1>(p10), getChannel<1>(p11))
           << RGBA::GREEN_SHIFT;
    res |= interpolateChannel(fx1, fy1, getChannel<2>(p00), getChannel<2>(p01), getChannel<2>(p10), getChannel<2>(p11))
           << RGBA::BLUE_SHIFT;
    res |= interpolateChannel(fx1, fy1, getChannel<3>(p00), getChannel<3>(p01), getChannel<3>(p10), getChannel<3>(p11))
           << RGBA::ALPHA_SHIFT;

    return RGBA(res);
}

bool comparePixelRGBA8(const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                       const RGBA threshold, int x, int y)
{
    const RGBA resPix = readRGBA8(result, (uint32_t)x, (uint32_t)y);

    // Step 1: Compare result pixel to 3x3 neighborhood pixels in reference.
    {
        const uint32_t x0 = (uint32_t)de::max(x - 1, 0);
        const uint32_t x1 = (uint32_t)x;
        const uint32_t x2 = (uint32_t)de::min(x + 1, reference.getWidth() - 1);
        const uint32_t y0 = (uint32_t)de::max(y - 1, 0);
        const uint32_t y1 = (uint32_t)y;
        const uint32_t y2 = (uint32_t)de::min(y + 1, reference.getHeight() - 1);

        if (compareThreshold(resPix, readRGBA8(reference, x1, y1), threshold) ||
            compareThreshold(resPix, readRGBA8(reference, x0, y1), threshold) ||
            compareThreshold(resPix, readRGBA8(reference, x2, y1), threshold) ||
            compareThreshold(resPix, readRGBA8(reference, x0, y0), threshold) ||
            compareThreshold(resPix, readRGBA8(reference, x1, y0), threshold) ||
            compareThreshold(resPix, readRGBA8(reference, x2, y0), threshold) ||
            compareThreshold(resPix, readRGBA8(reference, x0, y2), threshold) ||
            compareThreshold(resPix, readRGBA8(reference, x1, y2), threshold) ||
            compareThreshold(resPix, readRGBA8(reference, x2, y2), threshold))
            return true;
    }

    // Step 2: Compare using bilinear sampling.
    {
        // \todo [pyry] Optimize sample positions!
        static const uint32_t s_offsets[][2] = {{226, 186}, {335, 235}, {279, 334}, {178, 272}, {112, 202}, {306, 117},
                                                {396, 299}, {206, 382}, {146, 96},  {423, 155}, {361, 412}, {84, 339},
                                                {48, 130},  {367, 43},  {455, 367}, {105, 439}, {83, 46},   {217, 24},
                                                {461, 71},  {450, 459}, {239, 469}, {67, 267},  {459, 255}, {13, 416},
                                                {10, 192},  {141, 502}, {503, 304}, {380, 506}};

        for (int sampleNdx = 0; sampleNdx < DE_LENGTH_OF_ARRAY(s_offsets); sampleNdx++)
        {
            const int u = (x << NUM_SUBPIXEL_BITS) + (int)s_offsets[sampleNdx][0] - (1 << NUM_SUBPIXEL_BITS);
            const int v = (y << NUM_SUBPIXEL_BITS) + (int)s_offsets[sampleNdx][1] - (1 << NUM_SUBPIXEL_BITS);

            if (!de::inBounds(u, 0, (reference.getWidth() - 1) << NUM_SUBPIXEL_BITS) ||
                !de::inBounds(v, 0, (reference.getHeight() - 1) << NUM_SUBPIXEL_BITS))
                continue;

            if (compareThreshold(resPix, bilinearSampleRGBA8(reference, (uint32_t)u, (uint32_t)v), threshold))
                return true;
        }
    }

    return false;
}

bool bilinearCompareRGBA8(const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                          const PixelBufferAccess &errorMask, const RGBA threshold)
{
    DE_ASSERT(reference.getFormat() == TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8) &&
              result.getFormat() == TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8));

    // Clear error mask first to green (faster this way).
    clear(errorMask, Vec4(0.0f, 1.0f, 0.0f, 1.0f));

    bool allOk = true;

    for (int y = 0; y < reference.getHeight(); y++)
    {
        for (int x = 0; x < reference.getWidth(); x++)
        {
            if (!comparePixelRGBA8(reference, result, threshold, x, y) &&
                !comparePixelRGBA8(result, reference, threshold, x, y))
            {
                allOk = false;
                errorMask.setPixel(Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y);
            }
        }
    }

    return allOk;
}

} // namespace

bool bilinearCompare(const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                     const PixelBufferAccess &errorMask, const RGBA threshold)
{
    DE_ASSERT(reference.getWidth() == result.getWidth() && reference.getHeight() == result.getHeight() &&
              reference.getDepth() == result.getDepth() && reference.getFormat() == result.getFormat());
    DE_ASSERT(reference.getWidth() == errorMask.getWidth() && reference.getHeight() == errorMask.getHeight() &&
              reference.getDepth() == errorMask.getDepth());

    if (reference.getFormat() == TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
        return bilinearCompareRGBA8(reference, result, errorMask, threshold);
    else
        throw InternalError("Unsupported format for bilinear comparison");
}

} // namespace tcu
