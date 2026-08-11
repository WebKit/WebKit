/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
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
 * \brief Texture access function tests.
 *//*--------------------------------------------------------------------*/

#include "es3fShaderTextureFunctionTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "glsShaderLibrary.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuMatrix.hpp"
#include "tcuMatrixUtil.hpp"
#include "tcuTestLog.hpp"
#include "glwFunctions.hpp"
#include "deMath.h"

#include <sstream>

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

namespace
{

using glu::TextureTestUtil::computeLodFromDerivates;

enum Function
{
    FUNCTION_TEXTURE = 0,  //!< texture(), textureOffset()
    FUNCTION_TEXTUREPROJ,  //!< textureProj(), textureProjOffset()
    FUNCTION_TEXTUREPROJ3, //!< textureProj(sampler2D, vec3)
    FUNCTION_TEXTURELOD,   // ...
    FUNCTION_TEXTUREPROJLOD,
    FUNCTION_TEXTUREPROJLOD3, //!< textureProjLod(sampler2D, vec3)
    FUNCTION_TEXTUREGRAD,
    FUNCTION_TEXTUREPROJGRAD,
    FUNCTION_TEXTUREPROJGRAD3, //!< textureProjGrad(sampler2D, vec3)
    FUNCTION_TEXELFETCH,

    FUNCTION_LAST
};

inline bool functionHasAutoLod(glu::ShaderType shaderType, Function function)
{
    return shaderType == glu::SHADERTYPE_FRAGMENT &&
           (function == FUNCTION_TEXTURE || function == FUNCTION_TEXTUREPROJ || function == FUNCTION_TEXTUREPROJ3);
}

inline bool functionHasProj(Function function)
{
    return function == FUNCTION_TEXTUREPROJ || function == FUNCTION_TEXTUREPROJ3 ||
           function == FUNCTION_TEXTUREPROJLOD || function == FUNCTION_TEXTUREPROJGRAD ||
           function == FUNCTION_TEXTUREPROJLOD3 || function == FUNCTION_TEXTUREPROJGRAD3;
}

inline bool functionHasGrad(Function function)
{
    return function == FUNCTION_TEXTUREGRAD || function == FUNCTION_TEXTUREPROJGRAD ||
           function == FUNCTION_TEXTUREPROJGRAD3;
}

inline bool functionHasLod(Function function)
{
    return function == FUNCTION_TEXTURELOD || function == FUNCTION_TEXTUREPROJLOD ||
           function == FUNCTION_TEXTUREPROJLOD3 || function == FUNCTION_TEXELFETCH;
}

struct TextureLookupSpec
{
    Function function;

    tcu::Vec4 minCoord;
    tcu::Vec4 maxCoord;

    // Bias
    bool useBias;

    // Bias or Lod for *Lod* functions
    float minLodBias;
    float maxLodBias;

    // For *Grad* functions
    tcu::Vec3 minDX;
    tcu::Vec3 maxDX;
    tcu::Vec3 minDY;
    tcu::Vec3 maxDY;

    bool useOffset;
    tcu::IVec3 offset;

    TextureLookupSpec(void)
        : function(FUNCTION_LAST)
        , minCoord(0.0f)
        , maxCoord(1.0f)
        , useBias(false)
        , minLodBias(0.0f)
        , maxLodBias(0.0f)
        , minDX(0.0f)
        , maxDX(0.0f)
        , minDY(0.0f)
        , maxDY(0.0f)
        , useOffset(false)
        , offset(0)
    {
    }

    TextureLookupSpec(Function function_, const tcu::Vec4 &minCoord_, const tcu::Vec4 &maxCoord_, bool useBias_,
                      float minLodBias_, float maxLodBias_, const tcu::Vec3 &minDX_, const tcu::Vec3 &maxDX_,
                      const tcu::Vec3 &minDY_, const tcu::Vec3 &maxDY_, bool useOffset_, const tcu::IVec3 &offset_)
        : function(function_)
        , minCoord(minCoord_)
        , maxCoord(maxCoord_)
        , useBias(useBias_)
        , minLodBias(minLodBias_)
        , maxLodBias(maxLodBias_)
        , minDX(minDX_)
        , maxDX(maxDX_)
        , minDY(minDY_)
        , maxDY(maxDY_)
        , useOffset(useOffset_)
        , offset(offset_)
    {
    }
};

enum TextureType
{
    TEXTURETYPE_2D,
    TEXTURETYPE_CUBE_MAP,
    TEXTURETYPE_2D_ARRAY,
    TEXTURETYPE_3D,

    TEXTURETYPE_LAST
};

struct TextureSpec
{
    TextureType type; //!< Texture type (2D, cubemap, ...)
    uint32_t format;  //!< Internal format.
    int width;
    int height;
    int depth;
    int numLevels;
    tcu::Sampler sampler;

    TextureSpec(void) : type(TEXTURETYPE_LAST), format(GL_NONE), width(0), height(0), depth(0), numLevels(0)
    {
    }

    TextureSpec(TextureType type_, uint32_t format_, int width_, int height_, int depth_, int numLevels_,
                const tcu::Sampler &sampler_)
        : type(type_)
        , format(format_)
        , width(width_)
        , height(height_)
        , depth(depth_)
        , numLevels(numLevels_)
        , sampler(sampler_)
    {
    }
};

struct TexLookupParams
{
    float lod;
    tcu::IVec3 offset;
    tcu::Vec4 scale;
    tcu::Vec4 bias;

    TexLookupParams(void) : lod(0.0f), offset(0), scale(1.0f), bias(0.0f)
    {
    }
};

} // namespace

using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

static const glu::TextureTestUtil::LodMode DEFAULT_LOD_MODE = glu::TextureTestUtil::LODMODE_EXACT;

inline float computeLodFromGrad2D(const gls::ShaderEvalContext &c)
{
    float w = (float)c.textures[0].tex2D->getWidth();
    float h = (float)c.textures[0].tex2D->getHeight();
    return computeLodFromDerivates(DEFAULT_LOD_MODE, c.in[1].x() * w, c.in[1].y() * h, c.in[2].x() * w,
                                   c.in[2].y() * h);
}

inline float computeLodFromGrad2DArray(const gls::ShaderEvalContext &c)
{
    float w = (float)c.textures[0].tex2DArray->getWidth();
    float h = (float)c.textures[0].tex2DArray->getHeight();
    return computeLodFromDerivates(DEFAULT_LOD_MODE, c.in[1].x() * w, c.in[1].y() * h, c.in[2].x() * w,
                                   c.in[2].y() * h);
}

inline float computeLodFromGrad3D(const gls::ShaderEvalContext &c)
{
    float w = (float)c.textures[0].tex3D->getWidth();
    float h = (float)c.textures[0].tex3D->getHeight();
    float d = (float)c.textures[0].tex3D->getDepth();
    return computeLodFromDerivates(DEFAULT_LOD_MODE, c.in[1].x() * w, c.in[1].y() * h, c.in[1].z() * d, c.in[2].x() * w,
                                   c.in[2].y() * h, c.in[2].z() * d);
}

inline float computeLodFromGradCube(const gls::ShaderEvalContext &c)
{
    // \note Major axis is always -Z or +Z
    float m = de::abs(c.in[0].z());
    float d = (float)c.textures[0].texCube->getSize();
    float s = d / (2.0f * m);
    float t = d / (2.0f * m);
    return computeLodFromDerivates(DEFAULT_LOD_MODE, c.in[1].x() * s, c.in[1].y() * t, c.in[2].x() * s,
                                   c.in[2].y() * t);
}

typedef void (*TexEvalFunc)(gls::ShaderEvalContext &c, const TexLookupParams &lookupParams);

inline Vec4 texture2D(const gls::ShaderEvalContext &c, float s, float t, float lod)
{
    return c.textures[0].tex2D->sample(c.textures[0].sampler, s, t, lod);
}
inline Vec4 textureCube(const gls::ShaderEvalContext &c, float s, float t, float r, float lod)
{
    return c.textures[0].texCube->sample(c.textures[0].sampler, s, t, r, lod);
}
inline Vec4 texture2DArray(const gls::ShaderEvalContext &c, float s, float t, float r, float lod)
{
    return c.textures[0].tex2DArray->sample(c.textures[0].sampler, s, t, r, lod);
}
inline Vec4 texture3D(const gls::ShaderEvalContext &c, float s, float t, float r, float lod)
{
    return c.textures[0].tex3D->sample(c.textures[0].sampler, s, t, r, lod);
}

inline float texture2DShadow(const gls::ShaderEvalContext &c, float ref, float s, float t, float lod)
{
    return c.textures[0].tex2D->sampleCompare(c.textures[0].sampler, ref, s, t, lod);
}
inline float textureCubeShadow(const gls::ShaderEvalContext &c, float ref, float s, float t, float r, float lod)
{
    return c.textures[0].texCube->sampleCompare(c.textures[0].sampler, ref, s, t, r, lod);
}
inline float texture2DArrayShadow(const gls::ShaderEvalContext &c, float ref, float s, float t, float r, float lod)
{
    return c.textures[0].tex2DArray->sampleCompare(c.textures[0].sampler, ref, s, t, r, lod);
}

inline Vec4 texture2DOffset(const gls::ShaderEvalContext &c, float s, float t, float lod, IVec2 offset)
{
    return c.textures[0].tex2D->sampleOffset(c.textures[0].sampler, s, t, lod, offset);
}
inline Vec4 texture2DArrayOffset(const gls::ShaderEvalContext &c, float s, float t, float r, float lod, IVec2 offset)
{
    return c.textures[0].tex2DArray->sampleOffset(c.textures[0].sampler, s, t, r, lod, offset);
}
inline Vec4 texture3DOffset(const gls::ShaderEvalContext &c, float s, float t, float r, float lod, IVec3 offset)
{
    return c.textures[0].tex3D->sampleOffset(c.textures[0].sampler, s, t, r, lod, offset);
}

inline float texture2DShadowOffset(const gls::ShaderEvalContext &c, float ref, float s, float t, float lod,
                                   IVec2 offset)
{
    return c.textures[0].tex2D->sampleCompareOffset(c.textures[0].sampler, ref, s, t, lod, offset);
}
inline float texture2DArrayShadowOffset(const gls::ShaderEvalContext &c, float ref, float s, float t, float r,
                                        float lod, IVec2 offset)
{
    return c.textures[0].tex2DArray->sampleCompareOffset(c.textures[0].sampler, ref, s, t, r, lod, offset);
}

// Eval functions.
static void evalTexture2D(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x(), c.in[0].y(), p.lod) * p.scale + p.bias;
}
static void evalTextureCube(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = textureCube(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod) * p.scale + p.bias;
}
static void evalTexture2DArray(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DArray(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod) * p.scale + p.bias;
}
static void evalTexture3D(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3D(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod) * p.scale + p.bias;
}

static void evalTexture2DBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x(), c.in[0].y(), p.lod + c.in[1].x()) * p.scale + p.bias;
}
static void evalTextureCubeBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = textureCube(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x()) * p.scale + p.bias;
}
static void evalTexture2DArrayBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DArray(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x()) * p.scale + p.bias;
}
static void evalTexture3DBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3D(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x()) * p.scale + p.bias;
}

static void evalTexture2DProj3(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x() / c.in[0].z(), c.in[0].y() / c.in[0].z(), p.lod) * p.scale + p.bias;
}
static void evalTexture2DProj3Bias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture2D(c, c.in[0].x() / c.in[0].z(), c.in[0].y() / c.in[0].z(), p.lod + c.in[1].x()) * p.scale + p.bias;
}
static void evalTexture2DProj(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), p.lod) * p.scale + p.bias;
}
static void evalTexture2DProjBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture2D(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), p.lod + c.in[1].x()) * p.scale + p.bias;
}
static void evalTexture3DProj(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture3D(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[0].z() / c.in[0].w(), p.lod) * p.scale +
        p.bias;
}
static void evalTexture3DProjBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3D(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[0].z() / c.in[0].w(),
                        p.lod + c.in[1].x()) *
                  p.scale +
              p.bias;
}

static void evalTexture2DLod(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x(), c.in[0].y(), c.in[1].x()) * p.scale + p.bias;
}
static void evalTextureCubeLod(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = textureCube(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[1].x()) * p.scale + p.bias;
}
static void evalTexture2DArrayLod(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DArray(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[1].x()) * p.scale + p.bias;
}
static void evalTexture3DLod(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3D(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[1].x()) * p.scale + p.bias;
}

static void evalTexture2DProjLod3(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x() / c.in[0].z(), c.in[0].y() / c.in[0].z(), c.in[1].x()) * p.scale + p.bias;
}
static void evalTexture2DProjLod(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[1].x()) * p.scale + p.bias;
}
static void evalTexture3DProjLod(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture3D(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[0].z() / c.in[0].w(), c.in[1].x()) *
            p.scale +
        p.bias;
}

// Offset variants

static void evalTexture2DOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DOffset(c, c.in[0].x(), c.in[0].y(), p.lod, p.offset.swizzle(0, 1)) * p.scale + p.bias;
}
static void evalTexture2DArrayOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DArrayOffset(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod, p.offset.swizzle(0, 1)) * p.scale +
              p.bias;
}
static void evalTexture3DOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3DOffset(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod, p.offset) * p.scale + p.bias;
}

static void evalTexture2DOffsetBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture2DOffset(c, c.in[0].x(), c.in[0].y(), p.lod + c.in[1].x(), p.offset.swizzle(0, 1)) * p.scale + p.bias;
}
static void evalTexture2DArrayOffsetBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture2DArrayOffset(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x(), p.offset.swizzle(0, 1)) *
            p.scale +
        p.bias;
}
static void evalTexture3DOffsetBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture3DOffset(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x(), p.offset) * p.scale + p.bias;
}

static void evalTexture2DLodOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DOffset(c, c.in[0].x(), c.in[0].y(), c.in[1].x(), p.offset.swizzle(0, 1)) * p.scale + p.bias;
}
static void evalTexture2DArrayLodOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture2DArrayOffset(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[1].x(), p.offset.swizzle(0, 1)) * p.scale +
        p.bias;
}
static void evalTexture3DLodOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3DOffset(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[1].x(), p.offset) * p.scale + p.bias;
}

static void evalTexture2DProj3Offset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DOffset(c, c.in[0].x() / c.in[0].z(), c.in[0].y() / c.in[0].z(), p.lod, p.offset.swizzle(0, 1)) *
                  p.scale +
              p.bias;
}
static void evalTexture2DProj3OffsetBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DOffset(c, c.in[0].x() / c.in[0].z(), c.in[0].y() / c.in[0].z(), p.lod + c.in[1].x(),
                              p.offset.swizzle(0, 1)) *
                  p.scale +
              p.bias;
}
static void evalTexture2DProjOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DOffset(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), p.lod, p.offset.swizzle(0, 1)) *
                  p.scale +
              p.bias;
}
static void evalTexture2DProjOffsetBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DOffset(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), p.lod + c.in[1].x(),
                              p.offset.swizzle(0, 1)) *
                  p.scale +
              p.bias;
}
static void evalTexture3DProjOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3DOffset(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[0].z() / c.in[0].w(), p.lod,
                              p.offset) *
                  p.scale +
              p.bias;
}
static void evalTexture3DProjOffsetBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3DOffset(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[0].z() / c.in[0].w(),
                              p.lod + c.in[1].x(), p.offset) *
                  p.scale +
              p.bias;
}

static void evalTexture2DProjLod3Offset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture2DOffset(c, c.in[0].x() / c.in[0].z(), c.in[0].y() / c.in[0].z(), c.in[1].x(), p.offset.swizzle(0, 1)) *
            p.scale +
        p.bias;
}
static void evalTexture2DProjLodOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture2DOffset(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[1].x(), p.offset.swizzle(0, 1)) *
            p.scale +
        p.bias;
}
static void evalTexture3DProjLodOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3DOffset(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[0].z() / c.in[0].w(),
                              c.in[1].x(), p.offset) *
                  p.scale +
              p.bias;
}

// Shadow variants

static void evalTexture2DShadow(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadow(c, c.in[0].z(), c.in[0].x(), c.in[0].y(), p.lod);
}
static void evalTexture2DShadowBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadow(c, c.in[0].z(), c.in[0].x(), c.in[0].y(), p.lod + c.in[1].x());
}

static void evalTextureCubeShadow(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = textureCubeShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod);
}
static void evalTextureCubeShadowBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = textureCubeShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x());
}

static void evalTexture2DArrayShadow(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DArrayShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod);
}

static void evalTexture2DShadowLod(gls::ShaderEvalContext &c, const TexLookupParams &)
{
    c.color.x() = texture2DShadow(c, c.in[0].z(), c.in[0].x(), c.in[0].y(), c.in[1].x());
}
static void evalTexture2DShadowLodOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadowOffset(c, c.in[0].z(), c.in[0].x(), c.in[0].y(), c.in[1].x(), p.offset.swizzle(0, 1));
}

static void evalTexture2DShadowProj(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() =
        texture2DShadow(c, c.in[0].z() / c.in[0].w(), c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), p.lod);
}
static void evalTexture2DShadowProjBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadow(c, c.in[0].z() / c.in[0].w(), c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(),
                                  p.lod + c.in[1].x());
}

static void evalTexture2DShadowProjLod(gls::ShaderEvalContext &c, const TexLookupParams &)
{
    c.color.x() = texture2DShadow(c, c.in[0].z() / c.in[0].w(), c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(),
                                  c.in[1].x());
}
static void evalTexture2DShadowProjLodOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadowOffset(c, c.in[0].z() / c.in[0].w(), c.in[0].x() / c.in[0].w(),
                                        c.in[0].y() / c.in[0].w(), c.in[1].x(), p.offset.swizzle(0, 1));
}

static void evalTexture2DShadowOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadowOffset(c, c.in[0].z(), c.in[0].x(), c.in[0].y(), p.lod, p.offset.swizzle(0, 1));
}
static void evalTexture2DShadowOffsetBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() =
        texture2DShadowOffset(c, c.in[0].z(), c.in[0].x(), c.in[0].y(), p.lod + c.in[1].x(), p.offset.swizzle(0, 1));
}

static void evalTexture2DShadowProjOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadowOffset(c, c.in[0].z() / c.in[0].w(), c.in[0].x() / c.in[0].w(),
                                        c.in[0].y() / c.in[0].w(), p.lod, p.offset.swizzle(0, 1));
}
static void evalTexture2DShadowProjOffsetBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadowOffset(c, c.in[0].z() / c.in[0].w(), c.in[0].x() / c.in[0].w(),
                                        c.in[0].y() / c.in[0].w(), p.lod + c.in[1].x(), p.offset.swizzle(0, 1));
}

// Gradient variarts

static void evalTexture2DGrad(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x(), c.in[0].y(), computeLodFromGrad2D(c)) * p.scale + p.bias;
}
static void evalTextureCubeGrad(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = textureCube(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), computeLodFromGradCube(c)) * p.scale + p.bias;
}
static void evalTexture2DArrayGrad(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DArray(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), computeLodFromGrad2DArray(c)) * p.scale + p.bias;
}
static void evalTexture3DGrad(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3D(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), computeLodFromGrad3D(c)) * p.scale + p.bias;
}

static void evalTexture2DShadowGrad(gls::ShaderEvalContext &c, const TexLookupParams &)
{
    c.color.x() = texture2DShadow(c, c.in[0].z(), c.in[0].x(), c.in[0].y(), computeLodFromGrad2D(c));
}
static void evalTextureCubeShadowGrad(gls::ShaderEvalContext &c, const TexLookupParams &)
{
    c.color.x() = textureCubeShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), computeLodFromGradCube(c));
}
static void evalTexture2DArrayShadowGrad(gls::ShaderEvalContext &c, const TexLookupParams &)
{
    c.color.x() =
        texture2DArrayShadow(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(), computeLodFromGrad2DArray(c));
}

static void evalTexture2DGradOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DOffset(c, c.in[0].x(), c.in[0].y(), computeLodFromGrad2D(c), p.offset.swizzle(0, 1)) * p.scale +
              p.bias;
}
static void evalTexture2DArrayGradOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DArrayOffset(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), computeLodFromGrad2DArray(c),
                                   p.offset.swizzle(0, 1)) *
                  p.scale +
              p.bias;
}
static void evalTexture3DGradOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture3DOffset(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), computeLodFromGrad3D(c), p.offset) * p.scale + p.bias;
}

static void evalTexture2DShadowGradOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadowOffset(c, c.in[0].z(), c.in[0].x(), c.in[0].y(), computeLodFromGrad2D(c),
                                        p.offset.swizzle(0, 1));
}
static void evalTexture2DArrayShadowGradOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DArrayShadowOffset(c, c.in[0].w(), c.in[0].x(), c.in[0].y(), c.in[0].z(),
                                             computeLodFromGrad2DArray(c), p.offset.swizzle(0, 1));
}

static void evalTexture2DShadowProjGrad(gls::ShaderEvalContext &c, const TexLookupParams &)
{
    c.color.x() = texture2DShadow(c, c.in[0].z() / c.in[0].w(), c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(),
                                  computeLodFromGrad2D(c));
}
static void evalTexture2DShadowProjGradOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color.x() = texture2DShadowOffset(c, c.in[0].z() / c.in[0].w(), c.in[0].x() / c.in[0].w(),
                                        c.in[0].y() / c.in[0].w(), computeLodFromGrad2D(c), p.offset.swizzle(0, 1));
}

static void evalTexture2DProjGrad3(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture2D(c, c.in[0].x() / c.in[0].z(), c.in[0].y() / c.in[0].z(), computeLodFromGrad2D(c)) * p.scale + p.bias;
}
static void evalTexture2DProjGrad(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color =
        texture2D(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), computeLodFromGrad2D(c)) * p.scale + p.bias;
}
static void evalTexture3DProjGrad(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3D(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[0].z() / c.in[0].w(),
                        computeLodFromGrad3D(c)) *
                  p.scale +
              p.bias;
}

static void evalTexture2DProjGrad3Offset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DOffset(c, c.in[0].x() / c.in[0].z(), c.in[0].y() / c.in[0].z(), computeLodFromGrad2D(c),
                              p.offset.swizzle(0, 1)) *
                  p.scale +
              p.bias;
}
static void evalTexture2DProjGradOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2DOffset(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), computeLodFromGrad2D(c),
                              p.offset.swizzle(0, 1)) *
                  p.scale +
              p.bias;
}
static void evalTexture3DProjGradOffset(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture3DOffset(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[0].z() / c.in[0].w(),
                              computeLodFromGrad3D(c), p.offset) *
                  p.scale +
              p.bias;
}

// Texel fetch variants

static void evalTexelFetch2D(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    int x   = deChopFloatToInt32(c.in[0].x()) + p.offset.x();
    int y   = deChopFloatToInt32(c.in[0].y()) + p.offset.y();
    int lod = deChopFloatToInt32(c.in[1].x());
    c.color = c.textures[0].tex2D->getLevel(lod).getPixel(x, y) * p.scale + p.bias;
}

static void evalTexelFetch2DArray(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    int x   = deChopFloatToInt32(c.in[0].x()) + p.offset.x();
    int y   = deChopFloatToInt32(c.in[0].y()) + p.offset.y();
    int l   = deChopFloatToInt32(c.in[0].z());
    int lod = deChopFloatToInt32(c.in[1].x());
    c.color = c.textures[0].tex2DArray->getLevel(lod).getPixel(x, y, l) * p.scale + p.bias;
}

static void evalTexelFetch3D(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    int x   = deChopFloatToInt32(c.in[0].x()) + p.offset.x();
    int y   = deChopFloatToInt32(c.in[0].y()) + p.offset.y();
    int z   = deChopFloatToInt32(c.in[0].z()) + p.offset.z();
    int lod = deChopFloatToInt32(c.in[1].x());
    c.color = c.textures[0].tex3D->getLevel(lod).getPixel(x, y, z) * p.scale + p.bias;
}

class TexLookupEvaluator : public gls::ShaderEvaluator
{
public:
    TexLookupEvaluator(TexEvalFunc evalFunc, const TexLookupParams &lookupParams)
        : m_evalFunc(evalFunc)
        , m_lookupParams(lookupParams)
    {
    }

    virtual void evaluate(gls::ShaderEvalContext &ctx)
    {
        m_evalFunc(ctx, m_lookupParams);
    }

private:
    TexEvalFunc m_evalFunc;
    const TexLookupParams &m_lookupParams;
};

class ShaderTextureFunctionCase : public gls::ShaderRenderCase
{
public:
    ShaderTextureFunctionCase(Context &context, const char *name, const char *desc, const TextureLookupSpec &lookup,
                              const TextureSpec &texture, TexEvalFunc evalFunc, bool isVertexCase);
    ~ShaderTextureFunctionCase(void);

    void init(void);
    void deinit(void);

protected:
    void setupUniforms(int programID, const tcu::Vec4 &constCoords);

private:
    void initTexture(void);
    void initShaderSources(void);

    TextureLookupSpec m_lookupSpec;
    TextureSpec m_textureSpec;

    TexLookupParams m_lookupParams;
    TexLookupEvaluator m_evaluator;

    glu::Texture2D *m_texture2D;
    glu::TextureCube *m_textureCube;
    glu::Texture2DArray *m_texture2DArray;
    glu::Texture3D *m_texture3D;
};

ShaderTextureFunctionCase::ShaderTextureFunctionCase(Context &context, const char *name, const char *desc,
                                                     const TextureLookupSpec &lookup, const TextureSpec &texture,
                                                     TexEvalFunc evalFunc, bool isVertexCase)
    : gls::ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name, desc,
                            isVertexCase, m_evaluator)
    , m_lookupSpec(lookup)
    , m_textureSpec(texture)
    , m_evaluator(evalFunc, m_lookupParams)
    , m_texture2D(nullptr)
    , m_textureCube(nullptr)
    , m_texture2DArray(nullptr)
    , m_texture3D(nullptr)
{
}

ShaderTextureFunctionCase::~ShaderTextureFunctionCase(void)
{
    delete m_texture2D;
    delete m_textureCube;
    delete m_texture2DArray;
    delete m_texture3D;
}

void ShaderTextureFunctionCase::init(void)
{
    {
        // Base coord scale & bias
        Vec4 s = m_lookupSpec.maxCoord - m_lookupSpec.minCoord;
        Vec4 b = m_lookupSpec.minCoord;

        float baseCoordTrans[] = {s.x(),        0.0f,         0.f, b.x(),
                                  0.f,          s.y(),        0.f, b.y(),
                                  s.z() / 2.f,  -s.z() / 2.f, 0.f, s.z() / 2.f + b.z(),
                                  -s.w() / 2.f, s.w() / 2.f,  0.f, s.w() / 2.f + b.w()};

        m_userAttribTransforms.push_back(tcu::Mat4(baseCoordTrans));
    }

    bool hasLodBias = functionHasLod(m_lookupSpec.function) || m_lookupSpec.useBias;
    bool isGrad     = functionHasGrad(m_lookupSpec.function);
    DE_ASSERT(!isGrad || !hasLodBias);

    if (hasLodBias)
    {
        float s               = m_lookupSpec.maxLodBias - m_lookupSpec.minLodBias;
        float b               = m_lookupSpec.minLodBias;
        float lodCoordTrans[] = {s / 2.0f, s / 2.0f, 0.f,  b,    0.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f,     0.0f,     0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

        m_userAttribTransforms.push_back(tcu::Mat4(lodCoordTrans));
    }
    else if (isGrad)
    {
        Vec3 sx             = m_lookupSpec.maxDX - m_lookupSpec.minDX;
        Vec3 sy             = m_lookupSpec.maxDY - m_lookupSpec.minDY;
        float gradDxTrans[] = {sx.x() / 2.0f, sx.x() / 2.0f, 0.f,  m_lookupSpec.minDX.x(),
                               sx.y() / 2.0f, sx.y() / 2.0f, 0.0f, m_lookupSpec.minDX.y(),
                               sx.z() / 2.0f, sx.z() / 2.0f, 0.0f, m_lookupSpec.minDX.z(),
                               0.0f,          0.0f,          0.0f, 0.0f};
        float gradDyTrans[] = {-sy.x() / 2.0f, -sy.x() / 2.0f, 0.f,  m_lookupSpec.maxDY.x(),
                               -sy.y() / 2.0f, -sy.y() / 2.0f, 0.0f, m_lookupSpec.maxDY.y(),
                               -sy.z() / 2.0f, -sy.z() / 2.0f, 0.0f, m_lookupSpec.maxDY.z(),
                               0.0f,           0.0f,           0.0f, 0.0f};

        m_userAttribTransforms.push_back(tcu::Mat4(gradDxTrans));
        m_userAttribTransforms.push_back(tcu::Mat4(gradDyTrans));
    }

    initShaderSources();
    initTexture();

    gls::ShaderRenderCase::init();
}

void ShaderTextureFunctionCase::initTexture(void)
{
    static const IVec4 texCubeSwz[] = {IVec4(0, 0, 1, 1), IVec4(1, 1, 0, 0), IVec4(0, 1, 0, 1),
                                       IVec4(1, 0, 1, 0), IVec4(0, 1, 1, 0), IVec4(1, 0, 0, 1)};
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(texCubeSwz) == tcu::CUBEFACE_LAST);

    tcu::TextureFormat texFmt      = glu::mapGLInternalFormat(m_textureSpec.format);
    tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
    tcu::IVec2 viewportSize        = getViewportSize();
    bool useProj = functionHasProj(m_lookupSpec.function) && !functionHasGrad(m_lookupSpec.function) &&
                   !functionHasLod(m_lookupSpec.function);
    bool isAutoLod = functionHasAutoLod(m_isVertexCase ? glu::SHADERTYPE_VERTEX : glu::SHADERTYPE_FRAGMENT,
                                        m_lookupSpec.function); // LOD can vary significantly
    float proj = useProj ? 1.0f / m_lookupSpec.minCoord[m_lookupSpec.function == FUNCTION_TEXTUREPROJ3 ? 2 : 3] : 1.0f;

    switch (m_textureSpec.type)
    {
    case TEXTURETYPE_2D:
    {
        float levelStep  = isAutoLod ? 0.0f : 1.0f / (float)de::max(1, m_textureSpec.numLevels - 1);
        Vec4 cScale      = fmtInfo.valueMax - fmtInfo.valueMin;
        Vec4 cBias       = fmtInfo.valueMin;
        int baseCellSize = de::min(m_textureSpec.width / 4, m_textureSpec.height / 4);

        m_texture2D = new glu::Texture2D(m_renderCtx, m_textureSpec.format, m_textureSpec.width, m_textureSpec.height);
        for (int level = 0; level < m_textureSpec.numLevels; level++)
        {
            float fA    = float(level) * levelStep;
            float fB    = 1.0f - fA;
            Vec4 colorA = cBias + cScale * Vec4(fA, fB, fA, fB);
            Vec4 colorB = cBias + cScale * Vec4(fB, fA, fB, fA);

            m_texture2D->getRefTexture().allocLevel(level);
            tcu::fillWithGrid(m_texture2D->getRefTexture().getLevel(level), de::max(1, baseCellSize >> level), colorA,
                              colorB);
        }
        m_texture2D->upload();

        // Compute LOD.
        float dudx = (m_lookupSpec.maxCoord[0] - m_lookupSpec.minCoord[0]) * proj * (float)m_textureSpec.width /
                     (float)viewportSize[0];
        float dvdy = (m_lookupSpec.maxCoord[1] - m_lookupSpec.minCoord[1]) * proj * (float)m_textureSpec.height /
                     (float)viewportSize[1];
        m_lookupParams.lod = computeLodFromDerivates(DEFAULT_LOD_MODE, dudx, 0.0f, 0.0f, dvdy);

        // Append to texture list.
        m_textures.push_back(gls::TextureBinding(m_texture2D, m_textureSpec.sampler));
        break;
    }

    case TEXTURETYPE_CUBE_MAP:
    {
        float levelStep  = isAutoLod ? 0.0f : 1.0f / (float)de::max(1, m_textureSpec.numLevels - 1);
        Vec4 cScale      = fmtInfo.valueMax - fmtInfo.valueMin;
        Vec4 cBias       = fmtInfo.valueMin;
        Vec4 cCorner     = cBias + cScale * 0.5f;
        int baseCellSize = de::min(m_textureSpec.width / 4, m_textureSpec.height / 4);

        DE_ASSERT(m_textureSpec.width == m_textureSpec.height);
        m_textureCube = new glu::TextureCube(m_renderCtx, m_textureSpec.format, m_textureSpec.width);
        for (int level = 0; level < m_textureSpec.numLevels; level++)
        {
            float fA = float(level) * levelStep;
            float fB = 1.0f - fA;
            Vec2 f(fA, fB);

            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            {
                const IVec4 &swzA = texCubeSwz[face];
                IVec4 swzB        = 1 - swzA;
                Vec4 colorA       = cBias + cScale * f.swizzle(swzA[0], swzA[1], swzA[2], swzA[3]);
                Vec4 colorB       = cBias + cScale * f.swizzle(swzB[0], swzB[1], swzB[2], swzB[3]);

                m_textureCube->getRefTexture().allocLevel((tcu::CubeFace)face, level);

                {
                    const tcu::PixelBufferAccess access =
                        m_textureCube->getRefTexture().getLevelFace(level, (tcu::CubeFace)face);
                    const int lastPix = access.getWidth() - 1;

                    tcu::fillWithGrid(access, de::max(1, baseCellSize >> level), colorA, colorB);

                    // Ensure all corners have identical colors in order to avoid dealing with ambiguous corner texel filtering
                    access.setPixel(cCorner, 0, 0);
                    access.setPixel(cCorner, 0, lastPix);
                    access.setPixel(cCorner, lastPix, 0);
                    access.setPixel(cCorner, lastPix, lastPix);
                }
            }
        }
        m_textureCube->upload();

        // Compute LOD \note Assumes that only single side is accessed and R is constant major axis.
        DE_ASSERT(de::abs(m_lookupSpec.minCoord[2] - m_lookupSpec.maxCoord[2]) < 0.005);
        DE_ASSERT(de::abs(m_lookupSpec.minCoord[0]) < de::abs(m_lookupSpec.minCoord[2]) &&
                  de::abs(m_lookupSpec.maxCoord[0]) < de::abs(m_lookupSpec.minCoord[2]));
        DE_ASSERT(de::abs(m_lookupSpec.minCoord[1]) < de::abs(m_lookupSpec.minCoord[2]) &&
                  de::abs(m_lookupSpec.maxCoord[1]) < de::abs(m_lookupSpec.minCoord[2]));

        tcu::CubeFaceFloatCoords c00 = tcu::getCubeFaceCoords(
            Vec3(m_lookupSpec.minCoord[0] * proj, m_lookupSpec.minCoord[1] * proj, m_lookupSpec.minCoord[2] * proj));
        tcu::CubeFaceFloatCoords c10 = tcu::getCubeFaceCoords(
            Vec3(m_lookupSpec.maxCoord[0] * proj, m_lookupSpec.minCoord[1] * proj, m_lookupSpec.minCoord[2] * proj));
        tcu::CubeFaceFloatCoords c01 = tcu::getCubeFaceCoords(
            Vec3(m_lookupSpec.minCoord[0] * proj, m_lookupSpec.maxCoord[1] * proj, m_lookupSpec.minCoord[2] * proj));
        float dudx = (c10.s - c00.s) * (float)m_textureSpec.width / (float)viewportSize[0];
        float dvdy = (c01.t - c00.t) * (float)m_textureSpec.height / (float)viewportSize[1];

        m_lookupParams.lod = computeLodFromDerivates(DEFAULT_LOD_MODE, dudx, 0.0f, 0.0f, dvdy);

        m_textures.push_back(gls::TextureBinding(m_textureCube, m_textureSpec.sampler));
        break;
    }

    case TEXTURETYPE_2D_ARRAY:
    {
        float layerStep = 1.0f / (float)m_textureSpec.depth;
        float levelStep =
            isAutoLod ? 0.0f : 1.0f / (float)(de::max(1, m_textureSpec.numLevels - 1) * m_textureSpec.depth);
        Vec4 cScale      = fmtInfo.valueMax - fmtInfo.valueMin;
        Vec4 cBias       = fmtInfo.valueMin;
        int baseCellSize = de::min(m_textureSpec.width / 4, m_textureSpec.height / 4);

        m_texture2DArray = new glu::Texture2DArray(m_renderCtx, m_textureSpec.format, m_textureSpec.width,
                                                   m_textureSpec.height, m_textureSpec.depth);
        for (int level = 0; level < m_textureSpec.numLevels; level++)
        {
            m_texture2DArray->getRefTexture().allocLevel(level);
            tcu::PixelBufferAccess levelAccess = m_texture2DArray->getRefTexture().getLevel(level);

            for (int layer = 0; layer < levelAccess.getDepth(); layer++)
            {
                float fA    = (float)layer * layerStep + (float)level * levelStep;
                float fB    = 1.0f - fA;
                Vec4 colorA = cBias + cScale * Vec4(fA, fB, fA, fB);
                Vec4 colorB = cBias + cScale * Vec4(fB, fA, fB, fA);

                tcu::fillWithGrid(
                    tcu::getSubregion(levelAccess, 0, 0, layer, levelAccess.getWidth(), levelAccess.getHeight(), 1),
                    de::max(1, baseCellSize >> level), colorA, colorB);
            }
        }
        m_texture2DArray->upload();

        // Compute LOD.
        float dudx = (m_lookupSpec.maxCoord[0] - m_lookupSpec.minCoord[0]) * proj * (float)m_textureSpec.width /
                     (float)viewportSize[0];
        float dvdy = (m_lookupSpec.maxCoord[1] - m_lookupSpec.minCoord[1]) * proj * (float)m_textureSpec.height /
                     (float)viewportSize[1];
        m_lookupParams.lod = computeLodFromDerivates(DEFAULT_LOD_MODE, dudx, 0.0f, 0.0f, dvdy);

        // Append to texture list.
        m_textures.push_back(gls::TextureBinding(m_texture2DArray, m_textureSpec.sampler));
        break;
    }

    case TEXTURETYPE_3D:
    {
        float levelStep  = isAutoLod ? 0.0f : 1.0f / (float)de::max(1, m_textureSpec.numLevels - 1);
        Vec4 cScale      = fmtInfo.valueMax - fmtInfo.valueMin;
        Vec4 cBias       = fmtInfo.valueMin;
        int baseCellSize = de::min(de::min(m_textureSpec.width / 2, m_textureSpec.height / 2), m_textureSpec.depth / 2);

        m_texture3D = new glu::Texture3D(m_renderCtx, m_textureSpec.format, m_textureSpec.width, m_textureSpec.height,
                                         m_textureSpec.depth);
        for (int level = 0; level < m_textureSpec.numLevels; level++)
        {
            float fA    = (float)level * levelStep;
            float fB    = 1.0f - fA;
            Vec4 colorA = cBias + cScale * Vec4(fA, fB, fA, fB);
            Vec4 colorB = cBias + cScale * Vec4(fB, fA, fB, fA);

            m_texture3D->getRefTexture().allocLevel(level);
            tcu::fillWithGrid(m_texture3D->getRefTexture().getLevel(level), de::max(1, baseCellSize >> level), colorA,
                              colorB);
        }
        m_texture3D->upload();

        // Compute LOD.
        float dudx = (m_lookupSpec.maxCoord[0] - m_lookupSpec.minCoord[0]) * proj * (float)m_textureSpec.width /
                     (float)viewportSize[0];
        float dvdy = (m_lookupSpec.maxCoord[1] - m_lookupSpec.minCoord[1]) * proj * (float)m_textureSpec.height /
                     (float)viewportSize[1];
        float dwdx = (m_lookupSpec.maxCoord[2] - m_lookupSpec.minCoord[2]) * 0.5f * proj * (float)m_textureSpec.depth /
                     (float)viewportSize[0];
        float dwdy = (m_lookupSpec.maxCoord[2] - m_lookupSpec.minCoord[2]) * 0.5f * proj * (float)m_textureSpec.depth /
                     (float)viewportSize[1];
        m_lookupParams.lod = computeLodFromDerivates(DEFAULT_LOD_MODE, dudx, 0.0f, dwdx, 0.0f, dvdy, dwdy);

        // Append to texture list.
        m_textures.push_back(gls::TextureBinding(m_texture3D, m_textureSpec.sampler));
        break;
    }

    default:
        DE_ASSERT(false);
    }

    // Set lookup scale & bias
    m_lookupParams.scale  = fmtInfo.lookupScale;
    m_lookupParams.bias   = fmtInfo.lookupBias;
    m_lookupParams.offset = m_lookupSpec.offset;
}

void ShaderTextureFunctionCase::initShaderSources(void)
{
    Function function = m_lookupSpec.function;
    bool isVtxCase    = m_isVertexCase;
    bool isProj       = functionHasProj(function);
    bool isGrad       = functionHasGrad(function);
    bool isShadow     = m_textureSpec.sampler.compare != tcu::Sampler::COMPAREMODE_NONE;
    bool is2DProj4    = !isShadow && m_textureSpec.type == TEXTURETYPE_2D &&
                     (function == FUNCTION_TEXTUREPROJ || function == FUNCTION_TEXTUREPROJLOD ||
                      function == FUNCTION_TEXTUREPROJGRAD);
    bool isIntCoord           = function == FUNCTION_TEXELFETCH;
    bool hasLodBias           = functionHasLod(m_lookupSpec.function) || m_lookupSpec.useBias;
    int texCoordComps         = m_textureSpec.type == TEXTURETYPE_2D ? 2 : 3;
    int extraCoordComps       = (isProj ? (is2DProj4 ? 2 : 1) : 0) + (isShadow ? 1 : 0);
    glu::DataType coordType   = glu::getDataTypeFloatVec(texCoordComps + extraCoordComps);
    glu::Precision coordPrec  = glu::PRECISION_HIGHP;
    const char *coordTypeName = glu::getDataTypeName(coordType);
    const char *coordPrecName = glu::getPrecisionName(coordPrec);
    tcu::TextureFormat texFmt = glu::mapGLInternalFormat(m_textureSpec.format);
    glu::DataType samplerType = glu::TYPE_LAST;
    glu::DataType gradType    = (m_textureSpec.type == TEXTURETYPE_CUBE_MAP || m_textureSpec.type == TEXTURETYPE_3D) ?
                                    glu::TYPE_FLOAT_VEC3 :
                                    glu::TYPE_FLOAT_VEC2;
    const char *gradTypeName  = glu::getDataTypeName(gradType);
    const char *baseFuncName  = nullptr;

    DE_ASSERT(!isGrad || !hasLodBias);

    switch (m_textureSpec.type)
    {
    case TEXTURETYPE_2D:
        samplerType = isShadow ? glu::TYPE_SAMPLER_2D_SHADOW : glu::getSampler2DType(texFmt);
        break;
    case TEXTURETYPE_CUBE_MAP:
        samplerType = isShadow ? glu::TYPE_SAMPLER_CUBE_SHADOW : glu::getSamplerCubeType(texFmt);
        break;
    case TEXTURETYPE_2D_ARRAY:
        samplerType = isShadow ? glu::TYPE_SAMPLER_2D_ARRAY_SHADOW : glu::getSampler2DArrayType(texFmt);
        break;
    case TEXTURETYPE_3D:
        DE_ASSERT(!isShadow);
        samplerType = glu::getSampler3DType(texFmt);
        break;
    default:
        DE_ASSERT(false);
    }

    switch (m_lookupSpec.function)
    {
    case FUNCTION_TEXTURE:
        baseFuncName = "texture";
        break;
    case FUNCTION_TEXTUREPROJ:
        baseFuncName = "textureProj";
        break;
    case FUNCTION_TEXTUREPROJ3:
        baseFuncName = "textureProj";
        break;
    case FUNCTION_TEXTURELOD:
        baseFuncName = "textureLod";
        break;
    case FUNCTION_TEXTUREPROJLOD:
        baseFuncName = "textureProjLod";
        break;
    case FUNCTION_TEXTUREPROJLOD3:
        baseFuncName = "textureProjLod";
        break;
    case FUNCTION_TEXTUREGRAD:
        baseFuncName = "textureGrad";
        break;
    case FUNCTION_TEXTUREPROJGRAD:
        baseFuncName = "textureProjGrad";
        break;
    case FUNCTION_TEXTUREPROJGRAD3:
        baseFuncName = "textureProjGrad";
        break;
    case FUNCTION_TEXELFETCH:
        baseFuncName = "texelFetch";
        break;
    default:
        DE_ASSERT(false);
    }

    std::ostringstream vert;
    std::ostringstream frag;
    std::ostringstream &op = isVtxCase ? vert : frag;

    vert << "#version 300 es\n"
         << "in highp vec4 a_position;\n"
         << "in " << coordPrecName << " " << coordTypeName << " a_in0;\n";

    if (isGrad)
    {
        vert << "in " << coordPrecName << " " << gradTypeName << " a_in1;\n";
        vert << "in " << coordPrecName << " " << gradTypeName << " a_in2;\n";
    }
    else if (hasLodBias)
        vert << "in " << coordPrecName << " float a_in1;\n";

    frag << "#version 300 es\n"
         << "layout(location = 0) out mediump vec4 o_color;\n";

    if (isVtxCase)
    {
        vert << "out mediump vec4 v_color;\n";
        frag << "in mediump vec4 v_color;\n";
    }
    else
    {
        vert << "out " << coordPrecName << " " << coordTypeName << " v_texCoord;\n";
        frag << "in " << coordPrecName << " " << coordTypeName << " v_texCoord;\n";

        if (isGrad)
        {
            vert << "out " << coordPrecName << " " << gradTypeName << " v_gradX;\n";
            vert << "out " << coordPrecName << " " << gradTypeName << " v_gradY;\n";
            frag << "in " << coordPrecName << " " << gradTypeName << " v_gradX;\n";
            frag << "in " << coordPrecName << " " << gradTypeName << " v_gradY;\n";
        }

        if (hasLodBias)
        {
            vert << "out " << coordPrecName << " float v_lodBias;\n";
            frag << "in " << coordPrecName << " float v_lodBias;\n";
        }
    }

    // Uniforms
    op << "uniform highp " << glu::getDataTypeName(samplerType) << " u_sampler;\n"
       << "uniform highp vec4 u_scale;\n"
       << "uniform highp vec4 u_bias;\n";

    vert << "\nvoid main()\n{\n"
         << "\tgl_Position = a_position;\n";
    frag << "\nvoid main()\n{\n";

    if (isVtxCase)
        vert << "\tv_color = ";
    else
        frag << "\to_color = ";

    // Op.
    {
        const char *texCoord = isVtxCase ? "a_in0" : "v_texCoord";
        const char *gradX    = isVtxCase ? "a_in1" : "v_gradX";
        const char *gradY    = isVtxCase ? "a_in2" : "v_gradY";
        const char *lodBias  = isVtxCase ? "a_in1" : "v_lodBias";

        op << "vec4(" << baseFuncName;
        if (m_lookupSpec.useOffset)
            op << "Offset";
        op << "(u_sampler, ";

        if (isIntCoord)
            op << "ivec" << (texCoordComps + extraCoordComps) << "(";

        op << texCoord;

        if (isIntCoord)
            op << ")";

        if (isGrad)
            op << ", " << gradX << ", " << gradY;

        if (functionHasLod(function))
        {
            if (isIntCoord)
                op << ", int(" << lodBias << ")";
            else
                op << ", " << lodBias;
        }

        if (m_lookupSpec.useOffset)
        {
            int offsetComps = m_textureSpec.type == TEXTURETYPE_3D ? 3 : 2;

            op << ", ivec" << offsetComps << "(";
            for (int ndx = 0; ndx < offsetComps; ndx++)
            {
                if (ndx != 0)
                    op << ", ";
                op << m_lookupSpec.offset[ndx];
            }
            op << ")";
        }

        if (m_lookupSpec.useBias)
            op << ", " << lodBias;

        op << ")";

        if (isShadow)
            op << ", 0.0, 0.0, 1.0)";
        else
            op << ")*u_scale + u_bias";

        op << ";\n";
    }

    if (isVtxCase)
        frag << "\to_color = v_color;\n";
    else
    {
        vert << "\tv_texCoord = a_in0;\n";

        if (isGrad)
        {
            vert << "\tv_gradX = a_in1;\n";
            vert << "\tv_gradY = a_in2;\n";
        }
        else if (hasLodBias)
            vert << "\tv_lodBias = a_in1;\n";
    }

    vert << "}\n";
    frag << "}\n";

    m_vertShaderSource = vert.str();
    m_fragShaderSource = frag.str();
}

void ShaderTextureFunctionCase::deinit(void)
{
    gls::ShaderRenderCase::deinit();

    delete m_texture2D;
    delete m_textureCube;
    delete m_texture2DArray;
    delete m_texture3D;

    m_texture2D      = nullptr;
    m_textureCube    = nullptr;
    m_texture2DArray = nullptr;
    m_texture3D      = nullptr;
}

void ShaderTextureFunctionCase::setupUniforms(int programID, const tcu::Vec4 &)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    gl.uniform1i(gl.getUniformLocation(programID, "u_sampler"), 0);
    gl.uniform4fv(gl.getUniformLocation(programID, "u_scale"), 1, m_lookupParams.scale.getPtr());
    gl.uniform4fv(gl.getUniformLocation(programID, "u_bias"), 1, m_lookupParams.bias.getPtr());
}

class TextureSizeCase : public TestCase
{
public:
    TextureSizeCase(Context &context, const char *name, const char *desc, const char *samplerType,
                    const TextureSpec &texture, bool isVertexCase);
    ~TextureSizeCase(void);

    void deinit(void);
    IterateResult iterate(void);

private:
    struct TestSize
    {
        tcu::IVec3 textureSize;
        int lod;
        int lodBase;
        tcu::IVec3 expectedSize;
    };

    bool initShader(void);
    void freeShader(void);
    bool testTextureSize(const TestSize &);
    std::string genVertexShader(void) const;
    std::string genFragmentShader(void) const;
    glw::GLenum getGLTextureTarget(void) const;

    const char *m_samplerTypeStr;
    const TextureSpec m_textureSpec;
    const bool m_isVertexCase;
    const bool m_has3DSize;
    glu::ShaderProgram *m_program;
    int m_iterationCounter;
};

TextureSizeCase::TextureSizeCase(Context &context, const char *name, const char *desc, const char *samplerType,
                                 const TextureSpec &texture, bool isVertexCase)
    : TestCase(context, name, desc)
    , m_samplerTypeStr(samplerType)
    , m_textureSpec(texture)
    , m_isVertexCase(isVertexCase)
    , m_has3DSize(texture.type == TEXTURETYPE_3D || texture.type == TEXTURETYPE_2D_ARRAY)
    , m_program(nullptr)
    , m_iterationCounter(0)
{
}

TextureSizeCase::~TextureSizeCase(void)
{
    deinit();
}

void TextureSizeCase::deinit(void)
{
    freeShader();
}

TestCase::IterateResult TextureSizeCase::iterate(void)
{
    const int currentIteration = m_iterationCounter++;
    const TestSize testSizes[] = {
        {tcu::IVec3(1, 2, 1), 1, 0, tcu::IVec3(1, 1, 1)},
        {tcu::IVec3(1, 2, 1), 0, 0, tcu::IVec3(1, 2, 1)},

        {tcu::IVec3(1, 3, 2), 0, 0, tcu::IVec3(1, 3, 2)},
        {tcu::IVec3(1, 3, 2), 1, 0, tcu::IVec3(1, 1, 1)},

        {tcu::IVec3(100, 31, 18), 0, 0, tcu::IVec3(100, 31, 18)},
        {tcu::IVec3(100, 31, 18), 1, 0, tcu::IVec3(50, 15, 9)},
        {tcu::IVec3(100, 31, 18), 2, 0, tcu::IVec3(25, 7, 4)},
        {tcu::IVec3(100, 31, 18), 3, 0, tcu::IVec3(12, 3, 2)},
        {tcu::IVec3(100, 31, 18), 4, 0, tcu::IVec3(6, 1, 1)},
        {tcu::IVec3(100, 31, 18), 5, 0, tcu::IVec3(3, 1, 1)},
        {tcu::IVec3(100, 31, 18), 6, 0, tcu::IVec3(1, 1, 1)},

        {tcu::IVec3(100, 128, 32), 0, 0, tcu::IVec3(100, 128, 32)},
        {tcu::IVec3(100, 128, 32), 1, 0, tcu::IVec3(50, 64, 16)},
        {tcu::IVec3(100, 128, 32), 2, 0, tcu::IVec3(25, 32, 8)},
        {tcu::IVec3(100, 128, 32), 3, 0, tcu::IVec3(12, 16, 4)},
        {tcu::IVec3(100, 128, 32), 4, 0, tcu::IVec3(6, 8, 2)},
        {tcu::IVec3(100, 128, 32), 5, 0, tcu::IVec3(3, 4, 1)},
        {tcu::IVec3(100, 128, 32), 6, 0, tcu::IVec3(1, 2, 1)},
        {tcu::IVec3(100, 128, 32), 7, 0, tcu::IVec3(1, 1, 1)},

        // pow 2
        {tcu::IVec3(128, 64, 32), 0, 0, tcu::IVec3(128, 64, 32)},
        {tcu::IVec3(128, 64, 32), 1, 0, tcu::IVec3(64, 32, 16)},
        {tcu::IVec3(128, 64, 32), 2, 0, tcu::IVec3(32, 16, 8)},
        {tcu::IVec3(128, 64, 32), 3, 0, tcu::IVec3(16, 8, 4)},
        {tcu::IVec3(128, 64, 32), 4, 0, tcu::IVec3(8, 4, 2)},
        {tcu::IVec3(128, 64, 32), 5, 0, tcu::IVec3(4, 2, 1)},
        {tcu::IVec3(128, 64, 32), 6, 0, tcu::IVec3(2, 1, 1)},
        {tcu::IVec3(128, 64, 32), 7, 0, tcu::IVec3(1, 1, 1)},

        // w == h
        {tcu::IVec3(1, 1, 1), 0, 0, tcu::IVec3(1, 1, 1)},
        {tcu::IVec3(64, 64, 64), 0, 0, tcu::IVec3(64, 64, 64)},
        {tcu::IVec3(64, 64, 64), 1, 0, tcu::IVec3(32, 32, 32)},
        {tcu::IVec3(64, 64, 64), 2, 0, tcu::IVec3(16, 16, 16)},
        {tcu::IVec3(64, 64, 64), 3, 0, tcu::IVec3(8, 8, 8)},
        {tcu::IVec3(64, 64, 64), 4, 0, tcu::IVec3(4, 4, 4)},

        // with lod base
        {tcu::IVec3(100, 31, 18), 3, 1, tcu::IVec3(6, 1, 1)},
        {tcu::IVec3(128, 64, 32), 3, 1, tcu::IVec3(8, 4, 2)},
        {tcu::IVec3(64, 64, 64), 1, 1, tcu::IVec3(16, 16, 16)},

    };
    const int lastIterationIndex = DE_LENGTH_OF_ARRAY(testSizes) + 1;

    if (currentIteration == 0)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return initShader() ? CONTINUE : STOP;
    }
    else if (currentIteration == lastIterationIndex)
    {
        freeShader();
        return STOP;
    }
    else
    {
        if (!testTextureSize(testSizes[currentIteration - 1]))
            if (m_testCtx.getTestResult() != QP_TEST_RESULT_FAIL)
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected texture size");
        return CONTINUE;
    }
}

bool TextureSizeCase::initShader(void)
{
    const std::string vertSrc = genVertexShader();
    const std::string fragSrc = genFragmentShader();

    DE_ASSERT(m_program == nullptr);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(), glu::makeVtxFragSources(vertSrc, fragSrc));
    m_context.getTestContext().getLog() << *m_program;

    if (!m_program->isOk())
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Shader failed");
        return false;
    }

    return true;
}

void TextureSizeCase::freeShader(void)
{
    delete m_program;
    m_program = nullptr;
}

bool TextureSizeCase::testTextureSize(const TestSize &testSize)
{
    using tcu::TestLog;

    const tcu::Vec4 triangle[3] = // covers entire viewport
        {
            tcu::Vec4(-1, -1, 0, 1),
            tcu::Vec4(4, -1, 0, 1),
            tcu::Vec4(-1, 4, 0, 1),
        };

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    const glw::GLint positionLoc    = gl.getAttribLocation(m_program->getProgram(), "a_position");
    const glw::GLint samplerLoc     = gl.getUniformLocation(m_program->getProgram(), "u_sampler");
    const glw::GLint sizeLoc        = gl.getUniformLocation(m_program->getProgram(), "u_texSize");
    const glw::GLint lodLoc         = gl.getUniformLocation(m_program->getProgram(), "u_lod");
    const glw::GLenum textureTarget = getGLTextureTarget();
    const bool isSquare             = testSize.textureSize.x() == testSize.textureSize.y();
    const bool is2DLodValid         = (testSize.textureSize.x() >> (testSize.lod + testSize.lodBase)) != 0 ||
                              (testSize.textureSize.y() >> (testSize.lod + testSize.lodBase)) != 0;
    bool success = true;
    glw::GLenum errorValue;

    // Skip incompatible cases
    if (m_textureSpec.type == TEXTURETYPE_CUBE_MAP && !isSquare)
        return true;
    if (m_textureSpec.type == TEXTURETYPE_2D && !is2DLodValid)
        return true;
    if (m_textureSpec.type == TEXTURETYPE_2D_ARRAY && !is2DLodValid)
        return true;

    // setup rendering

    gl.useProgram(m_program->getProgram());
    gl.uniform1i(samplerLoc, 0);
    gl.clearColor(0.5f, 0.5f, 0.5f, 1.0f);
    gl.viewport(0, 0, 1, 1);
    gl.vertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, triangle);
    gl.enableVertexAttribArray(positionLoc);

    // setup texture
    {
        const int maxLevel = testSize.lod + testSize.lodBase;
        const int levels   = maxLevel + 1;
        glw::GLuint texId  = 0;

        // gen texture
        gl.genTextures(1, &texId);
        gl.bindTexture(textureTarget, texId);
        gl.texParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.texParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.texParameteri(textureTarget, GL_TEXTURE_BASE_LEVEL, testSize.lodBase);

        if (m_textureSpec.format == GL_DEPTH_COMPONENT16 &&
            std::string(m_samplerTypeStr).find("Shadow") != std::string::npos)
        {
            gl.texParameteri(textureTarget, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            gl.texParameteri(textureTarget, GL_TEXTURE_COMPARE_FUNC,
                             glu::getGLCompareFunc(m_textureSpec.sampler.compare));
        }

        // set up texture

        switch (m_textureSpec.type)
        {
        case TEXTURETYPE_3D:
        {
            m_context.getTestContext().getLog()
                << TestLog::Message << "Testing image size " << testSize.textureSize.x() << "x"
                << testSize.textureSize.y() << "x" << testSize.textureSize.z() << TestLog::EndMessage;
            m_context.getTestContext().getLog() << TestLog::Message << "Lod: " << testSize.lod
                                                << ", base level: " << testSize.lodBase << TestLog::EndMessage;
            m_context.getTestContext().getLog()
                << TestLog::Message << "Expecting: " << testSize.expectedSize.x() << "x" << testSize.expectedSize.y()
                << "x" << testSize.expectedSize.z() << TestLog::EndMessage;

            gl.uniform3iv(sizeLoc, 1, testSize.expectedSize.m_data);
            gl.uniform1iv(lodLoc, 1, &testSize.lod);

            gl.texStorage3D(textureTarget, levels, m_textureSpec.format, testSize.textureSize.x(),
                            testSize.textureSize.y(), testSize.textureSize.z());
            break;
        }

        case TEXTURETYPE_2D:
        case TEXTURETYPE_CUBE_MAP:
        {
            m_context.getTestContext().getLog() << TestLog::Message << "Testing image size " << testSize.textureSize.x()
                                                << "x" << testSize.textureSize.y() << TestLog::EndMessage;
            m_context.getTestContext().getLog() << TestLog::Message << "Lod: " << testSize.lod
                                                << ", base level: " << testSize.lodBase << TestLog::EndMessage;
            m_context.getTestContext().getLog() << TestLog::Message << "Expecting: " << testSize.expectedSize.x() << "x"
                                                << testSize.expectedSize.y() << TestLog::EndMessage;

            gl.uniform2iv(sizeLoc, 1, testSize.expectedSize.m_data);
            gl.uniform1iv(lodLoc, 1, &testSize.lod);

            gl.texStorage2D(textureTarget, levels, m_textureSpec.format, testSize.textureSize.x(),
                            testSize.textureSize.y());
            break;
        }

        case TEXTURETYPE_2D_ARRAY:
        {
            tcu::IVec3 expectedSize(testSize.expectedSize.x(), testSize.expectedSize.y(), testSize.textureSize.z());

            m_context.getTestContext().getLog() << TestLog::Message << "Testing image size " << testSize.textureSize.x()
                                                << "x" << testSize.textureSize.y() << " with "
                                                << testSize.textureSize.z() << " layer(s)" << TestLog::EndMessage;
            m_context.getTestContext().getLog() << TestLog::Message << "Lod: " << testSize.lod
                                                << ", base level: " << testSize.lodBase << TestLog::EndMessage;
            m_context.getTestContext().getLog()
                << TestLog::Message << "Expecting: " << testSize.expectedSize.x() << "x" << testSize.expectedSize.y()
                << " and " << testSize.textureSize.z() << " layer(s)" << TestLog::EndMessage;

            gl.uniform3iv(sizeLoc, 1, expectedSize.m_data);
            gl.uniform1iv(lodLoc, 1, &testSize.lod);

            gl.texStorage3D(textureTarget, levels, m_textureSpec.format, testSize.textureSize.x(),
                            testSize.textureSize.y(), testSize.textureSize.z());
            break;
        }

        default:
        {
            DE_ASSERT(false);
            break;
        }
        }

        errorValue = gl.getError();
        if (errorValue == GL_OUT_OF_MEMORY)
        {
            throw glu::OutOfMemoryError("Failed to allocate texture, got GL_OUT_OF_MEMORY.", "TexStorageXD", __FILE__,
                                        __LINE__);
        }
        else if (errorValue != GL_NO_ERROR)
        {
            // error is a failure too
            m_context.getTestContext().getLog() << tcu::TestLog::Message << "Failed, got "
                                                << glu::getErrorStr(errorValue) << "." << tcu::TestLog::EndMessage;
            success = false;
        }
        else
        {
            // test
            const float colorTolerance = 0.1f;
            tcu::TextureLevel sample(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), 1,
                                     1);
            tcu::Vec4 outputColor;

            gl.clear(GL_COLOR_BUFFER_BIT);
            gl.drawArrays(GL_TRIANGLES, 0, 3);
            gl.finish();

            glu::readPixels(m_context.getRenderContext(), 0, 0, sample.getAccess());

            outputColor = sample.getAccess().getPixel(0, 0);

            if (outputColor.x() >= 1.0f - colorTolerance && outputColor.y() >= 1.0f - colorTolerance &&
                outputColor.z() >= 1.0f - colorTolerance)
            {
                // success
                m_context.getTestContext().getLog() << TestLog::Message << "Passed" << TestLog::EndMessage;
            }
            else
            {
                // failure
                m_context.getTestContext().getLog() << TestLog::Message << "Failed" << TestLog::EndMessage;
                success = false;
            }
        }

        // empty line to format log nicely
        m_context.getTestContext().getLog() << TestLog::Message << TestLog::EndMessage;

        // free
        gl.bindTexture(textureTarget, 0);
        gl.deleteTextures(1, &texId);
    }

    gl.useProgram(0);

    return success;
}

std::string TextureSizeCase::genVertexShader() const
{
    std::ostringstream vert;

    vert << "#version 300 es\n"
         << "in highp vec4 a_position;\n";

    if (m_isVertexCase)
    {
        vert << "out mediump vec4 v_color;\n";
        vert << "uniform highp " << m_samplerTypeStr << " u_sampler;\n";
        vert << "uniform highp ivec" << (m_has3DSize ? 3 : 2) << " u_texSize;\n";
        vert << "uniform highp int u_lod;\n";
    }

    vert << "void main()\n{\n";

    if (m_isVertexCase)
        vert << "    v_color = (textureSize(u_sampler, u_lod) == u_texSize ? vec4(1.0, 1.0, 1.0, 1.0) : vec4(0.0, 0.0, "
                "0.0, 1.0));\n";

    vert << "    gl_Position = a_position;\n"
         << "}\n";

    return vert.str();
}

std::string TextureSizeCase::genFragmentShader() const
{
    std::ostringstream frag;

    frag << "#version 300 es\n"
         << "layout(location = 0) out mediump vec4 o_color;\n";

    if (m_isVertexCase)
        frag << "in mediump vec4 v_color;\n";

    if (!m_isVertexCase)
    {
        frag << "uniform highp " << m_samplerTypeStr << " u_sampler;\n";
        frag << "uniform highp ivec" << (m_has3DSize ? 3 : 2) << " u_texSize;\n";
        frag << "uniform highp int u_lod;\n";
    }

    frag << "void main()\n{\n";

    if (!m_isVertexCase)
        frag << "    o_color = (textureSize(u_sampler, u_lod) == u_texSize ? vec4(1.0, 1.0, 1.0, 1.0) : vec4(0.0, 0.0, "
                "0.0, 1.0));\n";
    else
        frag << "    o_color = v_color;\n";

    frag << "}\n";

    return frag.str();
}

glw::GLenum TextureSizeCase::getGLTextureTarget() const
{
    switch (m_textureSpec.type)
    {
    case TEXTURETYPE_2D:
        return GL_TEXTURE_2D;
    case TEXTURETYPE_CUBE_MAP:
        return GL_TEXTURE_CUBE_MAP;
    case TEXTURETYPE_2D_ARRAY:
        return GL_TEXTURE_2D_ARRAY;
    case TEXTURETYPE_3D:
        return GL_TEXTURE_3D;
    default:
        DE_ASSERT(false);
    }
    return 0;
}

ShaderTextureFunctionTests::ShaderTextureFunctionTests(Context &context)
    : TestCaseGroup(context, "texture_functions", "Texture Access Function Tests")
{
}

ShaderTextureFunctionTests::~ShaderTextureFunctionTests(void)
{
}

enum CaseFlags
{
    VERTEX   = (1 << 0),
    FRAGMENT = (1 << 1),
    BOTH     = VERTEX | FRAGMENT
};

struct TexFuncCaseSpec
{
    const char *name;
    TextureLookupSpec lookupSpec;
    TextureSpec texSpec;
    TexEvalFunc evalFunc;
    uint32_t flags;
};

#define CASE_SPEC(NAME, FUNC, MINCOORD, MAXCOORD, USEBIAS, MINLOD, MAXLOD, USEOFFSET, OFFSET, TEXSPEC, EVALFUNC,   \
                  FLAGS)                                                                                           \
    {                                                                                                              \
        #NAME,                                                                                                     \
            TextureLookupSpec(FUNC, MINCOORD, MAXCOORD, USEBIAS, MINLOD, MAXLOD, tcu::Vec3(0.0f), tcu::Vec3(0.0f), \
                              tcu::Vec3(0.0f), tcu::Vec3(0.0f), USEOFFSET, OFFSET),                                \
            TEXSPEC, EVALFUNC, FLAGS                                                                               \
    }
#define GRAD_CASE_SPEC(NAME, FUNC, MINCOORD, MAXCOORD, MINDX, MAXDX, MINDY, MAXDY, USEOFFSET, OFFSET, TEXSPEC,    \
                       EVALFUNC, FLAGS)                                                                           \
    {                                                                                                             \
        #NAME,                                                                                                    \
            TextureLookupSpec(FUNC, MINCOORD, MAXCOORD, false, 0.0f, 0.0f, MINDX, MAXDX, MINDY, MAXDY, USEOFFSET, \
                              OFFSET),                                                                            \
            TEXSPEC, EVALFUNC, FLAGS                                                                              \
    }

static void createCaseGroup(TestCaseGroup *parent, const char *groupName, const char *groupDesc,
                            const TexFuncCaseSpec *cases, int numCases)
{
    tcu::TestCaseGroup *group = new tcu::TestCaseGroup(parent->getTestContext(), groupName, groupDesc);
    parent->addChild(group);

    for (int ndx = 0; ndx < numCases; ndx++)
    {
        std::string name = cases[ndx].name;
        if (cases[ndx].flags & VERTEX)
            group->addChild(new ShaderTextureFunctionCase(parent->getContext(), (name + "_vertex").c_str(), "",
                                                          cases[ndx].lookupSpec, cases[ndx].texSpec,
                                                          cases[ndx].evalFunc, true));
        if (cases[ndx].flags & FRAGMENT)
            group->addChild(new ShaderTextureFunctionCase(parent->getContext(), (name + "_fragment").c_str(), "",
                                                          cases[ndx].lookupSpec, cases[ndx].texSpec,
                                                          cases[ndx].evalFunc, false));
    }
}

void ShaderTextureFunctionTests::init(void)
{
    // Samplers
    static const tcu::Sampler samplerNearestNoMipmap(
        tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::NEAREST,
        tcu::Sampler::NEAREST, 0.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_NONE,
        0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);
    static const tcu::Sampler samplerLinearNoMipmap(
        tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::LINEAR,
        tcu::Sampler::LINEAR, 0.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_NONE,
        0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);
    static const tcu::Sampler samplerNearestMipmap(
        tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::NEAREST_MIPMAP_NEAREST,
        tcu::Sampler::NEAREST, 0.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_NONE,
        0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);
    static const tcu::Sampler samplerLinearMipmap(
        tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::LINEAR_MIPMAP_NEAREST,
        tcu::Sampler::LINEAR, 0.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_NONE,
        0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);

    static const tcu::Sampler samplerShadowNoMipmap(
        tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::NEAREST,
        tcu::Sampler::NEAREST, 0.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_LESS,
        0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);
    static const tcu::Sampler samplerShadowMipmap(
        tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::NEAREST_MIPMAP_NEAREST,
        tcu::Sampler::NEAREST, 0.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_LESS,
        0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);

    static const tcu::Sampler samplerTexelFetch(
        tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::NEAREST_MIPMAP_NEAREST,
        tcu::Sampler::NEAREST, 0.0f /* LOD threshold */, false /* non-normalized coords */,
        tcu::Sampler::COMPAREMODE_NONE, 0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */,
        true /* seamless cube map */);

    // Default textures.
    //                                                Type                    Format                    W        H        D    L    Sampler
    static const TextureSpec tex2DFixed(TEXTURETYPE_2D, GL_RGBA8, 256, 256, 1, 1, samplerLinearNoMipmap);
    static const TextureSpec tex2DFloat(TEXTURETYPE_2D, GL_RGBA16F, 256, 256, 1, 1, samplerLinearNoMipmap);
    static const TextureSpec tex2DInt(TEXTURETYPE_2D, GL_RGBA8I, 256, 256, 1, 1, samplerNearestNoMipmap);
    static const TextureSpec tex2DUint(TEXTURETYPE_2D, GL_RGBA8UI, 256, 256, 1, 1, samplerNearestNoMipmap);
    static const TextureSpec tex2DMipmapFixed(TEXTURETYPE_2D, GL_RGBA8, 256, 256, 1, 9, samplerLinearMipmap);
    static const TextureSpec tex2DMipmapFloat(TEXTURETYPE_2D, GL_RGBA16F, 256, 256, 1, 9, samplerLinearMipmap);
    static const TextureSpec tex2DMipmapInt(TEXTURETYPE_2D, GL_RGBA8I, 256, 256, 1, 9, samplerNearestMipmap);
    static const TextureSpec tex2DMipmapUint(TEXTURETYPE_2D, GL_RGBA8UI, 256, 256, 1, 9, samplerNearestMipmap);

    static const TextureSpec tex2DShadow(TEXTURETYPE_2D, GL_DEPTH_COMPONENT16, 256, 256, 1, 9, samplerShadowNoMipmap);
    static const TextureSpec tex2DMipmapShadow(TEXTURETYPE_2D, GL_DEPTH_COMPONENT16, 256, 256, 1, 9,
                                               samplerShadowMipmap);

    static const TextureSpec tex2DTexelFetchFixed(TEXTURETYPE_2D, GL_RGBA8, 256, 256, 1, 9, samplerTexelFetch);
    static const TextureSpec tex2DTexelFetchFloat(TEXTURETYPE_2D, GL_RGBA16F, 256, 256, 1, 9, samplerTexelFetch);
    static const TextureSpec tex2DTexelFetchInt(TEXTURETYPE_2D, GL_RGBA8I, 256, 256, 1, 9, samplerTexelFetch);
    static const TextureSpec tex2DTexelFetchUint(TEXTURETYPE_2D, GL_RGBA8UI, 256, 256, 1, 9, samplerTexelFetch);

    static const TextureSpec texCubeFixed(TEXTURETYPE_CUBE_MAP, GL_RGBA8, 256, 256, 1, 1, samplerLinearNoMipmap);
    static const TextureSpec texCubeFloat(TEXTURETYPE_CUBE_MAP, GL_RGBA16F, 256, 256, 1, 1, samplerLinearNoMipmap);
    static const TextureSpec texCubeInt(TEXTURETYPE_CUBE_MAP, GL_RGBA8I, 256, 256, 1, 1, samplerNearestNoMipmap);
    static const TextureSpec texCubeUint(TEXTURETYPE_CUBE_MAP, GL_RGBA8UI, 256, 256, 1, 1, samplerNearestNoMipmap);
    static const TextureSpec texCubeMipmapFixed(TEXTURETYPE_CUBE_MAP, GL_RGBA8, 256, 256, 1, 9, samplerLinearMipmap);
    static const TextureSpec texCubeMipmapFloat(TEXTURETYPE_CUBE_MAP, GL_RGBA16F, 128, 128, 1, 8, samplerLinearMipmap);
    static const TextureSpec texCubeMipmapInt(TEXTURETYPE_CUBE_MAP, GL_RGBA8I, 256, 256, 1, 9, samplerNearestMipmap);
    static const TextureSpec texCubeMipmapUint(TEXTURETYPE_CUBE_MAP, GL_RGBA8UI, 256, 256, 1, 9, samplerNearestMipmap);

    static const TextureSpec texCubeShadow(TEXTURETYPE_CUBE_MAP, GL_DEPTH_COMPONENT16, 256, 256, 1, 1,
                                           samplerShadowNoMipmap);
    static const TextureSpec texCubeMipmapShadow(TEXTURETYPE_CUBE_MAP, GL_DEPTH_COMPONENT16, 256, 256, 1, 9,
                                                 samplerShadowMipmap);

    static const TextureSpec tex2DArrayFixed(TEXTURETYPE_2D_ARRAY, GL_RGBA8, 128, 128, 4, 1, samplerLinearNoMipmap);
    static const TextureSpec tex2DArrayFloat(TEXTURETYPE_2D_ARRAY, GL_RGBA16F, 128, 128, 4, 1, samplerLinearNoMipmap);
    static const TextureSpec tex2DArrayInt(TEXTURETYPE_2D_ARRAY, GL_RGBA8I, 128, 128, 4, 1, samplerNearestNoMipmap);
    static const TextureSpec tex2DArrayUint(TEXTURETYPE_2D_ARRAY, GL_RGBA8UI, 128, 128, 4, 1, samplerNearestNoMipmap);
    static const TextureSpec tex2DArrayMipmapFixed(TEXTURETYPE_2D_ARRAY, GL_RGBA8, 128, 128, 4, 8, samplerLinearMipmap);
    static const TextureSpec tex2DArrayMipmapFloat(TEXTURETYPE_2D_ARRAY, GL_RGBA16F, 128, 128, 4, 8,
                                                   samplerLinearMipmap);
    static const TextureSpec tex2DArrayMipmapInt(TEXTURETYPE_2D_ARRAY, GL_RGBA8I, 128, 128, 4, 8, samplerNearestMipmap);
    static const TextureSpec tex2DArrayMipmapUint(TEXTURETYPE_2D_ARRAY, GL_RGBA8UI, 128, 128, 4, 8,
                                                  samplerNearestMipmap);

    static const TextureSpec tex2DArrayShadow(TEXTURETYPE_2D_ARRAY, GL_DEPTH_COMPONENT16, 128, 128, 4, 1,
                                              samplerShadowNoMipmap);
    static const TextureSpec tex2DArrayMipmapShadow(TEXTURETYPE_2D_ARRAY, GL_DEPTH_COMPONENT16, 128, 128, 4, 8,
                                                    samplerShadowMipmap);

    static const TextureSpec tex2DArrayTexelFetchFixed(TEXTURETYPE_2D_ARRAY, GL_RGBA8, 128, 128, 4, 8,
                                                       samplerTexelFetch);
    static const TextureSpec tex2DArrayTexelFetchFloat(TEXTURETYPE_2D_ARRAY, GL_RGBA16F, 128, 128, 4, 8,
                                                       samplerTexelFetch);
    static const TextureSpec tex2DArrayTexelFetchInt(TEXTURETYPE_2D_ARRAY, GL_RGBA8I, 128, 128, 4, 8,
                                                     samplerTexelFetch);
    static const TextureSpec tex2DArrayTexelFetchUint(TEXTURETYPE_2D_ARRAY, GL_RGBA8UI, 128, 128, 4, 8,
                                                      samplerTexelFetch);

    static const TextureSpec tex3DFixed(TEXTURETYPE_3D, GL_RGBA8, 64, 32, 32, 1, samplerLinearNoMipmap);
    static const TextureSpec tex3DFloat(TEXTURETYPE_3D, GL_RGBA16F, 64, 32, 32, 1, samplerLinearNoMipmap);
    static const TextureSpec tex3DInt(TEXTURETYPE_3D, GL_RGBA8I, 64, 32, 32, 1, samplerNearestNoMipmap);
    static const TextureSpec tex3DUint(TEXTURETYPE_3D, GL_RGBA8UI, 64, 32, 32, 1, samplerNearestNoMipmap);
    static const TextureSpec tex3DMipmapFixed(TEXTURETYPE_3D, GL_RGBA8, 64, 32, 32, 7, samplerLinearMipmap);
    static const TextureSpec tex3DMipmapFloat(TEXTURETYPE_3D, GL_RGBA16F, 64, 32, 32, 7, samplerLinearMipmap);
    static const TextureSpec tex3DMipmapInt(TEXTURETYPE_3D, GL_RGBA8I, 64, 32, 32, 7, samplerNearestMipmap);
    static const TextureSpec tex3DMipmapUint(TEXTURETYPE_3D, GL_RGBA8UI, 64, 32, 32, 7, samplerNearestMipmap);

    static const TextureSpec tex3DTexelFetchFixed(TEXTURETYPE_3D, GL_RGBA8, 64, 32, 32, 7, samplerTexelFetch);
    static const TextureSpec tex3DTexelFetchFloat(TEXTURETYPE_3D, GL_RGBA16F, 64, 32, 32, 7, samplerTexelFetch);
    static const TextureSpec tex3DTexelFetchInt(TEXTURETYPE_3D, GL_RGBA8I, 64, 32, 32, 7, samplerTexelFetch);
    static const TextureSpec tex3DTexelFetchUint(TEXTURETYPE_3D, GL_RGBA8UI, 64, 32, 32, 7, samplerTexelFetch);

    // texture() cases
    static const TexFuncCaseSpec textureCases[] = {
        //          Name                            Function            MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Offset?    Offset        Format                    EvalFunc                Flags
        CASE_SPEC(sampler2d_fixed, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DFixed, evalTexture2D, VERTEX),
        CASE_SPEC(sampler2d_fixed, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapFixed, evalTexture2D, FRAGMENT),
        CASE_SPEC(sampler2d_float, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DFloat, evalTexture2D, VERTEX),
        CASE_SPEC(sampler2d_float, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapFloat, evalTexture2D, FRAGMENT),
        CASE_SPEC(isampler2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), tex2DInt, evalTexture2D, VERTEX),
        CASE_SPEC(isampler2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), tex2DMipmapInt, evalTexture2D, FRAGMENT),
        CASE_SPEC(usampler2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), tex2DUint, evalTexture2D, VERTEX),
        CASE_SPEC(usampler2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), tex2DMipmapUint, evalTexture2D, FRAGMENT),

        CASE_SPEC(sampler2d_bias_fixed, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapFixed, evalTexture2DBias, FRAGMENT),
        CASE_SPEC(sampler2d_bias_float, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapFloat, evalTexture2DBias, FRAGMENT),
        CASE_SPEC(isampler2d_bias, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), true,
                  -2.0f, 2.0f, false, IVec3(0), tex2DMipmapInt, evalTexture2DBias, FRAGMENT),
        CASE_SPEC(usampler2d_bias, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), true,
                  -2.0f, 2.0f, false, IVec3(0), tex2DMipmapUint, evalTexture2DBias, FRAGMENT),

        CASE_SPEC(samplercube_fixed, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), texCubeFixed, evalTextureCube, VERTEX),
        CASE_SPEC(samplercube_fixed, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), texCubeMipmapFixed, evalTextureCube, FRAGMENT),
        CASE_SPEC(samplercube_float, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, -1.01f, 0.0f), Vec4(1.0f, 1.0f, -1.01f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), texCubeFloat, evalTextureCube, VERTEX),
        CASE_SPEC(samplercube_float, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, -1.01f, 0.0f), Vec4(1.0f, 1.0f, -1.01f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), texCubeMipmapFloat, evalTextureCube, FRAGMENT),
        CASE_SPEC(isamplercube, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), texCubeInt, evalTextureCube, VERTEX),
        CASE_SPEC(isamplercube, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), texCubeMipmapInt, evalTextureCube, FRAGMENT),
        CASE_SPEC(usamplercube, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, -1.01f, 0.0f), Vec4(1.0f, 1.0f, -1.01f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), texCubeUint, evalTextureCube, VERTEX),
        CASE_SPEC(usamplercube, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, -1.01f, 0.0f), Vec4(1.0f, 1.0f, -1.01f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), texCubeMipmapUint, evalTextureCube, FRAGMENT),

        CASE_SPEC(samplercube_bias_fixed, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f),
                  Vec4(1.0f, 1.0f, 1.01f, 0.0f), true, -2.0f, 2.0f, false, IVec3(0), texCubeMipmapFixed,
                  evalTextureCubeBias, FRAGMENT),
        CASE_SPEC(samplercube_bias_float, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, -1.01f, 0.0f),
                  Vec4(1.0f, 1.0f, -1.01f, 0.0f), true, -2.0f, 2.0f, false, IVec3(0), texCubeMipmapFloat,
                  evalTextureCubeBias, FRAGMENT),
        CASE_SPEC(isamplercube_bias, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 0.0f),
                  true, -2.0f, 2.0f, false, IVec3(0), texCubeMipmapInt, evalTextureCubeBias, FRAGMENT),
        CASE_SPEC(usamplercube_bias, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, -1.01f, 0.0f), Vec4(1.0f, 1.0f, -1.01f, 0.0f),
                  true, -2.0f, 2.0f, false, IVec3(0), texCubeMipmapUint, evalTextureCubeBias, FRAGMENT),

        CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayFixed, evalTexture2DArray, VERTEX),
        CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayMipmapFixed, evalTexture2DArray, FRAGMENT),
        CASE_SPEC(sampler2darray_float, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayFloat, evalTexture2DArray, VERTEX),
        CASE_SPEC(sampler2darray_float, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayMipmapFloat, evalTexture2DArray, FRAGMENT),
        CASE_SPEC(isampler2darray, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayInt, evalTexture2DArray, VERTEX),
        CASE_SPEC(isampler2darray, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayMipmapInt, evalTexture2DArray, FRAGMENT),
        CASE_SPEC(usampler2darray, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayUint, evalTexture2DArray, VERTEX),
        CASE_SPEC(usampler2darray, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayMipmapUint, evalTexture2DArray, FRAGMENT),

        CASE_SPEC(sampler2darray_bias_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                  Vec4(1.5f, 2.3f, 3.5f, 0.0f), true, -2.0f, 2.0f, false, IVec3(0), tex2DArrayMipmapFixed,
                  evalTexture2DArrayBias, FRAGMENT),
        CASE_SPEC(sampler2darray_bias_float, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                  Vec4(1.5f, 2.3f, 3.5f, 0.0f), true, -2.0f, 2.0f, false, IVec3(0), tex2DArrayMipmapFloat,
                  evalTexture2DArrayBias, FRAGMENT),
        CASE_SPEC(isampler2darray_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  true, -2.0f, 2.0f, false, IVec3(0), tex2DArrayMipmapInt, evalTexture2DArrayBias, FRAGMENT),
        CASE_SPEC(usampler2darray_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  true, -2.0f, 2.0f, false, IVec3(0), tex2DArrayMipmapUint, evalTexture2DArrayBias, FRAGMENT),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex3DFixed, evalTexture3D, VERTEX),
        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex3DMipmapFixed, evalTexture3D, FRAGMENT),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex3DFloat, evalTexture3D, VERTEX),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex3DMipmapFloat, evalTexture3D, FRAGMENT),
        CASE_SPEC(isampler3d, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), tex3DInt, evalTexture3D, VERTEX),
        CASE_SPEC(isampler3d, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), tex3DMipmapInt, evalTexture3D, FRAGMENT),
        CASE_SPEC(usampler3d, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), tex3DUint, evalTexture3D, VERTEX),
        CASE_SPEC(usampler3d, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), tex3DMipmapUint, evalTexture3D, FRAGMENT),

        CASE_SPEC(sampler3d_bias_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  true, -2.0f, 1.0f, false, IVec3(0), tex3DMipmapFixed, evalTexture3DBias, FRAGMENT),
        CASE_SPEC(sampler3d_bias_float, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  true, -2.0f, 1.0f, false, IVec3(0), tex3DMipmapFloat, evalTexture3DBias, FRAGMENT),
        CASE_SPEC(isampler3d_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), true,
                  -2.0f, 2.0f, false, IVec3(0), tex3DMipmapInt, evalTexture3DBias, FRAGMENT),
        CASE_SPEC(usampler3d_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), true,
                  -2.0f, 2.0f, false, IVec3(0), tex3DMipmapUint, evalTexture3DBias, FRAGMENT),

        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 1.0f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DShadow, evalTexture2DShadow, VERTEX),
        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 1.0f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapShadow, evalTexture2DShadow, FRAGMENT),
        CASE_SPEC(sampler2dshadow_bias, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 1.0f, 0.0f),
                  true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapShadow, evalTexture2DShadowBias, FRAGMENT),

        CASE_SPEC(samplercubeshadow, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 1.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), texCubeShadow, evalTextureCubeShadow, VERTEX),
        CASE_SPEC(samplercubeshadow, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 1.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), texCubeMipmapShadow, evalTextureCubeShadow, FRAGMENT),
        CASE_SPEC(samplercubeshadow_bias, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f),
                  Vec4(1.0f, 1.0f, 1.01f, 1.0f), true, -2.0f, 2.0f, false, IVec3(0), texCubeMipmapShadow,
                  evalTextureCubeShadowBias, FRAGMENT),

        CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 1.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayShadow, evalTexture2DArrayShadow, VERTEX),
        CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 1.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayMipmapShadow, evalTexture2DArrayShadow, FRAGMENT)

        // Not in spec.
        //        CASE_SPEC(sampler2darrayshadow_bias, (FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f,  -0.5f,  0.0f), Vec4( 1.5f,  2.3f,  3.5f,  1.0f), true, -2.0f, 2.0f, Vec2(0.0f), Vec2(0.0f), false, IVec3(0)), tex2DArrayMipmapShadow, evalTexture2DArrayShadowBias, FRAGMENT)
    };
    createCaseGroup(this, "texture", "texture() Tests", textureCases, DE_LENGTH_OF_ARRAY(textureCases));

    // textureOffset() cases
    // \note _bias variants are not using mipmap thanks to wide allowed range for LOD computation
    static const TexFuncCaseSpec textureOffsetCases[] = {
        //          Name                            Function            MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Offset?    Offset                Format                    EvalFunc                        Flags
        CASE_SPEC(sampler2d_fixed, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DFixed, evalTexture2DOffset, VERTEX),
        CASE_SPEC(sampler2d_fixed, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapFixed, evalTexture2DOffset, FRAGMENT),
        CASE_SPEC(sampler2d_float, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DFloat, evalTexture2DOffset, VERTEX),
        CASE_SPEC(sampler2d_float, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapFloat, evalTexture2DOffset, FRAGMENT),
        CASE_SPEC(isampler2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DInt, evalTexture2DOffset, VERTEX),
        CASE_SPEC(isampler2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapInt, evalTexture2DOffset, FRAGMENT),
        CASE_SPEC(usampler2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DUint, evalTexture2DOffset, VERTEX),
        CASE_SPEC(usampler2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapUint, evalTexture2DOffset, FRAGMENT),

        CASE_SPEC(sampler2d_bias_fixed, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  true, -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DFixed, evalTexture2DOffsetBias, FRAGMENT),
        CASE_SPEC(sampler2d_bias_float, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  true, -2.0f, 2.0f, true, IVec3(7, -8, 0), tex2DFloat, evalTexture2DOffsetBias, FRAGMENT),
        CASE_SPEC(isampler2d_bias, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), true,
                  -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DInt, evalTexture2DOffsetBias, FRAGMENT),
        CASE_SPEC(usampler2d_bias, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), true,
                  -2.0f, 2.0f, true, IVec3(7, -8, 0), tex2DUint, evalTexture2DOffsetBias, FRAGMENT),

        CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DArrayFixed, evalTexture2DArrayOffset, VERTEX),
        CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DArrayMipmapFixed, evalTexture2DArrayOffset, FRAGMENT),
        CASE_SPEC(sampler2darray_float, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DArrayFloat, evalTexture2DArrayOffset, VERTEX),
        CASE_SPEC(sampler2darray_float, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DArrayMipmapFloat, evalTexture2DArrayOffset, FRAGMENT),
        CASE_SPEC(isampler2darray, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DArrayInt, evalTexture2DArrayOffset, VERTEX),
        CASE_SPEC(isampler2darray, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DArrayMipmapInt, evalTexture2DArrayOffset, FRAGMENT),
        CASE_SPEC(usampler2darray, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DArrayUint, evalTexture2DArrayOffset, VERTEX),
        CASE_SPEC(usampler2darray, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DArrayMipmapUint, evalTexture2DArrayOffset, FRAGMENT),

        CASE_SPEC(sampler2darray_bias_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                  Vec4(1.5f, 2.3f, 3.5f, 0.0f), true, -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DArrayFixed,
                  evalTexture2DArrayOffsetBias, FRAGMENT),
        CASE_SPEC(sampler2darray_bias_float, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                  Vec4(1.5f, 2.3f, 3.5f, 0.0f), true, -2.0f, 2.0f, true, IVec3(7, -8, 0), tex2DArrayFloat,
                  evalTexture2DArrayOffsetBias, FRAGMENT),
        CASE_SPEC(isampler2darray_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  true, -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DArrayInt, evalTexture2DArrayOffsetBias, FRAGMENT),
        CASE_SPEC(usampler2darray_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  true, -2.0f, 2.0f, true, IVec3(7, -8, 0), tex2DArrayUint, evalTexture2DArrayOffsetBias, FRAGMENT),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 3), tex3DFixed, evalTexture3DOffset, VERTEX),
        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(7, 3, -8), tex3DMipmapFixed, evalTexture3DOffset, FRAGMENT),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(3, -8, 7), tex3DFloat, evalTexture3DOffset, VERTEX),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 3), tex3DMipmapFloat, evalTexture3DOffset, FRAGMENT),
        CASE_SPEC(isampler3d, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  0.0f, 0.0f, true, IVec3(7, 3, -8), tex3DInt, evalTexture3DOffset, VERTEX),
        CASE_SPEC(isampler3d, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  0.0f, 0.0f, true, IVec3(3, -8, 7), tex3DMipmapInt, evalTexture3DOffset, FRAGMENT),
        CASE_SPEC(usampler3d, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  0.0f, 0.0f, true, IVec3(-8, 7, 3), tex3DUint, evalTexture3DOffset, VERTEX),
        CASE_SPEC(usampler3d, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  0.0f, 0.0f, true, IVec3(7, 3, -8), tex3DMipmapUint, evalTexture3DOffset, FRAGMENT),

        CASE_SPEC(sampler3d_bias_fixed, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  true, -2.0f, 1.0f, true, IVec3(-8, 7, 3), tex3DFixed, evalTexture3DOffsetBias, FRAGMENT),
        CASE_SPEC(sampler3d_bias_float, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  true, -2.0f, 1.0f, true, IVec3(7, 3, -8), tex3DFloat, evalTexture3DOffsetBias, FRAGMENT),
        CASE_SPEC(isampler3d_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), true,
                  -2.0f, 2.0f, true, IVec3(3, -8, 7), tex3DInt, evalTexture3DOffsetBias, FRAGMENT),
        CASE_SPEC(usampler3d_bias, FUNCTION_TEXTURE, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), true,
                  -2.0f, 2.0f, true, IVec3(-8, 7, 3), tex3DUint, evalTexture3DOffsetBias, FRAGMENT),

        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 1.0f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DShadow, evalTexture2DShadowOffset, VERTEX),
        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 1.0f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapShadow, evalTexture2DShadowOffset, FRAGMENT),
        CASE_SPEC(sampler2dshadow_bias, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 1.0f, 0.0f),
                  true, -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DShadow, evalTexture2DShadowOffsetBias, FRAGMENT)};
    createCaseGroup(this, "textureoffset", "textureOffset() Tests", textureOffsetCases,
                    DE_LENGTH_OF_ARRAY(textureOffsetCases));

    // textureProj() cases
    // \note Currently uses constant divider!
    static const TexFuncCaseSpec textureProjCases[] = {
        //          Name                            Function                MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Offset?    Offset        Format                    EvalFunc                Flags
        CASE_SPEC(sampler2d_vec3_fixed, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, false, IVec3(0), tex2DFixed, evalTexture2DProj3,
                  VERTEX),
        CASE_SPEC(sampler2d_vec3_fixed, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapFixed,
                  evalTexture2DProj3, FRAGMENT),
        CASE_SPEC(sampler2d_vec3_float, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, false, IVec3(0), tex2DFloat, evalTexture2DProj3,
                  VERTEX),
        CASE_SPEC(sampler2d_vec3_float, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapFloat,
                  evalTexture2DProj3, FRAGMENT),
        CASE_SPEC(isampler2d_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, false, IVec3(0), tex2DInt, evalTexture2DProj3,
                  VERTEX),
        CASE_SPEC(isampler2d_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapInt,
                  evalTexture2DProj3, FRAGMENT),
        CASE_SPEC(usampler2d_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, false, IVec3(0), tex2DUint, evalTexture2DProj3,
                  VERTEX),
        CASE_SPEC(usampler2d_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapUint,
                  evalTexture2DProj3, FRAGMENT),

        CASE_SPEC(sampler2d_vec3_bias_fixed, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapFixed,
                  evalTexture2DProj3Bias, FRAGMENT),
        CASE_SPEC(sampler2d_vec3_bias_float, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapFloat,
                  evalTexture2DProj3Bias, FRAGMENT),
        CASE_SPEC(isampler2d_vec3_bias, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapInt,
                  evalTexture2DProj3Bias, FRAGMENT),
        CASE_SPEC(usampler2d_vec3_bias, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapUint,
                  evalTexture2DProj3Bias, FRAGMENT),

        CASE_SPEC(sampler2d_vec4_fixed, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, false, IVec3(0), tex2DFixed, evalTexture2DProj,
                  VERTEX),
        CASE_SPEC(sampler2d_vec4_fixed, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapFixed,
                  evalTexture2DProj, FRAGMENT),
        CASE_SPEC(sampler2d_vec4_float, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, false, IVec3(0), tex2DFloat, evalTexture2DProj,
                  VERTEX),
        CASE_SPEC(sampler2d_vec4_float, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapFloat,
                  evalTexture2DProj, FRAGMENT),
        CASE_SPEC(isampler2d_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f), Vec4(2.25f, 3.45f, 0.0f, 1.5f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DInt, evalTexture2DProj, VERTEX),
        CASE_SPEC(isampler2d_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f), Vec4(2.25f, 3.45f, 0.0f, 1.5f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapInt, evalTexture2DProj, FRAGMENT),
        CASE_SPEC(usampler2d_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f), Vec4(2.25f, 3.45f, 0.0f, 1.5f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DUint, evalTexture2DProj, VERTEX),
        CASE_SPEC(usampler2d_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f), Vec4(2.25f, 3.45f, 0.0f, 1.5f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapUint, evalTexture2DProj, FRAGMENT),

        CASE_SPEC(sampler2d_vec4_bias_fixed, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapFixed,
                  evalTexture2DProjBias, FRAGMENT),
        CASE_SPEC(sampler2d_vec4_bias_float, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapFloat,
                  evalTexture2DProjBias, FRAGMENT),
        CASE_SPEC(isampler2d_vec4_bias, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapInt,
                  evalTexture2DProjBias, FRAGMENT),
        CASE_SPEC(usampler2d_vec4_bias, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapUint,
                  evalTexture2DProjBias, FRAGMENT),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, false, IVec3(0), tex3DFixed, evalTexture3DProj,
                  VERTEX),
        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, false, IVec3(0), tex3DMipmapFixed,
                  evalTexture3DProj, FRAGMENT),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, false, IVec3(0), tex3DFloat, evalTexture3DProj,
                  VERTEX),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, false, IVec3(0), tex3DMipmapFloat,
                  evalTexture3DProj, FRAGMENT),
        CASE_SPEC(isampler3d, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, false, IVec3(0), tex3DInt, evalTexture3DProj,
                  VERTEX),
        CASE_SPEC(isampler3d, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, false, IVec3(0), tex3DMipmapInt,
                  evalTexture3DProj, FRAGMENT),
        CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, false, IVec3(0), tex3DUint, evalTexture3DProj,
                  VERTEX),
        CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, false, IVec3(0), tex3DMipmapUint,
                  evalTexture3DProj, FRAGMENT),

        CASE_SPEC(sampler3d_bias_fixed, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), true, -2.0f, 1.0f, false, IVec3(0), tex3DMipmapFixed,
                  evalTexture3DProjBias, FRAGMENT),
        CASE_SPEC(sampler3d_bias_float, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), true, -2.0f, 1.0f, false, IVec3(0), tex3DMipmapFloat,
                  evalTexture3DProjBias, FRAGMENT),
        CASE_SPEC(isampler3d_bias, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), true, -2.0f, 2.0f, false, IVec3(0), tex3DMipmapInt,
                  evalTexture3DProjBias, FRAGMENT),
        CASE_SPEC(usampler3d_bias, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), true, -2.0f, 2.0f, false, IVec3(0), tex3DMipmapUint,
                  evalTexture3DProjBias, FRAGMENT),

        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJ, Vec4(0.2f, 0.6f, 0.0f, 1.5f), Vec4(-2.25f, -3.45f, 1.5f, 1.5f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DShadow, evalTexture2DShadowProj, VERTEX),
        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJ, Vec4(0.2f, 0.6f, 0.0f, 1.5f), Vec4(-2.25f, -3.45f, 1.5f, 1.5f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DMipmapShadow, evalTexture2DShadowProj, FRAGMENT),
        CASE_SPEC(sampler2dshadow_bias, FUNCTION_TEXTUREPROJ, Vec4(0.2f, 0.6f, 0.0f, 1.5f),
                  Vec4(-2.25f, -3.45f, 1.5f, 1.5f), true, -2.0f, 2.0f, false, IVec3(0), tex2DMipmapShadow,
                  evalTexture2DShadowProjBias, FRAGMENT)};
    createCaseGroup(this, "textureproj", "textureProj() Tests", textureProjCases, DE_LENGTH_OF_ARRAY(textureProjCases));

    // textureProjOffset() cases
    // \note Currently uses constant divider!
    static const TexFuncCaseSpec textureProjOffsetCases[] = {
        //          Name                            Function                MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Offset?    Offset                Format                    EvalFunc                        Flags
        CASE_SPEC(sampler2d_vec3_fixed, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DFixed,
                  evalTexture2DProj3Offset, VERTEX),
        CASE_SPEC(sampler2d_vec3_fixed, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapFixed,
                  evalTexture2DProj3Offset, FRAGMENT),
        CASE_SPEC(sampler2d_vec3_float, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DFloat,
                  evalTexture2DProj3Offset, VERTEX),
        CASE_SPEC(sampler2d_vec3_float, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapFloat,
                  evalTexture2DProj3Offset, FRAGMENT),
        CASE_SPEC(isampler2d_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DInt,
                  evalTexture2DProj3Offset, VERTEX),
        CASE_SPEC(isampler2d_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapInt,
                  evalTexture2DProj3Offset, FRAGMENT),
        CASE_SPEC(usampler2d_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DUint,
                  evalTexture2DProj3Offset, VERTEX),
        CASE_SPEC(usampler2d_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapUint,
                  evalTexture2DProj3Offset, FRAGMENT),

        CASE_SPEC(sampler2d_vec3_bias_fixed, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), true, -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DFixed,
                  evalTexture2DProj3OffsetBias, FRAGMENT),
        CASE_SPEC(sampler2d_vec3_bias_float, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), true, -2.0f, 2.0f, true, IVec3(7, -8, 0), tex2DFloat,
                  evalTexture2DProj3OffsetBias, FRAGMENT),
        CASE_SPEC(isampler2d_vec3_bias, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), true, -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DInt,
                  evalTexture2DProj3OffsetBias, FRAGMENT),
        CASE_SPEC(usampler2d_vec3_bias, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), true, -2.0f, 2.0f, true, IVec3(7, -8, 0), tex2DUint,
                  evalTexture2DProj3OffsetBias, FRAGMENT),

        CASE_SPEC(sampler2d_vec4_fixed, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DFixed,
                  evalTexture2DProjOffset, VERTEX),
        CASE_SPEC(sampler2d_vec4_fixed, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapFixed,
                  evalTexture2DProjOffset, FRAGMENT),
        CASE_SPEC(sampler2d_vec4_float, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DFloat,
                  evalTexture2DProjOffset, VERTEX),
        CASE_SPEC(sampler2d_vec4_float, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapFloat,
                  evalTexture2DProjOffset, FRAGMENT),
        CASE_SPEC(isampler2d_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f), Vec4(2.25f, 3.45f, 0.0f, 1.5f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DInt, evalTexture2DProjOffset, VERTEX),
        CASE_SPEC(isampler2d_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f), Vec4(2.25f, 3.45f, 0.0f, 1.5f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapInt, evalTexture2DProjOffset, FRAGMENT),
        CASE_SPEC(usampler2d_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f), Vec4(2.25f, 3.45f, 0.0f, 1.5f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DUint, evalTexture2DProjOffset, VERTEX),
        CASE_SPEC(usampler2d_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f), Vec4(2.25f, 3.45f, 0.0f, 1.5f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapUint, evalTexture2DProjOffset, FRAGMENT),

        CASE_SPEC(sampler2d_vec4_bias_fixed, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), true, -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DFixed,
                  evalTexture2DProjOffsetBias, FRAGMENT),
        CASE_SPEC(sampler2d_vec4_bias_float, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), true, -2.0f, 2.0f, true, IVec3(7, -8, 0), tex2DFloat,
                  evalTexture2DProjOffsetBias, FRAGMENT),
        CASE_SPEC(isampler2d_vec4_bias, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), true, -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DInt,
                  evalTexture2DProjOffsetBias, FRAGMENT),
        CASE_SPEC(usampler2d_vec4_bias, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), true, -2.0f, 2.0f, true, IVec3(7, -8, 0), tex2DUint,
                  evalTexture2DProjOffsetBias, FRAGMENT),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 3), tex3DFixed,
                  evalTexture3DProjOffset, VERTEX),
        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, true, IVec3(7, 3, -8), tex3DMipmapFixed,
                  evalTexture3DProjOffset, FRAGMENT),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, true, IVec3(3, -8, 7), tex3DFloat,
                  evalTexture3DProjOffset, VERTEX),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 3), tex3DMipmapFloat,
                  evalTexture3DProjOffset, FRAGMENT),
        CASE_SPEC(isampler3d, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, true, IVec3(7, 3, -8), tex3DInt,
                  evalTexture3DProjOffset, VERTEX),
        CASE_SPEC(isampler3d, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, true, IVec3(3, -8, 7), tex3DMipmapInt,
                  evalTexture3DProjOffset, FRAGMENT),
        CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 3), tex3DUint,
                  evalTexture3DProjOffset, VERTEX),
        CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, 0.0f, 0.0f, true, IVec3(7, 3, -8), tex3DMipmapUint,
                  evalTexture3DProjOffset, FRAGMENT),

        CASE_SPEC(sampler3d_bias_fixed, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), true, -2.0f, 2.0f, true, IVec3(-8, 7, 3), tex3DFixed,
                  evalTexture3DProjOffsetBias, FRAGMENT),
        CASE_SPEC(sampler3d_bias_float, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), true, -2.0f, 2.0f, true, IVec3(7, 3, -8), tex3DFloat,
                  evalTexture3DProjOffsetBias, FRAGMENT),
        CASE_SPEC(isampler3d_bias, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), true, -2.0f, 2.0f, true, IVec3(3, -8, 7), tex3DInt,
                  evalTexture3DProjOffsetBias, FRAGMENT),
        CASE_SPEC(usampler3d_bias, FUNCTION_TEXTUREPROJ, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), true, -2.0f, 2.0f, true, IVec3(-8, 7, 3), tex3DUint,
                  evalTexture3DProjOffsetBias, FRAGMENT),

        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJ, Vec4(0.2f, 0.6f, 0.0f, 1.5f), Vec4(-2.25f, -3.45f, 1.5f, 1.5f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DShadow, evalTexture2DShadowProjOffset, VERTEX),
        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJ, Vec4(0.2f, 0.6f, 0.0f, 1.5f), Vec4(-2.25f, -3.45f, 1.5f, 1.5f),
                  false, 0.0f, 0.0f, true, IVec3(7, -8, 0), tex2DMipmapShadow, evalTexture2DShadowProjOffset, FRAGMENT),
        CASE_SPEC(sampler2dshadow_bias, FUNCTION_TEXTUREPROJ, Vec4(0.2f, 0.6f, 0.0f, 1.5f),
                  Vec4(-2.25f, -3.45f, 1.5f, 1.5f), true, -2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DShadow,
                  evalTexture2DShadowProjOffsetBias, FRAGMENT)};
    createCaseGroup(this, "textureprojoffset", "textureOffsetProj() Tests", textureProjOffsetCases,
                    DE_LENGTH_OF_ARRAY(textureProjOffsetCases));

    // textureLod() cases
    static const TexFuncCaseSpec textureLodCases[] = {
        //          Name                            Function                MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Offset?    Offset        Format                    EvalFunc                Flags
        CASE_SPEC(sampler2d_fixed, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapFixed, evalTexture2DLod, BOTH),
        CASE_SPEC(sampler2d_float, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapFloat, evalTexture2DLod, BOTH),
        CASE_SPEC(isampler2d, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  -1.0f, 9.0f, false, IVec3(0), tex2DMipmapInt, evalTexture2DLod, BOTH),
        CASE_SPEC(usampler2d, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  -1.0f, 9.0f, false, IVec3(0), tex2DMipmapUint, evalTexture2DLod, BOTH),

        CASE_SPEC(samplercube_fixed, FUNCTION_TEXTURELOD, Vec4(-1.0f, -1.0f, 1.01f, 0.0f),
                  Vec4(1.0f, 1.0f, 1.01f, 0.0f), false, -1.0f, 9.0f, false, IVec3(0), texCubeMipmapFixed,
                  evalTextureCubeLod, BOTH),
        CASE_SPEC(samplercube_float, FUNCTION_TEXTURELOD, Vec4(-1.0f, -1.0f, -1.01f, 0.0f),
                  Vec4(1.0f, 1.0f, -1.01f, 0.0f), false, -1.0f, 9.0f, false, IVec3(0), texCubeMipmapFloat,
                  evalTextureCubeLod, BOTH),
        CASE_SPEC(isamplercube, FUNCTION_TEXTURELOD, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 0.0f),
                  false, -1.0f, 9.0f, false, IVec3(0), texCubeMipmapInt, evalTextureCubeLod, BOTH),
        CASE_SPEC(usamplercube, FUNCTION_TEXTURELOD, Vec4(-1.0f, -1.0f, -1.01f, 0.0f), Vec4(1.0f, 1.0f, -1.01f, 0.0f),
                  false, -1.0f, 9.0f, false, IVec3(0), texCubeMipmapUint, evalTextureCubeLod, BOTH),

        CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                  Vec4(1.5f, 2.3f, 3.5f, 0.0f), false, -1.0f, 8.0f, false, IVec3(0), tex2DArrayMipmapFixed,
                  evalTexture2DArrayLod, BOTH),
        CASE_SPEC(sampler2darray_float, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                  Vec4(1.5f, 2.3f, 3.5f, 0.0f), false, -1.0f, 8.0f, false, IVec3(0), tex2DArrayMipmapFloat,
                  evalTexture2DArrayLod, BOTH),
        CASE_SPEC(isampler2darray, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, -1.0f, 8.0f, false, IVec3(0), tex2DArrayMipmapInt, evalTexture2DArrayLod, BOTH),
        CASE_SPEC(usampler2darray, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, -1.0f, 8.0f, false, IVec3(0), tex2DArrayMipmapUint, evalTexture2DArrayLod, BOTH),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTURELOD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, -1.0f, 7.0f, false, IVec3(0), tex3DMipmapFixed, evalTexture3DLod, BOTH),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTURELOD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, -1.0f, 7.0f, false, IVec3(0), tex3DMipmapFloat, evalTexture3DLod, BOTH),
        CASE_SPEC(isampler3d, FUNCTION_TEXTURELOD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  -1.0f, 7.0f, false, IVec3(0), tex3DMipmapInt, evalTexture3DLod, BOTH),
        CASE_SPEC(usampler3d, FUNCTION_TEXTURELOD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  -1.0f, 7.0f, false, IVec3(0), tex3DMipmapUint, evalTexture3DLod, BOTH),

        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 1.0f, 0.0f),
                  false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapShadow, evalTexture2DShadowLod, BOTH)};
    createCaseGroup(this, "texturelod", "textureLod() Tests", textureLodCases, DE_LENGTH_OF_ARRAY(textureLodCases));

    // textureLodOffset() cases
    static const TexFuncCaseSpec textureLodOffsetCases[] = {
        //          Name                            Function                MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Offset?    Offset                Format                    EvalFunc                        Flags
        CASE_SPEC(sampler2d_fixed, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, -1.0f, 9.0f, true, IVec3(-8, 7, 0), tex2DMipmapFixed, evalTexture2DLodOffset, BOTH),
        CASE_SPEC(sampler2d_float, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, -1.0f, 9.0f, true, IVec3(7, -8, 0), tex2DMipmapFloat, evalTexture2DLodOffset, BOTH),
        CASE_SPEC(isampler2d, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  -1.0f, 9.0f, true, IVec3(-8, 7, 0), tex2DMipmapInt, evalTexture2DLodOffset, BOTH),
        CASE_SPEC(usampler2d, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  -1.0f, 9.0f, true, IVec3(7, -8, 0), tex2DMipmapUint, evalTexture2DLodOffset, BOTH),

        CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                  Vec4(1.5f, 2.3f, 3.5f, 0.0f), false, -1.0f, 8.0f, true, IVec3(-8, 7, 0), tex2DArrayMipmapFixed,
                  evalTexture2DArrayLodOffset, BOTH),
        CASE_SPEC(sampler2darray_float, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                  Vec4(1.5f, 2.3f, 3.5f, 0.0f), false, -1.0f, 8.0f, true, IVec3(7, -8, 0), tex2DArrayMipmapFloat,
                  evalTexture2DArrayLodOffset, BOTH),
        CASE_SPEC(isampler2darray, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, -1.0f, 8.0f, true, IVec3(-8, 7, 0), tex2DArrayMipmapInt, evalTexture2DArrayLodOffset, BOTH),
        CASE_SPEC(usampler2darray, FUNCTION_TEXTURELOD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f), Vec4(1.5f, 2.3f, 3.5f, 0.0f),
                  false, -1.0f, 8.0f, true, IVec3(7, -8, 0), tex2DArrayMipmapUint, evalTexture2DArrayLodOffset, BOTH),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTURELOD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, -1.0f, 7.0f, true, IVec3(-8, 7, 3), tex3DMipmapFixed, evalTexture3DLodOffset, BOTH),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTURELOD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                  false, -1.0f, 7.0f, true, IVec3(7, 3, -8), tex3DMipmapFloat, evalTexture3DLodOffset, BOTH),
        CASE_SPEC(isampler3d, FUNCTION_TEXTURELOD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  -1.0f, 7.0f, true, IVec3(3, -8, 7), tex3DMipmapInt, evalTexture3DLodOffset, BOTH),
        CASE_SPEC(usampler3d, FUNCTION_TEXTURELOD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f), false,
                  -1.0f, 7.0f, true, IVec3(-8, 7, 3), tex3DMipmapUint, evalTexture3DLodOffset, BOTH),

        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 1.0f, 0.0f),
                  false, -1.0f, 9.0f, true, IVec3(-8, 7, 0), tex2DMipmapShadow, evalTexture2DShadowLodOffset, BOTH)};
    createCaseGroup(this, "texturelodoffset", "textureLodOffset() Tests", textureLodOffsetCases,
                    DE_LENGTH_OF_ARRAY(textureLodOffsetCases));

    // textureProjLod() cases
    static const TexFuncCaseSpec textureProjLodCases[] = {
        //          Name                            Function                    MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Offset?    Offset        Format                    EvalFunc                    Flags
        CASE_SPEC(sampler2d_vec3_fixed, FUNCTION_TEXTUREPROJLOD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapFixed,
                  evalTexture2DProjLod3, BOTH),
        CASE_SPEC(sampler2d_vec3_float, FUNCTION_TEXTUREPROJLOD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapFloat,
                  evalTexture2DProjLod3, BOTH),
        CASE_SPEC(isampler2d_vec3, FUNCTION_TEXTUREPROJLOD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapInt,
                  evalTexture2DProjLod3, BOTH),
        CASE_SPEC(usampler2d_vec3, FUNCTION_TEXTUREPROJLOD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapUint,
                  evalTexture2DProjLod3, BOTH),

        CASE_SPEC(sampler2d_vec4_fixed, FUNCTION_TEXTUREPROJLOD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapFixed,
                  evalTexture2DProjLod, BOTH),
        CASE_SPEC(sampler2d_vec4_float, FUNCTION_TEXTUREPROJLOD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapFloat,
                  evalTexture2DProjLod, BOTH),
        CASE_SPEC(isampler2d_vec4, FUNCTION_TEXTUREPROJLOD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapInt,
                  evalTexture2DProjLod, BOTH),
        CASE_SPEC(usampler2d_vec4, FUNCTION_TEXTUREPROJLOD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapUint,
                  evalTexture2DProjLod, BOTH),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREPROJLOD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, -1.0f, 7.0f, false, IVec3(0), tex3DMipmapFixed,
                  evalTexture3DProjLod, BOTH),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJLOD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, -1.0f, 7.0f, false, IVec3(0), tex3DMipmapFloat,
                  evalTexture3DProjLod, BOTH),
        CASE_SPEC(isampler3d, FUNCTION_TEXTUREPROJLOD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, -1.0f, 7.0f, false, IVec3(0), tex3DMipmapInt,
                  evalTexture3DProjLod, BOTH),
        CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJLOD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, -1.0f, 7.0f, false, IVec3(0), tex3DMipmapUint,
                  evalTexture3DProjLod, BOTH),

        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJLOD, Vec4(0.2f, 0.6f, 0.0f, 1.5f),
                  Vec4(-2.25f, -3.45f, 1.5f, 1.5f), false, -1.0f, 9.0f, false, IVec3(0), tex2DMipmapShadow,
                  evalTexture2DShadowProjLod, BOTH)};
    createCaseGroup(this, "textureprojlod", "textureProjLod() Tests", textureProjLodCases,
                    DE_LENGTH_OF_ARRAY(textureProjLodCases));

    // textureProjLodOffset() cases
    static const TexFuncCaseSpec textureProjLodOffsetCases[] = {
        //          Name                            Function                    MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Offset?    Offset                Format                    EvalFunc                                Flags
        CASE_SPEC(sampler2d_vec3_fixed, FUNCTION_TEXTUREPROJLOD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, -1.0f, 9.0f, true, IVec3(-8, 7, 0), tex2DMipmapFixed,
                  evalTexture2DProjLod3Offset, BOTH),
        CASE_SPEC(sampler2d_vec3_float, FUNCTION_TEXTUREPROJLOD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, -1.0f, 9.0f, true, IVec3(7, -8, 0), tex2DMipmapFloat,
                  evalTexture2DProjLod3Offset, BOTH),
        CASE_SPEC(isampler2d_vec3, FUNCTION_TEXTUREPROJLOD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, -1.0f, 9.0f, true, IVec3(-8, 7, 0), tex2DMipmapInt,
                  evalTexture2DProjLod3Offset, BOTH),
        CASE_SPEC(usampler2d_vec3, FUNCTION_TEXTUREPROJLOD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, -1.0f, 9.0f, true, IVec3(7, -8, 0), tex2DMipmapUint,
                  evalTexture2DProjLod3Offset, BOTH),

        CASE_SPEC(sampler2d_vec4_fixed, FUNCTION_TEXTUREPROJLOD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, -1.0f, 9.0f, true, IVec3(-8, 7, 0), tex2DMipmapFixed,
                  evalTexture2DProjLodOffset, BOTH),
        CASE_SPEC(sampler2d_vec4_float, FUNCTION_TEXTUREPROJLOD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, -1.0f, 9.0f, true, IVec3(7, -8, 0), tex2DMipmapFloat,
                  evalTexture2DProjLodOffset, BOTH),
        CASE_SPEC(isampler2d_vec4, FUNCTION_TEXTUREPROJLOD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, -1.0f, 9.0f, true, IVec3(-8, 7, 0), tex2DMipmapInt,
                  evalTexture2DProjLodOffset, BOTH),
        CASE_SPEC(usampler2d_vec4, FUNCTION_TEXTUREPROJLOD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, -1.0f, 9.0f, true, IVec3(7, -8, 0), tex2DMipmapUint,
                  evalTexture2DProjLodOffset, BOTH),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREPROJLOD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, -1.0f, 7.0f, true, IVec3(-8, 7, 3), tex3DMipmapFixed,
                  evalTexture3DProjLodOffset, BOTH),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJLOD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, -1.0f, 7.0f, true, IVec3(7, 3, -8), tex3DMipmapFloat,
                  evalTexture3DProjLodOffset, BOTH),
        CASE_SPEC(isampler3d, FUNCTION_TEXTUREPROJLOD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, -1.0f, 7.0f, true, IVec3(3, -8, 7), tex3DMipmapInt,
                  evalTexture3DProjLodOffset, BOTH),
        CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJLOD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                  Vec4(-1.13f, -1.7f, -1.7f, -0.75f), false, -1.0f, 7.0f, true, IVec3(-8, 7, 3), tex3DMipmapUint,
                  evalTexture3DProjLodOffset, BOTH),

        CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJLOD, Vec4(0.2f, 0.6f, 0.0f, 1.5f),
                  Vec4(-2.25f, -3.45f, 1.5f, 1.5f), false, -1.0f, 9.0f, true, IVec3(-8, 7, 0), tex2DMipmapShadow,
                  evalTexture2DShadowProjLodOffset, BOTH)};
    createCaseGroup(this, "textureprojlodoffset", "textureProjLodOffset() Tests", textureProjLodOffsetCases,
                    DE_LENGTH_OF_ARRAY(textureProjLodOffsetCases));

    // textureGrad() cases
    // \note Only one of dudx, dudy, dvdx, dvdy is non-zero since spec allows approximating p from derivates by various methods.
    static const TexFuncCaseSpec textureGradCases[] = {
        //          Name                            Function                MinCoord                            MaxCoord                            MinDx                        MaxDx                        MinDy                        MaxDy                        Offset?    Offset        Format                    EvalFunc                Flags
        GRAD_CASE_SPEC(sampler2d_fixed, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f),
                       Vec4(1.5f, 2.3f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapFixed,
                       evalTexture2DGrad, BOTH),
        GRAD_CASE_SPEC(sampler2d_float, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f),
                       Vec4(1.5f, 2.3f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapFloat,
                       evalTexture2DGrad, BOTH),
        GRAD_CASE_SPEC(isampler2d, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f),
                       false, IVec3(0), tex2DMipmapInt, evalTexture2DGrad, BOTH),
        GRAD_CASE_SPEC(usampler2d, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f),
                       false, IVec3(0), tex2DMipmapUint, evalTexture2DGrad, BOTH),

        GRAD_CASE_SPEC(samplercube_fixed, FUNCTION_TEXTUREGRAD, Vec4(-1.0f, -1.0f, 1.01f, 0.0f),
                       Vec4(1.0f, 1.0f, 1.01f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), texCubeMipmapFixed,
                       evalTextureCubeGrad, BOTH),
        GRAD_CASE_SPEC(samplercube_float, FUNCTION_TEXTUREGRAD, Vec4(-1.0f, -1.0f, -1.01f, 0.0f),
                       Vec4(1.0f, 1.0f, -1.01f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), texCubeMipmapFloat,
                       evalTextureCubeGrad, BOTH),
        GRAD_CASE_SPEC(isamplercube, FUNCTION_TEXTUREGRAD, Vec4(-1.0f, -1.0f, 1.01f, 0.0f),
                       Vec4(1.0f, 1.0f, 1.01f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f), false, IVec3(0), texCubeMipmapInt,
                       evalTextureCubeGrad, BOTH),
        GRAD_CASE_SPEC(usamplercube, FUNCTION_TEXTUREGRAD, Vec4(-1.0f, -1.0f, -1.01f, 0.0f),
                       Vec4(1.0f, 1.0f, -1.01f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f), false, IVec3(0), texCubeMipmapUint,
                       evalTextureCubeGrad, BOTH),

        GRAD_CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DArrayMipmapFixed,
                       evalTexture2DArrayGrad, BOTH),
        GRAD_CASE_SPEC(sampler2darray_float, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DArrayMipmapFloat,
                       evalTexture2DArrayGrad, BOTH),
        GRAD_CASE_SPEC(isampler2darray, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f), false, IVec3(0), tex2DArrayMipmapInt,
                       evalTexture2DArrayGrad, BOTH),
        GRAD_CASE_SPEC(usampler2darray, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f), false, IVec3(0), tex2DArrayMipmapUint,
                       evalTexture2DArrayGrad, BOTH),

        GRAD_CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f),
                       Vec4(1.5f, 2.3f, 2.3f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex3DMipmapFixed,
                       evalTexture3DGrad, BOTH),
        GRAD_CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f),
                       Vec4(1.5f, 2.3f, 2.3f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex3DMipmapFloat,
                       evalTexture3DGrad, VERTEX),
        GRAD_CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f),
                       Vec4(1.5f, 2.3f, 2.3f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.2f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex3DMipmapFloat,
                       evalTexture3DGrad, FRAGMENT),
        GRAD_CASE_SPEC(isampler3d, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f),
                       false, IVec3(0), tex3DMipmapInt, evalTexture3DGrad, BOTH),
        GRAD_CASE_SPEC(usampler3d, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f),
                       false, IVec3(0), tex3DMipmapUint, evalTexture3DGrad, VERTEX),
        GRAD_CASE_SPEC(usampler3d, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -0.2f),
                       false, IVec3(0), tex3DMipmapUint, evalTexture3DGrad, FRAGMENT),

        GRAD_CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f),
                       Vec4(1.5f, 2.3f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapShadow,
                       evalTexture2DShadowGrad, BOTH),
        GRAD_CASE_SPEC(samplercubeshadow, FUNCTION_TEXTUREGRAD, Vec4(-1.0f, -1.0f, 1.01f, 0.0f),
                       Vec4(1.0f, 1.0f, 1.01f, 1.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), texCubeMipmapShadow,
                       evalTextureCubeShadowGrad, BOTH),
        GRAD_CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 1.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f), false, IVec3(0), tex2DArrayMipmapShadow,
                       evalTexture2DArrayShadowGrad, VERTEX),
        GRAD_CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 1.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f), false, IVec3(0), tex2DArrayMipmapShadow,
                       evalTexture2DArrayShadowGrad, FRAGMENT)};
    createCaseGroup(this, "texturegrad", "textureGrad() Tests", textureGradCases, DE_LENGTH_OF_ARRAY(textureGradCases));

    // textureGradOffset() cases
    static const TexFuncCaseSpec textureGradOffsetCases[] = {
        //          Name                            Function                MinCoord                            MaxCoord                            MinDx                        MaxDx                        MinDy                        MaxDy                        Offset?    Offset                Format                    EvalFunc                            Flags
        GRAD_CASE_SPEC(sampler2d_fixed, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f),
                       Vec4(1.5f, 2.3f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DMipmapFixed,
                       evalTexture2DGradOffset, BOTH),
        GRAD_CASE_SPEC(sampler2d_float, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f),
                       Vec4(1.5f, 2.3f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(7, -8, 0), tex2DMipmapFloat,
                       evalTexture2DGradOffset, BOTH),
        GRAD_CASE_SPEC(isampler2d, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f),
                       true, IVec3(-8, 7, 0), tex2DMipmapInt, evalTexture2DGradOffset, BOTH),
        GRAD_CASE_SPEC(usampler2d, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f),
                       true, IVec3(7, -8, 0), tex2DMipmapUint, evalTexture2DGradOffset, BOTH),

        GRAD_CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DArrayMipmapFixed,
                       evalTexture2DArrayGradOffset, BOTH),
        GRAD_CASE_SPEC(sampler2darray_float, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(7, -8, 0), tex2DArrayMipmapFloat,
                       evalTexture2DArrayGradOffset, BOTH),
        GRAD_CASE_SPEC(isampler2darray, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DArrayMipmapInt,
                       evalTexture2DArrayGradOffset, BOTH),
        GRAD_CASE_SPEC(usampler2darray, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f), true, IVec3(7, -8, 0), tex2DArrayMipmapUint,
                       evalTexture2DArrayGradOffset, BOTH),

        GRAD_CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f),
                       Vec4(1.5f, 2.3f, 2.3f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(-8, 7, 3), tex3DMipmapFixed,
                       evalTexture3DGradOffset, BOTH),
        GRAD_CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f),
                       Vec4(1.5f, 2.3f, 2.3f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(7, 3, -8), tex3DMipmapFloat,
                       evalTexture3DGradOffset, VERTEX),
        GRAD_CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f),
                       Vec4(1.5f, 2.3f, 2.3f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.2f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(3, -8, 7), tex3DMipmapFloat,
                       evalTexture3DGradOffset, FRAGMENT),
        GRAD_CASE_SPEC(isampler3d, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f),
                       true, IVec3(-8, 7, 3), tex3DMipmapInt, evalTexture3DGradOffset, BOTH),
        GRAD_CASE_SPEC(usampler3d, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f),
                       true, IVec3(7, 3, -8), tex3DMipmapUint, evalTexture3DGradOffset, VERTEX),
        GRAD_CASE_SPEC(usampler3d, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -1.4f, 0.1f, 0.0f), Vec4(1.5f, 2.3f, 2.3f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -0.2f),
                       true, IVec3(3, -8, 7), tex3DMipmapUint, evalTexture3DGradOffset, FRAGMENT),

        GRAD_CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f),
                       Vec4(1.5f, 2.3f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DMipmapShadow,
                       evalTexture2DShadowGradOffset, VERTEX),
        GRAD_CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREGRAD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f),
                       Vec4(1.5f, 2.3f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(7, -8, 0), tex2DMipmapShadow,
                       evalTexture2DShadowGradOffset, FRAGMENT),
        GRAD_CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 1.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DArrayMipmapShadow,
                       evalTexture2DArrayShadowGradOffset, VERTEX),
        GRAD_CASE_SPEC(sampler2darrayshadow, FUNCTION_TEXTUREGRAD, Vec4(-1.2f, -0.4f, -0.5f, 0.0f),
                       Vec4(1.5f, 2.3f, 3.5f, 1.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f), true, IVec3(7, -8, 0), tex2DArrayMipmapShadow,
                       evalTexture2DArrayShadowGradOffset, FRAGMENT)};
    createCaseGroup(this, "texturegradoffset", "textureGradOffset() Tests", textureGradOffsetCases,
                    DE_LENGTH_OF_ARRAY(textureGradOffsetCases));

    // textureProjGrad() cases
    static const TexFuncCaseSpec textureProjGradCases[] = {
        //          Name                            Function                    MinCoord                            MaxCoord                            MinDx                        MaxDx                        MinDy                        MaxDy                        Offset?    Offset        Format                    EvalFunc                    Flags
        GRAD_CASE_SPEC(sampler2d_vec3_fixed, FUNCTION_TEXTUREPROJGRAD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                       Vec4(2.25f, 3.45f, 1.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapFixed,
                       evalTexture2DProjGrad3, BOTH),
        GRAD_CASE_SPEC(sampler2d_vec3_float, FUNCTION_TEXTUREPROJGRAD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                       Vec4(2.25f, 3.45f, 1.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapFloat,
                       evalTexture2DProjGrad3, BOTH),
        GRAD_CASE_SPEC(isampler2d_vec3, FUNCTION_TEXTUREPROJGRAD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                       Vec4(2.25f, 3.45f, 1.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapInt,
                       evalTexture2DProjGrad3, BOTH),
        GRAD_CASE_SPEC(usampler2d_vec3, FUNCTION_TEXTUREPROJGRAD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                       Vec4(2.25f, 3.45f, 1.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f), false, IVec3(0), tex2DMipmapUint,
                       evalTexture2DProjGrad3, BOTH),

        GRAD_CASE_SPEC(sampler2d_vec4_fixed, FUNCTION_TEXTUREPROJGRAD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                       Vec4(2.25f, 3.45f, 0.0f, 1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapFixed,
                       evalTexture2DProjGrad, BOTH),
        GRAD_CASE_SPEC(sampler2d_vec4_float, FUNCTION_TEXTUREPROJGRAD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                       Vec4(2.25f, 3.45f, 0.0f, 1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapFloat,
                       evalTexture2DProjGrad, BOTH),
        GRAD_CASE_SPEC(isampler2d_vec4, FUNCTION_TEXTUREPROJGRAD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                       Vec4(2.25f, 3.45f, 0.0f, 1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapInt,
                       evalTexture2DProjGrad, BOTH),
        GRAD_CASE_SPEC(usampler2d_vec4, FUNCTION_TEXTUREPROJGRAD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                       Vec4(2.25f, 3.45f, 0.0f, 1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f), false, IVec3(0), tex2DMipmapUint,
                       evalTexture2DProjGrad, BOTH),

        GRAD_CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex3DMipmapFixed,
                       evalTexture3DProjGrad, BOTH),
        GRAD_CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex3DMipmapFloat,
                       evalTexture3DProjGrad, VERTEX),
        GRAD_CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.2f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex3DMipmapFloat,
                       evalTexture3DProjGrad, FRAGMENT),
        GRAD_CASE_SPEC(isampler3d, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f), false, IVec3(0), tex3DMipmapInt,
                       evalTexture3DProjGrad, BOTH),
        GRAD_CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f), false, IVec3(0), tex3DMipmapUint,
                       evalTexture3DProjGrad, VERTEX),
        GRAD_CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -0.2f), false, IVec3(0), tex3DMipmapUint,
                       evalTexture3DProjGrad, FRAGMENT),

        GRAD_CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJGRAD, Vec4(0.2f, 0.6f, 0.0f, -1.5f),
                       Vec4(-2.25f, -3.45f, -1.5f, -1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), false, IVec3(0), tex2DMipmapShadow,
                       evalTexture2DShadowProjGrad, VERTEX),
        GRAD_CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJGRAD, Vec4(0.2f, 0.6f, 0.0f, -1.5f),
                       Vec4(-2.25f, -3.45f, -1.5f, -1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f), false, IVec3(0), tex2DMipmapShadow,
                       evalTexture2DShadowProjGrad, FRAGMENT)};
    createCaseGroup(this, "textureprojgrad", "textureProjGrad() Tests", textureProjGradCases,
                    DE_LENGTH_OF_ARRAY(textureProjGradCases));

    // textureProjGradOffset() cases
    static const TexFuncCaseSpec textureProjGradOffsetCases[] = {
        //          Name                            Function                    MinCoord                            MaxCoord                            MinDx                        MaxDx                        MinDy                        MaxDy                        Offset?    Offset                Format                    EvalFunc                            Flags
        GRAD_CASE_SPEC(sampler2d_vec3_fixed, FUNCTION_TEXTUREPROJGRAD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                       Vec4(2.25f, 3.45f, 1.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DMipmapFixed,
                       evalTexture2DProjGrad3Offset, BOTH),
        GRAD_CASE_SPEC(sampler2d_vec3_float, FUNCTION_TEXTUREPROJGRAD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                       Vec4(2.25f, 3.45f, 1.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(7, -8, 0), tex2DMipmapFloat,
                       evalTexture2DProjGrad3Offset, BOTH),
        GRAD_CASE_SPEC(isampler2d_vec3, FUNCTION_TEXTUREPROJGRAD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                       Vec4(2.25f, 3.45f, 1.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DMipmapInt,
                       evalTexture2DProjGrad3Offset, BOTH),
        GRAD_CASE_SPEC(usampler2d_vec3, FUNCTION_TEXTUREPROJGRAD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                       Vec4(2.25f, 3.45f, 1.5f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f), true, IVec3(7, -8, 0), tex2DMipmapUint,
                       evalTexture2DProjGrad3Offset, BOTH),

        GRAD_CASE_SPEC(sampler2d_vec4_fixed, FUNCTION_TEXTUREPROJGRAD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                       Vec4(2.25f, 3.45f, 0.0f, 1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DMipmapFixed,
                       evalTexture2DProjGradOffset, BOTH),
        GRAD_CASE_SPEC(sampler2d_vec4_float, FUNCTION_TEXTUREPROJGRAD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                       Vec4(2.25f, 3.45f, 0.0f, 1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(7, -8, 0), tex2DMipmapFloat,
                       evalTexture2DProjGradOffset, BOTH),
        GRAD_CASE_SPEC(isampler2d_vec4, FUNCTION_TEXTUREPROJGRAD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                       Vec4(2.25f, 3.45f, 0.0f, 1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DMipmapInt,
                       evalTexture2DProjGradOffset, BOTH),
        GRAD_CASE_SPEC(usampler2d_vec4, FUNCTION_TEXTUREPROJGRAD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                       Vec4(2.25f, 3.45f, 0.0f, 1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f), true, IVec3(7, -8, 0), tex2DMipmapUint,
                       evalTexture2DProjGradOffset, BOTH),

        GRAD_CASE_SPEC(sampler3d_fixed, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(-8, 7, 3), tex3DMipmapFixed,
                       evalTexture3DProjGradOffset, BOTH),
        GRAD_CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(7, 3, -8), tex3DMipmapFloat,
                       evalTexture3DProjGradOffset, VERTEX),
        GRAD_CASE_SPEC(sampler3d_float, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.2f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(3, -8, 7), tex3DMipmapFloat,
                       evalTexture3DProjGradOffset, FRAGMENT),
        GRAD_CASE_SPEC(isampler3d, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(-0.2f, 0.0f, 0.0f), true, IVec3(-8, 7, 3), tex3DMipmapInt,
                       evalTexture3DProjGradOffset, BOTH),
        GRAD_CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.2f, 0.0f), true, IVec3(7, 3, -8), tex3DMipmapUint,
                       evalTexture3DProjGradOffset, VERTEX),
        GRAD_CASE_SPEC(usampler3d, FUNCTION_TEXTUREPROJGRAD, Vec4(0.9f, 1.05f, -0.08f, -0.75f),
                       Vec4(-1.13f, -1.7f, -1.7f, -0.75f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -0.2f), true, IVec3(3, -8, 7), tex3DMipmapUint,
                       evalTexture3DProjGradOffset, FRAGMENT),

        GRAD_CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJGRAD, Vec4(0.2f, 0.6f, 0.0f, -1.5f),
                       Vec4(-2.25f, -3.45f, -1.5f, -1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.2f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), true, IVec3(-8, 7, 0), tex2DMipmapShadow,
                       evalTexture2DShadowProjGradOffset, VERTEX),
        GRAD_CASE_SPEC(sampler2dshadow, FUNCTION_TEXTUREPROJGRAD, Vec4(0.2f, 0.6f, 0.0f, -1.5f),
                       Vec4(-2.25f, -3.45f, -1.5f, -1.5f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f),
                       Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -0.2f, 0.0f), true, IVec3(7, -8, 0), tex2DMipmapShadow,
                       evalTexture2DShadowProjGradOffset, FRAGMENT)};
    createCaseGroup(this, "textureprojgradoffset", "textureProjGradOffset() Tests", textureProjGradOffsetCases,
                    DE_LENGTH_OF_ARRAY(textureProjGradOffsetCases));

    // texelFetch() cases
    // \note Level is constant across quad
    static const TexFuncCaseSpec texelFetchCases[] = {
        //          Name                            Function                MinCoord                            MaxCoord                        Bias?    MinLod    MaxLod    Offset?    Offset        Format                        EvalFunc                Flags
        CASE_SPEC(sampler2d_fixed, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(255.9f, 255.9f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex2DTexelFetchFixed, evalTexelFetch2D, BOTH),
        CASE_SPEC(sampler2d_float, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(127.9f, 127.9f, 0.0f, 0.0f),
                  false, 1.0f, 1.0f, false, IVec3(0), tex2DTexelFetchFloat, evalTexelFetch2D, BOTH),
        CASE_SPEC(isampler2d, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(63.9f, 63.9f, 0.0f, 0.0f), false,
                  2.0f, 2.0f, false, IVec3(0), tex2DTexelFetchInt, evalTexelFetch2D, BOTH),
        CASE_SPEC(usampler2d, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(15.9f, 15.9f, 0.0f, 0.0f), false,
                  4.0f, 4.0f, false, IVec3(0), tex2DTexelFetchUint, evalTexelFetch2D, BOTH),

        CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                  Vec4(127.9f, 127.9f, 3.9f, 0.0f), false, 0.0f, 0.0f, false, IVec3(0), tex2DArrayTexelFetchFixed,
                  evalTexelFetch2DArray, BOTH),
        CASE_SPEC(sampler2darray_float, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                  Vec4(63.9f, 63.9f, 3.9f, 0.0f), false, 1.0f, 1.0f, false, IVec3(0), tex2DArrayTexelFetchFloat,
                  evalTexelFetch2DArray, BOTH),
        CASE_SPEC(isampler2darray, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(31.9f, 31.9f, 3.9f, 0.0f),
                  false, 2.0f, 2.0f, false, IVec3(0), tex2DArrayTexelFetchInt, evalTexelFetch2DArray, BOTH),
        CASE_SPEC(usampler2darray, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(15.9f, 15.9f, 3.9f, 0.0f),
                  false, 3.0f, 3.0f, false, IVec3(0), tex2DArrayTexelFetchUint, evalTexelFetch2DArray, BOTH),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(63.9f, 31.9f, 31.9f, 0.0f),
                  false, 0.0f, 0.0f, false, IVec3(0), tex3DTexelFetchFixed, evalTexelFetch3D, BOTH),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(31.9f, 15.9f, 15.9f, 0.0f),
                  false, 1.0f, 1.0f, false, IVec3(0), tex3DTexelFetchFloat, evalTexelFetch3D, BOTH),
        CASE_SPEC(isampler3d, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(15.9f, 7.9f, 7.9f, 0.0f), false,
                  2.0f, 2.0f, false, IVec3(0), tex3DTexelFetchInt, evalTexelFetch3D, BOTH),
        CASE_SPEC(usampler3d, FUNCTION_TEXELFETCH, Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(63.9f, 31.9f, 31.9f, 0.0f), false,
                  0.0f, 0.0f, false, IVec3(0), tex3DTexelFetchUint, evalTexelFetch3D, BOTH)};
    createCaseGroup(this, "texelfetch", "texelFetch() Tests", texelFetchCases, DE_LENGTH_OF_ARRAY(texelFetchCases));

    // texelFetchOffset() cases
    static const TexFuncCaseSpec texelFetchOffsetCases[] = {
        //          Name                            Function                MinCoord                            MaxCoord                        Bias?    MinLod    MaxLod    Offset?    Offset        Format                        EvalFunc                Flags
        CASE_SPEC(sampler2d_fixed, FUNCTION_TEXELFETCH, Vec4(8.0f, -7.0f, 0.0f, 0.0f), Vec4(263.9f, 248.9f, 0.0f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DTexelFetchFixed, evalTexelFetch2D, BOTH),
        CASE_SPEC(sampler2d_float, FUNCTION_TEXELFETCH, Vec4(-7.0f, 8.0f, 0.0f, 0.0f), Vec4(120.9f, 135.9f, 0.0f, 0.0f),
                  false, 1.0f, 1.0f, true, IVec3(7, -8, 0), tex2DTexelFetchFloat, evalTexelFetch2D, BOTH),
        CASE_SPEC(isampler2d, FUNCTION_TEXELFETCH, Vec4(8.0f, -7.0f, 0.0f, 0.0f), Vec4(71.9f, 56.9f, 0.0f, 0.0f), false,
                  2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DTexelFetchInt, evalTexelFetch2D, BOTH),
        CASE_SPEC(usampler2d, FUNCTION_TEXELFETCH, Vec4(-7.0f, 8.0f, 0.0f, 0.0f), Vec4(8.9f, 23.9f, 0.0f, 0.0f), false,
                  4.0f, 4.0f, true, IVec3(7, -8, 0), tex2DTexelFetchUint, evalTexelFetch2D, BOTH),

        CASE_SPEC(sampler2darray_fixed, FUNCTION_TEXELFETCH, Vec4(8.0f, -7.0f, 0.0f, 0.0f),
                  Vec4(135.9f, 120.9f, 3.9f, 0.0f), false, 0.0f, 0.0f, true, IVec3(-8, 7, 0), tex2DArrayTexelFetchFixed,
                  evalTexelFetch2DArray, BOTH),
        CASE_SPEC(sampler2darray_float, FUNCTION_TEXELFETCH, Vec4(-7.0f, 8.0f, 0.0f, 0.0f),
                  Vec4(56.9f, 71.9f, 3.9f, 0.0f), false, 1.0f, 1.0f, true, IVec3(7, -8, 0), tex2DArrayTexelFetchFloat,
                  evalTexelFetch2DArray, BOTH),
        CASE_SPEC(isampler2darray, FUNCTION_TEXELFETCH, Vec4(8.0f, -7.0f, 0.0f, 0.0f), Vec4(39.9f, 24.9f, 3.9f, 0.0f),
                  false, 2.0f, 2.0f, true, IVec3(-8, 7, 0), tex2DArrayTexelFetchInt, evalTexelFetch2DArray, BOTH),
        CASE_SPEC(usampler2darray, FUNCTION_TEXELFETCH, Vec4(-7.0f, 8.0f, 0.0f, 0.0f), Vec4(8.9f, 23.9f, 3.9f, 0.0f),
                  false, 3.0f, 3.0f, true, IVec3(7, -8, 0), tex2DArrayTexelFetchUint, evalTexelFetch2DArray, BOTH),

        CASE_SPEC(sampler3d_fixed, FUNCTION_TEXELFETCH, Vec4(8.0f, -7.0f, -3.0f, 0.0f), Vec4(71.9f, 24.9f, 28.9f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 3), tex3DTexelFetchFixed, evalTexelFetch3D, BOTH),
        CASE_SPEC(sampler3d_float, FUNCTION_TEXELFETCH, Vec4(-7.0f, -3.0f, 8.0f, 0.0f), Vec4(24.9f, 12.9f, 23.9f, 0.0f),
                  false, 1.0f, 1.0f, true, IVec3(7, 3, -8), tex3DTexelFetchFloat, evalTexelFetch3D, BOTH),
        CASE_SPEC(isampler3d, FUNCTION_TEXELFETCH, Vec4(-3.0f, 8.0f, -7.0f, 0.0f), Vec4(12.9f, 15.9f, 0.9f, 0.0f),
                  false, 2.0f, 2.0f, true, IVec3(3, -8, 7), tex3DTexelFetchInt, evalTexelFetch3D, BOTH),
        CASE_SPEC(usampler3d, FUNCTION_TEXELFETCH, Vec4(8.0f, -7.0f, -3.0f, 0.0f), Vec4(71.9f, 24.9f, 28.9f, 0.0f),
                  false, 0.0f, 0.0f, true, IVec3(-8, 7, 3), tex3DTexelFetchUint, evalTexelFetch3D, BOTH)};
    createCaseGroup(this, "texelfetchoffset", "texelFetchOffset() Tests", texelFetchOffsetCases,
                    DE_LENGTH_OF_ARRAY(texelFetchOffsetCases));

    // textureSize() cases
    {
        struct TextureSizeCaseSpec
        {
            const char *name;
            const char *samplerName;
            TextureSpec textureSpec;
        } textureSizeCases[] = {
            {"sampler2d_fixed", "sampler2D", tex2DFixed},
            {"sampler2d_float", "sampler2D", tex2DFloat},
            {"isampler2d", "isampler2D", tex2DInt},
            {"usampler2d", "usampler2D", tex2DUint},
            {"sampler2dshadow", "sampler2DShadow", tex2DShadow},
            {"sampler3d_fixed", "sampler3D", tex3DFixed},
            {"sampler3d_float", "sampler3D", tex3DFloat},
            {"isampler3d", "isampler3D", tex3DInt},
            {"usampler3d", "usampler3D", tex3DUint},
            {"samplercube_fixed", "samplerCube", texCubeFixed},
            {"samplercube_float", "samplerCube", texCubeFloat},
            {"isamplercube", "isamplerCube", texCubeInt},
            {"usamplercube", "usamplerCube", texCubeUint},
            {"samplercubeshadow", "samplerCubeShadow", texCubeShadow},
            {"sampler2darray_fixed", "sampler2DArray", tex2DArrayFixed},
            {"sampler2darray_float", "sampler2DArray", tex2DArrayFloat},
            {"isampler2darray", "isampler2DArray", tex2DArrayInt},
            {"usampler2darray", "usampler2DArray", tex2DArrayUint},
            {"sampler2darrayshadow", "sampler2DArrayShadow", tex2DArrayShadow},
        };

        tcu::TestCaseGroup *group = new tcu::TestCaseGroup(m_testCtx, "texturesize", "textureSize() Tests");
        addChild(group);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(textureSizeCases); ++ndx)
        {
            group->addChild(
                new TextureSizeCase(m_context, (std::string(textureSizeCases[ndx].name) + "_vertex").c_str(), "",
                                    textureSizeCases[ndx].samplerName, textureSizeCases[ndx].textureSpec, true));
            group->addChild(
                new TextureSizeCase(m_context, (std::string(textureSizeCases[ndx].name) + "_fragment").c_str(), "",
                                    textureSizeCases[ndx].samplerName, textureSizeCases[ndx].textureSpec, false));
        }
    }

    // Negative cases.
    {
        gls::ShaderLibrary library(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo());
        std::vector<tcu::TestNode *> negativeCases = library.loadShaderFile("shaders/invalid_texture_functions.test");

        tcu::TestCaseGroup *group =
            new tcu::TestCaseGroup(m_testCtx, "invalid", "Invalid texture function usage", negativeCases);
        addChild(group);
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
