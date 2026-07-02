#ifndef _TCUTEXCOMPAREVERIFIER_HPP
#define _TCUTEXCOMPAREVERIFIER_HPP
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

#include "tcuDefs.hpp"
#include "tcuTexture.hpp"
#include "tcuTexLookupVerifier.hpp"

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Texture compare (shadow) lookup precision parameters.
 * \see LookupPrecision
 *//*--------------------------------------------------------------------*/
struct TexComparePrecision
{
    IVec3 coordBits; //!< Bits per coordinate component before any transformations. Assumed to be floating-point.
    IVec3 uvwBits;   //!< Bits per component in final per-level UV(W) coordinates. Assumed to be fixed-point.
    int pcfBits; //!< Number of accurate bits in PCF. This is additional error taken into account when doing filtering. Assumed to be fixed-point.
    int referenceBits; //!< Number of accurate bits in compare reference value. Assumed to be fixed-point.
    int resultBits;    //!< Number of accurate bits in result value. Assumed to be fixed-point.

    TexComparePrecision(void) : coordBits(22), uvwBits(16), pcfBits(16), referenceBits(16), resultBits(16)
    {
    }
} DE_WARN_UNUSED_TYPE;

bool isTexCompareResultValid(const Texture2DView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec2 &coord, const Vec2 &lodBounds, const float cmpReference, const float result);
bool isTexCompareResultValid(const TextureCubeView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec3 &coord, const Vec2 &lodBounds, const float cmpReference, const float result);
bool isTexCompareResultValid(const Texture2DArrayView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec3 &coord, const Vec2 &lodBounds, const float cmpReference, const float result);
bool isTexCompareResultValid(const Texture1DView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec1 &coord, const Vec2 &lodBounds, const float cmpReference, const float result);
bool isTexCompareResultValid(const Texture1DArrayView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                             const Vec2 &coord, const Vec2 &lodBounds, const float cmpReference, const float result);
bool isTexCompareResultValid(const TextureCubeArrayView &texture, const Sampler &sampler,
                             const TexComparePrecision &prec, const Vec4 &coord, const Vec2 &lodBounds,
                             const float cmpReference, const float result);

bool isGatherOffsetsCompareResultValid(const Texture2DView &texture, const Sampler &sampler,
                                       const TexComparePrecision &prec, const Vec2 &coord, const IVec2 (&offsets)[4],
                                       float cmpReference, const Vec4 &result);
bool isGatherOffsetsCompareResultValid(const Texture2DArrayView &texture, const Sampler &sampler,
                                       const TexComparePrecision &prec, const Vec3 &coord, const IVec2 (&offsets)[4],
                                       float cmpReference, const Vec4 &result);
// \note For cube textures, gather is only defined without offset.
bool isGatherCompareResultValid(const TextureCubeView &texture, const Sampler &sampler, const TexComparePrecision &prec,
                                const Vec3 &coord, float cmpReference, const Vec4 &result);

} // namespace tcu

#endif // _TCUTEXCOMPAREVERIFIER_HPP
