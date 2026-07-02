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
 * \brief Mipmapping accuracy tests.
 *//*--------------------------------------------------------------------*/

#include "es3aTextureMipmapTests.hpp"

#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuMatrix.hpp"
#include "tcuMatrixUtil.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deString.h"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{
namespace Accuracy
{

using std::string;
using std::vector;
using tcu::IVec4;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using namespace gls::TextureTestUtil;
using namespace glu::TextureTestUtil;

enum CoordType
{
    COORDTYPE_BASIC,      //!< texCoord = translateScale(position).
    COORDTYPE_BASIC_BIAS, //!< Like basic, but with bias values.
    COORDTYPE_AFFINE,     //!< texCoord = translateScaleRotateShear(position).
    COORDTYPE_PROJECTED,  //!< Projected coordinates, w != 1

    COORDTYPE_LAST
};

// Texture2DMipmapCase

class Texture2DMipmapCase : public tcu::TestCase
{
public:
    Texture2DMipmapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &renderCtxInfo,
                        const char *name, const char *desc, CoordType coordType, uint32_t minFilter, uint32_t wrapS,
                        uint32_t wrapT, uint32_t format, uint32_t dataType, int width, int height);
    ~Texture2DMipmapCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    Texture2DMipmapCase(const Texture2DMipmapCase &other);
    Texture2DMipmapCase &operator=(const Texture2DMipmapCase &other);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    CoordType m_coordType;
    uint32_t m_minFilter;
    uint32_t m_wrapS;
    uint32_t m_wrapT;
    uint32_t m_format;
    uint32_t m_dataType;
    int m_width;
    int m_height;

    glu::Texture2D *m_texture;
    TextureRenderer m_renderer;
};

Texture2DMipmapCase::Texture2DMipmapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                         const glu::ContextInfo &renderCtxInfo, const char *name, const char *desc,
                                         CoordType coordType, uint32_t minFilter, uint32_t wrapS, uint32_t wrapT,
                                         uint32_t format, uint32_t dataType, int width, int height)
    : TestCase(testCtx, tcu::NODETYPE_ACCURACY, name, desc)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(renderCtxInfo)
    , m_coordType(coordType)
    , m_minFilter(minFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_format(format)
    , m_dataType(dataType)
    , m_width(width)
    , m_height(height)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
{
}

Texture2DMipmapCase::~Texture2DMipmapCase(void)
{
    deinit();
}

void Texture2DMipmapCase::init(void)
{
    m_texture = new glu::Texture2D(m_renderCtx, m_format, m_dataType, m_width, m_height);

    int numLevels = deLog2Floor32(de::max(m_width, m_height)) + 1;

    // Fill texture with colored grid.
    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        uint32_t step  = 0xff / (numLevels - 1);
        uint32_t inc   = deClamp32(step * levelNdx, 0x00, 0xff);
        uint32_t dec   = 0xff - inc;
        uint32_t rgb   = (inc << 16) | (dec << 8) | 0xff;
        uint32_t color = 0xff000000 | rgb;

        m_texture->getRefTexture().allocLevel(levelNdx);
        tcu::clear(m_texture->getRefTexture().getLevel(levelNdx), tcu::RGBA(color).toVec());
    }
}

void Texture2DMipmapCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

static void getBasicTexCoord2D(std::vector<float> &dst, int cellNdx)
{
    static const struct
    {
        Vec2 bottomLeft;
        Vec2 topRight;
    } s_basicCoords[] = {
        {Vec2(-0.1f, 0.1f), Vec2(0.8f, 1.0f)},     {Vec2(-0.3f, -0.6f), Vec2(0.7f, 0.4f)},
        {Vec2(-0.3f, 0.6f), Vec2(0.7f, -0.9f)},    {Vec2(-0.8f, 0.6f), Vec2(0.7f, -0.9f)},

        {Vec2(-0.5f, -0.5f), Vec2(1.5f, 1.5f)},    {Vec2(1.0f, -1.0f), Vec2(-1.3f, 1.0f)},
        {Vec2(1.2f, -1.0f), Vec2(-1.3f, 1.6f)},    {Vec2(2.2f, -1.1f), Vec2(-1.3f, 0.8f)},

        {Vec2(-1.5f, 1.6f), Vec2(1.7f, -1.4f)},    {Vec2(2.0f, 1.6f), Vec2(2.3f, -1.4f)},
        {Vec2(1.3f, -2.6f), Vec2(-2.7f, 2.9f)},    {Vec2(-0.8f, -6.6f), Vec2(6.0f, -0.9f)},

        {Vec2(-8.0f, 9.0f), Vec2(8.3f, -7.0f)},    {Vec2(-16.0f, 10.0f), Vec2(18.3f, 24.0f)},
        {Vec2(30.2f, 55.0f), Vec2(-24.3f, -1.6f)}, {Vec2(-33.2f, 64.1f), Vec2(32.1f, -64.1f)},
    };

    DE_ASSERT(de::inBounds(cellNdx, 0, DE_LENGTH_OF_ARRAY(s_basicCoords)));

    const Vec2 &bottomLeft = s_basicCoords[cellNdx].bottomLeft;
    const Vec2 &topRight   = s_basicCoords[cellNdx].topRight;

    computeQuadTexCoord2D(dst, bottomLeft, topRight);
}

static void getAffineTexCoord2D(std::vector<float> &dst, int cellNdx)
{
    // Use basic coords as base.
    getBasicTexCoord2D(dst, cellNdx);

    // Rotate based on cell index.
    float angle         = 2.0f * DE_PI * ((float)cellNdx / 16.0f);
    tcu::Mat2 rotMatrix = tcu::rotationMatrix(angle);

    // Second and third row are sheared.
    float shearX          = de::inRange(cellNdx, 4, 11) ? (float)(15 - cellNdx) / 16.0f : 0.0f;
    tcu::Mat2 shearMatrix = tcu::shearMatrix(tcu::Vec2(shearX, 0.0f));

    tcu::Mat2 transform = rotMatrix * shearMatrix;
    Vec2 p0             = transform * Vec2(dst[0], dst[1]);
    Vec2 p1             = transform * Vec2(dst[2], dst[3]);
    Vec2 p2             = transform * Vec2(dst[4], dst[5]);
    Vec2 p3             = transform * Vec2(dst[6], dst[7]);

    dst[0] = p0.x();
    dst[1] = p0.y();
    dst[2] = p1.x();
    dst[3] = p1.y();
    dst[4] = p2.x();
    dst[5] = p2.y();
    dst[6] = p3.x();
    dst[7] = p3.y();
}

Texture2DMipmapCase::IterateResult Texture2DMipmapCase::iterate(void)
{
    // Constants.
    const uint32_t magFilter = GL_NEAREST;

    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();

    const tcu::Texture2D &refTexture = m_texture->getRefTexture();
    const tcu::TextureFormat &texFmt = refTexture.getFormat();
    tcu::TextureFormatInfo fmtInfo   = tcu::getTextureFormatInfo(texFmt);

    int texWidth          = refTexture.getWidth();
    int texHeight         = refTexture.getHeight();
    int defViewportWidth  = texWidth * 4;
    int defViewportHeight = texHeight * 4;

    RandomViewport viewport(m_renderCtx.getRenderTarget(), defViewportWidth, defViewportHeight,
                            deStringHash(getName()));
    ReferenceParams sampleParams(TEXTURETYPE_2D);
    vector<float> texCoord;

    bool isProjected = m_coordType == COORDTYPE_PROJECTED;
    bool useLodBias  = m_coordType == COORDTYPE_BASIC_BIAS;

    tcu::Surface renderedFrame(viewport.width, viewport.height);

    // Accuracy cases test against ideal lod computation.
    tcu::Surface idealFrame(viewport.width, viewport.height);

    // Viewport is divided into 4x4 grid.
    int gridWidth  = 4;
    int gridHeight = 4;
    int cellWidth  = viewport.width / gridWidth;
    int cellHeight = viewport.height / gridHeight;

    // Accuracy measurements are off unless we get the expected viewport size.
    if (viewport.width < defViewportWidth || viewport.height < defViewportHeight)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    // Sampling parameters.
    sampleParams.sampler     = glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, magFilter);
    sampleParams.samplerType = glu::TextureTestUtil::getSamplerType(m_texture->getRefTexture().getFormat());
    sampleParams.colorBias   = fmtInfo.lookupBias;
    sampleParams.colorScale  = fmtInfo.lookupScale;
    sampleParams.flags = (isProjected ? ReferenceParams::PROJECTED : 0) | (useLodBias ? ReferenceParams::USE_BIAS : 0);

    // Upload texture data.
    m_texture->upload();

    // Use unit 0.
    gl.activeTexture(GL_TEXTURE0);

    // Bind gradient texture and setup sampler parameters.
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

    GLU_EXPECT_NO_ERROR(gl.getError(), "After texture setup");

    // Bias values.
    static const float s_bias[] = {1.0f, -2.0f, 0.8f, -0.5f, 1.5f, 0.9f, 2.0f, 4.0f};

    // Projection values.
    static const Vec4 s_projections[] = {Vec4(1.2f, 1.0f, 0.7f, 1.0f), Vec4(1.3f, 0.8f, 0.6f, 2.0f),
                                         Vec4(0.8f, 1.0f, 1.7f, 0.6f), Vec4(1.2f, 1.0f, 1.7f, 1.5f)};

    // Render cells.
    for (int gridY = 0; gridY < gridHeight; gridY++)
    {
        for (int gridX = 0; gridX < gridWidth; gridX++)
        {
            int curX    = cellWidth * gridX;
            int curY    = cellHeight * gridY;
            int curW    = gridX + 1 == gridWidth ? (viewport.width - curX) : cellWidth;
            int curH    = gridY + 1 == gridHeight ? (viewport.height - curY) : cellHeight;
            int cellNdx = gridY * gridWidth + gridX;

            // Compute texcoord.
            switch (m_coordType)
            {
            case COORDTYPE_BASIC_BIAS: // Fall-through.
            case COORDTYPE_PROJECTED:
            case COORDTYPE_BASIC:
                getBasicTexCoord2D(texCoord, cellNdx);
                break;
            case COORDTYPE_AFFINE:
                getAffineTexCoord2D(texCoord, cellNdx);
                break;
            default:
                DE_ASSERT(false);
            }

            if (isProjected)
                sampleParams.w = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];

            if (useLodBias)
                sampleParams.bias = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];

            // Render with GL.
            gl.viewport(viewport.x + curX, viewport.y + curY, curW, curH);
            m_renderer.renderQuad(0, &texCoord[0], sampleParams);

            // Render reference(s).
            {
                tcu::SurfaceAccess idealDst(idealFrame, m_renderCtx.getRenderTarget().getPixelFormat(), curX, curY,
                                            curW, curH);
                sampleParams.lodMode = LODMODE_EXACT;
                sampleTexture(idealDst, m_texture->getRefTexture(), &texCoord[0], sampleParams);
            }
        }
    }

    // Read result.
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compare and log.
    {
        const int bestScoreDiff  = (texWidth / 16) * (texHeight / 16);
        const int worstScoreDiff = texWidth * texHeight;

        int score = measureAccuracy(log, idealFrame, renderedFrame, bestScoreDiff, worstScoreDiff);
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::toString(score).c_str());
    }

    return STOP;
}

// TextureCubeMipmapCase

class TextureCubeMipmapCase : public tcu::TestCase
{
public:
    TextureCubeMipmapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                          const glu::ContextInfo &renderCtxInfo, const char *name, const char *desc,
                          CoordType coordType, uint32_t minFilter, uint32_t wrapS, uint32_t wrapT, uint32_t format,
                          uint32_t dataType, int size);
    ~TextureCubeMipmapCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    TextureCubeMipmapCase(const TextureCubeMipmapCase &other);
    TextureCubeMipmapCase &operator=(const TextureCubeMipmapCase &other);

    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_renderCtxInfo;

    CoordType m_coordType;
    uint32_t m_minFilter;
    uint32_t m_wrapS;
    uint32_t m_wrapT;
    uint32_t m_format;
    uint32_t m_dataType;
    int m_size;

    glu::TextureCube *m_texture;
    TextureRenderer m_renderer;
};

TextureCubeMipmapCase::TextureCubeMipmapCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                             const glu::ContextInfo &renderCtxInfo, const char *name, const char *desc,
                                             CoordType coordType, uint32_t minFilter, uint32_t wrapS, uint32_t wrapT,
                                             uint32_t format, uint32_t dataType, int size)
    : TestCase(testCtx, tcu::NODETYPE_ACCURACY, name, desc)
    , m_renderCtx(renderCtx)
    , m_renderCtxInfo(renderCtxInfo)
    , m_coordType(coordType)
    , m_minFilter(minFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_format(format)
    , m_dataType(dataType)
    , m_size(size)
    , m_texture(nullptr)
    , m_renderer(renderCtx, testCtx.getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
{
}

TextureCubeMipmapCase::~TextureCubeMipmapCase(void)
{
    deinit();
}

void TextureCubeMipmapCase::init(void)
{
    m_texture = new glu::TextureCube(m_renderCtx, m_format, m_dataType, m_size);

    int numLevels = deLog2Floor32(m_size) + 1;

    // Fill texture with colored grid.
    for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; faceNdx++)
    {
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            uint32_t step = 0xff / (numLevels - 1);
            uint32_t inc  = deClamp32(step * levelNdx, 0x00, 0xff);
            uint32_t dec  = 0xff - inc;
            uint32_t rgb  = 0;

            switch (faceNdx)
            {
            case 0:
                rgb = (inc << 16) | (dec << 8) | 255;
                break;
            case 1:
                rgb = (255 << 16) | (inc << 8) | dec;
                break;
            case 2:
                rgb = (dec << 16) | (255 << 8) | inc;
                break;
            case 3:
                rgb = (dec << 16) | (inc << 8) | 255;
                break;
            case 4:
                rgb = (255 << 16) | (dec << 8) | inc;
                break;
            case 5:
                rgb = (inc << 16) | (255 << 8) | dec;
                break;
            }

            uint32_t color = 0xff000000 | rgb;

            m_texture->getRefTexture().allocLevel((tcu::CubeFace)faceNdx, levelNdx);
            tcu::clear(m_texture->getRefTexture().getLevelFace(levelNdx, (tcu::CubeFace)faceNdx),
                       tcu::RGBA(color).toVec());
        }
    }
}

void TextureCubeMipmapCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;

    m_renderer.clear();
}

static void randomPartition(vector<IVec4> &dst, de::Random &rnd, int x, int y, int width, int height)
{
    const int minWidth  = 8;
    const int minHeight = 8;

    bool partition  = rnd.getFloat() > 0.4f;
    bool partitionX = partition && width > minWidth && rnd.getBool();
    bool partitionY = partition && height > minHeight && !partitionX;

    if (partitionX)
    {
        int split = width / 2 + rnd.getInt(-width / 4, +width / 4);
        randomPartition(dst, rnd, x, y, split, height);
        randomPartition(dst, rnd, x + split, y, width - split, height);
    }
    else if (partitionY)
    {
        int split = height / 2 + rnd.getInt(-height / 4, +height / 4);
        randomPartition(dst, rnd, x, y, width, split);
        randomPartition(dst, rnd, x, y + split, width, height - split);
    }
    else
        dst.push_back(IVec4(x, y, width, height));
}

static void computeGridLayout(vector<IVec4> &dst, int width, int height)
{
    de::Random rnd(7);
    randomPartition(dst, rnd, 0, 0, width, height);
}

TextureCubeMipmapCase::IterateResult TextureCubeMipmapCase::iterate(void)
{
    // Constants.
    const uint32_t magFilter = GL_NEAREST;

    int texWidth  = m_texture->getRefTexture().getSize();
    int texHeight = m_texture->getRefTexture().getSize();

    int defViewportWidth  = texWidth * 2;
    int defViewportHeight = texHeight * 2;

    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();
    RandomViewport viewport(m_renderCtx.getRenderTarget(), defViewportWidth, defViewportHeight,
                            deStringHash(getName()));
    tcu::Sampler sampler    = glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, magFilter);
    sampler.seamlessCubeMap = true;

    vector<float> texCoord;

    bool isProjected = m_coordType == COORDTYPE_PROJECTED;
    bool useLodBias  = m_coordType == COORDTYPE_BASIC_BIAS;

    tcu::Surface renderedFrame(viewport.width, viewport.height);

    // Accuracy cases test against ideal lod computation.
    tcu::Surface idealFrame(viewport.width, viewport.height);

    // Accuracy measurements are off unless we get the expected viewport size.
    if (viewport.width < defViewportWidth || viewport.height < defViewportHeight)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    // Upload texture data.
    m_texture->upload();

    // Use unit 0.
    gl.activeTexture(GL_TEXTURE0);

    // Bind gradient texture and setup sampler parameters.
    gl.bindTexture(GL_TEXTURE_CUBE_MAP, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, m_wrapS);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, m_wrapT);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, m_minFilter);
    gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, magFilter);

    GLU_EXPECT_NO_ERROR(gl.getError(), "After texture setup");

    // Compute grid.
    vector<IVec4> gridLayout;
    computeGridLayout(gridLayout, viewport.width, viewport.height);

    // Bias values.
    static const float s_bias[] = {1.0f, -2.0f, 0.8f, -0.5f, 1.5f, 0.9f, 2.0f, 4.0f};

    // Projection values \note Less agressive than in 2D case due to smaller quads.
    static const Vec4 s_projections[] = {Vec4(1.2f, 1.0f, 0.7f, 1.0f), Vec4(1.3f, 0.8f, 0.6f, 1.1f),
                                         Vec4(0.8f, 1.0f, 1.2f, 0.8f), Vec4(1.2f, 1.0f, 1.3f, 0.9f)};

    for (int cellNdx = 0; cellNdx < (int)gridLayout.size(); cellNdx++)
    {
        int curX               = gridLayout[cellNdx].x();
        int curY               = gridLayout[cellNdx].y();
        int curW               = gridLayout[cellNdx].z();
        int curH               = gridLayout[cellNdx].w();
        tcu::CubeFace cubeFace = (tcu::CubeFace)(cellNdx % tcu::CUBEFACE_LAST);
        ReferenceParams params(TEXTURETYPE_CUBE);

        params.sampler = sampler;

        DE_ASSERT(m_coordType != COORDTYPE_AFFINE); // Not supported.
        computeQuadTexCoordCube(texCoord, cubeFace);

        if (isProjected)
        {
            params.flags |= ReferenceParams::PROJECTED;
            params.w = s_projections[cellNdx % DE_LENGTH_OF_ARRAY(s_projections)];
        }

        if (useLodBias)
        {
            params.flags |= ReferenceParams::USE_BIAS;
            params.bias = s_bias[cellNdx % DE_LENGTH_OF_ARRAY(s_bias)];
        }

        // Render with GL.
        gl.viewport(viewport.x + curX, viewport.y + curY, curW, curH);
        m_renderer.renderQuad(0, &texCoord[0], params);

        // Render reference(s).
        {
            tcu::SurfaceAccess idealDst(idealFrame, m_renderCtx.getRenderTarget().getPixelFormat(), curX, curY, curW,
                                        curH);
            params.lodMode = LODMODE_EXACT;
            sampleTexture(idealDst, m_texture->getRefTexture(), &texCoord[0], params);
        }
    }

    // Read result.
    glu::readPixels(m_renderCtx, viewport.x, viewport.y, renderedFrame.getAccess());

    // Compare and log.
    {
        const int bestScoreDiff  = (texWidth / 16) * (texHeight / 16);
        const int worstScoreDiff = texWidth * texHeight;

        int score = measureAccuracy(log, idealFrame, renderedFrame, bestScoreDiff, worstScoreDiff);
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::toString(score).c_str());
    }

    return STOP;
}

TextureMipmapTests::TextureMipmapTests(Context &context) : TestCaseGroup(context, "mipmap", "Mipmapping accuracy tests")
{
}

TextureMipmapTests::~TextureMipmapTests(void)
{
}

void TextureMipmapTests::init(void)
{
    tcu::TestCaseGroup *group2D   = new tcu::TestCaseGroup(m_testCtx, "2d", "2D Texture Mipmapping");
    tcu::TestCaseGroup *groupCube = new tcu::TestCaseGroup(m_testCtx, "cube", "Cube Map Filtering");
    addChild(group2D);
    addChild(groupCube);

    static const struct
    {
        const char *name;
        uint32_t mode;
    } wrapModes[] = {{"clamp", GL_CLAMP_TO_EDGE}, {"repeat", GL_REPEAT}, {"mirror", GL_MIRRORED_REPEAT}};

    static const struct
    {
        const char *name;
        uint32_t mode;
    } minFilterModes[] = {{"nearest_nearest", GL_NEAREST_MIPMAP_NEAREST},
                          {"linear_nearest", GL_LINEAR_MIPMAP_NEAREST},
                          {"nearest_linear", GL_NEAREST_MIPMAP_LINEAR},
                          {"linear_linear", GL_LINEAR_MIPMAP_LINEAR}};

    static const struct
    {
        CoordType type;
        const char *name;
        const char *desc;
    } coordTypes[] = {{COORDTYPE_BASIC, "basic", "Mipmapping with translated and scaled coordinates"},
                      {COORDTYPE_AFFINE, "affine", "Mipmapping with affine coordinate transform"},
                      {COORDTYPE_PROJECTED, "projected", "Mipmapping with perspective projection"}};

    const int tex2DWidth  = 64;
    const int tex2DHeight = 64;

    // 2D cases.
    for (int coordType = 0; coordType < DE_LENGTH_OF_ARRAY(coordTypes); coordType++)
    {
        tcu::TestCaseGroup *coordTypeGroup =
            new tcu::TestCaseGroup(m_testCtx, coordTypes[coordType].name, coordTypes[coordType].desc);
        group2D->addChild(coordTypeGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
        {
            for (int wrapMode = 0; wrapMode < DE_LENGTH_OF_ARRAY(wrapModes); wrapMode++)
            {
                std::ostringstream name;
                name << minFilterModes[minFilter].name << "_" << wrapModes[wrapMode].name;

                coordTypeGroup->addChild(new Texture2DMipmapCase(
                    m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), name.str().c_str(), "",
                    coordTypes[coordType].type, minFilterModes[minFilter].mode, wrapModes[wrapMode].mode,
                    wrapModes[wrapMode].mode, GL_RGBA, GL_UNSIGNED_BYTE, tex2DWidth, tex2DHeight));
            }
        }
    }

    const int cubeMapSize = 64;

    static const struct
    {
        CoordType type;
        const char *name;
        const char *desc;
    } cubeCoordTypes[] = {{COORDTYPE_BASIC, "basic", "Mipmapping with translated and scaled coordinates"},
                          {COORDTYPE_PROJECTED, "projected", "Mipmapping with perspective projection"}};

    // Cubemap cases.
    for (int coordType = 0; coordType < DE_LENGTH_OF_ARRAY(cubeCoordTypes); coordType++)
    {
        tcu::TestCaseGroup *coordTypeGroup =
            new tcu::TestCaseGroup(m_testCtx, cubeCoordTypes[coordType].name, cubeCoordTypes[coordType].desc);
        groupCube->addChild(coordTypeGroup);

        for (int minFilter = 0; minFilter < DE_LENGTH_OF_ARRAY(minFilterModes); minFilter++)
        {
            coordTypeGroup->addChild(new TextureCubeMipmapCase(
                m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), minFilterModes[minFilter].name, "",
                cubeCoordTypes[coordType].type, minFilterModes[minFilter].mode, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                GL_RGBA, GL_UNSIGNED_BYTE, cubeMapSize));
        }
    }
}

} // namespace Accuracy
} // namespace gles3
} // namespace deqp
