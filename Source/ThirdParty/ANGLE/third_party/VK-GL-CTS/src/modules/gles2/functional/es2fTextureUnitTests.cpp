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
 * \brief Texture unit usage tests.
 *
 * \todo [2012-07-12 nuutti] Come up with a good way to make these tests faster.
 *//*--------------------------------------------------------------------*/

#include "es2fTextureUnitTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluContextInfo.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuMatrix.hpp"
#include "tcuRenderTarget.hpp"
#include "sglrContextUtil.hpp"
#include "sglrReferenceContext.hpp"
#include "sglrGLContext.hpp"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

using std::string;
using std::vector;
using tcu::IVec2;
using tcu::Mat3;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using namespace glw; // GL types

namespace deqp
{

using namespace gls::TextureTestUtil;

namespace gles2
{
namespace Functional
{

static const int VIEWPORT_WIDTH  = 128;
static const int VIEWPORT_HEIGHT = 128;

static const int TEXTURE_WIDTH_2D  = 128;
static const int TEXTURE_HEIGHT_2D = 128;

// \note Cube map texture size is larger in order to make minifications possible - otherwise would need to display different faces at same time.
static const int TEXTURE_WIDTH_CUBE  = 256;
static const int TEXTURE_HEIGHT_CUBE = 256;

static const int GRID_CELL_SIZE = 8;

static const GLenum s_testFormats[] = {GL_RGB, GL_RGBA, GL_ALPHA, GL_LUMINANCE, GL_LUMINANCE_ALPHA};

static const GLenum s_testDataTypes[] = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_SHORT_5_6_5,
    GL_UNSIGNED_SHORT_4_4_4_4,
    GL_UNSIGNED_SHORT_5_5_5_1,
};

static const GLenum s_testWrapModes[] = {
    GL_CLAMP_TO_EDGE,
    GL_REPEAT,
    GL_MIRRORED_REPEAT,
};

static const GLenum s_testMinFilters[] = {GL_NEAREST,
                                          GL_LINEAR,
                                          GL_NEAREST_MIPMAP_NEAREST,
                                          GL_LINEAR_MIPMAP_NEAREST,
                                          GL_NEAREST_MIPMAP_LINEAR,
                                          GL_LINEAR_MIPMAP_LINEAR};

static const GLenum s_testNonMipmapMinFilters[] = {GL_NEAREST, GL_LINEAR};

static const GLenum s_testMagFilters[] = {GL_NEAREST, GL_LINEAR};

static const GLenum s_cubeFaceTargets[] = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

static string generateMultiTexFragmentShader(int numUnits, const GLenum *unitTypes)
{
    // The fragment shader calculates the average of a set of textures.

    string samplersStr;
    string matricesStr;
    string lookupsStr;

    string colorMultiplier = "(1.0/" + de::toString(numUnits) + ".0)";

    for (int ndx = 0; ndx < numUnits; ndx++)
    {
        string ndxStr             = de::toString(ndx);
        string samplerName        = "u_sampler" + ndxStr;
        string transformationName = "u_trans" + ndxStr;
        const char *samplerType   = unitTypes[ndx] == GL_TEXTURE_2D ? "sampler2D" : "samplerCube";
        const char *lookupFunc    = unitTypes[ndx] == GL_TEXTURE_2D ? "texture2D" : "textureCube";

        samplersStr += string("") + "uniform mediump " + samplerType + " " + samplerName + ";\n";
        matricesStr += "uniform mediump mat3 " + transformationName + ";\n";

        string lookupCoord = transformationName + "*vec3(v_coord, 1.0)";

        if (unitTypes[ndx] == GL_TEXTURE_2D)
            lookupCoord = "vec2(" + lookupCoord + ")";

        lookupsStr +=
            "\tcolor += " + colorMultiplier + "*" + lookupFunc + "(" + samplerName + ", " + lookupCoord + ");\n";
    }

    return samplersStr + matricesStr +
           "varying mediump vec2 v_coord;\n"
           "\n"
           "void main (void)\n"
           "{\n"
           "    mediump vec4 color = vec4(0.0);\n" +
           lookupsStr +
           "    gl_FragColor = color;\n"
           "}\n";
}

static sglr::pdec::ShaderProgramDeclaration generateShaderProgramDeclaration(int numUnits, const GLenum *unitTypes)
{
    sglr::pdec::ShaderProgramDeclaration decl;

    decl << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT);
    decl << sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT);
    decl << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT);
    decl << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT);

    for (int ndx = 0; ndx < numUnits; ++ndx)
    {
        string samplerName        = "u_sampler" + de::toString(ndx);
        string transformationName = "u_trans" + de::toString(ndx);

        decl << sglr::pdec::Uniform(samplerName, (unitTypes[ndx] == GL_TEXTURE_2D) ? (glu::TYPE_SAMPLER_2D) :
                                                                                     (glu::TYPE_SAMPLER_CUBE));
        decl << sglr::pdec::Uniform(transformationName, glu::TYPE_FLOAT_MAT3);
    }

    decl << sglr::pdec::VertexSource("attribute highp vec4 a_position;\n"
                                     "attribute mediump vec2 a_coord;\n"
                                     "varying mediump vec2 v_coord;\n"
                                     "\n"
                                     "void main (void)\n"
                                     "{\n"
                                     "    gl_Position = a_position;\n"
                                     "    v_coord = a_coord;\n"
                                     "}\n");
    decl << sglr::pdec::FragmentSource(generateMultiTexFragmentShader(numUnits, unitTypes));

    return decl;
}

// Calculates values to be used in calculateLod().
static Vec4 calculateLodDerivateParts(const Mat3 &transformation)
{
    // Calculate transformed coordinates of three corners.
    Vec2 trans00 = (transformation * Vec3(0.0f, 0.0f, 1.0f)).xy();
    Vec2 trans01 = (transformation * Vec3(0.0f, 1.0f, 1.0f)).xy();
    Vec2 trans10 = (transformation * Vec3(1.0f, 0.0f, 1.0f)).xy();

    return Vec4(trans10.x() - trans00.x(), trans01.x() - trans00.x(), trans10.y() - trans00.y(),
                trans01.y() - trans00.y());
}

// Calculates the maximum allowed lod from derivates
static float calculateLodMax(const Vec4 &derivateParts, const tcu::IVec2 &textureSize, const Vec2 &screenDerivate)
{
    float dudx = derivateParts.x() * (float)textureSize.x() * screenDerivate.x();
    float dudy = derivateParts.y() * (float)textureSize.x() * screenDerivate.y();
    float dvdx = derivateParts.z() * (float)textureSize.y() * screenDerivate.x();
    float dvdy = derivateParts.w() * (float)textureSize.y() * screenDerivate.y();

    return deFloatLog2(de::max(de::abs(dudx), de::abs(dudy)) + de::max(de::abs(dvdx), de::abs(dvdy)));
}

// Calculates the minimum allowed lod from derivates
static float calculateLodMin(const Vec4 &derivateParts, const tcu::IVec2 &textureSize, const Vec2 &screenDerivate)
{
    float dudx = derivateParts.x() * (float)textureSize.x() * screenDerivate.x();
    float dudy = derivateParts.y() * (float)textureSize.x() * screenDerivate.y();
    float dvdx = derivateParts.z() * (float)textureSize.y() * screenDerivate.x();
    float dvdy = derivateParts.w() * (float)textureSize.y() * screenDerivate.y();

    return deFloatLog2(de::max(de::max(de::abs(dudx), de::abs(dudy)), de::max(de::abs(dvdx), de::abs(dvdy))));
}

class MultiTexShader : public sglr::ShaderProgram
{
public:
    MultiTexShader(uint32_t randSeed, int numUnits, const vector<GLenum> &unitTypes);

    void setUniforms(sglr::Context &context, uint32_t program) const;
    void makeSafeLods(
        const vector<IVec2> &textureSizes,
        const IVec2 &viewportSize); // Modifies texture coordinates so that LODs aren't too close to x.5 or 0.0 .

private:
    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const;
    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const;

    int m_numUnits;
    vector<GLenum> m_unitTypes; // 2d or cube map.
    vector<Mat3> m_transformations;
    vector<Vec4> m_lodDerivateParts; // Parts of lod derivates; computed in init(), used in eval().
};

MultiTexShader::MultiTexShader(uint32_t randSeed, int numUnits, const vector<GLenum> &unitTypes)
    : sglr::ShaderProgram(generateShaderProgramDeclaration(numUnits, &unitTypes[0]))
    , m_numUnits(numUnits)
    , m_unitTypes(unitTypes)
{
    // 2d-to-cube-face transformations.
    // \note 2d coordinates range from 0 to 1 and cube face coordinates from -1 to 1, so scaling is done as well.
    static const float s_cubeTransforms[][3 * 3] = {// Face -X: (x, y, 1) -> (-1, -(2*y-1), +(2*x-1))
                                                    {0.0f, 0.0f, -1.0f, 0.0f, -2.0f, 1.0f, 2.0f, 0.0f, -1.0f},
                                                    // Face +X: (x, y, 1) -> (+1, -(2*y-1), -(2*x-1))
                                                    {0.0f, 0.0f, 1.0f, 0.0f, -2.0f, 1.0f, -2.0f, 0.0f, 1.0f},
                                                    // Face -Y: (x, y, 1) -> (+(2*x-1), -1, -(2*y-1))
                                                    {2.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, -2.0f, 1.0f},
                                                    // Face +Y: (x, y, 1) -> (+(2*x-1), +1, +(2*y-1))
                                                    {2.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 2.0f, -1.0f},
                                                    // Face -Z: (x, y, 1) -> (-(2*x-1), -(2*y-1), -1)
                                                    {-2.0f, 0.0f, 1.0f, 0.0f, -2.0f, 1.0f, 0.0f, 0.0f, -1.0f},
                                                    // Face +Z: (x, y, 1) -> (+(2*x-1), -(2*y-1), +1)
                                                    {2.0f, 0.0f, -1.0f, 0.0f, -2.0f, 1.0f, 0.0f, 0.0f, 1.0f}};

    // Generate transformation matrices.

    de::Random rnd(randSeed);

    m_transformations.reserve(m_numUnits);
    m_lodDerivateParts.reserve(m_numUnits);

    DE_ASSERT((int)m_unitTypes.size() == m_numUnits);

    for (int unitNdx = 0; unitNdx < m_numUnits; unitNdx++)
    {
        if (m_unitTypes[unitNdx] == GL_TEXTURE_2D)
        {
            float rotAngle           = rnd.getFloat(0.0f, 2.0f * DE_PI);
            float xScaleFactor       = rnd.getFloat(0.7f, 1.5f);
            float yScaleFactor       = rnd.getFloat(0.7f, 1.5f);
            float xShearAmount       = rnd.getFloat(0.0f, 0.5f);
            float yShearAmount       = rnd.getFloat(0.0f, 0.5f);
            float xTranslationAmount = rnd.getFloat(-0.5f, 0.5f);
            float yTranslationAmount = rnd.getFloat(-0.5f, 0.5f);

            float tempOffsetData[3 * 3] = // For temporarily centering the coordinates to get nicer transformations.
                {1.0f, 0.0f, -0.5f, 0.0f, 1.0f, -0.5f, 0.0f, 0.0f, 1.0f};
            float rotTransfData[3 * 3]         = {deFloatCos(rotAngle),
                                                  -deFloatSin(rotAngle),
                                                  0.0f,
                                                  deFloatSin(rotAngle),
                                                  deFloatCos(rotAngle),
                                                  0.0f,
                                                  0.0f,
                                                  0.0f,
                                                  1.0f};
            float scaleTransfData[3 * 3]       = {xScaleFactor, 0.0f, 0.0f, 0.0f, yScaleFactor, 0.0f, 0.0f, 0.0f, 1.0f};
            float xShearTransfData[3 * 3]      = {1.0f, xShearAmount, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
            float yShearTransfData[3 * 3]      = {1.0f, 0.0f, 0.0f, yShearAmount, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
            float translationTransfData[3 * 3] = {1.0f, 0.0f, xTranslationAmount, 0.0f, 1.0f, yTranslationAmount, 0.0f,
                                                  0.0f, 1.0f};

            Mat3 transformation = Mat3(tempOffsetData) * Mat3(translationTransfData) * Mat3(rotTransfData) *
                                  Mat3(scaleTransfData) * Mat3(xShearTransfData) * Mat3(yShearTransfData) *
                                  (Mat3(tempOffsetData) * (-1.0f));

            // Calculate parts of lod derivates.
            m_lodDerivateParts.push_back(calculateLodDerivateParts(transformation));

            m_transformations.push_back(transformation);
        }
        else
        {
            DE_ASSERT(m_unitTypes[unitNdx] == GL_TEXTURE_CUBE_MAP);
            DE_STATIC_ASSERT((int)tcu::CUBEFACE_LAST == DE_LENGTH_OF_ARRAY(s_cubeTransforms));

            float planarTransData[3 * 3];

            // In case of a cube map, we only want to render one face, so the transformation needs to be restricted - only enlarging scaling is done.

            for (int i = 0; i < DE_LENGTH_OF_ARRAY(planarTransData); i++)
            {
                if (i == 0 || i == 4)
                    planarTransData[i] = rnd.getFloat(0.1f, 0.9f); // Two first diagonal cells control the scaling.
                else if (i == 8)
                    planarTransData[i] = 1.0f;
                else
                    planarTransData[i] = 0.0f;
            }

            int faceNdx = rnd.getInt(0, (int)tcu::CUBEFACE_LAST - 1);
            Mat3 planarTrans(planarTransData); // Planar, face-agnostic transformation.
            Mat3 finalTrans =
                Mat3(s_cubeTransforms[faceNdx]) *
                planarTrans; // Final transformation from planar to cube map coordinates, including the transformation just generated.

            // Calculate parts of lod derivates.
            m_lodDerivateParts.push_back(calculateLodDerivateParts(planarTrans));

            m_transformations.push_back(finalTrans);
        }
    }
}

void MultiTexShader::setUniforms(sglr::Context &ctx, uint32_t program) const
{
    ctx.useProgram(program);

    // Sampler and matrix uniforms.

    for (int ndx = 0; ndx < m_numUnits; ndx++)
    {
        string ndxStr = de::toString(ndx);

        ctx.uniform1i(ctx.getUniformLocation(program, ("u_sampler" + ndxStr).c_str()), ndx);
        ctx.uniformMatrix3fv(ctx.getUniformLocation(program, ("u_trans" + ndxStr).c_str()), 1, GL_FALSE,
                             (GLfloat *)&m_transformations[ndx].getColumnMajorData()[0]);
    }
}

void MultiTexShader::makeSafeLods(const vector<IVec2> &textureSizes, const IVec2 &viewportSize)
{
    DE_ASSERT((int)textureSizes.size() == m_numUnits);

    static const float shrinkScaleMatData[3 * 3] = {0.95f, 0.0f, 0.0f, 0.0f, 0.95f, 0.0f, 0.0f, 0.0f, 1.0f};
    Mat3 shrinkScaleMat(shrinkScaleMatData);

    Vec2 screenDerivate(1.0f / (float)viewportSize.x(), 1.0f / (float)viewportSize.y());

    for (int unitNdx = 0; unitNdx < m_numUnits; unitNdx++)
    {
        // As long as LOD is too close to 0.0 or is positive and too close to a something-and-a-half (0.5, 1.5, 2.5 etc) or allowed lod range could round to different levels, zoom in a little to get a safer LOD.
        for (;;)
        {
            const float threshold = 0.1f;
            const float epsilon   = 0.01f;

            const float lodMax = calculateLodMax(m_lodDerivateParts[unitNdx], textureSizes[unitNdx], screenDerivate);
            const float lodMin = calculateLodMin(m_lodDerivateParts[unitNdx], textureSizes[unitNdx], screenDerivate);

            const int32_t maxLevel =
                (lodMax + epsilon < 0.5f) ? (0) : (deCeilFloatToInt32(lodMax + epsilon + 0.5f) - 1);
            const int32_t minLevel =
                (lodMin - epsilon < 0.5f) ? (0) : (deCeilFloatToInt32(lodMin - epsilon + 0.5f) - 1);

            if (de::abs(lodMax) < threshold || (lodMax > 0.0f && de::abs(deFloatFrac(lodMax) - 0.5f) < threshold) ||
                de::abs(lodMin) < threshold || (lodMin > 0.0f && de::abs(deFloatFrac(lodMin) - 0.5f) < threshold) ||
                maxLevel != minLevel)
            {
                m_transformations[unitNdx]  = shrinkScaleMat * m_transformations[unitNdx];
                m_lodDerivateParts[unitNdx] = calculateLodDerivateParts(m_transformations[unitNdx]);
            }
            else
                break;
        }
    }
}

void MultiTexShader::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                   const int numPackets) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::VertexPacket &packet = *(packets[packetNdx]);

        packet.position   = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
        packet.outputs[0] = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx);
    }
}

void MultiTexShader::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                    const rr::FragmentShadingContext &context) const
{
    DE_ASSERT((int)m_unitTypes.size() == m_numUnits);
    DE_ASSERT((int)m_transformations.size() == m_numUnits);
    DE_ASSERT((int)m_lodDerivateParts.size() == m_numUnits);

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::FragmentPacket &packet  = packets[packetNdx];
        const float colorMultiplier = 1.0f / (float)m_numUnits;
        Vec4 outColors[4]           = {Vec4(0.0f), Vec4(0.0f), Vec4(0.0f), Vec4(0.0f)};

        for (int unitNdx = 0; unitNdx < m_numUnits; unitNdx++)
        {
            tcu::Vec4 texSamples[4];

            // Read tex coords
            const tcu::Vec2 texCoords[4] = {
                rr::readTriangleVarying<float>(packet, context, 0, 0).xy(),
                rr::readTriangleVarying<float>(packet, context, 0, 1).xy(),
                rr::readTriangleVarying<float>(packet, context, 0, 2).xy(),
                rr::readTriangleVarying<float>(packet, context, 0, 3).xy(),
            };

            if (m_unitTypes[unitNdx] == GL_TEXTURE_2D)
            {
                // Transform
                const tcu::Vec2 transformedTexCoords[4] = {
                    (m_transformations[unitNdx] * Vec3(texCoords[0].x(), texCoords[0].y(), 1.0f)).xy(),
                    (m_transformations[unitNdx] * Vec3(texCoords[1].x(), texCoords[1].y(), 1.0f)).xy(),
                    (m_transformations[unitNdx] * Vec3(texCoords[2].x(), texCoords[2].y(), 1.0f)).xy(),
                    (m_transformations[unitNdx] * Vec3(texCoords[3].x(), texCoords[3].y(), 1.0f)).xy(),
                };

                // Sample
                m_uniforms[2 * unitNdx].sampler.tex2D->sample4(texSamples, transformedTexCoords);
            }
            else
            {
                DE_ASSERT(m_unitTypes[unitNdx] == GL_TEXTURE_CUBE_MAP);

                // Transform
                const tcu::Vec3 transformedTexCoords[4] = {
                    m_transformations[unitNdx] * Vec3(texCoords[0].x(), texCoords[0].y(), 1.0f),
                    m_transformations[unitNdx] * Vec3(texCoords[1].x(), texCoords[1].y(), 1.0f),
                    m_transformations[unitNdx] * Vec3(texCoords[2].x(), texCoords[2].y(), 1.0f),
                    m_transformations[unitNdx] * Vec3(texCoords[3].x(), texCoords[3].y(), 1.0f),
                };

                // Sample
                m_uniforms[2 * unitNdx].sampler.texCube->sample4(texSamples, transformedTexCoords);
            }

            // Add to sum
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                outColors[fragNdx] += colorMultiplier * texSamples[fragNdx];
        }

        // output
        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, outColors[fragNdx]);
    }
}

class TextureUnitCase : public TestCase
{
public:
    enum CaseType
    {
        CASE_ONLY_2D = 0,
        CASE_ONLY_CUBE,
        CASE_MIXED,

        CASE_LAST
    };
    TextureUnitCase(Context &context, const char *name, const char *desc,
                    int numUnits /* \note If non-positive, use all units */, CaseType caseType, uint32_t randSeed);
    ~TextureUnitCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    struct TextureParameters
    {
        GLenum format;
        GLenum dataType;
        GLenum wrapModeS;
        GLenum wrapModeT;
        GLenum minFilter;
        GLenum magFilter;
    };

    TextureUnitCase(const TextureUnitCase &other);
    TextureUnitCase &operator=(const TextureUnitCase &other);

    void render(sglr::Context &context);

    const int m_numUnitsParam;
    const CaseType m_caseType;
    const uint32_t m_randSeed;

    int m_numTextures; //!< \note Needed in addition to m_numUnits since same texture may be bound to many texture units.
    int m_numUnits;    //!< = m_numUnitsParam > 0 ? m_numUnitsParam : implementationDefinedMaximum

    vector<GLenum> m_textureTypes;
    vector<TextureParameters> m_textureParams;
    vector<tcu::Texture2D *> m_textures2d;
    vector<tcu::TextureCube *> m_texturesCube;
    vector<int> m_unitTextures; //!< Which texture is used in a particular unit.
    vector<int>
        m_ndx2dOrCube; //!< Index of a texture in either m_textures2d or m_texturesCube, depending on texture type.
    MultiTexShader *m_shader;
};

TextureUnitCase::TextureUnitCase(Context &context, const char *name, const char *desc, int numUnits, CaseType caseType,
                                 uint32_t randSeed)
    : TestCase(context, tcu::NODETYPE_SELF_VALIDATE, name, desc)
    , m_numUnitsParam(numUnits)
    , m_caseType(caseType)
    , m_randSeed(randSeed)
    , m_numTextures(0)
    , m_numUnits(0)
    , m_shader(nullptr)
{
}

TextureUnitCase::~TextureUnitCase(void)
{
    TextureUnitCase::deinit();
}

void TextureUnitCase::deinit(void)
{
    for (vector<tcu::Texture2D *>::iterator i = m_textures2d.begin(); i != m_textures2d.end(); i++)
        delete *i;
    m_textures2d.clear();

    for (vector<tcu::TextureCube *>::iterator i = m_texturesCube.begin(); i != m_texturesCube.end(); i++)
        delete *i;
    m_texturesCube.clear();

    delete m_shader;
    m_shader = nullptr;
}

void TextureUnitCase::init(void)
{
    m_numUnits = m_numUnitsParam > 0 ? m_numUnitsParam : m_context.getContextInfo().getInt(GL_MAX_TEXTURE_IMAGE_UNITS);

    // Make the textures.

    try
    {
        tcu::TestLog &log = m_testCtx.getLog();
        de::Random rnd(m_randSeed);

        if (rnd.getFloat() < 0.7f)
            m_numTextures = m_numUnits; // In most cases use one unit per texture.
        else
            m_numTextures =
                rnd.getInt(deMax32(1, m_numUnits - 2), m_numUnits); // Sometimes assign same texture to multiple units.

        log << tcu::TestLog::Message
            << ("Using " + de::toString(m_numUnits) + " texture unit(s) and " + de::toString(m_numTextures) +
                " texture(s)")
                   .c_str()
            << tcu::TestLog::EndMessage;

        m_textureTypes.reserve(m_numTextures);
        m_textureParams.reserve(m_numTextures);
        m_ndx2dOrCube.reserve(m_numTextures);

        // Generate textures.

        for (int texNdx = 0; texNdx < m_numTextures; texNdx++)
        {
            // Either fixed or randomized target types (2d or cube), and randomized parameters for every texture.

            TextureParameters params;
            bool is2d = m_caseType == CASE_ONLY_2D ? true : m_caseType == CASE_ONLY_CUBE ? false : rnd.getBool();

            GLenum type         = is2d ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP;
            const int texWidth  = is2d ? TEXTURE_WIDTH_2D : TEXTURE_WIDTH_CUBE;
            const int texHeight = is2d ? TEXTURE_HEIGHT_2D : TEXTURE_HEIGHT_CUBE;
            bool mipmaps        = (deIsPowerOfTwo32(texWidth) && deIsPowerOfTwo32(texHeight));
            int numLevels       = mipmaps ? deLog2Floor32(de::max(texWidth, texHeight)) + 1 : 1;

            params.wrapModeS = s_testWrapModes[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testWrapModes) - 1)];
            params.wrapModeT = s_testWrapModes[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testWrapModes) - 1)];
            params.magFilter = s_testMagFilters[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testMagFilters) - 1)];
            params.dataType  = s_testDataTypes[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testDataTypes) - 1)];

            // Certain minification filters are only used when using mipmaps.
            if (mipmaps)
                params.minFilter = s_testMinFilters[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testMinFilters) - 1)];
            else
                params.minFilter =
                    s_testNonMipmapMinFilters[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testNonMipmapMinFilters) - 1)];

            // Format may depend on data type.
            if (params.dataType == GL_UNSIGNED_SHORT_5_6_5)
                params.format = GL_RGB;
            else if (params.dataType == GL_UNSIGNED_SHORT_4_4_4_4 || params.dataType == GL_UNSIGNED_SHORT_5_5_5_1)
                params.format = GL_RGBA;
            else
                params.format = s_testFormats[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testFormats) - 1)];

            m_textureTypes.push_back(type);
            m_textureParams.push_back(params);

            // Create new texture.

            if (is2d)
            {
                m_ndx2dOrCube.push_back(
                    (int)m_textures2d.size()); // Remember the index this texture has in the 2d array.
                m_textures2d.push_back(new tcu::Texture2D(glu::mapGLTransferFormat(params.format, params.dataType),
                                                          texWidth, texHeight,
                                                          isES2Context(m_context.getRenderContext().getType())));
            }
            else
            {
                m_ndx2dOrCube.push_back(
                    (int)m_texturesCube.size()); // Remember the index this texture has in the cube array.
                DE_ASSERT(texWidth == texHeight);
                m_texturesCube.push_back(
                    new tcu::TextureCube(glu::mapGLTransferFormat(params.format, params.dataType), texWidth));
            }

            tcu::TextureFormatInfo fmtInfo =
                tcu::getTextureFormatInfo(is2d ? m_textures2d.back()->getFormat() : m_texturesCube.back()->getFormat());
            Vec4 cBias  = fmtInfo.valueMin;
            Vec4 cScale = fmtInfo.valueMax - fmtInfo.valueMin;

            // Fill with grid texture.

            int numFaces = is2d ? 1 : (int)tcu::CUBEFACE_LAST;

            for (int face = 0; face < numFaces; face++)
            {
                uint32_t rgb    = rnd.getUint32() & 0x00ffffff;
                uint32_t alpha0 = 0xff000000;
                uint32_t alpha1 = 0xff000000;

                if (params.format == GL_ALPHA) // \note This needs alpha to be visible.
                {
                    alpha0 &= rnd.getUint32();
                    alpha1 = ~alpha0;
                }

                uint32_t colorA = alpha0 | rgb;
                uint32_t colorB = alpha1 | ~rgb;

                for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
                {
                    if (is2d)
                        m_textures2d.back()->allocLevel(levelNdx);
                    else
                        m_texturesCube.back()->allocLevel((tcu::CubeFace)face, levelNdx);

                    int curCellSize = deMax32(1, GRID_CELL_SIZE >> levelNdx); // \note Scale grid cell size for mipmaps.

                    tcu::PixelBufferAccess access =
                        is2d ? m_textures2d.back()->getLevel(levelNdx) :
                               m_texturesCube.back()->getLevelFace(levelNdx, (tcu::CubeFace)face);
                    tcu::fillWithGrid(access, curCellSize, tcu::RGBA(colorA).toVec() * cScale + cBias,
                                      tcu::RGBA(colorB).toVec() * cScale + cBias);
                }
            }
        }

        // Assign a texture index to each unit.

        m_unitTextures.reserve(m_numUnits);

        // \note Every texture is used at least once.
        for (int i = 0; i < m_numTextures; i++)
            m_unitTextures.push_back(i);

        // Assign a random texture to remaining units.
        while ((int)m_unitTextures.size() < m_numUnits)
            m_unitTextures.push_back(rnd.getInt(0, m_numTextures - 1));

        rnd.shuffle(m_unitTextures.begin(), m_unitTextures.end());

        // Create shader.

        vector<GLenum> unitTypes;
        unitTypes.reserve(m_numUnits);
        for (int i = 0; i < m_numUnits; i++)
            unitTypes.push_back(m_textureTypes[m_unitTextures[i]]);

        DE_ASSERT(m_shader == nullptr);
        m_shader = new MultiTexShader(rnd.getUint32(), m_numUnits, unitTypes);
    }
    catch (const std::exception &)
    {
        // Clean up to save memory.
        TextureUnitCase::deinit();
        throw;
    }
}

TextureUnitCase::IterateResult TextureUnitCase::iterate(void)
{
    glu::RenderContext &renderCtx         = m_context.getRenderContext();
    const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();
    tcu::TestLog &log                     = m_testCtx.getLog();
    de::Random rnd(m_randSeed);

    int viewportWidth  = deMin32(VIEWPORT_WIDTH, renderTarget.getWidth());
    int viewportHeight = deMin32(VIEWPORT_HEIGHT, renderTarget.getHeight());
    int viewportX      = rnd.getInt(0, renderTarget.getWidth() - viewportWidth);
    int viewportY      = rnd.getInt(0, renderTarget.getHeight() - viewportHeight);

    tcu::Surface gles2Frame(viewportWidth, viewportHeight);
    tcu::Surface refFrame(viewportWidth, viewportHeight);

    {
        // First we do some tricks to make the LODs safer wrt. precision issues. See MultiTexShader::makeSafeLods().

        vector<IVec2> texSizes;
        texSizes.reserve(m_numUnits);

        for (int i = 0; i < m_numUnits; i++)
        {
            int texNdx       = m_unitTextures[i];
            int texNdxInType = m_ndx2dOrCube[texNdx];
            GLenum type      = m_textureTypes[texNdx];

            switch (type)
            {
            case GL_TEXTURE_2D:
                texSizes.push_back(
                    IVec2(m_textures2d[texNdxInType]->getWidth(), m_textures2d[texNdxInType]->getHeight()));
                break;
            case GL_TEXTURE_CUBE_MAP:
                texSizes.push_back(
                    IVec2(m_texturesCube[texNdxInType]->getSize(), m_texturesCube[texNdxInType]->getSize()));
                break;
            default:
                DE_ASSERT(false);
            }
        }

        m_shader->makeSafeLods(texSizes, IVec2(viewportWidth, viewportHeight));
    }

    // Render using GLES2.
    {
        sglr::GLContext context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS | sglr::GLCONTEXT_LOG_PROGRAMS,
                                tcu::IVec4(viewportX, viewportY, viewportWidth, viewportHeight));

        render(context);

        context.readPixels(gles2Frame, 0, 0, viewportWidth, viewportHeight);
    }

    // Render reference image.
    {
        sglr::ReferenceContextBuffers buffers(
            tcu::PixelFormat(8, 8, 8, renderTarget.getPixelFormat().alphaBits ? 8 : 0), 0 /* depth */, 0 /* stencil */,
            viewportWidth, viewportHeight);
        sglr::ReferenceContext context(sglr::ReferenceContextLimits(renderCtx), buffers.getColorbuffer(),
                                       buffers.getDepthbuffer(), buffers.getStencilbuffer());

        render(context);

        context.readPixels(refFrame, 0, 0, viewportWidth, viewportHeight);
    }

    // Compare images.
    const float threshold = 0.001f;
    bool isOk = tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refFrame, gles2Frame, threshold,
                                  tcu::COMPARE_LOG_RESULT);

    // Store test result.
    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");

    return STOP;
}

void TextureUnitCase::render(sglr::Context &context)
{
    // Setup textures.

    vector<uint32_t> textureGLNames;
    vector<bool> isTextureSetUp(
        m_numTextures,
        false); // \note Same texture may be bound to multiple units, but we only want to set up parameters and data once per texture.

    textureGLNames.resize(m_numTextures);
    context.genTextures(m_numTextures, &textureGLNames[0]);

    for (int unitNdx = 0; unitNdx < m_numUnits; unitNdx++)
    {
        int texNdx = m_unitTextures[unitNdx];

        // Bind texture to unit.
        context.activeTexture(GL_TEXTURE0 + unitNdx);
        context.bindTexture(m_textureTypes[texNdx], textureGLNames[texNdx]);

        if (!isTextureSetUp[texNdx])
        {
            // Binding this texture for first time, so set parameters and data.

            context.texParameteri(m_textureTypes[texNdx], GL_TEXTURE_WRAP_S, m_textureParams[texNdx].wrapModeS);
            context.texParameteri(m_textureTypes[texNdx], GL_TEXTURE_WRAP_T, m_textureParams[texNdx].wrapModeT);
            context.texParameteri(m_textureTypes[texNdx], GL_TEXTURE_MIN_FILTER, m_textureParams[texNdx].minFilter);
            context.texParameteri(m_textureTypes[texNdx], GL_TEXTURE_MAG_FILTER, m_textureParams[texNdx].magFilter);

            if (m_textureTypes[texNdx] == GL_TEXTURE_2D)
            {
                int ndx2d                     = m_ndx2dOrCube[texNdx];
                const tcu::Texture2D *texture = m_textures2d[ndx2d];
                bool mipmaps  = (deIsPowerOfTwo32(texture->getWidth()) && deIsPowerOfTwo32(texture->getHeight()));
                int numLevels = mipmaps ? deLog2Floor32(de::max(texture->getWidth(), texture->getHeight())) + 1 : 1;

                context.pixelStorei(GL_UNPACK_ALIGNMENT, 1);

                for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
                {
                    tcu::ConstPixelBufferAccess access = texture->getLevel(levelNdx);
                    int width                          = access.getWidth();
                    int height                         = access.getHeight();

                    DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * width);

                    context.texImage2D(GL_TEXTURE_2D, levelNdx, m_textureParams[texNdx].format, width, height, 0,
                                       m_textureParams[texNdx].format, m_textureParams[texNdx].dataType,
                                       access.getDataPtr());
                }
            }
            else
            {
                DE_ASSERT(m_textureTypes[texNdx] == GL_TEXTURE_CUBE_MAP);

                int ndxCube                     = m_ndx2dOrCube[texNdx];
                const tcu::TextureCube *texture = m_texturesCube[ndxCube];
                bool mipmaps                    = deIsPowerOfTwo32(texture->getSize()) != false;
                int numLevels                   = mipmaps ? deLog2Floor32(texture->getSize()) + 1 : 1;

                context.pixelStorei(GL_UNPACK_ALIGNMENT, 1);

                for (int face = 0; face < (int)tcu::CUBEFACE_LAST; face++)
                {
                    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
                    {
                        tcu::ConstPixelBufferAccess access = texture->getLevelFace(levelNdx, (tcu::CubeFace)face);
                        int width                          = access.getWidth();
                        int height                         = access.getHeight();

                        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * width);

                        context.texImage2D(s_cubeFaceTargets[face], levelNdx, m_textureParams[texNdx].format, width,
                                           height, 0, m_textureParams[texNdx].format, m_textureParams[texNdx].dataType,
                                           access.getDataPtr());
                    }
                }
            }

            isTextureSetUp[texNdx] = true; // Don't set up this texture's parameters and data again later.
        }
    }

    GLU_EXPECT_NO_ERROR(context.getError(), "Set textures");

    // Setup shader

    uint32_t shaderID = context.createProgram(m_shader);

    // Draw.

    context.clearColor(0.125f, 0.25f, 0.5f, 1.0f);
    context.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    m_shader->setUniforms(context, shaderID);
    sglr::drawQuad(context, shaderID, Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
    GLU_EXPECT_NO_ERROR(context.getError(), "Draw");

    // Delete previously generated texture names.

    context.deleteTextures(m_numTextures, &textureGLNames[0]);
    GLU_EXPECT_NO_ERROR(context.getError(), "Delete textures");
}

TextureUnitTests::TextureUnitTests(Context &context) : TestCaseGroup(context, "units", "Texture Unit Usage Tests")
{
}

TextureUnitTests::~TextureUnitTests(void)
{
}

void TextureUnitTests::init(void)
{
    const int numTestsPerGroup = 10;

    static const int unitCounts[] = {
        2, 4, 8,
        -1 // \note Negative stands for the implementation-specified maximum.
    };

    for (int unitCountNdx = 0; unitCountNdx < DE_LENGTH_OF_ARRAY(unitCounts); unitCountNdx++)
    {
        int numUnits = unitCounts[unitCountNdx];

        string countGroupName = (unitCounts[unitCountNdx] < 0 ? "all" : de::toString(numUnits)) + "_units";

        tcu::TestCaseGroup *countGroup = new tcu::TestCaseGroup(m_testCtx, countGroupName.c_str(), "");
        addChild(countGroup);

        DE_STATIC_ASSERT((int)TextureUnitCase::CASE_ONLY_2D == 0);

        for (int caseType = (int)TextureUnitCase::CASE_ONLY_2D; caseType < (int)TextureUnitCase::CASE_LAST; caseType++)
        {
            const char *caseTypeGroupName =
                (TextureUnitCase::CaseType)caseType == TextureUnitCase::CASE_ONLY_2D   ? "only_2d" :
                (TextureUnitCase::CaseType)caseType == TextureUnitCase::CASE_ONLY_CUBE ? "only_cube" :
                (TextureUnitCase::CaseType)caseType == TextureUnitCase::CASE_MIXED     ? "mixed" :
                                                                                         nullptr;
            DE_ASSERT(caseTypeGroupName != nullptr);

            tcu::TestCaseGroup *caseTypeGroup = new tcu::TestCaseGroup(m_testCtx, caseTypeGroupName, "");
            countGroup->addChild(caseTypeGroup);

            for (int testNdx = 0; testNdx < numTestsPerGroup; testNdx++)
                caseTypeGroup->addChild(new TextureUnitCase(m_context, de::toString(testNdx).c_str(), "", numUnits,
                                                            (TextureUnitCase::CaseType)caseType,
                                                            (uint32_t)deInt32Hash(testNdx)));
        }
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
