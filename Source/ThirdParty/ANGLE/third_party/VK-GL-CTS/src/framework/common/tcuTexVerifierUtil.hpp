#ifndef _TCUTEXVERIFIERUTIL_HPP
#define _TCUTEXVERIFIERUTIL_HPP
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

#include "tcuDefs.hpp"
#include "tcuTexture.hpp"

namespace tcu
{
namespace TexVerifierUtil
{

// Error bound utilities

float computeFloatingPointError(const float value, const int numAccurateBits);
float computeFixedPointError(const int numAccurateBits);
float computeColorBitsError(const int bits, const int numAccurateBits);

template <int Size>
inline Vector<float, Size> computeFloatingPointError(const Vector<float, Size> &value,
                                                     const Vector<int32_t, Size> &numAccurateBits)
{
    Vector<float, Size> res;
    for (int ndx = 0; ndx < Size; ndx++)
        res[ndx] = computeFloatingPointError(value[ndx], numAccurateBits[ndx]);
    return res;
}

template <int Size>
inline Vector<float, Size> computeFixedPointError(const Vector<int32_t, Size> &numAccurateBits)
{
    Vector<float, Size> res;
    for (int ndx = 0; ndx < Size; ndx++)
        res[ndx] = computeFixedPointError(numAccurateBits[ndx]);
    return res;
}

template <int Size>
inline Vector<float, Size> computeColorBitsError(const Vector<int32_t, Size> &bits,
                                                 const Vector<int32_t, Size> &numAccurateBits)
{
    Vector<float, Size> res;
    for (int ndx = 0; ndx < Size; ndx++)
        res[ndx] = computeColorBitsError(bits[ndx], numAccurateBits[ndx]);
    return res;
}

// Sampler introspection

inline bool isNearestMipmapFilter(const Sampler::FilterMode mode)
{
    return mode == Sampler::NEAREST_MIPMAP_NEAREST || mode == Sampler::LINEAR_MIPMAP_NEAREST ||
           mode == Sampler::CUBIC_MIPMAP_NEAREST;
}

inline bool isLinearMipmapFilter(const Sampler::FilterMode mode)
{
    return mode == Sampler::NEAREST_MIPMAP_LINEAR || mode == Sampler::LINEAR_MIPMAP_LINEAR ||
           mode == Sampler::CUBIC_MIPMAP_LINEAR;
}

inline bool isMipmapFilter(const Sampler::FilterMode mode)
{
    return isNearestMipmapFilter(mode) || isLinearMipmapFilter(mode);
}

inline bool isNearestFilter(const Sampler::FilterMode mode)
{
    return mode == Sampler::NEAREST || mode == Sampler::NEAREST_MIPMAP_NEAREST ||
           mode == Sampler::NEAREST_MIPMAP_LINEAR;
}

inline bool isLinearFilter(const Sampler::FilterMode mode)
{
    return mode == Sampler::LINEAR || mode == Sampler::LINEAR_MIPMAP_NEAREST || mode == Sampler::LINEAR_MIPMAP_LINEAR;
}

inline bool isCubicFilter(const Sampler::FilterMode mode)
{
    return mode == Sampler::CUBIC || mode == Sampler::CUBIC_MIPMAP_NEAREST || mode == Sampler::CUBIC_MIPMAP_LINEAR;
}

inline Sampler::FilterMode getLevelFilter(const Sampler::FilterMode mode)
{
    if (isNearestFilter(mode))
        return Sampler::NEAREST;
    if (isLinearFilter(mode))
        return Sampler::LINEAR;
    return Sampler::CUBIC;
}

inline bool isWrapModeSupported(const Sampler::WrapMode mode)
{
    return mode != Sampler::MIRRORED_REPEAT_CL && mode != Sampler::REPEAT_CL;
}

// Misc utilities

Vec2 computeNonNormalizedCoordBounds(const bool normalizedCoords, const int dim, const float coord, const int coordBits,
                                     const int uvBits);
void getPossibleCubeFaces(const Vec3 &coord, const IVec3 &bits, CubeFace *faces, int &numFaces);

Sampler getUnnormalizedCoordSampler(const Sampler &sampler);
int wrap(Sampler::WrapMode mode, int c, int size);

} // namespace TexVerifierUtil
} // namespace tcu

#endif // _TCUTEXVERIFIERUTIL_HPP
