#ifndef _GLUTEXTURETESTUTIL_HPP
#define _GLUTEXTURETESTUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief Utility functions and structures for texture tests.
 *
 * This code is originated from the modules/glshared/glsTextureTestUtil.hpp
 * and it is tightly coupled with the GLES and Vulkan texture tests!
 *
 * About coordinates:
 *  + Quads consist of 2 triangles, rendered using explicit indices.
 *  + All TextureTestUtil functions and classes expect texture coordinates
 *    for quads to be specified in order (-1, -1), (-1, 1), (1, -1), (1, 1).
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuSurfaceAccess.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuTexCompareVerifier.hpp"
#include "qpWatchDog.h"

namespace glu
{
namespace TextureTestUtil
{
enum TextureType
{
    TEXTURETYPE_2D = 0,
    TEXTURETYPE_CUBE,
    TEXTURETYPE_2D_ARRAY,
    TEXTURETYPE_3D,
    TEXTURETYPE_CUBE_ARRAY,
    TEXTURETYPE_1D,
    TEXTURETYPE_1D_ARRAY,
    TEXTURETYPE_BUFFER,

    TEXTURETYPE_LAST
};

enum SamplerType
{
    SAMPLERTYPE_FLOAT,
    SAMPLERTYPE_INT,
    SAMPLERTYPE_UINT,
    SAMPLERTYPE_SHADOW,

    SAMPLERTYPE_FETCH_FLOAT,
    SAMPLERTYPE_FETCH_INT,
    SAMPLERTYPE_FETCH_UINT,

    SAMPLERTYPE_LAST
};

struct RenderParams
{
    enum Flags
    {
        PROJECTED    = (1 << 0),
        USE_BIAS     = (1 << 1),
        LOG_PROGRAMS = (1 << 2),
        LOG_UNIFORMS = (1 << 3),

        LOG_ALL = LOG_PROGRAMS | LOG_UNIFORMS
    };

    RenderParams(TextureType texType_)
        : texType(texType_)
        , samplerType(SAMPLERTYPE_FLOAT)
        , flags(0)
        , w(1.0f)
        , bias(0.0f)
        , ref(0.0f)
        , colorScale(1.0f)
        , colorBias(0.0f)
    {
    }

    TextureType texType;     //!< Texture type.
    SamplerType samplerType; //!< Sampler type.
    uint32_t flags;          //!< Feature flags.
    tcu::Vec4 w;             //!< w coordinates for quad vertices.
    float bias;              //!< User-supplied bias.
    float ref;               //!< Reference value for shadow lookups.

    // color = lookup() * scale + bias
    tcu::Vec4 colorScale; //!< Scale for texture color values.
    tcu::Vec4 colorBias;  //!< Bias for texture color values.
};

enum LodMode
{
    LODMODE_EXACT = 0, //!< Ideal lod computation.
    LODMODE_MIN_BOUND, //!< Use estimation range minimum bound.
    LODMODE_MAX_BOUND, //!< Use estimation range maximum bound.

    LODMODE_LAST
};

struct ReferenceParams : public RenderParams
{
    ReferenceParams(TextureType texType_)
        : RenderParams(texType_)
        , sampler()
        , lodMode(LODMODE_EXACT)
        , minLod(-1000.0f)
        , maxLod(1000.0f)
        , baseLevel(0)
        , maxLevel(1000)
        , unnormal(false)
        , float16TexCoord(false)
        , imageViewMinLod(0.0f)
        , imageViewMinLodMode(tcu::IMAGEVIEWMINLODMODE_PREFERRED)
        , lodTexelFetch(0)
    {
    }

    ReferenceParams(TextureType texType_, const tcu::Sampler &sampler_, LodMode lodMode_ = LODMODE_EXACT)
        : RenderParams(texType_)
        , sampler(sampler_)
        , lodMode(lodMode_)
        , minLod(-1000.0f)
        , maxLod(1000.0f)
        , baseLevel(0)
        , maxLevel(1000)
        , unnormal(false)
        , float16TexCoord(false)
        , imageViewMinLod(0.0f)
        , imageViewMinLodMode(tcu::IMAGEVIEWMINLODMODE_PREFERRED)
        , lodTexelFetch(0)
    {
    }

    tcu::Sampler sampler;
    LodMode lodMode;
    float minLod;
    float maxLod;
    int baseLevel;
    int maxLevel;
    bool unnormal;
    bool float16TexCoord;
    float imageViewMinLod;
    tcu::ImageViewMinLodMode imageViewMinLodMode;
    int lodTexelFetch;
};

SamplerType getSamplerType(tcu::TextureFormat format);
SamplerType getFetchSamplerType(tcu::TextureFormat format);

// Similar to sampleTexture() except uses texelFetch.
void fetchTexture(const tcu::SurfaceAccess &dst, const tcu::ConstPixelBufferAccess &src, const float *texCoord,
                  const tcu::Vec4 &colorScale, const tcu::Vec4 &colorBias);

void sampleTexture(const tcu::PixelBufferAccess &dst, const tcu::Texture2DView &src, const float *texCoord,
                   const ReferenceParams &params);

void sampleTexture(const tcu::SurfaceAccess &dst, const tcu::Texture2DView &src, const float *texCoord,
                   const ReferenceParams &params);
void sampleTexture(const tcu::SurfaceAccess &dst, const tcu::TextureCubeView &src, const float *texCoord,
                   const ReferenceParams &params);
void sampleTexture(const tcu::SurfaceAccess &dst, const tcu::Texture2DArrayView &src, const float *texCoord,
                   const ReferenceParams &params);
void sampleTexture(const tcu::SurfaceAccess &dst, const tcu::Texture3DView &src, const float *texCoord,
                   const ReferenceParams &params);
void sampleTexture(const tcu::SurfaceAccess &dst, const tcu::TextureCubeArrayView &src, const float *texCoord,
                   const ReferenceParams &params);
void sampleTexture(const tcu::SurfaceAccess &dst, const tcu::Texture1DView &src, const float *texCoord,
                   const ReferenceParams &params);
void sampleTexture(const tcu::SurfaceAccess &dst, const tcu::Texture1DArrayView &src, const float *texCoord,
                   const ReferenceParams &params);

float triangleInterpolate(float v0, float v1, float v2, float x, float y);
float triangleInterpolate(const tcu::Vec3 &v, float x, float y);

float computeLodFromDerivates(LodMode mode, float dudx, float dudy);
float computeLodFromDerivates(LodMode mode, float dudx, float dvdx, float dudy, float dvdy);
float computeLodFromDerivates(LodMode mode, float dudx, float dvdx, float dwdx, float dudy, float dvdy, float dwdy);

float computeNonProjectedTriLod(LodMode mode, const tcu::IVec2 &dstSize, int32_t srcSize, const tcu::Vec3 &sq);
float computeNonProjectedTriLod(LodMode mode, const tcu::IVec2 &dstSize, const tcu::IVec2 &srcSize, const tcu::Vec3 &sq,
                                const tcu::Vec3 &tq);
float computeNonProjectedTriLod(LodMode mode, const tcu::IVec2 &dstSize, const tcu::IVec3 &srcSize, const tcu::Vec3 &sq,
                                const tcu::Vec3 &tq, const tcu::Vec3 &rq);

void computeQuadTexCoord1D(std::vector<float> &dst, float left, float right);
void computeQuadTexCoord1DArray(std::vector<float> &dst, int layerNdx, float left, float right);
void computeQuadTexCoord2D(std::vector<float> &dst, const tcu::Vec2 &bottomLeft, const tcu::Vec2 &topRight);
void computeQuadTexCoord2D(std::vector<float> &dst, const tcu::IVec2 &bottomLeft, const tcu::IVec2 &topRight);
void computeQuadTexCoord2DArray(std::vector<float> &dst, int layerNdx, const tcu::Vec2 &bottomLeft,
                                const tcu::Vec2 &topRight);
void computeQuadTexCoord3D(std::vector<float> &dst, const tcu::Vec3 &p0, const tcu::Vec3 &p1, const tcu::IVec3 &dirSwz);
void computeQuadTexCoordCube(std::vector<float> &dst, tcu::CubeFace face);
void computeQuadTexCoordCube(std::vector<float> &dst, tcu::CubeFace face, const tcu::Vec2 &bottomLeft,
                             const tcu::Vec2 &topRight);
void computeQuadTexCoordCubeArray(std::vector<float> &dst, tcu::CubeFace face, const tcu::Vec2 &bottomLeft,
                                  const tcu::Vec2 &topRight, const tcu::Vec2 &layerRange);

bool compareImages(tcu::TestLog &log, const char *name, const char *desc, const tcu::Surface &reference,
                   const tcu::Surface &rendered, tcu::RGBA threshold);
bool compareImages(tcu::TestLog &log, const tcu::Surface &reference, const tcu::Surface &rendered, tcu::RGBA threshold);
int measureAccuracy(tcu::TestLog &log, const tcu::Surface &reference, const tcu::Surface &rendered, int bestScoreDiff,
                    int worstScoreDiff);

int computeTextureLookupDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                             const tcu::PixelBufferAccess &errorMask, const tcu::Texture1DView &src,
                             const float *texCoord, const ReferenceParams &sampleParams,
                             const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                             qpWatchDog *watchDog);

int computeTextureLookupDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                             const tcu::PixelBufferAccess &errorMask, const tcu::Texture2DView &src,
                             const float *texCoord, const ReferenceParams &sampleParams,
                             const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                             qpWatchDog *watchDog);

int computeTextureLookupDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                             const tcu::PixelBufferAccess &errorMask, const tcu::TextureCubeView &src,
                             const float *texCoord, const ReferenceParams &sampleParams,
                             const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                             qpWatchDog *watchDog);

int computeTextureLookupDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                             const tcu::PixelBufferAccess &errorMask, const tcu::Texture1DArrayView &src,
                             const float *texCoord, const ReferenceParams &sampleParams,
                             const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                             qpWatchDog *watchDog);

int computeTextureLookupDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                             const tcu::PixelBufferAccess &errorMask, const tcu::Texture2DArrayView &src,
                             const float *texCoord, const ReferenceParams &sampleParams,
                             const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                             qpWatchDog *watchDog);

int computeTextureLookupDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                             const tcu::PixelBufferAccess &errorMask, const tcu::Texture3DView &src,
                             const float *texCoord, const ReferenceParams &sampleParams,
                             const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                             qpWatchDog *watchDog);

int computeTextureLookupDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                             const tcu::PixelBufferAccess &errorMask, const tcu::TextureCubeArrayView &src,
                             const float *texCoord, const ReferenceParams &sampleParams,
                             const tcu::LookupPrecision &lookupPrec, const tcu::IVec4 &coordBits,
                             const tcu::LodPrecision &lodPrec, qpWatchDog *watchDog);

bool verifyTextureResult(tcu::TestContext &testCtx, const tcu::ConstPixelBufferAccess &result,
                         const tcu::Texture1DView &src, const float *texCoord, const ReferenceParams &sampleParams,
                         const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                         const tcu::PixelFormat &pixelFormat);

bool verifyTextureResult(tcu::TestContext &testCtx, const tcu::ConstPixelBufferAccess &result,
                         const tcu::Texture2DView &src, const float *texCoord, const ReferenceParams &sampleParams,
                         const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                         const tcu::PixelFormat &pixelFormat);

bool verifyTextureResult(tcu::TestContext &testCtx, const tcu::ConstPixelBufferAccess &result,
                         const tcu::TextureCubeView &src, const float *texCoord, const ReferenceParams &sampleParams,
                         const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                         const tcu::PixelFormat &pixelFormat);

bool verifyTextureResult(tcu::TestContext &testCtx, const tcu::ConstPixelBufferAccess &result,
                         const tcu::Texture1DArrayView &src, const float *texCoord, const ReferenceParams &sampleParams,
                         const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                         const tcu::PixelFormat &pixelFormat);

bool verifyTextureResult(tcu::TestContext &testCtx, const tcu::ConstPixelBufferAccess &result,
                         const tcu::Texture2DArrayView &src, const float *texCoord, const ReferenceParams &sampleParams,
                         const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                         const tcu::PixelFormat &pixelFormat);

bool verifyTextureResult(tcu::TestContext &testCtx, const tcu::ConstPixelBufferAccess &result,
                         const tcu::Texture3DView &src, const float *texCoord, const ReferenceParams &sampleParams,
                         const tcu::LookupPrecision &lookupPrec, const tcu::LodPrecision &lodPrec,
                         const tcu::PixelFormat &pixelFormat);

bool verifyTextureResult(tcu::TestContext &testCtx, const tcu::ConstPixelBufferAccess &result,
                         const tcu::TextureCubeArrayView &src, const float *texCoord,
                         const ReferenceParams &sampleParams, const tcu::LookupPrecision &lookupPrec,
                         const tcu::IVec4 &coordBits, const tcu::LodPrecision &lodPrec,
                         const tcu::PixelFormat &pixelFormat);

int computeTextureCompareDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                              const tcu::PixelBufferAccess &errorMask, const tcu::Texture2DView &src,
                              const float *texCoord, const ReferenceParams &sampleParams,
                              const tcu::TexComparePrecision &comparePrec, const tcu::LodPrecision &lodPrec,
                              const tcu::Vec3 &nonShadowThreshold);

int computeTextureCompareDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                              const tcu::PixelBufferAccess &errorMask, const tcu::TextureCubeView &src,
                              const float *texCoord, const ReferenceParams &sampleParams,
                              const tcu::TexComparePrecision &comparePrec, const tcu::LodPrecision &lodPrec,
                              const tcu::Vec3 &nonShadowThreshold);

int computeTextureCompareDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                              const tcu::PixelBufferAccess &errorMask, const tcu::Texture2DArrayView &src,
                              const float *texCoord, const ReferenceParams &sampleParams,
                              const tcu::TexComparePrecision &comparePrec, const tcu::LodPrecision &lodPrec,
                              const tcu::Vec3 &nonShadowThreshold);

int computeTextureCompareDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                              const tcu::PixelBufferAccess &errorMask, const tcu::Texture1DView &src,
                              const float *texCoord, const ReferenceParams &sampleParams,
                              const tcu::TexComparePrecision &comparePrec, const tcu::LodPrecision &lodPrec,
                              const tcu::Vec3 &nonShadowThreshold);

int computeTextureCompareDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                              const tcu::PixelBufferAccess &errorMask, const tcu::Texture1DArrayView &src,
                              const float *texCoord, const ReferenceParams &sampleParams,
                              const tcu::TexComparePrecision &comparePrec, const tcu::LodPrecision &lodPrec,
                              const tcu::Vec3 &nonShadowThreshold);

int computeTextureCompareDiff(const tcu::ConstPixelBufferAccess &result, const tcu::ConstPixelBufferAccess &reference,
                              const tcu::PixelBufferAccess &errorMask, const tcu::TextureCubeArrayView &src,
                              const float *texCoord, const ReferenceParams &sampleParams,
                              const tcu::TexComparePrecision &comparePrec, const tcu::LodPrecision &lodPrec,
                              const tcu::Vec3 &nonShadowThreshold);

inline tcu::IVec4 getBitsVec(const tcu::PixelFormat &format)
{
    return tcu::IVec4(format.redBits, format.greenBits, format.blueBits, format.alphaBits);
}

inline tcu::BVec4 getCompareMask(const tcu::PixelFormat &format)
{
    return tcu::BVec4(format.redBits > 0, format.greenBits > 0, format.blueBits > 0, format.alphaBits > 0);
}

// Mipmap generation comparison.

struct GenMipmapPrecision
{
    tcu::IVec3 filterBits;    //!< Bits in filtering parameters (fixed-point).
    tcu::Vec4 colorThreshold; //!< Threshold for color value comparison.
    tcu::BVec4 colorMask;     //!< Color channel comparison mask.
};

qpTestResult compareGenMipmapResult(tcu::TestLog &log, const tcu::Texture2D &resultTexture,
                                    const tcu::Texture2D &level0Reference, const GenMipmapPrecision &precision);
qpTestResult compareGenMipmapResult(tcu::TestLog &log, const tcu::TextureCube &resultTexture,
                                    const tcu::TextureCube &level0Reference, const GenMipmapPrecision &precision);

// Utility for logging texture gradient ranges.
struct LogGradientFmt
{
    LogGradientFmt(const tcu::Vec4 *min_, const tcu::Vec4 *max_) : valueMin(min_), valueMax(max_)
    {
    }
    const tcu::Vec4 *valueMin;
    const tcu::Vec4 *valueMax;
};

std::ostream &operator<<(std::ostream &str, const LogGradientFmt &fmt);
inline LogGradientFmt formatGradient(const tcu::Vec4 *minVal, const tcu::Vec4 *maxVal)
{
    return LogGradientFmt(minVal, maxVal);
}

} // namespace TextureTestUtil
} // namespace glu

#endif // _GLUTEXTURETESTUTIL_HPP
