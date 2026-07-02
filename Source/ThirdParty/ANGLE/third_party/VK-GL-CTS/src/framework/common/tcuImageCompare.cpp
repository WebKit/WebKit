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
 * \brief Image comparison utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuImageCompare.hpp"
#include "tcuSurface.hpp"
#include "tcuFuzzyImageCompare.hpp"
#include "tcuBilinearImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuRGBA.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuFloat.hpp"

#include <cmath>

namespace tcu
{

namespace
{

void computeScaleAndBias(const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                         tcu::Vec4 &scale, tcu::Vec4 &bias)
{
    Vec4 minVal;
    Vec4 maxVal;
    const float eps = 0.0001f;

    {
        Vec4 refMin;
        Vec4 refMax;
        estimatePixelValueRange(reference, refMin, refMax);

        minVal = refMin;
        maxVal = refMax;
    }

    {
        Vec4 resMin;
        Vec4 resMax;

        estimatePixelValueRange(result, resMin, resMax);

        minVal[0] = de::min(minVal[0], resMin[0]);
        minVal[1] = de::min(minVal[1], resMin[1]);
        minVal[2] = de::min(minVal[2], resMin[2]);
        minVal[3] = de::min(minVal[3], resMin[3]);

        maxVal[0] = de::max(maxVal[0], resMax[0]);
        maxVal[1] = de::max(maxVal[1], resMax[1]);
        maxVal[2] = de::max(maxVal[2], resMax[2]);
        maxVal[3] = de::max(maxVal[3], resMax[3]);
    }

    for (int c = 0; c < 4; c++)
    {
        if (maxVal[c] - minVal[c] < eps)
        {
            scale[c] = (maxVal[c] < eps) ? 1.0f : (1.0f / maxVal[c]);
            bias[c]  = (c == 3) ? (1.0f - maxVal[c] * scale[c]) : (0.0f - minVal[c] * scale[c]);
        }
        else
        {
            scale[c] = 1.0f / (maxVal[c] - minVal[c]);
            bias[c]  = 0.0f - minVal[c] * scale[c];
        }
    }
}

static int findNumPositionDeviationFailingPixels(const PixelBufferAccess &errorMask,
                                                 const ConstPixelBufferAccess &reference,
                                                 const ConstPixelBufferAccess &result, const UVec4 &threshold,
                                                 const tcu::IVec3 &maxPositionDeviation,
                                                 bool acceptOutOfBoundsAsAnyValue)
{
    const tcu::IVec4 okColor(0, 255, 0, 255);
    const tcu::IVec4 errorColor(255, 0, 0, 255);
    const int width      = reference.getWidth();
    const int height     = reference.getHeight();
    const int depth      = reference.getDepth();
    int numFailingPixels = 0;

    // Accept pixels "sampling" over the image bounds pixels since "taps" could be anything
    const int beginX = (acceptOutOfBoundsAsAnyValue) ? (maxPositionDeviation.x()) : (0);
    const int beginY = (acceptOutOfBoundsAsAnyValue) ? (maxPositionDeviation.y()) : (0);
    const int beginZ = (acceptOutOfBoundsAsAnyValue) ? (maxPositionDeviation.z()) : (0);
    const int endX   = (acceptOutOfBoundsAsAnyValue) ? (width - maxPositionDeviation.x()) : (width);
    const int endY   = (acceptOutOfBoundsAsAnyValue) ? (height - maxPositionDeviation.y()) : (height);
    const int endZ   = (acceptOutOfBoundsAsAnyValue) ? (depth - maxPositionDeviation.z()) : (depth);

    TCU_CHECK_INTERNAL(result.getWidth() == width && result.getHeight() == height && result.getDepth() == depth);
    DE_ASSERT(endX > 0 && endY > 0 && endZ > 0); // most likely a bug

    tcu::clear(errorMask, okColor);

    for (int z = beginZ; z < endZ; z++)
    {
        for (int y = beginY; y < endY; y++)
        {
            for (int x = beginX; x < endX; x++)
            {
                const IVec4 refPix = reference.getPixelInt(x, y, z);
                const IVec4 cmpPix = result.getPixelInt(x, y, z);

                // Exact match
                {
                    const UVec4 diff = abs(refPix - cmpPix).cast<uint32_t>();
                    const bool isOk  = boolAll(lessThanEqual(diff, threshold));

                    if (isOk)
                        continue;
                }

                // Find matching pixels for both result and reference pixel

                {
                    bool pixelFoundForReference = false;

                    // Find deviated result pixel for reference

                    for (int sz = de::max(0, z - maxPositionDeviation.z());
                         sz <= de::min(depth - 1, z + maxPositionDeviation.z()) && !pixelFoundForReference; ++sz)
                        for (int sy = de::max(0, y - maxPositionDeviation.y());
                             sy <= de::min(height - 1, y + maxPositionDeviation.y()) && !pixelFoundForReference; ++sy)
                            for (int sx = de::max(0, x - maxPositionDeviation.x());
                                 sx <= de::min(width - 1, x + maxPositionDeviation.x()) && !pixelFoundForReference;
                                 ++sx)
                            {
                                const IVec4 deviatedCmpPix = result.getPixelInt(sx, sy, sz);
                                const UVec4 diff           = abs(refPix - deviatedCmpPix).cast<uint32_t>();
                                const bool isOk            = boolAll(lessThanEqual(diff, threshold));

                                pixelFoundForReference = isOk;
                            }

                    if (!pixelFoundForReference)
                    {
                        errorMask.setPixel(errorColor, x, y, z);
                        ++numFailingPixels;
                        continue;
                    }
                }
                {
                    bool pixelFoundForResult = false;

                    // Find deviated reference pixel for result

                    for (int sz = de::max(0, z - maxPositionDeviation.z());
                         sz <= de::min(depth - 1, z + maxPositionDeviation.z()) && !pixelFoundForResult; ++sz)
                        for (int sy = de::max(0, y - maxPositionDeviation.y());
                             sy <= de::min(height - 1, y + maxPositionDeviation.y()) && !pixelFoundForResult; ++sy)
                            for (int sx = de::max(0, x - maxPositionDeviation.x());
                                 sx <= de::min(width - 1, x + maxPositionDeviation.x()) && !pixelFoundForResult; ++sx)
                            {
                                const IVec4 deviatedRefPix = reference.getPixelInt(sx, sy, sz);
                                const UVec4 diff           = abs(cmpPix - deviatedRefPix).cast<uint32_t>();
                                const bool isOk            = boolAll(lessThanEqual(diff, threshold));

                                pixelFoundForResult = isOk;
                            }

                    if (!pixelFoundForResult)
                    {
                        errorMask.setPixel(errorColor, x, y, z);
                        ++numFailingPixels;
                        continue;
                    }
                }
            }
        }
    }

    return numFailingPixels;
}

} // namespace

/*--------------------------------------------------------------------*//*!
 * \brief Fuzzy image comparison
 *
 * This image comparison is designed for comparing images rendered by 3D
 * graphics APIs such as OpenGL. The comparison allows small local differences
 * and compensates for aliasing.
 *
 * The algorithm first performs light blurring on both images and then
 * does per-pixel analysis. Pixels are compared to 3x3 bilinear surface
 * defined by adjecent pixels. This compensates for both 1-pixel deviations
 * in geometry and aliasing in texture data.
 *
 * Error metric is computed based on the differences. On valid images the
 * metric is usually <0.01. Thus good threshold values are in range 0.02 to
 * 0.05.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \note                Currently supports only UNORM_INT8 formats
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Error metric threshold (good values are 0.02-0.05)
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool fuzzyCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                  const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result, float threshold,
                  CompareLogMode logMode)
{
    FuzzyCompareParams params; // Use defaults.
    TextureLevel errorMask(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), reference.getWidth(),
                           reference.getHeight());
    float difference = fuzzyCompare(params, reference, result, errorMask.getAccess());
    bool isOk        = difference <= threshold;
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    if (!isOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        // Generate more accurate error mask.
        params.maxSampleSkip = 0;
        fuzzyCompare(params, reference, result, errorMask.getAccess());

        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8) &&
            reference.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computeScaleAndBias(reference, result, pixelScale, pixelBias);

        if (!isOk)
            log << TestLog::Message << "Image comparison failed: difference = " << difference
                << ", threshold = " << threshold << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
            << TestLog::Image("Reference", "Reference", reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", "Error mask", errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", "Result", result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return isOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel bitwise comparison
 *
 * This compare expects bit precision between result and reference image.
 * Comparison fails if any pixels do not match bitwise.
 * Reference and result format must match.
 *
 * This comparison can be used for any type texture formats since it does
 * not care about types.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool bitwiseCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                    const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                    CompareLogMode logMode)
{
    int width  = reference.getWidth();
    int height = reference.getHeight();
    int depth  = reference.getDepth();
    TCU_CHECK_INTERNAL(result.getWidth() == width && result.getHeight() == height && result.getDepth() == depth);

    // Enforce texture has same channel count and channel size
    TCU_CHECK_INTERNAL(reference.getFormat() == result.getFormat());
    result.getPixelPitch();

    TextureLevel errorMaskStorage(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);
    bool compareOk = true;

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                const U64Vec4 refPix = reference.getPixelBitsAsUint64(x, y, z);
                const U64Vec4 cmpPix = result.getPixelBitsAsUint64(x, y, z);
                const bool isOk      = (refPix == cmpPix);

                errorMask.setPixel(isOk ? IVec4(0, 0xff, 0, 0xff) : IVec4(0xff, 0, 0, 0xff), x, y, z);
                compareOk &= isOk;
            }
        }
    }

    if (!compareOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        {
            const auto refChannelClass = tcu::getTextureChannelClass(reference.getFormat().type);
            const auto resChannelClass = tcu::getTextureChannelClass(result.getFormat().type);

            const bool refIsUint8 = (reference.getFormat().type == TextureFormat::UNSIGNED_INT8);
            const bool resIsUint8 = (result.getFormat().type == TextureFormat::UNSIGNED_INT8);

            const bool calcScaleBias =
                ((refChannelClass != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT && !refIsUint8) ||
                 (resChannelClass != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT && !resIsUint8));

            // All formats except normalized unsigned fixed point ones need remapping in order to fit into unorm channels in logged images.
            if (calcScaleBias)
            {
                computeScaleAndBias(reference, result, pixelScale, pixelBias);
                log << TestLog::Message << "Result and reference images are normalized with formula p * " << pixelScale
                    << " + " << pixelBias << TestLog::EndMessage;
            }
        }

        if (!compareOk)
            log << TestLog::Message
                << "Image comparison failed: Pixels with different values were found when bitwise precision is expected"
                << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
            << TestLog::Image("Reference", "Reference", reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", "Error mask", errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", "Result", result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return compareOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Fuzzy image comparison using maximum error
 *
 * Variant of fuzzyCompare that uses the maximum error per pixel as a metric,
 * instead of accumulating errors for the entire image.
 *
 * The minimum possible error is sqrt(1)/255 (~0.0039), corresponding to a
 * 5-shade difference (1 over MIN_ERR_THRESHOLD) on one channel.
 *
 * The maximum possible error is sqrt(4*(251^2))/255 (~1.96), corresponding
 * to a 255-shade difference on all four channels.
 *
 * This image comparison is designed for comparing images rendered by 3D
 * graphics APIs such as OpenGL. The comparison allows small local differences
 * and compensates for aliasing.
 *
 * The algorithm first performs light blurring on both images and then
 * does per-pixel analysis. Pixels are compared to 3x3 bilinear surface
 * defined by adjecent pixels. This compensates for both 1-pixel deviations
 * in geometry and aliasing in texture data.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \note                Currently supports only UNORM_INT8 formats
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Error metric threshold
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool fuzzyCompareMaxError(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                          const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                          float threshold, CompareLogMode logMode)
{
    FuzzyCompareParams params(8, true);
    TextureLevel errorMask(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), reference.getWidth(),
                           reference.getHeight());
    float difference = fuzzyCompare(params, reference, result, errorMask.getAccess());
    bool isOk        = difference <= threshold;
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    if (!isOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        // Generate more accurate error mask.
        params.maxSampleSkip = 0;
        fuzzyCompare(params, reference, result, errorMask.getAccess());

        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8) &&
            reference.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computeScaleAndBias(reference, result, pixelScale, pixelBias);

        if (!isOk)
            log << TestLog::Message << "Image comparison failed: difference = " << difference
                << ", threshold = " << threshold << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
            << TestLog::Image("Reference", "Reference", reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", "Error mask", errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", "Result", result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return isOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Fuzzy image comparison
 *
 * This image comparison is designed for comparing images rendered by 3D
 * graphics APIs such as OpenGL. The comparison allows small local differences
 * and compensates for aliasing.
 *
 * The algorithm first performs light blurring on both images and then
 * does per-pixel analysis. Pixels are compared to 3x3 bilinear surface
 * defined by adjecent pixels. This compensates for both 1-pixel deviations
 * in geometry and aliasing in texture data.
 *
 * Error metric is computed based on the differences. On valid images the
 * metric is usually <0.01. Thus good threshold values are in range 0.02 to
 * 0.05.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \note                Currently supports only UNORM_INT8 formats
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Error metric threshold (good values are 0.02-0.05)
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool fuzzyCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Surface &reference,
                  const Surface &result, float threshold, CompareLogMode logMode)
{
    return fuzzyCompare(log, imageSetName, imageSetDesc, reference.getAccess(), result.getAccess(), threshold, logMode);
}

/*--------------------------------------------------------------------*//*!
 * \brief Fuzzy image comparison using maximum error
 *
 * Variant of fuzzyCompare that uses the maximum error per pixel as a metric,
 * instead of accumulating errors for the entire image.
 *
 * The minimum possible error is sqrt(1)/255 (~0.0039), corresponding to a
 * 5-shade difference (1 over MIN_ERR_THRESHOLD) on one channel.
 *
 * The maximum possible error is sqrt(4*(251^2))/255 (~1.96), corresponding
 * to a 255-shade difference on all four channels.
 *
 * This image comparison is designed for comparing images rendered by 3D
 * graphics APIs such as OpenGL. The comparison allows small local differences
 * and compensates for aliasing.
 *
 * The algorithm first performs light blurring on both images and then
 * does per-pixel analysis. Pixels are compared to 3x3 bilinear surface
 * defined by adjecent pixels. This compensates for both 1-pixel deviations
 * in geometry and aliasing in texture data.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \note                Currently supports only UNORM_INT8 formats
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Error metric threshold
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool fuzzyCompareMaxError(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Surface &reference,
                          const Surface &result, float threshold, CompareLogMode logMode)
{
    return fuzzyCompareMaxError(log, imageSetName, imageSetDesc, reference.getAccess(), result.getAccess(), threshold,
                                logMode);
}

static int64_t computeSquaredDiffSum(const ConstPixelBufferAccess &ref, const ConstPixelBufferAccess &cmp,
                                     const PixelBufferAccess &diffMask, int diffFactor)
{
    TCU_CHECK_INTERNAL(ref.getFormat().type == TextureFormat::UNORM_INT8 &&
                       cmp.getFormat().type == TextureFormat::UNORM_INT8);
    DE_ASSERT(ref.getWidth() == cmp.getWidth() && ref.getWidth() == diffMask.getWidth());
    DE_ASSERT(ref.getHeight() == cmp.getHeight() && ref.getHeight() == diffMask.getHeight());

    int64_t diffSum = 0;

    for (int y = 0; y < cmp.getHeight(); y++)
    {
        for (int x = 0; x < cmp.getWidth(); x++)
        {
            IVec4 a    = ref.getPixelInt(x, y);
            IVec4 b    = cmp.getPixelInt(x, y);
            IVec4 diff = abs(a - b);
            int sum    = diff.x() + diff.y() + diff.z() + diff.w();
            int sqSum  = diff.x() * diff.x() + diff.y() * diff.y() + diff.z() * diff.z() + diff.w() * diff.w();

            diffMask.setPixel(
                tcu::RGBA(deClamp32(sum * diffFactor, 0, 255), deClamp32(255 - sum * diffFactor, 0, 255), 0, 255)
                    .toVec(),
                x, y);

            diffSum += (int64_t)sqSum;
        }
    }

    return diffSum;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel difference accuracy metric
 *
 * Computes accuracy metric using per-pixel differences between reference
 * and result images.
 *
 * \note                    Supports only integer- and fixed-point formats
 * \param log                Test log for results
 * \param imageSetName        Name for image set when logging results
 * \param imageSetDesc        Description for image set
 * \param reference            Reference image
 * \param result            Result image
 * \param bestScoreDiff        Scaling factor
 * \param worstScoreDiff    Scaling factor
 * \param logMode            Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
int measurePixelDiffAccuracy(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                             const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                             int bestScoreDiff, int worstScoreDiff, CompareLogMode logMode)
{
    TextureLevel diffMask(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), reference.getWidth(),
                          reference.getHeight());
    int diffFactor     = 8;
    int64_t squaredSum = computeSquaredDiffSum(reference, result, diffMask.getAccess(), diffFactor);
    float sum          = deFloatSqrt((float)squaredSum);
    int score          = deClamp32(
        deFloorFloatToInt32(
            100.0f - (de::max(sum - (float)bestScoreDiff, 0.0f) / (float)(worstScoreDiff - bestScoreDiff)) * 100.0f),
        0, 100);
    const int failThreshold = 10;
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    if (logMode == COMPARE_LOG_EVERYTHING || log.logAllImages() || score <= failThreshold)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8) &&
            reference.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computeScaleAndBias(reference, result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
            << TestLog::Image("Reference", "Reference", reference, pixelScale, pixelBias)
            << TestLog::Image("DiffMask", "Difference", diffMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", "Result", result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    if (logMode != COMPARE_LOG_ON_ERROR || score <= failThreshold)
        log << TestLog::Integer("DiffSum", "Squared difference sum", "", QP_KEY_TAG_NONE, squaredSum)
            << TestLog::Integer("Score", "Score", "", QP_KEY_TAG_QUALITY, score);

    return score;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel difference accuracy metric
 *
 * Computes accuracy metric using per-pixel differences between reference
 * and result images.
 *
 * \note                    Supports only integer- and fixed-point formats
 * \param log                Test log for results
 * \param imageSetName        Name for image set when logging results
 * \param imageSetDesc        Description for image set
 * \param reference            Reference image
 * \param result            Result image
 * \param bestScoreDiff        Scaling factor
 * \param worstScoreDiff    Scaling factor
 * \param logMode            Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
int measurePixelDiffAccuracy(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Surface &reference,
                             const Surface &result, int bestScoreDiff, int worstScoreDiff, CompareLogMode logMode)
{
    return measurePixelDiffAccuracy(log, imageSetName, imageSetDesc, reference.getAccess(), result.getAccess(),
                                    bestScoreDiff, worstScoreDiff, logMode);
}

/*--------------------------------------------------------------------*//*!
 * Returns the index of float in a float space without denormals
 * so that:
 * 1) f(0.0) = 0
 * 2) f(-0.0) = 0
 * 3) f(b) = f(a) + 1  <==>  b = nextAfter(a)
 *
 * See computeFloatFlushRelaxedULPDiff for details
 *//*--------------------------------------------------------------------*/
static int32_t getPositionOfIEEEFloatWithoutDenormals(float x)
{
    DE_ASSERT(!std::isnan(x)); // not valid

    if (x == 0.0f)
        return 0;
    else if (x < 0.0f)
        return -getPositionOfIEEEFloatWithoutDenormals(-x);
    else
    {
        DE_ASSERT(x > 0.0f);

        const tcu::Float32 f(x);

        if (f.isDenorm())
        {
            // Denorms are flushed to zero
            return 0;
        }
        else
        {
            // sign is 0, and it's a normal number. Natural position is its bit
            // pattern but since we've collapsed the denorms, we must remove
            // the gap here too to keep the float enumeration continuous.
            //
            // Denormals occupy one exponent pattern. Removing one from
            // exponent should to the trick. Add one since the removed range
            // contained one representable value, 0.
            return (int32_t)(f.bits() - (1u << 23u) + 1u);
        }
    }
}

static uint32_t computeFloatFlushRelaxedULPDiff(float a, float b)
{
    if (std::isnan(a) && std::isnan(b))
        return 0;
    else if (std::isnan(a) || std::isnan(b))
    {
        return 0xFFFFFFFFu;
    }
    else
    {
        // Using the "definition 5" in Muller, Jean-Michel. "On the definition of ulp (x)" (2005)
        // assuming a floating point space is IEEE single precision floating point space without
        // denormals (and signed zeros).
        const int32_t aIndex = getPositionOfIEEEFloatWithoutDenormals(a);
        const int32_t bIndex = getPositionOfIEEEFloatWithoutDenormals(b);
        return (uint32_t)de::abs(aIndex - bIndex);
    }
}

static tcu::UVec4 computeFlushRelaxedULPDiff(const tcu::Vec4 &a, const tcu::Vec4 &b)
{
    return tcu::UVec4(computeFloatFlushRelaxedULPDiff(a.x(), b.x()), computeFloatFlushRelaxedULPDiff(a.y(), b.y()),
                      computeFloatFlushRelaxedULPDiff(a.z(), b.z()), computeFloatFlushRelaxedULPDiff(a.w(), b.w()));
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel threshold-based comparison
 *
 * This compare computes per-pixel differences between result and reference
 * image. Comparison fails if any pixels exceed the given threshold value.
 *
 * This comparison uses ULP (units in last place) metric for computing the
 * difference between floating-point values and thus this function can
 * be used only for comparing floating-point texture data. In ULP calculation
 * the denormal numbers are allowed to be flushed to zero.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Maximum allowed difference
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool floatUlpThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                              const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                              const UVec4 &threshold, CompareLogMode logMode)
{
    int width  = reference.getWidth();
    int height = reference.getHeight();
    int depth  = reference.getDepth();
    TextureLevel errorMaskStorage(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    UVec4 maxDiff(0, 0, 0, 0);
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    TCU_CHECK(result.getWidth() == width && result.getHeight() == height && result.getDepth() == depth);

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                const Vec4 refPix = reference.getPixel(x, y, z);
                const Vec4 cmpPix = result.getPixel(x, y, z);
                const UVec4 diff  = computeFlushRelaxedULPDiff(refPix, cmpPix);
                const bool isOk   = boolAll(lessThanEqual(diff, threshold));

                maxDiff = max(maxDiff, diff);

                errorMask.setPixel(isOk ? Vec4(0.0f, 1.0f, 0.0f, 1.0f) : Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y, z);
            }
        }
    }

    bool compareOk = boolAll(lessThanEqual(maxDiff, threshold));

    if (!compareOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        // All formats except normalized unsigned fixed point ones need remapping in order to fit into unorm channels in logged images.
        if (tcu::getTextureChannelClass(reference.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
            tcu::getTextureChannelClass(result.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
        {
            computeScaleAndBias(reference, result, pixelScale, pixelBias);
            log << TestLog::Message << "Result and reference images are normalized with formula p * " << pixelScale
                << " + " << pixelBias << TestLog::EndMessage;
        }

        if (!compareOk)
            log << TestLog::Message << "Image comparison failed: max difference = " << maxDiff
                << ", threshold = " << threshold << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias)
            << TestLog::Image("Reference", imageSetName, reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", imageSetName, errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return compareOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel threshold-based comparison
 *
 * This compare computes per-pixel differences between result and reference
 * image. Comparison fails if any pixels exceed the given threshold value.
 *
 * This comparison can be used for floating-point and fixed-point formats.
 * Difference is computed in floating-point space.
 *
 * On failure an error image is generated that shows where the failing
 * pixels are.
 *
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Maximum allowed difference
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool floatThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                           const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                           const Vec4 &threshold, CompareLogMode logMode)
{
    int width  = reference.getWidth();
    int height = reference.getHeight();
    int depth  = reference.getDepth();
    TextureLevel errorMaskStorage(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    Vec4 maxDiff(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    TCU_CHECK_INTERNAL(result.getWidth() == width && result.getHeight() == height && result.getDepth() == depth);

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                Vec4 refPix = reference.getPixel(x, y, z);
                Vec4 cmpPix = result.getPixel(x, y, z);

                Vec4 diff = abs(refPix - cmpPix);
                bool isOk = boolAll(lessThanEqual(diff, threshold));

                maxDiff = max(maxDiff, diff);

                if (isOk)
                    errorMask.setPixel(Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y, z);
                else
                    errorMask.setPixel(Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y, z);
            }
        }
    }

    bool compareOk = boolAll(lessThanEqual(maxDiff, threshold));

    if (!compareOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        // All formats except normalized unsigned fixed point ones need remapping in order to fit into unorm channels in logged images.
        if (tcu::getTextureChannelClass(reference.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
            tcu::getTextureChannelClass(result.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
        {
            computeScaleAndBias(reference, result, pixelScale, pixelBias);
            log << TestLog::Message << "Result and reference images are normalized with formula p * " << pixelScale
                << " + " << pixelBias << TestLog::EndMessage;
        }

        if (!compareOk)
            log << TestLog::Message << "Image comparison failed: max difference = " << maxDiff
                << ", threshold = " << threshold << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias)
            << TestLog::Image("Reference", imageSetName, reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", imageSetName, errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return compareOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel threshold-based comparison with ignore key
 *
 * This compare computes per-pixel differences between result and reference
 * image. Comparison fails if any pixels exceed the given threshold value.
 *
 * Any pixels in reference that match the ignore key are ignored.
 *
 * This comparison can be used for floating-point and fixed-point formats.
 * Difference is computed in floating-point space.
 *
 * On failure an error image is generated that shows where the failing
 * pixels are.
 *
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param ignorekey     Ignore key
 * \param threshold        Maximum allowed difference
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool floatThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                           const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                           const Vec4 &ignorekey, const Vec4 &threshold, CompareLogMode logMode)
{
    int width  = reference.getWidth();
    int height = reference.getHeight();
    int depth  = reference.getDepth();
    TextureLevel errorMaskStorage(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    Vec4 maxDiff(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    TCU_CHECK_INTERNAL(result.getWidth() == width && result.getHeight() == height && result.getDepth() == depth);

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                Vec4 refPix = reference.getPixel(x, y, z);
                Vec4 cmpPix = result.getPixel(x, y, z);

                if (refPix != ignorekey)
                {

                    Vec4 diff = abs(refPix - cmpPix);
                    bool isOk = boolAll(lessThanEqual(diff, threshold));

                    maxDiff = max(maxDiff, diff);

                    errorMask.setPixel(isOk ? Vec4(0.0f, 1.0f, 0.0f, 1.0f) : Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y, z);
                }
            }
        }
    }

    bool compareOk = boolAll(lessThanEqual(maxDiff, threshold));

    if (!compareOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        // All formats except normalized unsigned fixed point ones need remapping in order to fit into unorm channels in logged images.
        if (tcu::getTextureChannelClass(reference.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
            tcu::getTextureChannelClass(result.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
        {
            computeScaleAndBias(reference, result, pixelScale, pixelBias);
            log << TestLog::Message << "Result and reference images are normalized with formula p * " << pixelScale
                << " + " << pixelBias << TestLog::EndMessage;
        }

        if (!compareOk)
            log << TestLog::Message << "Image comparison failed: max difference = " << maxDiff
                << ", threshold = " << threshold << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias)
            << TestLog::Image("Reference", imageSetName, reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", imageSetName, errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return compareOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel threshold-based comparison
 *
 * This compare computes per-pixel differences between result and reference
 * color. Comparison fails if any pixels exceed the given threshold value.
 *
 * This comparison can be used for floating-point and fixed-point formats.
 * Difference is computed in floating-point space.
 *
 * On failure an error image is generated that shows where the failing
 * pixels are.
 *
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference color
 * \param result        Result image
 * \param threshold        Maximum allowed difference
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool floatThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Vec4 &reference,
                           const ConstPixelBufferAccess &result, const Vec4 &threshold, CompareLogMode logMode)
{
    const int width  = result.getWidth();
    const int height = result.getHeight();
    const int depth  = result.getDepth();

    TextureLevel errorMaskStorage(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    Vec4 maxDiff(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                const Vec4 cmpPix = result.getPixel(x, y, z);
                const Vec4 diff   = abs(reference - cmpPix);
                const bool isOk   = boolAll(lessThanEqual(diff, threshold));

                maxDiff = max(maxDiff, diff);

                if (isOk)
                    errorMask.setPixel(Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y, z);
                else
                    errorMask.setPixel(Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y, z);
            }
        }
    }

    bool compareOk = boolAll(lessThanEqual(maxDiff, threshold));

    if (!compareOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        // All formats except normalized unsigned fixed point ones need remapping in order to fit into unorm channels in logged images.
        if (tcu::getTextureChannelClass(result.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
        {
            computeScaleAndBias(result, result, pixelScale, pixelBias);
            log << TestLog::Message << "Result image is normalized with formula p * " << pixelScale << " + "
                << pixelBias << TestLog::EndMessage;
        }

        if (!compareOk)
            log << TestLog::Message << "Image comparison failed: max difference = " << maxDiff
                << ", threshold = " << threshold << ", reference = " << reference << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", imageSetName, errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return compareOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel threshold-based comparison
 *
 * This compare computes per-pixel differences between result and reference
 * image. Comparison fails if any pixels exceed the given threshold value.
 *
 * This comparison can be used for integer- and fixed-point texture formats.
 * Difference is computed in integer space.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Maximum allowed difference
 * \param logMode        Logging mode
 * \param use64Bits        Use 64-bit components when reading image data.
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool intThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                         const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                         const UVec4 &threshold, CompareLogMode logMode, bool use64Bits)
{
    int width  = reference.getWidth();
    int height = reference.getHeight();
    int depth  = reference.getDepth();
    TextureLevel errorMaskStorage(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    U64Vec4 maxDiff(0u, 0u, 0u, 0u);
    U64Vec4 diff(0u, 0u, 0u, 0u);
    const U64Vec4 threshold64 = threshold.cast<uint64_t>();
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    I64Vec4 refPix64;
    I64Vec4 cmpPix64;
    IVec4 refPix;
    IVec4 cmpPix;

    TCU_CHECK_INTERNAL(result.getWidth() == width && result.getHeight() == height && result.getDepth() == depth);

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (use64Bits)
                {
                    refPix64 = reference.getPixelInt64(x, y, z);
                    cmpPix64 = result.getPixelInt64(x, y, z);
                    diff     = abs(refPix64 - cmpPix64).cast<uint64_t>();
                }
                else
                {
                    refPix = reference.getPixelInt(x, y, z);
                    cmpPix = result.getPixelInt(x, y, z);
                    diff   = abs(refPix - cmpPix).cast<uint64_t>();
                }

                maxDiff = max(maxDiff, diff);

                const bool isOk = boolAll(lessThanEqual(diff, threshold64));
                if (isOk)
                    errorMask.setPixel(IVec4(0, 0xff, 0, 0xff), x, y, z);
                else
                    errorMask.setPixel(IVec4(0xff, 0, 0, 0xff), x, y, z);
            }
        }
    }

    bool compareOk = boolAll(lessThanEqual(maxDiff, threshold64));

    if (!compareOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        {
            const auto refChannelClass = tcu::getTextureChannelClass(reference.getFormat().type);
            const auto resChannelClass = tcu::getTextureChannelClass(result.getFormat().type);

            const bool refIsUint8 = (reference.getFormat().type == TextureFormat::UNSIGNED_INT8);
            const bool resIsUint8 = (result.getFormat().type == TextureFormat::UNSIGNED_INT8);

            const bool calcScaleBias =
                ((refChannelClass != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT && !refIsUint8) ||
                 (resChannelClass != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT && !resIsUint8));

            // All formats except normalized unsigned fixed point ones need remapping in order to fit into unorm channels in logged images.
            if (calcScaleBias)
            {
                computeScaleAndBias(reference, result, pixelScale, pixelBias);
                log << TestLog::Message << "Result and reference images are normalized with formula p * " << pixelScale
                    << " + " << pixelBias << TestLog::EndMessage;
            }
        }

        if (!compareOk)
            log << TestLog::Message << "Image comparison failed: max difference = " << maxDiff
                << ", threshold = " << threshold << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias)
            << TestLog::Image("Reference", imageSetName, reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", imageSetName, errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return compareOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel depth/stencil threshold-based comparison
 *
 * This compare computes per-pixel differences between result and reference
 * image. Comparison fails if any pixels exceed the given threshold value.
 *
 * This comparison can be used for depth and depth/stencil images.
 * Difference is computed in integer space.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Maximum allowed depth difference (stencil must be exact)
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool dsThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                        const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                        const float threshold, CompareLogMode logMode)
{
    int width  = reference.getWidth();
    int height = reference.getHeight();
    int depth  = reference.getDepth();
    TextureLevel errorLevelDepth(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    TextureLevel errorLevelStencil(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    PixelBufferAccess errorMaskDepth   = errorLevelDepth.getAccess();
    PixelBufferAccess errorMaskStencil = errorLevelStencil.getAccess();
    float maxDiff                      = 0.0;
    bool allStencilOk                  = true;
    bool hasDepth                      = tcu::hasDepthComponent(result.getFormat().order);
    bool hasStencil                    = tcu::hasStencilComponent(result.getFormat().order);

    TCU_CHECK_INTERNAL(result.getWidth() == width && result.getHeight() == height && result.getDepth() == depth);

    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (hasDepth)
                {
                    float refDepth = reference.getPixDepth(x, y, z);
                    float cmpDepth = result.getPixDepth(x, y, z);
                    float diff     = de::abs(refDepth - cmpDepth);

                    const bool depthOk = (diff <= threshold);
                    maxDiff            = (float)deMax(maxDiff, diff);

                    if (depthOk)
                        errorMaskDepth.setPixel(Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y, z);
                    else
                        errorMaskDepth.setPixel(Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y, z);
                }

                if (hasStencil)
                {
                    uint8_t refStencil = (uint8_t)reference.getPixStencil(x, y, z);
                    uint8_t cmpStencil = (uint8_t)result.getPixStencil(x, y, z);

                    const bool isStencilOk = (refStencil == cmpStencil);

                    if (isStencilOk)
                        errorMaskStencil.setPixel(Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y, z);
                    else
                        errorMaskStencil.setPixel(Vec4(1.0f, 0.0f, 0.0f, 1.0f), x, y, z);

                    allStencilOk = allStencilOk && isStencilOk;
                }
            }
        }
    }

    const bool allDepthOk    = (!hasDepth || (maxDiff <= threshold));
    const bool compareOk     = allDepthOk && allStencilOk;
    const bool logEverything = (logMode == COMPARE_LOG_EVERYTHING || log.logAllImages());

    if (!compareOk || logEverything)
    {
        if (!compareOk)
        {
            if (maxDiff > threshold)
                log << TestLog::Message << "Depth comparison failed: max difference = " << maxDiff
                    << ", threshold = " << threshold << TestLog::EndMessage;
            if (!allStencilOk)
                log << TestLog::Message << "Stencil comparison failed" << TestLog::EndMessage;
        }

        log << TestLog::ImageSet(imageSetName, imageSetDesc);

        if (!allDepthOk || (logEverything && hasDepth))
        {
            TextureLevel refDepthLevel(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height,
                                       depth);
            TextureLevel resDepthLevel(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height,
                                       depth);
            tcu::PixelBufferAccess refDepthAccess = refDepthLevel.getAccess();
            tcu::PixelBufferAccess resDepthAccess = resDepthLevel.getAccess();

            for (int z = 0; z < depth; z++)
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                    {
                        const float refDepth = reference.getPixDepth(x, y, z);
                        const float resDepth = result.getPixDepth(x, y, z);
                        refDepthAccess.setPixel(Vec4(refDepth, refDepth, refDepth, 1.0f), x, y, z);
                        resDepthAccess.setPixel(Vec4(resDepth, resDepth, resDepth, 1.0f), x, y, z);
                    }

            log << TestLog::Image("ResultDepth", imageSetName, resDepthAccess)
                << TestLog::Image("ReferenceDepth", imageSetName, refDepthAccess)
                << TestLog::Image("ErrorMaskDepth", imageSetName, errorMaskDepth);
        }

        if (!allStencilOk || (logEverything && hasStencil))
        {
            TextureLevel refStencilLevel(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height,
                                         depth);
            TextureLevel resStencilLevel(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height,
                                         depth);
            tcu::PixelBufferAccess refStencilAccess = refStencilLevel.getAccess();
            tcu::PixelBufferAccess resStencilAccess = resStencilLevel.getAccess();

            for (int z = 0; z < depth; z++)
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                    {
                        const float refStencil = static_cast<float>(reference.getPixStencil(x, y, z)) / 255.0f;
                        const float resStencil = static_cast<float>(result.getPixStencil(x, y, z)) / 255.0f;
                        refStencilAccess.setPixel(Vec4(refStencil, refStencil, refStencil, 1.0f), x, y, z);
                        resStencilAccess.setPixel(Vec4(resStencil, resStencil, resStencil, 1.0f), x, y, z);
                    }

            log << TestLog::Image("ResultStencil", imageSetName, resStencilAccess)
                << TestLog::Image("ReferenceStencil", imageSetName, refStencilAccess)
                << TestLog::Image("ErrorMaskStencil", imageSetName, errorMaskStencil);
        }

        log << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
#if 0
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias)
            << TestLog::EndImageSet;
#endif
    }

    return compareOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel threshold-based deviation-ignoring comparison
 *
 * This compare computes per-pixel differences between result and reference
 * image. Comparison fails if there is no pixel matching the given threshold
 * value in the search volume.
 *
 * If the search volume contains out-of-bounds pixels, comparison can be set
 * to either ignore these pixels in search or to accept any pixel that has
 * out-of-bounds pixels in its search volume.
 *
 * This comparison can be used for integer- and fixed-point texture formats.
 * Difference is computed in integer space.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \param log                            Test log for results
 * \param imageSetName                    Name for image set when logging results
 * \param imageSetDesc                    Description for image set
 * \param reference                        Reference image
 * \param result                        Result image
 * \param threshold                        Maximum allowed difference
 * \param maxPositionDeviation            Maximum allowed distance in the search
 *                                        volume.
 * \param acceptOutOfBoundsAsAnyValue    Accept any pixel in the boundary region
 * \param logMode                        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool intThresholdPositionDeviationCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                                          const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                                          const UVec4 &threshold, const tcu::IVec3 &maxPositionDeviation,
                                          bool acceptOutOfBoundsAsAnyValue, CompareLogMode logMode)
{
    const int width  = reference.getWidth();
    const int height = reference.getHeight();
    const int depth  = reference.getDepth();
    TextureLevel errorMaskStorage(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    const int numFailingPixels  = findNumPositionDeviationFailingPixels(
        errorMask, reference, result, threshold, maxPositionDeviation, acceptOutOfBoundsAsAnyValue);
    const bool compareOk = numFailingPixels == 0;
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    if (!compareOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        // All formats except normalized unsigned fixed point ones need remapping in order to fit into unorm channels in logged images.
        if (tcu::getTextureChannelClass(reference.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
            tcu::getTextureChannelClass(result.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
        {
            computeScaleAndBias(reference, result, pixelScale, pixelBias);
            log << TestLog::Message << "Result and reference images are normalized with formula p * " << pixelScale
                << " + " << pixelBias << TestLog::EndMessage;
        }

        if (!compareOk)
            log << TestLog::Message << "Image comparison failed:\n"
                << "\tallowed position deviation = " << maxPositionDeviation << "\n"
                << "\tcolor threshold = " << threshold << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias)
            << TestLog::Image("Reference", imageSetName, reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", imageSetName, errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return compareOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel threshold-based deviation-ignoring comparison
 *
 * This compare computes per-pixel differences between result and reference
 * image. Pixel fails the test if there is no pixel matching the given
 * threshold value in the search volume. Comparison fails if the number of
 * failing pixels exceeds the given limit.
 *
 * If the search volume contains out-of-bounds pixels, comparison can be set
 * to either ignore these pixels in search or to accept any pixel that has
 * out-of-bounds pixels in its search volume.
 *
 * This comparison can be used for integer- and fixed-point texture formats.
 * Difference is computed in integer space.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \param log                            Test log for results
 * \param imageSetName                    Name for image set when logging results
 * \param imageSetDesc                    Description for image set
 * \param reference                        Reference image
 * \param result                        Result image
 * \param threshold                        Maximum allowed difference
 * \param maxPositionDeviation            Maximum allowed distance in the search
 *                                        volume.
 * \param acceptOutOfBoundsAsAnyValue    Accept any pixel in the boundary region
 * \param maxAllowedFailingPixels        Maximum number of failing pixels
 * \param logMode                        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool intThresholdPositionDeviationErrorThresholdCompare(
    TestLog &log, const char *imageSetName, const char *imageSetDesc, const ConstPixelBufferAccess &reference,
    const ConstPixelBufferAccess &result, const UVec4 &threshold, const tcu::IVec3 &maxPositionDeviation,
    bool acceptOutOfBoundsAsAnyValue, int maxAllowedFailingPixels, CompareLogMode logMode)
{
    const int width  = reference.getWidth();
    const int height = reference.getHeight();
    const int depth  = reference.getDepth();
    TextureLevel errorMaskStorage(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), width, height, depth);
    PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    const int numFailingPixels  = findNumPositionDeviationFailingPixels(
        errorMask, reference, result, threshold, maxPositionDeviation, acceptOutOfBoundsAsAnyValue);
    const bool compareOk = numFailingPixels <= maxAllowedFailingPixels;
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    if (!compareOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        // All formats except normalized unsigned fixed point ones need remapping in order to fit into unorm channels in logged images.
        if (tcu::getTextureChannelClass(reference.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT ||
            tcu::getTextureChannelClass(result.getFormat().type) != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
        {
            computeScaleAndBias(reference, result, pixelScale, pixelBias);
            log << TestLog::Message << "Result and reference images are normalized with formula p * " << pixelScale
                << " + " << pixelBias << TestLog::EndMessage;
        }

        if (!compareOk)
            log << TestLog::Message << "Image comparison failed:\n"
                << "\tallowed position deviation = " << maxPositionDeviation << "\n"
                << "\tcolor threshold = " << threshold << TestLog::EndMessage;
        log << TestLog::Message << "Number of failing pixels = " << numFailingPixels
            << ", max allowed = " << maxAllowedFailingPixels << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias)
            << TestLog::Image("Reference", imageSetName, reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", imageSetName, errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return compareOk;
}

/*--------------------------------------------------------------------*//*!
 * \brief Per-pixel threshold-based comparison
 *
 * This compare computes per-pixel differences between result and reference
 * image. Comparison fails if any pixels exceed the given threshold value.
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Maximum allowed difference
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool pixelThresholdCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc, const Surface &reference,
                           const Surface &result, const RGBA &threshold, CompareLogMode logMode)
{
    return intThresholdCompare(log, imageSetName, imageSetDesc, reference.getAccess(), result.getAccess(),
                               threshold.toIVec().cast<uint32_t>(), logMode);
}

/*--------------------------------------------------------------------*//*!
 * \brief Bilinear image comparison
 *
 * \todo [pyry] Describe
 *
 * On failure error image is generated that shows where the failing pixels
 * are.
 *
 * \note                Currently supports only RGBA, UNORM_INT8 formats
 * \param log            Test log for results
 * \param imageSetName    Name for image set when logging results
 * \param imageSetDesc    Description for image set
 * \param reference        Reference image
 * \param result        Result image
 * \param threshold        Maximum local difference
 * \param logMode        Logging mode
 * \return true if comparison passes, false otherwise
 *//*--------------------------------------------------------------------*/
bool bilinearCompare(TestLog &log, const char *imageSetName, const char *imageSetDesc,
                     const ConstPixelBufferAccess &reference, const ConstPixelBufferAccess &result,
                     const RGBA threshold, CompareLogMode logMode)
{
    TextureLevel errorMask(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8), reference.getWidth(),
                           reference.getHeight());
    bool isOk = bilinearCompare(reference, result, errorMask, threshold);
    Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

    if (!isOk || logMode == COMPARE_LOG_EVERYTHING || log.logAllImages())
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8) &&
            reference.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computeScaleAndBias(reference, result, pixelScale, pixelBias);

        if (!isOk)
            log << TestLog::Message << "Image comparison failed, threshold = " << threshold << TestLog::EndMessage;

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias)
            << TestLog::Image("Reference", imageSetName, reference, pixelScale, pixelBias)
            << TestLog::Image("ErrorMask", imageSetName, errorMask) << TestLog::EndImageSet;
    }
    else if (logMode == COMPARE_LOG_RESULT)
    {
        if (result.getFormat() != TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8))
            computePixelScaleBias(result, pixelScale, pixelBias);

        log << TestLog::ImageSet(imageSetName, imageSetDesc)
            << TestLog::Image("Result", imageSetName, result, pixelScale, pixelBias) << TestLog::EndImageSet;
    }

    return isOk;
}

} // namespace tcu
