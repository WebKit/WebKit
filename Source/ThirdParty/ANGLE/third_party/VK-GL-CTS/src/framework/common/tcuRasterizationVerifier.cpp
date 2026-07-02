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
 * \brief Rasterization verifier utils.
 *//*--------------------------------------------------------------------*/

#include "tcuRasterizationVerifier.hpp"
#include "tcuVector.hpp"
#include "tcuSurface.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuFloat.hpp"

#include "deMath.h"
#include "deStringUtil.hpp"

#include "rrRasterizer.hpp"

#include <limits>

namespace tcu
{
namespace
{

bool verifyLineGroupInterpolationWithProjectedWeights(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                      const RasterizationArguments &args, tcu::TestLog &log);

bool lineLineIntersect(const tcu::Vector<int64_t, 2> &line0Beg, const tcu::Vector<int64_t, 2> &line0End,
                       const tcu::Vector<int64_t, 2> &line1Beg, const tcu::Vector<int64_t, 2> &line1End)
{
    typedef tcu::Vector<int64_t, 2> I64Vec2;

    // Lines do not intersect if the other line's endpoints are on the same side
    // otherwise, the do intersect

    // Test line 0
    {
        const I64Vec2 line          = line0End - line0Beg;
        const I64Vec2 v0            = line1Beg - line0Beg;
        const I64Vec2 v1            = line1End - line0Beg;
        const int64_t crossProduct0 = (line.x() * v0.y() - line.y() * v0.x());
        const int64_t crossProduct1 = (line.x() * v1.y() - line.y() * v1.x());

        // check signs
        if ((crossProduct0 < 0 && crossProduct1 < 0) || (crossProduct0 > 0 && crossProduct1 > 0))
            return false;
    }

    // Test line 1
    {
        const I64Vec2 line          = line1End - line1Beg;
        const I64Vec2 v0            = line0Beg - line1Beg;
        const I64Vec2 v1            = line0End - line1Beg;
        const int64_t crossProduct0 = (line.x() * v0.y() - line.y() * v0.x());
        const int64_t crossProduct1 = (line.x() * v1.y() - line.y() * v1.x());

        // check signs
        if ((crossProduct0 < 0 && crossProduct1 < 0) || (crossProduct0 > 0 && crossProduct1 > 0))
            return false;
    }

    return true;
}

bool isTriangleClockwise(const tcu::Vec4 &p0, const tcu::Vec4 &p1, const tcu::Vec4 &p2)
{
    const tcu::Vec2 u(p1.x() / p1.w() - p0.x() / p0.w(), p1.y() / p1.w() - p0.y() / p0.w());
    const tcu::Vec2 v(p2.x() / p2.w() - p0.x() / p0.w(), p2.y() / p2.w() - p0.y() / p0.w());
    const float crossProduct = (u.x() * v.y() - u.y() * v.x());

    return crossProduct > 0.0f;
}

bool compareColors(const tcu::RGBA &colorA, const tcu::RGBA &colorB, int redBits, int greenBits, int blueBits)
{
    const int thresholdRed   = 1 << (8 - redBits);
    const int thresholdGreen = 1 << (8 - greenBits);
    const int thresholdBlue  = 1 << (8 - blueBits);

    return deAbs32(colorA.getRed() - colorB.getRed()) <= thresholdRed &&
           deAbs32(colorA.getGreen() - colorB.getGreen()) <= thresholdGreen &&
           deAbs32(colorA.getBlue() - colorB.getBlue()) <= thresholdBlue;
}

bool pixelNearLineSegment(const tcu::IVec2 &pixel, const tcu::Vec2 &p0, const tcu::Vec2 &p1)
{
    const tcu::Vec2 pixelCenterPosition = tcu::Vec2((float)pixel.x() + 0.5f, (float)pixel.y() + 0.5f);

    // "Near" = Distance from the line to the pixel is less than 2 * pixel_max_radius. (pixel_max_radius = sqrt(2) / 2)
    const float maxPixelDistance        = 1.414f;
    const float maxPixelDistanceSquared = 2.0f;

    // Near the line
    {
        const tcu::Vec2 line     = p1 - p0;
        const tcu::Vec2 v        = pixelCenterPosition - p0;
        const float crossProduct = (line.x() * v.y() - line.y() * v.x());

        // distance to line: (line x v) / |line|
        //     |(line x v) / |line|| > maxPixelDistance
        // ==> (line x v)^2 / |line|^2 > maxPixelDistance^2
        // ==> (line x v)^2 > maxPixelDistance^2 * |line|^2

        if (crossProduct * crossProduct > maxPixelDistanceSquared * tcu::lengthSquared(line))
            return false;
    }

    // Between the endpoints
    {
        // distance from line endpoint 1 to pixel is less than line length + maxPixelDistance
        const float maxDistance = tcu::length(p1 - p0) + maxPixelDistance;

        if (tcu::length(pixelCenterPosition - p0) > maxDistance)
            return false;
        if (tcu::length(pixelCenterPosition - p1) > maxDistance)
            return false;
    }

    return true;
}

bool pixelOnlyOnASharedEdge(const tcu::IVec2 &pixel, const TriangleSceneSpec::SceneTriangle &triangle,
                            const tcu::IVec2 &viewportSize)
{
    if (triangle.sharedEdge[0] || triangle.sharedEdge[1] || triangle.sharedEdge[2])
    {
        const tcu::Vec2 triangleNormalizedDeviceSpace[3] = {
            tcu::Vec2(triangle.positions[0].x() / triangle.positions[0].w(),
                      triangle.positions[0].y() / triangle.positions[0].w()),
            tcu::Vec2(triangle.positions[1].x() / triangle.positions[1].w(),
                      triangle.positions[1].y() / triangle.positions[1].w()),
            tcu::Vec2(triangle.positions[2].x() / triangle.positions[2].w(),
                      triangle.positions[2].y() / triangle.positions[2].w()),
        };
        const tcu::Vec2 triangleScreenSpace[3] = {
            (triangleNormalizedDeviceSpace[0] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
                tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
            (triangleNormalizedDeviceSpace[1] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
                tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
            (triangleNormalizedDeviceSpace[2] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
                tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
        };

        const bool pixelOnEdge0 = pixelNearLineSegment(pixel, triangleScreenSpace[0], triangleScreenSpace[1]);
        const bool pixelOnEdge1 = pixelNearLineSegment(pixel, triangleScreenSpace[1], triangleScreenSpace[2]);
        const bool pixelOnEdge2 = pixelNearLineSegment(pixel, triangleScreenSpace[2], triangleScreenSpace[0]);

        // If the pixel is on a multiple edges return false

        if (pixelOnEdge0 && !pixelOnEdge1 && !pixelOnEdge2)
            return triangle.sharedEdge[0];
        if (!pixelOnEdge0 && pixelOnEdge1 && !pixelOnEdge2)
            return triangle.sharedEdge[1];
        if (!pixelOnEdge0 && !pixelOnEdge1 && pixelOnEdge2)
            return triangle.sharedEdge[2];
    }

    return false;
}

float triangleArea(const tcu::Vec2 &s0, const tcu::Vec2 &s1, const tcu::Vec2 &s2)
{
    const tcu::Vec2 u(s1.x() - s0.x(), s1.y() - s0.y());
    const tcu::Vec2 v(s2.x() - s0.x(), s2.y() - s0.y());
    const float crossProduct = (u.x() * v.y() - u.y() * v.x());

    return crossProduct / 2.0f;
}

tcu::IVec4 getTriangleAABB(const TriangleSceneSpec::SceneTriangle &triangle, const tcu::IVec2 &viewportSize)
{
    const tcu::Vec2 normalizedDeviceSpace[3] = {
        tcu::Vec2(triangle.positions[0].x() / triangle.positions[0].w(),
                  triangle.positions[0].y() / triangle.positions[0].w()),
        tcu::Vec2(triangle.positions[1].x() / triangle.positions[1].w(),
                  triangle.positions[1].y() / triangle.positions[1].w()),
        tcu::Vec2(triangle.positions[2].x() / triangle.positions[2].w(),
                  triangle.positions[2].y() / triangle.positions[2].w()),
    };
    const tcu::Vec2 screenSpace[3] = {
        (normalizedDeviceSpace[0] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
            tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
        (normalizedDeviceSpace[1] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
            tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
        (normalizedDeviceSpace[2] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
            tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
    };

    tcu::IVec4 aabb;

    aabb.x() = (int)deFloatFloor(de::min(de::min(screenSpace[0].x(), screenSpace[1].x()), screenSpace[2].x()));
    aabb.y() = (int)deFloatFloor(de::min(de::min(screenSpace[0].y(), screenSpace[1].y()), screenSpace[2].y()));
    aabb.z() = (int)deFloatCeil(de::max(de::max(screenSpace[0].x(), screenSpace[1].x()), screenSpace[2].x()));
    aabb.w() = (int)deFloatCeil(de::max(de::max(screenSpace[0].y(), screenSpace[1].y()), screenSpace[2].y()));

    return aabb;
}

float getExponentEpsilonFromULP(int valueExponent, uint32_t ulp)
{
    DE_ASSERT(ulp < (1u << 10));

    // assume mediump precision, using ulp as ulps in a 10 bit mantissa
    return tcu::Float32::construct(+1, valueExponent, (1u << 23) + (ulp << (23 - 10))).asFloat() -
           tcu::Float32::construct(+1, valueExponent, (1u << 23)).asFloat();
}

float getValueEpsilonFromULP(float value, uint32_t ulp)
{
    DE_ASSERT(value != std::numeric_limits<float>::infinity() && value != -std::numeric_limits<float>::infinity());

    const int exponent = tcu::Float32(value).exponent();
    return getExponentEpsilonFromULP(exponent, ulp);
}

float getMaxValueWithinError(float value, uint32_t ulp)
{
    if (value == std::numeric_limits<float>::infinity() || value == -std::numeric_limits<float>::infinity())
        return value;

    return value + getValueEpsilonFromULP(value, ulp);
}

float getMinValueWithinError(float value, uint32_t ulp)
{
    if (value == std::numeric_limits<float>::infinity() || value == -std::numeric_limits<float>::infinity())
        return value;

    return value - getValueEpsilonFromULP(value, ulp);
}

float getMinFlushToZero(float value)
{
    // flush to zero if that decreases the value
    // assume mediump precision
    if (value > 0.0f && value < tcu::Float32::construct(+1, -14, 1u << 23).asFloat())
        return 0.0f;
    return value;
}

float getMaxFlushToZero(float value)
{
    // flush to zero if that increases the value
    // assume mediump precision
    if (value < 0.0f && value > tcu::Float32::construct(-1, -14, 1u << 23).asFloat())
        return 0.0f;
    return value;
}

tcu::IVec3 convertRGB8ToNativeFormat(const tcu::RGBA &color, const RasterizationArguments &args)
{
    tcu::IVec3 pixelNativeColor;

    for (int channelNdx = 0; channelNdx < 3; ++channelNdx)
    {
        const int channelBitCount   = (channelNdx == 0) ? (args.redBits) :
                                      (channelNdx == 1) ? (args.greenBits) :
                                                          (args.blueBits);
        const int channelPixelValue = (channelNdx == 0) ? (color.getRed()) :
                                      (channelNdx == 1) ? (color.getGreen()) :
                                                          (color.getBlue());

        if (channelBitCount <= 8)
            pixelNativeColor[channelNdx] = channelPixelValue >> (8 - channelBitCount);
        else if (channelBitCount == 8)
            pixelNativeColor[channelNdx] = channelPixelValue;
        else
        {
            // just in case someone comes up with 8+ bits framebuffers pixel formats. But as
            // we can only read in rgba8, we have to guess the trailing bits. Guessing 0.
            pixelNativeColor[channelNdx] = channelPixelValue << (channelBitCount - 8);
        }
    }

    return pixelNativeColor;
}

/*--------------------------------------------------------------------*//*!
 * Returns the maximum value of x / y, where x c [minDividend, maxDividend]
 * and y c [minDivisor, maxDivisor]
 *//*--------------------------------------------------------------------*/
float maximalRangeDivision(float minDividend, float maxDividend, float minDivisor, float maxDivisor)
{
    DE_ASSERT(minDividend <= maxDividend);
    DE_ASSERT(minDivisor <= maxDivisor);

    // special cases
    if (minDividend == 0.0f && maxDividend == 0.0f)
        return 0.0f;
    if (minDivisor <= 0.0f && maxDivisor >= 0.0f)
        return std::numeric_limits<float>::infinity();

    return de::max(de::max(minDividend / minDivisor, minDividend / maxDivisor),
                   de::max(maxDividend / minDivisor, maxDividend / maxDivisor));
}

/*--------------------------------------------------------------------*//*!
 * Returns the minimum value of x / y, where x c [minDividend, maxDividend]
 * and y c [minDivisor, maxDivisor]
 *//*--------------------------------------------------------------------*/
float minimalRangeDivision(float minDividend, float maxDividend, float minDivisor, float maxDivisor)
{
    DE_ASSERT(minDividend <= maxDividend);
    DE_ASSERT(minDivisor <= maxDivisor);

    // special cases
    if (minDividend == 0.0f && maxDividend == 0.0f)
        return 0.0f;
    if (minDivisor <= 0.0f && maxDivisor >= 0.0f)
        return -std::numeric_limits<float>::infinity();

    return de::min(de::min(minDividend / minDivisor, minDividend / maxDivisor),
                   de::min(maxDividend / minDivisor, maxDividend / maxDivisor));
}

static bool isLineXMajor(const tcu::Vec2 &lineScreenSpaceP0, const tcu::Vec2 &lineScreenSpaceP1)
{
    return de::abs(lineScreenSpaceP1.x() - lineScreenSpaceP0.x()) >=
           de::abs(lineScreenSpaceP1.y() - lineScreenSpaceP0.y());
}

static bool isPackedSSLineXMajor(const tcu::Vec4 &packedLine)
{
    const tcu::Vec2 lineScreenSpaceP0 = packedLine.swizzle(0, 1);
    const tcu::Vec2 lineScreenSpaceP1 = packedLine.swizzle(2, 3);

    return isLineXMajor(lineScreenSpaceP0, lineScreenSpaceP1);
}

struct InterpolationRange
{
    tcu::Vec3 max;
    tcu::Vec3 min;
};

struct LineInterpolationRange
{
    tcu::Vec2 max;
    tcu::Vec2 min;
};

InterpolationRange calcTriangleInterpolationWeights(const tcu::Vec4 &p0, const tcu::Vec4 &p1, const tcu::Vec4 &p2,
                                                    const tcu::Vec2 &ndpixel)
{
    const int roundError       = 1;
    const int barycentricError = 3;
    const int divError         = 8;

    const tcu::Vec2 nd0 = p0.swizzle(0, 1) / p0.w();
    const tcu::Vec2 nd1 = p1.swizzle(0, 1) / p1.w();
    const tcu::Vec2 nd2 = p2.swizzle(0, 1) / p2.w();

    const float ka = triangleArea(ndpixel, nd1, nd2);
    const float kb = triangleArea(ndpixel, nd2, nd0);
    const float kc = triangleArea(ndpixel, nd0, nd1);

    const float kaMax = getMaxFlushToZero(getMaxValueWithinError(ka, barycentricError));
    const float kbMax = getMaxFlushToZero(getMaxValueWithinError(kb, barycentricError));
    const float kcMax = getMaxFlushToZero(getMaxValueWithinError(kc, barycentricError));
    const float kaMin = getMinFlushToZero(getMinValueWithinError(ka, barycentricError));
    const float kbMin = getMinFlushToZero(getMinValueWithinError(kb, barycentricError));
    const float kcMin = getMinFlushToZero(getMinValueWithinError(kc, barycentricError));
    DE_ASSERT(kaMin <= kaMax);
    DE_ASSERT(kbMin <= kbMax);
    DE_ASSERT(kcMin <= kcMax);

    // calculate weights: vec3(ka / p0.w, kb / p1.w, kc / p2.w) / (ka / p0.w + kb / p1.w + kc / p2.w)
    const float maxPreDivisionValues[3] = {
        getMaxFlushToZero(getMaxValueWithinError(getMaxFlushToZero(kaMax / p0.w()), divError)),
        getMaxFlushToZero(getMaxValueWithinError(getMaxFlushToZero(kbMax / p1.w()), divError)),
        getMaxFlushToZero(getMaxValueWithinError(getMaxFlushToZero(kcMax / p2.w()), divError)),
    };
    const float minPreDivisionValues[3] = {
        getMinFlushToZero(getMinValueWithinError(getMinFlushToZero(kaMin / p0.w()), divError)),
        getMinFlushToZero(getMinValueWithinError(getMinFlushToZero(kbMin / p1.w()), divError)),
        getMinFlushToZero(getMinValueWithinError(getMinFlushToZero(kcMin / p2.w()), divError)),
    };
    DE_ASSERT(minPreDivisionValues[0] <= maxPreDivisionValues[0]);
    DE_ASSERT(minPreDivisionValues[1] <= maxPreDivisionValues[1]);
    DE_ASSERT(minPreDivisionValues[2] <= maxPreDivisionValues[2]);

    const float maxDivisor = getMaxFlushToZero(getMaxValueWithinError(
        maxPreDivisionValues[0] + maxPreDivisionValues[1] + maxPreDivisionValues[2], 2 * roundError));
    const float minDivisor = getMinFlushToZero(getMinValueWithinError(
        minPreDivisionValues[0] + minPreDivisionValues[1] + minPreDivisionValues[2], 2 * roundError));
    DE_ASSERT(minDivisor <= maxDivisor);

    InterpolationRange returnValue;

    returnValue.max.x() = getMaxFlushToZero(
        getMaxValueWithinError(getMaxFlushToZero(maximalRangeDivision(minPreDivisionValues[0], maxPreDivisionValues[0],
                                                                      minDivisor, maxDivisor)),
                               divError));
    returnValue.max.y() = getMaxFlushToZero(
        getMaxValueWithinError(getMaxFlushToZero(maximalRangeDivision(minPreDivisionValues[1], maxPreDivisionValues[1],
                                                                      minDivisor, maxDivisor)),
                               divError));
    returnValue.max.z() = getMaxFlushToZero(
        getMaxValueWithinError(getMaxFlushToZero(maximalRangeDivision(minPreDivisionValues[2], maxPreDivisionValues[2],
                                                                      minDivisor, maxDivisor)),
                               divError));
    returnValue.min.x() = getMinFlushToZero(
        getMinValueWithinError(getMinFlushToZero(minimalRangeDivision(minPreDivisionValues[0], maxPreDivisionValues[0],
                                                                      minDivisor, maxDivisor)),
                               divError));
    returnValue.min.y() = getMinFlushToZero(
        getMinValueWithinError(getMinFlushToZero(minimalRangeDivision(minPreDivisionValues[1], maxPreDivisionValues[1],
                                                                      minDivisor, maxDivisor)),
                               divError));
    returnValue.min.z() = getMinFlushToZero(
        getMinValueWithinError(getMinFlushToZero(minimalRangeDivision(minPreDivisionValues[2], maxPreDivisionValues[2],
                                                                      minDivisor, maxDivisor)),
                               divError));

    DE_ASSERT(returnValue.min.x() <= returnValue.max.x());
    DE_ASSERT(returnValue.min.y() <= returnValue.max.y());
    DE_ASSERT(returnValue.min.z() <= returnValue.max.z());

    return returnValue;
}

LineInterpolationRange calcLineInterpolationWeights(const tcu::Vec2 &pa, float wa, const tcu::Vec2 &pb, float wb,
                                                    const tcu::Vec2 &pr)
{
    const int roundError = 1;
    const int divError   = 3;

    // calc weights:
    //            (1-t) / wa                    t / wb
    //        -------------------    , -------------------
    //        (1-t) / wa + t / wb        (1-t) / wa + t / wb

    // Allow 1 ULP
    const float dividend    = tcu::dot(pr - pa, pb - pa);
    const float dividendMax = getMaxValueWithinError(dividend, 1);
    const float dividendMin = getMinValueWithinError(dividend, 1);
    DE_ASSERT(dividendMin <= dividendMax);

    // Assuming lengthSquared will not be implemented as sqrt(x)^2, allow 1 ULP
    const float divisor    = tcu::lengthSquared(pb - pa);
    const float divisorMax = getMaxValueWithinError(divisor, 1);
    const float divisorMin = getMinValueWithinError(divisor, 1);
    DE_ASSERT(divisorMin <= divisorMax);

    // Allow 3 ULP precision for division
    const float tMax =
        getMaxValueWithinError(maximalRangeDivision(dividendMin, dividendMax, divisorMin, divisorMax), divError);
    const float tMin =
        getMinValueWithinError(minimalRangeDivision(dividendMin, dividendMax, divisorMin, divisorMax), divError);
    DE_ASSERT(tMin <= tMax);

    const float perspectiveTMax = getMaxValueWithinError(maximalRangeDivision(tMin, tMax, wb, wb), divError);
    const float perspectiveTMin = getMinValueWithinError(minimalRangeDivision(tMin, tMax, wb, wb), divError);
    DE_ASSERT(perspectiveTMin <= perspectiveTMax);

    const float perspectiveInvTMax =
        getMaxValueWithinError(maximalRangeDivision((1.0f - tMax), (1.0f - tMin), wa, wa), divError);
    const float perspectiveInvTMin =
        getMinValueWithinError(minimalRangeDivision((1.0f - tMax), (1.0f - tMin), wa, wa), divError);
    DE_ASSERT(perspectiveInvTMin <= perspectiveInvTMax);

    const float perspectiveDivisorMax = getMaxValueWithinError(perspectiveTMax + perspectiveInvTMax, roundError);
    const float perspectiveDivisorMin = getMinValueWithinError(perspectiveTMin + perspectiveInvTMin, roundError);
    DE_ASSERT(perspectiveDivisorMin <= perspectiveDivisorMax);

    LineInterpolationRange returnValue;
    returnValue.max.x() = getMaxValueWithinError(
        maximalRangeDivision(perspectiveInvTMin, perspectiveInvTMax, perspectiveDivisorMin, perspectiveDivisorMax),
        divError);
    returnValue.max.y() = getMaxValueWithinError(
        maximalRangeDivision(perspectiveTMin, perspectiveTMax, perspectiveDivisorMin, perspectiveDivisorMax), divError);
    returnValue.min.x() = getMinValueWithinError(
        minimalRangeDivision(perspectiveInvTMin, perspectiveInvTMax, perspectiveDivisorMin, perspectiveDivisorMax),
        divError);
    returnValue.min.y() = getMinValueWithinError(
        minimalRangeDivision(perspectiveTMin, perspectiveTMax, perspectiveDivisorMin, perspectiveDivisorMax), divError);

    DE_ASSERT(returnValue.min.x() <= returnValue.max.x());
    DE_ASSERT(returnValue.min.y() <= returnValue.max.y());

    return returnValue;
}

LineInterpolationRange calcLineInterpolationWeightsAxisProjected(const tcu::Vec2 &pa, float wa, const tcu::Vec2 &pb,
                                                                 float wb, const tcu::Vec2 &pr)
{
    const int roundError   = 1;
    const int divError     = 3;
    const bool isXMajor    = isLineXMajor(pa, pb);
    const int majorAxisNdx = (isXMajor) ? (0) : (1);

    // calc weights:
    //            (1-t) / wa                    t / wb
    //        -------------------    , -------------------
    //        (1-t) / wa + t / wb        (1-t) / wa + t / wb

    // Use axis projected (inaccurate) method, i.e. for X-major lines:
    //     (xd - xa) * (xb - xa)      xd - xa
    // t = ---------------------  ==  -------
    //       ( xb - xa ) ^ 2          xb - xa

    // Allow 1 ULP
    const float dividend    = (pr[majorAxisNdx] - pa[majorAxisNdx]);
    const float dividendMax = getMaxValueWithinError(dividend, 1);
    const float dividendMin = getMinValueWithinError(dividend, 1);
    DE_ASSERT(dividendMin <= dividendMax);

    // Allow 1 ULP
    const float divisor    = (pb[majorAxisNdx] - pa[majorAxisNdx]);
    const float divisorMax = getMaxValueWithinError(divisor, 1);
    const float divisorMin = getMinValueWithinError(divisor, 1);
    DE_ASSERT(divisorMin <= divisorMax);

    // Allow 3 ULP precision for division
    const float tMax =
        getMaxValueWithinError(maximalRangeDivision(dividendMin, dividendMax, divisorMin, divisorMax), divError);
    const float tMin =
        getMinValueWithinError(minimalRangeDivision(dividendMin, dividendMax, divisorMin, divisorMax), divError);
    DE_ASSERT(tMin <= tMax);

    const float perspectiveTMax = getMaxValueWithinError(maximalRangeDivision(tMin, tMax, wb, wb), divError);
    const float perspectiveTMin = getMinValueWithinError(minimalRangeDivision(tMin, tMax, wb, wb), divError);
    DE_ASSERT(perspectiveTMin <= perspectiveTMax);

    const float perspectiveInvTMax =
        getMaxValueWithinError(maximalRangeDivision((1.0f - tMax), (1.0f - tMin), wa, wa), divError);
    const float perspectiveInvTMin =
        getMinValueWithinError(minimalRangeDivision((1.0f - tMax), (1.0f - tMin), wa, wa), divError);
    DE_ASSERT(perspectiveInvTMin <= perspectiveInvTMax);

    const float perspectiveDivisorMax = getMaxValueWithinError(perspectiveTMax + perspectiveInvTMax, roundError);
    const float perspectiveDivisorMin = getMinValueWithinError(perspectiveTMin + perspectiveInvTMin, roundError);
    DE_ASSERT(perspectiveDivisorMin <= perspectiveDivisorMax);

    LineInterpolationRange returnValue;
    returnValue.max.x() = getMaxValueWithinError(
        maximalRangeDivision(perspectiveInvTMin, perspectiveInvTMax, perspectiveDivisorMin, perspectiveDivisorMax),
        divError);
    returnValue.max.y() = getMaxValueWithinError(
        maximalRangeDivision(perspectiveTMin, perspectiveTMax, perspectiveDivisorMin, perspectiveDivisorMax), divError);
    returnValue.min.x() = getMinValueWithinError(
        minimalRangeDivision(perspectiveInvTMin, perspectiveInvTMax, perspectiveDivisorMin, perspectiveDivisorMax),
        divError);
    returnValue.min.y() = getMinValueWithinError(
        minimalRangeDivision(perspectiveTMin, perspectiveTMax, perspectiveDivisorMin, perspectiveDivisorMax), divError);

    DE_ASSERT(returnValue.min.x() <= returnValue.max.x());
    DE_ASSERT(returnValue.min.y() <= returnValue.max.y());

    return returnValue;
}

template <typename WeightEquation>
LineInterpolationRange calcSingleSampleLineInterpolationRangeWithWeightEquation(const tcu::Vec2 &pa, float wa,
                                                                                const tcu::Vec2 &pb, float wb,
                                                                                const tcu::IVec2 &pixel,
                                                                                int subpixelBits,
                                                                                WeightEquation weightEquation)
{
    // allow interpolation weights anywhere in the central subpixels
    const float testSquareSize = (2.0f / (float)(1UL << subpixelBits));
    const float testSquarePos  = (0.5f - testSquareSize / 2);

    const tcu::Vec2 corners[4] = {
        tcu::Vec2((float)pixel.x() + testSquarePos + 0.0f, (float)pixel.y() + testSquarePos + 0.0f),
        tcu::Vec2((float)pixel.x() + testSquarePos + 0.0f, (float)pixel.y() + testSquarePos + testSquareSize),
        tcu::Vec2((float)pixel.x() + testSquarePos + testSquareSize, (float)pixel.y() + testSquarePos + testSquareSize),
        tcu::Vec2((float)pixel.x() + testSquarePos + testSquareSize, (float)pixel.y() + testSquarePos + 0.0f),
    };

    // calculate interpolation as a line
    const LineInterpolationRange weights[4] = {
        weightEquation(pa, wa, pb, wb, corners[0]),
        weightEquation(pa, wa, pb, wb, corners[1]),
        weightEquation(pa, wa, pb, wb, corners[2]),
        weightEquation(pa, wa, pb, wb, corners[3]),
    };

    const tcu::Vec2 minWeights =
        tcu::min(tcu::min(weights[0].min, weights[1].min), tcu::min(weights[2].min, weights[3].min));
    const tcu::Vec2 maxWeights =
        tcu::max(tcu::max(weights[0].max, weights[1].max), tcu::max(weights[2].max, weights[3].max));

    LineInterpolationRange result;
    result.min = minWeights;
    result.max = maxWeights;
    return result;
}

LineInterpolationRange calcSingleSampleLineInterpolationRange(const tcu::Vec2 &pa, float wa, const tcu::Vec2 &pb,
                                                              float wb, const tcu::IVec2 &pixel, int subpixelBits)
{
    return calcSingleSampleLineInterpolationRangeWithWeightEquation(pa, wa, pb, wb, pixel, subpixelBits,
                                                                    calcLineInterpolationWeights);
}

LineInterpolationRange calcSingleSampleLineInterpolationRangeAxisProjected(const tcu::Vec2 &pa, float wa,
                                                                           const tcu::Vec2 &pb, float wb,
                                                                           const tcu::IVec2 &pixel, int subpixelBits)
{
    return calcSingleSampleLineInterpolationRangeWithWeightEquation(pa, wa, pb, wb, pixel, subpixelBits,
                                                                    calcLineInterpolationWeightsAxisProjected);
}

struct TriangleInterpolator
{
    const TriangleSceneSpec &scene;

    TriangleInterpolator(const TriangleSceneSpec &scene_) : scene(scene_)
    {
    }

    InterpolationRange interpolate(int primitiveNdx, const tcu::IVec2 pixel, const tcu::IVec2 viewportSize,
                                   bool multisample, int subpixelBits) const
    {
        // allow anywhere in the pixel area in multisample
        // allow only in the center subpixels (4 subpixels) in singlesample
        const float testSquareSize = (multisample) ? (1.0f) : (2.0f / (float)(1UL << subpixelBits));
        const float testSquarePos  = (multisample) ? (0.0f) : (0.5f - testSquareSize / 2);
        const tcu::Vec2 corners[4] = {
            tcu::Vec2(((float)pixel.x() + testSquarePos + 0.0f) / (float)viewportSize.x() * 2.0f - 1.0f,
                      ((float)pixel.y() + testSquarePos + 0.0f) / (float)viewportSize.y() * 2.0f - 1.0f),
            tcu::Vec2(((float)pixel.x() + testSquarePos + 0.0f) / (float)viewportSize.x() * 2.0f - 1.0f,
                      ((float)pixel.y() + testSquarePos + testSquareSize) / (float)viewportSize.y() * 2.0f - 1.0f),
            tcu::Vec2(((float)pixel.x() + testSquarePos + testSquareSize) / (float)viewportSize.x() * 2.0f - 1.0f,
                      ((float)pixel.y() + testSquarePos + testSquareSize) / (float)viewportSize.y() * 2.0f - 1.0f),
            tcu::Vec2(((float)pixel.x() + testSquarePos + testSquareSize) / (float)viewportSize.x() * 2.0f - 1.0f,
                      ((float)pixel.y() + testSquarePos + 0.0f) / (float)viewportSize.y() * 2.0f - 1.0f),
        };
        const InterpolationRange weights[4] = {
            calcTriangleInterpolationWeights(scene.triangles[primitiveNdx].positions[0],
                                             scene.triangles[primitiveNdx].positions[1],
                                             scene.triangles[primitiveNdx].positions[2], corners[0]),
            calcTriangleInterpolationWeights(scene.triangles[primitiveNdx].positions[0],
                                             scene.triangles[primitiveNdx].positions[1],
                                             scene.triangles[primitiveNdx].positions[2], corners[1]),
            calcTriangleInterpolationWeights(scene.triangles[primitiveNdx].positions[0],
                                             scene.triangles[primitiveNdx].positions[1],
                                             scene.triangles[primitiveNdx].positions[2], corners[2]),
            calcTriangleInterpolationWeights(scene.triangles[primitiveNdx].positions[0],
                                             scene.triangles[primitiveNdx].positions[1],
                                             scene.triangles[primitiveNdx].positions[2], corners[3]),
        };

        InterpolationRange result;
        result.min = tcu::min(tcu::min(weights[0].min, weights[1].min), tcu::min(weights[2].min, weights[3].min));
        result.max = tcu::max(tcu::max(weights[0].max, weights[1].max), tcu::max(weights[2].max, weights[3].max));
        return result;
    }
};

/*--------------------------------------------------------------------*//*!
 * Used only by verifyMultisampleLineGroupInterpolation to calculate
 * correct line interpolations for the triangulated lines.
 *//*--------------------------------------------------------------------*/
struct MultisampleLineInterpolator
{
    const LineSceneSpec &scene;

    MultisampleLineInterpolator(const LineSceneSpec &scene_) : scene(scene_)
    {
    }

    InterpolationRange interpolate(int primitiveNdx, const tcu::IVec2 pixel, const tcu::IVec2 viewportSize,
                                   bool multisample, int subpixelBits) const
    {
        DE_UNREF(multisample);
        DE_UNREF(subpixelBits);

        // in triangulation, one line emits two triangles
        const int lineNdx = primitiveNdx / 2;

        // allow interpolation weights anywhere in the pixel
        const tcu::Vec2 corners[4] = {
            tcu::Vec2((float)pixel.x() + 0.0f, (float)pixel.y() + 0.0f),
            tcu::Vec2((float)pixel.x() + 0.0f, (float)pixel.y() + 1.0f),
            tcu::Vec2((float)pixel.x() + 1.0f, (float)pixel.y() + 1.0f),
            tcu::Vec2((float)pixel.x() + 1.0f, (float)pixel.y() + 0.0f),
        };

        const float wa = scene.lines[lineNdx].positions[0].w();
        const float wb = scene.lines[lineNdx].positions[1].w();
        const tcu::Vec2 pa =
            tcu::Vec2((scene.lines[lineNdx].positions[0].x() / wa + 1.0f) * 0.5f * (float)viewportSize.x(),
                      (scene.lines[lineNdx].positions[0].y() / wa + 1.0f) * 0.5f * (float)viewportSize.y());
        const tcu::Vec2 pb =
            tcu::Vec2((scene.lines[lineNdx].positions[1].x() / wb + 1.0f) * 0.5f * (float)viewportSize.x(),
                      (scene.lines[lineNdx].positions[1].y() / wb + 1.0f) * 0.5f * (float)viewportSize.y());

        // calculate interpolation as a line
        const LineInterpolationRange weights[4] = {
            calcLineInterpolationWeights(pa, wa, pb, wb, corners[0]),
            calcLineInterpolationWeights(pa, wa, pb, wb, corners[1]),
            calcLineInterpolationWeights(pa, wa, pb, wb, corners[2]),
            calcLineInterpolationWeights(pa, wa, pb, wb, corners[3]),
        };

        const tcu::Vec2 minWeights =
            tcu::min(tcu::min(weights[0].min, weights[1].min), tcu::min(weights[2].min, weights[3].min));
        const tcu::Vec2 maxWeights =
            tcu::max(tcu::max(weights[0].max, weights[1].max), tcu::max(weights[2].max, weights[3].max));

        // convert to three-component form. For all triangles, the vertex 0 is always emitted by the line starting point, and vertex 2 by the ending point
        InterpolationRange result;
        result.min = tcu::Vec3(minWeights.x(), 0.0f, minWeights.y());
        result.max = tcu::Vec3(maxWeights.x(), 0.0f, maxWeights.y());
        return result;
    }
};

template <typename Interpolator>
bool verifyTriangleGroupInterpolationWithInterpolator(const tcu::Surface &surface, const TriangleSceneSpec &scene,
                                                      const RasterizationArguments &args,
                                                      VerifyTriangleGroupInterpolationLogStash &logStash,
                                                      const Interpolator &interpolator)
{
    const tcu::RGBA invalidPixelColor = tcu::RGBA(255, 0, 0, 255);
    const bool multisampled           = (args.numSamples != 0);
    const tcu::IVec2 viewportSize     = tcu::IVec2(surface.getWidth(), surface.getHeight());
    const int errorFloodThreshold     = 4;
    int errorCount                    = 0;
    int invalidPixels                 = 0;
    int subPixelBits                  = args.subpixelBits;
    tcu::Surface errorMask(surface.getWidth(), surface.getHeight());

    tcu::clear(errorMask.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // log format

    logStash.messages.push_back(std::string("Verifying rasterization result. Native format is RGB" +
                                            de::toString(args.redBits) + de::toString(args.greenBits) +
                                            de::toString(args.blueBits)));
    if (args.redBits > 8 || args.greenBits > 8 || args.blueBits > 8)
        logStash.messages.push_back(
            std::string("Warning! More than 8 bits in a color channel, this may produce false negatives."));

    // subpixel bits in a valid range?

    if (subPixelBits < 0)
    {
        logStash.messages.push_back(
            std::string("Invalid subpixel count (" + de::toString(subPixelBits) + "), assuming 0"));
        subPixelBits = 0;
    }
    else if (subPixelBits > 16)
    {
        // At high subpixel bit counts we might overflow. Checking at lower bit count is ok, but is less strict
        logStash.messages.push_back(
            std::string("Subpixel count is greater than 16 (" + de::toString(subPixelBits) +
                        ")."
                        " Checking results using less strict 16 bit requirements. This may produce false positives."));
        subPixelBits = 16;
    }

    // check pixels

    for (int y = 0; y < surface.getHeight(); ++y)
        for (int x = 0; x < surface.getWidth(); ++x)
        {
            const tcu::RGBA color = surface.getPixel(x, y);
            bool stackBottomFound = false;
            int stackSize         = 0;
            tcu::Vec4 colorStackMin;
            tcu::Vec4 colorStackMax;

            // Iterate triangle coverage front to back, find the stack of pontentially contributing fragments
            for (int triNdx = (int)scene.triangles.size() - 1; triNdx >= 0; --triNdx)
            {
                const CoverageType coverage = calculateTriangleCoverage(
                    scene.triangles[triNdx].positions[0], scene.triangles[triNdx].positions[1],
                    scene.triangles[triNdx].positions[2], tcu::IVec2(x, y), viewportSize, subPixelBits, multisampled);

                if (coverage == COVERAGE_FULL || coverage == COVERAGE_PARTIAL)
                {
                    // potentially contributes to the result fragment's value
                    const InterpolationRange weights =
                        interpolator.interpolate(triNdx, tcu::IVec2(x, y), viewportSize, multisampled, subPixelBits);

                    const tcu::Vec4 fragmentColorMax =
                        de::clamp(weights.max.x(), 0.0f, 1.0f) * scene.triangles[triNdx].colors[0] +
                        de::clamp(weights.max.y(), 0.0f, 1.0f) * scene.triangles[triNdx].colors[1] +
                        de::clamp(weights.max.z(), 0.0f, 1.0f) * scene.triangles[triNdx].colors[2];
                    const tcu::Vec4 fragmentColorMin =
                        de::clamp(weights.min.x(), 0.0f, 1.0f) * scene.triangles[triNdx].colors[0] +
                        de::clamp(weights.min.y(), 0.0f, 1.0f) * scene.triangles[triNdx].colors[1] +
                        de::clamp(weights.min.z(), 0.0f, 1.0f) * scene.triangles[triNdx].colors[2];

                    if (stackSize++ == 0)
                    {
                        // first triangle, set the values properly
                        colorStackMin = fragmentColorMin;
                        colorStackMax = fragmentColorMax;
                    }
                    else
                    {
                        // contributing triangle
                        colorStackMin = tcu::min(colorStackMin, fragmentColorMin);
                        colorStackMax = tcu::max(colorStackMax, fragmentColorMax);
                    }

                    if (coverage == COVERAGE_FULL)
                    {
                        // loop terminates, this is the bottommost fragment
                        stackBottomFound = true;
                        break;
                    }
                }
            }

            // Partial coverage == background may be visible
            if (stackSize != 0 && !stackBottomFound)
            {
                stackSize++;
                colorStackMin = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
            }

            // Is the result image color in the valid range.
            if (stackSize == 0)
            {
                // No coverage, allow only background (black, value=0)
                const tcu::IVec3 pixelNativeColor = convertRGB8ToNativeFormat(color, args);
                const int threshold               = 1;

                if (pixelNativeColor.x() > threshold || pixelNativeColor.y() > threshold ||
                    pixelNativeColor.z() > threshold)
                {
                    ++errorCount;

                    // don't fill the logs with too much data
                    if (errorCount < errorFloodThreshold)
                    {
                        std::ostringstream str;

                        str << "Found an invalid pixel at (" << x << "," << y << ")\n"
                            << "\tPixel color:\t\t" << color << "\n"
                            << "\tExpected background color.\n";

                        logStash.messages.push_back(str.str());
                    }

                    ++invalidPixels;
                    errorMask.setPixel(x, y, invalidPixelColor);
                }
            }
            else
            {
                DE_ASSERT(stackSize);

                // Each additional step in the stack may cause conversion error of 1 bit due to undefined rounding direction
                const int thresholdRed   = stackSize - 1;
                const int thresholdGreen = stackSize - 1;
                const int thresholdBlue  = stackSize - 1;

                const tcu::Vec3 valueRangeMin = tcu::Vec3(colorStackMin.xyz());
                const tcu::Vec3 valueRangeMax = tcu::Vec3(colorStackMax.xyz());

                const tcu::IVec3 formatLimit((1 << args.redBits) - 1, (1 << args.greenBits) - 1,
                                             (1 << args.blueBits) - 1);
                const tcu::Vec3 colorMinF(
                    de::clamp(valueRangeMin.x() * (float)formatLimit.x(), 0.0f, (float)formatLimit.x()),
                    de::clamp(valueRangeMin.y() * (float)formatLimit.y(), 0.0f, (float)formatLimit.y()),
                    de::clamp(valueRangeMin.z() * (float)formatLimit.z(), 0.0f, (float)formatLimit.z()));
                const tcu::Vec3 colorMaxF(
                    de::clamp(valueRangeMax.x() * (float)formatLimit.x(), 0.0f, (float)formatLimit.x()),
                    de::clamp(valueRangeMax.y() * (float)formatLimit.y(), 0.0f, (float)formatLimit.y()),
                    de::clamp(valueRangeMax.z() * (float)formatLimit.z(), 0.0f, (float)formatLimit.z()));
                const tcu::IVec3 colorMin((int)deFloatFloor(colorMinF.x()), (int)deFloatFloor(colorMinF.y()),
                                          (int)deFloatFloor(colorMinF.z()));
                const tcu::IVec3 colorMax((int)deFloatCeil(colorMaxF.x()), (int)deFloatCeil(colorMaxF.y()),
                                          (int)deFloatCeil(colorMaxF.z()));

                // Convert pixel color from rgba8 to the real pixel format. Usually rgba8 or 565
                const tcu::IVec3 pixelNativeColor = convertRGB8ToNativeFormat(color, args);

                // Validity check
                if (pixelNativeColor.x() < colorMin.x() - thresholdRed ||
                    pixelNativeColor.y() < colorMin.y() - thresholdGreen ||
                    pixelNativeColor.z() < colorMin.z() - thresholdBlue ||
                    pixelNativeColor.x() > colorMax.x() + thresholdRed ||
                    pixelNativeColor.y() > colorMax.y() + thresholdGreen ||
                    pixelNativeColor.z() > colorMax.z() + thresholdBlue)
                {
                    ++errorCount;

                    // don't fill the logs with too much data
                    if (errorCount <= errorFloodThreshold)
                    {
                        std::ostringstream str;

                        str << "Found an invalid pixel at (" << x << "," << y << ")\n"
                            << "\tPixel color:\t\t" << color << "\n"
                            << "\tNative color:\t\t" << pixelNativeColor << "\n"
                            << "\tAllowed error:\t\t" << tcu::IVec3(thresholdRed, thresholdGreen, thresholdBlue) << "\n"
                            << "\tReference native color min: "
                            << tcu::clamp(colorMin - tcu::IVec3(thresholdRed, thresholdGreen, thresholdBlue),
                                          tcu::IVec3(0, 0, 0), formatLimit)
                            << "\n"
                            << "\tReference native color max: "
                            << tcu::clamp(colorMax + tcu::IVec3(thresholdRed, thresholdGreen, thresholdBlue),
                                          tcu::IVec3(0, 0, 0), formatLimit)
                            << "\n"
                            << "\tReference native float min: "
                            << tcu::clamp(colorMinF -
                                              tcu::IVec3(thresholdRed, thresholdGreen, thresholdBlue).cast<float>(),
                                          tcu::Vec3(0.0f, 0.0f, 0.0f), formatLimit.cast<float>())
                            << "\n"
                            << "\tReference native float max: "
                            << tcu::clamp(colorMaxF +
                                              tcu::IVec3(thresholdRed, thresholdGreen, thresholdBlue).cast<float>(),
                                          tcu::Vec3(0.0f, 0.0f, 0.0f), formatLimit.cast<float>())
                            << "\n"
                            << "\tFmin:\t"
                            << tcu::clamp(valueRangeMin, tcu::Vec3(0.0f, 0.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 1.0f))
                            << "\n"
                            << "\tFmax:\t"
                            << tcu::clamp(valueRangeMax, tcu::Vec3(0.0f, 0.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 1.0f))
                            << "\n";
                        logStash.messages.push_back(str.str());
                    }

                    ++invalidPixels;
                    errorMask.setPixel(x, y, invalidPixelColor);
                }
            }
        }

    // don't just hide failures
    if (errorCount > errorFloodThreshold)
        logStash.messages.push_back(
            std::string("Omitted " + de::toString(errorCount - errorFloodThreshold) + " pixel error description(s)."));

    logStash.success       = (invalidPixels == 0);
    logStash.invalidPixels = invalidPixels;

    // report result
    if (!logStash.success)
        logStash.errorMask = errorMask;

    return logStash.success;
}

float calculateIntersectionParameter(const tcu::Vec2 line[2], float w, int componentNdx)
{
    DE_ASSERT(componentNdx < 2);
    if (line[1][componentNdx] == line[0][componentNdx])
        return -1.0f;

    return (w - line[0][componentNdx]) / (line[1][componentNdx] - line[0][componentNdx]);
}

// Clips the given line with a ((-w, -w), (-w, w), (w, w), (w, -w)) rectangle
void applyClippingBox(tcu::Vec2 line[2], float w)
{
    for (int side = 0; side < 4; ++side)
    {
        const int sign      = ((side / 2) * -2) + 1;
        const int component = side % 2;
        const float t       = calculateIntersectionParameter(line, w * (float)sign, component);

        if ((t > 0) && (t < 1))
        {
            const float newCoord = t * line[1][1 - component] + (1 - t) * line[0][1 - component];

            if (line[1][component] > (w * (float)sign))
            {
                line[1 - side / 2][component]     = w * (float)sign;
                line[1 - side / 2][1 - component] = newCoord;
            }
            else
            {
                line[side / 2][component]     = w * (float)sign;
                line[side / 2][1 - component] = newCoord;
            }
        }
    }
}

enum ClipMode
{
    CLIPMODE_NO_CLIPPING = 0,
    CLIPMODE_USE_CLIPPING_BOX,

    CLIPMODE_LAST
};

bool verifyMultisampleLineGroupRasterization(const tcu::Surface &surface, const LineSceneSpec &scene,
                                             const RasterizationArguments &args, tcu::TestLog &log, ClipMode clipMode,
                                             VerifyTriangleGroupRasterizationLogStash *logStash,
                                             const bool vulkanLinesTest, const bool strictMode,
                                             const bool carryRemainder)
{
    // Multisampled line == 2 triangles

    const tcu::Vec2 viewportSize = tcu::Vec2((float)surface.getWidth(), (float)surface.getHeight());
    const float halfLineWidth    = scene.lineWidth * 0.5f;
    TriangleSceneSpec triangleScene;

    uint32_t stippleCounter = 0;
    float leftoverPhase     = 0.0f;

    triangleScene.triangles.resize(2 * scene.lines.size());
    for (int lineNdx = 0; lineNdx < (int)scene.lines.size(); ++lineNdx)
    {

        if (!scene.isStrip)
        {
            // reset stipple at the start of each line segment
            stippleCounter = 0;
            leftoverPhase  = 0;
        }

        // Transform to screen space, add pixel offsets, convert back to normalized device space, and test as triangles
        tcu::Vec2 lineNormalizedDeviceSpace[2] = {
            tcu::Vec2(scene.lines[lineNdx].positions[0].x() / scene.lines[lineNdx].positions[0].w(),
                      scene.lines[lineNdx].positions[0].y() / scene.lines[lineNdx].positions[0].w()),
            tcu::Vec2(scene.lines[lineNdx].positions[1].x() / scene.lines[lineNdx].positions[1].w(),
                      scene.lines[lineNdx].positions[1].y() / scene.lines[lineNdx].positions[1].w()),
        };

        if (clipMode == CLIPMODE_USE_CLIPPING_BOX)
        {
            applyClippingBox(lineNormalizedDeviceSpace, 1.0f);
        }

        const tcu::Vec2 lineScreenSpace[2] = {
            (lineNormalizedDeviceSpace[0] + tcu::Vec2(1.0f, 1.0f)) * 0.5f * viewportSize,
            (lineNormalizedDeviceSpace[1] + tcu::Vec2(1.0f, 1.0f)) * 0.5f * viewportSize,
        };

        const tcu::Vec2 lineDir       = tcu::normalize(lineScreenSpace[1] - lineScreenSpace[0]);
        const tcu::Vec2 lineNormalDir = (strictMode || scene.isRectangular) ? tcu::Vec2(lineDir.y(), -lineDir.x()) :
                                        isLineXMajor(lineScreenSpace[0], lineScreenSpace[1]) ? tcu::Vec2(0.0f, 1.0f) :
                                                                                               tcu::Vec2(1.0f, 0.0f);

        if (scene.stippleEnable)
        {
            float lineLength       = tcu::distance(lineScreenSpace[0], lineScreenSpace[1]);
            float lineOffset       = 0.0f;
            float lineSegmentStart = -1.0f;
            float lineSegmentEnd   = -1.0f;
            bool prevStipplePass   = false;

            while (lineOffset < lineLength)
            {
                float d0 = (float)lineOffset;
                float d1 = d0 + 1.0f;

                if (carryRemainder)
                {
                    // "leftoverPhase" carries over a fractional stipple phase that was "unused"
                    // by the last line segment in the strip, if it wasn't an integer length.
                    if (leftoverPhase > lineLength)
                    {
                        DE_ASSERT(d0 == 0.0f);
                        d1 = lineLength;
                        leftoverPhase -= lineLength;
                    }
                    else if (leftoverPhase != 0.0f)
                    {
                        DE_ASSERT(d0 == 0.0f);
                        d1            = leftoverPhase;
                        leftoverPhase = 0.0f;
                    }
                    else
                    {
                        if (d0 + 1.0f > lineLength)
                        {
                            d1            = lineLength;
                            leftoverPhase = d0 + 1.0f - lineLength;
                        }
                        else
                            d1 = d0 + 1.0f;
                    }
                }
                else
                {
                    if (d1 > lineLength)
                        d1 = lineLength;
                }

                // set offset for next iteration
                lineOffset = d1;

                int stippleBit   = (stippleCounter / scene.stippleFactor) % 16;
                bool stipplePass = (scene.stipplePattern & (1 << stippleBit)) != 0;

                if (leftoverPhase == 0)
                    stippleCounter++;

                if (stipplePass)
                {
                    // start new segment
                    if (!prevStipplePass)
                        lineSegmentStart = d0;

                    // extend current segment
                    lineSegmentEnd = d1;
                }

                // update state
                prevStipplePass = stipplePass;

                // keep accumulating lines
                if ((stipplePass && lineOffset < lineLength) || lineSegmentStart == lineSegmentEnd)
                {
                    continue;
                }

                d0               = lineSegmentStart / lineLength;
                d1               = lineSegmentEnd / lineLength;
                lineSegmentStart = lineSegmentEnd = -1.0;

                tcu::Vec2 l0 = mix(lineScreenSpace[0], lineScreenSpace[1], d0);
                tcu::Vec2 l1 = mix(lineScreenSpace[0], lineScreenSpace[1], d1);

                const tcu::Vec2 lineQuadScreenSpace[4] = {
                    l0 + lineNormalDir * halfLineWidth,
                    l0 - lineNormalDir * halfLineWidth,
                    l1 - lineNormalDir * halfLineWidth,
                    l1 + lineNormalDir * halfLineWidth,
                };
                const tcu::Vec2 lineQuadNormalizedDeviceSpace[4] = {
                    lineQuadScreenSpace[0] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
                    lineQuadScreenSpace[1] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
                    lineQuadScreenSpace[2] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
                    lineQuadScreenSpace[3] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
                };

                TriangleSceneSpec::SceneTriangle tri;

                tri.positions[0] =
                    tcu::Vec4(lineQuadNormalizedDeviceSpace[0].x(), lineQuadNormalizedDeviceSpace[0].y(), 0.0f, 1.0f);
                tri.sharedEdge[0] = false;
                tri.positions[1] =
                    tcu::Vec4(lineQuadNormalizedDeviceSpace[1].x(), lineQuadNormalizedDeviceSpace[1].y(), 0.0f, 1.0f);
                tri.sharedEdge[1] = false;
                tri.positions[2] =
                    tcu::Vec4(lineQuadNormalizedDeviceSpace[2].x(), lineQuadNormalizedDeviceSpace[2].y(), 0.0f, 1.0f);
                tri.sharedEdge[2] = true;

                triangleScene.triangles.push_back(tri);

                tri.positions[0] =
                    tcu::Vec4(lineQuadNormalizedDeviceSpace[0].x(), lineQuadNormalizedDeviceSpace[0].y(), 0.0f, 1.0f);
                tri.sharedEdge[0] = true;
                tri.positions[1] =
                    tcu::Vec4(lineQuadNormalizedDeviceSpace[2].x(), lineQuadNormalizedDeviceSpace[2].y(), 0.0f, 1.0f);
                tri.sharedEdge[1] = false;
                tri.positions[2] =
                    tcu::Vec4(lineQuadNormalizedDeviceSpace[3].x(), lineQuadNormalizedDeviceSpace[3].y(), 0.0f, 1.0f);
                tri.sharedEdge[2] = false;

                triangleScene.triangles.push_back(tri);
            }
            DE_ASSERT(lineSegmentStart < 0);
            DE_ASSERT(lineSegmentEnd < 0);
        }
        else
        {
            const tcu::Vec2 lineQuadScreenSpace[4] = {
                lineScreenSpace[0] + lineNormalDir * halfLineWidth,
                lineScreenSpace[0] - lineNormalDir * halfLineWidth,
                lineScreenSpace[1] - lineNormalDir * halfLineWidth,
                lineScreenSpace[1] + lineNormalDir * halfLineWidth,
            };
            const tcu::Vec2 lineQuadNormalizedDeviceSpace[4] = {
                lineQuadScreenSpace[0] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
                lineQuadScreenSpace[1] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
                lineQuadScreenSpace[2] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
                lineQuadScreenSpace[3] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
            };

            triangleScene.triangles[lineNdx * 2 + 0].positions[0] =
                tcu::Vec4(lineQuadNormalizedDeviceSpace[0].x(), lineQuadNormalizedDeviceSpace[0].y(), 0.0f, 1.0f);
            triangleScene.triangles[lineNdx * 2 + 0].sharedEdge[0] = false;
            triangleScene.triangles[lineNdx * 2 + 0].positions[1] =
                tcu::Vec4(lineQuadNormalizedDeviceSpace[1].x(), lineQuadNormalizedDeviceSpace[1].y(), 0.0f, 1.0f);
            triangleScene.triangles[lineNdx * 2 + 0].sharedEdge[1] = false;
            triangleScene.triangles[lineNdx * 2 + 0].positions[2] =
                tcu::Vec4(lineQuadNormalizedDeviceSpace[2].x(), lineQuadNormalizedDeviceSpace[2].y(), 0.0f, 1.0f);
            triangleScene.triangles[lineNdx * 2 + 0].sharedEdge[2] = true;

            triangleScene.triangles[lineNdx * 2 + 1].positions[0] =
                tcu::Vec4(lineQuadNormalizedDeviceSpace[0].x(), lineQuadNormalizedDeviceSpace[0].y(), 0.0f, 1.0f);
            triangleScene.triangles[lineNdx * 2 + 1].sharedEdge[0] = true;
            triangleScene.triangles[lineNdx * 2 + 1].positions[1] =
                tcu::Vec4(lineQuadNormalizedDeviceSpace[2].x(), lineQuadNormalizedDeviceSpace[2].y(), 0.0f, 1.0f);
            triangleScene.triangles[lineNdx * 2 + 1].sharedEdge[1] = false;
            triangleScene.triangles[lineNdx * 2 + 1].positions[2] =
                tcu::Vec4(lineQuadNormalizedDeviceSpace[3].x(), lineQuadNormalizedDeviceSpace[3].y(), 0.0f, 1.0f);
            triangleScene.triangles[lineNdx * 2 + 1].sharedEdge[2] = false;
        }
    }

    if (logStash != nullptr)
    {
        logStash->messages.push_back(
            "Rasterization clipping mode: " +
            std::string(clipMode == CLIPMODE_USE_CLIPPING_BOX ? "CLIPMODE_USE_CLIPPING_BOX" : "CLIPMODE_NO_CLIPPING") +
            ".");
        logStash->messages.push_back(
            "Rasterization line draw strictness mode: " + std::string(strictMode ? "strict" : "non-strict") + ".");
    }

    return verifyTriangleGroupRasterization(surface, triangleScene, args, log, scene.verificationMode, logStash,
                                            vulkanLinesTest);
}

bool verifyMultisampleLineGroupRasterization(const tcu::Surface &surface, const LineSceneSpec &scene,
                                             const RasterizationArguments &args, tcu::TestLog &log, ClipMode clipMode,
                                             VerifyTriangleGroupRasterizationLogStash *logStash,
                                             const bool vulkanLinesTest, const bool strictMode)
{
    if (scene.stippleEnable)
        return verifyMultisampleLineGroupRasterization(surface, scene, args, log, clipMode, logStash, vulkanLinesTest,
                                                       strictMode, true) ||
               verifyMultisampleLineGroupRasterization(surface, scene, args, log, clipMode, logStash, vulkanLinesTest,
                                                       strictMode, false);
    else
        return verifyMultisampleLineGroupRasterization(surface, scene, args, log, clipMode, logStash, vulkanLinesTest,
                                                       strictMode, true);
}

static bool verifyMultisampleLineGroupInterpolationInternal(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                            const RasterizationArguments &args,
                                                            VerifyTriangleGroupInterpolationLogStash &logStash,
                                                            const bool strictMode)
{
    // Multisampled line == 2 triangles

    const tcu::Vec2 viewportSize = tcu::Vec2((float)surface.getWidth(), (float)surface.getHeight());
    const float halfLineWidth    = scene.lineWidth * 0.5f;
    TriangleSceneSpec triangleScene;

    triangleScene.triangles.resize(2 * scene.lines.size());
    for (int lineNdx = 0; lineNdx < (int)scene.lines.size(); ++lineNdx)
    {
        // Need the w-coordinates a couple of times
        const float wa = scene.lines[lineNdx].positions[0].w();
        const float wb = scene.lines[lineNdx].positions[1].w();

        // Transform to screen space, add pixel offsets, convert back to normalized device space, and test as triangles
        const tcu::Vec2 lineNormalizedDeviceSpace[2] = {
            tcu::Vec2(scene.lines[lineNdx].positions[0].x() / wa, scene.lines[lineNdx].positions[0].y() / wa),
            tcu::Vec2(scene.lines[lineNdx].positions[1].x() / wb, scene.lines[lineNdx].positions[1].y() / wb),
        };
        const tcu::Vec2 lineScreenSpace[2] = {
            (lineNormalizedDeviceSpace[0] + tcu::Vec2(1.0f, 1.0f)) * 0.5f * viewportSize,
            (lineNormalizedDeviceSpace[1] + tcu::Vec2(1.0f, 1.0f)) * 0.5f * viewportSize,
        };

        const tcu::Vec2 lineDir       = tcu::normalize(lineScreenSpace[1] - lineScreenSpace[0]);
        const tcu::Vec2 lineNormalDir = (strictMode || scene.isRectangular) ? tcu::Vec2(lineDir.y(), -lineDir.x()) :
                                        isLineXMajor(lineScreenSpace[0], lineScreenSpace[1]) ? tcu::Vec2(0.0f, 1.0f) :
                                                                                               tcu::Vec2(1.0f, 0.0f);

        const tcu::Vec2 lineQuadScreenSpace[4] = {
            lineScreenSpace[0] + lineNormalDir * halfLineWidth,
            lineScreenSpace[0] - lineNormalDir * halfLineWidth,
            lineScreenSpace[1] - lineNormalDir * halfLineWidth,
            lineScreenSpace[1] + lineNormalDir * halfLineWidth,
        };
        const tcu::Vec2 lineQuadNormalizedDeviceSpace[4] = {
            lineQuadScreenSpace[0] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
            lineQuadScreenSpace[1] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
            lineQuadScreenSpace[2] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
            lineQuadScreenSpace[3] / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
        };

        // Re-construct un-projected geometry using the quantised positions
        const tcu::Vec4 lineQuadUnprojected[4] = {
            tcu::Vec4(lineQuadNormalizedDeviceSpace[0].x() * wa, lineQuadNormalizedDeviceSpace[0].y() * wa, 0.0f, wa),
            tcu::Vec4(lineQuadNormalizedDeviceSpace[1].x() * wa, lineQuadNormalizedDeviceSpace[1].y() * wa, 0.0f, wa),
            tcu::Vec4(lineQuadNormalizedDeviceSpace[2].x() * wb, lineQuadNormalizedDeviceSpace[2].y() * wb, 0.0f, wb),
            tcu::Vec4(lineQuadNormalizedDeviceSpace[3].x() * wb, lineQuadNormalizedDeviceSpace[3].y() * wb, 0.0f, wb),
        };

        triangleScene.triangles[lineNdx * 2 + 0].positions[0] = lineQuadUnprojected[0];
        triangleScene.triangles[lineNdx * 2 + 0].positions[1] = lineQuadUnprojected[1];
        triangleScene.triangles[lineNdx * 2 + 0].positions[2] = lineQuadUnprojected[2];

        triangleScene.triangles[lineNdx * 2 + 0].sharedEdge[0] = false;
        triangleScene.triangles[lineNdx * 2 + 0].sharedEdge[1] = false;
        triangleScene.triangles[lineNdx * 2 + 0].sharedEdge[2] = true;

        triangleScene.triangles[lineNdx * 2 + 0].colors[0] = scene.lines[lineNdx].colors[0];
        triangleScene.triangles[lineNdx * 2 + 0].colors[1] = scene.lines[lineNdx].colors[0];
        triangleScene.triangles[lineNdx * 2 + 0].colors[2] = scene.lines[lineNdx].colors[1];

        triangleScene.triangles[lineNdx * 2 + 1].positions[0] = lineQuadUnprojected[0];
        triangleScene.triangles[lineNdx * 2 + 1].positions[1] = lineQuadUnprojected[2];
        triangleScene.triangles[lineNdx * 2 + 1].positions[2] = lineQuadUnprojected[3];

        triangleScene.triangles[lineNdx * 2 + 1].sharedEdge[0] = true;
        triangleScene.triangles[lineNdx * 2 + 1].sharedEdge[1] = false;
        triangleScene.triangles[lineNdx * 2 + 1].sharedEdge[2] = false;

        triangleScene.triangles[lineNdx * 2 + 1].colors[0] = scene.lines[lineNdx].colors[0];
        triangleScene.triangles[lineNdx * 2 + 1].colors[1] = scene.lines[lineNdx].colors[1];
        triangleScene.triangles[lineNdx * 2 + 1].colors[2] = scene.lines[lineNdx].colors[1];
    }

    if (strictMode)
    {
        // Strict mode interpolation should be purely in the direction of the line-segment
        logStash.messages.push_back("Verify using line interpolator");
        return verifyTriangleGroupInterpolationWithInterpolator(surface, triangleScene, args, logStash,
                                                                MultisampleLineInterpolator(scene));
    }
    else
    {
        // For non-strict lines some allowance needs to be inplace for a few different styles of implementation.
        //
        // Some implementations duplicate the attributes at the endpoints to the corners of the triangle
        // deconstruted parallelogram. Gradients along the line will be seen to travel in the major axis,
        // with values effectively duplicated in the minor axis direction. In other cases, implementations
        // will use the original parameters of the line to calculate attribute interpolation so it will
        // follow the direction of the line-segment.
        logStash.messages.push_back("Verify using triangle interpolator");
        if (!verifyTriangleGroupInterpolationWithInterpolator(surface, triangleScene, args, logStash,
                                                              TriangleInterpolator(triangleScene)))
        {
            logStash.messages.push_back("Verify using line interpolator");
            return verifyTriangleGroupInterpolationWithInterpolator(surface, triangleScene, args, logStash,
                                                                    MultisampleLineInterpolator(scene));
        }
        return true;
    }
}

static void logTriangleGroupnterpolationStash(const tcu::Surface &surface, tcu::TestLog &log,
                                              VerifyTriangleGroupInterpolationLogStash &logStash)
{
    // Output results
    log << tcu::TestLog::Message << "Verifying rasterization result." << tcu::TestLog::EndMessage;

    for (size_t msgNdx = 0; msgNdx < logStash.messages.size(); ++msgNdx)
        log << tcu::TestLog::Message << logStash.messages[msgNdx] << tcu::TestLog::EndMessage;

    // report result
    if (!logStash.success)
    {
        log << tcu::TestLog::Message << logStash.invalidPixels << " invalid pixel(s) found."
            << tcu::TestLog::EndMessage;
        log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
            << tcu::TestLog::Image("Result", "Result", surface)
            << tcu::TestLog::Image("ErrorMask", "ErrorMask", logStash.errorMask) << tcu::TestLog::EndImageSet;
    }
    else
    {
        log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
        log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
            << tcu::TestLog::Image("Result", "Result", surface) << tcu::TestLog::EndImageSet;
    }
}

static bool verifyMultisampleLineGroupInterpolation(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                    const RasterizationArguments &args, tcu::TestLog &log,
                                                    const bool strictMode                      = true,
                                                    const bool allowBresenhamForNonStrictLines = false)
{
    bool result = false;
    VerifyTriangleGroupInterpolationLogStash nonStrictModeLogStash;
    VerifyTriangleGroupInterpolationLogStash strictModeLogStash;

    nonStrictModeLogStash.messages.push_back("Non-strict line draw mode.");
    strictModeLogStash.messages.push_back("Strict mode line draw mode.");

    if (strictMode)
    {
        result = verifyMultisampleLineGroupInterpolationInternal(surface, scene, args, strictModeLogStash, strictMode);

        logTriangleGroupnterpolationStash(surface, log, strictModeLogStash);
    }
    else
    {
        if (verifyMultisampleLineGroupInterpolationInternal(surface, scene, args, nonStrictModeLogStash, false))
        {
            logTriangleGroupnterpolationStash(surface, log, nonStrictModeLogStash);

            result = true;
        }
        else if (verifyMultisampleLineGroupInterpolationInternal(surface, scene, args, strictModeLogStash, true))
        {
            logTriangleGroupnterpolationStash(surface, log, strictModeLogStash);

            result = true;
        }
        else
        {
            logTriangleGroupnterpolationStash(surface, log, nonStrictModeLogStash);
            logTriangleGroupnterpolationStash(surface, log, strictModeLogStash);
        }

        // In the non-strict line case, bresenham is also permissable, though not specified. This is due
        // to a change in how lines are specified in Vulkan versus GLES; in GLES bresenham lines using the
        // diamond-exit rule were the preferred way to draw single pixel non-antialiased lines, and not all
        // GLES implementations are able to disable this behaviour.
        if (result == false)
        {
            log << tcu::TestLog::Message
                << "Checking line rasterisation using verifySinglesampleNarrowLineGroupInterpolation for nonStrict "
                   "lines"
                << tcu::TestLog::EndMessage;
            if (args.numSamples <= 1 && allowBresenhamForNonStrictLines &&
                verifyLineGroupInterpolationWithProjectedWeights(surface, scene, args, log))
            {
                log << tcu::TestLog::Message
                    << "verifySinglesampleNarrowLineGroupInterpolation for nonStrict lines Passed"
                    << tcu::TestLog::EndMessage;

                result = true;
            }
        }
    }

    return result;
}

bool verifyMultisamplePointGroupRasterization(const tcu::Surface &surface, const PointSceneSpec &scene,
                                              const RasterizationArguments &args, tcu::TestLog &log)
{
    // Multisampled point == 2 triangles

    const tcu::Vec2 viewportSize = tcu::Vec2((float)surface.getWidth(), (float)surface.getHeight());
    TriangleSceneSpec triangleScene;

    triangleScene.triangles.resize(2 * scene.points.size());
    for (int pointNdx = 0; pointNdx < (int)scene.points.size(); ++pointNdx)
    {
        // Transform to screen space, add pixel offsets, convert back to normalized device space, and test as triangles
        const tcu::Vec2 pointNormalizedDeviceSpace =
            tcu::Vec2(scene.points[pointNdx].position.x() / scene.points[pointNdx].position.w(),
                      scene.points[pointNdx].position.y() / scene.points[pointNdx].position.w());
        const tcu::Vec2 pointScreenSpace = (pointNormalizedDeviceSpace + tcu::Vec2(1.0f, 1.0f)) * 0.5f * viewportSize;
        const float offset               = scene.points[pointNdx].pointSize * 0.5f;
        const tcu::Vec2 lineQuadNormalizedDeviceSpace[4] = {
            (pointScreenSpace + tcu::Vec2(-offset, -offset)) / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
            (pointScreenSpace + tcu::Vec2(-offset, offset)) / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
            (pointScreenSpace + tcu::Vec2(offset, offset)) / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
            (pointScreenSpace + tcu::Vec2(offset, -offset)) / viewportSize * 2.0f - tcu::Vec2(1.0f, 1.0f),
        };

        triangleScene.triangles[pointNdx * 2 + 0].positions[0] =
            tcu::Vec4(lineQuadNormalizedDeviceSpace[0].x(), lineQuadNormalizedDeviceSpace[0].y(), 0.0f, 1.0f);
        triangleScene.triangles[pointNdx * 2 + 0].sharedEdge[0] = false;
        triangleScene.triangles[pointNdx * 2 + 0].positions[1] =
            tcu::Vec4(lineQuadNormalizedDeviceSpace[1].x(), lineQuadNormalizedDeviceSpace[1].y(), 0.0f, 1.0f);
        triangleScene.triangles[pointNdx * 2 + 0].sharedEdge[1] = false;
        triangleScene.triangles[pointNdx * 2 + 0].positions[2] =
            tcu::Vec4(lineQuadNormalizedDeviceSpace[2].x(), lineQuadNormalizedDeviceSpace[2].y(), 0.0f, 1.0f);
        triangleScene.triangles[pointNdx * 2 + 0].sharedEdge[2] = true;

        triangleScene.triangles[pointNdx * 2 + 1].positions[0] =
            tcu::Vec4(lineQuadNormalizedDeviceSpace[0].x(), lineQuadNormalizedDeviceSpace[0].y(), 0.0f, 1.0f);
        triangleScene.triangles[pointNdx * 2 + 1].sharedEdge[0] = true;
        triangleScene.triangles[pointNdx * 2 + 1].positions[1] =
            tcu::Vec4(lineQuadNormalizedDeviceSpace[2].x(), lineQuadNormalizedDeviceSpace[2].y(), 0.0f, 1.0f);
        triangleScene.triangles[pointNdx * 2 + 1].sharedEdge[1] = false;
        triangleScene.triangles[pointNdx * 2 + 1].positions[2] =
            tcu::Vec4(lineQuadNormalizedDeviceSpace[3].x(), lineQuadNormalizedDeviceSpace[3].y(), 0.0f, 1.0f);
        triangleScene.triangles[pointNdx * 2 + 1].sharedEdge[2] = false;
    }

    return verifyTriangleGroupRasterization(surface, triangleScene, args, log);
}

void genScreenSpaceLines(std::vector<tcu::Vec4> &screenspaceLines, const std::vector<LineSceneSpec::SceneLine> &lines,
                         const tcu::IVec2 &viewportSize)
{
    DE_ASSERT(screenspaceLines.size() == lines.size());

    for (int lineNdx = 0; lineNdx < (int)lines.size(); ++lineNdx)
    {
        const tcu::Vec2 lineNormalizedDeviceSpace[2] = {
            tcu::Vec2(lines[lineNdx].positions[0].x() / lines[lineNdx].positions[0].w(),
                      lines[lineNdx].positions[0].y() / lines[lineNdx].positions[0].w()),
            tcu::Vec2(lines[lineNdx].positions[1].x() / lines[lineNdx].positions[1].w(),
                      lines[lineNdx].positions[1].y() / lines[lineNdx].positions[1].w()),
        };
        const tcu::Vec4 lineScreenSpace[2] = {
            tcu::Vec4((lineNormalizedDeviceSpace[0].x() + 1.0f) * 0.5f * (float)viewportSize.x(),
                      (lineNormalizedDeviceSpace[0].y() + 1.0f) * 0.5f * (float)viewportSize.y(), 0.0f, 1.0f),
            tcu::Vec4((lineNormalizedDeviceSpace[1].x() + 1.0f) * 0.5f * (float)viewportSize.x(),
                      (lineNormalizedDeviceSpace[1].y() + 1.0f) * 0.5f * (float)viewportSize.y(), 0.0f, 1.0f),
        };

        screenspaceLines[lineNdx] =
            tcu::Vec4(lineScreenSpace[0].x(), lineScreenSpace[0].y(), lineScreenSpace[1].x(), lineScreenSpace[1].y());
    }
}

bool verifySinglesampleLineGroupRasterization(const tcu::Surface &surface, const LineSceneSpec &scene,
                                              const RasterizationArguments &args, tcu::TestLog &log)
{
    DE_ASSERT(scene.lines.size() < 255); // indices are stored as unsigned 8-bit ints

    bool allOK               = true;
    bool overdrawInReference = false;
    int referenceFragments   = 0;
    int resultFragments      = 0;
    int lineWidth            = deFloorFloatToInt32(scene.lineWidth + 0.5f);
    bool lineWidthHasFrac    = deFloatFrac(scene.lineWidth) > 0;
    std::vector<bool> lineIsXMajor(scene.lines.size());
    std::vector<tcu::Vec4> screenspaceLines(scene.lines.size());

    // Reference renderer produces correct fragments using the diamond-rule. Make 2D int array, each cell contains the highest index (first index = 1) of the overlapping lines or 0 if no line intersects the pixel
    tcu::TextureLevel referenceLineMap(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::UNSIGNED_INT8),
                                       surface.getWidth(), surface.getHeight());
    tcu::clear(referenceLineMap.getAccess(), tcu::IVec4(0, 0, 0, 0));

    tcu::Surface errorMask(surface.getWidth(), surface.getHeight());
    tcu::clear(errorMask.getAccess(), tcu::IVec4(0, 255, 0, 255));

    genScreenSpaceLines(screenspaceLines, scene.lines, tcu::IVec2(surface.getWidth(), surface.getHeight()));

    rr::SingleSampleLineRasterizer rasterizer(tcu::IVec4(0, 0, surface.getWidth(), surface.getHeight()),
                                              args.subpixelBits);
    for (int lineNdx = 0; lineNdx < (int)scene.lines.size(); ++lineNdx)
    {
        rasterizer.init(tcu::Vec4(screenspaceLines[lineNdx][0], screenspaceLines[lineNdx][1], 0.0f, 1.0f),
                        tcu::Vec4(screenspaceLines[lineNdx][2], screenspaceLines[lineNdx][3], 0.0f, 1.0f),
                        scene.lineWidth, scene.stippleFactor, scene.stipplePattern);

        if (!scene.isStrip)
            rasterizer.resetStipple();

        // calculate majority of later use
        lineIsXMajor[lineNdx] = isPackedSSLineXMajor(screenspaceLines[lineNdx]);

        for (;;)
        {
            const int maxPackets = 32;
            int numRasterized    = 0;
            rr::FragmentPacket packets[maxPackets];

            rasterizer.rasterize(packets, nullptr, maxPackets, numRasterized);

            for (int packetNdx = 0; packetNdx < numRasterized; ++packetNdx)
            {
                for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                {
                    if ((uint32_t)packets[packetNdx].coverage & (1 << fragNdx))
                    {
                        const tcu::IVec2 fragPos = packets[packetNdx].position + tcu::IVec2(fragNdx % 2, fragNdx / 2);

                        // Check for overdraw
                        if (!overdrawInReference)
                            overdrawInReference =
                                referenceLineMap.getAccess().getPixelInt(fragPos.x(), fragPos.y()).x() != 0;

                        // Output pixel
                        referenceLineMap.getAccess().setPixel(tcu::IVec4(lineNdx + 1, 0, 0, 0), fragPos.x(),
                                                              fragPos.y());
                    }
                }
            }

            if (numRasterized != maxPackets)
                break;
        }
    }

    // Requirement 1: The coordinates of a fragment produced by the algorithm may not deviate by more than one unit
    bool missingFragments = false;
    {
        log << tcu::TestLog::Message << "Searching for deviating fragments." << tcu::TestLog::EndMessage;

        for (int y = 0; y < referenceLineMap.getHeight(); ++y)
            for (int x = 0; x < referenceLineMap.getWidth(); ++x)
            {
                const bool reference = referenceLineMap.getAccess().getPixelInt(x, y).x() != 0;
                const bool result    = compareColors(surface.getPixel(x, y), tcu::RGBA::white(), args.redBits,
                                                     args.greenBits, args.blueBits);

                if (reference)
                    ++referenceFragments;
                if (result)
                    ++resultFragments;

                if (reference == result)
                    continue;

                // Reference fragment here, matching result fragment must be nearby
                if (reference && !result)
                {
                    bool foundFragment = false;

                    if (x == 0 || y == 0 || x == referenceLineMap.getWidth() - 1 ||
                        y == referenceLineMap.getHeight() - 1)
                    {
                        // image boundary, missing fragment could be over the image edge
                        foundFragment = true;
                    }

                    // find nearby fragment
                    for (int dy = -1; dy < 2 && !foundFragment; ++dy)
                        for (int dx = -1; dx < 2 && !foundFragment; ++dx)
                        {
                            if (compareColors(surface.getPixel(x + dx, y + dy), tcu::RGBA::white(), args.redBits,
                                              args.greenBits, args.blueBits))
                                foundFragment = true;
                        }

                    if (!foundFragment)
                    {
                        missingFragments = true;
                        errorMask.setPixel(x, y, tcu::RGBA::red());
                    }
                }
            }

        if (missingFragments)
        {
            log << tcu::TestLog::Message << "Invalid deviations found - missing fragments." << tcu::TestLog::EndMessage;
            allOK = false;
        }
        else
        {
            log << tcu::TestLog::Message << "No invalid deviations found." << tcu::TestLog::EndMessage;
        }
    }

    // Requirement 2: The total number of fragments produced by the algorithm may differ from
    //                that produced by the diamond-exit rule by no more than one.
    {
        // Check is not valid if the primitives intersect or otherwise share same fragments
        if (!overdrawInReference)
        {
            int allowedDeviation =
                (int)scene.lines.size() * lineWidth; // one pixel per primitive in the major direction

            log << tcu::TestLog::Message << "Verifying fragment counts:\n"
                << "\tDiamond-exit rule: " << referenceFragments << " fragments.\n"
                << "\tResult image: " << resultFragments << " fragments.\n"
                << "\tAllowing deviation of " << allowedDeviation << " fragments.\n"
                << tcu::TestLog::EndMessage;

            if (deAbs32(referenceFragments - resultFragments) > allowedDeviation)
            {
                tcu::Surface reference(surface.getWidth(), surface.getHeight());

                // show a helpful reference image
                tcu::clear(reference.getAccess(), tcu::IVec4(0, 0, 0, 255));
                for (int y = 0; y < surface.getHeight(); ++y)
                    for (int x = 0; x < surface.getWidth(); ++x)
                        if (referenceLineMap.getAccess().getPixelInt(x, y).x())
                            reference.setPixel(x, y, tcu::RGBA::white());

                log << tcu::TestLog::Message << "Invalid fragment count in result image." << tcu::TestLog::EndMessage;
                log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
                    << tcu::TestLog::Image("Reference", "Reference", reference)
                    << tcu::TestLog::Image("Result", "Result", surface) << tcu::TestLog::EndImageSet;

                allOK = false;
            }
            else
            {
                log << tcu::TestLog::Message << "Fragment count is valid." << tcu::TestLog::EndMessage;
            }
        }
        else
        {
            log << tcu::TestLog::Message
                << "Overdraw in scene. Fragment count cannot be verified. Skipping fragment count checks."
                << tcu::TestLog::EndMessage;
        }
    }

    // Requirement 3: Line width must be constant
    {
        bool invalidWidthFound = false;

        log << tcu::TestLog::Message << "Verifying line widths of the x-major lines." << tcu::TestLog::EndMessage;
        for (int y = 1; y < referenceLineMap.getHeight() - 1; ++y)
        {
            bool fullyVisibleLine       = false;
            bool previousPixelUndefined = false;
            int currentLine             = 0;
            int currentWidth            = 1;

            for (int x = 1; x < referenceLineMap.getWidth() - 1; ++x)
            {
                const bool result = compareColors(surface.getPixel(x, y), tcu::RGBA::white(), args.redBits,
                                                  args.greenBits, args.blueBits);
                int lineID        = 0;

                // Which line does this fragment belong to?

                if (result)
                {
                    bool multipleNearbyLines = false;
                    bool renderAtSurfaceEdge = false;

                    renderAtSurfaceEdge = (x == 1) || (x == referenceLineMap.getWidth() - 2);

                    for (int dy = -1; dy < 2; ++dy)
                        for (int dx = -1; dx < 2; ++dx)
                        {
                            const int nearbyID = referenceLineMap.getAccess().getPixelInt(x + dx, y + dy).x();
                            if (nearbyID)
                            {
                                if (lineID && lineID != nearbyID)
                                    multipleNearbyLines = true;
                            }
                        }

                    if (multipleNearbyLines || renderAtSurfaceEdge)
                    {
                        // Another line is too close, don't try to calculate width here
                        // Or the render result is outside of surface range
                        previousPixelUndefined = true;
                        continue;
                    }
                }

                // Only line with id of lineID is nearby

                if (previousPixelUndefined)
                {
                    // The line might have been overdrawn or not
                    currentLine            = lineID;
                    currentWidth           = 1;
                    fullyVisibleLine       = false;
                    previousPixelUndefined = false;
                }
                else if (lineID == currentLine)
                {
                    // Current line continues
                    ++currentWidth;
                }
                else if (lineID > currentLine)
                {
                    // Another line was drawn over or the line ends
                    currentLine      = lineID;
                    currentWidth     = 1;
                    fullyVisibleLine = true;
                }
                else
                {
                    // The line ends
                    if (fullyVisibleLine && !lineIsXMajor[currentLine - 1])
                    {
                        // check width
                        if (lineWidthHasFrac && currentWidth + 1 == lineWidth)
                        {
                            log << tcu::TestLog::Message << "\tAllowing width of " << currentWidth
                                << " due to fractional line width" << tcu::TestLog::EndMessage;
                        }
                        else if (currentWidth != lineWidth)
                        {
                            log << tcu::TestLog::Message << "\tInvalid line width at (" << x - currentWidth << ", " << y
                                << ") - (" << x - 1 << ", " << y << "). Detected width of " << currentWidth
                                << ", expected " << lineWidth << tcu::TestLog::EndMessage;
                            invalidWidthFound = true;
                        }
                    }

                    currentLine      = lineID;
                    currentWidth     = 1;
                    fullyVisibleLine = false;
                }
            }
        }

        log << tcu::TestLog::Message << "Verifying line widths of the y-major lines." << tcu::TestLog::EndMessage;
        for (int x = 1; x < referenceLineMap.getWidth() - 1; ++x)
        {
            bool fullyVisibleLine       = false;
            bool previousPixelUndefined = false;
            int currentLine             = 0;
            int currentWidth            = 1;

            for (int y = 1; y < referenceLineMap.getHeight() - 1; ++y)
            {
                const bool result = compareColors(surface.getPixel(x, y), tcu::RGBA::white(), args.redBits,
                                                  args.greenBits, args.blueBits);
                int lineID        = 0;

                // Which line does this fragment belong to?

                if (result)
                {
                    bool multipleNearbyLines = false;
                    bool renderAtSurfaceEdge = false;

                    renderAtSurfaceEdge = (y == 1) || (y == referenceLineMap.getWidth() - 2);

                    for (int dy = -1; dy < 2; ++dy)
                        for (int dx = -1; dx < 2; ++dx)
                        {
                            const int nearbyID = referenceLineMap.getAccess().getPixelInt(x + dx, y + dy).x();
                            if (nearbyID)
                            {
                                if (lineID && lineID != nearbyID)
                                    multipleNearbyLines = true;
                                lineID = nearbyID;
                            }
                        }

                    if (multipleNearbyLines || renderAtSurfaceEdge)
                    {
                        // Another line is too close, don't try to calculate width here
                        // Or the render result is outside of surface range
                        previousPixelUndefined = true;
                        continue;
                    }
                }

                // Only line with id of lineID is nearby

                if (previousPixelUndefined)
                {
                    // The line might have been overdrawn or not
                    currentLine            = lineID;
                    currentWidth           = 1;
                    fullyVisibleLine       = false;
                    previousPixelUndefined = false;
                }
                else if (lineID == currentLine)
                {
                    // Current line continues
                    ++currentWidth;
                }
                else if (lineID > currentLine)
                {
                    // Another line was drawn over or the line ends
                    currentLine      = lineID;
                    currentWidth     = 1;
                    fullyVisibleLine = true;
                }
                else
                {
                    // The line ends
                    if (fullyVisibleLine && lineIsXMajor[currentLine - 1])
                    {
                        // check width
                        if (lineWidthHasFrac && currentWidth + 1 == lineWidth)
                        {
                            log << tcu::TestLog::Message << "\tAllowing width of " << currentWidth
                                << " due to fractional line width" << tcu::TestLog::EndMessage;
                        }
                        else if (currentWidth != lineWidth)
                        {
                            log << tcu::TestLog::Message << "\tInvalid line width at (" << x << ", " << y - currentWidth
                                << ") - (" << x << ", " << y - 1 << "). Detected width of " << currentWidth
                                << ", expected " << lineWidth << tcu::TestLog::EndMessage;
                            invalidWidthFound = true;
                        }
                    }

                    currentLine      = lineID;
                    currentWidth     = 1;
                    fullyVisibleLine = false;
                }
            }
        }

        if (invalidWidthFound)
        {
            log << tcu::TestLog::Message << "Invalid line width found, image is not valid." << tcu::TestLog::EndMessage;
            allOK = false;
        }
        else
        {
            log << tcu::TestLog::Message << "Line widths are valid." << tcu::TestLog::EndMessage;
        }
    }

    //\todo [2013-10-24 jarkko].
    //Requirement 4. If two line segments share a common endpoint, and both segments are either
    //x-major (both left-to-right or both right-to-left) or y-major (both bottom-totop
    //or both top-to-bottom), then rasterizing both segments may not produce
    //duplicate fragments, nor may any fragments be omitted so as to interrupt
    //continuity of the connected segments.

    {
        tcu::Surface reference(surface.getWidth(), surface.getHeight());
        tcu::clear(reference.getAccess(), tcu::IVec4(0, 0, 0, 255));
        for (int y = 0; y < surface.getHeight(); ++y)
            for (int x = 0; x < surface.getWidth(); ++x)
                if (referenceLineMap.getAccess().getPixelInt(x, y).x())
                    reference.setPixel(x, y, tcu::RGBA::white());
        log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
            << tcu::TestLog::Image("Reference", "Reference", reference)
            << tcu::TestLog::Image("Result", "Result", surface);
        if (missingFragments)
            log << tcu::TestLog::Image("ErrorMask", "ErrorMask", errorMask);
        log << tcu::TestLog::EndImageSet;
    }

    return allOK;
}

struct SingleSampleNarrowLineCandidate
{
    int lineNdx;
    tcu::IVec3 colorMin;
    tcu::IVec3 colorMax;
    tcu::Vec3 colorMinF;
    tcu::Vec3 colorMaxF;
    tcu::Vec3 valueRangeMin;
    tcu::Vec3 valueRangeMax;
};

void setMaskMapCoverageBitForLine(int bitNdx, const tcu::Vec2 &screenSpaceP0, const tcu::Vec2 &screenSpaceP1,
                                  float lineWidth, tcu::PixelBufferAccess maskMap, const int subpixelBits)
{
    enum
    {
        MAX_PACKETS = 32,
    };

    rr::SingleSampleLineRasterizer rasterizer(tcu::IVec4(0, 0, maskMap.getWidth(), maskMap.getHeight()), subpixelBits);
    int numRasterized = MAX_PACKETS;
    rr::FragmentPacket packets[MAX_PACKETS];

    rasterizer.init(tcu::Vec4(screenSpaceP0.x(), screenSpaceP0.y(), 0.0f, 1.0f),
                    tcu::Vec4(screenSpaceP1.x(), screenSpaceP1.y(), 0.0f, 1.0f), lineWidth, 1, 0xFFFF);

    while (numRasterized == MAX_PACKETS)
    {
        rasterizer.rasterize(packets, nullptr, MAX_PACKETS, numRasterized);

        for (int packetNdx = 0; packetNdx < numRasterized; ++packetNdx)
        {
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            {
                if ((uint32_t)packets[packetNdx].coverage & (1 << fragNdx))
                {
                    const tcu::IVec2 fragPos = packets[packetNdx].position + tcu::IVec2(fragNdx % 2, fragNdx / 2);

                    DE_ASSERT(deInBounds32(fragPos.x(), 0, maskMap.getWidth()));
                    DE_ASSERT(deInBounds32(fragPos.y(), 0, maskMap.getHeight()));

                    const uint32_t previousMask = maskMap.getPixelUint(fragPos.x(), fragPos.y()).x();
                    const uint32_t newMask      = (previousMask) | ((uint32_t)1u << bitNdx);

                    maskMap.setPixel(tcu::UVec4(newMask, 0, 0, 0), fragPos.x(), fragPos.y());
                }
            }
        }
    }
}

void setMaskMapCoverageBitForLines(const std::vector<tcu::Vec4> &screenspaceLines, float lineWidth,
                                   tcu::PixelBufferAccess maskMap, const int subpixelBits)
{
    for (int lineNdx = 0; lineNdx < (int)screenspaceLines.size(); ++lineNdx)
    {
        const tcu::Vec2 pa = screenspaceLines[lineNdx].swizzle(0, 1);
        const tcu::Vec2 pb = screenspaceLines[lineNdx].swizzle(2, 3);

        setMaskMapCoverageBitForLine(lineNdx, pa, pb, lineWidth, maskMap, subpixelBits);
    }
}

// verify line interpolation assuming line pixels are interpolated independently depending only on screen space location
bool verifyLineGroupPixelIndependentInterpolation(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                  const RasterizationArguments &args, tcu::TestLog &log,
                                                  LineInterpolationMethod interpolationMethod)
{
    DE_ASSERT(scene.lines.size() < 8); // coverage indices are stored as bitmask in a unsigned 8-bit ints
    DE_ASSERT(interpolationMethod == LINEINTERPOLATION_STRICTLY_CORRECT ||
              interpolationMethod == LINEINTERPOLATION_PROJECTED);

    const tcu::RGBA invalidPixelColor = tcu::RGBA(255, 0, 0, 255);
    const tcu::IVec2 viewportSize     = tcu::IVec2(surface.getWidth(), surface.getHeight());
    const int errorFloodThreshold     = 4;
    int errorCount                    = 0;
    tcu::Surface errorMask(surface.getWidth(), surface.getHeight());
    int invalidPixels = 0;
    std::vector<tcu::Vec4> screenspaceLines(scene.lines.size()); //!< packed (x0, y0, x1, y1)

    // Reference renderer produces correct fragments using the diamond-exit-rule. Make 2D int array, store line coverage as a 8-bit bitfield
    // The map is used to find lines with potential coverage to a given pixel
    tcu::TextureLevel referenceLineMap(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::UNSIGNED_INT8),
                                       surface.getWidth(), surface.getHeight());

    tcu::clear(referenceLineMap.getAccess(), tcu::IVec4(0, 0, 0, 0));
    tcu::clear(errorMask.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // log format

    log << tcu::TestLog::Message << "Verifying rasterization result. Native format is RGB" << args.redBits
        << args.greenBits << args.blueBits << tcu::TestLog::EndMessage;
    if (args.redBits > 8 || args.greenBits > 8 || args.blueBits > 8)
        log << tcu::TestLog::Message
            << "Warning! More than 8 bits in a color channel, this may produce false negatives."
            << tcu::TestLog::EndMessage;

    // prepare lookup map

    genScreenSpaceLines(screenspaceLines, scene.lines, viewportSize);
    setMaskMapCoverageBitForLines(screenspaceLines, scene.lineWidth, referenceLineMap.getAccess(), args.subpixelBits);

    // Find all possible lines with coverage, check pixel color matches one of them

    for (int y = 1; y < surface.getHeight() - 1; ++y)
        for (int x = 1; x < surface.getWidth() - 1; ++x)
        {
            const tcu::RGBA color             = surface.getPixel(x, y);
            const tcu::IVec3 pixelNativeColor = convertRGB8ToNativeFormat(
                color, args); // Convert pixel color from rgba8 to the real pixel format. Usually rgba8 or 565
            int lineCoverageSet         = 0;      // !< lines that may cover this fragment
            int lineSurroundingCoverage = 0xFFFF; // !< lines that will cover this fragment
            bool matchFound             = false;
            const tcu::IVec3 formatLimit((1 << args.redBits) - 1, (1 << args.greenBits) - 1, (1 << args.blueBits) - 1);

            std::vector<SingleSampleNarrowLineCandidate> candidates;

            // Find lines with possible coverage

            for (int dy = -1; dy < 2; ++dy)
                for (int dx = -1; dx < 2; ++dx)
                {
                    const int coverage = referenceLineMap.getAccess().getPixelInt(x + dx, y + dy).x();

                    lineCoverageSet |= coverage;
                    lineSurroundingCoverage &= coverage;
                }

            // background color is possible?
            if (lineSurroundingCoverage == 0 &&
                compareColors(color, tcu::RGBA::black(), args.redBits, args.greenBits, args.blueBits))
                continue;

            // Check those lines

            for (int lineNdx = 0; lineNdx < (int)scene.lines.size(); ++lineNdx)
            {
                if (((lineCoverageSet >> lineNdx) & 0x01) != 0)
                {
                    const float wa     = scene.lines[lineNdx].positions[0].w();
                    const float wb     = scene.lines[lineNdx].positions[1].w();
                    const tcu::Vec2 pa = screenspaceLines[lineNdx].swizzle(0, 1);
                    const tcu::Vec2 pb = screenspaceLines[lineNdx].swizzle(2, 3);

                    const LineInterpolationRange range = (interpolationMethod == LINEINTERPOLATION_STRICTLY_CORRECT) ?
                                                             (calcSingleSampleLineInterpolationRange(
                                                                 pa, wa, pb, wb, tcu::IVec2(x, y), args.subpixelBits)) :
                                                             (calcSingleSampleLineInterpolationRangeAxisProjected(
                                                                 pa, wa, pb, wb, tcu::IVec2(x, y), args.subpixelBits));

                    const tcu::Vec4 valueMin = de::clamp(range.min.x(), 0.0f, 1.0f) * scene.lines[lineNdx].colors[0] +
                                               de::clamp(range.min.y(), 0.0f, 1.0f) * scene.lines[lineNdx].colors[1];
                    const tcu::Vec4 valueMax = de::clamp(range.max.x(), 0.0f, 1.0f) * scene.lines[lineNdx].colors[0] +
                                               de::clamp(range.max.y(), 0.0f, 1.0f) * scene.lines[lineNdx].colors[1];

                    const tcu::Vec3 colorMinF(
                        de::clamp(valueMin.x() * (float)formatLimit.x(), 0.0f, (float)formatLimit.x()),
                        de::clamp(valueMin.y() * (float)formatLimit.y(), 0.0f, (float)formatLimit.y()),
                        de::clamp(valueMin.z() * (float)formatLimit.z(), 0.0f, (float)formatLimit.z()));
                    const tcu::Vec3 colorMaxF(
                        de::clamp(valueMax.x() * (float)formatLimit.x(), 0.0f, (float)formatLimit.x()),
                        de::clamp(valueMax.y() * (float)formatLimit.y(), 0.0f, (float)formatLimit.y()),
                        de::clamp(valueMax.z() * (float)formatLimit.z(), 0.0f, (float)formatLimit.z()));
                    const tcu::IVec3 colorMin((int)deFloatFloor(colorMinF.x()), (int)deFloatFloor(colorMinF.y()),
                                              (int)deFloatFloor(colorMinF.z()));
                    const tcu::IVec3 colorMax((int)deFloatCeil(colorMaxF.x()), (int)deFloatCeil(colorMaxF.y()),
                                              (int)deFloatCeil(colorMaxF.z()));

                    // Verify validity
                    if (pixelNativeColor.x() < colorMin.x() || pixelNativeColor.y() < colorMin.y() ||
                        pixelNativeColor.z() < colorMin.z() || pixelNativeColor.x() > colorMax.x() ||
                        pixelNativeColor.y() > colorMax.y() || pixelNativeColor.z() > colorMax.z())
                    {
                        if (errorCount < errorFloodThreshold)
                        {
                            // Store candidate information for logging
                            SingleSampleNarrowLineCandidate candidate;

                            candidate.lineNdx       = lineNdx;
                            candidate.colorMin      = colorMin;
                            candidate.colorMax      = colorMax;
                            candidate.colorMinF     = colorMinF;
                            candidate.colorMaxF     = colorMaxF;
                            candidate.valueRangeMin = valueMin.swizzle(0, 1, 2);
                            candidate.valueRangeMax = valueMax.swizzle(0, 1, 2);

                            candidates.push_back(candidate);
                        }
                    }
                    else
                    {
                        matchFound = true;
                        break;
                    }
                }
            }

            if (matchFound)
                continue;

            // invalid fragment
            ++invalidPixels;
            errorMask.setPixel(x, y, invalidPixelColor);

            ++errorCount;

            // don't fill the logs with too much data
            if (errorCount < errorFloodThreshold)
            {
                log << tcu::TestLog::Message << "Found an invalid pixel at (" << x << "," << y << "), "
                    << (int)candidates.size() << " candidate reference value(s) found:\n"
                    << "\tPixel color:\t\t" << color << "\n"
                    << "\tNative color:\t\t" << pixelNativeColor << "\n"
                    << tcu::TestLog::EndMessage;

                for (int candidateNdx = 0; candidateNdx < (int)candidates.size(); ++candidateNdx)
                {
                    const SingleSampleNarrowLineCandidate &candidate = candidates[candidateNdx];

                    log << tcu::TestLog::Message << "\tCandidate (line " << candidate.lineNdx << "):\n"
                        << "\t\tReference native color min: "
                        << tcu::clamp(candidate.colorMin, tcu::IVec3(0, 0, 0), formatLimit) << "\n"
                        << "\t\tReference native color max: "
                        << tcu::clamp(candidate.colorMax, tcu::IVec3(0, 0, 0), formatLimit) << "\n"
                        << "\t\tReference native float min: "
                        << tcu::clamp(candidate.colorMinF, tcu::Vec3(0.0f, 0.0f, 0.0f), formatLimit.cast<float>())
                        << "\n"
                        << "\t\tReference native float max: "
                        << tcu::clamp(candidate.colorMaxF, tcu::Vec3(0.0f, 0.0f, 0.0f), formatLimit.cast<float>())
                        << "\n"
                        << "\t\tFmin:\t"
                        << tcu::clamp(candidate.valueRangeMin, tcu::Vec3(0.0f, 0.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 1.0f))
                        << "\n"
                        << "\t\tFmax:\t"
                        << tcu::clamp(candidate.valueRangeMax, tcu::Vec3(0.0f, 0.0f, 0.0f), tcu::Vec3(1.0f, 1.0f, 1.0f))
                        << "\n"
                        << tcu::TestLog::EndMessage;
                }
            }
        }

    // don't just hide failures
    if (errorCount > errorFloodThreshold)
        log << tcu::TestLog::Message << "Omitted " << (errorCount - errorFloodThreshold)
            << " pixel error description(s)." << tcu::TestLog::EndMessage;

    // report result
    if (invalidPixels)
    {
        log << tcu::TestLog::Message << invalidPixels << " invalid pixel(s) found." << tcu::TestLog::EndMessage;
        log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
            << tcu::TestLog::Image("Result", "Result", surface)
            << tcu::TestLog::Image("ErrorMask", "ErrorMask", errorMask) << tcu::TestLog::EndImageSet;

        return false;
    }
    else
    {
        log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
        log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
            << tcu::TestLog::Image("Result", "Result", surface) << tcu::TestLog::EndImageSet;

        return true;
    }
}

bool verifySinglesampleNarrowLineGroupInterpolation(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                    const RasterizationArguments &args, tcu::TestLog &log)
{
    DE_ASSERT(scene.lineWidth == 1.0f);
    return verifyLineGroupPixelIndependentInterpolation(surface, scene, args, log, LINEINTERPOLATION_STRICTLY_CORRECT);
}

bool verifyLineGroupInterpolationWithNonProjectedWeights(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                         const RasterizationArguments &args, tcu::TestLog &log)
{
    return verifyLineGroupPixelIndependentInterpolation(surface, scene, args, log, LINEINTERPOLATION_STRICTLY_CORRECT);
}

bool verifyLineGroupInterpolationWithProjectedWeights(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                      const RasterizationArguments &args, tcu::TestLog &log)
{
    return verifyLineGroupPixelIndependentInterpolation(surface, scene, args, log, LINEINTERPOLATION_PROJECTED);
}

struct SingleSampleWideLineCandidate
{
    struct InterpolationPointCandidate
    {
        tcu::IVec2 interpolationPoint;
        tcu::IVec3 colorMin;
        tcu::IVec3 colorMax;
        tcu::Vec3 colorMinF;
        tcu::Vec3 colorMaxF;
        tcu::Vec3 valueRangeMin;
        tcu::Vec3 valueRangeMax;
    };

    int lineNdx;
    int numCandidates;
    InterpolationPointCandidate interpolationCandidates[3];
};

// return point on line at a given position on a given axis
tcu::Vec2 getLineCoordAtAxisCoord(const tcu::Vec2 &pa, const tcu::Vec2 &pb, bool isXAxis, float axisCoord)
{
    const int fixedCoordNdx   = (isXAxis) ? (0) : (1);
    const int varyingCoordNdx = (isXAxis) ? (1) : (0);

    const float fixedDifference   = pb[fixedCoordNdx] - pa[fixedCoordNdx];
    const float varyingDifference = pb[varyingCoordNdx] - pa[varyingCoordNdx];

    DE_ASSERT(fixedDifference != 0.0f);

    const float resultFixedCoord = axisCoord;
    const float resultVaryingCoord =
        pa[varyingCoordNdx] + (axisCoord - pa[fixedCoordNdx]) * (varyingDifference / fixedDifference);

    return (isXAxis) ? (tcu::Vec2(resultFixedCoord, resultVaryingCoord)) :
                       (tcu::Vec2(resultVaryingCoord, resultFixedCoord));
}

bool isBlack(const tcu::RGBA &c)
{
    return c.getRed() == 0 && c.getGreen() == 0 && c.getBlue() == 0;
}

bool verifySinglesampleWideLineGroupInterpolation(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                  const RasterizationArguments &args, tcu::TestLog &log)
{
    DE_ASSERT(scene.lines.size() < 8); // coverage indices are stored as bitmask in a unsigned 8-bit ints

    enum
    {
        FLAG_ROOT_NOT_SET = (1u << 16)
    };

    const tcu::RGBA invalidPixelColor = tcu::RGBA(255, 0, 0, 255);
    const tcu::IVec2 viewportSize     = tcu::IVec2(surface.getWidth(), surface.getHeight());
    const int errorFloodThreshold     = 4;
    int errorCount                    = 0;
    tcu::Surface errorMask(surface.getWidth(), surface.getHeight());
    int invalidPixels = 0;
    std::vector<tcu::Vec4> effectiveLines(scene.lines.size()); //!< packed (x0, y0, x1, y1)
    std::vector<bool> lineIsXMajor(scene.lines.size());

    // for each line, for every distinct major direction fragment, store root pixel location (along
    // minor direction);
    std::vector<std::vector<uint32_t>> rootPixelLocation(
        scene.lines.size()); //!< packed [16b - flags] [16b - coordinate]

    // log format

    log << tcu::TestLog::Message << "Verifying rasterization result. Native format is RGB" << args.redBits
        << args.greenBits << args.blueBits << tcu::TestLog::EndMessage;
    if (args.redBits > 8 || args.greenBits > 8 || args.blueBits > 8)
        log << tcu::TestLog::Message
            << "Warning! More than 8 bits in a color channel, this may produce false negatives."
            << tcu::TestLog::EndMessage;

    // Reference renderer produces correct fragments using the diamond-exit-rule. Make 2D int array, store line coverage as a 8-bit bitfield
    // The map is used to find lines with potential coverage to a given pixel
    tcu::TextureLevel referenceLineMap(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::UNSIGNED_INT8),
                                       surface.getWidth(), surface.getHeight());
    tcu::clear(referenceLineMap.getAccess(), tcu::IVec4(0, 0, 0, 0));

    tcu::clear(errorMask.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // calculate mask and effective line coordinates
    {
        std::vector<tcu::Vec4> screenspaceLines(scene.lines.size());

        genScreenSpaceLines(screenspaceLines, scene.lines, viewportSize);
        setMaskMapCoverageBitForLines(screenspaceLines, scene.lineWidth, referenceLineMap.getAccess(),
                                      args.subpixelBits);

        for (int lineNdx = 0; lineNdx < (int)scene.lines.size(); ++lineNdx)
        {
            const tcu::Vec2 lineScreenSpaceP0 = screenspaceLines[lineNdx].swizzle(0, 1);
            const tcu::Vec2 lineScreenSpaceP1 = screenspaceLines[lineNdx].swizzle(2, 3);
            const bool isXMajor               = isPackedSSLineXMajor(screenspaceLines[lineNdx]);

            lineIsXMajor[lineNdx] = isXMajor;

            // wide line interpolations are calculated for a line moved in minor direction
            {
                const float offsetLength        = (scene.lineWidth - 1.0f) / 2.0f;
                const tcu::Vec2 offsetDirection = (isXMajor) ? (tcu::Vec2(0.0f, -1.0f)) : (tcu::Vec2(-1.0f, 0.0f));
                const tcu::Vec2 offset          = offsetDirection * offsetLength;

                effectiveLines[lineNdx] =
                    tcu::Vec4(lineScreenSpaceP0.x() + offset.x(), lineScreenSpaceP0.y() + offset.y(),
                              lineScreenSpaceP1.x() + offset.x(), lineScreenSpaceP1.y() + offset.y());
            }
        }
    }

    for (int lineNdx = 0; lineNdx < (int)scene.lines.size(); ++lineNdx)
    {
        // Calculate root pixel lookup table for this line. Since the implementation's fragment
        // major coordinate range might not be a subset of the correct line range (they are allowed
        // to vary by one pixel), we must extend the domain to cover whole viewport along major
        // dimension.
        //
        // Expanding line strip to (effectively) infinite line might result in exit-diamnod set
        // that is not a superset of the exit-diamond set of the line strip. In practice, this
        // won't be an issue, since the allow-one-pixel-variation rule should tolerate this even
        // if the original and extended line would resolve differently a diamond the line just
        // touches (precision lost in expansion changes enter/exit status).

        {
            const bool isXMajor = lineIsXMajor[lineNdx];
            const int majorSize = (isXMajor) ? (surface.getWidth()) : (surface.getHeight());
            rr::LineExitDiamondGenerator diamondGenerator(args.subpixelBits);
            rr::LineExitDiamond diamonds[32];
            int numRasterized = DE_LENGTH_OF_ARRAY(diamonds);

            // Expand to effectively infinite line (endpoints are just one pixel over viewport boundaries)
            const tcu::Vec2 expandedP0 = getLineCoordAtAxisCoord(
                effectiveLines[lineNdx].swizzle(0, 1), effectiveLines[lineNdx].swizzle(2, 3), isXMajor, -1.0f);
            const tcu::Vec2 expandedP1 =
                getLineCoordAtAxisCoord(effectiveLines[lineNdx].swizzle(0, 1), effectiveLines[lineNdx].swizzle(2, 3),
                                        isXMajor, (float)majorSize + 1.0f);

            diamondGenerator.init(tcu::Vec4(expandedP0.x(), expandedP0.y(), 0.0f, 1.0f),
                                  tcu::Vec4(expandedP1.x(), expandedP1.y(), 0.0f, 1.0f));

            rootPixelLocation[lineNdx].resize(majorSize, FLAG_ROOT_NOT_SET);

            while (numRasterized == DE_LENGTH_OF_ARRAY(diamonds))
            {
                diamondGenerator.rasterize(diamonds, DE_LENGTH_OF_ARRAY(diamonds), numRasterized);

                for (int packetNdx = 0; packetNdx < numRasterized; ++packetNdx)
                {
                    const tcu::IVec2 fragPos = diamonds[packetNdx].position;
                    const int majorPos       = (isXMajor) ? (fragPos.x()) : (fragPos.y());
                    const int rootPos        = (isXMajor) ? (fragPos.y()) : (fragPos.x());
                    const uint32_t packed    = (uint32_t)((uint16_t)((int16_t)rootPos));

                    // infinite line will generate some diamonds outside the viewport
                    if (deInBounds32(majorPos, 0, majorSize))
                    {
                        DE_ASSERT((rootPixelLocation[lineNdx][majorPos] & FLAG_ROOT_NOT_SET) != 0u);
                        rootPixelLocation[lineNdx][majorPos] = packed;
                    }
                }
            }

            // Filled whole lookup table
            for (int majorPos = 0; majorPos < majorSize; ++majorPos)
                DE_ASSERT((rootPixelLocation[lineNdx][majorPos] & FLAG_ROOT_NOT_SET) == 0u);
        }
    }

    // Find all possible lines with coverage, check pixel color matches one of them

    for (int y = 1; y < surface.getHeight() - 1; ++y)
        for (int x = 1; x < surface.getWidth() - 1; ++x)
        {
            const tcu::RGBA color             = surface.getPixel(x, y);
            const tcu::IVec3 pixelNativeColor = convertRGB8ToNativeFormat(
                color, args); // Convert pixel color from rgba8 to the real pixel format. Usually rgba8 or 565
            int lineCoverageSet         = 0;      // !< lines that may cover this fragment
            int lineSurroundingCoverage = 0xFFFF; // !< lines that will cover this fragment
            bool matchFound             = false;
            const tcu::IVec3 formatLimit((1 << args.redBits) - 1, (1 << args.greenBits) - 1, (1 << args.blueBits) - 1);

            std::vector<SingleSampleWideLineCandidate> candidates;

            // Find lines with possible coverage

            for (int dy = -1; dy < 2; ++dy)
                for (int dx = -1; dx < 2; ++dx)
                {
                    const int coverage = referenceLineMap.getAccess().getPixelInt(x + dx, y + dy).x();

                    lineCoverageSet |= coverage;
                    lineSurroundingCoverage &= coverage;
                }

            // background color is possible?
            if (lineSurroundingCoverage == 0 &&
                compareColors(color, tcu::RGBA::black(), args.redBits, args.greenBits, args.blueBits))
                continue;

            // Check those lines

            for (int lineNdx = 0; lineNdx < (int)scene.lines.size(); ++lineNdx)
            {
                if (((lineCoverageSet >> lineNdx) & 0x01) != 0)
                {
                    const float wa     = scene.lines[lineNdx].positions[0].w();
                    const float wb     = scene.lines[lineNdx].positions[1].w();
                    const tcu::Vec2 pa = effectiveLines[lineNdx].swizzle(0, 1);
                    const tcu::Vec2 pb = effectiveLines[lineNdx].swizzle(2, 3);

                    // \note Wide line fragments are generated by replicating the root fragment for each
                    //       fragment column (row for y-major). Calculate interpolation at the root
                    //       fragment.
                    const bool isXMajor             = lineIsXMajor[lineNdx];
                    const int majorPosition         = (isXMajor) ? (x) : (y);
                    const uint32_t minorInfoPacked  = rootPixelLocation[lineNdx][majorPosition];
                    const int minorPosition         = (int)((int16_t)((uint16_t)(minorInfoPacked & 0xFFFFu)));
                    const tcu::IVec2 idealRootPos   = (isXMajor) ? (tcu::IVec2(majorPosition, minorPosition)) :
                                                                   (tcu::IVec2(minorPosition, majorPosition));
                    const tcu::IVec2 minorDirection = (isXMajor) ? (tcu::IVec2(0, 1)) : (tcu::IVec2(1, 0));

                    SingleSampleWideLineCandidate candidate;

                    candidate.lineNdx       = lineNdx;
                    candidate.numCandidates = 0;
                    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(candidate.interpolationCandidates) == 3);

                    // Interpolation happens at the root fragment, which is then replicated in minor
                    // direction. Search for implementation's root position near accurate root.
                    for (int minorOffset = -1; minorOffset < 2; ++minorOffset)
                    {
                        const tcu::IVec2 rootPosition = idealRootPos + minorOffset * minorDirection;

                        // A fragment can be root fragment only if it exists
                        // \note root fragment can "exist" outside viewport
                        // \note no pixel format theshold since in this case allowing only black is more conservative
                        if (deInBounds32(rootPosition.x(), 0, surface.getWidth()) &&
                            deInBounds32(rootPosition.y(), 0, surface.getHeight()) &&
                            isBlack(surface.getPixel(rootPosition.x(), rootPosition.y())))
                        {
                            continue;
                        }

                        const LineInterpolationRange range =
                            calcSingleSampleLineInterpolationRange(pa, wa, pb, wb, rootPosition, args.subpixelBits);

                        const tcu::Vec4 valueMin =
                            de::clamp(range.min.x(), 0.0f, 1.0f) * scene.lines[lineNdx].colors[0] +
                            de::clamp(range.min.y(), 0.0f, 1.0f) * scene.lines[lineNdx].colors[1];
                        const tcu::Vec4 valueMax =
                            de::clamp(range.max.x(), 0.0f, 1.0f) * scene.lines[lineNdx].colors[0] +
                            de::clamp(range.max.y(), 0.0f, 1.0f) * scene.lines[lineNdx].colors[1];

                        const tcu::Vec3 colorMinF(
                            de::clamp(valueMin.x() * (float)formatLimit.x(), 0.0f, (float)formatLimit.x()),
                            de::clamp(valueMin.y() * (float)formatLimit.y(), 0.0f, (float)formatLimit.y()),
                            de::clamp(valueMin.z() * (float)formatLimit.z(), 0.0f, (float)formatLimit.z()));
                        const tcu::Vec3 colorMaxF(
                            de::clamp(valueMax.x() * (float)formatLimit.x(), 0.0f, (float)formatLimit.x()),
                            de::clamp(valueMax.y() * (float)formatLimit.y(), 0.0f, (float)formatLimit.y()),
                            de::clamp(valueMax.z() * (float)formatLimit.z(), 0.0f, (float)formatLimit.z()));
                        const tcu::IVec3 colorMin((int)deFloatFloor(colorMinF.x()), (int)deFloatFloor(colorMinF.y()),
                                                  (int)deFloatFloor(colorMinF.z()));
                        const tcu::IVec3 colorMax((int)deFloatCeil(colorMaxF.x()), (int)deFloatCeil(colorMaxF.y()),
                                                  (int)deFloatCeil(colorMaxF.z()));

                        // Verify validity
                        if (pixelNativeColor.x() < colorMin.x() || pixelNativeColor.y() < colorMin.y() ||
                            pixelNativeColor.z() < colorMin.z() || pixelNativeColor.x() > colorMax.x() ||
                            pixelNativeColor.y() > colorMax.y() || pixelNativeColor.z() > colorMax.z())
                        {
                            if (errorCount < errorFloodThreshold)
                            {
                                // Store candidate information for logging
                                SingleSampleWideLineCandidate::InterpolationPointCandidate &interpolationCandidate =
                                    candidate.interpolationCandidates[candidate.numCandidates++];
                                DE_ASSERT(candidate.numCandidates <=
                                          DE_LENGTH_OF_ARRAY(candidate.interpolationCandidates));

                                interpolationCandidate.interpolationPoint = rootPosition;
                                interpolationCandidate.colorMin           = colorMin;
                                interpolationCandidate.colorMax           = colorMax;
                                interpolationCandidate.colorMinF          = colorMinF;
                                interpolationCandidate.colorMaxF          = colorMaxF;
                                interpolationCandidate.valueRangeMin      = valueMin.swizzle(0, 1, 2);
                                interpolationCandidate.valueRangeMax      = valueMax.swizzle(0, 1, 2);
                            }
                        }
                        else
                        {
                            matchFound = true;
                            break;
                        }
                    }

                    if (!matchFound)
                    {
                        // store info for logging
                        if (errorCount < errorFloodThreshold && candidate.numCandidates > 0)
                            candidates.push_back(candidate);
                    }
                    else
                    {
                        // no need to check other lines
                        break;
                    }
                }
            }

            if (matchFound)
                continue;

            // invalid fragment
            ++invalidPixels;
            errorMask.setPixel(x, y, invalidPixelColor);

            ++errorCount;

            // don't fill the logs with too much data
            if (errorCount < errorFloodThreshold)
            {
                tcu::MessageBuilder msg(&log);

                msg << "Found an invalid pixel at (" << x << "," << y << "), " << (int)candidates.size()
                    << " candidate reference value(s) found:\n"
                    << "\tPixel color:\t\t" << color << "\n"
                    << "\tNative color:\t\t" << pixelNativeColor << "\n";

                for (int lineCandidateNdx = 0; lineCandidateNdx < (int)candidates.size(); ++lineCandidateNdx)
                {
                    const SingleSampleWideLineCandidate &candidate = candidates[lineCandidateNdx];

                    msg << "\tCandidate line (line " << candidate.lineNdx << "):\n";

                    for (int interpolationCandidateNdx = 0; interpolationCandidateNdx < candidate.numCandidates;
                         ++interpolationCandidateNdx)
                    {
                        const SingleSampleWideLineCandidate::InterpolationPointCandidate &interpolationCandidate =
                            candidate.interpolationCandidates[interpolationCandidateNdx];

                        msg << "\t\tCandidate interpolation point (index " << interpolationCandidateNdx << "):\n"
                            << "\t\t\tRoot fragment position (non-replicated fragment): "
                            << interpolationCandidate.interpolationPoint << ":\n"
                            << "\t\t\tReference native color min: "
                            << tcu::clamp(interpolationCandidate.colorMin, tcu::IVec3(0, 0, 0), formatLimit) << "\n"
                            << "\t\t\tReference native color max: "
                            << tcu::clamp(interpolationCandidate.colorMax, tcu::IVec3(0, 0, 0), formatLimit) << "\n"
                            << "\t\t\tReference native float min: "
                            << tcu::clamp(interpolationCandidate.colorMinF, tcu::Vec3(0.0f, 0.0f, 0.0f),
                                          formatLimit.cast<float>())
                            << "\n"
                            << "\t\t\tReference native float max: "
                            << tcu::clamp(interpolationCandidate.colorMaxF, tcu::Vec3(0.0f, 0.0f, 0.0f),
                                          formatLimit.cast<float>())
                            << "\n"
                            << "\t\t\tFmin:\t"
                            << tcu::clamp(interpolationCandidate.valueRangeMin, tcu::Vec3(0.0f, 0.0f, 0.0f),
                                          tcu::Vec3(1.0f, 1.0f, 1.0f))
                            << "\n"
                            << "\t\t\tFmax:\t"
                            << tcu::clamp(interpolationCandidate.valueRangeMax, tcu::Vec3(0.0f, 0.0f, 0.0f),
                                          tcu::Vec3(1.0f, 1.0f, 1.0f))
                            << "\n";
                    }
                }

                msg << tcu::TestLog::EndMessage;
            }
        }

    // don't just hide failures
    if (errorCount > errorFloodThreshold)
        log << tcu::TestLog::Message << "Omitted " << (errorCount - errorFloodThreshold)
            << " pixel error description(s)." << tcu::TestLog::EndMessage;

    // report result
    if (invalidPixels)
    {
        log << tcu::TestLog::Message << invalidPixels << " invalid pixel(s) found." << tcu::TestLog::EndMessage;
        log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
            << tcu::TestLog::Image("Result", "Result", surface)
            << tcu::TestLog::Image("ErrorMask", "ErrorMask", errorMask) << tcu::TestLog::EndImageSet;

        return false;
    }
    else
    {
        log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
        log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
            << tcu::TestLog::Image("Result", "Result", surface) << tcu::TestLog::EndImageSet;

        return true;
    }
}

} // namespace

CoverageType calculateTriangleCoverage(const tcu::Vec4 &p0, const tcu::Vec4 &p1, const tcu::Vec4 &p2,
                                       const tcu::IVec2 &pixel, const tcu::IVec2 &viewportSize, int subpixelBits,
                                       bool multisample)
{
    using tcu::I64Vec2;

    const uint64_t numSubPixels = ((uint64_t)1) << subpixelBits;
    const uint64_t pixelHitBoxSize =
        (multisample) ? (numSubPixels) :
                        5; //!< 5 = ceil(6 * sqrt(2) / 2) to account for a 3 subpixel fuzz around pixel center
    const bool order           = isTriangleClockwise(p0, p1, p2); //!< clockwise / counter-clockwise
    const tcu::Vec4 &orderedP0 = p0;                              //!< vertices of a clockwise triangle
    const tcu::Vec4 &orderedP1 = (order) ? (p1) : (p2);
    const tcu::Vec4 &orderedP2 = (order) ? (p2) : (p1);
    const tcu::Vec2 triangleNormalizedDeviceSpace[3] = {
        tcu::Vec2(orderedP0.x() / orderedP0.w(), orderedP0.y() / orderedP0.w()),
        tcu::Vec2(orderedP1.x() / orderedP1.w(), orderedP1.y() / orderedP1.w()),
        tcu::Vec2(orderedP2.x() / orderedP2.w(), orderedP2.y() / orderedP2.w()),
    };
    const tcu::Vec2 triangleScreenSpace[3] = {
        (triangleNormalizedDeviceSpace[0] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
            tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
        (triangleNormalizedDeviceSpace[1] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
            tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
        (triangleNormalizedDeviceSpace[2] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
            tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
    };

    // Broad bounding box - pixel check
    {
        const float minX =
            de::min(de::min(triangleScreenSpace[0].x(), triangleScreenSpace[1].x()), triangleScreenSpace[2].x());
        const float minY =
            de::min(de::min(triangleScreenSpace[0].y(), triangleScreenSpace[1].y()), triangleScreenSpace[2].y());
        const float maxX =
            de::max(de::max(triangleScreenSpace[0].x(), triangleScreenSpace[1].x()), triangleScreenSpace[2].x());
        const float maxY =
            de::max(de::max(triangleScreenSpace[0].y(), triangleScreenSpace[1].y()), triangleScreenSpace[2].y());

        if ((float)pixel.x() > maxX + 1 || (float)pixel.y() > maxY + 1 || (float)pixel.x() < minX - 1 ||
            (float)pixel.y() < minY - 1)
            return COVERAGE_NONE;
    }

    // Broad triangle - pixel area intersection
    {
        const DVec2 pixelCenterPosition =
            DVec2((double)pixel.x(), (double)pixel.y()) * DVec2((double)numSubPixels, (double)numSubPixels) +
            DVec2((double)numSubPixels / 2, (double)numSubPixels / 2);
        const DVec2 triangleSubPixelSpace[3] = {
            DVec2(triangleScreenSpace[0].x() * (double)numSubPixels, triangleScreenSpace[0].y() * (double)numSubPixels),
            DVec2(triangleScreenSpace[1].x() * (double)numSubPixels, triangleScreenSpace[1].y() * (double)numSubPixels),
            DVec2(triangleScreenSpace[2].x() * (double)numSubPixels, triangleScreenSpace[2].y() * (double)numSubPixels),
        };

        // Check (using cross product) if pixel center is
        // a) too far from any edge
        // b) fully inside all edges
        bool insideAllEdges = true;
        for (int vtxNdx = 0; vtxNdx < 3; ++vtxNdx)
        {
            const int otherVtxNdx = (vtxNdx + 1) % 3;
            const double maxPixelDistanceSquared =
                (double)(pixelHitBoxSize *
                         pixelHitBoxSize); // Max distance from the pixel center from within the pixel is (sqrt(2) * boxWidth/2). Use 2x value for rounding tolerance
            const DVec2 edge          = triangleSubPixelSpace[otherVtxNdx] - triangleSubPixelSpace[vtxNdx];
            const DVec2 v             = pixelCenterPosition - triangleSubPixelSpace[vtxNdx];
            const double crossProduct = (edge.x() * v.y() - edge.y() * v.x());

            // distance from edge: (edge x v) / |edge|
            //     (edge x v) / |edge| > maxPixelDistance
            // ==> (edge x v)^2 / edge^2 > maxPixelDistance^2    | edge x v > 0
            // ==> (edge x v)^2 > maxPixelDistance^2 * edge^2
            if (crossProduct < 0 && crossProduct * crossProduct > maxPixelDistanceSquared * tcu::lengthSquared(edge))
                return COVERAGE_NONE;
            if (crossProduct < 0 || crossProduct * crossProduct < maxPixelDistanceSquared * tcu::lengthSquared(edge))
                insideAllEdges = false;
        }

        if (insideAllEdges)
            return COVERAGE_FULL;
    }

    // Accurate intersection for edge pixels
    {
        //  In multisampling, the sample points can be anywhere in the pixel, and in single sampling only in the center.
        const I64Vec2 pixelCorners[4] = {
            I64Vec2((pixel.x() + 0) * numSubPixels, (pixel.y() + 0) * numSubPixels),
            I64Vec2((pixel.x() + 1) * numSubPixels, (pixel.y() + 0) * numSubPixels),
            I64Vec2((pixel.x() + 1) * numSubPixels, (pixel.y() + 1) * numSubPixels),
            I64Vec2((pixel.x() + 0) * numSubPixels, (pixel.y() + 1) * numSubPixels),
        };

        // 3 subpixel tolerance around pixel center to account for accumulated errors during various line rasterization methods
        const I64Vec2 pixelCenterCorners[4] = {
            I64Vec2(pixel.x() * numSubPixels + numSubPixels / 2 - 3, pixel.y() * numSubPixels + numSubPixels / 2 - 3),
            I64Vec2(pixel.x() * numSubPixels + numSubPixels / 2 + 3, pixel.y() * numSubPixels + numSubPixels / 2 - 3),
            I64Vec2(pixel.x() * numSubPixels + numSubPixels / 2 + 3, pixel.y() * numSubPixels + numSubPixels / 2 + 3),
            I64Vec2(pixel.x() * numSubPixels + numSubPixels / 2 - 3, pixel.y() * numSubPixels + numSubPixels / 2 + 3),
        };

        // both rounding directions
        const I64Vec2 triangleSubPixelSpaceFloor[3] = {
            I64Vec2(deFloorFloatToInt32(triangleScreenSpace[0].x() * (float)numSubPixels),
                    deFloorFloatToInt32(triangleScreenSpace[0].y() * (float)numSubPixels)),
            I64Vec2(deFloorFloatToInt32(triangleScreenSpace[1].x() * (float)numSubPixels),
                    deFloorFloatToInt32(triangleScreenSpace[1].y() * (float)numSubPixels)),
            I64Vec2(deFloorFloatToInt32(triangleScreenSpace[2].x() * (float)numSubPixels),
                    deFloorFloatToInt32(triangleScreenSpace[2].y() * (float)numSubPixels)),
        };
        const I64Vec2 triangleSubPixelSpaceCeil[3] = {
            I64Vec2(deCeilFloatToInt32(triangleScreenSpace[0].x() * (float)numSubPixels),
                    deCeilFloatToInt32(triangleScreenSpace[0].y() * (float)numSubPixels)),
            I64Vec2(deCeilFloatToInt32(triangleScreenSpace[1].x() * (float)numSubPixels),
                    deCeilFloatToInt32(triangleScreenSpace[1].y() * (float)numSubPixels)),
            I64Vec2(deCeilFloatToInt32(triangleScreenSpace[2].x() * (float)numSubPixels),
                    deCeilFloatToInt32(triangleScreenSpace[2].y() * (float)numSubPixels)),
        };
        const I64Vec2 *const corners = (multisample) ? (pixelCorners) : (pixelCenterCorners);

        // Test if any edge (with any rounding) intersects the pixel (boundary). If it does => Partial. If not => fully inside or outside

        for (int edgeNdx = 0; edgeNdx < 3; ++edgeNdx)
            for (int startRounding = 0; startRounding < 4; ++startRounding)
                for (int endRounding = 0; endRounding < 4; ++endRounding)
                {
                    const int nextEdgeNdx = (edgeNdx + 1) % 3;
                    const I64Vec2 startPos((startRounding & 0x01) ? (triangleSubPixelSpaceFloor[edgeNdx].x()) :
                                                                    (triangleSubPixelSpaceCeil[edgeNdx].x()),
                                           (startRounding & 0x02) ? (triangleSubPixelSpaceFloor[edgeNdx].y()) :
                                                                    (triangleSubPixelSpaceCeil[edgeNdx].y()));
                    const I64Vec2 endPos((endRounding & 0x01) ? (triangleSubPixelSpaceFloor[nextEdgeNdx].x()) :
                                                                (triangleSubPixelSpaceCeil[nextEdgeNdx].x()),
                                         (endRounding & 0x02) ? (triangleSubPixelSpaceFloor[nextEdgeNdx].y()) :
                                                                (triangleSubPixelSpaceCeil[nextEdgeNdx].y()));

                    for (int pixelEdgeNdx = 0; pixelEdgeNdx < 4; ++pixelEdgeNdx)
                    {
                        const int pixelEdgeEnd = (pixelEdgeNdx + 1) % 4;

                        if (lineLineIntersect(startPos, endPos, corners[pixelEdgeNdx], corners[pixelEdgeEnd]))
                            return COVERAGE_PARTIAL;
                    }
                }

        // fully inside or outside
        for (int edgeNdx = 0; edgeNdx < 3; ++edgeNdx)
        {
            const int nextEdgeNdx      = (edgeNdx + 1) % 3;
            const I64Vec2 &startPos    = triangleSubPixelSpaceFloor[edgeNdx];
            const I64Vec2 &endPos      = triangleSubPixelSpaceFloor[nextEdgeNdx];
            const I64Vec2 edge         = endPos - startPos;
            const I64Vec2 v            = corners[0] - endPos;
            const int64_t crossProduct = (edge.x() * v.y() - edge.y() * v.x());

            // a corner of the pixel is outside => "fully inside" option is impossible
            if (crossProduct < 0)
                return COVERAGE_NONE;
        }

        return COVERAGE_FULL;
    }
}

CoverageType calculateUnderestimateLineCoverage(const tcu::Vec4 &p0, const tcu::Vec4 &p1, const float lineWidth,
                                                const tcu::IVec2 &pixel, const tcu::IVec2 &viewportSize)
{
    DE_ASSERT(viewportSize.x() == viewportSize.y() && viewportSize.x() > 0);
    DE_ASSERT(p0.w() == 1.0f && p1.w() == 1.0f);

    const Vec2 p    = Vec2(p0.x(), p0.y());
    const Vec2 q    = Vec2(p1.x(), p1.y());
    const Vec2 pq   = Vec2(p1.x() - p0.x(), p1.y() - p0.y());
    const Vec2 pqn  = normalize(pq);
    const Vec2 lw   = 0.5f * lineWidth * pqn;
    const Vec2 n    = Vec2(lw.y(), -lw.x());
    const Vec2 vp   = Vec2(float(viewportSize.x()), float(viewportSize.y()));
    const Vec2 a    = 0.5f * (p + Vec2(1.0f, 1.0f)) * vp + n;
    const Vec2 b    = 0.5f * (p + Vec2(1.0f, 1.0f)) * vp - n;
    const Vec2 c    = 0.5f * (q + Vec2(1.0f, 1.0f)) * vp - n;
    const Vec2 ba   = b - a;
    const Vec2 bc   = b - c;
    const float det = ba.x() * bc.y() - ba.y() * bc.x();
    int within      = 0;

    if (det != 0.0f)
    {
        for (int cornerNdx = 0; cornerNdx < 4; ++cornerNdx)
        {
            const int pixelCornerOffsetX = ((cornerNdx & 1) ? 1 : 0);
            const int pixelCornerOffsetY = ((cornerNdx & 2) ? 1 : 0);
            const Vec2 f      = Vec2(float(pixel.x() + pixelCornerOffsetX), float(pixel.y() + pixelCornerOffsetY));
            const Vec2 bf     = b - f;
            const float alpha = (bf.x() * bc.y() - bc.x() * bf.y()) / det;
            const float beta  = (ba.x() * bf.y() - bf.x() * ba.y()) / det;
            bool cornerWithin = de::inRange(alpha, 0.0f, 1.0f) && de::inRange(beta, 0.0f, 1.0f);

            if (cornerWithin)
                within++;
        }
    }

    if (within == 0)
        return COVERAGE_NONE;
    else if (within == 4)
        return COVERAGE_FULL;
    else
        return COVERAGE_PARTIAL;
}

CoverageType calculateUnderestimateTriangleCoverage(const tcu::Vec4 &p0, const tcu::Vec4 &p1, const tcu::Vec4 &p2,
                                                    const tcu::IVec2 &pixel, int subpixelBits,
                                                    const tcu::IVec2 &viewportSize)
{
    using tcu::I64Vec2;

    const uint64_t numSubPixels = ((uint64_t)1) << subpixelBits;
    const bool order            = isTriangleClockwise(p0, p1, p2); //!< clockwise / counter-clockwise
    const tcu::Vec4 &orderedP0  = p0;                              //!< vertices of a clockwise triangle
    const tcu::Vec4 &orderedP1  = (order) ? (p1) : (p2);
    const tcu::Vec4 &orderedP2  = (order) ? (p2) : (p1);
    const tcu::Vec2 triangleNormalizedDeviceSpace[3] = {
        tcu::Vec2(orderedP0.x() / orderedP0.w(), orderedP0.y() / orderedP0.w()),
        tcu::Vec2(orderedP1.x() / orderedP1.w(), orderedP1.y() / orderedP1.w()),
        tcu::Vec2(orderedP2.x() / orderedP2.w(), orderedP2.y() / orderedP2.w()),
    };
    const tcu::Vec2 triangleScreenSpace[3] = {
        (triangleNormalizedDeviceSpace[0] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
            tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
        (triangleNormalizedDeviceSpace[1] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
            tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
        (triangleNormalizedDeviceSpace[2] + tcu::Vec2(1.0f, 1.0f)) * 0.5f *
            tcu::Vec2((float)viewportSize.x(), (float)viewportSize.y()),
    };

    // Broad bounding box - pixel check
    {
        const float minX =
            de::min(de::min(triangleScreenSpace[0].x(), triangleScreenSpace[1].x()), triangleScreenSpace[2].x());
        const float minY =
            de::min(de::min(triangleScreenSpace[0].y(), triangleScreenSpace[1].y()), triangleScreenSpace[2].y());
        const float maxX =
            de::max(de::max(triangleScreenSpace[0].x(), triangleScreenSpace[1].x()), triangleScreenSpace[2].x());
        const float maxY =
            de::max(de::max(triangleScreenSpace[0].y(), triangleScreenSpace[1].y()), triangleScreenSpace[2].y());

        if ((float)pixel.x() > maxX + 1 || (float)pixel.y() > maxY + 1 || (float)pixel.x() < minX - 1 ||
            (float)pixel.y() < minY - 1)
            return COVERAGE_NONE;
    }

    // Accurate intersection for edge pixels
    {
        //  In multisampling, the sample points can be anywhere in the pixel, and in single sampling only in the center.
        const I64Vec2 pixelCorners[4] = {
            I64Vec2((pixel.x() + 0) * numSubPixels, (pixel.y() + 0) * numSubPixels),
            I64Vec2((pixel.x() + 1) * numSubPixels, (pixel.y() + 0) * numSubPixels),
            I64Vec2((pixel.x() + 1) * numSubPixels, (pixel.y() + 1) * numSubPixels),
            I64Vec2((pixel.x() + 0) * numSubPixels, (pixel.y() + 1) * numSubPixels),
        };
        // both rounding directions
        const I64Vec2 triangleSubPixelSpaceFloor[3] = {
            I64Vec2(deFloorFloatToInt32(triangleScreenSpace[0].x() * (float)numSubPixels),
                    deFloorFloatToInt32(triangleScreenSpace[0].y() * (float)numSubPixels)),
            I64Vec2(deFloorFloatToInt32(triangleScreenSpace[1].x() * (float)numSubPixels),
                    deFloorFloatToInt32(triangleScreenSpace[1].y() * (float)numSubPixels)),
            I64Vec2(deFloorFloatToInt32(triangleScreenSpace[2].x() * (float)numSubPixels),
                    deFloorFloatToInt32(triangleScreenSpace[2].y() * (float)numSubPixels)),
        };
        const I64Vec2 triangleSubPixelSpaceCeil[3] = {
            I64Vec2(deCeilFloatToInt32(triangleScreenSpace[0].x() * (float)numSubPixels),
                    deCeilFloatToInt32(triangleScreenSpace[0].y() * (float)numSubPixels)),
            I64Vec2(deCeilFloatToInt32(triangleScreenSpace[1].x() * (float)numSubPixels),
                    deCeilFloatToInt32(triangleScreenSpace[1].y() * (float)numSubPixels)),
            I64Vec2(deCeilFloatToInt32(triangleScreenSpace[2].x() * (float)numSubPixels),
                    deCeilFloatToInt32(triangleScreenSpace[2].y() * (float)numSubPixels)),
        };

        // Test if any edge (with any rounding) intersects the pixel (boundary). If it does => Partial. If not => fully inside or outside

        for (int edgeNdx = 0; edgeNdx < 3; ++edgeNdx)
            for (int startRounding = 0; startRounding < 4; ++startRounding)
                for (int endRounding = 0; endRounding < 4; ++endRounding)
                {
                    const int nextEdgeNdx = (edgeNdx + 1) % 3;
                    const I64Vec2 startPos((startRounding & 0x01) ? (triangleSubPixelSpaceFloor[edgeNdx].x()) :
                                                                    (triangleSubPixelSpaceCeil[edgeNdx].x()),
                                           (startRounding & 0x02) ? (triangleSubPixelSpaceFloor[edgeNdx].y()) :
                                                                    (triangleSubPixelSpaceCeil[edgeNdx].y()));
                    const I64Vec2 endPos((endRounding & 0x01) ? (triangleSubPixelSpaceFloor[nextEdgeNdx].x()) :
                                                                (triangleSubPixelSpaceCeil[nextEdgeNdx].x()),
                                         (endRounding & 0x02) ? (triangleSubPixelSpaceFloor[nextEdgeNdx].y()) :
                                                                (triangleSubPixelSpaceCeil[nextEdgeNdx].y()));

                    for (int pixelEdgeNdx = 0; pixelEdgeNdx < 4; ++pixelEdgeNdx)
                    {
                        const int pixelEdgeEnd = (pixelEdgeNdx + 1) % 4;

                        if (lineLineIntersect(startPos, endPos, pixelCorners[pixelEdgeNdx], pixelCorners[pixelEdgeEnd]))
                            return COVERAGE_PARTIAL;
                    }
                }

        // fully inside or outside
        for (int edgeNdx = 0; edgeNdx < 3; ++edgeNdx)
        {
            const int nextEdgeNdx      = (edgeNdx + 1) % 3;
            const I64Vec2 &startPos    = triangleSubPixelSpaceFloor[edgeNdx];
            const I64Vec2 &endPos      = triangleSubPixelSpaceFloor[nextEdgeNdx];
            const I64Vec2 edge         = endPos - startPos;
            const I64Vec2 v            = pixelCorners[0] - endPos;
            const int64_t crossProduct = (edge.x() * v.y() - edge.y() * v.x());

            // a corner of the pixel is outside => "fully inside" option is impossible
            if (crossProduct < 0)
                return COVERAGE_NONE;
        }

        return COVERAGE_FULL;
    }
}

static void logTriangleGroupRasterizationStash(const tcu::Surface &surface, tcu::TestLog &log,
                                               VerifyTriangleGroupRasterizationLogStash &logStash)
{
    // Output results
    log << tcu::TestLog::Message << "Verifying rasterization result." << tcu::TestLog::EndMessage;

    for (size_t msgNdx = 0; msgNdx < logStash.messages.size(); ++msgNdx)
        log << tcu::TestLog::Message << logStash.messages[msgNdx] << tcu::TestLog::EndMessage;

    if (!logStash.result)
    {
        log << tcu::TestLog::Message << "Invalid pixels found:\n\t" << logStash.missingPixels
            << " missing pixels. (Marked with purple)\n\t" << logStash.unexpectedPixels
            << " incorrectly filled pixels. (Marked with red)\n\t"
            << "Unknown (subpixel on edge) pixels are marked with yellow." << tcu::TestLog::EndMessage;
        log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
            << tcu::TestLog::Image("Result", "Result", surface)
            << tcu::TestLog::Image("ErrorMask", "ErrorMask", logStash.errorMask) << tcu::TestLog::EndImageSet;
    }
    else
    {
        log << tcu::TestLog::Message << "No invalid pixels found." << tcu::TestLog::EndMessage;
        log << tcu::TestLog::ImageSet("Verification result", "Result of rendering")
            << tcu::TestLog::Image("Result", "Result", surface) << tcu::TestLog::EndImageSet;
    }
}

bool verifyTriangleGroupRasterization(const tcu::Surface &surface, const TriangleSceneSpec &scene,
                                      const RasterizationArguments &args, tcu::TestLog &log, VerificationMode mode,
                                      VerifyTriangleGroupRasterizationLogStash *logStash, const bool vulkanLinesTest)
{
    DE_ASSERT(mode < VERIFICATIONMODE_LAST);

    const tcu::RGBA backGroundColor       = tcu::RGBA(0, 0, 0, 255);
    const tcu::RGBA triangleColor         = tcu::RGBA(255, 255, 255, 255);
    const tcu::RGBA missingPixelColor     = tcu::RGBA(255, 0, 255, 255);
    const tcu::RGBA unexpectedPixelColor  = tcu::RGBA(255, 0, 0, 255);
    const tcu::RGBA partialPixelColor     = tcu::RGBA(255, 255, 0, 255);
    const tcu::RGBA primitivePixelColor   = tcu::RGBA(30, 30, 30, 255);
    const int weakVerificationThreshold   = 10;
    const int weakerVerificationThreshold = 25;
    const bool multisampled               = (args.numSamples != 0);
    const tcu::IVec2 viewportSize         = tcu::IVec2(surface.getWidth(), surface.getHeight());
    int missingPixels                     = 0;
    int unexpectedPixels                  = 0;
    int subPixelBits                      = args.subpixelBits;
    tcu::TextureLevel coverageMap(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::UNSIGNED_INT8),
                                  surface.getWidth(), surface.getHeight());
    tcu::Surface errorMask(surface.getWidth(), surface.getHeight());
    bool result = false;

    // subpixel bits in a valid range?

    if (subPixelBits < 0)
    {
        log << tcu::TestLog::Message << "Invalid subpixel count (" << subPixelBits << "), assuming 0"
            << tcu::TestLog::EndMessage;
        subPixelBits = 0;
    }
    else if (subPixelBits > 16)
    {
        // At high subpixel bit counts we might overflow. Checking at lower bit count is ok, but is less strict
        log << tcu::TestLog::Message << "Subpixel count is greater than 16 (" << subPixelBits
            << "). Checking results using less strict 16 bit requirements. This may produce false positives."
            << tcu::TestLog::EndMessage;
        subPixelBits = 16;
    }

    // generate coverage map

    tcu::clear(coverageMap.getAccess(), tcu::IVec4(COVERAGE_NONE, 0, 0, 0));

    for (int triNdx = 0; triNdx < (int)scene.triangles.size(); ++triNdx)
    {
        const tcu::IVec4 aabb = getTriangleAABB(scene.triangles[triNdx], viewportSize);

        for (int y = de::max(0, aabb.y()); y <= de::min(aabb.w(), coverageMap.getHeight() - 1); ++y)
            for (int x = de::max(0, aabb.x()); x <= de::min(aabb.z(), coverageMap.getWidth() - 1); ++x)
            {
                if (coverageMap.getAccess().getPixelUint(x, y).x() == COVERAGE_FULL)
                    continue;

                const CoverageType coverage = calculateTriangleCoverage(
                    scene.triangles[triNdx].positions[0], scene.triangles[triNdx].positions[1],
                    scene.triangles[triNdx].positions[2], tcu::IVec2(x, y), viewportSize, subPixelBits, multisampled);

                if (coverage == COVERAGE_FULL)
                {
                    coverageMap.getAccess().setPixel(tcu::IVec4(COVERAGE_FULL, 0, 0, 0), x, y);
                }
                else if (coverage == COVERAGE_PARTIAL)
                {
                    CoverageType resultCoverage = COVERAGE_PARTIAL;

                    // Sharing an edge with another triangle?
                    // There should always be such a triangle, but the pixel in the other triangle might be
                    // on multiple edges, some of which are not shared. In these cases the coverage cannot be determined.
                    // Assume full coverage if the pixel is only on a shared edge in shared triangle too.
                    if (pixelOnlyOnASharedEdge(tcu::IVec2(x, y), scene.triangles[triNdx], viewportSize))
                    {
                        bool friendFound = false;
                        for (int friendTriNdx = 0; friendTriNdx < (int)scene.triangles.size(); ++friendTriNdx)
                        {
                            if (friendTriNdx == triNdx)
                                continue;

                            const CoverageType friendCoverage = calculateTriangleCoverage(
                                scene.triangles[friendTriNdx].positions[0], scene.triangles[friendTriNdx].positions[1],
                                scene.triangles[friendTriNdx].positions[2], tcu::IVec2(x, y), viewportSize,
                                subPixelBits, multisampled);

                            if (friendCoverage != COVERAGE_NONE &&
                                pixelOnlyOnASharedEdge(tcu::IVec2(x, y), scene.triangles[friendTriNdx], viewportSize))
                            {
                                friendFound = true;
                                break;
                            }
                        }

                        if (friendFound)
                            resultCoverage = COVERAGE_FULL;
                    }

                    coverageMap.getAccess().setPixel(tcu::IVec4(resultCoverage, 0, 0, 0), x, y);
                }
            }
    }

    // check pixels

    tcu::clear(errorMask.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // Use these to quick check there is something drawn when a test expects something else than an empty picture.
    bool referenceEmpty = true;
    bool resultEmpty    = true;

    for (int y = 0; y < surface.getHeight(); ++y)
        for (int x = 0; x < surface.getWidth(); ++x)
        {
            const tcu::RGBA color = surface.getPixel(x, y);
            const bool imageNoCoverage =
                compareColors(color, backGroundColor, args.redBits, args.greenBits, args.blueBits);
            const bool imageFullCoverage =
                compareColors(color, triangleColor, args.redBits, args.greenBits, args.blueBits);
            CoverageType referenceCoverage = (CoverageType)coverageMap.getAccess().getPixelUint(x, y).x();

            if (!imageNoCoverage)
                resultEmpty = false;

            switch (referenceCoverage)
            {
            case COVERAGE_NONE:
                if (!imageNoCoverage)
                {
                    // coverage where there should not be
                    ++unexpectedPixels;
                    errorMask.setPixel(x, y, unexpectedPixelColor);
                }
                break;

            case COVERAGE_PARTIAL:
            {
                referenceEmpty     = false;
                bool foundFragment = false;
                if (vulkanLinesTest == true)
                {
                    for (int dy = -1; dy < 2 && !foundFragment; ++dy)
                        for (int dx = -1; dx < 2 && !foundFragment; ++dx)
                        {
                            if (x + dx >= 0 && x + dx != surface.getWidth() && y + dy >= 0 &&
                                y + dy != surface.getHeight() &&
                                (CoverageType)coverageMap.getAccess().getPixelUint(x + dx, y + dy).x() != COVERAGE_NONE)
                            {
                                const tcu::RGBA color2 = surface.getPixel(x + dx, y + dy);
                                if (compareColors(color2, triangleColor, args.redBits, args.greenBits, args.blueBits))
                                    foundFragment = true;
                            }
                        }
                }
                // anything goes
                if (foundFragment == false)
                {
                    errorMask.setPixel(x, y, partialPixelColor);
                    if (vulkanLinesTest == true)
                        ++missingPixels;
                }
            }
            break;

            case COVERAGE_FULL:
                referenceEmpty = false;
                if (!imageFullCoverage)
                {
                    // no coverage where there should be
                    ++missingPixels;
                    errorMask.setPixel(x, y, missingPixelColor);
                }
                else
                {
                    errorMask.setPixel(x, y, primitivePixelColor);
                }
                break;

            default:
                DE_ASSERT(false);
            }
        }

    if (((mode == VERIFICATIONMODE_STRICT) && (missingPixels + unexpectedPixels > 0)) ||
        ((mode == VERIFICATIONMODE_WEAK) && (missingPixels + unexpectedPixels > weakVerificationThreshold)) ||
        ((mode == VERIFICATIONMODE_WEAKER) && (missingPixels + unexpectedPixels > weakerVerificationThreshold)) ||
        ((mode == VERIFICATIONMODE_SMOOTH) && (missingPixels > weakVerificationThreshold)) ||
        referenceEmpty != resultEmpty)
    {
        result = false;
    }
    else
    {
        result = true;
    }

    // Output or stash results
    {
        VerifyTriangleGroupRasterizationLogStash *tempLogStash =
            (logStash == nullptr) ? new VerifyTriangleGroupRasterizationLogStash : logStash;

        tempLogStash->result           = result;
        tempLogStash->missingPixels    = missingPixels;
        tempLogStash->unexpectedPixels = unexpectedPixels;
        tempLogStash->errorMask        = errorMask;

        if (logStash == nullptr)
        {
            logTriangleGroupRasterizationStash(surface, log, *tempLogStash);
            delete tempLogStash;
        }
    }

    return result;
}

bool verifyLineGroupRasterization(const tcu::Surface &surface, const LineSceneSpec &scene,
                                  const RasterizationArguments &args, tcu::TestLog &log)
{
    const bool multisampled = args.numSamples != 0;

    if (multisampled)
        return verifyMultisampleLineGroupRasterization(surface, scene, args, log, CLIPMODE_NO_CLIPPING, nullptr, false,
                                                       true);
    else
        return verifySinglesampleLineGroupRasterization(surface, scene, args, log);
}

bool verifyClippedTriangulatedLineGroupRasterization(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                     const RasterizationArguments &args, tcu::TestLog &log)
{
    return verifyMultisampleLineGroupRasterization(surface, scene, args, log, CLIPMODE_USE_CLIPPING_BOX, nullptr, false,
                                                   true);
}

bool verifyRelaxedLineGroupRasterization(const tcu::Surface &surface, const LineSceneSpec &scene,
                                         const RasterizationArguments &args, tcu::TestLog &log,
                                         const bool vulkanLinesTest, const bool strict)
{
    VerifyTriangleGroupRasterizationLogStash useClippingLogStash;
    VerifyTriangleGroupRasterizationLogStash noClippingLogStash;
    VerifyTriangleGroupRasterizationLogStash useClippingForcedStrictLogStash;
    VerifyTriangleGroupRasterizationLogStash noClippingForcedStrictLogStash;

    if (verifyMultisampleLineGroupRasterization(surface, scene, args, log, CLIPMODE_USE_CLIPPING_BOX,
                                                &useClippingLogStash, vulkanLinesTest, strict))
    {
        logTriangleGroupRasterizationStash(surface, log, useClippingLogStash);

        return true;
    }
    else if (verifyMultisampleLineGroupRasterization(surface, scene, args, log, CLIPMODE_NO_CLIPPING,
                                                     &noClippingLogStash, vulkanLinesTest, strict))
    {
        logTriangleGroupRasterizationStash(surface, log, noClippingLogStash);

        return true;
    }
    else if (strict == false &&
             verifyMultisampleLineGroupRasterization(surface, scene, args, log, CLIPMODE_USE_CLIPPING_BOX,
                                                     &useClippingForcedStrictLogStash, vulkanLinesTest, true))
    {
        logTriangleGroupRasterizationStash(surface, log, useClippingForcedStrictLogStash);

        return true;
    }
    else if (strict == false &&
             verifyMultisampleLineGroupRasterization(surface, scene, args, log, CLIPMODE_NO_CLIPPING,
                                                     &noClippingForcedStrictLogStash, vulkanLinesTest, true))
    {
        logTriangleGroupRasterizationStash(surface, log, noClippingForcedStrictLogStash);

        return true;
    }
    else if (strict == false && args.numSamples == 0 && verifyLineGroupRasterization(surface, scene, args, log))
    {
        return true;
    }
    else
    {
        log << tcu::TestLog::Message << "Relaxed rasterization failed, details follow." << tcu::TestLog::EndMessage;

        logTriangleGroupRasterizationStash(surface, log, useClippingLogStash);
        logTriangleGroupRasterizationStash(surface, log, noClippingLogStash);

        if (strict == false)
        {
            logTriangleGroupRasterizationStash(surface, log, useClippingForcedStrictLogStash);
            logTriangleGroupRasterizationStash(surface, log, noClippingForcedStrictLogStash);
        }

        return false;
    }
}

bool verifyPointGroupRasterization(const tcu::Surface &surface, const PointSceneSpec &scene,
                                   const RasterizationArguments &args, tcu::TestLog &log)
{
    // Splitting to triangles is a valid solution in multisampled cases and even in non-multisample cases too.
    return verifyMultisamplePointGroupRasterization(surface, scene, args, log);
}

bool verifyTriangleGroupInterpolation(const tcu::Surface &surface, const TriangleSceneSpec &scene,
                                      const RasterizationArguments &args, tcu::TestLog &log)
{
    VerifyTriangleGroupInterpolationLogStash logStash;
    const bool result =
        verifyTriangleGroupInterpolationWithInterpolator(surface, scene, args, logStash, TriangleInterpolator(scene));

    logTriangleGroupnterpolationStash(surface, log, logStash);

    return result;
}

LineInterpolationMethod verifyLineGroupInterpolation(const tcu::Surface &surface, const LineSceneSpec &scene,
                                                     const RasterizationArguments &args, tcu::TestLog &log)
{
    const bool multisampled = args.numSamples != 0;

    if (multisampled)
    {
        if (verifyMultisampleLineGroupInterpolation(surface, scene, args, log))
            return LINEINTERPOLATION_STRICTLY_CORRECT;
        return LINEINTERPOLATION_INCORRECT;
    }
    else
    {
        const bool isNarrow = (scene.lineWidth == 1.0f);

        // accurate interpolation
        if (isNarrow)
        {
            if (verifySinglesampleNarrowLineGroupInterpolation(surface, scene, args, log))
                return LINEINTERPOLATION_STRICTLY_CORRECT;
        }
        else
        {
            if (verifySinglesampleWideLineGroupInterpolation(surface, scene, args, log))
                return LINEINTERPOLATION_STRICTLY_CORRECT;

            if (scene.allowNonProjectedInterpolation &&
                verifyLineGroupInterpolationWithNonProjectedWeights(surface, scene, args, log))
                return LINEINTERPOLATION_STRICTLY_CORRECT;
        }

        // check with projected (inaccurate) interpolation
        log << tcu::TestLog::Message
            << "Accurate verification failed, checking with projected weights (inaccurate equation)."
            << tcu::TestLog::EndMessage;
        if (verifyLineGroupInterpolationWithProjectedWeights(surface, scene, args, log))
            return LINEINTERPOLATION_PROJECTED;

        return LINEINTERPOLATION_INCORRECT;
    }
}

bool verifyTriangulatedLineGroupInterpolation(const tcu::Surface &surface, const LineSceneSpec &scene,
                                              const RasterizationArguments &args, tcu::TestLog &log,
                                              const bool strictMode, const bool allowBresenhamForNonStrictLines)
{
    return verifyMultisampleLineGroupInterpolation(surface, scene, args, log, strictMode,
                                                   allowBresenhamForNonStrictLines);
}

} // namespace tcu
