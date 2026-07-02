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
 * \brief Texture unit usage tests.
 *
 * \todo [2012-07-12 nuutti] Come up with a good way to make these tests faster.
 *//*--------------------------------------------------------------------*/

#include "es3fTextureUnitTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluContextInfo.hpp"
#include "gluTextureUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuMatrix.hpp"
#include "tcuRenderTarget.hpp"
#include "sglrContextUtil.hpp"
#include "sglrReferenceContext.hpp"
#include "sglrGLContext.hpp"
#include "deMath.h"
#include "deRandom.hpp"
#include "deStringUtil.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

using std::string;
using std::vector;
using tcu::IVec2;
using tcu::IVec3;
using tcu::Mat3;
using tcu::Mat4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using namespace glw; // GL types

namespace deqp
{

using namespace gls::TextureTestUtil;

namespace gles3
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

static const int TEXTURE_WIDTH_2D_ARRAY  = 64;
static const int TEXTURE_HEIGHT_2D_ARRAY = 64;
static const int TEXTURE_LAYERS_2D_ARRAY = 4;

static const int TEXTURE_WIDTH_3D  = 32;
static const int TEXTURE_HEIGHT_3D = 32;
static const int TEXTURE_DEPTH_3D  = 32;

static const int GRID_CELL_SIZE = 8;

static const GLenum s_testSizedInternalFormats[] = {
    GL_RGBA32F,    GL_RGBA32I,        GL_RGBA32UI, GL_RGBA16F,    GL_RGBA16I, GL_RGBA16UI, GL_RGBA8,       GL_RGBA8I,
    GL_RGBA8UI,    GL_SRGB8_ALPHA8,   GL_RGB10_A2, GL_RGB10_A2UI, GL_RGBA4,   GL_RGB5_A1,  GL_RGBA8_SNORM, GL_RGB8,
    GL_RGB565,     GL_R11F_G11F_B10F, GL_RGB32F,   GL_RGB32I,     GL_RGB32UI, GL_RGB16F,   GL_RGB16I,      GL_RGB16UI,
    GL_RGB8_SNORM, GL_RGB8I,          GL_RGB8UI,   GL_SRGB8,      GL_RGB9_E5, GL_RG32F,    GL_RG32I,       GL_RG32UI,
    GL_RG16F,      GL_RG16I,          GL_RG16UI,   GL_RG8,        GL_RG8I,    GL_RG8UI,    GL_RG8_SNORM,   GL_R32F,
    GL_R32I,       GL_R32UI,          GL_R16F,     GL_R16I,       GL_R16UI,   GL_R8,       GL_R8I,         GL_R8UI,
    GL_R8_SNORM};

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

static const GLenum s_testNearestMinFilters[] = {GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST};

static const GLenum s_testMagFilters[] = {GL_NEAREST, GL_LINEAR};

static const GLenum s_cubeFaceTargets[] = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

// Extend a 3x3 transformation matrix to an equivalent 4x4 transformation matrix (i.e. 1.0 in right-down cell, 0.0's in other new cells).
static Mat4 matExtend3To4(const Mat3 &mat)
{
    Mat4 res;
    for (int rowNdx = 0; rowNdx < 3; rowNdx++)
    {
        Vec3 row = mat.getRow(rowNdx);
        res.setRow(rowNdx, Vec4(row.x(), row.y(), row.z(), 0.0f));
    }
    res.setRow(3, Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    return res;
}

static string generateMultiTexFragmentShader(int numUnits, const vector<GLenum> &unitTypes,
                                             const vector<glu::DataType> &samplerTypes)
{
    // The fragment shader calculates the average of a set of textures.

    string samplersStr;
    string matricesStr;
    string scalesStr;
    string biasesStr;
    string lookupsStr;

    string colorMultiplier = "(1.0/" + de::toString(numUnits) + ".0)";

    for (int ndx = 0; ndx < numUnits; ndx++)
    {
        string ndxStr             = de::toString(ndx);
        string samplerName        = "u_sampler" + ndxStr;
        string transformationName = "u_trans" + ndxStr;
        string scaleName          = "u_texScale" + ndxStr;
        string biasName           = "u_texBias" + ndxStr;

        samplersStr +=
            string("") + "uniform highp " + glu::getDataTypeName(samplerTypes[ndx]) + " " + samplerName + ";\n";
        matricesStr += "uniform highp mat4 " + transformationName + ";\n";
        scalesStr += "uniform highp vec4 " + scaleName + ";\n";
        biasesStr += "uniform highp vec4 " + biasName + ";\n";

        string lookupCoord = transformationName + "*vec4(v_coord, 1.0, 1.0)";

        if (unitTypes[ndx] == GL_TEXTURE_2D)
            lookupCoord = "vec2(" + lookupCoord + ")";
        else
            lookupCoord = "vec3(" + lookupCoord + ")";

        lookupsStr += "\tcolor += " + colorMultiplier + "*(vec4(texture(" + samplerName + ", " + lookupCoord + "))*" +
                      scaleName + " + " + biasName + ");\n";
    }

    return "#version 300 es\n"
           "layout(location = 0) out mediump vec4 o_color;\n" +
           samplersStr + matricesStr + scalesStr + biasesStr +
           "in highp vec2 v_coord;\n"
           "\n"
           "void main (void)\n"
           "{\n"
           "    mediump vec4 color = vec4(0.0);\n" +
           lookupsStr +
           "    o_color = color;\n"
           "}\n";
}

static sglr::pdec::ShaderProgramDeclaration generateShaderProgramDeclaration(int numUnits,
                                                                             const vector<GLenum> &unitTypes,
                                                                             const vector<glu::DataType> &samplerTypes)
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
        string scaleName          = "u_texScale" + de::toString(ndx);
        string biasName           = "u_texBias" + de::toString(ndx);

        decl << sglr::pdec::Uniform(samplerName, samplerTypes[ndx]);
        decl << sglr::pdec::Uniform(transformationName, glu::TYPE_FLOAT_MAT4);
        decl << sglr::pdec::Uniform(scaleName, glu::TYPE_FLOAT_VEC4);
        decl << sglr::pdec::Uniform(biasName, glu::TYPE_FLOAT_VEC4);
    }

    decl << sglr::pdec::VertexSource("#version 300 es\n"
                                     "in highp vec4 a_position;\n"
                                     "in highp vec2 a_coord;\n"
                                     "out highp vec2 v_coord;\n"
                                     "\n"
                                     "void main (void)\n"
                                     "{\n"
                                     "    gl_Position = a_position;\n"
                                     "    v_coord = a_coord;\n"
                                     "}\n");
    decl << sglr::pdec::FragmentSource(generateMultiTexFragmentShader(numUnits, unitTypes, samplerTypes));

    return decl;
}

// Calculates values that will be used in calculateLod().
static tcu::Vector<tcu::Vec2, 3> calculateLodDerivateParts(const Mat4 &transformation)
{
    // Calculate transformed coordinates of three screen corners.
    Vec3 trans00 = (transformation * Vec4(0.0f, 0.0f, 1.0f, 1.0f)).xyz();
    Vec3 trans01 = (transformation * Vec4(0.0f, 1.0f, 1.0f, 1.0f)).xyz();
    Vec3 trans10 = (transformation * Vec4(1.0f, 0.0f, 1.0f, 1.0f)).xyz();

    return tcu::Vector<tcu::Vec2, 3>(Vec2(trans10.x() - trans00.x(), trans01.x() - trans00.x()),
                                     Vec2(trans10.y() - trans00.y(), trans01.y() - trans00.y()),
                                     Vec2(trans10.z() - trans00.z(), trans01.z() - trans00.z()));
}

// Calculates the maximum allowed lod from derivates
static float calculateLodMax(const tcu::Vector<tcu::Vec2, 3> &derivateParts, const tcu::IVec3 &textureSize,
                             const Vec2 &screenDerivate)
{
    float dudx = derivateParts[0].x() * (float)textureSize.x() * screenDerivate.x();
    float dudy = derivateParts[0].y() * (float)textureSize.x() * screenDerivate.y();
    float dvdx = derivateParts[1].x() * (float)textureSize.y() * screenDerivate.x();
    float dvdy = derivateParts[1].y() * (float)textureSize.y() * screenDerivate.y();
    float dwdx = derivateParts[2].x() * (float)textureSize.z() * screenDerivate.x();
    float dwdy = derivateParts[2].y() * (float)textureSize.z() * screenDerivate.y();

    const float mu = de::max(de::abs(dudx), de::abs(dudy));
    const float mv = de::max(de::abs(dvdx), de::abs(dvdy));
    const float mw = de::max(de::abs(dwdx), de::abs(dwdy));
    return deFloatLog2(mu + mv + mw);
}

// Calculates the minimum allowed lod from derivates
static float calculateLodMin(const tcu::Vector<tcu::Vec2, 3> &derivateParts, const tcu::IVec3 &textureSize,
                             const Vec2 &screenDerivate)
{
    float dudx = derivateParts[0].x() * (float)textureSize.x() * screenDerivate.x();
    float dudy = derivateParts[0].y() * (float)textureSize.x() * screenDerivate.y();
    float dvdx = derivateParts[1].x() * (float)textureSize.y() * screenDerivate.x();
    float dvdy = derivateParts[1].y() * (float)textureSize.y() * screenDerivate.y();
    float dwdx = derivateParts[2].x() * (float)textureSize.z() * screenDerivate.x();
    float dwdy = derivateParts[2].y() * (float)textureSize.z() * screenDerivate.y();

    const float mu = de::max(de::abs(dudx), de::abs(dudy));
    const float mv = de::max(de::abs(dvdx), de::abs(dvdy));
    const float mw = de::max(de::abs(dwdx), de::abs(dwdy));
    return deFloatLog2(de::max(mu, de::max(mv, mw)));
}

class MultiTexShader : public sglr::ShaderProgram
{
public:
    MultiTexShader(uint32_t randSeed, int numUnits, const vector<GLenum> &unitTypes,
                   const vector<glu::DataType> &samplerTypes, const vector<Vec4> &texScales,
                   const vector<Vec4> &texBiases,
                   const vector<int> &
                       num2dArrayLayers); // \note 2d array layer "coordinate" isn't normalized, so this is needed here.

    void setUniforms(sglr::Context &context, uint32_t program) const;
    void makeSafeLods(
        const vector<IVec3> &textureSizes,
        const IVec2 &viewportSize); // Modifies texture coordinates so that LODs aren't too close to x.5 or 0.0 .

private:
    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const;
    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const;

    int m_numUnits;
    vector<GLenum> m_unitTypes; // 2d, cube map, 2d array or 3d.
    vector<Vec4> m_texScales;
    vector<Vec4> m_texBiases;
    vector<Mat4> m_transformations;
    vector<tcu::Vector<tcu::Vec2, 3>> m_lodDerivateParts; // Parts of lod derivates; computed in init(), used in eval().
};

MultiTexShader::MultiTexShader(uint32_t randSeed, int numUnits, const vector<GLenum> &unitTypes,
                               const vector<glu::DataType> &samplerTypes, const vector<Vec4> &texScales,
                               const vector<Vec4> &texBiases, const vector<int> &num2dArrayLayers)
    : sglr::ShaderProgram(generateShaderProgramDeclaration(numUnits, unitTypes, samplerTypes))
    , m_numUnits(numUnits)
    , m_unitTypes(unitTypes)
    , m_texScales(texScales)
    , m_texBiases(texBiases)
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

    int tex2dArrayNdx = 0; // Keep track of 2d texture array index.

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

            static const float
                tempOffsetData[3 * 3] = // For temporarily centering the coordinates to get nicer transformations.
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

            Mat4 transformation = matExtend3To4(Mat3(tempOffsetData) * Mat3(translationTransfData) *
                                                Mat3(rotTransfData) * Mat3(scaleTransfData) * Mat3(xShearTransfData) *
                                                Mat3(yShearTransfData) * (Mat3(tempOffsetData) * (-1.0f)));

            m_lodDerivateParts.push_back(calculateLodDerivateParts(transformation));
            m_transformations.push_back(transformation);
        }
        else if (m_unitTypes[unitNdx] == GL_TEXTURE_CUBE_MAP)
        {
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
            Mat4 finalTrans = matExtend3To4(
                Mat3(s_cubeTransforms[faceNdx]) *
                planarTrans); // Final transformation from planar to cube map coordinates, including the transformation just generated.
            Mat4 planarTrans4x4 = matExtend3To4(planarTrans);

            m_lodDerivateParts.push_back(calculateLodDerivateParts(planarTrans4x4));
            m_transformations.push_back(finalTrans);
        }
        else
        {
            DE_ASSERT(m_unitTypes[unitNdx] == GL_TEXTURE_3D || m_unitTypes[unitNdx] == GL_TEXTURE_2D_ARRAY);

            float transData[4 * 4];

            for (int i = 0; i < 4 * 4; i++)
            {
                float sign   = rnd.getBool() ? 1.0f : -1.0f;
                transData[i] = rnd.getFloat(0.7f, 1.4f) * sign;
            }

            Mat4 transformation(transData);

            if (m_unitTypes[unitNdx] == GL_TEXTURE_2D_ARRAY)
            {
                // Z direction: Translate by 0.5 and scale by layer amount.

                float numLayers = (float)num2dArrayLayers[tex2dArrayNdx];

                static const float zTranslationTransfData[4 * 4] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                                                    0.0f, 0.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f};

                float zScaleTransfData[4 * 4] = {1.0f, 0.0f, 0.0f,      0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                                 0.0f, 0.0f, numLayers, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

                transformation = transformation * Mat4(zScaleTransfData) * Mat4(zTranslationTransfData);

                tex2dArrayNdx++;
            }

            m_lodDerivateParts.push_back(calculateLodDerivateParts(transformation));
            m_transformations.push_back(Mat4(transformation));
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
        ctx.uniformMatrix4fv(ctx.getUniformLocation(program, ("u_trans" + ndxStr).c_str()), 1, GL_FALSE,
                             (GLfloat *)&m_transformations[ndx].getColumnMajorData()[0]);
        ctx.uniform4fv(ctx.getUniformLocation(program, ("u_texScale" + ndxStr).c_str()), 1, m_texScales[ndx].getPtr());
        ctx.uniform4fv(ctx.getUniformLocation(program, ("u_texBias" + ndxStr).c_str()), 1, m_texBiases[ndx].getPtr());
    }
}

void MultiTexShader::makeSafeLods(const vector<IVec3> &textureSizes, const IVec2 &viewportSize)
{
    DE_ASSERT((int)textureSizes.size() == m_numUnits);

    static const float shrinkScaleMat2dData[3 * 3] = {0.95f, 0.0f, 0.0f, 0.0f, 0.95f, 0.0f, 0.0f, 0.0f, 1.0f};
    static const float shrinkScaleMat3dData[3 * 3] = {0.95f, 0.0f, 0.0f, 0.0f, 0.95f, 0.0f, 0.0f, 0.0f, 0.95f};
    Mat4 shrinkScaleMat2d                          = matExtend3To4(Mat3(shrinkScaleMat2dData));
    Mat4 shrinkScaleMat3d                          = matExtend3To4(Mat3(shrinkScaleMat3dData));

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
                m_transformations[unitNdx] =
                    (m_unitTypes[unitNdx] == GL_TEXTURE_3D ? shrinkScaleMat3d : shrinkScaleMat2d) *
                    m_transformations[unitNdx];
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

            // Transform
            tcu::Vec3 coords3D[4] = {
                (m_transformations[unitNdx] * Vec4(texCoords[0].x(), texCoords[0].y(), 1.0f, 1.0f)).xyz(),
                (m_transformations[unitNdx] * Vec4(texCoords[1].x(), texCoords[1].y(), 1.0f, 1.0f)).xyz(),
                (m_transformations[unitNdx] * Vec4(texCoords[2].x(), texCoords[2].y(), 1.0f, 1.0f)).xyz(),
                (m_transformations[unitNdx] * Vec4(texCoords[3].x(), texCoords[3].y(), 1.0f, 1.0f)).xyz(),
            };

            // To 2D
            const tcu::Vec2 coords2D[4] = {
                coords3D[0].xy(),
                coords3D[1].xy(),
                coords3D[2].xy(),
                coords3D[3].xy(),
            };

            // Sample
            switch (m_unitTypes[unitNdx])
            {
            case GL_TEXTURE_2D:
                m_uniforms[4 * unitNdx].sampler.tex2D->sample4(texSamples, coords2D);
                break;
            case GL_TEXTURE_CUBE_MAP:
                m_uniforms[4 * unitNdx].sampler.texCube->sample4(texSamples, coords3D);
                break;
            case GL_TEXTURE_2D_ARRAY:
                m_uniforms[4 * unitNdx].sampler.tex2DArray->sample4(texSamples, coords3D);
                break;
            case GL_TEXTURE_3D:
                m_uniforms[4 * unitNdx].sampler.tex3D->sample4(texSamples, coords3D);
                break;
            default:
                DE_ASSERT(false);
            }

            // Add to sum
            for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
                outColors[fragNdx] +=
                    colorMultiplier * (texSamples[fragNdx] * m_texScales[unitNdx] + m_texBiases[unitNdx]);
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
        CASE_ONLY_2D_ARRAY,
        CASE_ONLY_3D,
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
        GLenum internalFormat;
        GLenum wrapModeS;
        GLenum wrapModeT;
        GLenum wrapModeR;
        GLenum minFilter;
        GLenum magFilter;
    };

    TextureUnitCase(const TextureUnitCase &other);
    TextureUnitCase &operator=(const TextureUnitCase &other);

    void upload2dTexture(int texNdx, sglr::Context &context);
    void uploadCubeTexture(int texNdx, sglr::Context &context);
    void upload2dArrayTexture(int texNdx, sglr::Context &context);
    void upload3dTexture(int texNdx, sglr::Context &context);

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
    vector<tcu::Texture2DArray *> m_textures2dArray;
    vector<tcu::Texture3D *> m_textures3d;
    vector<int> m_unitTextures; //!< Which texture is used in a particular unit.
    vector<int>
        m_ndxTexType; //!< Index of a texture in m_textures2d, m_texturesCube, m_textures2dArray or m_textures3d, depending on texture type.
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

    for (vector<tcu::Texture2DArray *>::iterator i = m_textures2dArray.begin(); i != m_textures2dArray.end(); i++)
        delete *i;
    m_textures2dArray.clear();

    for (vector<tcu::Texture3D *>::iterator i = m_textures3d.begin(); i != m_textures3d.end(); i++)
        delete *i;
    m_textures3d.clear();

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
        m_ndxTexType.reserve(m_numTextures);

        // Generate textures.

        for (int texNdx = 0; texNdx < m_numTextures; texNdx++)
        {
            // Either fixed or randomized target types, and randomized parameters for every texture.

            TextureParameters params;

            DE_STATIC_ASSERT(CASE_ONLY_2D == 0 && CASE_MIXED + 1 == CASE_LAST);

            int texType       = m_caseType == CASE_MIXED ? rnd.getInt(0, (int)CASE_MIXED - 1) : (int)m_caseType;
            bool is2dTex      = texType == 0;
            bool isCubeTex    = texType == 1;
            bool is2dArrayTex = texType == 2;
            bool is3dTex      = texType == 3;

            DE_ASSERT(is2dTex || isCubeTex || is2dArrayTex || is3dTex);

            GLenum type         = is2dTex      ? GL_TEXTURE_2D :
                                  isCubeTex    ? GL_TEXTURE_CUBE_MAP :
                                  is2dArrayTex ? GL_TEXTURE_2D_ARRAY :
                                                 GL_TEXTURE_3D;
            const int texWidth  = is2dTex      ? TEXTURE_WIDTH_2D :
                                  isCubeTex    ? TEXTURE_WIDTH_CUBE :
                                  is2dArrayTex ? TEXTURE_WIDTH_2D_ARRAY :
                                                 TEXTURE_WIDTH_3D;
            const int texHeight = is2dTex      ? TEXTURE_HEIGHT_2D :
                                  isCubeTex    ? TEXTURE_HEIGHT_CUBE :
                                  is2dArrayTex ? TEXTURE_HEIGHT_2D_ARRAY :
                                                 TEXTURE_HEIGHT_3D;

            const int texDepth  = is3dTex ? TEXTURE_DEPTH_3D : 1;
            const int texLayers = is2dArrayTex ? TEXTURE_LAYERS_2D_ARRAY : 1;

            bool mipmaps  = (deIsPowerOfTwo32(texWidth) && deIsPowerOfTwo32(texHeight) && deIsPowerOfTwo32(texDepth));
            int numLevels = mipmaps ? deLog2Floor32(de::max(de::max(texWidth, texHeight), texDepth)) + 1 : 1;

            params.internalFormat =
                s_testSizedInternalFormats[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testSizedInternalFormats) - 1)];

            bool isFilterable = glu::isGLInternalColorFormatFilterable(params.internalFormat);

            params.wrapModeS = s_testWrapModes[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testWrapModes) - 1)];
            params.wrapModeT = s_testWrapModes[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testWrapModes) - 1)];
            params.wrapModeR = s_testWrapModes[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testWrapModes) - 1)];

            params.magFilter =
                isFilterable ? s_testMagFilters[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testMagFilters) - 1)] : GL_NEAREST;

            if (mipmaps)
                params.minFilter =
                    isFilterable ?
                        s_testMinFilters[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testMinFilters) - 1)] :
                        s_testNearestMinFilters[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testNearestMinFilters) - 1)];
            else
                params.minFilter =
                    isFilterable ?
                        s_testNonMipmapMinFilters[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_testNonMipmapMinFilters) - 1)] :
                        GL_NEAREST;

            m_textureTypes.push_back(type);
            m_textureParams.push_back(params);

            // Create new texture.

            tcu::TextureFormat texFormat = glu::mapGLInternalFormat((uint32_t)params.internalFormat);

            if (is2dTex)
            {
                m_ndxTexType.push_back(
                    (int)m_textures2d.size()); // Remember the index this texture has in the 2d texture vector.
                m_textures2d.push_back(new tcu::Texture2D(texFormat, texWidth, texHeight));
            }
            else if (isCubeTex)
            {
                m_ndxTexType.push_back(
                    (int)m_texturesCube.size()); // Remember the index this texture has in the cube texture vector.
                DE_ASSERT(texWidth == texHeight);
                m_texturesCube.push_back(new tcu::TextureCube(texFormat, texWidth));
            }
            else if (is2dArrayTex)
            {
                m_ndxTexType.push_back(
                    (int)m_textures2dArray
                        .size()); // Remember the index this texture has in the 2d array texture vector.
                m_textures2dArray.push_back(new tcu::Texture2DArray(texFormat, texWidth, texHeight, texLayers));
            }
            else
            {
                m_ndxTexType.push_back(
                    (int)m_textures3d.size()); // Remember the index this texture has in the 3d vector.
                m_textures3d.push_back(new tcu::Texture3D(texFormat, texWidth, texHeight, texDepth));
            }

            tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(texFormat);
            Vec4 cBias                     = fmtInfo.valueMin;
            Vec4 cScale                    = fmtInfo.valueMax - fmtInfo.valueMin;

            // Fill with grid texture.

            int numFaces = isCubeTex ? (int)tcu::CUBEFACE_LAST : 1;

            for (int face = 0; face < numFaces; face++)
            {
                uint32_t rgb   = rnd.getUint32() & 0x00ffffff;
                uint32_t alpha = 0xff000000;

                uint32_t colorA = alpha | rgb;
                uint32_t colorB = alpha | ((~rgb) & 0x00ffffff);

                for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
                {
                    if (is2dTex)
                        m_textures2d.back()->allocLevel(levelNdx);
                    else if (isCubeTex)
                        m_texturesCube.back()->allocLevel((tcu::CubeFace)face, levelNdx);
                    else if (is2dArrayTex)
                        m_textures2dArray.back()->allocLevel(levelNdx);
                    else
                        m_textures3d.back()->allocLevel(levelNdx);

                    int curCellSize = deMax32(1, GRID_CELL_SIZE >> levelNdx); // \note Scale grid cell size for mipmaps.

                    tcu::PixelBufferAccess access =
                        is2dTex      ? m_textures2d.back()->getLevel(levelNdx) :
                        isCubeTex    ? m_texturesCube.back()->getLevelFace(levelNdx, (tcu::CubeFace)face) :
                        is2dArrayTex ? m_textures2dArray.back()->getLevel(levelNdx) :
                                       m_textures3d.back()->getLevel(levelNdx);

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

        // Generate information for shader.

        vector<GLenum> unitTypes;
        vector<Vec4> texScales;
        vector<Vec4> texBiases;
        vector<glu::DataType> samplerTypes;
        vector<int> num2dArrayLayers;

        unitTypes.reserve(m_numUnits);
        texScales.reserve(m_numUnits);
        texBiases.reserve(m_numUnits);
        samplerTypes.reserve(m_numUnits);
        num2dArrayLayers.reserve(m_numUnits);

        for (int i = 0; i < m_numUnits; i++)
        {
            int texNdx                     = m_unitTextures[i];
            GLenum type                    = m_textureTypes[texNdx];
            tcu::TextureFormat fmt         = glu::mapGLInternalFormat(m_textureParams[texNdx].internalFormat);
            tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(fmt);

            unitTypes.push_back(type);

            if (type == GL_TEXTURE_2D_ARRAY)
                num2dArrayLayers.push_back(m_textures2dArray[m_ndxTexType[texNdx]]->getNumLayers());

            texScales.push_back(fmtInfo.lookupScale);
            texBiases.push_back(fmtInfo.lookupBias);

            switch (type)
            {
            case GL_TEXTURE_2D:
                samplerTypes.push_back(glu::getSampler2DType(fmt));
                break;
            case GL_TEXTURE_CUBE_MAP:
                samplerTypes.push_back(glu::getSamplerCubeType(fmt));
                break;
            case GL_TEXTURE_2D_ARRAY:
                samplerTypes.push_back(glu::getSampler2DArrayType(fmt));
                break;
            case GL_TEXTURE_3D:
                samplerTypes.push_back(glu::getSampler3DType(fmt));
                break;
            default:
                DE_ASSERT(false);
            }
        }

        // Create shader.

        DE_ASSERT(m_shader == nullptr);
        m_shader = new MultiTexShader(rnd.getUint32(), m_numUnits, unitTypes, samplerTypes, texScales, texBiases,
                                      num2dArrayLayers);
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

    tcu::Surface gles3Frame(viewportWidth, viewportHeight);
    tcu::Surface refFrame(viewportWidth, viewportHeight);

    {
        // First we do some tricks to make the LODs safer wrt. precision issues. See MultiTexShader::makeSafeLods().

        vector<IVec3> texSizes;
        texSizes.reserve(m_numUnits);

        for (int i = 0; i < m_numUnits; i++)
        {
            int texNdx       = m_unitTextures[i];
            int texNdxInType = m_ndxTexType[texNdx];
            GLenum type      = m_textureTypes[texNdx];

            switch (type)
            {
            case GL_TEXTURE_2D:
                texSizes.push_back(
                    IVec3(m_textures2d[texNdxInType]->getWidth(), m_textures2d[texNdxInType]->getHeight(), 0));
                break;
            case GL_TEXTURE_CUBE_MAP:
                texSizes.push_back(
                    IVec3(m_texturesCube[texNdxInType]->getSize(), m_texturesCube[texNdxInType]->getSize(), 0));
                break;
            case GL_TEXTURE_2D_ARRAY:
                texSizes.push_back(IVec3(m_textures2dArray[texNdxInType]->getWidth(),
                                         m_textures2dArray[texNdxInType]->getHeight(), 0));
                break;
            case GL_TEXTURE_3D:
                texSizes.push_back(IVec3(m_textures3d[texNdxInType]->getWidth(),
                                         m_textures3d[texNdxInType]->getHeight(),
                                         m_textures3d[texNdxInType]->getDepth()));
                break;
            default:
                DE_ASSERT(false);
            }
        }

        m_shader->makeSafeLods(texSizes, IVec2(viewportWidth, viewportHeight));
    }

    // Render using GLES3.
    {
        sglr::GLContext context(renderCtx, log, sglr::GLCONTEXT_LOG_CALLS | sglr::GLCONTEXT_LOG_PROGRAMS,
                                tcu::IVec4(viewportX, viewportY, viewportWidth, viewportHeight));

        render(context);

        context.readPixels(gles3Frame, 0, 0, viewportWidth, viewportHeight);
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
    bool isOk = tcu::fuzzyCompare(log, "ComparisonResult", "Image comparison result", refFrame, gles3Frame, threshold,
                                  tcu::COMPARE_LOG_RESULT);

    // Store test result.
    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");

    return STOP;
}

void TextureUnitCase::upload2dTexture(int texNdx, sglr::Context &context)
{
    int ndx2d                     = m_ndxTexType[texNdx];
    const tcu::Texture2D *texture = m_textures2d[ndx2d];
    glu::TransferFormat formatGl =
        glu::getTransferFormat(glu::mapGLInternalFormat(m_textureParams[texNdx].internalFormat));

    context.pixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int levelNdx = 0; levelNdx < texture->getNumLevels(); levelNdx++)
    {
        if (texture->isLevelEmpty(levelNdx))
            continue;

        tcu::ConstPixelBufferAccess access = texture->getLevel(levelNdx);
        int width                          = access.getWidth();
        int height                         = access.getHeight();

        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * width);

        context.texImage2D(GL_TEXTURE_2D, levelNdx, m_textureParams[texNdx].internalFormat, width, height,
                           0 /* border */, formatGl.format, formatGl.dataType, access.getDataPtr());
        GLU_EXPECT_NO_ERROR(context.getError(), "Set 2d texture image data");
    }
}

void TextureUnitCase::uploadCubeTexture(int texNdx, sglr::Context &context)
{
    int ndxCube                     = m_ndxTexType[texNdx];
    const tcu::TextureCube *texture = m_texturesCube[ndxCube];
    glu::TransferFormat formatGl =
        glu::getTransferFormat(glu::mapGLInternalFormat(m_textureParams[texNdx].internalFormat));

    context.pixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int face = 0; face < (int)tcu::CUBEFACE_LAST; face++)
    {
        for (int levelNdx = 0; levelNdx < texture->getNumLevels(); levelNdx++)
        {
            if (texture->isLevelEmpty((tcu::CubeFace)face, levelNdx))
                continue;

            tcu::ConstPixelBufferAccess access = texture->getLevelFace(levelNdx, (tcu::CubeFace)face);
            int width                          = access.getWidth();
            int height                         = access.getHeight();

            DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * width);

            context.texImage2D(s_cubeFaceTargets[face], levelNdx, m_textureParams[texNdx].internalFormat, width, height,
                               0 /* border */, formatGl.format, formatGl.dataType, access.getDataPtr());
            GLU_EXPECT_NO_ERROR(context.getError(), "Set cube map image data");
        }
    }
}

void TextureUnitCase::upload2dArrayTexture(int texNdx, sglr::Context &context)
{
    int ndx2dArray                     = m_ndxTexType[texNdx];
    const tcu::Texture2DArray *texture = m_textures2dArray[ndx2dArray];
    glu::TransferFormat formatGl =
        glu::getTransferFormat(glu::mapGLInternalFormat(m_textureParams[texNdx].internalFormat));

    context.pixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int levelNdx = 0; levelNdx < texture->getNumLevels(); levelNdx++)
    {
        if (texture->isLevelEmpty(levelNdx))
            continue;

        tcu::ConstPixelBufferAccess access = texture->getLevel(levelNdx);
        int width                          = access.getWidth();
        int height                         = access.getHeight();
        int layers                         = access.getDepth();

        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * width);
        DE_ASSERT(access.getSlicePitch() == access.getFormat().getPixelSize() * width * height);

        context.texImage3D(GL_TEXTURE_2D_ARRAY, levelNdx, m_textureParams[texNdx].internalFormat, width, height, layers,
                           0 /* border */, formatGl.format, formatGl.dataType, access.getDataPtr());
        GLU_EXPECT_NO_ERROR(context.getError(), "Set 2d array texture image data");
    }
}

void TextureUnitCase::upload3dTexture(int texNdx, sglr::Context &context)
{
    int ndx3d                     = m_ndxTexType[texNdx];
    const tcu::Texture3D *texture = m_textures3d[ndx3d];
    glu::TransferFormat formatGl =
        glu::getTransferFormat(glu::mapGLInternalFormat(m_textureParams[texNdx].internalFormat));

    context.pixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int levelNdx = 0; levelNdx < texture->getNumLevels(); levelNdx++)
    {
        if (texture->isLevelEmpty(levelNdx))
            continue;

        tcu::ConstPixelBufferAccess access = texture->getLevel(levelNdx);
        int width                          = access.getWidth();
        int height                         = access.getHeight();
        int depth                          = access.getDepth();

        DE_ASSERT(access.getRowPitch() == access.getFormat().getPixelSize() * width);
        DE_ASSERT(access.getSlicePitch() == access.getFormat().getPixelSize() * width * height);

        context.texImage3D(GL_TEXTURE_3D, levelNdx, m_textureParams[texNdx].internalFormat, width, height, depth,
                           0 /* border */, formatGl.format, formatGl.dataType, access.getDataPtr());
        GLU_EXPECT_NO_ERROR(context.getError(), "Set 3d texture image data");
    }
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
    GLU_EXPECT_NO_ERROR(context.getError(), "Generate textures");

    for (int unitNdx = 0; unitNdx < m_numUnits; unitNdx++)
    {
        int texNdx = m_unitTextures[unitNdx];

        // Bind texture to unit.
        context.activeTexture(GL_TEXTURE0 + unitNdx);
        GLU_EXPECT_NO_ERROR(context.getError(), "Set active texture");
        context.bindTexture(m_textureTypes[texNdx], textureGLNames[texNdx]);
        GLU_EXPECT_NO_ERROR(context.getError(), "Bind texture");

        if (!isTextureSetUp[texNdx])
        {
            // Binding this texture for first time, so set parameters and data.

            context.texParameteri(m_textureTypes[texNdx], GL_TEXTURE_WRAP_S, m_textureParams[texNdx].wrapModeS);
            context.texParameteri(m_textureTypes[texNdx], GL_TEXTURE_WRAP_T, m_textureParams[texNdx].wrapModeT);
            if (m_textureTypes[texNdx] == GL_TEXTURE_3D)
                context.texParameteri(m_textureTypes[texNdx], GL_TEXTURE_WRAP_R, m_textureParams[texNdx].wrapModeR);
            context.texParameteri(m_textureTypes[texNdx], GL_TEXTURE_MIN_FILTER, m_textureParams[texNdx].minFilter);
            context.texParameteri(m_textureTypes[texNdx], GL_TEXTURE_MAG_FILTER, m_textureParams[texNdx].magFilter);
            GLU_EXPECT_NO_ERROR(context.getError(), "Set texture parameters");

            switch (m_textureTypes[texNdx])
            {
            case GL_TEXTURE_2D:
                upload2dTexture(texNdx, context);
                break;
            case GL_TEXTURE_CUBE_MAP:
                uploadCubeTexture(texNdx, context);
                break;
            case GL_TEXTURE_2D_ARRAY:
                upload2dArrayTexture(texNdx, context);
                break;
            case GL_TEXTURE_3D:
                upload3dTexture(texNdx, context);
                break;
            default:
                DE_ASSERT(false);
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
                (TextureUnitCase::CaseType)caseType == TextureUnitCase::CASE_ONLY_2D       ? "only_2d" :
                (TextureUnitCase::CaseType)caseType == TextureUnitCase::CASE_ONLY_CUBE     ? "only_cube" :
                (TextureUnitCase::CaseType)caseType == TextureUnitCase::CASE_ONLY_2D_ARRAY ? "only_2d_array" :
                (TextureUnitCase::CaseType)caseType == TextureUnitCase::CASE_ONLY_3D       ? "only_3d" :
                (TextureUnitCase::CaseType)caseType == TextureUnitCase::CASE_MIXED         ? "mixed" :
                                                                                             nullptr;

            DE_ASSERT(caseTypeGroupName != nullptr);

            tcu::TestCaseGroup *caseTypeGroup = new tcu::TestCaseGroup(m_testCtx, caseTypeGroupName, "");
            countGroup->addChild(caseTypeGroup);

            for (int testNdx = 0; testNdx < numTestsPerGroup; testNdx++)
                caseTypeGroup->addChild(new TextureUnitCase(m_context, de::toString(testNdx).c_str(), "", numUnits,
                                                            (TextureUnitCase::CaseType)caseType,
                                                            deUint32Hash((uint32_t)testNdx)));
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
