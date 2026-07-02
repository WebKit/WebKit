/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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

#include "es2fShaderTextureFunctionTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "glsShaderLibrary.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuMatrix.hpp"
#include "tcuMatrixUtil.hpp"

#include <sstream>

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

namespace
{

enum Function
{
    FUNCTION_TEXTURE = 0,  //!< texture(), textureOffset()
    FUNCTION_TEXTUREPROJ,  //!< textureProj(), textureProjOffset()
    FUNCTION_TEXTUREPROJ3, //!< textureProj(sampler2D, vec3)
    FUNCTION_TEXTURELOD,   // ...
    FUNCTION_TEXTUREPROJLOD,
    FUNCTION_TEXTUREPROJLOD3, //!< textureProjLod(sampler2D, vec3)

    FUNCTION_LAST
};

inline bool functionHasProj(Function function)
{
    return function == FUNCTION_TEXTUREPROJ || function == FUNCTION_TEXTUREPROJ3 ||
           function == FUNCTION_TEXTUREPROJLOD || function == FUNCTION_TEXTUREPROJLOD3;
}

inline bool functionHasLod(Function function)
{
    return function == FUNCTION_TEXTURELOD || function == FUNCTION_TEXTUREPROJLOD ||
           function == FUNCTION_TEXTUREPROJLOD3;
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

    TextureLookupSpec(void)
        : function(FUNCTION_LAST)
        , minCoord(0.0f)
        , maxCoord(1.0f)
        , useBias(false)
        , minLodBias(0.0f)
        , maxLodBias(0.0f)
    {
    }

    TextureLookupSpec(Function function_, const tcu::Vec4 &minCoord_, const tcu::Vec4 &maxCoord_, bool useBias_,
                      float minLodBias_, float maxLodBias_)
        : function(function_)
        , minCoord(minCoord_)
        , maxCoord(maxCoord_)
        , useBias(useBias_)
        , minLodBias(minLodBias_)
        , maxLodBias(maxLodBias_)
    {
    }
};

enum TextureType
{
    TEXTURETYPE_2D,
    TEXTURETYPE_CUBE_MAP,

    TEXTURETYPE_LAST
};

struct TextureSpec
{
    TextureType type; //!< Texture type (2D, cubemap, ...)
    uint32_t format;
    uint32_t dataType;
    int width;
    int height;
    int numLevels;
    tcu::Sampler sampler;

    TextureSpec(void) : type(TEXTURETYPE_LAST), format(GL_NONE), dataType(GL_NONE), width(0), height(0), numLevels(0)
    {
    }

    TextureSpec(TextureType type_, uint32_t format_, uint32_t dataType_, int width_, int height_, int numLevels_,
                const tcu::Sampler &sampler_)
        : type(type_)
        , format(format_)
        , dataType(dataType_)
        , width(width_)
        , height(height_)
        , numLevels(numLevels_)
        , sampler(sampler_)
    {
    }
};

struct TexLookupParams
{
    float lod;
    tcu::Vec4 scale;
    tcu::Vec4 bias;

    TexLookupParams(void) : lod(0.0f), scale(1.0f), bias(0.0f)
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

typedef void (*TexEvalFunc)(gls::ShaderEvalContext &c, const TexLookupParams &lookupParams);

inline Vec4 texture2D(const gls::ShaderEvalContext &c, float s, float t, float lod)
{
    return c.textures[0].tex2D->sample(c.textures[0].sampler, s, t, lod);
}
inline Vec4 textureCube(const gls::ShaderEvalContext &c, float s, float t, float r, float lod)
{
    return c.textures[0].texCube->sample(c.textures[0].sampler, s, t, r, lod);
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

static void evalTexture2DBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x(), c.in[0].y(), p.lod + c.in[1].x()) * p.scale + p.bias;
}
static void evalTextureCubeBias(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = textureCube(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), p.lod + c.in[1].x()) * p.scale + p.bias;
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

static void evalTexture2DLod(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x(), c.in[0].y(), c.in[1].x()) * p.scale + p.bias;
}
static void evalTextureCubeLod(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = textureCube(c, c.in[0].x(), c.in[0].y(), c.in[0].z(), c.in[1].x()) * p.scale + p.bias;
}

static void evalTexture2DProjLod3(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x() / c.in[0].z(), c.in[0].y() / c.in[0].z(), c.in[1].x()) * p.scale + p.bias;
}
static void evalTexture2DProjLod(gls::ShaderEvalContext &c, const TexLookupParams &p)
{
    c.color = texture2D(c, c.in[0].x() / c.in[0].w(), c.in[0].y() / c.in[0].w(), c.in[1].x()) * p.scale + p.bias;
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
{
}

ShaderTextureFunctionCase::~ShaderTextureFunctionCase(void)
{
    delete m_texture2D;
    delete m_textureCube;
}

void ShaderTextureFunctionCase::init(void)
{
    if (m_isVertexCase)
    {
        const glw::Functions &gl = m_renderCtx.getFunctions();
        int numVertexUnits       = 0;
        gl.getIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &numVertexUnits);
        if (numVertexUnits < 1)
            throw tcu::NotSupportedError("Vertex shader texture access is not supported");
    }

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

    if (functionHasLod(m_lookupSpec.function) || m_lookupSpec.useBias)
    {
        float s               = m_lookupSpec.maxLodBias - m_lookupSpec.minLodBias;
        float b               = m_lookupSpec.minLodBias;
        float lodCoordTrans[] = {s / 2.0f, s / 2.0f, 0.f,  b,    0.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f,     0.0f,     0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

        m_userAttribTransforms.push_back(tcu::Mat4(lodCoordTrans));
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

    tcu::TextureFormat texFmt      = glu::mapGLTransferFormat(m_textureSpec.format, m_textureSpec.dataType);
    tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFmt);
    tcu::IVec2 viewportSize        = getViewportSize();
    bool useProj                   = functionHasProj(m_lookupSpec.function) && !functionHasLod(m_lookupSpec.function);
    float proj = useProj ? 1.0f / m_lookupSpec.minCoord[m_lookupSpec.function == FUNCTION_TEXTUREPROJ3 ? 2 : 3] : 1.0f;

    switch (m_textureSpec.type)
    {
    case TEXTURETYPE_2D:
    {
        float cStep      = 1.0f / (float)de::max(1, m_textureSpec.numLevels - 1);
        Vec4 cScale      = fmtInfo.valueMax - fmtInfo.valueMin;
        Vec4 cBias       = fmtInfo.valueMin;
        int baseCellSize = de::min(m_textureSpec.width / 4, m_textureSpec.height / 4);

        m_texture2D = new glu::Texture2D(m_renderCtx, m_textureSpec.format, m_textureSpec.dataType, m_textureSpec.width,
                                         m_textureSpec.height);
        for (int level = 0; level < m_textureSpec.numLevels; level++)
        {
            float fA    = float(level) * cStep;
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
        m_lookupParams.lod =
            glu::TextureTestUtil::computeLodFromDerivates(glu::TextureTestUtil::LODMODE_EXACT, dudx, 0.0f, 0.0f, dvdy);

        // Append to texture list.
        m_textures.push_back(gls::TextureBinding(m_texture2D, m_textureSpec.sampler));
        break;
    }

    case TEXTURETYPE_CUBE_MAP:
    {
        float cStep      = 1.0f / (float)de::max(1, m_textureSpec.numLevels - 1);
        Vec4 cScale      = fmtInfo.valueMax - fmtInfo.valueMin;
        Vec4 cBias       = fmtInfo.valueMin;
        int baseCellSize = de::min(m_textureSpec.width / 4, m_textureSpec.height / 4);

        DE_ASSERT(m_textureSpec.width == m_textureSpec.height);
        m_textureCube =
            new glu::TextureCube(m_renderCtx, m_textureSpec.format, m_textureSpec.dataType, m_textureSpec.width);
        for (int level = 0; level < m_textureSpec.numLevels; level++)
        {
            float fA = float(level) * cStep;
            float fB = 1.0f - fA;
            Vec2 f(fA, fB);

            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
            {
                const IVec4 &swzA = texCubeSwz[face];
                IVec4 swzB        = 1 - swzA;
                Vec4 colorA       = cBias + cScale * f.swizzle(swzA[0], swzA[1], swzA[2], swzA[3]);
                Vec4 colorB       = cBias + cScale * f.swizzle(swzB[0], swzB[1], swzB[2], swzB[3]);

                m_textureCube->getRefTexture().allocLevel((tcu::CubeFace)face, level);
                tcu::fillWithGrid(m_textureCube->getRefTexture().getLevelFace(level, (tcu::CubeFace)face),
                                  de::max(1, baseCellSize >> level), colorA, colorB);
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

        m_lookupParams.lod =
            glu::TextureTestUtil::computeLodFromDerivates(glu::TextureTestUtil::LODMODE_EXACT, dudx, 0.0f, 0.0f, dvdy);

        m_textures.push_back(gls::TextureBinding(m_textureCube, m_textureSpec.sampler));
        break;
    }

    default:
        DE_ASSERT(false);
    }

    // Set lookup scale & bias
    m_lookupParams.scale = fmtInfo.lookupScale;
    m_lookupParams.bias  = fmtInfo.lookupBias;
}

void ShaderTextureFunctionCase::initShaderSources(void)
{
    Function function = m_lookupSpec.function;
    bool isVtxCase    = m_isVertexCase;
    bool isProj       = functionHasProj(function);
    bool is2DProj4    = m_textureSpec.type == TEXTURETYPE_2D &&
                     (function == FUNCTION_TEXTUREPROJ || function == FUNCTION_TEXTUREPROJLOD);
    bool hasLodBias           = functionHasLod(m_lookupSpec.function) || m_lookupSpec.useBias;
    int texCoordComps         = m_textureSpec.type == TEXTURETYPE_2D ? 2 : 3;
    int extraCoordComps       = isProj ? (is2DProj4 ? 2 : 1) : 0;
    glu::DataType coordType   = glu::getDataTypeFloatVec(texCoordComps + extraCoordComps);
    glu::Precision coordPrec  = glu::PRECISION_MEDIUMP;
    const char *coordTypeName = glu::getDataTypeName(coordType);
    const char *coordPrecName = glu::getPrecisionName(coordPrec);
    tcu::TextureFormat texFmt = glu::mapGLTransferFormat(m_textureSpec.format, m_textureSpec.dataType);
    glu::DataType samplerType = glu::TYPE_LAST;
    const char *baseFuncName  = m_textureSpec.type == TEXTURETYPE_2D ? "texture2D" : "textureCube";
    const char *funcExt       = nullptr;

    switch (m_textureSpec.type)
    {
    case TEXTURETYPE_2D:
        samplerType = glu::getSampler2DType(texFmt);
        break;
    case TEXTURETYPE_CUBE_MAP:
        samplerType = glu::getSamplerCubeType(texFmt);
        break;
    default:
        DE_ASSERT(false);
    }

    switch (m_lookupSpec.function)
    {
    case FUNCTION_TEXTURE:
        funcExt = "";
        break;
    case FUNCTION_TEXTUREPROJ:
        funcExt = "Proj";
        break;
    case FUNCTION_TEXTUREPROJ3:
        funcExt = "Proj";
        break;
    case FUNCTION_TEXTURELOD:
        funcExt = "Lod";
        break;
    case FUNCTION_TEXTUREPROJLOD:
        funcExt = "ProjLod";
        break;
    case FUNCTION_TEXTUREPROJLOD3:
        funcExt = "ProjLod";
        break;
    default:
        DE_ASSERT(false);
    }

    std::ostringstream vert;
    std::ostringstream frag;
    std::ostringstream &op = isVtxCase ? vert : frag;

    vert << "attribute highp vec4 a_position;\n"
         << "attribute " << coordPrecName << " " << coordTypeName << " a_in0;\n";

    if (hasLodBias)
        vert << "attribute " << coordPrecName << " float a_in1;\n";

    if (isVtxCase)
    {
        vert << "varying mediump vec4 v_color;\n";
        frag << "varying mediump vec4 v_color;\n";
    }
    else
    {
        vert << "varying " << coordPrecName << " " << coordTypeName << " v_texCoord;\n";
        frag << "varying " << coordPrecName << " " << coordTypeName << " v_texCoord;\n";

        if (hasLodBias)
        {
            vert << "varying " << coordPrecName << " float v_lodBias;\n";
            frag << "varying " << coordPrecName << " float v_lodBias;\n";
        }
    }

    // Uniforms
    op << "uniform lowp " << glu::getDataTypeName(samplerType) << " u_sampler;\n";

    vert << "\nvoid main()\n{\n"
         << "\tgl_Position = a_position;\n";
    frag << "\nvoid main()\n{\n";

    if (isVtxCase)
        vert << "\tv_color = ";
    else
        frag << "\tgl_FragColor = ";

    // Op.
    {
        const char *texCoord = isVtxCase ? "a_in0" : "v_texCoord";
        const char *lodBias  = isVtxCase ? "a_in1" : "v_lodBias";

        op << baseFuncName << funcExt;
        op << "(u_sampler, " << texCoord;

        if (functionHasLod(function) || m_lookupSpec.useBias)
            op << ", " << lodBias;

        op << ");\n";
    }

    if (isVtxCase)
        frag << "\tgl_FragColor = v_color;\n";
    else
    {
        vert << "\tv_texCoord = a_in0;\n";

        if (hasLodBias)
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

    m_texture2D   = nullptr;
    m_textureCube = nullptr;
}

void ShaderTextureFunctionCase::setupUniforms(int programID, const tcu::Vec4 &)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    gl.uniform1i(gl.getUniformLocation(programID, "u_sampler"), 0);
}

ShaderTextureFunctionTests::ShaderTextureFunctionTests(Context &context)
    : TestCaseGroup(context, "texture_functions", "Texture Access Function Tests")
{
}

ShaderTextureFunctionTests::~ShaderTextureFunctionTests(void)
{
}

struct TexFuncCaseSpec
{
    const char *name;
    TextureLookupSpec lookupSpec;
    TextureSpec texSpec;
    TexEvalFunc evalFunc;
};

#define CASE_SPEC(NAME, FUNC, MINCOORD, MAXCOORD, USEBIAS, MINLOD, MAXLOD, TEXSPEC, EVALFUNC)          \
    {                                                                                                  \
        #NAME, TextureLookupSpec(FUNC, MINCOORD, MAXCOORD, USEBIAS, MINLOD, MAXLOD), TEXSPEC, EVALFUNC \
    }

static void createCaseGroup(TestCaseGroup *parent, const char *groupName, const char *groupDesc,
                            const TexFuncCaseSpec *cases, int numCases, bool isVertex)
{
    tcu::TestCaseGroup *group = new tcu::TestCaseGroup(parent->getTestContext(), groupName, groupDesc);
    parent->addChild(group);

    for (int ndx = 0; ndx < numCases; ndx++)
        group->addChild(new ShaderTextureFunctionCase(parent->getContext(), cases[ndx].name, "", cases[ndx].lookupSpec,
                                                      cases[ndx].texSpec, cases[ndx].evalFunc, isVertex));
}

void ShaderTextureFunctionTests::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // Samplers
    static const tcu::Sampler samplerLinearNoMipmap(tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL,
                                                    tcu::Sampler::REPEAT_GL, tcu::Sampler::LINEAR,
                                                    tcu::Sampler::LINEAR);
    static tcu::Sampler samplerLinearMipmap(tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL,
                                            tcu::Sampler::LINEAR_MIPMAP_NEAREST, tcu::Sampler::LINEAR);

    samplerLinearMipmap.seamlessCubeMap = glu::IsES3Compatible(gl);

    // Default textures.
    //                                                Type            Format        DataType            W        H        L    Sampler
    static const TextureSpec tex2D(TEXTURETYPE_2D, GL_RGBA, GL_UNSIGNED_BYTE, 256, 256, 1, samplerLinearNoMipmap);
    static const TextureSpec tex2DMipmap(TEXTURETYPE_2D, GL_RGBA, GL_UNSIGNED_BYTE, 256, 256, 9, samplerLinearMipmap);

    static const TextureSpec texCube(TEXTURETYPE_CUBE_MAP, GL_RGBA, GL_UNSIGNED_BYTE, 256, 256, 1,
                                     samplerLinearNoMipmap);
    static const TextureSpec texCubeMipmap(TEXTURETYPE_CUBE_MAP, GL_RGBA, GL_UNSIGNED_BYTE, 256, 256, 9,
                                           samplerLinearMipmap);

    // Vertex cases
    static const TexFuncCaseSpec vertexCases[] = {
        //          Name                        Function                    MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Texture            EvalFunc
        CASE_SPEC(texture2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, tex2D, evalTexture2D),
        //        CASE_SPEC(texture2d_bias, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f,  0.0f,  0.0f), Vec4( 1.5f,  2.3f,  0.0f,  0.0f), true, -2.0f, 2.0f, tex2D, evalTexture2DBias),
        CASE_SPEC(texture2dproj_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, tex2D, evalTexture2DProj3),
        CASE_SPEC(texture2dproj_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, tex2D, evalTexture2DProj),
        CASE_SPEC(texture2dlod, FUNCTION_TEXTURELOD, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f),
                  false, -1.0f, 9.0f, tex2DMipmap, evalTexture2DLod),
        //        CASE_SPEC(texture2dproj_vec3_bias, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f,  1.5f,  0.0f), Vec4(2.25f, 3.45f,  1.5f,  0.0f), true, -2.0f, 2.0f, tex2D, evalTexture2DProj3Bias),
        //        CASE_SPEC(texture2dproj_vec4_bias, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f,  0.0f,  1.5f), Vec4(2.25f, 3.45f,  0.0f,  1.5f), true, -2.0f, 2.0f, tex2D, evalTexture2DProjBias),
        CASE_SPEC(texture2dprojlod_vec3, FUNCTION_TEXTUREPROJLOD3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, -1.0f, 9.0f, tex2D, evalTexture2DProjLod3),
        CASE_SPEC(texture2dprojlod_vec4, FUNCTION_TEXTUREPROJLOD, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, -1.0f, 9.0f, tex2D, evalTexture2DProjLod),
        CASE_SPEC(texturecube, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 0.0f), false,
                  0.0f, 0.0f, texCube, evalTextureCube),
        //        CASE_SPEC(texturecube_bias, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, -1.01f,  0.0f), Vec4( 1.0f,  1.0f, -1.01f,  0.0f), true, -2.0f, 2.0f, texCube, evalTextureCubeBias),
        CASE_SPEC(texturecubelod, FUNCTION_TEXTURELOD, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 0.0f),
                  false, -1.0f, 9.0f, texCubeMipmap, evalTextureCubeLod),
    };
    createCaseGroup(this, "vertex", "Vertex Shader Texture Lookups", &vertexCases[0], DE_LENGTH_OF_ARRAY(vertexCases),
                    true);

    // Fragment cases
    static const TexFuncCaseSpec fragmentCases[] = {
        //          Name                        Function                MinCoord                            MaxCoord                            Bias?    MinLod    MaxLod    Texture            EvalFunc
        CASE_SPEC(texture2d, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), false,
                  0.0f, 0.0f, tex2DMipmap, evalTexture2D),
        CASE_SPEC(texture2d_bias, FUNCTION_TEXTURE, Vec4(-0.2f, -0.4f, 0.0f, 0.0f), Vec4(1.5f, 2.3f, 0.0f, 0.0f), true,
                  -2.0f, 2.0f, tex2DMipmap, evalTexture2DBias),
        CASE_SPEC(texture2dproj_vec3, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), false, 0.0f, 0.0f, tex2DMipmap, evalTexture2DProj3),
        CASE_SPEC(texture2dproj_vec4, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), false, 0.0f, 0.0f, tex2DMipmap, evalTexture2DProj),
        CASE_SPEC(texture2dproj_vec3_bias, FUNCTION_TEXTUREPROJ3, Vec4(-0.3f, -0.6f, 1.5f, 0.0f),
                  Vec4(2.25f, 3.45f, 1.5f, 0.0f), true, -2.0f, 2.0f, tex2DMipmap, evalTexture2DProj3Bias),
        CASE_SPEC(texture2dproj_vec4_bias, FUNCTION_TEXTUREPROJ, Vec4(-0.3f, -0.6f, 0.0f, 1.5f),
                  Vec4(2.25f, 3.45f, 0.0f, 1.5f), true, -2.0f, 2.0f, tex2DMipmap, evalTexture2DProjBias),
        CASE_SPEC(texturecube, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, 1.01f, 0.0f), Vec4(1.0f, 1.0f, 1.01f, 0.0f), false,
                  0.0f, 0.0f, texCubeMipmap, evalTextureCube),
        CASE_SPEC(texturecube_bias, FUNCTION_TEXTURE, Vec4(-1.0f, -1.0f, -1.01f, 0.0f), Vec4(1.0f, 1.0f, -1.01f, 0.0f),
                  true, -2.0f, 2.0f, texCubeMipmap, evalTextureCubeBias)};
    createCaseGroup(this, "fragment", "Fragment Shader Texture Lookups", &fragmentCases[0],
                    DE_LENGTH_OF_ARRAY(fragmentCases), false);

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
} // namespace gles2
} // namespace deqp
