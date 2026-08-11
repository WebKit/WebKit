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
 * \brief Texture compare (shadow) result verifier.
 *//*--------------------------------------------------------------------*/

#include "tcuTexCompareVerifier.hpp"
#include "tcuTexVerifierUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "deMath.h"

namespace tcu
{

using namespace TexVerifierUtil;

// Generic utilities

#if defined(DE_DEBUG)
static bool isSamplerSupported(const Sampler &sampler)
{
    return sampler.compare != Sampler::COMPAREMODE_NONE && isWrapModeSupported(sampler.wrapS) &&
           isWrapModeSupported(sampler.wrapT) && isWrapModeSupported(sampler.wrapR);
}
#endif // DE_DEBUG

struct CmpResultSet
{
    bool isTrue;
    bool isFalse;

    CmpResultSet(void) : isTrue(false), isFalse(false)
    {
    }
};

static CmpResultSet execCompare(const Sampler::CompareMode compareMode, const float cmpValue_,
                                const float cmpReference_, const int referenceBits, const bool isFixedPoint)
{
    const bool clampValues =
        isFixedPoint; // if comparing against a floating point texture, ref (and value) is not clamped
    const float cmpValue     = (clampValues) ? (de::clamp(cmpValue_, 0.0f, 1.0f)) : (cmpValue_);
    const float cmpReference = (clampValues) ? (de::clamp(cmpReference_, 0.0f, 1.0f)) : (cmpReference_);
    const float err          = computeFixedPointError(referenceBits);
    CmpResultSet res;

    switch (compareMode)
    {
    case Sampler::COMPAREMODE_LESS:
        res.isTrue  = cmpReference - err < cmpValue;
        res.isFalse = cmpReference + err >= cmpValue;
        break;

    case Sampler::COMPAREMODE_LESS_OR_EQUAL:
        res.isTrue  = cmpReference - err <= cmpValue;
        res.isFalse = cmpReference + err > cmpValue;
        break;

    case Sampler::COMPAREMODE_GREATER:
        res.isTrue  = cmpReference + err > cmpValue;
        res.isFalse = cmpReference - err <= cmpValue;
        break;

    case Sampler::COMPAREMODE_GREATER_OR_EQUAL:
        res.isTrue  = cmpReference + err >= cmpValue;
        res.isFalse = cmpReference - err < cmpValue;
        break;

    case Sampler::COMPAREMODE_EQUAL:
        res.isTrue  = de::inRange(cmpValue, cmpReference - err, cmpReference + err);
        res.isFalse = err != 0.0f || cmpValue != cmpReference;
        break;

    case Sampler::COMPAREMODE_NOT_EQUAL:
        res.isTrue  = err != 0.0f || cmpValue != cmpReference;
        res.isFalse = de::inRange(cmpValue, cmpReference - err, cmpReference + err);
        break;

    case Sampler::COMPAREMODE_ALWAYS:
        res.isTrue = true;
        break;

    case Sampler::COMPAREMODE_NEVER:
        res.isFalse = true;
        break;

    default:
        DE_ASSERT(false);
    }

    DE_ASSERT(res.isTrue || res.isFalse);
    return res;
}

static inline bool isResultInSet(const CmpResultSet resultSet, const float result, const int resultBits)
{
    const float err  = computeFixedPointError(resultBits);
    const float minR = result - err;
    const float maxR = result + err;

    return (resultSet.isTrue && de::inRange(1.0f, minR, maxR)) || (resultSet.isFalse && de::inRange(0.0f, minR, maxR));
}

static inline bool coordsInBounds(const ConstPixelBufferAccess &access, int x, int y, int z)
{
    return de::inBounds(x, 0, access.getWidth()) && de::inBounds(y, 0, access.getHeight()) &&
           de::inBounds(z, 0, access.getDepth());
}

// lookup depth value at a point that is guaranteed to not sample border such as cube map faces.
static float lookupDepthNoBorder(const tcu::ConstPixelBufferAccess &access, const Sampler &sampler, int i, int j,
                                 int k = 0)
{
    DE_UNREF(sampler);
    DE_ASSERT(coordsInBounds(access, i, j, k));
    DE_ASSERT(access.getFormat().order == TextureFormat::D || access.getFormat().order == TextureFormat::DS ||
              access.getFormat().order == TextureFormat::R);

    if (access.getFormat().order == TextureFormat::R)
        return access.getPixel(i, j, k).x();
    else
        return access.getPixDepth(i, j, k);
}

static float lookupDepth(const tcu::ConstPixelBufferAccess &access, const Sampler &sampler, int i, int j, int k)
{
    if (coordsInBounds(access, i, j, k))
        return lookupDepthNoBorder(access, sampler, i, j, k);
    else
        return sampleTextureBorder<float>(access.getFormat(), sampler).x();
}

// Values are in order (0,0), (1,0), (0,1), (1,1)
static float bilinearInterpolate(const Vec4 &values, const float x, const float y)
{
    const float v00 = values[0];
    const float v10 = values[1];
    const float v01 = values[2];
    const float v11 = values[3];
    const float res = v00 * (1.0f - x) * (1.0f - y) + v10 * x * (1.0f - y) + v01 * (1.0f - x) * y + v11 * x * y;
    return res;
}

static bool isFixedPointDepthTextureFormat(const tcu::TextureFormat &format)
{
    const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(format.type);

    if (format.order == TextureFormat::D || format.order == TextureFormat::R)
    {
        // depth internal formats cannot be non-normalized integers
        return channelClass != tcu::TEXTURECHANNELCLASS_FLOATING_POINT;
    }
    else if (format.order == TextureFormat::DS)
    {
        // combined formats have no single channel class, detect format manually
        switch (format.type)
        {
        case tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
            return false;
        case tcu::TextureFormat::UNSIGNED_INT_16_8_8:
            return true;
        case tcu::TextureFormat::UNSIGNED_INT_24_8:
            return true;
        case tcu::TextureFormat::UNSIGNED_INT_24_8_REV:
            return true;

        default:
        {
            // unknown format
            DE_ASSERT(false);
            return true;
        }
        }
    }

    return false;
}

static bool isLinearCompareValid(const Sampler::CompareMode compareMode, const TexComparePrecision &prec,
                                 const Vec2 &depths, const Vec2 &fBounds, const float cmpReference, const float result,
                                 const bool isFixedPointDepth)
{
    DE_ASSERT(0.0f <= fBounds.x() && fBounds.x() <= fBounds.y() && fBounds.y() <= 1.0f);

    const float d0 = depths[0];
    const float d1 = depths[1];

    const CmpResultSet cmp0 = execCompare(compareMode, d0, cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp1 = execCompare(compareMode, d1, cmpReference, prec.referenceBits, isFixedPointDepth);

    const uint32_t isTrue  = (uint32_t(cmp0.isTrue) << 0) | (uint32_t(cmp1.isTrue) << 1);
    const uint32_t isFalse = (uint32_t(cmp0.isFalse) << 0) | (uint32_t(cmp1.isFalse) << 1);

    // Interpolation parameters
    const float f0 = fBounds.x();
    const float f1 = fBounds.y();

    // Error parameters
    const float pcfErr   = computeFixedPointError(prec.pcfBits);
    const float resErr   = computeFixedPointError(prec.resultBits);
    const float totalErr = pcfErr + resErr;

    // Iterate over all valid combinations.
    for (uint32_t comb = 0; comb < (1 << 2); comb++)
    {
        // Filter out invalid combinations.
        if (((comb & isTrue) | (~comb & isFalse)) != (1 << 2) - 1)
            continue;

        const bool cmp0True = ((comb >> 0) & 1) != 0;
        const bool cmp1True = ((comb >> 1) & 1) != 0;

        const float ref0 = cmp0True ? 1.0f : 0.0f;
        const float ref1 = cmp1True ? 1.0f : 0.0f;

        const float v0   = ref0 * (1.0f - f0) + ref1 * f0;
        const float v1   = ref0 * (1.0f - f1) + ref1 * f1;
        const float minV = de::min(v0, v1);
        const float maxV = de::max(v0, v1);
        const float minR = minV - totalErr;
        const float maxR = maxV + totalErr;

        if (de::inRange(result, minR, maxR))
            return true;
    }

    return false;
}

static inline BVec4 extractBVec4(const uint32_t val, int offset)
{
    return BVec4(((val >> (offset + 0)) & 1) != 0, ((val >> (offset + 1)) & 1) != 0, ((val >> (offset + 2)) & 1) != 0,
                 ((val >> (offset + 3)) & 1) != 0);
}

static bool isBilinearAnyCompareValid(const Sampler::CompareMode compareMode, const TexComparePrecision &prec,
                                      const Vec4 &depths, const float cmpReference, const float result,
                                      const bool isFixedPointDepth)
{
    DE_ASSERT(prec.pcfBits == 0);

    const float d0 = depths[0];
    const float d1 = depths[1];
    const float d2 = depths[2];
    const float d3 = depths[3];

    const CmpResultSet cmp0 = execCompare(compareMode, d0, cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp1 = execCompare(compareMode, d1, cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp2 = execCompare(compareMode, d2, cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp3 = execCompare(compareMode, d3, cmpReference, prec.referenceBits, isFixedPointDepth);

    const bool canBeTrue  = cmp0.isTrue || cmp1.isTrue || cmp2.isTrue || cmp3.isTrue;
    const bool canBeFalse = cmp0.isFalse || cmp1.isFalse || cmp2.isFalse || cmp3.isFalse;

    const float resErr = computeFixedPointError(prec.resultBits);

    const float minBound = canBeFalse ? 0.0f : 1.0f;
    const float maxBound = canBeTrue ? 1.0f : 0.0f;

    return de::inRange(result, minBound - resErr, maxBound + resErr);
}

static bool isBilinearPCFCompareValid(const Sampler::CompareMode compareMode, const TexComparePrecision &prec,
                                      const Vec4 &depths, const Vec2 &xBounds, const Vec2 &yBounds,
                                      const float cmpReference, const float result, const bool isFixedPointDepth)
{
    DE_ASSERT(0.0f <= xBounds.x() && xBounds.x() <= xBounds.y() && xBounds.y() <= 1.0f);
    DE_ASSERT(0.0f <= yBounds.x() && yBounds.x() <= yBounds.y() && yBounds.y() <= 1.0f);
    DE_ASSERT(prec.pcfBits > 0);

    const float d0 = depths[0];
    const float d1 = depths[1];
    const float d2 = depths[2];
    const float d3 = depths[3];

    const CmpResultSet cmp0 = execCompare(compareMode, d0, cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp1 = execCompare(compareMode, d1, cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp2 = execCompare(compareMode, d2, cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp3 = execCompare(compareMode, d3, cmpReference, prec.referenceBits, isFixedPointDepth);

    const uint32_t isTrue = (uint32_t(cmp0.isTrue) << 0) | (uint32_t(cmp1.isTrue) << 1) | (uint32_t(cmp2.isTrue) << 2) |
                            (uint32_t(cmp3.isTrue) << 3);
    const uint32_t isFalse = (uint32_t(cmp0.isFalse) << 0) | (uint32_t(cmp1.isFalse) << 1) |
                             (uint32_t(cmp2.isFalse) << 2) | (uint32_t(cmp3.isFalse) << 3);

    // Interpolation parameters
    const float x0 = xBounds.x();
    const float x1 = xBounds.y();
    const float y0 = yBounds.x();
    const float y1 = yBounds.y();

    // Error parameters
    const float pcfErr   = computeFixedPointError(prec.pcfBits);
    const float resErr   = computeFixedPointError(prec.resultBits);
    const float totalErr = pcfErr + resErr;

    // Iterate over all valid combinations.
    // \note It is not enough to compute minmax over all possible result sets, as ranges may
    //         not necessarily overlap, i.e. there are gaps between valid ranges.
    for (uint32_t comb = 0; comb < (1 << 4); comb++)
    {
        // Filter out invalid combinations:
        //  1) True bit is set in comb but not in isTrue => sample can not be true
        //  2) True bit is NOT set in comb and not in isFalse => sample can not be false
        if (((comb & isTrue) | (~comb & isFalse)) != (1 << 4) - 1)
            continue;

        const BVec4 cmpTrue = extractBVec4(comb, 0);
        const Vec4 refVal   = select(Vec4(1.0f), Vec4(0.0f), cmpTrue);

        const float v0   = bilinearInterpolate(refVal, x0, y0);
        const float v1   = bilinearInterpolate(refVal, x1, y0);
        const float v2   = bilinearInterpolate(refVal, x0, y1);
        const float v3   = bilinearInterpolate(refVal, x1, y1);
        const float minV = de::min(v0, de::min(v1, de::min(v2, v3)));
        const float maxV = de::max(v0, de::max(v1, de::max(v2, v3)));
        const float minR = minV - totalErr;
        const float maxR = maxV + totalErr;

        if (de::inRange(result, minR, maxR))
            return true;
    }

    return false;
}

static bool isBilinearCompareValid(const Sampler::CompareMode compareMode, const TexComparePrecision &prec,
                                   const Vec4 &depths, const Vec2 &xBounds, const Vec2 &yBounds,
                                   const float cmpReference, const float result, const bool isFixedPointDepth)
{
    if (prec.pcfBits > 0)
        return isBilinearPCFCompareValid(compareMode, prec, depths, xBounds, yBounds, cmpReference, result,
                                         isFixedPointDepth);
    else
        return isBilinearAnyCompareValid(compareMode, prec, depths, cmpReference, result, isFixedPointDepth);
}

static bool isTrilinearAnyCompareValid(const Sampler::CompareMode compareMode, const TexComparePrecision &prec,
                                       const Vec4 &depths0, const Vec4 &depths1, const float cmpReference,
                                       const float result, const bool isFixedPointDepth)
{
    DE_ASSERT(prec.pcfBits == 0);

    const CmpResultSet cmp00 =
        execCompare(compareMode, depths0[0], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp01 =
        execCompare(compareMode, depths0[1], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp02 =
        execCompare(compareMode, depths0[2], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp03 =
        execCompare(compareMode, depths0[3], cmpReference, prec.referenceBits, isFixedPointDepth);

    const CmpResultSet cmp10 =
        execCompare(compareMode, depths1[0], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp11 =
        execCompare(compareMode, depths1[1], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp12 =
        execCompare(compareMode, depths1[2], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp13 =
        execCompare(compareMode, depths1[3], cmpReference, prec.referenceBits, isFixedPointDepth);

    const bool canBeTrue = cmp00.isTrue || cmp01.isTrue || cmp02.isTrue || cmp03.isTrue || cmp10.isTrue ||
                           cmp11.isTrue || cmp12.isTrue || cmp13.isTrue;
    const bool canBeFalse = cmp00.isFalse || cmp01.isFalse || cmp02.isFalse || cmp03.isFalse || cmp10.isFalse ||
                            cmp11.isFalse || cmp12.isFalse || cmp13.isFalse;

    const float resErr = computeFixedPointError(prec.resultBits);

    const float minBound = canBeFalse ? 0.0f : 1.0f;
    const float maxBound = canBeTrue ? 1.0f : 0.0f;

    return de::inRange(result, minBound - resErr, maxBound + resErr);
}

static bool isTrilinearPCFCompareValid(const Sampler::CompareMode compareMode, const TexComparePrecision &prec,
                                       const Vec4 &depths0, const Vec4 &depths1, const Vec2 &xBounds0,
                                       const Vec2 &yBounds0, const Vec2 &xBounds1, const Vec2 &yBounds1,
                                       const Vec2 &fBounds, const float cmpReference, const float result,
                                       const bool isFixedPointDepth)
{
    DE_ASSERT(0.0f <= xBounds0.x() && xBounds0.x() <= xBounds0.y() && xBounds0.y() <= 1.0f);
    DE_ASSERT(0.0f <= yBounds0.x() && yBounds0.x() <= yBounds0.y() && yBounds0.y() <= 1.0f);
    DE_ASSERT(0.0f <= xBounds1.x() && xBounds1.x() <= xBounds1.y() && xBounds1.y() <= 1.0f);
    DE_ASSERT(0.0f <= yBounds1.x() && yBounds1.x() <= yBounds1.y() && yBounds1.y() <= 1.0f);
    DE_ASSERT(0.0f <= fBounds.x() && fBounds.x() <= fBounds.y() && fBounds.y() <= 1.0f);
    DE_ASSERT(prec.pcfBits > 0);

    const CmpResultSet cmp00 =
        execCompare(compareMode, depths0[0], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp01 =
        execCompare(compareMode, depths0[1], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp02 =
        execCompare(compareMode, depths0[2], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp03 =
        execCompare(compareMode, depths0[3], cmpReference, prec.referenceBits, isFixedPointDepth);

    const CmpResultSet cmp10 =
        execCompare(compareMode, depths1[0], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp11 =
        execCompare(compareMode, depths1[1], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp12 =
        execCompare(compareMode, depths1[2], cmpReference, prec.referenceBits, isFixedPointDepth);
    const CmpResultSet cmp13 =
        execCompare(compareMode, depths1[3], cmpReference, prec.referenceBits, isFixedPointDepth);

    const uint32_t isTrue = (uint32_t(cmp00.isTrue) << 0) | (uint32_t(cmp01.isTrue) << 1) |
                            (uint32_t(cmp02.isTrue) << 2) | (uint32_t(cmp03.isTrue) << 3) |
                            (uint32_t(cmp10.isTrue) << 4) | (uint32_t(cmp11.isTrue) << 5) |
                            (uint32_t(cmp12.isTrue) << 6) | (uint32_t(cmp13.isTrue) << 7);
    const uint32_t isFalse = (uint32_t(cmp00.isFalse) << 0) | (uint32_t(cmp01.isFalse) << 1) |
                             (uint32_t(cmp02.isFalse) << 2) | (uint32_t(cmp03.isFalse) << 3) |
                             (uint32_t(cmp10.isFalse) << 4) | (uint32_t(cmp11.isFalse) << 5) |
                             (uint32_t(cmp12.isFalse) << 6) | (uint32_t(cmp13.isFalse) << 7);

    // Error parameters
    const float pcfErr   = computeFixedPointError(prec.pcfBits);
    const float resErr   = computeFixedPointError(prec.resultBits);
    const float totalErr = pcfErr + resErr;

    // Iterate over all valid combinations.
    for (uint32_t comb = 0; comb < (1 << 8); comb++)
    {
        // Filter out invalid combinations.
        if (((comb & isTrue) | (~comb & isFalse)) != (1 << 8) - 1)
            continue;

        const BVec4 cmpTrue0 = extractBVec4(comb, 0);
        const BVec4 cmpTrue1 = extractBVec4(comb, 4);
        const Vec4 refVal0   = select(Vec4(1.0f), Vec4(0.0f), cmpTrue0);
        const Vec4 refVal1   = select(Vec4(1.0f), Vec4(0.0f), cmpTrue1);

        // Bilinear interpolation within levels.
        const float v00   = bilinearInterpolate(refVal0, xBounds0.x(), yBounds0.x());
        const float v01   = bilinearInterpolate(refVal0, xBounds0.y(), yBounds0.x());
        const float v02   = bilinearInterpolate(refVal0, xBounds0.x(), yBounds0.y());
        const float v03   = bilinearInterpolate(refVal0, xBounds0.y(), yBounds0.y());
        const float minV0 = de::min(v00, de::min(v01, de::min(v02, v03)));
        const float maxV0 = de::max(v00, de::max(v01, de::max(v02, v03)));

        const float v10   = bilinearInterpolate(refVal1, xBounds1.x(), yBounds1.x());
        const float v11   = bilinearInterpolate(refVal1, xBounds1.y(), yBounds1.x());
        const float v12   = bilinearInterpolate(refVal1, xBounds1.x(), yBounds1.y());
        const float v13   = bilinearInterpolate(refVal1, xBounds1.y(), yBounds1.y());
        const float minV1 = de::min(v10, de::min(v11, de::min(v12, v13)));
        const float maxV1 = de::max(v10, de::max(v11, de::max(v12, v13)));

        // Compute min-max bounds by filtering between minimum bounds and maximum bounds between levels.
        // HW can end up choosing pretty much any of samples between levels, and thus interpolating
        // between minimums should yield lower bound for range, and same for upper bound.
        // \todo [2013-07-17 pyry] This seems separable? Can this be optimized? At least ranges could be pre-computed and later combined.
        const float minF0 = minV0 * (1.0f - fBounds.x()) + minV1 * fBounds.x();
        const float minF1 = minV0 * (1.0f - fBounds.y()) + minV1 * fBounds.y();
        const float maxF0 = maxV0 * (1.0f - fBounds.x()) + maxV1 * fBounds.x();
        const float maxF1 = maxV0 * (1.0f - fBounds.y()) + maxV1 * fBounds.y();

        const float minF = de::min(minF0, minF1);
        const float maxF = de::max(maxF0, maxF1);

        const float minR = minF - totalErr;
        const float maxR = maxF + totalErr;

        if (de::inRange(result, minR, maxR))
            return true;
    }

    return false;
}

static bool isTrilinearCompareValid(const Sampler::CompareMode compareMode, const TexComparePrecision &prec,
                                    const Vec4 &depths0, const Vec4 &depths1, const Vec2 &xBounds0,
                                    const Vec2 &yBounds0, const Vec2 &xBounds1, const Vec2 &yBounds1,
                                    const Vec2 &fBounds, const float cmpReference, const float result,
                                    const bool isFixedPointDepth)
{
    if (prec.pcfBits > 0)
        return isTrilinearPCFCompareValid(compareMode, prec, depths0, depths1, xBounds0, yBounds0, xBounds1, yBounds1,
                                          fBounds, cmpReference, result, isFixedPointDepth);
    else
        return isTrilinearAnyCompareValid(compareMode, prec, depths0, depths1, cmpReference, result, isFixedPointDepth);
}

static bool isNearestCompareResultValid(const ConstPixelBufferAccess &level, const Sampler &sampler,
                                        const TexComparePrecision &prec, const Vec2 &coord, const int coordZ,
                                        const float cmpReference, const float result)
{
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(level.getFormat());
    const Vec2 uBounds = computeNonNormalizedCoordBounds(sampler.normalizedCoords, level.getWidth(), coord.x(),
                                                         prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 vBounds = computeNonNormalizedCoordBounds(sampler.normalizedCoords, level.getHeight(), coord.y(),
                                                         prec.coordBits.y(), prec.uvwBits.y());

    // Integer coordinates - without wrap mode
    const int minI = deFloorFloatToInt32(uBounds.x());
    const int maxI = deFloorFloatToInt32(uBounds.y());
    const int minJ = deFloorFloatToInt32(vBounds.x());
    const int maxJ = deFloorFloatToInt32(vBounds.y());

    for (int j = minJ; j <= maxJ; j++)
    {
        for (int i = minI; i <= maxI; i++)
        {
            const int x       = wrap(sampler.wrapS, i, level.getWidth());
            const int y       = wrap(sampler.wrapT, j, level.getHeight());
            const float depth = lookupDepth(level, sampler, x, y, coordZ);
            const CmpResultSet resSet =
                execCompare(sampler.compare, depth, cmpReference, prec.referenceBits, isFixedPointDepth);

            if (isResultInSet(resSet, result, prec.resultBits))
                return true;
        }
    }

    return false;
}

static bool isLinearCompareResultValid(const ConstPixelBufferAccess &level, const Sampler &sampler,
                                       const TexComparePrecision &prec, const Vec2 &coord, const int coordZ,
                                       const float cmpReference, const float result)
{
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(level.getFormat());
    const Vec2 uBounds = computeNonNormalizedCoordBounds(sampler.normalizedCoords, level.getWidth(), coord.x(),
                                                         prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 vBounds = computeNonNormalizedCoordBounds(sampler.normalizedCoords, level.getHeight(), coord.y(),
                                                         prec.coordBits.y(), prec.uvwBits.y());

    // Integer coordinate bounds for (x0,y0) - without wrap mode
    const int minI = deFloorFloatToInt32(uBounds.x() - 0.5f);
    const int maxI = deFloorFloatToInt32(uBounds.y() - 0.5f);
    const int minJ = deFloorFloatToInt32(vBounds.x() - 0.5f);
    const int maxJ = deFloorFloatToInt32(vBounds.y() - 0.5f);

    const int w = level.getWidth();
    const int h = level.getHeight();

    // \todo [2013-07-03 pyry] This could be optimized by first computing ranges based on wrap mode.

    for (int j = minJ; j <= maxJ; j++)
    {
        for (int i = minI; i <= maxI; i++)
        {
            // Wrapped coordinates
            const int x0 = wrap(sampler.wrapS, i, w);
            const int x1 = wrap(sampler.wrapS, i + 1, w);
            const int y0 = wrap(sampler.wrapT, j, h);
            const int y1 = wrap(sampler.wrapT, j + 1, h);

            // Bounds for filtering factors
            const float minA = de::clamp((uBounds.x() - 0.5f) - float(i), 0.0f, 1.0f);
            const float maxA = de::clamp((uBounds.y() - 0.5f) - float(i), 0.0f, 1.0f);
            const float minB = de::clamp((vBounds.x() - 0.5f) - float(j), 0.0f, 1.0f);
            const float maxB = de::clamp((vBounds.y() - 0.5f) - float(j), 0.0f, 1.0f);

            const Vec4 depths(lookupDepth(level, sampler, x0, y0, coordZ), lookupDepth(level, sampler, x1, y0, coordZ),
                              lookupDepth(level, sampler, x0, y1, coordZ), lookupDepth(level, sampler, x1, y1, coordZ));

            if (isBilinearCompareValid(sampler.compare, prec, depths, Vec2(minA, maxA), Vec2(minB, maxB), cmpReference,
                                       result, isFixedPointDepth))
                return true;
        }
    }

    return false;
}

static bool isLevelCompareResultValid(const ConstPixelBufferAccess &level, const Sampler &sampler,
                                      const Sampler::FilterMode filterMode, const TexComparePrecision &prec,
                                      const Vec2 &coord, const int coordZ, const float cmpReference, const float result)
{
    if (filterMode == Sampler::LINEAR)
        return isLinearCompareResultValid(level, sampler, prec, coord, coordZ, cmpReference, result);
    else
        return isNearestCompareResultValid(level, sampler, prec, coord, coordZ, cmpReference, result);
}

static bool isNearestMipmapLinearCompareResultValid(const ConstPixelBufferAccess &level0,
                                                    const ConstPixelBufferAccess &level1, const Sampler &sampler,
                                                    const TexComparePrecision &prec, const Vec2 &coord,
                                                    const int coordZ, const Vec2 &fBounds, const float cmpReference,
                                                    const float result)
{
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(level0.getFormat());

    const int w0 = level0.getWidth();
    const int w1 = level1.getWidth();
    const int h0 = level0.getHeight();
    const int h1 = level1.getHeight();

    const Vec2 uBounds0 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, w0, coord.x(), prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 uBounds1 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, w1, coord.x(), prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 vBounds0 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, h0, coord.y(), prec.coordBits.y(), prec.uvwBits.y());
    const Vec2 vBounds1 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, h1, coord.y(), prec.coordBits.y(), prec.uvwBits.y());

    // Integer coordinates - without wrap mode
    const int minI0 = deFloorFloatToInt32(uBounds0.x());
    const int maxI0 = deFloorFloatToInt32(uBounds0.y());
    const int minI1 = deFloorFloatToInt32(uBounds1.x());
    const int maxI1 = deFloorFloatToInt32(uBounds1.y());
    const int minJ0 = deFloorFloatToInt32(vBounds0.x());
    const int maxJ0 = deFloorFloatToInt32(vBounds0.y());
    const int minJ1 = deFloorFloatToInt32(vBounds1.x());
    const int maxJ1 = deFloorFloatToInt32(vBounds1.y());

    for (int j0 = minJ0; j0 <= maxJ0; j0++)
    {
        for (int i0 = minI0; i0 <= maxI0; i0++)
        {
            const float depth0 =
                lookupDepth(level0, sampler, wrap(sampler.wrapS, i0, w0), wrap(sampler.wrapT, j0, h0), coordZ);

            for (int j1 = minJ1; j1 <= maxJ1; j1++)
            {
                for (int i1 = minI1; i1 <= maxI1; i1++)
                {
                    const float depth1 =
                        lookupDepth(level1, sampler, wrap(sampler.wrapS, i1, w1), wrap(sampler.wrapT, j1, h1), coordZ);

                    if (isLinearCompareValid(sampler.compare, prec, Vec2(depth0, depth1), fBounds, cmpReference, result,
                                             isFixedPointDepth))
                        return true;
                }
            }
        }
    }

    return false;
}

static bool isLinearMipmapLinearCompareResultValid(const ConstPixelBufferAccess &level0,
                                                   const ConstPixelBufferAccess &level1, const Sampler &sampler,
                                                   const TexComparePrecision &prec, const Vec2 &coord, const int coordZ,
                                                   const Vec2 &fBounds, const float cmpReference, const float result)
{
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(level0.getFormat());

    // \todo [2013-07-04 pyry] This is strictly not correct as coordinates between levels should be dependent.
    //                           Right now this allows pairing any two valid bilinear quads.

    const int w0 = level0.getWidth();
    const int w1 = level1.getWidth();
    const int h0 = level0.getHeight();
    const int h1 = level1.getHeight();

    const Vec2 uBounds0 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, w0, coord.x(), prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 uBounds1 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, w1, coord.x(), prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 vBounds0 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, h0, coord.y(), prec.coordBits.y(), prec.uvwBits.y());
    const Vec2 vBounds1 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, h1, coord.y(), prec.coordBits.y(), prec.uvwBits.y());

    // Integer coordinates - without wrap mode
    const int minI0 = deFloorFloatToInt32(uBounds0.x() - 0.5f);
    const int maxI0 = deFloorFloatToInt32(uBounds0.y() - 0.5f);
    const int minI1 = deFloorFloatToInt32(uBounds1.x() - 0.5f);
    const int maxI1 = deFloorFloatToInt32(uBounds1.y() - 0.5f);
    const int minJ0 = deFloorFloatToInt32(vBounds0.x() - 0.5f);
    const int maxJ0 = deFloorFloatToInt32(vBounds0.y() - 0.5f);
    const int minJ1 = deFloorFloatToInt32(vBounds1.x() - 0.5f);
    const int maxJ1 = deFloorFloatToInt32(vBounds1.y() - 0.5f);

    for (int j0 = minJ0; j0 <= maxJ0; j0++)
    {
        for (int i0 = minI0; i0 <= maxI0; i0++)
        {
            const float minA0 = de::clamp((uBounds0.x() - 0.5f) - float(i0), 0.0f, 1.0f);
            const float maxA0 = de::clamp((uBounds0.y() - 0.5f) - float(i0), 0.0f, 1.0f);
            const float minB0 = de::clamp((vBounds0.x() - 0.5f) - float(j0), 0.0f, 1.0f);
            const float maxB0 = de::clamp((vBounds0.y() - 0.5f) - float(j0), 0.0f, 1.0f);
            Vec4 depths0;

            {
                const int x0 = wrap(sampler.wrapS, i0, w0);
                const int x1 = wrap(sampler.wrapS, i0 + 1, w0);
                const int y0 = wrap(sampler.wrapT, j0, h0);
                const int y1 = wrap(sampler.wrapT, j0 + 1, h0);

                depths0[0] = lookupDepth(level0, sampler, x0, y0, coordZ);
                depths0[1] = lookupDepth(level0, sampler, x1, y0, coordZ);
                depths0[2] = lookupDepth(level0, sampler, x0, y1, coordZ);
                depths0[3] = lookupDepth(level0, sampler, x1, y1, coordZ);
            }

            for (int j1 = minJ1; j1 <= maxJ1; j1++)
            {
                for (int i1 = minI1; i1 <= maxI1; i1++)
                {
                    const float minA1 = de::clamp((uBounds1.x() - 0.5f) - float(i1), 0.0f, 1.0f);
                    const float maxA1 = de::clamp((uBounds1.y() - 0.5f) - float(i1), 0.0f, 1.0f);
                    const float minB1 = de::clamp((vBounds1.x() - 0.5f) - float(j1), 0.0f, 1.0f);
                    const float maxB1 = de::clamp((vBounds1.y() - 0.5f) - float(j1), 0.0f, 1.0f);
                    Vec4 depths1;

                    {
                        const int x0 = wrap(sampler.wrapS, i1, w1);
                        const int x1 = wrap(sampler.wrapS, i1 + 1, w1);
                        const int y0 = wrap(sampler.wrapT, j1, h1);
                        const int y1 = wrap(sampler.wrapT, j1 + 1, h1);

                        depths1[0] = lookupDepth(level1, sampler, x0, y0, coordZ);
                        depths1[1] = lookupDepth(level1, sampler, x1, y0, coordZ);
                        depths1[2] = lookupDepth(level1, sampler, x0, y1, coordZ);
                        depths1[3] = lookupDepth(level1, sampler, x1, y1, coordZ);
                    }

                    if (isTrilinearCompareValid(sampler.compare, prec, depths0, depths1, Vec2(minA0, maxA0),
                                                Vec2(minB0, maxB0), Vec2(minA1, maxA1), Vec2(minB1, maxB1), fBounds,
                                                cmpReference, result, isFixedPointDepth))
                        return true;
                }
            }
        }
    }

    return false;
}

static bool isMipmapLinearCompareResultValid(const ConstPixelBufferAccess &level0, const ConstPixelBufferAccess &level1,
                                             const Sampler &sampler, const Sampler::FilterMode levelFilter,
                                             const TexComparePrecision &prec, const Vec2 &coord, const int coordZ,
                                             const Vec2 &fBounds, const float cmpReference, const float result)
{
    if (levelFilter == Sampler::LINEAR)
        return isLinearMipmapLinearCompareResultValid(level0, level1, sampler, prec, coord, coordZ, fBounds,
                                                      cmpReference, result);
    else
        return isNearestMipmapLinearCompareResultValid(level0, level1, sampler, prec, coord, coordZ, fBounds,
                                                       cmpReference, result);
}

bool isTexCompareResultValid(const Texture2DView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec2 &coord, const Vec2 &lodBounds, const float cmpReference, const float result)
{
    const float minLod        = lodBounds.x();
    const float maxLod        = lodBounds.y();
    const bool canBeMagnified = minLod <= sampler.lodThreshold;
    const bool canBeMinified  = maxLod > sampler.lodThreshold;

    DE_ASSERT(isSamplerSupported(sampler));

    if (canBeMagnified)
    {
        if (isLevelCompareResultValid(texture.getLevel(0), sampler, sampler.magFilter, prec, coord, 0, cmpReference,
                                      result))
            return true;
    }

    if (canBeMinified)
    {
        const bool isNearestMipmap = isNearestMipmapFilter(sampler.minFilter);
        const bool isLinearMipmap  = isLinearMipmapFilter(sampler.minFilter);
        const int minTexLevel      = 0;
        const int maxTexLevel      = texture.getNumLevels() - 1;

        DE_ASSERT(minTexLevel < maxTexLevel);

        if (isLinearMipmap)
        {
            const int minLevel = de::clamp((int)deFloatFloor(minLod), minTexLevel, maxTexLevel - 1);
            const int maxLevel = de::clamp((int)deFloatFloor(maxLod), minTexLevel, maxTexLevel - 1);

            DE_ASSERT(minLevel <= maxLevel);

            for (int level = minLevel; level <= maxLevel; level++)
            {
                const float minF = de::clamp(minLod - float(level), 0.0f, 1.0f);
                const float maxF = de::clamp(maxLod - float(level), 0.0f, 1.0f);

                if (isMipmapLinearCompareResultValid(texture.getLevel(level), texture.getLevel(level + 1), sampler,
                                                     getLevelFilter(sampler.minFilter), prec, coord, 0,
                                                     Vec2(minF, maxF), cmpReference, result))
                    return true;
            }
        }
        else if (isNearestMipmap)
        {
            // \note The accurate formula for nearest mipmapping is level = ceil(lod + 0.5) - 1 but Khronos has made
            //         decision to allow floor(lod + 0.5) as well.
            const int minLevel = de::clamp((int)deFloatCeil(minLod + 0.5f) - 1, minTexLevel, maxTexLevel);
            const int maxLevel = de::clamp((int)deFloatFloor(maxLod + 0.5f), minTexLevel, maxTexLevel);

            DE_ASSERT(minLevel <= maxLevel);

            for (int level = minLevel; level <= maxLevel; level++)
            {
                if (isLevelCompareResultValid(texture.getLevel(level), sampler, getLevelFilter(sampler.minFilter), prec,
                                              coord, 0, cmpReference, result))
                    return true;
            }
        }
        else
        {
            if (isLevelCompareResultValid(texture.getLevel(0), sampler, sampler.minFilter, prec, coord, 0, cmpReference,
                                          result))
                return true;
        }
    }

    return false;
}

static bool isSeamplessLinearMipmapLinearCompareResultValid(const TextureCubeView &texture, const int baseLevelNdx,
                                                            const Sampler &sampler, const TexComparePrecision &prec,
                                                            const CubeFaceFloatCoords &coords, const Vec2 &fBounds,
                                                            const float cmpReference, const float result)
{
    const bool isFixedPointDepth =
        isFixedPointDepthTextureFormat(texture.getLevelFace(baseLevelNdx, CUBEFACE_NEGATIVE_X).getFormat());
    const int size0 = texture.getLevelFace(baseLevelNdx, coords.face).getWidth();
    const int size1 = texture.getLevelFace(baseLevelNdx + 1, coords.face).getWidth();

    const Vec2 uBounds0 = computeNonNormalizedCoordBounds(sampler.normalizedCoords, size0, coords.s, prec.coordBits.x(),
                                                          prec.uvwBits.x());
    const Vec2 uBounds1 = computeNonNormalizedCoordBounds(sampler.normalizedCoords, size1, coords.s, prec.coordBits.x(),
                                                          prec.uvwBits.x());
    const Vec2 vBounds0 = computeNonNormalizedCoordBounds(sampler.normalizedCoords, size0, coords.t, prec.coordBits.y(),
                                                          prec.uvwBits.y());
    const Vec2 vBounds1 = computeNonNormalizedCoordBounds(sampler.normalizedCoords, size1, coords.t, prec.coordBits.y(),
                                                          prec.uvwBits.y());

    // Integer coordinates - without wrap mode
    const int minI0 = deFloorFloatToInt32(uBounds0.x() - 0.5f);
    const int maxI0 = deFloorFloatToInt32(uBounds0.y() - 0.5f);
    const int minI1 = deFloorFloatToInt32(uBounds1.x() - 0.5f);
    const int maxI1 = deFloorFloatToInt32(uBounds1.y() - 0.5f);
    const int minJ0 = deFloorFloatToInt32(vBounds0.x() - 0.5f);
    const int maxJ0 = deFloorFloatToInt32(vBounds0.y() - 0.5f);
    const int minJ1 = deFloorFloatToInt32(vBounds1.x() - 0.5f);
    const int maxJ1 = deFloorFloatToInt32(vBounds1.y() - 0.5f);

    tcu::ConstPixelBufferAccess faces0[CUBEFACE_LAST];
    tcu::ConstPixelBufferAccess faces1[CUBEFACE_LAST];

    for (int face = 0; face < CUBEFACE_LAST; face++)
    {
        faces0[face] = texture.getLevelFace(baseLevelNdx, CubeFace(face));
        faces1[face] = texture.getLevelFace(baseLevelNdx + 1, CubeFace(face));
    }

    for (int j0 = minJ0; j0 <= maxJ0; j0++)
    {
        for (int i0 = minI0; i0 <= maxI0; i0++)
        {
            const float minA0 = de::clamp((uBounds0.x() - 0.5f) - float(i0), 0.0f, 1.0f);
            const float maxA0 = de::clamp((uBounds0.y() - 0.5f) - float(i0), 0.0f, 1.0f);
            const float minB0 = de::clamp((vBounds0.x() - 0.5f) - float(j0), 0.0f, 1.0f);
            const float maxB0 = de::clamp((vBounds0.y() - 0.5f) - float(j0), 0.0f, 1.0f);
            Vec4 depths0;

            {
                const CubeFaceIntCoords c00 =
                    remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i0 + 0, j0 + 0)), size0);
                const CubeFaceIntCoords c10 =
                    remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i0 + 1, j0 + 0)), size0);
                const CubeFaceIntCoords c01 =
                    remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i0 + 0, j0 + 1)), size0);
                const CubeFaceIntCoords c11 =
                    remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i0 + 1, j0 + 1)), size0);

                // If any of samples is out of both edges, implementations can do pretty much anything according to spec.
                // \todo [2013-07-08 pyry] Test the special case where all corner pixels have exactly the same color.
                if (c00.face == CUBEFACE_LAST || c01.face == CUBEFACE_LAST || c10.face == CUBEFACE_LAST ||
                    c11.face == CUBEFACE_LAST)
                    return true;

                depths0[0] = lookupDepthNoBorder(faces0[c00.face], sampler, c00.s, c00.t);
                depths0[1] = lookupDepthNoBorder(faces0[c10.face], sampler, c10.s, c10.t);
                depths0[2] = lookupDepthNoBorder(faces0[c01.face], sampler, c01.s, c01.t);
                depths0[3] = lookupDepthNoBorder(faces0[c11.face], sampler, c11.s, c11.t);
            }

            for (int j1 = minJ1; j1 <= maxJ1; j1++)
            {
                for (int i1 = minI1; i1 <= maxI1; i1++)
                {
                    const float minA1 = de::clamp((uBounds1.x() - 0.5f) - float(i1), 0.0f, 1.0f);
                    const float maxA1 = de::clamp((uBounds1.y() - 0.5f) - float(i1), 0.0f, 1.0f);
                    const float minB1 = de::clamp((vBounds1.x() - 0.5f) - float(j1), 0.0f, 1.0f);
                    const float maxB1 = de::clamp((vBounds1.y() - 0.5f) - float(j1), 0.0f, 1.0f);
                    Vec4 depths1;

                    {
                        const CubeFaceIntCoords c00 =
                            remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i1 + 0, j1 + 0)), size1);
                        const CubeFaceIntCoords c10 =
                            remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i1 + 1, j1 + 0)), size1);
                        const CubeFaceIntCoords c01 =
                            remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i1 + 0, j1 + 1)), size1);
                        const CubeFaceIntCoords c11 =
                            remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i1 + 1, j1 + 1)), size1);

                        if (c00.face == CUBEFACE_LAST || c01.face == CUBEFACE_LAST || c10.face == CUBEFACE_LAST ||
                            c11.face == CUBEFACE_LAST)
                            return true;

                        depths1[0] = lookupDepthNoBorder(faces1[c00.face], sampler, c00.s, c00.t);
                        depths1[1] = lookupDepthNoBorder(faces1[c10.face], sampler, c10.s, c10.t);
                        depths1[2] = lookupDepthNoBorder(faces1[c01.face], sampler, c01.s, c01.t);
                        depths1[3] = lookupDepthNoBorder(faces1[c11.face], sampler, c11.s, c11.t);
                    }

                    if (isTrilinearCompareValid(sampler.compare, prec, depths0, depths1, Vec2(minA0, maxA0),
                                                Vec2(minB0, maxB0), Vec2(minA1, maxA1), Vec2(minB1, maxB1), fBounds,
                                                cmpReference, result, isFixedPointDepth))
                        return true;
                }
            }
        }
    }

    return false;
}

static bool isCubeMipmapLinearCompareResultValid(const TextureCubeView &texture, const int baseLevelNdx,
                                                 const Sampler &sampler, const Sampler::FilterMode levelFilter,
                                                 const TexComparePrecision &prec, const CubeFaceFloatCoords &coords,
                                                 const Vec2 &fBounds, const float cmpReference, const float result)
{
    if (levelFilter == Sampler::LINEAR)
    {
        if (sampler.seamlessCubeMap)
            return isSeamplessLinearMipmapLinearCompareResultValid(texture, baseLevelNdx, sampler, prec, coords,
                                                                   fBounds, cmpReference, result);
        else
            return isLinearMipmapLinearCompareResultValid(
                texture.getLevelFace(baseLevelNdx, coords.face), texture.getLevelFace(baseLevelNdx + 1, coords.face),
                sampler, prec, Vec2(coords.s, coords.t), 0, fBounds, cmpReference, result);
    }
    else
        return isNearestMipmapLinearCompareResultValid(
            texture.getLevelFace(baseLevelNdx, coords.face), texture.getLevelFace(baseLevelNdx + 1, coords.face),
            sampler, prec, Vec2(coords.s, coords.t), 0, fBounds, cmpReference, result);
}

static bool isSeamlessLinearCompareResultValid(const TextureCubeView &texture, const int levelNdx,
                                               const Sampler &sampler, const TexComparePrecision &prec,
                                               const CubeFaceFloatCoords &coords, const float cmpReference,
                                               const float result)
{
    const bool isFixedPointDepth =
        isFixedPointDepthTextureFormat(texture.getLevelFace(levelNdx, CUBEFACE_NEGATIVE_X).getFormat());
    const int size = texture.getLevelFace(levelNdx, coords.face).getWidth();

    const Vec2 uBounds =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, size, coords.s, prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 vBounds =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, size, coords.t, prec.coordBits.y(), prec.uvwBits.y());

    // Integer coordinate bounds for (x0,y0) - without wrap mode
    const int minI = deFloorFloatToInt32(uBounds.x() - 0.5f);
    const int maxI = deFloorFloatToInt32(uBounds.y() - 0.5f);
    const int minJ = deFloorFloatToInt32(vBounds.x() - 0.5f);
    const int maxJ = deFloorFloatToInt32(vBounds.y() - 0.5f);

    // Face accesses
    ConstPixelBufferAccess faces[CUBEFACE_LAST];
    for (int face = 0; face < CUBEFACE_LAST; face++)
        faces[face] = texture.getLevelFace(levelNdx, CubeFace(face));

    for (int j = minJ; j <= maxJ; j++)
    {
        for (int i = minI; i <= maxI; i++)
        {
            const CubeFaceIntCoords c00 =
                remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i + 0, j + 0)), size);
            const CubeFaceIntCoords c10 =
                remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i + 1, j + 0)), size);
            const CubeFaceIntCoords c01 =
                remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i + 0, j + 1)), size);
            const CubeFaceIntCoords c11 =
                remapCubeEdgeCoords(CubeFaceIntCoords(coords.face, IVec2(i + 1, j + 1)), size);

            // If any of samples is out of both edges, implementations can do pretty much anything according to spec.
            // \todo [2013-07-08 pyry] Test the special case where all corner pixels have exactly the same color.
            if (c00.face == CUBEFACE_LAST || c01.face == CUBEFACE_LAST || c10.face == CUBEFACE_LAST ||
                c11.face == CUBEFACE_LAST)
                return true;

            // Bounds for filtering factors
            const float minA = de::clamp((uBounds.x() - 0.5f) - float(i), 0.0f, 1.0f);
            const float maxA = de::clamp((uBounds.y() - 0.5f) - float(i), 0.0f, 1.0f);
            const float minB = de::clamp((vBounds.x() - 0.5f) - float(j), 0.0f, 1.0f);
            const float maxB = de::clamp((vBounds.y() - 0.5f) - float(j), 0.0f, 1.0f);

            Vec4 depths;
            depths[0] = lookupDepthNoBorder(faces[c00.face], sampler, c00.s, c00.t);
            depths[1] = lookupDepthNoBorder(faces[c10.face], sampler, c10.s, c10.t);
            depths[2] = lookupDepthNoBorder(faces[c01.face], sampler, c01.s, c01.t);
            depths[3] = lookupDepthNoBorder(faces[c11.face], sampler, c11.s, c11.t);

            if (isBilinearCompareValid(sampler.compare, prec, depths, Vec2(minA, maxA), Vec2(minB, maxB), cmpReference,
                                       result, isFixedPointDepth))
                return true;
        }
    }

    return false;
}

static bool isCubeLevelCompareResultValid(const TextureCubeView &texture, const int levelNdx, const Sampler &sampler,
                                          const Sampler::FilterMode filterMode, const TexComparePrecision &prec,
                                          const CubeFaceFloatCoords &coords, const float cmpReference,
                                          const float result)
{
    if (filterMode == Sampler::LINEAR)
    {
        if (sampler.seamlessCubeMap)
            return isSeamlessLinearCompareResultValid(texture, levelNdx, sampler, prec, coords, cmpReference, result);
        else
            return isLinearCompareResultValid(texture.getLevelFace(levelNdx, coords.face), sampler, prec,
                                              Vec2(coords.s, coords.t), 0, cmpReference, result);
    }
    else
        return isNearestCompareResultValid(texture.getLevelFace(levelNdx, coords.face), sampler, prec,
                                           Vec2(coords.s, coords.t), 0, cmpReference, result);
}

static bool isCubeLevelCompareResultValid(const TextureCubeArrayView &texture, const int baseLevelNdx,
                                          const Sampler &sampler, const Sampler::FilterMode filterMode,
                                          const TexComparePrecision &prec, const CubeFaceFloatCoords &coords,
                                          const float depth, const float cmpReference, const float result)
{
    const float depthErr =
        computeFloatingPointError(depth, prec.coordBits.z()) + computeFixedPointError(prec.uvwBits.z());
    const float minZ    = depth - depthErr;
    const float maxZ    = depth + depthErr;
    const int minLayer  = de::clamp(deFloorFloatToInt32(minZ + 0.5f), 0, texture.getNumLayers() - 1);
    const int maxLayer  = de::clamp(deFloorFloatToInt32(maxZ + 0.5f), 0, texture.getNumLayers() - 1);
    const int numLevels = texture.getNumLevels();

    for (int layer = minLayer; layer <= maxLayer; layer++)
    {
        std::vector<tcu::ConstPixelBufferAccess> levelsAtLayer[CUBEFACE_LAST];

        for (int faceNdx = 0; faceNdx < CUBEFACE_LAST; faceNdx++)
        {
            levelsAtLayer[faceNdx].resize(numLevels);

            for (int levelNdx = 0; levelNdx < numLevels; ++levelNdx)
            {
                const tcu::ConstPixelBufferAccess &level = texture.getLevel(levelNdx);

                levelsAtLayer[faceNdx][levelNdx] =
                    ConstPixelBufferAccess(level.getFormat(), level.getWidth(), level.getHeight(), 1,
                                           level.getPixelPtr(0, 0, CUBEFACE_LAST * layer + faceNdx));
            }
        }

        const tcu::ConstPixelBufferAccess *levels[CUBEFACE_LAST]{
            // Such a strange order due to sampleCompare TextureCubeArrayView uses getCubeArrayFaceIndex while in TextureCubeView does not
            &levelsAtLayer[1][0], &levelsAtLayer[0][0], &levelsAtLayer[3][0],
            &levelsAtLayer[2][0], &levelsAtLayer[5][0], &levelsAtLayer[4][0],
        };

        if (isCubeLevelCompareResultValid(TextureCubeView(numLevels, levels), baseLevelNdx, sampler, filterMode, prec,
                                          coords, cmpReference, result))
            return true;
    }

    return false;
}

static bool isCubeMipmapLinearCompareResultValid(const TextureCubeArrayView &texture, const int baseLevelNdx,
                                                 const Sampler &sampler, const Sampler::FilterMode levelFilter,
                                                 const TexComparePrecision &prec, const CubeFaceFloatCoords &coords,
                                                 const float depth, const Vec2 &fBounds, const float cmpReference,
                                                 const float result)
{
    const float depthErr =
        computeFloatingPointError(depth, prec.coordBits.z()) + computeFixedPointError(prec.uvwBits.z());
    const float minZ    = depth - depthErr;
    const float maxZ    = depth + depthErr;
    const int minLayer  = de::clamp(deFloorFloatToInt32(minZ + 0.5f), 0, texture.getNumLayers() - 1);
    const int maxLayer  = de::clamp(deFloorFloatToInt32(maxZ + 0.5f), 0, texture.getNumLayers() - 1);
    const int numLevels = texture.getNumLevels();

    for (int layer = minLayer; layer <= maxLayer; layer++)
    {
        std::vector<tcu::ConstPixelBufferAccess> levelsAtLayer[CUBEFACE_LAST];

        for (int faceNdx = 0; faceNdx < CUBEFACE_LAST; faceNdx++)
        {
            levelsAtLayer[faceNdx].resize(numLevels);

            for (int levelNdx = 0; levelNdx < numLevels; ++levelNdx)
            {
                const tcu::ConstPixelBufferAccess &level = texture.getLevel(levelNdx);

                levelsAtLayer[faceNdx][levelNdx] =
                    ConstPixelBufferAccess(level.getFormat(), level.getWidth(), level.getHeight(), 1,
                                           level.getPixelPtr(0, 0, CUBEFACE_LAST * layer + faceNdx));
            }
        }

        const tcu::ConstPixelBufferAccess *levels[CUBEFACE_LAST]{
            // Such a strange order due to sampleCompare TextureCubeArrayView uses getCubeArrayFaceIndex while in TextureCubeView does not
            &levelsAtLayer[1][0], &levelsAtLayer[0][0], &levelsAtLayer[3][0],
            &levelsAtLayer[2][0], &levelsAtLayer[5][0], &levelsAtLayer[4][0],
        };

        if (isCubeMipmapLinearCompareResultValid(TextureCubeView(numLevels, levels), baseLevelNdx, sampler, levelFilter,
                                                 prec, coords, fBounds, cmpReference, result))
            return true;
    }

    return false;
}

static bool isNearestCompareResultValid(const ConstPixelBufferAccess &level, const Sampler &sampler,
                                        const TexComparePrecision &prec, const Vec1 &coord, const int coordZ,
                                        const float cmpReference, const float result)
{
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(level.getFormat());
    const Vec2 uBounds = computeNonNormalizedCoordBounds(sampler.normalizedCoords, level.getWidth(), coord.x(),
                                                         prec.coordBits.x(), prec.uvwBits.x());

    // Integer coordinates - without wrap mode
    const int minI = deFloorFloatToInt32(uBounds.x());
    const int maxI = deFloorFloatToInt32(uBounds.y());

    for (int i = minI; i <= maxI; i++)
    {
        const int x       = wrap(sampler.wrapS, i, level.getWidth());
        const float depth = lookupDepth(level, sampler, x, coordZ, 0);
        const CmpResultSet resSet =
            execCompare(sampler.compare, depth, cmpReference, prec.referenceBits, isFixedPointDepth);

        if (isResultInSet(resSet, result, prec.resultBits))
            return true;
    }

    return false;
}

static bool isLinearCompareResultValid(const ConstPixelBufferAccess &level, const Sampler &sampler,
                                       const TexComparePrecision &prec, const Vec1 &coord, const int coordZ,
                                       const float cmpReference, const float result)
{
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(level.getFormat());
    const Vec2 uBounds = computeNonNormalizedCoordBounds(sampler.normalizedCoords, level.getWidth(), coord.x(),
                                                         prec.coordBits.x(), prec.uvwBits.x());

    // Integer coordinate bounds for (x0,y0) - without wrap mode
    const int minI = deFloorFloatToInt32(uBounds.x() - 0.5f);
    const int maxI = deFloorFloatToInt32(uBounds.y() - 0.5f);

    const int w = level.getWidth();

    // \todo [2013-07-03 pyry] This could be optimized by first computing ranges based on wrap mode.

    for (int i = minI; i <= maxI; i++)
    {
        // Wrapped coordinates
        const int x0 = wrap(sampler.wrapS, i, w);
        const int x1 = wrap(sampler.wrapS, i + 1, w);

        // Bounds for filtering factors
        const float minA = de::clamp((uBounds.x() - 0.5f) - float(i), 0.0f, 1.0f);
        const float maxA = de::clamp((uBounds.y() - 0.5f) - float(i), 0.0f, 1.0f);

        const Vec2 depths(lookupDepth(level, sampler, x0, coordZ, 0), lookupDepth(level, sampler, x1, coordZ, 0));

        if (isLinearCompareValid(sampler.compare, prec, depths, Vec2(minA, maxA), cmpReference, result,
                                 isFixedPointDepth))
            return true;
    }

    return false;
}

static bool isLevelCompareResultValid(const ConstPixelBufferAccess &level, const Sampler &sampler,
                                      const Sampler::FilterMode filterMode, const TexComparePrecision &prec,
                                      const Vec1 &coord, const int coordZ, const float cmpReference, const float result)
{
    if (filterMode == Sampler::LINEAR)
        return isLinearCompareResultValid(level, sampler, prec, coord, coordZ, cmpReference, result);
    else
        return isNearestCompareResultValid(level, sampler, prec, coord, coordZ, cmpReference, result);
}

static bool isNearestMipmapLinearCompareResultValid(const ConstPixelBufferAccess &level0,
                                                    const ConstPixelBufferAccess &level1, const Sampler &sampler,
                                                    const TexComparePrecision &prec, const Vec1 &coord,
                                                    const int coordZ, const Vec2 &fBounds, const float cmpReference,
                                                    const float result)
{
    DE_UNREF(fBounds);
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(level0.getFormat());

    const int w0 = level0.getWidth();
    const int w1 = level1.getWidth();

    const Vec2 uBounds0 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, w0, coord.x(), prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 uBounds1 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, w1, coord.x(), prec.coordBits.x(), prec.uvwBits.x());

    // Integer coordinates - without wrap mode
    const int minI0 = deFloorFloatToInt32(uBounds0.x());
    const int maxI0 = deFloorFloatToInt32(uBounds0.y());
    const int minI1 = deFloorFloatToInt32(uBounds1.x());
    const int maxI1 = deFloorFloatToInt32(uBounds1.y());

    for (int i0 = minI0; i0 <= maxI0; i0++)
    {
        const float depth0 = lookupDepth(level0, sampler, wrap(sampler.wrapS, i0, w0), coordZ, 0);

        for (int i1 = minI1; i1 <= maxI1; i1++)
        {
            const float depth1 = lookupDepth(level1, sampler, wrap(sampler.wrapS, i1, w1), coordZ, 0);

            if (isLinearCompareValid(sampler.compare, prec, Vec2(depth0, depth1), fBounds, cmpReference, result,
                                     isFixedPointDepth))
                return true;
        }
    }

    return false;
}

static bool isLinearMipmapLinearCompareResultValid(const ConstPixelBufferAccess &level0,
                                                   const ConstPixelBufferAccess &level1, const Sampler &sampler,
                                                   const TexComparePrecision &prec, const Vec1 &coord, const int coordZ,
                                                   const Vec2 &fBounds, const float cmpReference, const float result)
{
    DE_UNREF(fBounds);
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(level0.getFormat());

    // \todo [2013-07-04 pyry] This is strictly not correct as coordinates between levels should be dependent.
    //                           Right now this allows pairing any two valid bilinear quads.

    const int w0 = level0.getWidth();
    const int w1 = level1.getWidth();

    const Vec2 uBounds0 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, w0, coord.x(), prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 uBounds1 =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, w1, coord.x(), prec.coordBits.x(), prec.uvwBits.x());

    // Integer coordinates - without wrap mode
    const int minI0 = deFloorFloatToInt32(uBounds0.x() - 0.5f);
    const int maxI0 = deFloorFloatToInt32(uBounds0.y() - 0.5f);
    const int minI1 = deFloorFloatToInt32(uBounds1.x() - 0.5f);
    const int maxI1 = deFloorFloatToInt32(uBounds1.y() - 0.5f);

    for (int i0 = minI0; i0 <= maxI0; i0++)
    {
        const float minA0 = de::clamp((uBounds0.x() - 0.5f) - float(i0), 0.0f, 1.0f);
        const float maxA0 = de::clamp((uBounds0.y() - 0.5f) - float(i0), 0.0f, 1.0f);
        const Vec2 ptA0   = Vec2(minA0, maxA0);
        Vec4 depths;

        {
            const int x0 = wrap(sampler.wrapS, i0, w0);
            const int x1 = wrap(sampler.wrapS, i0 + 1, w0);

            depths[0] = lookupDepth(level0, sampler, x0, coordZ, 0);
            depths[1] = lookupDepth(level0, sampler, x1, coordZ, 0);
        }

        for (int i1 = minI1; i1 <= maxI1; i1++)
        {
            const float minA1 = de::clamp((uBounds1.x() - 0.5f) - float(i1), 0.0f, 1.0f);
            const float maxA1 = de::clamp((uBounds1.y() - 0.5f) - float(i1), 0.0f, 1.0f);
            const Vec2 ptA1   = Vec2(minA1, maxA1);

            {
                const int x0 = wrap(sampler.wrapS, i1, w1);
                const int x1 = wrap(sampler.wrapS, i1 + 1, w1);

                depths[2] = lookupDepth(level1, sampler, x0, coordZ, 0);
                depths[3] = lookupDepth(level1, sampler, x1, coordZ, 0);
            }

            if (isBilinearCompareValid(sampler.compare, prec, depths, ptA0, ptA1, cmpReference, result,
                                       isFixedPointDepth))
                return true;
        }
    }

    return false;
}

static bool isMipmapLinearCompareResultValid(const ConstPixelBufferAccess &level0, const ConstPixelBufferAccess &level1,
                                             const Sampler &sampler, const Sampler::FilterMode levelFilter,
                                             const TexComparePrecision &prec, const Vec1 &coord, const int coordZ,
                                             const Vec2 &fBounds, const float cmpReference, const float result)
{
    if (levelFilter == Sampler::LINEAR)
        return isLinearMipmapLinearCompareResultValid(level0, level1, sampler, prec, coord, coordZ, fBounds,
                                                      cmpReference, result);
    else
        return isNearestMipmapLinearCompareResultValid(level0, level1, sampler, prec, coord, coordZ, fBounds,
                                                       cmpReference, result);
}

bool isTexCompareResultValid(const TextureCubeView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec3 &coord, const Vec2 &lodBounds, const float cmpReference, const float result)
{
    int numPossibleFaces = 0;
    CubeFace possibleFaces[CUBEFACE_LAST];

    DE_ASSERT(isSamplerSupported(sampler));

    getPossibleCubeFaces(coord, prec.coordBits, &possibleFaces[0], numPossibleFaces);

    if (numPossibleFaces == 0)
        return true; // Result is undefined.

    for (int tryFaceNdx = 0; tryFaceNdx < numPossibleFaces; tryFaceNdx++)
    {
        const CubeFaceFloatCoords faceCoords(possibleFaces[tryFaceNdx],
                                             projectToFace(possibleFaces[tryFaceNdx], coord));
        const float minLod        = lodBounds.x();
        const float maxLod        = lodBounds.y();
        const bool canBeMagnified = minLod <= sampler.lodThreshold;
        const bool canBeMinified  = maxLod > sampler.lodThreshold;

        if (canBeMagnified)
        {
            if (isCubeLevelCompareResultValid(texture, 0, sampler, sampler.magFilter, prec, faceCoords, cmpReference,
                                              result))
                return true;
        }

        if (canBeMinified)
        {
            const bool isNearestMipmap = isNearestMipmapFilter(sampler.minFilter);
            const bool isLinearMipmap  = isLinearMipmapFilter(sampler.minFilter);
            const int minTexLevel      = 0;
            const int maxTexLevel      = texture.getNumLevels() - 1;

            DE_ASSERT(minTexLevel < maxTexLevel);

            if (isLinearMipmap)
            {
                const int minLevel = de::clamp((int)deFloatFloor(minLod), minTexLevel, maxTexLevel - 1);
                const int maxLevel = de::clamp((int)deFloatFloor(maxLod), minTexLevel, maxTexLevel - 1);

                DE_ASSERT(minLevel <= maxLevel);

                for (int level = minLevel; level <= maxLevel; level++)
                {
                    const float minF = de::clamp(minLod - float(level), 0.0f, 1.0f);
                    const float maxF = de::clamp(maxLod - float(level), 0.0f, 1.0f);

                    if (isCubeMipmapLinearCompareResultValid(texture, level, sampler, getLevelFilter(sampler.minFilter),
                                                             prec, faceCoords, Vec2(minF, maxF), cmpReference, result))
                        return true;
                }
            }
            else if (isNearestMipmap)
            {
                // \note The accurate formula for nearest mipmapping is level = ceil(lod + 0.5) - 1 but Khronos has made
                //         decision to allow floor(lod + 0.5) as well.
                const int minLevel = de::clamp((int)deFloatCeil(minLod + 0.5f) - 1, minTexLevel, maxTexLevel);
                const int maxLevel = de::clamp((int)deFloatFloor(maxLod + 0.5f), minTexLevel, maxTexLevel);

                DE_ASSERT(minLevel <= maxLevel);

                for (int level = minLevel; level <= maxLevel; level++)
                {
                    if (isCubeLevelCompareResultValid(texture, level, sampler, getLevelFilter(sampler.minFilter), prec,
                                                      faceCoords, cmpReference, result))
                        return true;
                }
            }
            else
            {
                if (isCubeLevelCompareResultValid(texture, 0, sampler, sampler.minFilter, prec, faceCoords,
                                                  cmpReference, result))
                    return true;
            }
        }
    }

    return false;
}

bool isTexCompareResultValid(const Texture2DArrayView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec3 &coord, const Vec2 &lodBounds, const float cmpReference, const float result)
{
    const float depthErr =
        computeFloatingPointError(coord.z(), prec.coordBits.z()) + computeFixedPointError(prec.uvwBits.z());
    const float minZ   = coord.z() - depthErr;
    const float maxZ   = coord.z() + depthErr;
    const int minLayer = de::clamp(deFloorFloatToInt32(minZ + 0.5f), 0, texture.getNumLayers() - 1);
    const int maxLayer = de::clamp(deFloorFloatToInt32(maxZ + 0.5f), 0, texture.getNumLayers() - 1);

    DE_ASSERT(isSamplerSupported(sampler));

    for (int layer = minLayer; layer <= maxLayer; layer++)
    {
        const float minLod        = lodBounds.x();
        const float maxLod        = lodBounds.y();
        const bool canBeMagnified = minLod <= sampler.lodThreshold;
        const bool canBeMinified  = maxLod > sampler.lodThreshold;

        if (canBeMagnified)
        {
            if (isLevelCompareResultValid(texture.getLevel(0), sampler, sampler.magFilter, prec, coord.swizzle(0, 1),
                                          layer, cmpReference, result))
                return true;
        }

        if (canBeMinified)
        {
            const bool isNearestMipmap = isNearestMipmapFilter(sampler.minFilter);
            const bool isLinearMipmap  = isLinearMipmapFilter(sampler.minFilter);
            const int minTexLevel      = 0;
            const int maxTexLevel      = texture.getNumLevels() - 1;

            DE_ASSERT(minTexLevel < maxTexLevel);

            if (isLinearMipmap)
            {
                const int minLevel = de::clamp((int)deFloatFloor(minLod), minTexLevel, maxTexLevel - 1);
                const int maxLevel = de::clamp((int)deFloatFloor(maxLod), minTexLevel, maxTexLevel - 1);

                DE_ASSERT(minLevel <= maxLevel);

                for (int level = minLevel; level <= maxLevel; level++)
                {
                    const float minF = de::clamp(minLod - float(level), 0.0f, 1.0f);
                    const float maxF = de::clamp(maxLod - float(level), 0.0f, 1.0f);

                    if (isMipmapLinearCompareResultValid(texture.getLevel(level), texture.getLevel(level + 1), sampler,
                                                         getLevelFilter(sampler.minFilter), prec, coord.swizzle(0, 1),
                                                         layer, Vec2(minF, maxF), cmpReference, result))
                        return true;
                }
            }
            else if (isNearestMipmap)
            {
                // \note The accurate formula for nearest mipmapping is level = ceil(lod + 0.5) - 1 but Khronos has made
                //         decision to allow floor(lod + 0.5) as well.
                const int minLevel = de::clamp((int)deFloatCeil(minLod + 0.5f) - 1, minTexLevel, maxTexLevel);
                const int maxLevel = de::clamp((int)deFloatFloor(maxLod + 0.5f), minTexLevel, maxTexLevel);

                DE_ASSERT(minLevel <= maxLevel);

                for (int level = minLevel; level <= maxLevel; level++)
                {
                    if (isLevelCompareResultValid(texture.getLevel(level), sampler, getLevelFilter(sampler.minFilter),
                                                  prec, coord.swizzle(0, 1), layer, cmpReference, result))
                        return true;
                }
            }
            else
            {
                if (isLevelCompareResultValid(texture.getLevel(0), sampler, sampler.minFilter, prec,
                                              coord.swizzle(0, 1), layer, cmpReference, result))
                    return true;
            }
        }
    }

    return false;
}

bool isTexCompareResultValid(const Texture1DView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec1 &coord, const Vec2 &lodBounds, const float cmpReference, const float result)
{
    const float minLod        = lodBounds.x();
    const float maxLod        = lodBounds.y();
    const bool canBeMagnified = minLod <= sampler.lodThreshold;
    const bool canBeMinified  = maxLod > sampler.lodThreshold;

    DE_ASSERT(isSamplerSupported(sampler));

    if (canBeMagnified)
    {
        if (isLevelCompareResultValid(texture.getLevel(0), sampler, sampler.magFilter, prec, coord, 0, cmpReference,
                                      result))
            return true;
    }

    if (canBeMinified)
    {
        const bool isNearestMipmap = isNearestMipmapFilter(sampler.minFilter);
        const bool isLinearMipmap  = isLinearMipmapFilter(sampler.minFilter);
        const int minTexLevel      = 0;
        const int maxTexLevel      = texture.getNumLevels() - 1;

        DE_ASSERT(minTexLevel < maxTexLevel);

        if (isLinearMipmap)
        {
            const int minLevel = de::clamp((int)deFloatFloor(minLod), minTexLevel, maxTexLevel - 1);
            const int maxLevel = de::clamp((int)deFloatFloor(maxLod), minTexLevel, maxTexLevel - 1);

            DE_ASSERT(minLevel <= maxLevel);

            for (int level = minLevel; level <= maxLevel; level++)
            {
                const float minF = de::clamp(minLod - float(level), 0.0f, 1.0f);
                const float maxF = de::clamp(maxLod - float(level), 0.0f, 1.0f);

                if (isMipmapLinearCompareResultValid(texture.getLevel(level), texture.getLevel(level + 1), sampler,
                                                     getLevelFilter(sampler.minFilter), prec, coord, 0,
                                                     Vec2(minF, maxF), cmpReference, result))
                    return true;
            }
        }
        else if (isNearestMipmap)
        {
            // \note The accurate formula for nearest mipmapping is level = ceil(lod + 0.5) - 1 but Khronos has made
            //         decision to allow floor(lod + 0.5) as well.
            const int minLevel = de::clamp((int)deFloatCeil(minLod + 0.5f) - 1, minTexLevel, maxTexLevel);
            const int maxLevel = de::clamp((int)deFloatFloor(maxLod + 0.5f), minTexLevel, maxTexLevel);

            DE_ASSERT(minLevel <= maxLevel);

            for (int level = minLevel; level <= maxLevel; level++)
            {
                if (isLevelCompareResultValid(texture.getLevel(level), sampler, getLevelFilter(sampler.minFilter), prec,
                                              coord, 0, cmpReference, result))
                    return true;
            }
        }
        else
        {
            if (isLevelCompareResultValid(texture.getLevel(0), sampler, sampler.minFilter, prec, coord, 0, cmpReference,
                                          result))
                return true;
        }
    }

    return false;
}

bool isTexCompareResultValid(const Texture1DArrayView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec2 &coord, const Vec2 &lodBounds, const float cmpReference, const float result)
{
    const float depthErr = computeFloatingPointError(coord.y(), prec.coordBits.y()) +
                           computeFixedPointError(prec.uvwBits.y()); //\todo: should we go with y in prec?
    const float minZ   = coord.y() - depthErr;
    const float maxZ   = coord.y() + depthErr;
    const int minLayer = de::clamp(deFloorFloatToInt32(minZ + 0.5f), 0, texture.getNumLayers() - 1);
    const int maxLayer = de::clamp(deFloorFloatToInt32(maxZ + 0.5f), 0, texture.getNumLayers() - 1);

    DE_ASSERT(isSamplerSupported(sampler));

    for (int layer = minLayer; layer <= maxLayer; layer++)
    {
        const float minLod        = lodBounds.x();
        const float maxLod        = lodBounds.y();
        const bool canBeMagnified = minLod <= sampler.lodThreshold;
        const bool canBeMinified  = maxLod > sampler.lodThreshold;

        if (canBeMagnified)
        {
            if (isLevelCompareResultValid(texture.getLevel(0), sampler, sampler.magFilter, prec, Vec1(coord.x()), layer,
                                          cmpReference, result))
                return true;
        }

        if (canBeMinified)
        {
            const bool isNearestMipmap = isNearestMipmapFilter(sampler.minFilter);
            const bool isLinearMipmap  = isLinearMipmapFilter(sampler.minFilter);
            const int minTexLevel      = 0;
            const int maxTexLevel      = texture.getNumLevels() - 1;

            DE_ASSERT(minTexLevel < maxTexLevel);

            if (isLinearMipmap)
            {
                const int minLevel = de::clamp((int)deFloatFloor(minLod), minTexLevel, maxTexLevel - 1);
                const int maxLevel = de::clamp((int)deFloatFloor(maxLod), minTexLevel, maxTexLevel - 1);

                DE_ASSERT(minLevel <= maxLevel);

                for (int level = minLevel; level <= maxLevel; level++)
                {
                    const float minF = de::clamp(minLod - float(level), 0.0f, 1.0f);
                    const float maxF = de::clamp(maxLod - float(level), 0.0f, 1.0f);

                    if (isMipmapLinearCompareResultValid(texture.getLevel(level), texture.getLevel(level + 1), sampler,
                                                         getLevelFilter(sampler.minFilter), prec, Vec1(coord.x()),
                                                         layer, Vec2(minF, maxF), cmpReference, result))
                        return true;
                }
            }
            else if (isNearestMipmap)
            {
                // \note The accurate formula for nearest mipmapping is level = ceil(lod + 0.5) - 1 but Khronos has made
                //         decision to allow floor(lod + 0.5) as well.
                const int minLevel = de::clamp((int)deFloatCeil(minLod + 0.5f) - 1, minTexLevel, maxTexLevel);
                const int maxLevel = de::clamp((int)deFloatFloor(maxLod + 0.5f), minTexLevel, maxTexLevel);

                DE_ASSERT(minLevel <= maxLevel);

                for (int level = minLevel; level <= maxLevel; level++)
                {
                    if (isLevelCompareResultValid(texture.getLevel(level), sampler, getLevelFilter(sampler.minFilter),
                                                  prec, Vec1(coord.x()), layer, cmpReference, result))
                        return true;
                }
            }
            else
            {
                if (isLevelCompareResultValid(texture.getLevel(0), sampler, sampler.minFilter, prec, Vec1(coord.x()),
                                              layer, cmpReference, result))
                    return true;
            }
        }
    }

    return false;
}

bool isTexCompareResultValid(const TextureCubeArrayView &texture, const Sampler &sampler,
                             const TexComparePrecision &prec, const Vec4 &coord, const Vec2 &lodBounds,
                             const float cmpReference, const float result)
{
    const Vec3 coord3    = coord.swizzle(0, 1, 2);
    int numPossibleFaces = 0;
    CubeFace possibleFaces[CUBEFACE_LAST];

    DE_ASSERT(isSamplerSupported(sampler));

    getPossibleCubeFaces(coord3, prec.coordBits, &possibleFaces[0], numPossibleFaces);

    if (numPossibleFaces == 0)
        return true; // Result is undefined.

    for (int tryFaceNdx = 0; tryFaceNdx < numPossibleFaces; tryFaceNdx++)
    {
        const CubeFaceFloatCoords faceCoords(possibleFaces[tryFaceNdx],
                                             projectToFace(possibleFaces[tryFaceNdx], coord3));
        const float minLod        = lodBounds.x();
        const float maxLod        = lodBounds.y();
        const bool canBeMagnified = minLod <= sampler.lodThreshold;
        const bool canBeMinified  = maxLod > sampler.lodThreshold;

        if (canBeMagnified)
        {
            if (isCubeLevelCompareResultValid(texture, 0, sampler, sampler.magFilter, prec, faceCoords, coord.w(),
                                              cmpReference, result))
                return true;
        }

        if (canBeMinified)
        {
            const bool isNearestMipmap = isNearestMipmapFilter(sampler.minFilter);
            const bool isLinearMipmap  = isLinearMipmapFilter(sampler.minFilter);
            const int minTexLevel      = 0;
            const int maxTexLevel      = texture.getNumLevels() - 1;

            DE_ASSERT(minTexLevel < maxTexLevel);

            if (isLinearMipmap)
            {
                const int minLevel = de::clamp((int)deFloatFloor(minLod), minTexLevel, maxTexLevel - 1);
                const int maxLevel = de::clamp((int)deFloatFloor(maxLod), minTexLevel, maxTexLevel - 1);

                DE_ASSERT(minLevel <= maxLevel);

                for (int level = minLevel; level <= maxLevel; level++)
                {
                    const float minF = de::clamp(minLod - float(level), 0.0f, 1.0f);
                    const float maxF = de::clamp(maxLod - float(level), 0.0f, 1.0f);

                    if (isCubeMipmapLinearCompareResultValid(texture, level, sampler, getLevelFilter(sampler.minFilter),
                                                             prec, faceCoords, coord.w(), Vec2(minF, maxF),
                                                             cmpReference, result))
                        return true;
                }
            }
            else if (isNearestMipmap)
            {
                // \note The accurate formula for nearest mipmapping is level = ceil(lod + 0.5) - 1 but Khronos has made
                //         decision to allow floor(lod + 0.5) as well.
                const int minLevel = de::clamp((int)deFloatCeil(minLod + 0.5f) - 1, minTexLevel, maxTexLevel);
                const int maxLevel = de::clamp((int)deFloatFloor(maxLod + 0.5f), minTexLevel, maxTexLevel);

                DE_ASSERT(minLevel <= maxLevel);

                for (int level = minLevel; level <= maxLevel; level++)
                {
                    if (isCubeLevelCompareResultValid(texture, level, sampler, getLevelFilter(sampler.minFilter), prec,
                                                      faceCoords, coord.w(), cmpReference, result))
                        return true;
                }
            }
            else
            {
                if (isCubeLevelCompareResultValid(texture, 0, sampler, sampler.minFilter, prec, faceCoords, coord.w(),
                                                  cmpReference, result))
                    return true;
            }
        }
    }

    return false;
}

static bool isGatherOffsetsCompareResultValid(const ConstPixelBufferAccess &texture, const Sampler &sampler,
                                              const TexComparePrecision &prec, const Vec2 &coord, int coordZ,
                                              const IVec2 (&offsets)[4], float cmpReference, const Vec4 &result)
{
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(texture.getFormat());
    const Vec2 uBounds = computeNonNormalizedCoordBounds(sampler.normalizedCoords, texture.getWidth(), coord.x(),
                                                         prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 vBounds = computeNonNormalizedCoordBounds(sampler.normalizedCoords, texture.getHeight(), coord.y(),
                                                         prec.coordBits.y(), prec.uvwBits.y());

    // Integer coordinate bounds for (x0, y0) - without wrap mode
    const int minI = deFloorFloatToInt32(uBounds.x() - 0.5f);
    const int maxI = deFloorFloatToInt32(uBounds.y() - 0.5f);
    const int minJ = deFloorFloatToInt32(vBounds.x() - 0.5f);
    const int maxJ = deFloorFloatToInt32(vBounds.y() - 0.5f);

    const int w = texture.getWidth();
    const int h = texture.getHeight();

    for (int j = minJ; j <= maxJ; j++)
    {
        for (int i = minI; i <= maxI; i++)
        {
            bool isCurrentPixelValid = true;

            for (int offNdx = 0; offNdx < 4 && isCurrentPixelValid; offNdx++)
            {
                // offNdx-th coordinate offset and then wrapped.
                const int x       = wrap(sampler.wrapS, i + offsets[offNdx].x(), w);
                const int y       = wrap(sampler.wrapT, j + offsets[offNdx].y(), h);
                const float depth = lookupDepth(texture, sampler, x, y, coordZ);
                const CmpResultSet resSet =
                    execCompare(sampler.compare, depth, cmpReference, prec.referenceBits, isFixedPointDepth);

                if (!isResultInSet(resSet, result[offNdx], prec.resultBits))
                    isCurrentPixelValid = false;
            }

            if (isCurrentPixelValid)
                return true;
        }
    }

    return false;
}

bool isGatherOffsetsCompareResultValid(const Texture2DView &texture, const Sampler &sampler,
                                       const TexComparePrecision &prec, const Vec2 &coord, const IVec2 (&offsets)[4],
                                       float cmpReference, const Vec4 &result)
{
    DE_ASSERT(isSamplerSupported(sampler));

    return isGatherOffsetsCompareResultValid(texture.getLevel(0), sampler, prec, coord, 0, offsets, cmpReference,
                                             result);
}

bool isGatherOffsetsCompareResultValid(const Texture2DArrayView &texture, const Sampler &sampler,
                                       const TexComparePrecision &prec, const Vec3 &coord, const IVec2 (&offsets)[4],
                                       float cmpReference, const Vec4 &result)
{
    const float depthErr =
        computeFloatingPointError(coord.z(), prec.coordBits.z()) + computeFixedPointError(prec.uvwBits.z());
    const float minZ   = coord.z() - depthErr;
    const float maxZ   = coord.z() + depthErr;
    const int minLayer = de::clamp(deFloorFloatToInt32(minZ + 0.5f), 0, texture.getNumLayers() - 1);
    const int maxLayer = de::clamp(deFloorFloatToInt32(maxZ + 0.5f), 0, texture.getNumLayers() - 1);

    DE_ASSERT(isSamplerSupported(sampler));

    for (int layer = minLayer; layer <= maxLayer; layer++)
    {
        if (isGatherOffsetsCompareResultValid(texture.getLevel(0), sampler, prec, coord.swizzle(0, 1), layer, offsets,
                                              cmpReference, result))
            return true;
    }
    return false;
}

static bool isGatherCompareResultValid(const TextureCubeView &texture, const Sampler &sampler,
                                       const TexComparePrecision &prec, const CubeFaceFloatCoords &coords,
                                       float cmpReference, const Vec4 &result)
{
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(texture.getLevelFace(0, coords.face).getFormat());
    const int size               = texture.getLevelFace(0, coords.face).getWidth();
    const Vec2 uBounds =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, size, coords.s, prec.coordBits.x(), prec.uvwBits.x());
    const Vec2 vBounds =
        computeNonNormalizedCoordBounds(sampler.normalizedCoords, size, coords.t, prec.coordBits.y(), prec.uvwBits.y());

    // Integer coordinate bounds for (x0,y0) - without wrap mode
    const int minI = deFloorFloatToInt32(uBounds.x() - 0.5f);
    const int maxI = deFloorFloatToInt32(uBounds.y() - 0.5f);
    const int minJ = deFloorFloatToInt32(vBounds.x() - 0.5f);
    const int maxJ = deFloorFloatToInt32(vBounds.y() - 0.5f);

    // Face accesses
    ConstPixelBufferAccess faces[CUBEFACE_LAST];
    for (int face = 0; face < CUBEFACE_LAST; face++)
        faces[face] = texture.getLevelFace(0, CubeFace(face));

    for (int j = minJ; j <= maxJ; j++)
    {
        for (int i = minI; i <= maxI; i++)
        {
            static const IVec2 offsets[4] = {IVec2(0, 1), IVec2(1, 1), IVec2(1, 0), IVec2(0, 0)};

            bool isCurrentPixelValid = true;

            for (int offNdx = 0; offNdx < 4 && isCurrentPixelValid; offNdx++)
            {
                const CubeFaceIntCoords c = remapCubeEdgeCoords(
                    CubeFaceIntCoords(coords.face, i + offsets[offNdx].x(), j + offsets[offNdx].y()), size);
                // If any of samples is out of both edges, implementations can do pretty much anything according to spec.
                // \todo [2014-06-05 nuutti] Test the special case where all corner pixels have exactly the same color.
                //                             See also isSeamlessLinearCompareResultValid and similar.
                if (c.face == CUBEFACE_LAST)
                    return true;

                const float depth = lookupDepthNoBorder(faces[c.face], sampler, c.s, c.t);
                const CmpResultSet resSet =
                    execCompare(sampler.compare, depth, cmpReference, prec.referenceBits, isFixedPointDepth);

                if (!isResultInSet(resSet, result[offNdx], prec.resultBits))
                    isCurrentPixelValid = false;
            }

            if (isCurrentPixelValid)
                return true;
        }
    }

    return false;
}

bool isGatherCompareResultValid(const TextureCubeView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                                const Vec3 &coord, float cmpReference, const Vec4 &result)
{
    int numPossibleFaces = 0;
    CubeFace possibleFaces[CUBEFACE_LAST];

    DE_ASSERT(isSamplerSupported(sampler));

    getPossibleCubeFaces(coord, prec.coordBits, &possibleFaces[0], numPossibleFaces);

    if (numPossibleFaces == 0)
        return true; // Result is undefined.

    for (int tryFaceNdx = 0; tryFaceNdx < numPossibleFaces; tryFaceNdx++)
    {
        const CubeFaceFloatCoords faceCoords(possibleFaces[tryFaceNdx],
                                             projectToFace(possibleFaces[tryFaceNdx], coord));

        if (isGatherCompareResultValid(texture, sampler, prec, faceCoords, cmpReference, result))
            return true;
    }

    return false;
}

} // namespace tcu
