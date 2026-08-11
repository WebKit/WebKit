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
 * \brief Internal utilities shared between TexLookup and TexCompare verifiers.
 *//*--------------------------------------------------------------------*/

#include "tcuTexVerifierUtil.hpp"
#include "tcuFloat.hpp"

namespace tcu
{
namespace TexVerifierUtil
{

float computeFloatingPointError(const float value, const int numAccurateBits)
{
    DE_ASSERT(numAccurateBits >= 0);
    DE_ASSERT(numAccurateBits <= 23);

    const int numGarbageBits = 23 - numAccurateBits;
    const uint32_t mask      = (1u << numGarbageBits) - 1u;
    const int exp            = tcu::Float32(value).exponent();

    return Float32::construct(+1, exp, (1u << 23) | mask).asFloat() - Float32::construct(+1, exp, 1u << 23).asFloat();
}

float computeFixedPointError(const int numAccurateBits)
{
    return computeFloatingPointError(1.0f, numAccurateBits);
}

float computeColorBitsError(const int bits, const int numAccurateBits)
{
    // Color bits error is not a generic function, it just for compute the error value that cannot be accurately shown in integer data format.
    //
    //        "bits" is color bit width, "numAccurateBits" is the number of accurate bits in color bits, "1 << (bits - numAccurateBits)" is the threshold in integer.
    //        "1.0f / 256.0f" is epsilon value, to make sure the threshold use to calculate in float can be a little bigger than the real value.
    return (float(1 << (bits - numAccurateBits)) + 1.0f / 256.0f) / float((1 << bits) - 1);
}

Vec2 computeNonNormalizedCoordBounds(const bool normalizedCoords, const int dim, const float coord, const int coordBits,
                                     const int uvBits)
{
    const float coordErr = computeFloatingPointError(coord, coordBits);
    const float minN     = coord - coordErr;
    const float maxN     = coord + coordErr;
    const float minA     = normalizedCoords ? minN * float(dim) : minN;
    const float maxA     = normalizedCoords ? maxN * float(dim) : maxN;
    const float minC     = minA - computeFixedPointError(uvBits);
    const float maxC     = maxA + computeFixedPointError(uvBits);

    DE_ASSERT(minC <= maxC);

    return Vec2(minC, maxC);
}

void getPossibleCubeFaces(const Vec3 &coord, const IVec3 &bits, CubeFace *faces, int &numFaces)
{
    const float x  = coord.x();
    const float y  = coord.y();
    const float z  = coord.z();
    const float ax = de::abs(x);
    const float ay = de::abs(y);
    const float az = de::abs(z);
    const float ex = computeFloatingPointError(x, bits.x());
    const float ey = computeFloatingPointError(y, bits.y());
    const float ez = computeFloatingPointError(z, bits.z());

    numFaces = 0;

    if (ay + ey < ax - ex && az + ez < ax - ex)
    {
        if (x >= ex)
            faces[numFaces++] = CUBEFACE_POSITIVE_X;
        if (x <= ex)
            faces[numFaces++] = CUBEFACE_NEGATIVE_X;
    }
    else if (ax + ex < ay - ey && az + ez < ay - ey)
    {
        if (y >= ey)
            faces[numFaces++] = CUBEFACE_POSITIVE_Y;
        if (y <= ey)
            faces[numFaces++] = CUBEFACE_NEGATIVE_Y;
    }
    else if (ax + ex < az - ez && ay + ey < az - ez)
    {
        if (z >= ez)
            faces[numFaces++] = CUBEFACE_POSITIVE_Z;
        if (z <= ez)
            faces[numFaces++] = CUBEFACE_NEGATIVE_Z;
    }
    else
    {
        // One or more components are equal (or within error bounds). Allow all faces where major axis is not zero.
        if (ax > ex)
        {
            faces[numFaces++] = CUBEFACE_NEGATIVE_X;
            faces[numFaces++] = CUBEFACE_POSITIVE_X;
        }

        if (ay > ey)
        {
            faces[numFaces++] = CUBEFACE_NEGATIVE_Y;
            faces[numFaces++] = CUBEFACE_POSITIVE_Y;
        }

        if (az > ez)
        {
            faces[numFaces++] = CUBEFACE_NEGATIVE_Z;
            faces[numFaces++] = CUBEFACE_POSITIVE_Z;
        }
    }
}

Sampler getUnnormalizedCoordSampler(const Sampler &sampler)
{
    Sampler copy          = sampler;
    copy.normalizedCoords = false;
    return copy;
}

static inline int imod(int a, int b)
{
    int m = a % b;
    return m < 0 ? m + b : m;
}

static inline int mirror(int a)
{
    return a >= 0 ? a : -(1 + a);
}

int wrap(Sampler::WrapMode mode, int c, int size)
{
    switch (mode)
    {
    // \note CL and GL modes are handled identically here, as verification process accounts for
    //         accuracy differences caused by different methods (wrapping vs. denormalizing first).
    case tcu::Sampler::CLAMP_TO_BORDER:
        return deClamp32(c, -1, size);

    case tcu::Sampler::CLAMP_TO_EDGE:
        return deClamp32(c, 0, size - 1);

    case tcu::Sampler::REPEAT_GL:
    case tcu::Sampler::REPEAT_CL:
        return imod(c, size);

    case tcu::Sampler::MIRRORED_ONCE:
        c = deClamp32(c, -size, size);
        // Fall-through

    case tcu::Sampler::MIRRORED_REPEAT_GL:
    case tcu::Sampler::MIRRORED_REPEAT_CL:
        return (size - 1) - mirror(imod(c, 2 * size) - size);

    default:
        DE_ASSERT(false);
        return 0;
    }
}
} // namespace TexVerifierUtil
} // namespace tcu
