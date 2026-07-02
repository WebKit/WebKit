/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Shader execute test.
 *
 * \todo [petri] Multiple grid with differing constants/uniforms.
 * \todo [petri]
 *//*--------------------------------------------------------------------*/

#include "glsShaderRenderCase.hpp"

#include "tcuSurface.hpp"
#include "tcuVector.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"

#include "gluPixelTransfer.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "gluDrawUtil.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "deRandom.hpp"
#include "deString.h"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <vector>
#include <string>

namespace deqp
{
namespace gls
{

using namespace std;
using namespace tcu;
using namespace glu;

static const int GRID_SIZE                 = 92;
static const int MAX_RENDER_WIDTH          = 128;
static const int MAX_RENDER_HEIGHT         = 112;
static const tcu::Vec4 DEFAULT_CLEAR_COLOR = tcu::Vec4(0.125f, 0.25f, 0.5f, 1.0f);

// TextureBinding

TextureBinding::TextureBinding(const glu::Texture2D *tex2D, const tcu::Sampler &sampler)
    : m_type(TYPE_2D)
    , m_sampler(sampler)
{
    m_binding.tex2D = tex2D;
}

TextureBinding::TextureBinding(const glu::TextureCube *texCube, const tcu::Sampler &sampler)
    : m_type(TYPE_CUBE_MAP)
    , m_sampler(sampler)
{
    m_binding.texCube = texCube;
}

TextureBinding::TextureBinding(const glu::Texture2DArray *tex2DArray, const tcu::Sampler &sampler)
    : m_type(TYPE_2D_ARRAY)
    , m_sampler(sampler)
{
    m_binding.tex2DArray = tex2DArray;
}

TextureBinding::TextureBinding(const glu::Texture3D *tex3D, const tcu::Sampler &sampler)
    : m_type(TYPE_3D)
    , m_sampler(sampler)
{
    m_binding.tex3D = tex3D;
}

TextureBinding::TextureBinding(void) : m_type(TYPE_NONE)
{
    m_binding.tex2D = nullptr;
}

void TextureBinding::setSampler(const tcu::Sampler &sampler)
{
    m_sampler = sampler;
}

void TextureBinding::setTexture(const glu::Texture2D *tex2D)
{
    m_type          = TYPE_2D;
    m_binding.tex2D = tex2D;
}

void TextureBinding::setTexture(const glu::TextureCube *texCube)
{
    m_type            = TYPE_CUBE_MAP;
    m_binding.texCube = texCube;
}

void TextureBinding::setTexture(const glu::Texture2DArray *tex2DArray)
{
    m_type               = TYPE_2D_ARRAY;
    m_binding.tex2DArray = tex2DArray;
}

void TextureBinding::setTexture(const glu::Texture3D *tex3D)
{
    m_type          = TYPE_3D;
    m_binding.tex3D = tex3D;
}

// QuadGrid.

class QuadGrid
{
public:
    QuadGrid(int gridSize, int screenWidth, int screenHeight, const Vec4 &constCoords,
             const vector<Mat4> &userAttribTransforms, const vector<TextureBinding> &textures);
    ~QuadGrid(void);

    int getGridSize(void) const
    {
        return m_gridSize;
    }
    int getNumVertices(void) const
    {
        return m_numVertices;
    }
    int getNumTriangles(void) const
    {
        return m_numTriangles;
    }
    const Vec4 &getConstCoords(void) const
    {
        return m_constCoords;
    }
    const vector<Mat4> getUserAttribTransforms(void) const
    {
        return m_userAttribTransforms;
    }
    const vector<TextureBinding> &getTextures(void) const
    {
        return m_textures;
    }

    const Vec4 *getPositions(void) const
    {
        return &m_positions[0];
    }
    const float *getAttribOne(void) const
    {
        return &m_attribOne[0];
    }
    const Vec4 *getCoords(void) const
    {
        return &m_coords[0];
    }
    const Vec4 *getUnitCoords(void) const
    {
        return &m_unitCoords[0];
    }
    const Vec4 *getUserAttrib(int attribNdx) const
    {
        return &m_userAttribs[attribNdx][0];
    }
    const uint16_t *getIndices(void) const
    {
        return &m_indices[0];
    }

    Vec4 getCoords(float sx, float sy) const;
    Vec4 getUnitCoords(float sx, float sy) const;

    int getNumUserAttribs(void) const
    {
        return (int)m_userAttribTransforms.size();
    }
    Vec4 getUserAttrib(int attribNdx, float sx, float sy) const;

private:
    int m_gridSize;
    int m_numVertices;
    int m_numTriangles;
    Vec4 m_constCoords;
    vector<Mat4> m_userAttribTransforms;
    vector<TextureBinding> m_textures;

    vector<Vec4> m_screenPos;
    vector<Vec4> m_positions;
    vector<Vec4> m_coords;     //!< Near-unit coordinates, roughly [-2.0 .. 2.0].
    vector<Vec4> m_unitCoords; //!< Positive-only coordinates [0.0 .. 1.5].
    vector<float> m_attribOne;
    vector<Vec4> m_userAttribs[ShaderEvalContext::MAX_TEXTURES];
    vector<uint16_t> m_indices;
};

QuadGrid::QuadGrid(int gridSize, int width, int height, const Vec4 &constCoords,
                   const vector<Mat4> &userAttribTransforms, const vector<TextureBinding> &textures)
    : m_gridSize(gridSize)
    , m_numVertices((gridSize + 1) * (gridSize + 1))
    , m_numTriangles(gridSize * gridSize * 2)
    , m_constCoords(constCoords)
    , m_userAttribTransforms(userAttribTransforms)
    , m_textures(textures)
{
    Vec4 viewportScale = Vec4((float)width, (float)height, 0.0f, 0.0f);

    // Compute vertices.
    m_positions.resize(m_numVertices);
    m_coords.resize(m_numVertices);
    m_unitCoords.resize(m_numVertices);
    m_attribOne.resize(m_numVertices);
    m_screenPos.resize(m_numVertices);

    // User attributes.
    for (int i = 0; i < DE_LENGTH_OF_ARRAY(m_userAttribs); i++)
        m_userAttribs[i].resize(m_numVertices);

    for (int y = 0; y < gridSize + 1; y++)
        for (int x = 0; x < gridSize + 1; x++)
        {
            float sx   = (float)x / (float)gridSize;
            float sy   = (float)y / (float)gridSize;
            float fx   = 2.0f * sx - 1.0f;
            float fy   = 2.0f * sy - 1.0f;
            int vtxNdx = ((y * (gridSize + 1)) + x);

            m_positions[vtxNdx]  = Vec4(fx, fy, 0.0f, 1.0f);
            m_attribOne[vtxNdx]  = 1.0f;
            m_screenPos[vtxNdx]  = Vec4(sx, sy, 0.0f, 1.0f) * viewportScale;
            m_coords[vtxNdx]     = getCoords(sx, sy);
            m_unitCoords[vtxNdx] = getUnitCoords(sx, sy);

            for (int attribNdx = 0; attribNdx < getNumUserAttribs(); attribNdx++)
                m_userAttribs[attribNdx][vtxNdx] = getUserAttrib(attribNdx, sx, sy);
        }

    // Compute indices.
    m_indices.resize(3 * m_numTriangles);
    for (int y = 0; y < gridSize; y++)
        for (int x = 0; x < gridSize; x++)
        {
            int stride = gridSize + 1;
            int v00    = (y * stride) + x;
            int v01    = (y * stride) + x + 1;
            int v10    = ((y + 1) * stride) + x;
            int v11    = ((y + 1) * stride) + x + 1;

            int baseNdx            = ((y * gridSize) + x) * 6;
            m_indices[baseNdx + 0] = (uint16_t)v10;
            m_indices[baseNdx + 1] = (uint16_t)v00;
            m_indices[baseNdx + 2] = (uint16_t)v01;

            m_indices[baseNdx + 3] = (uint16_t)v10;
            m_indices[baseNdx + 4] = (uint16_t)v01;
            m_indices[baseNdx + 5] = (uint16_t)v11;
        }
}

QuadGrid::~QuadGrid(void)
{
}

inline Vec4 QuadGrid::getCoords(float sx, float sy) const
{
    float fx = 2.0f * sx - 1.0f;
    float fy = 2.0f * sy - 1.0f;
    return Vec4(fx, fy, -fx + 0.33f * fy, -0.275f * fx - fy);
}

inline Vec4 QuadGrid::getUnitCoords(float sx, float sy) const
{
    return Vec4(sx, sy, 0.33f * sx + 0.5f * sy, 0.5f * sx + 0.25f * sy);
}

inline Vec4 QuadGrid::getUserAttrib(int attribNdx, float sx, float sy) const
{
    // homogeneous normalized screen-space coordinates
    return m_userAttribTransforms[attribNdx] * Vec4(sx, sy, 0.0f, 1.0f);
}

// ShaderEvalContext.

ShaderEvalContext::ShaderEvalContext(const QuadGrid &quadGrid_)
    : constCoords(quadGrid_.getConstCoords())
    , isDiscarded(false)
    , quadGrid(quadGrid_)
{
    const vector<TextureBinding> &bindings = quadGrid.getTextures();
    DE_ASSERT((int)bindings.size() <= MAX_TEXTURES);

    // Fill in texture array.
    for (int ndx = 0; ndx < (int)bindings.size(); ndx++)
    {
        const TextureBinding &binding = bindings[ndx];

        if (binding.getType() == TextureBinding::TYPE_NONE)
            continue;

        textures[ndx].sampler = binding.getSampler();

        switch (binding.getType())
        {
        case TextureBinding::TYPE_2D:
            textures[ndx].tex2D = &binding.get2D()->getRefTexture();
            break;
        case TextureBinding::TYPE_CUBE_MAP:
            textures[ndx].texCube = &binding.getCube()->getRefTexture();
            break;
        case TextureBinding::TYPE_2D_ARRAY:
            textures[ndx].tex2DArray = &binding.get2DArray()->getRefTexture();
            break;
        case TextureBinding::TYPE_3D:
            textures[ndx].tex3D = &binding.get3D()->getRefTexture();
            break;
        default:
            DE_ASSERT(false);
        }
    }
}

ShaderEvalContext::~ShaderEvalContext(void)
{
}

void ShaderEvalContext::reset(float sx, float sy)
{
    // Clear old values
    color       = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    isDiscarded = false;

    // Compute coords
    coords     = quadGrid.getCoords(sx, sy);
    unitCoords = quadGrid.getUnitCoords(sx, sy);

    // Compute user attributes.
    int numAttribs = quadGrid.getNumUserAttribs();
    DE_ASSERT(numAttribs <= MAX_USER_ATTRIBS);
    for (int attribNdx = 0; attribNdx < numAttribs; attribNdx++)
        in[attribNdx] = quadGrid.getUserAttrib(attribNdx, sx, sy);
}

tcu::Vec4 ShaderEvalContext::texture2D(int unitNdx, const tcu::Vec2 &texCoords)
{
    if (textures[unitNdx].tex2D)
        return textures[unitNdx].tex2D->sample(textures[unitNdx].sampler, texCoords.x(), texCoords.y(), 0.0f);
    else
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

// ShaderEvaluator

ShaderEvaluator::ShaderEvaluator(void) : m_evalFunc(nullptr)
{
}

ShaderEvaluator::ShaderEvaluator(ShaderEvalFunc evalFunc) : m_evalFunc(evalFunc)
{
}

ShaderEvaluator::~ShaderEvaluator(void)
{
}

void ShaderEvaluator::evaluate(ShaderEvalContext &ctx)
{
    DE_ASSERT(m_evalFunc);
    m_evalFunc(ctx);
}

// ShaderRenderCase.

ShaderRenderCase::ShaderRenderCase(TestContext &testCtx, RenderContext &renderCtx, const ContextInfo &ctxInfo,
                                   const char *name, const char *description, bool isVertexCase,
                                   ShaderEvalFunc evalFunc, bool useLevel)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_ctxInfo(ctxInfo)
    , m_isVertexCase(isVertexCase)
    , m_useLevel(useLevel)
    , m_defaultEvaluator(evalFunc)
    , m_evaluator(m_defaultEvaluator)
    , m_clearColor(DEFAULT_CLEAR_COLOR)
    , m_program(nullptr)
{
}

ShaderRenderCase::ShaderRenderCase(TestContext &testCtx, RenderContext &renderCtx, const ContextInfo &ctxInfo,
                                   const char *name, const char *description, bool isVertexCase,
                                   ShaderEvaluator &evaluator, bool useLevel)
    : TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_ctxInfo(ctxInfo)
    , m_isVertexCase(isVertexCase)
    , m_useLevel(useLevel)
    , m_defaultEvaluator(nullptr)
    , m_evaluator(evaluator)
    , m_clearColor(DEFAULT_CLEAR_COLOR)
    , m_program(nullptr)
{
}

ShaderRenderCase::~ShaderRenderCase(void)
{
    ShaderRenderCase::deinit();
}

void ShaderRenderCase::init(void)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();

    GLU_EXPECT_NO_ERROR(gl.getError(), "ShaderRenderCase::init() begin");

    if (m_vertShaderSource.empty() || m_fragShaderSource.empty())
    {
        DE_ASSERT(m_vertShaderSource.empty() && m_fragShaderSource.empty());
        setupShaderData();
    }

    DE_ASSERT(!m_program);
    m_program = new ShaderProgram(m_renderCtx, makeVtxFragSources(m_vertShaderSource, m_fragShaderSource));

    try
    {
        log << *m_program; // Always log shader program.

        if (!m_program->isOk())
            throw CompileFailed(__FILE__, __LINE__);

        GLU_EXPECT_NO_ERROR(gl.getError(), "ShaderRenderCase::init() end");
    }
    catch (const std::exception &)
    {
        // Clean up.
        ShaderRenderCase::deinit();
        throw;
    }

    m_gridSize = GRID_SIZE;
}

void ShaderRenderCase::deinit(void)
{
    delete m_program;
    m_program = nullptr;
}

tcu::IVec2 ShaderRenderCase::getViewportSize(void) const
{
    return tcu::IVec2(de::min(m_renderCtx.getRenderTarget().getWidth(), MAX_RENDER_WIDTH),
                      de::min(m_renderCtx.getRenderTarget().getHeight(), MAX_RENDER_HEIGHT));
}

TestNode::IterateResult ShaderRenderCase::iterate(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    GLU_EXPECT_NO_ERROR(gl.getError(), "ShaderRenderCase::iterate() begin");

    DE_ASSERT(m_program);
    uint32_t programID = m_program->getProgram();
    gl.useProgram(programID);

    // Create quad grid.
    IVec2 viewportSize = getViewportSize();
    int width          = viewportSize.x();
    int height         = viewportSize.y();

    // \todo [petri] Better handling of constCoords (render in multiple chunks, vary coords).
    QuadGrid quadGrid(m_isVertexCase ? m_gridSize : 4, width, height, Vec4(0.125f, 0.25f, 0.5f, 1.0f),
                      m_userAttribTransforms, m_textures);

    // Render result. When using levels, the render will be done to the internal level with the right format. After
    // finishing and reading the pixels back correctly, we'll convert the level pixels to a tcu::Surface in order to be
    // used with fuzzyCompare, which does not take arbitrary formats.
    using ISPtr    = de::MovePtr<InternalSurface>;
    using LevelPtr = de::MovePtr<tcu::TextureLevel>;

    // Always used and point either to the surface directly or to an intermediate level.
    ISPtr resInternalSurfacePtr;
    ISPtr refInternalSurfacePtr;

    // Used when m_useLevel is true as intermediate results.
    LevelPtr resLevelPtr;
    LevelPtr refLevelPtr;

    tcu::Surface refImage(width, height);
    tcu::Surface resImage(width, height);

    if (m_useLevel)
    {
        const auto pixelFormat = m_renderCtx.getRenderTarget().getPixelFormat();
        uint32_t type = ((pixelFormat.redBits == 10) && (pixelFormat.greenBits == 10) && (pixelFormat.blueBits == 10) &&
                         (pixelFormat.alphaBits == 2 || pixelFormat.alphaBits == 0)) ?
                            GL_UNSIGNED_INT_2_10_10_10_REV :
                            GL_UNSIGNED_BYTE;
        const auto textureFormat = mapGLTransferFormat(GL_RGBA, type);

        resLevelPtr = LevelPtr(new tcu::TextureLevel(textureFormat, width, height));
        refLevelPtr = LevelPtr(new tcu::TextureLevel(textureFormat, width, height));

        resInternalSurfacePtr = ISPtr(new InternalSurface(*resLevelPtr));
        refInternalSurfacePtr = ISPtr(new InternalSurface(*refLevelPtr));
    }
    else
    {
        // In this case, the internal surface points directly to the result surface.
        resInternalSurfacePtr = ISPtr(new InternalSurface(resImage));
        refInternalSurfacePtr = ISPtr(new InternalSurface(refImage));
    }

    render(*resInternalSurfacePtr, programID, quadGrid);

    // Compute reference.
    if (m_isVertexCase)
        computeVertexReference(*refInternalSurfacePtr, quadGrid);
    else
        computeFragmentReference(*refInternalSurfacePtr, quadGrid);

    if (m_useLevel)
    {
        // Convert intermediate result and reference levels to a surface for fuzzyCompare.
        const auto resAccess = resLevelPtr->getAccess();
        const auto refAccess = refLevelPtr->getAccess();

        const auto convertPixel = [](tcu::Surface &out, const PixelBufferAccess &access, int x, int y)
        {
            const auto color = access.getPixel(x, y);
            const auto rgba  = tcu::RGBA(color);
            out.setPixel(x, y, rgba);
        };

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                convertPixel(resImage, resAccess, x, y);
                convertPixel(refImage, refAccess, x, y);
            }
    }

    // Compare.
    bool testOk = compareImages(resImage, refImage, 0.07f);

    // De-initialize.
    gl.useProgram(0);

    m_testCtx.setTestResult((testOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL), (testOk ? "Pass" : "Fail"));
    return TestNode::STOP;
}

void ShaderRenderCase::setupShaderData(void)
{
}

void ShaderRenderCase::setup(int programID)
{
    DE_UNREF(programID);
}

void ShaderRenderCase::setupUniforms(int programID, const Vec4 &constCoords)
{
    DE_UNREF(programID);
    DE_UNREF(constCoords);
}

void ShaderRenderCase::setupDefaultInputs(int programID)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    // SETUP UNIFORMS.

    setupDefaultUniforms(m_renderCtx, programID);

    GLU_EXPECT_NO_ERROR(gl.getError(), "post uniform setup");

    // SETUP TEXTURES.

    for (int ndx = 0; ndx < (int)m_textures.size(); ndx++)
    {
        const TextureBinding &tex   = m_textures[ndx];
        const tcu::Sampler &sampler = tex.getSampler();
        uint32_t texTarget          = GL_NONE;
        uint32_t texObj             = 0;

        if (tex.getType() == TextureBinding::TYPE_NONE)
            continue;

        // Feature check.
        if (m_renderCtx.getType().getAPI() == glu::ApiType::es(2, 0))
        {
            if (tex.getType() == TextureBinding::TYPE_2D_ARRAY)
                throw tcu::NotSupportedError("2D array texture binding is not supported");

            if (tex.getType() == TextureBinding::TYPE_3D)
                throw tcu::NotSupportedError("3D texture binding is not supported");

            if (sampler.compare != tcu::Sampler::COMPAREMODE_NONE)
                throw tcu::NotSupportedError("Shadow lookups are not supported");
        }

        switch (tex.getType())
        {
        case TextureBinding::TYPE_2D:
            texTarget = GL_TEXTURE_2D;
            texObj    = tex.get2D()->getGLTexture();
            break;
        case TextureBinding::TYPE_CUBE_MAP:
            texTarget = GL_TEXTURE_CUBE_MAP;
            texObj    = tex.getCube()->getGLTexture();
            break;
        case TextureBinding::TYPE_2D_ARRAY:
            texTarget = GL_TEXTURE_2D_ARRAY;
            texObj    = tex.get2DArray()->getGLTexture();
            break;
        case TextureBinding::TYPE_3D:
            texTarget = GL_TEXTURE_3D;
            texObj    = tex.get3D()->getGLTexture();
            break;
        default:
            DE_ASSERT(false);
        }

        gl.activeTexture(GL_TEXTURE0 + ndx);
        gl.bindTexture(texTarget, texObj);
        gl.texParameteri(texTarget, GL_TEXTURE_WRAP_S, glu::getGLWrapMode(sampler.wrapS));
        gl.texParameteri(texTarget, GL_TEXTURE_WRAP_T, glu::getGLWrapMode(sampler.wrapT));
        gl.texParameteri(texTarget, GL_TEXTURE_MIN_FILTER, glu::getGLFilterMode(sampler.minFilter));
        gl.texParameteri(texTarget, GL_TEXTURE_MAG_FILTER, glu::getGLFilterMode(sampler.magFilter));

        if (texTarget == GL_TEXTURE_3D)
            gl.texParameteri(texTarget, GL_TEXTURE_WRAP_R, glu::getGLWrapMode(sampler.wrapR));

        if (sampler.compare != tcu::Sampler::COMPAREMODE_NONE)
        {
            gl.texParameteri(texTarget, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            gl.texParameteri(texTarget, GL_TEXTURE_COMPARE_FUNC, glu::getGLCompareFunc(sampler.compare));
        }
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "texture sampler setup");
}

static void getDefaultVertexArrays(const glw::Functions &gl, const QuadGrid &quadGrid, uint32_t program,
                                   vector<VertexArrayBinding> &vertexArrays)
{
    const int numElements = quadGrid.getNumVertices();

    vertexArrays.push_back(va::Float("a_position", 4, numElements, 0, (const float *)quadGrid.getPositions()));
    vertexArrays.push_back(va::Float("a_coords", 4, numElements, 0, (const float *)quadGrid.getCoords()));
    vertexArrays.push_back(va::Float("a_unitCoords", 4, numElements, 0, (const float *)quadGrid.getUnitCoords()));
    vertexArrays.push_back(va::Float("a_one", 1, numElements, 0, quadGrid.getAttribOne()));

    // a_inN.
    for (int userNdx = 0; userNdx < quadGrid.getNumUserAttribs(); userNdx++)
    {
        string name = string("a_in") + de::toString(userNdx);
        vertexArrays.push_back(va::Float(name, 4, numElements, 0, (const float *)quadGrid.getUserAttrib(userNdx)));
    }

    // Matrix attributes - these are set by location
    static const struct
    {
        const char *name;
        int numCols;
        int numRows;
    } matrices[] = {{"a_mat2", 2, 2},   {"a_mat2x3", 2, 3}, {"a_mat2x4", 2, 4}, {"a_mat3x2", 3, 2}, {"a_mat3", 3, 3},
                    {"a_mat3x4", 3, 4}, {"a_mat4x2", 4, 2}, {"a_mat4x3", 4, 3}, {"a_mat4", 4, 4}};

    for (int matNdx = 0; matNdx < DE_LENGTH_OF_ARRAY(matrices); matNdx++)
    {
        int loc = gl.getAttribLocation(program, matrices[matNdx].name);

        if (loc < 0)
            continue; // Not used in shader.

        int numRows = matrices[matNdx].numRows;
        int numCols = matrices[matNdx].numCols;

        for (int colNdx = 0; colNdx < numCols; colNdx++)
            vertexArrays.push_back(va::Float(loc + colNdx, numRows, numElements, 4 * (int)sizeof(float),
                                             (const float *)quadGrid.getUserAttrib(colNdx)));
    }
}

void ShaderRenderCase::render(InternalSurface &result, int programID, const QuadGrid &quadGrid)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    GLU_EXPECT_NO_ERROR(gl.getError(), "pre render");

    // Buffer info.
    int width  = result.getWidth();
    int height = result.getHeight();

    int xOffsetMax = m_renderCtx.getRenderTarget().getWidth() - width;
    int yOffsetMax = m_renderCtx.getRenderTarget().getHeight() - height;

    uint32_t hash = deStringHash(m_vertShaderSource.c_str()) + deStringHash(m_fragShaderSource.c_str());
    de::Random rnd(hash);

    int xOffset = rnd.getInt(0, xOffsetMax);
    int yOffset = rnd.getInt(0, yOffsetMax);

    gl.viewport(xOffset, yOffset, width, height);

    // Setup program.
    setupUniforms(programID, quadGrid.getConstCoords());
    setupDefaultInputs(programID);

    // Clear.
    gl.clearColor(m_clearColor.x(), m_clearColor.y(), m_clearColor.z(), m_clearColor.w());
    gl.clear(GL_COLOR_BUFFER_BIT);

    // Draw.
    {
        std::vector<VertexArrayBinding> vertexArrays;
        const int numElements = quadGrid.getNumTriangles() * 3;

        getDefaultVertexArrays(gl, quadGrid, programID, vertexArrays);
        draw(m_renderCtx, programID, (int)vertexArrays.size(), &vertexArrays[0],
             pr::Triangles(numElements, quadGrid.getIndices()));
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "draw");

    // Read back results.
    glu::readPixels(m_renderCtx, xOffset, yOffset, result.getAccess());

    GLU_EXPECT_NO_ERROR(gl.getError(), "post render");
}

void ShaderRenderCase::computeVertexReference(InternalSurface &result, const QuadGrid &quadGrid)
{
    // Buffer info.
    int width     = result.getWidth();
    int height    = result.getHeight();
    int gridSize  = quadGrid.getGridSize();
    int stride    = gridSize + 1;
    bool hasAlpha = m_renderCtx.getRenderTarget().getPixelFormat().alphaBits > 0;
    ShaderEvalContext evalCtx(quadGrid);

    // Evaluate color for each vertex.
    vector<Vec4> colors((gridSize + 1) * (gridSize + 1));
    for (int y = 0; y < gridSize + 1; y++)
        for (int x = 0; x < gridSize + 1; x++)
        {
            float sx   = (float)x / (float)gridSize;
            float sy   = (float)y / (float)gridSize;
            int vtxNdx = ((y * (gridSize + 1)) + x);

            evalCtx.reset(sx, sy);
            m_evaluator.evaluate(evalCtx);
            DE_ASSERT(!evalCtx.isDiscarded); // Discard is not available in vertex shader.
            Vec4 color = evalCtx.color;

            if (!hasAlpha)
                color.w() = 1.0f;

            colors[vtxNdx] = color;
        }

    // Render quads.
    for (int y = 0; y < gridSize; y++)
        for (int x = 0; x < gridSize; x++)
        {
            float x0 = (float)x / (float)gridSize;
            float x1 = (float)(x + 1) / (float)gridSize;
            float y0 = (float)y / (float)gridSize;
            float y1 = (float)(y + 1) / (float)gridSize;

            float sx0  = x0 * (float)width;
            float sx1  = x1 * (float)width;
            float sy0  = y0 * (float)height;
            float sy1  = y1 * (float)height;
            float oosx = 1.0f / (sx1 - sx0);
            float oosy = 1.0f / (sy1 - sy0);

            int ix0 = deCeilFloatToInt32(sx0 - 0.5f);
            int ix1 = deCeilFloatToInt32(sx1 - 0.5f);
            int iy0 = deCeilFloatToInt32(sy0 - 0.5f);
            int iy1 = deCeilFloatToInt32(sy1 - 0.5f);

            int v00  = (y * stride) + x;
            int v01  = (y * stride) + x + 1;
            int v10  = ((y + 1) * stride) + x;
            int v11  = ((y + 1) * stride) + x + 1;
            Vec4 c00 = colors[v00];
            Vec4 c01 = colors[v01];
            Vec4 c10 = colors[v10];
            Vec4 c11 = colors[v11];

            //printf("(%d,%d) -> (%f..%f, %f..%f) (%d..%d, %d..%d)\n", x, y, sx0, sx1, sy0, sy1, ix0, ix1, iy0, iy1);

            for (int iy = iy0; iy < iy1; iy++)
                for (int ix = ix0; ix < ix1; ix++)
                {
                    DE_ASSERT(deInBounds32(ix, 0, width));
                    DE_ASSERT(deInBounds32(iy, 0, height));

                    float sfx = (float)ix + 0.5f;
                    float sfy = (float)iy + 0.5f;
                    float fx1 = deFloatClamp((sfx - sx0) * oosx, 0.0f, 1.0f);
                    float fy1 = deFloatClamp((sfy - sy0) * oosy, 0.0f, 1.0f);

                    // Triangle quad interpolation.
                    bool tri       = fx1 + fy1 <= 1.0f;
                    float tx       = tri ? fx1 : (1.0f - fx1);
                    float ty       = tri ? fy1 : (1.0f - fy1);
                    const Vec4 &t0 = tri ? c00 : c11;
                    const Vec4 &t1 = tri ? c01 : c10;
                    const Vec4 &t2 = tri ? c10 : c01;
                    Vec4 color     = t0 + (t1 - t0) * tx + (t2 - t0) * ty;

                    result.setPixel(ix, iy, color);
                }
        }
}

void ShaderRenderCase::computeFragmentReference(InternalSurface &result, const QuadGrid &quadGrid)
{
    // Buffer info.
    int width     = result.getWidth();
    int height    = result.getHeight();
    bool hasAlpha = m_renderCtx.getRenderTarget().getPixelFormat().alphaBits > 0;
    ShaderEvalContext evalCtx(quadGrid);

    // Render.
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            float sx = ((float)x + 0.5f) / (float)width;
            float sy = ((float)y + 0.5f) / (float)height;

            evalCtx.reset(sx, sy);
            m_evaluator.evaluate(evalCtx);
            // Select either clear color or computed color based on discarded bit.
            Vec4 color = evalCtx.isDiscarded ? m_clearColor : evalCtx.color;

            if (!hasAlpha)
                color.w() = 1.0f;

            result.setPixel(x, y, color);
        }
}

bool ShaderRenderCase::compareImages(const Surface &resImage, const Surface &refImage, float errorThreshold)
{
    return tcu::fuzzyCompare(m_testCtx.getLog(), "ComparisonResult", "Image comparison result", refImage, resImage,
                             errorThreshold, tcu::COMPARE_LOG_EVERYTHING);
}

// Uniform name helpers.

const char *getIntUniformName(int number)
{
    switch (number)
    {
    case 0:
        return "ui_zero";
    case 1:
        return "ui_one";
    case 2:
        return "ui_two";
    case 3:
        return "ui_three";
    case 4:
        return "ui_four";
    case 5:
        return "ui_five";
    case 6:
        return "ui_six";
    case 7:
        return "ui_seven";
    case 8:
        return "ui_eight";
    case 101:
        return "ui_oneHundredOne";
    default:
        DE_ASSERT(false);
        return "";
    }
}

const char *getFloatUniformName(int number)
{
    switch (number)
    {
    case 0:
        return "uf_zero";
    case 1:
        return "uf_one";
    case 2:
        return "uf_two";
    case 3:
        return "uf_three";
    case 4:
        return "uf_four";
    case 5:
        return "uf_five";
    case 6:
        return "uf_six";
    case 7:
        return "uf_seven";
    case 8:
        return "uf_eight";
    default:
        DE_ASSERT(false);
        return "";
    }
}

const char *getFloatFractionUniformName(int number)
{
    switch (number)
    {
    case 1:
        return "uf_one";
    case 2:
        return "uf_half";
    case 3:
        return "uf_third";
    case 4:
        return "uf_fourth";
    case 5:
        return "uf_fifth";
    case 6:
        return "uf_sixth";
    case 7:
        return "uf_seventh";
    case 8:
        return "uf_eighth";
    default:
        DE_ASSERT(false);
        return "";
    }
}

void setupDefaultUniforms(const glu::RenderContext &context, uint32_t programID)
{
    const glw::Functions &gl = context.getFunctions();

    // Bool.
    struct BoolUniform
    {
        const char *name;
        bool value;
    };
    static const BoolUniform s_boolUniforms[] = {
        {"ub_true", true},
        {"ub_false", false},
    };

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_boolUniforms); i++)
    {
        int uniLoc = gl.getUniformLocation(programID, s_boolUniforms[i].name);
        if (uniLoc != -1)
            gl.uniform1i(uniLoc, s_boolUniforms[i].value);
    }

    // BVec4.
    struct BVec4Uniform
    {
        const char *name;
        BVec4 value;
    };
    static const BVec4Uniform s_bvec4Uniforms[] = {
        {"ub4_true", BVec4(true)},
        {"ub4_false", BVec4(false)},
    };

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_bvec4Uniforms); i++)
    {
        const BVec4Uniform &uni = s_bvec4Uniforms[i];
        int arr[4];
        arr[0]     = (int)uni.value.x();
        arr[1]     = (int)uni.value.y();
        arr[2]     = (int)uni.value.z();
        arr[3]     = (int)uni.value.w();
        int uniLoc = gl.getUniformLocation(programID, uni.name);
        if (uniLoc != -1)
            gl.uniform4iv(uniLoc, 1, &arr[0]);
    }

    // Int.
    struct IntUniform
    {
        const char *name;
        int value;
    };
    static const IntUniform s_intUniforms[] = {
        {"ui_minusOne", -1}, {"ui_zero", 0}, {"ui_one", 1},   {"ui_two", 2},   {"ui_three", 3},          {"ui_four", 4},
        {"ui_five", 5},      {"ui_six", 6},  {"ui_seven", 7}, {"ui_eight", 8}, {"ui_oneHundredOne", 101}};

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_intUniforms); i++)
    {
        int uniLoc = gl.getUniformLocation(programID, s_intUniforms[i].name);
        if (uniLoc != -1)
            gl.uniform1i(uniLoc, s_intUniforms[i].value);
    }

    // IVec2.
    struct IVec2Uniform
    {
        const char *name;
        IVec2 value;
    };
    static const IVec2Uniform s_ivec2Uniforms[] = {{"ui2_minusOne", IVec2(-1)}, {"ui2_zero", IVec2(0)},
                                                   {"ui2_one", IVec2(1)},       {"ui2_two", IVec2(2)},
                                                   {"ui2_four", IVec2(4)},      {"ui2_five", IVec2(5)}};

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_ivec2Uniforms); i++)
    {
        int uniLoc = gl.getUniformLocation(programID, s_ivec2Uniforms[i].name);
        if (uniLoc != -1)
            gl.uniform2iv(uniLoc, 1, s_ivec2Uniforms[i].value.getPtr());
    }

    // IVec3.
    struct IVec3Uniform
    {
        const char *name;
        IVec3 value;
    };
    static const IVec3Uniform s_ivec3Uniforms[] = {{"ui3_minusOne", IVec3(-1)}, {"ui3_zero", IVec3(0)},
                                                   {"ui3_one", IVec3(1)},       {"ui3_two", IVec3(2)},
                                                   {"ui3_four", IVec3(4)},      {"ui3_five", IVec3(5)}};

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_ivec3Uniforms); i++)
    {
        int uniLoc = gl.getUniformLocation(programID, s_ivec3Uniforms[i].name);
        if (uniLoc != -1)
            gl.uniform3iv(uniLoc, 1, s_ivec3Uniforms[i].value.getPtr());
    }

    // IVec4.
    struct IVec4Uniform
    {
        const char *name;
        IVec4 value;
    };
    static const IVec4Uniform s_ivec4Uniforms[] = {{"ui4_minusOne", IVec4(-1)}, {"ui4_zero", IVec4(0)},
                                                   {"ui4_one", IVec4(1)},       {"ui4_two", IVec4(2)},
                                                   {"ui4_four", IVec4(4)},      {"ui4_five", IVec4(5)}};

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_ivec4Uniforms); i++)
    {
        int uniLoc = gl.getUniformLocation(programID, s_ivec4Uniforms[i].name);
        if (uniLoc != -1)
            gl.uniform4iv(uniLoc, 1, s_ivec4Uniforms[i].value.getPtr());
    }

    // Float.
    struct FloatUniform
    {
        const char *name;
        float value;
    };
    static const FloatUniform s_floatUniforms[] = {
        {"uf_zero", 0.0f},         {"uf_one", 1.0f},          {"uf_two", 2.0f},
        {"uf_three", 3.0f},        {"uf_four", 4.0f},         {"uf_five", 5.0f},
        {"uf_six", 6.0f},          {"uf_seven", 7.0f},        {"uf_eight", 8.0f},
        {"uf_half", 1.0f / 2.0f},  {"uf_third", 1.0f / 3.0f}, {"uf_fourth", 1.0f / 4.0f},
        {"uf_fifth", 1.0f / 5.0f}, {"uf_sixth", 1.0f / 6.0f}, {"uf_seventh", 1.0f / 7.0f},
        {"uf_eighth", 1.0f / 8.0f}};

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_floatUniforms); i++)
    {
        int uniLoc = gl.getUniformLocation(programID, s_floatUniforms[i].name);
        if (uniLoc != -1)
            gl.uniform1f(uniLoc, s_floatUniforms[i].value);
    }

    // Vec2.
    struct Vec2Uniform
    {
        const char *name;
        Vec2 value;
    };
    static const Vec2Uniform s_vec2Uniforms[] = {
        {"uv2_minusOne", Vec2(-1.0f)}, {"uv2_zero", Vec2(0.0f)}, {"uv2_half", Vec2(0.5f)},
        {"uv2_one", Vec2(1.0f)},       {"uv2_two", Vec2(2.0f)},
    };

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_vec2Uniforms); i++)
    {
        int uniLoc = gl.getUniformLocation(programID, s_vec2Uniforms[i].name);
        if (uniLoc != -1)
            gl.uniform2fv(uniLoc, 1, s_vec2Uniforms[i].value.getPtr());
    }

    // Vec3.
    struct Vec3Uniform
    {
        const char *name;
        Vec3 value;
    };
    static const Vec3Uniform s_vec3Uniforms[] = {
        {"uv3_minusOne", Vec3(-1.0f)}, {"uv3_zero", Vec3(0.0f)}, {"uv3_half", Vec3(0.5f)},
        {"uv3_one", Vec3(1.0f)},       {"uv3_two", Vec3(2.0f)},
    };

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_vec3Uniforms); i++)
    {
        int uniLoc = gl.getUniformLocation(programID, s_vec3Uniforms[i].name);
        if (uniLoc != -1)
            gl.uniform3fv(uniLoc, 1, s_vec3Uniforms[i].value.getPtr());
    }

    // Vec4.
    struct Vec4Uniform
    {
        const char *name;
        Vec4 value;
    };
    static const Vec4Uniform s_vec4Uniforms[] = {
        {"uv4_minusOne", Vec4(-1.0f)},
        {"uv4_zero", Vec4(0.0f)},
        {"uv4_half", Vec4(0.5f)},
        {"uv4_one", Vec4(1.0f)},
        {"uv4_two", Vec4(2.0f)},
        {"uv4_black", Vec4(0.0f, 0.0f, 0.0f, 1.0f)},
        {"uv4_gray", Vec4(0.5f, 0.5f, 0.5f, 1.0f)},
        {"uv4_white", Vec4(1.0f, 1.0f, 1.0f, 1.0f)},
    };

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(s_vec4Uniforms); i++)
    {
        int uniLoc = gl.getUniformLocation(programID, s_vec4Uniforms[i].name);
        if (uniLoc != -1)
            gl.uniform4fv(uniLoc, 1, s_vec4Uniforms[i].value.getPtr());
    }
}

ShaderRenderCase::InternalSurface::InternalSurface(tcu::Surface &surface)
    : m_surfacePtr(&surface)
    , m_levelPtr(nullptr)
    , m_levelAccess()
{
}

ShaderRenderCase::InternalSurface::InternalSurface(tcu::TextureLevel &level)
    : m_surfacePtr(nullptr)
    , m_levelPtr(&level)
    , m_levelAccess(m_levelPtr->getAccess())
{
}

int ShaderRenderCase::InternalSurface::getWidth() const
{
    if (m_surfacePtr)
        return m_surfacePtr->getWidth();
    return m_levelPtr->getWidth();
}

int ShaderRenderCase::InternalSurface::getHeight() const
{
    if (m_surfacePtr)
        return m_surfacePtr->getHeight();
    return m_levelPtr->getHeight();
}

tcu::ConstPixelBufferAccess ShaderRenderCase::InternalSurface::getAccess(void) const
{
    if (m_surfacePtr)
        return m_surfacePtr->getAccess();
    return m_levelPtr->getAccess();
}

tcu::PixelBufferAccess ShaderRenderCase::InternalSurface::getAccess(void)
{
    if (m_surfacePtr)
        return m_surfacePtr->getAccess();
    return m_levelPtr->getAccess();
}

void ShaderRenderCase::InternalSurface::setPixel(int x, int y, const tcu::Vec4 &color) const
{
    if (m_surfacePtr)
        m_surfacePtr->setPixel(x, y, tcu::RGBA(color));
    else
        m_levelAccess.setPixel(color, x, y);
}

} // namespace gls
} // namespace deqp
