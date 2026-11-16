#ifndef _TCUTEXLOOKUPVERIFIER_HPP
#define _TCUTEXLOOKUPVERIFIER_HPP
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
 * \brief Texture lookup simulator that is capable of verifying generic
 *          lookup results based on accuracy parameters.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTexture.hpp"

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Generic lookup precision parameters.
 *
 * For (assumed) floating-point values recision is defined by number of
 * accurate bits in significand. Maximum number of accurate bits supported
 * is 23 (limit of single-precision FP).
 *
 * For fixed-point values precision is defined by number of bits in
 * the fractional part.
 *//*--------------------------------------------------------------------*/
struct LookupPrecision
{
    IVec3 coordBits;     //!< Bits per coordinate component before any transformations. Assumed to be floating-point.
    IVec3 uvwBits;       //!< Bits per component in final per-level UV(W) coordinates. Assumed to be fixed-point.
    Vec4 colorThreshold; //!< Threshold for match.
    BVec4 colorMask;     //!< Channel mask for comparison.

    LookupPrecision(void) : coordBits(22), uvwBits(16), colorThreshold(0.0f), colorMask(true)
    {
    }
};

struct IntLookupPrecision
{
    IVec3 coordBits;      //!< Bits per coordinate component before any transformations. Assumed to be floating-point.
    IVec3 uvwBits;        //!< Bits per component in final per-level UV(W) coordinates. Assumed to be fixed-point.
    UVec4 colorThreshold; //!< Threshold for match.
    BVec4 colorMask;      //!< Channel mask for comparison.

    IntLookupPrecision(void) : coordBits(22), uvwBits(16), colorThreshold(0), colorMask(true)
    {
    }
};

/*--------------------------------------------------------------------*//*!
 * \brief Lod computation precision parameters.
 *//*--------------------------------------------------------------------*/
struct LodPrecision
{
    int derivateBits; //!< Number of bits in derivates. (Floating-point)
    int lodBits;      //!< Number of bits in final lod (accuracy of log2()). (Fixed-point)

    LodPrecision(void) : derivateBits(22), lodBits(16)
    {
    }

    LodPrecision(int derivateBits_, int lodBits_) : derivateBits(derivateBits_), lodBits(lodBits_)
    {
    }
};

enum TexLookupScaleMode
{
    TEX_LOOKUP_SCALE_MINIFY = 0,
    TEX_LOOKUP_SCALE_MAGNIFY,

    TEX_LOOKUP_SCALE_MODE_LAST
};

Vec4 computeFixedPointThreshold(const IVec4 &bits);
Vec4 computeFloatingPointThreshold(const IVec4 &bits, const Vec4 &value);
Vec4 computeColorBitsThreshold(const IVec4 &bits, const IVec4 &numAccurateBits);

Vec2 computeLodBoundsFromDerivates(const float dudx, const float dudy, const LodPrecision &prec);
Vec2 computeLodBoundsFromDerivates(const float dudx, const float dvdx, const float dudy, const float dvdy,
                                   const LodPrecision &prec);
Vec2 computeLodBoundsFromDerivates(const float dudx, const float dvdx, const float dwdx, const float dudy,
                                   const float dvdy, const float dwdy, const LodPrecision &prec);
Vec2 computeCubeLodBoundsFromDerivates(const Vec3 &coord, const Vec3 &coordDx, const Vec3 &coordDy, const int faceSize,
                                       const LodPrecision &prec);

Vec2 clampLodBounds(const Vec2 &lodBounds, const Vec2 &lodMinMax, const LodPrecision &prec);

bool isLookupResultValid(const Texture1DView &texture, const Sampler &sampler, const LookupPrecision &prec,
                         const float coord, const Vec2 &lodBounds, const Vec4 &result);
bool isLookupResultValid(const Texture2DView &texture, const Sampler &sampler, const LookupPrecision &prec,
                         const Vec2 &coord, const Vec2 &lodBounds, const Vec4 &result);
bool isLookupResultValid(const TextureCubeView &texture, const Sampler &sampler, const LookupPrecision &prec,
                         const Vec3 &coord, const Vec2 &lodBounds, const Vec4 &result);
bool isLookupResultValid(const Texture1DArrayView &texture, const Sampler &sampler, const LookupPrecision &prec,
                         const Vec2 &coord, const Vec2 &lodBounds, const Vec4 &result);
bool isLookupResultValid(const Texture2DArrayView &texture, const Sampler &sampler, const LookupPrecision &prec,
                         const Vec3 &coord, const Vec2 &lodBounds, const Vec4 &result);
bool isLookupResultValid(const Texture3DView &texture, const Sampler &sampler, const LookupPrecision &prec,
                         const Vec3 &coord, const Vec2 &lodBounds, const Vec4 &result);
bool isLookupResultValid(const TextureCubeArrayView &texture, const Sampler &sampler, const LookupPrecision &prec,
                         const IVec4 &coordBits, const Vec4 &coord, const Vec2 &lodBounds, const Vec4 &result);

bool isLevel1DLookupResultValid(const ConstPixelBufferAccess &access, const Sampler &sampler,
                                TexLookupScaleMode scaleMode, const LookupPrecision &prec, const float coordX,
                                const int coordY, const Vec4 &result);
bool isLevel1DLookupResultValid(const ConstPixelBufferAccess &access, const Sampler &sampler,
                                TexLookupScaleMode scaleMode, const IntLookupPrecision &prec, const float coordX,
                                const int coordY, const IVec4 &result);
bool isLevel1DLookupResultValid(const ConstPixelBufferAccess &access, const Sampler &sampler,
                                TexLookupScaleMode scaleMode, const IntLookupPrecision &prec, const float coordX,
                                const int coordY, const UVec4 &result);

bool isLevel2DLookupResultValid(const ConstPixelBufferAccess &access, const Sampler &sampler,
                                TexLookupScaleMode scaleMode, const LookupPrecision &prec, const Vec2 &coord,
                                const int coordZ, const Vec4 &result);
bool isLevel2DLookupResultValid(const ConstPixelBufferAccess &access, const Sampler &sampler,
                                TexLookupScaleMode scaleMode, const IntLookupPrecision &prec, const Vec2 &coord,
                                const int coordZ, const IVec4 &result);
bool isLevel2DLookupResultValid(const ConstPixelBufferAccess &access, const Sampler &sampler,
                                TexLookupScaleMode scaleMode, const IntLookupPrecision &prec, const Vec2 &coord,
                                const int coordZ, const UVec4 &result);

bool isLevel3DLookupResultValid(const ConstPixelBufferAccess &access, const Sampler &sampler,
                                TexLookupScaleMode scaleMode, const LookupPrecision &prec, const Vec3 &coord,
                                const Vec4 &result);
bool isLevel3DLookupResultValid(const ConstPixelBufferAccess &access, const Sampler &sampler,
                                TexLookupScaleMode scaleMode, const IntLookupPrecision &prec, const Vec3 &coord,
                                const IVec4 &result);
bool isLevel3DLookupResultValid(const ConstPixelBufferAccess &access, const Sampler &sampler,
                                TexLookupScaleMode scaleMode, const IntLookupPrecision &prec, const Vec3 &coord,
                                const UVec4 &result);

bool isLinearSampleResultValid(const ConstPixelBufferAccess &level, const Sampler &sampler, const LookupPrecision &prec,
                               const Vec2 &coord, const int coordZ, const Vec4 &result);

bool isGatherOffsetsResultValid(const Texture2DView &texture, const Sampler &sampler, const LookupPrecision &prec,
                                const Vec2 &coord, int componentNdx, const IVec2 (&offsets)[4], const Vec4 &result);
bool isGatherOffsetsResultValid(const Texture2DView &texture, const Sampler &sampler, const IntLookupPrecision &prec,
                                const Vec2 &coord, int componentNdx, const IVec2 (&offsets)[4], const IVec4 &result);
bool isGatherOffsetsResultValid(const Texture2DView &texture, const Sampler &sampler, const IntLookupPrecision &prec,
                                const Vec2 &coord, int componentNdx, const IVec2 (&offsets)[4], const UVec4 &result);

bool isGatherOffsetsResultValid(const Texture2DArrayView &texture, const Sampler &sampler, const LookupPrecision &prec,
                                const Vec3 &coord, int componentNdx, const IVec2 (&offsets)[4], const Vec4 &result);
bool isGatherOffsetsResultValid(const Texture2DArrayView &texture, const Sampler &sampler,
                                const IntLookupPrecision &prec, const Vec3 &coord, int componentNdx,
                                const IVec2 (&offsets)[4], const IVec4 &result);
bool isGatherOffsetsResultValid(const Texture2DArrayView &texture, const Sampler &sampler,
                                const IntLookupPrecision &prec, const Vec3 &coord, int componentNdx,
                                const IVec2 (&offsets)[4], const UVec4 &result);

// \note For cube textures, gather is only defined without offset.
bool isGatherResultValid(const TextureCubeView &texture, const Sampler &sampler, const LookupPrecision &prec,
                         const Vec3 &coord, int componentNdx, const Vec4 &result);
bool isGatherResultValid(const TextureCubeView &texture, const Sampler &sampler, const IntLookupPrecision &prec,
                         const Vec3 &coord, int componentNdx, const IVec4 &result);
bool isGatherResultValid(const TextureCubeView &texture, const Sampler &sampler, const IntLookupPrecision &prec,
                         const Vec3 &coord, int componentNdx, const UVec4 &result);

} // namespace tcu

#endif // _TCUTEXLOOKUPVERIFIER_HPP
